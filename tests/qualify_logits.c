#define _POSIX_C_SOURCE 200809L
#include "ailo/model.h"
#include "ailo/native.h"
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Manual trained-model qualification. Feed the CPU reference's token to both
 * runs, so every vocabulary comparison has exactly the same input prefix.
 */
#define STEPS 16
#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "%s:%d: %s (%s)\n", __FILE__, __LINE__, #condition,      \
              error.message);                                                  \
      result = 1;                                                              \
      goto cleanup;                                                            \
    }                                                                          \
  } while (0)
struct reference {
  ailo_i32 selected[STEPS];
  ailo_i32 vocabulary;
  float *logits;
};
static int run(const char *path, int gpu, int compare,
               struct reference *reference) {
  struct ailo_llama_model_params *mp = NULL;
  struct ailo_llama_context_params *cp = NULL;
  struct ailo_llama_model *model = NULL;
  struct ailo_llama_context *context = NULL;
  struct ailo_llama_batch *batch = NULL;
  struct ailo_storage_struct_ggml_backend_device *devices = NULL;
  struct ailo_ggml_backend_device *device = NULL;
  struct ailo_ggml_backend_reg *reg = NULL;
  const struct ailo_llama_vocab *vocab = NULL;
  const char *name = NULL;
  const char *prompt = "The sky appears blue because";
  ailo_error error;
  ailo_i32 input[128], count = 0, vocabulary = 0, decoded = 0, i, step;
  float *logits = NULL;
  size_t n = 0, index;
  int result = 0, found = 0, differing_choices = 0, violations = 0;
  double largest = 0, largest_rms = 0, largest_kl = 0;
  ailo_error_clear(&error);
  CHECK(ailo_llama_model_default_params(&mp, &error) == AILO_OK);
  CHECK(ailo_llama_model_params_n_gpu_layers_set(mp, gpu ? -1 : 0, &error) ==
        AILO_OK);
  CHECK(ailo_storage_struct_ggml_backend_device_array_new(2, &devices,
                                                          &error) == AILO_OK);
  if (gpu) {
    CHECK(ailo_ggml_backend_dev_count(&n, &error) == AILO_OK);
    for (index = 0; index < n; ++index) {
      CHECK(ailo_ggml_backend_dev_get(index, &device, &error) == AILO_OK);
      CHECK(ailo_ggml_backend_dev_backend_reg(device, &reg, &error) == AILO_OK);
      CHECK(ailo_ggml_backend_reg_name(reg, &name, &error) == AILO_OK);
      if (strcmp(name, "Vulkan") == 0) {
        found = 1;
        break;
      }
    }
    CHECK(found); /* Missing hardware fails this manual gate. */
    CHECK(ailo_storage_struct_ggml_backend_device_set(devices, 2, 0, device,
                                                      &error) == AILO_OK);
  }
  CHECK(ailo_llama_model_params_devices_set(mp, devices, &error) == AILO_OK);
  CHECK(ailo_llama_model_load_from_file(path, mp, &model, &error) == AILO_OK &&
        model);
  CHECK(ailo_llama_model_get_vocab(model, &vocab, &error) == AILO_OK && vocab);
  CHECK(ailo_llama_vocab_n_tokens(vocab, &vocabulary, &error) == AILO_OK);
  CHECK(vocabulary > 1 && vocabulary <= 262144);
  if (!compare) {
    reference->vocabulary = vocabulary;
    reference->logits =
        (float *)malloc((size_t)vocabulary * STEPS * sizeof(float));
    CHECK(reference->logits);
  }
  CHECK(reference->vocabulary == vocabulary);
  CHECK(ailo_llama_tokenize(vocab, prompt, (ailo_i32)strlen(prompt), input, 128,
                            1, 0, &count, &error) == AILO_OK &&
        count > 0 && count <= 128);
  CHECK(ailo_llama_context_default_params(&cp, &error) == AILO_OK);
  CHECK(ailo_llama_context_params_n_ctx_set(cp, 256, &error) == AILO_OK);
  CHECK(ailo_llama_context_params_n_batch_set(cp, 128, &error) == AILO_OK);
  CHECK(ailo_llama_context_params_n_ubatch_set(cp, 128, &error) == AILO_OK);
  CHECK(ailo_llama_context_params_n_threads_set(cp, 2, &error) == AILO_OK);
  CHECK(ailo_llama_context_params_n_threads_batch_set(cp, 2, &error) ==
        AILO_OK);
  CHECK(ailo_llama_context_params_offload_kqv_set(cp, gpu, &error) == AILO_OK);
  CHECK(ailo_llama_context_params_op_offload_set(cp, gpu, &error) == AILO_OK);
  CHECK(ailo_llama_init_from_model(model, cp, &context, &error) == AILO_OK &&
        context);
  for (step = 0; step < STEPS; ++step) {
    float *expected = reference->logits + (size_t)step * vocabulary;
    ailo_i32 selected = 0;
    double max_cpu, max_gpu, sum_cpu = 0, sum_gpu = 0, z_cpu, z_gpu;
    double squared = 0, kl = 0, maximum = 0;
    CHECK(
        ailo_llama_batch_get_one(step ? &reference->selected[step - 1] : input,
                                 step ? 1 : count, &batch, &error) == AILO_OK);
    CHECK(ailo_llama_decode(context, batch, &decoded, &error) == AILO_OK &&
          decoded == 0);
    CHECK(ailo_llama_batch_array_free(batch, &error) == AILO_OK);
    batch = NULL;
    CHECK(ailo_llama_get_logits_ith(context, -1, &logits, &error) == AILO_OK &&
          logits);
    for (i = 0; i < vocabulary; ++i) {
      CHECK(logits[i] == logits[i] && fabs(logits[i]) < FLT_MAX);
      if (logits[i] > logits[selected])
        selected = i;
    }
    if (!compare) {
      memcpy(expected, logits, (size_t)vocabulary * sizeof(float));
      reference->selected[step] = selected;
      continue;
    }
    if (selected != reference->selected[step])
      ++differing_choices;
    max_cpu = expected[0];
    max_gpu = logits[0];
    for (i = 1; i < vocabulary; ++i) {
      if (expected[i] > max_cpu)
        max_cpu = expected[i];
      if (logits[i] > max_gpu)
        max_gpu = logits[i];
    }
    for (i = 0; i < vocabulary; ++i) {
      sum_cpu += exp(expected[i] - max_cpu);
      sum_gpu += exp(logits[i] - max_gpu);
    }
    z_cpu = max_cpu + log(sum_cpu);
    z_gpu = max_gpu + log(sum_gpu);
    for (i = 0; i < vocabulary; ++i) {
      double cpu = expected[i] - z_cpu, actual = logits[i] - z_gpu;
      double delta = cpu - actual;
      if (fabs(delta) > maximum)
        maximum = fabs(delta);
      squared += delta * delta;
      kl += exp(cpu) * delta;
    }
    squared = sqrt(squared / vocabulary);
    printf("position=%d vocabulary=%d max_logprob_error=%.9g "
           "rms_logprob_error=%.9g kl_cpu_gpu=%.9g\n",
           step, vocabulary, maximum, squared, kl);
    fflush(stdout);
    if (maximum > largest)
      largest = maximum;
    if (squared > largest_rms)
      largest_rms = squared;
    if (kl > largest_kl)
      largest_kl = kl;
    /* Stated before observing results; no equality claim for greedy choices. */
    if (!(maximum <= 0.25 && squared <= 0.025 && kl >= -1e-10 && kl <= 0.001))
      ++violations;
  }
  if (compare)
    printf("Reference/candidate identical-prefix %d-position qualification %s: "
           "max=%.9g RMS=%.9g KL=%.9g differing_argmax=%d violations=%d\n",
           STEPS, violations ? "FAILED" : "passed", largest, largest_rms,
           largest_kl, differing_choices, violations);
  if (violations)
    result = 1;
cleanup:
  ailo_llama_batch_array_free(batch, NULL);
  ailo_llama_free(context, NULL);
  ailo_llama_model_free(model, NULL);
  ailo_llama_context_params_array_free(cp, NULL);
  ailo_llama_model_params_array_free(mp, NULL);
  ailo_storage_struct_ggml_backend_device_array_free(devices, NULL);
  return result;
}
static int dequantize(const char *source, const char *destination) {
  struct ailo_llama_model_quantize_params *params = NULL;
  struct stat existing;
  ailo_error error;
  ailo_u32 native_result = 1;
  int result = 0;
  ailo_error_clear(&error);
  if (lstat(destination, &existing) == 0 || errno != ENOENT) {
    fputs("Reference output must be a new file\n", stderr);
    return 1;
  }
  CHECK(ailo_llama_model_quantize_default_params(&params, &error) == AILO_OK);
  CHECK(ailo_llama_model_quantize_params_nthread_set(params, 2, &error) ==
        AILO_OK);
  CHECK(ailo_llama_model_quantize_params_allow_requantize_set(
            params, 1, &error) == AILO_OK);
  CHECK(ailo_llama_model_quantize_params_ftype_set(
            params, AILO_LLAMA_FTYPE_MOSTLY_F16, &error) == AILO_OK);
  CHECK(ailo_llama_model_quantize_params_pure_set(params, 1, &error) ==
        AILO_OK);
  CHECK(ailo_llama_model_quantize(source, destination, params, &native_result,
                                  &error) == AILO_OK &&
        native_result == 0);
cleanup:
  ailo_llama_model_quantize_params_array_free(params, NULL);
  return result;
}
int main(int argc, char **argv) {
  struct reference reference = {{0}, 0, NULL};
  ailo_model_source source;
  ailo_error error;
  char *path = NULL, *candidate = NULL;
  const char *checksum[1];
  int result, conversion = argc == 5 && !strcmp(argv[1], "dequantize");
  if (!conversion && (argc != 7 || strcmp(argv[1], "compare") ||
                      (strcmp(argv[6], "cpu") && strcmp(argv[6], "vulkan")))) {
    fputs("Usage: ailo_qualify_logits dequantize MODEL SHA256 NEW_OUTPUT\n"
          "   or: ailo_qualify_logits compare REFERENCE SHA256 CANDIDATE "
          "SHA256 cpu|vulkan\n",
          stderr);
    return 2;
  }
  memset(&source, 0, sizeof(source));
  source.local_path = argv[2];
  checksum[0] = argv[3];
  source.sha256 = checksum;
  source.sha256_count = 1;
  if (ailo_model_acquire(&source, &path, &error) != AILO_OK) {
    fprintf(stderr, "%s\n", error.message);
    return 1;
  }
  if (conversion) {
    result = dequantize(path, argv[4]);
    ailo_string_free(path);
    return result;
  }
  source.local_path = argv[4];
  checksum[0] = argv[5];
  if (ailo_model_acquire(&source, &candidate, &error) != AILO_OK) {
    fprintf(stderr, "%s\n", error.message);
    ailo_string_free(path);
    return 1;
  }
  if (ailo_llama_backend_init(&error) != AILO_OK) {
    ailo_string_free(candidate);
    ailo_string_free(path);
    return 1;
  }
  result = run(path, 0, 0, &reference);
  if (!result)
    result = run(candidate, !strcmp(argv[6], "vulkan"), 1, &reference);
  ailo_string_free(candidate);
  free(reference.logits);
  ailo_llama_backend_free(NULL);
  ailo_string_free(path);
  return result;
}
