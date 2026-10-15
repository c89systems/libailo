#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
preset=${1:-x86_64-linux-gnu-release}
[[ $# -le 1 && "$preset" =~ ^(cpu|debug|x86_64-linux-gnu-release)$ ]] || { printf 'Usage: test_openai_agent.sh [native-preset]\n' >&2; exit 2; }
for tool in codex timeout curl jq rg; do
  command -v "$tool" >/dev/null || { printf 'agent acceptance requires host tool: %s\n' "$tool" >&2; exit 2; }
done
cd "$root"
cmake --build "build/$preset" --target ailo_serving_fixture --parallel "${AILO_JOBS:-2}"
ctest --test-dir "build/$preset" --output-on-failure -j1 -R '^model-fixture$'
work=$(mktemp -d "$root/build/openai-agent.XXXXXX")
fixture_pid=
cleanup() {
  result=$?
  trap - EXIT
  if [[ -n "$fixture_pid" ]]; then
    kill "$fixture_pid" 2>/dev/null || true
    wait "$fixture_pid" 2>/dev/null || true
  fi
  printf 'Agent acceptance evidence: %s\n' "$work"
  if [[ $result -ne 0 ]]; then
    tail -15 "$work/fixture.log" >&2 || true
    [[ ! -f "$work/codex.stderr" ]] || tail -15 "$work/codex.stderr" >&2
  fi
  exit "$result"
}
trap cleanup EXIT
mkdir -p "$work/agent" "$work/state" "$work/logs"
"build/$preset/ailo_serving_fixture" "build/$preset/model-fixtures/tiny-llama.gguf" "$work" > "$work/fixture.stdout" 2> "$work/fixture.log" &
fixture_pid=$!
for ((attempt=0; attempt<100; ++attempt)); do
  [[ ! -s "$work/port" ]] || break
  sleep .05
done
[[ -s "$work/port" ]] || { printf 'Fixture did not become ready\n' >&2; exit 1; }
read -r port < "$work/port"
[[ "$port" =~ ^[0-9]+$ ]] || exit 1
url="http://127.0.0.1:$port/v1"
curl --silent --show-error --fail --max-time 10 "$url/models" > "$work/models.json"
jq -e '.object=="list" and .data[0].id=="ailo-fixture"' "$work/models.json" >/dev/null
status=$(curl --silent --show-error --max-time 10 -o "$work/error.json" -w '%{http_code}' -H 'Content-Type: application/json' --data-binary '{' "$url/responses")
[[ "$status" == 400 ]]
jq -e '.error.type=="invalid_request_error" and .error.param==null' "$work/error.json" >/dev/null
codex --version > "$work/codex.version"
codex debug models --bundled > "$work/bundled-models.json"
jq '{models:[(.models[0] | .slug="ailo-fixture" | .display_name="libailo fixture" |
  .base_instructions="Execute the requested harmless tool call and return its real result. Then report the final model answer." |
  .model_messages=null | .tool_mode=null | .default_reasoning_level="none" |
  .default_reasoning_summary="none" | .supported_reasoning_levels=[] |
  .support_verbosity=false | .multi_agent_version=null |
  .supports_reasoning_effort_updates=false | .apply_patch_tool_type=null |
  .experimental_supported_tools=[] | .use_responses_lite=false |
  .include_apps_usage_instructions=false | .include_plugin_usage_instructions=false |
  .include_skills_usage_instructions=false | .supports_search_tool=false)]}' \
  "$work/bundled-models.json" > "$work/models-catalog.json"
timeout --signal=TERM --kill-after=5 90 env -u CODEX_SESSION_ID -u CODEX_THREAD_ID -u CODEX_TOOL_CALL_ID codex exec \
  --ignore-user-config --ephemeral --skip-git-repo-check --sandbox read-only \
  --disable apps --disable plugins --disable remote_plugin --disable goals \
  --disable multi_agent --disable multi_agent_v2 \
  --disable shell_snapshot --disable shell_snapshot_v2 \
  -C "$work/agent" -m ailo-fixture --json -o "$work/final.txt" \
  -c 'model_provider="ailo-test"' \
  -c 'model_providers.ailo-test.name="libailo acceptance fixture"' \
  -c "model_providers.ailo-test.base_url=\"$url\"" \
  -c 'model_providers.ailo-test.wire_api="responses"' \
  -c 'model_providers.ailo-test.requires_openai_auth=false' \
  -c 'model_providers.ailo-test.supports_websockets=false' \
  -c 'model_providers.ailo-test.request_max_retries=0' \
  -c 'model_providers.ailo-test.stream_max_retries=0' \
  -c "model_catalog_json=\"$work/models-catalog.json\"" \
  -c 'model_reasoning_summary="none"' \
  -c 'web_search="disabled"' -c 'approval_policy="never"' \
  -c 'analytics.enabled=false' -c 'history.persistence="none"' \
  -c "sqlite_home=\"$work/state\"" -c "log_dir=\"$work/logs\"" \
  -c 'project_doc_max_bytes=0' \
  'This is a local protocol acceptance test. Execute the requested harmless printf tool call, return its real output to the model, and then report the final answer.' \
  < /dev/null > "$work/codex.jsonl" 2> "$work/codex.stderr"
rg -q '^AILO_AGENT_DONE$' "$work/final.txt"
jq -se 'any(.[]; .type=="item.completed" and .item.type=="command_execution" and .item.exit_code==0 and (.item.aggregated_output|contains("AILO_TOOL_OK")))' "$work/codex.jsonl" >/dev/null
rg -q 'fixture calls=1 results=1 answers=1' "$work/fixture.log"
kill "$fixture_pid"
wait "$fixture_pid"
fixture_pid=
printf 'Codex tool execution, returned result and final answer passed through libailo Responses/SSE.\n'
