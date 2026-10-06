#include "ailo/native_macros.h"
#include "ailo_integer_bridge.hpp"
#include "ggml.h"

extern "C" __attribute__((visibility("default"))) int
ailo_ggml_unused_values(int marker, ...) {
  (void)marker;
  return 0;
}

extern "C" __attribute__((visibility("default"))) size_t
ailo_ggml_tensor_native_size(void) {
  return GGML_TENSOR_SIZE;
}
extern "C" __attribute__((visibility("default"))) ailo_i64
ailo_ggml_tensor_ne_value(const struct ailo_ggml_tensor *tensor, size_t index) {
  if (tensor == nullptr || index >= GGML_MAX_DIMS) {
    return ailo_i64{};
  }
  return ailo::detail::from_signed(
      reinterpret_cast<const ggml_tensor *>(tensor)->ne[index]);
}
extern "C" __attribute__((visibility("default"))) size_t
ailo_ggml_tensor_nb_value(const struct ailo_ggml_tensor *tensor, size_t index) {
  if (tensor == nullptr || index >= GGML_MAX_DIMS)
    return 0;
  return reinterpret_cast<const ggml_tensor *>(tensor)->nb[index];
}
