#include "ailo/base.h"
#include "ailo/model.h"
#include "ailo/native.h"
#include "ailo/version.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  struct ailo_gguf_context *gguf = NULL;
  ailo_model_source source;
  ailo_error error;
  ailo_i64 key;
  ailo_u64 value, actual;
  char *path = NULL;
  if (argc != 2)
    return 2;
  if (strcmp(AILO_VERSION, EXPECTED_VERSION) != 0 || AILO_ABI_VERSION != 0)
    return 1;
  memset(&source, 0, sizeof(source));
  source.local_path = argv[1];
  if (ailo_model_acquire(&source, &path, &error) != AILO_OK) {
    fprintf(stderr, "%s\n", error.message);
    return 1;
  }
  ailo_string_free(path);
  value.hi = 0xffffffffU;
  value.lo = 0xffffffffU;
  if (ailo_gguf_init_empty(&gguf, &error) != AILO_OK || !gguf ||
      ailo_gguf_set_val_u64(gguf, "full.width", value, &error) != AILO_OK ||
      ailo_gguf_find_key(gguf, "full.width", &key, &error) != AILO_OK ||
      ailo_gguf_get_val_u64(gguf, key, &actual, &error) != AILO_OK ||
      actual.hi != value.hi || actual.lo != value.lo ||
      ailo_gguf_free(gguf, &error) != AILO_OK)
    return 1;
  puts("Installed C89/C++ facade, full-width GGUF and model acquisition: "
       "passed");
  return 0;
}
