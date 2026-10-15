#ifndef AILO_CLIENT_H
#define AILO_CLIENT_H

#include "ailo/cache.h"
#include "ailo/model.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ailo_client ailo_client;
struct pslog_logger;

typedef enum ailo_backend_policy {
  AILO_BACKEND_AUTO = 0,
  AILO_BACKEND_CPU,
  AILO_BACKEND_VULKAN
} ailo_backend_policy;

typedef enum ailo_role {
  AILO_ROLE_USER = 0,
  AILO_ROLE_SYSTEM,
  AILO_ROLE_DEVELOPER,
  AILO_ROLE_ASSISTANT,
  AILO_ROLE_TOOL
} ailo_role;

typedef enum ailo_item_kind {
  AILO_ITEM_MESSAGE = 0,
  AILO_ITEM_FUNCTION_CALL,
  AILO_ITEM_FUNCTION_RESULT,
  AILO_ITEM_REASONING,
  AILO_ITEM_COMPACTION
} ailo_item_kind;

typedef enum ailo_content_kind {
  AILO_CONTENT_TEXT = 0,
  AILO_CONTENT_IMAGE,
  AILO_CONTENT_AUDIO,
  AILO_CONTENT_REFUSAL
} ailo_content_kind;

/** Borrowed bytes; text is UTF-8. The length excludes any terminating NUL. */
typedef struct ailo_content {
  ailo_content_kind kind;
  const void *data;
  size_t size;
} ailo_content;

/** Ordered conversation item. Function calls require call_id/name/arguments;
 * arguments is a JSON object. Results require call_id and text content.
 * Call/result roles may be zero (implied assistant/tool) or their explicit
 * role. Message name is an optional participant name. Item IDs and call IDs
 * are independent, unique identities. Fields unused by a kind are zero/NULL. */
typedef struct ailo_item {
  ailo_item_kind kind;
  ailo_role role;
  const char *id;
  const char *call_id;
  const char *name;
  const ailo_content *content;
  size_t content_count;
  const char *arguments;
} ailo_item;

/** Constructor copies source strings/checksums and model_id/template strings.
 * Source callback user state stays borrowed until destroy. Zero selects eager
 * loading and automatic backend selection. Source itself is always explicit. */
typedef struct ailo_client_config {
  ailo_model_source source;
  const char *model_id;
  const char *chat_template;
  ailo_backend_policy backend;
  ailo_bool lazy;
  /** Zero selects all eligible GPU layers; positive values bound offload. */
  int gpu_layers;
  /** Zero selects min(trained context, 4096). */
  ailo_u32 context_tokens;
  /** Zero selects min(context_tokens, 512). */
  ailo_u32 batch_tokens;
  /** Zero selects two CPU threads. */
  int threads;
  /** NULL creates a private default cache. An explicit owner is retained. */
  ailo_cache *cache;
  /** Disable the private default cache; cannot accompany an explicit owner. */
  ailo_bool disable_prompt_cache;
} ailo_client_config;

typedef struct ailo_logit_bias {
  ailo_i32 token_id;
  double bias;
} ailo_logit_bias;

typedef enum ailo_output_format {
  AILO_OUTPUT_TEXT = 0,
  AILO_OUTPUT_JSON_OBJECT,
  AILO_OUTPUT_JSON_SCHEMA
} ailo_output_format;

typedef struct ailo_function_tool {
  const char *name;
  const char *description;
  /** JSON-object parameter schema. NULL selects an unconstrained object. */
  const char *parameters;
  /** Constrain arguments with the published schema subset. */
  ailo_bool strict;
} ailo_function_tool;

typedef enum ailo_tool_choice {
  AILO_TOOLS_AUTO = 0,
  AILO_TOOLS_NONE,
  AILO_TOOLS_REQUIRED,
  AILO_TOOLS_NAMED
} ailo_tool_choice;

/** Keep borrowed request data unchanged throughout the synchronous call.
 * Zero max_tokens/candidates selects 128/1.
 * Zero temperature is greedy; top_p zero selects 1.0. The default factory uses
 * temperature 0.8/top_p 0.95. Seed is the native 32-bit sampler seed.
 * Stops are UTF-8 strings; maximum four, maximum 4096 bytes each. */
typedef struct ailo_request {
  const ailo_item *items;
  size_t item_count;
  size_t max_tokens;
  unsigned int candidates;
  double temperature;
  double top_p;
  ailo_u32 seed;
  double frequency_penalty;
  double presence_penalty;
  const char *const *stop;
  size_t stop_count;
  /** Unique vocabulary token IDs, finite biases in [-100, 100]; at most 4096.
   */
  const ailo_logit_bias *logit_bias;
  size_t logit_bias_count;
  /** Emit sampled-token probability events, including stop/EOG tokens.
   * Probabilities normalize logits after bias, penalties and grammar, before
   * top-p and temperature. They describe model uncertainty even in greedy mode.
   */
  ailo_bool logprobs;
  /** Zero to twenty alternatives; nonzero requires logprobs. A grammar may
   * permit fewer alternatives. */
  unsigned int top_logprobs;
  /** Total non-EOG sampled bytes across all candidates; zero selects 16 MiB.
   * Includes bytes withheld by stop/UTF-8 handling. No output is collected. */
  size_t max_output_bytes;
  ailo_output_format output_format;
  /** Required only for JSON_SCHEMA: a UTF-8 JSON schema, at most 64 KiB.
   * Unsupported constraints fail before events/evaluation. */
  const char *json_schema;
  /** At most 128 unique functions; definitions total at most 4 MiB. */
  const ailo_function_tool *tools;
  size_t tool_count;
  ailo_tool_choice tool_choice;
  /** Required only for NAMED. */
  const char *tool_name;
  /** Optional callable subset; definitions remain in the rendered prompt. */
  const char *const *allowed_tools;
  size_t allowed_tool_count;
  /** Factory default true. False allows at most one call per candidate. */
  ailo_bool parallel_tool_calls;
  /** Per candidate, zero selects 128 (one when parallel calls are disabled). */
  size_t max_tool_calls;
  /** Optional application partition within the explicit/private owner. UTF-8,
   * at most 256 bytes. Never bypasses token/model/configuration identity. */
  const char *prompt_cache_key;
} ailo_request;

typedef enum ailo_finish_reason {
  AILO_FINISH_NONE = 0,
  AILO_FINISH_STOP,
  AILO_FINISH_LENGTH,
  AILO_FINISH_TOOL_CALLS
} ailo_finish_reason;

typedef enum ailo_event_kind {
  AILO_EVENT_RUN_BEGIN = 0,
  AILO_EVENT_CHOICE_BEGIN,
  AILO_EVENT_TEXT_DELTA,
  AILO_EVENT_CHOICE_END,
  AILO_EVENT_RUN_END,
  AILO_EVENT_TOKEN,
  AILO_EVENT_ITEM_BEGIN,
  AILO_EVENT_CONTENT_BEGIN,
  AILO_EVENT_CONTENT_END,
  AILO_EVENT_ITEM_END,
  AILO_EVENT_USAGE,
  AILO_EVENT_FUNCTION_NAME_DELTA,
  AILO_EVENT_FUNCTION_ARGUMENTS_DELTA
} ailo_event_kind;

/** A token piece is arbitrary bytes, possibly incomplete UTF-8 or empty.
 * It is separate from the UTF-8 text-delta path. All pointers are borrowed. */
typedef struct ailo_token_probability {
  ailo_i32 token_id;
  const void *data;
  size_t size;
  double logprob;
} ailo_token_probability;

typedef struct ailo_token_info {
  ailo_token_probability chosen;
  const ailo_token_probability *top;
  size_t top_count;
  /** Per-candidate sampled token index and byte offset before stop filtering.
   */
  ailo_u64 index;
  ailo_u64 byte_offset;
  ailo_bool end_of_generation;
} ailo_token_info;

typedef struct ailo_usage {
  ailo_u64 prompt_tokens;
  ailo_u64 completion_tokens;
  ailo_u64 cached_tokens;
} ailo_usage;

/** Payload and model_id are borrowed only during the callback. Text deltas are
 * safe UTF-8 and emitted as soon as possible, with only bounded stop/UTF-8
 * carry. Sequence numbers are monotonic within a run. No complete answer is
 * retained. */
typedef struct ailo_event {
  ailo_event_kind kind;
  ailo_u64 sequence;
  unsigned int candidate;
  const char *model_id;
  const void *data;
  size_t size;
  ailo_finish_reason finish_reason;
  ailo_usage usage;
  /** Non-NULL only for AILO_EVENT_TOKEN, delivered before its text delta and
   * before evaluating the next token. Adapters exclude suppressed stop/EOG
   * tokens from user-visible content logprobs using byte offsets/text deltas.
   */
  const ailo_token_info *token;
  const char *run_id;
  /** Non-NULL only while an item is active, including its begin/end events.
   * IDs are stable within this run, unique across runs, and borrowed. */
  const char *item_id;
  ailo_item_kind item_kind;
  unsigned int item_index;
  unsigned int content_index;
  /** Stable receiver-owned identity for a function-call item. Name is the
   * bounded prefix known at this event; final at ITEM_END. */
  const char *call_id;
  const char *name;
} ailo_event;

/** Return zero to cancel; a negative value reports sink failure. The consumer
 * accepts the whole event before returning. Reentry into the client is busy. */
typedef int (*ailo_event_sink)(void *user_data, const ailo_event *event);

typedef struct ailo_run_info {
  ailo_usage usage;
  unsigned int completed_candidates;
  ailo_finish_reason finish_reason;
  ailo_backend_policy actual_backend;
  /** Owned run identity, empty for preflight failures. */
  char id[37];
} ailo_run_info;

/** Allocated receiver shell. Do not copy or modify its state/method pointers.
 * Operations are synchronous and serialized; overlap/reentry returns busy.
 * cancel is safe from a callback or another thread while a call is active.
 * Callers must join all operations before destroy; concurrent destruction is
 * invalid. Destroying NULL and freeing a NULL string are accepted. */
struct ailo_client {
  void *state;
  ailo_status (*generate)(ailo_client *, const ailo_request *, ailo_event_sink,
                          void *, ailo_run_info *, ailo_error *);
  ailo_status (*model_card)(ailo_client *, char **, ailo_error *);
  ailo_status (*capabilities)(ailo_client *, char **, ailo_error *);
  ailo_status (*devices)(ailo_client *, char **, ailo_error *);
  ailo_status (*cache_stats)(ailo_client *, ailo_cache_stats *, ailo_error *);
  ailo_status (*cache_clear)(ailo_client *, ailo_error *);
  /** Export the last successful prompt checkpoint to a named cache slot.
   * Import validates into disposable state and seeds reuse; it does not resume
   * generation or change conversation history. Slots are quota/TTL evictable.
   */
  ailo_status (*snapshot_export)(ailo_client *, const char *, ailo_error *);
  ailo_status (*snapshot_import)(ailo_client *, const char *, const char *,
                                 ailo_error *);
  void (*cancel)(ailo_client *);
  void (*destroy)(ailo_client *);
};

ailo_client_config ailo_client_default_config(void);
ailo_request ailo_request_default(void);
/** Output is NULL on failure. Lazy construction validates/copies the source
 * but performs no acquisition, model load or inference. */
ailo_status ailo_client_new(const ailo_client_config *config,
                            ailo_client **output, ailo_error *error);
/** Derives an owned child from logger; NULL selects the ordinary constructor.
 * Logger output/palette/user resources follow libpslog's borrowed lifetimes
 * and must support native worker threads. Destroying the parent logger is
 * allowed; keep its borrowed resources live until this client is destroyed.
 * Native callbacks must not throw or longjmp. No process-global hook is
 * replaced.
 */
ailo_status ailo_client_new_with_logger(const ailo_client_config *config,
                                        struct pslog_logger *logger,
                                        ailo_client **output,
                                        ailo_error *error);
ailo_status ailo_client_generate(ailo_client *client,
                                 const ailo_request *request,
                                 ailo_event_sink sink, void *user_data,
                                 ailo_run_info *output, ailo_error *error);
ailo_status ailo_client_cache_stats(ailo_client *, ailo_cache_stats *,
                                    ailo_error *);
ailo_status ailo_client_cache_clear(ailo_client *, ailo_error *);
ailo_status ailo_client_snapshot_export(ailo_client *, const char *,
                                        ailo_error *);
ailo_status ailo_client_snapshot_import(ailo_client *, const char *,
                                        const char *, ailo_error *);
/** Owned JSON outputs; release with ailo_string_free. model_card forces a
 * configured lazy load; devices and capabilities never download model files. */
ailo_status ailo_client_model_card(ailo_client *client, char **output,
                                   ailo_error *error);
ailo_status ailo_client_capabilities(ailo_client *client, char **output,
                                     ailo_error *error);
ailo_status ailo_client_devices(ailo_client *client, char **output,
                                ailo_error *error);
void ailo_client_cancel(ailo_client *client);
void ailo_client_destroy(ailo_client *client);

#ifdef __cplusplus
}
#endif
#endif
