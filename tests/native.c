#include "ailo/native.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

int main(void) {
  struct ailo_gguf_context *gguf = NULL;
  struct ailo_ggml_threadpool_params *pool_params = NULL;
  struct ailo_ggml_threadpool *pool = NULL;
  struct ailo_ggml_init_params *params = NULL;
  struct ailo_ggml_context *context = NULL;
  struct ailo_ggml_tensor *a = NULL;
  struct ailo_ggml_tensor *b = NULL;
  struct ailo_ggml_tensor *sum = NULL;
  struct ailo_ggml_cgraph *graph = NULL;
  struct ailo_storage_int64_t *shape = NULL;
  ailo_error error;
  ailo_i64 key, signed_value, index;
  ailo_u64 unsigned_value, u_result;
  ailo_i64 s_result;
  ailo_i64 size;
  long key_number;
  int threads;
  float result;
  enum ailo_ggml_status native_status;
  ailo_bool truth;

  CHECK(ailo_gguf_init_empty(&gguf, &error) == AILO_OK && gguf);
  signed_value.hi = 0x80000000U;
  signed_value.lo = 0;
  unsigned_value.hi = 0xffffffffU;
  unsigned_value.lo = 0xffffffffU;
  CHECK(ailo_gguf_set_val_i64(gguf, "signed.minimum", signed_value, &error) ==
        AILO_OK);
  CHECK(ailo_gguf_set_val_u64(gguf, "unsigned.maximum", unsigned_value,
                              &error) == AILO_OK);
  CHECK(ailo_gguf_find_key(gguf, "signed.minimum", &key, &error) == AILO_OK);
  CHECK(ailo_i64_to_long(key, &key_number, &error) == AILO_OK &&
        key_number >= 0);
  CHECK(ailo_gguf_get_val_i64(gguf, key, &s_result, &error) == AILO_OK);
  CHECK(s_result.hi == signed_value.hi && s_result.lo == signed_value.lo);
  CHECK(ailo_gguf_find_key(gguf, "unsigned.maximum", &key, &error) == AILO_OK);
  CHECK(ailo_gguf_get_val_u64(gguf, key, &u_result, &error) == AILO_OK);
  CHECK(u_result.hi == unsigned_value.hi && u_result.lo == unsigned_value.lo);
  CHECK(ailo_gguf_free(gguf, &error) == AILO_OK);

  CHECK(ailo_ggml_threadpool_params_default(2, &pool_params, &error) ==
        AILO_OK);
  CHECK(ailo_ggml_threadpool_new(pool_params, &pool, &error) == AILO_OK &&
        pool);
  CHECK(ailo_ggml_threadpool_get_n_threads(pool, &threads, &error) == AILO_OK &&
        threads == 2);
  CHECK(ailo_ggml_threadpool_free(pool, &error) == AILO_OK);
  CHECK(ailo_ggml_threadpool_params_array_free(pool_params, &error) == AILO_OK);

  CHECK(ailo_ggml_init_params_array_new(1, &params, &error) == AILO_OK);
  CHECK(ailo_ggml_init_params_mem_size_set(params, 1024 * 1024, &error) ==
        AILO_OK);
  CHECK(ailo_ggml_init(params, &context, &error) == AILO_OK && context);
  CHECK(ailo_storage_int64_t_array_new(1, &shape, &error) == AILO_OK);
  size = ailo_i64_from_long(3);
  CHECK(ailo_storage_int64_t_set(shape, 1, 0, size, &error) == AILO_OK);
  CHECK(ailo_ggml_new_tensor(context, AILO_GGML_TYPE_F32, 1, shape, &a,
                             &error) == AILO_OK &&
        a);
  CHECK(ailo_ggml_new_tensor_1d(context, AILO_GGML_TYPE_F32, size, &b,
                                &error) == AILO_OK &&
        b);
  CHECK(ailo_ggml_set_f32(a, 2.0f, &a, &error) == AILO_OK);
  CHECK(ailo_ggml_set_f32(b, 3.0f, &b, &error) == AILO_OK);
  CHECK(ailo_ggml_add(context, a, b, &sum, &error) == AILO_OK && sum);
  CHECK(ailo_ggml_new_graph(context, &graph, &error) == AILO_OK && graph);
  CHECK(ailo_ggml_build_forward_expand(graph, sum, &error) == AILO_OK);
  CHECK(ailo_ggml_graph_compute_with_ctx(context, graph, 2, &native_status,
                                         &error) == AILO_OK);
  CHECK(native_status == AILO_GGML_STATUS_SUCCESS);
  index = ailo_i64_from_long(2);
  CHECK(ailo_ggml_get_f32_1d(sum, 2, &result, &error) == AILO_OK &&
        result == 5.0f);
  CHECK(ailo_ggml_is_contiguous(sum, &truth, &error) == AILO_OK && truth);
  CHECK(ailo_storage_int64_t_get(shape, 1, 0, &s_result, &error) == AILO_OK);
  CHECK(s_result.hi == size.hi && s_result.lo == size.lo);
  CHECK(ailo_storage_int64_t_get(shape, 1, 1, &s_result, &error) ==
        AILO_OUT_OF_RANGE);
  CHECK(ailo_storage_int64_t_array_free(shape, &error) == AILO_OK);
  CHECK(ailo_ggml_free(context, &error) == AILO_OK);
  CHECK(ailo_ggml_init_params_array_free(params, &error) == AILO_OK);
  (void)index;
  puts("C89 native GGUF, threadpool and graph execution: passed");
  return 0;
}
