#include "ailo/native.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

int main(int argc, char **argv) {
  struct ailo_ggml_tensor *tensor = NULL;
  struct ailo_ggml_tensor *result = NULL;
  const char *name = NULL;
  ailo_error error;
  int written = -1;
  char huge_file[2050];
  if (argc == 2 && strcmp(argv[1], "--abort") == 0) {
    ailo_ggml_abort("facade-test", 37, NULL, "fatal-%d/%s", 19, "message");
    return 99;
  }
  CHECK(ailo_ggml_tensor_array_new(1, &tensor, &error) == AILO_OK);
  CHECK(ailo_ggml_format_name(tensor, "tensor-%d %.2f%s%n", &result, &error, 17,
                              0.25, "-tail", &written) == AILO_OK);
  CHECK(result == tensor && written == 19);
  CHECK(ailo_ggml_get_name(tensor, &name, &error) == AILO_OK);
  CHECK(strcmp(name, "tensor-17 0.25-tail") == 0);
  written = -1;
  CHECK(ailo_ggml_format_name(tensor, "%080d%n", &result, &error, 3,
                              &written) == AILO_OK);
  CHECK(written == 80);
  CHECK(ailo_ggml_get_name(tensor, &name, &error) == AILO_OK);
  CHECK(strlen(name) == AILO_GGML_MAX_NAME - 1);
  CHECK(ailo_ggml_format_name(NULL, "x", &result, &error) ==
            AILO_INVALID_ARGUMENT &&
        result == NULL);
  CHECK(ailo_ggml_format_name(tensor, NULL, &result, &error) ==
            AILO_INVALID_ARGUMENT &&
        result == NULL);
  CHECK(ailo_ggml_format_name(tensor, "x", NULL, &error) ==
        AILO_INVALID_ARGUMENT);
  CHECK(ailo_ggml_abort(NULL, 1, &error, "x") == AILO_INVALID_ARGUMENT);
  CHECK(ailo_ggml_abort("file", 1, &error, NULL) == AILO_INVALID_ARGUMENT);
  memset(huge_file, 'x', sizeof(huge_file) - 1);
  huge_file[sizeof(huge_file) - 1] = '\0';
  CHECK(ailo_ggml_abort(huge_file, 1, &error, "x") == AILO_OUT_OF_RANGE);
  CHECK(ailo_ggml_tensor_array_free(tensor, &error) == AILO_OK);
  puts("C89 variadic formatting and validation: passed");
  return 0;
}
