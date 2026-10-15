#include "ailo_client_internal.hpp"
#include "ailo_sampling_internal.hpp"
#include "ailo_schema_internal.hpp"
#include "ailo_text_stream_internal.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace ailo {
namespace client {
namespace {
const char *role_name(ailo_role role) {
  switch (role) {
  case AILO_ROLE_USER:
    return "user";
  case AILO_ROLE_SYSTEM:
    return "system";
  case AILO_ROLE_DEVELOPER:
    return "developer";
  case AILO_ROLE_ASSISTANT:
    return "assistant";
  case AILO_ROLE_TOOL:
    return "tool";
  }
  throw failure(AILO_INVALID_ARGUMENT, "Unknown message role");
}

std::string render(state &self, const ailo_request &request) {
  if (!request.items || !request.item_count || request.item_count > 4096)
    throw failure(AILO_INVALID_ARGUMENT,
                  "Request needs between 1 and 4096 items");
  std::vector<std::string> content;
  std::vector<llama_chat_message> messages;
  content.reserve(request.item_count);
  messages.reserve(request.item_count);
  size_t total = 0;
  for (size_t i = 0; i < request.item_count; ++i) {
    const ailo_item &item = request.items[i];
    if (item.kind != AILO_ITEM_MESSAGE || item.role == AILO_ROLE_TOOL ||
        item.call_id || item.arguments || item.name)
      throw failure(AILO_UNSUPPORTED, "This template adapter does not support "
                                      "the requested item semantics");
    const char *role = role_name(item.role);
    if ((!item.content && item.content_count) || item.content_count > 4096)
      throw failure(AILO_INVALID_ARGUMENT, "Invalid message content array");
    std::string text;
    for (size_t j = 0; j < item.content_count; ++j) {
      const ailo_content &part = item.content[j];
      if (part.kind != AILO_CONTENT_TEXT)
        throw failure(AILO_UNSUPPORTED,
                      "This model adapter accepts text content only");
      if ((!part.data && part.size) || part.size > 4 * 1024 * 1024 - total)
        throw failure(AILO_INVALID_ARGUMENT,
                      "Request text is missing or exceeds 4 MiB");
      if (part.size) {
        const auto *bytes = static_cast<const char *>(part.data);
        require_utf8(bytes, part.size);
        if (std::memchr(bytes, 0, part.size))
          throw failure(
              AILO_UNSUPPORTED,
              "This template adapter cannot represent embedded NUL text");
        text.append(bytes, part.size);
        total += part.size;
      }
    }
    content.push_back(std::move(text));
    messages.push_back({role, content.back().c_str()});
  }
  self.ensure_loaded();
  const char *tmpl = self.config.chat_template
                         ? self.config.chat_template
                         : llama_model_chat_template(self.model.get(), nullptr);
  if (!tmpl)
    throw failure(
        AILO_UNSUPPORTED,
        "The model has no chat template; configure an explicit template");
  return apply_template(tmpl, llama_model_get_vocab(self.model.get()),
                        messages);
}

struct emitter {
  state &self;
  ailo_event_sink sink;
  void *user;
  ailo_run_info &info;
  uint64_t sequence = 0;
  unsigned candidate = 0;
  void send(ailo_event_kind kind, const char *data = nullptr, size_t size = 0,
            ailo_finish_reason finish = AILO_FINISH_NONE,
            const ailo_token_info *token = nullptr) {
    if (self.cancelled.load())
      throw failure(AILO_CANCELLED, "Generation cancelled");
    ailo_event event{};
    event.kind = kind;
    event.sequence = detail::from_unsigned(sequence++);
    event.candidate = candidate;
    event.model_id = self.id.c_str();
    event.data = data;
    event.size = size;
    event.finish_reason = finish;
    event.usage = info.usage;
    event.token = token;
    const int accepted = sink(user, &event);
    if (accepted < 0)
      throw failure(AILO_SINK_FAILED, "The event sink failed");
    if (!accepted || self.cancelled.load())
      throw failure(AILO_CANCELLED, "Generation cancelled by the event sink");
  }
};

void evaluate(state &self, const llama_token *tokens, size_t size,
              size_t position) {
  const size_t batch_size = llama_n_batch(self.context.get());
  for (size_t offset = 0; offset < size;) {
    if (self.cancelled.load())
      throw failure(AILO_CANCELLED, "Generation cancelled");
    const auto n = static_cast<int32_t>(std::min(batch_size, size - offset));
    llama_batch batch = llama_batch_init(n, 0, 1);
    struct cleanup {
      llama_batch &batch;
      ~cleanup() { llama_batch_free(batch); }
    } guard{batch};
    batch.n_tokens = n;
    for (int32_t i = 0; i < n; ++i) {
      batch.token[i] = tokens[offset + static_cast<size_t>(i)];
      batch.pos[i] =
          static_cast<llama_pos>(position + offset + static_cast<size_t>(i));
      batch.n_seq_id[i] = 1;
      batch.seq_id[i][0] = 0;
      batch.logits[i] = i == n - 1;
    }
    if (llama_decode(self.context.get(), batch))
      throw failure(self.cancelled.load() ? AILO_CANCELLED : AILO_MODEL_FAILED,
                    "Native token evaluation failed");
    offset += static_cast<size_t>(n);
  }
}
} // namespace

void generate(state &self, const ailo_request &request, ailo_event_sink sink,
              void *user, ailo_run_info &info) {
  const size_t limit = request.max_tokens ? request.max_tokens : 128;
  const unsigned candidates = request.candidates ? request.candidates : 1;
  const double top_p = request.top_p ? request.top_p : 1.0;
  validate_sampling(request);
  if (candidates > 16 || limit > static_cast<size_t>(INT32_MAX) ||
      !std::isfinite(request.temperature) || request.temperature < 0 ||
      request.temperature > 2 || !std::isfinite(top_p) || top_p <= 0 ||
      top_p > 1 || !std::isfinite(request.frequency_penalty) ||
      std::abs(request.frequency_penalty) > 2 ||
      !std::isfinite(request.presence_penalty) ||
      std::abs(request.presence_penalty) > 2 || request.stop_count > 4 ||
      (request.stop_count && !request.stop))
    throw failure(AILO_INVALID_ARGUMENT,
                  "Invalid sampling limits or stop array");
  std::vector<std::string> stops;
  for (size_t i = 0; i < request.stop_count; ++i) {
    if (!request.stop[i])
      throw failure(AILO_INVALID_ARGUMENT, "Stop string is missing");
    const size_t size = strnlen(request.stop[i], 4097);
    if (!size || size > 4096)
      throw failure(AILO_INVALID_ARGUMENT,
                    "Stop strings must contain 1 to 4096 bytes");
    require_utf8(request.stop[i], size);
    stops.emplace_back(request.stop[i], size);
  }
  const std::string grammar = output_grammar(request);
  const std::string prompt = render(self, request);
  const auto *vocab = llama_model_get_vocab(self.model.get());
  int32_t count =
      llama_tokenize(vocab, prompt.data(), static_cast<int32_t>(prompt.size()),
                     nullptr, 0, true, true);
  if (count >= 0 || count == std::numeric_limits<int32_t>::min())
    throw failure(AILO_MODEL_FAILED, "Cannot size prompt tokenization");
  count = -count;
  if (static_cast<size_t>(count) + limit > llama_n_ctx(self.context.get()))
    throw failure(AILO_CONTEXT_LIMIT,
                  "Prompt plus output budget exceeds the loaded context");
  std::vector<llama_token> tokens(static_cast<size_t>(count));
  if (llama_tokenize(vocab, prompt.data(), static_cast<int32_t>(prompt.size()),
                     tokens.data(), count, true, true) != count)
    throw failure(AILO_MODEL_FAILED, "Prompt tokenization failed");
  info.usage.prompt_tokens =
      detail::from_unsigned(static_cast<uint64_t>(count));
  info.actual_backend = self.actual;
  // Validate model-specific token IDs before emitting events or evaluating.
  sampling first_sampler(vocab, request, 0, llama_n_ctx(self.context.get()),
                         grammar);
  emitter output{self, sink, user, info};
  output.send(AILO_EVENT_RUN_BEGIN);
  struct reset {
    state &self;
    ~reset() { llama_memory_clear(llama_get_memory(self.context.get()), true); }
  } reset_context{self};
  uint64_t generated = 0;
  size_t output_bytes = 0;
  const size_t output_limit =
      request.max_output_bytes ? request.max_output_bytes : 16 * 1024 * 1024;
  for (unsigned candidate = 0; candidate < candidates; ++candidate) {
    output.candidate = candidate;
    llama_memory_clear(llama_get_memory(self.context.get()), true);
    output.send(AILO_EVENT_CHOICE_BEGIN);
    evaluate(self, tokens.data(), tokens.size(), 0);
    std::unique_ptr<sampling> additional;
    if (candidate)
      additional = std::make_unique<sampling>(
          vocab, request, candidate, llama_n_ctx(self.context.get()), grammar);
    sampling &sampler = candidate ? *additional : first_sampler;
    for (llama_token token : tokens)
      sampler.accept(token);
    std::unique_ptr<json_validation> validation;
    if (!grammar.empty())
      validation = std::make_unique<json_validation>(output_limit);
    text_stream carry(stops, [&](const char *data, size_t size) {
      if (validation)
        validation->push(data, size);
      output.send(AILO_EVENT_TEXT_DELTA, data, size);
    });
    ailo_finish_reason finish = AILO_FINISH_LENGTH;
    size_t position = tokens.size();
    uint64_t byte_offset = 0;
    for (size_t i = 0; i < limit; ++i) {
      if (self.cancelled.load())
        throw failure(AILO_CANCELLED, "Generation cancelled");
      ailo_token_info token_info{};
      const llama_token token = sampler.sample(self.context.get(), token_info);
      ++generated;
      info.usage.completion_tokens = detail::from_unsigned(generated);
      const bool eog = llama_vocab_is_eog(vocab, token);
      const std::string piece =
          eog ? "" : token_piece(vocab, token, !grammar.empty());
      if (piece.size() > output_limit - output_bytes)
        throw failure(AILO_OUT_OF_RANGE,
                      "Generated output exceeds its byte budget");
      output_bytes += piece.size();
      if (request.logprobs) {
        token_info.index = detail::from_unsigned(i);
        token_info.byte_offset = detail::from_unsigned(byte_offset);
        output.send(AILO_EVENT_TOKEN, nullptr, 0, AILO_FINISH_NONE,
                    &token_info);
      }
      byte_offset += piece.size();
      if (eog) {
        finish = AILO_FINISH_STOP;
        break;
      }
      if (carry.push(piece)) {
        if (validation)
          throw failure(AILO_MODEL_FAILED,
                        "A stop sequence interrupted constrained output");
        finish = AILO_FINISH_STOP;
        break;
      }
      if (i + 1 < limit)
        evaluate(self, &token, 1, position++);
    }
    (void)carry.push({}, true);
    if (validation && finish == AILO_FINISH_STOP)
      validation->finish();
    output.send(AILO_EVENT_CHOICE_END, nullptr, 0, finish);
    info.finish_reason = finish;
    ++info.completed_candidates;
  }
  output.send(AILO_EVENT_RUN_END, nullptr, 0, info.finish_reason);
}
} // namespace client
} // namespace ailo
