#!/usr/bin/env bash
# The ordinary transport fixture, using the production encrypted repository.
set -euo pipefail
[[ $# -eq 3 ]] || exit 2
source "$(dirname -- "${BASH_SOURCE[0]}")/../scripts/serving_test_lib.sh"
serving_start "$1" "$2" "$3"
control '{"pouch":true}'
http 200 POST /responses '{"input":"AILO_REMEMBER:violet","stream":true}'
terminal
parent=$(jq -r .id "$work/terminal.json")
http 200 GET "/responses/$parent"
check '.status=="completed"'
http 200 POST /responses "$(jq -nc --arg id "$parent" '{previous_response_id:$id,input:"AILO_EXPECT:violet",stream:true}')"
terminal
jq -e '.output[0].content[0].text=="AILO_RECALLED:violet"' "$work/terminal.json" >/dev/null
http 200 GET "/responses/$parent/input_items?limit=1&order=asc"
check '.data[0].role=="user"'
http 200 POST /chat/completions '{"model":"ailo-fixture","messages":[{"role":"user","content":"hello"}],"store":true,"metadata":{"owner":"initial"}}'
chat=$(jq -r .id "$body_file")
http 200 GET "/chat/completions/$chat"
check '.object=="chat.completion"'
http 200 POST "/chat/completions/$chat" '{"metadata":{"owner":"updated"}}'
check '.metadata.owner=="updated"'
http 200 GET '/chat/completions?metadata[owner]=updated'
check '.data|length==1'
http 200 GET "/chat/completions/$chat/messages?limit=1"
check '.data|length==1'
http 200 DELETE "/chat/completions/$chat"
http 404 GET "/chat/completions/$chat"
http 200 POST /conversations '{}'
conversation=$(jq -r .id "$body_file")
http 200 POST /responses "$(jq -nc --arg id "$conversation" '{conversation:$id,input:"AILO_REMEMBER:amber",stream:true}')"
terminal
response=$(jq -r .id "$work/terminal.json")
http 200 GET "/responses/$response"
check '.status=="completed"'
http 200 POST /responses "$(jq -nc --arg id "$conversation" '{conversation:$id,input:"AILO_EXPECT:amber",store:false,stream:true}')"
terminal
jq -e '.output[0].content[0].text=="AILO_RECALLED:amber"' "$work/terminal.json" >/dev/null
http 200 GET "/conversations/$conversation/items?limit=1&order=asc"
check '.data|length==1'
item=$(jq -r '.data[0].id' "$body_file")
http 200 GET "/conversations/$conversation/items/$item"
check '.id=="'"$item"'"'
http 200 DELETE "/conversations/$conversation"
http 200 GET "/conversations/$conversation/items?limit=1"
check '.data|length==1'
http 200 DELETE "/responses/$parent"
http 404 GET "/responses/$parent"
[[ -s "$work/pouch-config/pouch.key" ]]
[[ $(stat -c %a "$work/pouch-config/pouch.key") == 600 ]]
printf 'Production encrypted Pouch HTTP continuation, conversation archive and deletion: passed\n'
