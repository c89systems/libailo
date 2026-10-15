-- Test-only in-memory repository. Production providers own durable storage.
local M={}
local function key(ns,protocol,id)return ns.."\0"..protocol.."\0"..id end
function M.new()
  local r={records={},active={},reading={},reserved=0,aborted=0,committed=0,closed=0,erased=0}
  function r:reserve(metadata)
    assert(type(metadata.namespace_id)=="string" and (metadata.protocol=="chat" or metadata.protocol=="response") and metadata.created_at>0 and metadata.max_bytes>0)
    self.reserved=self.reserved+1
    if self.hook then self.hook()end
    if self.fault=="reserve_throw" then error(self.marker)end
    local tx={metadata=metadata,parts={output={},input={},metadata={}},bytes=0}
    self.active[tx]=true;self.last_metadata=metadata
    if self.fault=="reserve" then return tx,self.marker end
    return tx
  end
  function r:write(tx,section,bytes)
    assert(self.active[tx] and tx.parts[section] and type(bytes)=="string")
    if self.fault=="write" then error(self.marker)end
    tx.bytes=tx.bytes+#bytes;assert(tx.bytes<=tx.metadata.max_bytes)
    tx.parts[section][#tx.parts[section]+1]=bytes
    if self.gc then collectgarbage("collect")end
    return true
  end
  function r:commit(tx)
    assert(self.active[tx])
    if self.fault=="commit" then return nil,self.marker end
    local m=tx.metadata
    self.records[key(m.namespace_id,m.protocol,m.id)]={parts={output=table.concat(tx.parts.output),input=table.concat(tx.parts.input),metadata=table.concat(tx.parts.metadata)},expires=m.expires_at}
    self.active[tx]=nil;self.committed=self.committed+1
    return true
  end
  function r:abort(tx)
    assert(self.active[tx]);self.active[tx]=nil;self.aborted=self.aborted+1
    if self.fault=="abort" then error(self.marker)end
  end
  function r:open(ns,protocol,id)
    local handle=self.records[key(ns,protocol,id)]
    if not handle then return nil,{code="not_found",message="fixture record is unavailable"}end
    self.reading[handle]=true
    if self.fault=="open" then return handle,0,self.marker end
    if self.fault=="expires" then return handle,"invalid expiry"end
    return handle,self.expires or handle.expires
  end
  function r:read(handle,section,emit)
    assert(self.reading[handle]);self.last_emit=emit
    if self.fault=="read" then error(self.marker)end
    if self.fault=="read_coroutine" then
      local success,why=coroutine.resume(coroutine.create(function()emit("wrong thread")end))
      assert(not success and tostring(why):find("invoking Lua thread"))
      return nil,self.marker
    end
    local bytes=assert(handle.parts[section])
    for i=1,#bytes,17 do
      if not emit(bytes:sub(i,i+16)) and not self.ignore_rejection then return nil,{code="cancelled"}end
    end
    return true
  end
  function r:close(handle)
    assert(self.reading[handle]);self.reading[handle]=nil;self.closed=self.closed+1
    if self.fault=="close" then error(self.marker)end
  end
  function r:erase(ns,protocol,id)
    if self.fault=="erase" then return nil,self.marker end
    self.records[key(ns,protocol,id)]=nil;self.erased=self.erased+1;return true
  end
  return r
end
function M.exercise(ailo,path,memory)
  local function ok(value,err)assert(value,err and (err.code..": "..err.message));return value end
  local function fails(code,fn)local v,e=fn();assert(v==nil and e and e.code==code,tostring(e and e.code));return e end
  local r=M.new();r.gc=memory
  local function client(ns,extra)
    local api={namespace_id=ns or "A",repository=r,chat_retention_seconds=60,max_record_bytes=4096}
    for k,v in pairs(extra or {})do api[k]=v end
    return ok(ailo.new{source={local_path=path},backend="cpu",threads=1,context_tokens=128,lazy=true,openai=api})
  end
  local c=client()
  local json='{"model":"local-model","messages":[{"role":"user","content":"Hi"}],"stream":true,"max_completion_tokens":2,"temperature":0,"metadata":{"ticket":"T-1"},"stream_options":{"include_obfuscation":false}}'
  local chunks={}
  r.hook=function()fails("busy",function()return c:close()end)end
  local run=ok(c:chat_completions(json,function(bytes)chunks[#chunks+1]=bytes end));r.hook=nil
  assert(run.stored and r.committed==1 and not next(r.active) and table.concat(chunks):find("data: %[DONE%]"))
  assert(r.last_metadata.expires_at==r.last_metadata.created_at+60)
  local id=run.id;c:close();c=client()
  local result={};ok(c:chat_retrieve(id,function(bytes)result[#result+1]=bytes;if memory then collectgarbage("collect")end end))
  assert(table.concat(result)==r.records[key("A","chat",id)].parts.output and not next(r.reading))
  local escaped,why=pcall(r.last_emit,"late");assert(not escaped and tostring(why):find("no longer active"))
  local marker={repository=true};r.marker=marker;r.fault="read"
  local failed=fails("io_failed",function()return c:chat_retrieve(id,function()end)end)
  assert(failed.cause==marker and not next(r.reading))
  escaped=pcall(r.last_emit,"late");assert(not escaped)
  r.fault="read_coroutine"
  failed=fails("io_failed",function()return c:chat_retrieve(id,function()error("wrong-thread delivery")end)end)
  assert(failed.cause==marker and not next(r.reading))
  r.fault="close"
  local read_error={read_sink=true}
  failed=fails("sink_failed",function()return c:chat_retrieve(id,function()
    fails("busy",function()return c:close()end)
    error(read_error)
  end)end)
  assert(failed.cause==read_error and failed.cleanup_cause==marker and not next(r.reading))
  r.fault="abort"
  local original={sink=true}
  failed=fails("sink_failed",function()return c:chat_completions(json,function()error(original)end)end)
  assert(failed.cause==original and failed.cleanup_cause==marker and not next(r.active))
  r.fault=nil
  if not memory then
    local old=r.reserve;r.reserve=function()error("method snapshot was not copied")end
    ok(c:chat_completions(json,function()end));r.reserve=old
    local sink_calls=0;r.ignore_rejection=true
    failed=fails("cancelled",function()return c:chat_retrieve(id,function()sink_calls=sink_calls+1;return false end)end)
    assert(sink_calls==1 and not next(r.reading));r.ignore_rejection=false
    local other=client("B");fails("not_found",function()return other:chat_retrieve(id,function()end)end);other:close()
    result={};ok(c:chat_retrieve(id,function(bytes)result[#result+1]=bytes end,{input=true}));assert(table.concat(result):find("Hi"))
    assert(r.records[key("A","chat",id)].parts.metadata:find("T%-1"))
    for _,fault in ipairs{"reserve","reserve_throw","write","commit","open","expires","erase","close"}do
      r.marker={code=fault=="reserve" and "out_of_range" or "io_failed",fault=fault};r.fault=fault
      local e=fails(fault=="reserve" and "out_of_range" or "io_failed",function()
        if fault=="open" or fault=="expires" or fault=="close" then return c:chat_retrieve(id,function()end)
        elseif fault=="erase" then return c:chat_delete(id,function()end)
        else return c:chat_completions(json,function()end)end
      end)
      assert(not next(r.active) and not next(r.reading))
      if fault=="close" then assert(e.cleanup_cause==r.marker)
      elseif fault=="expires" then assert(type(e.cause)=="string")
      else assert(e.cause==r.marker)end
    end
    r.fault=nil;r.expires=1;fails("not_found",function()return c:chat_retrieve(id,function()end)end);r.expires=nil
    local zdr=client("A",{zero_data_retention=true});local before=r.reserved
    local stateless=ok(zdr:chat_completions(json,function()end));assert(not stateless.stored and r.reserved==before);zdr:close()
    local opt=client("A",{chat_store_opt_in=true});stateless=ok(opt:chat_completions(json,function()end));assert(not stateless.stored);opt:close()
    fails("invalid_argument",function()return ailo.new{source={local_path=path},openai={repository={}}}end)
    fails("invalid_argument",function()return ailo.new{source={local_path=path},openai={response_retention_seconds=1}}end)
    local huge=client("A",{chat_retention_seconds={hi=0xffffffff,lo=0xffffffff}})
    fails("out_of_range",function()return huge:chat_completions(json,function()end)end);huge:close()
    fails("invalid_argument",function()return c:chat_retrieve(id,function()end,false)end)
  end
  r.fault=nil;ok(c:chat_delete(id,function()end));assert(r.erased>0 and not next(r.active) and not next(r.reading))
  local response_json='{"input":"Hi","stream":true,"max_output_tokens":2,"temperature":0,"stream_options":{"include_obfuscation":false}}'
  local events={};local response=ok(c:responses(response_json,function(bytes)events[#events+1]=bytes;if memory then collectgarbage("collect")end end))
  assert(response.stored and table.concat(events):find("event: response.incomplete") and r.last_metadata.expires_at>=r.last_metadata.created_at+2592000)
  result={};ok(c:response_retrieve(response.id,function(bytes)result[#result+1]=bytes end))
  assert(table.concat(result)==r.records[key("A","response",response.id)].parts.output)
  local chained='{"previous_response_id":"'..response.id..'","input":"Next","store":false,"max_output_tokens":2,"temperature":0}'
  assert(not ok(c:responses(chained,function()if memory then collectgarbage("collect")end end)).stored)
  failed=fails("sink_failed",function()return c:responses(response_json,function()error(original)end)end)
  assert(failed.cause==original and not next(r.active) and not next(r.reading))
  ok(c:response_delete(response.id,function()end))
  fails("not_found",function()return c:response_retrieve(response.id,function()end)end)
  c:close()
  -- Retained closures can outlive client/reader teardown without borrowed C
  -- pointers. The retained provider itself is released when the client closes.
  local late=r.last_emit;local weak=setmetatable({r},{__mode="v"});r=nil
  collectgarbage("collect");assert(weak[1]==nil)
  assert(not pcall(late,"after close"));c=nil;collectgarbage("collect")
  M.compaction(ailo,path,memory)
end
function M.compaction(ailo,path,memory)
  local function ok(v,e)assert(v,e and (e.code..": "..e.message));return v end
  local r=M.new()
  local function client(mode,thinking)
    local owner
    if mode=="ephemeral" then owner=ok(ailo.new_compactor())
    else owner=ok(ailo.new_compactor{namespace_id="key-domain",key_id="one",key=string.rep("k",32),ttl_seconds=mode=="expiring" and 2591999 or mode=="retained" and 2592000 or 0})end
    local template=thinking and "{% for m in messages %}{{m.role}}:{% if m.reasoning_content is defined %}<think>{{m.reasoning_content}}</think>{% endif %}{{m.content}}|{% endfor %}{% if add_generation_prompt %}assistant:{% if enable_thinking %}<think>{% endif %}{% endif %}" or "{% for m in messages %}{{m.role}}:{{m.content}}|{% endfor %}"
    local c=ok(ailo.new{source={local_path=path..(thinking and "-reasoning" or "-auto-compaction")},backend="cpu",threads=1,context_tokens=4096,
      chat_template=template,compactor=owner,
      openai={namespace_id="A",repository=r,max_record_bytes=16384,
        compaction={target_tokens=800,summary_context_tokens=8192}}})
    owner:close();return c
  end
  local c=client()
  local text={"Finish ticket T-1. "..string.rep("x",1000),"old answer","older","answer","recent","answer","newest","answer","current"}
  local items={};for i,s in ipairs(text)do items[i]='{"role":"'..(i%2==1 and "user" or "assistant")..'","content":"'..s..'"}'end
  local body='{"model":"local-model","instructions":"Only compact","input":['..table.concat(items,",")..']}'
  local chunks={};local compact=ok(c:response_compact(body,function(bytes)chunks[#chunks+1]=bytes;if memory then collectgarbage("collect")end end))
  assert(not compact.stored and r.reserved==0 and compact.usage.compaction_passes>0 and #chunks>3)
  local json=table.concat(chunks)
  -- Fixed test data has no brackets inside strings; this extracts the canonical
  -- output array, not a production JSON parser.
  local window=assert(json:match('"output":(%b[])'))
  assert(json:find('"object":"response.compaction"') and not json:find("Only compact"))
  local marker={compact_sink=true}
  local value,err=c:response_compact(body,function()error(marker)end)
  assert(value==nil and err.code=="sink_failed" and err.cause==marker and r.reserved==0)
  local replay='{"input":'..window..',"instructions":"New per-request","stream":true,"max_output_tokens":2,"temperature":0,"stream_options":{"include_obfuscation":false}}'
  local response=ok(c:responses(replay,function()if memory then collectgarbage("collect")end end));assert(response.stored)
  c:close();c=client()
  local next_response=ok(c:responses('{"previous_response_id":"'..response.id..'","input":"Next","max_output_tokens":2,"temperature":0}',function()if memory then collectgarbage("collect")end end))
  assert(next_response.stored and not next(r.active) and not next(r.reading))
  ok(c:response_delete(response.id,function()end));ok(c:response_delete(next_response.id,function()end));c:close()
  for _,mode in ipairs{"ephemeral","expiring"}do
    local temporary=client(mode);local own_window=window
    if mode=="ephemeral" then
      local bytes={};ok(temporary:response_compact(body,function(s)bytes[#bytes+1]=s end))
      own_window=assert(table.concat(bytes):match('"output":(%b[])'))
    end
    local saved=r.aborted
    local v,e=temporary:responses('{"input":'..own_window..',"max_output_tokens":2,"temperature":0}',function()end)
    assert(v==nil and e.code=="invalid_argument" and e.message:find("covering response retention") and r.aborted==saved+1)
    assert(not ok(temporary:responses('{"input":'..own_window..',"store":false,"max_output_tokens":2,"temperature":0}',function()end)).stored)
    temporary:close()
  end
  local finite=client("retained")
  local saved=ok(finite:responses('{"input":'..window..',"max_output_tokens":2,"temperature":0}',function()end));assert(saved.stored)
  finite:close();finite=client("retained")
  local child=ok(finite:responses('{"previous_response_id":"'..saved.id..'","input":"Next","max_output_tokens":2,"temperature":0}',function()end));assert(child.stored)
  ok(finite:response_delete(saved.id,function()end));ok(finite:response_delete(child.id,function()end));finite:close()
  -- Automatic preflight compaction uses the same ABI and durable lineage.
  -- A stored create is followed across receiver reconstruction with new input.
  c=client("retained")
  local automatic=body:sub(1,-2)..',"stream":true,"max_output_tokens":2,"temperature":0,"context_management":[{"type":"compaction","compact_threshold":900}],"stream_options":{"include_obfuscation":false}}'
  local frames={}
  local run=ok(c:responses(automatic,function(s)frames[#frames+1]=s end))
  assert(run.stored and run.usage.compaction_passes==1 and table.concat(frames):find('"type":"compaction"') and table.concat(frames):find('event: response.output_item.done'))
  local response_id=run.id;c:close();c=client("retained")
  local descendant=ok(c:responses('{"previous_response_id":"'..response_id..'","input":"Next","instructions":"Replacement policy","max_output_tokens":2,"temperature":0}',function()if memory then collectgarbage("collect")end end))
  assert(descendant.stored and not next(r.active) and not next(r.reading))
  ok(c:response_delete(response_id,function()end));ok(c:response_delete(descendant.id,function()end))
  r.marker={automatic_commit=true};r.fault="commit";local aborted=r.aborted
  local v,e=c:responses(automatic,function()end)
  assert(v==nil and e.code=="io_failed" and e.cause==r.marker and r.aborted==aborted+1 and not next(r.active))
  r.fault=nil;c:close()
  -- Encrypted reasoning is emitted by default, survives stored retrieval and
  -- reconstructs an incomplete reasoning-only historical assistant turn.
  c=client("retained",true)
  local thinking='{"input":"Hi","stream":true,"max_output_tokens":8,"temperature":0,"stream_options":{"include_obfuscation":false}}'
  frames={};run=ok(c:responses(thinking,function(s)frames[#frames+1]=s end))
  local wire=table.concat(frames)
  local token=assert(wire:match('"encrypted_content":"([^"]+)"'))
  local item_id=assert(wire:match('"id":"(rs_[^"]+)"'))
  assert(run.stored and run.usage.reasoning_tokens>0 and wire:find('event: response.reasoning_text.delta')<wire:find('event: response.output_item.done'))
  c:close();c=client("retained",true)
  local retrieved={};ok(c:response_retrieve(run.id,function(s)retrieved[#retrieved+1]=s end))
  assert(table.concat(retrieved):find(token,1,true))
  local next_run=ok(c:responses('{"previous_response_id":"'..run.id..'","input":"Next","max_output_tokens":2,"temperature":0}',function()end));assert(next_run.stored)
  local opaque='{"input":[{"type":"reasoning","id":"'..item_id..'","summary":[],"encrypted_content":"'..token..'"},{"role":"user","content":"Next"}],"store":false,"include":["reasoning.encrypted_content"],"max_output_tokens":2,"temperature":0}'
  assert(not ok(c:responses(opaque,function()end)).stored)
  ok(c:response_delete(run.id,function()end));ok(c:response_delete(next_run.id,function()end));c:close()
  c=client("ephemeral",true);local deliveries=0;aborted=r.aborted
  v,e=c:responses(thinking,function()deliveries=deliveries+1 end)
  assert(v==nil and e.code=="invalid_argument" and e.message:find("covering response retention") and deliveries==0 and r.aborted==aborted+1)
  local transient=thinking:sub(1,-2)..',"store":false}'
  frames={};assert(not ok(c:responses(transient,function(s)frames[#frames+1]=s end)).stored)
  assert(table.concat(frames):find('"encrypted_content":"'));c:close()
end
return M
