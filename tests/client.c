#include "ailo/client.h"
#include "ailo/native.h"
#include "ailo/openai.h"
#include <lonejson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "%s:%d: %s (%s)\n", __FILE__, __LINE__, #condition,      \
              error.message);                                                  \
      result = 1;                                                              \
      goto cleanup;                                                            \
    }                                                                          \
  } while (0)

typedef struct observation {
  ailo_client *client;
  const ailo_request *request;
  ailo_run_info *info;
  unsigned int sequence, begins, ends, deltas, complete;
  int mode, failed, first_byte_seen;
  char bytes[128];
  size_t size;
  lonejson_writer *writer;
} observation;

typedef struct route_output {
  char bytes[16384];
  size_t size;
  ailo_api_info *info;
  ailo_client *client;
  int action, first, failed;
} route_output;

static int route_bytes(void *data, const void *bytes, size_t size) {
  route_output *out = (route_output *)data;
  if (size >= sizeof(out->bytes) - out->size)
    return -1;
  memcpy(out->bytes + out->size, bytes, size);
  out->size += size;
  out->bytes[out->size] = '\0';
  if (!out->first && strstr(out->bytes, "\"content\":\"a")) {
    ailo_api_info nested;
    ailo_error problem;
    out->first = 1;
    if (out->info->run.usage.completion_tokens.lo != 1 ||
        out->client->handle(out->client, "GET", "/v1/models", NULL, 0,
                            route_bytes, out, &nested, &problem) != AILO_BUSY)
      out->failed = 1;
    return out->action;
  }
  return 1;
}

static int router(ailo_client *client) {
  ailo_client_config cfg = ailo_client_default_config();
  ailo_client *catalog = NULL;
  ailo_api_info info;
  ailo_error error;
  route_output out;
  const char *json =
      "{\"model\":\"receiver-test\",\"messages\":[{\"role\":\"user\","
      "\"content\":\"hello\"}],\"store\":false,\"stream\":true,\"max_"
      "completion_tokens\":8,\"temperature\":0,\"stream_options\":{\"include_"
      "obfuscation\":false}}";
  const char *bad[] = {"/unknown",
                       "/v1/models/unknown",
                       "/v1/models/%GG",
                       "/v1/models/%00",
                       "/v1/models/%ff",
                       "/v1/models?x=1",
                       "/v1/chat/completions?limit=1&limit=2",
                       "/v1/chat/completions?limit=0",
                       "/v1/chat/completions?limit=1.5",
                       "/v1/chat/completions?after=bad%GG",
                       "https://example.com/v1/models",
                       "/v1/models#fragment"};
  size_t n;
  int action, result = 0;
  cfg.source.local_path = "/ailo-catalog-never-load-this-file.gguf";
  cfg.model_id = "model/one+two";
  cfg.backend = AILO_BACKEND_CPU;
  cfg.lazy = 1;
  CHECK(ailo_client_new(&cfg, &catalog, &error) == AILO_OK);
  memset(&out, 0, sizeof(out));
  CHECK(catalog->handle(catalog, "GET", "/v1/models", NULL, 0, route_bytes,
                        &out, &info, &error) == AILO_OK);
  CHECK(info.http_status == 200 && info.started && !info.allow &&
        strstr(out.bytes, "\"id\":\"model/one+two\"") &&
        !strstr(out.bytes, "never-load"));
  memset(&out, 0, sizeof(out));
  CHECK(ailo_client_handle(catalog, "GET", "/v1/models/model%2Fone+two", NULL,
                           0, route_bytes, &out, &info, &error) == AILO_OK);
  CHECK(strstr(out.bytes, "\"object\":\"model\"") &&
        !strstr(out.bytes, "\"data\""));
  for (n = 0; n < sizeof(bad) / sizeof(bad[0]); ++n) {
    memset(&out, 0, sizeof(out));
    CHECK(catalog->handle(catalog, "GET", bad[n], NULL, 0, route_bytes, &out,
                          &info, &error) != AILO_OK);
    CHECK(!info.started && !out.size &&
          info.http_status == (n < 2 ? 404 : 400));
  }
  CHECK(catalog->handle(catalog, "POST", "/v1/models", NULL, 0, route_bytes,
                        &out, &info, &error) == AILO_UNSUPPORTED);
  CHECK(info.http_status == 405 && info.allow && !strcmp(info.allow, "GET"));
  CHECK(catalog->handle(catalog, "PUT", "/v1/chat/completions", NULL, 0,
                        route_bytes, &out, &info, &error) == AILO_UNSUPPORTED);
  CHECK(info.http_status == 405 && !strcmp(info.allow, "GET, POST"));
  CHECK(catalog->handle(catalog, "GET", "/v1/models", "{}", 2, route_bytes,
                        &out, &info, &error) == AILO_INVALID_ARGUMENT &&
        !info.started);
  CHECK(catalog->handle(catalog, "BAD METHOD", "/v1/models", NULL, 0,
                        route_bytes, &out, &info,
                        &error) == AILO_INVALID_ARGUMENT);
  for (action = -1; action <= 1; ++action) {
    memset(&out, 0, sizeof(out));
    out.client = client;
    out.info = &info;
    out.action = action;
    CHECK(client->handle(client, "POST", "/v1/chat/completions", json,
                         strlen(json), route_bytes, &out, &info,
                         &error) == (action < 0    ? AILO_SINK_FAILED
                                     : action == 0 ? AILO_CANCELLED
                                                   : AILO_OK));
    CHECK(out.first && !out.failed && info.started && !info.stored);
    CHECK((strstr(out.bytes, "data: [DONE]") != NULL) == (action == 1));
    CHECK(info.run.usage.completion_tokens.lo == (action == 1 ? 8U : 1U));
  }
cleanup:
  ailo_client_destroy(catalog);
  return result;
}

static lonejson_status json_bytes(void *data, const void *bytes, size_t size,
                                  lonejson_error *error) {
  observation *seen = (observation *)data;
  (void)error;
  if (size > sizeof(seen->bytes) - seen->size)
    return LONEJSON_STATUS_OVERFLOW;
  if (memchr(bytes, 'a', size) != NULL && !seen->first_byte_seen) {
    seen->first_byte_seen = 1;
    if (seen->info->usage.completion_tokens.hi != 0 ||
        seen->info->usage.completion_tokens.lo != 1)
      seen->failed = 1;
  }
  memcpy(seen->bytes + seen->size, bytes, size);
  seen->size += size;
  return LONEJSON_STATUS_OK;
}

static int observe(void *data, const ailo_event *event) {
  observation *seen = (observation *)data;
  if (event->sequence.hi || event->sequence.lo != seen->sequence++)
    seen->failed = 1;
  if (strcmp(event->model_id, "receiver-test") != 0)
    seen->failed = 1;
  if (event->kind == AILO_EVENT_CHOICE_BEGIN)
    ++seen->begins;
  if (event->kind == AILO_EVENT_CHOICE_END)
    ++seen->ends;
  if (event->kind == AILO_EVENT_RUN_END)
    ++seen->complete;
  if (event->kind == AILO_EVENT_TEXT_DELTA) {
    ailo_error nested_error;
    char *card = (char *)1;
    ++seen->deltas;
    if (!event->size || seen->info->usage.completion_tokens.lo == 0)
      seen->failed = 1;
    if (seen->mode == 1 && seen->deltas == 1) {
      ailo_run_info nested_info;
      if (seen->client->model_card(seen->client, &card, &nested_error) !=
              AILO_BUSY ||
          card)
        seen->failed = 1;
      if (seen->client->generate(seen->client, seen->request, observe, seen,
                                 &nested_info, &nested_error) != AILO_BUSY)
        seen->failed = 1;
    }
    if (seen->writer &&
        lonejson_writer_string_chunk(seen->writer, event->data, event->size,
                                     NULL) != LONEJSON_STATUS_OK)
      return -1;
    if (seen->mode == 2) {
      seen->client->cancel(seen->client);
      return 1;
    }
    if (seen->mode == 3)
      return 0;
    if (seen->mode == 4)
      return -1;
  }
  return seen->failed ? -1 : 1;
}

static int run(const char *path) {
  ailo_client_config config = ailo_client_default_config();
  ailo_request request = ailo_request_default();
  ailo_content content;
  ailo_item item;
  ailo_run_info info;
  ailo_error error;
  ailo_client *client = NULL, *other = NULL;
  lonejson *json = NULL;
  lonejson_writer writer;
  observation seen;
  char *card = NULL;
  char *owned_path = NULL;
  char model_id[] = "receiver-test";
  char supplemental[] = "{\"name\":\"caller "
                        "model\",\"license\":{\"id\":\"MIT\"},\"source\":{"
                        "\"base_models\":[\"fixture/base\"]}}";
  const char *stops[] = {"aa"};
  int result = 0, mode;
  memset(&writer, 0, sizeof(writer));
  memset(&seen, 0, sizeof(seen));
  memset(&item, 0, sizeof(item));
  memset(&error, 0, sizeof(error));
  content.kind = AILO_CONTENT_TEXT;
  content.data = "hello";
  content.size = 5;
  item.kind = AILO_ITEM_MESSAGE;
  item.role = AILO_ROLE_USER;
  item.content = &content;
  item.content_count = 1;
  request.items = &item;
  request.item_count = 1;
  request.max_tokens = 8;
  request.temperature = 0;
  config.backend = AILO_BACKEND_CPU;
  config.context_tokens = 128;
  config.lazy = 1;
  owned_path = (char *)malloc(strlen(path) + 1);
  CHECK(owned_path != NULL);
  strcpy(owned_path, path);
  config.source.local_path = owned_path;
  config.model_id = model_id;
  config.model_metadata = supplemental;
  CHECK(ailo_client_new(&config, &client, &error) == AILO_OK && client);
  owned_path[0] = '!';
  model_id[0] = '!';
  supplemental[0] = '!';
  CHECK(client->capabilities(client, &card, &error) == AILO_OK);
  CHECK(strstr(card, "\"streaming\":true") != NULL);
  ailo_string_free(card);
  card = NULL;
  CHECK(client->devices(client, &card, &error) == AILO_OK);
  CHECK(strstr(card, "\"provider\":\"CPU\"") != NULL);
  CHECK(strstr(card, "\"memory_free_bytes\":null") != NULL);
  ailo_string_free(card);
  card = NULL;
  CHECK(client->model_card(client, &card, &error) == AILO_OK);
  json = lonejson_new(NULL, NULL);
  CHECK(json != NULL);
  CHECK(lonejson_validate_cstr(json, card, NULL) == LONEJSON_STATUS_OK);
  CHECK(strstr(card, "\"id\":\"receiver-test\"") != NULL);
  CHECK(strstr(card, "\"architecture\":\"llama\"") != NULL);
  CHECK(strstr(card, "\"configured_tokens\":128") != NULL);
  CHECK(strstr(card, "\"effective_tokens\":256") != NULL);
  CHECK(strstr(card, "\"checksum_validated\":false") != NULL);
  CHECK(strstr(card, "\"id\":\"MIT\"") != NULL);
  CHECK(strstr(card, "\"origin\":\"caller\"") != NULL);
  CHECK(strstr(card, "\"name\":\"ailo-deterministic-test\"") != NULL);
  CHECK(strstr(card, "\"claim\":\"caller model\"") != NULL);
  CHECK(strstr(card, "\"base_models\":[\"fixture/base\"]") != NULL);
  CHECK(strstr(card, path) == NULL);
  ailo_string_free(card);
  card = NULL;

  CHECK(router(client) == 0);

  seen.client = client;
  seen.request = &request;
  seen.info = &info;
  seen.mode = 1;
  CHECK(lonejson_writer_init_sink(json, &writer, json_bytes, &seen, NULL) ==
        LONEJSON_STATUS_OK);
  seen.writer = &writer;
  CHECK(lonejson_writer_string_begin(&writer, NULL) == LONEJSON_STATUS_OK);
  CHECK(client->generate(client, &request, observe, &seen, &info, &error) ==
        AILO_OK);
  CHECK(lonejson_writer_string_end(&writer, NULL) == LONEJSON_STATUS_OK);
  CHECK(lonejson_writer_finish(&writer, NULL) == LONEJSON_STATUS_OK);
  CHECK(!seen.failed && seen.first_byte_seen && seen.begins == 1 &&
        seen.ends == 1 && seen.complete == 1 && seen.deltas == 8);
  CHECK(info.usage.completion_tokens.hi == 0 &&
        info.usage.completion_tokens.lo == 8);
  CHECK(info.completed_candidates == 1 &&
        info.finish_reason == AILO_FINISH_LENGTH);
  CHECK(seen.size == 10 && memcmp(seen.bytes, "\"aaaaaaaa\"", 10) == 0);
  lonejson_writer_cleanup(&writer);

  for (mode = 2; mode <= 4; ++mode) {
    memset(&seen, 0, sizeof(seen));
    seen.client = client;
    seen.request = &request;
    seen.info = &info;
    seen.mode = mode;
    CHECK(client->generate(client, &request, observe, &seen, &info, &error) ==
          (mode == 4 ? AILO_SINK_FAILED : AILO_CANCELLED));
    CHECK(!seen.failed && seen.deltas == 1 && seen.ends == 0 &&
          seen.complete == 0);
    CHECK(info.usage.completion_tokens.lo == 1 &&
          info.completed_candidates == 0);
  }
  memset(&seen, 0, sizeof(seen));
  seen.client = client;
  seen.request = &request;
  seen.info = &info;
  request.stop = stops;
  request.stop_count = 1;
  CHECK(client->generate(client, &request, observe, &seen, &info, &error) ==
        AILO_OK);
  CHECK(!seen.failed && seen.deltas == 0 &&
        info.usage.completion_tokens.lo == 2 &&
        info.finish_reason == AILO_FINISH_STOP);
  request.stop = NULL;
  request.stop_count = 0;
  request.candidates = 2;
  request.max_tokens = 4;
  memset(&seen, 0, sizeof(seen));
  seen.client = client;
  seen.request = &request;
  seen.info = &info;
  CHECK(client->generate(client, &request, observe, &seen, &info, &error) ==
        AILO_OK);
  CHECK(!seen.failed && seen.deltas == 8 && seen.begins == 2 &&
        seen.ends == 2 && info.completed_candidates == 2 &&
        info.usage.completion_tokens.lo == 8);
  request.max_tokens = 256;
  memset(&seen, 0, sizeof(seen));
  seen.info = &info;
  CHECK(client->generate(client, &request, observe, &seen, &info, &error) ==
        AILO_CONTEXT_LIMIT);
  CHECK(seen.sequence == 0 && info.completed_candidates == 0);
  request.max_tokens = 4;

  config.source.local_path = path;
  config.model_id = "receiver-test";
  config.model_metadata = NULL;
  config.lazy = 0;
  CHECK(ailo_llama_backend_init(&error) == AILO_OK);
  CHECK(ailo_client_new(&config, &other, &error) == AILO_OK);
  CHECK(ailo_llama_backend_free(&error) == AILO_OK);
  client->destroy(client);
  client = other;
  other = NULL;
  memset(&seen, 0, sizeof(seen));
  seen.client = client;
  seen.request = &request;
  seen.info = &info;
  CHECK(client->generate(client, &request, observe, &seen, &info, &error) ==
        AILO_OK);

  config.source.local_path = "/ailo-file-that-does-not-exist.gguf";
  config.lazy = 1;
  CHECK(ailo_client_new(&config, &other, &error) == AILO_OK);
  item.kind = AILO_ITEM_FUNCTION_CALL;
  CHECK(other->generate(other, &request, observe, &seen, &info, &error) ==
        AILO_INVALID_ARGUMENT);
  item.kind = AILO_ITEM_MESSAGE;
  content.data = "\xc0\x80";
  content.size = 2;
  CHECK(other->generate(other, &request, observe, &seen, &info, &error) ==
        AILO_INVALID_ARGUMENT);
  CHECK(other->model_card(other, &card, &error) == AILO_IO_FAILED &&
        card == NULL);
  puts("C89 receiver: native deltas through lonejson before token two, "
       "stop/cancel/busy/ownership/card: passed");
cleanup:
  lonejson_writer_cleanup(&writer);
  lonejson_free(json);
  ailo_string_free(card);
  ailo_client_destroy(other);
  ailo_client_destroy(client);
  free(owned_path);
  return result;
}

int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  return run(argv[1]);
}
