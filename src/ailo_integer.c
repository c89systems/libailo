#include "ailo_error_internal.h"

/* Only 32- or 64-bit long is part of the supported target contract. */
typedef char
    ailo_long_width_check[(sizeof(long) == 4 || sizeof(long) == 8) ? 1 : -1];

ailo_u64 ailo_u64_from_ulong(unsigned long value) {
  ailo_u64 result;
  result.lo = (ailo_u32)value;
#if ULONG_MAX > 4294967295UL
  result.hi = (ailo_u32)(value >> 16 >> 16);
#else
  result.hi = 0;
#endif
  return result;
}

ailo_i64 ailo_i64_from_long(long value) {
  ailo_i64 result;
  result.lo = (ailo_u32)value;
#if LONG_MAX > 2147483647L
  result.hi = (ailo_u32)((unsigned long)value >> 16 >> 16);
#else
  result.hi = value < 0 ? UINT_MAX : 0;
#endif
  return result;
}

ailo_status ailo_u64_to_ulong(ailo_u64 value, unsigned long *output,
                              ailo_error *error) {
  if (output == NULL) {
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "Unsigned integer conversion requires an output");
  }
  *output = 0;
#if ULONG_MAX > 4294967295UL
  *output = ((unsigned long)value.hi << 16 << 16) | value.lo;
#else
  if (value.hi != 0) {
    return ailo_error_set(error, AILO_OUT_OF_RANGE,
                          "64-bit value does not fit unsigned long");
  }
  *output = value.lo;
#endif
  ailo_error_clear(error);
  return AILO_OK;
}

ailo_status ailo_i64_to_long(ailo_i64 value, long *output, ailo_error *error) {
  unsigned long bits;
  unsigned long magnitude;
  if (output == NULL) {
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "Signed integer conversion requires an output");
  }
  *output = 0;
#if LONG_MAX > 2147483647L
  bits = ((unsigned long)value.hi << 16 << 16) | value.lo;
#else
  if ((value.hi & 0x80000000U) != 0) {
    if (value.hi != UINT_MAX || value.lo < 0x80000000U) {
      return ailo_error_set(error, AILO_OUT_OF_RANGE,
                            "64-bit value does not fit signed long");
    }
  } else if (value.hi != 0 || value.lo > (unsigned long)LONG_MAX) {
    return ailo_error_set(error, AILO_OUT_OF_RANGE,
                          "64-bit value does not fit signed long");
  }
  bits = value.lo;
#endif
  if ((value.hi & 0x80000000U) == 0) {
    *output = (long)bits;
  } else {
    magnitude = 0UL - bits;
    /* The minimum's magnitude is one beyond LONG_MAX; never cast it to long. */
    if (magnitude == (unsigned long)LONG_MAX + 1UL) {
      *output = LONG_MIN;
    } else {
      *output = -(long)magnitude;
    }
  }
  ailo_error_clear(error);
  return AILO_OK;
}
