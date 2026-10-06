#!/usr/bin/env bash
set -euo pipefail
script_root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$script_root/scripts/version_parser.sh"
source_root=$script_root
override=${AILO_VERSION_OVERRIDE:-}
require_release=false
while [[ $# -gt 0 ]]; do
  case "$1" in
    --source-root) [[ $# -ge 2 ]] || exit 2; source_root=$2; shift 2 ;;
    --override) [[ $# -ge 2 ]] || exit 2; override=$2; shift 2 ;;
    --require-release) require_release=true; shift ;;
    *) printf 'Usage: release_version.sh [--source-root directory] [--override X.Y.Z] [--require-release]\n' >&2; exit 2 ;;
  esac
done
[[ -d "$source_root" ]] || { printf 'version: source root does not exist\n' >&2; exit 2; }
source_root=$(realpath -- "$source_root")
if [[ -n "$override" ]] && ! ailo_valid_version "$override"; then
  printf 'version: AILO_VERSION_OVERRIDE must be X.Y.Z without leading zeros\n' >&2
  exit 2
fi
exact=
in_git=false
# Root-local .git is intentional: an archive extracted under another checkout
# must not inherit that enclosing checkout's HEAD or tags.
if [[ -e "$source_root/.git" ]]; then
  in_git=true
  records=$(git -C "$source_root" for-each-ref --points-at=HEAD --format='%(refname:strip=2)|%(objecttype)' refs/tags)
  exact=$(printf '%s\n' "$records" | ailo_exact_tag_version)
fi
if [[ -n "$exact" ]]; then
  version=$exact
elif [[ -n "$override" ]]; then
  version=$override
elif ! "$in_git" && [[ -f "$source_root/VERSION" ]]; then
  version=$(cat "$source_root/VERSION")
  ailo_valid_version "$version" || { printf 'version: source archive VERSION is invalid\n' >&2; exit 2; }
else
  version=0.0.0
fi
if "$require_release" && { [[ "$version" == 0.0.0 ]] || { "$in_git" && [[ -z "$exact" ]]; }; }; then
  printf 'version: release artifacts require an exact lightweight tag (candidate overrides are not releases)\n' >&2
  exit 2
fi
printf '%s\n' "$version"
