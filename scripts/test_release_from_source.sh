#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# -eq 1 && -f "$1" ]] || { printf 'Usage: test_release_from_source.sh archive.tar.gz\n' >&2; exit 2; }
archive=$(realpath -- "$1")
"$root/scripts/verify_source_archive.sh" "$archive"
mkdir -p "$root/build/source-packaging"
work=$(mktemp -d "$root/build/source-packaging/smoke.XXXXXXXX")
trap 'cmake -E rm -rf "$work"' EXIT
tar -xzf "$archive" -C "$work"
source_root=$(find "$work" -mindepth 1 -maxdepth 1 -type d)
[[ -f "$source_root/VERSION" && ! -e "$source_root/.git" ]] || exit 1
make -C "$source_root" finalize-slice PRESET=cpu
printf 'Source archive reconstructs the complete native CPU build/test/install surfaces without VCS metadata: passed\n'
