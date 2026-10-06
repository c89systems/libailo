#include "ailo/native.h"
#include <stdio.h>

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static int unary(const struct ailo_ggml_tensor *src0,
                 const struct ailo_ggml_tensor *dst) {
  AILO_GGML_TENSOR_UNARY_OP_LOCALS
  AILO_GGML_UNUSED_VARS((0, ne00, ne01, ne02, ne03, nb00, nb01, nb02, nb03, ne0,
                         ne1, ne2, ne3, nb0, nb1, nb2, nb3));
  return ne00.lo == 3 && ne0.lo == 7 && nb00 == 4 && nb0 == 8;
}
static int binary(const struct ailo_ggml_tensor *src0,
                  const struct ailo_ggml_tensor *src1,
                  const struct ailo_ggml_tensor *dst) {
  AILO_GGML_TENSOR_BINARY_OP_LOCALS
  AILO_GGML_UNUSED_VARS((0, ne00, ne01, ne02, ne03, nb00, nb01, nb02, nb03,
                         ne10, ne11, ne12, ne13, nb10, nb11, nb12, nb13, ne0,
                         ne1, ne2, ne3, nb0, nb1, nb2, nb3));
  return ne10.lo == 5 && nb10 == 12 && ne0.lo == 7;
}
static int ternary(const struct ailo_ggml_tensor *src0,
                   const struct ailo_ggml_tensor *src1,
                   const struct ailo_ggml_tensor *src2,
                   const struct ailo_ggml_tensor *dst) {
  AILO_GGML_TENSOR_TERNARY_OP_LOCALS
  AILO_GGML_UNUSED_VARS((0, ne00, ne01, ne02, ne03, nb00, nb01, nb02, nb03,
                         ne10, ne11, ne12, ne13, nb10, nb11, nb12, nb13, ne20,
                         ne21, ne22, ne23, nb20, nb21, nb22, nb23, ne0, ne1,
                         ne2, ne3, nb0, nb1, nb2, nb3));
  return ne20.lo == 9 && nb20 == 16 && ne0.lo == 7;
}

int main(void) {
  struct ailo_ggml_tensor *tensors = NULL;
  struct ailo_ggml_tensor *a = NULL, *b = NULL, *c = NULL, *d = NULL;
  ailo_error error;
  ailo_i64 zero;
  int unevaluated = 0;
  AILO_GGML_UNUSED_VARS((0, ++unevaluated, ++unevaluated));
  CHECK(unevaluated == 0);
  CHECK(AILO_GGML_PAD(5, 4) == 8 && AILO_GGML_N_TASKS_MAX == -1);
  CHECK(AILO_LLAMA_SESSION_MAGIC == AILO_LLAMA_FILE_MAGIC_GGSN);
  CHECK(AILO_LLAMA_STATE_SEQ_MAGIC == AILO_LLAMA_FILE_MAGIC_GGSQ);
  CHECK(AILO_GGML_TENSOR_SIZE > 0);
  zero = ailo_ggml_tensor_ne_value(NULL, 0);
  CHECK(zero.hi == 0 && zero.lo == 0);
  CHECK(ailo_ggml_tensor_nb_value(NULL, 0) == 0);
  CHECK(ailo_ggml_tensor_array_new(4, &tensors, &error) == AILO_OK);
  CHECK(ailo_ggml_tensor_array_at(tensors, 4, 0, &a, &error) == AILO_OK);
  CHECK(ailo_ggml_tensor_array_at(tensors, 4, 1, &b, &error) == AILO_OK);
  CHECK(ailo_ggml_tensor_array_at(tensors, 4, 2, &c, &error) == AILO_OK);
  CHECK(ailo_ggml_tensor_array_at(tensors, 4, 3, &d, &error) == AILO_OK);
  CHECK(ailo_ggml_tensor_ne_set(a, 0, ailo_i64_from_long(3), &error) ==
        AILO_OK);
  CHECK(ailo_ggml_tensor_ne_set(b, 0, ailo_i64_from_long(5), &error) ==
        AILO_OK);
  CHECK(ailo_ggml_tensor_ne_set(c, 0, ailo_i64_from_long(9), &error) ==
        AILO_OK);
  CHECK(ailo_ggml_tensor_ne_set(d, 0, ailo_i64_from_long(7), &error) ==
        AILO_OK);
  CHECK(ailo_ggml_tensor_nb_set(a, 0, 4, &error) == AILO_OK);
  CHECK(ailo_ggml_tensor_nb_set(b, 0, 12, &error) == AILO_OK);
  CHECK(ailo_ggml_tensor_nb_set(c, 0, 16, &error) == AILO_OK);
  CHECK(ailo_ggml_tensor_nb_set(d, 0, 8, &error) == AILO_OK);
  CHECK(unary(a, d) && binary(a, b, d) && ternary(a, b, c, d));
  zero = ailo_ggml_tensor_ne_value(a, AILO_GGML_MAX_DIMS);
  CHECK(zero.hi == 0 && zero.lo == 0);
  AILO_GGML_ASSERT(unevaluated == 0);
  CHECK(ailo_ggml_tensor_array_free(tensors, &error) == AILO_OK);
  puts("C89 macro composition, aliases, values and evaluation semantics: "
       "passed");
  return 0;
}
