#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# -eq 1 && -f "$1" ]] || { printf 'Usage: verify_source_archive.sh archive.tar.gz\n' >&2; exit 2; }
archive=$(realpath -- "$1")
mkdir -p "$root/build/source-packaging"
work=$(mktemp -d "$root/build/source-packaging/verify.XXXXXXXX")
trap 'cmake -E rm -rf "$work"' EXIT
tar -tzf "$archive" > "$work/entries"
if [[ -n $(LC_ALL=C sort "$work/entries" | uniq -d) ]]; then
  printf 'source-verify: duplicate archive entries are forbidden\n' >&2
  exit 1
fi
while IFS= read -r detail; do
  case "${detail:0:1}" in -|d) ;; *) printf 'source-verify: links/special archive entries are forbidden\n' >&2; exit 1 ;; esac
done < <(tar -tvzf "$archive")
first=$(head -1 "$work/entries")
name=${first%%/*}
[[ "$name" =~ ^libailo-(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$ ]] || { printf 'source-verify: invalid archive root\n' >&2; exit 1; }
while IFS= read -r entry; do
  if [[ "$entry" != "$name/"* || "$entry" =~ (^|/)\.\.?(/|$) ]]; then
    printf 'source-verify: escaping or multiple archive root: %s\n' "$entry" >&2
    exit 1
  fi
done < "$work/entries"
tar -xzf "$archive" -C "$work"
source_root="$work/$name"
[[ ! -e "$source_root/.git" ]] || { printf 'source-verify: VCS state in archive\n' >&2; exit 1; }
ledger_names="$work/ledger-names"
: > "$ledger_names"
while IFS= read -r row; do
  if [[ ! "$row" =~ ^[0-9a-f]{64}[[:space:]][[:space:]](.+)$ ]]; then
    printf 'source-verify: malformed source checksum ledger\n' >&2
    exit 1
  fi
  file=${BASH_REMATCH[1]}
  if [[ "$file" == /* || "$file" =~ (^|/)\.\.?(/|$) || "$file" == SOURCE-MANIFEST.sha256 ]]; then
    printf 'source-verify: escaping checksum ledger path\n' >&2
    exit 1
  fi
  printf '%s\n' "$file" >> "$ledger_names"
done < "$source_root/SOURCE-MANIFEST.sha256"
{ sed '/^#/d;/^$/d' "$source_root/cmake/source-manifest.txt"; printf 'VERSION\n'; } | LC_ALL=C sort > "$work/ledger-expected"
LC_ALL=C sort "$ledger_names" > "$work/ledger-actual"
diff -u "$work/ledger-expected" "$work/ledger-actual"
(
  cd -- "$source_root"
  sha256sum --strict -c SOURCE-MANIFEST.sha256 > "$work/checksums.log"
)
"$root/scripts/release_version.sh" --source-root "$source_root" --override '' > "$work/version"
[[ $(cat "$work/version") == "${name#libailo-}" ]] || { printf 'source-verify: root/VERSION disagreement\n' >&2; exit 1; }
{ sed '/^#/d;/^$/d' "$source_root/cmake/source-manifest.txt"; printf 'VERSION\nSOURCE-MANIFEST.sha256\n'; } | LC_ALL=C sort > "$work/expected"
(cd "$source_root" && rg --files --hidden --no-ignore | LC_ALL=C sort) > "$work/actual"
diff -u "$work/expected" "$work/actual"
while IFS= read -r directory; do
  case "$directory" in */.git|*/.cache|*/build|*/dist|*/stash) printf 'source-verify: generated directory in archive: %s\n' "$directory" >&2; exit 1 ;; esac
done < <(find "$source_root" -type d)
if [[ -n $(find "$source_root" -type l -print) ]]; then
  printf 'source-verify: source archive symlinks are not permitted\n' >&2
  exit 1
fi
home_uri='file://'
home_uri+='$HOME'
scan_status=0
rg -a -l -F -e "$root" -e "$HOME" -e "$home_uri" "$source_root" || scan_status=$?
if [[ "$scan_status" == 0 || "$scan_status" -gt 1 ]]; then
  printf 'source-verify: local path in source payload\n' >&2
  exit 1
fi
printf 'Source archive root, exact inclusion manifest, checksums, version and privacy: passed\n'
