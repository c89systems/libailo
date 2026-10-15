#include "ailo_conversation_internal.hpp"
#include <algorithm>
#include <cstring>
#include <lonejson.h>
#include <map>
#include <set>

namespace ailo {
namespace client {
namespace {
const char *role_name(ailo_role role) {
  switch (role) {
  case AILO_ROLE_USER:
    return "user";
  case AILO_ROLE_SYSTEM:
    return "system";
  case AILO_ROLE_DEVELOPER:
    return "developer";
  case AILO_ROLE_ASSISTANT:
    return "assistant";
  case AILO_ROLE_TOOL:
    break;
  }
  throw failure(
      AILO_INVALID_ARGUMENT,
      "Unknown message role or tool message without a function result");
}
std::string bounded(const char *value, size_t limit, bool required) {
  if (!value) {
    if (required)
      throw failure(AILO_INVALID_ARGUMENT,
                    "A function identity or argument value is missing");
    return {};
  }
  const size_t size = strnlen(value, limit + 1);
  if (!size || size > limit)
    throw failure(
        AILO_INVALID_ARGUMENT,
        "An item identity, name or argument value is empty or too long");
  require_utf8(value, size);
  return std::string(value, size);
}
std::string name(const char *value, bool required = false) {
  auto result = bounded(value, 64, required);
  for (unsigned char ch : result)
    if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
          (ch >= '0' && ch <= '9') || ch == '_' || ch == '-'))
      throw failure(AILO_INVALID_ARGUMENT,
                    "Participant/function names use letters, digits, "
                    "underscores or hyphens");
  return result;
}
std::string content(const ailo_item &item, size_t &total) {
  if ((!item.content && item.content_count) || item.content_count > 4096)
    throw failure(AILO_INVALID_ARGUMENT, "Invalid message content array");
  std::string result;
  for (size_t i = 0; i < item.content_count; ++i) {
    const auto &part = item.content[i];
    if (part.kind != AILO_CONTENT_TEXT)
      throw failure(AILO_UNSUPPORTED,
                    "This model adapter accepts text content only");
    if ((!part.data && part.size) || part.size > 4 * 1024 * 1024 - total)
      throw failure(AILO_INVALID_ARGUMENT,
                    "Request text is missing or exceeds 4 MiB");
    if (part.size) {
      const auto *bytes = static_cast<const char *>(part.data);
      require_utf8(bytes, part.size);
      if (std::memchr(bytes, 0, part.size))
        throw failure(
            AILO_UNSUPPORTED,
            "This template adapter cannot represent embedded NUL text");
      result.append(bytes, part.size);
      total += part.size;
    }
  }
  return result;
}
void validate_arguments(const std::string &value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos || value[first] != '{')
    throw failure(AILO_INVALID_ARGUMENT,
                  "Function arguments must be a JSON object");
  auto config = lonejson_default_config();
  config.max_alloc_bytes = 4 * 1024 * 1024;
  config.max_depth = 64;
  std::unique_ptr<lonejson, decltype(&lonejson_free)> runtime(
      lonejson_new(&config, nullptr), lonejson_free);
  if (!runtime)
    throw std::bad_alloc();
  const auto status = lonejson_validate_buffer(runtime.get(), value.data(),
                                               value.size(), nullptr);
  if (status != LONEJSON_STATUS_OK)
    throw failure(status == LONEJSON_STATUS_ALLOCATION_FAILED
                      ? AILO_NO_MEMORY
                      : AILO_INVALID_ARGUMENT,
                  "Function arguments are malformed or exceed JSON limits");
}
} // namespace

conversation prepare_conversation(const ailo_request &request,
                                  bool allow_pending) {
  if (!request.items || !request.item_count || request.item_count > 4096)
    throw failure(AILO_INVALID_ARGUMENT,
                  "Request needs between 1 and 4096 items");
  conversation output;
  size_t total = 0, group_size = 0;
  bool results_started = false;
  std::string reasoning;
  bool pending_reasoning = false;
  std::set<std::string> item_ids, pending;
  struct call_state {
    std::string name;
    bool answered = false;
  };
  std::map<std::string, call_state> calls;
  for (size_t i = 0; i < request.item_count; ++i) {
    const auto &item = request.items[i];
    if (item.encrypted_content)
      throw failure(
          AILO_INVALID_ARGUMENT,
          "Encrypted content requires authenticated compaction replay");
    const auto id = bounded(item.id, 256, false);
    if (!id.empty() && !item_ids.insert(id).second)
      throw failure(AILO_INVALID_ARGUMENT,
                    "Conversation item IDs must be unique");
    if (item.kind == AILO_ITEM_MESSAGE) {
      if (pending_reasoning && item.role != AILO_ROLE_ASSISTANT)
        throw failure(AILO_INVALID_ARGUMENT,
                      "Reasoning history must precede an assistant message or "
                      "function call");
      if (!pending.empty())
        throw failure(AILO_INVALID_ARGUMENT,
                      "Function call groups need complete results before "
                      "another message");
      if (item.call_id || item.arguments)
        throw failure(AILO_INVALID_ARGUMENT,
                      "Message items cannot contain function call fields");
      chat_message message;
      message.role = role_name(item.role);
      message.name = name(item.name);
      message.content = content(item, total);
      message.reasoning = std::move(reasoning);
      message.has_reasoning = pending_reasoning;
      pending_reasoning = false;
      reasoning.clear();
      output.rich = output.rich || !message.name.empty();
      output.messages.push_back(std::move(message));
      continue;
    }
    if (item.kind == AILO_ITEM_FUNCTION_CALL) {
      if (item.content || item.content_count ||
          (item.role != AILO_ROLE_USER && item.role != AILO_ROLE_ASSISTANT))
        throw failure(AILO_INVALID_ARGUMENT,
                      "Function call items contain invalid message fields");
      if (!pending.empty() && results_started)
        throw failure(
            AILO_INVALID_ARGUMENT,
            "A function call cannot interrupt a pending result group");
      chat_call call;
      call.id = bounded(item.call_id, 256, true);
      call.name = name(item.name, true);
      call.arguments = bounded(item.arguments, 4 * 1024 * 1024 - total, true);
      total += call.arguments.size();
      validate_arguments(call.arguments);
      if (!calls.emplace(call.id, call_state{call.name, false}).second)
        throw failure(AILO_INVALID_ARGUMENT,
                      "Function call IDs must be unique");
      if (pending.empty()) {
        group_size = 0;
        results_started = false;
        const bool append = i &&
                            request.items[i - 1].kind == AILO_ITEM_MESSAGE &&
                            request.items[i - 1].role == AILO_ROLE_ASSISTANT;
        if (!append) {
          chat_message message;
          message.role = "assistant";
          output.messages.push_back(std::move(message));
        }
        if (pending_reasoning) {
          output.messages.back().reasoning = std::move(reasoning);
          output.messages.back().has_reasoning = true;
          pending_reasoning = false;
          reasoning.clear();
        }
      }
      pending.insert(call.id);
      output.max_parallel_calls =
          std::max(output.max_parallel_calls, ++group_size);
      output.messages.back().calls.push_back(std::move(call));
      output.rich = true;
      continue;
    }
    if (item.kind == AILO_ITEM_REASONING) {
      if (pending_reasoning || !pending.empty() || item.call_id || item.name ||
          item.arguments ||
          (item.role != AILO_ROLE_USER && item.role != AILO_ROLE_ASSISTANT))
        throw failure(AILO_INVALID_ARGUMENT,
                      "Invalid or interrupted reasoning history item");
      reasoning = content(item, total);
      pending_reasoning = true;
      output.rich = true;
      continue;
    }
    if (item.kind == AILO_ITEM_FUNCTION_RESULT) {
      if (item.arguments ||
          (item.role != AILO_ROLE_USER && item.role != AILO_ROLE_TOOL))
        throw failure(AILO_INVALID_ARGUMENT,
                      "Function result items contain invalid message fields");
      const auto call_id = bounded(item.call_id, 256, true);
      const auto found = calls.find(call_id);
      if (found == calls.end() || found->second.answered ||
          !pending.count(call_id))
        throw failure(AILO_INVALID_ARGUMENT,
                      "Function result is orphaned, duplicated or outside its "
                      "pending group");
      const auto result_name = name(item.name);
      if (!result_name.empty() && result_name != found->second.name)
        throw failure(AILO_INVALID_ARGUMENT,
                      "Function result name does not match its call");
      chat_message message;
      message.role = "tool";
      message.name = found->second.name;
      message.call_id = call_id;
      message.content = content(item, total);
      output.messages.push_back(std::move(message));
      found->second.answered = true;
      pending.erase(call_id);
      results_started = true;
      output.rich = true;
      continue;
    }
    throw failure(
        AILO_UNSUPPORTED,
        "The requested conversation item kind needs an unavailable adapter");
  }
  if (!allow_pending && !pending.empty())
    throw failure(
        AILO_INVALID_ARGUMENT,
        "Unresolved function calls need their results before generation");
  if (pending_reasoning)
    throw failure(AILO_INVALID_ARGUMENT,
                  "Reasoning history needs its assistant message or call");
  output.pending_calls = pending.size();
  return output;
}
} // namespace client
} // namespace ailo
