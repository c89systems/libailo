#include "ailo/cache.h"
#include "ailo/client.h"
#include "ailo/openai.h"
#include "ailo_continuation_internal.hpp"
#include "ailo_media_internal.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <pslog.h>
#include <stdexcept>
#include <sys/resource.h>

#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c))                                                                  \
      throw std::runtime_error(std::string(#c) + ":" +                         \
                               std::to_string(__LINE__));                      \
  } while (0)
using namespace ailo::client;
std::string read(const char *path) {
  std::ifstream input(path, std::ios::binary);
  CHECK(bool(input));
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}
struct observations {
  std::string text;
  unsigned deltas = 0;
  bool finished = false, cancel = false;
  std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();
  uint64_t first_delta_us = 0;
  static int event(void *p, const ailo_event *e) {
    auto &s = *static_cast<observations *>(p);
    if (e->kind == AILO_EVENT_TEXT_DELTA) {
      CHECK(!s.finished);
      if (!s.deltas)
        s.first_delta_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - s.start)
                .count());
      ++s.deltas;
      s.text.append(static_cast<const char *>(e->data), e->size);
      if (s.cancel)
        return 0;
    }
    if (e->kind == AILO_EVENT_RUN_END)
      s.finished = true;
    return 1;
  }
  static int wire(void *p, const void *data, size_t n) {
    auto &s = *static_cast<observations *>(p);
    s.text.append(static_cast<const char *>(data), n);
    return 1;
  }
};
struct image_reference {
  const std::string &bytes;
  unsigned calls = 0;
  static ailo_status resolve(void *p, ailo_media_reference kind, const char *id,
                             size_t limit, long, ailo_media_sink sink,
                             void *user, ailo_error *) {
    auto &s = *static_cast<image_reference *>(p);
    CHECK(kind == AILO_MEDIA_FILE_ID && std::string(id) == "fixture-red" &&
          s.bytes.size() <= limit);
    ++s.calls;
    return sink(user, s.bytes.data(), s.bytes.size()) > 0 ? AILO_OK
                                                          : AILO_CANCELLED;
  }
};
std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return s;
}
void ok(ailo_status status, const ailo_error &error) {
  if (status != AILO_OK)
    throw std::runtime_error(std::string(ailo_status_name(status)) + ": " +
                             error.message);
}
int main(int argc, char **argv) {
  try {
    if (argc != 7) {
      std::fprintf(stderr, "Usage: media_inference vision|audio ABSOLUTE-CACHE "
                           "download|offline cpu|vulkan FIXTURE STATE-ROOT\n");
      return 2;
    }
    const bool vision = std::string(argv[1]) == "vision";
    CHECK(vision || std::string(argv[1]) == "audio");
    const bool download = std::string(argv[3]) == "download";
    CHECK(download || std::string(argv[3]) == "offline");
    const auto bytes = read(argv[5]);
    image_reference reference{bytes};
    auto config = ailo_client_default_config();
    config.backend =
        std::string(argv[4]) == "cpu" ? AILO_BACKEND_CPU : AILO_BACKEND_VULKAN;
    config.context_tokens = 4096;
    config.threads = 2;
    config.source.repo = vision ? "ggml-org/SmolVLM-256M-Instruct-GGUF"
                                : "ggml-org/Qwen3-ASR-0.6B-GGUF";
    config.source.revision = vision
                                 ? "b9e4379657e1450d04d02eec8e345667265b0a00"
                                 : "928ab958557df9aa2ef1c93e0e83c7ad0933fae2";
    config.source.filename =
        vision ? "SmolVLM-256M-Instruct-Q8_0.gguf" : "Qwen3-ASR-0.6B-Q8_0.gguf";
    config.source.cache_root = argv[2];
    config.source.allow_download = download;
    const char *model_hash[] = {
        vision
            ? "2a31195d3769c0b0fd0a4906201666108834848db768af11de1d2cef7cd35e65"
            : "bca259818b50ca7c4c05e9bdb35a5dc04fa039653a6d6f3f0f331f96f6aa197"
              "1"};
    const char *projector_hash[] = {
        vision
            ? "7e943f7c53f0382a6fc41b6ee0c2def63ba4fded9ab8ed039cc9e2ab905e0edd"
            : "41a342b5e4c514e968cb756de6cd1b7be39eff43c44c57a2ef5fc6522e36603"
              "d"};
    config.source.sha256 = model_hash;
    config.source.sha256_count = 1;
    ailo_multimodal_config media{};
    media.projector = config.source;
    media.projector.filename = vision ? "mmproj-SmolVLM-256M-Instruct-Q8_0.gguf"
                                      : "mmproj-Qwen3-ASR-0.6B-Q8_0.gguf";
    media.projector.sha256 = projector_hash;
    media.backend = config.backend;
    if (vision) {
      media.resolver = image_reference::resolve;
      media.user_data = &reference;
    }
    config.multimodal = &media;
    ailo_error error{};
    const std::string kv_root = std::string(argv[6]) + "/kv";
    const std::string state_root = std::string(argv[6]) + "/state";
    const std::string key_file = std::string(argv[6]) + "/config/pouch.key";
    auto cache_config = ailo_cache_default_config();
    cache_config.namespace_id = "media-e2e";
    cache_config.directory = kv_root.c_str();
    const unsigned char key[32] = {17, 19, 23, 29};
    cache_config.key = key;
    cache_config.key_size = sizeof(key);
    cache_config.key_id = "fixture-key";
    cache_config.max_disk_bytes = 128 * 1024 * 1024;
    cache_config.disk_ttl_seconds = 3600;
    ailo_cache *cache_raw = nullptr;
    ok(ailo_cache_new(&cache_config, &cache_raw, &error), error);
    std::unique_ptr<ailo_cache, decltype(&ailo_cache_destroy)> cache(
        cache_raw, ailo_cache_destroy);
    ok(ailo_cache_clear(cache.get(), &error), error);
    config.cache = cache.get();
    auto api = ailo_openai_default_config();
    api.state_directory = state_root.c_str();
    api.encryption_key_file = key_file.c_str();
    api.namespace_id = "media-e2e";
    config.openai = &api;
    ailo_client *raw = nullptr;
    std::filesystem::create_directories(argv[6]);
    const std::string log_path = std::string(argv[6]) + "/native.log";
    std::unique_ptr<FILE, void (*)(FILE *)> log_file(
        std::fopen(log_path.c_str(), "w+"), [](FILE *p) { std::fclose(p); });
    CHECK(log_file);
    pslog_config logging;
    pslog_default_config(&logging);
    logging.mode = PSLOG_MODE_JSON;
    logging.min_level = PSLOG_LEVEL_TRACE;
    logging.output = pslog_output_from_fp(log_file.get(), 0);
    std::unique_ptr<pslog_logger, void (*)(pslog_logger *)> logger(
        pslog_new(&logging), [](pslog_logger *p) {
          if (p)
            p->destroy(p);
        });
    CHECK(logger);
    ok(ailo_client_new_with_logger(&config, logger.get(), &raw, &error), error);
    std::unique_ptr<ailo_client, decltype(&ailo_client_destroy)> client(
        raw, ailo_client_destroy);
    auto &native = *static_cast<state *>(raw->state);
    CHECK(native.media && native.media->context &&
          native.media->actual_device ==
              (config.backend == AILO_BACKEND_CPU
                   ? "cpu"
                   : native.selected_info.front().id));
    const std::string question = vision ? "What is the main color of this "
                                          "image? Answer with one color word."
                                        : "";
    ailo_content parts[] = {
        {vision ? AILO_CONTENT_IMAGE : AILO_CONTENT_AUDIO, bytes.data(),
         bytes.size(), nullptr},
        {AILO_CONTENT_TEXT, question.data(), question.size(), nullptr}};
    ailo_item item{};
    item.content = parts;
    item.content_count = 2;
    auto request = ailo_request_default();
    request.items = &item;
    request.item_count = 1;
    request.max_tokens = vision ? 16 : 128;
    request.temperature = 0;
    request.seed = 42;
    ailo_run_info run{};
    observations first;
    ok(raw->generate(raw, &request, observations::event, &first, &run, &error),
       error);
    CHECK(first.deltas && first.finished && !first.text.empty());
    CHECK(std::fflush(log_file.get()) == 0);
    const auto native_log = read(log_path.c_str());
    CHECK(native_log.find(config.backend == AILO_BACKEND_CPU
                              ? "CLIP using CPU backend"
                              : "CLIP using Vulkan") != std::string::npos);
    const auto preparation_at = native_log.find("media_preprocess_us=");
    CHECK(preparation_at != std::string::npos);
    std::printf("media decoding/preprocessing/tokenization: %llu us\n",
                std::stoull(native_log.substr(preparation_at + 20)));
    std::printf("%s %s: %s\n", argv[1], argv[4], first.text.c_str());
    std::fflush(stdout);
    struct rusage resources{};
    CHECK(getrusage(RUSAGE_SELF, &resources) == 0);
    std::printf(
        "first transcript/text delta: %llu us; peak process RSS: %ld KiB\n",
        static_cast<unsigned long long>(first.first_delta_us),
        resources.ru_maxrss);
    CHECK(vision ? lower(first.text).find("red") != std::string::npos
                 : lower(first.text).find("country") != std::string::npos);
    if (!vision)
      CHECK(first.text.find("<asr_text>") == std::string::npos &&
            first.text.rfind("language ", 0) != 0);
    observations repeated;
    ok(raw->generate(raw, &request, observations::event, &repeated, &run,
                     &error),
       error);
    CHECK(repeated.text == first.text && run.usage.cached_tokens.lo > 0);
    ok(raw->snapshot_export(raw, "media", &error), error);
    observations cancelled;
    cancelled.cancel = true;
    CHECK(raw->generate(raw, &request, observations::event, &cancelled, &run,
                        &error) == AILO_CANCELLED);
    observations recovered;
    ok(raw->generate(raw, &request, observations::event, &recovered, &run,
                     &error),
       error);
    CHECK(recovered.text == first.text);
    if (vision) {
      const std::string other_path =
          (std::filesystem::path(argv[5]).parent_path() / "blue.png").string();
      const auto blue = read(other_path.c_str());
      parts[0].data = blue.data();
      parts[0].size = blue.size();
      observations changed;
      ok(raw->generate(raw, &request, observations::event, &changed, &run,
                       &error),
         error);
      std::printf("changed image: %s\n", changed.text.c_str());
      std::fflush(stdout);
      CHECK(lower(changed.text).find("blue") != std::string::npos &&
            run.usage.cached_tokens.lo == 0);
      const std::string label1 = "First image:\n", label2 = "\nSecond image:\n";
      const std::string compare =
          "\nName the colors of the first and second images, in that order.";
      ailo_content pair[] = {
          {AILO_CONTENT_TEXT, label1.data(), label1.size(), nullptr},
          parts[0],
          {AILO_CONTENT_TEXT, label2.data(), label2.size(), nullptr},
          {AILO_CONTENT_IMAGE, blue.data(), blue.size(), nullptr},
          {AILO_CONTENT_TEXT, compare.data(), compare.size(), nullptr}};
      pair[1].data = bytes.data();
      pair[1].size = bytes.size();
      auto pair_item = item;
      pair_item.content = pair;
      pair_item.content_count = 5;
      auto pair_request = request;
      pair_request.items = &pair_item;
      pair_request.max_tokens = 32;
      observations compared;
      ok(raw->generate(raw, &pair_request, observations::event, &compared, &run,
                       &error),
         error);
      std::printf("two ordered images: %s\n", compared.text.c_str());
      std::fflush(stdout);
      const auto colors = lower(compared.text);
      CHECK(colors.find("red") != std::string::npos &&
            colors.find("blue") != std::string::npos &&
            colors.find("red") < colors.find("blue"));
      parts[0].data = bytes.data();
      parts[0].size = bytes.size();
    }
    const auto format = media_format(bytes.data(), bytes.size(), parts[0].kind);
    CHECK(format == (vision ? "png" : "wav"));
    const std::string b64 = media_base64(bytes.data(), bytes.size());
    std::string wire =
        vision ? "{\"input\":[{\"role\":\"user\",\"content\":[{\"type\":"
                 "\"input_image\",\"file_id\":\"fixture-red\"},{\"type\":"
                 "\"input_text\",\"text\":\"What is the main "
                 "color?\"}]}],\"stream\":true,\"max_output_tokens\":16,"
                 "\"temperature\":0}"
               : "{\"model\":\"local-model\",\"messages\":[{\"role\":\"user\","
                 "\"content\":[{\"type\":\"input_audio\",\"input_audio\":{"
                 "\"format\":\"wav\",\"data\":\"" +
                     b64 +
                     "\"}}]}],\"store\":false,\"stream\":true,\"max_completion_"
                     "tokens\":128,\"temperature\":0}";
    observations api_output;
    ailo_api_info api_info{};
    ok(vision
           ? raw->responses(raw, wire.data(), wire.size(), observations::wire,
                            &api_output, &api_info, &error)
           : raw->chat_completions(raw, wire.data(), wire.size(),
                                   observations::wire, &api_output, &api_info,
                                   &error),
       error);
    CHECK(api_output.text.find(vision ? "response.output_text.delta"
                                      : "chat.completion.chunk") !=
          std::string::npos);
    std::puts("Wire streaming completed");
    std::fflush(stdout);
    std::string previous = api_info.id;
    CHECK(!vision || reference.calls == 1);
    client.reset();
    cache.reset();
    std::puts("Closed original owners");
    std::fflush(stdout);
    // Recreate both owners and reopen artifacts offline, including a persistent
    // checkpoint.
    config.source.allow_download = media.projector.allow_download = 0;
    media.resolver = nullptr;
    ok(ailo_cache_new(&cache_config, &cache_raw, &error), error);
    cache.reset(cache_raw);
    config.cache = cache.get();
    ok(ailo_client_new_with_logger(&config, logger.get(), &raw, &error), error);
    client.reset(raw);
    std::puts("Reopened offline");
    std::fflush(stdout);
    ok(raw->snapshot_import(raw, "media", nullptr, &error), error);
    std::puts("Imported encrypted media checkpoint");
    std::fflush(stdout);
    observations offline;
    ok(raw->generate(raw, &request, observations::event, &offline, &run,
                     &error),
       error);
    CHECK(offline.text == first.text && run.usage.cached_tokens.lo > 0);
    std::puts("Offline generation reused checkpoint");
    std::fflush(stdout);
    if (vision) {
      wire =
          "{\"previous_response_id\":\"" + previous +
          "\",\"input\":\"What color was that image? Answer with one color "
          "word.\",\"stream\":true,\"max_output_tokens\":16,\"temperature\":0}";
      observations continuation;
      ok(raw->responses(raw, wire.data(), wire.size(), observations::wire,
                        &continuation, &api_info, &error),
         error);
      CHECK(lower(continuation.text).find("red") != std::string::npos);
      observations conversation_created;
      const std::string conversation_json =
          "{\"items\":[{\"role\":\"user\",\"content\":[{\"type\":\"input_"
          "image\",\"image_url\":\"data:image/png;base64," +
          b64 + "\"}]}]}";
      ok(raw->conversation_create(raw, conversation_json.data(),
                                  conversation_json.size(), observations::wire,
                                  &conversation_created, &api_info, &error),
         error);
      const auto at = conversation_created.text.find("\"id\":\"");
      CHECK(at != std::string::npos);
      const auto end = conversation_created.text.find('"', at + 6);
      CHECK(end != std::string::npos);
      const auto id = conversation_created.text.substr(at + 6, end - at - 6);
      wire =
          "{\"conversation\":\"" + id +
          "\",\"input\":\"What is the main color? Answer with one color "
          "word.\",\"stream\":true,\"max_output_tokens\":16,\"temperature\":0}";
      observations conversation_answer;
      ok(raw->responses(raw, wire.data(), wire.size(), observations::wire,
                        &conversation_answer, &api_info, &error),
         error);
      CHECK(lower(conversation_answer.text).find("red") != std::string::npos);
    }
    char *card = nullptr;
    ok(raw->model_card(raw, &card, &error), error);
    CHECK(std::string(card).find(projector_hash[0]) != std::string::npos);
    ailo_string_free(card);
    std::puts(
        "Qualified media semantics, immediate deltas, cancellation/recovery, "
        "cache identity, encrypted snapshots and offline replay");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
