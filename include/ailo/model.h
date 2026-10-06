#ifndef AILO_MODEL_H
#define AILO_MODEL_H
#include "ailo/base.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Synchronous progress/cancellation. Return zero to cancel, nonzero to
 * continue. Shards are numbered from 1. Total zero means unknown. Counts are
 * per shard/attempt; validation and lock waiting may also invoke this callback.
 * Arguments/user_data are borrowed for the call; do not mutate the source. */
typedef int (*ailo_model_progress)(void *user_data, unsigned int shard,
                                   unsigned int shards, ailo_u64 received,
                                   ailo_u64 total);

/** Zero-initializable acquisition options. Choose local_path OR the complete
 * repo/revision/filename triple. Strings/arrays remain borrowed during the
 * synchronous operation. No environment token lookup or implicit download.
 */
typedef struct ailo_model_source {
  /** Caller-owned local path; relative paths resolve against the current cwd.
   * Local files are validated in place, never copied or overwritten. */
  const char *local_path;
  /** Hugging Face namespace/repository, e.g. "Qwen/Qwen2.5-3B-Instruct-GGUF".
   */
  const char *repo;
  /** Explicit revision, preferably an immutable commit. No implicit "main". */
  const char *revision;
  /** Relative filename; nested directories are supported, traversal rejected.
   * Components beginning .ailo- are reserved for cache implementation state. */
  const char *filename;
  /** Absolute override. NULL/empty selects absolute XDG_CACHE_HOME/ailo, then
   * HOME/.cache/ailo. Nonempty relative environment paths fail. */
  const char *cache_root;
  /** Optional SHA-256 array, exactly one entry per requested file. NULL entries
   * omit that file's checksum. Hex is case insensitive. Count zero requires
   * a NULL array and omits all checksums.
   */
  const char *const *sha256;
  size_t sha256_count;
  /** Missing cached files fail unless nonzero. Existing invalid files fail
   * without replacement, even when downloads are allowed. */
  ailo_bool allow_download;
  /** Fetch/validate all siblings of a *-00001-of-NNNNN.gguf primary file.
   * A failure may leave earlier complete shards cached; never a partial final.
   */
  ailo_bool split_shards;
  /** Explicit bearer token; never forwarded across origins or put in errors. */
  const char *bearer_token;
  /** Per-file limit. Zero defaults to 32 GiB; native-width conversion is exact.
   */
  ailo_u64 max_bytes;
  /** Zero defaults to 15000 ms. Also bounds waiting for a publication lock. */
  long connect_timeout_ms;
  /** Zero defaults to 3600000 ms per file attempt, including redirects. */
  long transfer_timeout_ms;
  /** Zero defaults to 3 attempts, 1 disables retries; maximum 6. */
  unsigned int max_attempts;
  /** Optional progress/cancellation with borrowed user state. */
  ailo_model_progress progress;
  void *user_data;
} ailo_model_source;

/** Resolve the primary path and validate options. Reads cwd/cache environment,
 * creates no files, makes no network requests. Returned string is owned;
 * release with ailo_string_free. Output is NULL on failure. */
ailo_status ailo_model_cache_path(const ailo_model_source *source,
                                  char **output, ailo_error *error);

/** Validate local/cached GGUF files or acquire explicitly permitted missing
 * files over verified HTTPS. Streams bytes through bounded buffers to an
 * exclusive partial file, checks header/size/checksum, then atomically
 * publishes without replacing an existing name. Maximum five HTTPS redirects.
 * No resume. Cache directories/files must be real directories/regular files,
 * not symlinks. Success returns an owned primary path; release with
 * ailo_string_free. Reserved .ailo-partial-<target-sha256>-<nonce> files left
 * by process death are removed on the next permitted download of that target,
 * under its lock. Header checks do not establish model architecture or
 * inference capability. */
ailo_status ailo_model_acquire(const ailo_model_source *source, char **output,
                               ailo_error *error);

#ifdef __cplusplus
}
#endif
#endif
