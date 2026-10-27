#include "ggml-alloc.h"
#include "ggml-backend.h"
#include "ggml.h"
#include "llama.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

// FP16 operands fit exactly; their dot product requires an FP32 accumulator.
// A scalar FP16 accumulator overflows although the FP32 result is finite.
static bool multiply(ggml_backend_t backend, bool explicit_precision,
                     bool large, int n, bool f16) {
  constexpr int k = 256, m = 16;
  ggml_context *ctx = ggml_init({1024 * 1024, nullptr, true});
  if (!ctx)
    return false;
  auto *a = ggml_new_tensor_2d(ctx, f16 ? GGML_TYPE_F16 : GGML_TYPE_F32, k, m);
  auto *b = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, k, n);
  auto *c = ggml_mul_mat(ctx, a, b);
  if (explicit_precision)
    ggml_prec_set_acc(c, GGML_PREC_F32);
  auto *graph = ggml_new_graph(ctx);
  ggml_build_forward_expand(graph, c);
  auto buffer = ggml_backend_alloc_ctx_tensors(ctx, backend);
  if (!buffer) {
    ggml_free(ctx);
    return false;
  }
  ggml_backend_buffer_clear(buffer, 0xff);
  const float av = large ? 8.0f : 1.0f, bv = large ? 128.0f : 1.0f;
  const float expected = k * av * bv;
  std::vector<ggml_fp16_t> left(k * m, ggml_fp32_to_fp16(av));
  std::vector<float> left_f32(k * m, av);
  std::vector<float> right(k * n, bv), result(m * n);
  if (f16)
    ggml_backend_tensor_set(a, left.data(), 0, left.size() * sizeof(left[0]));
  else
    ggml_backend_tensor_set(a, left_f32.data(), 0,
                            left_f32.size() * sizeof(left_f32[0]));
  ggml_backend_tensor_set(b, right.data(), 0, right.size() * sizeof(right[0]));
  bool passed =
      ggml_backend_graph_compute(backend, graph) == GGML_STATUS_SUCCESS;
  if (passed) {
    ggml_backend_tensor_get(c, result.data(), 0,
                            result.size() * sizeof(result[0]));
    size_t bad = 0;
    for (size_t i = 0; i < result.size(); ++i) {
      const float value = result[i];
      if (!std::isfinite(value) || std::fabs(value - expected) >= 1.0f) {
        if (bad < 4)
          std::printf("incorrect index=%zu value=%g\n", i, value);
        ++bad;
      }
    }
    passed = bad == 0;
    std::printf("Vulkan matrix storage=%s n=%d precision=%s expected=%g "
                "first=%g correct=%d\n",
                f16 ? "f16" : "f32", n, explicit_precision ? "f32" : "default",
                expected, result[0], passed);
  }
  ggml_backend_buffer_free(buffer);
  ggml_free(ctx);
  return passed;
}

int main(int argc, char **argv) {
  if (argc != 2 ||
      (std::strcmp(argv[1], "f16") != 0 && std::strcmp(argv[1], "f32") != 0)) {
    std::fprintf(stderr, "Usage: ailo_qualify_matrix f16|f32\n");
    return 2;
  }
  const bool f16 = std::strcmp(argv[1], "f16") == 0;
  llama_backend_init();
  auto registry = ggml_backend_reg_by_name("Vulkan");
  if (!registry || !ggml_backend_reg_dev_count(registry)) {
    std::puts("SKIP: Vulkan matrix qualification requires actual hardware");
    llama_backend_free();
    return 77;
  }
  auto device = ggml_backend_reg_dev_get(registry, 0);
  auto backend = ggml_backend_dev_init(device, nullptr);
  if (!backend) {
    llama_backend_free();
    return 1;
  }
  bool passed = true;
  for (int repeat = 0; repeat < 8; ++repeat) {
    passed &= multiply(backend, true, true, 128, f16);
    passed &= multiply(backend, true, true, 121, f16);
    passed &= multiply(backend, false, false, 121, f16);
  }
  ggml_backend_free(backend);
  llama_backend_free();
  return passed ? 0 : 1;
}
