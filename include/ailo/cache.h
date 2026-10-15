#ifndef AILO_CACHE_H
#define AILO_CACHE_H
#include "ailo/base.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct ailo_cache ailo_cache;
/** Prompt/KV state only: neither model weights nor retained API responses.
 * The factory selects 64 MiB, eight entries and 300 seconds idle expiry.
 * Zero in any RAM limit disables memory storage. Explicit owners require a
 * trusted, nonempty application namespace; this is not an authorization
 * mechanism. */
typedef struct ailo_cache_config {
  size_t max_memory_bytes;
  size_t max_entries;
  ailo_u32 idle_ttl_seconds;
  const char *namespace_id;
  /** Persistence is off unless a nonzero disk budget and TTL are supplied
   * together with a 32-byte key and nonempty key ID. NULL directory accepts
   * the default XDG model-cache root's inference subdirectory. */
  const char *directory;
  size_t max_disk_bytes;
  ailo_u32 disk_ttl_seconds;
  size_t max_disk_entries;
  /** Bounds one materialized tokens/native-state payload; default 64 MiB. */
  size_t max_snapshot_bytes;
  const char *key_id;
  /** Borrowed during construction only; derived keys are owned and erased.
   * The caller remains responsible for its original key bytes. */
  const void *key;
  size_t key_size;
} ailo_cache_config;

typedef struct ailo_cache_stats {
  ailo_u64 hits, misses, evictions, oversized, invalid, busy_skips;
  ailo_u64 cached_tokens, evaluated_tokens;
  ailo_u64 lookup_microseconds, capture_microseconds, prefill_microseconds;
  size_t entries, memory_bytes;
  ailo_u64 snapshot_reads, snapshot_writes, snapshot_errors, snapshot_skipped,
      disk_evictions;
  size_t disk_entries, disk_bytes;
} ailo_cache_stats;

/** Owned receiver shell. Clients retain the underlying owner, so destroying
 * this shell does not invalidate clients. Methods are serialized; overlap
 * returns BUSY. Inference bypasses a busy cache rather than waiting for it.
 * Join operations before destroying the shell. */
struct ailo_cache {
  void *state;
  ailo_status (*stats)(ailo_cache *, ailo_cache_stats *, ailo_error *);
  ailo_status (*clear)(ailo_cache *, ailo_error *);
  ailo_status (*prune)(ailo_cache *, ailo_error *);
  void (*destroy)(ailo_cache *);
};
ailo_cache_config ailo_cache_default_config(void);
ailo_status ailo_cache_new(const ailo_cache_config *, ailo_cache **,
                           ailo_error *);
ailo_status ailo_cache_get_stats(ailo_cache *, ailo_cache_stats *,
                                 ailo_error *);
ailo_status ailo_cache_clear(ailo_cache *, ailo_error *);
ailo_status ailo_cache_prune(ailo_cache *, ailo_error *);
void ailo_cache_destroy(ailo_cache *);
#ifdef __cplusplus
}
#endif
#endif
