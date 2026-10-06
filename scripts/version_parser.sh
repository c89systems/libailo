#!/usr/bin/env bash
# Pure parsing shared by the resolver and its non-mutating contract tests.
ailo_valid_version() {
  [[ "$1" =~ ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$ ]]
}
ailo_exact_tag_version() {
  local name kind selected= version
  while IFS='|' read -r name kind; do
    [[ "$name" == v* ]] || continue
    version=${name#v}
    ailo_valid_version "$version" || continue
    if [[ "$kind" != commit ]]; then
      printf 'version: exact semver tag %s is annotated/signed; a lightweight tag is required\n' "$name" >&2
      return 2
    fi
    if [[ -n "$selected" && "$selected" != "$version" ]]; then
      printf 'version: multiple exact lightweight versions identify HEAD\n' >&2
      return 2
    fi
    selected=$version
  done
  printf '%s\n' "$selected"
}
