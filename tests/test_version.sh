#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$root/scripts/version_parser.sh"
mkdir -p "$root/build/tests"
work=$(mktemp -d "$root/build/tests/version.XXXXXXXX")
trap 'cmake -E rm -rf "$work"' EXIT
for value in 0.0.0 1.2.3 15.100.99; do
  ailo_valid_version "$value"
done
for value in 01.2.3 v1.2.3 1.2 1.2.3-rc1 '1.2.3 ' ''; do
  if ailo_valid_version "$value"; then printf 'version test: accepted invalid value %s\n' "$value" >&2; exit 1; fi
done
[[ $(printf 'v1.2.3|commit\n' | ailo_exact_tag_version) == 1.2.3 ]]
[[ $(printf 'not-semver|tag\nv1.2.3-rc1|tag\n' | ailo_exact_tag_version) == '' ]]
if printf 'v1.2.3|tag\n' | ailo_exact_tag_version >"$work/result" 2>"$work/error"; then exit 1; fi
rg -q 'annotated/signed' "$work/error"
if printf 'v1.2.3|commit\nv2.0.0|commit\n' | ailo_exact_tag_version >"$work/result" 2>"$work/error"; then exit 1; fi
rg -q 'multiple exact' "$work/error"
mkdir "$work/archive"
printf '2.3.4\n' >"$work/archive/VERSION"
[[ $("$root/scripts/release_version.sh" --source-root "$work/archive" --override '') == 2.3.4 ]]
[[ $("$root/scripts/release_version.sh" --source-root "$work/archive" --override 3.4.5) == 3.4.5 ]]
printf 'bad\n' >"$work/archive/VERSION"
if "$root/scripts/release_version.sh" --source-root "$work/archive" --override '' >"$work/result" 2>"$work/error"; then exit 1; fi
rg -q 'VERSION is invalid' "$work/error"
current=$("$root/scripts/release_version.sh" --override '')
expected=
if [[ -e "$root/.git" ]]; then
  expected=$(git -C "$root" for-each-ref --points-at=HEAD --format='%(refname:strip=2)|%(objecttype)' refs/tags | ailo_exact_tag_version)
  base_expected=${expected:-0.0.0}
else
  base_expected=$(cat "$root/VERSION")
fi
[[ "$current" == "$base_expected" ]]
[[ $(make -s -C "$root" print-release-version AILO_VERSION_OVERRIDE=9.8.7) == "${expected:-9.8.7}" ]]
if [[ -e "$root/.git" && -z "$expected" ]]; then
  if "$root/scripts/release_version.sh" --require-release --override 9.8.7 >"$work/result" 2>"$work/error"; then exit 1; fi
  rg -q 'exact lightweight tag' "$work/error"
fi
printf 'Read-only active-worktree resolution, tag record validation, overrides and archive VERSION: passed\n'
