#include "ailo_sampling_internal.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

using namespace ailo::client;
#define CHECK(value)                                                           \
  do {                                                                         \
    if (!(value))                                                              \
      throw std::runtime_error(#value);                                        \
  } while (0)

struct observation {
  unsigned events = 0, tokens = 0, deltas = 0, ends = 0;
  unsigned alternatives = 3;
  int action = 1;
  ailo_i32 chosen = 100;
  unsigned candidate = 0, index = 0;
  ailo_u64 sequence{};
  bool eog = false;
  std::string text;
};

int observe(void *user, const ailo_event *event) {
  auto &seen = *static_cast<observation *>(user);
  CHECK(!event->sequence.hi && event->sequence.lo == seen.events++);
  if (event->kind != AILO_EVENT_TOKEN)
    CHECK(!event->token);
  if (event->kind == AILO_EVENT_CHOICE_BEGIN) {
    seen.candidate = event->candidate;
    seen.index = 0;
  }
  if (event->kind == AILO_EVENT_TOKEN) {
    const auto &token = *event->token;
    CHECK(event->candidate == seen.candidate);
    CHECK(!token.index.hi && token.index.lo == seen.index++);
    CHECK(!token.byte_offset.hi && token.byte_offset.lo == token.index.lo);
    CHECK(token.chosen.token_id == seen.chosen);
    CHECK(token.end_of_generation == seen.eog);
    CHECK(token.top_count == seen.alternatives);
    CHECK(std::isfinite(token.chosen.logprob) && token.chosen.logprob <= 0);
    if (token.top_count) {
      CHECK(token.top[0].token_id == token.chosen.token_id);
      CHECK(token.top[0].logprob == token.chosen.logprob);
    }
    for (size_t i = 1; i < token.top_count; ++i)
      CHECK(token.top[i - 1].logprob >= token.top[i].logprob);
    if (!seen.eog) {
      CHECK(token.chosen.size == 1);
      CHECK(*static_cast<const char *>(token.chosen.data) == seen.chosen - 3);
    }
    ++seen.tokens;
    // A token callback always precedes its text and any next evaluation.
    CHECK(event->usage.completion_tokens.lo == seen.tokens);
    return seen.action;
  }
  if (event->kind == AILO_EVENT_TEXT_DELTA) {
    CHECK(seen.tokens > seen.deltas);
    ++seen.deltas;
    seen.text.append(static_cast<const char *>(event->data), event->size);
  }
  if (event->kind == AILO_EVENT_RUN_END)
    ++seen.ends;
  return 1;
}

void public_contract(const char *path) {
  ailo_error error{};
  ailo_client_config config = ailo_client_default_config();
  config.source.local_path = path;
  config.backend = AILO_BACKEND_CPU;
  config.context_tokens = 128;
  ailo_client *raw = nullptr;
  CHECK(ailo_client_new(&config, &raw, &error) == AILO_OK);
  std::unique_ptr<ailo_client, decltype(&ailo_client_destroy)> client(
      raw, ailo_client_destroy);
  ailo_content content{AILO_CONTENT_TEXT, "hello", 5};
  ailo_item item{};
  item.content = &content;
  item.content_count = 1;
  ailo_request request = ailo_request_default();
  request.items = &item;
  request.item_count = 1;
  request.temperature = 0;
  request.max_tokens = 3;
  request.logprobs = 1;
  request.top_logprobs = 3;
  ailo_run_info info{};
  observation seen;
  CHECK(client->generate(client.get(), &request, observe, &seen, &info,
                         &error) == AILO_OK);
  CHECK(seen.text == "aaa" && seen.tokens == 3 && seen.ends == 1);
  request.candidates = 2;
  seen = {};
  CHECK(client->generate(client.get(), &request, observe, &seen, &info,
                         &error) == AILO_OK);
  CHECK(seen.text == "aaaaaa" && seen.tokens == 6 &&
        info.completed_candidates == 2);
  request.candidates = 1;
  ailo_logit_bias biases[] = {{100, -100}, {101, 100}};
  request.logit_bias = biases;
  request.logit_bias_count = 2;
  seen = {};
  seen.chosen = 101;
  CHECK(client->generate(client.get(), &request, observe, &seen, &info,
                         &error) == AILO_OK);
  CHECK(seen.text == "bbb");
  request.logit_bias = nullptr;
  request.logit_bias_count = 0;
  for (int action : {0, -1}) {
    seen = {};
    seen.action = action;
    CHECK(client->generate(client.get(), &request, observe, &seen, &info,
                           &error) ==
          (action ? AILO_SINK_FAILED : AILO_CANCELLED));
    CHECK(seen.tokens == 1 && !seen.deltas && !seen.ends &&
          !info.completed_candidates);
  }
  request.max_output_bytes = 1;
  seen = {};
  CHECK(client->generate(client.get(), &request, observe, &seen, &info,
                         &error) == AILO_OUT_OF_RANGE);
  CHECK(seen.text == "a" && !seen.ends && !info.completed_candidates);
  CHECK(info.usage.completion_tokens.lo == 2);
  request.max_output_bytes = 0;
  const char *stops[] = {"aa"};
  request.stop = stops;
  request.stop_count = 1;
  seen = {};
  CHECK(client->generate(client.get(), &request, observe, &seen, &info,
                         &error) == AILO_OK);
  CHECK(seen.tokens == 2 && seen.text.empty() &&
        info.finish_reason == AILO_FINISH_STOP);
  request.stop = nullptr;
  request.stop_count = 0;
  for (int mode = 0; mode < 9; ++mode) {
    auto invalid = request;
    ailo_logit_bias bad[] = {{100, 0}, {100, 1}};
    invalid.logit_bias = bad;
    invalid.logit_bias_count = 1;
    switch (mode) {
    case 0:
      invalid.top_logprobs = 21;
      break;
    case 1:
      invalid.logprobs = 0;
      break;
    case 2:
      invalid.logit_bias = nullptr;
      break;
    case 3:
      bad[0].token_id = -1;
      break;
    case 4:
      bad[0].token_id = 1000000;
      break;
    case 5:
      bad[0].bias = std::numeric_limits<double>::quiet_NaN();
      break;
    case 6:
      invalid.logit_bias_count = 2;
      break;
    case 7:
      bad[0].bias = 100.01;
      break;
    case 8:
      invalid.logit_bias_count = 4097;
      break;
    }
    seen = {};
    CHECK(client->generate(client.get(), &invalid, observe, &seen, &info,
                           &error) == AILO_INVALID_ARGUMENT);
    CHECK(!seen.events && !info.completed_candidates);
  }
  // Failures cannot poison the next generation or retain old biases.
  seen = {};
  CHECK(client->generate(client.get(), &request, observe, &seen, &info,
                         &error) == AILO_OK);
  CHECK(seen.text == "aaa");
  // A token reporting only the chosen probability needs no alternatives.
  request.top_logprobs = 0;
  seen = {};
  seen.alternatives = 0;
  CHECK(client->generate(client.get(), &request, observe, &seen, &info,
                         &error) == AILO_OK);
  const std::string eos_path = std::string(path) + ".eos";
  config.source.local_path = eos_path.c_str();
  CHECK(ailo_client_new(&config, &raw, &error) == AILO_OK);
  client.reset(raw);
  seen = {};
  seen.alternatives = 0;
  seen.chosen = 2;
  seen.eog = true;
  CHECK(client->generate(client.get(), &request, observe, &seen, &info,
                         &error) == AILO_OK);
  CHECK(seen.tokens == 1 && seen.text.empty() &&
        info.finish_reason == AILO_FINISH_STOP);
}

void numeric_contract(const char *path) {
  ailo_client_config config{};
  config.source.local_path = path;
  config.backend = AILO_BACKEND_CPU;
  state self(config, nullptr);
  self.ensure_loaded();
  llama_token prompt = 100;
  CHECK(llama_decode(self.context.get(), llama_batch_get_one(&prompt, 1)) == 0);
  const auto *vocab = llama_model_get_vocab(self.model.get());
  auto request = ailo_request_default();
  request.temperature = 0;
  request.logprobs = 1;
  request.top_logprobs = 20;
  // Prompt acceptance contributes exactly one frequency and presence penalty.
  request.frequency_penalty = 0.25;
  request.presence_penalty = 0.125;
  ailo_logit_bias bias{100, 0.5};
  request.logit_bias = &bias;
  request.logit_bias_count = 1;
  sampling sampler(vocab, request, 0, llama_n_ctx(self.context.get()));
  sampler.accept(prompt);
  const auto *logits = llama_get_logits_ith(self.context.get(), -1);
  std::vector<double> adjusted;
  double sum = 0;
  for (int32_t i = 0; i < llama_vocab_n_tokens(vocab); ++i) {
    // Native processors perform float arithmetic, in this defined order.
    float value = logits[i];
    if (i == 100) {
      value += 0.5f;
      value -= 0.25f;
      value -= 0.125f;
    }
    adjusted.push_back(value);
    sum += std::exp(static_cast<double>(value));
  }
  ailo_token_info result{};
  CHECK(sampler.sample(self.context.get(), result) == 100);
  CHECK(result.chosen.logprob < -0.01); // Greedy does not mean probability 1.
  CHECK(std::abs(result.chosen.logprob - adjusted[100] + std::log(sum)) < 1e-6);
  for (size_t i = 0; i < result.top_count; ++i)
    CHECK(std::abs(result.top[i].logprob - adjusted[result.top[i].token_id] +
                   std::log(sum)) < 1e-6);
  // Large finite logits still form a uniform distribution, not probability 1.
  auto *mutable_logits = llama_get_logits_ith(self.context.get(), -1);
  for (int32_t i = 0; i < llama_vocab_n_tokens(vocab); ++i)
    mutable_logits[i] = 1e30f;
  request.logit_bias_count = 0;
  request.frequency_penalty = request.presence_penalty = 0;
  sampling uniform(vocab, request, 0, llama_n_ctx(self.context.get()));
  CHECK(uniform.sample(self.context.get(), result) == 0);
  CHECK(std::abs(result.chosen.logprob +
                 std::log(llama_vocab_n_tokens(vocab))) < 1e-9);
  for (size_t i = 0; i < result.top_count; ++i) {
    CHECK(result.top[i].token_id == static_cast<ailo_i32>(i));
    CHECK(result.top[i].logprob == result.chosen.logprob);
  }
  mutable_logits[0] = std::numeric_limits<float>::quiet_NaN();
  try {
    uniform.sample(self.context.get(), result);
    CHECK(false);
  } catch (const failure &e) {
    CHECK(e.status == AILO_MODEL_FAILED);
  }
}

int main(int argc, char **argv) {
  try {
    CHECK(argc == 2);
    public_contract(argv[1]);
    numeric_contract(argv[1]);
    std::puts("Generation controls: bias, probabilities, streaming and failure "
              "recovery passed");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "generation controls: %s\n", e.what());
    return 1;
  }
}
