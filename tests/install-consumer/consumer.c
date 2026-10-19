#include "ailo/base.h"
#include "ailo/client.h"
#include "ailo/model.h"
#include "ailo/native.h"
#include "ailo/version.h"
#include <stdio.h>
#include <string.h>

static int count_tokens(void *data, const ailo_event *event) {
  if (event->kind == AILO_EVENT_TEXT_DELTA)
    ++*(unsigned int *)data;
  return 1;
}

int main(int argc, char **argv) {
  struct ailo_gguf_context *gguf = NULL;
  ailo_model_source source;
  ailo_error error;
  ailo_i64 key;
  ailo_u64 value, actual;
  char *path = NULL;
  ailo_client *client = NULL;
  ailo_client_config config;
  ailo_request request;
  ailo_item item;
  ailo_content content;
  ailo_run_info info;
  unsigned int deltas = 0;
  if (argc != 2)
    return 2;
  if (strcmp(AILO_VERSION, EXPECTED_VERSION) != 0 || AILO_ABI_VERSION != 0)
    return 1;
  memset(&source, 0, sizeof(source));
  source.local_path = argv[1];
  if (ailo_model_acquire(&source, &path, &error) != AILO_OK) {
    fprintf(stderr, "%s\n", error.message);
    return 1;
  }
  ailo_string_free(path);
  config = ailo_client_default_config();
  config.source.local_path = argv[1];
  config.backend = AILO_BACKEND_CPU;
  if (ailo_client_new(&config, &client, &error) != AILO_OK)
    return 1;
  if (client->model_card(client, &path, &error) != AILO_OK) {
    client->destroy(client);
    return 1;
  }
  if (!strstr(path, "\"architecture\":\"llama\"")) {
    ailo_string_free(path);
    client->destroy(client);
    return 1;
  }
  ailo_string_free(path);
  memset(&item, 0, sizeof(item));
  content.kind = AILO_CONTENT_TEXT;
  content.media = NULL;
  content.data = "hello";
  content.size = 5;
  item.content = &content;
  item.content_count = 1;
  request = ailo_request_default();
  request.items = &item;
  request.item_count = 1;
  request.max_tokens = 4;
  request.temperature = 0;
  if (client->generate(client, &request, count_tokens, &deltas, &info,
                       &error) != AILO_OK ||
      deltas != 4) {
    client->destroy(client);
    return 1;
  }
  client->destroy(client);
  value.hi = 0xffffffffU;
  value.lo = 0xffffffffU;
  if (ailo_gguf_init_empty(&gguf, &error) != AILO_OK || !gguf ||
      ailo_gguf_set_val_u64(gguf, "full.width", value, &error) != AILO_OK ||
      ailo_gguf_find_key(gguf, "full.width", &key, &error) != AILO_OK ||
      ailo_gguf_get_val_u64(gguf, key, &actual, &error) != AILO_OK ||
      actual.hi != value.hi || actual.lo != value.lo ||
      ailo_gguf_free(gguf, &error) != AILO_OK)
    return 1;
  puts("Installed C89/C++ facade, full-width GGUF and model acquisition: "
       "passed");
  return 0;
}
