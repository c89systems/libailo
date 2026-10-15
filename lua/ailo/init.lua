local core = require("ailo.core")
local M = { version = core.version }
local methods = {}
local continuation_methods={}
local continuation_mt={__index=continuation_methods,__close=function(self)self:close()end}
local function wrap_continuation(native,err)
  if not native then return nil,err end
  return setmetatable({_native=native},continuation_mt)
end
local function invalid(message) error({code="invalid_argument",status=1,message=message}, 0) end
local function object(t, keys, name)
  if type(t) ~= "table" then invalid(name .. " must be a table") end
  for k in pairs(t) do if not keys[k] then invalid(name .. ": unknown field " .. tostring(k)) end end
  return t
end
local function keys(s)
  local out = {}; for k in s:gmatch("%S+") do out[k] = true end; return out
end
local function str(v, name, optional, bytes)
  if v == nil and optional then return nil end
  if type(v) ~= "string" or (not bytes and v:find("\0",1,true)) then invalid(name .. " must be a string without NUL") end
  return v
end
local function number(v, name, default, integer, low, high)
  if v == nil then return default end
  if type(v) ~= "number" or v ~= v or v == math.huge or v == -math.huge or
    (integer and math.tointeger(v) == nil) or v < low or v > high then invalid(name .. " is out of range") end
  return integer and math.tointeger(v) or v
end
local function boolean(v, name)
  if v == nil then return false end
  if type(v) ~= "boolean" then invalid(name .. " must be boolean") end
  return v
end
local function array(t, name)
  if type(t) ~= "table" then invalid(name .. " must be a dense array") end
  local n = #t
  for k in pairs(t) do if type(k) ~= "number" or math.tointeger(k) == nil or k < 1 or k > n then invalid(name .. " must be a dense array") end end
  for i=1,n do if rawget(t,i) == nil then invalid(name .. " contains a hole") end end
  return n
end
local function enumeration(v, map, default, name)
  if v == nil then return default end
  if not map[v] then invalid("invalid " .. name) end
  return map[v]
end
local function guarded(fn, ...)
  local result = table.pack(pcall(fn, ...))
  if not result[1] then
    local e = result[2]
    if type(e) ~= "table" or not e.code then e={code="invalid_argument",status=1,message=tostring(e)} end
    return nil,e
  end
  return table.unpack(result,2,result.n)
end
local source_keys=keys("local_path repo revision filename cache_root sha256 allow_download split_shards bearer_token max_bytes connect_timeout_ms transfer_timeout_ms max_attempts progress")
local config_keys=keys("source model_id chat_template backend lazy gpu_layers context_tokens batch_tokens threads logger cache disable_prompt_cache compactor")
local backends={auto=0,cpu=1,vulkan=2}
local function source(input)
  object(input,source_keys,"source")
  local out={}
  for _,k in ipairs{"local_path","repo","revision","filename","cache_root","bearer_token"} do out[k]=str(input[k],k,true) end
  out.allow_download=boolean(input.allow_download,"allow_download")
  out.split_shards=boolean(input.split_shards,"split_shards")
  if input.progress ~= nil and type(input.progress) ~= "function" then invalid("progress must be a function") end
  out.progress=input.progress
  for _,k in ipairs{"connect_timeout_ms","transfer_timeout_ms"} do out[k]=number(input[k],k,0,true,0,math.maxinteger) end
  out.max_attempts=number(input.max_attempts,"max_attempts",0,true,0,6)
  local b=input.max_bytes
  if type(b)=="table" then
    object(b,keys("hi lo"),"max_bytes")
    out.max_bytes={hi=number(b.hi,"max_bytes.hi",0,true,0,0xffffffff),lo=number(b.lo,"max_bytes.lo",0,true,0,0xffffffff)}
  else out.max_bytes=number(b,"max_bytes",0,true,0,math.maxinteger) end
  out.sha256={}
  if input.sha256 ~= nil then
    for i=1,array(input.sha256,"sha256") do
      local v=input.sha256[i]
      if v ~= false then v=str(v,"sha256",false) end
      out.sha256[i]=v
    end
  end
  return out
end
local function logger_helpers(logger)
  if logger == nil then return nil end
  require("pslog")
  local path,err=package.searchpath("pslog.core",package.cpath)
  if not path then invalid("pslog.core loader: " .. tostring(err)) end
  local out={}
  for _,name in ipairs{"check_logger","ref_logger","unref_logger"} do
    local fn,why=package.loadlib(path,"pslog_lua_"..name)
    if not fn then invalid("pslog interop: "..tostring(why)) end
    out[name]=fn
  end
  return out
end
function M.new(config)
  return guarded(function()
    object(config,config_keys,"client")
    local out={source=source(config.source),logger=config.logger}
    out.helpers=logger_helpers(config.logger)
    for _,k in ipairs{"model_id","chat_template"} do out[k]=str(config[k],k,true) end
    out.backend=enumeration(config.backend,backends,0,"backend")
    out.lazy=boolean(config.lazy,"lazy")
    out.gpu_layers=number(config.gpu_layers,"gpu_layers",0,true,-1,0x7fffffff)
    out.threads=number(config.threads,"threads",0,true,0,0x7fffffff)
    out.context_tokens=number(config.context_tokens,"context_tokens",0,true,0,0xffffffff)
    out.batch_tokens=number(config.batch_tokens,"batch_tokens",0,true,0,0xffffffff)
    out.disable_prompt_cache=boolean(config.disable_prompt_cache,"disable_prompt_cache")
    out.cache=config.cache
    out.compactor=config.compactor
    local native,err=core.new(out)
    if not native then return nil,err end
    return setmetatable({_native=native},{__index=methods,__close=function(self) self:close() end})
  end)
end
function M.new_cache(config)
  return guarded(function()
    object(config,keys("namespace_id max_memory_bytes max_entries idle_ttl_seconds directory max_disk_bytes disk_ttl_seconds max_disk_entries max_snapshot_bytes key_id key"),"cache")
    return core.new_cache{
      namespace_id=str(config.namespace_id,"namespace_id",false),
      max_memory_bytes=number(config.max_memory_bytes,"max_memory_bytes",64*1024*1024,true,0,math.maxinteger),
      max_entries=number(config.max_entries,"max_entries",8,true,0,4096),
      idle_ttl_seconds=number(config.idle_ttl_seconds,"idle_ttl_seconds",300,true,0,0xffffffff),
      directory=str(config.directory,"directory",true),key_id=str(config.key_id,"key_id",true),key=str(config.key,"key",true,true),
      max_disk_bytes=number(config.max_disk_bytes,"max_disk_bytes",0,true,0,math.maxinteger),
      disk_ttl_seconds=number(config.disk_ttl_seconds,"disk_ttl_seconds",0,true,0,0xffffffff),
      max_disk_entries=number(config.max_disk_entries,"max_disk_entries",8,true,0,4096),
      max_snapshot_bytes=number(config.max_snapshot_bytes,"max_snapshot_bytes",64*1024*1024,true,0,math.maxinteger)}
  end)
end
function M.new_compactor(config)
  return guarded(function()
    if config==nil then config={} end
    object(config,keys("namespace_id key_id key old_keys ttl_seconds max_item_bytes"),"compactor")
    local out={namespace_id=str(config.namespace_id,"namespace_id",true),key_id=str(config.key_id,"key_id",true),
      key=str(config.key,"key",true,true),ttl_seconds=number(config.ttl_seconds,"ttl_seconds",0,true,0,0xffffffff),
      max_item_bytes=number(config.max_item_bytes,"max_item_bytes",1024*1024,true,256,4*1024*1024),old_keys={}}
    if config.old_keys~=nil then
      local n=array(config.old_keys,"old_keys");if n>8 then invalid("too many old compaction keys")end
      for i=1,n do
        local old=object(config.old_keys[i],keys("key_id key"),"old compaction key")
        out.old_keys[i]={key_id=str(old.key_id,"key_id",false),key=str(old.key,"key",false,true)}
      end
    end
    return core.new_compactor(out)
  end)
end
local roles={user=0,system=1,developer=2,assistant=3,tool=4}
local kinds={message=0,function_call=1,function_result=2,reasoning=3,compaction=4}
local content_kinds={text=0,image=1,audio=2,refusal=3}
local request_keys=keys("items max_tokens candidates temperature top_p seed frequency_penalty presence_penalty stop logit_bias logprobs top_logprobs max_output_bytes output_format json_schema tools tool_choice tool_name allowed_tools parallel_tool_calls max_tool_calls prompt_cache_key continuation reasoning reasoning_effort max_reasoning_tokens auto_compaction")
local item_keys=keys("kind role id call_id name content arguments pinned encrypted_content")
local function compaction_policy(policy,automatic)
  if policy==nil then policy={} end
  object(policy,keys("target_tokens recent_turns output_reserve_tokens summary_context_tokens summary_tokens max_summary_bytes"..(automatic and " trigger_tokens max_passes" or "")),"compaction policy")
  local out={}
  out.target_tokens=number(policy.target_tokens,"target_tokens",0,true,0,math.maxinteger)
  out.recent_turns=number(policy.recent_turns,"recent_turns",2,true,0,4096)
  out.output_reserve_tokens=number(policy.output_reserve_tokens,"output_reserve_tokens",128,true,0,math.maxinteger)
  out.summary_context_tokens=number(policy.summary_context_tokens,"summary_context_tokens",0,true,0,32768)
  out.summary_tokens=number(policy.summary_tokens,"summary_tokens",256,true,1,4096)
  out.max_summary_bytes=number(policy.max_summary_bytes,"max_summary_bytes",32768,true,256,1024*1024)
  if automatic then
    out.trigger_tokens=number(policy.trigger_tokens,"trigger_tokens",0,true,0,math.maxinteger)
    out.max_passes=number(policy.max_passes,"max_passes",4,true,1,16)
  end
  return out
end
local function request(input)
  object(input,request_keys,"request")
  local out={items={},stop={},logit_bias={},tools={},allowed_tools={}}
  if input.auto_compaction~=nil then out.auto_compaction=compaction_policy(input.auto_compaction,true) end
  if input.continuation~=nil then
    if getmetatable(input.continuation)~=continuation_mt then invalid("continuation must be an ailo continuation") end
    out.continuation=input.continuation._native
  end
  local input_items=input.items
  if input_items==nil and out.continuation then input_items={} end
  local item_count=array(input_items,"items")
  if item_count>4096 then invalid("too many conversation items") end
  for i=1,item_count do
    local item=object(input_items[i],item_keys,"item")
    local v={kind=enumeration(item.kind,kinds,0,"item kind"),role=enumeration(item.role,roles,0,"role"),content={}}
    v.pinned=boolean(item.pinned,"pinned")
    for _,k in ipairs{"id","call_id","name","arguments","encrypted_content"} do v[k]=str(item[k],k,true) end
    if type(item.content)=="string" then v.content[1]={kind=0,data=item.content}
    elseif item.content ~= nil then
      for j=1,array(item.content,"content") do
        local p=object(item.content[j],keys("kind data"),"content part")
        v.content[j]={kind=enumeration(p.kind,content_kinds,0,"content kind"),data=str(p.data,"content data",false,true)}
      end
    end
    out.items[i]=v
  end
  out.max_tokens=number(input.max_tokens,"max_tokens",0,true,0,math.maxinteger)
  out.candidates=number(input.candidates,"candidates",0,true,0,0xffffffff)
  out.seed=number(input.seed,"seed",0xffffffff,true,0,0xffffffff)
  out.temperature=number(input.temperature,"temperature",0.8,false,0,math.huge)
  out.top_p=number(input.top_p,"top_p",0.95,false,0,1)
  for _,k in ipairs{"frequency_penalty","presence_penalty"} do out[k]=number(input[k],k,0,false,-2,2) end
  out.logprobs=boolean(input.logprobs,"logprobs")
  out.top_logprobs=number(input.top_logprobs,"top_logprobs",0,true,0,20)
  if out.top_logprobs>0 and not out.logprobs then invalid("top_logprobs requires logprobs") end
  out.max_output_bytes=number(input.max_output_bytes,"max_output_bytes",0,true,0,math.maxinteger)
  out.output_format=enumeration(input.output_format,{text=0,json_object=1,json_schema=2},0,"output_format")
  out.json_schema=str(input.json_schema,"json_schema",true)
  out.prompt_cache_key=str(input.prompt_cache_key,"prompt_cache_key",true)
  out.reasoning=enumeration(input.reasoning,{auto=0,disabled=1,enabled=2},0,"reasoning mode")
  out.reasoning_effort=str(input.reasoning_effort,"reasoning_effort",true)
  out.max_reasoning_tokens=number(input.max_reasoning_tokens,"max_reasoning_tokens",0,true,0,0x7fffffff)
  out.tool_choice=enumeration(input.tool_choice,{auto=0,none=1,required=2,named=3},0,"tool_choice")
  out.tool_name=str(input.tool_name,"tool_name",true)
  out.parallel_tool_calls=input.parallel_tool_calls==nil or boolean(input.parallel_tool_calls,"parallel_tool_calls")
  out.max_tool_calls=number(input.max_tool_calls,"max_tool_calls",0,true,0,128)
  if input.tools~=nil then
    local n=array(input.tools,"tools")
    if n>128 then invalid("too many function tools") end
    for i=1,n do
      local tool=object(input.tools[i],keys("name description parameters strict"),"function tool")
      out.tools[i]={name=str(tool.name,"tool name",false),description=str(tool.description,"tool description",true),
        parameters=str(tool.parameters,"tool parameters",true),strict=boolean(tool.strict,"tool strict")}
    end
  end
  if input.allowed_tools~=nil then
    local n=array(input.allowed_tools,"allowed_tools")
    if n>128 then invalid("too many allowed functions") end
    for i=1,n do out.allowed_tools[i]=str(input.allowed_tools[i],"allowed function",false) end
  end
  if input.logit_bias ~= nil then
    local n=array(input.logit_bias,"logit_bias")
    if n>4096 then invalid("too many logit biases") end
    local seen={}
    for i=1,n do
      local v=object(input.logit_bias[i],keys("token_id bias"),"logit_bias entry")
      local id=number(v.token_id,"token_id",nil,true,0,0x7fffffff)
      local bias=number(v.bias,"bias",nil,false,-100,100)
      if id==nil or bias==nil or seen[id] then invalid("logit biases need unique token IDs and finite biases") end
      seen[id]=true;out.logit_bias[i]={token_id=id,bias=bias}
    end
  end
  if input.stop ~= nil then for i=1,array(input.stop,"stop") do out.stop[i]=str(input.stop[i],"stop",false) end end
  return out
end
function methods:new_continuation(config)
  return guarded(function()
    if config==nil then config={} end
    config=object(config,keys("max_items max_bytes"),"continuation")
    return wrap_continuation(self._native:new_continuation{
      max_items=number(config.max_items,"max_items",4096,true,1,4096),
      max_bytes=number(config.max_bytes,"max_bytes",4*1024*1024,true,1,64*1024*1024)})
  end)
end
function continuation_methods:append(items)
  return guarded(function()return self._native:append(request{items=items})end)
end
function continuation_methods:clone()
  return wrap_continuation(self._native:clone())
end
function continuation_methods:visit(sink)
  return guarded(function()
    if type(sink)~="function" then invalid("sink must be a function") end
    return self._native:visit(sink)
  end)
end
for _,name in ipairs{"info","close"} do
  continuation_methods[name]=function(self)return self._native[name](self._native)end
end
function methods:generate(input,sink)
  return guarded(function()
    if type(sink) ~= "function" then invalid("sink must be a function") end
    return self._native:generate(request(input),sink)
  end)
end
function methods:chat_completions(json,sink)
  return guarded(function()
    if type(sink) ~= "function" then invalid("sink must be a function") end
    return self._native:chat_completions(str(json,"Chat Completions JSON",false,true),sink)
  end)
end
function methods:compact(input,policy)
  return guarded(function()
    local out=compaction_policy(policy,false)
    local window,info=self._native:compact(request(input),out)
    if not window then return nil,info end
    return wrap_continuation(window),info
  end)
end
function methods:snapshot_export(slot)
  return guarded(function()return self._native:snapshot_export(str(slot,"snapshot slot",false))end)
end
function methods:snapshot_import(slot,key)
  return guarded(function()return self._native:snapshot_import(str(slot,"snapshot slot",false),str(key,"prompt_cache_key",true))end)
end
for _,name in ipairs{"model_card","capabilities","devices","cache_stats","cache_clear","cancel","close"} do
  methods[name]=function(self) return self._native[name](self._native) end
end
return M
