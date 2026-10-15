local ailo=require('ailo')
local function ok(value,err)assert(value,err and err.message);return value end
local receiver=ok(ailo.new{source={local_path=arg[1]},backend='cpu',threads=1,
  context_tokens=1024,lazy=true})
local handler=ok(receiver:new_handler())
local value,err=receiver:close();assert(not value and err.code=='busy')
local request={method='GET',target='/v1/models'}
local function exercise(req,failure,cancel)
  local phase,bytes,header,first=0,0,nil,false
  local thrown={transport='failure'}
  local transport={state='connection',
    begin=function(self,h)
      assert(self.state=='connection')
      assert(phase==0 and h.request_id:match('^req_%x+$') and h.cache_control=='no-store')
      phase=1;header=h
      if failure=='begin'then error(thrown)end
    end,
    write=function(_,s)
      assert(phase==1 and #s>0)
      if failure=='write'then error(thrown)end
      if cancel then return false end
      bytes=bytes+#s
      if s:find('a',1,true)then first=true end
      collectgarbage('collect')
      local closed,problem=handler:close();assert(not closed and problem.code=='busy')
      closed,problem=receiver:close();assert(not closed and problem.code=='busy')
    end,
    finish=function()
      assert(phase==1);phase=2
      if failure=='finish'then error(thrown)end
    end,
    abort=function()
      assert(phase==1 or phase==2);phase=3
      if failure=='abort'then error(thrown)end
    end}
  if failure=='abort'then
    transport.write=function()return false end
  end
  local result,problem=handler:handle(req,transport)
  if failure then
    assert(not result and problem.code=='sink_failed' and problem.cause==thrown)
    assert(problem.info.aborted and not problem.info.completed and phase==3)
  elseif cancel then
    assert(not result and problem.code=='cancelled' and phase==3)
  elseif req.target=='/missing'then
    assert(not result and problem.code=='not_found' and problem.info.completed and phase==2)
    assert(header.status==404 and problem.info.bytes_written==bytes)
  else
    result=ok(result,problem)
    assert(result.completed and not result.aborted and phase==2)
    assert(header.status==200 and result.bytes_written==bytes and bytes>0 and first)
  end
end
exercise(request)
exercise({method='GET',target='/missing'})
for _,kind in ipairs{'begin','write','finish','abort'}do exercise(request,kind)end
exercise(request,nil,true)
local body='{"model":"local-model","messages":[{"role":"user","content":"Hi"}],"stream":true,"store":false,"max_completion_tokens":8,"temperature":0,"logit_bias":{"100":100}}'
exercise{method='POST',target='/v1/chat/completions',content_type='application/json',body=body}
ok(handler:close());ok(receiver:close())
value,err=handler:handle(request,{begin=function()end,write=function()end,finish=function()end,abort=function()end})
assert(not value and err.code=='invalid_argument')
-- An otherwise unreachable receiver stays alive through the native owner pin.
do
  local owner=ok(ailo.new{source={local_path=arg[1]},backend='cpu',lazy=true})
  local kept=ok(owner:new_handler())
  owner=nil;collectgarbage('collect')
  ok(kept:handle(request,{begin=function()end,write=function()end,finish=function()end,abort=function()end}))
  ok(kept:close())
end
collectgarbage('collect')
-- Exercise the real repository admission/commit/abort path through the handler.
do
  local repository=dofile(arg[2]).new()
  local owner=ok(ailo.new{source={local_path=arg[1]},backend='cpu',threads=1,
    context_tokens=1024,lazy=true,openai={namespace_id='handler-test',repository=repository}})
  local serving=ok(owner:new_handler())
  local stored={method='POST',target='/v1/chat/completions',content_type='application/json',
    body=body:gsub('"store":false','"store":true')}
  local chunks,header={},nil
  local normal={begin=function(_,h)header=h end,write=function(_,s)chunks[#chunks+1]=s end,
    finish=function()end,abort=function()error('unexpected abort')end}
  repository.marker={code='io_failed',message='PRIVATE_REPOSITORY_CAUSE'}
  repository.fault='reserve_throw'
  local result,problem=serving:handle(stored,normal)
  assert(not result and problem.code=='io_failed' and problem.cause==repository.marker)
  assert(problem.info.completed and header.status==500)
  assert(not table.concat(chunks):find('PRIVATE_REPOSITORY_CAUSE',1,true))
  assert(repository.committed==0 and next(repository.active)==nil)
  repository.fault=nil;chunks={}
  ok(serving:handle(stored,normal));assert(repository.committed==1 and header.status==200)
  repository.fault='abort'
  result,problem=serving:handle(stored,{begin=function()end,write=function()return false end,
    finish=function()error('unexpected finish')end,abort=function()end})
  assert(not result and problem.code=='cancelled' and problem.cleanup_cause==repository.marker)
  assert(problem.info.aborted and repository.aborted==1 and next(repository.active)==nil)
  repository.fault=nil;ok(serving:close());ok(owner:close())
end
print('Lua handler lifecycle, callback failures, cancellation and owner pin passed')
