#include "ailo_json_stream_internal.hpp"
#include "ailo_openai_internal.hpp"
#include <cstring>
#include <set>
namespace ailo {
namespace client {
namespace {
bool equal(const lonejson_path_segment &part, const char *s) {
  return part.len == std::strlen(s) && !std::memcmp(part.data, s, part.len);
}
[[noreturn]] void invalid() {
  throw failure(AILO_IO_FAILED, "Invalid conversation item query result");
}
struct projection {
  wire_json &out;
  const openai_request &options;
  const char *expected;
  size_t rows = 0, current = 0;
  std::string key, number, id, first, last, type;
  std::set<std::string> ids;
  bool root = false, ended = false;
  bool diagnostics() const {
    return options.protocol == openai_protocol::conversation_query ||
           options.protocol == openai_protocol::response_input_items;
  }
  size_t limit() const { return expected ? 1 : options.page_limit; }
  bool emit(const lonejson_value_path *p) const {
    if (!p->segment_count || current >= limit())
      return false;
    for (size_t n = 1; n < p->segment_count; ++n) {
      if (diagnostics() &&
          ((!options.include_reasoning && type == "reasoning" &&
            equal(p->segments[n], "encrypted_content")) ||
           (!options.generation.logprobs && equal(p->segments[n], "logprobs"))))
        return false;
    }
    return true;
  }
  static lonejson_status object_begin(void *u, const lonejson_value_path *p,
                                      lonejson_error *) {
    auto &s = *static_cast<projection *>(u);
    if (!p->segment_count)
      invalid();
    if (p->segment_count == 1) {
      current_row(s, p);
      s.id.clear();
      s.type.clear();
    }
    if (s.emit(p))
      s.out.object();
    return LONEJSON_STATUS_OK;
  }
  static void current_row(projection &s, const lonejson_value_path *p) {
    if (!s.root || s.ended ||
        !equal(p->segments[0], std::to_string(s.rows).c_str()) ||
        s.rows >= s.limit() + (s.expected ? 0 : 1))
      invalid();
    s.current = s.rows++;
  }
  static lonejson_status object_end(void *u, const lonejson_value_path *p,
                                    lonejson_error *) {
    auto &s = *static_cast<projection *>(u);
    if (s.emit(p))
      s.out.end_object();
    if (p->segment_count == 1) {
      if (s.id.empty() || !s.ids.insert(s.id).second ||
          (s.expected && s.id != s.expected))
        invalid();
      if (s.current < s.limit()) {
        if (s.first.empty())
          s.first = s.id;
        s.last = s.id;
      }
    }
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status array_begin(void *u, const lonejson_value_path *p,
                                     lonejson_error *) {
    auto &s = *static_cast<projection *>(u);
    if (!p->segment_count) {
      if (s.root)
        invalid();
      s.root = true;
      if (!s.expected) {
        s.out.object();
        s.out.key("object");
        s.out.string("list");
        s.out.key("data");
        s.out.array();
      }
    } else {
      if (p->segment_count == 1)
        invalid();
      if (s.emit(p))
        s.out.array();
    }
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status array_end(void *u, const lonejson_value_path *p,
                                   lonejson_error *) {
    auto &s = *static_cast<projection *>(u);
    if (!p->segment_count)
      s.ended = true;
    else if (s.emit(p))
      s.out.end_array();
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status key_begin(void *u, const lonejson_value_path *,
                                   lonejson_error *) {
    static_cast<projection *>(u)->key.clear();
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status key_chunk(void *u, const lonejson_value_path *,
                                   const char *p, size_t n, lonejson_error *) {
    auto &s = *static_cast<projection *>(u);
    if (n > 1024 - s.key.size())
      invalid();
    s.key.append(p, n);
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status key_end(void *u, const lonejson_value_path *p,
                                 lonejson_error *) {
    auto &s = *static_cast<projection *>(u);
    if (s.diagnostics() && !s.options.include_reasoning &&
        p->segment_count == 1 && s.key == "encrypted_content" && s.type.empty())
      invalid();
    if (s.emit(p) &&
        (!s.diagnostics() || s.options.include_reasoning ||
         s.type != "reasoning" || s.key != "encrypted_content") &&
        (!s.diagnostics() || s.options.generation.logprobs ||
         s.key != "logprobs"))
      s.out.key(s.key);
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status string_begin(void *u, const lonejson_value_path *p,
                                      lonejson_error *) {
    auto &s = *static_cast<projection *>(u);
    if (p->segment_count < 2)
      invalid();
    if (s.emit(p))
      s.out.string_begin();
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status string_chunk(void *u, const lonejson_value_path *p,
                                      const char *bytes, size_t n,
                                      lonejson_error *) {
    auto &s = *static_cast<projection *>(u);
    if (p->segment_count == 2 && equal(p->segments[1], "id")) {
      if (n > 256 - s.id.size())
        invalid();
      s.id.append(bytes, n);
    }
    if (p->segment_count == 2 && equal(p->segments[1], "type")) {
      if (n > 256 - s.type.size())
        invalid();
      s.type.append(bytes, n);
    }
    if (s.emit(p))
      s.out.string_chunk(bytes, n);
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status string_end(void *u, const lonejson_value_path *p,
                                    lonejson_error *) {
    auto &s = *static_cast<projection *>(u);
    if (s.emit(p))
      s.out.string_end();
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status number_begin(void *u, const lonejson_value_path *p,
                                      lonejson_error *) {
    if (p->segment_count < 2)
      invalid();
    static_cast<projection *>(u)->number.clear();
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status number_chunk(void *u, const lonejson_value_path *,
                                      const char *p, size_t n,
                                      lonejson_error *) {
    auto &s = *static_cast<projection *>(u);
    if (n > 1024 - s.number.size())
      invalid();
    s.number.append(p, n);
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status number_end(void *u, const lonejson_value_path *p,
                                    lonejson_error *) {
    auto &s = *static_cast<projection *>(u);
    if (s.emit(p))
      s.out.number_text(s.number);
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status boolean(void *u, const lonejson_value_path *p, int v,
                                 lonejson_error *) {
    auto &s = *static_cast<projection *>(u);
    if (p->segment_count < 2)
      invalid();
    if (s.emit(p))
      s.out.boolean(v != 0);
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status null(void *u, const lonejson_value_path *p,
                              lonejson_error *) {
    auto &s = *static_cast<projection *>(u);
    if (p->segment_count < 2)
      invalid();
    if (s.emit(p))
      s.out.null();
    return LONEJSON_STATUS_OK;
  }
  lonejson_path_value_visitor visitor() {
    lonejson_path_value_visitor v{};
    v.object_begin = object_begin;
    v.object_end = object_end;
    v.array_begin = array_begin;
    v.array_end = array_end;
    v.object_key_begin = key_begin;
    v.object_key_chunk = key_chunk;
    v.object_key_end = key_end;
    v.string_begin = string_begin;
    v.string_chunk = string_chunk;
    v.string_end = string_end;
    v.number_begin = number_begin;
    v.number_chunk = number_chunk;
    v.number_end = number_end;
    v.boolean_value = boolean;
    v.null_value = null;
    return v;
  }
  void finish() {
    if (!root || !ended)
      invalid();
    if (expected) {
      if (!rows)
        throw failure(AILO_NOT_FOUND, "Conversation item was not found");
    } else {
      out.end_array();
      for (auto p :
           {std::pair<const char *, const std::string &>{"first_id", first},
            {"last_id", last}}) {
        out.key(p.first);
        if (p.second.empty())
          out.null();
        else
          out.string(p.second);
      }
      out.key("has_more");
      out.boolean(rows > limit());
      out.end_object();
    }
    out.finish();
  }
};
} // namespace
void api_project_items(const openai_request &options, const char *item,
                       byte_sink sink, size_t limit,
                       const std::function<void(byte_sink)> &produce) {
  wire_json out(std::move(sink), limit);
  projection project{out, options, item, 0,  0,  {},    {},
                     {},  {},      {},   {}, {}, false, false};
  json_stream parser(project.visitor(), &project, limit);
  try {
    produce([&](const void *p, size_t n) {
      parser.push(static_cast<const char *>(p), n);
    });
    parser.finish();
    project.finish();
  } catch (const failure &e) {
    if (e.status == AILO_MODEL_FAILED)
      throw failure(AILO_IO_FAILED, "Repository returned invalid item JSON");
    throw;
  }
}
void api_conversation_query_output(state &self, const char *id,
                                   const char *item, const void *json,
                                   size_t size, ailo_output_sink sink,
                                   void *user, ailo_api_info &info) {
  openai_request options(openai_protocol::conversation_query,
                         json ? json : "{}", json ? size : 2, self.id);
  ailo_conversation_query query{self.openai->policy.namespace_id.c_str(),
                                id,
                                item,
                                options.after.empty() ? nullptr
                                                      : options.after.c_str(),
                                item ? 1 : options.page_limit,
                                options.descending,
                                0};
  api_project_items(options, item, api_sink(self, sink, user, info),
                    self.openai->record_limit, [&](byte_sink output) {
                      query_conversation(*self.openai, query,
                                         std::move(output));
                    });
  info.stored = 1;
}
} // namespace client
} // namespace ailo
