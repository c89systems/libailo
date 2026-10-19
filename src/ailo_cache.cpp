#include "ailo_cache_internal.hpp"
#include "ailo_logging_internal.hpp"
#include "ailo_snapshot_internal.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <openssl/crypto.h>

#define AILO_PUBLIC extern "C" __attribute__((visibility("default")))
namespace ailo {
namespace client {
namespace {
struct timing {
  std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();
  std::atomic<uint64_t> &destination;
  ~timing() { destination.fetch_add(elapsed_us(start)); }
};
} // namespace
cache_guard::cache_guard(std::atomic<bool> &flag, bool required)
    : active(flag) {
  bool expected = false;
  owned = active.compare_exchange_strong(
      expected, true, std::memory_order_acquire, std::memory_order_relaxed);
  if (!owned && required)
    throw failure(AILO_BUSY, "Prompt cache is active");
}
cache_guard::~cache_guard() {
  if (owned)
    active.store(false, std::memory_order_release);
}
uint64_t elapsed_us(std::chrono::steady_clock::time_point since) {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - since)
          .count());
}
std::string cache_partition(const char *source) {
  if (!source)
    return {};
  const size_t size = strnlen(source, 257);
  if (size > 256)
    throw failure(AILO_OUT_OF_RANGE, "Cache namespace/key exceeds 256 bytes");
  require_utf8(source, size);
  return {source, size};
}
size_t cache_entry::charge() const {
  return sizeof(*this) + 2 * sizeof(void *) + identity.capacity() +
         partition.capacity() + media_key.capacity() +
         tokens.capacity() * sizeof(llama_token) + bytes.capacity();
}
cache_entry::~cache_entry() {
  if (!bytes.empty())
    OPENSSL_cleanse(bytes.data(), bytes.size());
  if (!tokens.empty())
    OPENSSL_cleanse(tokens.data(), tokens.size() * sizeof(llama_token));
}
cache_state::cache_state(const ailo_cache_config &input)
    : config(input), namespace_id(cache_partition(input.namespace_id)) {
  if (namespace_id.empty())
    throw failure(AILO_INVALID_ARGUMENT,
                  "An explicit trusted cache namespace is required");
  if (config.max_entries > 4096)
    throw failure(AILO_OUT_OF_RANGE,
                  "Prompt cache supports at most 4096 entries");
  config.namespace_id = namespace_id.c_str();
  if (input.directory || input.max_disk_bytes || input.disk_ttl_seconds ||
      input.key || input.key_id || input.key_size)
    disk = std::make_unique<snapshot_store>(*this, input);
  config.directory = disk ? disk->root.c_str() : nullptr;
  config.key_id = disk ? disk->key_id.c_str() : nullptr;
  config.key = nullptr;
  config.key_size = 0;
}
cache_state::~cache_state() = default;
bool cache_state::enabled() const {
  return config.max_memory_bytes && config.max_entries &&
         config.idle_ttl_seconds;
}
void cache_state::erase(std::list<cache_entry>::iterator item) {
  bytes -= item->charge();
  entries.erase(item);
  evictions.fetch_add(1);
}
void cache_state::prune_locked() {
  const auto now = std::chrono::steady_clock::now();
  for (auto it = entries.begin(); it != entries.end();) {
    auto current = it++;
    if (now - current->used >= std::chrono::seconds(config.idle_ttl_seconds))
      erase(current);
  }
}
void cache_state::stats(ailo_cache_stats &output) {
  cache_guard lock(active, true);
  prune_locked();
  output = {};
  if (disk) {
    auto sizes = disk->prune();
    output.disk_entries = sizes.first;
    output.disk_bytes = sizes.second;
  }
#define COUNT(name) output.name = detail::from_unsigned(name.load())
  COUNT(hits);
  COUNT(misses);
  COUNT(evictions);
  COUNT(oversized);
  COUNT(invalid);
  COUNT(busy_skips);
  COUNT(cached_tokens);
  COUNT(evaluated_tokens);
  COUNT(lookup_microseconds);
  COUNT(capture_microseconds);
  COUNT(prefill_microseconds);
  COUNT(snapshot_reads);
  COUNT(snapshot_writes);
  COUNT(snapshot_errors);
  COUNT(snapshot_skipped);
  COUNT(disk_evictions);
#undef COUNT
  output.entries = entries.size();
  output.memory_bytes = bytes;
}
void cache_state::clear() {
  cache_guard lock(active, true);
  while (!entries.empty())
    erase(entries.begin());
  if (disk)
    (void)disk->prune(true);
}
void cache_state::prune() {
  cache_guard lock(active, true);
  prune_locked();
  if (disk)
    (void)disk->prune();
}

size_t matching_prefix(const cache_entry &entry,
                       const std::vector<llama_token> &tokens, bool trim) {
  if (tokens.size() < 2)
    return 0;
  const size_t end = std::min(entry.tokens.size(), tokens.size() - 1);
  size_t prefix = 0;
  while (prefix < end && entry.tokens[prefix] == tokens[prefix])
    ++prefix;
  return trim || prefix == entry.tokens.size() ? prefix : 0;
}
std::unique_ptr<llama_context, decltype(&llama_free)>
restore_checkpoint(state &client, const cache_entry &entry, size_t count,
                   bool *allocation_failed) {
  if (allocation_failed)
    *allocation_failed = false;
  if (client.cancelled.load())
    throw failure(AILO_CANCELLED, "Snapshot restore cancelled");
  std::unique_ptr<llama_context, decltype(&llama_free)> empty(nullptr,
                                                              llama_free);
  if (!count || count > entry.tokens.size() ||
      entry.positions >= llama_n_ctx(client.context.get()))
    return empty;
  const auto vocab = llama_model_get_vocab(client.model.get());
  for (auto token : entry.tokens)
    if (token < 0 || token >= llama_vocab_n_tokens(vocab))
      return empty;
  const bool media = !entry.media_key.empty();
  if (media && count != entry.tokens.size())
    return empty;
  const size_t positions = media ? entry.positions : count;
  auto candidate = client.new_context();
  if (!candidate) {
    if (allocation_failed)
      *allocation_failed = true;
    return empty;
  }
  bool valid = llama_state_seq_set_data_ext(
                   candidate.get(), entry.bytes.data(), entry.bytes.size(), 0,
                   LLAMA_STATE_SEQ_FLAGS_NONE) == entry.bytes.size();
  if (valid && count < entry.tokens.size())
    valid = llama_memory_seq_rm(llama_get_memory(candidate.get()), 0,
                                static_cast<llama_pos>(count), -1);
  if (valid)
    valid = llama_memory_seq_pos_max(llama_get_memory(candidate.get()), 0) ==
            static_cast<llama_pos>(positions - 1);
  const bool trim = client.active_media_key.empty() &&
                    !llama_model_is_recurrent(client.model.get()) &&
                    !llama_model_is_hybrid(client.model.get()) &&
                    llama_model_n_swa(client.model.get()) == 0;
  if (valid && trim)
    valid = llama_memory_seq_pos_min(llama_get_memory(candidate.get()), 0) == 0;
  if (!valid)
    return empty;
  return candidate;
}

size_t cache_state::restore(state &client,
                            const std::vector<llama_token> &tokens,
                            const std::string &partition) {
  if (!available() || tokens.size() < 2)
    return 0;
  timing timer{{std::chrono::steady_clock::now()}, lookup_microseconds};
  cache_guard lock(active);
  if (!lock) {
    busy_skips.fetch_add(1);
    misses.fetch_add(1);
    return 0;
  }
  prune_locked();
  const bool trim = client.active_media_key.empty() &&
                    !llama_model_is_recurrent(client.model.get()) &&
                    !llama_model_is_hybrid(client.model.get()) &&
                    llama_model_n_swa(client.model.get()) == 0;
  auto best = entries.end();
  size_t count = 0;
  for (auto it = entries.begin(); it != entries.end(); ++it) {
    if (it->identity != client.cache_identity || it->partition != partition ||
        it->media_key != client.active_media_key ||
        (!it->media_key.empty() && it->tokens.size() != tokens.size() - 1))
      continue;
    const size_t prefix = matching_prefix(*it, tokens, trim);
    if (prefix > count) {
      best = it;
      count = prefix;
    }
  }
  if (disk) {
    try {
      std::unique_ptr<cache_entry> loaded;
      const size_t reused =
          disk->restore(client, tokens, partition, count, trim, loaded);
      if (reused) {
        (void)admit(loaded);
        hits.fetch_add(1);
        cached_tokens.fetch_add(client.active_media_key.empty()
                                    ? reused
                                    : client.active_media_tokens - 1);
        return reused;
      }
    } catch (const failure &error) {
      if (error.status == AILO_CANCELLED)
        throw;
      snapshot_errors.fetch_add(1);
      if (error.status == AILO_BUSY)
        busy_skips.fetch_add(1);
      log_scope(client.logging.get(), log_phase::generate)
          .trace(
              "Persistent cache lookup unavailable; using RAM or recomputing");
    }
  }
  if (best == entries.end()) {
    misses.fetch_add(1);
    return 0;
  }
  // Native restore may partially modify its context on failure. Never restore
  // into the live context or adopt a candidate until every check has passed.
  bool allocation_failed = false;
  auto candidate = restore_checkpoint(client, *best, count, &allocation_failed);
  if (allocation_failed) {
    misses.fetch_add(1);
    return 0;
  }
  if (!candidate) {
    erase(best);
    invalid.fetch_add(1);
    misses.fetch_add(1);
    log_scope(client.logging.get(), log_phase::generate)
        .trace("Invalid prompt checkpoint evicted; recomputing");
    return 0;
  }
  client.context = std::move(candidate);
  best->used = std::chrono::steady_clock::now();
  entries.splice(entries.begin(), entries, best);
  hits.fetch_add(1);
  cached_tokens.fetch_add(best->media_key.empty() ? count
                                                  : best->evaluated_tokens);
  return count;
}

std::string cache_state::capture(state &client,
                                 const std::vector<llama_token> &tokens,
                                 size_t count, const std::string &partition) {
  if (!available() || !count)
    return {};
  timing timer{{std::chrono::steady_clock::now()}, capture_microseconds};
  cache_guard lock(active);
  if (!lock) {
    busy_skips.fetch_add(1);
    return {};
  }
  prune_locked();
  for (auto it = entries.begin(); it != entries.end(); ++it)
    if (it->identity == client.cache_identity && it->partition == partition &&
        it->tokens.size() == count &&
        it->media_key == client.active_media_key &&
        std::equal(it->tokens.begin(), it->tokens.end(), tokens.begin())) {
      it->used = std::chrono::steady_clock::now();
      entries.splice(entries.begin(), entries, it);
      const auto key = checkpoint_key(*it);
      if (disk) {
        try {
          disk->write(*it, ".ailo-kv-a-" + key + ".bin", &client);
        } catch (const failure &error) {
          if (error.status == AILO_CANCELLED)
            throw;
          snapshot_errors.fetch_add(1);
          snapshot_skipped.fetch_add(1);
        }
      }
      return key;
    }
  const size_t size = llama_state_seq_get_size_ext(client.context.get(), 0,
                                                   LLAMA_STATE_SEQ_FLAGS_NONE);
  if (!size) {
    invalid.fetch_add(1);
    return {};
  }
  // Bound the complete native state BEFORE allocating it. Evict first, so a
  // new cache buffer cannot temporarily double the configured resident bound.
  const size_t overhead = sizeof(cache_entry) + 2 * sizeof(void *) +
                          client.active_media_key.capacity() +
                          std::max(size_t(64), client.cache_identity.size()) +
                          std::max(size_t(256), partition.size());
  const bool fits =
      enabled() &&
      !(overhead > config.max_memory_bytes ||
        count > (config.max_memory_bytes - overhead) / sizeof(llama_token) ||
        size >
            config.max_memory_bytes - overhead - count * sizeof(llama_token));
  const bool fits_disk =
      disk && config.max_snapshot_bytes >= 80 &&
      count <= (config.max_snapshot_bytes - 80) / sizeof(llama_token) &&
      size <= config.max_snapshot_bytes - 80 - count * sizeof(llama_token);
  if (!fits && !fits_disk) {
    oversized.fetch_add(1);
    return {};
  }
  const size_t required = overhead + size + count * sizeof(llama_token);
  while (fits && !entries.empty() &&
         (entries.size() >= config.max_entries ||
          bytes > config.max_memory_bytes - required))
    erase(std::prev(entries.end()));
  auto owned = std::make_unique<cache_entry>();
  auto &entry = *owned;
  entry.identity = client.cache_identity;
  entry.partition = partition;
  entry.media_key = client.active_media_key;
  entry.positions =
      entry.media_key.empty() ? count : client.active_media_positions - 1;
  entry.evaluated_tokens =
      entry.media_key.empty() ? count : client.active_media_tokens - 1;
  entry.tokens.assign(tokens.begin(),
                      tokens.begin() + static_cast<ptrdiff_t>(count));
  entry.bytes.resize(size);
  if (llama_state_seq_get_data_ext(client.context.get(), entry.bytes.data(),
                                   size, 0,
                                   LLAMA_STATE_SEQ_FLAGS_NONE) != size) {
    invalid.fetch_add(1);
    return {};
  }
  entry.used = std::chrono::steady_clock::now();
  const auto key = checkpoint_key(entry);
  bool saved = false;
  if (disk) {
    try {
      saved = disk->write(entry, ".ailo-kv-a-" + key + ".bin", &client);
    } catch (const failure &error) {
      if (error.status == AILO_CANCELLED)
        throw;
      snapshot_errors.fetch_add(1);
      snapshot_skipped.fetch_add(1);
      log_scope(client.logging.get(), log_phase::generate)
          .trace("Automatic snapshot skipped; prompt inference continues");
    }
  }
  saved = admit(owned) || saved;
  return saved ? key : std::string{};
}
bool cache_state::admit(std::unique_ptr<cache_entry> &entry) {
  if (!enabled() || entry->charge() > config.max_memory_bytes)
    return false;
  for (auto it = entries.begin(); it != entries.end();) {
    auto current = it++;
    if (current->identity == entry->identity &&
        current->partition == entry->partition &&
        current->media_key == entry->media_key &&
        current->tokens == entry->tokens)
      erase(current);
  }
  while (!entries.empty() &&
         (entries.size() >= config.max_entries ||
          bytes > config.max_memory_bytes - entry->charge()))
    erase(std::prev(entries.end()));
  entry->used = std::chrono::steady_clock::now();
  const auto charge = entry->charge();
  entries.emplace_front(std::move(*entry));
  bytes += charge;
  entry.reset();
  return true;
}
void cache_state::snapshot_export(state &client, const char *slot) {
  cache_guard guard(active, true);
  prune_locked();
  if (!disk)
    throw failure(AILO_UNSUPPORTED, "Encrypted snapshots are not configured");
  const auto name = cache_partition(slot);
  if (name.empty())
    throw failure(AILO_INVALID_ARGUMENT, "Snapshot slot is required");
  if (client.last_checkpoint.empty())
    throw failure(AILO_INVALID_ARGUMENT,
                  "No successfully cached prompt is available");
  const auto target =
      ".ailo-kv-s-" + model::digest_hex(name.data(), name.size()) + ".bin";
  for (const auto &entry : entries)
    if (checkpoint_key(entry) == client.last_checkpoint) {
      disk->write(entry, target, &client);
      return;
    }
  auto entry = disk->load(".ailo-kv-a-" + client.last_checkpoint + ".bin",
                          client.cache_identity,
                          client.last_checkpoint_partition, &client);
  disk->write(*entry, target, &client);
}
void cache_state::snapshot_import(state &client, const char *slot,
                                  const char *key) {
  cache_guard guard(active, true);
  if (!disk)
    throw failure(AILO_UNSUPPORTED, "Encrypted snapshots are not configured");
  const auto name = cache_partition(slot), partition = cache_partition(key);
  if (name.empty())
    throw failure(AILO_INVALID_ARGUMENT, "Snapshot slot is required");
  auto entry = disk->load(
      ".ailo-kv-s-" + model::digest_hex(name.data(), name.size()) + ".bin",
      client.cache_identity, partition, &client);
  bool allocation_failed = false;
  auto candidate = restore_checkpoint(client, *entry, entry->tokens.size(),
                                      &allocation_failed);
  if (allocation_failed)
    throw failure(AILO_NO_MEMORY,
                  "Cannot allocate a disposable snapshot context");
  if (!candidate)
    throw failure(AILO_MODEL_FAILED,
                  "Snapshot native state cannot be restored");
  const auto id = checkpoint_key(*entry);
  // Explicit import validates without replacing the live context. Publish the
  // automatic entry as well so disk-only owners can reuse the imported state.
  disk->write(*entry, ".ailo-kv-a-" + id + ".bin", &client, true);
  (void)admit(entry);
  client.last_checkpoint = id;
  client.last_checkpoint_partition = partition;
}
std::shared_ptr<cache_state> retain_cache(ailo_cache *cache) {
  if (!cache || !cache->state)
    throw failure(AILO_INVALID_ARGUMENT, "A live prompt cache is required");
  return *static_cast<std::shared_ptr<cache_state> *>(cache->state);
}
} // namespace client
} // namespace ailo

AILO_PUBLIC ailo_cache_config ailo_cache_default_config(void) {
  ailo_cache_config value{};
  value.max_memory_bytes = 64 * 1024 * 1024;
  value.max_entries = 8;
  value.idle_ttl_seconds = 300;
  value.max_disk_entries = 8;
  value.max_snapshot_bytes = 64 * 1024 * 1024;
  return value;
}
AILO_PUBLIC ailo_status ailo_cache_new(const ailo_cache_config *config,
                                       ailo_cache **output, ailo_error *error) {
  if (output)
    *output = nullptr;
  if (!config || !output)
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "Cache configuration and output are required");
  return ailo::client::boundary(error, [&]() {
    auto owner = std::make_unique<std::shared_ptr<ailo::client::cache_state>>(
        std::make_shared<ailo::client::cache_state>(*config));
    auto shell = std::make_unique<ailo_cache>();
    *shell = {owner.get(), ailo_cache_get_stats, ailo_cache_clear,
              ailo_cache_prune, ailo_cache_destroy};
    owner.release();
    *output = shell.release();
  });
}
AILO_PUBLIC ailo_status ailo_cache_get_stats(ailo_cache *cache,
                                             ailo_cache_stats *output,
                                             ailo_error *error) {
  if (output)
    *output = {};
  if (!output)
    return ailo_error_set(error, AILO_INVALID_ARGUMENT,
                          "Cache statistics output is required");
  return ailo::client::boundary(
      error, [&]() { ailo::client::retain_cache(cache)->stats(*output); });
}
AILO_PUBLIC ailo_status ailo_cache_clear(ailo_cache *cache, ailo_error *error) {
  return ailo::client::boundary(
      error, [&]() { ailo::client::retain_cache(cache)->clear(); });
}
AILO_PUBLIC ailo_status ailo_cache_prune(ailo_cache *cache, ailo_error *error) {
  return ailo::client::boundary(
      error, [&]() { ailo::client::retain_cache(cache)->prune(); });
}
AILO_PUBLIC void ailo_cache_destroy(ailo_cache *cache) {
  if (!cache)
    return;
  delete static_cast<std::shared_ptr<ailo::client::cache_state> *>(
      cache->state);
  delete cache;
}
