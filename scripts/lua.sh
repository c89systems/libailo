#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
action=${1:-test}
preset=${2:-cpu}
[[ $# -le 2 && "$action" =~ ^(build|test)$ && "$preset" =~ ^(cpu|debug|x86_64-linux-gnu-release)$ ]] || { printf 'Usage: lua.sh [build|test] [cpu|debug]\n' >&2; exit 2; }
cd "$root"
scripts/build.sh --preset "$preset"
scripts/deps.sh --target x86_64-linux-gnu --component lua-pslog
binary="$root/build/$preset"
work="$root/build/lua-$preset"
dep="$root/.cache/deps/x86_64-linux-gnu"
mkdir -p "$work/interop" "$work/state"
# The facade interop header is separate from the provider C SDK headers.
cp "$root/.cache/deps-build/x86_64-linux-gnu/lua-pslog/source/include/pslog_lua.h" "$work/interop/"
cmake --install "$binary" --prefix "$work/sdk" --component ailo > "$work/install-core.log"
ctest --test-dir "$binary" --output-on-failure -j1 -R '^(model-fixture|compaction-replay)$'
loader=
# Read the selected compiler collection through the declared resolver.
description=$(scripts/cpkt-toolchains.sh discover x86_64-linux-gnu)
cc= cxx= sysroot=
while IFS='=' read -r key value; do case "$key" in cc) cc=$value ;; cxx) cxx=$value ;; sysroot) sysroot=$value ;; esac; done <<< "$description"
loaders=("$sysroot"/lib/ld-linux*.so*)
[[ ${#loaders[@]} -eq 1 && -f ${loaders[0]} ]] || { printf 'lua: expected one pinned loader\n' >&2; exit 1; }
loader=${loaders[0]}
runtime="$sysroot/lib;$sysroot/usr/lib;$sysroot/usr/lib64;$work/sdk/lib;$work/sdk/lib/ailo/native;$dep/cpkt/install/lib;$dep/cpktmisc/install/lib;$dep/lonejson/install/lib;$dep/pslog/install/lib"
for lib in libstdc++.so.6 libgcc_s.so.1; do
  file=$("$cxx" -print-file-name="$lib")
  runtime+=";$(dirname "$(realpath "$file")")"
done
cmake -S lua -B "$work/build" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$root/cmake/AiloToolchain.cmake" -DAILO_TARGET_ID=x86_64-linux-gnu \
  "-DCMAKE_PREFIX_PATH=$work/sdk;$dep/cpkt/install;$dep/pslog/install" \
  "-DPSLOG_LUA_INCLUDE_DIR=$work/interop" -DCMAKE_BUILD_TYPE=Debug \
  "-DCMAKE_INSTALL_PREFIX=$work/install" -DAILO_LUA_TESTS=ON \
  "-DAILO_LUA_PROVIDER_SOURCE=$root/.cache/deps-build/x86_64-linux-gnu/lua-pslog/source" \
  "-DAILO_LUA_RUNTIME_PATHS=$runtime" "-DAILO_LUA_LOADER=$loader" \
  "-DAILO_LUA_MODEL=$binary/model-fixtures/tiny-llama.gguf" "-DAILO_LUA_WORK=$work/state" \
  "-DCMAKE_C_FLAGS=-ffile-prefix-map=$root=libailo"
cmake --build "$work/build" --parallel "${AILO_JOBS:-2}"
cmake --install "$work/build"
if [[ "$action" == test ]]; then ctest --test-dir "$work/build" --output-on-failure -j1; fi
