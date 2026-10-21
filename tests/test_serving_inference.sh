#!/usr/bin/env bash
# Trained-model protocol/semantic acceptance; never part of the fast unit suite.
set -euo pipefail
[[ $# -ge 3 && $# -le 4 ]] || exit 2
fixture=$1 model=$2 parent=$3
case_name=${4:-all}
[[ "$case_name" =~ ^(all|protocol|compaction|transport)$ ]] || exit 2
source "$(dirname -- "${BASH_SOURCE[0]}")/../scripts/serving_test_lib.sh"
http_timeout=90
serving_start "$fixture" "$model" "$parent"
control '{"pouch":true,"target_tokens":512,"recent_turns":1,"summary_tokens":512,"inspect_compaction":true}'
if [[ "$case_name" == all || "$case_name" == protocol ]]; then
# An automatic tool decision, not a forced tool_choice. The application executes
# the selected read against an owned fixture file and submits the real contents.
printf 'The deployment code is ORCHID-742.\n' > "$work/deployment.txt"
question='Use read_note to read deployment.txt. What is the deployment code?'
chat=$(jq -nc --arg q "$question" '{model:"ailo-fixture",stream:true,temperature:0,max_tokens:128,messages:[{role:"system",content:"Use the supplied tool to read files. Never invent file contents. After a tool result, answer briefly with the exact deployment code."},{role:"user",content:$q}],tools:[{type:"function",function:{name:"read_note",description:"Read a local note by filename.",parameters:{type:"object",properties:{filename:{type:"string"}},required:["filename"],additionalProperties:false}}}]}')
http 200 POST /chat/completions "$chat"
sed -n 's/^data: //p' "$body_file" | sed '/^\[DONE\]$/d' | jq -s '[.[]|.choices[]?.delta.tool_calls[]?] | {id:([.[].id // empty]|join("")),type:"function",function:{name:([.[].function.name // empty]|join("")),arguments:([.[].function.arguments // empty]|join(""))}}' > "$work/call.json"
jq -e '.function.name=="read_note" and (.id|length)>0 and (.function.arguments|fromjson|.filename=="deployment.txt")' "$work/call.json" >/dev/null
chat=$(jq -c --slurpfile call "$work/call.json" --rawfile result "$work/deployment.txt" '.messages += [{role:"assistant",tool_calls:$call},{role:"tool",tool_call_id:$call[0].id,content:$result}]' <<< "$chat")
http 200 POST /chat/completions "$chat"
sed -n 's/^data: //p' "$body_file" | sed '/^\[DONE\]$/d' | jq -se '[.[]|.choices[]?.delta.content // empty]|join("")|contains("ORCHID-742")' >/dev/null
# Responses keeps prior inputs/results in encrypted Pouch. Only the new user
# input is sent on continuation, including after a real listener restart.
response_request=$(jq -c '{model,stream,temperature,max_output_tokens:128,input:.messages[0:2],tools:[.tools[]|.function+{type:"function"}]}' <<< "$chat")
http 200 POST /responses "$response_request"
terminal
tool_response=$(jq -r .id "$work/terminal.json")
jq -e '.output[0].type=="function_call" and .output[0].name=="read_note" and (.output[0].arguments|fromjson|.filename=="deployment.txt")' "$work/terminal.json" >/dev/null
call_id=$(jq -r .output[0].call_id "$work/terminal.json")
http 200 POST /responses "$(jq -nc --arg id "$tool_response" --arg call "$call_id" --rawfile result "$work/deployment.txt" '{previous_response_id:$id,input:[{type:"function_call_output",call_id:$call,output:$result}],stream:true,temperature:0,max_output_tokens:48}')"
terminal
response=$(jq -r .id "$work/terminal.json")
jq -e 'any(.output[];.type=="message" and any(.content[];.text|contains("ORCHID-742")))' "$work/terminal.json" >/dev/null
serving_stop
serving_start "$fixture" "$model" "$parent" "$work"
http 200 POST /responses "$(jq -nc --arg id "$response" '{previous_response_id:$id,input:"What is the deployment code?",stream:true,temperature:0,max_output_tokens:48}')"
terminal
jq -e 'any(.output[];.type=="message" and any(.content[];.text|contains("ORCHID-742")))' "$work/terminal.json" >/dev/null
[[ $(stat -c %a "$work/pouch-config/pouch.key") == 600 ]]
fi
if [[ "$case_name" == all || "$case_name" == compaction ]]; then
# Actual summary generation, with old tool groups and a protected instruction.
# Padding is semantically irrelevant history, sufficient to force reduction.
history=$(jq -nc '[{role:"system",content:"Always begin final answers with VERIFIED. Answer questions using facts from historical conversation summaries. Never change the deployment code."},{role:"user",content:"Read the deployment note. Remember its exact code for future questions."},{type:"function_call",call_id:"call_history",name:"read_note",arguments:"{\"filename\":\"deployment.txt\"}"},{type:"function_call_output",call_id:"call_history",output:"The deployment code is ORCHID-742."},{role:"assistant",content:"The deployment code is ORCHID-742."},{role:"user",content:("Historical routine checks found no other changes. "*90)},{role:"assistant",content:"Routine checks complete."},{role:"user",content:"Keep the deployment code available for the next question."},{role:"assistant",content:"OK."}]')
http 200 POST /responses/compact "$(jq -nc --argjson h "$history" '{model:"ailo-fixture",input:$h}')"
check '.object=="response.compaction" and any(.output[];.type=="compaction" and (.encrypted_content|length)>0)'
jq -e '.compaction_passes>0 and .completion_tokens>0' "$work/latest.json" >/dev/null
cp "$body_file" "$work/compacted.json"
jq -e 'any(.output[];.role?=="system" and any(.content[];.text=="Always begin final answers with VERIFIED. Answer questions using facts from historical conversation summaries. Never change the deployment code."))' "$work/compacted.json" >/dev/null
http 200 POST /responses "$(jq -c '{input:(.output+[{role:"user",content:"What deployment code did the earlier tool read?"}]),stream:true,store:false,temperature:0,max_output_tokens:64}' "$work/compacted.json")"
terminal
jq -e 'any(.output[];.type=="message" and any(.content[];(.text|contains("ORCHID-742"))))' "$work/terminal.json" >/dev/null
[[ -s "$work/replay-$http_count.txt" ]]
tail -n +2 "$work/replay-$http_count.txt" | jq -e 'any(.facts[];.kind=="function_result" and .call_id=="call_history" and (.text|contains("ORCHID-742")))' >/dev/null
control '{"pouch":true}'
http 400 POST /responses "$(jq -c '{input:(.output+[{role:"user",content:"What code?"}]),stream:true} | (.input[]|select(.type=="compaction").encrypted_content)+="A"' "$work/compacted.json")"
fi
if [[ "$case_name" == all || "$case_name" == protocol ]]; then
http 200 DELETE "/responses/$response"
http 404 GET "/responses/$response"
fi
if [[ "$case_name" == all || "$case_name" == transport ]]; then
  control '{"pouch":true,"hold_transport":true}'
  mkfifo "$work/release"
  exec {release_in}<>"$work/release"
  for mode in continue disconnect; do
    http_count=$((http_count+1))
    "$(dirname -- "$fixture")/ailo_serving_stream_client" "$port" "$work" "$http_count" "$mode"
    serving_await "$http_count"
    if [[ "$mode" == continue ]]; then
      jq -e '.result=="ok" and .completed and .stored and .completion_tokens>1' "$work/latest.json" >/dev/null
    else
      jq -e '.result=="cancelled" and (.completed|not) and (.stored|not)' "$work/latest.json" >/dev/null
    fi
  done
  control '{"pouch":true}'
  read -r cancelled < "$work/cancelled-id"
  http 404 GET "/responses/$cancelled"
  http 200 POST /responses '{"input":"Say hello.","stream":true,"temperature":0,"max_output_tokens":16}'
  terminal
  jq -e '.status=="completed"' "$work/terminal.json" >/dev/null
  printf 'Trained streaming under backpressure, peer disconnect, unpublished cancellation and recovery: passed\n'
fi
[[ ! -s "$work/fixture.stdout" ]]
printf 'Trained serving %s: passed\n' "$case_name"
