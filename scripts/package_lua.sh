#!/usr/bin/env bash
set -euo pipefail
# Artifact permissions are independent of the invoking account's umask.
umask 022
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
for tool in zip unzip luarocks; do
  command -v "$tool" >/dev/null || { printf 'lua-package: required host tool missing: %s\n' "$tool" >&2; exit 2; }
done
development=false
output=
while [[ $# -gt 0 ]]; do
  case "$1" in --development) development=true; shift ;; --output-directory) [[ $# -ge 2 ]] || exit 2; output=$2; shift 2 ;; *) printf 'Usage: package_lua.sh [--development --output-directory build/path]\n' >&2; exit 2 ;; esac
done
if "$development"; then
  [[ -n "$output" ]] || { printf 'lua-package: explicit development directory required\n' >&2; exit 2; }
  version=$("$root/scripts/release_version.sh")
else
  version=$("$root/scripts/release_version.sh" --require-release)
  python3 "$root/scripts/generate_native.py" --check --require-complete
  output=${output:-"$root/dist"}
fi
output=$(realpath -m -- "$output")
case "$output" in "$root/build/"*|"$root/dist") ;; *) printf 'lua-package: output must be under build/ or dist/\n' >&2; exit 2 ;; esac
if "$development" && [[ "$output" != "$root/build/"* ]]; then exit 2; fi
mkdir -p "$root/build/lua-packaging" "$output"
work=$(mktemp -d "$root/build/lua-packaging/stage.XXXXXXXX")
trap 'cmake -E rm -rf "$work"' EXIT
name="lua-ailo-$version"
mkdir "$work/$name"
while IFS= read -r file; do
  [[ "$file" != /* && ! "$file" =~ (^|/)\.\.?(/|$) && -f "$root/lua/$file" && ! -L "$root/lua/$file" ]] || exit 1
  mkdir -p "$work/$name/$(dirname "$file")"
  cp "$root/lua/$file" "$work/$name/$file"
done < "$root/lua/source-manifest.txt"
cp "$root/LICENSE" "$work/$name/LICENSE"
printf '%s\n' "$version" > "$work/$name/VERSION"
(cd "$work/$name" && { cat source-manifest.txt; printf 'LICENSE\nVERSION\n'; } | LC_ALL=C sort | while IFS= read -r file; do sha256sum "$file"; done > SOURCE-MANIFEST.sha256)
tar --sort=name --mtime="@${SOURCE_DATE_EPOCH:-0}" --owner=0 --group=0 --numeric-owner -C "$work" -cf - "$name" | gzip -n > "$work/$name.tar.gz"
sed "s/@VERSION@/$version/g" "$root/lua/lua-ailo.rockspec.in" > "$work/$name-1.rockspec"
zip_epoch=${SOURCE_DATE_EPOCH:-315532800}
(( zip_epoch >= 315532800 )) || zip_epoch=315532800
touch -d "@$zip_epoch" "$work/$name.tar.gz" "$work/$name-1.rockspec"
(cd "$work" && TZ=UTC zip -X -q "$name-1.src.rock" "$name-1.rockspec" "$name.tar.gz")
for file in "$name.tar.gz" "$name-1.rockspec" "$name-1.src.rock"; do
  "$root/scripts/verify_lua_artifact.sh" "$work/$file"
  mv "$work/$file" "$output/$file"
done
printf 'Lua source, rockspec and source rock: %s\n' "$output"
