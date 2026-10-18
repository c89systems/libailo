#include "ailo_store_internal.hpp"
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <lc/lc.h>
#include <mutex>
#include <sys/stat.h>
#include <thread>

using namespace ailo::client;
namespace fs = std::filesystem;
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x))                                                                  \
      throw std::runtime_error(std::string("line ") +                          \
                               std::to_string(__LINE__) + ": " #x);            \
  } while (0)
namespace {
std::atomic<bool> fail_marker{false};
using acquire_fn = decltype(lc_client::acquire);
std::map<lc_client *, acquire_fn> acquires;
int acquire(lc_client *c, const lc_acquire_req *r, lc_lease **out,
            lc_error *e) {
  const auto rc = acquires.at(c)(c, r, out, e);
  if (rc == LC_OK && std::string(r->key).compare(0, 2, "c-") == 0 &&
      fail_marker.exchange(false))
    (*out)->update = [](lc_lease *, lc_source *, const lc_update_opts *,
                        lc_error *e) {
      return lc_error_set(e, LC_ERR_TRANSPORT, 0,
                          "injected commit write failure", nullptr, nullptr,
                          nullptr);
    };
  return rc;
}
struct repository {
  ailo_record_repository api{};
  std::shared_ptr<void> owner;
  explicit repository(
      const ailo_openai_config &c = ailo_openai_default_config())
      : owner(default_repository(c, api)) {}
  void *reserve(const char *id, const char *ns = "tenant",
                uint64_t expiry = 0) {
    ailo_record_metadata m{ns,
                           id,
                           AILO_RECORD_RESPONSE,
                           ailo_u64_from_ulong(1),
                           ailo::detail::from_unsigned(expiry),
                           2 * 1024 * 1024};
    void *t = nullptr;
    CHECK(api.reserve(api.user_data, &m, &t, nullptr) == AILO_OK);
    return t;
  }
  void write(void *t, ailo_record_section part, const std::string &s) {
    for (size_t i = 0; i < s.size(); i += 173)
      CHECK(api.write(api.user_data, t, part, s.data() + i,
                      std::min(size_t(173), s.size() - i), nullptr) == AILO_OK);
  }
  void commit(void *t) {
    CHECK(api.commit(api.user_data, t, nullptr) == AILO_OK);
  }
  void *open(const char *id,
             ailo_record_protocol protocol = AILO_RECORD_RESPONSE,
             const char *ns = "tenant") {
    void *r = nullptr;
    ailo_u64 expiry{};
    CHECK(api.open(api.user_data, ns, protocol, id, &r, &expiry, nullptr) ==
          AILO_OK);
    return r;
  }
  bool missing(const char *id,
               ailo_record_protocol protocol = AILO_RECORD_RESPONSE,
               const char *ns = "tenant") {
    void *r = nullptr;
    ailo_u64 expiry{};
    return api.open(api.user_data, ns, protocol, id, &r, &expiry, nullptr) ==
               AILO_NOT_FOUND &&
           !r;
  }
  std::string read(void *r, ailo_record_section part = AILO_RECORD_OUTPUT) {
    std::string s;
    CHECK(api.read(
              api.user_data, r, part,
              [](void *u, const void *p, size_t n) {
                CHECK(n <= 32768);
                static_cast<std::string *>(u)->append(
                    static_cast<const char *>(p), n);
                return 1;
              },
              &s, nullptr) == AILO_OK);
    return s;
  }
  void close(void *r) { api.close(api.user_data, r); }
  void *conversation(const char *id, uint64_t revision = 0,
                     const ailo_record_metadata *response = nullptr) {
    ailo_conversation_change c{"tenant",
                               id,
                               revision ? AILO_CONVERSATION_APPEND
                                        : AILO_CONVERSATION_CREATE,
                               ailo::detail::from_unsigned(revision),
                               2 * 1024 * 1024,
                               response,
                               nullptr,
                               nullptr,
                               0};
    void *t = nullptr;
    CHECK(api.amend(api.user_data, &c, &t, nullptr) == AILO_OK);
    return t;
  }
  void state(void *t, unsigned revision) {
    write(t, AILO_RECORD_CONVERSATION_STATE,
          "{\"ailo_revision\":" + std::to_string(revision) +
              ",\"ailo_deleted\":false,\"ailo_rebuild\":false,\"input\":[],"
              "\"output\":[]}");
  }
};
std::string file(const fs::path &path) {
  std::ifstream f(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
void mode(const fs::path &p, unsigned bits) {
  struct stat s{};
  CHECK(!stat(p.c_str(), &s) && (s.st_mode & 0777) == bits);
}
} // namespace
extern "C" int __real_lc_client_open(const lc_client_config *, lc_client **,
                                     lc_error *);
extern "C" int __wrap_lc_client_open(const lc_client_config *c, lc_client **out,
                                     lc_error *e) {
  const auto rc = __real_lc_client_open(c, out, e);
  if (rc == LC_OK) {
    acquires[*out] = (*out)->acquire;
    (*out)->acquire = acquire;
  }
  return rc;
}
int main(int argc, char **argv) {
  try {
    CHECK(argc == 3);
    const fs::path work = fs::absolute(argv[1]);
    CHECK(work.filename() == "pouch-test" &&
          work.parent_path().parent_path().filename() == "build");
    fs::remove_all(work);
    fs::create_directories(work);
    const auto state = (work / "state").string(),
               config = (work / "config").string(),
               home = (work / "home").string();
    setenv("XDG_STATE_HOME", state.c_str(), 1);
    setenv("XDG_CONFIG_HOME", config.c_str(), 1);
    setenv("HOME", home.c_str(), 1);
    const auto body =
        std::string("{\"id\":\"response-one\",\"model\":\"tiny\",\"text\":\"") +
        std::string(150000, 'z') + "sentinel-secret-payload\"}";
    std::string key;
    {
      repository first, second;
      CHECK(!fs::exists(work / "state/ailo") &&
            !fs::exists(work / "config/ailo"));
      auto t = first.reserve("response-one");
      first.write(t, AILO_RECORD_OUTPUT, body);
      first.write(t, AILO_RECORD_INPUT, "{\"input\":[],\"output\":[]}");
      first.write(t, AILO_RECORD_METADATA, "{\"purpose\":\"test\"}");
      first.write(t, AILO_RECORD_RESPONSE_INPUT_ITEMS,
                  "[{\"id\":\"input-one\",\"type\":\"message\",\"content\":"
                  "\"hello\"}]");
      CHECK(second.missing("response-one"));
      first.commit(t);
      CHECK(
          first.missing("response-one", AILO_RECORD_RESPONSE, "other-tenant"));
      auto old = second.open("response-one");
      CHECK(second.read(old) == body);
      // A blocked consumer must not hold a repository or namespace write lock.
      std::mutex gate;
      std::condition_variable ready;
      bool reading = false, written = false, timed_out = false;
      struct blocked_read {
        std::mutex &gate;
        std::condition_variable &ready;
        bool &reading, &written, &timed_out;
      } blocked{gate, ready, reading, written, timed_out};
      ailo_status read_status = AILO_INTERNAL_ERROR;
      std::thread slow([&] {
        read_status = second.api.read(
            second.api.user_data, old, AILO_RECORD_OUTPUT,
            [](void *u, const void *, size_t) {
              auto &b = *static_cast<blocked_read *>(u);
              std::unique_lock<std::mutex> lock(b.gate);
              b.reading = true;
              b.ready.notify_all();
              if (!b.ready.wait_for(lock, std::chrono::seconds(5),
                                    [&] { return b.written; })) {
                b.timed_out = true;
                return -1;
              }
              return 1;
            },
            &blocked, nullptr);
      });
      {
        std::unique_lock<std::mutex> lock(gate);
        ready.wait(lock, [&] { return reading; });
      }
      auto concurrent = first.reserve("concurrent");
      first.write(concurrent, AILO_RECORD_OUTPUT, "{}");
      first.commit(concurrent);
      {
        std::lock_guard<std::mutex> lock(gate);
        written = true;
        ready.notify_all();
      }
      slow.join();
      CHECK(!timed_out && read_status == AILO_OK);
      CHECK(second.api.read(
                second.api.user_data, old, AILO_RECORD_OUTPUT,
                [](void *, const void *, size_t) { return 0; }, nullptr,
                nullptr) == AILO_CANCELLED);
      CHECK(second.api.read(
                second.api.user_data, old, AILO_RECORD_OUTPUT,
                [](void *, const void *, size_t) { return -1; }, nullptr,
                nullptr) == AILO_SINK_FAILED);
      ailo_record_query query{};
      query.namespace_id = "tenant";
      query.protocol = AILO_RECORD_RESPONSE;
      query.id = "response-one";
      query.section = AILO_RECORD_RESPONSE_INPUT_ITEMS;
      query.limit = 1;
      void *page = nullptr;
      ailo_u64 expiry{};
      CHECK(first.api.scan(first.api.user_data, &query, &page, &expiry,
                           nullptr) == AILO_OK);
      CHECK(first.read(page).find("input-one") != std::string::npos);
      first.close(page);
      ailo_record_edit edit{
          "tenant", AILO_RECORD_RESPONSE, "response-one", 2 * 1024 * 1024, {}};
      void *pre = nullptr;
      CHECK(first.api.edit(first.api.user_data, &edit, &t, &pre, &expiry,
                           nullptr) == AILO_OK);
      const std::string updated = "{\"id\":\"response-one\",\"model\":\"tiny\","
                                  "\"metadata\":{\"purpose\":\"changed\"}}";
      first.write(t, AILO_RECORD_OUTPUT, updated);
      first.write(t, AILO_RECORD_METADATA, "{\"purpose\":\"changed\"}");
      first.commit(t);
      CHECK(first.read(pre) == body && second.read(old) == body);
      first.close(pre);
      second.close(old);
      auto current = first.open("response-one");
      CHECK(first.read(current) == updated);
      first.close(current);
      t = first.reserve("aborted");
      first.write(t, AILO_RECORD_OUTPUT, body);
      first.api.abort(first.api.user_data, t);
      CHECK(first.missing("aborted"));
      ailo_record_metadata small{"tenant", "quota", AILO_RECORD_RESPONSE,
                                 {},       {},      8};
      t = nullptr;
      CHECK(first.api.reserve(first.api.user_data, &small, &t, nullptr) ==
            AILO_OK);
      CHECK(first.api.write(first.api.user_data, t, AILO_RECORD_OUTPUT,
                            body.data(), body.size(),
                            nullptr) == AILO_OUT_OF_RANGE);
      first.api.abort(first.api.user_data, t);
      CHECK(first.missing("quota"));
      t = first.reserve("expired", "tenant", 1);
      first.write(t, AILO_RECORD_OUTPUT, "{}");
      first.commit(t);
      CHECK(first.missing("expired"));
      t = first.conversation("conversation");
      first.state(t, 1);
      first.write(t, AILO_RECORD_OUTPUT,
                  "{\"id\":\"conversation\",\"created_at\":1,\"metadata\":{}}");
      first.commit(t);
      t = first.conversation("conversation", 1);
      ailo_conversation_change conflicting{"tenant",
                                           "conversation",
                                           AILO_CONVERSATION_APPEND,
                                           ailo_u64_from_ulong(1),
                                           1024,
                                           nullptr,
                                           nullptr,
                                           nullptr,
                                           0};
      void *conflict = nullptr;
      CHECK(second.api.amend(second.api.user_data, &conflicting, &conflict,
                             nullptr) == AILO_BUSY &&
            !conflict);
      auto other = second.conversation("independent");
      second.state(other, 1);
      second.write(other, AILO_RECORD_OUTPUT, "{\"id\":\"independent\"}");
      second.commit(other);
      first.api.abort(first.api.user_data, t);
      const ailo_record_metadata response{"tenant",
                                          "response-two",
                                          AILO_RECORD_RESPONSE,
                                          ailo_u64_from_ulong(2),
                                          {},
                                          2 * 1024 * 1024};
      auto append = [&] {
        auto tx = first.conversation("conversation", 1, &response);
        first.state(tx, 2);
        first.write(tx, AILO_RECORD_OUTPUT,
                    "{\"id\":\"response-two\",\"model\":\"tiny\"}");
        first.write(tx, AILO_RECORD_INPUT, "{\"input\":[],\"output\":[]}");
        first.write(tx, AILO_RECORD_METADATA, "{}");
        first.write(tx, AILO_RECORD_CONVERSATION_ITEMS,
                    "[{\"id\":\"message-one\",\"wire\":{\"type\":\"message\","
                    "\"id\":\"message-one\",\"content\":\"hello\"},"
                    "\"canonical\":{\"type\":\"message\",\"id\":\"message-"
                    "one\",\"role\":\"user\",\"content\":\"hello\"}}]");
        return tx;
      };
      t = append();
      fail_marker = true;
      CHECK(first.api.commit(first.api.user_data, t, nullptr) ==
            AILO_IO_FAILED);
      first.api.abort(first.api.user_data, t);
      CHECK(first.missing("response-two"));
      current = first.open("conversation", AILO_RECORD_CONVERSATION);
      CHECK(
          first.read(current, AILO_RECORD_INPUT).find("\"ailo_revision\":1") !=
          std::string::npos);
      first.close(current);
      mode(work / "config/ailo/pouch.key", 0600);
      mode(work / "config/ailo", 0700);
      mode(work / "state/ailo", 0700);
      key = file(work / "config/ailo/pouch.key");
      CHECK(!key.empty());
    }
    // Reopen after the deliberately interrupted multi-record publication.
    {
      repository r;
      CHECK(r.missing("response-two") && r.missing("aborted") &&
            r.missing("expired"));
      auto old = r.open("conversation", AILO_RECORD_CONVERSATION);
      CHECK(r.read(old, AILO_RECORD_INPUT).find("\"ailo_revision\":1") !=
            std::string::npos);
      r.close(old);
      const ailo_record_metadata response{"tenant",
                                          "response-two",
                                          AILO_RECORD_RESPONSE,
                                          ailo_u64_from_ulong(2),
                                          {},
                                          2 * 1024 * 1024};
      auto t = r.conversation("conversation", 1, &response);
      r.state(t, 2);
      r.write(t, AILO_RECORD_OUTPUT,
              "{\"id\":\"response-two\",\"model\":\"tiny\"}");
      r.write(t, AILO_RECORD_INPUT, "{}");
      r.write(t, AILO_RECORD_METADATA, "{}");
      r.write(t, AILO_RECORD_CONVERSATION_ITEMS,
              "[{\"id\":\"message-one\",\"wire\":{\"type\":\"message\",\"id\":"
              "\"message-one\",\"content\":\"hello\"},\"canonical\":{\"type\":"
              "\"message\",\"id\":\"message-one\",\"content\":\"hello\"}}]");
      r.commit(t);
      auto stored = r.open("response-two");
      CHECK(r.read(stored).find("response-two") != std::string::npos);
      r.close(stored);
      ailo_conversation_query q{
          "tenant", "conversation", nullptr, nullptr, 1, 0, 0};
      void *items = nullptr;
      CHECK(r.api.query(r.api.user_data, &q, &items, nullptr) == AILO_OK);
      CHECK(r.read(items).find("message-one") != std::string::npos);
      r.close(items);
      q.canonical = 1;
      CHECK(r.api.query(r.api.user_data, &q, &items, nullptr) == AILO_OK);
      CHECK(r.read(items).find("hello") != std::string::npos);
      r.close(items);
      CHECK(file(work / "config/ailo/pouch.key") == key);
      q.id = "independent";
      q.item_id = "message-one";
      items = nullptr;
      CHECK(r.api.query(r.api.user_data, &q, &items, nullptr) ==
                AILO_NOT_FOUND &&
            !items);
      const char *ids[] = {"message-one"};
      ailo_conversation_change duplicate{"tenant",
                                         "conversation",
                                         AILO_CONVERSATION_APPEND,
                                         ailo_u64_from_ulong(2),
                                         1024,
                                         nullptr,
                                         nullptr,
                                         ids,
                                         1};
      t = nullptr;
      CHECK(r.api.amend(r.api.user_data, &duplicate, &t, nullptr) ==
                AILO_INVALID_ARGUMENT &&
            !t);
    }
    for (const auto &entry :
         fs::recursive_directory_iterator(work / "state/ailo"))
      if (entry.is_regular_file())
        CHECK(file(entry.path()).find("sentinel-secret-payload") ==
              std::string::npos);
    // Missing original keys fail closed instead of generating a replacement.
    fs::rename(work / "config/ailo/pouch.key", work / "config/ailo/saved.key");
    {
      repository r;
      void *out = nullptr;
      ailo_u64 expiry{};
      CHECK(r.api.open(r.api.user_data, "tenant", AILO_RECORD_RESPONSE,
                       "response-one", &out, &expiry,
                       nullptr) == AILO_IO_FAILED &&
            !out);
      CHECK(!fs::exists(work / "config/ailo/pouch.key"));
      auto c = ailo_client_default_config();
      c.source.local_path = argv[2];
      c.lazy = 1;
      c.backend = AILO_BACKEND_CPU;
      auto api = ailo_openai_default_config();
      api.namespace_id = "tenant";
      c.openai = &api;
      ailo_client *client = nullptr;
      ailo_error error{};
      ailo_api_info info{};
      CHECK(ailo_client_new(&c, &client, &error) == AILO_OK);
      CHECK(client->response_retrieve(
                client, "response-one",
                [](void *, const void *, size_t) { return 1; }, nullptr, &info,
                &error) == AILO_IO_FAILED);
      CHECK(std::string(error.message).find("restore the original key") !=
            std::string::npos);
      ailo_client_destroy(client);
    }
    fs::rename(work / "config/ailo/saved.key", work / "config/ailo/pouch.key");
    unsetenv("XDG_STATE_HOME");
    unsetenv("XDG_CONFIG_HOME");
    {
      repository r;
      auto t = r.reserve("fallback");
      r.write(t, AILO_RECORD_OUTPUT, "{}");
      r.commit(t);
      CHECK(fs::exists(work / "home/.local/state/ailo") &&
            fs::exists(work / "home/.config/ailo/pouch.key"));
    }
    // Actual model generation through the public receiver, using default Pouch.
    {
      auto c = ailo_client_default_config();
      c.source.local_path = argv[2];
      c.lazy = 1;
      c.backend = AILO_BACKEND_CPU;
      c.context_tokens = 512;
      c.threads = 1;
      auto api = ailo_openai_default_config();
      const auto root = (work / "real pouch %?#").string(),
                 secret = (work / "real-key/pouch.key").string();
      api.state_directory = root.c_str();
      api.encryption_key_file = secret.c_str();
      c.openai = &api;
      ailo_client *client = nullptr;
      ailo_error error{};
      ailo_api_info info{};
      std::string output;
      auto sink = [](void *u, const void *p, size_t n) {
        static_cast<std::string *>(u)->append(static_cast<const char *>(p), n);
        return 1;
      };
      CHECK(ailo_client_new(&c, &client, &error) == AILO_OK);
      CHECK(!fs::exists(root));
      const std::string request = "{\"input\":\"Hi\",\"stream\":true,\"max_"
                                  "output_tokens\":1,\"temperature\":0}";
      CHECK(client->responses(client, request.data(), request.size(), sink,
                              &output, &info, &error) == AILO_OK);
      CHECK(info.stored &&
            output.find("response.output_text.delta") != std::string::npos);
      const std::string id = info.id;
      ailo_client_destroy(client);
      output.clear();
      CHECK(ailo_client_new(&c, &client, &error) == AILO_OK);
      CHECK(client->response_retrieve(client, id.c_str(), sink, &output, &info,
                                      &error) == AILO_OK &&
            output.find(id) != std::string::npos);
      ailo_client_destroy(client);
      const auto absent = (work / "zdr-pouch").string(),
                 absent_key = (work / "zdr-key/pouch.key").string();
      api.state_directory = absent.c_str();
      api.encryption_key_file = absent_key.c_str();
      api.zero_data_retention = 1;
      CHECK(ailo_client_new(&c, &client, &error) == AILO_OK);
      output.clear();
      CHECK(client->responses(client, request.data(), request.size(), sink,
                              &output, &info, &error) == AILO_OK &&
            !info.stored);
      ailo_client_destroy(client);
      CHECK(!fs::exists(absent) && !fs::exists(absent_key));
      c.openai = nullptr;
      unsetenv("HOME");
      unsetenv("XDG_STATE_HOME");
      unsetenv("XDG_CONFIG_HOME");
      CHECK(ailo_client_new(&c, &client, &error) == AILO_OK);
      ailo_client_destroy(client);
    }
    std::cout << "Encrypted Pouch persistence, snapshots, pagination, "
                 "namespace isolation, cancellation, reservation conflicts, "
                 "atomic failure/recovery and XDG fallbacks passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
