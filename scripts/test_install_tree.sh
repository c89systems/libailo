#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# -eq 1 && "$1" =~ ^[A-Za-z0-9_-]+$ ]] || { printf 'Usage: test_install_tree.sh preset\n' >&2; exit 2; }
binary="$root/build/$1"
[[ -f "$binary/install/test-settings.txt" ]] || { printf 'install-tree: configure/build the preset first\n' >&2; exit 2; }
target= cc= sysroot= nm= readelf= strip_tool= toolchain=
while IFS='=' read -r key value; do
  case "$key" in target) target=$value ;; cc) cc=$value ;; sysroot) sysroot=$value ;; nm) nm=$value ;; readelf) readelf=$value ;; strip) strip_tool=$value ;; toolchain) toolchain=$value ;; esac
done < "$binary/install/test-settings.txt"
[[ "$target" == x86_64-linux-gnu ]] || { printf 'install-tree: execution currently requires the native GNU target\n' >&2; exit 2; }
mkdir -p "$root/build/install-tests"
work=$(mktemp -d "$root/build/install-tests/$1.XXXXXXXX")
trap 'cmake -E rm -rf "$work"' EXIT
cmake --install "$binary" --prefix "$work/original" --component ailo >"$work/install.log"
mv "$work/original" "$work/relocated sdk"
prefix="$work/relocated sdk"
while IFS= read -r -d '' file; do
  case "$file" in *.a) "$strip_tool" --enable-deterministic-archives --strip-debug "$file" ;; *.so|*.so.*) "$strip_tool" --strip-unneeded "$file" ;; esac
done < <(find "$prefix/lib" -type f -print0)
# Scan extracted bytes, not only the install staging directory.
(cd "$work" && cmake -E tar czf sdk.tar.gz --format=gnutar 'relocated sdk')
mkdir "$work/extracted"
(cd "$work/extracted" && cmake -E tar xf "$work/sdk.tar.gz")
prefix="$work/extracted/relocated sdk"
"$root/scripts/verify_install_privacy.sh" "$prefix" "$readelf"
dep="$root/.cache/deps/$target"
ctest --test-dir "$binary" --output-on-failure -j1 -R '^model-fixture$'
fixture="$binary/model-fixtures/tiny-llama.gguf"
if rg -a -l -F -e "$root" -e "$HOME" "$prefix/lib/cmake" "$prefix/lib/pkgconfig" "$prefix/share/ailo/dependencies.json"; then
  printf 'install-tree: installed metadata contains a local path\n' >&2
  exit 1
fi
for header in "$prefix/include/ailo/"*.h; do
  [[ -f "$root/include/ailo/${header##*/}" || "${header##*/}" == version.h ]] || { printf 'install-tree: leaked header %s\n' "$header" >&2; exit 1; }
done
if cmake -S "$root/tests/install-consumer" -B "$work/missing-sdk" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" -DAILO_TARGET_ID="$target" \
    "-Dailo_DIR=$prefix/lib/cmake/ailo" "-Dailo_EXTERNAL_ROOT=$work/missing" \
    -DAILO_CONSUMER_KIND=static "-DAILO_POLICY_ROOT=$root" \
    "-DAILO_TEST_DEPENDENCY_ROOT=$dep" "-DAILO_TEST_PREFIX=$prefix" >"$work/missing.log" 2>&1; then
  printf 'install-tree: missing external SDK was silently accepted\n' >&2
  exit 1
fi
rg -q 'cpkt >= 0.5.0' "$work/missing.log"
[[ ! -d "$prefix/include/ggml" && ! -f "$prefix/include/llama.h" ]] || { printf 'install-tree: native headers leaked\n' >&2; exit 1; }
for mode in static shared pkg-static pkg-shared; do
  for language in OFF ON; do
    consumer="$work/$mode-$language"
    cmake -E env "PKG_CONFIG_PATH=$prefix/lib/pkgconfig:$dep/cpkt/install/lib/pkgconfig:$dep/lonejson/install/lib/pkgconfig:$dep/cpktmisc/install/lib/pkgconfig:$dep/pslog/install/lib/pkgconfig:$dep/lockdc/install/lib/pkgconfig:$dep/liblql/install/lib/pkgconfig" \
      cmake -S "$root/tests/install-consumer" -B "$consumer" -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE="$toolchain" -DAILO_TARGET_ID="$target" \
      -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_PREFIX_PATH=$prefix;$dep/cpkt/install;$dep/lonejson/install;$dep/cpktmisc/install;$dep/pslog/install;$dep/lockdc/install;$dep/liblql/install" \
      "-DAILO_CONSUMER_KIND=$mode" "-DAILO_CONSUMER_CXX=$language" \
      "-DAILO_POLICY_ROOT=$root" "-DAILO_TEST_DEPENDENCY_ROOT=$dep" "-DAILO_TEST_PREFIX=$prefix"
    cmake --build "$consumer" --parallel 1
    "$consumer/consumer" "$fixture"
    "$consumer/model_consumer" "$fixture" >"$consumer/model.log" 2>&1 || { tail -25 "$consumer/model.log" >&2; exit 1; }
    rg -q 'C89 CPU GGUF inference.*passed' "$consumer/model.log"
    if [[ "$mode" == static || "$mode" == pkg-static ]]; then
      if "$readelf" -d "$consumer/consumer" | rg -q 'NEEDED.*(libailo|libstdc\+\+)'; then
        printf 'install-tree: static consumer used a shared facade/C++ runtime\n' >&2
        exit 1
      fi
    fi
  done
done
while IFS= read -r path; do
  IFS=: read -r -a entries <<< "$path"
  for entry in "${entries[@]}"; do
    if [[ "$entry" != '$ORIGIN' && "$entry" != '$ORIGIN/'* ]]; then
      printf 'install-tree: non-relocatable runtime path: %s\n' "$entry" >&2
      exit 1
    fi
  done
done < <("$readelf" -d "$prefix/lib/libailo.so.0" | sed -n '/(RPATH)\|(RUNPATH)/s/.*\[\(.*\)\].*/\1/p')
"$root/scripts/verify_exports.sh" --library "$prefix/lib/libailo.so.0" --nm "$nm" \
  --cc "$cc" \
  --sysroot "$sysroot" \
  --work "$work/exports" --runtime-directory "$dep/cpkt/install/lib" --runtime-directory "$dep/lonejson/install/lib" --runtime-directory "$dep/cpktmisc/install/lib" --runtime-directory "$dep/pslog/install/lib" --runtime-directory "$dep/lockdc/install/lib" --runtime-directory "$dep/liblql/install/lib" --runtime-directory "$prefix/lib/ailo/native"
printf 'Relocated CMake/pkg-config C-only and C++ consumers, header privacy and exports: passed\n'
