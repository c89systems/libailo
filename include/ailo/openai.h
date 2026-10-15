#ifndef AILO_OPENAI_H
#define AILO_OPENAI_H
#include "ailo/client.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum ailo_record_protocol {
  AILO_RECORD_CHAT = 0,
  AILO_RECORD_RESPONSE,
  AILO_RECORD_CONVERSATION
} ailo_record_protocol;
typedef enum ailo_record_section {
  AILO_RECORD_OUTPUT = 0,
  AILO_RECORD_INPUT,
  AILO_RECORD_METADATA,
  AILO_RECORD_CONVERSATION_STATE,
  AILO_RECORD_CONVERSATION_ITEMS
} ailo_record_section;
/** Borrowed reservation arguments. The repository scopes every operation to
 * namespace_id and enforces authorization and the promised retention/quota.
 * expires_at=0 means retain until deletion, never a KV-cache eviction policy.
 */
typedef struct ailo_record_metadata {
  const char *namespace_id;
  const char *id;
  ailo_record_protocol protocol;
  ailo_u64 created_at;
  ailo_u64 expires_at;
  size_t max_bytes;
} ailo_record_metadata;
typedef enum ailo_conversation_action {
  AILO_CONVERSATION_CREATE = 0,
  AILO_CONVERSATION_UPDATE,
  AILO_CONVERSATION_DELETE,
  AILO_CONVERSATION_APPEND,
  AILO_CONVERSATION_DELETE_ITEM
} ailo_conversation_action;
/** Optimistic atomic mutation. revision=0 creates; other actions compare the
 * exact prior revision and hold the conversation until commit/abort. BUSY
 * rejects a conflicting writer before inference. response is optional and is
 * published in the SAME transaction. Conversation state/items never expire.
 * input_ids are new explicit input IDs to admit before inference. */
typedef struct ailo_conversation_change {
  const char *namespace_id;
  const char *id;
  ailo_conversation_action action;
  ailo_u64 revision;
  size_t max_bytes;
  const ailo_record_metadata *response;
  const char *item_id;
  const char *const *input_ids;
  size_t input_count;
} ailo_conversation_change;
/** Query OUTPUT is an array of original wire items, ordered and scoped to this
 * namespace/conversation. Return at most limit+1 rows for pagination. A zero
 * limit is an internal ascending full-history read for context reconstruction.
 * Deleting a conversation retains its item rows; queries may read those rows.
 * The returned handle uses ordinary read/close, without a TTL. */
typedef struct ailo_conversation_query {
  const char *namespace_id;
  const char *id;
  const char *item_id;
  const char *after;
  size_t limit;
  ailo_bool descending;
  /** Internal context reconstruction selects stored canonical item views;
   * generated compaction control rows have no canonical item and are skipped.
   */
  ailo_bool canonical;
} ailo_conversation_query;
/** Application-owned durable repository, not an evictable cache. Callbacks are
 * synchronous, must not throw/longjmp, and consume borrowed data before return.
 * reserve admits the full max_bytes and TTL BEFORE inference. It sets a unique
 * non-NULL transaction, including on failures requiring abort. write receives
 * incremental JSON in independent output/input/metadata sections. commit
 * atomically publishes all, retaining the transaction on failure; on success
 * it consumes it.
 * abort consumes unpublished state, must not fail, and tolerates NULL.
 * open returns NOT_FOUND for unavailable/unauthorized records and a non-NULL
 * stable snapshot handle on success; close consumes it, including after failed
 * reads. read forwards the original section bytes to the sink, honors
 * cancellation, and must never return success after a rejected sink call. The
 * eight record callbacks are required; the conversation pair is optional.
 * Resources live until client destroy; distinct clients may call the same
 * provider concurrently. */
typedef struct ailo_record_repository {
  void *user_data;
  ailo_status (*reserve)(void *, const ailo_record_metadata *, void **,
                         ailo_error *);
  ailo_status (*write)(void *, void *, ailo_record_section, const void *,
                       size_t, ailo_error *);
  ailo_status (*commit)(void *, void *, ailo_error *);
  void (*abort)(void *, void *);
  ailo_status (*open)(void *, const char *, ailo_record_protocol, const char *,
                      void **, ailo_u64 *, ailo_error *);
  ailo_status (*read)(void *, void *, ailo_record_section, ailo_output_sink,
                      void *, ailo_error *);
  void (*close)(void *, void *);
  ailo_status (*erase)(void *, const char *, ailo_record_protocol, const char *,
                       ailo_error *);
  /** Optional pair enabling conversations. amend owns a transaction using
   * write/commit/abort. STATE replaces private context; ITEMS appends item
   * rows. Each row is {id,wire,canonical}; generated compaction control rows
   * have canonical=null and canonical queries skip them. IDs are namespace-wide
   * unique; item queries must also verify parent ID.
   * Without response, OUTPUT replaces the conversation object. Keep
   * deleted roots as minimal tombstones (private deleted flag); never erase
   * item rows as a side effect. query owns a reader using read/close. All
   * revisions and item IDs must be checked atomically, including cross-client
   * callers. */
  ailo_status (*amend)(void *, const ailo_conversation_change *, void **,
                       ailo_error *);
  ailo_status (*query)(void *, const ailo_conversation_query *, void **,
                       ailo_error *);
} ailo_record_repository;

typedef struct ailo_openai_config {
  const char *namespace_id;
  const ailo_record_repository *repository;
  ailo_bool zero_data_retention;
  /** Select the historical opt-in Chat storage default. Responses defaults
   * to stored. Explicit request store overrides defaults, except under ZDR. */
  ailo_bool chat_store_opt_in;
  /** Zero selects 30 days; smaller nonzero values are invalid. */
  ailo_u64 response_retention_seconds;
  /** Zero means until deletion; no provider Chat TTL is assumed. */
  ailo_u64 chat_retention_seconds;
  /** Zero selects 32 MiB. Charged output/probability memory, not model/KV RAM.
   * Unstored Chat streaming retains only bounded pending token metadata. */
  size_t max_output_memory_bytes;
  /** Zero selects 64 MiB across all serialized repository sections. */
  size_t max_record_bytes;
  /** Trusted standalone/automatic compaction policy; copied at construction.
   * NULL selects the native defaults. No extra wire request controls. */
  const struct ailo_compaction_policy *compaction;
} ailo_openai_config;

typedef struct ailo_api_info {
  int http_status;
  const char *content_type;
  /** Positive bytes have reached the sink; errors afterwards require the
   * embedding transport to close an already-started response. */
  ailo_bool started;
  ailo_bool stored;
  char id[64];
  ailo_run_info run;
} ailo_api_info;

ailo_openai_config ailo_openai_default_config(void);
/** JSON input is borrowed for the synchronous call; output is direct JSON or
 * SSE according to stream. No sockets/HTTP runtime. out_info is cleared on
 * entry, provides status/usage on failure. Preflight errors emit no bytes.
 * Sink returns positive to accept ALL bytes, zero to cancel, negative to fail.
 * Streaming never waits for the reply to finish, including store=true. */
ailo_status ailo_client_chat_completions(ailo_client *, const void *, size_t,
                                         ailo_output_sink, void *,
                                         ailo_api_info *, ailo_error *);
/** Repository reads/deletion use only the configured trusted namespace.
 * Input=true retrieves the stored input-message array instead of completion.
 * Missing, unauthorized or expired records return NOT_FOUND/HTTP 404. */
ailo_status ailo_client_chat_retrieve(ailo_client *, const char *, ailo_bool,
                                      ailo_output_sink, void *, ailo_api_info *,
                                      ailo_error *);
ailo_status ailo_client_chat_delete(ailo_client *, const char *,
                                    ailo_output_sink, void *, ailo_api_info *,
                                    ailo_error *);
/** Responses output is explicitly retained within max_output_memory_bytes for
 * full done/terminal repeats; every available delta is still sent immediately.
 * Stored records default to at least 30 days. previous_response_id reads the
 * trusted repository and does not inherit prior top-level instructions. */
ailo_status ailo_client_responses(ailo_client *, const void *, size_t,
                                  ailo_output_sink, void *, ailo_api_info *,
                                  ailo_error *);
ailo_status ailo_client_response_retrieve(ailo_client *, const char *,
                                          ailo_output_sink, void *,
                                          ailo_api_info *, ailo_error *);
ailo_status ailo_client_response_delete(ailo_client *, const char *,
                                        ailo_output_sink, void *,
                                        ailo_api_info *, ailo_error *);
/** Stateless standalone Responses compaction. Writes the canonical next window
 * as response.compaction JSON; performs no response-record storage. */
ailo_status ailo_client_response_compact(ailo_client *, const void *, size_t,
                                         ailo_output_sink, void *,
                                         ailo_api_info *, ailo_error *);
ailo_status ailo_client_conversation_create(ailo_client *, const void *, size_t,
                                            ailo_output_sink, void *,
                                            ailo_api_info *, ailo_error *);
ailo_status ailo_client_conversation_retrieve(ailo_client *, const char *,
                                              ailo_output_sink, void *,
                                              ailo_api_info *, ailo_error *);
ailo_status ailo_client_conversation_update(ailo_client *, const char *,
                                            const void *, size_t,
                                            ailo_output_sink, void *,
                                            ailo_api_info *, ailo_error *);
ailo_status ailo_client_conversation_delete(ailo_client *, const char *,
                                            ailo_output_sink, void *,
                                            ailo_api_info *, ailo_error *);
ailo_status ailo_client_conversation_items_create(ailo_client *, const char *,
                                                  const void *, size_t,
                                                  ailo_output_sink, void *,
                                                  ailo_api_info *,
                                                  ailo_error *);
ailo_status ailo_client_conversation_item_retrieve(ailo_client *, const char *,
                                                   const char *, const void *,
                                                   size_t, ailo_output_sink,
                                                   void *, ailo_api_info *,
                                                   ailo_error *);
ailo_status ailo_client_conversation_item_delete(ailo_client *, const char *,
                                                 const char *, ailo_output_sink,
                                                 void *, ailo_api_info *,
                                                 ailo_error *);
ailo_status ailo_client_conversation_items_list(ailo_client *, const char *,
                                                const void *, size_t,
                                                ailo_output_sink, void *,
                                                ailo_api_info *, ailo_error *);
#ifdef __cplusplus
}
#endif
#endif
