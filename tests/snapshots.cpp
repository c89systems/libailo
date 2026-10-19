#include "ailo_snapshot_internal.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sys/resource.h>
#include <sys/stat.h>
#include <thread>
using namespace ailo::client;
namespace fs = std::filesystem;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c))                                                                  \
      throw std::runtime_error(std::string(#c) + " at " +                      \
                               std::to_string(__LINE__));                      \
  } while (0)
using client_owner =
    std::unique_ptr<ailo_client, decltype(&ailo_client_destroy)>;
using cache_owner = std::unique_ptr<ailo_cache, decltype(&ailo_cache_destroy)>;
ailo_error error{};
cache_owner new_cache(const ailo_cache_config &config) {
  ailo_cache *cache = nullptr;
  auto status = ailo_cache_new(&config, &cache, &error);
  if (status != AILO_OK)
    std::fprintf(stderr, "%s\n", error.message);
  CHECK(status == AILO_OK);
  return {cache, ailo_cache_destroy};
}
client_owner new_client(ailo_client_config config, ailo_cache *cache) {
  config.cache = cache;
  ailo_client *client = nullptr;
  CHECK(ailo_client_new(&config, &client, &error) == AILO_OK);
  return {client, ailo_client_destroy};
}
int observe(void *, const ailo_event *) { return 1; }
int reject_end(void *, const ailo_event *event) {
  return event->kind == AILO_EVENT_RUN_END ? -1 : 1;
}
std::vector<unsigned char> read(const fs::path &path) {
  std::ifstream stream(path, std::ios::binary);
  CHECK(stream);
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}
void write(const fs::path &path, const std::vector<unsigned char> &bytes) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  CHECK(stream);
  stream.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  CHECK(stream.good());
}
std::vector<unsigned char> native_state(ailo_client *client) {
  auto &self = *static_cast<state *>(client->state);
  const auto size = llama_state_seq_get_size_ext(self.context.get(), 0, 0);
  std::vector<unsigned char> bytes(size);
  CHECK(llama_state_seq_get_data_ext(self.context.get(), bytes.data(),
                                     bytes.size(), 0, 0) == size);
  return bytes;
}
static size_t peak_rss_kib() {
  struct rusage usage{};
  CHECK(getrusage(RUSAGE_SELF, &usage) == 0 && usage.ru_maxrss >= 0);
  return static_cast<size_t>(usage.ru_maxrss);
}
static void storage_memory(ailo_cache_config config) {
  // Exercise encrypted storage itself before any model/context is created.
  // The input is a byte fixture, not a claim of native inference-state
  // validity.
  constexpr size_t payload = 16 * 1024 * 1024;
  config.namespace_id = "snapshot-memory";
  config.max_snapshot_bytes = payload + 4 * sizeof(llama_token) + 80;
  config.max_disk_bytes = 2 * config.max_snapshot_bytes + 512;
  cache_state owner(config);
  cache_entry entry;
  entry.identity = "resource-fixture";
  entry.partition = "memory";
  entry.tokens = {1, 2, 3, 4};
  entry.positions = entry.evaluated_tokens = entry.tokens.size();
  entry.bytes.assign(payload, 42);
  const auto name =
      ".ailo-kv-s-" + ailo::model::digest_hex("memory", 6) + ".bin";
  const auto baseline = peak_rss_kib();
  CHECK(owner.disk->write(entry, name));
  const auto exported = peak_rss_kib();
  CHECK(exported <= baseline + 8 * 1024);
  auto restored = owner.disk->load(name, entry.identity, entry.partition);
  CHECK(restored->bytes == entry.bytes && restored->tokens == entry.tokens);
  const auto imported = peak_rss_kib();
  CHECK(imported <= exported + payload / 1024 + 8 * 1024);
  std::printf("Model-free 16 MiB snapshot RSS input/export/import: %zu/%zu/%zu "
              "KiB (export slack 8192 KiB; import one payload + slack)\n",
              baseline, exported, imported);
}
int main(int argc, char **argv) {
  try {
    CHECK(argc == 2);
    std::string directory =
        (fs::path(argv[1]).parent_path() / "snapshot-test-XXXXXX").string();
    CHECK(mkdtemp(directory.data()));
    struct cleanup {
      fs::path path;
      ~cleanup() {
        std::error_code e;
        fs::remove_all(path, e);
      }
    } temporary{directory};
    std::array<unsigned char, 32> key{};
    key.fill(42);
    auto config = ailo_cache_default_config();
    config.namespace_id = "snapshot-tenant";
    config.directory = directory.c_str();
    config.max_disk_bytes = 8 * 1024 * 1024;
    config.disk_ttl_seconds = 60;
    config.max_snapshot_bytes = 1024 * 1024;
    config.key_id = "key-one";
    config.key = key.data();
    config.key_size = key.size();
    storage_memory(config);
    auto cache = new_cache(config);
    const auto scoped =
        fs::path(directory) /
        ailo::model::digest_hex(config.namespace_id,
                                std::strlen(config.namespace_id)) /
        ailo::model::digest_hex(config.key_id, std::strlen(config.key_id));
    const auto slot = scoped / (".ailo-kv-s-" +
                                ailo::model::digest_hex("chosen", 6) + ".bin");
    ailo_client_config client_config{};
    client_config.source.local_path = argv[1];
    client_config.backend = AILO_BACKEND_CPU;
    client_config.context_tokens = 256;
    client_config.chat_template =
        "{% for m in messages %}{{ m.content }}{% endfor %}";
    auto client = new_client(client_config, cache.get());
    ailo_content content{AILO_CONTENT_TEXT, "abcdefghijk", 11, nullptr};
    ailo_item item{};
    item.content = &content;
    item.content_count = 1;
    auto request = ailo_request_default();
    request.items = &item;
    request.item_count = 1;
    request.max_tokens = 3;
    request.temperature = 0;
    ailo_run_info info{};
    auto run = [&](ailo_client *receiver) {
      auto status = receiver->generate(receiver, &request, observe, nullptr,
                                       &info, &error);
      if (status != AILO_OK)
        std::fprintf(stderr, "%s\n", error.message);
      CHECK(status == AILO_OK);
    };
    CHECK(client->snapshot_export(client.get(), "chosen", &error) ==
          AILO_INVALID_ARGUMENT);
    run(client.get());
    CHECK(!info.usage.cached_tokens.lo);
    CHECK(client->snapshot_export(client.get(), "chosen", &error) == AILO_OK);
    const auto first = read(slot);
    CHECK(first.size() > 256);
    CHECK(client->snapshot_export(client.get(), "chosen", &error) == AILO_OK);
    const auto baseline = read(slot);
    CHECK(baseline != first);
    struct stat permissions{};
    CHECK(!stat(slot.c_str(), &permissions) &&
          (permissions.st_mode & 0777) == 0600);
    CHECK(!stat(scoped.c_str(), &permissions) &&
          (permissions.st_mode & 0777) == 0700);
    {
      auto owner = retain_cache(cache.get());
      cache_guard guard(owner->active);
      CHECK(guard);
      const auto &plain = owner->entries.front();
      CHECK(std::search(baseline.begin(), baseline.end(), plain.bytes.begin(),
                        plain.bytes.begin() + 32) == baseline.end());
      const auto *raw =
          reinterpret_cast<const unsigned char *>(plain.tokens.data());
      CHECK(std::search(baseline.begin(), baseline.end(), raw,
                        raw + plain.tokens.size() * sizeof(llama_token)) ==
            baseline.end());
    }
    client.reset();
    cache.reset();
    cache = new_cache(config);
    client = new_client(client_config, cache.get());
    run(client.get());
    CHECK(info.usage.cached_tokens.lo == info.usage.prompt_tokens.lo - 1);
    ailo_cache_stats stats{};
    CHECK(cache->stats(cache.get(), &stats, &error) == AILO_OK &&
          stats.snapshot_reads.lo > 0);
    auto &self = *static_cast<state *>(client->state);
    // A sentinel active sequence proves failed/valid import leaves native state
    // intact, not just that the context pointer remains unchanged.
    llama_token sentinel = 100;
    auto batch = llama_batch_get_one(&sentinel, 1);
    CHECK(llama_decode(self.context.get(), batch) == 0);
    const auto before = native_state(client.get());
    auto *context = self.context.get();
    for (size_t offset :
         {size_t(0), size_t(24), size_t(40), size_t(200), size_t(224),
          size_t(240), size_t(256), baseline.size() - 1}) {
      auto changed = baseline;
      changed[offset] ^= 1;
      write(slot, changed);
      CHECK(client->snapshot_import(client.get(), "chosen", nullptr, &error) !=
            AILO_OK);
      CHECK(self.context.get() == context &&
            native_state(client.get()) == before);
    }
    for (size_t size : {size_t(7), size_t(255), baseline.size() - 1}) {
      write(slot, {baseline.begin(),
                   baseline.begin() + static_cast<ptrdiff_t>(size)});
      CHECK(client->snapshot_import(client.get(), "chosen", nullptr, &error) !=
            AILO_OK);
      CHECK(native_state(client.get()) == before);
    }
    write(slot, baseline);
    CHECK(client->snapshot_import(client.get(), "chosen", "wrong-partition",
                                  &error) == AILO_INVALID_ARGUMENT);
    CHECK(client->snapshot_import(client.get(), "chosen", nullptr, &error) ==
          AILO_OK);
    CHECK(self.context.get() == context &&
          native_state(client.get()) == before);
    CHECK(client->snapshot_import(client.get(), "chosen", nullptr, &error) ==
          AILO_OK);
    CHECK(cache->stats(cache.get(), &stats, &error) == AILO_OK &&
          stats.entries == 1);
    // Slot names are authenticated; valid ciphertext cannot be
    // renamed/replayed.
    const auto other_slot =
        scoped /
        (".ailo-kv-s-" + ailo::model::digest_hex("swapped", 7) + ".bin");
    fs::copy_file(slot, other_slot);
    fs::permissions(other_slot, fs::perms::owner_read | fs::perms::owner_write);
    CHECK(client->snapshot_import(client.get(), "swapped", nullptr, &error) ==
          AILO_INVALID_ARGUMENT);
    fs::remove(other_slot);
    auto wrong_key = key;
    wrong_key[0] ^= 1;
    auto bad_config = config;
    bad_config.key = wrong_key.data();
    auto bad_cache = new_cache(bad_config);
    auto bad_client = new_client(client_config, bad_cache.get());
    CHECK(bad_client->snapshot_import(bad_client.get(), "chosen", nullptr,
                                      &error) == AILO_INVALID_ARGUMENT);
    // Copied ciphertext cannot cross namespace or key-ID domains, even when
    // the caller supplies the same root key.
    for (bool rotate : {false, true}) {
      auto separated_config = config;
      if (rotate)
        separated_config.key_id = "key-two";
      else
        separated_config.namespace_id = "another-tenant";
      auto separated_cache = new_cache(separated_config);
      auto separated_client = new_client(client_config, separated_cache.get());
      const auto separated_slot =
          fs::path(directory) /
          ailo::model::digest_hex(separated_config.namespace_id,
                                  std::strlen(separated_config.namespace_id)) /
          ailo::model::digest_hex(separated_config.key_id,
                                  std::strlen(separated_config.key_id)) /
          slot.filename();
      fs::copy_file(slot, separated_slot);
      fs::permissions(separated_slot,
                      fs::perms::owner_read | fs::perms::owner_write);
      CHECK(separated_client->snapshot_import(separated_client.get(), "chosen",
                                              nullptr,
                                              &error) == AILO_INVALID_ARGUMENT);
    }
    {
      auto owner = retain_cache(cache.get());
      auto disk_lock = owner->disk->lock();
      CHECK(client->snapshot_export(client.get(), "busy", &error) == AILO_BUSY);
      run(client.get());
      CHECK(info.usage.cached_tokens.lo == info.usage.prompt_tokens.lo - 1);
    }
    auto wrong_context = client_config;
    wrong_context.context_tokens = 512;
    auto incompatible = new_client(wrong_context, cache.get());
    CHECK(incompatible->snapshot_import(incompatible.get(), "chosen", nullptr,
                                        &error) == AILO_INVALID_ARGUMENT);
    // Automatic corrupt state must be evicted and recomputed, not fatal.
    client.reset();
    cache.reset();
    fs::path automatic;
    for (const auto &entry : fs::directory_iterator(scoped))
      if (entry.path().filename().string().find(".ailo-kv-a-") == 0)
        automatic = entry.path();
    CHECK(!automatic.empty());
    auto broken = read(automatic);
    broken.back() ^= 1;
    write(automatic, broken);
    cache = new_cache(config);
    client = new_client(client_config, cache.get());
    run(client.get());
    CHECK(!info.usage.cached_tokens.lo);
    CHECK(cache->stats(cache.get(), &stats, &error) == AILO_OK &&
          stats.snapshot_errors.lo > 0);
    client.reset();
    cache.reset();
    cache = new_cache(config);
    client = new_client(client_config, cache.get());
    run(client.get());
    CHECK(info.usage.cached_tokens.lo > 0);
    // Disk-only owners support import and reuse without storing a RAM entry.
    auto disk_only_config = config;
    disk_only_config.max_memory_bytes = 0;
    auto disk_only_cache = new_cache(disk_only_config);
    auto disk_only = new_client(client_config, disk_only_cache.get());
    CHECK(disk_only->snapshot_import(disk_only.get(), "chosen", nullptr,
                                     &error) == AILO_OK);
    run(disk_only.get());
    CHECK(info.usage.cached_tokens.lo > 0);
    CHECK(disk_only_cache->stats(disk_only_cache.get(), &stats, &error) ==
              AILO_OK &&
          !stats.entries && stats.disk_entries);
    // Materialization limit rejects before native restore or a huge allocation.
    auto small_config = config;
    small_config.max_snapshot_bytes = 16;
    auto small_cache = new_cache(small_config);
    auto small = new_client(client_config, small_cache.get());
    CHECK(small->snapshot_import(small.get(), "chosen", nullptr, &error) ==
          AILO_OUT_OF_RANGE);
    auto disabled_config = ailo_cache_default_config();
    disabled_config.namespace_id = "ram-only";
    auto disabled_cache = new_cache(disabled_config);
    auto disabled = new_client(client_config, disabled_cache.get());
    CHECK(disabled->snapshot_import(disabled.get(), "chosen", nullptr,
                                    &error) == AILO_UNSUPPORTED);
    auto invalid_config = config;
    invalid_config.key_size = 31;
    ailo_cache *invalid = nullptr;
    CHECK(ailo_cache_new(&invalid_config, &invalid, &error) ==
              AILO_INVALID_ARGUMENT &&
          !invalid);
    // Never follow an explicit slot symlink or alter its target.
    const auto outside = fs::path(directory) / "untouched";
    write(outside, {1, 2, 3});
    fs::remove(slot);
    fs::create_symlink(outside, slot);
    CHECK(client->snapshot_import(client.get(), "chosen", nullptr, &error) ==
          AILO_IO_FAILED);
    CHECK(read(outside) == std::vector<unsigned char>({1, 2, 3}));
    fs::remove(slot);
    write(slot, baseline);
    fs::permissions(slot, fs::perms::owner_read | fs::perms::owner_write);
    // A longer disk prefix wins over the sole, shorter RAM entry.
    auto prefix_config = config;
    prefix_config.key_id = "prefix-key";
    prefix_config.max_entries = 1;
    auto prefix_cache = new_cache(prefix_config);
    auto prefix_client = new_client(client_config, prefix_cache.get());
    run(prefix_client.get());
    content.data = "abcdXXXXXXX";
    run(prefix_client.get());
    CHECK(info.usage.cached_tokens.lo < info.usage.prompt_tokens.lo - 1);
    content.data = "abcdefghijk";
    run(prefix_client.get());
    CHECK(info.usage.cached_tokens.lo == info.usage.prompt_tokens.lo - 1);
    // Isolated quota/expiry domain; bounded file count includes named slots.
    auto quota_config = config;
    quota_config.key_id = "quota-key";
    quota_config.max_disk_entries = 1;
    auto quota_cache = new_cache(quota_config);
    auto quota_client = new_client(client_config, quota_cache.get());
    run(quota_client.get());
    content.data = "abcdXXXXXXX";
    CHECK(quota_client->generate(quota_client.get(), &request, reject_end,
                                 nullptr, &info, &error) == AILO_SINK_FAILED);
    content.data = "abcdefghijk";
    CHECK(quota_client->snapshot_export(quota_client.get(), "one", &error) ==
          AILO_OK);
    CHECK(quota_cache->stats(quota_cache.get(), &stats, &error) == AILO_OK &&
          stats.disk_entries == 1 && stats.disk_evictions.lo > 0);
    quota_client.reset();
    quota_cache.reset();
    quota_cache = new_cache(quota_config);
    quota_client = new_client(client_config, quota_cache.get());
    CHECK(quota_client->snapshot_import(quota_client.get(), "one", nullptr,
                                        &error) == AILO_OK);
    run(quota_client.get());
    CHECK(info.usage.cached_tokens.lo == info.usage.prompt_tokens.lo - 1);
    auto expiry_config = config;
    expiry_config.key_id = "expiring-key";
    expiry_config.disk_ttl_seconds = 2;
    expiry_config.idle_ttl_seconds = 1;
    auto expiry_cache = new_cache(expiry_config);
    auto expiry_client = new_client(client_config, expiry_cache.get());
    run(expiry_client.get());
    CHECK(expiry_client->snapshot_export(expiry_client.get(), "expired",
                                         &error) == AILO_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(2100));
    CHECK(expiry_client->snapshot_export(expiry_client.get(), "revived",
                                         &error) != AILO_OK);
    CHECK(expiry_client->snapshot_import(expiry_client.get(), "expired",
                                         nullptr,
                                         &error) == AILO_INVALID_ARGUMENT);
    CHECK(expiry_cache->prune(expiry_cache.get(), &error) == AILO_OK);
    CHECK(expiry_cache->stats(expiry_cache.get(), &stats, &error) == AILO_OK &&
          !stats.disk_entries);
    std::puts("Encrypted snapshot restart/import, authentication, bounds, "
              "quota, expiry and filesystem failures passed");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "snapshots: %s\n", e.what());
    return 1;
  }
}
