#include "ailo_cache_internal.hpp"
#include "ailo_client_internal.hpp"
#include "ailo_compaction_internal.hpp"
#include "ailo_conversation_internal.hpp"
#include "ailo_reasoning_internal.hpp"
#include "ailo_sampling_internal.hpp"
#include "ailo_schema_internal.hpp"
#include "ailo_text_stream_internal.hpp"
#include "ailo_tools_internal.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <openssl/rand.h>

namespace ailo {
namespace client {
reasoning_plan render_prompt(state &self, const conversation &input,
                             tool_plan &tools, const ailo_request &request) {
  self.ensure_loaded();
  const char *tmpl = self.config.chat_template
                         ? self.config.chat_template
                         : llama_model_chat_template(self.model.get(), nullptr);
  if (!tmpl)
    throw failure(
        AILO_UNSUPPORTED,
        "The model has no chat template; configure an explicit template");
  qualify_tools(tools, tmpl, llama_model_get_vocab(self.model.get()));
  auto result = qualify_reasoning(tmpl, llama_model_get_vocab(self.model.get()),
                                  input, tools.definitions_json, request);
  if (!input.compaction_text.empty() &&
      result.prompt.find(input.compaction_text) == std::string::npos)
    throw failure(
        AILO_UNSUPPORTED,
        "The chat template does not preserve authenticated compaction context");
  return result;
}
size_t prompt_token_count(state &self, const std::string &prompt) {
  if (prompt.size() > static_cast<size_t>(INT32_MAX))
    throw failure(AILO_OUT_OF_RANGE,
                  "Rendered prompt exceeds tokenizer limits");
  const int32_t count = llama_tokenize(
      llama_model_get_vocab(self.model.get()), prompt.data(),
      static_cast<int32_t>(prompt.size()), nullptr, 0, true, true);
  if (count >= 0 || count == std::numeric_limits<int32_t>::min())
    throw failure(AILO_MODEL_FAILED, "Cannot size prompt tokenization");
  return static_cast<size_t>(-count);
}
namespace {

struct emitter {
  state &self;
  ailo_event_sink sink;
  void *user;
  ailo_run_info &info;
  uint64_t sequence = 0;
  unsigned candidate = 0;
  std::string run_id{}, item_id{}, call_id{}, name{};
  ailo_item_kind item_kind = AILO_ITEM_MESSAGE;
  unsigned item_index = 0;
  bool item_active = false;
  emitter(state &value, ailo_event_sink callback, void *context,
          ailo_run_info &run)
      : self(value), sink(callback), user(context), info(run) {
    unsigned char nonce[16];
    if (RAND_bytes(nonce, sizeof(nonce)) != 1)
      throw failure(AILO_INTERNAL_ERROR,
                    "Cannot create an inference run identity");
    static const char hex[] = "0123456789abcdef";
    run_id = "run_";
    for (unsigned char ch : nonce) {
      run_id += hex[ch >> 4];
      run_id += hex[ch & 15];
    }
    std::memcpy(info.id, run_id.c_str(), run_id.size() + 1);
  }
  void send(ailo_event_kind kind, const char *data = nullptr, size_t size = 0,
            ailo_finish_reason finish = AILO_FINISH_NONE,
            const ailo_token_info *token = nullptr) {
    if (self.is_cancelled())
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
    event.run_id = run_id.c_str();
    event.item_id = item_active ? item_id.c_str() : nullptr;
    event.item_kind = item_kind;
    event.item_index = item_index;
    event.call_id = item_active && item_kind == AILO_ITEM_FUNCTION_CALL
                        ? call_id.c_str()
                        : nullptr;
    event.name = event.call_id ? name.c_str() : nullptr;
    const int accepted = sink(user, &event);
    if (accepted < 0)
      throw failure(AILO_SINK_FAILED, "The event sink failed");
    if (!accepted || self.is_cancelled())
      throw failure(AILO_CANCELLED, "Generation cancelled by the event sink");
  }
  void begin_message() {
    begin_item(AILO_ITEM_MESSAGE);
    send(AILO_EVENT_CONTENT_BEGIN);
  }
  void begin_item(ailo_item_kind kind) {
    item_kind = kind;
    const auto suffix = run_id.substr(4) + "_" + std::to_string(candidate) +
                        "_" + std::to_string(item_index);
    item_id = (kind == AILO_ITEM_MESSAGE     ? "msg_"
               : kind == AILO_ITEM_REASONING ? "rs_"
                                             : "fc_") +
              suffix;
    call_id = "call_" + suffix;
    name.clear();
    item_active = true;
    send(AILO_EVENT_ITEM_BEGIN);
  }
  void end_message(ailo_finish_reason finish) {
    send(AILO_EVENT_CONTENT_END, nullptr, 0, finish);
    send(AILO_EVENT_ITEM_END, nullptr, 0, finish);
    item_active = false;
    ++item_index;
  }
  void tool(const tool_output_event &event) {
    if (event.kind == tool_output_kind::text) {
      if (!item_active)
        begin_message();
      send(AILO_EVENT_TEXT_DELTA, event.data, event.size);
    } else if (event.kind == tool_output_kind::begin) {
      if (item_active)
        end_message(AILO_FINISH_NONE);
      begin_item(AILO_ITEM_FUNCTION_CALL);
    } else if (event.kind == tool_output_kind::name) {
      name.append(event.data, event.size);
      send(AILO_EVENT_FUNCTION_NAME_DELTA, event.data, event.size);
    } else if (event.kind == tool_output_kind::arguments) {
      send(AILO_EVENT_FUNCTION_ARGUMENTS_DELTA, event.data, event.size);
    } else {
      send(AILO_EVENT_ITEM_END, nullptr, 0, AILO_FINISH_TOOL_CALLS);
      item_active = false;
      ++item_index;
    }
  }
};

void evaluate(state &self, const llama_token *tokens, size_t size,
              size_t position, bool prompt = false) {
  const size_t batch_size = llama_n_batch(self.context.get());
  for (size_t offset = 0; offset < size;) {
    if (self.is_cancelled())
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
      throw failure(self.is_cancelled() ? AILO_CANCELLED : AILO_MODEL_FAILED,
                    "Native token evaluation failed");
    if (prompt && self.cache)
      self.cache->evaluated_tokens.fetch_add(static_cast<size_t>(n));
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
  const auto answer_grammar = output_grammar(request);
  const auto partition = cache_partition(request.prompt_cache_key);
  auto tools = prepare_tools(request);
  const auto input = prepare_replay(self, request);
  const auto reasoning = render_prompt(self, input, tools, request);
  const std::string &prompt = reasoning.prompt;
  std::string grammar =
      tools.definitions.empty() ? answer_grammar : tool_grammar(tools, request);
  if (reasoning.active)
    grammar = reasoning_grammar(reasoning.format, grammar);
  const char *grammar_root = reasoning.active ? "reasoning-root" : "root";
  const auto *vocab = llama_model_get_vocab(self.model.get());
  const auto count = static_cast<int32_t>(prompt_token_count(self, prompt));
  if (static_cast<size_t>(count) + limit > llama_n_ctx(self.context.get()))
    throw failure(AILO_CONTEXT_LIMIT,
                  "Prompt plus output budget exceeds the loaded context");
  std::vector<llama_token> tokens(static_cast<size_t>(count));
  if (llama_tokenize(vocab, prompt.data(), static_cast<int32_t>(prompt.size()),
                     tokens.data(), count, true, true) != count)
    throw failure(AILO_MODEL_FAILED, "Prompt tokenization failed");
  identify_cache(self);
  info.usage.prompt_tokens =
      detail::from_unsigned(static_cast<uint64_t>(count));
  info.actual_backend = self.actual;
  // Validate model-specific token IDs before emitting events or evaluating.
  sampling first_sampler(vocab, request, 0, llama_n_ctx(self.context.get()),
                         grammar, grammar_root);
  emitter output{self, sink, user, info};
  output.send(AILO_EVENT_RUN_BEGIN);
  struct reset {
    state &self;
    ~reset() { llama_memory_clear(llama_get_memory(self.context.get()), true); }
  } reset_context{self};
  uint64_t generated = 0;
  uint64_t reasoned = 0;
  size_t output_bytes = 0;
  std::string checkpoint_key;
  const size_t output_limit =
      request.max_output_bytes ? request.max_output_bytes : 16 * 1024 * 1024;
  for (unsigned candidate = 0; candidate < candidates; ++candidate) {
    output.candidate = candidate;
    output.item_index = 0;
    llama_memory_clear(llama_get_memory(self.context.get()), true);
    output.send(AILO_EVENT_CHOICE_BEGIN);
    if (reasoning.active) {
      output.begin_item(AILO_ITEM_REASONING);
      output.send(AILO_EVENT_CONTENT_BEGIN);
    } else if (tools.definitions.empty())
      output.begin_message();
    {
      struct prefill_timing {
        cache_state *cache;
        std::chrono::steady_clock::time_point start =
            std::chrono::steady_clock::now();
        ~prefill_timing() {
          if (cache)
            cache->prefill_microseconds.fetch_add(elapsed_us(start));
        }
      } timing{self.cache.get()};
      const size_t reused =
          self.cache ? self.cache->restore(self, tokens, partition) : 0;
      if (!candidate)
        info.usage.cached_tokens = detail::from_unsigned(reused);
      const size_t checkpoint = tokens.size() - 1;
      if (reused < checkpoint)
        evaluate(self, tokens.data() + reused, checkpoint - reused, reused,
                 true);
      if (self.cache)
        checkpoint_key =
            self.cache->capture(self, tokens, checkpoint, partition);
      // Sequence states don't include fresh logits. Always replay the last
      // token.
      evaluate(self, tokens.data() + checkpoint, 1, checkpoint, true);
    }
    std::unique_ptr<sampling> additional;
    if (candidate)
      additional = std::make_unique<sampling>(vocab, request, candidate,
                                              llama_n_ctx(self.context.get()),
                                              grammar, grammar_root);
    sampling &sampler = candidate ? *additional : first_sampler;
    for (llama_token token : tokens)
      sampler.accept(token);
    std::unique_ptr<json_validation> validation;
    if (request.output_format != AILO_OUTPUT_TEXT)
      validation = std::make_unique<json_validation>(output_limit);
    bool validated_text = false;
    std::unique_ptr<tool_output_stream> tool_output;
    if (!tools.definitions.empty())
      tool_output = std::make_unique<tool_output_stream>(
          tools, output_limit, [&](const tool_output_event &event) {
            if (validation && event.kind == tool_output_kind::text) {
              validation->push(event.data, event.size);
              validated_text = true;
            }
            output.tool(event);
          });
    auto answer_sink = [&](const char *data, size_t size) {
      if (tool_output) {
        tool_output->push(data, size);
        return;
      }
      if (validation) {
        validation->push(data, size);
        validated_text = true;
      }
      output.send(AILO_EVENT_TEXT_DELTA, data, size);
    };
    std::unique_ptr<reasoning_stream> reasoning_output;
    if (reasoning.active)
      reasoning_output = std::make_unique<reasoning_stream>(
          reasoning.format,
          [&](reasoning_output_kind kind, const char *data, size_t size) {
            if (kind == reasoning_output_kind::reasoning)
              output.send(AILO_EVENT_REASONING_DELTA, data, size);
            else if (kind == reasoning_output_kind::end) {
              output.end_message(AILO_FINISH_NONE);
              if (tools.definitions.empty())
                output.begin_message();
            } else
              answer_sink(data, size);
          });
    text_stream carry(stops, [&](const char *data, size_t size) {
      if (reasoning_output)
        reasoning_output->push(data, size);
      else
        answer_sink(data, size);
    });
    ailo_finish_reason finish = AILO_FINISH_LENGTH;
    size_t position = tokens.size();
    uint64_t byte_offset = 0;
    size_t candidate_reasoned = 0;
    for (size_t i = 0; i < limit; ++i) {
      if (self.is_cancelled())
        throw failure(AILO_CANCELLED, "Generation cancelled");
      ailo_token_info token_info{};
      const llama_token token = sampler.sample(self.context.get(), token_info);
      ++generated;
      if (reasoning_output && reasoning_output->in_reasoning()) {
        ++candidate_reasoned;
        ++reasoned;
        info.usage.reasoning_tokens = detail::from_unsigned(reasoned);
      }
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
        if (validation ||
            (reasoning_output && reasoning_output->in_reasoning()) ||
            (tool_output && !tool_output->between_calls()))
          throw failure(AILO_MODEL_FAILED,
                        "A stop sequence interrupted constrained output");
        finish = AILO_FINISH_STOP;
        break;
      }
      if (request.max_reasoning_tokens &&
          candidate_reasoned >= request.max_reasoning_tokens &&
          reasoning_output && reasoning_output->in_reasoning())
        break;
      if (i + 1 < limit)
        evaluate(self, &token, 1, position++);
    }
    (void)carry.push({}, true);
    if (reasoning_output)
      reasoning_output->finish(finish == AILO_FINISH_LENGTH);
    if (tool_output) {
      tool_output->finish(finish == AILO_FINISH_LENGTH);
      if (finish == AILO_FINISH_STOP && tool_output->count())
        finish = AILO_FINISH_TOOL_CALLS;
    }
    if (validation && validated_text && finish == AILO_FINISH_STOP)
      validation->finish();
    if (output.item_active) {
      if (output.item_kind == AILO_ITEM_MESSAGE ||
          output.item_kind == AILO_ITEM_REASONING)
        output.end_message(finish);
      else {
        output.send(AILO_EVENT_ITEM_END, nullptr, 0, finish);
        output.item_active = false;
      }
    }
    output.send(AILO_EVENT_CHOICE_END, nullptr, 0, finish);
    info.finish_reason = finish;
    ++info.completed_candidates;
  }
  output.send(AILO_EVENT_USAGE, nullptr, 0, info.finish_reason);
  output.send(AILO_EVENT_RUN_END, nullptr, 0, info.finish_reason);
  self.last_checkpoint = std::move(checkpoint_key);
  self.last_checkpoint_partition = partition;
}
} // namespace client
} // namespace ailo
