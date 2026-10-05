#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
target=x86_64-linux-gnu
components=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --target) [[ $# -ge 2 ]] || { printf 'deps: --target requires a value\n' >&2; exit 2; }; target=$2; shift 2 ;;
    --component) [[ $# -ge 2 ]] || { printf 'deps: --component requires a value\n' >&2; exit 2; }; [[ -z "$2" ]] || components+=("$2"); shift 2 ;;
    *) printf 'Usage: scripts/deps.sh [--target id] [--component name]\n' >&2; exit 2 ;;
  esac
done
args=(-DAILO_TARGET_ID="$target")
if [[ -n "${CPKT_DEPENDENCY_CACHE:-}" ]]; then
  args+=(-DCPKT_DEPENDENCY_CACHE="$CPKT_DEPENDENCY_CACHE")
fi
if [[ ${#components[@]} -eq 0 ]]; then
  cmake "${args[@]}" -P "$root/cmake/AiloDependencies.cmake"
else
  for component in "${components[@]}"; do
    cmake "${args[@]}" -DAILO_DEPENDENCY="$component" -P "$root/cmake/AiloDependencies.cmake"
  done
fi
