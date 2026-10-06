#include "ailo/model.h"
#include <stdio.h>
#include <string.h>

static void usage(void) {
  puts("Usage: ailo_model_cache_example --local MODEL\n"
       "   or: ailo_model_cache_example --repo OWNER/REPO --revision REV "
       "--file FILE\n"
       "Options: --cache ABSOLUTE-DIRECTORY --sha256 HEX --download\n"
       "Downloads require --download; missing files otherwise fail offline.");
}

int main(int argc, char **argv) {
  ailo_model_source source = {0};
  ailo_error error;
  ailo_status status;
  const char *checksum[1];
  char *path = NULL;
  int i;
  for (i = 1; i < argc; ++i) {
    const char *flag = argv[i];
    if (strcmp(flag, "--help") == 0) {
      usage();
      return 0;
    }
    if (strcmp(flag, "--download") == 0) {
      source.allow_download = 1;
      continue;
    }
    if (i + 1 >= argc) {
      fputs("Option requires a value; use --help\n", stderr);
      return 2;
    }
    ++i;
    if (strcmp(flag, "--local") == 0)
      source.local_path = argv[i];
    else if (strcmp(flag, "--repo") == 0)
      source.repo = argv[i];
    else if (strcmp(flag, "--revision") == 0)
      source.revision = argv[i];
    else if (strcmp(flag, "--file") == 0)
      source.filename = argv[i];
    else if (strcmp(flag, "--cache") == 0)
      source.cache_root = argv[i];
    else if (strcmp(flag, "--sha256") == 0) {
      checksum[0] = argv[i];
      source.sha256 = checksum;
      source.sha256_count = 1;
    } else {
      fputs("Unknown option; use --help\n", stderr);
      return 2;
    }
  }
  status = ailo_model_acquire(&source, &path, &error);
  if (status != AILO_OK) {
    fprintf(stderr, "%s: %s\n", ailo_status_name(status), error.message);
    return 1;
  }
  puts(path);
  ailo_string_free(path);
  return 0;
}
