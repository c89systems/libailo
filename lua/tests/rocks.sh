#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 9 ]] || { printf 'Usage: rocks.sh source provider sdk cpkt pslog interop toolchain runner fixture\n' >&2; exit 2; }
source=$1 provider=$2 sdk=$3 cpkt=$4 pslog=$5 interop=$6 toolchain=$7 runner=$8 fixture=$9
mkdir -p "$PWD/rocks-tests"
work=$(mktemp -d "$PWD/rocks-tests/install.XXXXXXXX")
trap 'cmake -E rm -rf "$work"' EXIT
printf 'local_cache = [[%s/cache]]\n' "$work" > "$work/config.lua"
mkdir "$work/tmp"
export TMPDIR="$work/tmp"
export LUAROCKS_CONFIG="$work/config.lua" LUAROCKS_CONFIG_5_5="$work/config.lua"
# Host LuaRocks is tooling; all target compilation/runtime uses selected SDKs.
cc=$(sed -n 's/^CMAKE_C_COMPILER:FILEPATH=//p' CMakeCache.txt)
[[ -x "$cc" ]] || { printf 'rocks-test: missing selected target compiler\n' >&2; exit 1; }
cp -R "$provider" "$work/provider"
(cd "$work/provider" && bash lua/scripts/render_release_rockspec.sh 0.12.0 "$work/provider/lua-pslog-0.12.0-1.rockspec")
(cd "$work/provider" && luarocks --lua-version=5.5 --tree "$work/tree" make lua-pslog-0.12.0-1.rockspec --deps-mode=one CC="$cc" LD="$cc" LUA_INCDIR="$cpkt/include" LIBPSLOG_DIR="$pslog")
mkdir "$work/ailo"
while IFS= read -r file; do
  mkdir -p "$work/ailo/$(dirname "$file")"
  cp "$source/$file" "$work/ailo/$file"
done < "$source/source-manifest.txt"
version=$(sed -n 's/^#define AILO_VERSION "\([^"]*\)"/\1/p' "$sdk/include/ailo/version.h")
printf '%s\n' "$version" > "$work/ailo/VERSION"
sed "s/@VERSION@/$version/g" "$source/lua-ailo.rockspec.in" > "$work/ailo/lua-ailo-$version-1.rockspec"
(cd "$work/ailo" && CMAKE_TOOLCHAIN_FILE="$toolchain" CMAKE_BUILD_PARALLEL_LEVEL=1 luarocks --lua-version=5.5 --tree "$work/tree" make "lua-ailo-$version-1.rockspec" --deps-mode=one CC="$cc" LUA_INCDIR="$cpkt/include" AILO_DIR="$sdk" PSLOG_DIR="$pslog" CPKTLUA_DIR="$cpkt" PSLOG_LUA_INCDIR="$interop")
[[ -f "$work/tree/share/lua/5.5/ailo/init.lua" && -f "$work/tree/lib/lua/5.5/ailo/core.so" ]] || { printf 'rocks-test: modules were not deployed into the Lua search paths\n' >&2; exit 1; }
mkdir "$work/state"
"$runner" "$source/tests/receiver.lua" "$work/tree/share/lua/5.5/?.lua;$work/tree/share/lua/5.5/?/init.lua" "$work/tree/lib/lua/5.5/?.so" "$fixture" "$work/state" "$source/tests/record_repository.lua"
printf 'LuaRocks dependency resolution, native compilation, deployment and pinned runtime imports: passed\n'
