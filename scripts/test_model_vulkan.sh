#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# -eq 1 && "$1" =~ ^[a-zA-Z0-9_-]+$ ]] || {
  printf 'Usage: test_model_vulkan.sh preset\n' >&2
  exit 2
}
work="$root/build/$1"
if ! rg -qi '^AILO_VULKAN:BOOL=(ON|TRUE|YES|Y|1)$' "$work/CMakeCache.txt"; then
  printf 'model-vulkan: preset %s must enable Vulkan\n' "$1" >&2
  exit 2
fi
ctest --test-dir "$work" --output-on-failure -j1 -R '^model-fixture$'
log="$work/model-vulkan.log"
result=0
"$work/ailo_model_test" "$work/model-fixtures/tiny-llama.gguf" --vulkan >"$log" 2>&1 || result=$?
if [[ "$result" -ne 0 ]]; then
  tail -40 "$log" >&2
  printf 'model-vulkan: actual Vulkan inference is required (status %s)\n' "$result" >&2
  exit 1
fi
if ! rg -q 'offloaded 2/2 layers to GPU' "$log" ||
   ! rg -q 'Vulkan[0-9]+ KV buffer size' "$log"; then
  printf 'model-vulkan: missing native evidence of model and KV offload; inspect %s\n' "$log" >&2
  exit 1
fi
rg 'offloaded 2/2|Vulkan[0-9]+ KV buffer size|C89 Vulkan GGUF' "$log"
