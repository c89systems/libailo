#ifndef AILO_BASE_H
#define AILO_BASE_H

#include <limits.h>

#if CHAR_BIT != 8 || USHRT_MAX != 65535U || UINT_MAX != 4294967295U
#error libailo requires 8-bit bytes, 16-bit short and 32-bit int
#endif
#if SCHAR_MIN != (-127 - 1) || SHRT_MIN != (-32767 - 1) ||                     \
    INT_MIN != (-2147483647 - 1)
#error libailo requires the supported signed integer ranges
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Exact-width C89 integers on the supported target set. */
typedef signed char ailo_i8;
typedef unsigned char ailo_u8;
typedef short ailo_i16;
typedef unsigned short ailo_u16;
typedef int ailo_i32;
typedef unsigned int ailo_u32;
/** Zero is false; nonzero is true. Native booleans require conversion. */
typedef int ailo_bool;

/** Unsigned 64-bit value: hi * 2^32 + lo; not a native-layout integer. */
typedef struct ailo_u64 {
  ailo_u32 hi;
  ailo_u32 lo;
} ailo_u64;

/** Signed 64-bit value encoded as two's-complement hi/lo words. */
typedef struct ailo_i64 {
  ailo_u32 hi;
  ailo_u32 lo;
} ailo_i64;

/** Bridge/library outcomes; native API result codes are separate values. */
typedef enum ailo_status {
  AILO_OK = 0,
  AILO_INVALID_ARGUMENT,
  AILO_OUT_OF_RANGE,
  AILO_NO_MEMORY,
  AILO_UNSUPPORTED,
  AILO_INVALID_JSON,
  AILO_UNKNOWN_MODEL,
  AILO_DOWNLOAD_FAILED,
  AILO_IO_FAILED,
  AILO_MODEL_FAILED,
  AILO_CONTEXT_LIMIT,
  AILO_BUSY,
  AILO_CANCELLED,
  AILO_SINK_FAILED,
  AILO_INTERNAL_ERROR
} ailo_status;

/** Includes the terminating NUL byte. */
#define AILO_ERROR_MESSAGE_CAPACITY 256

/** Caller-owned, zero-initializable diagnostic; no heap allocation to release.
 */
typedef struct ailo_error {
  ailo_status status;
  char message[AILO_ERROR_MESSAGE_CAPACITY];
} ailo_error;

/** Borrowed constant status name; unknown values return "unknown_status". */
const char *ailo_status_name(int status);
/** Clear a caller-owned error. NULL is accepted. */
void ailo_error_clear(ailo_error *error);
/** Free a project-allocated string. NULL is accepted; borrowed strings are not.
 */
void ailo_string_free(char *string);

/** Exact, allocation-free construction from a supported C89 unsigned long. */
ailo_u64 ailo_u64_from_ulong(unsigned long value);
/** Exact, allocation-free construction from a supported C89 signed long. */
ailo_i64 ailo_i64_from_long(long value);
/** Checked conversion. Clear error on success and zero output on failure. */
ailo_status ailo_u64_to_ulong(ailo_u64 value, unsigned long *output,
                              ailo_error *error);
/** Checked conversion. Clear error on success and zero output on failure. */
ailo_status ailo_i64_to_long(ailo_i64 value, long *output, ailo_error *error);

#ifdef __cplusplus
}
#endif
#endif
