#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
library= nm= cc= sysroot= work=
runtime_directories=()
while [[ $# -gt 0 ]]; do
  [[ $# -ge 2 ]] || { printf 'exports: option requires a value\n' >&2; exit 2; }
  case "$1" in
    --library) library=$2 ;; --nm) nm=$2 ;; --cc) cc=$2 ;;
    --sysroot) sysroot=$2 ;; --work) work=$2 ;;
    --runtime-directory) runtime_directories+=("$2") ;;
    *) printf 'Usage: verify_exports.sh --library path --nm path --cc path --sysroot path --work path\n' >&2; exit 2 ;;
  esac
  shift 2
done
[[ -n "$library" && -n "$nm" && -n "$cc" && -n "$sysroot" && -n "$work" ]] || {
  printf 'exports: all options are required\n' >&2; exit 2;
}
work=$(realpath -m -- "$work")
case "$work" in "$root/build/"*) ;; *) printf 'exports: scratch must be inside build/\n' >&2; exit 2 ;; esac
mkdir -p "$work"
workspace=$(mktemp -d "$work/exports.XXXXXXXX")
cleanup() { cmake -E rm -rf "$workspace"; }
trap cleanup EXIT
LC_ALL=C "$nm" -D --defined-only -P "$library" | awk 'NF { print $1 }' | LC_ALL=C sort > "$workspace/actual"
cat "$root/cmake/ailo.exports" "$root/cmake/ailo-native.exports" "$root/cmake/ailo-variadic.exports" "$root/cmake/ailo-macros.exports" "$root/cmake/ailo-model.exports" | LC_ALL=C sort > "$workspace/expected"
if ! diff -u "$workspace/expected" "$workspace/actual"; then
  printf 'exports: shared library differs from its public ABI allowlist\n' >&2
  exit 1
fi
cat > "$workspace/consumer.c" <<'SOURCE'
#include "ailo/base.h"
int main(void) { return ailo_status_name(AILO_OK) == 0; }
SOURCE
command=("$cc" "--sysroot=$sysroot" -std=c89 -Wall -Wextra -Wpedantic \
  -pedantic-errors -Werror -Wl,--fatal-warnings "-I$root/include" \
  "$workspace/consumer.c" "$library" -o "$workspace/consumer")
for directory in "${runtime_directories[@]}"; do
  command+=("-Wl,-rpath-link,$directory")
done
"${command[@]}"
cat > "$workspace/consumer.c" <<'SOURCE'
#include "ailo/base.h"
extern ailo_status ailo_error_set(ailo_error *, ailo_status, const char *);
int main(void) { return (int)ailo_error_set(0, AILO_BUSY, "busy"); }
SOURCE
if "${command[@]}" > "$workspace/negative.log" 2>&1; then
  printf 'exports: private error helper linked successfully\n' >&2
  exit 1
fi
if ! rg -q 'ailo_error_set' "$workspace/negative.log"; then
  cat "$workspace/negative.log" >&2
  printf 'exports: negative fixture failed for a different reason\n' >&2
  exit 1
fi
printf 'Exact shared exports and private-helper negative link: passed\n'
