#include "ggml-backend.h"
#include "llama.h"
#include "mtmd.h"
#include <cstdio>
#include <cstring>

int main() {
  llama_backend_init();
  bool cpu = false;
  bool vulkan = false;
  for (size_t i = 0; i < ggml_backend_reg_count(); ++i) {
    const char *name = ggml_backend_reg_name(ggml_backend_reg_get(i));
    std::printf("backend=%s\n", name);
    cpu |= std::strcmp(name, "CPU") == 0;
    vulkan |= std::strcmp(name, "Vulkan") == 0;
  }
  for (size_t i = 0; i < ggml_backend_dev_count(); ++i) {
    std::printf("device=%s description=%s\n",
                ggml_backend_dev_name(ggml_backend_dev_get(i)),
                ggml_backend_dev_description(ggml_backend_dev_get(i)));
  }
  std::printf("cpu=%d vulkan=%d\n", cpu, vulkan);
#ifdef AILO_TEST_VULKAN
  if (!vulkan)
    return 3;
#endif
  const struct llama_model_params params = llama_model_default_params();
  if (params.vocab_only)
    return 2;
  llama_backend_free();
  return cpu ? 0 : 1;
}
