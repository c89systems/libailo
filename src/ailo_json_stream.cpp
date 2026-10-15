#include "ailo_json_stream_internal.hpp"

namespace ailo {
namespace client {
namespace {
void checked(lonejson_status status) {
  if (status != LONEJSON_STATUS_OK)
    throw failure(
        status == LONEJSON_STATUS_ALLOCATION_FAILED ? AILO_NO_MEMORY
                                                    : AILO_MODEL_FAILED,
        "Generated JSON is invalid, incomplete or exceeds parser limits");
}
} // namespace

const lonejson_map &json_stream::map() {
  static const lonejson_field fields[] = {
      LONEJSON_FIELD_JSON_VALUE_REQ(record, value, "value")};
  LONEJSON_MAP_DEFINE(value_map, record, fields);
  return value_map;
}

lonejson_status json_stream::complete(void *user, void *) {
  auto &self = *static_cast<json_stream *>(user);
  return ++self.completed == 1 ? LONEJSON_STATUS_OK : LONEJSON_STATUS_OVERFLOW;
}

json_stream::json_stream(const lonejson_path_value_visitor &input,
                         void *context, size_t max_bytes)
    : limit(max_bytes), visitor(input), user(context) {
#define PROXY(name) proxy.name = forward<&lonejson_path_value_visitor::name>
  PROXY(object_begin);
  PROXY(object_end);
  PROXY(object_key_begin);
  PROXY(object_key_chunk);
  PROXY(object_key_end);
  PROXY(array_begin);
  PROXY(array_end);
  PROXY(string_begin);
  PROXY(string_chunk);
  PROXY(string_end);
  PROXY(number_begin);
  PROXY(number_chunk);
  PROXY(number_end);
  PROXY(boolean_value);
  PROXY(null_value);
#undef PROXY
  auto config = lonejson_default_config();
  config.clear_destination_by_default = 0;
  config.max_alloc_bytes = 4 * 1024 * 1024;
  config.max_depth = config.json_value_max_depth = 64;
  config.json_value_max_total_bytes = max_bytes;
  config.json_value_max_string_bytes = max_bytes;
  config.json_value_max_key_bytes = 1024;
  config.json_value_max_number_bytes = 1024;
  runtime.reset(lonejson_new(&config, nullptr));
  if (!runtime)
    throw std::bad_alloc();
  lonejson_init(runtime.get(), &map(), &item);
  try {
    checked(lonejson_json_value_set_parse_path_visitor(&item.value, &proxy,
                                                       this, nullptr));
    stream = lonejson_array_stream_open_push(runtime.get(), "", nullptr);
    if (!stream)
      throw std::bad_alloc();
    feed("[{\"value\":", 10);
  } catch (...) {
    if (stream)
      lonejson_array_stream_close(stream);
    lonejson_cleanup(&map(), &item);
    throw;
  }
}

json_stream::~json_stream() {
  if (stream)
    lonejson_array_stream_close(stream);
  lonejson_cleanup(&map(), &item);
}

void json_stream::feed(const void *data, size_t size) {
  const auto status = lonejson_array_stream_push(stream, &map(), &item, data,
                                                 size, complete, this, nullptr);
  if (callback_error)
    std::rethrow_exception(callback_error);
  checked(status);
}
void json_stream::push(const char *data, size_t size) {
  if (finished || (!data && size) || size > limit - bytes)
    throw failure(
        AILO_MODEL_FAILED,
        "Generated JSON exceeds its byte budget or is already closed");
  bytes += size;
  feed(data, size);
}
void json_stream::finish() {
  if (finished)
    throw failure(AILO_MODEL_FAILED, "Generated JSON is already closed");
  feed("}]", 2);
  const auto status = lonejson_array_stream_finish(stream, &map(), &item,
                                                   complete, this, nullptr);
  if (callback_error)
    std::rethrow_exception(callback_error);
  checked(status);
  if (completed != 1)
    throw failure(AILO_MODEL_FAILED, "Generated JSON value is missing");
  finished = true;
}
} // namespace client
} // namespace ailo
