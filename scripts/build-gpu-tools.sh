#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
component=all
while [[ $# -gt 0 ]]; do
  case "$1" in
    --component) [[ $# -ge 2 ]] || exit 2; component=$2; shift 2 ;;
    *) printf 'Usage: build-gpu-tools.sh [--component all|spirv-tools|spirv-llvm-translator|glslang|mesa]\n' >&2; exit 2 ;;
  esac
done
case "$component" in all|spirv-tools|spirv-llvm-translator|glslang|mesa) ;; *) printf 'gpu-tools: unknown component %s\n' "$component" >&2; exit 2 ;; esac
jobs=${AILO_JOBS:-2}
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { printf 'gpu-tools: AILO_JOBS must be positive\n' >&2; exit 2; }
for tool in cmake ninja cc c++ rg; do
  command -v "$tool" >/dev/null || { printf 'gpu-tools: required host tool missing: %s\n' "$tool" >&2; exit 2; }
done
if [[ "$component" == all || "$component" == spirv-llvm-translator || "$component" == mesa ]]; then
  llvm_config=$(command -v llvm-config-23 || command -v llvm-config || true)
  [[ -n "$llvm_config" ]] || { printf 'gpu-tools: LLVM 23 development tools are required for the pinned translator\n' >&2; exit 2; }
  llvm_version=$("$llvm_config" --version)
  [[ "$llvm_version" == 23.* ]] || { printf 'gpu-tools: pinned translator requires LLVM 23; found %s\n' "$llvm_version" >&2; exit 2; }
  llvm_dir=$("$llvm_config" --cmakedir)
  llvm_libdir=$("$llvm_config" --libdir)
fi
if [[ "$component" == all || "$component" == mesa ]]; then
  llvm_includedir=$("$llvm_config" --includedir)
  clang_version="$llvm_includedir/clang/Basic/Version.inc"
  [[ -f "$clang_version" ]] || { printf 'gpu-tools: Clang development headers missing in %s\n' "$llvm_includedir" >&2; exit 2; }
  rg -Fxq "#define CLANG_VERSION_STRING \"$llvm_version\"" "$clang_version" || { printf 'gpu-tools: Clang development headers must match LLVM %s\n' "$llvm_version" >&2; exit 2; }
  [[ -f "$llvm_libdir/libclangBasic.a" ]] || { printf 'gpu-tools: Clang %s static development libraries missing in %s\n' "$llvm_version" "$llvm_libdir" >&2; exit 2; }
fi
if [[ "$component" == all || "$component" == mesa ]]; then
  command -v meson >/dev/null || { printf 'gpu-tools: required host tool missing: meson\n' >&2; exit 2; }
fi

# Host generators have their own source/build/install roots. These native
# libraries are never target dependency roots or SDK payloads.
configure_native_dependency() {
  local name=$1
  shift
  "$root/scripts/deps.sh" --target host --component "$name"
  local base="$root/.cache/deps-build/host/$name"
  local prefix="$root/.cache/deps/host/$name/install"
  cmake -S "$base/source" -B "$base/build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE= -DCMAKE_C_COMPILER="$(command -v cc)" -DCMAKE_CXX_COMPILER="$(command -v c++)" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_INSTALL_MESSAGE=NEVER "$@"
}

native_dependency() {
  local name=$1
  configure_native_dependency "$@"
  cmake --build "$root/.cache/deps-build/host/$name/build" --parallel "$jobs"
  cmake --install "$root/.cache/deps-build/host/$name/build"
}

build_spirv_tools() {
  "$root/scripts/deps.sh" --target host --component spirv-tools-headers
  native_dependency spirv-tools \
    -DSPIRV-Headers_SOURCE_DIR="$root/.cache/deps-build/host/spirv-tools-headers/source" \
    -DSPIRV_SKIP_TESTS=ON -DSPIRV_SKIP_EXECUTABLES=ON -DSPIRV_WERROR=OFF \
    -DBUILD_SHARED_LIBS=OFF -DSPIRV_TOOLS_BUILD_STATIC=ON
}

build_translator() {
  "$root/scripts/deps.sh" --target x86_64-linux-gnu --component cpkt --component cpktmisc
  "$root/scripts/deps.sh" --target host --component spirv-tools-headers
  PKG_CONFIG_PATH= PKG_CONFIG_LIBDIR="$root/.cache/deps/x86_64-linux-gnu/cpkt/install/lib/pkgconfig:$root/.cache/deps/x86_64-linux-gnu/cpktmisc/install/lib/pkgconfig" \
  native_dependency spirv-llvm-translator \
    -DCMAKE_PROJECT_LLVM_SPIRV_INCLUDE="$root/cmake/AiloHostSdk.cmake" \
    -DAILO_HOST_LLVM_CONFIG="$llvm_config" \
    -DLLVM_DIR="$llvm_dir" -DLLVM_SPIRV_INCLUDE_TESTS=OFF \
    -DLLVM_EXTERNAL_SPIRV_HEADERS_SOURCE_DIR="$root/.cache/deps-build/host/spirv-tools-headers/source" \
    -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
    -DLLVM_SPIRV_ENABLE_LIBSPIRV_DIS=OFF -DLLVM_LINK_LLVM_DYLIB=OFF -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_DISABLE_FIND_PACKAGE_SPIRV-Tools=ON -DCMAKE_DISABLE_FIND_PACKAGE_SPIRV-Tools-tools=ON
}

build_glslang() {
  native_dependency glslang -DBUILD_EXTERNAL=OFF -DGLSLANG_TESTS=OFF \
    -DGLSLANG_ENABLE_INSTALL=ON -DENABLE_GLSLANG_BINARIES=ON -DENABLE_SPIRV=ON \
    -DENABLE_OPT=OFF -DENABLE_HLSL=OFF -DENABLE_PCH=OFF -DBUILD_SHARED_LIBS=OFF
}

build_mesa() {
  local sdk="$root/.cache/deps/host"
  local base="$root/.cache/deps-build/host/mesa"
  local prefix="$sdk/mesa/install"
  for prerequisite in spirv-tools/install/lib/pkgconfig/SPIRV-Tools.pc \
    spirv-llvm-translator/install/lib/pkgconfig/LLVMSPIRVLib.pc; do
    [[ -f "$sdk/$prerequisite" ]] || { printf 'gpu-tools: prepare host prerequisites with make gpu-tools: missing %s\n' "$prerequisite" >&2; exit 2; }
  done
  [[ -f "$root/build/host-gpu-sdk.ini" ]] || { printf 'gpu-tools: prepare host prerequisites with make gpu-tools: missing native SDK configuration\n' >&2; exit 2; }
  "$root/scripts/deps.sh" --target host --component mesa
  local configure=(setup)
  [[ ! -f "$base/build/meson-private/coredata.dat" ]] || configure+=(--reconfigure)
  local rtti
  case "$("$llvm_config" --has-rtti)" in
    YES) rtti=true ;;
    NO) rtti=false ;;
    *) printf 'gpu-tools: LLVM did not report its RTTI configuration\n' >&2; exit 2 ;;
  esac
  local core="$root/.cache/deps/x86_64-linux-gnu/cpkt/install"
  local misc="$root/.cache/deps/x86_64-linux-gnu/cpktmisc/install"
  env -u LLVM_CONFIG \
    LIBRARY_PATH="$llvm_libdir" \
    PKG_CONFIG_PATH= \
    PKG_CONFIG_LIBDIR="$sdk/spirv-tools/install/lib/pkgconfig:$sdk/spirv-llvm-translator/install/lib/pkgconfig:$core/lib/pkgconfig:$misc/lib/pkgconfig" \
    PKG_CONFIG_SYSROOT_DIR= \
    CPPFLAGS="-I$llvm_includedir -I$core/include -I$core/include/libxml2 -I$misc/include" \
    LDFLAGS="-L$llvm_libdir" \
    meson "${configure[@]}" "$base/build" "$base/source" --prefix "$prefix" \
    --native-file "$root/build/host-gpu-sdk.ini" \
    -Dcpp_rtti="$rtti" \
    --libdir lib --buildtype release --wrap-mode nofallback -Dauto_features=disabled \
    -Ddefault_library=static -Db_staticpic=true -Dplatforms=[] -Dvulkan-drivers=[] \
    -Dgallium-drivers=[] -Dopengl=false -Dgles1=disabled -Dgles2=disabled \
    -Degl=disabled -Dgbm=disabled -Dglx=disabled -Dllvm=enabled -Dshared-llvm=disabled \
    -Damd-use-llvm=false -Dspirv-tools=enabled -Dmesa-clc=enabled -Dinstall-mesa-clc=true \
    -Dmesa-clc-bundle-headers=enabled -Dshader-cache=disabled -Dxmlconfig=disabled \
    -Dzlib=disabled -Dzstd=disabled -Dexpat=disabled -Dbuild-tests=false
  meson compile -C "$base/build" -j "$jobs"
  meson install -C "$base/build" --no-rebuild --only-changed
}

case "$component" in
  all) build_spirv_tools; build_translator; build_glslang; build_mesa ;;
  spirv-tools) build_spirv_tools ;;
  spirv-llvm-translator) build_translator ;;
  glslang) build_glslang ;;
  mesa) build_mesa ;;
esac
