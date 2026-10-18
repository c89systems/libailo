#include "ailo_store_internal.hpp"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <lc/lc.h>
#include <openssl/sha.h>
#include <set>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <thread>
#include <unistd.h>

namespace ailo {
namespace client {
namespace storage {
namespace {
constexpr size_t chunk_size = 32768;
constexpr int item_protocol = 3;
namespace fs = std::filesystem;
[[noreturn]] void fail(const char *s, ailo_status status = AILO_IO_FAILED) {
  throw failure(status, s);
}
void checked(int rc, lc_error &error) {
  if (rc == LC_OK) {
    lc_error_cleanup(&error);
    return;
  }
  const auto status = rc == LC_ERR_NOMEM ? AILO_NO_MEMORY : AILO_IO_FAILED;
  lc_error_cleanup(&error);
  fail("Pouch storage operation failed", status);
}
std::string unique(const char *prefix) {
  ailo_api_info info{};
  api_identity(info, prefix);
  return info.id;
}
std::string alias(const std::string &ns, int protocol, const std::string &id) {
  const auto text = ns + '\0' + std::to_string(protocol) + '\0' + id;
  unsigned char digest[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char *>(text.data()), text.size(),
         digest);
  std::string key = "a-";
  for (auto b : digest) {
    key += "0123456789abcdef"[b >> 4];
    key += "0123456789abcdef"[b & 15];
  }
  return key;
}
const char *space(const std::string &key) {
  return key.compare(0, 2, "b-") == 0 ? "ailo-data" : "ailo-index";
}
std::string absolute(const std::string &s) {
  if (s.empty() || s.front() != '/')
    fail("Pouch paths must be absolute", AILO_INVALID_ARGUMENT);
  return fs::path(s).lexically_normal().string();
}
std::string xdg(const char *name, const char *fallback) {
  const char *p = std::getenv(name);
  if (p && *p && *p == '/')
    return absolute(std::string(p) + "/ailo");
  p = std::getenv("HOME");
  if (!p || *p != '/')
    fail("An absolute HOME or XDG directory is required for Pouch",
         AILO_INVALID_ARGUMENT);
  return absolute(std::string(p) + fallback + "/ailo");
}
void directory(const std::string &s) {
  std::error_code ec;
  if (!fs::exists(s, ec)) {
    fs::create_directories(fs::path(s).parent_path(), ec);
    if (ec || (::mkdir(s.c_str(), 0700) && errno != EEXIST))
      fail("Cannot create private Pouch directory");
  }
  struct stat st{};
  if (::lstat(s.c_str(), &st) || !S_ISDIR(st.st_mode) ||
      st.st_uid != ::geteuid() || (st.st_mode & 077))
    fail("Pouch directories must be owned by this user and have mode 0700");
}
struct transaction;
struct reader {
  std::shared_ptr<record> snapshot;
  std::vector<blob> selected;
  std::vector<std::shared_ptr<record>> pins;
  bool array = false;
};
struct store {
  std::mutex mutex;
  std::condition_variable wake;
  bool stopping = false;
  lc_client *client = nullptr;
  std::string root, key_file;
  std::map<std::string, std::shared_ptr<record>> records;
  std::vector<std::weak_ptr<record>> versions;
  std::set<std::string> reservations, active_objects, objects;
  size_t reserved_bytes = 0;
  std::thread maintenance;
  explicit store(std::string, std::string);
  ~store();
  void initialize();
  bool get(const std::string &, byte_sink);
  std::string get(const std::string &);
  void put(const std::string &, const std::string &);
  void remove(const std::string &);
  std::vector<std::string> keys(const char *, const char *);
  void read(const blob &, byte_sink);
  void publish(const std::vector<std::shared_ptr<record>> &,
               const std::vector<std::string> & = {});
  void collect();
  void pin(const std::shared_ptr<record> &r) { versions.push_back(r); }
  std::shared_ptr<record> find(const char *, int, const char *, uint64_t);
};
struct builder {
  store &owner;
  blob result;
  std::string pending;
  std::set<std::string> *allocated;
  explicit builder(store &s, std::set<std::string> *keys = nullptr)
      : owner(s), allocated(keys) {
    pending.reserve(chunk_size);
  }
  void flush() {
    if (pending.empty())
      return;
    const auto key = unique("b-");
    std::string encoded((pending.size() + 2) / 3 * 4, '\0');
    size_t size = 0;
    if (lonejson_base64_encode(pending.data(), pending.size(),
                               LONEJSON_BASE64_STANDARD, encoded.data(),
                               encoded.size(), &size,
                               nullptr) != LONEJSON_STATUS_OK)
      fail("Cannot encode Pouch storage chunk");
    value v;
    v.fields["kind"] = value(std::string("chunk"));
    v.fields["data"] = value(std::move(encoded));
    owner.active_objects.insert(key);
    if (allocated)
      allocated->insert(key);
    owner.put(key, encode(v, 65536));
    result.chunks.push_back(key);
    pending.clear();
  }
  void write(const void *bytes, size_t size) {
    const char *p = static_cast<const char *>(bytes);
    result.size += size;
    while (size) {
      const size_t n = std::min(size, chunk_size - pending.size());
      pending.append(p, n);
      p += n;
      size -= n;
      if (pending.size() == chunk_size)
        flush();
    }
  }
  blob finish() {
    flush();
    return result;
  }
};
struct transaction {
  std::shared_ptr<store> owner;
  std::shared_ptr<record> base, result, response;
  std::map<int, std::unique_ptr<builder>> parts;
  std::set<std::string> held, objects;
  size_t maximum = 0, bytes = 0;
  std::string removed;
  bool conversation = false;
  explicit transaction(std::shared_ptr<store> s, size_t n)
      : owner(std::move(s)), maximum(n) {}
  ~transaction() {
    for (const auto &key : held)
      owner->reservations.erase(key);
    for (const auto &p : parts)
      for (const auto &key : p.second->result.chunks)
        owner->active_objects.erase(key);
    for (const auto &key : objects)
      owner->active_objects.erase(key);
    owner->reserved_bytes -= maximum;
  }
  void hold(const std::string &key) {
    if (owner->reservations.count(key))
      fail("Pouch record already has an active writer", AILO_BUSY);
    held.insert(key);
    owner->reservations.insert(key);
  }
  void admit() {
    struct statvfs disk{};
    if (::statvfs(owner->root.c_str(), &disk))
      fail("Cannot inspect Pouch storage capacity");
    const uint64_t free = uint64_t(disk.f_bavail) * disk.f_frsize;
    if (maximum > SIZE_MAX - owner->reserved_bytes ||
        uint64_t(maximum) + owner->reserved_bytes > free / 2)
      fail("Pouch has insufficient unreserved storage capacity",
           AILO_OUT_OF_RANGE);
    owner->reserved_bytes += maximum;
  }
};
// Streaming extraction of wire/canonical row values. Only IDs are accumulated.
std::vector<row> rows(store &s, const blob &input, bool paired,
                      std::set<std::string> &allocated) {
  std::vector<row> result;
  std::unique_ptr<builder> wire, canonical;
  std::unique_ptr<wire_json> wout, cout;
  row current;
  events parser(
      [&](event e, const path &p, std::string_view v) {
        if (e == event::object && p.size() == 1) {
          current = {};
          if (!paired) {
            wire = std::make_unique<builder>(s, &allocated);
            wout = std::make_unique<wire_json>(
                [&](const void *x, size_t n) { wire->write(x, n); });
          }
        }
        if (e == event::string_chunk && p.size() == 2 && p[1] == "id") {
          if (v.size() > 256 - current.id.size())
            fail("Stored item ID exceeds its limit");
          current.id.append(v);
        }
        if (paired && p.size() == 2 && e == event::object &&
            (p[1] == "wire" || p[1] == "canonical")) {
          if (p[1] == "wire") {
            wire = std::make_unique<builder>(s, &allocated);
            wout = std::make_unique<wire_json>(
                [&](const void *x, size_t n) { wire->write(x, n); });
          } else {
            canonical = std::make_unique<builder>(s, &allocated);
            cout = std::make_unique<wire_json>(
                [&](const void *x, size_t n) { canonical->write(x, n); });
          }
        }
        if (!paired && p.size() >= 1 && wout)
          forward(*wout, e, v);
        if (paired && p.size() >= 2) {
          if (p[1] == "wire" && wout)
            forward(*wout, e, v);
          if (p[1] == "canonical" && cout)
            forward(*cout, e, v);
        }
        if (e == event::object_end && p.size() == 1) {
          if (current.id.empty() || !wout)
            fail("Invalid stored item row");
          wout->finish();
          current.wire = wire->finish();
          wout.reset();
          wire.reset();
          if (cout) {
            cout->finish();
            current.canonical = canonical->finish();
            cout.reset();
            canonical.reset();
          }
          for (const auto *b : {&current.wire, &current.canonical})
            for (const auto &key : b->chunks)
              allocated.insert(key);
          result.push_back(std::move(current));
        }
      },
      input.size);
  s.read(input, [&](const void *p, size_t n) { parser.push(p, n); });
  parser.finish();
  return result;
}
void attributes(store &s, record &r) {
  for (auto part : {AILO_RECORD_OUTPUT, AILO_RECORD_METADATA}) {
    auto found = r.parts.find(part);
    if (found == r.parts.end())
      continue;
    std::string text;
    events parser(
        [&](event e, const path &p, std::string_view v) {
          const bool select = p.size() == 1 &&
                              (part == AILO_RECORD_METADATA || p[0] == "model");
          if (!select)
            return;
          if (e == event::string)
            text.clear();
          if (e == event::string_chunk) {
            if (v.size() > 4096 - text.size())
              fail("Stored metadata exceeds its limit");
            text.append(v);
          }
          if (e == event::string_end) {
            if (part == AILO_RECORD_METADATA)
              r.metadata[p[0]] = text;
            else
              r.model = text;
          }
        },
        found->second.size);
    s.read(found->second, [&](const void *p, size_t n) { parser.push(p, n); });
    parser.finish();
  }
}
void store::put(const std::string &key, const std::string &text) {
  lc_error error{};
  lc_acquire_req req{};
  req.ns = space(key);
  req.key = key.c_str();
  req.owner = "libailo";
  req.ttl_seconds = 60;
  lc_lease *lease = nullptr;
  checked(client->acquire(client, &req, &lease, &error), error);
  lc_source *source = nullptr;
  try {
    checked(lc_source_from_memory(text.data(), text.size(), &source, &error),
            error);
    lc_update_opts opts{};
    opts.content_type = "application/json";
    objects.insert(key);
    checked(lease->update(lease, source, &opts, &error), error);
    source->close(source);
    source = nullptr;
    // update is the durable publication boundary. Lease cleanup must never
    // turn a successfully published commit marker into a reported failure.
    if (lease->release(lease, nullptr, &error) != LC_OK)
      lease->close(lease);
    lc_error_cleanup(&error);
    lease = nullptr;
  } catch (...) {
    if (source)
      source->close(source);
    if (lease) {
      lc_error ignored{};
      if (lease->release(lease, nullptr, &ignored) != LC_OK)
        lease->close(lease);
      lc_error_cleanup(&ignored);
    }
    throw;
  }
}
bool store::get(const std::string &key, byte_sink output) {
  struct target {
    byte_sink &output;
    std::exception_ptr error;
  } t{output, {}};
  lc_sink sink{};
  sink.impl = &t;
  sink.write = [](lc_sink *s, const void *p, size_t n, lc_error *) {
    auto &t = *static_cast<target *>(s->impl);
    try {
      t.output(p, n);
      return 1;
    } catch (...) {
      t.error = std::current_exception();
      return 0;
    }
  };
  lc_get_res result{};
  lc_error error{};
  const int rc = client->get_in_namespace(client, space(key), key.c_str(),
                                          nullptr, &sink, &result, &error);
  const bool found = !result.no_content;
  lc_get_res_cleanup(&result);
  if (t.error) {
    lc_error_cleanup(&error);
    std::rethrow_exception(t.error);
  }
  checked(rc, error);
  return found;
}
std::string store::get(const std::string &key) {
  std::string text;
  if (!get(key, [&](const void *p, size_t n) {
        if (n > 2 * 1024 * 1024 - text.size())
          fail("Pouch index record exceeds 2 MiB");
        text.append(static_cast<const char *>(p), n);
      }))
    return {};
  return text;
}
void store::remove(const std::string &key) {
  lc_error error{};
  lc_acquire_req req{};
  lc_lease *lease = nullptr;
  req.ns = space(key);
  req.key = key.c_str();
  req.owner = "libailo";
  req.ttl_seconds = 60;
  checked(client->acquire(client, &req, &lease, &error), error);
  try {
    checked(lease->remove(lease, nullptr, &error), error);
    checked(lease->release(lease, nullptr, &error), error);
    lease = nullptr;
    objects.erase(key);
  } catch (...) {
    if (lease)
      lease->close(lease);
    throw;
  }
}
std::vector<std::string> store::keys(const char *ns, const char *selector) {
  struct capture {
    std::vector<std::string> keys;
    std::string key;
    std::exception_ptr error;
  } c;
  lc_query_key_handler handler{};
  handler.begin = [](void *u, lc_error *) {
    static_cast<capture *>(u)->key.clear();
    return 1;
  };
  handler.chunk = [](void *u, const char *p, size_t n, lc_error *) {
    auto &c = *static_cast<capture *>(u);
    try {
      if (n > 256 - c.key.size())
        fail("Invalid Pouch key");
      c.key.append(p, n);
      return 1;
    } catch (...) {
      c.error = std::current_exception();
      return 0;
    }
  };
  handler.end = [](void *u, lc_error *) {
    auto &c = *static_cast<capture *>(u);
    try {
      c.keys.push_back(c.key);
      return 1;
    } catch (...) {
      c.error = std::current_exception();
      return 0;
    }
  };
  std::string cursor;
  do {
    lc_query_req req{};
    req.ns = ns;
    req.engine = "scan";
    req.selector_json = selector;
    req.limit = 256;
    req.cursor = cursor.empty() ? nullptr : cursor.c_str();
    lc_query_res result{};
    lc_error error{};
    const int rc =
        client->query_keys(client, &req, &handler, &c, &result, &error);
    cursor = result.cursor ? result.cursor : "";
    lc_query_res_cleanup(&result);
    if (c.error) {
      lc_error_cleanup(&error);
      std::rethrow_exception(c.error);
    }
    checked(rc, error);
  } while (!cursor.empty());
  return c.keys;
}
void store::read(const blob &b, byte_sink sink) {
  size_t total = 0;
  for (const auto &key : b.chunks) {
    const auto text = get(key);
    if (text.empty())
      fail("Pouch payload chunk is missing");
    const auto encoded = parse(text, 65536).at("data").str();
    unsigned char bytes[chunk_size];
    size_t n = 0;
    if (lonejson_base64_decode(encoded.data(), encoded.size(),
                               LONEJSON_BASE64_STANDARD, bytes, sizeof(bytes),
                               &n, nullptr) != LONEJSON_STATUS_OK ||
        n > b.size - total)
      fail("Invalid Pouch payload chunk");
    sink(bytes, n);
    total += n;
  }
  if (total != b.size)
    fail("Incomplete Pouch payload");
}
std::shared_ptr<record> store::find(const char *ns, int protocol,
                                    const char *id, uint64_t now) {
  auto p = records.find(alias(ns, protocol, id));
  if (p == records.end() || (p->second->expires && p->second->expires <= now))
    fail("Stored record was not found", AILO_NOT_FOUND);
  return p->second;
}
void store::publish(const std::vector<std::shared_ptr<record>> &changed,
                    const std::vector<std::string> &removed) {
  const auto marker = unique("c-");
  // Prepare every potentially allocating in-memory update before publication.
  auto next = records;
  std::vector<std::pair<std::string, value>> aliases;
  for (const auto &r : changed) {
    r->descriptor = unique("d-");
    r->marker = marker;
    r->alias = alias(r->ns, r->protocol, r->id);
    put(r->descriptor, encode(describe(*r)));
    value a;
    a.fields["kind"] = value(std::string("alias"));
    a.fields["current"] = value(r->descriptor);
    a.fields["commit"] = value(marker);
    a.fields["previous"] =
        value(records.count(r->alias) ? records.at(r->alias)->descriptor : "");
    aliases.emplace_back(r->alias, std::move(a));
    next[r->alias] = r;
    pin(r);
  }
  for (const auto &key : removed) {
    value a;
    a.fields["kind"] = value(std::string("alias"));
    a.fields["current"] = value(std::string());
    a.fields["commit"] = value(marker);
    a.fields["previous"] =
        value(records.count(key) ? records.at(key)->descriptor : "");
    aliases.emplace_back(key, std::move(a));
    next.erase(key);
  }
  for (const auto &a : aliases)
    put(a.first, encode(a.second));
  // One durable KV publication governs all response/conversation/item aliases.
  put(marker, "{\"kind\":\"commit\"}");
  records.swap(next);
}
store::store(std::string path, std::string key)
    : root(std::move(path)), key_file(std::move(key)) {
  directory(root);
  directory(fs::path(key_file).parent_path().string());
  if (!fs::exists(key_file) && !fs::is_empty(root))
    fail("Pouch key is missing for an existing store; restore the original key "
         "file");
  try {
    initialize();
    maintenance = std::thread([this] {
      std::unique_lock<std::mutex> lock(mutex);
      while (!wake.wait_for(lock, std::chrono::seconds(60),
                            [&] { return stopping; })) {
        try {
          collect();
        } catch (...) { /* Retry maintenance after transient I/O errors. */
        }
      }
    });
  } catch (...) {
    if (client) {
      client->close(client);
      client = nullptr;
    }
    throw;
  }
}
void store::initialize() {
  lc_error error{};
  char *raw_endpoint = nullptr;
  checked(
      lc_pouch_endpoint_build(root.c_str(), nullptr, 0, &raw_endpoint, &error),
      error);
  std::unique_ptr<char, decltype(&lc_pouch_endpoint_free)> endpoint(
      raw_endpoint, lc_pouch_endpoint_free);
  const char *endpoints[] = {endpoint.get()};
  lc_pouch_settings settings{};
  lc_pouch_settings_init(&settings);
  settings.set_mask = LC_POUCH_SETTING_DURABLE_SYNC |
                      LC_POUCH_SETTING_CRYPTO_KEY_FILE |
                      LC_POUCH_SETTING_CRYPTO_GENERATE_KEY_FILE |
                      LC_POUCH_SETTING_QUERY_INDEXING;
  settings.durable_sync = 1;
  settings.crypto_key_file = key_file.c_str();
  settings.crypto_generate_key_file = 1;
  settings.query_indexing_enabled = 0;
  lc_client_config config{};
  lc_client_config_init(&config);
  config.endpoints = endpoints;
  config.endpoint_count = 1;
  config.pouch_settings = &settings;
  config.default_namespace = "ailo-index";
  checked(lc_client_open(&config, &client, &error), error);
  // No selector means Pouch enumerates metadata without opening payloads or
  // allocating query-parser state on borrowed application/Lua threads.
  for (const auto &key : keys("ailo-index", nullptr))
    objects.insert(key);
  for (const auto &key : keys("ailo-data", nullptr))
    objects.insert(key);
  for (const auto &key : objects)
    if (key.compare(0, 2, "a-") == 0) {
      const auto text = get(key);
      if (text.empty())
        continue; // An interrupted first put has no published body.
      const auto a = parse(text);
      const auto mark = a.at("commit").str();
      const bool committed = !get(mark).empty();
      const auto desc = a.at(committed ? "current" : "previous").str();
      if (desc.empty())
        continue;
      const auto descriptor = get(desc);
      if (descriptor.empty())
        fail("Pouch record descriptor is missing");
      auto r = describe(parse(descriptor));
      r->alias = key;
      r->descriptor = desc;
      r->marker = committed ? mark : "";
      if (alias(r->ns, r->protocol, r->id) != key)
        fail("Pouch record identity does not match its index");
      records.emplace(key, r);
      pin(r);
    }
  collect();
}
store::~store() {
  {
    std::lock_guard<std::mutex> lock(mutex);
    stopping = true;
    wake.notify_all();
  }
  if (maintenance.joinable())
    maintenance.join();
  if (client)
    client->close(client);
}
void store::collect() {
  std::vector<std::string> expired;
  const auto now = api_now();
  for (const auto &p : records)
    if (p.second->expires && p.second->expires <= now &&
        !reservations.count(p.first))
      expired.push_back(p.first);
  if (!expired.empty())
    publish({}, expired);
  std::set<std::string> live = active_objects;
  for (const auto &p : records)
    live.insert(p.first);
  auto keep_blob = [&](const blob &b) {
    live.insert(b.chunks.begin(), b.chunks.end());
  };
  for (auto it = versions.begin(); it != versions.end();) {
    if (auto r = it->lock()) {
      live.insert(r->descriptor);
      if (!r->marker.empty())
        live.insert(r->marker);
      for (const auto &p : r->parts)
        keep_blob(p.second);
      for (const auto &p : r->pages)
        for (const auto &row : p.second) {
          keep_blob(row.wire);
          keep_blob(row.canonical);
        }
      ++it;
    } else
      it = versions.erase(it);
  }
  // Remove unused aliases first. No crash may make a deletion marker disappear
  // while its alias can still fall back to the previous descriptor.
  auto all = objects;
  for (const auto &key : all)
    if (key.compare(0, 2, "a-") == 0 && !live.count(key))
      remove(key);
  for (const auto &key : objects)
    if (key.compare(0, 2, "a-") == 0) {
      const auto a = parse(get(key));
      live.insert(a.at("commit").str());
      // An interrupted publication still resolves through its previous value.
      if (get(a.at("commit").str()).empty())
        live.insert(a.at("previous").str());
    }
  all = objects;
  for (const auto &key : all)
    if (!live.count(key) && !reservations.count(key))
      remove(key);
}
std::shared_ptr<record> fresh(const ailo_record_metadata &m) {
  auto r = std::make_shared<record>();
  r->ns = m.namespace_id;
  r->id = m.id;
  r->protocol = m.protocol;
  r->created = detail::to_native(m.created_at);
  r->expires = detail::to_native(m.expires_at);
  return r;
}
void complete(transaction &tx) {
  auto &s = *tx.owner;
  for (const auto &p : tx.parts) {
    auto b = p.second->finish();
    if (!tx.conversation)
      tx.result->parts[p.first] = std::move(b);
    else if (p.first == AILO_RECORD_CONVERSATION_STATE)
      tx.result->parts[AILO_RECORD_INPUT] = std::move(b);
    else if (p.first == AILO_RECORD_OUTPUT && !tx.response)
      tx.result->parts[p.first] = std::move(b);
  }
  std::vector<std::shared_ptr<record>> changed;
  std::vector<std::string> removed;
  if (tx.conversation) {
    auto &r = *tx.result;
    if (tx.response) {
      tx.response->parts.clear();
      for (const auto &p : tx.parts)
        if (p.first != AILO_RECORD_CONVERSATION_STATE &&
            p.first != AILO_RECORD_CONVERSATION_ITEMS)
          tx.response->parts[p.first] = p.second->result;
      // Response OUTPUT belongs to the response, never the conversation header.
      if (tx.base)
        r.parts[AILO_RECORD_OUTPUT] = tx.base->parts.at(AILO_RECORD_OUTPUT);
      changed.push_back(tx.response);
    }
    if (!tx.removed.empty())
      removed.push_back(alias(r.ns, item_protocol, tx.removed));
    auto found = tx.parts.find(AILO_RECORD_CONVERSATION_ITEMS);
    r.ordinal = tx.base ? tx.base->ordinal : 0;
    std::set<std::string> new_ids;
    if (found != tx.parts.end())
      for (auto &item : rows(s, found->second->result, true, tx.objects)) {
        auto child = std::make_shared<record>();
        child->ns = r.ns;
        child->id = item.id;
        child->parent = r.id;
        child->protocol = item_protocol;
        child->ordinal = ++r.ordinal;
        const auto key = alias(r.ns, item_protocol, item.id);
        if (s.records.count(key) || !new_ids.insert(key).second)
          fail("Conversation item IDs must be unique within the namespace",
               AILO_INVALID_ARGUMENT);
        if (s.reservations.count(key) && !tx.held.count(key))
          fail("Conversation item ID is reserved", AILO_BUSY);
        child->parts[AILO_RECORD_OUTPUT] = item.wire;
        if (item.canonical.size)
          child->parts[AILO_RECORD_INPUT] = item.canonical;
        changed.push_back(std::move(child));
      }
    r.revision = tx.base ? tx.base->revision + 1 : 1;
  }
  changed.push_back(tx.result);
  for (auto &r : changed)
    if (r->protocol == AILO_RECORD_CHAT ||
        r->protocol == AILO_RECORD_RESPONSE) {
      r->metadata.clear();
      attributes(s, *r);
      for (auto part :
           {AILO_RECORD_CHAT_MESSAGES, AILO_RECORD_RESPONSE_INPUT_ITEMS})
        if (r->parts.count(part))
          r->pages[part] = rows(s, r->parts.at(part), false, tx.objects);
    }
  // Check final retained size too: metadata edits cannot bypass the quota by
  // retaining untouched sections from their previous version.
  for (const auto &r : changed)
    if (r->protocol != item_protocol) {
      size_t n = 0;
      for (const auto &p : r->parts) {
        if (p.second.size > tx.maximum - n)
          fail("Stored record exceeds its byte limit", AILO_OUT_OF_RANGE);
        n += p.second.size;
      }
    }
  s.publish(changed, removed);
}
struct provider {
  std::string root, key_file;
  std::shared_ptr<store> instance;
  std::mutex mutex;
  explicit provider(const ailo_openai_config &c)
      : root(c.state_directory ? absolute(c.state_directory) : ""),
        key_file(c.encryption_key_file ? absolute(c.encryption_key_file) : "") {
  }
  std::shared_ptr<store> get() {
    std::lock_guard<std::mutex> lock(mutex);
    if (instance)
      return instance;
    if (root.empty())
      root = xdg("XDG_STATE_HOME", "/.local/state");
    if (key_file.empty())
      key_file = xdg("XDG_CONFIG_HOME", "/.config") + "/pouch.key";
    static std::mutex registry_mutex;
    static std::map<std::string, std::weak_ptr<store>> registry;
    std::lock_guard<std::mutex> guard(registry_mutex);
    directory(root);
    root = fs::canonical(root).string();
    if (auto existing = registry[root].lock()) {
      if (existing->key_file != key_file)
        fail("Pouch root is already open with a different key file",
             AILO_INVALID_ARGUMENT);
      return instance = std::move(existing);
    }
    instance = std::make_shared<store>(root, key_file);
    registry[root] = instance;
    return instance;
  }
};
template <class F> ailo_status boundary(ailo_error *error, F work) noexcept {
  try {
    work();
    return AILO_OK;
  } catch (const failure &f) {
    return ailo_error_set(error, f.status, f.what());
  } catch (const std::bad_alloc &) {
    return ailo_error_set(error, AILO_NO_MEMORY,
                          "Pouch repository allocation failed");
  } catch (...) {
    return ailo_error_set(error, AILO_IO_FAILED,
                          "Pouch repository operation failed");
  }
}
std::unique_ptr<transaction> begin(std::shared_ptr<store> s, size_t size) {
  auto tx = std::make_unique<transaction>(std::move(s), 0);
  tx->maximum = size;
  // Destructor only releases admitted bytes.
  try {
    tx->admit();
  } catch (...) {
    tx->maximum = 0;
    throw;
  }
  return tx;
}
void select(reader &out, const std::vector<row> &rows, const char *after,
            size_t limit, bool descending, bool canonical = false,
            const char *item = nullptr) {
  bool ready = !after;
  auto add = [&](const row &r) {
    if (item && r.id != item)
      return;
    const auto &b = canonical ? r.canonical : r.wire;
    if (canonical && !b.size)
      return;
    if (!ready) {
      if (r.id == after)
        ready = true;
      return;
    }
    if (!limit || out.selected.size() < limit + 1)
      out.selected.push_back(b);
  };
  if (descending)
    for (auto it = rows.rbegin(); it != rows.rend(); ++it)
      add(*it);
  else
    for (const auto &r : rows)
      add(r);
  if (!ready || (item && out.selected.empty()))
    fail("Stored query cursor or item was not found", AILO_NOT_FOUND);
  out.array = true;
}
} // namespace
} // namespace storage
using namespace storage;
std::shared_ptr<void> default_repository(const ailo_openai_config &config,
                                         ailo_record_repository &api) {
  auto p = std::make_shared<provider>(config);
  api.user_data = p.get();
  api.reserve = [](void *u, const ailo_record_metadata *m, void **result,
                   ailo_error *e) {
    return storage::boundary(e, [&] {
      auto s = static_cast<provider *>(u)->get();
      std::lock_guard<std::mutex> lock(s->mutex);
      auto tx = begin(s, m->max_bytes);
      tx->result = fresh(*m);
      auto key = alias(m->namespace_id, m->protocol, m->id);
      if (s->records.count(key))
        fail("Stored response ID already exists", AILO_INVALID_ARGUMENT);
      tx->hold(key);
      *result = tx.release();
    });
  };
  api.write = [](void *, void *t, ailo_record_section part, const void *p,
                 size_t n, ailo_error *e) {
    return storage::boundary(e, [&] {
      auto &tx = *static_cast<transaction *>(t);
      std::lock_guard<std::mutex> lock(tx.owner->mutex);
      if (n > tx.maximum - tx.bytes)
        fail("Stored record exceeds its byte limit", AILO_OUT_OF_RANGE);
      auto &b = tx.parts[part];
      if (!b)
        b = std::make_unique<builder>(*tx.owner, &tx.objects);
      b->write(p, n);
      tx.bytes += n;
    });
  };
  api.commit = [](void *, void *t, ailo_error *e) {
    return storage::boundary(e, [&] {
      auto tx = static_cast<transaction *>(t);
      auto s = tx->owner;
      std::lock_guard<std::mutex> lock(s->mutex);
      complete(*tx);
      delete tx;
    });
  };
  api.abort = [](void *, void *t) {
    if (t) {
      auto tx = static_cast<transaction *>(t);
      auto s = tx->owner;
      std::lock_guard<std::mutex> lock(s->mutex);
      delete tx;
    }
  };
  api.open = [](void *u, const char *ns, ailo_record_protocol protocol,
                const char *id, void **out, ailo_u64 *expiry, ailo_error *e) {
    return storage::boundary(e, [&] {
      auto s = static_cast<provider *>(u)->get();
      std::lock_guard<std::mutex> lock(s->mutex);
      auto r = std::make_unique<reader>();
      r->snapshot = s->find(ns, protocol, id, api_now());
      *expiry = detail::from_unsigned(r->snapshot->expires);
      *out = r.release();
    });
  };
  api.read = [](void *u, void *r, ailo_record_section part,
                ailo_output_sink sink, void *user, ailo_error *e) {
    return storage::boundary(e, [&] {
      auto s = static_cast<provider *>(u)->get();
      auto &input = *static_cast<reader *>(r);
      byte_sink output = [&](const void *p, size_t n) {
        int rc = sink(user, p, n);
        if (rc <= 0)
          fail("Stored output sink rejected bytes",
               rc ? AILO_SINK_FAILED : AILO_CANCELLED);
      };
      // Immutable chunks and pinned record versions allow slow readers without
      // holding the commit mutex or blocking independent conversations.
      if (input.array) {
        output("[", 1);
        bool comma = false;
        for (const auto &b : input.selected) {
          if (comma)
            output(",", 1);
          s->read(b, output);
          comma = true;
        }
        output("]", 1);
      } else {
        auto b = input.snapshot->parts.find(part);
        if (b == input.snapshot->parts.end())
          fail("Stored section was not found", AILO_NOT_FOUND);
        s->read(b->second, output);
      }
    });
  };
  api.close = [](void *, void *r) { delete static_cast<reader *>(r); };
  api.erase = [](void *u, const char *ns, ailo_record_protocol protocol,
                 const char *id, ailo_error *e) {
    return storage::boundary(e, [&] {
      auto s = static_cast<provider *>(u)->get();
      std::lock_guard<std::mutex> lock(s->mutex);
      s->find(ns, protocol, id, api_now());
      const auto key = alias(ns, protocol, id);
      if (s->reservations.count(key))
        fail("Stored record is busy", AILO_BUSY);
      s->publish({}, {key});
    });
  };
  api.amend = [](void *u, const ailo_conversation_change *c, void **out,
                 ailo_error *e) {
    return storage::boundary(e, [&] {
      auto s = static_cast<provider *>(u)->get();
      std::lock_guard<std::mutex> lock(s->mutex);
      auto tx = begin(s, c->max_bytes);
      const auto key = alias(c->namespace_id, AILO_RECORD_CONVERSATION, c->id);
      tx->hold(key);
      if (c->action == AILO_CONVERSATION_CREATE) {
        if (s->records.count(key))
          fail("Conversation already exists", AILO_BUSY);
        tx->result = std::make_shared<record>();
        tx->result->ns = c->namespace_id;
        tx->result->id = c->id;
        tx->result->protocol = AILO_RECORD_CONVERSATION;
      } else {
        tx->base = s->find(c->namespace_id, AILO_RECORD_CONVERSATION, c->id, 0);
        if (tx->base->revision != detail::to_native(c->revision))
          fail("Conversation revision changed", AILO_BUSY);
        tx->result = std::make_shared<record>(*tx->base);
      }
      tx->conversation = true;
      for (size_t n = 0; n < c->input_count; ++n) {
        const auto item =
            alias(c->namespace_id, item_protocol, c->input_ids[n]);
        if (s->records.count(item))
          fail("Conversation item ID already exists", AILO_INVALID_ARGUMENT);
        tx->hold(item);
      }
      if (c->item_id) {
        auto item = s->find(c->namespace_id, item_protocol, c->item_id, 0);
        if (item->parent != c->id)
          fail("Conversation item was not found", AILO_NOT_FOUND);
        tx->removed = c->item_id;
      }
      if (c->response) {
        tx->response = fresh(*c->response);
        const auto response =
            alias(c->namespace_id, AILO_RECORD_RESPONSE, c->response->id);
        if (s->records.count(response))
          fail("Response ID already exists", AILO_INVALID_ARGUMENT);
        tx->hold(response);
      }
      *out = tx.release();
    });
  };
  api.query = [](void *u, const ailo_conversation_query *q, void **out,
                 ailo_error *e) {
    return storage::boundary(e, [&] {
      auto s = static_cast<provider *>(u)->get();
      std::lock_guard<std::mutex> lock(s->mutex);
      auto r = std::make_unique<reader>();
      r->snapshot =
          s->find(q->namespace_id, AILO_RECORD_CONVERSATION, q->id, 0);
      std::vector<std::shared_ptr<record>> items;
      for (const auto &p : s->records)
        if (p.second->protocol == item_protocol &&
            p.second->ns == q->namespace_id && p.second->parent == q->id)
          items.push_back(p.second);
      std::sort(items.begin(), items.end(), [](const auto &a, const auto &b) {
        return a->ordinal < b->ordinal;
      });
      std::vector<row> rows;
      for (const auto &item : items)
        rows.push_back({item->id, item->parts.at(AILO_RECORD_OUTPUT),
                        item->parts.count(AILO_RECORD_INPUT)
                            ? item->parts.at(AILO_RECORD_INPUT)
                            : blob{}});
      select(*r, rows, q->after, q->limit, q->descending, q->canonical,
             q->item_id);
      r->pins = std::move(items);
      *out = r.release();
    });
  };
  api.scan = [](void *u, const ailo_record_query *q, void **out,
                ailo_u64 *expiry, ailo_error *e) {
    return storage::boundary(e, [&] {
      auto s = static_cast<provider *>(u)->get();
      std::lock_guard<std::mutex> lock(s->mutex);
      auto r = std::make_unique<reader>();
      std::vector<row> rows;
      *expiry = {};
      if (q->id) {
        r->snapshot = s->find(q->namespace_id, q->protocol, q->id,
                              detail::to_native(q->now));
        *expiry = detail::from_unsigned(r->snapshot->expires);
        auto page = r->snapshot->pages.find(q->section);
        if (page != r->snapshot->pages.end())
          rows = page->second;
      } else {
        std::vector<std::shared_ptr<record>> sorted;
        for (const auto &p : s->records) {
          const auto &v = *p.second;
          if (v.ns != q->namespace_id || v.protocol != q->protocol ||
              (v.expires && v.expires <= detail::to_native(q->now)) ||
              (q->model && v.model != q->model))
            continue;
          bool matches = true;
          for (size_t n = 0; n < q->metadata_count; ++n) {
            const auto &f = q->metadata[n];
            auto field = v.metadata.find(std::string(f.key, f.key_size));
            matches &= field != v.metadata.end() &&
                       field->second == std::string(f.value, f.value_size);
          }
          if (matches)
            sorted.push_back(p.second);
        }
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto &a, const auto &b) {
                    return std::make_pair(a->created, a->id) <
                           std::make_pair(b->created, b->id);
                  });
        for (const auto &p : sorted)
          rows.push_back({p->id, p->parts.at(AILO_RECORD_OUTPUT), {}});
        r->pins = std::move(sorted);
      }
      select(*r, rows, q->after, q->limit, q->descending);
      *out = r.release();
    });
  };
  api.edit = [](void *u, const ailo_record_edit *edit, void **out,
                void **snapshot, ailo_u64 *expiry, ailo_error *e) {
    return storage::boundary(e, [&] {
      auto s = static_cast<provider *>(u)->get();
      std::lock_guard<std::mutex> lock(s->mutex);
      auto tx = begin(s, edit->max_bytes);
      tx->base = s->find(edit->namespace_id, edit->protocol, edit->id,
                         detail::to_native(edit->now));
      tx->hold(alias(edit->namespace_id, edit->protocol, edit->id));
      tx->result = std::make_shared<record>(*tx->base);
      auto r = std::make_unique<reader>();
      r->snapshot = tx->base;
      *expiry = detail::from_unsigned(tx->base->expires);
      *snapshot = r.release();
      *out = tx.release();
    });
  };
  return p;
}
} // namespace client
} // namespace ailo
