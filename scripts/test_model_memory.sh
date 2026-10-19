#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 0 ]] || { printf 'Usage: test_model_memory.sh\n' >&2; exit 2; }
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
command -v valgrind >/dev/null || { printf 'model-memory: host Valgrind is required\n' >&2; exit 2; }
"$root/scripts/build.sh" --preset cpu
work="$root/build/cpu"
# Use the native CPU build to avoid process-lifetime allocations from the Vulkan
# loader/ICD. This gate does not claim to check the host GPU driver for leaks.
if ! rg -qi '^AILO_VULKAN:BOOL=(OFF|FALSE|NO|N|0)$' "$work/CMakeCache.txt"; then
  printf 'model-memory: the CPU-only native build is required\n' >&2
  exit 2
fi
ctest --test-dir "$work" --output-on-failure -j1 -R '^model-fixture$'
for executable in ailo_model_test ailo_model_shared_test ailo_client_static_test ailo_client_shared_test ailo_client_threads_test ailo_callbacks_static_test ailo_callbacks_shared_test; do
  valgrind --fair-sched=yes --suppressions="$root/scripts/valgrind-libffi.supp" --error-exitcode=99 --leak-check=full --track-origins=yes \
    --show-leak-kinds=all --errors-for-leak-kinds=all \
    "$work/$executable" "$work/model-fixtures/tiny-llama.gguf"
done

valgrind --error-exitcode=99 --leak-check=full --track-origins=yes \
  --show-leak-kinds=all --errors-for-leak-kinds=all \
  "$work/ailo_media_test" "$work/model-fixtures/tiny-llama.gguf" "$work/media-fixtures"

mkdir -p "$work/logging-tests"
valgrind --fair-sched=yes --error-exitcode=99 --leak-check=full --track-origins=yes \
  --show-leak-kinds=all --errors-for-leak-kinds=all \
  "$work/ailo_logging_test" "$work/model-fixtures/tiny-llama.gguf" "$work/logging-tests"
