#ifndef AILO_NATIVE_MACROS_H
#define AILO_NATIVE_MACROS_H
#include "ailo/native_variadic.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* C89 declarations intentionally omit non-C89 compiler annotations. */
#define AILO_GGML_API extern
#define AILO_GGML_BACKEND_API extern
#define AILO_LLAMA_API
#define AILO_MTMD_API
#define AILO_GGML_RESTRICT
#define AILO_GGML_NORETURN
#define AILO_GGML_DEPRECATED(function, hint) function
#define AILO_GGML_ATTRIBUTE_FORMAT(format_index, first_argument)

#define AILO_GGML_N_TASKS_MAX (-1)
#define AILO_GGML_PAD(value, alignment)                                        \
  (((value) + (alignment) - 1) & ~((alignment) - 1))
#define AILO_GGML_UNUSED(value) ((void)(value))
/* Use a marker followed by a tuple: AILO_GGML_UNUSED_VARS((0, a, b, c)). The
 * sizeof expression references a typed call but never evaluates arguments. */
#define AILO_GGML_UNUSED_VARS(values)                                          \
  ((void)sizeof(ailo_ggml_unused_values values))
/* A C89 tuple replaces variadic preprocessing. Include file, line and error:
 * AILO_GGML_ABORT((__FILE__, __LINE__, NULL, "failure %d", code)). */
#define AILO_GGML_ABORT(arguments) ((void)ailo_ggml_abort arguments)
#define AILO_GGML_ASSERT(expression)                                           \
  do {                                                                         \
    if (!(expression))                                                         \
      (void)ailo_ggml_abort(__FILE__, __LINE__, NULL,                          \
                            "GGML_ASSERT(%s) failed", #expression);            \
  } while (0)
#define AILO_GGML_UNREACHABLE()                                                \
  do {                                                                         \
    fputs("statement should be unreachable\n", stderr);                        \
    abort();                                                                   \
  } while (0)

#ifdef __cplusplus
extern "C" {
#endif
struct ailo_ggml_tensor;
/** Expression-type anchor for the unevaluated unused-values macro. */
int ailo_ggml_unused_values(int marker, ...);
/** Native tensor size without exposing its layout. */
size_t ailo_ggml_tensor_native_size(void);
/** Dimension helpers for the C89 locals macros. NULL/out-of-range returns 0;
 * a nonnull tensor must be a valid live native-backed record. */
ailo_i64 ailo_ggml_tensor_ne_value(const struct ailo_ggml_tensor *tensor,
                                   size_t index);
size_t ailo_ggml_tensor_nb_value(const struct ailo_ggml_tensor *tensor,
                                 size_t index);
#ifdef __cplusplus
}
#endif
#define AILO_GGML_TENSOR_SIZE (ailo_ggml_tensor_native_size())

/* Keep declarations together so these compositions remain valid C89. The
 * selected type must be ailo_i64 for ne, or size_t for nb. */
#define AILO_GGML_TENSOR_LOCALS_1(type, prefix, pointer, array)                \
  const type prefix##0 = ailo_ggml_tensor_##array##_value((pointer), 0);
#define AILO_GGML_TENSOR_LOCALS_2(type, prefix, pointer, array)                \
  AILO_GGML_TENSOR_LOCALS_1(type, prefix, pointer, array)                      \
  const type prefix##1 = ailo_ggml_tensor_##array##_value((pointer), 1);
#define AILO_GGML_TENSOR_LOCALS_3(type, prefix, pointer, array)                \
  AILO_GGML_TENSOR_LOCALS_2(type, prefix, pointer, array)                      \
  const type prefix##2 = ailo_ggml_tensor_##array##_value((pointer), 2);
#define AILO_GGML_TENSOR_LOCALS(type, prefix, pointer, array)                  \
  AILO_GGML_TENSOR_LOCALS_3(type, prefix, pointer, array)                      \
  const type prefix##3 = ailo_ggml_tensor_##array##_value((pointer), 3);
#define AILO_GGML_TENSOR_UNARY_OP_LOCALS                                       \
  AILO_GGML_TENSOR_LOCALS(ailo_i64, ne0, src0, ne)                             \
  AILO_GGML_TENSOR_LOCALS(size_t, nb0, src0, nb)                               \
  AILO_GGML_TENSOR_LOCALS(ailo_i64, ne, dst, ne)                               \
  AILO_GGML_TENSOR_LOCALS(size_t, nb, dst, nb)
#define AILO_GGML_TENSOR_BINARY_OP_LOCALS01                                    \
  AILO_GGML_TENSOR_LOCALS(ailo_i64, ne0, src0, ne)                             \
  AILO_GGML_TENSOR_LOCALS(size_t, nb0, src0, nb)                               \
  AILO_GGML_TENSOR_LOCALS(ailo_i64, ne1, src1, ne)                             \
  AILO_GGML_TENSOR_LOCALS(size_t, nb1, src1, nb)
#define AILO_GGML_TENSOR_BINARY_OP_LOCALS                                      \
  AILO_GGML_TENSOR_BINARY_OP_LOCALS01                                          \
  AILO_GGML_TENSOR_LOCALS(ailo_i64, ne, dst, ne)                               \
  AILO_GGML_TENSOR_LOCALS(size_t, nb, dst, nb)
#define AILO_GGML_TENSOR_TERNARY_OP_LOCALS                                     \
  AILO_GGML_TENSOR_LOCALS(ailo_i64, ne0, src0, ne)                             \
  AILO_GGML_TENSOR_LOCALS(size_t, nb0, src0, nb)                               \
  AILO_GGML_TENSOR_LOCALS(ailo_i64, ne1, src1, ne)                             \
  AILO_GGML_TENSOR_LOCALS(size_t, nb1, src1, nb)                               \
  AILO_GGML_TENSOR_LOCALS(ailo_i64, ne2, src2, ne)                             \
  AILO_GGML_TENSOR_LOCALS(size_t, nb2, src2, nb)                               \
  AILO_GGML_TENSOR_LOCALS(ailo_i64, ne, dst, ne)                               \
  AILO_GGML_TENSOR_LOCALS(size_t, nb, dst, nb)
#endif
