#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$root/scripts/version_parser.sh"
[[ $# -eq 3 ]] || { printf 'Usage: package_checksums.sh generated-directory X.Y.Z artifact-name-list\n' >&2; exit 2; }
directory=$(realpath -- "$1")
version=$2
list=$3
ailo_valid_version "$version" || { printf 'checksums: invalid version\n' >&2; exit 2; }
case "$directory" in "$root/build/"*|"$root/dist") ;; *) printf 'checksums: directory must be repository build/ or dist/\n' >&2; exit 2 ;; esac
[[ -f "$list" ]] || { printf 'checksums: explicit artifact list is required\n' >&2; exit 2; }
mkdir -p "$root/build/packaging"
work=$(mktemp -d "$root/build/packaging/checksums.XXXXXXXX")
trap 'cmake -E rm -rf "$work"' EXIT
files=()
while IFS= read -r named; do
  [[ "$named" == "libailo-$version-CHECKSUMS" ]] || { printf 'checksums: stale checksum manifest\n' >&2; exit 2; }
done < <(find "$directory" -maxdepth 1 -type f -name 'libailo-*-CHECKSUMS' -printf '%f\n')
while IFS= read -r file || [[ -n "$file" ]]; do
  [[ -n "$file" ]] || continue
  if [[ "$file" == */* || ! ( "$file" == "libailo-$version"*.tar.gz || "$file" == "lua-ailo-$version.tar.gz" || "$file" == "lua-ailo-$version-1.rockspec" || "$file" == "lua-ailo-$version-1.src.rock" ) || ! -f "$directory/$file" || -L "$directory/$file" ]]; then
    printf 'checksums: missing/invalid artifact name: %s\n' "$file" >&2; exit 2
  fi
  files+=("$file")
done < "$list"
[[ ${#files[@]} -gt 0 ]] || { printf 'checksums: no artifacts selected\n' >&2; exit 2; }
printf '%s\n' "${files[@]}" | LC_ALL=C sort > "$work/expected"
if [[ -n $(uniq -d "$work/expected") ]]; then printf 'checksums: duplicate artifact\n' >&2; exit 2; fi
find "$directory" -maxdepth 1 -type f \( -name 'libailo-*.tar.gz' -o -name 'lua-ailo-*.tar.gz' -o -name 'lua-ailo-*.rockspec' -o -name 'lua-ailo-*.src.rock' \) -printf '%f\n' | LC_ALL=C sort > "$work/actual"
diff -u "$work/expected" "$work/actual" || { printf 'checksums: stale or unlisted source/SDK artifact\n' >&2; exit 2; }
(
  cd -- "$directory"
  while IFS= read -r file; do sha256sum -- "$file"; done < "$work/expected"
) > "$work/manifest"
mv -- "$work/manifest" "$directory/libailo-$version-CHECKSUMS"
printf '%s\n' "$directory/libailo-$version-CHECKSUMS"
