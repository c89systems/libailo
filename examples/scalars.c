#include "ailo/base.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  const ailo_u64 value = {0xffffffffU, 0xffffffffU};
  ailo_error error;
  ailo_status status;
  unsigned long native;
  if (argc == 2 && strcmp(argv[1], "--help") == 0) {
    puts("Usage: ailo_scalars_example [--help]");
    puts("Show a portable 64-bit value and checked native-long conversion.");
    return 0;
  }
  if (argc != 1) {
    fputs("ailo_scalars_example: use --help for usage\n", stderr);
    return 2;
  }
  printf("u64 maximum: 0x%08lx%08lx\n", (unsigned long)value.hi,
         (unsigned long)value.lo);
  status = ailo_u64_to_ulong(value, &native, &error);
  if (status == AILO_OK) {
    printf("native unsigned long: %lu\n", native);
  } else if (status == AILO_OUT_OF_RANGE) {
    printf("native unsigned long: %s\n", error.message);
  } else {
    fprintf(stderr, "%s: %s\n", ailo_status_name(status), error.message);
    return 1;
  }
  return 0;
}
