#include "ailo_schema_internal.hpp"
#include "ailo_tools_internal.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <json-schema-to-grammar.h>
#include <nlohmann/json.hpp>
#include <sstream>

namespace ailo {
namespace client {
namespace {
using json = nlohmann::ordered_json;
[[noreturn]] void invalid(const char *message) {
  throw failure(AILO_INVALID_ARGUMENT, message);
}
[[noreturn]] void unsupported() {
  throw failure(AILO_UNSUPPORTED,
                "Chat template has no qualified streaming JSON tool format");
}
std::string bounded(const char *value, size_t limit, bool optional = false) {
  if (!value) {
    if (optional)
      return {};
    invalid("Required function-tool field is missing");
  }
  const size_t size = strnlen(value, limit + 1);
  if (size > limit)
    throw failure(AILO_OUT_OF_RANGE,
                  "Function-tool field exceeds its byte limit");
  require_utf8(value, size);
  return {value, size};
}
std::string function_name(const char *input) {
  auto name = bounded(input, 64);
  if (name.empty())
    invalid("Function name is empty");
  for (unsigned char ch : name)
    if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
          (ch >= '0' && ch <= '9') || ch == '_' || ch == '-'))
      invalid(
          "Function names require ASCII letters, digits, underscore or hyphen");
  return name;
}
bool object_schema(const json &value) {
  if (value.contains("type"))
    return value["type"] == "object";
  if (value.contains("const"))
    return value["const"].is_object();
  if (value.contains("enum"))
    return std::all_of(value["enum"].begin(), value["enum"].end(),
                       [](const json &entry) { return entry.is_object(); });
  if (value.contains("anyOf"))
    return std::all_of(value["anyOf"].begin(), value["anyOf"].end(),
                       object_schema);
  return false;
}
bool white(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
std::string trim(std::string value) {
  while (!value.empty() && white(value.back()))
    value.pop_back();
  while (!value.empty() && white(value.front()))
    value.erase(0, 1);
  return value;
}
bool starts(const std::string &value, const std::string &prefix) {
  return value.compare(0, prefix.size(), prefix) == 0;
}
bool ends(const std::string &value, const std::string &suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
             0;
}
std::string probe_output(const tool_plan &plan, const char *source,
                         const llama_vocab *vocab, size_t count,
                         const std::string &function) {
  conversation input;
  input.rich = true;
  input.messages.push_back({"user", "AILO_PROBE_USER", {}, {}, {}});
  const auto prefix =
      apply_template(source, vocab, input, plan.definitions_json);
  const std::string marker = "AILO_PROBE_CONTENT_7ab94d";
  input.messages.push_back({"assistant", marker, {}, {}, {}});
  const auto normal =
      apply_template(source, vocab, input, plan.definitions_json, false);
  const auto pos = normal.find(marker, prefix.size());
  if (!starts(normal, prefix) || pos != prefix.size() ||
      normal.find(marker, pos + marker.size()) != std::string::npos)
    unsupported();
  const auto tail = normal.substr(pos + marker.size());
  auto &assistant = input.messages.back();
  assistant.content.clear();
  for (size_t i = 0; i < count; ++i)
    assistant.calls.push_back({"ailo.probe.call." + std::to_string(i), function,
                               "{\"ailo_probe_key\":\"ailo_probe_value\"}"});
  input.max_parallel_calls = count;
  const auto call =
      apply_template(source, vocab, input, plan.definitions_json, false);
  if (!starts(call, prefix) || !ends(call, tail) ||
      call.size() < prefix.size() + tail.size())
    unsupported();
  auto result =
      call.substr(prefix.size(), call.size() - prefix.size() - tail.size());
  if (result.size() > 16384)
    unsupported();
  return result;
}
json envelope_schema(const tool_plan &plan, const function_tool &tool) {
  const json args =
      tool.strict ? json::parse(tool.argument_schema)
                  : json{{"type", "object"}, {"additionalProperties", true}};
  json properties = json::object(), required = json::array();
  if (plan.format.name_is_key) {
    properties[tool.name] = args;
    required.push_back(tool.name);
  } else {
    properties[plan.format.name] = {{"const", tool.name}};
    properties[plan.format.arguments] = args;
    required = {plan.format.name, plan.format.arguments};
  }
  json function = {{"type", "object"},
                   {"properties", properties},
                   {"required", required},
                   {"additionalProperties", false}};
  if (!plan.format.wrapper.empty()) {
    properties = json::object();
    properties[plan.format.wrapper] = function;
    required = {plan.format.wrapper};
  }
  if (!plan.format.id.empty())
    properties[plan.format.id] = {{"type", "string"}, {"maxLength", 256}};
  if (!plan.format.name_is_key)
    properties["type"] = {{"const", "function"}};
  return {{"type", "object"},
          {"properties", properties},
          {"required", required},
          {"additionalProperties", false}};
}
} // namespace

tool_plan prepare_tools(const ailo_request &request) {
  tool_plan plan;
  plan.choice = request.tool_choice;
  plan.limit = request.max_tool_calls        ? request.max_tool_calls
               : request.parallel_tool_calls ? 128
                                             : 1;
  if (request.tool_count > 128 || (request.tool_count && !request.tools) ||
      request.allowed_tool_count > 128 ||
      (request.allowed_tool_count && !request.allowed_tools) ||
      plan.limit > 128 || request.tool_choice < AILO_TOOLS_AUTO ||
      request.tool_choice > AILO_TOOLS_NAMED ||
      (!request.parallel_tool_calls && plan.limit > 1))
    invalid("Invalid function-tool array, choice or call limits");
  if (plan.choice == AILO_TOOLS_NAMED)
    plan.limit = 1;
  if ((plan.choice == AILO_TOOLS_NAMED) != bool(request.tool_name))
    invalid("Named function choice requires only its function name");
  if (!request.tool_count &&
      (request.tool_name || request.allowed_tool_count ||
       request.max_tool_calls || plan.choice == AILO_TOOLS_REQUIRED ||
       plan.choice == AILO_TOOLS_NAMED))
    invalid("Function choice requires function definitions");
  std::set<std::string> available;
  size_t bytes = 0;
  json definitions = json::array();
  for (size_t i = 0; i < request.tool_count; ++i) {
    const auto &input = request.tools[i];
    function_tool tool;
    tool.name = function_name(input.name);
    if (!available.insert(tool.name).second)
      invalid("Duplicate function definition");
    tool.description = bounded(input.description, 65536, true);
    tool.parameters = schema_document(input.parameters, false);
    tool.strict = input.strict != 0;
    if (tool.strict)
      tool.argument_schema = schema_document(input.parameters, true);
    const auto parameters = json::parse(tool.parameters);
    if (tool.strict && !object_schema(json::parse(tool.argument_schema)))
      invalid("Strict function parameters must describe JSON objects");
    bytes +=
        tool.name.size() + tool.description.size() + tool.parameters.size();
    if (bytes > 4 * 1024 * 1024)
      throw failure(AILO_OUT_OF_RANGE, "Tool definitions exceed 4 MiB");
    definitions.push_back({{"type", "function"},
                           {"function",
                            {{"name", tool.name},
                             {"description", tool.description},
                             {"parameters", parameters},
                             {"strict", tool.strict}}}});
    plan.definitions.push_back(std::move(tool));
  }
  std::set<std::string> selected;
  for (size_t i = 0; i < request.allowed_tool_count; ++i) {
    const auto name = function_name(request.allowed_tools[i]);
    if (!available.count(name) || !selected.insert(name).second)
      invalid("Allowed functions must be unique defined names");
  }
  if (!request.allowed_tool_count)
    selected = available;
  if (request.tool_name) {
    const auto name = function_name(request.tool_name);
    if (!selected.count(name))
      invalid("Named function is not allowed");
    selected = {name};
  }
  if (plan.choice != AILO_TOOLS_NONE)
    plan.names.assign(selected.begin(), selected.end());
  plan.definitions_json = definitions.dump();
  return plan;
}

void qualify_tools(tool_plan &plan, const char *source,
                   const llama_vocab *vocab) {
  if (plan.definitions.empty())
    return;
  try {
    const auto &name = plan.definitions.front().name;
    const auto single = probe_output(plan, source, vocab, 1, name);
    const auto start = single.find_first_of("{[");
    if (start == std::string::npos || start > 128)
      unsupported();
    bool done = false;
    auto visitor = lonejson_default_path_value_visitor();
    visitor.object_end =
        visitor.array_end = [](void *user, const lonejson_value_path *path,
                               lonejson_error *) -> lonejson_status {
      if (!path->segment_count)
        *static_cast<bool *>(user) = true;
      return LONEJSON_STATUS_OK;
    };
    json_stream parser(visitor, &done, 16384);
    size_t end = start;
    while (end < single.size() && !done)
      parser.push(single.data() + end++, 1);
    parser.finish();
    auto value = json::parse(single.substr(start, end - start));
    plan.format.array = value.is_array();
    if (plan.format.array) {
      if (value.size() != 1)
        unsupported();
      value = value[0];
    }
    if (!value.is_object())
      unsupported();
    plan.format.id.clear();
    for (const auto &field : value.items())
      if (field.value() == "ailo.probe.call.0")
        plan.format.id = field.key();
    if (value.contains("function") && value["function"].is_object()) {
      plan.format.wrapper = "function";
      value = value["function"];
    }
    if (value.contains(name) && value[name].is_object()) {
      plan.format.name_is_key = true;
      plan.format.id.clear();
    } else {
      bool found_name = false, found_args = false;
      for (const auto &field : value.items()) {
        if (field.value() == name) {
          plan.format.name = field.key();
          found_name = true;
        }
        if (field.value().is_object() &&
            field.value().contains("ailo_probe_key")) {
          plan.format.arguments = field.key();
          found_args = true;
        }
      }
      if (!found_name || !found_args)
        unsupported();
    }
    plan.open = single.substr(0, start);
    while (!plan.open.empty() && white(plan.open.back()))
      plan.open.pop_back();
    plan.close = trim(single.substr(end));
    if (plan.open.empty() || plan.open.size() > 128 || plan.close.size() > 128)
      unsupported();
    for (unsigned char ch : plan.open + plan.close)
      if (!ch || ch > 127)
        unsupported();
    // Exercise the exact decoder against the rendered examples, including
    // parallel call layout. Model output never participates in qualification.
    auto check = [&](const std::string &probe, size_t count,
                     const std::string &function) {
      tool_plan qualification = plan;
      qualification.limit = count;
      qualification.names = {function};
      size_t calls = 0;
      std::string arguments;
      tool_output_stream stream(
          qualification, 16384, [&](const tool_output_event &event) {
            if (event.kind == tool_output_kind::text) {
              for (size_t i = 0; i < event.size; ++i)
                if (!white(event.data[i]))
                  unsupported();
            }
            if (event.kind == tool_output_kind::arguments)
              arguments.append(event.data, event.size);
            if (event.kind == tool_output_kind::end) {
              if (json::parse(arguments) !=
                  json{{"ailo_probe_key", "ailo_probe_value"}})
                unsupported();
              arguments.clear();
              ++calls;
            }
          });
      stream.push(probe.data(), probe.size());
      stream.finish(false);
      if (calls != count)
        unsupported();
    };
    check(single, 1, name);
    for (const auto &function : plan.names)
      if (function != name)
        check(probe_output(plan, source, vocab, 1, function), 1, function);
    if (plan.limit > 1 && plan.choice != AILO_TOOLS_NONE)
      check(probe_output(plan, source, vocab, 2, name), 2, name);
  } catch (const std::bad_alloc &) {
    throw;
  } catch (...) {
    unsupported();
  }
}

std::string tool_grammar(const tool_plan &plan, const ailo_request &request) {
  if (plan.definitions.empty())
    return output_grammar(request);
  // The receiver validates the normal-answer schema before model acquisition.
  const bool required =
      plan.choice == AILO_TOOLS_REQUIRED || plan.choice == AILO_TOOLS_NAMED;
  const size_t limit = plan.names.empty()  ? 0
                       : plan.format.array ? 1
                                           : plan.limit;
  std::set<char> alphabet(plan.open.begin(), plan.open.end());
  auto state = [](size_t n, size_t k) {
    return "text-" + std::to_string(n) + "-" + std::to_string(k);
  };
  auto result = build_grammar([&](const common_grammar_builder &builder) {
    std::string body;
    if (limit) {
      std::vector<std::string> alternatives;
      for (const auto &tool : plan.definitions)
        if (std::find(plan.names.begin(), plan.names.end(), tool.name) !=
            plan.names.end())
          alternatives.push_back(builder.add_schema(
              "function-" + std::to_string(alternatives.size()),
              *common_chat_schema_from_json(
                   common_json::parse(envelope_schema(plan, tool).dump()))
                   .root));
      std::string choice;
      for (const auto &alt : alternatives)
        choice += (choice.empty() ? "" : " | ") + alt;
      auto call = builder.add_rule("call", choice);
      if (plan.format.array) {
        body = "\"[\" [ \\t\\r\\n]{0,20} " + call +
               " (\",\" [ \\t\\r\\n]{0,20} " + call + "){0," +
               std::to_string(plan.limit - 1) + "} \"]\"";
      } else
        body = call;
      body = "[ \\t\\r\\n]{0,20} " + body + " [ \\t\\r\\n]{0,20} " +
             gbnf_format_literal(plan.close);
      builder.add_rule("call-body", body);
    }
    const bool constrained = request.output_format != AILO_OUTPUT_TEXT;
    auto after = [&](size_t n) {
      return constrained ? "after-" + std::to_string(n) : state(n, 0);
    };
    if (constrained)
      for (size_t n = 1; n <= limit; ++n)
        builder.add_rule(
            after(n),
            "\"\"" + (n < limit ? " | " + gbnf_format_literal(plan.open) +
                                      " call-body " + after(n + 1)
                                : ""));
    // Each state remembers only a marker prefix. The full marker is reserved
    // exclusively for a constrained call, never the unrestricted text branch.
    if (!constrained)
      for (size_t n = 0; n <= limit; ++n)
        for (size_t k = 0; k < plan.open.size(); ++k) {
          std::string rule = "\"\"";
          std::string excluded = "[^";
          for (unsigned char ch : alphabet) {
            char escape[8];
            std::snprintf(escape, sizeof(escape), "\\x%02x", ch);
            excluded += escape;
          }
          rule += " | " + excluded + "] " + state(n, 0);
          for (char ch : alphabet) {
            auto prefix = plan.open.substr(0, k) + ch;
            size_t next = std::min(plan.open.size(), prefix.size());
            while (next && !ends(prefix, plan.open.substr(0, next)))
              --next;
            if (next == plan.open.size()) {
              if (n < limit)
                rule += " | " + gbnf_format_literal(std::string(1, ch)) +
                        " call-body " + state(n + 1, 0);
            } else
              rule += " | " + gbnf_format_literal(std::string(1, ch)) + " " +
                      state(n, next);
          }
          builder.add_rule(state(n, k), rule);
        }
    std::string normal = state(0, 0);
    if (request.output_format != AILO_OUTPUT_TEXT) {
      normal = builder.add_schema(
          "answer",
          *common_chat_schema_from_json(
               common_json::parse(schema_document(request.json_schema, true)))
               .root);
      if (limit)
        normal +=
            " | " + gbnf_format_literal(plan.open) + " call-body " + after(1);
    }
    builder.add_rule("root", required ? gbnf_format_literal(plan.open) +
                                            " call-body " + after(1)
                                      : normal);
  });
  if (result.size() > 4 * 1024 * 1024)
    throw failure(AILO_OUT_OF_RANGE, "Compiled tool grammar exceeds 4 MiB");
  return result;
}
} // namespace client
} // namespace ailo
