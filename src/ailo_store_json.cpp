#include "ailo_store_internal.hpp"
#include <charconv>

namespace ailo {
namespace client {
namespace storage {
namespace {
[[noreturn]] void invalid() {
  throw failure(AILO_IO_FAILED, "Invalid Pouch repository metadata");
}
path location(const lonejson_value_path *p) {
  path result;
  for (size_t n = 0; n < p->segment_count; ++n)
    result.emplace_back(p->segments[n].data, p->segments[n].len);
  return result;
}
void write(wire_json &out, const value &v) {
  switch (v.kind) {
  case value::object:
    out.object();
    for (const auto &f : v.fields) {
      out.key(f.first);
      write(out, f.second);
    }
    out.end_object();
    break;
  case value::array:
    out.array();
    for (const auto &x : v.elements)
      write(out, x);
    out.end_array();
    break;
  case value::string:
    out.string(v.text);
    break;
  case value::number:
    out.number_text(v.text);
    break;
  case value::boolean:
    out.boolean(v.text == "true");
    break;
  case value::null:
    out.null();
    break;
  }
}
value pack(const blob &b) {
  value v;
  v.fields["size"] = value(uint64_t(b.size));
  auto &chunks = v.fields["chunks"];
  chunks = value::list();
  for (const auto &key : b.chunks)
    chunks.elements.emplace_back(key);
  return v;
}
blob unpack(const value &v) {
  blob b;
  b.size = v.at("size").integer();
  if (v.at("chunks").kind != value::array)
    invalid();
  for (const auto &key : v.at("chunks").elements)
    b.chunks.push_back(key.str());
  return b;
}
} // namespace
const value &value::at(const std::string &key) const {
  if (kind != object || !fields.count(key))
    invalid();
  return fields.at(key);
}
std::string value::str() const {
  if (kind != string)
    invalid();
  return text;
}
uint64_t value::integer() const {
  if (kind != number)
    invalid();
  uint64_t n = 0;
  auto r = std::from_chars(text.data(), text.data() + text.size(), n);
  if (r.ec != std::errc{} || r.ptr != text.data() + text.size())
    invalid();
  return n;
}
void events::send(event e, const lonejson_value_path *p, std::string_view v) {
  sink(e, location(p), v);
}
events::events(event_sink s, size_t limit) : sink(std::move(s)) {
#define BEGIN(member, kind)                                                    \
  visitor.member = [](void *u, const lonejson_value_path *p,                   \
                      lonejson_error *) {                                      \
    static_cast<events *>(u)->send(event::kind, p);                            \
    return LONEJSON_STATUS_OK;                                                 \
  }
  BEGIN(object_begin, object);
  BEGIN(object_end, object_end);
  BEGIN(array_begin, array);
  BEGIN(array_end, array_end);
  BEGIN(string_begin, string);
  BEGIN(string_end, string_end);
  BEGIN(null_value, null);
#undef BEGIN
  visitor.string_chunk = [](void *u, const lonejson_value_path *p,
                            const char *v, size_t n, lonejson_error *) {
    static_cast<events *>(u)->send(event::string_chunk, p, {v, n});
    return LONEJSON_STATUS_OK;
  };
  visitor.object_key_begin = visitor.number_begin =
      [](void *u, const lonejson_value_path *, lonejson_error *) {
        static_cast<events *>(u)->token.clear();
        return LONEJSON_STATUS_OK;
      };
  visitor.object_key_chunk =
      visitor.number_chunk = [](void *u, const lonejson_value_path *,
                                const char *v, size_t n, lonejson_error *) {
        auto &s = *static_cast<events *>(u);
        if (n > 1024 - s.token.size())
          invalid();
        s.token.append(v, n);
        return LONEJSON_STATUS_OK;
      };
  visitor.object_key_end = [](void *u, const lonejson_value_path *p,
                              lonejson_error *) {
    auto &s = *static_cast<events *>(u);
    s.send(event::key, p, s.token);
    return LONEJSON_STATUS_OK;
  };
  visitor.number_end = [](void *u, const lonejson_value_path *p,
                          lonejson_error *) {
    auto &s = *static_cast<events *>(u);
    s.send(event::number, p, s.token);
    return LONEJSON_STATUS_OK;
  };
  visitor.boolean_value = [](void *u, const lonejson_value_path *p, int v,
                             lonejson_error *) {
    static_cast<events *>(u)->send(event::boolean, p, v ? "true" : "false");
    return LONEJSON_STATUS_OK;
  };
  parser = std::make_unique<json_stream>(visitor, this, limit);
}
void events::push(const void *p, size_t n) {
  parser->push(static_cast<const char *>(p), n);
}
void events::finish() { parser->finish(); }
value parse(const std::string &input, size_t maximum) {
  value result;
  value *scalar = nullptr;
  std::vector<value *> stack;
  std::string key;
  auto child = [&](value::type type) -> value * {
    value *v = &result;
    if (!stack.empty()) {
      auto &parent = *stack.back();
      if (parent.kind == value::array) {
        parent.elements.emplace_back();
        v = &parent.elements.back();
      } else {
        if (parent.fields.count(key))
          invalid();
        v = &parent.fields[key];
      }
    }
    v->kind = type;
    return v;
  };
  events reader(
      [&](event e, const path &, std::string_view text) {
        switch (e) {
        case event::object:
          stack.push_back(child(value::object));
          break;
        case event::array:
          stack.push_back(child(value::array));
          break;
        case event::object_end:
        case event::array_end:
          if (stack.empty())
            invalid();
          stack.pop_back();
          break;
        case event::key:
          key = text;
          break;
        case event::string:
          scalar = child(value::string);
          break;
        case event::string_chunk:
          scalar->text.append(text);
          break;
        case event::string_end:
          scalar = nullptr;
          break;
        case event::number:
          child(value::number)->text = text;
          break;
        case event::boolean:
          child(value::boolean)->text = text;
          break;
        case event::null:
          child(value::null);
          break;
        }
      },
      maximum);
  reader.push(input.data(), input.size());
  reader.finish();
  return result;
}
std::string encode(const value &v, size_t limit) {
  std::string text;
  wire_json out([&](const void *p,
                    size_t n) { text.append(static_cast<const char *>(p), n); },
                limit);
  write(out, v);
  out.finish();
  return text;
}
void forward(wire_json &out, event e, std::string_view v) {
  switch (e) {
  case event::object:
    out.object();
    break;
  case event::object_end:
    out.end_object();
    break;
  case event::array:
    out.array();
    break;
  case event::array_end:
    out.end_array();
    break;
  case event::key:
    out.key(std::string(v));
    break;
  case event::string:
    out.string_begin();
    break;
  case event::string_chunk:
    out.string_chunk(v.data(), v.size());
    break;
  case event::string_end:
    out.string_end();
    break;
  case event::number:
    out.number_text(std::string(v));
    break;
  case event::boolean:
    out.boolean(v == "true");
    break;
  case event::null:
    out.null();
    break;
  }
}
value describe(const record &r) {
  value v;
  v.fields["kind"] = value(std::string("record"));
  v.fields["ns"] = value(r.ns);
  v.fields["id"] = value(r.id);
  v.fields["parent"] = value(r.parent);
  v.fields["model"] = value(r.model);
  v.fields["protocol"] = value(uint64_t(r.protocol));
  v.fields["created"] = value(r.created);
  v.fields["expires"] = value(r.expires);
  v.fields["revision"] = value(r.revision);
  v.fields["ordinal"] = value(r.ordinal);
  for (const auto &p : r.parts)
    v.fields["parts"].fields[std::to_string(p.first)] = pack(p.second);
  v.fields["parts"].kind = value::object;
  for (const auto &p : r.metadata)
    v.fields["metadata"].fields[p.first] = value(p.second);
  v.fields["metadata"].kind = value::object;
  for (const auto &page : r.pages) {
    auto &items = v.fields["pages"].fields[std::to_string(page.first)];
    items = value::list();
    for (const auto &r : page.second) {
      value x;
      x.fields["id"] = value(r.id);
      x.fields["wire"] = pack(r.wire);
      x.fields["canonical"] = pack(r.canonical);
      items.elements.push_back(std::move(x));
    }
  }
  v.fields["pages"].kind = value::object;
  return v;
}
std::shared_ptr<record> describe(const value &v) {
  auto r = std::make_shared<record>();
  r->ns = v.at("ns").str();
  r->id = v.at("id").str();
  r->parent = v.at("parent").str();
  r->model = v.at("model").str();
  r->protocol = v.at("protocol").integer();
  r->created = v.at("created").integer();
  r->expires = v.at("expires").integer();
  r->revision = v.at("revision").integer();
  r->ordinal = v.at("ordinal").integer();
  for (const auto &p : v.at("parts").fields)
    r->parts.emplace(std::stoi(p.first), unpack(p.second));
  for (const auto &p : v.at("metadata").fields)
    r->metadata.emplace(p.first, p.second.str());
  for (const auto &p : v.at("pages").fields)
    for (const auto &x : p.second.elements)
      r->pages[std::stoi(p.first)].push_back(
          {x.at("id").str(), unpack(x.at("wire")), unpack(x.at("canonical"))});
  return r;
}
} // namespace storage
} // namespace client
} // namespace ailo
