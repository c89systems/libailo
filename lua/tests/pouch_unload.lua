-- Opening a lazy receiver's default repository initializes the SDK logger.
-- runner.c closes the Lua state, unloading native modules before process exit.
-- No model is loaded and no clock, expiry wait or inference is involved.
local ailo=require("ailo")
local root=assert(arg[1])
local function ok(value,err) assert(value,err and err.message);return value end
for _=1,2 do
  local client=ok(ailo.new{
    source={local_path="/unused-pouch-unload-model.gguf"},lazy=true,
    openai={state_directory=root.."/pouch",encryption_key_file=root.."/keys/pouch.key"},
  })
  local bytes=0
  ok(client:chat_list(function(chunk)bytes=bytes+#chunk end))
  assert(bytes>0)
  local result,err=client:chat_list(function()error("unload sink failure")end)
  assert(result==nil and err and err.code=="sink_failed")
  assert(client:close() and client:close())
  client=nil
  collectgarbage("collect")
end
print("Model-free Pouch open/close passed; runner will unload native modules")
