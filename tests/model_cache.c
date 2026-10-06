#include "ailo/model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression)                                                      \
  do {                                                                         \
    if (!(expression)) {                                                       \
      fprintf(stderr, "%s:%d: %s (%s)\n", __FILE__, __LINE__, #expression,     \
              error.message);                                                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static int progress(void *data, unsigned int shard, unsigned int shards,
                    ailo_u64 received, ailo_u64 total) {
  int *calls = (int *)data;
  (void)received;
  (void)total;
  if (shard != 1 || shards != 1)
    return 0;
  ++*calls;
  return 1;
}

int main(int argc, char **argv) {
  ailo_model_source source = {0};
  const char *hashes[1];
  char *path = NULL, *other = NULL;
  ailo_error error;
  int calls = 0;
  if (argc != 3) {
    fputs("Usage: ailo_model_cache_test local.gguf isolated-cache-root\n",
          stderr);
    return 2;
  }
  source.local_path = argv[1];
  source.progress = progress;
  source.user_data = &calls;
  hashes[0] =
      "e32806b9651b70b20c6109e4f4aab9e8ef0099108b69a1214e937f8ef1a77668";
  source.sha256 = hashes;
  source.sha256_count = 1;
  CHECK(ailo_model_cache_path(&source, &path, &error) == AILO_OK);
  CHECK(strcmp(path, argv[1]) == 0);
  ailo_string_free(path);
  path = NULL;
  CHECK(ailo_model_acquire(&source, &path, &error) == AILO_OK);
  CHECK(strcmp(path, argv[1]) == 0 && error.status == AILO_OK && calls > 1);
  ailo_string_free(path);
  path = NULL;
  hashes[0] =
      "0000000000000000000000000000000000000000000000000000000000000000";
  CHECK(ailo_model_acquire(&source, &path, &error) == AILO_DOWNLOAD_FAILED &&
        !path);
  source.sha256_count = 0;
  source.sha256 = NULL;
  source.max_bytes.lo = 16;
  CHECK(ailo_model_acquire(&source, &path, &error) == AILO_INVALID_ARGUMENT &&
        !path);
  source.max_bytes.lo = 0;
  source.local_path = NULL;
  source.repo = "owner/model";
  source.revision = "fixed-revision";
  source.filename = "nested/model.gguf";
  source.cache_root = argv[2];
  CHECK(ailo_model_cache_path(&source, &path, &error) == AILO_OK);
  CHECK(strncmp(path, argv[2], strlen(argv[2])) == 0);
  source.revision = "other-revision";
  CHECK(ailo_model_cache_path(&source, &other, &error) == AILO_OK);
  CHECK(strcmp(path, other) != 0);
  ailo_string_free(path);
  ailo_string_free(other);
  path = other = NULL;
  CHECK(ailo_model_acquire(&source, &path, &error) == AILO_DOWNLOAD_FAILED &&
        !path);
  source.filename = "nested/../model.gguf";
  CHECK(ailo_model_cache_path(&source, &path, &error) ==
            AILO_INVALID_ARGUMENT &&
        !path);
  CHECK(ailo_model_cache_path(NULL, &path, &error) == AILO_INVALID_ARGUMENT &&
        !path);
  CHECK(ailo_model_cache_path(&source, NULL, &error) == AILO_INVALID_ARGUMENT);
  puts("Standalone C89 model source, path identity, offline policy and "
       "checksum validation: passed");
  return 0;
}
