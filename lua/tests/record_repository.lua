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
    local tx={metadata=metadata,parts={output={},input={},metadata={},chat_messages={},response_input_items={}},bytes=0}
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
    local parts={};for name,chunks in pairs(tx.parts)do parts[name]=table.concat(chunks)end
    self.records[key(m.namespace_id,m.protocol,m.id)]={parts=parts,expires=m.expires_at,identity=m}
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
  M.record_operations(ailo,path,memory)
  M.routes(ailo,path)
end
function M.compaction(ailo,path,memory)
  local function ok(v,e)assert(v,e and (e.code..": "..e.message));return v end
  local r=M.record_repository()
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
  local items={};for i,s in ipairs(text)do items[i]='{'..(i==9 and '"id":"history-9",'or'')..'"role":"'..(i%2==1 and "user" or "assistant")..'","content":"'..s..'"}'end
  local body='{"model":"local-model","instructions":"Only compact","input":['..table.concat(items,",")..']}'
  local chunks={};local compact=ok(c:response_compact(body,function(bytes)chunks[#chunks+1]=bytes;if memory then collectgarbage("collect")end end))
  assert(not compact.stored and r.reserved==0 and compact.usage.compaction_passes>0 and #chunks>3)
  local json=table.concat(chunks)
  -- Current Codex uses a final compaction_trigger through Responses create.
  -- Verify the Lua receiver reaches the same streamed control/replay path.
  local triggered=body:gsub('%]%}$', ',{"type":"compaction_trigger"}]}')
  triggered=triggered:sub(1,-2)..',"stream":true,"store":false,"stream_options":{"include_obfuscation":false}}'
  local frames={};local control=ok(c:responses(triggered,function(s)frames[#frames+1]=s end))
  local wire=table.concat(frames)
  assert(not control.stored and control.usage.compaction_passes==1 and wire:find('event: response.completed') and not wire:find('event: response.output_text.delta'))
  local encrypted=assert(wire:match('"encrypted_content":"([^"]+)"'))
  local compact_id=assert(wire:match('"id":"(cmp_[^"]+)"'))
  local restored=ok(c:responses('{"input":[{"type":"compaction","id":"'..compact_id..'","encrypted_content":"'..encrypted..'"},{"role":"user","content":"Next"}],"store":false,"max_output_tokens":2,"temperature":0}',function()end))
  assert(not restored.stored and not next(r.active) and not next(r.reading))
  -- Fixed test data has no brackets inside strings; this extracts the canonical
  -- output array, not a production JSON parser.
  local window=assert(json:match('"output":(%b[])'))
  assert(json:find('"object":"response.compaction"') and not json:find("Only compact"))
  local marker={compact_sink=true}
  local value,err=c:response_compact(body,function()error(marker)end)
  assert(value==nil and err.code=="sink_failed" and err.cause==marker and r.reserved==0)
  local replay='{"input":'..window..',"instructions":"New per-request","stream":true,"max_output_tokens":2,"temperature":0,"stream_options":{"include_obfuscation":false}}'
  local response=ok(c:responses(replay,function()if memory then collectgarbage("collect")end end));assert(response.stored)
  if not memory then M.replay_input_page(c,response.id)end
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
local function quoted(s)
  local short={['\b']='\\b',['\f']='\\f',['\n']='\\n',['\r']='\\r',['\t']='\\t',['"']='\\"',['\\']='\\\\'}
  return '"'..s:gsub('[%z\1-\31"\\]',function(c)return short[c]or string.format('\\u%04x',c:byte())end)..'"'
end
local function member(s,name)
  name=quoted(name):sub(2,-2)
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
    local tx={change=c,key=k,old=old,parts={output={},input={},metadata={},conversation_state={},conversation_items={},chat_messages={},response_input_items={}},bytes=0,metadata={max_bytes=c.max_bytes}}
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
      self.records[key(m.namespace_id,m.protocol,m.id)]={parts={input=table.concat(tx.parts.input),output=output,metadata=table.concat(tx.parts.metadata),response_input_items=table.concat(tx.parts.response_input_items)},expires=m.expires_at,identity=m}
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
function M.record_repository()
  local r=M.new();r.locks={}
  local base_commit,base_abort,base_read=r.commit,r.abort,r.read
  function r:edit(e)
    local k=key(e.namespace_id,e.protocol,e.id);local old=self.records[k]
    if not old or(old.expires~=0 and old.expires<=e.now)then return nil,{code='not_found'}end
    if self.locks[k]then return nil,{code='busy'}end
    local tx={edit=true,key=k,old=old,metadata={max_bytes=e.max_bytes},parts={output={},metadata={}},bytes=0}
    local reader={parts=old.parts,expires=old.expires,edit_source=true}
    self.active[tx]=true;self.locks[k]=tx;self.reading[reader]=true
    if self.edit_hook then self.edit_hook()end
    if self.fault=='edit' then return tx,reader,old.expires,self.marker end
    if self.fault=='edit_expiry' then return tx,reader,'bad' end
    return tx,reader,old.expires
  end
  function r:commit(tx)
    if not tx.edit then return base_commit(self,tx)end
    if self.fault=='commit' then return nil,self.marker end
    local parts={};for name,value in pairs(tx.old.parts)do parts[name]=value end
    for name,chunks in pairs(tx.parts)do if #chunks>0 then parts[name]=table.concat(chunks)end end
    self.records[tx.key]={parts=parts,expires=tx.old.expires,identity=tx.old.identity}
    self.active[tx]=nil;self.locks[tx.key]=nil;self.committed=self.committed+1
    return true
  end
  function r:abort(tx)
    if tx.edit then self.locks[tx.key]=nil end
    return base_abort(self,tx)
  end
  function r:scan(q)
    local eligible={};local expiry=0
    if q.id then
      local old=self.records[key(q.namespace_id,q.protocol,q.id)]
      if not old or(old.expires~=0 and old.expires<=q.now)then return nil,{code='not_found'}end
      expiry=old.expires
      for _,row in ipairs(rows(assert(old.parts[q.section])))do eligible[#eligible+1]=row end
    else
      local selected={}
      for _,old in pairs(self.records)do
        local m=old.identity
        if m and m.namespace_id==q.namespace_id and m.protocol==q.protocol and(old.expires==0 or old.expires>q.now)then
          local matches=not q.model or member(old.parts.output,'model')==quoted(q.model)
          for k,v in pairs(q.metadata)do if member(old.parts.metadata,k)~=quoted(v)then matches=false end end
          if matches then selected[#selected+1]=old end
        end
      end
      table.sort(selected,function(a,b)local x,y=a.identity,b.identity;return x.created_at<y.created_at or(x.created_at==y.created_at and x.id<y.id)end)
      for _,old in ipairs(selected)do eligible[#eligible+1]=old.parts.output end
    end
    if q.descending then local reversed={};for n=#eligible,1,-1 do reversed[#reversed+1]=eligible[n]end;eligible=reversed end
    local selected={};local started=not q.after
    for _,row in ipairs(eligible)do
      if started then selected[#selected+1]=row elseif member(row,'id')=='"'..q.after..'"' then started=true end
      if #selected>=q.limit+1 then break end
    end
    if not started then return nil,{code='not_found'}end
    local handle={parts={output='['..table.concat(selected,',')..']'},scan=true};self.reading[handle]=true
    if self.bad_scan then handle.parts.output='[{"id":"broken"'end
    if self.fault=='scan' then return handle,expiry,self.marker end
    return handle,self.scan_expiry or expiry
  end
  function r:read(handle,section,emit)
    self.in_scan=handle.scan;self.query_total=handle.scan and #handle.parts.output or 0;self.query_sent=0
    self.read_passes=(self.read_passes or 0)+1
    local v,e=base_read(self,handle,section,function(s)self.query_sent=self.query_sent+#s;return emit(s)end)
    self.in_scan=false;return v,e
  end
  return r
end
function M.record_operations(ailo,path,memory)
  local function ok(v,e)assert(v,e and(e.code..': '..e.message));return v end
  local function fails(code,fn)local v,e=fn();assert(v==nil and e and e.code==code,tostring(e and e.code));return e end
  local r=M.record_repository()
  local function client(ns)
    return ok(ailo.new{source={local_path=path},backend='cpu',threads=1,context_tokens=256,lazy=true,
      openai={namespace_id=ns or'R',repository=r,chat_retention_seconds=60,max_record_bytes=32768}})
  end
  local c=client();local chunks={};local function sink(s)chunks[#chunks+1]=s end
  local function text()local s=table.concat(chunks);chunks={};return s end
  local body='{"model":"local-model","messages":[{"role":"system","content":"Be helpful"},{"role":"user","content":"Hi"}],"max_completion_tokens":2,"temperature":0,"stream":true,"metadata":{"team":"alpha","ticket":"one"},"stream_options":{"include_obfuscation":false}}'
  local one=ok(c:chat_completions(body,sink));text()
  local original=r.records[key('R','chat',one.id)];local expiry=original.expires
  ok(c:chat_messages_list(one.id,sink,'{"limit":1}'));local page=text()
  assert(page:find('"has_more":true')and page:find('"first_id":"'..one.id..'-0"',1,true)and page:find('Be helpful'),page)
  ok(c:chat_messages_list(one.id,sink,'{"after":"'..one.id..'-0"}'));assert(text():find('"content":"Hi"'))
  ok(c:chat_messages_list(one.id,sink,'{"order":"desc","limit":1}'));assert(text():find('"first_id":"'..one.id..'-1"',1,true))
  ok(c:chat_update(one.id,'{"metadata":{"ticket":"new"}}',sink));local updated=text()
  local record=r.records[key('R','chat',one.id)]
  assert(record.expires==expiry and record.parts.input==original.parts.input and record.parts.chat_messages==original.parts.chat_messages)
  assert(member(updated,'choices')==member(original.parts.output,'choices')and record.parts.metadata=='{"ticket":"new"}'and updated==record.parts.output)
  ok(c:chat_list(sink,'{"metadata":{"team":"alpha"}}'));assert(text():find('"data":%[%]'))
  ok(c:chat_list(sink,'{"metadata":{"ticket":"new"},"model":"local-model"}'));assert(text():find(one.id,1,true))
  ok(c:chat_list(sink,'{"metadata":{"ticket":"new","team":"alpha"}}'));assert(text():find('"data":%[%]'))
  ok(c:chat_update(one.id,'{"metadata":{"a\\u0000b":"v\\u0000x"}}',sink));text()
  ok(c:chat_list(sink,'{"metadata":{"a\\u0000b":"v\\u0000x"}}'));assert(text():find(one.id,1,true))
  ok(c:chat_list(sink,'{"metadata":{"a":"v"}}'));assert(text():find('"data":%[%]'))
  ok(c:chat_list(sink,'{"model":"not-this-model"}'));assert(text():find('"data":%[%]'))
  local before=r.committed
  local e=fails('cancelled',function()return c:chat_update(one.id,'{"metadata":null}',function()assert(r.committed==before+1);return false end)end)
  assert(e.info.stored and r.records[key('R','chat',one.id)].parts.metadata=='{}'and not next(r.reading)and not next(r.active))
  for _,fault in ipairs{'edit','edit_expiry','write','commit','read'}do
    local old=r.records[key('R','chat',one.id)];r.marker={fault=fault};r.fault=fault;local deliveries=0
    e=fails('io_failed',function()return c:chat_update(one.id,'{"metadata":{"x":"y"}}',function()deliveries=deliveries+1 end)end)
    assert(deliveries==0 and r.records[key('R','chat',one.id)]==old and not next(r.reading)and not next(r.active)and not next(r.locks))
    if fault~='edit_expiry'then assert(e.cause==r.marker)end
  end
  r.fault='scan';r.marker={scan=true};e=fails('io_failed',function()return c:chat_list(function()end)end);assert(e.cause==r.marker and not next(r.reading));r.fault=nil
  local deliveries=0;r.scan_expiry=1
  fails('not_found',function()return c:chat_messages_list(one.id,function()deliveries=deliveries+1 end)end)
  assert(deliveries==0 and not next(r.reading));r.scan_expiry=nil
  r.bad_scan=true;fails('io_failed',function()return c:chat_list(function()end)end);r.bad_scan=nil
  fails('invalid_argument',function()return c:chat_update(one.id,'{}',function()end)end)
  fails('unsupported',function()return c:chat_update(one.id,'{"metadata":{},"temperature":1}',function()end)end)
  fails('invalid_argument',function()return c:chat_list(function()end,'{"limit":0}')end)
  fails('unsupported',function()return c:chat_messages_list(one.id,function()end,'{"include":[]}')end)
  local other=client('different');ok(other:chat_list(sink));assert(text():find('"data":%[%]'))
  fails('not_found',function()return other:chat_update(one.id,'{"metadata":{}}',function()end)end);other:close()
  local json='{"input":[{"id":"user_one","role":"user","content":"Hi"},{"role":"assistant","content":[{"type":"output_text","text":"Hello","annotations":[],"logprobs":[]}]},{"role":"user","content":"Again"}],"instructions":"Be helpful","max_output_tokens":2,"temperature":0}'
  local response=ok(c:responses(json,sink));text()
  local streaming=false
  ok(c:response_input_items_list(response.id,function(s)sink(s);if table.concat(chunks):find('Hi')and r.in_scan and r.query_sent<r.query_total then streaming=true end end,'{"order":"asc","limit":100}'))
  local all=text();assert(streaming and all:find('user_one')and not all:find('Be helpful')and not all:find('logprobs'))
  ok(c:response_input_items_list(response.id,sink,'{"order":"asc","limit":100,"include":["message.output_text.logprobs"]}'));assert(text():find('"logprobs":%[%]'))
  ok(c:response_input_items_list(response.id,sink,'{"limit":1}'));assert(text():find('"has_more":true'))
  local input_record=r.records[key('R','response',response.id)];local saved_inputs=input_record.parts.response_input_items
  input_record.parts.response_input_items='[{"type":"reasoning","id":"rs_test","encrypted_content":"reason-secret","summary":[]},{"type":"compaction","id":"cmp_test","encrypted_content":"compact-secret"}]'
  ok(c:response_input_items_list(response.id,sink));local projected=text()
  assert(projected:find('compact%-secret')and not projected:find('reason%-secret'))
  ok(c:response_input_items_list(response.id,sink,'{"include":["reasoning.encrypted_content"]}'));assert(text():find('reason%-secret'))
  input_record.parts.response_input_items=saved_inputs
  if not memory then
    local two=ok(c:chat_completions(body,sink));text()
    ok(c:chat_list(sink,'{"limit":1}'));page=text();local cursor=assert(page:match('"last_id":"([^"]+)"'));assert(page:find('"has_more":true'))
    ok(c:chat_list(sink,'{"after":"'..cursor..'","limit":1}'));assert(text():find('"has_more":false'))
    local second=client();r.edit_hook=function()r.edit_hook=nil;fails('busy',function()return second:chat_update(one.id,'{"metadata":{}}',function()error('conflict emitted bytes')end)end)end
    ok(c:chat_update(one.id,'{"metadata":{"owner":"one"}}',sink));text();second:close()
    second=client();local switched=false
    ok(c:chat_update(one.id,'{"metadata":{"owner":"snapshot"}}',function(s)
      sink(s)
      if not switched then switched=true;ok(second:chat_update(one.id,'{"metadata":{"owner":"later"}}',function()end))end
    end))
    assert(text():find('"owner":"snapshot"')and r.records[key('R','chat',one.id)].parts.metadata=='{"owner":"later"}');second:close()
    local next_response=ok(c:responses('{"previous_response_id":"'..response.id..'","input":"Next","max_output_tokens":2,"temperature":0}',sink));text()
    ok(c:response_input_items_list(next_response.id,sink,'{"order":"asc","limit":100}'));local continued=text()
    assert(continued:find('user_one')and continued:find('Next')and not continued:find('Be helpful'))
    local old=r.records[key('R','chat',one.id)];local stored=old.parts.output;old.parts.output='{"id":"broken"'
    fails('io_failed',function()return c:chat_update(one.id,'{"metadata":{}}',function()error('uncommitted output')end)end);old.parts.output=stored
    assert(not next(r.active)and not next(r.reading))
    local missing=ok(c:responses('{"input":"Not stored","store":false,"max_output_tokens":2,"temperature":0}',sink));text()
    fails('not_found',function()return c:response_input_items_list(missing.id,function()end)end)
    local expired=r.records[key('R','chat',two.id)];expired.expires=1
    ok(c:chat_list(sink));local visible=text();assert(visible:find(one.id,1,true)and not visible:find(two.id,1,true));expired.expires=expired.identity.expires_at
    ok(c:chat_delete(two.id,function()end));ok(c:response_delete(next_response.id,function()end))
  end
  ok(c:chat_delete(one.id,function()end));fails('not_found',function()return c:chat_messages_list(one.id,function()end)end)
  ok(c:response_delete(response.id,function()end));c:close()
  assert(not next(r.active)and not next(r.reading)and not next(r.locks))
  local bare=ok(ailo.new{source={local_path=path},backend='cpu',lazy=true})
  local empty={};ok(bare:chat_list(function(s)empty[#empty+1]=s end));assert(table.concat(empty):find('"data":%[%]'));bare:close()
end
function M.replay_input_page(c,id)
  local chunks={};local v,e=c:response_input_items_list(id,function(s)chunks[#chunks+1]=s end,'{"order":"asc","limit":100}')
  assert(v,e and e.message)
  local input=assert(member(table.concat(chunks),'data'))
  v,e=c:responses('{"input":'..input..',"store":false,"max_output_tokens":2,"temperature":0}',function()end)
  assert(v,e and e.message)
  local changed,n=input:gsub('"id":"history%-9"','"id":"changed"',1);assert(n==1)
  local deliveries=0
  v,e=c:responses('{"input":'..changed..',"store":false,"max_output_tokens":2}',function()deliveries=deliveries+1 end)
  assert(v==nil and e.code=='invalid_argument'and deliveries==0)
end
function M.routes(ailo,path)
  local function ok(v,e)assert(v,e and(e.code..': '..e.message));return v end
  local function client(repository)
    return ok(ailo.new{source={local_path=path},backend='cpu',threads=1,context_tokens=512,lazy=true,
      openai={namespace_id='routes',repository=repository,chat_retention_seconds=60,max_record_bytes=32768}})
  end
  local r=M.record_repository();local c=client(r)
  local function call(method,target,body)
    local chunks={}
    local info=ok(c:handle(method,target,body,function(s)chunks[#chunks+1]=s end))
    return info,table.concat(chunks)
  end
  local json='{"model":"local-model","messages":[{"role":"user","content":"Hi"}],"stream":true,"max_completion_tokens":2,"temperature":0,"metadata":{"ticket":"one"},"stream_options":{"include_obfuscation":false}}'
  local created=call('POST','/v1/chat/completions',json)
  assert(created.stored and r.last_metadata.expires_at==r.last_metadata.created_at+60)
  local _,text=call('GET','/v1/chat/completions/'..created.id)
  assert(text:find('"object":"chat.completion"')and text:find(created.id,1,true))
  call('POST','/v1/chat/completions/'..created.id,'{"metadata":{"ticket":"a+b"}}')
  _,text=call('GET','/v1/chat/completions?metadata%5Bticket%5D=a%2Bb&limit=01&model=local%2Dmodel&order=asc')
  assert(text:find(created.id,1,true))
  _,text=call('GET','/v1/chat/completions/'..created.id..'/messages?limit=1')
  assert(text:find('Hi'))
  local response=call('POST','/v1/responses','{"input":"Hello","stream":true,"max_output_tokens":2,"temperature":0}')
  assert(response.stored and r.last_metadata.expires_at>=r.last_metadata.created_at+2592000)
  _,text=call('GET','/v1/responses/'..response.id);assert(text:find(response.id,1,true))
  _,text=call('GET','/v1/responses/'..response.id..'/input_items?include%5B%5D=reasoning.encrypted_content&limit=1')
  assert(text:find('Hello'))
  _,text=call('POST','/v1/responses/compact','{"model":"local-model","input":[{"role":"user","content":"Hi"}]}')
  assert(text:find('"object":"response.compaction"'))
  call('DELETE','/v1/chat/completions/'..created.id)
  call('DELETE','/v1/responses/'..response.id)
  local v,e=c:handle('GET','/v1/responses/'..response.id,nil,function()error('must not emit')end)
  assert(v==nil and e.code=='not_found'and e.info.http_status==404 and not e.info.started)
  local marker={router_sink=true}
  v,e=c:handle('GET','/v1/models',nil,function()error(marker)end)
  assert(v==nil and e.code=='sink_failed'and e.cause==marker and e.info.started)
  v,e=c:handle('PATCH','/v1/models',nil,function()end)
  assert(v==nil and e.code=='unsupported'and e.info.http_status==405 and e.info.allow=='GET')
  assert(not next(r.active)and not next(r.reading));c:close()
  r=M.conversation_repository();c=client(r)
  created=call('POST','/v1/conversations')
  local prefix='/v1/conversations/'..created.id
  call('GET',prefix)
  call('POST',prefix,'{"metadata":{"owner":"route"}}')
  call('POST',prefix..'/items','{"items":[{"id":"one+two","role":"user","content":"First"},{"id":"three","role":"user","content":"Second"}]}')
  _,text=call('GET',prefix..'/items?limit=1&order=asc');assert(text:find('First')and text:find('"has_more":true'))
  _,text=call('GET',prefix..'/items?after=one%2Btwo&limit=1&order=asc');assert(text:find('Second'))
  _,text=call('GET',prefix..'/items/one+two?include[]=message.output_text.logprobs');assert(text:find('First'))
  call('DELETE',prefix..'/items/one%2Btwo')
  call('DELETE',prefix)
  v,e=c:handle('GET',prefix,nil,function()end);assert(v==nil and e.code=='not_found')
  _,text=call('GET',prefix..'/items');assert(text:find('Second'))
  assert(not next(r.active)and not next(r.reading));c:close()
end
return M
