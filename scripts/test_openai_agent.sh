#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
preset=${1:-x86_64-linux-gnu-release}
[[ $# -le 1 && "$preset" =~ ^(cpu|debug|x86_64-linux-gnu-release)$ ]] || { printf 'Usage: test_openai_agent.sh [native-preset]\n' >&2; exit 2; }
for tool in codex timeout curl jq rg; do
  command -v "$tool" >/dev/null || { printf 'Agent acceptance requires host tool: %s\n' "$tool" >&2; exit 2; }
done
cd "$root"
cmake --build "build/$preset" --target ailo_serving_fixture --parallel "${AILO_JOBS:-2}"
ctest --test-dir "build/$preset" --output-on-failure -j1 -R '^model-fixture$'
source "$root/scripts/serving_test_lib.sh"
serving_start "$root/build/$preset/ailo_serving_fixture" "$root/build/$preset/model-fixtures/tiny-llama.gguf" "$root/build"
control '{"pouch":true}'
mkdir -p "$work/agent" "$work/state" "$work/logs" "$work/codex-home"
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
# Declare remote compaction explicitly on a custom HTTP Responses provider.
coproc CODEX {
  exec timeout --signal=TERM --kill-after=5 120 env -u CODEX_SESSION_ID -u CODEX_THREAD_ID -u CODEX_TOOL_CALL_ID CODEX_HOME="$work/codex-home" codex \
    --disable apps --disable plugins --disable remote_plugin --disable goals \
    --disable multi_agent --disable multi_agent_v2 --disable shell_snapshot --disable shell_snapshot_v2 \
    -c 'model_provider="ailo-test"' \
    -c 'model_providers.ailo-test.name="libailo acceptance"' \
    -c 'model_providers.ailo-test.capabilities.remote_compaction="v2"' \
    -c "model_providers.ailo-test.base_url=\"$url\"" \
    -c 'model_providers.ailo-test.wire_api="responses"' \
    -c 'model_providers.ailo-test.requires_openai_auth=false' \
    -c 'model_providers.ailo-test.supports_websockets=false' \
    -c 'model_providers.ailo-test.request_max_retries=0' \
    -c 'model_providers.ailo-test.stream_max_retries=0' \
    -c "model_catalog_json=\"$work/models-catalog.json\"" \
    -c 'model_reasoning_summary="none"' -c 'model_auto_compact_token_limit=1000000' \
    -c 'web_search="disabled"' -c 'approval_policy="never"' \
    -c 'analytics.enabled=false' -c 'history.persistence="none"' \
    -c "sqlite_home=\"$work/state\"" -c "log_dir=\"$work/logs\"" \
    -c 'project_doc_max_bytes=0' app-server --stdio 2> "$work/codex.stderr"
}
codex_pid=$CODEX_PID
exec {codex_in}>&"${CODEX[1]}"
exec {codex_out}<&"${CODEX[0]}"
rpc() {
  jq -nc --argjson id "$1" --arg method "$2" --argjson params "$3" '{id:$id,method:$method,params:$params}' >&"$codex_in"
}
read_event() {
  if ! IFS= read -r -t 45 -u "$codex_out" event; then
    printf 'Codex ended or timed out before expected event\n' >&2
    return 1
  fi
  printf '%s\n' "$event" >> "$work/codex.jsonl"
  if [[ "$event" == *'"error":{'* ]]; then
    printf 'Codex reported error: %s\n' "$event" >&2
    return 1
  fi
}
reply() {
  while read_event; do
    if [[ "$event" != *'"method":'* ]] && jq -e --argjson id "$1" '.id==$id' <<< "$event" >/dev/null; then
      printf '%s\n' "$event" > "$work/reply-$1.json"
      return 0
    fi
  done
  return 1
}
turn_done() {
  while read_event; do
    if [[ "$event" == *'"method":"turn/completed"'* ]]; then
      jq -e '.params.turn.status=="completed"' <<< "$event" >/dev/null
      return
    fi
  done
  return 1
}
rpc 1 initialize '{"clientInfo":{"name":"ailo_e2e","version":"1"},"capabilities":{"experimentalApi":true}}'
reply 1
printf '%s\n' '{"method":"initialized"}' >&"$codex_in"
rpc 2 thread/start "$(jq -nc --arg cwd "$work/agent" '{model:"ailo-fixture",modelProvider:"ailo-test",cwd:$cwd,approvalPolicy:"never",sandbox:"read-only",ephemeral:true,baseInstructions:"Execute the harmless requested tool and report the model answer.",developerInstructions:"Local protocol acceptance only."}')"
reply 2
thread=$(jq -r .result.thread.id "$work/reply-2.json")
[[ -n "$thread" && "$thread" != null ]]
# Two user turns, with actual shell execution and function-result round trips.
for stage in first second; do
  if [[ "$stage" == first ]]; then
    prompt=$(jq -nr '"AILO_REMEMBER:orchid AILO_TOOL:first. Historical padding: "+("x"*12000)')
    id=3
  else
    prompt='AILO_EXPECT:orchid AILO_TOOL:second'
    id=4
  fi
  rpc "$id" turn/start "$(jq -nc --arg t "$thread" --arg text "$prompt" '{threadId:$t,input:[{type:"text",text:$text}]}')"
  reply "$id"
  turn_done
  jq -se --arg answer "AILO_DONE:$stage" 'any(.[];.method=="item/completed" and .params.item.type=="agentMessage" and .params.item.text==$answer)' "$work/codex.jsonl" >/dev/null
done
rpc 5 thread/compact/start "$(jq -nc --arg t "$thread" '{threadId:$t}')"
reply 5
turn_done
jq -se 'any(.[];.method=="item/completed" and .params.item.type=="contextCompaction")' "$work/codex.jsonl" >/dev/null
# Require remote compaction plus a real summary pass, not local fallback or a
# successful no-op. The following turn must carry the opaque result back.
jq -se 'any(.[];.input? and any(.input[]?;.type=="compaction_trigger"))' "$work"/request-*.json >/dev/null
jq -e '.summaries>0' "$work/latest.json" >/dev/null
rpc 6 turn/start "$(jq -nc --arg t "$thread" '{threadId:$t,input:[{type:"text",text:"AILO_EXPECT:orchid AILO_CHECK_TOOL:first AILO_TOOL:third"}]}')"
reply 6
turn_done
jq -se 'any(.[];.method=="item/completed" and .params.item.type=="agentMessage" and .params.item.text=="AILO_DONE:third") and ([.[]|select(.method=="item/completed" and .params.item.type=="commandExecution" and .params.item.exitCode==0)]|length)==3' "$work/codex.jsonl" >/dev/null
jq -se 'any(.[];.input? and any(.input[]?;.type=="compaction" and (.encrypted_content|length)>0))' "$work"/request-*.json >/dev/null
rg -q 'fixture calls=3 results=3 answers=3 summaries=1' "$work/fixture.log"
printf 'Codex multi-turn tool execution, remote compaction, opaque replay and post-compaction tool execution passed.\n'
