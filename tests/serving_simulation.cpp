// Deterministic generator used only by the fast protocol fixture.
#include "ailo_auto_compaction_internal.hpp"
#include "ailo_reasoning_internal.hpp"
#include "ailo_tools_internal.hpp"
#include "serving_fixture_internal.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <thread>
using namespace serving_test;
namespace serving_test {
void configure_model(ailo_client_config &config) {
  config.chat_template =
      "{% if tools %}{{tools | tojson}}{% endif %}{% for m in messages "
      "%}{{m.role}}:{{m.content}}{% if m.tool_calls is defined %}{% for c in "
      "m.tool_calls %}<call>{\"name\":{{c.function.name | "
      "tojson}},\"arguments\":{% if c.function.arguments is string "
      "%}{{c.function.arguments}}{% else %}{{c.function.arguments | "
      "tojson}}{% endif %}}</call>{% endfor %}{% endif %}|{% endfor %}{% if "
      "add_generation_prompt %}assistant:{% endif %}";
}
} // namespace serving_test
namespace ailo {
namespace client {
void generate(state &self, const ailo_request &request, ailo_event_sink sink,
              void *user, ailo_run_info &info,
              continuation_transaction *history, const compaction_task *task) {
  const unsigned generation = ++generations;
  require(generation <= 256, "Fixture exceeded its generation bound");
  self.ensure_loaded();
  std::unique_ptr<automatic_compaction> automatic;
  std::unique_ptr<compaction_change> changed;
  if (request.auto_compaction) {
    require(history != nullptr, "Fixture automatic history is missing");
    automatic = std::make_unique<automatic_compaction>(
        self, *history, *request.auto_compaction, task);
    auto tools = prepare_tools(request);
    const auto rendered =
        render_prompt(self, prepare_replay(self, request), tools, request);
    if (prompt_token_count(self, rendered.prompt) >=
        automatic->policy.trigger_tokens) {
      changed = std::make_unique<compaction_change>(automatic->propose(0));
      automatic->adopt(*changed);
    }
  }
  const auto conversation = prepare_replay(self, request);
  std::string effective, latest;
  for (const auto &m : conversation.messages) {
    effective += m.role + ":" + m.content + "\n";
    for (const auto &c : m.calls)
      effective += c.id + ":" + c.arguments + "\n";
    if (m.role == "user")
      latest = m.content;
  }
  serving_test::save(directory + "/generation-" + std::to_string(generation) +
                         ".json",
                     {{"request", current_request},
                      {"effective", effective},
                      {"latest", latest},
                      {"automatic", bool(changed)},
                      {"items", request.item_count}});
  const auto marker = [](const std::string &s, const std::string &prefix) {
    const auto at = s.rfind(prefix);
    if (at == std::string::npos)
      return std::string();
    const auto begin = at + prefix.size();
    return s.substr(
        begin,
        s.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-",
            begin) -
            begin);
  };
  std::string name, text;
  const bool summary =
      request.json_schema && std::strstr(request.json_schema, "\"facts\"");
  const auto memory = marker(effective, "AILO_REMEMBER:");
  const auto expected = marker(latest, "AILO_EXPECT:");
  const auto finding = marker(latest, "AILO_CHECK_TOOL:");
  const auto command = marker(latest, "AILO_TOOL:");
  bool received = false;
  const auto call_id = "call_ailo_" + command;
  if (!command.empty())
    for (size_t n = 0; n < request.item_count; ++n) {
      const auto &item = request.items[n];
      if (item.kind == AILO_ITEM_FUNCTION_RESULT && item.call_id &&
          call_id == item.call_id)
        for (size_t c = 0; c < item.content_count; ++c)
          received |=
              std::string(static_cast<const char *>(item.content[c].data),
                          item.content[c].size)
                  .find("AILO_TOOL_OK_" + command) != std::string::npos;
    }
  if (summary) {
    ++summaries;
    const auto &part = request.items[request.item_count - 1].content[0];
    const auto sources =
        json::parse(static_cast<const char *>(part.data),
                    static_cast<const char *>(part.data) + part.size);
    auto facts = json::array();
    for (const auto &source : sources) {
      const auto remembered = marker(source.dump(), "AILO_REMEMBER:");
      if (!remembered.empty())
        facts.push_back({{"source", source.at("source")},
                         {"category", "identifier"},
                         {"text", "AILO_REMEMBER:" + remembered}});
      if (source.at("kind") == "function_call") {
        require(std::any_of(sources.begin(), sources.end(),
                            [&](const json &row) {
                              return row.at("kind") == "function_result" &&
                                     row.at("call_id") == source.at("call_id");
                            }),
                "Compaction split a complete function call/result group");
      }
      if (source.at("kind") == "function_result") {
        const auto result = marker(source.dump(), "AILO_TOOL_OK_");
        if (!result.empty())
          facts.push_back({{"source", source.at("source")},
                           {"category", "finding"},
                           {"text", "AILO_TOOL_OK_" + result}});
      }
    }
    if (facts.empty())
      facts.push_back({{"source", sources.at(0).at("source")},
                       {"category", "other"},
                       {"text", "Earlier task completed."}});
    text = json{{"facts", facts}}.dump();
  } else if ((!expected.empty() && memory != expected) ||
             (!finding.empty() &&
              effective.find("AILO_TOOL_OK_" + finding) == std::string::npos)) {
    throw failure(AILO_MODEL_FAILED,
                  "Fixture resolved history lost the expected memory");
  } else if (!command.empty() && !received) {
    for (size_t n = 0; n < request.tool_count; ++n) {
      const auto &tool = request.tools[n];
      if (!std::strcmp(tool.name, "exec_command")) {
        name = tool.name;
        text = json{{"cmd", "printf 'AILO_TOOL_OK_" + command + "\\n'"},
                    {"max_output_tokens", 128},
                    {"yield_time_ms", 1000}}
                   .dump();
        break;
      }
      if (!std::strcmp(tool.name, "shell_command")) {
        name = tool.name;
        text = json{{"command", "printf 'AILO_TOOL_OK_" + command + "\\n'"},
                    {"timeout_ms", 1000}}
                   .dump();
        break;
      }
      if (!std::strcmp(tool.name, "shell")) {
        name = tool.name;
        text =
            json{{"command",
                  {"bash", "-c", "printf 'AILO_TOOL_OK_" + command + "\\n'"}},
                 {"timeout_ms", 1000}}
                .dump();
        break;
      }
      if (!std::strcmp(tool.name, "fixture_echo")) {
        name = tool.name;
        text = json{{"value", command}}.dump();
        break;
      }
    }
    require(!name.empty(),
            "Agent must advertise a supported function shell tool");
    ++tool_calls;
  } else {
    if (received)
      ++tool_results;
    ++answers;
    text = !command.empty()    ? "AILO_DONE:" + command
           : !expected.empty() ? "AILO_RECALLED:" + memory
                               : "AILO_ACK:" + memory;
  }
  const bool function = !name.empty();
  info = {};
  std::snprintf(info.id, sizeof(info.id), "00000000-0000-4000-8000-%012u",
                generation);
  info.actual_backend = AILO_BACKEND_CPU;
  if (changed) {
    info.usage.compaction_passes = changed->info.summary_passes;
    info.usage.compaction_prompt_tokens = changed->info.summary_prompt_tokens;
    info.usage.compaction_completion_tokens =
        changed->info.summary_completion_tokens;
  }
  ailo_event event{};
  event.model_id = self.id.c_str();
  event.run_id = info.id;
  event.item_kind = function ? AILO_ITEM_FUNCTION_CALL : AILO_ITEM_MESSAGE;
  const auto item_id = std::string(function ? "fc_" : "msg_") + info.id;
  event.item_id = item_id.c_str();
  event.call_id = function ? call_id.c_str() : nullptr;
  unsigned sequence = 0, tokens = 0;
  const auto emit = [&](ailo_event_kind kind, const void *data = nullptr,
                        size_t size = 0) {
    if (self.is_cancelled())
      throw failure(AILO_CANCELLED, "Fixture generation cancelled");
    event.kind = kind;
    event.data = data;
    event.size = size;
    event.usage = info.usage;
    event.sequence = detail::from_unsigned(sequence++);
    const int accepted = sink(user, &event);
    if (accepted <= 0)
      throw failure(accepted ? AILO_SINK_FAILED : AILO_CANCELLED,
                    "Fixture event consumer stopped");
  };
  emit(AILO_EVENT_RUN_BEGIN);
  if (changed) {
    event.compaction = &changed->info;
    event.compacted_items = changed->items.data();
    event.compacted_item_count = changed->items.size();
    emit(AILO_EVENT_COMPACTION);
    event.compaction = nullptr;
    event.compacted_items = nullptr;
    event.compacted_item_count = 0;
  }
  emit(AILO_EVENT_CHOICE_BEGIN);
  emit(AILO_EVENT_ITEM_BEGIN);
  if (!function)
    emit(AILO_EVENT_CONTENT_BEGIN);
  const auto stream = [&](const std::string &bytes, ailo_event_kind kind) {
    for (char c : bytes) {
      ailo_token_info token{};
      token.chosen.data = &c;
      token.chosen.size = 1;
      token.chosen.token_id = static_cast<unsigned char>(c);
      token.index = detail::from_unsigned(tokens);
      token.byte_offset = detail::from_unsigned(tokens);
      event.raw_offset = detail::from_unsigned(tokens);
      event.raw_offset_valid = 1;
      info.usage.completion_tokens = detail::from_unsigned(++tokens);
      event.token = &token;
      if (request.logprobs)
        emit(AILO_EVENT_TOKEN);
      event.token = nullptr;
      emit(kind, &c, 1);
      if (!summary && tokens == 1 &&
          std::ifstream(directory + "/hold-stream")) {
        std::ofstream(directory + "/first-delta") << "ready\n";
        unsigned waits = 0;
        while (!std::ifstream(directory + "/release-stream") && ++waits < 500 &&
               !stopping)
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        require(waits < 500 && !stopping,
                "HTTP consumer did not release first delta");
      }
      if (request.logprobs) {
        event.raw_offset = detail::from_unsigned(tokens);
        emit(AILO_EVENT_OUTPUT_PROGRESS);
      }
    }
  };
  if (function)
    stream(name, AILO_EVENT_FUNCTION_NAME_DELTA);
  stream(text, function ? AILO_EVENT_FUNCTION_ARGUMENTS_DELTA
                        : AILO_EVENT_TEXT_DELTA);
  event.finish_reason = function ? AILO_FINISH_TOOL_CALLS : AILO_FINISH_STOP;
  if (!function)
    emit(AILO_EVENT_CONTENT_END);
  emit(AILO_EVENT_ITEM_END);
  emit(AILO_EVENT_CHOICE_END);
  info.finish_reason = event.finish_reason;
  info.completed_candidates = 1;
  emit(AILO_EVENT_USAGE);
  emit(AILO_EVENT_RUN_END);
  std::fprintf(
      stderr, "fixture calls=%u results=%u answers=%u summaries=%u tokens=%u\n",
      tool_calls, tool_results, answers, summaries, tokens);
}
} // namespace client
} // namespace ailo
namespace serving_test {
void inspect_model_input(ailo_client *, const std::string &) {}
} // namespace serving_test
namespace serving_test {
void observe_model(ailo_client *, json &) {}
} // namespace serving_test
