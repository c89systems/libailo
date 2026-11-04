#include <algorithm>
#include <cstdint>
#include <cstring>
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
  void rejected(int rc, const char *code) {
    CHECK(rc == LC_ERR_INVALID && value.server_code &&
          std::strcmp(value.server_code, code) == 0);
    lc_error_cleanup(&value);
    lc_error_init(&value);
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
          caps.append_collection_retention && caps.ranges && caps.atomic_edit &&
          caps.leases && caps.lease_takeover && caps.private_reads);
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
    auto read = [&](const std::string &expected, uint64_t count,
                    lc_collection *from = nullptr, const char *head = nullptr,
                    const char *rejection = nullptr) {
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
      opts.expected_head = head;
      lc_collection_read_res result;
      lc_collection_read_res_init(&result);
      const auto rc = lc_collection_read(from ? from : collection.get(), &opts,
                                         &sink, &result, &e.value);
      const auto complete = result.complete;
      const auto records = result.completed_records;
      lc_collection_read_res_cleanup(&result);
      if (rejection) {
        e.rejected(rc, rejection);
        CHECK(comparison.calls == 0 && comparison.offset == 0);
        return;
      }
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

    // Lease observations are not credentials. Private admission rejects before
    // any body byte; takeover fences an already prepared writer without sleeps.
    lc_collection_lease_req lease_req;
    lc_collection_lease_req_init(&lease_req);
    lease_req.owner = "first-owner";
    lease_req.ttl_seconds = 300;
    lease_req.private_read = 1;
    lc_collection *raw_owner = nullptr;
    e.check(lc_collection_acquire(collection.get(), &lease_req, &raw_owner,
                                  &e.value));
    receiver<lc_collection> owner(raw_owner);
    e.check(lc_collection_describe(collection.get(), &info, &e.value));
    const auto first_lease = info.lease;
    CHECK(first_lease.active && first_lease.private_read);
    read("", 0, nullptr, nullptr, "collection_private");
    read(body + "\n", 1, owner.get());
    lc_collection_head_res leased_head{};
    e.check(lc_collection_head(owner.get(), &leased_head, &e.value));
    lc_collection_lease_info renewed{};
    e.check(lc_collection_keepalive(owner.get(), 600, &renewed, &e.value));
    CHECK(std::strcmp(renewed.lease_id, first_lease.lease_id) == 0 &&
          renewed.fencing_token == first_lease.fencing_token);
    e.check(lc_collection_describe(collection.get(), &info, &e.value));
    CHECK(info.retention_expires_at_unix == expiry);
    read(body + "\n", 1, owner.get(), leased_head.etag);
    lc_collection_append_req pending_req;
    lc_collection_append_req_init(&pending_req);
    pending_req.append_id = "fenced";
    lc_collection_writer *raw_pending = nullptr;
    e.check(lc_collection_append_begin(owner.get(), &pending_req, &raw_pending,
                                       &e.value));
    receiver<lc_collection_writer> pending(raw_pending);
    write(pending.get(), "{}");
    lease_req.owner = "replacement-owner";
    e.check(lc_collection_takeover(collection.get(), &lease_req,
                                   first_lease.lease_id, &raw_owner, &e.value));
    receiver<lc_collection> replacement_owner(raw_owner);
    lc_collection *not_owned = nullptr;
    e.rejected(lc_collection_takeover(collection.get(), &lease_req,
                                      first_lease.lease_id, &not_owned,
                                      &e.value),
               "lease_changed");
    CHECK(!not_owned);
    e.rejected(lc_collection_writer_commit(pending.get(), &rejected, &e.value),
               "lease_lost");
    pending.reset();
    e.rejected(lc_collection_keepalive(owner.get(), 300, &renewed, &e.value),
               "lease_lost");
    e.rejected(lc_collection_release(owner.get(), &e.value), "lease_lost");
    owner.reset();
    lc_collection_append_status_res pending_status{};
    e.check(lc_collection_append_status(collection.get(), "fenced",
                                        &pending_status, &e.value));
    CHECK(pending_status.state == LC_COLLECTION_APPEND_ABSENT);
    e.check(lc_collection_describe(collection.get(), &info, &e.value));
    CHECK(info.lease.fencing_token > first_lease.fencing_token);
    const auto replacement_lease = info.lease;
    replacement_owner.reset(); // Local close preserves durable ownership.
    e.rejected(lc_collection_acquire(collection.get(), &lease_req, &not_owned,
                                     &e.value),
               "collection_leased");
    CHECK(!not_owned);
    lease_req.private_read = 0;
    e.check(lc_collection_takeover(collection.get(), &lease_req,
                                   replacement_lease.lease_id, &raw_owner,
                                   &e.value));
    replacement_owner.reset(raw_owner);
    read(body + "\n", 1); // Public reads remain possible during a public lease.
    e.check(lc_collection_release(replacement_owner.get(), &e.value));
    (void)replacement_owner.release(); // Successful SDK release closes it.

    // An edit replaces the selected interval at a new immutable position and
    // publishes its small recovery descriptor with that replacement.
    lc_collection_head_res before{};
    e.check(lc_collection_head(collection.get(), &before, &e.value));
    lc_collection_edit_range range{0, appended.position};
    lc_collection_append_req replacement_req;
    lc_collection_append_req_init(&replacement_req);
    replacement_req.append_id = "replacement";
    lc_collection_edit_req edit;
    lc_collection_edit_req_init(&edit);
    edit.operation_id = "compact";
    edit.expected_head = before.etag;
    edit.ranges = &range;
    edit.range_count = 1;
    edit.replacement = &replacement_req;
    edit.has_recovery = 1;
    edit.recovery = "committed-state";
    edit.recovery_anchor = UINT64_MAX;
    const std::string replacement_body = "{\"text\":\"replacement\"}";
    struct input {
      const std::string &bytes;
      size_t offset = 0;
    } input{replacement_body};
    lc_source source{};
    source.impl = &input;
    source.read = [](lc_source *s, void *bytes, size_t n, lc_error *) {
      auto &in = *static_cast<struct input *>(s->impl);
      n = std::min({n, size_t(3), in.bytes.size() - in.offset});
      std::memcpy(bytes, in.bytes.data() + in.offset, n);
      in.offset += n;
      return n;
    };
    lc_collection_edit_res edited{};
    e.check(lc_collection_edit(collection.get(), &edit, &source, &edited,
                               &e.value));
    CHECK(input.offset == replacement_body.size() && edited.published &&
          !edited.duplicate && edited.deleted_count == 1 &&
          edited.live_count == 1 && edited.has_replacement &&
          edited.replacement.position > appended.position);
    read("", 0, nullptr, before.etag, "collection_changed");
    lc_collection_head_res after{};
    e.check(lc_collection_head(collection.get(), &after, &e.value));
    CHECK(identity == after.collection_id && after.live_count == 1 &&
          after.upper_position == edited.replacement.position &&
          after.recovery_anchor == edited.replacement.position &&
          after.recovery_anchor_available && after.recovery_revision > 0 &&
          std::strcmp(after.recovery, edit.recovery) == 0 &&
          std::strcmp(after.etag, edited.head_etag) == 0);
    read(replacement_body + "\n", 1, nullptr, after.etag);
    lc_collection_edit_req stale = edit;
    stale.operation_id = "stale-head";
    lc_collection_edit_res conflicted{};
    input.offset = 0;
    e.rejected(lc_collection_edit(collection.get(), &stale, &source,
                                  &conflicted, &e.value),
               "collection_changed");
    CHECK(!conflicted.published && conflicted.live_count == 1 &&
          std::strcmp(conflicted.head_etag, after.etag) == 0);
    lc_collection_edit_status_res receipt{};
    e.check(lc_collection_edit_status(collection.get(), stale.operation_id,
                                      &receipt, &e.value));
    CHECK(!receipt.found);
    input.offset = 0;
    lc_collection_edit_res retried{};
    e.check(lc_collection_edit(collection.get(), &edit, &source, &retried,
                               &e.value));
    CHECK(retried.duplicate && retried.published &&
          retried.replacement.position == edited.replacement.position &&
          std::strcmp(retried.head_etag, edited.head_etag) == 0);
    collection.reset();
    client.reset();
    client = open_client();
    e.check(lc_client_open_collection(client.get(), &collection_config,
                                      &raw_collection, &e.value));
    collection.reset(raw_collection);
    read(replacement_body + "\n", 1);
    e.check(lc_collection_edit_status(collection.get(), edit.operation_id,
                                      &receipt, &e.value));
    CHECK(receipt.found && receipt.result.published &&
          receipt.result.replacement.position == edited.replacement.position);
    lc_collection_head_res recovered{};
    e.check(lc_collection_head(collection.get(), &recovered, &e.value));
    CHECK(identity == recovered.collection_id && recovered.live_count == 1 &&
          recovered.recovery_anchor == after.recovery_anchor &&
          recovered.recovery_anchor_available &&
          recovered.recovery_revision == after.recovery_revision &&
          std::strcmp(recovered.recovery, after.recovery) == 0);
    std::cout << "Encrypted/zlib collection push commit, abort, retry, "
                 "fragmented read, parent retention, lease fencing, atomic "
                 "edit, read preconditions and reopen passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
