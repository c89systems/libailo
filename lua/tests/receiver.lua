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
local info=ok(client:generate(request(),function(event)
  assert(event.sequence>sequence); sequence=event.sequence
  if event.kind=="text_delta" then
    count=count+1; assert(type(event.data)=="string" and #event.data>0)
    if count==1 then
      fails("busy",function() return client:close() end)
      fails("busy",function() return client:capabilities() end)
      fails("busy",function() return client:generate(request(),function()end) end)
    end
  end
end))
assert(count==3 and info.usage.completion_tokens==3 and info.completed_candidates==1)
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
fails("invalid_argument",function() return client:model_card() end)
collectgarbage("collect"); assert(not weak[1],"logger leaked after close")
local file=assert(io.open(work.."/lua.log","rb")); local logs=file:read("a");file:close()
assert(logs:find('"model_id":"lua-model"',1,true) and logs:find('"lvl":"trace"',1,true))
assert(logs:find("Generation complete",1,true) and logs:find("Destroying inference receiver",1,true))
-- Lazy acquisition callbacks use the active coroutine only during each call.
local lazy
local co=coroutine.create(function()
  local l=pslog.new_json{output=work.."/coroutine.log"}
  lazy=ok(ailo.new(config{logger=l,lazy=true,source={local_path=path,progress=function(p) assert(p.shard==1); return true end}}))
end)
assert(coroutine.resume(co)); co=nil;collectgarbage("collect")
ok(lazy:model_card());ok(lazy:generate(request(),function()end));lazy:close()
local progress=ok(ailo.new(config{lazy=true,source={local_path=path,progress=function()error(marker)end}}))
local progress_error=fails("sink_failed",function()return progress:model_card()end)
assert(progress_error.cause==marker);progress:close()
local cancelled=ok(ailo.new(config{lazy=true,source={local_path=path,progress=function()return false end}}))
fails("cancelled",function()return cancelled:model_card()end);cancelled:close()
-- Progress captures the wrapper: ordinary Lua GC must collect the cycle.
local cycle
cycle=ok(ailo.new(config{lazy=true,source={local_path=path,progress=function()return cycle~=nil end}}))
local refs=setmetatable({cycle._native},{__mode="v"});cycle=nil;collectgarbage("collect");collectgarbage("collect");assert(not refs[1])
-- Yielding from native callbacks is rejected without escaping native frames.
local y=ok(ailo.new(config()))
local thread=coroutine.create(function()
  fails("sink_failed",function()return y:generate(request(),function()coroutine.yield()end)end)
end)
assert(coroutine.resume(thread));assert(coroutine.status(thread)=="dead");y:close()
collectgarbage("collect")
print("Lua installed shared ABI, token streaming, pslog ownership, callbacks and validation: passed")
-- Refuse allocation while native generation is active. OOM must be caught
-- inside the callback boundary, and the receiver must recover after unwinding.
local oom=ok(ailo.new(config()))
local toggle=_ailo_fail_allocations
local succeeded=pcall(function()
  oom:generate(request(),function(event)
    if event.kind=="text_delta" then toggle(true) end
  end)
end)
toggle(false)
assert(not succeeded,"allocation fault was not exercised")
ok(oom:generate(request(),function()end));oom:close()
-- Mutating caller-owned tables during callbacks cannot invalidate native views.
local mutated=request()
local m=ok(ailo.new(config()))
ok(m:generate(mutated,function() mutated.items=nil;collectgarbage("collect") end));m:close()
-- Actual capture cycle (the closure keeps a wrapper in an upvalue).
local cycleweak=setmetatable({},{__mode="v"})
do
  local box={}
  box.client=ok(ailo.new(config{lazy=true,source={local_path=path,progress=function()return box.client~=nil end}}))
  cycleweak[1]=box.client._native
end
collectgarbage("collect");collectgarbage("collect");assert(not cycleweak[1])
print("Lua allocation failure, immutable request views and finalizer cycles: passed")
-- Owned metadata JSON must be freed even when Lua cannot allocate its string.
local metadata_oom=ok(ailo.new(config{lazy=true,source={local_path=path,progress=function(p)if p.total>0 then toggle(true)end end}}))
local metadata_success=pcall(function()metadata_oom:model_card()end)
toggle(false);assert(not metadata_success)
collectgarbage("collect");ok(metadata_oom:model_card());metadata_oom:close()
-- Constructor completion can fail while building the Lua wrapper; native
-- state and the retained logger must remain finalizable in that case too.
local construction_log=pslog.new_json{output=work.."/construction-oom.log"}
local construction_weak=setmetatable({construction_log},{__mode="v"})
local construction_success=pcall(function()
  ailo.new(config{logger=construction_log,source={local_path=path,progress=function(p)if p.total>0 then toggle(true)end end}})
end)
toggle(false);assert(not construction_success)
construction_log=nil;collectgarbage("collect");collectgarbage("collect");assert(not construction_weak[1])
print("Lua metadata and constructor OOM ownership: passed")

local preflight=ok(ailo.new(config{lazy=true}))
local preflight_error=fails("invalid_argument",function()return preflight:generate({items={}},function()end)end)
assert(preflight_error.info.actual_backend=="auto","backend must remain unselected before inference")
preflight:close()
