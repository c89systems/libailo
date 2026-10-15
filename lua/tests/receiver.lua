local ailo=require("ailo")
local pslog=require("pslog")
local path,work=arg[1],arg[2]
local function ok(value,err) assert(value,err and (err.code..": "..err.message)); return value end
local function fails(code,fn) local v,e=fn(); assert(v==nil and e and e.code==code,tostring(e and e.code)); return e end
local function config(extra)
  local c={source={local_path=path},backend="cpu",context_tokens=256,threads=2}
  for k,v in pairs(extra or {}) do c[k]=v end
  return c
end
local function request() return {items={{role="user",content="hello"}},max_tokens=3,temperature=0} end
local tool_template=[[{% if tools %}{{ tools | tojson }}{% endif %}{% for m in messages %}{{ m.role }}:{{ m.content }}{% if m.tool_calls is defined %}{% for c in m.tool_calls %}<call>{"name":{{ c.function.name | tojson }},"arguments":{% if c.function.arguments is string %}{{ c.function.arguments }}{% else %}{{ c.function.arguments | tojson }}{% endif %}}</call>{% endfor %}{% endif %}|{% endfor %}{% if add_generation_prompt %}assistant:{% endif %}]]
do
  local tools=ok(ailo.new(config{chat_template=tool_template,context_tokens=1024}))
  local function tool_request()
    return {items={{content="hello"}},max_tokens=256,temperature=0,
      tools={{name="lookup",parameters='{"const":{"answer":4}}',strict=true}},
      allowed_tools={"lookup"},tool_choice="named",tool_name="lookup",parallel_tool_calls=false,
      logit_bias={{token_id=2,bias=100},{token_id=35,bias=-100},{token_id=13,bias=-100},
        {token_id=12,bias=-100},{token_id=16,bias=-100}}}
  end
  local name,arguments,id,deltas,done="","",nil,0,false
  local original=tool_request()
  local result=ok(tools:generate(original,function(e)
    original.tools=nil;original.allowed_tools=nil;collectgarbage("collect")
    if e.kind=="item_begin" then assert(e.item_kind=="function_call");id=e.call_id;assert(id~=e.item_id) end
    if e.kind=="function_name_delta" then name=name..e.data end
    if e.kind=="function_arguments_delta" then
      assert(not done and e.call_id==id and name=="lookup")
      arguments=arguments..e.data;deltas=deltas+1
    end
    if e.kind=="item_end" then assert(e.call_id==id and e.name==name);done=true end
  end))
  assert(done and deltas>1 and arguments=='{"answer":4}' and result.finish_reason=="tool_calls")
  fails("cancelled",function()return tools:generate(tool_request(),function(e)if e.kind=="function_arguments_delta" then return false end end)end)
  local bad=tool_request();bad.tool_name="missing"
  fails("invalid_argument",function()return tools:generate(bad,function()error("preflight emitted an event")end)end)
  ok(tools:generate(tool_request(),function()end));tools:close()
end
assert(type(ailo.version())=="string")
fails("invalid_argument",function() return ailo.new{} end)
fails("invalid_argument",function() return ailo.new(config{typo=true}) end)
fails("invalid_argument",function() return ailo.new(config{model_id="bad\0id"}) end)
local unknown=pslog.new_json{output=function() error("must not call Lua output") end}
fails("invalid_argument",function() return ailo.new(config{logger=unknown}) end)
local closed=pslog.new_json{output=work.."/closed.log"}; closed:close()
fails("invalid_argument",function() return ailo.new(config{logger=closed}) end)
local logger=pslog.new_json{output=work.."/lua.log",min_level="trace",disable_timestamp=true}
local weak=setmetatable({logger},{__mode="v"})
local client=ok(ailo.new(config{logger=logger,model_id="lua-model"}))
logger=nil; collectgarbage("collect"); assert(weak[1],"logger not retained")
assert(client:capabilities():find('"streaming":true',1,true))
assert(client:devices():find('CPU',1,true))
assert(client:model_card():find('lua-model',1,true))
local count,sequence=0,-1
local run_id,item_id,usage_events
usage_events=0
local info=ok(client:generate(request(),function(event)
  assert(event.sequence>sequence); sequence=event.sequence
  assert(type(event.run_id)=="string" and #event.run_id==36)
  run_id=run_id or event.run_id;assert(event.run_id==run_id)
  if event.kind=="item_begin" then item_id=event.item_id;assert(event.item_kind=="message") end
  if event.kind=="usage" then usage_events=usage_events+1 end
  if event.kind=="text_delta" then
    assert(event.item_id==item_id and event.item_index==0 and event.content_index==0)
    count=count+1; assert(type(event.data)=="string" and #event.data>0)
    if count==1 then
      fails("busy",function() return client:close() end)
      fails("busy",function() return client:capabilities() end)
      fails("busy",function() return client:generate(request(),function()end) end)
    end
  end
end))
assert(count==3 and info.usage.completion_tokens==3 and info.completed_candidates==1)
assert(info.id==run_id and usage_events==1)
local seen=0
fails("cancelled",function() return client:generate(request(),function(event)
  if event.kind=="text_delta" then seen=seen+1; return false end
end) end)
assert(seen==1,"cancel was not applied at first token")
local marker={message="callback marker"}
local err=fails("sink_failed",function() return client:generate(request(),function() error(marker) end) end)
assert(err.cause==marker and err.info)
ok(client:generate(request(),function() end))
fails("invalid_argument",function() return client:generate({items={},tools={}},function()end) end)
fails("invalid_argument",function() return client:generate({items={[1]={content="x"},[3]={content="y"}}},function()end) end)
fails("invalid_argument",function() return client:generate({items={},max_tokens=1.5},function()end) end)
fails("unsupported",function() return client:generate({items={{content={{kind="image",data="image"}}}}},function()end) end)
-- All typed options, multiple candidates, explicit content parts and stops.
local two=ok(client:generate({items={{kind="message",role="system",content="Be concise"},{role="user",content={{kind="text",data="hi"}}}},max_tokens=2,candidates=2,temperature=0.8,top_p=0.95,seed=42,frequency_penalty=0.2,presence_penalty=0.1,stop={"ZZZ"}},function() end))
assert(two.completed_candidates==2)
-- Bounded token probability records arrive before the matching text delta.
local controlled=request()
controlled.logprobs=true;controlled.top_logprobs=3
controlled.logit_bias={{token_id=100,bias=-100},{token_id=101,bias=100}}
local token_count,text_count=0,0
ok(client:generate(controlled,function(e)
  if e.kind=="token" then
    token_count=token_count+1
    assert(e.token.index==token_count-1 and e.token.byte_offset==token_count-1)
    assert(e.token.token_id==101 and e.token.data=="b" and e.token.logprob<=0)
    assert(#e.token.top_logprobs==3 and e.token.top_logprobs[1].token_id==101)
    assert(not e.token.end_of_generation)
  elseif e.kind=="text_delta" then
    text_count=text_count+1;assert(token_count==text_count and e.data=="b")
  end
end))
assert(token_count==3 and text_count==3)
local token_cancel=fails("cancelled",function()return client:generate(controlled,function(e)
  if e.kind=="token" then return false end
end)end)
assert(token_cancel.info.usage.completion_tokens==1)
local quota=request();quota.max_output_bytes=1
local quota_error=fails("out_of_range",function()return client:generate(quota,function()end)end)
assert(quota_error.info.completed_candidates==0)
for _,fields in ipairs{
  {top_logprobs=1}, {logprobs=true,top_logprobs=21},
  {logit_bias={{token_id=100,bias=101}}},
  {logit_bias={{token_id=100,bias=0},{token_id=100,bias=1}}},
  {logit_bias={{token_id=1000000,bias=0}}}
} do
  local bad=request();for k,v in pairs(fields)do bad[k]=v end
  fails("invalid_argument",function()return client:generate(bad,function()error("preflight emitted an event")end)end)
end
ok(client:generate(request(),function()end))
local constrained=request()
constrained.max_tokens=32;constrained.output_format="json_schema"
constrained.json_schema='{"const":{"answer":4}}'
constrained.logit_bias={{token_id=2,bias=100}}
local fragment_count,constrained_text=0,""
local constrained_info=ok(client:generate(constrained,function(e)
  if e.kind=="text_delta" then
    fragment_count=fragment_count+1
    assert(e.usage.completion_tokens==fragment_count)
    constrained_text=constrained_text..e.data
  end
end))
assert(constrained_text=='{"answer":4}' and fragment_count==#constrained_text)
assert(constrained_info.finish_reason=="stop")
local bad_schema=request();bad_schema.output_format="json_schema"
bad_schema.json_schema='{"type":"number","minimum":1}'
fails("unsupported",function()return client:generate(bad_schema,function()error("unsupported schema emitted events")end)end)
ok(client:generate(request(),function()end))
assert(client:close() and client:close())
local history_template=[[{% for m in messages %}{{ m.role }}:{{ m.content }}{% if m.tool_calls is defined %}{% for c in m.tool_calls %}{{ c.id }}={{ c.function.name }}:{{ c.function.arguments | tojson }}{% endfor %}{% endif %}{% if m.tool_call_id is defined %}#{{ m.tool_call_id }}{% endif %};{% endfor %}]]
local history=ok(ailo.new(config{chat_template=history_template}))
local history_request={items={
  {role="user",content="question"},
  {kind="function_call",id="function-item",call_id="call-one",name="lookup",arguments='{ "x" : 1 }'},
  {kind="function_result",call_id="call-one",content="result"}
},max_tokens=1,temperature=0}
local history_info=ok(history:generate(history_request,function(e) if e.kind=="text_delta" then assert(e.data=="a") end end))
assert(history_info.completed_candidates==1 and history_info.id~=run_id)
history_request.items[3].call_id="missing"
fails("invalid_argument",function()return history:generate(history_request,function()error("orphan emitted an event")end)end)
history:close()
fails("invalid_argument",function() return client:model_card() end)
collectgarbage("collect"); assert(not weak[1],"logger leaked after close")
local file=assert(io.open(work.."/lua.log","rb")); local logs=file:read("a");file:close()
assert(logs:find('"model_id":"lua-model"',1,true) and logs:find('"lvl":"trace"',1,true))
assert(logs:find("Generation complete",1,true) and logs:find("Destroying inference receiver",1,true))
print("Lua installed shared ABI, token streaming, pslog ownership, callbacks and validation: passed")

local preflight=ok(ailo.new(config{lazy=true}))
local preflight_error=fails("invalid_argument",function()return preflight:generate({items={}},function()end)end)
assert(preflight_error.info.actual_backend=="auto","backend must remain unselected before inference")
preflight:close()
