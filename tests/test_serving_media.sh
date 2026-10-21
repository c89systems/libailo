#!/usr/bin/env bash
# Real media inference over the shared loopback transport and production Pouch.
set -euo pipefail
[[ $# -eq 5 ]] || exit 2
fixture=$1 model=$2 parent=$3 kind=$4 sample=$5
[[ "$kind" =~ ^(vision|audio)$ ]]
source "$(dirname -- "${BASH_SOURCE[0]}")/../scripts/serving_test_lib.sh"
http_timeout=90
serving_start "$fixture" "$model" "$parent"
control '{"pouch":true}'
base64 -w0 "$sample" > "$work/encoded.txt"
if [[ "$kind" == vision ]]; then
  jq -nc --rawfile data "$work/encoded.txt" '{type:"input_image",image_url:("data:image/jpeg;base64,"+$data)}' > "$work/part.json"
  request=$(jq -nc --slurpfile image "$work/part.json" '{input:[{role:"user",content:[$image[0],{type:"input_text",text:"What is the main color? Answer with one color word."}]}],stream:true,temperature:0,max_output_tokens:16}')
  http 200 POST /responses "$request"
  terminal
  jq -e 'any(.output[];.type=="message" and any(.content[];.text|ascii_downcase|contains("red")))' "$work/terminal.json" >/dev/null
  id=$(jq -r .id "$work/terminal.json")
  serving_stop
  serving_start "$fixture" "$model" "$parent" "$work"
  http 200 POST /responses "$(jq -nc --arg id "$id" '{previous_response_id:$id,input:"What color was the image? Answer with one color word.",stream:true,temperature:0,max_output_tokens:16}')"
  terminal
  jq -e 'any(.output[];.type=="message" and any(.content[];.text|ascii_downcase|contains("red")))' "$work/terminal.json" >/dev/null
  # Chat supplies full caller-owned history using its own image framing.
  http 200 POST /chat/completions "$(jq -nc --rawfile data "$work/encoded.txt" '{model:"ailo-fixture",messages:[{role:"user",content:[{type:"image_url",image_url:{url:("data:image/jpeg;base64,"+$data)}},{type:"text",text:"What is the main color? Answer with one color word."}]}],stream:true,temperature:0,max_tokens:16}')"
else
  jq -nc --rawfile data "$work/encoded.txt" '{type:"input_audio",input_audio:{format:"wav",data:$data}}' > "$work/part.json"
  request=$(jq -nc --slurpfile audio "$work/part.json" '{model:"ailo-fixture",messages:[{role:"user",content:[$audio[0]]}],store:true,stream:true,temperature:0,max_tokens:128}')
  http 200 POST /chat/completions "$request"
  sed -n 's/^data: //p' "$body_file" | sed '/^\[DONE\]$/d' | jq -s '.[0].id' > "$work/chat-id.json"
  id=$(jq -r . "$work/chat-id.json")
fi
chat_text() {
  rg -q '^data: \[DONE\]' "$body_file"
  sed -n 's/^data: //p' "$body_file" | sed '/^\[DONE\]$/d' | jq -rs '[.[]|.choices[]?.delta.content // empty]|join("")' > "$work/text.txt"
  if [[ "$kind" == vision ]]; then
    rg -qi red "$work/text.txt"
  else
    rg -qi country "$work/text.txt"
    ! rg -q '<asr_text>|^language ' "$work/text.txt"
  fi
}
chat_text
if [[ "$kind" == audio ]]; then
  cp "$work/text.txt" "$work/first-transcript.txt"
  serving_stop
  serving_start "$fixture" "$model" "$parent" "$work"
  http 200 GET "/chat/completions/$id/messages?order=asc&limit=10"
  jq -e --rawfile data "$work/encoded.txt" '.data[0].content_parts[0].input_audio.data==$data' "$body_file" >/dev/null
  # ASR is transcription-only: replay the retrieved audio, not a conversational
  # follow-up that this model does not implement.
  replay=$(jq -c '{model:"ailo-fixture",messages:[.data[0]|{role,content:.content_parts}],stream:true,temperature:0,max_tokens:128}' "$body_file")
  http 200 POST /chat/completions "$replay"
  chat_text
  cmp "$work/first-transcript.txt" "$work/text.txt"
  http 400 POST /responses "$(jq -nc --slurpfile audio "$work/part.json" '{input:[{role:"user",content:[$audio[0]]}],stream:true}')"
  check '.error.code=="unsupported"'
fi
# Malformed media must fail without publishing a retained answer. The following
# known-good request proves that failure did not poison the receiver/store.
if [[ "$kind" == vision ]]; then
  http 400 POST /responses '{"input":[{"role":"user","content":[{"type":"input_image","image_url":"data:image/jpeg;base64,bm90LWEtanBlZw=="}]}],"stream":true}'
  http 200 POST /responses "$request"
  terminal
  jq -e 'any(.output[];.type=="message" and any(.content[];.text|ascii_downcase|contains("red")))' "$work/terminal.json" >/dev/null
else
  http 400 POST /chat/completions '{"model":"ailo-fixture","messages":[{"role":"user","content":[{"type":"input_audio","input_audio":{"format":"wav","data":"bm90LWEtd2F2"}}]}],"stream":true}'
  http 200 POST /chat/completions "$request"
  chat_text
fi
[[ ! -s "$work/fixture.stdout" ]]
# Observe an actual text delta before production reaches its terminal event.
control '{"pouch":true,"hold_transport":true}'
mkfifo "$work/release"
exec {release_in}<>"$work/release"
printf '%s' "$request" > "$work/stream-input.json"
http_count=$((http_count+1))
"$(dirname -- "$fixture")/ailo_serving_stream_client" "$port" "$work" "$http_count" continue "$work/stream-input.json"
serving_await "$http_count"
jq -e '.result=="ok" and .completed' "$work/latest.json" >/dev/null
printf 'Real %s HTTP input, live deltas, Pouch restart/replay and malformed-media recovery passed\n' "$kind"
