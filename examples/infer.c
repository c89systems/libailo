#include "ailo/client.h"
#include <errno.h>
#include <lonejson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void) {
  puts(
      "Usage: ailo_infer_example --local MODEL [options]\n"
      "   or: ailo_infer_example --repo OWNER/REPO --revision REV --file FILE\n"
      "Options: --prompt TEXT --tokens N --backend auto|cpu|vulkan --card\n"
      "         --cache ABSOLUTE-DIRECTORY --sha256 HEX --download\n"
      "         --projector LOCAL-GGUF --image FILE (or --audio FILE)\n"
      "Output is one incrementally written JSON string, or a model card.\n"
      "Downloads require --download; missing files otherwise fail offline.");
}

static lonejson_status output(void *user, const void *data, size_t size,
                              lonejson_error *error) {
  (void)user;
  (void)error;
  if (fwrite(data, 1, size, stdout) != size || fflush(stdout) != 0)
    return LONEJSON_STATUS_IO_ERROR;
  return LONEJSON_STATUS_OK;
}

static int event(void *user, const ailo_event *value) {
  lonejson_writer *writer = (lonejson_writer *)user;
  if (value->kind == AILO_EVENT_TEXT_DELTA &&
      writer->string_chunk(writer, value->data, value->size, NULL) !=
          LONEJSON_STATUS_OK)
    return -1;
  return 1;
}

int main(int argc, char **argv) {
  ailo_client_config config = ailo_client_default_config();
  ailo_request request = ailo_request_default();
  ailo_content parts[2];
  ailo_multimodal_config media;
  ailo_item item;
  ailo_run_info info;
  ailo_error error;
  ailo_client *client = NULL;
  lonejson *json = NULL;
  lonejson_writer writer;
  const char *checksum[1], *prompt = "What is two plus two?";
  char *card = NULL;
  const char *media_file = NULL;
  void *media_bytes = NULL;
  size_t media_size = 0;
  ailo_content_kind media_kind = AILO_CONTENT_IMAGE;
  int i, card_only = 0, result = 1;
  ailo_status status;
  memset(&writer, 0, sizeof(writer));
  memset(&item, 0, sizeof(item));
  memset(parts, 0, sizeof(parts));
  memset(&media, 0, sizeof(media));
  request.max_tokens = 64;
  request.temperature = 0;
  config.context_tokens = 2048;
  for (i = 1; i < argc; ++i) {
    const char *flag = argv[i];
    if (strcmp(flag, "--help") == 0) {
      usage();
      return 0;
    }
    if (strcmp(flag, "--download") == 0) {
      config.source.allow_download = 1;
      continue;
    }
    if (strcmp(flag, "--card") == 0) {
      card_only = 1;
      continue;
    }
    if (i + 1 >= argc) {
      fputs("Option requires a value; use --help\n", stderr);
      return 2;
    }
    ++i;
    if (strcmp(flag, "--local") == 0)
      config.source.local_path = argv[i];
    else if (strcmp(flag, "--repo") == 0)
      config.source.repo = argv[i];
    else if (strcmp(flag, "--revision") == 0)
      config.source.revision = argv[i];
    else if (strcmp(flag, "--file") == 0)
      config.source.filename = argv[i];
    else if (strcmp(flag, "--cache") == 0)
      config.source.cache_root = argv[i];
    else if (strcmp(flag, "--sha256") == 0) {
      checksum[0] = argv[i];
      config.source.sha256 = checksum;
      config.source.sha256_count = 1;
    } else if (strcmp(flag, "--prompt") == 0)
      prompt = argv[i];
    else if (strcmp(flag, "--projector") == 0)
      media.projector.local_path = argv[i];
    else if (strcmp(flag, "--image") == 0 || strcmp(flag, "--audio") == 0) {
      if (media_file) {
        fputs("Choose one --image or --audio input\n", stderr);
        return 2;
      }
      media_file = argv[i];
      media_kind = strcmp(flag, "--image") == 0 ? AILO_CONTENT_IMAGE
                                                : AILO_CONTENT_AUDIO;
    } else if (strcmp(flag, "--tokens") == 0) {
      char *end;
      unsigned long count;
      errno = 0;
      count = strtoul(argv[i], &end, 10);
      if (errno || *end || end == argv[i] || count < 1 || count > 4096) {
        fputs("--tokens requires an integer from 1 to 4096\n", stderr);
        return 2;
      }
      request.max_tokens = (size_t)count;
    } else if (strcmp(flag, "--backend") == 0) {
      if (strcmp(argv[i], "cpu") == 0)
        config.backend = AILO_BACKEND_CPU;
      else if (strcmp(argv[i], "vulkan") == 0)
        config.backend = AILO_BACKEND_VULKAN;
      else if (strcmp(argv[i], "auto") == 0)
        config.backend = AILO_BACKEND_AUTO;
      else {
        fputs("--backend requires auto, cpu or vulkan\n", stderr);
        return 2;
      }
    } else {
      fputs("Unknown option; use --help\n", stderr);
      return 2;
    }
  }
  if (media.projector.local_path) {
    media.backend = config.backend;
    config.multimodal = &media;
  }
  if (media_file && !config.multimodal) {
    fputs("Media input requires --projector with its matched local GGUF\n",
          stderr);
    return 2;
  }
  if (media_file) {
    FILE *input = fopen(media_file, "rb");
    long size;
    if (!input || fseek(input, 0, SEEK_END) || (size = ftell(input)) <= 0 ||
        size > 16L * 1024L * 1024L || fseek(input, 0, SEEK_SET)) {
      if (input)
        fclose(input);
      fputs("Media file must be readable, finite and at most 16 MiB\n", stderr);
      return 2;
    }
    media_size = (size_t)size;
    media_bytes = malloc(media_size);
    if (!media_bytes ||
        fread(media_bytes, 1, media_size, input) != media_size) {
      free(media_bytes);
      fclose(input);
      fputs("Cannot read media file\n", stderr);
      return 2;
    }
    fclose(input);
  }
  status = ailo_client_new(&config, &client, &error);
  if (status != AILO_OK)
    goto failed;
  if (card_only) {
    status = client->model_card(client, &card, &error);
    if (status != AILO_OK)
      goto failed;
    result = puts(card) == EOF ? 1 : 0;
    goto cleanup;
  }
  json = lonejson_new(NULL, NULL);
  if (!json ||
      lonejson_writer_init_sink(json, &writer, output, NULL, NULL) !=
          LONEJSON_STATUS_OK ||
      writer.string_begin(&writer, NULL) != LONEJSON_STATUS_OK)
    goto cleanup;
  parts[0].kind = AILO_CONTENT_TEXT;
  parts[0].data = prompt;
  parts[0].size = strlen(prompt);
  if (media_file) {
    parts[1] = parts[0];
    parts[0].kind = media_kind;
    parts[0].data = media_bytes;
    parts[0].size = media_size;
  }
  item.kind = AILO_ITEM_MESSAGE;
  item.role = AILO_ROLE_USER;
  item.content = parts;
  item.content_count = media_file ? 2 : 1;
  request.items = &item;
  request.item_count = 1;
  status = client->generate(client, &request, event, &writer, &info, &error);
  if (status != AILO_OK)
    goto failed;
  if (writer.string_end(&writer, NULL) != LONEJSON_STATUS_OK ||
      writer.finish(&writer, NULL) != LONEJSON_STATUS_OK ||
      putchar('\n') == EOF)
    goto cleanup;
  result = 0;
  goto cleanup;
failed:
  fprintf(stderr, "%s: %s\n", ailo_status_name(status), error.message);
cleanup:
  lonejson_writer_cleanup(&writer);
  lonejson_free(json);
  ailo_string_free(card);
  ailo_client_destroy(client);
  free(media_bytes);
  return result;
}
