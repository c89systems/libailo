#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$root/build/lua-packaging"
work=$(mktemp -d "$root/build/lua-packaging/tests.XXXXXXXX")
trap 'cmake -E rm -rf "$work"' EXIT
version=$("$root/scripts/release_version.sh")
name="lua-ailo-$version"
reject() {
  if "$@" > "$work/rejection.log" 2>&1; then printf 'lua-package-test: accepted invalid artifact\n' >&2; exit 1; fi
}
(umask 022; "$root/scripts/package_lua.sh" --development --output-directory "$work/one")
(umask 077; "$root/scripts/package_lua.sh" --development --output-directory "$work/two")
for suffix in .tar.gz -1.rockspec -1.src.rock; do cmp "$work/one/$name$suffix" "$work/two/$name$suffix"; done
printf '%s\n' "$name.tar.gz" "$name-1.rockspec" "$name-1.src.rock" > "$work/list"
"$root/scripts/package_checksums.sh" "$work/one" "$version" "$work/list"
"$root/scripts/verify_release_artifacts.sh" "$work/one/libailo-$version-CHECKSUMS"
cp "$work/one/$name-1.rockspec" "$work/one/lua-ailo-99.99.99-1.rockspec"
reject "$root/scripts/package_checksums.sh" "$work/one" "$version" "$work/list"
mkdir "$work/bad"
tar -xzf "$work/two/$name.tar.gz" -C "$work/bad"
printf 'forbidden\n' > "$work/bad/$name/hidden-private.a"
tar -czf "$work/bad/$name.tar.gz" -C "$work/bad" "$name"
reject "$root/scripts/verify_lua_artifact.sh" "$work/bad/$name.tar.gz"
cp "$work/two/$name-1.rockspec" "$work/bad/"
(cd "$work/bad" && zip -X -q "$name-1.src.rock" "$name-1.rockspec" "$name.tar.gz")
reject "$root/scripts/verify_lua_artifact.sh" "$work/bad/$name-1.src.rock"
printf '\n-- file:///private/developer\n' >> "$work/bad/$name-1.rockspec"
reject "$root/scripts/verify_lua_artifact.sh" "$work/bad/$name-1.rockspec"
printf 'Lua deterministic source/rock products, complete checksums and nested negative scans: passed\n'
