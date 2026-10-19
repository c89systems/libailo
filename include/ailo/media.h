#ifndef AILO_MEDIA_H
#define AILO_MEDIA_H
#include "ailo/base.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ailo_media_detail {
  AILO_MEDIA_DETAIL_AUTO = 0,
  AILO_MEDIA_DETAIL_LOW,
  AILO_MEDIA_DETAIL_HIGH,
  AILO_MEDIA_DETAIL_ORIGINAL
} ailo_media_detail;
typedef enum ailo_media_reference {
  AILO_MEDIA_INLINE = 0,
  AILO_MEDIA_URL,
  AILO_MEDIA_FILE_ID
} ailo_media_reference;
/** Optional borrowed media description. format is jpeg/png/wav/mp3/flac or
 * NULL for detection. A reference with absent bytes is resolved only during a
 * receiver operation; append-only continuations require resolved bytes. */
typedef struct ailo_media_metadata {
  const char *format;
  ailo_media_detail detail;
  ailo_media_reference reference;
  const char *value;
} ailo_media_metadata;
typedef int (*ailo_media_sink)(void *, const void *, size_t);
/** Application-owned synchronous resolution. Enforce timeout_ms including
 * connection/read work; libailo also checks elapsed time and cancellation at
 * each sink call. Honor max_bytes. The sink consumes borrowed chunks and
 * returns 1 to continue, 0 to cancel, -1 on failure. No exceptions/longjmp.
 * References are untrusted; access/network policy belongs to the resolver. */
typedef ailo_status (*ailo_media_resolver)(void *, ailo_media_reference,
                                           const char *, size_t max_bytes,
                                           long timeout_ms, ailo_media_sink,
                                           void *, ailo_error *);

#ifdef __cplusplus
}
#endif
#endif
