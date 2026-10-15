#include "ailo_sampling_internal.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>

namespace ailo {
namespace client {
void validate_sampling(const ailo_request &request) {
  if (request.top_logprobs > 20 ||
      (request.top_logprobs && !request.logprobs) ||
      request.logit_bias_count > 4096 ||
      (request.logit_bias_count && !request.logit_bias))
    throw failure(AILO_INVALID_ARGUMENT,
                  "Invalid log probabilities or logit bias array");
  std::set<ailo_i32> ids;
  for (size_t i = 0; i < request.logit_bias_count; ++i) {
    const auto &value = request.logit_bias[i];
    if (value.token_id < 0 || !std::isfinite(value.bias) ||
        std::abs(value.bias) > 100 || !ids.insert(value.token_id).second)
      throw failure(
          AILO_INVALID_ARGUMENT,
          "Logit biases need unique token IDs and values in [-100, 100]");
  }
}

std::string token_piece(const llama_vocab *vocab, llama_token token,
                        bool special) {
  char small[256];
  const int32_t size =
      llama_token_to_piece(vocab, token, small, sizeof(small), 0, special);
  if (size >= 0)
    return std::string(small, static_cast<size_t>(size));
  if (size == INT32_MIN || -size > 1024 * 1024)
    throw failure(AILO_MODEL_FAILED, "Native token piece exceeds 1 MiB");
  std::string large(static_cast<size_t>(-size), '\0');
  const int32_t actual =
      llama_token_to_piece(vocab, token, &large[0], -size, 0, special);
  if (actual < 0 || actual > -size)
    throw failure(AILO_MODEL_FAILED, "Native token piece conversion failed");
  large.resize(static_cast<size_t>(actual));
  return large;
}

sampling::sampling(const llama_vocab *input, const ailo_request &request,
                   unsigned candidate, size_t context_tokens,
                   const std::string &grammar_source)
    : vocab(input), top_count(request.top_logprobs),
      report(request.logprobs != 0) {
  validate_sampling(request);
  const int32_t count = llama_vocab_n_tokens(vocab);
  std::vector<llama_logit_bias> biases;
  for (size_t i = 0; i < request.logit_bias_count; ++i) {
    const auto &value = request.logit_bias[i];
    if (value.token_id >= count)
      throw failure(AILO_INVALID_ARGUMENT,
                    "Logit bias token ID is outside the model vocabulary");
    biases.push_back({value.token_id, static_cast<float>(value.bias)});
  }
  processors.reset(
      llama_sampler_chain_init(llama_sampler_chain_default_params()));
  selector.reset(
      llama_sampler_chain_init(llama_sampler_chain_default_params()));
  if (!processors || !selector)
    throw std::bad_alloc();
  if (!grammar_source.empty()) {
    grammar.reset(
        llama_sampler_init_grammar(vocab, grammar_source.c_str(), "root"));
    if (!grammar)
      throw failure(AILO_UNSUPPORTED,
                    "The compiled output grammar cannot be initialized");
  }
  auto add = [](llama_sampler *chain, llama_sampler *next) {
    if (!next)
      throw std::bad_alloc();
    llama_sampler_chain_add(chain, next);
  };
  if (!biases.empty())
    add(processors.get(),
        llama_sampler_init_logit_bias(count, biases.size(), biases.data()));
  add(processors.get(), llama_sampler_init_penalties(
                            count, static_cast<int32_t>(context_tokens), 1.0f,
                            static_cast<float>(request.frequency_penalty),
                            static_cast<float>(request.presence_penalty)));
  if (request.temperature > 0) {
    add(selector.get(),
        llama_sampler_init_top_p(
            static_cast<float>(request.top_p ? request.top_p : 1), 1));
    add(selector.get(),
        llama_sampler_init_temp(static_cast<float>(request.temperature)));
    add(selector.get(),
        llama_sampler_init_dist(request.seed == LLAMA_DEFAULT_SEED
                                    ? request.seed
                                    : request.seed + candidate));
  } else {
    add(selector.get(), llama_sampler_init_greedy());
  }
  scratch.resize(static_cast<size_t>(count));
  if (report) {
    before.resize(scratch.size());
    ranking.resize(scratch.size());
    alternatives.resize(
        std::min(static_cast<size_t>(top_count), scratch.size()));
    pieces.resize(alternatives.size());
  }
}

void sampling::accept(llama_token token) {
  llama_sampler_accept(processors.get(), token);
  llama_sampler_accept(selector.get(), token);
}

llama_token sampling::sample(llama_context *context, ailo_token_info &info) {
  const auto *logits = llama_get_logits_ith(context, -1);
  if (!logits)
    throw failure(AILO_MODEL_FAILED, "Native sampling logits are unavailable");
  for (size_t i = 0; i < scratch.size(); ++i) {
    if (std::isnan(logits[i]) || logits[i] == INFINITY)
      throw failure(AILO_MODEL_FAILED, "Native sampling logits are not finite");
    scratch[i] = {static_cast<llama_token>(i), logits[i], 0};
  }
  llama_token_data_array values{scratch.data(), scratch.size(), -1, false};
  llama_sampler_apply(processors.get(), &values);
  if (grammar)
    llama_sampler_apply(grammar.get(), &values);
  size_t eligible = 0;
  for (const auto &value : scratch) {
    if (std::isnan(value.logit) || value.logit == INFINITY)
      throw failure(AILO_MODEL_FAILED, "Sampling produced invalid logits");
    eligible += std::isfinite(value.logit) ? 1 : 0;
  }
  if (!eligible)
    throw failure(
        AILO_MODEL_FAILED,
        "No token satisfies the output grammar and sampling constraints");
  const size_t active_top = std::min(eligible, alternatives.size());
  // Record the pre-selector distribution. Selectors may reorder or truncate
  // values, so only a vocabulary-sized probability scratch is retained.
  double normalizer = 0;
  double maximum = -INFINITY;
  if (report) {
    for (const auto &value : scratch)
      maximum = std::max(maximum, static_cast<double>(value.logit));
    for (const auto &value : scratch) {
      normalizer += std::exp(static_cast<double>(value.logit) - maximum);
    }
    normalizer = std::log(normalizer);
    std::iota(ranking.begin(), ranking.end(), size_t{0});
    std::partial_sort(ranking.begin(), ranking.begin() + active_top,
                      ranking.end(), [&](size_t a, size_t b) {
                        return scratch[a].logit == scratch[b].logit
                                   ? scratch[a].id < scratch[b].id
                                   : scratch[a].logit > scratch[b].logit;
                      });
    for (size_t i = 0; i < active_top; ++i) {
      const auto &value = scratch[ranking[i]];
      pieces[i] = token_piece(vocab, value.id,
                              grammar && !llama_vocab_is_eog(vocab, value.id));
      alternatives[i] = {value.id, pieces[i].data(), pieces[i].size(),
                         (static_cast<double>(value.logit) - maximum) -
                             normalizer};
    }
  }
  // Native selectors overwrite p as well as logits. Preserve the chosen
  // token's pre-selection logit in a second float vector only when requested.
  if (report) {
    for (const auto &value : scratch)
      before[static_cast<size_t>(value.id)] = value.logit;
  }
  llama_sampler_apply(selector.get(), &values);
  if (values.selected < 0 ||
      static_cast<size_t>(values.selected) >= values.size)
    throw failure(AILO_MODEL_FAILED, "Native sampler did not select a token");
  const auto token = values.data[values.selected].id;
  if (token < 0 || static_cast<size_t>(token) >= scratch.size())
    throw failure(AILO_MODEL_FAILED,
                  "Native sampler selected an invalid token");
  if (report) {
    chosen_piece =
        token_piece(vocab, token, grammar && !llama_vocab_is_eog(vocab, token));
    info.chosen = {
        token, chosen_piece.data(), chosen_piece.size(),
        (static_cast<double>(before[static_cast<size_t>(token)]) - maximum) -
            normalizer};
    info.top = alternatives.data();
    info.top_count = active_top;
    info.end_of_generation = llama_vocab_is_eog(vocab, token);
  }
  accept(token);
  if (grammar)
    llama_sampler_accept(grammar.get(), token);
  return token;
}
} // namespace client
} // namespace ailo
