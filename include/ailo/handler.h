#ifndef AILO_HANDLER_H
#define AILO_HANDLER_H
#include "ailo/openai.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct ailo_handler ailo_handler;
typedef struct ailo_handler_config {
  /** Borrowed until handler destruction. No model or repository is created. */
  ailo_client *client;
  /** Zero selects the default. Nonzero may only lower the default limit. */
  size_t max_body_bytes;
  size_t max_target_bytes;
} ailo_handler_config;

typedef struct ailo_http_request {
  const char *method;
  const char *target;
  const char *content_type;
  const char *content_encoding;
  const void *body;
  size_t body_size;
} ailo_http_request;

/** Borrowed during begin only. Header values contain no CR/LF. The embedding
 * transport maps these fields to its native header API; it owns HTTP framing.
 * retry_after is NULL except for a busy response. */
typedef struct ailo_http_response {
  int status;
  const char *content_type;
  const char *allow;
  const char *cache_control;
  const char *request_id;
  const char *retry_after;
} ailo_http_response;

/** All callbacks are required, synchronous, and must not throw/longjmp.
 * Positive accepts, zero cancels, negative fails. write consumes ALL bytes.
 * finish finalizes successful framing. abort consumes a failed response after
 * begin was attempted, including a failed begin/finish; it must not fail.
 * No callback receives a complete buffered streaming reply. */
typedef struct ailo_http_transport {
  void *user_data;
  int (*begin)(void *, const ailo_http_response *);
  ailo_output_sink write;
  int (*finish)(void *);
  void (*abort)(void *);
} ailo_http_transport;

typedef struct ailo_handler_info {
  ailo_api_info api;
  int http_status;
  /** begin has been attempted; its failure may have partially written headers.
   */
  ailo_bool started;
  ailo_bool completed;
  ailo_bool aborted;
  ailo_u64 bytes_written;
  char request_id[64];
} ailo_handler_info;

/** One synchronous call per handler, no queue. Overlap returns HTTP 503.
 * Join calls before destroy; the borrowed client must outlive this receiver.
 * External cancellation uses client->cancel; sink zero reports disconnect.
 * HTTP errors are written and finalized but return the original non-OK status.
 * Failure after begin aborts the response, without changing its wire status.
 * Invalid C arguments/missing callbacks invoke no transport callbacks. */
struct ailo_handler {
  void *state;
  ailo_status (*handle)(ailo_handler *, const ailo_http_request *,
                        const ailo_http_transport *, ailo_handler_info *,
                        ailo_error *);
  void (*destroy)(ailo_handler *);
};
ailo_handler_config ailo_handler_default_config(void);
ailo_status ailo_handler_new(const ailo_handler_config *, ailo_handler **,
                             ailo_error *);
ailo_status ailo_handler_handle(ailo_handler *, const ailo_http_request *,
                                const ailo_http_transport *,
                                ailo_handler_info *, ailo_error *);
void ailo_handler_destroy(ailo_handler *);
#ifdef __cplusplus
}
#endif
#endif
