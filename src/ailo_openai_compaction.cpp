#include "ailo_bounded_json_internal.hpp"
#include "ailo_cache_internal.hpp"
#include "ailo_compaction_internal.hpp"
#include "ailo_logging_internal.hpp"
#include "ailo_openai_crypto_internal.hpp"
#include "ailo_openai_internal.hpp"
#include "ailo_reasoning_internal.hpp"
#include "ailo_tools_internal.hpp"
#include <algorithm>
#include <openssl/crypto.h>

#define AILO_PUBLIC extern "C" __attribute__((visibility("default")))
namespace ailo {
namespace client {
namespace {
[[noreturn]] void invalid() {
  throw failure(AILO_INVALID_ARGUMENT,
                "Responses compaction authentication or window binding failed");
}
sealed_summary seal(state &self, const std::vector<ailo_item> &items,
                    const std::vector<ailo_item> *output = nullptr,
                    const std::string &id = {}) {
  ailo_api_info identity{};
  if (id.empty())
    api_identity(identity, "cmp_");
  sealed_summary result{id.empty() ? identity.id : id, {}};
  result.encrypted_content = api_seal_item(self, api_item_purpose::compaction,
                                           result.id, [&](wire_json &out) {
                                             out.object();
                                             out.key("input");
                                             out.array();
                                             for (const auto &i : items)
                                               api_canonical_item(out, i);
                                             out.end_array();
                                             if (output) {
                                               out.key("ailo_automatic");
                                               out.boolean(true);
                                               out.key("output");
                                               out.array();
                                               for (const auto &i : *output)
                                                 api_canonical_item(out, i);
                                               out.end_array();
                                             }
                                             out.end_object();
                                           });
  return result;
}
void add(transcript &out, const ailo_item &item) {
  if (out.items.size() == 4096)
    throw failure(AILO_OUT_OF_RANGE, "Responses compaction exceeds 4096 items");
  out.items.push_back(std::make_shared<owned_item>(item, out.bytes,
                                                   4 * 1024 * 1024, out.parts));
}
size_t token_position(const std::vector<ailo_item> &items, bool required) {
  size_t position = items.size();
  for (size_t i = 0; i < items.size(); ++i)
    if (items[i].kind == AILO_ITEM_COMPACTION) {
      if (position != items.size())
        invalid();
      position = i;
    }
  if (required && position == items.size())
    invalid();
  return position;
}
void bind_request(openai_request &request, transcript &&input) {
  request.input = std::move(input);
  request.items = request.input.views();
  request.generation.items = request.items.data();
  request.generation.item_count = request.items.size();
}
void shift_sources(compaction_payload &payload, size_t prefix, bool remove,
                   size_t limit) {
  if (!prefix)
    return;
  const auto facts = parse_summary(payload.summary, payload.source_items);
  bounded_json out(limit);
  out.object();
  out.key("facts");
  out.array();
  for (const auto &fact : facts) {
    if (remove && fact.source < prefix)
      invalid();
    out.object();
    out.key("source");
    out.number(remove ? fact.source - prefix : fact.source + prefix);
    for (const auto &p :
         {std::pair<const char *, const std::string &>{"kind", fact.kind},
          {"role", fact.role},
          {"id", fact.id},
          {"call_id", fact.call_id},
          {"name", fact.name},
          {"category", fact.category},
          {"text", fact.text}}) {
      out.key(p.first);
      out.string(p.second);
    }
    out.end_object();
  }
  out.end_array();
  out.end_object();
  auto summary = out.finish();
  OPENSSL_cleanse(payload.summary.data(), payload.summary.size());
  payload.summary = std::move(summary);
}
} // namespace

// Canonical wire windows exclude ephemeral top-level instructions. Native
// binding is authenticated before removing that prefix and resealing the token.
static transcript canonical_window(state &self,
                                   const std::vector<ailo_item> &items,
                                   size_t instructions) {
  if (instructions > items.size())
    invalid();
  transcript result;
  const size_t position = token_position(items, false);
  if (position == items.size()) {
    for (size_t i = instructions; i < items.size(); ++i)
      add(result, items[i]);
    return result;
  }
  if (position < instructions)
    invalid();
  self.ensure_loaded();
  identify_cache(self, true);
  auto payload = self.compactor->open(items[position].id,
                                      items[position].encrypted_content,
                                      self.conversation_identity, &self);
  verify_compaction_binding(*payload, items.data(), items.size(), position);
  if (payload->cut < instructions || payload->retained.size() < instructions ||
      payload->source_items < instructions)
    invalid();
  shift_sources(*payload, instructions, true, self.compactor->max_item_bytes);
  payload->retained.erase(payload->retained.begin(),
                          payload->retained.begin() +
                              static_cast<ptrdiff_t>(instructions));
  payload->cut -= instructions;
  payload->source_items -= instructions;
  const auto native =
      self.compactor->seal(*payload, self.conversation_identity, &self);
  std::vector<ailo_item> window(
      items.begin() + static_cast<ptrdiff_t>(instructions), items.end());
  const size_t cut = position - instructions;
  window[cut].id = native.id.c_str();
  window[cut].encrypted_content = native.encrypted_content.c_str();
  for (const auto &i : window)
    add(result, i);
  return result;
}
transcript api_compacted_window(state &self,
                                const std::vector<ailo_item> &items,
                                size_t instructions) {
  auto result = canonical_window(self, items, instructions);
  const auto window = result.views();
  const size_t cut = token_position(window, false);
  if (cut == window.size())
    return result;
  const auto wire = seal(self, window);
  auto token = window[cut];
  token.id = wire.id.c_str();
  token.encrypted_content = wire.encrypted_content.c_str();
  transcript replaced;
  for (size_t i = 0; i < window.size(); ++i)
    add(replaced, i == cut ? token : window[i]);
  return replaced;
}
sealed_summary api_automatic_window(state &self,
                                    const std::vector<ailo_item> &items,
                                    size_t instructions,
                                    const std::vector<ailo_item> &output,
                                    const std::string &id) {
  const auto window = canonical_window(self, items, instructions);
  return seal(self, window.views(), &output, id);
}
void api_expand_compaction(state &self, openai_request &request) {
  size_t position = request.items.size();
  for (size_t i = 0; i < request.items.size(); ++i)
    if (request.items[i].kind == AILO_ITEM_COMPACTION)
      position = i;
  if (position == request.items.size())
    return;
  self.ensure_loaded();
  identify_cache(self, true);
  api_plaintext plain;
  api_open_item(self, api_item_purpose::compaction, request.items[position].id,
                request.items[position].encrypted_content, plain);
  openai_request decoded(openai_protocol::responses, plain.bytes.data(),
                         plain.bytes.size(), self.id, "input");
  const size_t cut = token_position(decoded.items, false),
               count = decoded.items.size(), prefix = request.instruction_items;
  // A triggered compact of an already-small window still returns an opaque,
  // replayable checkpoint. The outer authenticated envelope contains the whole
  // canonical window; there is no inner semantic summary in this case.
  if (cut == count) {
    if (!decoded.record_automatic)
      invalid();
    transcript expanded;
    for (size_t i = 0; i < prefix; ++i)
      add(expanded, request.items[i]);
    for (const auto &item : decoded.items)
      add(expanded, item);
    openai_request output(openai_protocol::responses, plain.bytes.data(),
                          plain.bytes.size(), self.id, "output");
    for (const auto &item : output.items)
      add(expanded, item);
    for (size_t i = position + 1; i < request.items.size(); ++i)
      add(expanded, request.items[i]);
    bind_request(request, std::move(expanded));
    return;
  }
  if (!decoded.record_automatic) {
    (void)token_position(request.items, false);
    if (position != prefix + cut || count > request.items.size() - prefix)
      invalid();
    for (size_t i = 0; i < count; ++i) {
      if (i == cut)
        continue;
      auto supplied = request.items[prefix + i];
      // Storage can name a previously anonymous item. Authenticate all original
      // fields, including every explicit ID, then restore the bound native
      // view.
      if (!decoded.items[i].id)
        supplied.id = nullptr;
      if (fingerprint(decoded.items[i]) != fingerprint(supplied))
        invalid();
    }
  }
  auto payload = self.compactor->open(decoded.items[cut].id,
                                      decoded.items[cut].encrypted_content,
                                      self.conversation_identity, &self);
  verify_compaction_binding(*payload, decoded.items.data(), count, cut);
  if (prefix > 4096 - payload->source_items)
    throw failure(
        AILO_OUT_OF_RANGE,
        "Responses instruction prefix exceeds its compaction item budget");
  shift_sources(*payload, prefix, false, self.compactor->max_item_bytes);
  std::vector<compaction_digest> retained;
  for (size_t i = 0; i < prefix; ++i)
    retained.push_back(fingerprint(request.items[i]));
  retained.insert(retained.end(), payload->retained.begin(),
                  payload->retained.end());
  payload->retained = std::move(retained);
  payload->cut += prefix;
  payload->source_items += prefix;
  const auto native =
      self.compactor->seal(*payload, self.conversation_identity, &self);
  transcript expanded;
  for (size_t i = 0; i < prefix; ++i)
    add(expanded, request.items[i]);
  for (size_t i = 0; i < count; ++i) {
    auto item = decoded.items[i];
    if (i == cut) {
      item.id = native.id.c_str();
      item.encrypted_content = native.encrypted_content.c_str();
    }
    add(expanded, item);
  }
  if (decoded.record_automatic) {
    openai_request output(openai_protocol::responses, plain.bytes.data(),
                          plain.bytes.size(), self.id, "output");
    for (const auto &i : output.items)
      add(expanded, i);
  }
  const size_t suffix =
      decoded.record_automatic ? position + 1 : prefix + count;
  for (size_t i = suffix; i < request.items.size(); ++i)
    add(expanded, request.items[i]);
  bind_request(request, std::move(expanded));
}
} // namespace client
} // namespace ailo

AILO_PUBLIC ailo_status ailo_client_response_compact(
    ailo_client *client, const void *json, size_t size, ailo_output_sink sink,
    void *user, ailo_api_info *info, ailo_error *error) {
  using namespace ailo::client;
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
    auto &self = call.self;
    openai_request request(openai_protocol::compact, json, size, self.id);
    // Compaction itself is stateless. Only an explicit prior-response read
    // needs a durable repository; no record is reserved or published here.
    request.store = 0;
    (void)resolve_retention(request, self.openai->policy, api_now());
    api_prior_input(self, request);
    api_expand_compaction(self, request);
    api_expand_reasoning(self, request);
    self.ensure_loaded();
    identify_cache(self, true);
    log_scope logs(self.logging.get(), log_phase::compact);
    auto tools = prepare_tools(request.generation);
    const auto prompt =
        render_prompt(self, prepare_replay(self, request.generation), tools,
                      request.generation)
            .prompt;
    if (prompt_token_count(self, prompt) > llama_n_ctx(self.context.get()))
      throw failure(
          AILO_CONTEXT_LIMIT,
          "Standalone Responses compaction input exceeds the model context");
    ailo_compaction_info compact{};
    ailo_usage work{};
    compaction_task task{};
    task.instructions =
        request.instructions.empty() ? nullptr : request.instructions.c_str();
    task.usage = &work;
    std::unique_ptr<ailo_continuation, decltype(&ailo_continuation_destroy)>
        window(compact_window(self, request.generation, self.openai->compaction,
                              compact, nullptr, &task),
               ailo_continuation_destroy);
    const auto canonical = api_compacted_window(
        self, get_continuation(window.get()).current->views(),
        request.instruction_items);
    if (canonical.bytes > self.openai->memory_limit)
      throw failure(AILO_OUT_OF_RANGE,
                    "Responses compaction output exceeds its memory limit");
    info->content_type = "application/json";
    api_identity(*info, "resp_");
    info->run.actual_backend = self.actual;
    info->run.usage = work;
    info->run.usage.compaction_prompt_tokens = compact.summary_prompt_tokens;
    info->run.usage.compaction_completion_tokens =
        compact.summary_completion_tokens;
    info->run.usage.compaction_passes = compact.summary_passes;
    wire_json out(api_sink(self, sink, user, *info), self.openai->record_limit);
    out.object();
    out.key("id");
    out.string(info->id);
    out.key("object");
    out.string("response.compaction");
    out.key("created_at");
    out.number(api_now());
    out.key("output");
    out.array();
    for (const auto &i : canonical.items)
      api_canonical_item(out, i->view());
    out.end_array();
    out.key("usage");
    out.object();
    const auto input_tokens =
                   ailo::detail::to_native(compact.summary_prompt_tokens),
               output_tokens =
                   ailo::detail::to_native(compact.summary_completion_tokens);
    out.key("input_tokens");
    out.number(input_tokens);
    out.key("output_tokens");
    out.number(output_tokens);
    out.key("total_tokens");
    out.number(input_tokens + output_tokens);
    out.key("input_tokens_details");
    out.object();
    out.key("cached_tokens");
    out.number(0);
    out.key("cache_write_tokens");
    out.number(0);
    out.end_object();
    out.key("output_tokens_details");
    out.object();
    out.key("reasoning_tokens");
    out.number(ailo::detail::to_native(work.reasoning_tokens));
    out.end_object();
    out.end_object();
    out.end_object();
    out.finish();
  });
  if (!info->started)
    info->http_status = api_http_status(status);
  return status;
}
