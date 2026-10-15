#!/usr/bin/env bash
set -euo pipefail
[[ $# -ge 4 ]] || { printf 'Usage: test_model_download.sh driver model consumer python [--valgrind]\n' >&2; exit 2; }
driver=$1 fixture=$2 consumer=$3 python=$4
shift 4
memcheck=false
if [[ ${1:-} == --valgrind ]]; then memcheck=true; shift; fi
[[ $# -eq 0 ]] || exit 2
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$root/build/tests"
suite=$(mktemp -d "$root/build/tests/model-tls.XXXXXXXX")
fixture_pid= first_pid= second_pid=
cleanup() {
  for pid in "$first_pid" "$second_pid" "$fixture_pid"; do
    if [[ -n "$pid" ]]; then
      kill "$pid" 2>/dev/null || true
      wait "$pid" 2>/dev/null || true
    fi
  done
  cmake -E rm -rf "$suite"
}
trap cleanup EXIT
trap 'printf "Model transport assertion failed at line %s (case %s)\n" "$LINENO" "${work:-setup}" >&2' ERR
openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=localhost \
  -addext subjectAltName=DNS:localhost,IP:127.0.0.1 \
  -keyout "$suite/key.pem" -out "$suite/cert.pem" > "$suite/cert.log" 2>&1
openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=unrelated.invalid \
  -addext subjectAltName=DNS:unrelated.invalid -keyout "$suite/wrong-key.pem" \
  -out "$suite/wrong-cert.pem" > "$suite/wrong-cert.log" 2>&1
"$python" "$root/tests/download_fixture.py" "$suite" "$fixture" > "$suite/server.log" 2>&1 &
fixture_pid=$!
for ((attempt=0; attempt<100; ++attempt)); do
  [[ ! -s "$suite/ports" ]] || break
  kill -0 "$fixture_pid" 2>/dev/null || { cat "$suite/server.log" >&2; exit 1; }
  sleep 0.02
done
[[ -s "$suite/ports" ]] || { printf 'download fixture did not become ready\n' >&2; exit 1; }
read -r port other_port wrong_port < "$suite/ports"
sha=$(sha256sum "$fixture"); sha=${sha%% *}
part1="$fixture-00001-of-00002.gguf" part2="$fixture-00002-of-00002.gguf"
sha1=$(sha256sum "$part1"); sha1=${sha1%% *}
sha2=$(sha256sum "$part2"); sha2=${sha2%% *}
zeros=0000000000000000000000000000000000000000000000000000000000000000
request_count() { wc -l < "$suite/requests"; }
case_begin() {
  printf 'Model transport: %s\n' "$1"
  work="$suite/$1"
  mkdir "$work"
  cache="$work/cache" filename=model.gguf repo=owner/model revision=fixed
  hashes=$sha allow=1 mode=normal limit=0 attempts=1 timeout=2000
  origin="https://127.0.0.1:$port" ca="$suite/cert.pem"
  environment=()
  begin_count=$(request_count)
}
prepare() {
  command=(env HTTPS_PROXY= https_proxy= ALL_PROXY= all_proxy= NO_PROXY='*' no_proxy='*' "${environment[@]}")
  if "$memcheck"; then
    command+=(valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --track-origins=yes)
  fi
  command+=("$driver" "$origin" "$ca" "$cache" "$repo" "$revision" "$filename" "$hashes" "$allow" "$mode" "$limit" "$attempts" "$timeout")
}
read_result() {
  result_status= path= output_null= message= total_hi=
  while IFS='=' read -r key value; do
    case "$key" in status) result_status=$value ;; path) path=$value ;; output_null) output_null=$value ;; message) message=$value ;; total_hi) total_hi=$value ;; esac
  done < "$1"
  [[ "$message" != *SYNTHETIC_SECRET* ]]
}
run() {
  prepare
  if ! "${command[@]}" > "$work/result" 2> "$work/error"; then
    cat "$work/error" >&2; return 1
  fi
  read_result "$work/result"
  if [[ "$result_status" != "$1" ]]; then cat "$work/result" "$work/error" >&2; return 1; fi
  [[ "$1" == ok || "$output_null" == 1 ]]
  [[ -z $(find "$work" -name '.ailo-partial-*' -print -quit) ]]
}
no_requests() { [[ $(request_count) -eq "$begin_count" ]]; }
case_begin offline
filename=nested/model.gguf run ok
cmp "$path" "$fixture"
cached=$path
begin_count=$(request_count)
filename=nested/model.gguf allow=0 run ok
filename=missing.gguf allow=0 run download_failed
no_requests
[[ ! -e ${cached%/*}/missing.gguf ]]

case_begin identities
filename='nested/a space%?#.gguf' revision=refs/pr/1 run ok
cached=$path
tail -1 "$suite/requests" | rg -q '/resolve/refs%2Fpr%2F1/nested/a%20space%25%3F%23.gguf\?download=true'
filename='nested/a space%?#.gguf' revision=other mode=path run ok
[[ "$path" != "$cached" ]]
environment=("XDG_CACHE_HOME=$work/xdg" "HOME=$work/home")
mode=path run ok; [[ "$path" == "$work/cache/models/"* ]]
cache=- mode=path run ok; [[ "$path" == "$work/xdg/ailo/models/"* ]]
environment=(XDG_CACHE_HOME= "HOME=$work/home")
cache=- mode=path run ok; [[ "$path" == "$work/home/.cache/ailo/models/"* ]]
environment=(XDG_CACHE_HOME=relative)
cache=- mode=path run invalid_argument
environment=(XDG_CACHE_HOME= HOME=)
cache=- mode=path run invalid_argument
[[ ! -e "$work/xdg" && ! -e "$work/home" ]]

case_begin invalid
for name in '../evil.gguf' '/absolute.gguf' 'a/../evil.gguf' 'a//evil.gguf' 'a\evil.gguf' '' '.ailo-state.gguf' 'nested/.ailo-state/model.gguf'; do
  filename=$name run invalid_argument
done
for name in '../model' model a/b/c owner/../model; do repo=$name run invalid_argument; done
revision=../bad run invalid_argument
hashes=bad run invalid_argument
cache=relative run invalid_argument
mode=bad-token run invalid_argument
attempts=7 run invalid_argument
timeout=-1 run invalid_argument
no_requests; [[ ! -e "$cache" ]]

case_begin failures
for name in error.gguf bad.gguf short.gguf truncated.gguf; do filename=$name run download_failed; done
filename=wrong-sha.gguf hashes=$zeros run download_failed
filename=too-big.gguf limit=1024 run download_failed
filename=chunked.gguf limit=1024 run download_failed
filename=stall.gguf timeout=25 run download_failed
[[ -z $(find "$cache" -name '*.gguf' -print -quit) ]]

case_begin redirects
filename=redirect.gguf run ok
IFS=$'\t' read -r seen_port seen_path auth < <(tail -1 "$suite/requests")
[[ "$auth" == 'Bearer SYNTHETIC_SECRET' ]]
filename=cross.gguf run ok
IFS=$'\t' read -r seen_port seen_path auth < <(tail -1 "$suite/requests")
[[ "$seen_port" == "$other_port" && "$auth" == - ]]
other_count=$(awk -F '\t' -v port="$other_port" '$1==port {n++} END {print n+0}' "$suite/requests")
for name in http.gguf credentials.gguf loop.gguf; do filename=$name run download_failed; done
[[ $(awk -F '\t' -v port="$other_port" '$1==port {n++} END {print n+0}' "$suite/requests") -eq "$other_count" ]]
begin_count=$(request_count)
filename=origin.gguf origin="http://127.0.0.1:$port" run download_failed
filename=untrusted.gguf ca=- run download_failed
filename=wrong-host.gguf ca="$suite/wrong-cert.pem" origin="https://127.0.0.1:$wrong_port" run download_failed
no_requests

case_begin retry
filename=retry.gguf attempts=2 run ok
[[ $(awk '/retry.gguf/ {n++} END {print n+0}' "$suite/requests") -eq 2 ]]
filename=error.gguf attempts=3 run download_failed
# One earlier failure request plus three bounded retries in this case.
[[ $(awk '/error.gguf/ {n++} END {print n+0}' "$suite/requests") -eq 4 ]]
filename=chunked.gguf run ok

case_begin threads
filename=slow.gguf mode=threaded run ok
[[ $(request_count) -eq $((begin_count+2)) && $(find "$cache" -name '*.gguf' | wc -l) -eq 2 ]]

case_begin cancellation
filename=before.gguf mode=cancel-before run cancelled
no_requests
filename=slow.gguf mode=cancel-transfer run cancelled
filename=throw.gguf mode=throw-transfer run internal_error
filename=throw-before.gguf mode=throw-before run internal_error
[[ -z $(find "$cache" -name '*.gguf' -print -quit) ]]

if ! "$memcheck"; then
  case_begin corrupt_hit
  run ok
  cached=$path
  cp "$fixture" "$work/corrupt"
  printf X | dd of="$work/corrupt" bs=1 seek=$(($(wc -c < "$fixture")-1)) conv=notrunc status=none
  cmp -s "$fixture" "$work/corrupt" && { printf 'corruption fixture was unchanged\n' >&2; exit 1; }
  cp "$work/corrupt" "$cached"
  begin_count=$(request_count)
  run download_failed
  cmp "$cached" "$work/corrupt"; no_requests

  case_begin publication
  mode=clobber run busy
  [[ $(cat "$path") == 'caller-owned sentinel' ]]
fi

case_begin processes
filename=slow.gguf; prepare
"${command[@]}" > "$work/first" 2> "$work/first-error" &
first_pid=$!
for ((i=0; i<500; ++i)); do
  [[ $(request_count) -le "$begin_count" ]] || break
  kill -0 "$first_pid" 2>/dev/null || { cat "$work/first-error" >&2; exit 1; }
  sleep 0.01
done
[[ $(request_count) -gt "$begin_count" ]]
"${command[@]}" > "$work/second" 2> "$work/second-error" &
second_pid=$!
wait "$first_pid" || { cat "$work/first-error" >&2; exit 1; }; first_pid=
wait "$second_pid" || { cat "$work/second-error" >&2; exit 1; }; second_pid=
read_result "$work/first"; [[ "$result_status" == ok ]]
read_result "$work/second"; [[ "$result_status" == ok ]]
[[ $(request_count) -eq $((begin_count+1)) ]]
filename=model.gguf mode=path run ok
directory=${path%/*}; mkdir -p "$directory"
lock_hash=$(printf model.gguf | sha256sum); lock_hash=${lock_hash%% *}
exec {lock_fd}> "$directory/.ailo-$lock_hash.lock"
flock -x "$lock_fd"
filename=model.gguf mode=busy-lock run busy
filename=model.gguf mode=cancel-lock run cancelled
exec {lock_fd}>&-
[[ $(request_count) -eq $((begin_count+1)) ]]

if ! "$memcheck"; then
  case_begin shards
  filename=nested/part-00001-of-00002.gguf hashes="$sha1,$zeros" mode=split run download_failed
  first=$path
  cmp "$first" "$part1"; [[ ! -e ${first%/*}/part-00002-of-00002.gguf ]]
  filename=nested/part-00001-of-00002.gguf hashes="$sha1,$sha2" mode=split run ok
  cmp "${first%/*}/part-00002-of-00002.gguf" "$part2"
  "$consumer" "$first" > "$work/consumer.log" 2>&1 || { cat "$work/consumer.log" >&2; exit 1; }
  rg -q 'C89 CPU GGUF inference' "$work/consumer.log"
  [[ $(request_count) -eq $((begin_count+3)) ]]
  begin_count=$(request_count)
  filename=part-00002-of-00002.gguf mode=split run invalid_argument
  no_requests

  case_begin interruption
  filename=slow.gguf; prepare
  "${command[@]}" > "$work/first" 2> "$work/first-error" &
  first_pid=$!
  partial=
  for ((i=0; i<500; ++i)); do
    partial=$(find "$work" -name '.ailo-partial-*' -size +0c -print -quit)
    [[ -z "$partial" ]] || break
    kill -0 "$first_pid" 2>/dev/null || { cat "$work/first-error" >&2; exit 1; }
    sleep 0.002
  done
  [[ -n "$partial" ]]
  kill -KILL "$first_pid"
  status=0; wait "$first_pid" 2>/dev/null || status=$?; first_pid=
  [[ "$status" -eq 137 && -e "$partial" && -z $(find "$cache" -name slow.gguf -print -quit) ]]
  run ok
  cmp "$path" "$fixture"

  case_begin shard_lock
  filename=part-00002-of-00002.gguf mode=path run ok
  directory=${path%/*}; mkdir -p "$directory"
  lock_hash=$(printf part-00001-of-00002.gguf | sha256sum); lock_hash=${lock_hash%% *}
  exec {lock_fd}> "$directory/.ailo-$lock_hash.lock"
  flock -x "$lock_fd"
  filename=part-00002-of-00002.gguf mode=busy-lock run busy
  exec {lock_fd}>&-
  target_hash=$(printf part-00002-of-00002.gguf | sha256sum); target_hash=${target_hash%% *}
  foreign_hash=$(printf other.gguf | sha256sum); foreign_hash=${foreign_hash%% *}
  target="$directory/.ailo-partial-$target_hash-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  foreign="$directory/.ailo-partial-$foreign_hash-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  printf 'interrupted target' > "$target"
  printf 'other model partial' > "$foreign"
  # This case deliberately keeps an unrelated partial, so assert cleanup narrowly.
  filename=part-00002-of-00002.gguf hashes=$sha2 prepare
  "${command[@]}" > "$work/result" 2> "$work/error"
  read_result "$work/result"
  [[ "$result_status" == ok && ! -e "$target" && $(cat "$foreign") == 'other model partial' ]]

  case_begin symlinks
  mode=path run ok
  mkdir -p "${path%/*}"
  cp "$fixture" "$work/caller.gguf"
  ln -s "$work/caller.gguf" "$path"
  run io_failed; cmp "$work/caller.gguf" "$fixture"
  cmake -E rm -rf "$cache"
  ln -s "$work" "$cache"
  run io_failed; no_requests
  cache="$work/wide" mode=sparse hashes=- run ok
  [[ "$total_hi" == 2 ]]
  cache="$work/wide" mode=sparse hashes=- limit=8589934591 run download_failed
fi
printf 'Model acquisition TLS, streaming, cache, publication and failure-mode checks passed\n'
