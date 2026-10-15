#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 4 ]] || exit 2
driver=$1 model=$2 python=$3 binary=$4
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
work=$(mktemp -d "$binary/metadata.XXXXXXXX")
fixture_pid=
cleanup() {
  if [[ -n "$fixture_pid" ]]; then
    kill "$fixture_pid" 2>/dev/null || true
    wait "$fixture_pid" 2>/dev/null || true
  fi
  cmake -E rm -rf "$work"
}
trap cleanup EXIT
openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=localhost \
  -addext subjectAltName=IP:127.0.0.1 -keyout "$work/key.pem" -out "$work/cert.pem" \
  > "$work/cert.log" 2>&1
"$python" "$root/tests/metadata_fixture.py" "$work/cert.pem" "$work/key.pem" "$work/port" > "$work/server.log" 2>&1 &
fixture_pid=$!
for ((attempt=0; attempt<100; ++attempt)); do
  [[ ! -s "$work/port" ]] || break
  kill -0 "$fixture_pid" 2>/dev/null || { cat "$work/server.log" >&2; exit 1; }
  sleep 0.02
done
[[ -s "$work/port" ]] || { printf 'metadata fixture did not become ready\n' >&2; exit 1; }
"$driver" "$model" "$work/cache" "https://127.0.0.1:$(cat "$work/port")" "$work/cert.pem"
