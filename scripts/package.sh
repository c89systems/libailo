#!/usr/bin/env bash
set -euo pipefail
# Canonical permissions make generated artifact bytes independent of umask.
umask 022
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
preset=x86_64-linux-gnu-release
development=false
no_build=false
output_directory=
while [[ $# -gt 0 ]]; do
  case "$1" in
    --preset) [[ $# -ge 2 ]] || exit 2; preset=$2; shift 2 ;;
    --development) development=true; shift ;;
    --no-build) no_build=true; shift ;;
    --output-directory) [[ $# -ge 2 ]] || exit 2; output_directory=$2; shift 2 ;;
    *) printf 'Usage: package.sh [--preset name] [--development --no-build --output-directory build/path]\n' >&2; exit 2 ;;
  esac
done
[[ "$preset" =~ ^[A-Za-z0-9_-]+$ ]] || { printf 'package: invalid preset\n' >&2; exit 2; }
if "$development"; then
  [[ -n "$output_directory" ]] || { printf 'package: development output directory must be explicit\n' >&2; exit 2; }
  version=$("$root/scripts/release_version.sh")
else
  version=$("$root/scripts/release_version.sh" --require-release)
  python3 "$root/scripts/generate_native.py" --check --require-complete
  output_directory=${output_directory:-"$root/dist"}
  "$no_build" && { printf 'package: --no-build is only a development verification surface\n' >&2; exit 2; }
fi
output_directory=$(realpath -m -- "$output_directory")
case "$output_directory" in "$root/build/"*|"$root/dist") ;; *) printf 'package: output must be repository build/ or dist/\n' >&2; exit 2 ;; esac
if "$development" && [[ "$output_directory" != "$root/build/"* ]]; then
  printf 'package: development artifacts cannot enter dist/\n' >&2; exit 2
fi
if ! "$no_build"; then "$root/scripts/build.sh" --preset "$preset"; fi
binary="$root/build/$preset"
[[ -f "$binary/install/test-settings.txt" ]] || { printf 'package: configure/build the selected preset first\n' >&2; exit 2; }
target= strip_tool= built_version=
while IFS='=' read -r key value; do
  case "$key" in target) target=$value ;; strip) strip_tool=$value ;; version) built_version=$value ;; esac
done < "$binary/install/test-settings.txt"
[[ "$version" == "$built_version" && -x "$strip_tool" ]] || { printf 'package: stale configured version or missing selected strip tool; rebuild the preset\n' >&2; exit 2; }
mkdir -p "$root/build/packaging" "$output_directory"
work=$(mktemp -d "$root/build/packaging/sdk.XXXXXXXX")
trap 'cmake -E rm -rf "$work"' EXIT
name="libailo-$version-$target"
prefix="$work/$name"
cmake --install "$binary" --prefix "$prefix" --component ailo >"$work/install.log"
while IFS= read -r -d '' file; do
  case "$file" in *.a) "$strip_tool" --enable-deterministic-archives --strip-debug "$file" ;; *.so|*.so.*) "$strip_tool" --strip-unneeded "$file" ;; esac
done < <(find "$prefix/lib" -type f -print0)
cp "$root/docs/facade-coverage.json" "$prefix/share/ailo/facade-coverage.json"
printf 'version=%s\ntarget=%s\ndevelopment=%s\n' "$version" "$target" "$development" > "$prefix/share/ailo/package.txt"
(
  cd -- "$prefix"
  find . -type l -printf '%P\t%l\n' | LC_ALL=C sort > PACKAGE-SYMLINKS.txt
  find . -type f ! -name PACKAGE-MANIFEST.sha256 -printf '%P\n' | LC_ALL=C sort | while IFS= read -r file; do sha256sum -- "$file"; done
) > "$prefix/PACKAGE-MANIFEST.sha256"
epoch=${SOURCE_DATE_EPOCH:-0}
[[ "$epoch" =~ ^[0-9]+$ ]] || { printf 'package: invalid SOURCE_DATE_EPOCH\n' >&2; exit 2; }
tar --sort=name --mtime="@$epoch" --owner=0 --group=0 --numeric-owner -C "$work" -cf - "$name" | gzip -n > "$work/archive.tar.gz"
mv -- "$work/archive.tar.gz" "$output_directory/$name.tar.gz"
printf '%s\n' "$output_directory/$name.tar.gz"
