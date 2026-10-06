#include "ailo/native_variadic.h"
#include "ailo_error_internal.h"
#include "ggml.h"
#include <cstdarg>
#include <cstdio>

extern "C" __attribute__((visibility("default"))) ailo_status
ailo_ggml_format_name(struct ailo_ggml_tensor *self, const char *format,
                      struct ailo_ggml_tensor **output, ailo_error *error,
                      ...) {
  if (output == nullptr) {
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "Tensor formatting requires an output");
  }
  *output = nullptr;
  if (self == nullptr || format == nullptr) {
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "Tensor and format are required");
  }
  auto *tensor = reinterpret_cast<ggml_tensor *>(self);
  va_list arguments;
  va_start(arguments, error);
  std::vsnprintf(tensor->name, sizeof(tensor->name), format, arguments);
  va_end(arguments);
  *output = self;
  ailo_error_clear(error);
  return AILO_OK;
}

extern "C" __attribute__((visibility("default"))) ailo_status ailo_ggml_abort(
    const char *file, int line, ailo_error *error, const char *format, ...) {
  if (file == nullptr || format == nullptr) {
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "Fatal formatting requires file and format");
  }
  /* Match the native 2048-byte message and prefix reservation. Consume the
   * caller's printf arguments exactly once, including any %n side effects. */
  char prefix[2048];
  const int offset =
      std::snprintf(prefix, sizeof(prefix), "%s:%d: ", file, line);
  if (offset < 0 || static_cast<size_t>(offset) >= sizeof(prefix)) {
    return ailo_error_set(error, AILO_OUT_OF_RANGE,
                          "Fatal diagnostic prefix exceeds its native buffer");
  }
  char body[2048] = {};
  va_list arguments;
  va_start(arguments, format);
  std::vsnprintf(body, sizeof(body) - static_cast<size_t>(offset), format,
                 arguments);
  va_end(arguments);
  ggml_abort(file, line, "%s", body);
}
