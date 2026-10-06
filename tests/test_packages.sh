#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# -eq 1 && "$1" =~ ^[A-Za-z0-9_-]+$ ]] || { printf 'Usage: test_packages.sh native-preset\n' >&2; exit 2; }
mkdir -p "$root/build/tests"
work=$(mktemp -d "$root/build/tests/packages.XXXXXXXX")
trap 'cmake -E rm -rf "$work"' EXIT
mkdir "$work/first" "$work/second"
"$root/scripts/package.sh" --development --no-build --preset "$1" --output-directory "$work/first"
"$root/scripts/package.sh" --development --no-build --preset "$1" --output-directory "$work/second"
version=$("$root/scripts/release_version.sh")
target=$(sed -n 's/^target=//p' "$root/build/$1/install/test-settings.txt")
name="libailo-$version-$target.tar.gz"
cmp "$work/first/$name" "$work/second/$name"
printf '%s\n' "$name" > "$work/artifacts"
"$root/scripts/package_checksums.sh" "$work/first" "$version" "$work/artifacts"
manifest="$work/first/libailo-$version-CHECKSUMS"
"$root/scripts/verify_release_artifacts.sh" "$manifest"
"$root/scripts/release_upload_files.sh" "$manifest" > "$work/upload-set"
printf '%s\n%s\n' "$work/first/$name" "$manifest" > "$work/upload-expected"
diff -u "$work/upload-expected" "$work/upload-set"
cp "$work/first/$name" "$work/original.tar.gz"
printf 'corrupt\n' >> "$work/first/$name"
if "$root/scripts/verify_release_artifacts.sh" "$manifest" >"$work/result" 2>&1; then
  printf 'package-test: corrupt artifact checksum was accepted\n' >&2; exit 1
fi
rg -q 'checksum' "$work/result"
cp "$work/original.tar.gz" "$work/first/$name"
cp "$work/original.tar.gz" "$work/first/libailo-99.99.99-$target.tar.gz"
if "$root/scripts/package_checksums.sh" "$work/first" "$version" "$work/artifacts" >"$work/result" 2>&1; then
  printf 'package-test: stale artifact was accepted\n' >&2; exit 1
fi
rg -q 'stale or unlisted' "$work/result"
cmake -E rm "$work/first/libailo-99.99.99-$target.tar.gz"
cat "$manifest" "$manifest" > "$work/duplicate"
cp "$work/duplicate" "$manifest"
if "$root/scripts/verify_release_artifacts.sh" "$manifest" >"$work/result" 2>&1; then
  printf 'package-test: duplicate manifest entry was accepted\n' >&2; exit 1
fi
rg -q 'duplicate artifact' "$work/result"
"$root/scripts/package_checksums.sh" "$work/first" "$version" "$work/artifacts"
mkdir "$work/changed"
tar -xzf "$work/original.tar.gz" -C "$work/changed"
sdk="$work/changed/${name%.tar.gz}"
printf 'corrupt\n' >> "$sdk/share/doc/ailo/LICENSE"
tar -czf "$work/changed/$name" -C "$work/changed" "${name%.tar.gz}"
if "$root/scripts/verify_sdk_archive.sh" "$work/changed/$name" >"$work/result" 2>&1; then
  printf 'package-test: corrupt inner payload was accepted\n' >&2; exit 1
fi
rg -q 'checksum' "$work/result"
printf 'Deterministic SDK archives, explicit checksum set and negative artifact/payload cases: passed\n'
