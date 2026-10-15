#ifndef AILO_TOOL_JSON_INTERNAL_HPP
#define AILO_TOOL_JSON_INTERNAL_HPP
#include "ailo_json_stream_internal.hpp"
#include "ailo_text_stream_internal.hpp"
#include <functional>
#include <set>

namespace ailo {
namespace client {
// Qualified template metadata, independent of the model's actual output.
struct tool_json_format {
  bool array = false;
  bool name_is_key = false;
  std::string wrapper;
  std::string name = "name", arguments = "arguments", id = "id";
};
enum class tool_json_kind { begin, name, arguments, end };
struct tool_json_event {
  tool_json_kind kind;
  size_t index;
  const char *data;
  size_t size;
};

// One JSON tool envelope, never a collected call/argument value. Only bounded
// metadata is retained. push returns at the JSON boundary so the caller can
// handle the template's closing delimiter from the same producer chunk.
class tool_json_stream {
  tool_json_format format;
  std::set<std::string> allowed;
  size_t max_calls, calls = 0;
  std::function<void(const tool_json_event &)> sink;
  std::unique_ptr<json_stream> parser;
  text_stream argument_utf8;
  std::string name, id, type, key;
  std::set<std::string> call_keys, function_keys;
  bool active = false, root_started = false, root_done = false, closed = false,
       args_active = false, args_done = false, name_done = false,
       wrapper_done = false, args_ended = false;
  enum class field { invalid, name, args, id, type, wrapper };
  bool at_call(const lonejson_value_path &path) const;
  bool at_function(const lonejson_value_path &path) const;
  field location(const lonejson_value_path &path) const;
  void send(tool_json_kind kind, const char *data = nullptr, size_t size = 0);
  void begin_call();
  void end_call();
  void structure(const lonejson_value_path &path, bool object, bool begin);
  void key_event(const lonejson_value_path &path, int stage,
                 const char *data = nullptr, size_t size = 0);
  void string_event(const lonejson_value_path &path, int stage,
                    const char *data = nullptr, size_t size = 0);
  void scalar(const lonejson_value_path &path);
  static lonejson_path_value_visitor visitor();

public:
  tool_json_stream(tool_json_format format, std::vector<std::string> names,
                   size_t call_limit, size_t byte_limit,
                   std::function<void(const tool_json_event &)> sink);
  size_t push(const char *data, size_t size);
  void finish();
  bool complete() const { return root_done; }
  size_t count() const { return calls; }
};
} // namespace client
} // namespace ailo
#endif
