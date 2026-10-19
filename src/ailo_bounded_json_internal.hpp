#ifndef AILO_BOUNDED_JSON_INTERNAL_HPP
#define AILO_BOUNDED_JSON_INTERNAL_HPP
#include "ailo_client_internal.hpp"
#include <cstring>
#include <lonejson.h>

namespace ailo {
namespace client {
/** Explicitly bounded materialization for metadata/compaction/request
 * fragments, never ordinary generated output or protocol delta transport. */
class bounded_json {
  std::unique_ptr<lonejson, decltype(&lonejson_free)> runtime{nullptr,
                                                              lonejson_free};
  lonejson_writer writer{};
  std::string buffer;
  size_t limit;
  static lonejson_status append(void *user, const void *data, size_t size,
                                lonejson_error *) {
    try {
      auto &self = *static_cast<bounded_json *>(user);
      if (size > self.limit - self.buffer.size())
        return LONEJSON_STATUS_OVERFLOW;
      self.buffer.append(static_cast<const char *>(data), size);
      return LONEJSON_STATUS_OK;
    } catch (...) {
      return LONEJSON_STATUS_ALLOCATION_FAILED;
    }
  }
  static void check(lonejson_status status) {
    if (status != LONEJSON_STATUS_OK)
      throw failure(status == LONEJSON_STATUS_ALLOCATION_FAILED
                        ? AILO_NO_MEMORY
                        : AILO_OUT_OF_RANGE,
                    "Cannot serialize bounded receiver JSON");
  }

public:
  explicit bounded_json(size_t maximum = 4 * 1024 * 1024) : limit(maximum) {
    auto config = lonejson_default_config();
    config.write_max_output_bytes = maximum;
    runtime.reset(lonejson_new(&config, nullptr));
    if (!runtime)
      throw std::bad_alloc();
    check(lonejson_writer_init_sink(runtime.get(), &writer, append, this,
                                    nullptr));
  }
  ~bounded_json() { lonejson_writer_cleanup(&writer); }
  bounded_json(const bounded_json &) = delete;
  void object() { check(writer.begin_object(&writer, nullptr)); }
  void end_object() { check(writer.end_object(&writer, nullptr)); }
  void array() { check(writer.begin_array(&writer, nullptr)); }
  void end_array() { check(writer.end_array(&writer, nullptr)); }
  void key(const char *name) {
    check(writer.key(&writer, name, std::strlen(name), nullptr));
  }
  void key(const std::string &name) {
    check(writer.key(&writer, name.data(), name.size(), nullptr));
  }
  void string(const std::string &value) {
    check(writer.string(&writer, value.data(), value.size(), nullptr));
  }
  void string_begin() { check(writer.string_begin(&writer, nullptr)); }
  void string_chunk(const void *p, size_t n) {
    check(
        writer.string_chunk(&writer, static_cast<const char *>(p), n, nullptr));
  }
  void string_end() { check(writer.string_end(&writer, nullptr)); }
  void nullable(const std::string &value) {
    if (value.empty())
      null();
    else
      string(value);
  }
  void null() { check(writer.null_fn(&writer, nullptr)); }
  void boolean(bool value) { check(writer.bool_fn(&writer, value, nullptr)); }
  void number(uint64_t value) { check(writer.u64(&writer, value, nullptr)); }
  void real(double value) { check(writer.f64(&writer, value, nullptr)); }
  void number_text(const std::string &value) {
    check(writer.number_text(&writer, value.data(), value.size(), nullptr));
  }
  std::string finish() {
    check(lonejson_writer_finish(&writer, nullptr));
    return std::move(buffer);
  }
};
} // namespace client
} // namespace ailo
#endif
