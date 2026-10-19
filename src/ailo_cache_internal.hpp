#ifndef AILO_CACHE_INTERNAL_HPP
#define AILO_CACHE_INTERNAL_HPP
#include "ailo_client_internal.hpp"
#include <chrono>
#include <list>

namespace ailo {
namespace client {
// A try-only guard also handles same-thread callback reentry without relying
// on recursively locking a non-recursive std::mutex.
class cache_guard {
  std::atomic<bool> &active;
  bool owned;

public:
  explicit cache_guard(std::atomic<bool> &flag, bool required = false);
  ~cache_guard();
  cache_guard(const cache_guard &) = delete;
  cache_guard &operator=(const cache_guard &) = delete;
  explicit operator bool() const { return owned; }
};
struct cache_entry {
  std::string identity, partition;
  std::string media_key;
  size_t positions = 0, evaluated_tokens = 0;
  std::vector<llama_token> tokens;
  std::vector<unsigned char> bytes;
  std::chrono::steady_clock::time_point used;
  size_t charge() const;
  ~cache_entry();
  cache_entry() = default;
  cache_entry(cache_entry &&) noexcept = default;
  cache_entry(const cache_entry &) = delete;
};
class snapshot_store;
struct cache_state {
  std::atomic<bool> active{false};
  ailo_cache_config config;
  std::string namespace_id;
  std::list<cache_entry> entries;
  size_t bytes = 0;
  std::unique_ptr<snapshot_store> disk;
  std::atomic<uint64_t> hits{0}, misses{0}, evictions{0}, oversized{0},
      invalid{0}, busy_skips{0}, cached_tokens{0}, evaluated_tokens{0},
      lookup_microseconds{0}, capture_microseconds{0}, prefill_microseconds{0};
  std::atomic<uint64_t> snapshot_reads{0}, snapshot_writes{0},
      snapshot_errors{0}, snapshot_skipped{0}, disk_evictions{0};
  explicit cache_state(const ailo_cache_config &config);
  ~cache_state();
  bool enabled() const;
  bool available() const { return enabled() || bool(disk); }
  void erase(std::list<cache_entry>::iterator item);
  void prune_locked();
  void stats(ailo_cache_stats &output);
  void clear();
  void prune();
  size_t restore(state &client, const std::vector<llama_token> &tokens,
                 const std::string &partition);
  std::string capture(state &client, const std::vector<llama_token> &tokens,
                      size_t count, const std::string &partition);
  bool admit(std::unique_ptr<cache_entry> &entry);
  void snapshot_export(state &client, const char *slot);
  void snapshot_import(state &client, const char *slot, const char *partition);
};
size_t matching_prefix(const cache_entry &, const std::vector<llama_token> &,
                       bool trim);
std::unique_ptr<llama_context, decltype(&llama_free)>
restore_checkpoint(state &, const cache_entry &, size_t count,
                   bool *allocation_failed = nullptr);
std::shared_ptr<cache_state> retain_cache(ailo_cache *cache);
std::string cache_partition(const char *source);
void identify_cache(state &client, bool required = false);
std::string loaded_file_digest(state &, const std::string &);
uint64_t elapsed_us(std::chrono::steady_clock::time_point since);
} // namespace client
} // namespace ailo
#endif
