#include "ailo/base.h"
#include <stdio.h>
#include <string.h>

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr);               \
      return 1;                                                                \
    }                                                                          \
  } while (0)

int main(void) {
  const unsigned long unsigned_values[] = {0UL, 1UL, 2147483647UL, 4294967295UL,
                                           ULONG_MAX};
  const long signed_values[] = {
      0L, 1L, -1L, 2147483647L, -2147483647L - 1L, LONG_MIN, LONG_MAX};
  const char *names[] = {
      "ok",          "invalid_argument", "out_of_range",   "no_memory",
      "unsupported", "invalid_json",     "unknown_model",  "download_failed",
      "io_failed",   "model_failed",     "context_limit",  "busy",
      "cancelled",   "sink_failed",      "internal_error", "not_found"};
  ailo_error error;
  ailo_u64 u;
  ailo_i64 s;
  unsigned long unsigned_output;
  long signed_output;
  size_t i;

  CHECK(sizeof(ailo_u8) == 1 && sizeof(ailo_i8) == 1);
  CHECK(sizeof(ailo_u16) == 2 && sizeof(ailo_i16) == 2);
  CHECK(sizeof(ailo_u32) == 4 && sizeof(ailo_i32) == 4);
  CHECK(sizeof(ailo_u64) == 8 && sizeof(ailo_i64) == 8);
  for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
    CHECK(strcmp(ailo_status_name((ailo_status)i), names[i]) == 0);
  }
  CHECK(strcmp(ailo_status_name(-1), "unknown_status") == 0);
  CHECK(strcmp(ailo_status_name(999), "unknown_status") == 0);
  ailo_error_clear(NULL);
  ailo_string_free(NULL);
  error.status = AILO_BUSY;
  strcpy(error.message, "old error");
  ailo_error_clear(&error);
  CHECK(error.status == AILO_OK && error.message[0] == '\0');

  for (i = 0; i < sizeof(unsigned_values) / sizeof(unsigned_values[0]); ++i) {
    u = ailo_u64_from_ulong(unsigned_values[i]);
    error.status = AILO_BUSY;
    strcpy(error.message, "old error");
    CHECK(ailo_u64_to_ulong(u, &unsigned_output, &error) == AILO_OK);
    CHECK(unsigned_output == unsigned_values[i]);
    CHECK(error.status == AILO_OK && error.message[0] == '\0');
    CHECK(ailo_u64_to_ulong(u, &unsigned_output, NULL) == AILO_OK);
  }
  for (i = 0; i < sizeof(signed_values) / sizeof(signed_values[0]); ++i) {
    s = ailo_i64_from_long(signed_values[i]);
    error.status = AILO_BUSY;
    strcpy(error.message, "old error");
    CHECK(ailo_i64_to_long(s, &signed_output, &error) == AILO_OK);
    CHECK(signed_output == signed_values[i]);
    CHECK(error.status == AILO_OK && error.message[0] == '\0');
    CHECK(ailo_i64_to_long(s, &signed_output, NULL) == AILO_OK);
  }

  s = ailo_i64_from_long(-1);
  CHECK(s.hi == UINT_MAX && s.lo == UINT_MAX);
  s = ailo_i64_from_long(-2147483647L - 1L);
  CHECK(s.hi == UINT_MAX && s.lo == 0x80000000U);
  u = ailo_u64_from_ulong(4294967295UL);
  CHECK(u.hi == 0 && u.lo == UINT_MAX);
  CHECK(ailo_u64_to_ulong(u, NULL, &error) == AILO_INVALID_ARGUMENT);
  CHECK(error.status == AILO_INVALID_ARGUMENT && error.message[0] != '\0');
  CHECK(ailo_i64_to_long(s, NULL, &error) == AILO_INVALID_ARGUMENT);
  CHECK(error.status == AILO_INVALID_ARGUMENT && error.message[0] != '\0');
  CHECK(ailo_u64_to_ulong(u, NULL, NULL) == AILO_INVALID_ARGUMENT);
  CHECK(ailo_i64_to_long(s, NULL, NULL) == AILO_INVALID_ARGUMENT);

  u.hi = UINT_MAX;
  u.lo = UINT_MAX;
  s.hi = 0x80000000U;
  s.lo = 0;
  unsigned_output = 7;
  signed_output = 7;
#if LONG_MAX > 2147483647L
  CHECK(ailo_u64_to_ulong(u, &unsigned_output, &error) == AILO_OK);
  CHECK(unsigned_output == ULONG_MAX);
  CHECK(ailo_i64_to_long(s, &signed_output, &error) == AILO_OK);
  CHECK(signed_output == LONG_MIN);
  s.hi = 0x7fffffffU;
  s.lo = UINT_MAX;
  CHECK(ailo_i64_to_long(s, &signed_output, &error) == AILO_OK);
  CHECK(signed_output == LONG_MAX);
#else
  CHECK(ailo_u64_to_ulong(u, &unsigned_output, &error) == AILO_OUT_OF_RANGE);
  CHECK(unsigned_output == 0 && error.status == AILO_OUT_OF_RANGE);
  CHECK(ailo_i64_to_long(s, &signed_output, &error) == AILO_OUT_OF_RANGE);
  CHECK(signed_output == 0 && error.status == AILO_OUT_OF_RANGE);
  s.hi = UINT_MAX;
  s.lo = 0x7fffffffU;
  CHECK(ailo_i64_to_long(s, &signed_output, &error) == AILO_OUT_OF_RANGE);
  CHECK(signed_output == 0 && error.message[0] != '\0');
  s.hi = 0;
  s.lo = 0x80000000U;
  CHECK(ailo_i64_to_long(s, &signed_output, &error) == AILO_OUT_OF_RANGE);
  CHECK(signed_output == 0);
  u.hi = 1;
  u.lo = 0;
  CHECK(ailo_u64_to_ulong(u, &unsigned_output, NULL) == AILO_OUT_OF_RANGE);
  CHECK(unsigned_output == 0);
#endif
  puts("C89 scalar boundary: passed");
  return 0;
}
