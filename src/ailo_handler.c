#define _POSIX_C_SOURCE 200809L
#include "ailo/handler.h"
#include "ailo_error_internal.h"
#include <errno.h>
#include <lonejson.h>
#include <openssl/rand.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define BODY_LIMIT ((size_t)8 * 1024 * 1024)
#define TARGET_LIMIT ((size_t)16384)

typedef struct handler_state {
  ailo_handler_config config;
  pthread_mutex_t mutex;
} handler_state;
typedef struct exchange {
  const ailo_http_transport *transport;
  ailo_handler_info *info;
  ailo_status transport_failure;
} exchange;

static ailo_status rejected(int accepted) {
  return accepted == 0 ? AILO_CANCELLED : AILO_SINK_FAILED;
}
static int start(exchange *call, int status, const char *type,
                 const char *allow) {
  ailo_http_response response;
  int accepted;
  if (call->transport_failure != AILO_OK)
    return -1;
  if (call->info->started)
    return 1;
  memset(&response, 0, sizeof(response));
  response.status = status;
  response.content_type = type;
  response.allow = allow;
  response.cache_control = "no-store";
  response.request_id = call->info->request_id;
  response.retry_after = status == 503 ? "1" : NULL;
  call->info->http_status = status;
  call->info->started = 1;
  accepted = call->transport->begin(call->transport->user_data, &response);
  if (accepted <= 0)
    call->transport_failure = rejected(accepted);
  return accepted;
}
static int forward(void *user, const void *data, size_t size) {
  exchange *call = (exchange *)user;
  ailo_u64 added;
  ailo_u32 prior;
  int accepted;
  if (call->transport_failure != AILO_OK)
    return -1;
  if (!size)
    return 1;
  if (start(call,
            call->info->api.http_status ? call->info->api.http_status : 200,
            call->info->api.content_type ? call->info->api.content_type
                                         : "application/json",
            call->info->api.allow) <= 0)
    return -1;
  accepted = call->transport->write(call->transport->user_data, data, size);
  if (accepted <= 0) {
    call->transport_failure = rejected(accepted);
    return accepted;
  }
  added = ailo_u64_from_ulong((unsigned long)size);
  prior = call->info->bytes_written.lo;
  call->info->bytes_written.lo += added.lo;
  call->info->bytes_written.hi +=
      added.hi + (call->info->bytes_written.lo < prior);
  return accepted;
}
static lonejson_status error_bytes(void *user, const void *data, size_t size,
                                   lonejson_error *error) {
  (void)error;
  return forward(user, data, size) > 0 ? LONEJSON_STATUS_OK
                                       : LONEJSON_STATUS_IO_ERROR;
}
static lonejson_status field(lonejson_writer *writer, const char *name,
                             const char *value) {
  lonejson_status result;
  result = writer->key(writer, name, strlen(name), NULL);
  if (result == LONEJSON_STATUS_OK)
    result = value ? writer->string(writer, value, strlen(value), NULL)
                   : writer->null_fn(writer, NULL);
  return result;
}
static ailo_status error_body(exchange *call, ailo_status status,
                              const ailo_error *error) {
  lonejson *runtime;
  lonejson_writer writer;
  lonejson_status result;
  const char *message;
  const char *type;
  runtime = lonejson_new(NULL, NULL);
  if (!runtime)
    return AILO_NO_MEMORY;
  memset(&writer, 0, sizeof(writer));
  message =
      call->info->api.http_status >= 500
          ? (status == AILO_BUSY ? "The inference receiver is busy."
                                 : "The server could not complete the request.")
          : error->message;
  type = call->info->api.http_status >= 500 ? "server_error"
                                            : "invalid_request_error";
  call->info->api.content_type = "application/json";
  result = lonejson_writer_init_sink(runtime, &writer, error_bytes, call, NULL);
  if (result == LONEJSON_STATUS_OK)
    result = writer.begin_object(&writer, NULL);
  if (result == LONEJSON_STATUS_OK)
    result = writer.key(&writer, "error", 5, NULL);
  if (result == LONEJSON_STATUS_OK)
    result = writer.begin_object(&writer, NULL);
  if (result == LONEJSON_STATUS_OK)
    result = field(&writer, "message", message);
  if (result == LONEJSON_STATUS_OK)
    result = field(&writer, "type", type);
  if (result == LONEJSON_STATUS_OK)
    result = field(&writer, "param", NULL);
  if (result == LONEJSON_STATUS_OK)
    result = field(&writer, "code", ailo_status_name(status));
  if (result == LONEJSON_STATUS_OK)
    result = writer.end_object(&writer, NULL);
  if (result == LONEJSON_STATUS_OK)
    result = writer.end_object(&writer, NULL);
  if (result == LONEJSON_STATUS_OK)
    result = writer.finish(&writer, NULL);
  lonejson_writer_cleanup(&writer);
  lonejson_free(runtime);
  return result == LONEJSON_STATUS_OK         ? AILO_OK
         : call->transport_failure != AILO_OK ? call->transport_failure
                                              : AILO_NO_MEMORY;
}
/* Small typed header values only; never pass arbitrary input to response
 * headers. */
static int header(const char *input, char *output, size_t capacity) {
  size_t i, used;
  int gap;
  unsigned char c;
  used = 0;
  gap = 0;
  if (!input) {
    output[0] = 0;
    return 1;
  }
  for (i = 0; i < capacity; ++i) {
    c = (unsigned char)input[i];
    if (!c) {
      output[used] = 0;
      return 1;
    }
    if (c == ' ' || c == '\t') {
      gap = 1;
      continue;
    }
    if (c < 33 || c > 126 || used + 1 == capacity)
      return 0;
    if (gap && used && c != ';' && c != '=' && output[used - 1] != ';' &&
        output[used - 1] != '=')
      return 0;
    gap = 0;
    output[used++] = (char)(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
  }
  return 0;
}
static ailo_status validate(handler_state *state,
                            const ailo_http_request *request,
                            ailo_api_info *info, ailo_error *error) {
  char value[129];
  int valid;
  if (strnlen(request->target, state->config.max_target_bytes + 1) >
      state->config.max_target_bytes) {
    info->http_status = 414;
    return ailo_error_set(error, AILO_OUT_OF_RANGE,
                          "API request target is too long");
  }
  if (request->body_size > state->config.max_body_bytes) {
    info->http_status = 413;
    return ailo_error_set(error, AILO_OUT_OF_RANGE,
                          "API request body is too large");
  }
  if (!header(request->content_encoding, value, sizeof(value)) ||
      (value[0] && strcmp(value, "identity"))) {
    info->http_status = 415;
    return ailo_error_set(error, AILO_UNSUPPORTED,
                          "Unsupported request content encoding");
  }
  valid = header(request->content_type, value, sizeof(value));
  if (request->body_size &&
      (!valid || (strcmp(value, "application/json") &&
                  strcmp(value, "application/json;charset=utf-8") &&
                  strcmp(value, "application/json;charset=\"utf-8\"")))) {
    info->http_status = 415;
    return ailo_error_set(
        error, AILO_UNSUPPORTED,
        "Request body requires application/json with UTF-8 encoding");
  }
  return AILO_OK;
}

ailo_handler_config ailo_handler_default_config(void) {
  ailo_handler_config config;
  memset(&config, 0, sizeof(config));
  config.max_body_bytes = BODY_LIMIT;
  config.max_target_bytes = TARGET_LIMIT;
  return config;
}
ailo_status ailo_handler_new(const ailo_handler_config *config,
                             ailo_handler **output, ailo_error *error) {
  ailo_handler *receiver;
  handler_state *state;
  if (output)
    *output = NULL;
  if (!config || !config->client || !output ||
      config->max_body_bytes > BODY_LIMIT ||
      config->max_target_bytes > TARGET_LIMIT)
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "Handler requires a client and valid request limits");
  receiver = (ailo_handler *)calloc(1, sizeof(*receiver));
  state = (handler_state *)calloc(1, sizeof(*state));
  if (!receiver || !state) {
    free(receiver);
    free(state);
    return ailo_error_set(error, AILO_NO_MEMORY, "Handler allocation failed");
  }
  state->config = *config;
  if (!state->config.max_body_bytes)
    state->config.max_body_bytes = BODY_LIMIT;
  if (!state->config.max_target_bytes)
    state->config.max_target_bytes = TARGET_LIMIT;
  if (pthread_mutex_init(&state->mutex, NULL)) {
    free(receiver);
    free(state);
    return ailo_error_set(error, AILO_INTERNAL_ERROR,
                          "Handler mutex initialization failed");
  }
  receiver->state = state;
  receiver->handle = ailo_handler_handle;
  receiver->destroy = ailo_handler_destroy;
  *output = receiver;
  ailo_error_clear(error);
  return AILO_OK;
}
void ailo_handler_destroy(ailo_handler *receiver) {
  handler_state *state;
  if (!receiver)
    return;
  state = (handler_state *)receiver->state;
  if (state) {
    pthread_mutex_destroy(&state->mutex);
    free(state);
  }
  free(receiver);
}
ailo_status ailo_handler_handle(ailo_handler *receiver,
                                const ailo_http_request *request,
                                const ailo_http_transport *transport,
                                ailo_handler_info *info, ailo_error *error) {
  static const char hex[] = "0123456789abcdef";
  handler_state *state;
  exchange call;
  ailo_error diagnostic;
  ailo_status status, emitted;
  unsigned char random[16];
  size_t i;
  int lock, accepted, reply_ready;
  if (info)
    memset(info, 0, sizeof(*info));
  if (!receiver || !receiver->state || !request || !transport || !info ||
      !transport->begin || !transport->write || !transport->finish ||
      !transport->abort || !request->method || !request->target ||
      (!request->body && request->body_size))
    return ailo_error_set(
        error, AILO_INVALID_ARGUMENT,
        "Handler, request, transport callbacks and output info are required");
  if (RAND_bytes(random, sizeof(random)) != 1)
    return ailo_error_set(error, AILO_INTERNAL_ERROR,
                          "Request ID generation failed");
  memcpy(info->request_id, "req_", 4);
  for (i = 0; i < sizeof(random); ++i) {
    info->request_id[4 + 2 * i] = hex[random[i] >> 4];
    info->request_id[5 + 2 * i] = hex[random[i] & 15];
  }
  state = (handler_state *)receiver->state;
  call.transport = transport;
  call.info = info;
  call.transport_failure = AILO_OK;
  reply_ready = 0;
  memset(&diagnostic, 0, sizeof(diagnostic));
  lock = pthread_mutex_trylock(&state->mutex);
  if (lock) {
    info->api.http_status = lock == EBUSY ? 503 : 500;
    status = ailo_error_set(&diagnostic,
                            lock == EBUSY ? AILO_BUSY : AILO_INTERNAL_ERROR,
                            "Handler is unavailable");
  } else {
    status = validate(state, request, &info->api, &diagnostic);
    if (status == AILO_OK)
      status = ailo_client_handle(
          state->config.client, request->method, request->target, request->body,
          request->body_size, forward, &call, &info->api, &diagnostic);
  }
  if (status == AILO_OK)
    reply_ready = 1;
  if (call.transport_failure == AILO_OK && !info->started &&
      status != AILO_CANCELLED) {
    if (status != AILO_OK) {
      emitted = error_body(&call, status, &diagnostic);
      reply_ready = emitted == AILO_OK;
      if (emitted != AILO_OK) {
        status = emitted;
        ailo_error_set(&diagnostic, status,
                       "Could not write API error response");
      }
    } else
      start(&call, info->api.http_status ? info->api.http_status : 200,
            info->api.content_type ? info->api.content_type
                                   : "application/json",
            info->api.allow);
  }
  if (call.transport_failure != AILO_OK) {
    status = call.transport_failure;
    ailo_error_set(&diagnostic, status, "HTTP transport cancelled or failed");
  }
  if (info->started && call.transport_failure == AILO_OK && reply_ready) {
    accepted = transport->finish(transport->user_data);
    if (accepted > 0)
      info->completed = 1;
    else {
      status = rejected(accepted);
      ailo_error_set(&diagnostic, status,
                     "HTTP transport finalization cancelled or failed");
    }
  }
  if (info->started && !info->completed) {
    info->aborted = 1;
    transport->abort(transport->user_data);
  }
  if (!lock)
    pthread_mutex_unlock(&state->mutex);
  if (error)
    *error = diagnostic;
  return status;
}
