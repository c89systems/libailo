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
do
  local receiver=ok(ailo.new(config()))
  local json='{"model":"local-model","messages":[{"role":"user","content":"Hi"}],"store":false,"stream":true,"max_completion_tokens":8,"temperature":0,"logprobs":true,"top_logprobs":2,"logit_bias":{"100":100},"stream_options":{"include_usage":true,"include_obfuscation":false}}'
  local parts={}
  local run=ok(receiver:chat_completions(json,function(bytes)parts[#parts+1]=bytes;collectgarbage("collect")end))
  local result=table.concat(parts)
  assert(run.http_status==200 and run.content_type=="text/event-stream" and run.started and not run.stored and run.usage.completion_tokens==8 and result:find("data: %[DONE%]"))
  local cancelled=fails("cancelled",function()return receiver:chat_completions(json,function()return false end)end)
  assert(cancelled.info.started and cancelled.info.usage.completion_tokens==0)
  local marker={sink=true}
  local failed=fails("sink_failed",function()return receiver:chat_completions(json,function()error(marker)end)end)
  assert(failed.cause==marker and failed.info.started)
  fails("invalid_json",function()return receiver:chat_completions("{",function()end)end)
  receiver:close()
end
do
  local receiver=ok(ailo.new(config{chat_template="{% for m in messages %}{{m.role}}:{{m.content}}|{% endfor %}"}))
  local input=request();input.max_tokens=32;input.logprobs=true;input.logit_bias={{token_id=100,bias=100}};input.stop={"aaaZ"}
  local raw,text,watermark,progress="","",0,0
  local run=ok(receiver:generate(input,function(e)
    if e.kind=="token" then assert(e.token.byte_offset==#raw);raw=raw..e.token.data
    elseif e.kind=="text_delta" then
      assert(e.raw_offset>=watermark and raw:sub(e.raw_offset+1,e.raw_offset+#e.data)==e.data);text=text..e.data
    elseif e.kind=="output_progress" then
      assert(e.raw_offset>=watermark and e.raw_offset<=#raw and #raw-e.raw_offset<=3)
      watermark=e.raw_offset;progress=progress+1
    end
  end))
  assert(text==string.rep("a",32) and raw==text and watermark==#raw and progress>=32 and run.usage.completion_tokens==32)
  local cancelled=fails("cancelled",function()return receiver:generate(input,function(e)if e.kind=="output_progress" then return false end end)end)
  assert(cancelled.info.usage.completion_tokens==1)
  receiver:close()
end
do
  local receiver=ok(ailo.new(config{source={local_path=path.."-auto-compaction"},context_tokens=4096,
    chat_template="{% for m in messages %}{{m.role}}:{{m.content}}|{% endfor %}"}))
  local history=ok(receiver:new_continuation())
  assert(history:append{
    {role="system",id="system",pinned=true,content="Preserve instructions."},
    {id="goal",content="Finish ticket T-1. "..string.rep("x",1000)},{role="assistant",content="old answer"},
    {content="older question"},{role="assistant",content="older answer"},
    {content="recent question"},{role="assistant",content="recent answer"},
    {content="newest question"},{role="assistant",content="newest answer"},{content="current question"}})
  local input={continuation=history,max_tokens=32,temperature=0,logit_bias={{token_id=100,bias=100}}}
  local baseline=ok(receiver:generate(input,function()end))
  input.auto_compaction={target_tokens=800,output_reserve_tokens=64,summary_context_tokens=8192,
    trigger_tokens=baseline.usage.prompt_tokens+3,max_passes=1}
  local failed=fails("cancelled",function()return receiver:generate(input,function(e)if e.kind=="compaction" then return false end end)end)
  assert(failed.info.usage.completion_tokens==3 and failed.info.usage.compaction_passes==1 and ok(history:info()).item_count==10)
  local marker={automatic=true}
  local err=fails("sink_failed",function()return receiver:generate(input,function(e)if e.kind=="compaction" then error(marker)end end)end)
  assert(err.cause==marker and ok(history:info()).item_count==10)
  local controls,deltas=0,0;local id
  local run=ok(receiver:generate(input,function(e)
    if e.kind=="text_delta" then
      deltas=deltas+1;id=id or e.item_id;assert(id==e.item_id)
      if deltas==1 then assert(e.usage.completion_tokens==1) end
    elseif e.kind=="compaction" then
      controls=controls+1;assert(e.item_id==id and e.usage.completion_tokens==3 and #e.compacted_items==7)
      assert(e.compaction.changed and e.compaction.after_tokens<e.compaction.before_tokens and e.compaction.replayed_tokens>0)
      assert(e.compacted_items[2].kind=="compaction" and e.compacted_items[2].encrypted_content)
      fails("busy",function()return history:close()end)
    end
  end))
  assert(controls==1 and deltas==32 and run.usage.compaction_passes==1 and run.usage.context_replay_tokens>0 and ok(history:info()).item_count==7)
  fails("invalid_argument",function()return receiver:generate({items={{content="x"}},auto_compaction={}},function()end)end)
  fails("invalid_argument",function()return receiver:generate({continuation=history,auto_compaction={max_passes=0}},function()end)end)
  fails("invalid_argument",function()return receiver:compact(input)end)
  history:close();receiver:close()
end
do
  local receiver=ok(ailo.new(config{source={local_path=path.."-compaction"},context_tokens=4096,
    chat_template="{% for m in messages %}{{m.role}}:{{m.content}}|{% endfor %}"}))
  local history=ok(receiver:new_continuation())
  assert(history:append{
    {id="system",role="system",content="Original instructions.",pinned=true},
    {id="old-goal",content="Finish ticket T-1. "..string.rep("Unneeded detail. ",100)},
    {role="assistant",content=string.rep("a",500)},
    {content="recent question"},{role="assistant",content="recent answer"},
    {content="newest question"},{role="assistant",content="newest answer"}})
  local input={continuation=history,max_tokens=3,temperature=0,logit_bias={{token_id=100,bias=100}}}
  local policy={target_tokens=1000,summary_context_tokens=4096}
  local window,details=receiver:compact(input,policy);assert(window,details and details.message)
  assert(details.changed and details.after_tokens<1000 and details.before_tokens>details.after_tokens and
    details.removed_items==2 and details.retained_items==5 and details.summary_passes==1 and details.summary_completion_tokens==2)
  assert(ok(history:info()).item_count==7)
  local seen=0
  assert(window:visit(function(item)
    if item.kind=="compaction" then seen=seen+1;assert(item.id and item.encrypted_content and #item.content==0) end
    if item.role=="system" then assert(item.pinned and item.content[1].data=="Original instructions.") end
  end));assert(seen==1)
  fails("model_failed",function()return receiver:compact(input,{target_tokens=1000,summary_context_tokens=4096,summary_tokens=1})end)
  fails("context_limit",function()return receiver:compact(input,{target_tokens=5})end)
  fails("invalid_argument",function()return receiver:compact(input,{summary_tokens=0})end)
  fails("invalid_argument",function()return receiver:compact(input,{unknown=1})end)
  local replay={continuation=window,max_tokens=3,temperature=0,logit_bias={{token_id=100,bias=100}}}
  local copy,unchanged=receiver:compact(replay,policy);assert(copy,unchanged and unchanged.message);assert(not unchanged.changed and unchanged.summary_passes==0)
  local count=0;local run=ok(receiver:generate(replay,function(e)if e.kind=="text_delta" then count=count+1 end end))
  assert(count==3 and run.usage.prompt_tokens==details.after_tokens and ok(history:info()).item_count==7)
  copy:close();window:close();history:close();receiver:close()
end
do
  local fixture=assert(loadfile(path.."-compaction.lua"))()
  local owner=ok(ailo.new_compactor{namespace_id=fixture.namespace_id,key_id=fixture.key_id,key=fixture.key})
  local receiver=ok(ailo.new(config{compactor=owner,chat_template=fixture.template,context_tokens=1024}))
  local capabilities=ok(receiver:capabilities());assert(capabilities:find('"compaction_replay":true',1,true) and capabilities:find('"compaction":true',1,true) and capabilities:find('"automatic_compaction":true',1,true))
  local input={items={
    {id="instruction",role="system",pinned=true,content="Keep original instructions."},
    {kind="compaction",id=fixture.id,encrypted_content=fixture.encrypted_content},
    {id="recent",content="continue"}},
    max_tokens=3,temperature=0,logit_bias={{token_id=100,bias=100}}}
  local history=ok(receiver:new_continuation());assert(history:append(input.items))
  local fork=ok(history:clone());local item
  assert(fork:visit(function(v)if v.kind=="compaction" then item=v end end))
  assert(item.id==fixture.id and item.encrypted_content==fixture.encrypted_content and #item.content==0)
  item.encrypted_content="caller copy"
  input.items[2].encrypted_content="tampered"
  local seen=0
  fails("invalid_argument",function()return receiver:generate(input,function()seen=seen+1 end)end);assert(seen==0)
  input.items=nil;input.continuation=history
  local cancelled=fails("cancelled",function()return receiver:generate(input,function(e)
    if e.kind=="text_delta" then seen=seen+1;return false end
  end)end)
  assert(seen==1 and cancelled.info.usage.completion_tokens==1 and ok(history:info()).item_count==3)
  assert(fails("sink_failed",function()return receiver:generate(input,function(e)if e.kind=="text_delta" then error("replay sink")end end)end).info.usage.completion_tokens==1)
  local foreign=ok(ailo.new(config{chat_template=fixture.template,context_tokens=1024}))
  fails("invalid_argument",function()return foreign:generate(input,function()error("unexpected event")end)end);foreign:close()
  owner:close();receiver:close()
  owner=ok(ailo.new_compactor{namespace_id=fixture.namespace_id,key_id=fixture.key_id,key=fixture.key})
  receiver=ok(ailo.new(config{compactor=owner,chat_template=fixture.template,context_tokens=1024}));owner:close()
  seen=0;ok(receiver:generate(input,function(e)if e.kind=="text_delta" then seen=seen+1 end end));assert(seen==3)
  assert(ok(fork:info()).item_count==3);history:close();fork:close();receiver:close()
  local ordinary=request();ordinary.items[1].encrypted_content="hidden"
  receiver=ok(ailo.new(config{}));fails("invalid_argument",function()return receiver:generate(ordinary,function()end)end);receiver:close()
end
do
  local key="\0"..string.rep("k",31)
  local owner=ok(ailo.new_compactor{namespace_id="lua-tenant",key_id="current",key=key,
    old_keys={{key_id="previous",key=string.rep("p",32)}}})
  local info=ok(owner:info());assert(not info.ephemeral and info.decryption_keys==2 and info.ttl_seconds==0)
  local receiver=ok(ailo.new(config{compactor=owner}))
  assert(owner:close() and owner:close());fails("invalid_argument",function()return owner:info()end)
  fails("invalid_argument",function()return ailo.new(config{compactor=owner})end)
  ok(receiver:generate(request(),function()end));receiver:close()
  owner=ok(ailo.new_compactor());assert(ok(owner:info()).ephemeral);owner:close()
  fails("invalid_argument",function()return ailo.new_compactor{namespace_id="tenant",key_id="x",key="short"}end)
  fails("invalid_argument",function()return ailo.new_compactor{namespace_id="tenant",key_id="x",key=key,
    old_keys={{key_id="x",key=key}}}end)
end
do
  local template=[[{% for m in messages %}{{ m.role }}:{% if m.reasoning_content is defined %}<think>{{ m.reasoning_content }}</think>{% endif %}{{ m.content }}|{% endfor %}{% if add_generation_prompt %}assistant:{% if enable_thinking %}<think>{% endif %}{% endif %}]]
  local receiver=ok(ailo.new(config{source={local_path=path.."-reasoning"},chat_template=template,context_tokens=512}))
  local input={items={{content="hello"}},max_tokens=32,temperature=0,reasoning="enabled",
    output_format="json_schema",json_schema='{"const":{}}',logit_bias={{token_id=260,bias=100},{token_id=2,bias=80}}}
  local reason,text="",""
  local run=ok(receiver:generate(input,function(e)
    if e.kind=="reasoning_delta" then assert(e.item_kind=="reasoning");reason=reason..e.data end
    if e.kind=="text_delta" then assert(e.item_kind=="message");text=text..e.data end
  end))
  assert(reason=="r" and text=="{}" and run.usage.reasoning_tokens==1)
  assert(fails("sink_failed",function()return receiver:generate(input,function(e)if e.kind=="reasoning_delta" then error("reasoning callback")end end)end).info.usage.completion_tokens==1)
  input.reasoning="disabled";run=ok(receiver:generate(input,function()end));assert(run.usage.reasoning_tokens==0)
  input.reasoning="enabled";input.max_reasoning_tokens=1;input.logit_bias={{token_id=100,bias=100}}
  run=ok(receiver:generate(input,function()end));assert(run.finish_reason=="length" and run.usage.reasoning_tokens==1)
  receiver:close()
end
do
  local client=ok(ailo.new(config{disable_prompt_cache=true}))
  fails("invalid_argument",function()return client:new_continuation(false)end)
  local history=ok(client:new_continuation())
  local original={{id="instruction",role="system",content="remember",pinned=true}}
  assert(history:append(original));original[1].content="changed"
  local fork=ok(history:clone());assert(ok(fork:info()).item_count==1)
  local input=request();input.items[1].id="question";input.continuation=history
  fails("invalid_argument",function()return client:generate({items=false,continuation=history},function()end)end)
  fails("cancelled",function()return client:generate(input,function(e)if e.kind=="run_end" then return false end end)end)
  assert(ok(history:info()).item_count==1)
  local count=0
  ok(client:generate(input,function(e)
    if e.kind=="text_delta" then count=count+1 end
    fails("busy",function()return history:append{}end)
    fails("busy",function()return history:clone()end)
    fails("busy",function()return history:close()end)
  end))
  assert(count>0 and ok(history:info()).item_count==2 and ok(fork:info()).item_count==1)
  local seen=0
  assert(history:visit(function(item)
    seen=seen+1;if seen==1 then assert(item.pinned and item.content[1].data=="remember") end
    fails("busy",function()return history:close()end)
    item.content[1].data="caller copy"
  end));assert(seen==2)
  local marker={failure=true}
  assert(fails("sink_failed",function()return history:visit(function()error(marker)end)end).cause==marker)
  fails("cancelled",function()return history:visit(function()return false end)end)
  fails("invalid_argument",function()return history:append{{id="question",content="duplicate"}}end)
  client:close();client=ok(ailo.new(config{disable_prompt_cache=true}))
  ok(client:generate({continuation=history,max_tokens=3,temperature=0},function()end))
  assert(ok(history:info()).revision==2)
  assert(fork:close() and history:close() and history:close())
  fails("invalid_argument",function()return history:info()end)
  client:close()
end
do
  local options={namespace_id="lua-snapshots",directory=work.."/snapshots",max_disk_bytes=1024*1024,
    disk_ttl_seconds=60,key_id="test-key",key=string.rep("k",32)}
  local cache=ok(ailo.new_cache(options));local first=ok(ailo.new(config{cache=cache}))
  ok(first:generate(request(),function()end));assert(first:snapshot_export("lua-slot"))
  local stats=ok(cache:stats());assert(stats.snapshot_writes>=2 and stats.disk_entries>=2)
  first:close();cache:close()
  cache=ok(ailo.new_cache(options));local second=ok(ailo.new(config{cache=cache}))
  assert(second:snapshot_import("lua-slot"))
  assert(ok(second:generate(request(),function()end)).usage.cached_tokens>0)
  fails("invalid_argument",function()return second:snapshot_import("lua-slot","another-partition")end)
  local invalid={};for k,v in pairs(options)do invalid[k]=v end;invalid.key="short"
  fails("invalid_argument",function()return ailo.new_cache(invalid)end)
  assert(second:cache_clear());second:close();cache:close()
end
do
  fails("invalid_argument",function()return ailo.new_cache{}end)
  local cache=ok(ailo.new_cache{namespace_id="lua-tenant",max_entries=2})
  local first=ok(ailo.new(config{cache=cache}))
  local second=ok(ailo.new(config{cache=cache}))
  local cold=ok(first:generate(request(),function()end));assert(cold.usage.cached_tokens==0)
  local warm=ok(second:generate(request(),function()end));assert(warm.usage.cached_tokens==warm.usage.prompt_tokens-1)
  local stats=ok(cache:stats());assert(stats.hits==1 and stats.misses==1 and stats.entries==1)
  local partition=request();partition.prompt_cache_key="another-application"
  assert(ok(first:generate(partition,function()end)).usage.cached_tokens==0)
  assert(cache:prune());assert(cache:close() and cache:close())
  fails("invalid_argument",function()return cache:stats()end)
  fails("invalid_argument",function()return ailo.new(config{cache=cache})end)
  assert(ok(first:generate(request(),function()end)).usage.cached_tokens>0)
  assert(first:cache_clear() and ok(second:cache_stats()).entries==0)
  first:close();second:close()
  local disabled=ok(ailo.new(config{disable_prompt_cache=true}))
  ok(disabled:generate(request(),function()end));assert(ok(disabled:generate(request(),function()end)).usage.cached_tokens==0)
  assert(ok(disabled:cache_stats()).entries==0);disabled:close()
end
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
