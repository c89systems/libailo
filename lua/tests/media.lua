local ailo=require('ailo')
local path=arg[1]
local function ok(v,e) assert(v,e and (e.code..': '..e.message));return v end
local function fails(code,fn)
  local v,e=fn();assert(v==nil and e and e.code==code,tostring(e and e.code));return e
end
local function config(resolver)
  return {source={local_path=path},backend='cpu',lazy=true,
    multimodal={projector={local_path=path},backend='cpu',resolver=resolver}}
end
local function request()
  return {items={{role='user',content={{kind='image',file_id='file-fixture',detail='low'}}}},max_tokens=1}
end
do
  local c=ok(ailo.new(config()))
  local capabilities=ok(c:capabilities())
  assert(capabilities:find('"media_configured":true',1,true))
  assert(capabilities:find('"media_loaded":false',1,true))
  fails('unsupported',function()return c:generate(request(),function()end)end)
  ok(c:close())
end
do
  local c,saved,calls
  calls=0
  c=ok(ailo.new(config(function(reference,emit)
    calls=calls+1;saved=emit
    assert(reference.kind=='file_id' and reference.value=='file-fixture')
    assert(reference.max_bytes==16*1024*1024 and reference.timeout_ms==15000)
    fails('busy',function()return c:generate({items={{role='user',content='reenter'}}},function()end)end)
    assert(emit('\137PNG\r\n\26\nfixture'))
    return true
  end)))
  -- Resolution succeeds; the deliberately text-only GGUF is an invalid projector.
  fails('model_failed',function()return c:generate(request(),function()end)end)
  assert(calls==1)
  assert(not pcall(saved,'late bytes'),'escaped emitter must be invalidated')
  ok(c:close())
end
do
  local marker={why='resolver failure'}
  local c=ok(ailo.new(config(function()error(marker)end)))
  local e=fails('sink_failed',function()return c:generate(request(),function()end)end)
  assert(e.cause==marker)
  ok(c:close())
end
do
  local c
  c=ok(ailo.new(config(function(_,emit)
    c:cancel();assert(not emit('cancelled'));return true
  end)))
  fails('cancelled',function()return c:generate(request(),function()end)end)
  ok(c:close())
end
fails('invalid_argument',function()return ailo.new{source={local_path=path},multimodal={projector={local_path=path},resolver='invalid'}}end)
print('Lua media configuration, resolution, reentry, escaped-emitter lifetime and cancellation passed')
