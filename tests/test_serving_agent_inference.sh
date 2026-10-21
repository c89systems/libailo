#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 3 ]] || exit 2
root=$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$root/scripts/serving_test_lib.sh"
serving_start "$1" "$2" "$3"
control '{"pouch":true,"target_tokens":2450,"recent_turns":0,"summary_tokens":512}'
source "$root/scripts/codex_test_lib.sh"
agent_timeout=240 event_timeout=90
codex_start
jq -nr '"ORCHID-742\n"+("Routine deployment checks passed unchanged. "*120)' > "$work/agent/deployment.txt"
printf 'ORCHID-742\n' > "$work/agent/verification.txt"
for id in 3 4; do
  if [[ "$id" == 3 ]]; then
    code=ORCHID-742
    prompt='Call exec_command with exactly {"cmd":"cat deployment.txt"}. Omit workdir and all other optional arguments. Tell me the deployment code from the file, briefly, and remember it for later. Call the tool before answering.'
  else
    code=ORCHID-742
    prompt='A separate file verification.txt records the verification result. Its contents have not been read yet. Call exec_command with exactly {"cmd":"cat verification.txt"}, no other arguments. Read the new file before answering. Report its code briefly.'
  fi
  rpc "$id" turn/start "$(jq -nc --arg t "$thread" --arg text "$prompt" '{threadId:$t,input:[{type:"text",text:$text}]}')"
  reply "$id"
  turn_done
  turn_id=$(jq -r .params.turn.id <<< "$event")
  jq -se --arg turn "$turn_id" --arg code "$code" 'any(.[];.method=="item/completed" and .params.turnId==$turn and .params.item.type=="agentMessage" and (.params.item.text|contains($code))) and any(.[];.method=="item/completed" and .params.turnId==$turn and .params.item.type=="commandExecution" and .params.item.exitCode==0 and (.params.item.aggregatedOutput|contains($code)))' "$work/codex.jsonl" >/dev/null
done
rpc 5 thread/compact/start "$(jq -nc --arg t "$thread" '{threadId:$t}')"
reply 5
turn_done
turn_id=$(jq -r .params.turn.id <<< "$event")
jq -se 'any(.[];.method=="item/completed" and .params.item.type=="contextCompaction")' "$work/codex.jsonl" >/dev/null
jq -se 'any(.[];.input? and any(.input[]?;.type=="compaction_trigger"))' "$work"/request-*.json >/dev/null
jq -e '.compaction_passes>0' "$work/latest.json" >/dev/null
rpc 6 turn/start "$(jq -nc --arg t "$thread" '{threadId:$t,input:[{type:"text",text:"Without calling any tools, what exact deployment code did our earlier cat commands find? Answer with only the code."}]}')"
reply 6
turn_done
turn_id=$(jq -r .params.turn.id <<< "$event")
jq -se --arg turn "$turn_id" 'any(.[];.method=="item/completed" and .params.turnId==$turn and .params.item.type=="agentMessage" and (.params.item.text|contains("ORCHID-742")))' "$work/codex.jsonl" >/dev/null
jq -se 'any(.[];.input? and any(.input[]?;.type=="compaction" and (.encrypted_content|length)>0))' "$work"/request-*.json >/dev/null
read -r last_request < "$work/sequence"
jq -e 'any(.input[]?;.type=="compaction") and (.input|tojson|contains("ORCHID-742")|not) and all(.input[]?;.type!="function_call_output")' "$work/request-$last_request.json" >/dev/null
jq -se '([.[]|select(.method=="item/completed" and .params.item.type=="commandExecution")]|length)==2' "$work/codex.jsonl" >/dev/null
printf 'Trained Codex tool execution, multi-turn HTTP and semantic remote compaction: passed\n'
