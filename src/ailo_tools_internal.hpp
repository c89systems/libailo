#ifndef AILO_TOOLS_INTERNAL_HPP
#define AILO_TOOLS_INTERNAL_HPP
#include "ailo_conversation_internal.hpp"
#include "ailo_tool_json_internal.hpp"

namespace ailo {
namespace client {
struct function_tool {
  std::string name, description, parameters, argument_schema;
  bool strict;
};
struct tool_plan {
  std::vector<function_tool> definitions;
  std::vector<std::string> names;
  std::string definitions_json = "[]";
  ailo_tool_choice choice;
  size_t limit;
  tool_json_format format;
  std::string open, close;
};
tool_plan prepare_tools(const ailo_request &request);
void qualify_tools(tool_plan &plan, const char *source,
                   const llama_vocab *vocab);
std::string tool_grammar(const tool_plan &plan, const ailo_request &request);

enum class tool_output_kind { text, begin, name, arguments, end };
struct tool_output_event {
  tool_output_kind kind;
  size_t index;
  const char *data;
  size_t size;
};
class tool_output_stream {
  const tool_plan &plan;
  size_t quota, calls = 0, base = 0, closing = 0;
  std::string pending;
  std::function<void(const tool_output_event &)> sink;
  text_stream text;
  std::unique_ptr<tool_json_stream> json;
  enum class stage { text, json, close } current = stage::text;

public:
  tool_output_stream(const tool_plan &plan, size_t byte_quota,
                     std::function<void(const tool_output_event &)> sink);
  void push(const char *data, size_t size);
  /** Length may leave an unfinished call; stop must be complete. */
  void finish(bool truncated);
  size_t count() const { return calls; }
  bool between_calls() const { return current == stage::text; }
};
} // namespace client
} // namespace ailo
#endif
