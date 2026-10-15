#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
target=x86_64-linux-gnu
component=libdrm
while [[ $# -gt 0 ]]; do
  case "$1" in
    --target) [[ $# -ge 2 ]] || exit 2; target=$2; shift 2 ;;
    --component) [[ $# -ge 2 ]] || exit 2; component=$2; shift 2 ;;
    *) printf 'Usage: build-gpu-deps.sh [--target ID] [--component libdrm|mesa|vulkan-loader]\n' >&2; exit 2 ;;
  esac
done
[[ "$target" =~ ^(x86_64|aarch64)-linux-(gnu|musl)$ ]] || { printf 'gpu-deps: unsupported target %s\n' "$target" >&2; exit 2; }
[[ "$component" == libdrm || "$component" == mesa || "$component" == vulkan-loader ]] || { printf 'gpu-deps: unknown component %s\n' "$component" >&2; exit 2; }
jobs=${AILO_JOBS:-2}
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { printf 'gpu-deps: AILO_JOBS must be positive\n' >&2; exit 2; }
for tool in cmake meson ninja pkg-config patch; do
  command -v "$tool" >/dev/null || { printf 'gpu-deps: required host tool missing: %s\n' "$tool" >&2; exit 2; }
done
if [[ "$component" == mesa ]]; then
  "$root/scripts/build-gpu-tools.sh"
  "$root/scripts/build-gpu-deps.sh" --target "$target" --component libdrm
fi
if [[ "$component" == vulkan-loader ]]; then
  "$root/scripts/build-gpu-deps.sh" --target "$target" --component mesa
fi
"$root/scripts/cpkt-toolchains.sh" ensure "$target" >/dev/null
"$root/scripts/deps.sh" --target "$target" --component "$component"
cmake -DAILO_TARGET_ID="$target" -P "$root/cmake/AiloMesonToolchain.cmake"
base="$root/.cache/deps-build/$target/$component"
prefix="$root/.cache/deps/$target/$component/install"
cross="$root/build/deps/$target/meson.ini"
target_env=(env -u CFLAGS -u CXXFLAGS -u CPPFLAGS -u LDFLAGS -u CPATH
  -u C_INCLUDE_PATH -u CPLUS_INCLUDE_PATH -u LIBRARY_PATH
  PKG_CONFIG_PATH= PKG_CONFIG_LIBDIR= PKG_CONFIG_SYSROOT_DIR=)
if [[ "$component" == vulkan-loader ]]; then
  configure_dependency() {
    local name=$1
    shift
    "$root/scripts/deps.sh" --target "$target" --component "$name"
    local source="$root/.cache/deps-build/$target/$name/source"
    local binary="$root/.cache/deps-build/$target/$name/build"
    local install="$root/.cache/deps/$target/$name/install"
    "${target_env[@]}" cmake -S "$source" -B "$binary" -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE="$root/cmake/AiloToolchain.cmake" \
      -DAILO_TARGET_ID="$target" -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$install" -DCMAKE_INSTALL_LIBDIR=lib \
      -DCMAKE_INSTALL_MESSAGE=NEVER \
      "-DCMAKE_C_FLAGS=-ffile-prefix-map=$root/.cache=ailo-dependencies" \
      "-DCMAKE_CXX_FLAGS=-ffile-prefix-map=$root/.cache=ailo-dependencies" "$@"
    "${target_env[@]}" cmake --build "$binary" --parallel "$jobs"
    "${target_env[@]}" cmake --install "$binary" --prefix "$install"
  }
  while IFS= read -r name; do
    [[ -n "$name" && "$name" != \#* ]] || continue
    patch_file="$root/vendor/vulkan-loader/patches/$name"
    if ! patch --directory="$base/source" --dry-run --reverse --silent -p1 -i "$patch_file" >/dev/null 2>&1; then
      patch --directory="$base/source" --batch --forward -p1 -i "$patch_file"
    fi
  done < "$root/vendor/vulkan-loader/patches/series"
  configure_dependency vulkan-headers -DVULKAN_HEADERS_ENABLE_TESTS=OFF
  configure_dependency spirv-headers -DSPIRV_HEADERS_ENABLE_TESTS=OFF
  configure_dependency vulkan-loader -DSYSCONFDIR=/etc \
    -DVulkanHeaders_DIR="$root/.cache/deps/$target/vulkan-headers/install/share/cmake/VulkanHeaders" \
    -DBUILD_TESTS=OFF -DBUILD_WERROR=OFF \
    -DBUILD_WSI_XCB_SUPPORT=OFF -DBUILD_WSI_XLIB_SUPPORT=OFF \
    -DBUILD_WSI_WAYLAND_SUPPORT=OFF -DBUILD_WSI_XLIB_XRANDR_SUPPORT=OFF
  cmake -E rm -f "$prefix"/lib/libvulkan.so*
  exit
fi
if [[ "$component" == mesa ]]; then
  while IFS= read -r name; do
    [[ -n "$name" && "$name" != \#* ]] || continue
    patch_file="$root/vendor/mesa/patches/$name"
    if ! patch --directory="$base/source" --dry-run --reverse --silent -p1 -i "$patch_file" >/dev/null 2>&1; then
      patch --directory="$base/source" --batch --forward -p1 -i "$patch_file"
    fi
  done < "$root/vendor/mesa/patches/series"
  mkdir -p "$base/source/ailo-static"
  cmake -E copy_if_different "$root/cmake/mesa-static.build" "$base/source/ailo-static/meson.build"
  cmake -E copy_if_different "$root/tests/static_icds.c" "$base/source/ailo-static/static_icds.c"
  cmake -DAILO_TARGET_ID="$target" -P "$root/cmake/AiloMesaIdentity.cmake"
  configure=(setup)
  [[ ! -f "$base/build/meson-private/coredata.dat" ]] || configure+=(--reconfigure)
  "${target_env[@]}" meson "${configure[@]}" "$base/build" "$base/source" \
    --cross-file "$cross" --native-file "$root/build/deps/$target/native.ini" \
    --prefix "$prefix" --libdir lib --buildtype release --wrap-mode nofallback \
    -Dauto_features=disabled -Ddefault_library=static -Db_staticpic=true \
    -Dplatforms=[] -Dvulkan-drivers=amd,intel -Dgallium-drivers=[] \
    -Dopengl=false -Dgles1=disabled -Dgles2=disabled -Degl=disabled -Dgbm=disabled \
    -Dglx=disabled -Dllvm=disabled -Damd-use-llvm=false -Dmesa-clc=system \
    -Dprecomp-compiler=system -Dspirv-tools=disabled -Dshader-cache=disabled \
    -Dxmlconfig=disabled -Dzlib=disabled -Dzstd=disabled -Dexpat=disabled \
    -Dlibunwind=disabled -Dintel-rt=disabled -Dperfetto=false -Ddatasources=[] \
    -Dradv-u_trace=false -Dbuild-tests=false -Dtools=[] -Dvulkan-layers=[] \
    -Dvideo-codecs=[] -Dpkgconfig.relocatable=true
  "${target_env[@]}" meson compile -C "$base/build" -j "$jobs"
  "${target_env[@]}" meson install -C "$base/build" --no-rebuild --only-changed
  # Upstream's filesystem ICD manifests describe shared drivers, not this
  # built-in static registry profile. Keep them out of the prepared payload.
  cmake -E rm -rf "$prefix/share/vulkan/icd.d"
  exit
fi
patch_file="$root/vendor/libdrm/patches/0001-embed-marketing-names.patch"
if ! patch --directory="$base/source" --dry-run --reverse --silent -p1 -i "$patch_file" >/dev/null 2>&1; then
  patch --directory="$base/source" --batch --forward -p1 -i "$patch_file"
fi
cmake -E copy_if_different "$root/cmake/AiloEmbedDrmIds.cmake" "$base/source/amdgpu/ailo_embed_ids.cmake"
# Ambient host include/library paths must not enter target dependency discovery.
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
"${target_env[@]}" meson install -C "$base/build" --no-rebuild --only-changed
# This data file is no longer part of this profile, including in an existing
# owned dependency install prepared before the embedded-table cutover.
cmake -E rm -f "$prefix/share/libdrm/amdgpu.ids"
