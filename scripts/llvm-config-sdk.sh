#!/usr/bin/env bash
set -euo pipefail
[[ $# -ge 3 ]] || { printf 'Usage: llvm-config-sdk.sh LLVM_CONFIG CORE_PREFIX MISC_PREFIX [LLVM_ARGS...]\n' >&2; exit 2; }
llvm_config=$1
core=$2
misc=$3
shift 3
output=$("$llvm_config" "$@")
for argument in "$@"; do
  [[ "$argument" == --system-libs ]] || continue
  original=$("$llvm_config" --link-static --system-libs)
  read -r -a libraries <<< "$original"
  selected=()
  for library in "${libraries[@]}"; do
    case "$library" in
      -lz|*/libz.a|*/libz.so*) path="$core/lib/libz.a" ;;
      -lzstd|*/libzstd.a|*/libzstd.so*) path="$misc/lib/libzstd.a" ;;
      -lxml2|*/libxml2.a|*/libxml2.so*) path="$core/lib/libxml2.a" ;;
      -lc|-lm|-ldl|-lrt|-lpthread|-pthread|-lstdc++|-lgcc_s|-lgcc|-latomic)
        selected+=("$library"); continue ;;
      *) printf 'llvm-config-sdk: unqualified LLVM dependency: %s\n' "$library" >&2; exit 2 ;;
    esac
    [[ -f "$path" ]] || { printf 'llvm-config-sdk: missing pinned SDK archive: %s\n' "$path" >&2; exit 2; }
    # llvm-config emits shell words; retain paths containing spaces for Meson.
    path=${path//\\/\\\\}
    path=${path//\"/\\\"}
    selected+=("\"$path\"")
  done
  output=${output//"$original"/"${selected[*]}"}
  break
done
printf '%s\n' "$output"
