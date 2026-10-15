#include "ailo/handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __STRICT_ANSI__
#error Handler tests must use strict C89
#endif
#ifdef __STDC_VERSION__
#error Handler tests must use C89
#endif
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                  \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

typedef struct output {
  ailo_handler *handler;
  ailo_client *client;
  ailo_handler_info *info;
  ailo_http_request *request;
  int begins, writes, finishes, aborts;
  int fail_begin, fail_write, fail_finish, cancel_begin, cancel_finish;
  int cancel_write, reenter, first_token;
  int status, retry;
  char type[64], allow[64], id[64];
  char bytes[65536];
  size_t size;
} output;
static ailo_http_transport transport(output *out);
static int begin(void *user, const ailo_http_response *response) {
  output *out = (output *)user;
  CHECK(!out->begins && !out->writes && !out->finishes && !out->aborts);
  ++out->begins;
  CHECK(response->request_id && strlen(response->request_id) == 36);
  CHECK(!strcmp(response->cache_control, "no-store"));
  strcpy(out->id, response->request_id);
  out->status = response->status;
  out->retry = response->retry_after != NULL;
  strcpy(out->type, response->content_type);
  if (response->allow)
    strcpy(out->allow, response->allow);
  return out->cancel_begin ? 0 : out->fail_begin ? out->fail_begin : 1;
}
static int write_bytes(void *user, const void *data, size_t size) {
  output *out = (output *)user;
  output nested;
  ailo_http_transport nested_transport;
  ailo_handler_info nested_info;
  ailo_error error;
  CHECK(out->begins == 1 && !out->finishes && !out->aborts);
  ++out->writes;
  if (out->cancel_write)
    return 0;
  if (out->fail_write)
    return -1;
  CHECK(size < sizeof(out->bytes) - out->size);
  memcpy(out->bytes + out->size, data, size);
  out->size += size;
  out->bytes[out->size] = 0;
  if (!out->first_token && strstr(out->bytes, "\"content\":\"a")) {
    CHECK(out->info->api.run.usage.completion_tokens.lo == 1);
    out->first_token = 1;
    if (out->reenter) {
      memset(&nested, 0, sizeof(nested));
      nested_transport = transport(&nested);
      CHECK(out->handler->handle(out->handler, out->request, &nested_transport,
                                 &nested_info, &error) == AILO_BUSY);
      CHECK(nested.status == 503 && nested.retry && nested.finishes == 1 &&
            nested.aborts == 0);
    }
    if (out->client)
      out->client->cancel(out->client);
  }
  return 1;
}
static int finish(void *user) {
  output *out = (output *)user;
  CHECK(out->begins == 1 && !out->finishes && !out->aborts);
  ++out->finishes;
  return out->cancel_finish ? 0 : out->fail_finish ? out->fail_finish : 1;
}
static void abort_response(void *user) {
  output *out = (output *)user;
  CHECK(out->begins == 1 && !out->aborts);
  ++out->aborts;
}
static ailo_http_transport transport(output *out) {
  ailo_http_transport result;
  result.user_data = out;
  result.begin = begin;
  result.write = write_bytes;
  result.finish = finish;
  result.abort = abort_response;
  return result;
}
static void run(ailo_handler *handler, ailo_http_request *request, output *out,
                ailo_handler_info *info, ailo_status expected) {
  ailo_http_transport sink;
  ailo_error error;
  sink = transport(out);
  out->handler = handler;
  out->request = request;
  out->info = info;
  CHECK(handler->handle(handler, request, &sink, info, &error) == expected);
  CHECK(error.status == expected);
  CHECK(info->http_status == out->status);
  CHECK(info->started == (out->begins != 0));
  CHECK(info->aborted == (out->aborts != 0));
  CHECK(!strcmp(info->request_id, out->id));
  CHECK(info->bytes_written.hi == 0 && info->bytes_written.lo == out->size);
}
int main(int argc, char **argv) {
  ailo_client_config client_config;
  ailo_handler_config config;
  ailo_http_request request;
  ailo_handler_info info;
  ailo_http_transport sink;
  ailo_client *client;
  ailo_handler *handler, *invalid;
  ailo_error error;
  output out;
  const char *json;
  int n;
  CHECK(argc == 2);
  client_config = ailo_client_default_config();
  client_config.source.local_path = argv[1];
  client_config.backend = AILO_BACKEND_CPU;
  client_config.threads = 1;
  client_config.context_tokens = 1024;
  client_config.lazy = 1;
  CHECK(ailo_client_new(&client_config, &client, &error) == AILO_OK);
  config = ailo_handler_default_config();
  CHECK(ailo_handler_new(&config, &invalid, &error) == AILO_INVALID_ARGUMENT &&
        !invalid);
  config.client = client;
  CHECK(ailo_handler_new(&config, &handler, &error) == AILO_OK);
  memset(&request, 0, sizeof(request));
  request.method = "GET";
  request.target = "/v1/models";
  memset(&out, 0, sizeof(out));
  run(handler, &request, &out, &info, AILO_OK);
  CHECK(out.status == 200 && out.finishes == 1 && !out.aborts);
  CHECK(strstr(out.bytes, "\"object\":\"list\"") && info.completed);
  CHECK(out.writes > 1);
  request.target = "/not-found";
  memset(&out, 0, sizeof(out));
  run(handler, &request, &out, &info, AILO_NOT_FOUND);
  CHECK(out.status == 404 && out.finishes == 1 && info.completed &&
        !out.aborts);
  CHECK(strstr(out.bytes, "\"error\":{") &&
        strstr(out.bytes, "\"param\":null"));
  request.target = "/v1/models";
  request.method = "POST";
  memset(&out, 0, sizeof(out));
  run(handler, &request, &out, &info, AILO_UNSUPPORTED);
  CHECK(out.status == 405 && !strcmp(out.allow, "GET") && out.finishes == 1);
  request.target = "/v1/chat/completions";
  request.body = "{";
  request.body_size = 1;
  request.content_type = "application/json";
  memset(&out, 0, sizeof(out));
  run(handler, &request, &out, &info, AILO_INVALID_JSON);
  CHECK(out.status == 400 && out.finishes == 1 && !out.aborts);
  request.content_type = "application/js on";
  memset(&out, 0, sizeof(out));
  run(handler, &request, &out, &info, AILO_UNSUPPORTED);
  CHECK(out.status == 415 && out.finishes == 1);
  request.content_type = NULL;
  memset(&out, 0, sizeof(out));
  run(handler, &request, &out, &info, AILO_UNSUPPORTED);
  CHECK(out.status == 415);
  request.content_type = "application/json";
  request.content_encoding = "gzip";
  memset(&out, 0, sizeof(out));
  run(handler, &request, &out, &info, AILO_UNSUPPORTED);
  CHECK(out.status == 415);
  request.content_encoding = NULL;
  request.body_size = 8 * 1024 * 1024 + 1;
  memset(&out, 0, sizeof(out));
  run(handler, &request, &out, &info, AILO_OUT_OF_RANGE);
  CHECK(out.status == 413);
  json = "{\"model\":\"local-model\",\"messages\":[{\"role\":\"user\","
         "\"content\":\"Hi\"}],\"store\":false,\"stream\":true,\"max_"
         "completion_tokens\":8,\"temperature\":0,\"logit_bias\":{\"100\":100},"
         "\"stream_options\":{\"include_obfuscation\":false}}";
  request.body = json;
  request.body_size = strlen(json);
  request.content_type = " Application/JSON ; charset = \"UTF-8\" ";
  memset(&out, 0, sizeof(out));
  out.reenter = 1;
  run(handler, &request, &out, &info, AILO_OK);
  CHECK(out.first_token && strstr(out.bytes, "data: [DONE]\n\n"));
  CHECK(!strcmp(out.type, "text/event-stream") && out.status == 200 &&
        info.completed);
  /* An adapter error after first output cannot replace the wire status. */
  memset(&out, 0, sizeof(out));
  out.client = client;
  run(handler, &request, &out, &info, AILO_CANCELLED);
  CHECK(out.first_token && out.status == 200 && out.aborts == 1 &&
        !out.finishes);
  CHECK(!strstr(out.bytes, "[DONE]") && !strstr(out.bytes, "\"error\""));
  for (n = 0; n < 6; ++n) {
    memset(&out, 0, sizeof(out));
    out.fail_begin = n == 0 ? -1 : 0;
    out.fail_write = n == 1;
    out.cancel_write = n == 2;
    out.fail_finish = n == 3 ? -1 : 0;
    out.cancel_finish = n == 4;
    out.cancel_begin = n == 5;
    run(handler, &request, &out, &info,
        n == 2 || n >= 4 ? AILO_CANCELLED : AILO_SINK_FAILED);
    CHECK(out.aborts == 1 && !info.completed);
    if (n < 3)
      CHECK(!out.finishes);
  }
  request.method = "GET";
  request.target = "/v1/models";
  request.body = NULL;
  request.body_size = 0;
  memset(&out, 0, sizeof(out));
  sink = transport(&out);
  sink.abort = NULL;
  CHECK(handler->handle(handler, &request, &sink, &info, &error) ==
        AILO_INVALID_ARGUMENT);
  CHECK(!out.begins && !info.started);
  handler->destroy(handler);
  /* Lowered ingress limits do not change the adapter's global bounds. */
  config.max_target_bytes = 3;
  CHECK(ailo_handler_new(&config, &handler, &error) == AILO_OK);
  memset(&out, 0, sizeof(out));
  run(handler, &request, &out, &info, AILO_OUT_OF_RANGE);
  CHECK(out.status == 414 && info.completed);
  handler->destroy(handler);
  client->destroy(client);
  /* Native diagnostics stay local; wire errors never expose private paths. */
  client_config.source.local_path =
      "/libailo-private-model-does-not-exist.gguf";
  CHECK(ailo_client_new(&client_config, &client, &error) == AILO_OK);
  config = ailo_handler_default_config();
  config.client = client;
  CHECK(ailo_handler_new(&config, &handler, &error) == AILO_OK);
  request.method = "POST";
  request.target = "/v1/chat/completions";
  request.body = json;
  request.body_size = strlen(json);
  memset(&out, 0, sizeof(out));
  sink = transport(&out);
  CHECK(handler->handle(handler, &request, &sink, &info, &error) != AILO_OK);
  CHECK(out.status == 500 && out.finishes == 1 && info.completed);
  CHECK(strstr(out.bytes, "server_error") &&
        !strstr(out.bytes, "libailo-private"));
  handler->destroy(handler);
  client->destroy(client);
  ailo_handler_destroy(NULL);
  puts("strict C89 handler lifecycle and immediate streaming passed");
  return 0;
}
