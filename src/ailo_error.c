#include "ailo_error_internal.h"
#include <stdlib.h>

const char *ailo_status_name(int status) {
  switch (status) {
  case AILO_OK:
    return "ok";
  case AILO_INVALID_ARGUMENT:
    return "invalid_argument";
  case AILO_OUT_OF_RANGE:
    return "out_of_range";
  case AILO_NO_MEMORY:
    return "no_memory";
  case AILO_UNSUPPORTED:
    return "unsupported";
  case AILO_INVALID_JSON:
    return "invalid_json";
  case AILO_UNKNOWN_MODEL:
    return "unknown_model";
  case AILO_DOWNLOAD_FAILED:
    return "download_failed";
  case AILO_IO_FAILED:
    return "io_failed";
  case AILO_MODEL_FAILED:
    return "model_failed";
  case AILO_CONTEXT_LIMIT:
    return "context_limit";
  case AILO_BUSY:
    return "busy";
  case AILO_CANCELLED:
    return "cancelled";
  case AILO_SINK_FAILED:
    return "sink_failed";
  case AILO_INTERNAL_ERROR:
    return "internal_error";
  }
  return "unknown_status";
}

void ailo_error_clear(ailo_error *error) {
  if (error != NULL) {
    error->status = AILO_OK;
    error->message[0] = '\0';
  }
}

ailo_status ailo_error_set(ailo_error *error, ailo_status status,
                           const char *message) {
  size_t i;
  if (error != NULL) {
    error->status = status;
    i = 0;
    while (message[i] != '\0' && i + 1 < sizeof(error->message)) {
      error->message[i] = message[i];
      ++i;
    }
    error->message[i] = '\0';
  }
  return status;
}

void ailo_string_free(char *string) { free(string); }
