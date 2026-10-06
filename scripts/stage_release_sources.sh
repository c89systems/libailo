#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$root/scripts/version_parser.sh"
[[ $# -eq 2 ]] || { printf 'Usage: stage_release_sources.sh generated-destination X.Y.Z\n' >&2; exit 2; }
destination=$(realpath -m -- "$1")
version=$2
ailo_valid_version "$version" || { printf 'source-stage: invalid version\n' >&2; exit 2; }
case "$destination" in "$root/build/"*|"$root/dist/"*) ;; *) printf 'source-stage: destination must be under repository build/ or dist/\n' >&2; exit 2 ;; esac
[[ ! -e "$destination" ]] || { printf 'source-stage: destination already exists\n' >&2; exit 2; }
manifest="$root/cmake/source-manifest.txt"
files=()
while IFS= read -r file || [[ -n "$file" ]]; do
  [[ -n "$file" && "$file" != \#* ]] || continue
  if [[ "$file" == /* || "$file" =~ (^|/)\.\.?(/|$) || "$file" == .git/* || "$file" == .cache/* || "$file" == build/* || "$file" == dist/* || "$file" == stash/* || "$file" == VERSION || "$file" == SOURCE-MANIFEST.sha256 ]]; then
    printf 'source-stage: forbidden manifest path: %s\n' "$file" >&2
    exit 2
  fi
  [[ -f "$root/$file" && ! -L "$root/$file" ]] || { printf 'source-stage: missing/nonregular input: %s\n' "$file" >&2; exit 2; }
  files+=("$file")
done < "$manifest"
[[ ${#files[@]} -gt 0 ]] || { printf 'source-stage: empty manifest\n' >&2; exit 2; }
if [[ -e "$root/.git" ]]; then
  while IFS= read -r tracked; do
    present=false
    for file in "${files[@]}"; do [[ "$file" != "$tracked" ]] || present=true; done
    if ! "$present"; then
      printf 'source-stage: tracked source omitted from explicit manifest: %s\n' "$tracked" >&2
      exit 2
    fi
  done < <(git -C "$root" ls-files --cached)
fi
mkdir -p "$destination"
for file in "${files[@]}"; do
  mkdir -p "$destination/$(dirname -- "$file")"
  cp -p -- "$root/$file" "$destination/$file"
done
printf '%s\n' "$version" > "$destination/VERSION"
(
  cd -- "$destination"
  printf '%s\n' "${files[@]}" VERSION | LC_ALL=C sort | while IFS= read -r file; do sha256sum -- "$file"; done
) > "$destination/SOURCE-MANIFEST.sha256"
printf 'Staged %s explicit source inputs plus version/checksum ledger\n' "${#files[@]}"
