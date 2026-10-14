-- Arguments: local GGUF, cpu|vulkan|auto, native log path.
local ailo=require("ailo")
local pslog=require("pslog")
assert(arg[1] and arg[3],"expected GGUF path, backend and log path")
local log=pslog.new_json(arg[3],{min_level="trace"})
local client,err=ailo.new{source={local_path=arg[1]},backend=arg[2] or "auto",logger=log,context_tokens=512}
assert(client,err and err.message)
local deltas=0
local info,failure=client:generate({items={{role="user",content="What is 2 + 2? Answer briefly."}},max_tokens=16,temperature=0},function(event)
  if event.kind=="text_delta" then deltas=deltas+1;io.write(event.data);io.flush() end
end)
assert(info,failure and failure.message)
assert(deltas>0 and info.usage.completion_tokens>0)
if arg[2]~="auto" then assert(info.actual_backend==arg[2]) end
io.write("\n");io.stderr:write("backend="..info.actual_backend.." deltas="..deltas.."\n")
client:close();log:close()
