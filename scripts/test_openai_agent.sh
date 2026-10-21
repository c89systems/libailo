#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
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
source "$root/scripts/codex_test_lib.sh"
codex_start
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
