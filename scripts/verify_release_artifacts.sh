#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# -eq 1 && -f "$1" ]] || { printf 'Usage: verify_release_artifacts.sh libailo-X.Y.Z-CHECKSUMS\n' >&2; exit 2; }
manifest=$(realpath -- "$1")
directory=${manifest%/*}
name=${manifest##*/}
[[ "$name" =~ ^libailo-([0-9]+\.[0-9]+\.[0-9]+)-CHECKSUMS$ ]] || { printf 'artifact-verify: invalid checksum manifest name\n' >&2; exit 2; }
version=${BASH_REMATCH[1]}
while IFS= read -r named; do
  [[ "$named" == "$name" ]] || { printf 'artifact-verify: stale checksum manifest\n' >&2; exit 1; }
done < <(find "$directory" -maxdepth 1 -type f -name 'libailo-*-CHECKSUMS' -printf '%f\n')
mkdir -p "$root/build/packaging"
work=$(mktemp -d "$root/build/packaging/verify-set.XXXXXXXX")
trap 'cmake -E rm -rf "$work"' EXIT
: > "$work/expected"
while IFS= read -r row; do
  [[ "$row" =~ ^[0-9a-f]{64}[[:space:]][[:space:]](.+)$ ]] || { printf 'artifact-verify: malformed checksum manifest\n' >&2; exit 1; }
  file=${BASH_REMATCH[1]}
  [[ "$file" != */* && "$file" == "libailo-$version"*.tar.gz && -f "$directory/$file" && ! -L "$directory/$file" ]] || { printf 'artifact-verify: missing/escaping artifact\n' >&2; exit 1; }
  printf '%s\n' "$file" >> "$work/expected"
done < "$manifest"
[[ -s "$work/expected" ]] || { printf 'artifact-verify: empty manifest\n' >&2; exit 1; }
LC_ALL=C sort "$work/expected" -o "$work/expected"
[[ -z $(uniq -d "$work/expected") ]] || { printf 'artifact-verify: duplicate artifact\n' >&2; exit 1; }
find "$directory" -maxdepth 1 -type f -name 'libailo-*.tar.gz' -printf '%f\n' | LC_ALL=C sort > "$work/actual"
diff -u "$work/expected" "$work/actual" || { printf 'artifact-verify: stale or unlisted artifact\n' >&2; exit 1; }
(cd "$directory" && sha256sum --strict -c "${manifest##*/}")
while IFS= read -r file; do
  if [[ "$file" == "libailo-$version.tar.gz" ]]; then
    "$root/scripts/verify_source_archive.sh" "$directory/$file"
  else
    "$root/scripts/verify_sdk_archive.sh" "$directory/$file"
  fi
done < "$work/expected"
printf 'Checksum-listed artifact set verified; upload selection must use this manifest plus the manifest itself\n'
