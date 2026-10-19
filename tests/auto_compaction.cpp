#include "ailo/compaction.h"
#include "ailo/continuation.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c))                                                                  \
      throw std::runtime_error(std::string(#c) + " at " +                      \
                               std::to_string(__LINE__));                      \
  } while (0)
ailo_error error{};
using client_owner =
    std::unique_ptr<ailo_client, decltype(&ailo_client_destroy)>;
using history_owner =
    std::unique_ptr<ailo_continuation, decltype(&ailo_continuation_destroy)>;
const char *plain =
    "{% for m in messages %}{{m.role}}:{{m.content}}|{% endfor %}";
const char *thinking =
    "{% for m in messages %}{{m.role}}:{% if m.reasoning_content is defined "
    "%}<think>{{m.reasoning_content}}</think>{% endif %}{{m.content}}|{% "
    "endfor %}{% if add_generation_prompt %}assistant:{% if "
    "enable_thinking %}<think>{% endif %}{% endif %}";
const char *tools =
    "{% if tools %}{{tools | tojson}}{% endif %}{% for m in messages "
    "%}{{m.role}}:{{m.content}}{% if m.tool_calls is defined %}{% for c in "
    "m.tool_calls %}<call>{\"name\":{{c.function.name | "
    "tojson}},\"arguments\":{% if c.function.arguments is string "
    "%}{{c.function.arguments}}{% else %}{{c.function.arguments | tojson}}{% "
    "endif %}}</call>{% endfor %}{% endif %}|{% endfor %}{% if "
    "add_generation_prompt %}assistant:{% endif %}";
client_owner client(const std::string &path, const char *source) {
  ailo_client_config config{};
  config.source.local_path = path.c_str();
  config.chat_template = source;
  config.context_tokens = 4096;
  config.backend = AILO_BACKEND_CPU;
  config.threads = 1;
  ailo_client *out = nullptr;
  CHECK(ailo_client_new(&config, &out, &error) == AILO_OK);
  return {out, ailo_client_destroy};
}
history_owner history(ailo_client *client) {
  ailo_continuation *out = nullptr;
  CHECK(ailo_continuation_new(client, nullptr, &out, &error) == AILO_OK);
  history_owner result(out, ailo_continuation_destroy);
  std::string text[] = {"Preserve instructions.",
                        "Finish ticket T-1. " + std::string(1000, 'x'),
                        "old answer",
                        "older question",
                        "older answer",
                        "recent question",
                        "recent answer",
                        "newest question",
                        "newest answer",
                        "current question"};
  ailo_content parts[10]{};
  ailo_item items[10]{};
  for (size_t i = 0; i < 10; ++i) {
    parts[i] = {AILO_CONTENT_TEXT, text[i].data(), text[i].size(), nullptr};
    items[i].role = i % 2 ? AILO_ROLE_USER : AILO_ROLE_ASSISTANT;
    items[i].content = &parts[i];
    items[i].content_count = 1;
  }
  items[0].role = AILO_ROLE_SYSTEM;
  items[0].pinned = 1;
  items[0].id = "system";
  items[1].id = "goal";
  CHECK(out->append(out, items, 10, &error) == AILO_OK);
  return result;
}
history_owner clone(ailo_continuation *source) {
  ailo_continuation *out = nullptr;
  CHECK(source->clone(source, &out, &error) == AILO_OK);
  return {out, ailo_continuation_destroy};
}
ailo_continuation_info info(ailo_continuation *value) {
  ailo_continuation_info out{};
  CHECK(value->info(value, &out, &error) == AILO_OK);
  return out;
}
struct observation {
  std::vector<std::string> text{2}, arguments{2}, ids{2}, call_ids{2};
  std::vector<unsigned> item_begins = std::vector<unsigned>(2, 0);
  std::string run;
  unsigned events = 0, deltas = 0, controls = 0;
  size_t at = 0;
  int control_action = 1, end_action = 1;
  bool cancel_first = false;
  bool immediate = true;
  size_t first_delta_token = 0;
  ailo_continuation *history = nullptr;
  static int event(void *user, const ailo_event *e) {
    try {
      return handle(user, e);
    } catch (const std::exception &failure) {
      std::fprintf(stderr, "auto observer: %s\n", failure.what());
      return -1;
    }
  }
  static int handle(void *user, const ailo_event *e) {
    auto &out = *static_cast<observation *>(user);
    CHECK(e->sequence.hi == 0 && e->sequence.lo == out.events++);
    if (out.run.empty())
      out.run = e->run_id;
    CHECK(out.run == e->run_id && e->candidate < 2);
    auto candidate = e->candidate;
    if (e->kind == AILO_EVENT_ITEM_BEGIN) {
      // Every controlled candidate here has exactly one output item. A
      // rebuild must not close/reopen it under a new identity.
      CHECK(++out.item_begins[candidate] == 1);
      out.ids[candidate] = e->item_id;
      if (e->call_id)
        out.call_ids[candidate] = e->call_id;
    }
    if (e->item_id)
      CHECK(out.ids[candidate] == e->item_id);
    if (e->call_id)
      CHECK(out.call_ids[candidate] == e->call_id);
    if (e->kind == AILO_EVENT_TEXT_DELTA ||
        e->kind == AILO_EVENT_REASONING_DELTA) {
      if (!out.deltas) {
        out.first_delta_token = e->usage.completion_tokens.lo;
        if (out.immediate)
          CHECK(out.first_delta_token == 1);
      }
      CHECK(out.text[candidate].size() + e->size < 4096);
      out.text[candidate].append(static_cast<const char *>(e->data), e->size);
      ++out.deltas;
      if (out.cancel_first)
        return 0;
    }
    if (e->kind == AILO_EVENT_FUNCTION_ARGUMENTS_DELTA) {
      CHECK(out.arguments[candidate].size() + e->size < 4096);
      out.arguments[candidate].append(static_cast<const char *>(e->data),
                                      e->size);
    }
    if (e->kind == AILO_EVENT_COMPACTION) {
      ++out.controls;
      out.at = e->usage.completion_tokens.lo;
      CHECK(e->compaction && e->compaction->changed &&
            e->compaction->after_tokens < e->compaction->before_tokens &&
            e->compacted_item_count == 7);
      unsigned opaque = 0;
      for (size_t i = 0; i < e->compacted_item_count; ++i)
        if (e->compacted_items[i].kind == AILO_ITEM_COMPACTION) {
          ++opaque;
          CHECK(e->compacted_items[i].encrypted_content);
        }
      CHECK(opaque == 1 && e->usage.compaction_passes >= 1 &&
            e->usage.compaction_completion_tokens.lo);
      if (out.at)
        CHECK(e->compaction->replayed_tokens &&
              e->usage.context_replay_tokens.lo);
      if (out.history) {
        ailo_continuation_info unused{};
        CHECK(out.history->info(out.history, &unused, &error) == AILO_BUSY);
      }
      return out.control_action;
    }
    return e->kind == AILO_EVENT_RUN_END ? out.end_action : 1;
  }
};
ailo_run_info generate(ailo_client *client, const ailo_request &request,
                       observation &seen, ailo_status status = AILO_OK) {
  ailo_run_info run{};
  const auto actual = client->generate(client, &request, observation::event,
                                       &seen, &run, &error);
  if (actual != status)
    throw std::runtime_error("generation status " + std::to_string(actual) +
                             " expected " + std::to_string(status));
  return run;
}
int main(int argc, char **argv) {
  try {
    CHECK(argc == 2);
    const std::string base = argv[1];
    auto receiver = client(base + "-auto-compaction", plain);
    auto original = history(receiver.get());
    auto request = ailo_request_default();
    request.continuation = original.get();
    request.max_tokens = 32;
    request.temperature = 0;
    request.seed = 42;
    ailo_logit_bias biases[] = {{100, 100}, {101, 100}};
    request.logit_bias = biases;
    request.logit_bias_count = 1;
    observation baseline;
    const auto first = generate(receiver.get(), request, baseline);
    CHECK(baseline.text[0] == std::string(32, 'a') &&
          first.usage.compaction_passes == 0);
    auto policy = ailo_auto_compaction_default_policy();
    policy.compaction.target_tokens = 800;
    policy.compaction.output_reserve_tokens = 64;
    policy.compaction.summary_context_tokens = 8192;
    policy.trigger_tokens = first.usage.prompt_tokens.lo + 3;
    request.auto_compaction = &policy;
    for (int action : {0, -1, 1}) {
      auto fork = clone(original.get());
      request.continuation = fork.get();
      observation seen;
      seen.history = fork.get();
      seen.control_action = action;
      auto run = generate(receiver.get(), request, seen,
                          action == 0  ? AILO_CANCELLED
                          : action < 0 ? AILO_SINK_FAILED
                                       : AILO_OK);
      CHECK(seen.controls == 1 && seen.at == 3 &&
            run.usage.compaction_passes == 1 &&
            run.usage.context_replay_tokens.lo);
      CHECK(info(fork.get()).item_count == (action > 0 ? 7 : 10));
      CHECK(info(fork.get()).revision.lo == (action > 0 ? 2 : 1));
      if (action > 0)
        CHECK(seen.text[0] == baseline.text[0] &&
              run.usage.completion_tokens.lo == 32);
    }
    auto withheld = clone(original.get());
    request.continuation = withheld.get();
    const char *stop = "aaaZ";
    request.stop = &stop;
    request.stop_count = 1;
    observation stopped;
    stopped.immediate = false;
    generate(receiver.get(), request, stopped);
    CHECK(stopped.controls == 1 && stopped.at == 3 &&
          stopped.first_delta_token == 4 &&
          stopped.text[0] == baseline.text[0]);
    request.stop = nullptr;
    request.stop_count = 0;
    auto failed_final = clone(original.get());
    request.continuation = failed_final.get();
    observation final;
    final.end_action = -1;
    generate(receiver.get(), request, final, AILO_SINK_FAILED);
    CHECK(info(failed_final.get()).item_count == 10);
    auto immediate = clone(original.get());
    request.continuation = immediate.get();
    observation cancelled;
    cancelled.cancel_first = true;
    auto early = generate(receiver.get(), request, cancelled, AILO_CANCELLED);
    CHECK(cancelled.deltas == 1 && !cancelled.controls &&
          early.usage.completion_tokens.lo == 1);
    // Preflight compaction and explicit invalid-policy/no-state failures.
    auto preflight = clone(original.get());
    request.continuation = preflight.get();
    policy.trigger_tokens = first.usage.prompt_tokens.lo - 1;
    observation before;
    generate(receiver.get(), request, before);
    CHECK(before.controls == 1 && before.at == 0 &&
          info(preflight.get()).item_count == 7);
    request.continuation = original.get();
    auto valid = policy;
    biases[0].token_id = 5000;
    policy.compaction.summary_tokens = 1;
    observation invalid_bias;
    generate(receiver.get(), request, invalid_bias, AILO_INVALID_ARGUMENT);
    CHECK(!invalid_bias.events);
    biases[0].token_id = 100;
    policy = valid;
    for (int which = 0; which < 4; ++which) {
      policy = valid;
      if (which == 0)
        policy.max_passes = 0;
      if (which == 1)
        policy.compaction.target_tokens = policy.trigger_tokens;
      if (which == 2)
        policy.compaction.output_reserve_tokens = 0;
      if (which == 3)
        policy.trigger_tokens = 4096;
      observation seen;
      generate(receiver.get(), request, seen, AILO_INVALID_ARGUMENT);
      CHECK(!seen.events);
    }
    policy = valid;
    request.continuation = nullptr;
    observation stateless;
    generate(receiver.get(), request, stateless, AILO_INVALID_ARGUMENT);
    CHECK(!stateless.events);
    // A protected long output ends explicitly; no unbounded token/text window.
    auto exhausted = clone(original.get());
    request.continuation = exhausted.get();
    request.max_tokens = 3000;
    policy.max_passes = 1;
    policy.trigger_tokens = first.usage.prompt_tokens.lo + 3;
    observation long_output;
    generate(receiver.get(), request, long_output, AILO_CONTEXT_LIMIT);
    CHECK(long_output.controls == 1 && info(exhausted.get()).item_count == 10);
    // Two candidates keep independent RNG streams across native rebuild.
    request = ailo_request_default();
    request.continuation = original.get();
    request.max_tokens = 32;
    request.candidates = 2;
    request.seed = 42;
    request.temperature = 1;
    request.top_p = 1;
    request.logit_bias = biases;
    request.logit_bias_count = 2;
    observation random_before;
    generate(receiver.get(), request, random_before);
    auto random_history = clone(original.get());
    request.continuation = random_history.get();
    request.auto_compaction = &policy;
    observation random_after;
    generate(receiver.get(), request, random_after);
    CHECK(random_before.text == random_after.text &&
          random_after.controls == 1 && random_after.at == 3 &&
          random_before.text[0] != random_before.text[1]);
    // Constrained JSON parser + eager sampler grammar survive mid-string.
    request = ailo_request_default();
    request.continuation = original.get();
    request.max_tokens = 128;
    request.temperature = 0;
    request.output_format = AILO_OUTPUT_JSON_SCHEMA;
    const std::string answer = std::string(32, 'a');
    const std::string schema = "{\"const\":{\"text\":\"" + answer + "\"}}";
    request.json_schema = schema.c_str();
    observation json_before;
    json_before.immediate = false;
    auto json_run = generate(receiver.get(), request, json_before);
    policy.trigger_tokens = json_run.usage.prompt_tokens.lo + 3;
    policy.max_passes = 4;
    auto json_history = clone(original.get());
    request.continuation = json_history.get();
    request.auto_compaction = &policy;
    observation json_after;
    json_after.immediate = false;
    auto json_done = generate(receiver.get(), request, json_after);
    CHECK(json_after.controls == 1 &&
          json_after.first_delta_token == json_before.first_delta_token &&
          json_after.text == json_before.text &&
          json_after.text[0] == "{\"text\":\"" + answer + "\"}" &&
          json_done.finish_reason == AILO_FINISH_STOP);
    // Qualified function-call decoder survives a checkpoint inside arguments.
    auto tool_client = client(base + "-auto-compaction", tools);
    auto tool_history = history(tool_client.get());
    request = ailo_request_default();
    request.continuation = tool_history.get();
    request.temperature = 0;
    request.max_tokens = 128;
    const std::string parameters =
        "{\"type\":\"object\",\"properties\":{\"text\":{\"const\":\"" + answer +
        "\"}},\"required\":[\"text\"],\"additionalProperties\":false}";
    ailo_function_tool function{"lookup", "Look up a value", parameters.c_str(),
                                1};
    request.tools = &function;
    request.tool_count = 1;
    request.tool_choice = AILO_TOOLS_NAMED;
    request.tool_name = "lookup";
    request.parallel_tool_calls = 0;
    request.max_tool_calls = 1;
    ailo_logit_bias end_bias{2, 100};
    request.logit_bias = &end_bias;
    request.logit_bias_count = 1;
    observation tool_before;
    tool_before.immediate = false;
    auto tool_run = generate(tool_client.get(), request, tool_before);
    policy.trigger_tokens = tool_run.usage.prompt_tokens.lo + 3;
    auto tool_fork = clone(tool_history.get());
    request.continuation = tool_fork.get();
    request.auto_compaction = &policy;
    observation tool_after;
    tool_after.immediate = false;
    auto tool_done = generate(tool_client.get(), request, tool_after);
    CHECK(tool_after.controls == 1 && tool_after.at == 3 &&
          tool_after.arguments == tool_before.arguments &&
          tool_after.arguments[0] == "{\"text\":\"" + answer + "\"}" &&
          tool_done.finish_reason == AILO_FINISH_TOOL_CALLS);
    // Reasoning parser/lifecycle stays active, with no repeated opener/deltas.
    auto reason_client = client(base + "-auto-compaction-reasoning", thinking);
    auto reason_history = history(reason_client.get());
    request = ailo_request_default();
    request.continuation = reason_history.get();
    request.temperature = 0;
    request.max_tokens = 32;
    request.reasoning = AILO_REASONING_ENABLED;
    request.logit_bias = biases;
    request.logit_bias_count = 1;
    observation reason_before;
    reason_before.immediate = false;
    auto reason_run = generate(reason_client.get(), request, reason_before);
    policy.trigger_tokens = reason_run.usage.prompt_tokens.lo + 3;
    auto reason_fork = clone(reason_history.get());
    request.continuation = reason_fork.get();
    request.auto_compaction = &policy;
    observation reason_after;
    reason_after.immediate = false;
    auto reason_done = generate(reason_client.get(), request, reason_after);
    CHECK(reason_after.controls == 1 &&
          reason_after.text == reason_before.text &&
          reason_done.usage.reasoning_tokens.lo == 32);
    // A receiving client with a different private key issues the new token.
    // Commit must retain that key owner for later append, even after client
    // destruction. It must not grant decryption to the original client.
    auto transferred = clone(original.get());
    auto peer = client(base + "-auto-compaction", plain);
    request = ailo_request_default();
    request.continuation = transferred.get();
    request.temperature = 0;
    request.max_tokens = 32;
    request.logit_bias = biases;
    request.logit_bias_count = 1;
    policy.trigger_tokens = first.usage.prompt_tokens.lo + 3;
    request.auto_compaction = &policy;
    observation peer_output;
    generate(peer.get(), request, peer_output);
    CHECK(peer_output.controls == 1);
    peer.reset();
    ailo_content next_text{AILO_CONTENT_TEXT, "next", 4, nullptr};
    ailo_item next{};
    next.content = &next_text;
    next.content_count = 1;
    CHECK(transferred->append(transferred.get(), &next, 1, &error) == AILO_OK &&
          info(transferred.get()).item_count == 8);
    request.auto_compaction = nullptr;
    observation wrong_key;
    generate(receiver.get(), request, wrong_key, AILO_INVALID_ARGUMENT);
    CHECK(!wrong_key.events);
    std::puts("Automatic semantic compaction: preflight/in-flight streaming, "
              "transactional failures, bounded output, RNG/candidates, "
              "JSON/tools/reasoning state passed");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "auto compaction: %s (%s)\n", e.what(), error.message);
    return 1;
  }
}
