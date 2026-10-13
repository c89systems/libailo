#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# -eq 0 ]] || { printf 'Usage: scripts/test_scalar_matrix.sh\n' >&2; exit 2; }
for target in x86_64-linux-gnu x86_64-linux-musl aarch64-linux-gnu aarch64-linux-musl armhf-linux-gnu armhf-linux-musl; do
  "$root/scripts/cpkt-toolchains.sh" ensure "$target" >/dev/null
  directory="$root/build/scalar-matrix/$target"
  cmake -S "$root/tests/scalar-matrix" -B "$directory" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$root/cmake/AiloToolchain.cmake" \
    -DAILO_PROJECT_ROOT="$root" -DAILO_TARGET_ID="$target" -DCMAKE_BUILD_TYPE=Debug
  cmake --build "$directory" --parallel "${AILO_JOBS:-2}"
  ctest --test-dir "$directory" --output-on-failure -j1
done
