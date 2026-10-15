#include "ailo_cache_internal.hpp"
#include "ailo_compaction_internal.hpp"
#include "ailo_openai_crypto_internal.hpp"
#include "ailo_openai_internal.hpp"
#include "ailo_reasoning_internal.hpp"
#include "ailo_tools_internal.hpp"
#include <cstring>
namespace ailo {
namespace client {
namespace {
[[noreturn]] void invalid() {
  throw failure(AILO_INVALID_ARGUMENT,
                "Responses encrypted reasoning does not match its item");
}
void add(transcript &out, const ailo_item &item) {
  if (out.items.size() == 4096)
    throw failure(AILO_OUT_OF_RANGE, "Responses reasoning exceeds 4096 items");
  out.items.push_back(std::make_shared<owned_item>(item, out.bytes,
                                                   4 * 1024 * 1024, out.parts));
}
bool follows_reasoning(const ailo_item &item) {
  return item.kind == AILO_ITEM_FUNCTION_CALL ||
         (item.kind == AILO_ITEM_MESSAGE && item.role == AILO_ROLE_ASSISTANT);
}
} // namespace
void api_require_replay_storage(state &self, bool forever) {
  if (self.compactor->ephemeral ||
      (self.compactor->ttl_seconds &&
       (forever || self.compactor->ttl_seconds <
                       self.openai->policy.response_retention_seconds)))
    throw failure(
        AILO_INVALID_ARGUMENT,
        forever
            ? "Conversation replay state requires stable keys without expiry"
            : "Stored replayable Responses require stable keys and a "
              "compaction TTL covering response retention");
}
void api_prepare_reasoning(state &self, const openai_request &request,
                           bool store, bool forever) {
  self.ensure_loaded();
  auto tools = prepare_tools(request.generation);
  const auto plan =
      render_prompt(self, prepare_replay(self, request.generation), tools,
                    request.generation);
  if (plan.active) {
    if (store || forever)
      api_require_replay_storage(self, forever);
    // Do model identity work before token generation, including with KV reuse
    // disabled. Sealing a done item must not hash model files between deltas.
    identify_cache(self, true);
  }
}
std::string api_seal_reasoning(state &self, const std::string &id,
                               const std::string &text) {
  ailo_content content{AILO_CONTENT_TEXT, text.data(), text.size()};
  ailo_item item{};
  item.kind = AILO_ITEM_REASONING;
  item.role = AILO_ROLE_ASSISTANT;
  item.id = id.c_str();
  item.content = &content;
  item.content_count = 1;
  return api_seal_item(self, api_item_purpose::reasoning, id,
                       [&](wire_json &out) {
                         out.object();
                         out.key("input");
                         out.array();
                         api_canonical_item(out, item);
                         out.end_array();
                         out.end_object();
                       });
}
void api_expand_reasoning(state &self, openai_request &request) {
  bool found = false;
  for (const auto &item : request.items)
    found |= item.kind == AILO_ITEM_REASONING;
  if (!found)
    return;
  transcript expanded;
  for (size_t i = 0; i < request.items.size(); ++i) {
    const auto &item = request.items[i];
    if (item.kind == AILO_ITEM_REASONING && item.encrypted_content) {
      self.ensure_loaded();
      api_plaintext plain;
      api_open_item(self, api_item_purpose::reasoning, item.id,
                    item.encrypted_content, plain);
      openai_request decoded(openai_protocol::responses, plain.bytes.data(),
                             plain.bytes.size(), self.id, "input");
      if (decoded.items.size() != 1 ||
          decoded.items[0].kind != AILO_ITEM_REASONING ||
          decoded.items[0].encrypted_content || !decoded.items[0].id ||
          std::strcmp(item.id, decoded.items[0].id))
        invalid();
      const auto &value = decoded.items[0];
      if (item.content_count) {
        if (item.content_count != value.content_count)
          invalid();
        for (size_t p = 0; p < item.content_count; ++p)
          if (item.content[p].kind != value.content[p].kind ||
              item.content[p].size != value.content[p].size ||
              (item.content[p].size &&
               std::memcmp(item.content[p].data, value.content[p].data,
                           item.content[p].size)))
            invalid();
      }
      add(expanded, value);
    } else
      add(expanded, item);
    // An incomplete response can contain reasoning without a visible answer.
    // Close that historical assistant turn internally; no fabricated message
    // is returned on the wire and no generated answer text is introduced.
    if (item.kind == AILO_ITEM_REASONING &&
        (i + 1 == request.items.size() ||
         !follows_reasoning(request.items[i + 1]))) {
      ailo_item empty{};
      empty.role = AILO_ROLE_ASSISTANT;
      add(expanded, empty);
    }
  }
  request.input = std::move(expanded);
  request.items = request.input.views();
  request.generation.items = request.items.data();
  request.generation.item_count = request.items.size();
}
} // namespace client
} // namespace ailo
