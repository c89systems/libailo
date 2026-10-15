#include "ailo_client_internal.hpp"
#include "ailo_conversation_internal.hpp"
#include <algorithm>
#include <cstring>
#include <jinja/caps.h>
#include <jinja/parser.h>
#include <json.h>

namespace ailo {
namespace client {
namespace {
bool is_jinja(const char *source) {
  return std::strstr(source, "{{") || std::strstr(source, "{%");
}

jinja::program parse(const char *source) {
  const size_t size = strnlen(source, 65537);
  if (size > 65536)
    throw failure(AILO_OUT_OF_RANGE, "Chat template exceeds 64 KiB");
  require_utf8(source, size);
  jinja::lexer lexer;
  return jinja::parse_from_tokens(lexer.tokenize(std::string(source, size)));
}

std::string token_text(const llama_vocab *vocab, llama_token token) {
  return token == LLAMA_TOKEN_NULL ? "" : llama_vocab_get_text(vocab, token);
}
} // namespace

bool template_supported(const char *source) {
  if (!source)
    return false;
  try {
    if (is_jinja(source)) {
      (void)parse(source);
      return true;
    }
    return llama_chat_apply_template(source, nullptr, 0, true, nullptr, 0) >= 0;
  } catch (const std::bad_alloc &) {
    throw;
  } catch (...) {
    return false;
  }
}

std::string apply_template(const char *source, const llama_vocab *vocab,
                           const conversation &input, const std::string &tools,
                           bool generation, const template_options *options) {
  std::string result;
  const bool rich = input.rich || tools != "[]" || (options && options->effort);
  if (is_jinja(source)) {
    try {
      auto program = parse(source);
      jinja::caps caps;
      if (rich) {
        caps = jinja::caps_get(program);
        if (input.max_parallel_calls && !caps.supports_tool_calls)
          throw failure(
              AILO_UNSUPPORTED,
              "This chat template cannot represent function-call history");
        if (input.max_parallel_calls > 1 && !caps.supports_parallel_tool_calls)
          throw failure(AILO_UNSUPPORTED, "This chat template cannot represent "
                                          "parallel function-call history");
      }
      jinja::context context(source);
      context.is_get_stats = rich;
      common_json inputs{
          {"messages", common_json::array()},
          {"tools", common_json::parse(tools)},
          {"bos_token", token_text(vocab, llama_vocab_bos(vocab))},
          {"eos_token", token_text(vocab, llama_vocab_eos(vocab))},
          {"add_generation_prompt", generation},
          {"enable_thinking", options && options->thinking}};
      for (const auto &message : input.messages) {
        common_json value{{"role", message.role}, {"content", message.content}};
        if (message.has_reasoning || !message.reasoning.empty())
          value["reasoning_content"] = message.reasoning;
        if (!message.name.empty())
          value["name"] = message.name;
        if (!message.call_id.empty())
          value["tool_call_id"] = message.call_id;
        if (!message.calls.empty()) {
          value["tool_calls"] = common_json::array();
          for (const auto &call : message.calls) {
            common_json arguments = caps.supports_object_arguments
                                        ? common_json::parse(call.arguments)
                                        : common_json::make(call.arguments);
            value["tool_calls"].push_back(
                {{"id", call.id},
                 {"type", "function"},
                 {"function",
                  {{"name", call.name}, {"arguments", std::move(arguments)}}}});
          }
        }
        inputs["messages"].push_back(std::move(value));
      }
      jinja::global_from_json(context, inputs, rich);
      jinja::caps_apply_preserve_reasoning(context, true);
      if (options && options->effort)
        jinja::caps_apply_reasoning_effort(context, options->effort);
      jinja::runtime runtime(context);
      result = jinja::runtime::gather_string_parts(runtime.execute(program))
                   ->as_string()
                   .str();
      if (rich) {
        if (options && options->effort &&
            !context.get_val("reasoning_effort")->stats.used)
          throw failure(AILO_UNSUPPORTED,
                        "Chat template ignores reasoning effort");
        if (tools != "[]" && !context.get_val("tools")->stats.used)
          throw failure(AILO_UNSUPPORTED,
                        "This chat template ignores tool definitions");
        for (size_t i = 0; i < inputs["tools"].size(); ++i) {
          auto function = context.get_val("tools")->at(i)->at("function");
          if (!function->at("name")->stats.used ||
              !function->at("parameters")->stats.used ||
              (!inputs["tools"][i]["function"]["description"]
                    .get<std::string>()
                    .empty() &&
               !function->at("description")->stats.used))
            throw failure(
                AILO_UNSUPPORTED,
                "This chat template ignores a function definition field");
        }
        auto messages = context.get_val("messages");
        for (size_t i = 0; i < input.messages.size(); ++i) {
          const auto &message = input.messages[i];
          auto current = messages->at(i);
          if ((message.has_reasoning || !message.reasoning.empty()) &&
              (!current->at("reasoning_content")->stats.used ||
               result.find(message.reasoning) == std::string::npos))
            throw failure(
                AILO_UNSUPPORTED,
                "Chat template does not preserve supplied reasoning history");
          if ((!message.content.empty() || message.role == "tool") &&
              !current->at("content")->stats.used)
            throw failure(AILO_UNSUPPORTED,
                          "This chat template ignores message or "
                          "function-result content");
          if (!message.name.empty() && message.role != "tool" &&
              !current->at("name")->stats.used)
            throw failure(AILO_UNSUPPORTED,
                          "This chat template ignores participant names");
          if (!message.calls.empty() && !current->at("tool_calls")->stats.used)
            throw failure(AILO_UNSUPPORTED,
                          "This chat template ignores function-call history");
          for (size_t j = 0; j < message.calls.size(); ++j) {
            auto function = current->at("tool_calls")->at(j)->at("function");
            if (!function->at("name")->stats.used ||
                !function->at("arguments")->stats.used)
              throw failure(AILO_UNSUPPORTED, "This chat template ignores a "
                                              "function name or its arguments");
          }
        }
        // Implicit result formats associate parallel results by call order.
        // Normalize only the model prompt, preserving canonical input order.
        std::unique_ptr<conversation> ordered;
        for (size_t i = 0; i < input.messages.size(); ++i) {
          const auto &calls = input.messages[i].calls;
          if (calls.size() < 2 || i + calls.size() >= input.messages.size())
            continue;
          const size_t first = i + 1;
          if (calls.size() > input.messages.size() - first)
            throw failure(AILO_INVALID_ARGUMENT,
                          "Function result group is incomplete");
          bool implicit = false;
          for (size_t j = 0; j < calls.size(); ++j)
            implicit = implicit ||
                       !messages->at(first + j)->at("tool_call_id")->stats.used;
          if (!implicit)
            continue;
          for (size_t j = 0; j < calls.size(); ++j) {
            const auto begin =
                input.messages.begin() + static_cast<ptrdiff_t>(first);
            const auto end = begin + static_cast<ptrdiff_t>(calls.size());
            const auto found =
                std::find_if(begin, end, [&](const chat_message &value) {
                  return value.role == "tool" && value.call_id == calls[j].id;
                });
            if (found == end)
              throw failure(AILO_INVALID_ARGUMENT,
                            "Function result group has an unknown identity");
            if (found != begin + static_cast<ptrdiff_t>(j)) {
              if (!ordered)
                ordered = std::make_unique<conversation>(input);
              ordered->messages[first + j] = *found;
            }
          }
        }
        if (ordered)
          return apply_template(source, vocab, *ordered, tools, generation,
                                options);
      }
    } catch (const failure &) {
      throw;
    } catch (const std::bad_alloc &) {
      throw;
    } catch (...) {
      // Native exceptions can contain source text or conversation contents.
      throw failure(AILO_UNSUPPORTED,
                    "The chat template cannot render this conversation");
    }
  } else {
    if (rich)
      throw failure(
          AILO_UNSUPPORTED,
          "Named text templates cannot represent participant names or function "
          "histories; provide a compatible Jinja template");
    std::vector<llama_chat_message> messages;
    for (const auto &message : input.messages)
      messages.push_back({message.role.c_str(), message.content.c_str()});
    const int32_t size = llama_chat_apply_template(
        source, messages.data(), messages.size(), generation, nullptr, 0);
    if (size < 0)
      throw failure(AILO_UNSUPPORTED, "Unknown named chat template");
    if (size > 8 * 1024 * 1024)
      throw failure(AILO_OUT_OF_RANGE, "Rendered prompt exceeds 8 MiB");
    std::vector<char> buffer(static_cast<size_t>(size) + 1);
    if (llama_chat_apply_template(source, messages.data(), messages.size(),
                                  generation, buffer.data(),
                                  buffer.size()) != size)
      throw failure(AILO_MODEL_FAILED, "Chat template rendering failed");
    result.assign(buffer.data(), static_cast<size_t>(size));
  }
  if (result.size() > 8 * 1024 * 1024)
    throw failure(AILO_OUT_OF_RANGE, "Rendered prompt exceeds 8 MiB");
  require_utf8(result.data(), result.size());
  return result;
}
} // namespace client
} // namespace ailo
