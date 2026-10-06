#include "ailo/native.h"
#include "ggml.h"
#include <cstdint>
#include <cstdio>

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);             \
      return 1;                                                                \
    }                                                                          \
  } while (0)

int main() {
  ailo_error error = {};
  ailo_storage_int64_t *signed_storage = nullptr;
  ailo_storage_uint64_t *unsigned_storage = nullptr;
  ailo_storage_bool *booleans = nullptr;
  ailo_storage_struct_ggml_tensor *pointers = nullptr;
  ailo_ggml_tensor *tensor = nullptr, *borrowed = nullptr;
  const ailo_i64 minimum = {0x80000000U, 0};
  const ailo_u64 maximum = {0xffffffffU, 0xffffffffU};
  ailo_i64 s = {};
  ailo_u64 u = {};
  ailo_bool truth = 0;
  CHECK(ailo_storage_int64_t_array_new(2, &signed_storage, &error) == AILO_OK);
  CHECK(ailo_storage_uint64_t_array_new(2, &unsigned_storage, &error) ==
        AILO_OK);
  CHECK(ailo_storage_bool_array_new(2, &booleans, &error) == AILO_OK);
  CHECK(ailo_storage_int64_t_set(signed_storage, 2, 1, minimum, &error) ==
        AILO_OK);
  CHECK(ailo_storage_uint64_t_set(unsigned_storage, 2, 1, maximum, &error) ==
        AILO_OK);
  CHECK(ailo_storage_bool_set(booleans, 2, 1, 123, &error) == AILO_OK);
  CHECK(ailo_storage_int64_t_get(signed_storage, 2, 1, &s, &error) == AILO_OK);
  CHECK(ailo_storage_uint64_t_get(unsigned_storage, 2, 1, &u, &error) ==
        AILO_OK);
  CHECK(ailo_storage_bool_get(booleans, 2, 1, &truth, &error) == AILO_OK);
  CHECK(s.hi == minimum.hi && s.lo == minimum.lo);
  CHECK(u.hi == maximum.hi && u.lo == maximum.lo && truth == 1);
  CHECK(reinterpret_cast<const std::int64_t *>(signed_storage)[1] == INT64_MIN);
  CHECK(reinterpret_cast<const std::uint64_t *>(unsigned_storage)[1] ==
        UINT64_MAX);
  CHECK(reinterpret_cast<const bool *>(booleans)[1]);
  CHECK(ailo_storage_bool_get(booleans, 2, 2, &truth, &error) ==
            AILO_OUT_OF_RANGE &&
        truth == 0);
  CHECK(ailo_ggml_tensor_array_new(1, &tensor, &error) == AILO_OK);
  CHECK(ailo_storage_struct_ggml_tensor_array_new(2, &pointers, &error) ==
        AILO_OK);
  CHECK(ailo_storage_struct_ggml_tensor_set(pointers, 2, 1, tensor, &error) ==
        AILO_OK);
  CHECK(ailo_storage_struct_ggml_tensor_get(pointers, 2, 1, &borrowed,
                                            &error) == AILO_OK);
  CHECK(borrowed == tensor);
  CHECK(reinterpret_cast<ggml_tensor *const *>(pointers)[1] ==
        reinterpret_cast<ggml_tensor *>(tensor));
  CHECK(ailo_storage_struct_ggml_tensor_array_free(pointers, &error) ==
        AILO_OK);
  CHECK(ailo_ggml_tensor_array_free(tensor, &error) == AILO_OK);
  CHECK(ailo_storage_bool_array_free(booleans, &error) == AILO_OK);
  CHECK(ailo_storage_uint64_t_array_free(unsigned_storage, &error) == AILO_OK);
  CHECK(ailo_storage_int64_t_array_free(signed_storage, &error) == AILO_OK);
  std::puts("Typed native integer/boolean/object-pointer storage: passed");
  return 0;
}
