#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
args=(-i)
case "${1:-}" in
  --check) args=(--dry-run --Werror); shift ;;
  '') ;;
  *) printf 'Usage: scripts/format.sh [--check]\n' >&2; exit 2 ;;
esac
[[ $# -eq 0 ]] || { printf 'format: unexpected arguments\n' >&2; exit 2; }
directories=()
for directory in src include tests examples; do
  [[ ! -d "$root/$directory" ]] || directories+=("$root/$directory")
done
files=()
if [[ ${#directories[@]} -gt 0 ]]; then
  mapfile -d '' -t files < <(rg --files --null "${directories[@]}" -g '*.c' -g '*.h' -g '*.cpp' -g '*.hpp' | sort -z)
fi
if [[ ${#files[@]} -gt 0 ]]; then
  clang-format "${args[@]}" "${files[@]}"
fi
