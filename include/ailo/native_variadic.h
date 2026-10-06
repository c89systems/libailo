#ifndef AILO_NATIVE_VARIADIC_H
#define AILO_NATIVE_VARIADIC_H
#include "ailo/base.h"

#ifdef __cplusplus
extern "C" {
#endif
struct ailo_ggml_tensor;

/** Native printf-style name formatting, including its bounded truncation.
 * Output borrows self. Extra arguments follow error; format/argument types must
 * agree. No exception crosses the C boundary. NULL arguments fail safely. */
ailo_status ailo_ggml_format_name(struct ailo_ggml_tensor *self,
                                  const char *format,
                                  struct ailo_ggml_tensor **output,
                                  ailo_error *error, ...);

/** Native fatal diagnostic and abort. Extra arguments follow format. The
 * original file/line prefix, bounded formatting and native abort hook are
 * preserved. Valid calls do not return. Invalid file/format/prefix fails before
 * aborting; ordinary native printf argument preconditions still apply. */
ailo_status ailo_ggml_abort(const char *file, int line, ailo_error *error,
                            const char *format, ...);
#ifdef __cplusplus
}
#endif
#endif
