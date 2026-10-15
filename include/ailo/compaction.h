#ifndef AILO_COMPACTION_H
#define AILO_COMPACTION_H
#include "ailo/base.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct ailo_compactor ailo_compactor;
typedef struct ailo_compaction_key {
  const char *id;
  const void *bytes;
  size_t size;
} ailo_compaction_key;

/** Stable replay requires an explicit namespace and current 32-byte key.
 * NULL config selects a private ephemeral owner. Keys are copied/derived
 * during construction and erased when the last owner releases them. */
typedef struct ailo_compactor_config {
  const char *namespace_id;
  ailo_compaction_key current_key;
  const ailo_compaction_key *old_keys;
  size_t old_key_count;
  /** Zero means no automatic expiry; API record retention is separate. */
  ailo_u32 ttl_seconds;
  /** Maximum decoded envelope, default 1 MiB, at most 4 MiB. */
  size_t max_item_bytes;
} ailo_compactor_config;

typedef struct ailo_compactor_info {
  ailo_bool ephemeral;
  size_t decryption_keys;
  size_t max_item_bytes;
  ailo_u32 ttl_seconds;
} ailo_compactor_info;

/** Immutable key/policy owner, retained independently by attached clients.
 * Info is read-only and safe across independent callers. Join operations
 * before destroying the receiver shell. */
struct ailo_compactor {
  void *state;
  ailo_status (*info)(ailo_compactor *, ailo_compactor_info *, ailo_error *);
  void (*destroy)(ailo_compactor *);
};
ailo_compactor_config ailo_compactor_default_config(void);
ailo_status ailo_compactor_new(const ailo_compactor_config *, ailo_compactor **,
                               ailo_error *);
ailo_status ailo_compactor_get_info(ailo_compactor *, ailo_compactor_info *,
                                    ailo_error *);
void ailo_compactor_destroy(ailo_compactor *);
#ifdef __cplusplus
}
#endif
#endif
