#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# == 2 && "$1" =~ ^[a-zA-Z0-9_-]+$ && "$2" =~ ^[1-9][0-9]*$ ]] || {
  printf 'Usage: test_vulkan_memory.sh PRESET POSITIVE_RUN_COUNT\n' >&2; exit 2;
}
work="$root/build/$1"
rg -qi '^AILO_VULKAN:BOOL=(ON|TRUE|YES|Y|1)$' "$work/CMakeCache.txt" || {
  printf 'Shared-memory qualification requires a Vulkan build\n' >&2; exit 2;
}
cmake --build "$work" --target ailo_qualify_shared --parallel "${AILO_JOBS:-2}"
log="$work/vulkan-shared-memory.log"
: > "$log"
for ((run=1;run<=$2;run++)); do
  printf 'fresh_process=%d\n' "$run" >> "$log"
  if ! MESA_SHADER_CACHE_DISABLE=true timeout --signal=TERM --kill-after=5 60 \
      "$work/ailo_qualify_shared" >> "$log" 2>&1; then
    tail -40 "$log" >&2
    exit 1
  fi
done
printf 'Vulkan shared-memory qualification: %s fresh processes passed; %s\n' "$2" "$log"
