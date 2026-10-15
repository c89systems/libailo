#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
preset=debug
configure_only=false
while [[ $# -gt 0 ]]; do
  case "$1" in
    --preset) [[ $# -ge 2 ]] || { printf 'build: --preset requires a value\n' >&2; exit 2; }; preset=$2; shift 2 ;;
    --configure-only) configure_only=true; shift ;;
    *) printf 'Usage: scripts/build.sh [--preset name] [--configure-only]\n' >&2; exit 2 ;;
  esac
done
jobs=${AILO_JOBS:-2}
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { printf 'build: AILO_JOBS must be a positive integer\n' >&2; exit 2; }
[[ "$preset" =~ ^[a-zA-Z0-9_-]+$ ]] || { printf 'build: invalid preset name\n' >&2; exit 2; }
mkdir -p "$root/build/presets"
result="$root/build/presets/$preset.txt"
cmake -DAILO_PRESET="$preset" -DAILO_PRESET_RESULT="$result" -P "$root/cmake/AiloPreset.cmake"
target=x86_64-linux-gnu
vulkan=ON
while IFS='=' read -r key value; do
  case "$key" in target) target=$value ;; vulkan) vulkan=$value ;; esac
done < "$result"
cd -- "$root"
"$root/scripts/cpkt-toolchains.sh" ensure "$target" >/dev/null
if [[ "$vulkan" == ON ]]; then
  "$root/scripts/cpkt-toolchains.sh" ensure x86_64-linux-gnu >/dev/null
fi
"$root/scripts/deps.sh" --target "$target" --component cpkt --component cpktmisc --component lonejson --component pslog --component llama
source_root="$root/.cache/deps-build/$target/llama/source"
while IFS= read -r name; do
  [[ -n "$name" && "$name" != \#* ]] || continue
  patch_file="$root/vendor/llama/patches/$name"
  if ! patch --directory="$source_root" --dry-run --reverse --silent -p1 -i "$patch_file" >/dev/null 2>&1; then
    patch --directory="$source_root" --batch --forward -p1 -i "$patch_file"
  fi
done < "$root/vendor/llama/patches/series"
helper="$root/.cache/deps-build/$target/llama/build/ggml/src/ggml-vulkan/vulkan-shaders-gen-prefix"
cache="$helper/src/vulkan-shaders-gen-build/CMakeCache.txt"
if [[ -f "$cache" ]] && rg -q '^CMAKE_C(X{2})?_COMPILER:(FILEPATH|STRING)=/usr/' "$cache"; then
  cmake -E rm -rf "$helper"
fi

if [[ "$vulkan" == ON ]]; then
  "$root/scripts/build-gpu-deps.sh" --target "$target" --component vulkan-loader
fi
cmake --preset "$preset" "-DAILO_VERSION_OVERRIDE:STRING=${AILO_VERSION_OVERRIDE:-}"
if ! "$configure_only"; then
  cmake --build --preset "$preset" --parallel "$jobs"
fi
