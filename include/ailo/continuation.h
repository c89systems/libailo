#ifndef AILO_CONTINUATION_H
#define AILO_CONTINUATION_H
#include "ailo/client.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct ailo_continuation_config {
  /** Defaults: 4096 items and 4 MiB charged canonical storage. */
  size_t max_items;
  size_t max_bytes;
} ailo_continuation_config;

typedef struct ailo_continuation_info {
  size_t item_count;
  size_t storage_bytes;
  size_t pending_calls;
  ailo_u64 revision;
  /** Model/shard content and effective tokenizer/template identity. */
  char identity[65];
} ailo_continuation_info;

/** Borrowed item valid only during this callback. Positive continues, zero
 * cancels, negative reports sink failure. No mutation/reentry on this owner. */
typedef int (*ailo_item_sink)(void *, const ailo_item *);

/** Owned canonical history, independent of KV caches and client lifetime.
 * All operations are try-only; overlap/reentry returns BUSY. Clones share
 * immutable items and diverge on append. Join operations before destroy.
 * This is explicit bounded materialization, never an output collector. */
struct ailo_continuation {
  void *state;
  ailo_status (*clone)(ailo_continuation *, ailo_continuation **, ailo_error *);
  ailo_status (*append)(ailo_continuation *, const ailo_item *, size_t,
                        ailo_error *);
  ailo_status (*info)(ailo_continuation *, ailo_continuation_info *,
                      ailo_error *);
  ailo_status (*visit)(ailo_continuation *, ailo_item_sink, void *,
                       ailo_error *);
  void (*destroy)(ailo_continuation *);
};
ailo_continuation_config ailo_continuation_default_config(void);
/** NULL config selects defaults. Binds to the loaded model/tokenizer/template;
 * may perform its configured lazy acquisition. Does not retain the client.
 * Outputs are cleared on failure. No files/records are written. */
ailo_status ailo_continuation_new(ailo_client *,
                                  const ailo_continuation_config *,
                                  ailo_continuation **, ailo_error *);
ailo_status ailo_continuation_clone(ailo_continuation *, ailo_continuation **,
                                    ailo_error *);
/** Atomic append: copies and validates the complete resulting history first.
 * A pending final tool-call/result group is allowed, but generation requires
 * complete results. Empty append is a no-op. Unsupported items fail clearly. */
ailo_status ailo_continuation_append(ailo_continuation *, const ailo_item *,
                                     size_t, ailo_error *);
ailo_status ailo_continuation_get_info(ailo_continuation *,
                                       ailo_continuation_info *, ailo_error *);
ailo_status ailo_continuation_visit(ailo_continuation *, ailo_item_sink, void *,
                                    ailo_error *);
void ailo_continuation_destroy(ailo_continuation *);
#ifdef __cplusplus
}
#endif
#endif
