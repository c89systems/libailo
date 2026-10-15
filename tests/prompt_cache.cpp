#include "ailo_cache_internal.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <thread>
using namespace ailo::client;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c))                                                                  \
      throw std::runtime_error(std::string(#c) + " at " +                      \
                               std::to_string(__LINE__));                      \
  } while (0)
struct observation {
  std::string text;
  unsigned terminals = 0;
  int action = 1;
};
int observe(void *data, const ailo_event *event) {
  auto &seen = *static_cast<observation *>(data);
  if (event->kind == AILO_EVENT_TEXT_DELTA) {
    seen.text.append(static_cast<const char *>(event->data), event->size);
    return seen.action;
  }
  if (event->kind == AILO_EVENT_RUN_END)
    ++seen.terminals;
  return 1;
}
using client_owner =
    std::unique_ptr<ailo_client, decltype(&ailo_client_destroy)>;
using cache_owner = std::unique_ptr<ailo_cache, decltype(&ailo_cache_destroy)>;
ailo_error error{};
client_owner make_client(const ailo_client_config &config) {
  ailo_client *client = nullptr;
  CHECK(ailo_client_new(&config, &client, &error) == AILO_OK);
  return {client, ailo_client_destroy};
}
cache_owner make_cache(ailo_cache_config config) {
  ailo_cache *cache = nullptr;
  CHECK(ailo_cache_new(&config, &cache, &error) == AILO_OK);
  return {cache, ailo_cache_destroy};
}
int main(int argc, char **argv) {
  try {
    CHECK(argc == 2);
    auto options = ailo_cache_default_config();
    options.namespace_id = "tenant-one";
    auto cache = make_cache(options);
    ailo_client_config config{};
    config.source.local_path = argv[1];
    config.backend = AILO_BACKEND_CPU;
    config.context_tokens = 256;
    config.chat_template = "{% for m in messages %}{{ m.content }}{% endfor %}";
    config.cache = cache.get();
    auto client = make_client(config);
    const std::string original = "abcdefghijk", changed = "abcdefghXX";
    ailo_content content{AILO_CONTENT_TEXT, original.data(), original.size()};
    ailo_item item{};
    item.content = &content;
    item.content_count = 1;
    auto request = ailo_request_default();
    request.items = &item;
    request.item_count = 1;
    request.max_tokens = 3;
    request.temperature = 0;
    ailo_run_info info{};
    observation seen;
    auto run = [&](ailo_client *receiver) {
      auto status =
          receiver->generate(receiver, &request, observe, &seen, &info, &error);
      if (status != AILO_OK && seen.action == 1)
        std::fprintf(stderr, "%s\n", error.message);
      return status;
    };
    auto stats = [&]() {
      ailo_cache_stats value{};
      CHECK(client->cache_stats(client.get(), &value, &error) == AILO_OK);
      return value;
    };
    CHECK(run(client.get()) == AILO_OK && !info.usage.cached_tokens.lo);
    const auto cold = seen.text;
    CHECK(!cold.empty() && stats().misses.lo == 1 && stats().entries == 1);
    seen = {};
    CHECK(run(client.get()) == AILO_OK);
    CHECK(seen.text == cold &&
          info.usage.cached_tokens.lo == info.usage.prompt_tokens.lo - 1);
    CHECK(stats().hits.lo == 1 &&
          stats().memory_bytes <= options.max_memory_bytes);
    CHECK(stats().evaluated_tokens.lo == info.usage.prompt_tokens.lo + 1 &&
          stats().cached_tokens.lo == info.usage.prompt_tokens.lo - 1);
    auto other = make_client(config);
    seen = {};
    CHECK(run(other.get()) == AILO_OK);
    CHECK(seen.text == cold && info.usage.cached_tokens.lo);
    // Same owner, explicit request partition, independent KV state.
    request.prompt_cache_key = "other-partition";
    seen = {};
    CHECK(run(client.get()) == AILO_OK);
    CHECK(!info.usage.cached_tokens.lo);
    request.prompt_cache_key = nullptr;
    content.data = changed.data();
    content.size = changed.size();
    seen = {};
    CHECK(run(client.get()) == AILO_OK && info.usage.cached_tokens.lo > 0 &&
          info.usage.cached_tokens.lo < info.usage.prompt_tokens.lo - 1);
    config.cache = nullptr;
    config.disable_prompt_cache = 1;
    auto uncached = make_client(config);
    observation warm = seen;
    seen = {};
    CHECK(run(uncached.get()) == AILO_OK && seen.text == warm.text &&
          !info.usage.cached_tokens.lo);
    // Config identity differs even with matching weights/template/tokens.
    config.disable_prompt_cache = 0;
    config.cache = cache.get();
    config.context_tokens = 512;
    auto different_context = make_client(config);
    seen = {};
    CHECK(run(different_context.get()) == AILO_OK &&
          !info.usage.cached_tokens.lo);
    config.context_tokens = 256;
    // Same path, size and mtime, different bytes: identity must hash content.
    {
      namespace fs = std::filesystem;
      std::string directory =
          (fs::path(argv[1]).parent_path() / "cache-identity-XXXXXX").string();
      CHECK(mkdtemp(directory.data()));
      struct cleanup {
        fs::path path;
        ~cleanup() {
          std::error_code error;
          fs::remove_all(path, error);
        }
      } temporary{directory};
      const auto copy = fs::path(directory) / "model.gguf";
      fs::copy_file(argv[1], copy);
      const auto modified = fs::last_write_time(copy);
      auto same_path = config;
      const std::string filename = copy.string();
      same_path.source.local_path = filename.c_str();
      auto first = make_client(same_path);
      seen = {};
      CHECK(run(first.get()) == AILO_OK);
      first.reset();
      CHECK(fs::file_size(copy) ==
            fs::file_size(std::string(argv[1]) + ".eos"));
      fs::copy_file(std::string(argv[1]) + ".eos", copy,
                    fs::copy_options::overwrite_existing);
      fs::last_write_time(copy, modified);
      auto replacement = make_client(same_path);
      seen = {};
      CHECK(run(replacement.get()) == AILO_OK && !info.usage.cached_tokens.lo);
    }
    // Malformed native data cannot alter the live context; fallback recomputes.
    CHECK(cache->clear(cache.get(), &error) == AILO_OK);
    seen = {};
    CHECK(run(client.get()) == AILO_OK);
    {
      auto state = retain_cache(cache.get());
      cache_guard lock(state->active);
      CHECK(lock);
      CHECK(state->entries.size() == 1);
      state->entries.front().bytes[0] ^= 0xff;
    }
    seen = {};
    CHECK(run(client.get()) == AILO_OK && !info.usage.cached_tokens.lo &&
          seen.text == warm.text);
    CHECK(stats().invalid.lo == 1);
    for (int action : {0, -1}) {
      seen = {};
      seen.action = action;
      CHECK(run(client.get()) == (action ? AILO_SINK_FAILED : AILO_CANCELLED));
      CHECK(!seen.terminals);
    }
    seen = {};
    CHECK(run(client.get()) == AILO_OK && seen.text == warm.text);
    // A busy shared owner is bypassed, never deadlocked or mistaken for a hit.
    {
      auto state = retain_cache(cache.get());
      cache_guard lock(state->active);
      CHECK(lock);
      ailo_cache_stats busy{};
      CHECK(cache->stats(cache.get(), &busy, &error) == AILO_BUSY);
      seen = {};
      CHECK(run(client.get()) == AILO_OK && !info.usage.cached_tokens.lo);
    }
    CHECK(stats().busy_skips.lo >= 1);
    cache.reset(); // Client-retained owner survives shell destruction.
    seen = {};
    CHECK(run(client.get()) == AILO_OK && info.usage.cached_tokens.lo);
    CHECK(client->cache_clear(client.get(), &error) == AILO_OK &&
          stats().entries == 0);

    options.max_memory_bytes = 32;
    auto tiny = make_cache(options);
    config.cache = tiny.get();
    auto small = make_client(config);
    seen = {};
    CHECK(run(small.get()) == AILO_OK);
    ailo_cache_stats output{};
    CHECK(tiny->stats(tiny.get(), &output, &error) == AILO_OK);
    CHECK(output.entries == 0 && output.oversized.lo == 1 &&
          !output.memory_bytes);
    options = ailo_cache_default_config();
    options.namespace_id = "short-lived";
    options.idle_ttl_seconds = 1;
    options.max_entries = 1;
    auto short_lived = make_cache(options);
    config.cache = short_lived.get();
    auto expiring = make_client(config);
    seen = {};
    CHECK(run(expiring.get()) == AILO_OK);
    request.prompt_cache_key = "new";
    seen = {};
    CHECK(run(expiring.get()) == AILO_OK);
    CHECK(short_lived->stats(short_lived.get(), &output, &error) == AILO_OK &&
          output.entries == 1 && output.evictions.lo == 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    CHECK(short_lived->prune(short_lived.get(), &error) == AILO_OK);
    CHECK(short_lived->stats(short_lived.get(), &output, &error) == AILO_OK &&
          output.entries == 0 && output.evictions.lo == 2);
    seen = {};
    CHECK(run(expiring.get()) == AILO_OK && !info.usage.cached_tokens.lo);
    options.namespace_id = nullptr;
    ailo_cache *invalid = reinterpret_cast<ailo_cache *>(1);
    CHECK(ailo_cache_new(&options, &invalid, &error) == AILO_INVALID_ARGUMENT &&
          !invalid);
    std::puts("Prompt prefix reuse, identity, ownership, bounds, expiry and "
              "failure recovery passed");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "prompt cache: %s\n", e.what());
    return 1;
  }
}
