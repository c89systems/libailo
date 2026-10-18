#include "ailo/native.h"
#include "ggml.h"
#include <cstdio>
#include <cstring>

static char captured[2048];
static void capture(const char *message) {
  std::snprintf(captured, sizeof(captured), "%s", message);
  std::fputs(captured, stderr);
  std::fputc('\n', stderr);
}
static void logged(ggml_log_level level, const char *message, void *) {
  std::fprintf(stderr, "%s: %s",
               level == GGML_LOG_LEVEL_ERROR ? "error" : "other", message);
}

int main(int argc, char **argv) {
  if (argc == 2 && std::strcmp(argv[1], "--logged-abort") == 0) {
    ggml_log_set(logged, nullptr);
    ggml_abort("logger-test", 42, "logged-fatal");
  }
  if (argc == 2 && std::strcmp(argv[1], "--native-abort") == 0) {
    ggml_set_abort_callback(capture);
    ggml_abort("parity-test", 41, "%03000d", 7);
  }
  if (argc == 2 && std::strcmp(argv[1], "--facade-abort") == 0) {
    ggml_set_abort_callback(capture);
    ailo_ggml_abort("parity-test", 41, nullptr, "%03000d", 7);
    return 99;
  }
  ggml_tensor native = {};
  ailo_ggml_tensor *record = nullptr, *output = nullptr;
  ailo_error error = {};
  if (ailo_ggml_tensor_array_new(1, &record, &error) != AILO_OK)
    return 1;
  int native_count = -1, facade_count = -1;
  ggml_format_name(&native, "%s-%090d%n", "name", 29, &native_count);
  if (ailo_ggml_format_name(record, "%s-%090d%n", &output, &error, "name", 29,
                            &facade_count) != AILO_OK)
    return 2;
  const char *name = nullptr;
  if (ailo_ggml_get_name(record, &name, &error) != AILO_OK)
    return 3;
  if (std::strcmp(name, native.name) || native_count != facade_count)
    return 4;
  if (ailo_ggml_tensor_array_free(record, &error) != AILO_OK)
    return 5;
  std::puts("Native variadic name parity: passed");
  return 0;
}
