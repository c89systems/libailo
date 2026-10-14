#include "ailo/native.h"
#include <stdio.h>
#include <string.h>

typedef struct progress_state {
  unsigned int calls;
  void *expected_user;
  int failed;
} progress_state;

static ailo_bool progress(float fraction, void *native_user, void *user) {
  progress_state *state = (progress_state *)user;
  if (fraction < 0 || fraction > 1 || native_user != state->expected_user)
    state->failed = 1;
  ++state->calls;
  return !state->failed;
}

static size_t read_bytes(void *native_user, void *buffer, ailo_u64 offset,
                         size_t size, void *user) {
  progress_state *state = (progress_state *)user;
  if (native_user != state->expected_user || offset.hi != 0xfedcba98U ||
      offset.lo != 0x76543210U || size != 3)
    state->failed = 1;
  memcpy(buffer, "abc", 3);
  return size;
}

int main(int argc, char **argv) {
  ailo_error error;
  ailo_status status;
  ailo_llama_progress_callback callback = NULL;
  ailo_gguf_reader_callback_t reader = NULL;
  struct ailo_llama_model_params *parameters = NULL;
  struct ailo_llama_model *model = NULL;
  progress_state state;
  ailo_u64 offset;
  char buffer[3];
  size_t received = 0;
  int result = 1, initialized = 0;
  if (argc != 2)
    return 2;
  memset(&state, 0, sizeof(state));
  if (ailo_llama_backend_init(&error) != AILO_OK)
    goto cleanup;
  initialized = 1;
  state.expected_user = &state;
  status =
      ailo_llama_progress_callback_new(progress, &state, &callback, &error);
  if (status != AILO_OK)
    goto cleanup;
  if (ailo_llama_model_default_params(&parameters, &error) != AILO_OK ||
      ailo_llama_model_params_n_gpu_layers_set(parameters, 0, &error) !=
          AILO_OK ||
      ailo_llama_model_params_progress_callback_set(parameters, callback,
                                                    &error) != AILO_OK ||
      ailo_llama_model_params_progress_callback_user_data_set(
          parameters, &state, &error) != AILO_OK ||
      ailo_llama_model_load_from_file(argv[1], parameters, &model, &error) !=
          AILO_OK ||
      !model || !state.calls || state.failed ||
      ailo_llama_progress_callback_error(callback, &error) != AILO_OK)
    goto cleanup;
  offset.hi = 0xfedcba98U;
  offset.lo = 0x76543210U;
  if (ailo_gguf_reader_callback_t_new(read_bytes, &state, &reader, &error) !=
          AILO_OK ||
      ailo_gguf_reader_callback_t_call(reader, &state, buffer, offset, 3,
                                       &received, &error) != AILO_OK ||
      received != 3 || memcmp(buffer, "abc", 3) || state.failed ||
      ailo_gguf_reader_callback_t_error(reader, &error) != AILO_OK)
    goto cleanup;
  result = 0;
  puts("C89 callbacks through actual native model loading and full-width "
       "reader offsets: passed");
cleanup:
  if (result)
    fprintf(stderr, "C89 callback failure: %s\n", error.message);
  ailo_llama_model_free(model, NULL);
  ailo_llama_model_params_array_free(parameters, NULL);
  ailo_llama_progress_callback_free(callback, NULL);
  ailo_gguf_reader_callback_t_free(reader, NULL);
  if (initialized)
    ailo_llama_backend_free(NULL);
  return result;
}
