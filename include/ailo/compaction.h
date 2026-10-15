#ifndef AILO_COMPACTION_H
#define AILO_COMPACTION_H
#include "ailo/client.h"
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

typedef struct ailo_compaction_policy {
  /** Maximum rendered input tokens after compaction. Zero selects 75% of
   * context space remaining after the output reserve. */
  size_t target_tokens;
  /** Recent complete user turns to retain; default two. Instructions, pinned
   * groups and incomplete turns/call groups are always protected. */
  size_t recent_turns;
  /** Output space to preserve in the main context, default 128 tokens. */
  size_t output_reserve_tokens;
  /** Separate summary context, zero selects min(main context, 32768). */
  size_t summary_context_tokens;
  /** Total summary generation budget, including reasoning, default 256. */
  size_t summary_tokens;
  /** Bounded materialization of the summary answer, default 32 KiB. */
  size_t max_summary_bytes;
} ailo_compaction_policy;

typedef struct ailo_compaction_info {
  ailo_bool changed;
  size_t before_tokens;
  size_t after_tokens;
  size_t retained_items;
  size_t removed_items;
  ailo_u64 summary_prompt_tokens;
  ailo_u64 summary_completion_tokens;
  unsigned int summary_passes;
} ailo_compaction_info;

ailo_compaction_policy ailo_compaction_default_policy(void);
/** Returns a new owned canonical window. Input and its continuation are never
 * mutated. NULL policy selects defaults. Tools/rendering controls on request
 * are used for exact before/after counts; its ordinary sampling fields do not
 * control the separate greedy summary pass. Summary generation is bounded
 * materialization, not user output. Oversized summary input fails explicitly.
 * Both outputs are cleared on failure; cancellation is safe from another
 * thread.
 */
ailo_status ailo_client_compact(ailo_client *, const ailo_request *,
                                const ailo_compaction_policy *,
                                ailo_continuation **, ailo_compaction_info *,
                                ailo_error *);
#ifdef __cplusplus
}
#endif
#endif
