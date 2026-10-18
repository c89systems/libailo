#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# -eq 2 ]] || { printf 'Usage: test_variadic_abort.sh c89-test parity-test\n' >&2; exit 2; }
work=$(mktemp -d "$root/build/variadic-abort.XXXXXXXX")
trap 'cmake -E rm -rf "$work"' EXIT
ulimit -c 0
run_abort() {
  local log=$1
  shift
  local result=0
  "$@" >"$log" 2>&1 || result=$?
  if [[ "$result" -ne 134 ]]; then
    cat "$log" >&2
    printf 'variadic: expected SIGABRT, received status %s\n' "$result" >&2
    exit 1
  fi
}
run_abort "$work/plain" "$1" --abort
[[ ! -s "$work/plain" ]]
GGML_NO_BACKTRACE=1 run_abort "$work/logged" "$2" --logged-abort
rg -q '^error: logger-test:42: logged-fatal$' "$work/logged"
run_abort "$work/native" "$2" --native-abort
run_abort "$work/facade" "$2" --facade-abort
cmp "$work/native" "$work/facade"
printf 'Native abort hook, truncation and process-exit parity: passed\n'
