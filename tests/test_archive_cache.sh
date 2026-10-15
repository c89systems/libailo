#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 1 ]] || exit 2
python=$1
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$root/build/tests"
work=$(mktemp -d "$root/build/tests/archive.XXXXXXXX")
fixture_pid=
cleanup() {
  if [[ -n "$fixture_pid" ]]; then
    kill "$fixture_pid" 2>/dev/null || true
    wait "$fixture_pid" 2>/dev/null || true
  fi
  cmake -E rm -rf "$work"
}
trap cleanup EXIT
trap 'printf "Archive-cache assertion failed at line %s\n" "$LINENO" >&2' ERR
printf 'synthetic pinned dependency archive\n' > "$work/payload"
sha=$(sha256sum "$work/payload"); sha=${sha%% *}
"$python" "$root/tests/archive_fixture.py" "$work" > "$work/server.log" 2>&1 &
fixture_pid=$!
for ((attempt=0; attempt<100; ++attempt)); do
  [[ ! -s "$work/port" ]] || break
  kill -0 "$fixture_pid" 2>/dev/null || { cat "$work/server.log" >&2; exit 1; }
  sleep 0.02
done
[[ -s "$work/port" ]] || { printf 'archive fixture did not become ready\n' >&2; exit 1; }
port=$(cat "$work/port")
acquire() {
  cmake "-DCPKT_DEPENDENCY_CACHE=$work/shared" -DAILO_ARCHIVE_TEST_HTTP=ON \
    -DAILO_ARCHIVE_COMPONENT=synthetic "-DAILO_ARCHIVE_URL=http://127.0.0.1:$port/$1" \
    "-DAILO_ARCHIVE_SHA256=$2" "-DAILO_ARCHIVE_RESULT=$work/result" \
    -P "$root/cmake/AiloArchive.cmake" > "$work/result.log" 2>&1
}
# Miss, then digest alias hit with no request to the different URL.
acquire first.tar.gz "$sha" || { cat "$work/result.log" >&2; exit 1; }
original=$(cat "$work/result")
cmp "$original" "$work/payload"
[[ $(wc -l < "$work/requests") -eq 1 ]]
acquire different-name-and-url.tar.gz "$sha"
[[ $(cat "$work/result") == "$original" && $(wc -l < "$work/requests") -eq 1 ]]
# A corrupt hit is replaced from the origin.
printf corrupt > "$original"
acquire first.tar.gz "$sha"
cmp "$original" "$work/payload"
[[ $(wc -l < "$work/requests") -eq 2 ]]
cmake -E rm -rf "$work/shared" "$work/result"
# A bad digest must leave neither a published archive nor a result path.
if acquire bad "$sha"; then printf 'wrong digest was accepted\n' >&2; exit 1; fi
rg -q 'SHA-256 mismatch' "$work/result.log"
[[ ! -e "$work/result" && -z $(find "$work/shared/archives/sha256/$sha" -mindepth 1 -print -quit) ]]
count=$(wc -l < "$work/requests")
if acquire first.tar.gz invalid; then printf 'invalid pin was accepted\n' >&2; exit 1; fi
[[ $(wc -l < "$work/requests") -eq "$count" ]]
printf 'Archive cache miss, offline alias hit, corrupt replacement and rejected pins passed\n'
