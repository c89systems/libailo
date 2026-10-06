#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# -eq 1 && "$1" =~ ^(x86_64|aarch64|armhf)-linux-(gnu|musl)$ ]] || { printf 'Usage: discover_target_tools.sh target-id\n' >&2; exit 2; }
description=$("$root/scripts/cpkt-toolchains.sh" discover "$1")
for key in cc sysroot nm readelf strip; do
  value=$(printf '%s\n' "$description" | sed -n "s/^$key=//p")
  [[ -n "$value" && -e "$value" ]] || { printf 'target-tools: pinned collection missing %s for %s\n' "$key" "$1" >&2; exit 2; }
  printf '%s=%s\n' "$key" "$value"
done
