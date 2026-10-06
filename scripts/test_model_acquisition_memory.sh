#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# -eq 1 && "$1" =~ ^[a-zA-Z0-9_-]+$ ]] || {
  printf 'Usage: test_model_acquisition_memory.sh native-preset\n' >&2
  exit 2
}
work="$root/build/$1"
command -v valgrind >/dev/null || { printf 'model-acquisition: host Valgrind is required\n' >&2; exit 2; }
if [[ $(uname -s) != Linux || $(uname -m) != x86_64 ]] ||
   ! rg -q '^AILO_TARGET_ID:[^=]+=x86_64-linux-gnu$' "$work/CMakeCache.txt"; then
  printf 'model-acquisition: native x86_64 Linux GNU build is required\n' >&2
  exit 2
fi
ctest --test-dir "$work" --output-on-failure -j1 -R '^model-fixture$'
for executable in ailo_model_cache_test ailo_model_cache_shared_test; do
  valgrind --error-exitcode=99 --leak-check=full --track-origins=yes \
    --show-leak-kinds=all --errors-for-leak-kinds=all \
    "$work/$executable" "$work/model-fixtures/tiny-llama.gguf" "$work/memcheck-offline-cache"
done
python3 "$root/tests/test_model_download.py" --valgrind \
  --driver "$work/ailo_model_download_driver" --fixture "$work/model-fixtures/tiny-llama.gguf" \
  --model-consumer "$work/ailo_model_test" \
  ModelDownload.test_download_offline_hit_and_no_network \
  ModelDownload.test_http_errors_wrong_content_checksum_truncation_and_limits \
  ModelDownload.test_cancel_and_callback_exception_cleanup \
  ModelDownload.test_redirects_tls_and_credential_isolation \
  ModelDownload.test_concurrent_process_publication_and_lock_cancellation \
  ModelDownload.test_independent_threads_share_transport_lifetime
