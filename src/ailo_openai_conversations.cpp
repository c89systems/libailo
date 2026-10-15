#include "ailo_cache_internal.hpp"
#include "ailo_openai_internal.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <set>
#define AILO_PUBLIC extern "C" __attribute__((visibility("default")))
namespace ailo {
namespace client {
namespace {
void required(openai_state &owner, const char *id) {
  if (!owner.policy.conversation_writable || owner.policy.namespace_id.empty())
    throw failure(AILO_UNSUPPORTED, "A conversation repository is required");
  if (!id || !*id || strnlen(id, 257) > 256)
    throw failure(AILO_INVALID_ARGUMENT, "A conversation/item ID is required");
  require_utf8(id, std::strlen(id));
}
byte_sink collect(std::string &out, size_t limit) {
  return [&out, limit](const void *p, size_t n) {
    if (n > limit - out.size())
      throw failure(AILO_OUT_OF_RANGE,
                    "Conversation context exceeds its input budget");
    out.append(static_cast<const char *>(p), n);
  };
}
void add(transcript &out, const ailo_item &item) {
  if (out.items.size() == 4096)
    throw failure(AILO_OUT_OF_RANGE, "Conversation context exceeds 4096 items");
  out.items.push_back(std::make_shared<owned_item>(item, out.bytes,
                                                   4 * 1024 * 1024, out.parts));
}
void incoming(openai_request &request, conversation_snapshot &snapshot) {
  std::set<std::string> ids;
  for (size_t i = request.instruction_items; i < request.items.size(); ++i) {
    auto item = request.items[i];
    ailo_api_info identity{};
    const bool missing = !item.id;
    if (missing) {
      api_identity(identity, item.kind == AILO_ITEM_FUNCTION_CALL     ? "fc_"
                             : item.kind == AILO_ITEM_FUNCTION_RESULT ? "fco_"
                             : item.kind == AILO_ITEM_REASONING       ? "rs_"
                                                                      : "msg_");
      item.id = identity.id;
    }
    if (!ids.insert(item.id).second)
      throw failure(AILO_INVALID_ARGUMENT, "Duplicate conversation input ID");
    add(snapshot.incoming, item);
    const size_t row = i - request.instruction_items;
    std::string wire;
    if (row < request.wire_items.size() && !request.wire_simple[row]) {
      wire = request.wire_items[row];
      wire.pop_back();
      if (missing)
        wire += ",\"id\":\"" + std::string(item.id) + "\"";
      if (!request.wire_type[row])
        wire += ",\"type\":\"message\"";
      if (!request.wire_status[row] && item.kind != AILO_ITEM_COMPACTION)
        wire += ",\"status\":\"completed\"";
      wire += '}';
    } else {
      wire_json out([&](const void *p, size_t n) {
        wire.append(static_cast<const char *>(p), n);
      });
      api_canonical_item(out, snapshot.incoming.items.back()->view());
      out.finish();
      if (item.kind != AILO_ITEM_COMPACTION) {
        wire.pop_back();
        wire += ",\"status\":\"completed\"}";
      }
    }
    snapshot.wire_items.push_back(std::move(wire));
  }
  for (const auto &item : snapshot.incoming.items)
    snapshot.input_ids.push_back(item->id.c_str());
}
void bind(openai_request &request, transcript &&input) {
  request.input = std::move(input);
  request.items = request.input.views();
  request.generation.items = request.items.data();
  request.generation.item_count = request.items.size();
}
bool compacted(const std::vector<ailo_item> &items) {
  return std::any_of(items.begin(), items.end(), [](const ailo_item &i) {
    return i.kind == AILO_ITEM_COMPACTION;
  });
}
void state_header(wire_json &out, const conversation_snapshot &snapshot) {
  out.key("ailo_revision");
  out.number(snapshot.revision + 1);
  out.key("ailo_deleted");
  out.boolean(snapshot.deleted);
  out.key("ailo_rebuild");
  out.boolean(snapshot.rebuild);
}
ailo_conversation_change change(openai_state &owner,
                                const conversation_snapshot &s,
                                ailo_conversation_action action,
                                const char *item = nullptr) {
  return {owner.policy.namespace_id.c_str(),
          s.id.c_str(),
          action,
          detail::from_unsigned(s.revision),
          owner.record_limit,
          nullptr,
          item,
          s.input_ids.data(),
          s.input_ids.size()};
}
void write_state(state &self, record_transaction &tx, openai_state &owner,
                 const conversation_snapshot &s,
                 const std::vector<ailo_item> &items) {
  wire_json out(tx.section(AILO_RECORD_CONVERSATION_STATE), owner.record_limit);
  out.object();
  state_header(out, s);
  transcript portable;
  const bool has_compaction = compacted(items);
  if (has_compaction) {
    portable = api_compacted_window(self, items, 0);
    out.key("ailo_model_identity");
    out.string(self.conversation_identity);
  }
  out.key("input");
  out.array();
  for (const auto &i : has_compaction ? portable.views() : items)
    api_canonical_item(out, i);
  out.end_array();
  out.key("output");
  out.array();
  out.end_array();
  out.end_object();
  out.finish();
}
} // namespace
std::unique_ptr<conversation_snapshot>
api_load_conversation(openai_state &owner, const char *id, bool allow_deleted) {
  required(owner, id);
  auto s = std::make_unique<conversation_snapshot>();
  s->id = id;
  std::string header;
  read_conversation_snapshot(owner, id, collect(s->state_json, 8 * 1024 * 1024),
                             collect(header, 65536));
  openai_request state(openai_protocol::responses, s->state_json.data(),
                       s->state_json.size(), "", "__header__");
  s->revision = state.record_revision;
  s->deleted = state.record_deleted;
  s->rebuild = state.record_rebuild;
  if (!s->revision || s->revision == std::numeric_limits<uint64_t>::max())
    throw failure(AILO_OUT_OF_RANGE,
                  "Invalid or exhausted conversation revision");
  if (s->deleted && !allow_deleted)
    throw failure(AILO_NOT_FOUND, "Conversation was deleted");
  openai_request parsed(openai_protocol::responses, header.data(),
                        header.size(), "", "__header__");
  if (parsed.record_id != id)
    throw failure(AILO_IO_FAILED,
                  "Conversation repository returned a different ID");
  s->created = parsed.record_created;
  s->metadata = std::move(parsed.metadata);
  return s;
}
void api_conversation_header(wire_json &out, const conversation_snapshot &s) {
  out.object();
  out.key("id");
  out.string(s.id);
  out.key("object");
  out.string("conversation");
  out.key("created_at");
  out.number(s.created);
  out.key("metadata");
  out.object();
  for (const auto &p : s.metadata) {
    out.key(p.first);
    out.string(p.second);
  }
  out.end_object();
  out.end_object();
}
void api_conversation_items(wire_json &out, const conversation_snapshot &s,
                            const std::vector<ailo_item> &canonical) {
  // Item rows contain both the original wire object and the durable native
  // view. The latter never depends on an expiring public reasoning token.
  for (size_t n = 0; n < s.incoming.items.size(); ++n) {
    auto item = s.incoming.items[n]->view();
    for (const auto &candidate : canonical)
      if (candidate.id && !std::strcmp(candidate.id, item.id)) {
        item = candidate;
        break;
      }
    out.object();
    out.key("id");
    out.string(item.id);
    out.key("wire");
    out.value(s.wire_items[n]);
    out.key("canonical");
    api_canonical_item(out, item);
    out.end_object();
  }
}
std::unique_ptr<conversation_snapshot>
api_attach_conversation(state &self, openai_request &request) {
  if (request.conversation_id.empty())
    return nullptr;
  auto s = api_load_conversation(*self.openai, request.conversation_id.c_str());
  openai_request posted(openai_protocol::responses, request.raw_json,
                        request.raw_size, self.id, nullptr, true);
  incoming(posted, *s);
  openai_request prior(openai_protocol::responses, s->state_json.data(),
                       s->state_json.size(), self.id, "input");
  openai_request output(openai_protocol::responses, s->state_json.data(),
                        s->state_json.size(), self.id, "output");
  if (compacted(prior.items)) {
    self.ensure_loaded();
    identify_cache(self, true);
    s->rebuild |= prior.record_identity != self.conversation_identity;
  }
  transcript merged;
  for (size_t i = 0; i < request.instruction_items; ++i)
    add(merged, request.items[i]);
  if (s->rebuild) {
    std::string bytes = "{\"input\":";
    ailo_conversation_query query{self.openai->policy.namespace_id.c_str(),
                                  s->id.c_str(),
                                  nullptr,
                                  nullptr,
                                  0,
                                  0,
                                  1};
    query_conversation(*self.openai, query, [&](const void *p, size_t n) {
      if (n > 8 * 1024 * 1024 - bytes.size() - 1)
        throw failure(
            AILO_OUT_OF_RANGE,
            "Conversation history exceeds its context reconstruction budget");
      bytes.append(static_cast<const char *>(p), n);
    });
    bytes += '}';
    openai_request rebuilt(openai_protocol::responses, bytes.data(),
                           bytes.size(), self.id, "input");
    for (const auto &item : rebuilt.items)
      add(merged, item);
  } else {
    for (const auto &i : prior.items)
      add(merged, i);
    for (const auto &i : output.items)
      add(merged, i);
  }
  for (const auto &i : s->incoming.items)
    add(merged, i->view());
  bind(request, std::move(merged));
  return s;
}

// Streaming item query projection lives separately from bounded context loads.
void api_conversation_query_output(state &, const char *, const char *,
                                   const void *, size_t, ailo_output_sink,
                                   void *, ailo_api_info &);
namespace {
void manage(state &self, ailo_conversation_action action, const char *id,
            const char *item_id, const void *json, size_t size,
            ailo_output_sink sink, void *user, ailo_api_info &info) {
  auto &owner = *self.openai;
  if ((action == AILO_CONVERSATION_UPDATE ||
       action == AILO_CONVERSATION_APPEND) &&
      (!json || !size))
    throw failure(AILO_INVALID_ARGUMENT,
                  "Conversation mutation requires request JSON");
  std::unique_ptr<conversation_snapshot> s;
  if (action == AILO_CONVERSATION_CREATE) {
    required(owner, "new");
    s = std::make_unique<conversation_snapshot>();
    api_identity(info, "conv_");
    s->id = info.id;
    s->created = api_now();
  } else
    s = api_load_conversation(owner, id,
                              action == AILO_CONVERSATION_DELETE_ITEM);
  if (action == AILO_CONVERSATION_DELETE_ITEM)
    required(owner, item_id);
  const bool additions =
      action == AILO_CONVERSATION_CREATE || action == AILO_CONVERSATION_APPEND;
  openai_request request(additions ? (action == AILO_CONVERSATION_CREATE
                                          ? openai_protocol::conversation
                                          : openai_protocol::conversation_items)
                                   : openai_protocol::conversation_update,
                         json ? json : "{\"metadata\":null}", json ? size : 17,
                         self.id, nullptr, additions);
  if (action == AILO_CONVERSATION_UPDATE || action == AILO_CONVERSATION_CREATE)
    s->metadata = request.metadata;
  if (additions) {
    incoming(request, *s);
    transcript assigned;
    for (const auto &i : s->incoming.items)
      add(assigned, i->view());
    bind(request, std::move(assigned));
    if (compacted(request.items))
      api_require_replay_storage(self, true);
    api_expand_compaction(self, request);
    api_expand_reasoning(self, request);
  }
  if (action == AILO_CONVERSATION_DELETE) {
    s->deleted = true;
    s->metadata.clear();
  }
  if (action == AILO_CONVERSATION_APPEND ||
      action == AILO_CONVERSATION_DELETE_ITEM)
    s->rebuild = true;
  record_transaction tx(owner, change(owner, *s, action, item_id));
  {
    wire_json object(tx.section(AILO_RECORD_OUTPUT), owner.record_limit);
    api_conversation_header(object, *s);
    object.finish();
  }
  if (action == AILO_CONVERSATION_UPDATE) {
    // Metadata-only updates preserve the exact checkpoint but advance revision.
    openai_request input(openai_protocol::responses, s->state_json.data(),
                         s->state_json.size(), self.id, "input");
    openai_request output(openai_protocol::responses, s->state_json.data(),
                          s->state_json.size(), self.id, "output");
    wire_json saved(tx.section(AILO_RECORD_CONVERSATION_STATE),
                    owner.record_limit);
    saved.object();
    state_header(saved, *s);
    if (!input.record_identity.empty()) {
      saved.key("ailo_model_identity");
      saved.string(input.record_identity);
    }
    for (auto part : {std::pair<const char *, const std::vector<ailo_item> &>{
                          "input", input.items},
                      {"output", output.items}}) {
      saved.key(part.first);
      saved.array();
      for (const auto &i : part.second)
        api_canonical_item(saved, i);
      saved.end_array();
    }
    saved.end_object();
    saved.finish();
  } else
    write_state(self, tx, owner, *s,
                action == AILO_CONVERSATION_CREATE ? request.items
                                                   : std::vector<ailo_item>{});
  if (additions) {
    wire_json rows(tx.section(AILO_RECORD_CONVERSATION_ITEMS),
                   owner.record_limit);
    rows.array();
    api_conversation_items(rows, *s, request.items);
    rows.end_array();
    rows.finish();
  }
  tx.commit();
  info.stored = 1;
  wire_json out(api_sink(self, sink, user, info), owner.record_limit);
  if (action == AILO_CONVERSATION_DELETE) {
    out.object();
    out.key("id");
    out.string(s->id);
    out.key("object");
    out.string("conversation.deleted");
    out.key("deleted");
    out.boolean(true);
    out.end_object();
  } else if (action == AILO_CONVERSATION_APPEND) {
    out.object();
    out.key("object");
    out.string("list");
    out.key("data");
    out.array();
    for (const auto &wire : s->wire_items)
      out.value(wire);
    out.end_array();
    for (const char *name : {"first_id", "last_id"}) {
      out.key(name);
      if (s->incoming.items.empty())
        out.null();
      else
        out.string(std::string(name) == "first_id"
                       ? s->incoming.items.front()->id
                       : s->incoming.items.back()->id);
    }
    out.key("has_more");
    out.boolean(false);
    out.end_object();
  } else
    api_conversation_header(out, *s);
  out.finish();
}
ailo_status operation_call(ailo_client *client, int mode, const char *id,
                           const char *item, const void *json, size_t size,
                           ailo_output_sink sink, void *user,
                           ailo_api_info *info, ailo_error *error) {
  if (info)
    *info = {};
  if (!sink || !info) {
    if (info)
      info->http_status = 400;
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "API sink and info are required");
  }
  const auto status = boundary(error, [&] {
    operation call(client);
    info->content_type = "application/json";
    if (mode == 5 || mode == 6) {
      required(*call.self.openai, id);
      if (mode == 5)
        required(*call.self.openai, item);
      api_conversation_query_output(call.self, id, mode == 5 ? item : nullptr,
                                    json, size, sink, user, *info);
    } else if (mode == 7) {
      const auto snapshot = api_load_conversation(*call.self.openai, id);
      wire_json out(api_sink(call.self, sink, user, *info),
                    call.self.openai->record_limit);
      api_conversation_header(out, *snapshot);
      out.finish();
      info->stored = 1;
    } else
      manage(call.self, static_cast<ailo_conversation_action>(mode), id, item,
             json, size, sink, user, *info);
  });
  if (!info->started)
    info->http_status = api_http_status(status);
  return status;
}
} // namespace
} // namespace client
} // namespace ailo
#define CONVERSATION_CALL(mode, id, item, json, size)                          \
  return ailo::client::operation_call(client, mode, id, item, json, size,      \
                                      sink, user, info, error)
AILO_PUBLIC ailo_status ailo_client_conversation_create(
    ailo_client *client, const void *json, size_t size, ailo_output_sink sink,
    void *user, ailo_api_info *info, ailo_error *error) {
  CONVERSATION_CALL(0, nullptr, nullptr, json, size);
}
AILO_PUBLIC ailo_status ailo_client_conversation_retrieve(
    ailo_client *client, const char *id, ailo_output_sink sink, void *user,
    ailo_api_info *info, ailo_error *error) {
  CONVERSATION_CALL(7, id, nullptr, nullptr, 0);
}
AILO_PUBLIC ailo_status ailo_client_conversation_update(
    ailo_client *client, const char *id, const void *json, size_t size,
    ailo_output_sink sink, void *user, ailo_api_info *info, ailo_error *error) {
  CONVERSATION_CALL(1, id, nullptr, json, size);
}
AILO_PUBLIC ailo_status ailo_client_conversation_delete(
    ailo_client *client, const char *id, ailo_output_sink sink, void *user,
    ailo_api_info *info, ailo_error *error) {
  CONVERSATION_CALL(2, id, nullptr, nullptr, 0);
}
AILO_PUBLIC ailo_status ailo_client_conversation_items_create(
    ailo_client *client, const char *id, const void *json, size_t size,
    ailo_output_sink sink, void *user, ailo_api_info *info, ailo_error *error) {
  CONVERSATION_CALL(3, id, nullptr, json, size);
}
AILO_PUBLIC ailo_status ailo_client_conversation_item_delete(
    ailo_client *client, const char *id, const char *item,
    ailo_output_sink sink, void *user, ailo_api_info *info, ailo_error *error) {
  CONVERSATION_CALL(4, id, item, nullptr, 0);
}
AILO_PUBLIC ailo_status ailo_client_conversation_item_retrieve(
    ailo_client *client, const char *id, const char *item, const void *json,
    size_t size, ailo_output_sink sink, void *user, ailo_api_info *info,
    ailo_error *error) {
  CONVERSATION_CALL(5, id, item, json, size);
}
AILO_PUBLIC ailo_status ailo_client_conversation_items_list(
    ailo_client *client, const char *id, const void *json, size_t size,
    ailo_output_sink sink, void *user, ailo_api_info *info, ailo_error *error) {
  CONVERSATION_CALL(6, id, nullptr, json, size);
}
