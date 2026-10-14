#!/usr/bin/env bash
set -euo pipefail
# Canonical permissions make generated artifact bytes independent of umask.
umask 022
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
development=false
output=
while [[ $# -gt 0 ]]; do
  case "$1" in
    --development) development=true; shift ;;
    --output) [[ $# -ge 2 ]] || exit 2; output=$2; shift 2 ;;
    *) printf 'Usage: package_source.sh [--development --output build/path.tar.gz]\n' >&2; exit 2 ;;
  esac
done
if "$development"; then
  [[ -n "$output" ]] || { printf 'source-package: development output must be explicit\n' >&2; exit 2; }
  version=$("$root/scripts/release_version.sh")
else
  version=$("$root/scripts/release_version.sh" --require-release)
  python3 "$root/scripts/generate_native.py" --check --require-complete
  output=${output:-"$root/dist/libailo-$version.tar.gz"}
fi
output=$(realpath -m -- "$output")
case "$output" in "$root/build/"*|"$root/dist/"*) ;; *) printf 'source-package: output must be under build/ or dist/\n' >&2; exit 2 ;; esac
if "$development" && [[ "$output" != "$root/build/"* ]]; then
  printf 'source-package: development artifacts belong under build/\n' >&2
  exit 2
fi
mkdir -p "$root/build/source-packaging" "$(dirname -- "$output")"
work=$(mktemp -d "$root/build/source-packaging/stage.XXXXXXXX")
trap 'cmake -E rm -rf "$work"' EXIT
name="libailo-$version"
"$root/scripts/stage_release_sources.sh" "$work/$name" "$version"
epoch=${SOURCE_DATE_EPOCH:-0}
[[ "$epoch" =~ ^[0-9]+$ ]] || { printf 'source-package: SOURCE_DATE_EPOCH must be a nonnegative integer\n' >&2; exit 2; }
tar --sort=name --mtime="@$epoch" --owner=0 --group=0 --numeric-owner -C "$work" -cf - "$name" | gzip -n > "$work/archive.tar.gz"
mv -- "$work/archive.tar.gz" "$output"
printf '%s\n' "$output"
