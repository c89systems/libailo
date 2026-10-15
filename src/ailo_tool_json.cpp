#include "ailo_tool_json_internal.hpp"
#include <algorithm>
#include <cstring>

namespace ailo {
namespace client {
namespace {
[[noreturn]] void malformed() {
  throw failure(
      AILO_MODEL_FAILED,
      "Generated function call has an invalid or unsupported envelope");
}
bool equal(const lonejson_path_segment &value, const std::string &expected) {
  return value.len == expected.size() &&
         !std::memcmp(value.data, expected.data(), value.len);
}
bool identifier(const std::string &value) {
  if (value.empty() || value.size() > 64)
    return false;
  for (unsigned char c : value)
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-'))
      return false;
  return true;
}
void append(std::string &value, const char *data, size_t size, size_t limit) {
  if (size > limit - value.size())
    malformed();
  value.append(data, size);
}
} // namespace

bool tool_json_stream::at_call(const lonejson_value_path &path) const {
  if (!format.array)
    return path.segment_count == 0;
  return path.segment_count == 1 &&
         equal(path.segments[0], std::to_string(calls));
}
bool tool_json_stream::at_function(const lonejson_value_path &path) const {
  if (format.wrapper.empty())
    return at_call(path);
  if (!path.segment_count ||
      !equal(path.segments[path.segment_count - 1], format.wrapper))
    return false;
  auto parent = path;
  --parent.segment_count;
  return at_call(parent);
}
tool_json_stream::field
tool_json_stream::location(const lonejson_value_path &path) const {
  if (!path.segment_count)
    return field::invalid;
  auto parent = path;
  --parent.segment_count;
  const auto &last = path.segments[parent.segment_count];
  if (at_function(parent)) {
    if (format.name_is_key)
      return equal(last, name) ? field::args : field::invalid;
    if (equal(last, format.name))
      return field::name;
    if (equal(last, format.arguments))
      return field::args;
  }
  if (at_call(parent)) {
    if (!format.id.empty() && equal(last, format.id))
      return field::id;
    if (equal(last, "type"))
      return field::type;
    if (!format.wrapper.empty() && equal(last, format.wrapper))
      return field::wrapper;
  }
  return field::invalid;
}
void tool_json_stream::send(tool_json_kind kind, const char *data,
                            size_t size) {
  sink({kind, calls, data, size});
}
void tool_json_stream::begin_call() {
  if (active || calls >= max_calls)
    malformed();
  name.clear();
  id.clear();
  type.clear();
  key.clear();
  call_keys.clear();
  function_keys.clear();
  args_done = name_done = wrapper_done = false;
  active = true;
  send(tool_json_kind::begin);
}
void tool_json_stream::end_call() {
  if (!active || !args_done || !name_done ||
      (!format.wrapper.empty() && !wrapper_done))
    malformed();
  send(tool_json_kind::end);
  active = false;
  ++calls;
}
void tool_json_stream::structure(const lonejson_value_path &path, bool object,
                                 bool begin) {
  if (args_active) {
    if (!begin && object && location(path) == field::args) {
      args_active = false;
      args_ended = true;
      args_done = true;
    }
    return;
  }
  if (begin && !root_started) {
    if (path.segment_count || object == format.array)
      malformed();
    root_started = true;
    if (!object)
      return;
  }
  if (format.array && !path.segment_count) {
    if (object || begin || active || !calls)
      malformed();
    root_done = true;
    return;
  }
  if (object && at_call(path)) {
    if (begin)
      begin_call();
    else {
      end_call();
      if (!format.array)
        root_done = true;
    }
    return;
  }
  if (!active || !object)
    malformed();
  switch (location(path)) {
  case field::wrapper:
    if (!begin)
      wrapper_done = true;
    return;
  case field::args:
    if (!begin || args_done)
      malformed();
    args_active = true;
    return;
  default:
    malformed();
  }
}
void tool_json_stream::key_event(const lonejson_value_path &path, int stage,
                                 const char *data, size_t size) {
  if (args_active)
    return;
  if (!active || (!at_call(path) && !at_function(path)))
    malformed();
  if (stage == 0) {
    key.clear();
  } else if (stage == 1) {
    append(key, data, size, 256);
  } else {
    auto &seen = at_call(path) ? call_keys : function_keys;
    if (!seen.insert(key).second || seen.size() > 5)
      malformed();
    lonejson_path_segment segments[3];
    if (path.segment_count > 2)
      malformed();
    std::copy_n(path.segments, path.segment_count, segments);
    segments[path.segment_count] = {key.data(), key.size()};
    if (format.name_is_key && at_function(path)) {
      if (!identifier(key) || !allowed.count(key) || name_done)
        malformed();
      name = key;
      name_done = true;
      send(tool_json_kind::name, name.data(), name.size());
    } else if (location({segments, path.segment_count + 1}) == field::invalid) {
      malformed();
    }
  }
}
void tool_json_stream::string_event(const lonejson_value_path &path, int stage,
                                    const char *data, size_t size) {
  if (args_active)
    return;
  if (!active)
    malformed();
  const auto which = location(path);
  if (which != field::name && which != field::id && which != field::type)
    malformed();
  auto &value = which == field::name ? name : which == field::id ? id : type;
  if (stage == 0) {
    value.clear();
  } else if (stage == 1) {
    append(value, data, size, which == field::name ? 64 : 256);
    if (which == field::name)
      send(tool_json_kind::name, data, size);
  } else if (which == field::name) {
    if (!identifier(name) || !allowed.count(name))
      malformed();
    name_done = true;
  } else if ((which == field::type && type != "function") ||
             (which == field::id &&
              (id.empty() || id.find('\0') != std::string::npos))) {
    malformed();
  }
}
void tool_json_stream::scalar(const lonejson_value_path &) {
  if (!args_active)
    malformed();
}

lonejson_path_value_visitor tool_json_stream::visitor() {
  auto result = lonejson_default_path_value_visitor();
#define STRUCTURE(member, object, begin)                                       \
  result.member = [](void *user, const lonejson_value_path *path,              \
                     lonejson_error *) -> lonejson_status {                    \
    static_cast<tool_json_stream *>(user)->structure(*path, object, begin);    \
    return LONEJSON_STATUS_OK;                                                 \
  }
  STRUCTURE(object_begin, true, true);
  STRUCTURE(object_end, true, false);
  STRUCTURE(array_begin, false, true);
  STRUCTURE(array_end, false, false);
#undef STRUCTURE
#define EVENT(member, method, stage)                                           \
  result.member = [](void *user, const lonejson_value_path *path,              \
                     lonejson_error *) -> lonejson_status {                    \
    static_cast<tool_json_stream *>(user)->method(*path, stage);               \
    return LONEJSON_STATUS_OK;                                                 \
  }
#define CHUNK(member, method)                                                  \
  result.member = [](void *user, const lonejson_value_path *path,              \
                     const char *data, size_t size,                            \
                     lonejson_error *) -> lonejson_status {                    \
    static_cast<tool_json_stream *>(user)->method(*path, 1, data, size);       \
    return LONEJSON_STATUS_OK;                                                 \
  }
  EVENT(object_key_begin, key_event, 0);
  CHUNK(object_key_chunk, key_event);
  EVENT(object_key_end, key_event, 2);
  EVENT(string_begin, string_event, 0);
  CHUNK(string_chunk, string_event);
  EVENT(string_end, string_event, 2);
#undef EVENT
#undef CHUNK
  result.number_begin =
      result.null_value = [](void *user, const lonejson_value_path *path,
                             lonejson_error *) -> lonejson_status {
    static_cast<tool_json_stream *>(user)->scalar(*path);
    return LONEJSON_STATUS_OK;
  };
  result.boolean_value = [](void *user, const lonejson_value_path *path, int,
                            lonejson_error *) -> lonejson_status {
    static_cast<tool_json_stream *>(user)->scalar(*path);
    return LONEJSON_STATUS_OK;
  };
  return result;
}

tool_json_stream::tool_json_stream(
    tool_json_format protocol, std::vector<std::string> names,
    size_t call_limit, size_t byte_limit,
    std::function<void(const tool_json_event &)> callback)
    : format(std::move(protocol)), allowed(names.begin(), names.end()),
      max_calls(call_limit), sink(std::move(callback)),
      argument_utf8({}, [&](const char *data, size_t size) {
        emitted = argument_utf8.output_offset();
        send(tool_json_kind::arguments, data, size);
      }) {
  if (!call_limit || call_limit > 128 || names.empty() || names.size() > 128 ||
      allowed.size() != names.size() || !sink || !byte_limit)
    throw failure(AILO_INVALID_ARGUMENT, "Invalid tool stream limits");
  if (format.name_is_key && !format.id.empty())
    throw failure(AILO_INVALID_ARGUMENT,
                  "Function-key tool format cannot include an ID field");
  for (const auto &value : allowed)
    if (!identifier(value))
      throw failure(AILO_INVALID_ARGUMENT, "Invalid tool name");
  // Ambiguous field layouts are rejected before accepting any model bytes.
  std::set<std::string> fields{"type"};
  auto add = [&](const std::string &value) {
    if (value.empty() || value.size() > 256 || !fields.insert(value).second)
      throw failure(AILO_INVALID_ARGUMENT, "Ambiguous tool JSON format");
    require_utf8(value.data(), value.size());
  };
  if (!format.id.empty())
    add(format.id);
  if (!format.wrapper.empty())
    add(format.wrapper);
  if (!format.name_is_key) {
    if (!format.wrapper.empty())
      fields.clear();
    add(format.name);
    add(format.arguments);
  }
  const auto callbacks = visitor();
  parser = std::make_unique<json_stream>(callbacks, this, byte_limit);
}
size_t tool_json_stream::push(const char *data, size_t size, uint64_t origin) {
  if (closed || (!data && size))
    throw failure(AILO_MODEL_FAILED, "Tool envelope is already closed");
  size_t consumed = 0;
  if (origin == no_offset)
    origin = next;
  size_t argument_start = size;
  auto flush = [&](size_t end, bool final) {
    if (argument_start != size) {
      (void)argument_utf8.push(
          std::string(data + argument_start, end - argument_start), false,
          origin + argument_start);
      argument_start = size;
    }
    if (final)
      (void)argument_utf8.push({}, true);
  };
  // At most one input byte is pending in the parser. No future token or
  // argument completion is needed to forward the original argument bytes.
  while (consumed < size && !root_done) {
    const bool was_argument = args_active;
    parser->push(data + consumed, 1);
    if ((was_argument || args_active || args_ended) && argument_start == size)
      argument_start = consumed;
    ++consumed;
    if (args_ended) {
      flush(consumed, true);
      args_ended = false;
    } else if (argument_start != size && consumed - argument_start >= 4096)
      flush(consumed, false);
  }
  flush(consumed, false);
  next = origin + consumed;
  return consumed;
}
void tool_json_stream::finish() {
  if (closed || !root_done || active || !calls)
    malformed();
  parser->finish();
  closed = true;
}
} // namespace client
} // namespace ailo
