#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# -eq 1 && -f "$1" ]] || { printf 'Usage: release_upload_files.sh checksum-manifest\n' >&2; exit 2; }
manifest=$(realpath -- "$1")
"$root/scripts/verify_release_artifacts.sh" "$manifest" >&2
directory=${manifest%/*}
while IFS= read -r row; do
  [[ "$row" =~ ^[0-9a-f]{64}[[:space:]][[:space:]](.+)$ ]] || exit 1
  printf '%s\n' "$directory/${BASH_REMATCH[1]}"
done < "$manifest"
printf '%s\n' "$manifest"
