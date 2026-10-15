#include "ailo_tools_internal.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <stdexcept>
using namespace ailo::client;
#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition))                                                          \
      throw std::runtime_error(std::string(#condition) + " at " +              \
                               std::to_string(__LINE__));                      \
  } while (0)
const char *tool_template =
    "{% if tools %}{{ tools | tojson }}{% endif %}"
    "{% for m in messages %}{{ m.role }}:{{ m.content }}"
    "{% if m.tool_calls is defined %}{% for c in m.tool_calls %}"
    "<call>{\"name\":{{ c.function.name | tojson }},\"arguments\":"
    "{% if c.function.arguments is string %}{{ c.function.arguments }}"
    "{% else %}{{ c.function.arguments | tojson }}{% endif %}}</call>"
    "{% endfor %}{% endif %}|{% endfor %}"
    "{% if add_generation_prompt %}assistant:{% endif %}";
const char *array_template =
    "{% if tools %}{{ tools | tojson }}{% endif %}"
    "{% for m in messages %}{{ m.role }}:{{ m.content }}"
    "{% if m.tool_calls is defined %}<calls>[{% for c in m.tool_calls %}"
    "{\"name\":{{ c.function.name | tojson }},\"arguments\":"
    "{% if c.function.arguments is string %}{{ c.function.arguments }}"
    "{% else %}{{ c.function.arguments | tojson }}{% endif %}}"
    "{% if not loop.last %},{% endif %}{% endfor %}]</calls>{% endif %}|{% "
    "endfor %}"
    "{% if add_generation_prompt %}assistant:{% endif %}";

bool accepts(const llama_vocab *vocab, const std::string &grammar,
             const std::string &value) {
  std::unique_ptr<llama_sampler, decltype(&llama_sampler_free)> sampler(
      llama_sampler_init_grammar(vocab, grammar.c_str(), "root"),
      llama_sampler_free);
  CHECK(sampler);
  auto allowed = [&](llama_token token) {
    std::vector<llama_token_data> values(
        static_cast<size_t>(llama_vocab_n_tokens(vocab)));
    for (size_t i = 0; i < values.size(); ++i)
      values[i] = {static_cast<llama_token>(i), 0, 0};
    llama_token_data_array view{values.data(), values.size(), -1, false};
    llama_sampler_apply(sampler.get(), &view);
    return std::isfinite(values[static_cast<size_t>(token)].logit);
  };
  for (unsigned char ch : value) {
    const auto token = static_cast<llama_token>(ch) + 3;
    if (!allowed(token))
      return false;
    llama_sampler_accept(sampler.get(), token);
  }
  return allowed(2);
}
struct observation {
  size_t events = 0, deltas = 0, calls = 0, terminals = 0;
  int action = 1;
  bool active = false;
  std::string name, arguments, id;
  std::set<std::string> ids;
};
int observe(void *data, const ailo_event *event) {
  auto &seen = *static_cast<observation *>(data);
  ++seen.events;
  if (event->kind == AILO_EVENT_ITEM_BEGIN) {
    CHECK(!seen.active && event->item_kind == AILO_ITEM_FUNCTION_CALL);
    CHECK(event->call_id && event->item_id &&
          std::strcmp(event->call_id, event->item_id));
    CHECK(seen.ids.insert(event->call_id).second);
    seen.id = event->call_id;
    seen.name.clear();
    seen.arguments.clear();
    seen.active = true;
  }
  if (event->kind == AILO_EVENT_FUNCTION_NAME_DELTA) {
    CHECK(seen.active && seen.id == event->call_id);
    seen.name.append(static_cast<const char *>(event->data), event->size);
  }
  if (event->kind == AILO_EVENT_FUNCTION_ARGUMENTS_DELTA) {
    CHECK(seen.active && seen.id == event->call_id && seen.name == "lookup");
    CHECK(!seen.calls); // Available before the call or generation completes.
    CHECK(event->usage.completion_tokens.lo > 0 &&
          event->usage.completion_tokens.lo < 256);
    ++seen.deltas;
    seen.arguments.append(static_cast<const char *>(event->data), event->size);
    return seen.action;
  }
  if (event->kind == AILO_EVENT_ITEM_END) {
    CHECK(seen.active && seen.name == event->name && seen.id == event->call_id);
    if (event->finish_reason == AILO_FINISH_TOOL_CALLS)
      CHECK(seen.arguments == "{\"answer\":4}");
    seen.active = false;
    ++seen.calls;
  }
  if (event->kind == AILO_EVENT_CHOICE_BEGIN)
    seen.calls = 0;
  if (event->kind == AILO_EVENT_RUN_END)
    ++seen.terminals;
  CHECK(event->kind != AILO_EVENT_TEXT_DELTA);
  return 1;
}
int main(int argc, char **argv) {
  try {
    CHECK(argc == 2);
    ailo_client_config config{};
    config.source.local_path = argv[1];
    config.backend = AILO_BACKEND_CPU;
    config.context_tokens = 1024;
    config.chat_template = tool_template;
    state self(config, nullptr);
    self.ensure_loaded();
    const auto *vocab = llama_model_get_vocab(self.model.get());
    ailo_function_tool definitions[] = {
        {"lookup", "Find an answer",
         R"({"const":{"answer":4},"description":"metadata-marker"})", 1},
        {"other", "Another function", nullptr, 0}};
    const char *allowed[] = {"lookup"};
    auto request = ailo_request_default();
    request.tools = definitions;
    request.tool_count = 2;
    request.allowed_tools = allowed;
    request.allowed_tool_count = 1;
    request.max_tool_calls = 2;
    const std::string call =
        "<call>{\"name\":\"lookup\",\"arguments\":{\"answer\":4}}</call>";
    for (auto choice : {AILO_TOOLS_AUTO, AILO_TOOLS_NONE, AILO_TOOLS_REQUIRED,
                        AILO_TOOLS_NAMED}) {
      request.tool_choice = choice;
      request.tool_name = choice == AILO_TOOLS_NAMED ? "lookup" : nullptr;
      auto plan = prepare_tools(request);
      qualify_tools(plan, tool_template, vocab);
      CHECK(plan.open == "<call>" && plan.close == "</call>");
      CHECK(plan.definitions_json.find("other") != std::string::npos);
      CHECK(plan.definitions_json.find("metadata-marker") != std::string::npos);
      const auto grammar = tool_grammar(plan, request);
      CHECK(accepts(vocab, grammar, call) == (choice != AILO_TOOLS_NONE));
      CHECK(accepts(vocab, grammar, "plain text") ==
            (choice == AILO_TOOLS_NONE || choice == AILO_TOOLS_AUTO));
      CHECK(accepts(vocab, grammar, call + call) ==
            (choice == AILO_TOOLS_AUTO || choice == AILO_TOOLS_REQUIRED));
      CHECK(!accepts(vocab, grammar, call + call + call));
      CHECK(!accepts(vocab, grammar,
                     "<call>{\"name\":\"other\",\"arguments\":{}}</call>"));
      CHECK(!accepts(
          vocab, grammar,
          "<call>{\"name\":\"lookup\",\"arguments\":{\"answer\":5}}</call>"));
    }
    request.tool_choice = AILO_TOOLS_REQUIRED;
    request.tool_name = nullptr;
    auto ignored = std::string(tool_template);
    const std::string serialization = "{{ tools | tojson }}";
    ignored.replace(ignored.find(serialization), serialization.size(),
                    "{{ tools[0].function.name }}");
    try {
      auto ignored_plan = prepare_tools(request);
      qualify_tools(ignored_plan, ignored.c_str(), vocab);
      CHECK(false);
    } catch (const failure &e) {
      CHECK(e.status == AILO_UNSUPPORTED);
    }
    auto plan = prepare_tools(request);
    qualify_tools(plan, tool_template, vocab);
    size_t ends = 0;
    std::string raw;
    tool_output_stream output(plan, 4096, [&](const tool_output_event &event) {
      if (event.kind == tool_output_kind::arguments)
        raw.append(event.data, event.size);
      if (event.kind == tool_output_kind::end) {
        CHECK(raw == "{\"answer\":4}");
        raw.clear();
        ++ends;
      }
    });
    for (char c : call + call)
      output.push(&c, 1);
    output.finish(false);
    CHECK(ends == 2);
    auto array_plan = prepare_tools(request);
    qualify_tools(array_plan, array_template, vocab);
    CHECK(array_plan.format.array);
    const auto array_grammar = tool_grammar(array_plan, request);
    const std::string object =
        "{\"name\":\"lookup\",\"arguments\":{\"answer\":4}}";
    const std::string array = "<calls>[" + object + "," + object + "]</calls>";
    CHECK(accepts(vocab, array_grammar, array));
    CHECK(!accepts(vocab, array_grammar, "<calls>[]</calls>"));
    CHECK(!accepts(vocab, array_grammar, array + array));
    ends = 0;
    tool_output_stream arrays(array_plan, 4096,
                              [&](const tool_output_event &event) {
                                if (event.kind == tool_output_kind::end)
                                  ++ends;
                              });
    for (char c : array)
      arrays.push(&c, 1);
    arrays.finish(false);
    CHECK(ends == 2);
    request.output_format = AILO_OUTPUT_JSON_SCHEMA;
    request.json_schema = R"({"const":{"answer":4}})";
    request.tool_choice = AILO_TOOLS_AUTO;
    plan = prepare_tools(request);
    qualify_tools(plan, tool_template, vocab);
    const auto combined = tool_grammar(plan, request);
    CHECK(accepts(vocab, combined, "{\"answer\":4}"));
    CHECK(accepts(vocab, combined, call + call));
    CHECK(!accepts(vocab, combined, call + "junk"));
    CHECK(!accepts(vocab, combined, "{\"answer\":5}"));
    request.output_format = AILO_OUTPUT_TEXT;
    request.json_schema = nullptr;
    request.tool_choice = AILO_TOOLS_REQUIRED;

    ailo_client *native = nullptr;
    ailo_error error{};
    CHECK(ailo_client_new(&config, &native, &error) == AILO_OK);
    std::unique_ptr<ailo_client, decltype(&ailo_client_destroy)> client(
        native, ailo_client_destroy);
    ailo_content content{AILO_CONTENT_TEXT, "hello", 5};
    ailo_item item{};
    item.content = &content;
    item.content_count = 1;
    request.items = &item;
    request.item_count = 1;
    request.max_tokens = 256;
    request.temperature = 0;
    ailo_logit_bias bias[] = {
        {2, 100}, {35, -100}, {13, -100}, {12, -100}, {16, -100}};
    request.logit_bias = bias;
    request.logit_bias_count = 5;
    ailo_run_info info{};
    observation seen;
    auto run = [&]() {
      return client->generate(client.get(), &request, observe, &seen, &info,
                              &error);
    };
    auto status = run();
    if (status != AILO_OK)
      std::fprintf(stderr, "generation: %s\n", error.message);
    CHECK(status == AILO_OK);
    CHECK(seen.calls == 1 && seen.deltas > 1 && seen.terminals == 1 &&
          info.finish_reason == AILO_FINISH_TOOL_CALLS);
    request.candidates = 2;
    seen = {};
    CHECK(run() == AILO_OK);
    CHECK(seen.ids.size() == 2 && info.completed_candidates == 2);
    request.candidates = 1;
    for (int action : {0, -1}) {
      seen = {};
      seen.action = action;
      CHECK(run() == (action ? AILO_SINK_FAILED : AILO_CANCELLED));
      CHECK(seen.deltas == 1 && !seen.calls && !seen.terminals);
    }
    request.max_tokens = 1;
    seen = {};
    CHECK(run() == AILO_OK);
    CHECK(info.finish_reason == AILO_FINISH_LENGTH && !seen.deltas);
    request.max_tokens = 256;
    request.tool_name = "missing";
    request.tool_choice = AILO_TOOLS_NAMED;
    seen = {};
    CHECK(run() == AILO_INVALID_ARGUMENT && !seen.events);
    request.tool_name = nullptr;
    request.tool_choice = AILO_TOOLS_REQUIRED;
    seen = {};
    CHECK(run() == AILO_OK && seen.terminals == 1);
    request.max_output_bytes = 16;
    seen = {};
    CHECK(run() == AILO_OUT_OF_RANGE && !seen.terminals);
    request.max_output_bytes = 0;
    const char *stop[] = {"4"};
    request.stop = stop;
    request.stop_count = 1;
    seen = {};
    CHECK(run() == AILO_MODEL_FAILED && !seen.calls && !seen.terminals);
    request.stop = nullptr;
    request.stop_count = 0;
    seen = {};
    CHECK(run() == AILO_OK && seen.terminals == 1);
    request.parallel_tool_calls = 0;
    seen = {};
    CHECK(run() == AILO_INVALID_ARGUMENT && !seen.events);
    request.parallel_tool_calls = 1;
    config.chat_template = "{% for m in messages %}{{ m.content }}{% endfor %}";
    ailo_client *unsupported_client = nullptr;
    CHECK(ailo_client_new(&config, &unsupported_client, &error) == AILO_OK);
    std::unique_ptr<ailo_client, decltype(&ailo_client_destroy)>
        unsupported_receiver(unsupported_client, ailo_client_destroy);
    seen = {};
    CHECK(unsupported_client->generate(unsupported_client, &request, observe,
                                       &seen, &info,
                                       &error) == AILO_UNSUPPORTED);
    CHECK(!seen.events);
    std::puts("Tool choices, schema masking, immediate arguments, parallel "
              "layouts and failures passed");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "tools: %s\n", e.what());
    return 1;
  }
}
