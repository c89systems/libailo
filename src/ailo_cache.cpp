#include "ailo_cache_internal.hpp"
#include "ailo_logging_internal.hpp"
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
         partition.capacity() + tokens.capacity() * sizeof(llama_token) +
         bytes.capacity();
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
}
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
#undef COUNT
  output.entries = entries.size();
  output.memory_bytes = bytes;
}
void cache_state::clear() {
  cache_guard lock(active, true);
  while (!entries.empty())
    erase(entries.begin());
}
void cache_state::prune() {
  cache_guard lock(active, true);
  prune_locked();
}

size_t cache_state::restore(state &client,
                            const std::vector<llama_token> &tokens,
                            const std::string &partition) {
  if (!enabled() || tokens.size() < 2)
    return 0;
  timing timer{{std::chrono::steady_clock::now()}, lookup_microseconds};
  cache_guard lock(active);
  if (!lock) {
    busy_skips.fetch_add(1);
    misses.fetch_add(1);
    return 0;
  }
  prune_locked();
  const bool trim = !llama_model_is_recurrent(client.model.get()) &&
                    !llama_model_is_hybrid(client.model.get()) &&
                    llama_model_n_swa(client.model.get()) == 0;
  auto best = entries.end();
  size_t count = 0;
  for (auto it = entries.begin(); it != entries.end(); ++it) {
    if (it->identity != client.cache_identity || it->partition != partition)
      continue;
    const size_t end = std::min(it->tokens.size(), tokens.size() - 1);
    size_t prefix = 0;
    while (prefix < end && it->tokens[prefix] == tokens[prefix])
      ++prefix;
    if (prefix > count && (trim || prefix == it->tokens.size())) {
      best = it;
      count = prefix;
    }
  }
  if (best == entries.end()) {
    misses.fetch_add(1);
    return 0;
  }
  // Native restore may partially modify its context on failure. Never restore
  // into the live context or adopt a candidate until every check has passed.
  auto candidate = client.new_context();
  if (!candidate) {
    misses.fetch_add(1);
    return 0;
  }
  bool valid = llama_state_seq_set_data_ext(
                   candidate.get(), best->bytes.data(), best->bytes.size(), 0,
                   LLAMA_STATE_SEQ_FLAGS_NONE) == best->bytes.size();
  if (valid && count < best->tokens.size())
    valid = llama_memory_seq_rm(llama_get_memory(candidate.get()), 0,
                                static_cast<llama_pos>(count), -1);
  if (valid)
    valid = llama_memory_seq_pos_max(llama_get_memory(candidate.get()), 0) ==
            static_cast<llama_pos>(count - 1);
  if (valid && trim)
    valid = llama_memory_seq_pos_min(llama_get_memory(candidate.get()), 0) == 0;
  if (!valid) {
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
  cached_tokens.fetch_add(count);
  return count;
}

void cache_state::capture(state &client, const std::vector<llama_token> &tokens,
                          size_t count, const std::string &partition) {
  if (!enabled() || !count)
    return;
  timing timer{{std::chrono::steady_clock::now()}, capture_microseconds};
  cache_guard lock(active);
  if (!lock) {
    busy_skips.fetch_add(1);
    return;
  }
  prune_locked();
  for (auto it = entries.begin(); it != entries.end(); ++it)
    if (it->identity == client.cache_identity && it->partition == partition &&
        it->tokens.size() == count &&
        std::equal(it->tokens.begin(), it->tokens.end(), tokens.begin())) {
      it->used = std::chrono::steady_clock::now();
      entries.splice(entries.begin(), entries, it);
      return;
    }
  const size_t size = llama_state_seq_get_size_ext(client.context.get(), 0,
                                                   LLAMA_STATE_SEQ_FLAGS_NONE);
  if (!size) {
    invalid.fetch_add(1);
    return;
  }
  // Bound the complete native state BEFORE allocating it. Evict first, so a
  // new cache buffer cannot temporarily double the configured resident bound.
  const size_t overhead = sizeof(cache_entry) + 2 * sizeof(void *) +
                          std::max(size_t(64), client.cache_identity.size()) +
                          std::max(size_t(256), partition.size());
  if (overhead > config.max_memory_bytes ||
      count > (config.max_memory_bytes - overhead) / sizeof(llama_token) ||
      size > config.max_memory_bytes - overhead - count * sizeof(llama_token)) {
    oversized.fetch_add(1);
    return;
  }
  const size_t required = overhead + size + count * sizeof(llama_token);
  while (!entries.empty() && (entries.size() >= config.max_entries ||
                              bytes > config.max_memory_bytes - required))
    erase(std::prev(entries.end()));
  entries.emplace_front();
  auto &entry = entries.front();
  try {
    entry.identity = client.cache_identity;
    entry.partition = partition;
    entry.tokens.assign(tokens.begin(),
                        tokens.begin() + static_cast<ptrdiff_t>(count));
    entry.bytes.resize(size);
    if (entry.charge() > config.max_memory_bytes - bytes ||
        llama_state_seq_get_data_ext(client.context.get(), entry.bytes.data(),
                                     size, 0,
                                     LLAMA_STATE_SEQ_FLAGS_NONE) != size) {
      entries.pop_front();
      invalid.fetch_add(1);
      return;
    }
    entry.used = std::chrono::steady_clock::now();
    bytes += entry.charge();
  } catch (...) {
    entries.pop_front();
    throw;
  }
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
