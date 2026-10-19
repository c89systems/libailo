#include "ailo/compaction.h"
#include "ailo/continuation.h"
#include "ailo_client_internal.hpp"
#include "ailo_text_stream_internal.hpp"
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace ailo::client;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c))                                                                  \
      throw std::runtime_error(std::string(#c) + " at " +                      \
                               std::to_string(__LINE__));                      \
  } while (0)
ailo_error error{};
using owner = std::unique_ptr<ailo_client, decltype(&ailo_client_destroy)>;
const char *plain =
    "{% for m in messages %}{{m.role}}:{{m.content}}|{% endfor %}";
const char *thinking =
    "{% for m in messages %}{{m.role}}:{% if m.reasoning_content is defined "
    "%}<think>{{m.reasoning_content}}</think>{% endif %}{{m.content}}|{% "
    "endfor %}{% if add_generation_prompt %}assistant:{% if enable_thinking "
    "%}<think>{% endif %}{% endif %}";
const char *tools =
    "{% if tools %}{{tools | tojson}}{% endif %}{% for m in messages "
    "%}{{m.role}}:{{m.content}}{% if m.tool_calls is defined %}{% for c in "
    "m.tool_calls %}<call>{\"name\":{{c.function.name | "
    "tojson}},\"arguments\":{% if c.function.arguments is string "
    "%}{{c.function.arguments}}{% else %}{{c.function.arguments | tojson}}{% "
    "endif %}}</call>{% endfor %}{% endif %}|{% endfor %}{% if "
    "add_generation_prompt %}assistant:{% endif %}";
owner client(const std::string &path, const char *source) {
  ailo_client_config config{};
  config.source.local_path = path.c_str();
  config.chat_template = source;
  config.backend = AILO_BACKEND_CPU;
  config.threads = 1;
  config.context_tokens = 4096;
  ailo_client *out = nullptr;
  CHECK(ailo_client_new(&config, &out, &error) == AILO_OK);
  return {out, ailo_client_destroy};
}
struct observed {
  struct candidate {
    std::string raw, text, reason, arguments;
    size_t watermark = 0, tokens = 0;
  };
  candidate values[2];
  unsigned progress = 0, controls = 0, deltas = 0;
  int action = 1;
  bool stop_bound = false;
  static int event(void *user, const ailo_event *e) {
    try {
      return receive(*static_cast<observed *>(user), *e);
    } catch (const std::exception &problem) {
      std::fprintf(stderr, "provenance callback: %s\n", problem.what());
      return -1;
    }
  }
  static int receive(observed &self, const ailo_event &e) {
    CHECK(e.candidate < 2);
    auto &out = self.values[e.candidate];
    if (e.kind == AILO_EVENT_TOKEN) {
      CHECK(e.token && e.token->byte_offset.hi == 0 &&
            e.token->byte_offset.lo == out.raw.size() &&
            e.token->index.lo == out.tokens++);
      CHECK(out.raw.size() + e.token->chosen.size < 16384);
      out.raw.append(static_cast<const char *>(e.token->chosen.data),
                     e.token->chosen.size);
    }
    const bool mapped = e.kind == AILO_EVENT_TEXT_DELTA ||
                        e.kind == AILO_EVENT_REASONING_DELTA ||
                        e.kind == AILO_EVENT_FUNCTION_ARGUMENTS_DELTA;
    CHECK(bool(e.raw_offset_valid) ==
          (mapped || e.kind == AILO_EVENT_OUTPUT_PROGRESS));
    if (mapped) {
      const size_t offset = e.raw_offset.lo;
      CHECK(e.raw_offset.hi == 0 && offset >= out.watermark &&
            offset <= out.raw.size() && e.size <= out.raw.size() - offset);
      CHECK(!std::memcmp(out.raw.data() + offset, e.data, e.size));
      auto &body = e.kind == AILO_EVENT_REASONING_DELTA ? out.reason
                   : e.kind == AILO_EVENT_FUNCTION_ARGUMENTS_DELTA
                       ? out.arguments
                       : out.text;
      body.append(static_cast<const char *>(e.data), e.size);
      ++self.deltas;
    }
    if (e.kind == AILO_EVENT_OUTPUT_PROGRESS) {
      CHECK(e.raw_offset.hi == 0 && e.raw_offset.lo >= out.watermark &&
            e.raw_offset.lo <= out.raw.size());
      out.watermark = e.raw_offset.lo;
      ++self.progress;
      if (self.stop_bound)
        CHECK(out.raw.size() - out.watermark <= 3);
      return self.action;
    }
    if (e.kind == AILO_EVENT_COMPACTION)
      ++self.controls;
    if (e.kind == AILO_EVENT_CHOICE_END)
      CHECK(out.watermark == out.raw.size());
    return 1;
  }
};
ailo_run_info run(ailo_client *client, const ailo_request &request,
                  observed &seen, ailo_status status = AILO_OK) {
  ailo_run_info out{};
  CHECK(client->generate(client, &request, observed::event, &seen, &out,
                         &error) == status);
  return out;
}
int main(int argc, char **argv) {
  try {
    CHECK(argc == 2);
    // Composable byte sources: pending UTF-8 retains its ORIGINAL offset,
    // including an explicit origin jump between already-finished channels.
    std::string delivered;
    std::vector<uint64_t> offsets;
    text_stream text({}, [&](const char *p, size_t n) {
      offsets.push_back(text.output_offset());
      delivered.append(p, n);
    });
    text.push("\xe2", false, 100);
    CHECK(text.pending_offset() == 100);
    text.push("\x98", false, 101);
    CHECK(text.pending_offset() == 100);
    text.push("\x83", false, 102);
    CHECK(text.pending_offset() == no_offset && delivered == "\xe2\x98\x83");
    text.push("x", false, 500);
    CHECK(offsets == std::vector<uint64_t>({100, 500}));
    text.push("\xe2", false, 501);
    bool rejected = false;
    try {
      text.push("\x98", false, 900);
    } catch (const failure &e) {
      rejected = e.status == AILO_INTERNAL_ERROR;
    }
    CHECK(rejected);
    const std::string base = argv[1];
    auto receiver = client(base, plain);
    ailo_content part{AILO_CONTENT_TEXT, "hello", 5, nullptr};
    ailo_item item{};
    item.content = &part;
    item.content_count = 1;
    auto request = ailo_request_default();
    request.items = &item;
    request.item_count = 1;
    request.temperature = 0;
    request.logprobs = 1;
    request.top_logprobs = 2;
    request.max_tokens = 32;
    ailo_logit_bias bias{100, 100};
    request.logit_bias = &bias;
    request.logit_bias_count = 1;
    const char *stop = "aaaZ";
    request.stop = &stop;
    request.stop_count = 1;
    observed ordinary;
    ordinary.stop_bound = true;
    run(receiver.get(), request, ordinary);
    CHECK(ordinary.values[0].text == std::string(32, 'a') &&
          ordinary.progress >= 32);
    for (int action : {0, -1}) {
      observed stopped;
      stopped.action = action;
      auto result = run(receiver.get(), request, stopped,
                        action == 0 ? AILO_CANCELLED : AILO_SINK_FAILED);
      CHECK(stopped.progress == 1 && result.usage.completion_tokens.lo == 1);
    }
    // Unicode spans three sampled byte tokens; each output source span must
    // match the original raw bytes without exposing incomplete UTF-8 deltas.
    request.stop = nullptr;
    request.stop_count = 0;
    request.max_tokens = 16;
    request.output_format = AILO_OUTPUT_JSON_SCHEMA;
    request.json_schema = "{\"const\":\"\xe2\x98\x83\"}";
    ailo_logit_bias unicode[] = {
        {37, 100}, {229, 100}, {155, 100}, {134, 100}, {2, 100}};
    request.logit_bias = unicode;
    request.logit_bias_count = 5;
    request.candidates = 2;
    observed utf8;
    auto unicode_result = run(receiver.get(), request, utf8);
    CHECK(unicode_result.finish_reason == AILO_FINISH_STOP &&
          utf8.values[0].text == "\"\xe2\x98\x83\"" &&
          utf8.values[1].text == utf8.values[0].text);
    auto reasoning = client(base + "-reasoning", thinking);
    request = ailo_request_default();
    request.items = &item;
    request.item_count = 1;
    request.max_tokens = 32;
    request.temperature = 0;
    request.logprobs = 1;
    request.reasoning = AILO_REASONING_ENABLED;
    request.output_format = AILO_OUTPUT_JSON_SCHEMA;
    request.json_schema = "{\"const\":{}}";
    ailo_logit_bias trace[] = {{260, 100}, {2, 80}};
    request.logit_bias = trace;
    request.logit_bias_count = 2;
    observed reason;
    run(reasoning.get(), request, reason);
    CHECK(reason.values[0].reason == "r" && reason.values[0].text == "{}" &&
          reason.values[0].raw == "r</think>{}");
    // Function syntax is classified immediately; only lexical argument bytes
    // receive source spans, so a wire adapter can exclude the hidden envelope.
    auto functions = client(base + "-auto-compaction", tools);
    request = ailo_request_default();
    request.items = &item;
    request.item_count = 1;
    request.max_tokens = 32;
    request.temperature = 0;
    request.logprobs = 1;
    const std::string expected = "{\"text\":\"" + std::string(32, 'a') + "\"}";
    const std::string parameters = "{\"const\":" + expected + "}";
    ailo_function_tool function{"lookup", "Lookup", parameters.c_str(), 1};
    request.tools = &function;
    request.tool_count = 1;
    request.tool_choice = AILO_TOOLS_NAMED;
    request.tool_name = "lookup";
    request.parallel_tool_calls = 0;
    ailo_logit_bias eos{2, 100};
    request.logit_bias = &eos;
    request.logit_bias_count = 1;
    observed tool;
    auto tool_result = run(functions.get(), request, tool);
    CHECK(tool_result.finish_reason == AILO_FINISH_TOOL_CALLS &&
          tool.values[0].arguments == expected && tool.values[0].text.empty());
    // Rebuild coordinates describe the sampled stream, not new KV positions.
    auto automatic = client(base + "-auto-compaction", plain);
    ailo_continuation *raw = nullptr;
    CHECK(ailo_continuation_new(automatic.get(), nullptr, &raw, &error) ==
          AILO_OK);
    std::unique_ptr<ailo_continuation, decltype(&ailo_continuation_destroy)>
        history(raw, ailo_continuation_destroy);
    std::string bodies[] = {
        "Instructions", "Finish ticket T-1. " + std::string(1000, 'x'),
        "answer",       "older",
        "answer",       "recent",
        "answer",       "latest",
        "answer",       "current"};
    ailo_item items[10]{};
    ailo_content parts[10]{};
    for (size_t i = 0; i < 10; ++i) {
      parts[i] = {AILO_CONTENT_TEXT, bodies[i].data(), bodies[i].size(),
                  nullptr};
      items[i].content = &parts[i];
      items[i].content_count = 1;
      items[i].role = i % 2 ? AILO_ROLE_USER : AILO_ROLE_ASSISTANT;
    }
    items[0].role = AILO_ROLE_SYSTEM;
    CHECK(history->append(history.get(), items, 10, &error) == AILO_OK);
    request = ailo_request_default();
    request.continuation = history.get();
    request.max_tokens = 32;
    request.temperature = 0;
    request.logprobs = 1;
    request.logit_bias = &bias;
    request.logit_bias_count = 1;
    observed before;
    auto baseline = run(automatic.get(), request, before);
    auto policy = ailo_auto_compaction_default_policy();
    policy.compaction.target_tokens = 800;
    policy.compaction.output_reserve_tokens = 64;
    policy.compaction.summary_context_tokens = 8192;
    policy.trigger_tokens = baseline.usage.prompt_tokens.lo + 3;
    request.auto_compaction = &policy;
    observed rebuilt;
    run(automatic.get(), request, rebuilt);
    CHECK(rebuilt.controls == 1 &&
          rebuilt.values[0].raw == std::string(32, 'a') &&
          rebuilt.values[0].text == before.values[0].text);
    std::puts("Output byte provenance and classified watermarks: UTF-8/stops, "
              "reasoning/tool syntax, candidates, cancellation and rebuild "
              "continuity passed");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "output provenance: %s (%s)\n", e.what(),
                 error.message);
    return 1;
  }
}
