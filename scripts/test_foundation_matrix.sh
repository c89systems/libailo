#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# -eq 0 ]] || { printf 'Usage: test_foundation_matrix.sh\n' >&2; exit 2; }
for target in x86_64-linux-gnu x86_64-linux-musl aarch64-linux-gnu aarch64-linux-musl armhf-linux-gnu armhf-linux-musl; do
  "$root/scripts/cpkt-toolchains.sh" ensure "$target" >/dev/null
  "$root/scripts/deps.sh" --target "$target" --component cpkt --component cpktmisc
  directory="$root/build/foundation-matrix/$target"
  cmake -S "$root/tests/foundation-matrix" -B "$directory" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$root/cmake/AiloToolchain.cmake" \
    -DAILO_PROJECT_ROOT="$root" -DAILO_TARGET_ID="$target" -DCMAKE_BUILD_TYPE=Debug
  cmake --build "$directory" --parallel "${AILO_JOBS:-2}"
  ctest --test-dir "$directory" --output-on-failure -j1
done
