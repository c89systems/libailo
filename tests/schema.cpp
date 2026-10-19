#include "ailo_schema_internal.hpp"
#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace ailo::client;
#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition))                                                          \
      throw std::runtime_error(#condition);                                    \
  } while (0)

bool accepts(const llama_vocab *vocab, const std::string &grammar,
             const std::string &value) {
  std::unique_ptr<llama_sampler, decltype(&llama_sampler_free)> sampler(
      llama_sampler_init_grammar(vocab, grammar.c_str(), "root"),
      llama_sampler_free);
  CHECK(sampler);
  auto allowed = [&](llama_token token) {
    std::vector<llama_token_data> data(
        static_cast<size_t>(llama_vocab_n_tokens(vocab)));
    for (size_t i = 0; i < data.size(); ++i)
      data[i] = {static_cast<llama_token>(i), 0, 0};
    llama_token_data_array view{data.data(), data.size(), -1, false};
    llama_sampler_apply(sampler.get(), &view);
    return std::isfinite(data[static_cast<size_t>(token)].logit);
  };
  for (unsigned char ch : value) {
    const auto token = static_cast<llama_token>(ch) + 3;
    if (!allowed(token))
      return false;
    llama_sampler_accept(sampler.get(), token);
  }
  if (!allowed(2))
    return false;
  json_validation validation(16384);
  for (char ch : value)
    validation.push(&ch, 1);
  validation.finish();
  return true;
}

std::string compile(const char *schema) {
  auto request = ailo_request_default();
  request.output_format = AILO_OUTPUT_JSON_SCHEMA;
  request.json_schema = schema;
  return output_grammar(request);
}

void grammar_contract(const llama_vocab *vocab) {
  const auto object = compile(
      R"({"type":"object","properties":{"x":{"type":"integer","minimum":-2,"maximum":3}},"required":["x"],"additionalProperties":false})");
  CHECK(accepts(vocab, object, "{\"x\":-2}"));
  CHECK(accepts(vocab, object, "{\"x\":3}"));
  for (const char *bad :
       {"{}", "{\"x\":4}", "{\"x\":-3}", "{\"x\":2.5}", "{\"x\":1,\"y\":2}"})
    CHECK(!accepts(vocab, object, bad));
  const auto array = compile(
      R"({"type":"array","items":{"enum":[true,null]},"minItems":1,"maxItems":2})");
  CHECK(accepts(vocab, array, "[true,null]"));
  CHECK(!accepts(vocab, array, "[]"));
  CHECK(!accepts(vocab, array, "[true,true,true]"));
  CHECK(!accepts(vocab, array, "[false]"));
  const auto ref = compile(
      R"({"$defs":{"a/b":{"type":["string","null"]}},"$ref":"#/$defs/a~1b"})");
  CHECK(accepts(vocab, ref, "null"));
  CHECK(accepts(vocab, ref, "\"ok\""));
  CHECK(!accepts(vocab, ref, "3"));
  const auto alt = compile(
      R"({"anyOf":[{"const":true},{"type":"string","enum":["a","b"]}]})");
  CHECK(accepts(vocab, alt, "true"));
  CHECK(accepts(vocab, alt, "\"a\""));
  CHECK(!accepts(vocab, alt, "false"));
  CHECK(!accepts(vocab, alt, "\"c\""));
  const auto additional = compile(
      R"({"type":"object","properties":{"x":{"type":"boolean"}},"additionalProperties":{"type":"null"}})");
  CHECK(accepts(vocab, additional, "{\"z\":null}"));
  CHECK(!accepts(vocab, additional, "{\"z\":true}"));
  const auto defaults =
      compile(R"({"type":"object","properties":{"x":{"type":"boolean"}}})");
  CHECK(accepts(vocab, defaults, "{\"z\":1}"));
  for (
      const char *invalid :
      {R"({"type":"number","minimum":1})",
       R"({"oneOf":[{"type":"number"},{"type":"integer"}]})",
       R"({"type":"string","pattern":"^a$"})",
       R"({"type":"object","properties":{"x":{"type":"integer","multipleOf":2}}})",
       R"({"type":"integer","minimum":4,"maximum":3})",
       R"({"type":"object","required":["missing"]})",
       R"({"type":"array","minItems":2,"maxItems":1})",
       R"({"$defs":{"x":{"$ref":"#/$defs/x"}},"$ref":"#/$defs/x"})",
       R"({"$ref":"https://example.invalid/schema"})",
       R"({"$ref":"#/$defs/missing"})", R"({"type":"string","type":"integer"})",
       R"({"type":"string","const":1})", R"({"type":"string","unknown":true})",
       "{"}) {
    try {
      compile(invalid);
      CHECK(false);
    } catch (const failure &e) {
      CHECK(e.status == AILO_INVALID_ARGUMENT || e.status == AILO_UNSUPPORTED);
    }
  }
  const std::string oversized(65537, ' ');
  try {
    compile(oversized.c_str());
    CHECK(false);
  } catch (const failure &e) {
    CHECK(e.status == AILO_OUT_OF_RANGE);
  }
  json_validation incomplete(1024);
  incomplete.push("{", 1);
  try {
    incomplete.finish();
    CHECK(false);
  } catch (const failure &e) {
    CHECK(e.status == AILO_MODEL_FAILED);
  }
}

struct observation {
  std::string text;
  unsigned deltas = 0, complete = 0;
  unsigned choice_deltas = 0, prompt_base = 0, token_events = 0;
  int action = 1;
};
int observe(void *data, const ailo_event *event) {
  auto &seen = *static_cast<observation *>(data);
  if (event->kind == AILO_EVENT_CHOICE_BEGIN) {
    seen.choice_deltas = 0;
    seen.prompt_base = event->usage.completion_tokens.lo;
  }
  if (event->kind == AILO_EVENT_TOKEN) {
    CHECK(event->token && event->token->top_count > 0 &&
          event->token->top_count <= 5);
    CHECK(std::isfinite(event->token->chosen.logprob) &&
          event->token->chosen.logprob <= 0);
    if (!seen.token_events)
      CHECK(event->token->chosen.logprob == 0);
    ++seen.token_events;
  }
  if (event->kind == AILO_EVENT_TEXT_DELTA) {
    ++seen.deltas;
    seen.text.append(static_cast<const char *>(event->data), event->size);
    CHECK(event->usage.completion_tokens.lo ==
          seen.prompt_base + ++seen.choice_deltas);
    return seen.action;
  }
  if (event->kind == AILO_EVENT_RUN_END)
    ++seen.complete;
  return 1;
}

void receiver_contract(const char *path) {
  ailo_client_config config{};
  config.source.local_path = path;
  config.backend = AILO_BACKEND_CPU;
  ailo_client *raw = nullptr;
  ailo_error error{};
  CHECK(ailo_client_new(&config, &raw, &error) == AILO_OK);
  std::unique_ptr<ailo_client, decltype(&ailo_client_destroy)> client(
      raw, ailo_client_destroy);
  ailo_content content{AILO_CONTENT_TEXT, "hello", 5, nullptr};
  ailo_item item{};
  item.content = &content;
  item.content_count = 1;
  auto request = ailo_request_default();
  request.items = &item;
  request.item_count = 1;
  request.temperature = 0;
  request.max_tokens = 32;
  request.output_format = AILO_OUTPUT_JSON_SCHEMA;
  request.json_schema = R"({"const":{"answer":4}})";
  request.logprobs = 1;
  request.top_logprobs = 5;
  ailo_logit_bias bias[] = {{2, 100}};
  request.logit_bias = bias;
  request.logit_bias_count = 1;
  ailo_run_info info{};
  observation seen;
  CHECK(client->generate(client.get(), &request, observe, &seen, &info,
                         &error) == AILO_OK);
  CHECK(seen.text == "{\"answer\":4}" && seen.deltas == seen.text.size());
  CHECK(info.finish_reason == AILO_FINISH_STOP && seen.complete == 1);
  CHECK(seen.token_events == seen.deltas + 1);
  request.candidates = 2;
  seen = {};
  CHECK(client->generate(client.get(), &request, observe, &seen, &info,
                         &error) == AILO_OK);
  CHECK(seen.text == "{\"answer\":4}{\"answer\":4}" &&
        info.completed_candidates == 2);
  request.candidates = 1;
  for (int action : {0, -1}) {
    seen = {};
    seen.action = action;
    CHECK(client->generate(client.get(), &request, observe, &seen, &info,
                           &error) ==
          (action ? AILO_SINK_FAILED : AILO_CANCELLED));
    CHECK(seen.deltas == 1 && !seen.complete);
  }
  request.max_tokens = 1;
  seen = {};
  CHECK(client->generate(client.get(), &request, observe, &seen, &info,
                         &error) == AILO_OK);
  CHECK(seen.text == "{" && info.finish_reason == AILO_FINISH_LENGTH);
  request.max_tokens = 32;
  const char *stop[] = {"4"};
  request.stop = stop;
  request.stop_count = 1;
  seen = {};
  CHECK(client->generate(client.get(), &request, observe, &seen, &info,
                         &error) == AILO_MODEL_FAILED);
  CHECK(!seen.complete && !info.completed_candidates);
  request.stop = nullptr;
  request.stop_count = 0;
  seen = {};
  CHECK(client->generate(client.get(), &request, observe, &seen, &info,
                         &error) == AILO_OK);
  CHECK(seen.text == "{\"answer\":4}");
  // Grammar matching includes special-token text. Suppressing a matched BOS
  // piece would silently change this constant to an empty JSON string.
  request.json_schema = R"({"const":"<s>"})";
  ailo_logit_bias special_bias[] = {{1, 100}, {2, 100}};
  request.logit_bias = special_bias;
  request.logit_bias_count = 2;
  seen = {};
  CHECK(client->generate(client.get(), &request, observe, &seen, &info,
                         &error) == AILO_OK);
  CHECK(seen.text == "\"<s>\"");
  request.output_format = AILO_OUTPUT_JSON_OBJECT;
  request.json_schema = nullptr;
  ailo_logit_bias object_bias[] = {{2, 100}, {100, -100}, {128, 100}};
  request.logit_bias = object_bias;
  request.logit_bias_count = 3;
  seen = {};
  CHECK(client->generate(client.get(), &request, observe, &seen, &info,
                         &error) == AILO_OK);
  CHECK(seen.text == "{}" && info.finish_reason == AILO_FINISH_STOP);
}

int main(int argc, char **argv) {
  try {
    CHECK(argc == 2);
    ailo_client_config config{};
    config.source.local_path = argv[1];
    config.backend = AILO_BACKEND_CPU;
    state self(config, nullptr);
    self.ensure_loaded();
    grammar_contract(llama_model_get_vocab(self.model.get()));
    receiver_contract(argv[1]);
    std::puts("Schema constraints, immediate deltas, incomplete output and "
              "failure recovery passed");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "schema: %s\n", e.what());
    return 1;
  }
}
