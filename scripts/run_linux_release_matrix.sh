#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
development=false
output=
while [[ $# -gt 0 ]]; do
  case "$1" in
    --development) development=true; shift ;;
    --output-directory) [[ $# -ge 2 ]] || exit 2; output=$2; shift 2 ;;
    *) printf 'Usage: run_linux_release_matrix.sh [--development --output-directory build/path]\n' >&2; exit 2 ;;
  esac
done
if "$development"; then
  [[ -n "$output" ]] || { printf 'release-matrix: explicit development output required\n' >&2; exit 2; }
  output=$(realpath -m -- "$output")
  [[ "$output" == "$root/build/"* ]] || { printf 'release-matrix: development output must be under build/\n' >&2; exit 2; }
  version=$("$root/scripts/release_version.sh")
else
  version=$("$root/scripts/release_version.sh" --require-release)
  python3 "$root/scripts/generate_native.py" --check --require-complete
  output="$root/dist"
fi
mkdir -p "$output" "$root/build/packaging"
list=$(mktemp "$root/build/packaging/matrix.XXXXXXXX")
trap 'cmake -E rm -f "$list"' EXIT
for target in x86_64-linux-gnu x86_64-linux-musl aarch64-linux-gnu aarch64-linux-musl; do
  "$root/scripts/build.sh" --preset "$target-release"
  if [[ "$target" == x86_64-linux-gnu ]]; then
    ctest --test-dir "$root/build/$target-release" --output-on-failure -j1
  fi
  if "$development"; then
    "$root/scripts/package.sh" --preset "$target-release" --development --no-build --output-directory "$output"
  else
    "$root/scripts/package.sh" --preset "$target-release" --output-directory "$output"
  fi
  printf 'libailo-%s-%s.tar.gz\n' "$version" "$target" >> "$list"
  "$root/scripts/verify_sdk_archive.sh" "$output/libailo-$version-$target.tar.gz"
done
# Existing four-target QEMU scalar/bridge coverage is mandatory, never a skip.
"$root/scripts/test_scalar_matrix.sh"
"$root/scripts/package_checksums.sh" "$output" "$version" "$list"
"$root/scripts/verify_release_artifacts.sh" "$output/libailo-$version-CHECKSUMS"
printf 'Four-target Linux package matrix verified\n'
