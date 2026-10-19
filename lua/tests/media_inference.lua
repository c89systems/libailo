-- Explicit trained-model qualification; cache must already exist offline.
local ailo=require('ailo')
local cache,image,speech,backend=arg[1],arg[2],arg[3],arg[4] or 'cpu'
local function ok(v,e) assert(v,e and (e.code..': '..e.message));return v end
local function bytes(path)
  local f=assert(io.open(path,'rb'))
  local value=assert(f:read(16*1024*1024+1));f:close()
  assert(#value<=16*1024*1024);return value
end
local pairs={
  {kind='image',fixture=image,repo='ggml-org/SmolVLM-256M-Instruct-GGUF',
   revision='b9e4379657e1450d04d02eec8e345667265b0a00',
   model='SmolVLM-256M-Instruct-Q8_0.gguf',
   model_sha='2a31195d3769c0b0fd0a4906201666108834848db768af11de1d2cef7cd35e65',
   projector='mmproj-SmolVLM-256M-Instruct-Q8_0.gguf',
   projector_sha='7e943f7c53f0382a6fc41b6ee0c2def63ba4fded9ab8ed039cc9e2ab905e0edd'},
  {kind='audio',fixture=speech,repo='ggml-org/Qwen3-ASR-0.6B-GGUF',
   revision='928ab958557df9aa2ef1c93e0e83c7ad0933fae2',
   model='Qwen3-ASR-0.6B-Q8_0.gguf',
   model_sha='bca259818b50ca7c4c05e9bdb35a5dc04fa039653a6d6f3f0f331f96f6aa1971',
   projector='mmproj-Qwen3-ASR-0.6B-Q8_0.gguf',
   projector_sha='41a342b5e4c514e968cb756de6cd1b7be39eff43c44c57a2ef5fc6522e36603d'},
}
for _,pair in ipairs(pairs) do
  local function source(file,sha)
    return {repo=pair.repo,revision=pair.revision,filename=file,sha256={sha},cache_root=cache}
  end
  local payload=bytes(pair.fixture)
  local format=pair.fixture:match('%.([^./]+)$')
  if format=='jpg' then format='jpeg' end
  local resolved=0
  local client=ok(ailo.new{source=source(pair.model,pair.model_sha),backend=backend,threads=2,context_tokens=4096,
    multimodal={projector=source(pair.projector,pair.projector_sha),backend=backend,
      resolver=function(reference,emit)
        assert(reference.kind=='file_id' and reference.value=='fixture')
        resolved=resolved+1;assert(emit(payload));return true
      end}})
  local request={items={{role='user',content={
    {kind=pair.kind,data=payload,format=format},
    {kind='text',data=pair.kind=='image' and 'What is the main color? Answer with one color word.' or ''},
  }}},max_tokens=pair.kind=='image' and 16 or 128,temperature=0,seed=42}
  local output,deltas,finished={},0,false
  ok(client:generate(request,function(e)
    if e.kind=='text_delta' then
      assert(not finished);deltas=deltas+1;output[#output+1]=e.data
    elseif e.kind=='run_end' then finished=true end
  end))
  local text=table.concat(output)
  assert(deltas>0 and finished and text:lower():find(pair.kind=='image' and 'red' or 'country',1,true),text)
  assert(not text:find('<asr_text>',1,true) and text:sub(1,9)~='language ')
  request.items[1].content[1].data=nil
  request.items[1].content[1].file_id='fixture'
  local v,e=client:generate(request,function(event)if event.kind=='text_delta' then return false end end)
  assert(v==nil and e.code=='cancelled' and resolved==1)
  ok(client:close())
  print('Lua installed-ABI '..pair.kind..' semantics, streaming, binary parts, resolver and cancellation: passed')
end
