#include "ailo/native.h"
#include "ggml.h"
#include <cstdio>
#include <initializer_list>

int main() {
  if (AILO_GGML_TENSOR_SIZE != GGML_TENSOR_SIZE)
    return 1;
  for (size_t alignment : {size_t(2), size_t(4), size_t(16)}) {
    for (size_t value = 0; value < 33; ++value) {
      if (AILO_GGML_PAD(value, alignment) != GGML_PAD(value, alignment))
        return 2;
    }
  }
  std::puts("Native size and padding constant parity: passed");
  return 0;
}
