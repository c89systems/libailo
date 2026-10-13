#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# -eq 1 && -f "$1" ]] || { printf 'Usage: verify_sdk_archive.sh SDK.tar.gz\n' >&2; exit 2; }
archive=$(realpath -- "$1")
name=${archive##*/}
name=${name%.tar.gz}
[[ "$name" =~ ^libailo-([0-9]+\.[0-9]+\.[0-9]+)-((x86_64|aarch64)-linux-(gnu|musl))$ ]] || { printf 'sdk-verify: invalid SDK artifact name\n' >&2; exit 2; }
version=${BASH_REMATCH[1]}
target=${BASH_REMATCH[2]}
cc= sysroot= nm= readelf= strip_tool=
while IFS='=' read -r key value; do case "$key" in cc) cc=$value ;; sysroot) sysroot=$value ;; nm) nm=$value ;; readelf) readelf=$value ;; strip) strip_tool=$value ;; esac; done < <("$root/scripts/discover_target_tools.sh" "$target")
[[ -x "$readelf" && -x "$nm" && -x "$cc" && -d "$sysroot" ]] || { printf 'sdk-verify: selected target tools are incomplete\n' >&2; exit 2; }
mkdir -p "$root/build/packaging"
work=$(mktemp -d "$root/build/packaging/verify-sdk.XXXXXXXX")
trap 'cmake -E rm -rf "$work"' EXIT
tar -tzf "$archive" > "$work/entries"
[[ -z $(LC_ALL=C sort "$work/entries" | uniq -d) ]] || { printf 'sdk-verify: duplicate archive entries\n' >&2; exit 1; }
while IFS= read -r file; do
  [[ "$file" == "$name/"* && ! "$file" =~ (^|/)\.\.?(/|$) ]] || { printf 'sdk-verify: escaping/multiple archive root\n' >&2; exit 1; }
done < "$work/entries"
while IFS= read -r detail; do
  case "${detail:0:1}" in
    -|d) ;;
    l)
      destination=${detail##* -> }
      [[ "$destination" != /* && "$destination" != *..* ]] || { printf 'sdk-verify: escaping symlink before extraction\n' >&2; exit 1; }
      ;;
    *) printf 'sdk-verify: special/hardlinked archive entry\n' >&2; exit 1 ;;
  esac
done < <(tar -tvzf "$archive")
tar -xzf "$archive" -C "$work"
sdk="$work/$name"
[[ -f "$sdk/PACKAGE-MANIFEST.sha256" && -f "$sdk/PACKAGE-SYMLINKS.txt" ]] || { printf 'sdk-verify: missing payload ledger\n' >&2; exit 1; }
: > "$work/expected"
while IFS= read -r row; do
  [[ "$row" =~ ^[0-9a-f]{64}[[:space:]][[:space:]](.+)$ ]] || { printf 'sdk-verify: malformed payload ledger\n' >&2; exit 1; }
  file=${BASH_REMATCH[1]}
  [[ "$file" != /* && ! "$file" =~ (^|/)\.\.?(/|$) && "$file" != PACKAGE-MANIFEST.sha256 ]] || { printf 'sdk-verify: escaping payload ledger\n' >&2; exit 1; }
  printf '%s\n' "$file" >> "$work/expected"
done < "$sdk/PACKAGE-MANIFEST.sha256"
(cd "$sdk" && sha256sum --strict -c PACKAGE-MANIFEST.sha256 > "$work/checksums.log")
printf 'PACKAGE-MANIFEST.sha256\n' >> "$work/expected"
LC_ALL=C sort "$work/expected" -o "$work/expected"
(cd "$sdk" && find . -type f -printf '%P\n' | LC_ALL=C sort) > "$work/actual"
diff -u "$work/expected" "$work/actual"
(cd "$sdk" && find . -type l -printf '%P\t%l\n' | LC_ALL=C sort) > "$work/symlinks"
diff -u "$sdk/PACKAGE-SYMLINKS.txt" "$work/symlinks"
for file in include/ailo/base.h include/ailo/native.h include/ailo/native_macros.h include/ailo/native_variadic.h include/ailo/model.h include/ailo/version.h lib/libailo.a lib/libailo.so.0 lib/cmake/ailo/ailoConfig.cmake lib/pkgconfig/ailo.pc lib/pkgconfig/ailo-static.pc share/doc/ailo/LICENSE share/ailo/dependencies.json share/ailo/package.txt share/ailo/facade-coverage.json; do
  [[ -f "$sdk/$file" ]] || { printf 'sdk-verify: missing required file %s\n' "$file" >&2; exit 1; }
done
while IFS= read -r -d '' file; do
  relative=${file#"$sdk/include/"}
  [[ "$relative" == ailo/* && "$relative" != *../* && ( -f "$root/include/$relative" || "$relative" == ailo/version.h ) ]] || { printf 'sdk-verify: unexpected public header\n' >&2; exit 1; }
done < <(find "$sdk/include" -type f -print0)
if [[ -e "$sdk/include/llama.h" || -e "$sdk/lib/libcurl.a" || -e "$sdk/lib/libcrypto.a" || -e "$sdk/.git" || -e "$sdk/build" || -e "$sdk/.cache" ]]; then
  printf 'sdk-verify: leaked private source/SDK payload\n' >&2; exit 1
fi
rg -q "^version=$version$" "$sdk/share/ailo/package.txt"
rg -q "^target=$target$" "$sdk/share/ailo/package.txt"
rg -q "#define AILO_VERSION \"$version\"" "$sdk/include/ailo/version.h"
header=$("$readelf" -h "$sdk/lib/libailo.so.0")
case "$target" in x86_64-*) machine='Advanced Micro Devices X86-64' ;; aarch64-*) machine=AArch64 ;; esac
printf '%s\n' "$header" | rg -q "Machine:.*$machine"
while IFS= read -r -d '' library; do
  case "$library" in *.a|*.so|*.so.*)
    machines=$("$readelf" -h "$library" | sed -n 's/^[[:space:]]*Machine:[[:space:]]*//p' | LC_ALL=C sort -u)
    [[ "$machines" == "$machine" ]] || { printf 'sdk-verify: mixed/wrong target archive: %s\n' "$library" >&2; exit 1; }
    ;;
  esac
done < <(find "$sdk/lib" -type f -print0)
"$root/scripts/verify_install_privacy.sh" "$sdk" "$readelf"
"$root/scripts/verify_exports.sh" --library "$sdk/lib/libailo.so.0" --nm "$nm" --cc "$cc" --sysroot "$sysroot" --work "$work/exports" --runtime-directory "$root/.cache/deps/$target/cpkt/install/lib" --runtime-directory "$sdk/lib/ailo/native"
printf 'Checksummed extracted SDK %s: layout, target ABI, privacy and exact exports passed\n' "$target"
