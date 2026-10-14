#ifndef AILO_CLIENT_H
#define AILO_CLIENT_H

#include "ailo/model.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ailo_client ailo_client;

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

/** Ordered conversation item. Fields not used by its kind must be zero/NULL. */
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
} ailo_client_config;

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
  AILO_EVENT_RUN_END
} ailo_event_kind;

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
} ailo_event;

/** Return zero to cancel; a negative value reports sink failure. The consumer
 * accepts the whole event before returning. Reentry into the client is busy. */
typedef int (*ailo_event_sink)(void *user_data, const ailo_event *event);

typedef struct ailo_run_info {
  ailo_usage usage;
  unsigned int completed_candidates;
  ailo_finish_reason finish_reason;
  ailo_backend_policy actual_backend;
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
  void (*cancel)(ailo_client *);
  void (*destroy)(ailo_client *);
};

ailo_client_config ailo_client_default_config(void);
ailo_request ailo_request_default(void);
/** Output is NULL on failure. Lazy construction validates/copies the source
 * but performs no acquisition, model load or inference. */
ailo_status ailo_client_new(const ailo_client_config *config,
                            ailo_client **output, ailo_error *error);
ailo_status ailo_client_generate(ailo_client *client,
                                 const ailo_request *request,
                                 ailo_event_sink sink, void *user_data,
                                 ailo_run_info *output, ailo_error *error);
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
