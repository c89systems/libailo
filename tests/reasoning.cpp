#include "ailo/continuation.h"
#include "ailo_reasoning_internal.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

using namespace ailo::client;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c))                                                                  \
      throw std::runtime_error(std::string(#c) + " at " +                      \
                               std::to_string(__LINE__));                      \
  } while (0)
const char *chat =
    R"({% if tools %}{{ tools | tojson }}{% endif %}{% for m in messages %}{{ m.role }}:{% if m.reasoning_content is defined %}<think>{{ m.reasoning_content }}</think>{% endif %}{{ m.content }}{% if m.tool_calls is defined %}{% for c in m.tool_calls %}<call>{"name":{{ c.function.name | tojson }},"arguments":{% if c.function.arguments is string %}{{ c.function.arguments }}{% else %}{{ c.function.arguments | tojson }}{% endif %}}</call>{% endfor %}{% endif %}|{% endfor %}{% if add_generation_prompt %}assistant:{% if enable_thinking %}<think>{% endif %}{% endif %})";
struct observed {
  std::string reasoning{}, answer{}, arguments{}, active{}, reason_id{};
  unsigned reason_deltas = 0, answer_deltas = 0, events = 0;
  int reject = 1;
  bool done = false;
};
int sink(void *user, const ailo_event *event) {
  auto &out = *static_cast<observed *>(user);
  ++out.events;
  if (event->kind == AILO_EVENT_ITEM_BEGIN) {
    out.active = event->item_id;
    if (event->item_kind == AILO_ITEM_REASONING) {
      CHECK(out.active.rfind("rs_", 0) == 0);
      out.reason_id = out.active;
    }
  }
  if (event->kind == AILO_EVENT_REASONING_DELTA) {
    CHECK(!out.done && event->item_kind == AILO_ITEM_REASONING &&
          event->item_id == out.active);
    out.reasoning.append(static_cast<const char *>(event->data), event->size);
    ++out.reason_deltas;
    return out.reject;
  }
  if (event->kind == AILO_EVENT_TEXT_DELTA) {
    CHECK(!out.done && event->item_kind == AILO_ITEM_MESSAGE &&
          out.active != out.reason_id);
    out.answer.append(static_cast<const char *>(event->data), event->size);
    ++out.answer_deltas;
  }
  if (event->kind == AILO_EVENT_FUNCTION_ARGUMENTS_DELTA) {
    CHECK(!out.done && event->item_kind == AILO_ITEM_FUNCTION_CALL);
    out.arguments.append(static_cast<const char *>(event->data), event->size);
  }
  if (event->kind == AILO_EVENT_RUN_END)
    out.done = true;
  return 1;
}
int main(int argc, char **argv) {
  ailo_error error{};
  try {
    CHECK(argc == 2);
    for (bool prompt_open : {false, true}) {
      const reasoning_format format{"<think>", "</think>", prompt_open};
      std::string reasoning, answer;
      unsigned ends = 0;
      reasoning_stream stream(format, [&](reasoning_output_kind kind,
                                          const char *data, size_t size) {
        if (kind == reasoning_output_kind::reasoning) {
          CHECK(!ends);
          reasoning.append(data, size);
        } else if (kind == reasoning_output_kind::answer) {
          CHECK(ends == 1);
          answer.append(data, size);
        } else
          ++ends;
      });
      if (!prompt_open)
        for (char c : format.open)
          stream.push(&c, 1);
      const std::string large(1024 * 1024, 'x');
      stream.push(large.data(), large.size());
      CHECK(reasoning.size() == large.size() && !ends);
      const std::string tail = "\xe2\x82\xac</th</think>answer\xf0\x9f\x98\x80";
      for (char c : tail)
        stream.push(&c, 1);
      stream.finish(false);
      CHECK(reasoning == large + "\xe2\x82\xac</th" &&
            answer == "answer\xf0\x9f\x98\x80" && ends == 1);
    }
    {
      std::string reason;
      reasoning_stream stream(
          {"[THINK]", "[/THINK]", true},
          [&](reasoning_output_kind kind, const char *data, size_t size) {
            if (kind == reasoning_output_kind::reasoning)
              reason.append(data, size);
          });
      stream.push("abc[/TH", 7);
      stream.finish(true);
      CHECK(reason == "abc");
      bool failed = false;
      try {
        stream.finish(false);
      } catch (const failure &e) {
        failed = e.status == AILO_MODEL_FAILED;
      }
      CHECK(failed);
    }
    const std::string path = std::string(argv[1]) + "-reasoning";
    {
      std::string reason, answer;
      reasoning_stream stream(
          {"<think>", "\n</think>\n\n", true},
          [&](reasoning_output_kind kind, const char *data, size_t size) {
            if (kind == reasoning_output_kind::reasoning)
              reason.append(data, size);
            if (kind == reasoning_output_kind::answer)
              answer.append(data, size);
          });
      stream.push("r</think>\n\nok", 13);
      stream.finish(false);
      CHECK(reason == "r" && answer == "ok");
    }
    ailo_client_config config{};
    config.source.local_path = path.c_str();
    config.chat_template = chat;
    config.backend = AILO_BACKEND_CPU;
    config.context_tokens = 1024;
    config.threads = 1;
    ailo_client *raw = nullptr;
    CHECK(ailo_client_new(&config, &raw, &error) == AILO_OK);
    std::unique_ptr<ailo_client, decltype(&ailo_client_destroy)> client(
        raw, ailo_client_destroy);
    auto *native = static_cast<state *>(client->state);
    const auto vocab = llama_model_get_vocab(native->model.get());
    const auto grammar = reasoning_grammar(
        {"<think>", "</think>", true}, "root ::= \"ok\" | \"(\" root \")\"\n");
    auto sampler =
        std::unique_ptr<llama_sampler, decltype(&llama_sampler_free)>(
            llama_sampler_init_grammar(vocab, grammar.c_str(),
                                       "reasoning-root"),
            llama_sampler_free);
    CHECK(sampler);
    const auto padded = reasoning_grammar({"<think>", "\n</think>\n\n", true},
                                          "root ::= \"ok\"\n");
    sampler.reset(
        llama_sampler_init_grammar(vocab, padded.c_str(), "reasoning-root"));
    CHECK(sampler);
    for (unsigned char ch : std::string("\n</think>"))
      llama_sampler_accept(sampler.get(), ch + 3);
    llama_token_data choices[] = {{13, 0, 0}, {123, 0, 0}};
    llama_token_data_array candidates{choices, 2, -1, false};
    llama_sampler_apply(sampler.get(), &candidates);
    CHECK(std::isfinite(choices[0].logit) && !std::isfinite(choices[1].logit));
    ailo_content content{AILO_CONTENT_TEXT, "question", 8, nullptr};
    ailo_item item{};
    item.content = &content;
    item.content_count = 1;
    auto request = ailo_request_default();
    request.items = &item;
    request.item_count = 1;
    request.max_tokens = 128;
    request.temperature = 0;
    request.output_format = AILO_OUTPUT_JSON_SCHEMA;
    request.json_schema = "{\"const\":{\"ok\":true}}";
    ailo_logit_bias biases[] = {{260, 100}, {2, 80}};
    request.logit_bias = biases;
    request.logit_bias_count = 2;
    ailo_run_info run{};
    auto generate = [&](observed &out) {
      return client->generate(client.get(), &request, sink, &out, &run, &error);
    };
    observed out;
    CHECK(generate(out) == AILO_OK);
    CHECK(out.done && out.reasoning == "r" && out.reason_deltas == 1 &&
          out.answer == "{\"ok\":true}" && out.answer_deltas > 1);
    CHECK(run.usage.reasoning_tokens.lo == 1 &&
          run.usage.completion_tokens.lo > run.usage.reasoning_tokens.lo);
    const char *stop = "r";
    request.stop = &stop;
    request.stop_count = 1;
    out = {};
    CHECK(generate(out) == AILO_MODEL_FAILED && !out.done);
    request.stop = nullptr;
    request.stop_count = 0;
    request.max_output_bytes = 1;
    out = {};
    CHECK(generate(out) == AILO_OUT_OF_RANGE && !out.done);
    request.max_output_bytes = 0;
    ailo_function_tool tool{"lookup", nullptr, "{\"const\":{\"x\":1}}", 1};
    request.tools = &tool;
    request.tool_count = 1;
    request.tool_choice = AILO_TOOLS_NAMED;
    request.tool_name = "lookup";
    request.parallel_tool_calls = 0;
    out = {};
    CHECK(generate(out) == AILO_OK && out.reasoning == "r" &&
          out.arguments == "{\"x\":1}" && out.answer.empty() &&
          run.finish_reason == AILO_FINISH_TOOL_CALLS);
    request.tools = nullptr;
    request.tool_count = 0;
    request.tool_choice = AILO_TOOLS_AUTO;
    request.tool_name = nullptr;
    request.parallel_tool_calls = 1;
    for (int reject : {0, -1}) {
      out = {};
      out.reject = reject;
      CHECK(generate(out) == (reject ? AILO_SINK_FAILED : AILO_CANCELLED));
      CHECK(!out.done && out.reason_deltas == 1 &&
            run.usage.completion_tokens.lo == 1);
    }
    request.candidates = 2;
    out = {};
    CHECK(generate(out) == AILO_OK && out.reasoning == "rr" &&
          run.usage.reasoning_tokens.lo == 2);
    request.candidates = 1;
    request.reasoning = AILO_REASONING_DISABLED;
    out = {};
    CHECK(generate(out) == AILO_OK && out.reasoning.empty() &&
          out.answer == "{\"ok\":true}" && !run.usage.reasoning_tokens.lo);
    request.reasoning = AILO_REASONING_ENABLED;
    request.max_reasoning_tokens = 1;
    biases[0] = {100, 100};
    out = {};
    CHECK(generate(out) == AILO_OK && run.finish_reason == AILO_FINISH_LENGTH &&
          out.answer.empty() && out.reasoning == "a" &&
          run.usage.reasoning_tokens.lo == 1);
    request.max_reasoning_tokens = 0;
    biases[0] = {260, 100};
    request.reasoning_effort = "low";
    out = {};
    CHECK(generate(out) == AILO_UNSUPPORTED && !out.events);
    request.reasoning_effort = nullptr;
    request.reasoning = static_cast<ailo_reasoning_mode>(99);
    out = {};
    CHECK(generate(out) == AILO_INVALID_ARGUMENT && !out.events);
    request.reasoning = AILO_REASONING_AUTO;
    ailo_content trace{AILO_CONTENT_TEXT, "old reasoning", 13, nullptr},
        answer{AILO_CONTENT_TEXT, "old answer", 10, nullptr};
    ailo_item history[4]{};
    history[0] = item;
    history[1].kind = AILO_ITEM_REASONING;
    history[1].content = &trace;
    history[1].content_count = 1;
    history[2].role = AILO_ROLE_ASSISTANT;
    history[2].content = &answer;
    history[2].content_count = 1;
    history[3] = item;
    request.items = history;
    request.item_count = 4;
    out = {};
    CHECK(generate(out) == AILO_OK && out.reasoning == "r");
    trace.size = 0;
    out = {};
    CHECK(generate(out) == AILO_OK && out.reasoning == "r");
    trace.size = 13;
    ailo_continuation *saved = nullptr;
    CHECK(ailo_continuation_new(client.get(), nullptr, &saved, &error) ==
          AILO_OK);
    CHECK(saved->append(saved, history, 4, &error) == AILO_OK);
    request.continuation = saved;
    request.items = nullptr;
    request.item_count = 0;
    out = {};
    CHECK(generate(out) == AILO_OK);
    ailo_continuation_destroy(saved);
    request.continuation = nullptr;
    // Compatible controls must be consumed by the actual rendered prompt.
    const std::string effort_template =
        std::string("{% if reasoning_effort is defined %}{{ reasoning_effort "
                    "}}{% endif %}") +
        chat;
    config.chat_template = effort_template.c_str();
    CHECK(ailo_client_new(&config, &raw, &error) == AILO_OK);
    client.reset(raw);
    request.items = &item;
    request.item_count = 1;
    request.reasoning_effort = "high";
    out = {};
    CHECK(generate(out) == AILO_OK);
    request.reasoning_effort = nullptr;
    config.chat_template = "reasoning_content documentation: {% for m in "
                           "messages %}{{ m.content }}{% endfor %}";
    CHECK(ailo_client_new(&config, &raw, &error) == AILO_OK);
    client.reset(raw);
    out = {};
    CHECK(generate(out) == AILO_OK && out.reasoning.empty());
    std::puts("Bounded reasoning streaming, eager grammar composition, "
              "controls, history, usage and failures passed");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "reasoning: %s (%s)\n", e.what(), error.message);
    return 1;
  }
}
