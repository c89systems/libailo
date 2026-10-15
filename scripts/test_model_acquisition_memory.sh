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
bash "$root/tests/test_model_download.sh" "$work/ailo_model_download_driver" \
  "$work/model-fixtures/tiny-llama.gguf" "$work/ailo_model_test" python3 --valgrind
