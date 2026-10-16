#include "ailo_json_stream_internal.hpp"
#include "ailo_openai_internal.hpp"
#include <cstring>
#include <set>
#define AILO_PUBLIC extern "C" __attribute__((visibility("default")))
namespace ailo {
namespace client {
namespace {
void metadata(wire_json &out, const openai_request &request) {
  out.object();
  for (const auto &p : request.metadata) {
    out.key(p.first);
    out.string(p.second);
  }
  out.end_object();
}
void required(openai_state &owner, const char *id) {
  if (!owner.policy.repository_readable || owner.policy.namespace_id.empty())
    throw failure(AILO_UNSUPPORTED, "A retained-record repository is required");
  if (id && (!*id || strnlen(id, 257) > 256))
    throw failure(AILO_INVALID_ARGUMENT, "A retained record ID is required");
  if (id)
    require_utf8(id, std::strlen(id));
}
bool equal(const lonejson_path_segment &p, const char *s) {
  return p.len == std::strlen(s) && !std::memcmp(p.data, s, p.len);
}
// Copy a stored object incrementally, replacing only its top-level metadata.
// Also normalizes bounded input identity/type before preserving its content.
// Metadata updates read the same stable source after commit for the result.
struct object_projection {
  wire_json &out;
  const openai_request &request;
  const char *expected;
  std::string key, number, id;
  bool root = false, ended = false;
  const ailo_item *input = nullptr;
  bool has_status = false;
  object_projection(wire_json &output, const openai_request &input,
                    const char *identity)
      : out(output), request(input), expected(identity) {}
  bool emit(const lonejson_value_path *p) const {
    return !p->segment_count || (input ? !equal(p->segments[0], "type") &&
                                             !equal(p->segments[0], "id")
                                       : !equal(p->segments[0], "metadata"));
  }
  static lonejson_status object_begin(void *u, const lonejson_value_path *p,
                                      lonejson_error *) {
    auto &s = *static_cast<object_projection *>(u);
    if (!p->segment_count) {
      if (s.root)
        invalid();
      s.root = true;
    }
    if (s.emit(p))
      s.out.object();
    if (!p->segment_count && s.input) {
      const auto &i = *s.input;
      s.out.key("type");
      s.out.string(i.kind == AILO_ITEM_REASONING       ? "reasoning"
                   : i.kind == AILO_ITEM_FUNCTION_CALL ? "function_call"
                   : i.kind == AILO_ITEM_FUNCTION_RESULT
                       ? "function_call_output"
                   : i.kind == AILO_ITEM_COMPACTION ? "compaction"
                                                    : "message");
      s.out.key("id");
      s.out.string(i.id);
      if (!s.has_status && i.kind != AILO_ITEM_COMPACTION) {
        s.out.key("status");
        s.out.string("completed");
      }
    }
    return LONEJSON_STATUS_OK;
  }
  [[noreturn]] static void invalid() {
    throw failure(AILO_IO_FAILED, "Invalid stored completion JSON");
  }
  static lonejson_status object_end(void *u, const lonejson_value_path *p,
                                    lonejson_error *) {
    auto &s = *static_cast<object_projection *>(u);
    if (!p->segment_count) {
      if (!s.root || s.ended || (!s.input && s.id != s.expected))
        invalid();
      if (!s.input) {
        s.out.key("metadata");
        metadata(s.out, s.request);
      }
      s.ended = true;
    }
    if (s.emit(p))
      s.out.end_object();
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status array_begin(void *u, const lonejson_value_path *p,
                                     lonejson_error *) {
    auto &s = *static_cast<object_projection *>(u);
    if (!p->segment_count)
      invalid();
    if (s.emit(p))
      s.out.array();
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status array_end(void *u, const lonejson_value_path *p,
                                   lonejson_error *) {
    auto &s = *static_cast<object_projection *>(u);
    if (s.emit(p))
      s.out.end_array();
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status key_begin(void *u, const lonejson_value_path *,
                                   lonejson_error *) {
    static_cast<object_projection *>(u)->key.clear();
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status key_chunk(void *u, const lonejson_value_path *,
                                   const char *p, size_t n, lonejson_error *) {
    auto &s = *static_cast<object_projection *>(u);
    if (n > 1024 - s.key.size())
      invalid();
    s.key.append(p, n);
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status key_end(void *u, const lonejson_value_path *p,
                                 lonejson_error *) {
    auto &s = *static_cast<object_projection *>(u);
    if (s.emit(p) &&
        (p->segment_count ||
         (s.input ? s.key != "id" && s.key != "type" : s.key != "metadata")))
      s.out.key(s.key);
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status string_begin(void *u, const lonejson_value_path *p,
                                      lonejson_error *) {
    auto &s = *static_cast<object_projection *>(u);
    if (!p->segment_count)
      invalid();
    if (s.emit(p))
      s.out.string_begin();
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status string_chunk(void *u, const lonejson_value_path *p,
                                      const char *v, size_t n,
                                      lonejson_error *) {
    auto &s = *static_cast<object_projection *>(u);
    if (!s.input && p->segment_count == 1 && equal(p->segments[0], "id")) {
      if (n > 256 - s.id.size())
        invalid();
      s.id.append(v, n);
    }
    if (s.emit(p))
      s.out.string_chunk(v, n);
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status string_end(void *u, const lonejson_value_path *p,
                                    lonejson_error *) {
    auto &s = *static_cast<object_projection *>(u);
    if (s.emit(p))
      s.out.string_end();
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status number_begin(void *u, const lonejson_value_path *p,
                                      lonejson_error *) {
    if (!p->segment_count)
      invalid();
    static_cast<object_projection *>(u)->number.clear();
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status number_chunk(void *u, const lonejson_value_path *,
                                      const char *v, size_t n,
                                      lonejson_error *) {
    auto &s = *static_cast<object_projection *>(u);
    if (n > 1024 - s.number.size())
      invalid();
    s.number.append(v, n);
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status number_end(void *u, const lonejson_value_path *p,
                                    lonejson_error *) {
    auto &s = *static_cast<object_projection *>(u);
    if (s.emit(p))
      s.out.number_text(s.number);
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status boolean(void *u, const lonejson_value_path *p, int v,
                                 lonejson_error *) {
    auto &s = *static_cast<object_projection *>(u);
    if (!p->segment_count)
      invalid();
    if (s.emit(p))
      s.out.boolean(v);
    return LONEJSON_STATUS_OK;
  }
  static lonejson_status null(void *u, const lonejson_value_path *p,
                              lonejson_error *) {
    auto &s = *static_cast<object_projection *>(u);
    if (!p->segment_count)
      invalid();
    if (s.emit(p))
      s.out.null();
    return LONEJSON_STATUS_OK;
  }
  void copy(size_t limit, const std::function<void(byte_sink)> &produce) {
    auto v = lonejson_default_path_value_visitor();
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
    json_stream parser(v, this, limit);
    try {
      produce([&](const void *p, size_t n) {
        parser.push(static_cast<const char *>(p), n);
      });
      parser.finish();
      if (!root || !ended)
        invalid();
      if (!input)
        out.finish();
    } catch (const failure &e) {
      if (e.status == AILO_MODEL_FAILED)
        invalid();
      throw;
    }
  }
  void copy(openai_state &owner, void *reader) {
    copy(owner.record_limit, [&](byte_sink sink) {
      read_record_handle(owner, reader, AILO_RECORD_OUTPUT, std::move(sink));
    });
  }
};
void update(state &self, const char *id, openai_request &request,
            ailo_output_sink sink, void *user, ailo_api_info &info) {
  required(*self.openai, id);
  ailo_record_edit edit{self.openai->policy.namespace_id.c_str(),
                        AILO_RECORD_CHAT, id, self.openai->record_limit,
                        detail::from_unsigned(api_now())};
  void *reader = nullptr;
  record_transaction tx(*self.openai, edit, &reader);
  struct cleanup {
    openai_state &owner;
    void *reader;
    ~cleanup() { owner.repository.close(owner.repository.user_data, reader); }
  } guard{*self.openai, reader};
  wire_json attributes(tx.section(AILO_RECORD_METADATA),
                       self.openai->record_limit);
  metadata(attributes, request);
  attributes.finish();
  wire_json saved(tx.section(AILO_RECORD_OUTPUT), self.openai->record_limit);
  object_projection patch{saved, request, id};
  patch.copy(*self.openai, reader);
  if (self.is_cancelled())
    throw failure(AILO_CANCELLED, "Metadata update cancelled");
  tx.commit();
  info.stored = 1;
  wire_json result(api_sink(self, sink, user, info), self.openai->record_limit);
  object_projection response{result, request, id};
  response.copy(*self.openai, reader);
}
ailo_status retained(ailo_client *client, const char *id, const void *json,
                     size_t size, openai_protocol protocol,
                     ailo_output_sink sink, void *user, ailo_api_info *info,
                     ailo_error *error) {
  if (info)
    *info = {};
  if (!sink || !info) {
    if (info)
      info->http_status = 400;
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "API sink and output info are required");
  }
  const auto status = boundary(error, [&] {
    operation call(client);
    if (protocol != openai_protocol::chat_list && !id)
      throw failure(AILO_INVALID_ARGUMENT, "A retained record ID is required");
    required(*call.self.openai, id);
    openai_request request(protocol, json ? json : "{}", json ? size : 2,
                           call.self.id);
    info->content_type = "application/json";
    if (id && std::strlen(id) < sizeof(info->id))
      std::strcpy(info->id, id);
    if (protocol == openai_protocol::chat_update) {
      update(call.self, id, request, sink, user, *info);
      return;
    }
    std::vector<ailo_record_filter> filters;
    for (const auto &p : request.metadata)
      filters.push_back(
          {p.first.data(), p.first.size(), p.second.data(), p.second.size()});
    const bool response = protocol == openai_protocol::response_input_items;
    ailo_record_query query{
        call.self.openai->policy.namespace_id.c_str(),
        response ? AILO_RECORD_RESPONSE : AILO_RECORD_CHAT,
        !id        ? AILO_RECORD_OUTPUT
        : response ? AILO_RECORD_RESPONSE_INPUT_ITEMS
                   : AILO_RECORD_CHAT_MESSAGES,
        id,
        request.after.empty() ? nullptr : request.after.c_str(),
        protocol == openai_protocol::chat_list && !request.model.empty()
            ? request.model.c_str()
            : nullptr,
        filters.data(),
        filters.size(),
        request.page_limit,
        request.descending,
        detail::from_unsigned(api_now())};
    api_project_items(request, nullptr, api_sink(call.self, sink, user, *info),
                      call.self.openai->record_limit, [&](byte_sink target) {
                        query_records(*call.self.openai, query,
                                      std::move(target));
                      });
    info->stored = 1;
  });
  info->http_status = api_http_status(status);
  return status;
}
} // namespace
void api_input_wire(wire_json &out, const openai_request &request,
                    const ailo_item &item, const std::string &wire,
                    bool has_status) {
  object_projection patch(out, request, item.id);
  patch.input = &item;
  patch.has_status = has_status;
  patch.copy(8 * 1024 * 1024,
             [&](byte_sink sink) { sink(wire.data(), wire.size()); });
}
void api_response_input_page(state &self, openai_request &request,
                             record_transaction &tx) {
  openai_request original(openai_protocol::responses, request.raw_json,
                          request.raw_size, self.id, nullptr, true);
  const size_t count = original.items.size() - original.instruction_items;
  if (count > request.items.size())
    throw failure(AILO_INTERNAL_ERROR,
                  "Response input index has inconsistent history");
  const size_t first = request.items.size() - count;
  std::set<std::string> ids;
  wire_json out(tx.section(AILO_RECORD_RESPONSE_INPUT_ITEMS),
                self.openai->record_limit);
  out.array();
  for (size_t n = request.instruction_items; n < request.items.size(); ++n) {
    auto item = request.items[n];
    ailo_api_info identity{};
    const bool missing = !item.id;
    if (missing) {
      api_identity(identity, item.kind == AILO_ITEM_FUNCTION_CALL     ? "fc_"
                             : item.kind == AILO_ITEM_FUNCTION_RESULT ? "fco_"
                             : item.kind == AILO_ITEM_REASONING       ? "rs_"
                             : item.kind == AILO_ITEM_COMPACTION      ? "cmp_"
                                                                      : "msg_");
      item.id = identity.id;
    }
    if (!ids.insert(item.id).second)
      throw failure(AILO_INVALID_ARGUMENT, "Duplicate response input ID");
    const size_t raw = n - first;
    if (n >= first && raw < original.wire_items.size() &&
        !original.wire_simple[raw]) {
      api_input_wire(out, original, item, original.wire_items[raw],
                     original.wire_status[raw]);
    } else
      api_canonical_item(out, item);
  }
  if (request.compaction_trigger) {
    ailo_api_info identity{};
    api_identity(identity, "ct_");
    const auto &id = request.compaction_trigger_id.empty()
                         ? std::string(identity.id)
                         : request.compaction_trigger_id;
    if (!ids.insert(id).second)
      throw failure(AILO_INVALID_ARGUMENT, "Duplicate response input ID");
    out.object();
    out.key("type");
    out.string("compaction_trigger");
    out.key("id");
    out.string(id);
    out.end_object();
  }
  out.end_array();
  out.finish();
}
} // namespace client
} // namespace ailo
AILO_PUBLIC ailo_status ailo_client_chat_list(ailo_client *c, const void *j,
                                              size_t n, ailo_output_sink s,
                                              void *u, ailo_api_info *i,
                                              ailo_error *e) {
  return ailo::client::retained(
      c, nullptr, j, n, ailo::client::openai_protocol::chat_list, s, u, i, e);
}
AILO_PUBLIC ailo_status ailo_client_chat_update(ailo_client *c, const char *id,
                                                const void *j, size_t n,
                                                ailo_output_sink s, void *u,
                                                ailo_api_info *i,
                                                ailo_error *e) {
  return ailo::client::retained(
      c, id, j, n, ailo::client::openai_protocol::chat_update, s, u, i, e);
}
AILO_PUBLIC ailo_status ailo_client_chat_messages_list(
    ailo_client *c, const char *id, const void *j, size_t n, ailo_output_sink s,
    void *u, ailo_api_info *i, ailo_error *e) {
  return ailo::client::retained(
      c, id, j, n, ailo::client::openai_protocol::chat_messages, s, u, i, e);
}
AILO_PUBLIC ailo_status ailo_client_response_input_items_list(
    ailo_client *c, const char *id, const void *j, size_t n, ailo_output_sink s,
    void *u, ailo_api_info *i, ailo_error *e) {
  return ailo::client::retained(
      c, id, j, n, ailo::client::openai_protocol::response_input_items, s, u, i,
      e);
}
