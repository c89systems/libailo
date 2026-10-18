#include "ailo/native.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "%s:%d: %s (%s)\n", __FILE__, __LINE__, #x,              \
              error.message);                                                  \
      result = 1;                                                              \
      goto cleanup;                                                            \
    }                                                                          \
  } while (0)

static int run(const char *path, int gpu) {
  struct ailo_llama_model_params *mp = NULL;
  struct ailo_llama_context_params *cp = NULL;
  struct ailo_llama_model *model = NULL;
  struct ailo_llama_context *ctx = NULL;
  struct ailo_llama_context *other = NULL;
  struct ailo_llama_sampler *sampler = NULL;
  struct ailo_llama_batch *batch = NULL;
  struct ailo_llama_memory_i *memory = NULL;
  struct ailo_llama_memory_i *other_memory = NULL;
  struct ailo_llama_chat_message *message = NULL;
  const struct ailo_llama_vocab *vocab = NULL;
  struct ailo_ggml_backend_device *device = NULL;
  struct ailo_ggml_backend_reg *reg = NULL;
  struct ailo_storage_struct_ggml_backend_device *devices = NULL;
  ailo_error error;
  ailo_u8 *state = NULL;
  float *logits = NULL;
  float next[260];
  ailo_i32 tokens[128], count, native_status, selected, pos;
  ailo_i32 dims, vocab_size, piece_size, short_count, i;
  const char *name = NULL, *tmpl = NULL;
  char piece[8], prompt[128], roundtrip[128];
  char *invalid = NULL;
  size_t size = 0, used = 0, seq_size = 0, total = 0, j, device_count = 0;
  int result = 0;
  int found_gpu = 0;
  ailo_bool truth;
  ailo_u64 parameters;

  ailo_error_clear(&error);
  CHECK(ailo_llama_model_default_params(&mp, &error) == AILO_OK && mp);
  CHECK(ailo_llama_model_params_n_gpu_layers_set(mp, gpu ? 2 : 0, &error) ==
        AILO_OK);
  CHECK(ailo_storage_struct_ggml_backend_device_array_new(2, &devices,
                                                          &error) == AILO_OK);
  if (gpu) {
    CHECK(ailo_ggml_backend_dev_count(&device_count, &error) == AILO_OK);
    for (j = 0; j < device_count; ++j) {
      CHECK(ailo_ggml_backend_dev_get(j, &device, &error) == AILO_OK);
      CHECK(ailo_ggml_backend_dev_backend_reg(device, &reg, &error) == AILO_OK);
      CHECK(ailo_ggml_backend_reg_name(reg, &name, &error) == AILO_OK);
      if (strcmp(name, "Vulkan") == 0) {
        found_gpu = 1;
        break;
      }
    }
    if (!found_gpu) {
      fputs("SKIP: no Vulkan device for actual inference\n", stderr);
      result = 77;
      goto cleanup;
    }
    CHECK(ailo_storage_struct_ggml_backend_device_set(devices, 2, 0, device,
                                                      &error) == AILO_OK);
  }
  CHECK(ailo_llama_model_params_devices_set(mp, devices, &error) == AILO_OK);
  invalid = (char *)malloc(strlen(path) + 20);
  CHECK(invalid);
  strcpy(invalid, path);
  strcat(invalid, ".truncated");
  /* AILO_OK preserves the native nullable result; the facade must not turn
   * native model-load failures into fabricated model handles. */
  CHECK(ailo_llama_model_load_from_file(invalid, mp, &model, &error) ==
            AILO_OK &&
        model == NULL);
  strcpy(invalid, path);
  strcat(invalid, ".unsupported");
  CHECK(ailo_llama_model_load_from_file(invalid, mp, &model, &error) ==
            AILO_OK &&
        model == NULL);
  strcpy(invalid, path);
  strcat(invalid, ".missing");
  CHECK(ailo_llama_model_load_from_file(invalid, mp, &model, &error) ==
            AILO_OK &&
        model == NULL);
  CHECK(ailo_llama_model_load_from_file(path, mp, &model, &error) == AILO_OK &&
        model);
  CHECK(ailo_llama_model_n_embd(model, &dims, &error) == AILO_OK && dims == 32);
  CHECK(ailo_llama_model_n_layer(model, &dims, &error) == AILO_OK && dims == 1);
  CHECK(ailo_llama_model_n_ctx_train(model, &dims, &error) == AILO_OK &&
        dims == 128);
  CHECK(ailo_llama_model_n_params(model, &parameters, &error) == AILO_OK &&
        parameters.hi == 0 && parameters.lo == 26976);
  CHECK(ailo_llama_model_get_vocab(model, &vocab, &error) == AILO_OK && vocab);
  CHECK(ailo_llama_vocab_n_tokens(vocab, &vocab_size, &error) == AILO_OK &&
        vocab_size == 260);
  CHECK(ailo_llama_model_chat_template(model, NULL, &tmpl, &error) == AILO_OK &&
        strcmp(tmpl, "chatml") == 0);
  CHECK(ailo_llama_chat_message_array_new(1, &message, &error) == AILO_OK);
  CHECK(ailo_llama_chat_message_role_set(message, "user", &error) == AILO_OK);
  CHECK(ailo_llama_chat_message_content_set(message, "hello", &error) ==
        AILO_OK);
  CHECK(ailo_llama_chat_apply_template(tmpl, message, 1, 1, prompt,
                                       sizeof(prompt), &count,
                                       &error) == AILO_OK);
  CHECK(count > 0 && count < (ailo_i32)sizeof(prompt));
  CHECK(strstr(prompt, "hello") && strstr(prompt, "assistant"));
  CHECK(ailo_llama_tokenize(vocab, "hello", 5, tokens, 128, 1, 0, &count,
                            &error) == AILO_OK &&
        count > 0);
  CHECK(ailo_llama_tokenize(vocab, "hello", 5, NULL, 0, 1, 0, &short_count,
                            &error) == AILO_OK &&
        short_count == -count);
  for (i = 1; i < count; ++i) {
    CHECK(ailo_llama_token_to_piece(vocab, tokens[i], piece, sizeof(piece), 0,
                                    0, &piece_size, &error) == AILO_OK);
    CHECK(piece_size >= 0 && total + (size_t)piece_size < sizeof(roundtrip));
    memcpy(roundtrip + total, piece, (size_t)piece_size);
    total += (size_t)piece_size;
  }
  roundtrip[total] = '\0';
  CHECK(strcmp(roundtrip, " hello") == 0);

  CHECK(ailo_llama_context_default_params(&cp, &error) == AILO_OK && cp);
  CHECK(ailo_llama_context_params_n_ctx_set(cp, 128, &error) == AILO_OK);
  CHECK(ailo_llama_context_params_n_batch_set(cp, 128, &error) == AILO_OK);
  CHECK(ailo_llama_context_params_n_ubatch_set(cp, 32, &error) == AILO_OK);
  CHECK(ailo_llama_context_params_n_threads_set(cp, 2, &error) == AILO_OK);
  CHECK(ailo_llama_context_params_n_threads_batch_set(cp, 2, &error) ==
        AILO_OK);
  CHECK(ailo_llama_context_params_offload_kqv_set(cp, gpu, &error) == AILO_OK);
  CHECK(ailo_llama_context_params_op_offload_set(cp, gpu, &error) == AILO_OK);
  CHECK(ailo_llama_init_from_model(model, cp, &ctx, &error) == AILO_OK && ctx);
  CHECK(ailo_llama_init_from_model(model, cp, &other, &error) == AILO_OK &&
        other);
  CHECK(ailo_llama_batch_get_one(tokens, count, &batch, &error) == AILO_OK &&
        batch);
  CHECK(ailo_llama_decode(ctx, batch, &native_status, &error) == AILO_OK &&
        native_status == 0);
  CHECK(ailo_llama_batch_array_free(batch, &error) == AILO_OK);
  batch = NULL;
  CHECK(ailo_llama_get_memory(ctx, &memory, &error) == AILO_OK && memory);
  CHECK(ailo_llama_memory_seq_pos_max(memory, 0, &pos, &error) == AILO_OK &&
        pos == count - 1);
  CHECK(ailo_llama_get_logits_ith(ctx, -1, &logits, &error) == AILO_OK &&
        logits);
  for (i = 0; i < vocab_size; ++i) {
    CHECK(logits[i] == logits[i] && fabs(logits[i]) < FLT_MAX);
  }
  CHECK(ailo_llama_sampler_init_greedy(&sampler, &error) == AILO_OK && sampler);
  CHECK(ailo_llama_sampler_sample(sampler, ctx, -1, &selected, &error) ==
            AILO_OK &&
        selected == 100);
  CHECK(ailo_llama_token_to_piece(vocab, selected, piece, sizeof(piece), 0, 0,
                                  &piece_size, &error) == AILO_OK &&
        piece_size == 1 && piece[0] == 'a');
  CHECK(ailo_llama_state_get_size(ctx, &size, &error) == AILO_OK && size > 0);
  state = (ailo_u8 *)malloc(size);
  CHECK(state);
  CHECK(ailo_llama_state_get_data(ctx, state, size, &used, &error) == AILO_OK &&
        used == size);
  CHECK(ailo_llama_state_set_data(other, state, size, &used, &error) ==
            AILO_OK &&
        used == size);
  /* Restored KV state needs a decoded continuation before querying logits. */
  CHECK(ailo_llama_get_memory(other, &other_memory, &error) == AILO_OK);
  CHECK(ailo_llama_memory_seq_pos_max(other_memory, 0, &pos, &error) ==
            AILO_OK &&
        pos == count - 1);
  CHECK(ailo_llama_batch_get_one(&selected, 1, &batch, &error) == AILO_OK);
  CHECK(ailo_llama_decode(ctx, batch, &native_status, &error) == AILO_OK &&
        native_status == 0);
  CHECK(ailo_llama_get_logits_ith(ctx, -1, &logits, &error) == AILO_OK &&
        logits);
  memcpy(next, logits, sizeof(next));
  CHECK(ailo_llama_decode(other, batch, &native_status, &error) == AILO_OK &&
        native_status == 0);
  CHECK(ailo_llama_get_logits_ith(other, -1, &logits, &error) == AILO_OK &&
        logits);
  CHECK(memcmp(next, logits, sizeof(next)) == 0);
  CHECK(ailo_llama_batch_array_free(batch, &error) == AILO_OK);
  batch = NULL;
  free(state);
  state = NULL;

  CHECK(ailo_llama_state_seq_get_size(ctx, 0, &seq_size, &error) == AILO_OK &&
        seq_size > 0);
  state = (ailo_u8 *)malloc(seq_size);
  CHECK(state);
  CHECK(ailo_llama_state_seq_get_data(ctx, state, seq_size, 0, &used, &error) ==
            AILO_OK &&
        used == seq_size);
  CHECK(ailo_llama_memory_seq_rm(memory, 0, -1, -1, &truth, &error) ==
            AILO_OK &&
        truth);
  CHECK(ailo_llama_memory_seq_pos_max(memory, 0, &pos, &error) == AILO_OK &&
        pos == -1);
  CHECK(ailo_llama_state_seq_set_data(ctx, state, seq_size, 0, &used, &error) ==
            AILO_OK &&
        used == seq_size);
  CHECK(ailo_llama_memory_seq_pos_max(memory, 0, &pos, &error) == AILO_OK &&
        pos == count);
  CHECK(ailo_llama_sampler_sample(sampler, other, -1, &selected, &error) ==
            AILO_OK &&
        selected == 100);
  /* Match a continuation against the context whose sequence was never removed,
   * proving restored key/value contents as well as the position metadata. */
  CHECK(ailo_llama_batch_get_one(&selected, 1, &batch, &error) == AILO_OK);
  CHECK(ailo_llama_decode(other, batch, &native_status, &error) == AILO_OK &&
        native_status == 0);
  CHECK(ailo_llama_get_logits_ith(other, -1, &logits, &error) == AILO_OK &&
        logits);
  memcpy(next, logits, sizeof(next));
  CHECK(ailo_llama_decode(ctx, batch, &native_status, &error) == AILO_OK &&
        native_status == 0);
  CHECK(ailo_llama_get_logits_ith(ctx, -1, &logits, &error) == AILO_OK &&
        logits);
  CHECK(memcmp(next, logits, sizeof(next)) == 0);
  printf("C89 %s GGUF inference, tokenization, sampling and state restoration: "
         "passed\n",
         gpu ? "Vulkan" : "CPU");
cleanup:
  free(invalid);
  free(state);
  ailo_llama_batch_array_free(batch, NULL);
  ailo_llama_sampler_free(sampler, NULL);
  ailo_llama_free(other, NULL);
  ailo_llama_free(ctx, NULL);
  ailo_llama_model_free(model, NULL);
  ailo_llama_context_params_array_free(cp, NULL);
  ailo_llama_model_params_array_free(mp, NULL);
  ailo_llama_chat_message_array_free(message, NULL);
  ailo_storage_struct_ggml_backend_device_array_free(devices, NULL);
  return result;
}

/* The offload gate opts into diagnostics through the public logging hook. */
static void log_offload(enum ailo_ggml_log_level level, const char *text,
                        void *native_user, void *binding_user) {
  (void)level;
  (void)native_user;
  (void)binding_user;
  fputs(text, stderr);
}

int main(int argc, char **argv) {
  ailo_error error;
  ailo_ggml_log_callback logger = NULL;
  int result;
  if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "--vulkan") != 0)) {
    fputs("Usage: ailo_model_test model.gguf [--vulkan]\n", stderr);
    return 2;
  }
  if (argc == 3) {
    if (ailo_ggml_log_callback_new(log_offload, NULL, &logger, &error) !=
        AILO_OK)
      return 1;
    if (ailo_llama_log_set(logger, NULL, &error) != AILO_OK) {
      ailo_ggml_log_callback_free(logger, NULL);
      return 1;
    }
  }
  if (ailo_llama_backend_init(&error) != AILO_OK)
    result = 1;
  else {
    result = run(argv[1], argc == 3);
    ailo_llama_backend_free(NULL);
  }
  if (logger) {
    ailo_llama_log_set(NULL, NULL, NULL);
    ailo_ggml_log_callback_free(logger, NULL);
  }
  return result;
}
