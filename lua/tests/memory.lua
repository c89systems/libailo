-- Ownership/failure gate; feature semantics run in receiver.lua. One CPU thread
-- avoids instrumenting unrelated worker scheduling in every Lua lifetime case.
local ailo,pslog=require("ailo"),require("pslog")
local path,work=arg[1],arg[2]
local function ok(value,err) assert(value,err and err.message);return value end
local function fails(code,fn) local v,e=fn();assert(v==nil and e and e.code==code);return e end
local function config(extra)
  local c={source={local_path=path},backend="cpu",context_tokens=128,threads=1}
  for k,v in pairs(extra or {})do c[k]=v end
  return c
end
local function request()
  return {items={{role="user",content="x"}},max_tokens=2,temperature=0,
    output_format="json_schema",json_schema='{"const":{}}',
    logprobs=true,top_logprobs=2,logit_bias={{token_id=2,bias=100}}}
end
local marker={message="callback marker"}
do
  local receiver=ok(ailo.new(config{source={local_path=path.."-compaction"},context_tokens=4096,
    chat_template="{% for m in messages %}{{m.role}}:{{m.content}}|{% endfor %}"}))
  local input={items={
    {role="system",content="Preserve instructions.",pinned=true},
    {id="goal",content="Finish ticket T-1. "..string.rep("detail ",200)},
    {role="assistant",content=string.rep("a",300)},
    {content="recent"},{role="assistant",content="answer"},
    {content="newest"},{role="assistant",content="answer"}}}
  local policy={target_tokens=800,summary_context_tokens=4096}
  local window,info=receiver:compact(input,policy);ok(window,info);assert(info.changed)
  fails("model_failed",function()return receiver:compact(input,{target_tokens=800,summary_context_tokens=4096,summary_tokens=1})end)
  receiver:close();input=nil;collectgarbage("collect")
  assert(window:visit(function(item)assert(item.kind);collectgarbage("collect")end));window:close()
end
do
  local fixture=assert(loadfile(path.."-compaction.lua"))()
  local owner=ok(ailo.new_compactor{namespace_id=fixture.namespace_id,key_id=fixture.key_id,key=fixture.key})
  local receiver=ok(ailo.new(config{compactor=owner,chat_template=fixture.template,context_tokens=1024}))
  local history=ok(receiver:new_continuation())
  assert(history:append{
    {id="instruction",role="system",pinned=true,content="Keep original instructions."},
    {kind="compaction",id=fixture.id,encrypted_content=fixture.encrypted_content},
    {id="recent",content="continue"}})
  owner:close();owner=nil;collectgarbage("collect")
  local input={continuation=history,max_tokens=2,temperature=0,logit_bias={{token_id=100,bias=100}}}
  fails("sink_failed",function()return receiver:generate(input,function(e)if e.kind=="text_delta" then error(marker)end end)end)
  ok(receiver:generate(input,function()collectgarbage("collect")end))
  local fork=ok(history:clone());receiver:close();history:close()
  assert(fork:visit(function(item)if item.kind=="compaction" then assert(item.encrypted_content==fixture.encrypted_content)end;collectgarbage("collect")end))
  fork:close()
end
do
  local owner=ok(ailo.new_compactor{namespace_id="memory-tenant",key_id="one",key=string.rep("k",32),
    old_keys={{key_id="old",key=string.rep("o",32)}}})
  local receiver=ok(ailo.new(config{compactor=owner}));owner:close();owner=nil;collectgarbage("collect")
  ok(receiver:generate(request(),function()collectgarbage("collect")end));receiver:close()
  local invalid,err=ailo.new_compactor{namespace_id="memory-tenant",key_id="one",key=string.rep("k",32),
    old_keys={{key_id="one",key=string.rep("o",32)}}}
  assert(not invalid and err.code=="invalid_argument")
end
do
  local template=[[{% for m in messages %}{{ m.role }}:{% if m.reasoning_content is defined %}<think>{{ m.reasoning_content }}</think>{% endif %}{{ m.content }}|{% endfor %}{% if add_generation_prompt %}assistant:{% if enable_thinking %}<think>{% endif %}{% endif %}]]
  local receiver=ok(ailo.new(config{source={local_path=path.."-reasoning"},chat_template=template,context_tokens=512}))
  local input={items={{content="x"}},max_tokens=16,temperature=0,reasoning="enabled",
    output_format="json_schema",json_schema='{"const":{}}',logit_bias={{token_id=260,bias=100},{token_id=2,bias=80}}}
  fails("sink_failed",function()return receiver:generate(input,function(e)if e.kind=="reasoning_delta" then error(marker)end end)end)
  ok(receiver:generate(input,function()collectgarbage("collect")end));receiver:close()
end
do
  local receiver=ok(ailo.new(config{disable_prompt_cache=true}))
  local history=ok(receiver:new_continuation())
  assert(history:append{{content="x",pinned=true}})
  local fork=ok(history:clone())
  local input=request();input.continuation=history
  fails("sink_failed",function()return receiver:generate(input,function()error(marker)end)end)
  ok(receiver:generate(input,function()collectgarbage("collect")end))
  assert(ok(history:info()).item_count==2 and ok(fork:info()).item_count==1)
  fails("sink_failed",function()return history:visit(function()error(marker)end)end)
  assert(history:visit(function(item)item.content=nil;collectgarbage("collect")end))
  receiver:close();history:close()
  assert(fork:visit(function()end));fork:close()
end
local logger=pslog.new_json{output=work.."/memory.log",min_level="trace"}
local weak=setmetatable({logger},{__mode="v"})
local client=ok(ailo.new(config{logger=logger}))
logger=nil;collectgarbage("collect");assert(weak[1])
ok(client:model_card());ok(client:capabilities());ok(client:devices())
fails("cancelled",function()return client:generate(request(),function(e)if e.kind=="token" then return false end end)end)
local callback=fails("sink_failed",function()return client:generate(request(),function()error(marker)end)end)
assert(callback.cause==marker and callback.info)
local thread=coroutine.create(function()
  fails("sink_failed",function()return client:generate(request(),function()coroutine.yield()end)end)
end)
assert(coroutine.resume(thread));assert(coroutine.status(thread)=="dead")
-- Fail allocation in a callback while the native grammar/parser/sampler live.
local toggle=_ailo_fail_allocations
local succeeded=pcall(function()
  client:generate(request(),function(e)if e.kind=="text_delta" then toggle(true)end end)
end)
toggle(false);assert(not succeeded)
local mutated=request()
ok(client:generate(mutated,function()mutated.items=nil;mutated.logit_bias=nil;collectgarbage("collect")end))
assert(client:close() and client:close())
collectgarbage("collect");assert(not weak[1])
do
  local options={namespace_id="snapshot-memory",directory=work.."/snapshots",max_disk_bytes=1024*1024,
    disk_ttl_seconds=60,key_id="memory-key",key=string.rep("s",32)}
  local cache=ok(ailo.new_cache(options));local snapshot=ok(ailo.new(config{cache=cache}))
  ok(snapshot:generate(request(),function()end));assert(snapshot:snapshot_export("memory-slot"))
  snapshot:close();cache:close()
  cache=ok(ailo.new_cache(options));snapshot=ok(ailo.new(config{cache=cache}))
  assert(snapshot:snapshot_import("memory-slot"));ok(snapshot:generate(request(),function()end))
  assert(snapshot:cache_clear());snapshot:close();cache:close()
end
do
  local cache=ok(ailo.new_cache{namespace_id="memory-tenant"})
  local weak_cache=setmetatable({cache},{__mode="v"})
  local cached=ok(ailo.new(config{cache=cache}))
  ok(cached:generate(request(),function()end))
  cache=nil;collectgarbage("collect");assert(not weak_cache[1])
  assert(ok(cached:generate(request(),function()end)).usage.cached_tokens>0)
  local stats=ok(cached:cache_stats());assert(stats.entries>0)
  assert(cached:cache_clear());cached:close()
end
-- The new tool/allowed-name arrays and protected argument callback share the
-- same work userdata ownership, including a failure after native generation.
do
  local template=[[{% if tools %}{{ tools | tojson }}{% endif %}{% for m in messages %}{{ m.role }}:{{ m.content }}{% if m.tool_calls is defined %}{% for c in m.tool_calls %}<call>{"name":{{ c.function.name | tojson }},"arguments":{% if c.function.arguments is string %}{{ c.function.arguments }}{% else %}{{ c.function.arguments | tojson }}{% endif %}}</call>{% endfor %}{% endif %}|{% endfor %}{% if add_generation_prompt %}assistant:{% endif %}]]
  local tool=ok(ailo.new(config{chat_template=template,context_tokens=1024}))
  local input={items={{content="hello"}},max_tokens=128,temperature=0,
    tools={{name="lookup",parameters='{"const":{}}',strict=true}},allowed_tools={"lookup"},
    tool_choice="required",parallel_tool_calls=false,
    logit_bias={{token_id=2,bias=100},{token_id=35,bias=-100},{token_id=13,bias=-100},{token_id=12,bias=-100},{token_id=16,bias=-100}}}
  local failure=fails("sink_failed",function()return tool:generate(input,function(e)if e.kind=="function_arguments_delta" then error(marker)end end)end)
  assert(failure.cause==marker)
  ok(tool:generate(input,function()collectgarbage("collect")end));tool:close()
end

-- The retained logger belongs to the main registry, not a dead coroutine.
local lazy
local co=coroutine.create(function()
  local log=pslog.new_json{output=work.."/coroutine-memory.log"}
  lazy=ok(ailo.new(config{logger=log,lazy=true,source={local_path=path,progress=function(p)assert(p.shard==1);return true end}}))
end)
assert(coroutine.resume(co));co=nil;collectgarbage("collect")
ok(lazy:model_card());ok(lazy:generate(request(),function()end));lazy:close()
for _,cancel in ipairs{false,true}do
  local progress=ok(ailo.new(config{lazy=true,source={local_path=path,progress=function()if cancel then return false end;error(marker)end}}))
  local err=fails(cancel and "cancelled" or "sink_failed",function()return progress:model_card()end)
  if not cancel then assert(err.cause==marker)end
  progress:close()
end
-- A closure capturing its receiver must not become an uncollectable cycle.
local cycleweak=setmetatable({},{__mode="v"})
do
  local box={}
  box.client=ok(ailo.new(config{lazy=true,source={local_path=path,progress=function()return box.client~=nil end}}))
  cycleweak[1]=box.client._native
end
collectgarbage("collect");collectgarbage("collect");assert(not cycleweak[1])
-- Metadata JSON remains owned when Lua cannot allocate the returned string.
local metadata=ok(ailo.new(config{lazy=true,source={local_path=path,progress=function(p)if p.total>0 then toggle(true)end end}}))
local metadata_success=pcall(function()metadata:model_card()end)
toggle(false);assert(not metadata_success)
collectgarbage("collect");ok(metadata:model_card());metadata:close()
-- Wrapper allocation can fail after the native constructor succeeds.
local construction_log=pslog.new_json{output=work.."/construction-memory.log"}
local construction_weak=setmetatable({construction_log},{__mode="v"})
local construction_success=pcall(function()
  ailo.new(config{logger=construction_log,source={local_path=path,progress=function(p)if p.total>0 then toggle(true)end end}})
end)
toggle(false);assert(not construction_success)
construction_log=nil;collectgarbage("collect");collectgarbage("collect");assert(not construction_weak[1])
print("Lua ownership: logger/coroutine/GC, callback failure, cancellation, mutation and OOM passed")
