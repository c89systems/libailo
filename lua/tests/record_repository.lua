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
  M.conversations(ailo,path,memory)
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
-- Small, explicitly materialized test repository. The scanner extracts values
-- from library-written JSON objects; it is not used by the shipped binding.
local function value_end(s,p)
  local first=s:sub(p,p)
  if first=='"' then
    p=p+1
    while p<=#s do local c=s:sub(p,p);if c=='\\' then p=p+2 elseif c=='"' then return p else p=p+1 end end
  elseif first=='{' or first=='[' then
    local close=first=='{' and '}' or ']';p=p+1
    while p<=#s do local c=s:sub(p,p);if c==close then return p elseif c==',' or c==':' or c:match('%s')then p=p+1 else p=value_end(s,p)+1 end end
  else
    while p<=#s and not s:sub(p,p):match('[,%]}%s]')do p=p+1 end
    return p-1
  end
  error('incomplete fixture JSON')
end
local function rows(s)
  local out={};local p=2
  while p<#s do
    if s:sub(p,p)==',' or s:sub(p,p):match('%s')then p=p+1 else
      local endp=value_end(s,p);out[#out+1]=s:sub(p,endp);p=endp+1
    end
  end
  return out
end
local function member(s,name)
  local p=2
  while p<#s do
    if s:sub(p,p)==',' or s:sub(p,p):match('%s')then p=p+1 else
      local endp=value_end(s,p);local keyname=s:sub(p+1,endp-1);p=endp+1
      while s:sub(p,p)==':' or s:sub(p,p):match('%s')do p=p+1 end
      endp=value_end(s,p);if keyname==name then return s:sub(p,endp)end;p=endp+1
    end
  end
end
function M.conversation_repository()
  local r=M.new();r.locks={}
  local base_commit,base_abort,base_read=r.commit,r.abort,r.read
  function r:amend(c)
    local k=key(c.namespace_id,'conversation',c.id)
    if self.amend_hook then self.amend_hook(c)end
    local old=self.records[k]
    if self.locks[k] or (c.action=='create' and old) or (c.action~='create' and old and old.revision~=c.revision)then return nil,{code='busy'}end
    if c.action~='create' and not old then return nil,{code='not_found'}end
    local present={};for _,row in ipairs(old and old.rows or {})do present[row.id]=true end
    for _,id in ipairs(c.input_ids)do if present[id]then return nil,{code='invalid_argument'}end;present[id]=true end
    if c.action=='delete_item' and not present[c.item_id]then return nil,{code='not_found'}end
    local tx={change=c,key=k,old=old,parts={output={},input={},metadata={},conversation_state={},conversation_items={}},bytes=0,metadata={max_bytes=c.max_bytes}}
    self.reserved=self.reserved+1;self.active[tx]=true;self.locks[k]=tx
    if self.fault=='amend' then return tx,self.marker end
    return tx
  end
  function r:commit(tx)
    if not tx.change then return base_commit(self,tx)end
    if self.fault=='commit' then return nil,self.marker end
    local c=tx.change;local old=tx.old
    local entries={};for _,entry in ipairs(old and old.rows or {})do if entry.id~=c.item_id then entries[#entries+1]=entry end end
    local seen={};for _,entry in ipairs(entries)do seen[entry.id]=true end
    for _,row in ipairs(rows(table.concat(tx.parts.conversation_items)))do
      local id=assert(member(row,'id')):sub(2,-2)
      assert(not seen[id]);seen[id]=true
      entries[#entries+1]={id=id,wire=assert(member(row,'wire')),canonical=assert(member(row,'canonical'))}
    end
    local output=table.concat(tx.parts.output)
    if c.response then
      local m=c.response
      self.records[key(m.namespace_id,m.protocol,m.id)]={parts={input=table.concat(tx.parts.input),output=output,metadata=table.concat(tx.parts.metadata)},expires=m.expires_at}
      output=old.parts.output
    elseif output=='' then output=old.parts.output end
    self.records[tx.key]={parts={output=output,input=table.concat(tx.parts.conversation_state)},expires=0,revision=c.revision+1,rows=entries}
    self.active[tx]=nil;self.locks[tx.key]=nil;self.committed=self.committed+1;return true
  end
  function r:abort(tx)
    if tx.change then self.locks[tx.key]=nil end
    return base_abort(self,tx)
  end
  function r:query(q)
    local old=self.records[key(q.namespace_id,'conversation',q.id)]
    if not old then return nil,{code='not_found'}end
    local selected={};local eligible={}
    for _,row in ipairs(old.rows)do if (not q.item_id or row.id==q.item_id)and(not q.canonical or row.canonical~='null')then eligible[#eligible+1]=row end end
    if q.descending then local rev={};for i=#eligible,1,-1 do rev[#rev+1]=eligible[i]end;eligible=rev end
    local started=not q.after
    for _,row in ipairs(eligible)do
      if started then selected[#selected+1]=q.canonical and row.canonical or row.wire
      elseif row.id==q.after then started=true end
      if q.limit>0 and #selected>=q.limit+1 then break end
    end
    if not started then return nil,{code='not_found'}end
    local handle={parts={output='['..table.concat(selected,',')..']'},query=true}
    if self.bad_query then handle.parts.output='[{"id":"broken"' end
    self.reading[handle]=true
    if self.fault=='query' then return handle,self.marker end
    return handle
  end
  function r:read(handle,section,emit)
    self.in_query=handle.query
    self.query_total=handle.query and #handle.parts.output or 0;self.query_sent=0
    local result,err=base_read(self,handle,section,function(s)self.query_sent=self.query_sent+#s;return emit(s)end)
    self.in_query=false;return result,err
  end
  return r
end
function M.conversations(ailo,path,memory)
  local function ok(v,e)assert(v,e and(e.code..': '..e.message));return v end
  local r=M.conversation_repository()
  local function client(ns,zdr)
    return ok(ailo.new{source={local_path=path},backend='cpu',threads=1,context_tokens=512,lazy=true,
      openai={namespace_id=ns or 'C',repository=r,zero_data_retention=zdr,max_record_bytes=16384}})
  end
  local c=client();local output={}
  local function sink(s)output[#output+1]=s end
  local created=ok(c:conversation_create('{"metadata":{"topic":"test"},"items":[{"role":"user","content":"Hello"}]}',sink))
  local id=created.id;assert(id:match('^conv_')and created.stored)
  local function text()local s=table.concat(output);output={};return s end
  assert(text():find('"topic":"test"'))
  ok(c:conversation_retrieve(id,sink));assert(text():find(id,1,true))
  ok(c:conversation_items_list(id,sink,'{"order":"asc","limit":1}'))
  local listed=text();local first=assert(listed:match('"first_id":"([^"]+)"'));assert(listed:find('"type":"message"')and listed:find('"content":%['))
  ok(c:conversation_item_retrieve(id,first,sink));assert(text():find('Hello'))
  ok(c:conversation_update(id,'{"metadata":{"owner":"new"}}',sink));assert(text():find('"owner":"new"'))
  local unsupported_deliveries=0
  local av,ae=c:responses('{"conversation":"'..id..'","input":"No permanent keys","store":false,"context_management":[{"type":"compaction"}]}',function()unsupported_deliveries=unsupported_deliveries+1 end)
  assert(av==nil and ae.code=='invalid_argument'and unsupported_deliveries==0 and not next(r.active))
  local reserved=r.reserved
  local bv,be=c:conversation_create('{"items":['..string.rep('{"role":"user","content":"x"},',20)..'{"role":"user","content":"x"}]}',function()end)
  assert(bv==nil and be.code=='invalid_argument'and r.reserved==reserved)
  bv,be=c:conversation_update(id,'{}',function()end);assert(bv==nil and be.code=='invalid_argument')
  for _,options in ipairs{'{"limit":0}','{"order":"bad"}','{"include":["unsupported"]}'}do
    bv,be=c:conversation_items_list(id,function()end,options);assert(bv==nil and be)
  end
  r.marker={amend_failure=true};r.fault='amend';local aborted=r.aborted
  bv,be=c:conversation_update(id,'{"metadata":null}',function()end)
  assert(bv==nil and be.cause==r.marker and r.aborted==aborted+1 and not next(r.active)and not next(r.locks));r.fault=nil
  r.amend_hook=function(change)
    r.amend_hook=nil
    local k=key(change.namespace_id,'conversation',change.id);local old=r.records[k]
    local copied={parts={output=old.parts.output,input=old.parts.input:gsub('"ailo_revision":%d+','"ailo_revision":'..(old.revision+1),1)},expires=0,revision=old.revision+1,rows=old.rows}
    r.records[k]=copied
  end
  bv,be=c:conversation_update(id,'{"metadata":null}',function()end);assert(bv==nil and be.code=='busy')
  local run=ok(c:responses('{"conversation":"'..id..'","input":"Next","store":false,"stream":true,"max_output_tokens":2,"temperature":0,"stream_options":{"include_obfuscation":false}}',sink))
  assert(not run.stored and text():find('event: response.incomplete')and not next(r.active))
  local stored=ok(c:responses('{"conversation":{"id":"'..id..'"},"input":"Again","max_output_tokens":2,"temperature":0}',sink));assert(stored.stored);text()
  local streaming=false;local chunks={}
  ok(c:conversation_items_list(id,function(s)chunks[#chunks+1]=s;if table.concat(chunks):find('Hello')and r.in_query and r.query_sent<r.query_total then streaming=true end end,'{"order":"asc","limit":100}'))
  assert(streaming and r.records[key('C','conversation',id)].expires==0)
  local marker={query_sink=true}
  bv,be=c:conversation_items_list(id,function()error(marker)end)
  assert(bv==nil and be.code=='sink_failed'and be.cause==marker and not next(r.reading))
  r.bad_query=true;bv,be=c:conversation_items_list(id,function()end);assert(bv==nil and be and not next(r.reading));r.bad_query=false
  r.marker={query_failure=true};r.fault='query';local closed=r.closed
  local qv,qe=c:conversation_items_list(id,function()end);assert(qv==nil and qe.cause==r.marker and r.closed==closed+1 and not next(r.reading));r.fault=nil
  c:close();c=client()
  ok(c:conversation_items_list(id,sink,'{"order":"asc","limit":1}'));assert(text():find('"has_more":true'))
  ok(c:conversation_items_list(id,sink,'{"order":"asc","after":"'..first..'","limit":2}'));assert(not text():find('Hello'))
  ok(c:conversation_items_create(id,'{"items":[{"role":"assistant","content":"Added"}]}',sink));assert(text():find('Added'))
  local before=r.committed;r.marker={conversation=true};r.fault='commit'
  local v,e=c:responses('{"conversation":"'..id..'","input":"Failed","max_output_tokens":2,"temperature":0}',function()end)
  assert(v==nil and e.code=='io_failed'and e.cause==r.marker and r.committed==before and not next(r.active)and not next(r.locks));r.fault=nil
  v,e=c:responses('{"conversation":"'..id..'","input":"Cancelled","stream":true,"max_output_tokens":2,"temperature":0}',function()return false end)
  assert(v==nil and e.code=='cancelled'and r.committed==before and not next(r.active)and not next(r.locks))
  local deliveries=0
  v,e=c:responses('{"conversation":"'..id..'","input":[{"id":"'..first..'","role":"user","content":"Duplicate"}],"store":false,"max_output_tokens":2}',function()deliveries=deliveries+1 end)
  assert(v==nil and e.code=='invalid_argument'and deliveries==0)
  local second=client();local checked=false
  local response=ok(c:responses('{"conversation":"'..id..'","input":"Check locks","store":false,"stream":true,"max_output_tokens":2,"temperature":0,"stream_options":{"include_obfuscation":false}}',function()
    if not checked then checked=true;local v,e=second:conversation_update(id,'{"metadata":null}',function()end);assert(v==nil and e.code=='busy')end
  end));assert(not response.stored and checked);second:close()
  local isolated=client('other');v,e=isolated:conversation_retrieve(id,function()end);assert(v==nil and e.code=='not_found');isolated:close()
  ok(c:conversation_item_delete(id,first,sink));text()
  v,e=c:conversation_item_retrieve(id,first,function()end);assert(v==nil and e.code=='not_found')
  local all={};ok(c:conversation_items_list(id,function(s)all[#all+1]=s end,'{"limit":100}'));assert(not table.concat(all):find('Hello'))
  assert(not ok(c:responses('{"conversation":"'..id..'","input":"After deletion","store":false,"max_output_tokens":2,"temperature":0}',function()end)).stored)
  ok(c:conversation_delete(id,sink));assert(text():find('"deleted":true'))
  v,e=c:conversation_retrieve(id,function()end);assert(v==nil and e.code=='not_found')
  local remaining={};ok(c:conversation_items_list(id,function(s)remaining[#remaining+1]=s end));local retained=table.concat(remaining);assert(#retained>50)
  local orphan=assert(retained:match('"first_id":"([^"]+)"'))
  ok(c:conversation_item_delete(id,orphan,sink));text()
  v,e=c:conversation_retrieve(id,function()end);assert(v==nil and e.code=='not_found')
  ok(c:response_delete(stored.id,function()end));c:close()
  local zdr=client('C',true);local zroot=ok(zdr:conversation_create('{}',function()end))
  local zr=ok(zdr:responses('{"conversation":"'..zroot.id..'","input":"ZDR explicit conversation","store":true,"max_output_tokens":2,"temperature":0}',function()end))
  assert(not zr.stored and r.records[key('C','conversation',zroot.id)].expires==0 and not r.records[key('C','response',zr.id)])
  ok(zdr:conversation_delete(zroot.id,function()end));zdr:close()
  if memory then return end
  -- Joint compaction/reasoning behavior has native memory coverage already;
  -- these additional semantic roundtrips run in the functional/rock gates.
  local function keyed(suffix,thinking)
    local owner=ok(ailo.new_compactor{namespace_id='conversations',key_id='one',key=string.rep('k',32)})
    local template=thinking and "{% for m in messages %}{{m.role}}:{% if m.reasoning_content is defined %}<think>{{m.reasoning_content}}</think>{% endif %}{{m.content}}|{% endfor %}{% if add_generation_prompt %}assistant:{% if enable_thinking %}<think>{% endif %}{% endif %}" or "{% for m in messages %}{{m.role}}:{{m.content}}|{% endfor %}"
    local receiver=ok(ailo.new{source={local_path=path..suffix},backend='cpu',threads=1,context_tokens=4096,compactor=owner,chat_template=template,
      openai={namespace_id='J',repository=r,max_record_bytes=65536,compaction={target_tokens=800,summary_context_tokens=8192}}})
    owner:close();return receiver
  end
  local joint=keyed('-auto-compaction',false)
  local history={};for i,s in ipairs{'Finish ticket T-1. '..string.rep('x',1000),'old answer','older','answer','recent','answer','newest','answer','current'}do history[i]='{"role":"'..(i%2==1 and 'user' or 'assistant')..'","content":"'..s..'"}'end
  local root=ok(joint:conversation_create('{"items":['..table.concat(history,',')..']}',function()end))
  local compacted=ok(joint:responses('{"conversation":"'..root.id..'","instructions":"Only this request","input":"Next","stream":true,"max_output_tokens":2,"temperature":0,"context_management":[{"type":"compaction","compact_threshold":900}]}',function()end))
  assert(compacted.stored and compacted.usage.compaction_passes>0);joint:close();joint=keyed('-auto-compaction',false)
  assert(not ok(joint:responses('{"conversation":"'..root.id..'","input":"After checkpoint","store":false,"max_output_tokens":2,"temperature":0}',function()end)).stored)
  local different=keyed('',false)
  local cv,ce=different:responses('{"conversation":"'..root.id..'","input":"Switch models","store":false,"stream":true,"max_output_tokens":2,"temperature":0}',function()return false end)
  assert(cv==nil and ce.code=='cancelled'and ce.info.usage.prompt_tokens>800)
  different:close()
  local page={};ok(joint:conversation_items_list(root.id,function(s)page[#page+1]=s end,'{"order":"asc","limit":1}'))
  local goal=assert(table.concat(page):match('"first_id":"([^"]+)"'))
  ok(joint:conversation_item_delete(root.id,goal,function()end))
  ok(joint:responses('{"conversation":"'..root.id..'","input":"After removal","store":false,"max_output_tokens":2,"temperature":0}',function()end))
  ok(joint:response_delete(compacted.id,function()end));ok(joint:conversation_delete(root.id,function()end));joint:close()
  joint=keyed('-reasoning',true);root=ok(joint:conversation_create('{}',function()end))
  local reason=ok(joint:responses('{"conversation":"'..root.id..'","input":"Hi","store":false,"max_output_tokens":2,"temperature":0}',function()end));assert(not reason.stored and reason.usage.reasoning_tokens>0)
  joint:close();joint=keyed('-reasoning',true)
  ok(joint:responses('{"conversation":"'..root.id..'","input":"Next","store":false,"max_output_tokens":2,"temperature":0}',function()end))
  ok(joint:conversation_delete(root.id,function()end));joint:close()
end
return M
