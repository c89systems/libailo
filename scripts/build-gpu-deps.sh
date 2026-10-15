#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
target=x86_64-linux-gnu
component=libdrm
while [[ $# -gt 0 ]]; do
  case "$1" in
    --target) [[ $# -ge 2 ]] || exit 2; target=$2; shift 2 ;;
    --component) [[ $# -ge 2 ]] || exit 2; component=$2; shift 2 ;;
    *) printf 'Usage: build-gpu-deps.sh [--target ID] [--component libdrm]\n' >&2; exit 2 ;;
  esac
done
[[ "$target" =~ ^(x86_64|aarch64)-linux-(gnu|musl)$ ]] || { printf 'gpu-deps: unsupported target %s\n' "$target" >&2; exit 2; }
[[ "$component" == libdrm ]] || { printf 'gpu-deps: unknown component %s\n' "$component" >&2; exit 2; }
jobs=${AILO_JOBS:-2}
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { printf 'gpu-deps: AILO_JOBS must be positive\n' >&2; exit 2; }
for tool in meson ninja pkg-config; do
  command -v "$tool" >/dev/null || { printf 'gpu-deps: required host tool missing: %s\n' "$tool" >&2; exit 2; }
done
"$root/scripts/cpkt-toolchains.sh" ensure "$target" >/dev/null
"$root/scripts/deps.sh" --target "$target" --component "$component"
cmake -DAILO_TARGET_ID="$target" -P "$root/cmake/AiloMesonToolchain.cmake"
base="$root/.cache/deps-build/$target/$component"
prefix="$root/.cache/deps/$target/$component/install"
cross="$root/build/deps/$target/meson.ini"
# Ambient host include/library paths must not enter target dependency discovery.
target_env=(env -u CFLAGS -u CXXFLAGS -u CPPFLAGS -u LDFLAGS -u CPATH
  -u C_INCLUDE_PATH -u CPLUS_INCLUDE_PATH -u LIBRARY_PATH
  PKG_CONFIG_PATH= PKG_CONFIG_LIBDIR= PKG_CONFIG_SYSROOT_DIR=)
configure=(setup)
[[ ! -f "$base/build/meson-private/coredata.dat" ]] || configure+=(--reconfigure)
"${target_env[@]}" meson "${configure[@]}" "$base/build" "$base/source" \
  --cross-file "$cross" --prefix "$prefix" --libdir lib --buildtype release \
  --wrap-mode nofallback -Dauto_features=disabled -Ddefault_library=static \
  -Db_staticpic=true -Dintel=disabled -Dradeon=disabled -Damdgpu=enabled \
  -Dnouveau=disabled -Dvmwgfx=disabled -Domap=disabled -Dexynos=disabled \
  -Dfreedreno=disabled -Dtegra=disabled -Dvc4=disabled -Detnaviv=disabled \
  -Dudev=false -Dtests=false -Dinstall-test-programs=false -Dman-pages=disabled \
  -Dcairo-tests=disabled -Dvalgrind=disabled -Dpkgconfig.relocatable=true
"${target_env[@]}" meson compile -C "$base/build" -j "$jobs"
"${target_env[@]}" meson install -C "$base/build" --no-rebuild
