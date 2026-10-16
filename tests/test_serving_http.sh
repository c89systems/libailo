#!/usr/bin/env bash
# Wire-level acceptance for the implemented OpenAI profile, with simulated
# generation. Ordinary CTest runs this without Codex or external credentials.
set -euo pipefail
[[ $# -eq 3 ]] || { printf 'Usage: test_serving_http.sh FIXTURE MODEL BUILD_DIRECTORY\n' >&2; exit 2; }
source "$(dirname -- "${BASH_SOURCE[0]}")/../scripts/serving_test_lib.sh"
serving_start "$1" "$2" "$3"
http 200 GET /models
check '.object=="list" and .data[0].id=="ailo-fixture"'
http 200 GET /models/ailo-fixture
check '.id=="ailo-fixture"'
http 404 GET /models/missing
check '.error.type=="invalid_request_error"'
http 400 POST /responses '{'
check '.error.param==null and .error.type=="invalid_request_error"'
http 405 DELETE /responses
rg -qi '^Allow: POST' "$work/headers-$http_count"
http 404 GET /unknown

# Stored lineage: next requests carry ONLY new input. Instructions are per-call;
# each child remains usable after its parent has been deleted.
http 200 POST /responses '{"input":"AILO_REMEMBER:violet","instructions":"FIRST_ONLY","stream":true}'
terminal
parent=$(jq -r .id "$work/terminal.json")
jq -e '.status=="completed" and .output[0].content[0].text=="AILO_ACK:violet"' "$work/terminal.json" >/dev/null
http 200 GET "/responses/$parent"
check '.status=="completed"'
http 200 GET "/responses/$parent/input_items?limit=1&order=asc"
check '.data[0].role=="user"'
http 200 POST /responses "$(jq -nc --arg id "$parent" '{previous_response_id:$id,input:"AILO_EXPECT:violet",instructions:"SECOND_ONLY",stream:true}')"
terminal
child=$(jq -r .id "$work/terminal.json")
jq -e '.output[0].content[0].text=="AILO_RECALLED:violet"' "$work/terminal.json" >/dev/null
generation=$(jq -r .generations "$work/latest.json")
jq -e '(.effective | contains("SECOND_ONLY")) and (.effective | contains("FIRST_ONLY") | not) and ([.effective | scan("AILO_REMEMBER:violet")]|length)==1' "$work/generation-$generation.json" >/dev/null
http 200 DELETE "/responses/$parent"
check '.deleted==true'
http 404 GET "/responses/$parent"
http 404 POST /responses "$(jq -nc --arg id "$parent" '{previous_response_id:$id,input:"Next"}')"
http 200 POST /responses "$(jq -nc --arg id "$child" '{previous_response_id:$id,input:"AILO_EXPECT:violet",store:false,stream:true}')"
terminal
jq -e '.output[0].content[0].text=="AILO_RECALLED:violet"' "$work/terminal.json" >/dev/null
control '{"namespace":"tenant-B"}'
http 404 GET "/responses/$child"
http 404 POST /responses "$(jq -nc --arg id "$child" '{previous_response_id:$id,input:"Next"}')"
control '{"fault":"expire"}'
http 404 GET "/responses/$child"
http 404 POST /responses "$(jq -nc --arg id "$child" '{previous_response_id:$id,input:"Next"}')"
control '{}'
http 200 POST /responses '{"input":"Unstored","store":false}'
unstored=$(jq -r .id "$body_file")
http 404 POST /responses "$(jq -nc --arg id "$unstored" '{previous_response_id:$id,input:"Next"}')"
for fault in open read; do
  control "{\"fault\":\"$fault\"}"
  before=$(jq -r .generations "$work/latest.json")
  http 500 POST /responses "$(jq -nc --arg id "$child" '{previous_response_id:$id,input:"Next"}')"
  jq -e --argjson n "$before" '.generations==$n and .stored==false and .aborted==1' "$work/latest.json" >/dev/null
done
control '{}'
http 200 POST /responses '{"input":"AILO_REMEMBER:birch AILO_TOOL:wire","stream":true,"tools":[{"type":"function","name":"fixture_echo","parameters":{"type":"object","properties":{"value":{"type":"string"}}}}]}'
terminal
tool_response=$(jq -r .id "$work/terminal.json")
jq -e '.output[0].type=="function_call" and .output[0].call_id=="call_ailo_wire"' "$work/terminal.json" >/dev/null
http 200 POST /responses "$(jq -nc --arg id "$tool_response" '{previous_response_id:$id,input:[{type:"function_call_output",call_id:"call_ailo_wire",output:"AILO_TOOL_OK_wire"}],stream:true}')"
terminal
jq -e '.output[0].content[0].text=="AILO_DONE:wire"' "$work/terminal.json" >/dev/null
tool_child=$(jq -r .id "$work/terminal.json")
http 200 POST /responses "$(jq -nc --arg id "$tool_child" '{previous_response_id:$id,input:"AILO_EXPECT:birch",stream:true}')"
terminal
jq -e '.output[0].content[0].text=="AILO_RECALLED:birch"' "$work/terminal.json" >/dev/null

# Chat state belongs to the caller; tool calls/results must also be replayed.
chat='{"model":"ailo-fixture","store":true,"stream":true,"messages":[{"role":"user","content":"AILO_REMEMBER:cedar AILO_TOOL:chat"}],"tools":[{"type":"function","function":{"name":"fixture_echo","parameters":{"type":"object","properties":{"value":{"type":"string"}}}}}]}'
http 200 POST /chat/completions "$chat"
rg -q 'data: \[DONE\]' "$body_file"
sed -n 's/^data: //p' "$body_file" | sed '/^\[DONE\]$/d' | jq -s '.[0].id' > "$work/chat-id.json"
chat_id=$(jq -r . "$work/chat-id.json")
http 200 POST /chat/completions '{"model":"ailo-fixture","store":false,"stream":true,"messages":[{"role":"user","content":"AILO_REMEMBER:cedar AILO_TOOL:chat"},{"role":"assistant","tool_calls":[{"id":"call_ailo_chat","type":"function","function":{"name":"fixture_echo","arguments":"{\"value\":\"chat\"}"}}]},{"role":"tool","tool_call_id":"call_ailo_chat","content":"AILO_TOOL_OK_chat"},{"role":"user","content":"AILO_EXPECT:cedar"}]}'
rg -q 'data: \[DONE\]' "$body_file"
sed -n 's/^data: //p' "$body_file" | sed '/^\[DONE\]$/d' | jq -se '[.[]|.choices[]?.delta.content // empty]|join("")=="AILO_RECALLED:cedar"' >/dev/null
http 200 GET "/chat/completions/$chat_id"
check '.choices[0].message.tool_calls[0].id=="call_ailo_chat"'
http 200 POST "/chat/completions/$chat_id" '{"metadata":{"suite":"wire"}}'
check '.metadata.suite=="wire"'
http 200 GET '/chat/completions?metadata[suite]=wire&limit=1&order=asc'
check '.data|length==1'
http 200 GET "/chat/completions/$chat_id/messages?limit=1&order=asc"
check '.data[0].content|contains("AILO_REMEMBER:cedar")'
http 200 DELETE "/chat/completions/$chat_id"
check '.deleted==true'
http 404 GET "/chat/completions/$chat_id"

# Real standalone compaction: only the summary model output is simulated.
history=$(jq -nc '[{role:"system",content:"Protected policy"},{role:"user",content:("AILO_REMEMBER:maple "+("x"*12000))},{role:"assistant",content:"Earlier answer"}]')
http 200 POST /responses/compact "$(jq -nc --argjson input "$history" '{model:"ailo-fixture",input:$input,instructions:"COMPACT_ONLY"}')"
check '.object=="response.compaction" and any(.output[];.type=="compaction" and (.encrypted_content|length)>0)'
cp "$body_file" "$work/compacted.json"
jq -e '.summaries>0 and .reserved==0' "$work/latest.json" >/dev/null
replay=$(jq -c '{input:(.output+[{role:"user",content:"AILO_EXPECT:maple"}]),store:false,stream:true}' "$work/compacted.json")
http 200 POST /responses "$replay"
terminal
jq -e '.output[0].content[0].text=="AILO_RECALLED:maple"' "$work/terminal.json" >/dev/null
generation=$(jq -r .generations "$work/latest.json")
jq -e '(.effective|contains("COMPACT_ONLY")|not)' "$work/generation-$generation.json" >/dev/null
http 400 POST /responses "$(jq -c '(.input[]|select(.type=="compaction").encrypted_content)+="A"' <<< "$replay")"
http 400 POST /responses "$(jq -c '.input|=.[1:]' <<< "$replay")"
control '{"namespace":"tenant-B"}'
http 400 POST /responses "$replay"
control '{}'

# Automatic compaction: check streamed control items, stateful chaining and the
# documented stateless tail replay after dropping the preceding input window.
http 200 POST /responses "$(jq -nc --argjson h "$history" '{input:($h+[{role:"user",content:"AILO_EXPECT:maple"}]),stream:true,context_management:[{type:"compaction",compact_threshold:10000}]}')"
terminal
cp "$work/terminal.json" "$work/automatic.json"
jq -e 'any(.output[];.type=="compaction" and (.encrypted_content|length)>0) and any(.output[];.type=="message" and .content[0].text=="AILO_RECALLED:maple")' "$work/automatic.json" >/dev/null
auto_id=$(jq -r .id "$work/automatic.json")
http 200 POST /responses "$(jq -nc --arg id "$auto_id" '{previous_response_id:$id,input:"AILO_EXPECT:maple",stream:true}')"
terminal
jq -e '.output[0].content[0].text=="AILO_RECALLED:maple"' "$work/terminal.json" >/dev/null
http 200 POST /responses "$(jq -c '{input:(.output+[{role:"user",content:"AILO_EXPECT:maple"}]),store:false,stream:true}' "$work/automatic.json")"
terminal
jq -e '.output[0].content[0].text=="AILO_RECALLED:maple"' "$work/terminal.json" >/dev/null

# The current Codex remote-compaction protocol uses a final input trigger on
# Responses create. Its only output is a replayable compaction item.
http 200 POST /responses "$(jq -nc --argjson h "$history" '{input:($h+[{type:"compaction_trigger",id:"trigger-wire"}]),stream:true}')"
terminal
cp "$work/terminal.json" "$work/triggered.json"
jq -e '.status=="completed" and (.output|length)==1 and .output[0].type=="compaction"' "$work/triggered.json" >/dev/null
trigger_id=$(jq -r .id "$work/triggered.json")
http 200 GET "/responses/$trigger_id/input_items?order=asc&limit=100"
check '.data[-1].type=="compaction_trigger" and .data[-1].id=="trigger-wire"'
http 200 POST /responses "$(jq -nc --arg id "$trigger_id" '{previous_response_id:$id,input:"AILO_EXPECT:maple",stream:true}')"
terminal
jq -e '.output[0].content[0].text=="AILO_RECALLED:maple"' "$work/terminal.json" >/dev/null
http 200 POST /responses "$(jq -c '{input:(.output+[{role:"user",content:"AILO_EXPECT:maple"}]),store:false,stream:true}' "$work/triggered.json")"
terminal
jq -e '.output[0].content[0].text=="AILO_RECALLED:maple"' "$work/terminal.json" >/dev/null
http 400 POST /responses '{"input":[{"type":"compaction_trigger"},{"role":"user","content":"Not final"}]}'
http 400 POST /responses '{"input":[{"type":"compaction_trigger"}],"store":false}'
http 200 POST /responses '{"input":[{"role":"user","content":"AILO_REMEMBER:ash"},{"role":"assistant","content":"Remembered"},{"type":"compaction_trigger"}],"instructions":"NOOP_ONLY","stream":false}'
check '.status=="completed" and (.output|length)==1 and .output[0].type=="compaction" and .usage.total_tokens==0'
cp "$body_file" "$work/checkpoint.json"
short_id=$(jq -r .id "$work/checkpoint.json")
http 200 POST /responses "$(jq -nc --arg id "$short_id" '{previous_response_id:$id,input:"AILO_EXPECT:ash",stream:true}')"
terminal
jq -e '.output[0].content[0].text=="AILO_RECALLED:ash"' "$work/terminal.json" >/dev/null
http 200 POST /responses "$(jq -c '{input:(.output+[{role:"user",content:"AILO_EXPECT:ash"}]),store:false,stream:true}' "$work/checkpoint.json")"
terminal
jq -e '.output[0].content[0].text=="AILO_RECALLED:ash"' "$work/terminal.json" >/dev/null
generation=$(jq -r .generations "$work/latest.json")
jq -e '(.effective|contains("NOOP_ONLY")|not)' "$work/generation-$generation.json" >/dev/null

# Durable conversations and every implemented conversation route.
http 200 POST /conversations '{"metadata":{"suite":"wire"},"items":[{"role":"user","content":"AILO_REMEMBER:pine"}]}'
conv=$(jq -r .id "$body_file")
http 200 GET "/conversations/$conv"
check '.metadata.suite=="wire"'
http 200 POST "/conversations/$conv" '{"metadata":{"suite":"updated"}}'
check '.metadata.suite=="updated"'
http 200 POST "/conversations/$conv/items" '{"items":[{"role":"assistant","content":"Recorded"}]}'
http 200 GET "/conversations/$conv/items?order=asc&limit=1"
check '.has_more==true and (.data|length)==1'
item=$(jq -r .first_id "$body_file")
http 200 GET "/conversations/$conv/items/$item"
check '.content[0].text=="AILO_REMEMBER:pine"'
http 200 GET "/conversations/$conv/items?order=asc&after=$item&limit=1"
check '.data[0].role=="assistant"'
http 200 POST /responses "$(jq -nc --arg c "$conv" '{conversation:$c,input:"AILO_EXPECT:pine",stream:true}')"
terminal
jq -e '.output[0].content[0].text=="AILO_RECALLED:pine"' "$work/terminal.json" >/dev/null
http 400 POST /responses "$(jq -nc --arg c "$conv" --arg p "$child" '{conversation:$c,previous_response_id:$p,input:"Invalid combination"}')"
control '{"namespace":"tenant-B"}'
http 404 GET "/conversations/$conv"
control '{}'
http 200 DELETE "/conversations/$conv/items/$item"
check '.object=="conversation"'
http 404 GET "/conversations/$conv/items/$item"
http 200 DELETE "/conversations/$conv"
check '.deleted==true'
http 404 GET "/conversations/$conv"
http 200 GET "/conversations/$conv/items?limit=100"
check '(.data|length)>0'

# Failed storage never publishes a response or pretends generation completed.
for fault in reserve write commit; do
  control "{\"fault\":\"$fault\"}"
  http 500 POST /responses '{"input":"Storage failure","stream":false}'
  check '.error.type=="server_error" and (.error.message|contains("Private")|not)'
  jq -e '.committed==0 and .aborted==1 and .stored==false' "$work/latest.json" >/dev/null
done
control '{"fault":"commit"}'
expected_curl=18 http 200 POST /responses '{"input":"Streaming commit failure","stream":true}'
rg -q '^event: response.failed$' "$body_file"
! rg -q '^event: response.completed$' "$body_file"
control '{}'

# The HTTP client must see a delta while generation is still paused. Then
# disconnect before publication and prove the unfinished record was aborted.
touch "$work/hold-stream"
http_count=$((http_count+1))
curl --silent --show-error --no-buffer --max-time 15 -H 'Content-Type: application/json' --data-binary '{"input":"AILO_REMEMBER:stream","stream":true}' "$url/responses" > "$work/live.sse" 2> "$work/live.stderr" &
curl_pid=$!
for ((attempt=0; attempt<300; ++attempt)); do
  if rg -q '^event: response.output_text.delta$' "$work/live.sse"; then break; fi
  sleep .01
done
rg -q '^event: response.output_text.delta$' "$work/live.sse"
[[ -s "$work/first-delta" && ! -e "$work/receipt-$http_count.json" ]]
kill "$curl_pid"
wait "$curl_pid" 2>/dev/null || true
curl_pid=
touch "$work/release-stream"
for ((attempt=0; attempt<200; ++attempt)); do
  [[ ! -s "$work/receipt-$http_count.json" ]] || break
  sleep .01
done
jq -e '.stored==false and .aborted==1 and .committed==0' "$work/receipt-$http_count.json" >/dev/null
! rg -q '^event: response.completed$' "$work/live.sse"
http 200 GET /models
printf 'HTTP profile passed: stateful Responses, stateless Chat/tools, compaction/replay, conversations/CRUD, storage failures and live disconnect.\n'
