#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <lc/lc.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/stat.h>

namespace fs = std::filesystem;
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      throw std::runtime_error(std::string("line ") +                          \
                               std::to_string(__LINE__) + ": " #x);            \
  } while (0)

template <class T> struct close_receiver {
  void operator()(T *p) const { p->close(p); }
};
template <class T> using receiver = std::unique_ptr<T, close_receiver<T>>;
struct sdk_error {
  lc_error value{};
  ~sdk_error() { lc_error_cleanup(&value); }
  void check(int rc) {
    if (rc != LC_OK)
      throw std::runtime_error(value.message ? value.message : "SDK error");
  }
};

int main(int argc, char **argv) {
  try {
    CHECK(argc == 2);
    const fs::path work = fs::absolute(argv[1]);
    CHECK(work.filename().string().find("lockdc-collections-") == 0 &&
          work.parent_path().parent_path().filename() == "build");
    fs::remove_all(work);
    fs::create_directories(work);
    CHECK(::chmod(work.c_str(), 0700) == 0);
    const auto root = (work / "pouch").string();
    const auto key = (work / "pouch.key").string();
    sdk_error e;
    char *raw_endpoint = nullptr;
    e.check(lc_pouch_endpoint_build(root.c_str(), nullptr, 0, &raw_endpoint,
                                    &e.value));
    std::unique_ptr<char, decltype(&lc_pouch_endpoint_free)> endpoint(
        raw_endpoint, lc_pouch_endpoint_free);
    const char *endpoints[] = {endpoint.get()};
    lc_pouch_settings settings;
    lc_pouch_settings_init(&settings);
    settings.set_mask = LC_POUCH_SETTING_CRYPTO_KEY_FILE |
                        LC_POUCH_SETTING_CRYPTO_GENERATE_KEY_FILE |
                        LC_POUCH_SETTING_COMPRESSION |
                        LC_POUCH_SETTING_DURABLE_SYNC;
    settings.crypto_key_file = key.c_str();
    settings.crypto_generate_key_file = 1;
    settings.compression = "zlib";
    settings.durable_sync = 1;
    lc_client_config config;
    lc_client_config_init(&config);
    config.endpoints = endpoints;
    config.endpoint_count = 1;
    config.default_namespace = "collection-test";
    config.pouch_settings = &settings;
    auto open_client = [&] {
      lc_client *p = nullptr;
      e.check(lc_client_open(&config, &p, &e.value));
      return receiver<lc_client>(p);
    };
    auto client = open_client();
    lc_collection_capabilities caps{};
    e.check(lc_client_collection_capabilities(client.get(), &caps, &e.value));
    CHECK(caps.collections && caps.append_writer &&
          caps.append_collection_retention && caps.ranges && caps.atomic_edit);
    lc_collection_config collection_config;
    lc_collection_config_init(&collection_config);
    collection_config.name = "conversation";
    collection_config.has_retention_seconds = 1;
    collection_config.retention_seconds = 60;
    lc_collection *raw_collection = nullptr;
    e.check(lc_client_create_collection(client.get(), &collection_config,
                                        &raw_collection, &e.value));
    receiver<lc_collection> collection(raw_collection);
    lc_collection_info initial{};
    e.check(lc_collection_describe(collection.get(), &initial, &e.value));
    CHECK(initial.has_retention_expires_at_unix && initial.live_count == 0);
    const auto initial_expiry = initial.retention_expires_at_unix;
    const std::string identity = initial.collection_id;
    // Test input/expected output may be materialized. The SDK read sink checks
    // every chunk directly. The SDK push writer is explicitly staged/spooled.
    std::string body = "{\"text\":\"";
    uint32_t random = 1;
    for (size_t i = 0; i < 256 * 1024; ++i) {
      random = random * 1664525u + 1013904223u;
      body += "0123456789abcdef"[random >> 28];
    }
    body += "\"}";
    auto begin = [&](const char *id, long retention) {
      lc_collection_append_req req;
      lc_collection_append_req_init(&req);
      req.append_id = id;
      req.has_collection_retention_seconds = 1;
      req.collection_retention_seconds = retention;
      lc_collection_writer *p = nullptr;
      e.check(lc_collection_append_begin(collection.get(), &req, &p, &e.value));
      return receiver<lc_collection_writer>(p);
    };
    auto write = [&](lc_collection_writer *writer, const std::string &bytes) {
      for (size_t offset = 0; offset < bytes.size(); offset += 173)
        e.check(lc_collection_writer_write(
            writer, bytes.data() + offset,
            std::min(size_t(173), bytes.size() - offset), &e.value));
    };
    auto read = [&](const std::string &expected, uint64_t count) {
      struct comparison {
        const std::string &expected;
        size_t offset = 0;
        size_t calls = 0;
      } comparison{expected};
      lc_sink sink{};
      sink.impl = &comparison;
      sink.write = [](lc_sink *s, const void *bytes, size_t n, lc_error *) {
        auto &c = *static_cast<struct comparison *>(s->impl);
        CHECK(n <= 65536 && n <= c.expected.size() - c.offset);
        CHECK(c.expected.compare(c.offset, n, static_cast<const char *>(bytes),
                                 n) == 0);
        c.offset += n;
        ++c.calls;
        return 1;
      };
      lc_collection_read_opts opts;
      lc_collection_read_opts_init(&opts);
      lc_collection_read_res result;
      lc_collection_read_res_init(&result);
      const auto rc =
          lc_collection_read(collection.get(), &opts, &sink, &result, &e.value);
      const auto complete = result.complete;
      const auto records = result.completed_records;
      lc_collection_read_res_cleanup(&result);
      e.check(rc);
      CHECK(complete && records == count &&
            comparison.offset == expected.size());
      CHECK(expected.size() <= 65536 || comparison.calls > 1);
    };
    auto writer = begin("first", 90L * 24 * 60 * 60);
    write(writer.get(), body);
    read("", 0);
    lc_collection_append_res appended{};
    e.check(lc_collection_writer_commit(writer.get(), &appended, &e.value));
    writer.reset();
    CHECK(!appended.duplicate && appended.position > 0 &&
          appended.bytes == body.size());
    CHECK(appended.has_collection_retention_expires_at_unix &&
          appended.collection_retention_expires_at_unix > initial_expiry);
    CHECK(appended.has_retention_expires_at_unix &&
          appended.retention_expires_at_unix == 0);
    const auto expiry = appended.collection_retention_expires_at_unix;
    read(body + "\n", 1);
    auto retry = begin("first", 180L * 24 * 60 * 60);
    write(retry.get(), body);
    lc_collection_append_res duplicate{};
    e.check(lc_collection_writer_commit(retry.get(), &duplicate, &e.value));
    retry.reset();
    CHECK(duplicate.duplicate && duplicate.position == appended.position &&
          duplicate.collection_retention_expires_at_unix == expiry);
    auto aborted = begin("aborted", 180L * 24 * 60 * 60);
    write(aborted.get(), body);
    e.check(lc_collection_writer_abort(aborted.get(), &e.value));
    e.check(lc_collection_writer_abort(aborted.get(), &e.value));
    aborted.reset();
    auto invalid = begin("invalid", 180L * 24 * 60 * 60);
    write(invalid.get(), "{\"incomplete\":");
    lc_collection_append_res rejected{};
    CHECK(lc_collection_writer_commit(invalid.get(), &rejected, &e.value) !=
          LC_OK);
    invalid.reset();
    lc_error_cleanup(&e.value);
    lc_error_init(&e.value);
    for (const auto *id : {"aborted", "invalid"}) {
      lc_collection_append_status_res status{};
      e.check(
          lc_collection_append_status(collection.get(), id, &status, &e.value));
      CHECK(status.state == LC_COLLECTION_APPEND_ABSENT);
    }
    lc_collection_info info{};
    e.check(lc_collection_describe(collection.get(), &info, &e.value));
    CHECK(info.live_count == 1 && info.retention_expires_at_unix == expiry);
    collection.reset();
    client.reset();
    client = open_client();
    collection_config.has_retention_seconds = 0;
    e.check(lc_client_open_collection(client.get(), &collection_config,
                                      &raw_collection, &e.value));
    collection.reset(raw_collection);
    read(body + "\n", 1);
    e.check(lc_collection_describe(collection.get(), &info, &e.value));
    CHECK(identity == info.collection_id && info.live_count == 1 &&
          info.upper_position == appended.position &&
          info.retention_expires_at_unix == expiry);
    std::cout << "Encrypted/zlib collection push commit, abort, retry, "
                 "fragmented read, parent retention and reopen passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
