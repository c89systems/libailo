#ifndef AILO_TEST_SERVING_REPOSITORY_HPP
#define AILO_TEST_SERVING_REPOSITORY_HPP
// Test application storage, deliberately materialized and bounded. This is not
// a product provider. The HTTP fixture is serial; each request reconstructs its
// provider and receiver from this file. Generated transport bytes never enter
// it except through the production adapter's declared record sections.
#include "ailo/openai.h"
#include "vendor/nlohmann/json.hpp"
#include <algorithm>
#include <fstream>
#include <memory>
#include <set>

namespace serving_test {
using json = nlohmann::ordered_json;
inline uint64_t integer(ailo_u64 n) { return (uint64_t(n.hi) << 32) | n.lo; }
inline ailo_u64 integer(uint64_t n) {
  return {static_cast<ailo_u32>(n >> 32), static_cast<ailo_u32>(n)};
}
inline json load(const std::string &path, json fallback = json::object()) {
  std::ifstream input(path);
  if (!input)
    return fallback;
  input.seekg(0, std::ios::end);
  if (input.tellg() > 32 * 1024 * 1024)
    throw std::runtime_error("Test storage exceeds 32 MiB");
  input.seekg(0);
  return json::parse(input);
}
inline void save(const std::string &path, const json &value) {
  std::ofstream output(path + ".tmp");
  output << value.dump();
  output.close();
  if (!output || std::rename((path + ".tmp").c_str(), path.c_str()))
    throw std::runtime_error("Cannot publish test storage");
}
struct repository {
  struct transaction {
    std::string key, response, removed;
    json value, parts = json::object();
    size_t bytes = 0, maximum = 0;
    bool conversation = false;
  };
  std::string path, fault;
  json records, stats = {{"reserved", 0},
                         {"committed", 0},
                         {"aborted", 0},
                         {"opened", 0},
                         {"closed", 0}};
  ailo_record_repository api{};
  static std::string key(const char *ns, ailo_record_protocol kind,
                         const char *id) {
    return std::string(ns) + "/" + std::to_string(kind) + "/" + id;
  }
  static std::string section(ailo_record_section n) {
    return std::to_string(n);
  }
  template <class F> static ailo_status guard(ailo_error *error, F f) noexcept {
    try {
      return f();
    } catch (const std::exception &e) {
      return ailo_error_set(error, AILO_IO_FAILED, e.what());
    }
  }
  ailo_status injected(const char *phase, ailo_error *error) const {
    return fault == phase ? ailo_error_set(error, AILO_IO_FAILED,
                                           "Private fixture storage failure")
                          : AILO_OK;
  }
  void count(const char *name) {
    stats[name] = stats[name].get<unsigned>() + 1;
  }
  json &find(const char *ns, ailo_record_protocol kind, const char *id) {
    return records.at(key(ns, kind, id));
  }
  bool exists(const char *ns, ailo_record_protocol kind, const char *id) const {
    return records.contains(key(ns, kind, id));
  }
  static json metadata(const ailo_record_metadata &m) {
    return {{"namespace", m.namespace_id},
            {"protocol", m.protocol},
            {"id", m.id},
            {"created", integer(m.created_at)},
            {"expires", integer(m.expires_at)},
            {"parts", json::object()}};
  }
  explicit repository(const std::string &directory, const std::string &failure)
      : path(directory + "/repository.json"), fault(failure),
        records(load(path)) {
    api.user_data = this;
    api.reserve = [](void *u, const ailo_record_metadata *m, void **result,
                     ailo_error *e) {
      auto &s = *static_cast<repository *>(u);
      return guard(e, [&] {
        auto tx = std::make_unique<transaction>();
        tx->key = key(m->namespace_id, m->protocol, m->id);
        tx->value = metadata(*m);
        tx->maximum = m->max_bytes;
        *result = tx.release();
        s.count("reserved");
        return s.injected("reserve", e);
      });
    };
    api.write = [](void *u, void *handle, ailo_record_section part,
                   const void *bytes, size_t size, ailo_error *e) {
      auto &s = *static_cast<repository *>(u);
      return guard(e, [&] {
        if (const auto fail = s.injected("write", e); fail != AILO_OK)
          return fail;
        auto &tx = *static_cast<transaction *>(handle);
        if (size > tx.maximum - tx.bytes)
          return AILO_OUT_OF_RANGE;
        tx.bytes += size;
        const auto name = section(part);
        if (!tx.parts.contains(name))
          tx.parts[name] = "";
        tx.parts[name].get_ref<std::string &>().append(
            static_cast<const char *>(bytes), size);
        return AILO_OK;
      });
    };
    api.commit = [](void *u, void *handle, ailo_error *e) {
      auto &s = *static_cast<repository *>(u);
      return guard(e, [&] {
        if (const auto fail = s.injected("commit", e); fail != AILO_OK)
          return fail;
        auto &tx = *static_cast<transaction *>(handle);
        auto next = s.records;
        auto value = tx.value;
        if (tx.conversation) {
          auto rows = value.value("rows", json::array());
          if (!tx.removed.empty())
            rows.erase(std::remove_if(rows.begin(), rows.end(),
                                      [&](const json &r) {
                                        return r.at("id") == tx.removed;
                                      }),
                       rows.end());
          const auto row_section = section(AILO_RECORD_CONVERSATION_ITEMS);
          if (tx.parts.contains(row_section))
            for (const auto &row :
                 json::parse(tx.parts[row_section].get<std::string>()))
              rows.push_back(row);
          value["rows"] = rows;
          value["revision"] = value.value("revision", uint64_t(0)) + 1;
          value["parts"][section(AILO_RECORD_INPUT)] =
              tx.parts.at(section(AILO_RECORD_CONVERSATION_STATE));
          if (!tx.response.empty()) {
            auto response = value.at("response");
            response["parts"] = tx.parts;
            next[tx.response] = response;
            value.erase("response");
          } else if (tx.parts.contains(section(AILO_RECORD_OUTPUT))) {
            value["parts"][section(AILO_RECORD_OUTPUT)] =
                tx.parts.at(section(AILO_RECORD_OUTPUT));
          }
        } else {
          for (const auto &part : tx.parts.items())
            value["parts"][part.key()] = part.value();
        }
        next[tx.key] = value;
        save(s.path, next);
        s.records = std::move(next);
        s.count("committed");
        delete &tx;
        return AILO_OK;
      });
    };
    api.abort = [](void *u, void *tx) {
      static_cast<repository *>(u)->count("aborted");
      delete static_cast<transaction *>(tx);
    };
    api.open = [](void *u, const char *ns, ailo_record_protocol kind,
                  const char *id, void **reader, ailo_u64 *expiry,
                  ailo_error *e) {
      auto &s = *static_cast<repository *>(u);
      return guard(e, [&] {
        if (!s.exists(ns, kind, id))
          return AILO_NOT_FOUND;
        const auto &value = s.find(ns, kind, id);
        *reader = new json(value.at("parts"));
        *expiry =
            integer(s.fault == "expire" ? uint64_t(1)
                                        : value.at("expires").get<uint64_t>());
        s.count("opened");
        return s.injected("open", e);
      });
    };
    api.read = [](void *u, void *handle, ailo_record_section part,
                  ailo_output_sink sink, void *user, ailo_error *e) {
      auto &s = *static_cast<repository *>(u);
      return guard(e, [&] {
        if (const auto fail = s.injected("read", e); fail != AILO_OK)
          return fail;
        const auto &parts = *static_cast<json *>(handle);
        const auto bytes = parts.at(section(part)).get<std::string>();
        for (size_t offset = 0; offset < bytes.size(); offset += 97) {
          const int result = sink(user, bytes.data() + offset,
                                  std::min(size_t(97), bytes.size() - offset));
          if (result <= 0)
            return result ? AILO_SINK_FAILED : AILO_CANCELLED;
        }
        return AILO_OK;
      });
    };
    api.close = [](void *u, void *handle) {
      static_cast<repository *>(u)->count("closed");
      delete static_cast<json *>(handle);
    };
    api.erase = [](void *u, const char *ns, ailo_record_protocol kind,
                   const char *id, ailo_error *e) {
      auto &s = *static_cast<repository *>(u);
      return guard(e, [&] {
        if (!s.exists(ns, kind, id))
          return AILO_NOT_FOUND;
        auto next = s.records;
        next.erase(key(ns, kind, id));
        save(s.path, next);
        s.records = std::move(next);
        return AILO_OK;
      });
    };
    api.amend = [](void *u, const ailo_conversation_change *c, void **result,
                   ailo_error *e) {
      auto &s = *static_cast<repository *>(u);
      return guard(e, [&] {
        auto tx = std::make_unique<transaction>();
        tx->key = key(c->namespace_id, AILO_RECORD_CONVERSATION, c->id);
        const bool present = s.records.contains(tx->key);
        if (c->action == AILO_CONVERSATION_CREATE
                ? present
                : present &&
                      s.records[tx->key].at("revision") != integer(c->revision))
          return AILO_BUSY;
        if (!present && c->action != AILO_CONVERSATION_CREATE)
          return AILO_NOT_FOUND;
        tx->value = present ? s.records[tx->key]
                            : json{{"expires", 0}, {"parts", json::object()}};
        std::set<std::string> ids;
        const auto prefix = std::string(c->namespace_id) + "/2/";
        for (const auto &r : s.records.items())
          if (r.key().compare(0, prefix.size(), prefix) == 0)
            for (const auto &row : r.value().value("rows", json::array()))
              ids.insert(row.at("id").get<std::string>());
        for (size_t n = 0; n < c->input_count; ++n)
          if (!ids.insert(c->input_ids[n]).second)
            return AILO_INVALID_ARGUMENT;
        if (c->item_id)
          tx->removed = c->item_id;
        if (c->action == AILO_CONVERSATION_DELETE_ITEM) {
          bool found = false;
          for (const auto &row : tx->value.value("rows", json::array()))
            found |= row.at("id") == tx->removed;
          if (!found)
            return AILO_NOT_FOUND;
        }
        if (c->response) {
          tx->response =
              key(c->namespace_id, AILO_RECORD_RESPONSE, c->response->id);
          tx->value["response"] = metadata(*c->response);
        }
        tx->maximum = c->max_bytes;
        tx->conversation = true;
        *result = tx.release();
        s.count("reserved");
        return s.injected("reserve", e);
      });
    };
    api.query = [](void *u, const ailo_conversation_query *q, void **result,
                   ailo_error *e) {
      auto &s = *static_cast<repository *>(u);
      return guard(e, [&] {
        if (!s.exists(q->namespace_id, AILO_RECORD_CONVERSATION, q->id))
          return AILO_NOT_FOUND;
        auto rows = s.find(q->namespace_id, AILO_RECORD_CONVERSATION, q->id)
                        .value("rows", json::array());
        if (q->descending)
          std::reverse(rows.begin(), rows.end());
        auto selected = json::array();
        bool start = !q->after;
        for (const auto &r : rows) {
          if (q->item_id && r.at("id") != q->item_id)
            continue;
          if (q->canonical && r.at("canonical").is_null())
            continue;
          if (start)
            selected.push_back(r.at(q->canonical ? "canonical" : "wire"));
          else if (r.at("id") == q->after)
            start = true;
          if (q->limit && selected.size() >= q->limit + 1)
            break;
        }
        if (!start)
          return AILO_NOT_FOUND;
        *result = new json{{section(AILO_RECORD_OUTPUT), selected.dump()}};
        s.count("opened");
        return AILO_OK;
      });
    };
    api.scan = [](void *u, const ailo_record_query *q, void **result,
                  ailo_u64 *expiry, ailo_error *e) {
      auto &s = *static_cast<repository *>(u);
      return guard(e, [&] {
        auto rows = json::array();
        *expiry = {};
        if (q->id) {
          if (!s.exists(q->namespace_id, q->protocol, q->id))
            return AILO_NOT_FOUND;
          const auto &r = s.find(q->namespace_id, q->protocol, q->id);
          *expiry = integer(r.at("expires").get<uint64_t>());
          rows = json::parse(
              r.at("parts").at(section(q->section)).get<std::string>());
        } else {
          for (const auto &r : s.records) {
            if (r.value("namespace", "") != q->namespace_id ||
                r.value("protocol", -1) != q->protocol ||
                (r.at("expires") != 0 && r.at("expires") <= integer(q->now)))
              continue;
            auto row = json::parse(r.at("parts")
                                       .at(section(AILO_RECORD_OUTPUT))
                                       .get<std::string>());
            auto meta = json::parse(r.at("parts")
                                        .at(section(AILO_RECORD_METADATA))
                                        .get<std::string>());
            bool matches = !q->model || row.at("model") == q->model;
            for (size_t n = 0; n < q->metadata_count; ++n) {
              const auto &f = q->metadata[n];
              matches &= meta.value(std::string(f.key, f.key_size), "") ==
                         std::string(f.value, f.value_size);
            }
            if (matches)
              rows.push_back(row);
          }
          std::sort(rows.begin(), rows.end(), [](const json &a, const json &b) {
            return std::make_pair(a.at("created").get<uint64_t>(),
                                  a.at("id").get<std::string>()) <
                   std::make_pair(b.at("created").get<uint64_t>(),
                                  b.at("id").get<std::string>());
          });
        }
        if (q->descending)
          std::reverse(rows.begin(), rows.end());
        auto selected = json::array();
        bool start = !q->after;
        for (const auto &row : rows) {
          if (start)
            selected.push_back(row);
          else if (row.at("id") == q->after)
            start = true;
          if (selected.size() >= q->limit + 1)
            break;
        }
        if (!start)
          return AILO_NOT_FOUND;
        *result = new json{{section(AILO_RECORD_OUTPUT), selected.dump()}};
        s.count("opened");
        return AILO_OK;
      });
    };
    api.edit = [](void *u, const ailo_record_edit *edit, void **result,
                  void **reader, ailo_u64 *expiry, ailo_error *e) {
      auto &s = *static_cast<repository *>(u);
      return guard(e, [&] {
        if (!s.exists(edit->namespace_id, edit->protocol, edit->id))
          return AILO_NOT_FOUND;
        auto tx = std::make_unique<transaction>();
        tx->key = key(edit->namespace_id, edit->protocol, edit->id);
        tx->value = s.records.at(tx->key);
        *expiry = integer(tx->value.at("expires").get<uint64_t>());
        tx->maximum = edit->max_bytes;
        *reader = new json(tx->value.at("parts"));
        *result = tx.release();
        s.count("opened");
        s.count("reserved");
        return s.injected("reserve", e);
      });
    };
  }
};
} // namespace serving_test
#endif
