#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# -eq 2 ]] || { printf 'Usage: verify_install_privacy.sh install-tree target-readelf\n' >&2; exit 2; }
tree=$(realpath -- "$1")
readelf=$2
[[ -d "$tree/lib" && -d "$tree/include" && -d "$tree/share" ]] || {
  printf 'install-privacy: expected an extracted SDK install tree\n' >&2
  exit 2
}
scan_status=0
leaks=$(rg -a -l --hidden --no-ignore -F -e "$root" -e "$HOME" -e 'file://$HOME' -e 'file:///home/' -e 'file:///tmp/' "$tree") || scan_status=$?
if [[ "$scan_status" -gt 1 ]]; then
  printf 'install-privacy: payload scan failed\n' >&2
  exit 1
fi
if [[ -n "$leaks" ]]; then
  printf 'install-privacy: local path in extracted payload:\n%s\n' "$leaks" >&2
  exit 1
fi
scan_status=0
leaks=$(rg -a -l --hidden --no-ignore '/(home|Users|tmp|var/tmp|workspace|workspaces|build)/[A-Za-z0-9_.-]+/' "$tree") || scan_status=$?
if [[ "$scan_status" -gt 1 || -n "$leaks" ]]; then
  printf 'install-privacy: foreign workstation path or unreadable payload:\n%s\n' "$leaks" >&2
  exit 1
fi
while IFS= read -r -d '' link; do
  destination=$(readlink -- "$link")
  resolved=$(realpath -m -- "$(dirname -- "$link")/$destination")
  if [[ "$destination" == /* || "$resolved" != "$tree/"* || ! -e "$link" ]]; then
    printf 'install-privacy: escaping or dangling symlink: %s\n' "$link" >&2
    exit 1
  fi
done < <(find "$tree" -type l -print0)
while IFS= read -r -d '' file; do
  case "$file" in *.so|*.so.*)
    if ! metadata=$("$readelf" -d "$file" 2>&1); then
      printf 'install-privacy: cannot inspect %s: %s\n' "$file" "$metadata" >&2
      exit 1
    fi
    while IFS= read -r path; do
      IFS=: read -r -a entries <<< "$path"
      for entry in "${entries[@]}"; do
        if [[ "$entry" != '$ORIGIN' && "$entry" != '$ORIGIN/'* ]]; then
          printf 'install-privacy: non-relocatable runtime path in %s: %s\n' "$file" "$entry" >&2
          exit 1
        fi
      done
    done < <(printf '%s\n' "$metadata" | sed -n '/(RPATH)\|(RUNPATH)/s/.*\[\(.*\)\].*/\1/p')
    ;;
  esac
done < <(find "$tree/lib" -type f -print0)
printf 'Extracted SDK text/binary paths and runtime metadata: passed\n'
