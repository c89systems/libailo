#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# -eq 0 ]] || { printf 'Usage: test_source_archive.sh\n' >&2; exit 2; }
mkdir -p "$root/build/tests"
work=$(mktemp -d "$root/build/tests/source.XXXXXXXX")
trap 'cmake -E rm -rf "$work"' EXIT
(umask 022; "$root/scripts/package_source.sh" --development --output "$work/first.tar.gz")
(umask 077; "$root/scripts/package_source.sh" --development --output "$work/second.tar.gz")
cmp "$work/first.tar.gz" "$work/second.tar.gz"
"$root/scripts/verify_source_archive.sh" "$work/first.tar.gz"
mkdir "$work/bad"
tar -xzf "$work/first.tar.gz" -C "$work/bad"
source_root=$(find "$work/bad" -mindepth 1 -maxdepth 1 -type d)
name=${source_root##*/}
printf 'unexpected input\n' > "$source_root/unexpected.txt"
tar -czf "$work/extra.tar.gz" -C "$work/bad" "$name"
if "$root/scripts/verify_source_archive.sh" "$work/extra.tar.gz" >"$work/result" 2>&1; then
  printf 'source-test: extra unmanifested file was accepted\n' >&2; exit 1
fi
rg -q 'unexpected.txt' "$work/result"
cmake -E rm "$source_root/unexpected.txt"
mkdir "$source_root/build"
tar -czf "$work/generated.tar.gz" -C "$work/bad" "$name"
if "$root/scripts/verify_source_archive.sh" "$work/generated.tar.gz" >"$work/result" 2>&1; then
  printf 'source-test: empty generated directory was accepted\n' >&2; exit 1
fi
rg -q 'generated directory' "$work/result"
cmake -E rm -rf "$source_root/build"
ln -s README.md "$source_root/link"
tar -czf "$work/link.tar.gz" -C "$work/bad" "$name"
if "$root/scripts/verify_source_archive.sh" "$work/link.tar.gz" >"$work/result" 2>&1; then
  printf 'source-test: archive symlink was accepted\n' >&2; exit 1
fi
rg -q 'links/special archive entries' "$work/result"
cmake -E rm "$source_root/link"
printf 'corrupt\n' >> "$source_root/README.md"
tar -czf "$work/corrupt.tar.gz" -C "$work/bad" "$name"
if "$root/scripts/verify_source_archive.sh" "$work/corrupt.tar.gz" >"$work/result" 2>&1; then
  printf 'source-test: checksum-corrupt source was accepted\n' >&2; exit 1
fi
rg -q 'checksum' "$work/result"
printf 'Deterministic source staging and negative source archive fixtures: passed\n'
