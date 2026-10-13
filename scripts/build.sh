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
"$root/scripts/deps.sh" --target "$target" --component cpkt --component cpktmisc --component lonejson --component llama
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

configure_dependency() {
  local name=$1
  shift
  "$root/scripts/deps.sh" --target "$target" --component "$name"
  local base="$root/.cache/deps-build/$target/$name"
  local install="$root/.cache/deps/$target/$name/install"
  cmake -S "$base/source" -B "$base/build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$root/cmake/AiloToolchain.cmake" \
    -DAILO_TARGET_ID="$target" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$install" -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_INSTALL_MESSAGE=NEVER \
    "-DCMAKE_C_FLAGS=-ffile-prefix-map=$root/.cache/deps-build/$target/$name/source=$name -ffile-prefix-map=$root/.cache/deps-build/$target/$name/build=$name-generated" \
    "-DCMAKE_CXX_FLAGS=-ffile-prefix-map=$root/.cache/deps-build/$target/$name/source=$name -ffile-prefix-map=$root/.cache/deps-build/$target/$name/build=$name-generated" "$@"
  cmake --build "$base/build" --parallel "$jobs"
  cmake --install "$base/build"
}

if [[ "$vulkan" == ON ]]; then
  configure_dependency vulkan-headers -DVULKAN_HEADERS_ENABLE_TESTS=OFF
  configure_dependency spirv-headers -DSPIRV_HEADERS_ENABLE_TESTS=OFF
  configure_dependency vulkan-loader \
    -DSYSCONFDIR=/etc \
    -DVulkanHeaders_DIR="$root/.cache/deps/$target/vulkan-headers/install/share/cmake/VulkanHeaders" \
    -DBUILD_TESTS=OFF -DBUILD_WERROR=OFF \
    -DBUILD_WSI_XCB_SUPPORT=OFF -DBUILD_WSI_XLIB_SUPPORT=OFF \
    -DBUILD_WSI_WAYLAND_SUPPORT=OFF -DBUILD_WSI_XLIB_XRANDR_SUPPORT=OFF
fi
cmake --preset "$preset" "-DAILO_VERSION_OVERRIDE:STRING=${AILO_VERSION_OVERRIDE:-}"
if ! "$configure_only"; then
  cmake --build --preset "$preset" --parallel "$jobs"
fi
