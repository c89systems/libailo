#ifndef AILO_STORE_INTERNAL_HPP
#define AILO_STORE_INTERNAL_HPP
#include "ailo_json_stream_internal.hpp"
#include "ailo_openai_internal.hpp"
#include <map>
#include <string_view>

namespace ailo {
namespace client {
// Only bounded storage metadata is materialized. Payloads use fixed-size
// immutable chunks; neither this index nor readers contain response bodies.
namespace storage {
struct value {
  enum type { object, array, string, number, boolean, null } kind = object;
  std::map<std::string, value> fields;
  std::vector<value> elements;
  std::string text;
  value() = default;
  explicit value(std::string s) : kind(string), text(std::move(s)) {}
  explicit value(uint64_t n) : kind(number), text(std::to_string(n)) {}
  static value list() {
    value v;
    v.kind = array;
    return v;
  }
  const value &at(const std::string &) const;
  std::string str() const;
  uint64_t integer() const;
};
using path = std::vector<std::string>;
enum class event {
  object,
  object_end,
  array,
  array_end,
  key,
  string,
  string_chunk,
  string_end,
  number,
  boolean,
  null
};
using event_sink = std::function<void(event, const path &, std::string_view)>;
class events {
  event_sink sink;
  std::string token;
  lonejson_path_value_visitor visitor{};
  std::unique_ptr<json_stream> parser;
  void send(event, const lonejson_value_path *, std::string_view = {});

public:
  events(event_sink, size_t);
  void push(const void *, size_t);
  void finish();
};
value parse(const std::string &, size_t maximum = 2 * 1024 * 1024);
std::string encode(const value &, size_t maximum = 2 * 1024 * 1024);
void forward(wire_json &, event, std::string_view);
struct blob {
  std::vector<std::string> chunks;
  size_t size = 0;
};
struct row {
  std::string id;
  blob wire, canonical;
};
struct record {
  std::string ns, id, parent, model, descriptor, marker, alias;
  int protocol = 0;
  uint64_t created = 0, expires = 0, revision = 0, ordinal = 0;
  std::map<int, blob> parts;
  std::map<int, std::vector<row>> pages;
  std::map<std::string, std::string> metadata;
};
value describe(const record &);
std::shared_ptr<record> describe(const value &);
} // namespace storage
std::shared_ptr<void> default_repository(const ailo_openai_config &,
                                         ailo_record_repository &);
} // namespace client
} // namespace ailo
#endif
