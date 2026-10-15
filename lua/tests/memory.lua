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
