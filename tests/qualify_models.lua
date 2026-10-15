-- Manual hardware qualification; never part of the offline CTest suite.
-- Run through the installed Lua runner with: CACHE_ROOT OUTPUT_DIRECTORY [--download]
local ailo = require('ailo')
local cache, work = assert(arg[1]), assert(arg[2])
assert(arg[3] == nil or arg[3] == '--download')
local models = {
  {name='dense',repo='bartowski/SmolLM2-1.7B-Instruct-GGUF',
   revision='1f03464768bfcc0319fc50da8ff5fb20b6417ba2',
   filename='SmolLM2-1.7B-Instruct-Q4_K_M.gguf',size=1055609824,
   sha='77665ea4815999596525c636fbeb56ba8b080b46ae85efef4f0d986a139834d7'},
  {name='moe',repo='bartowski/granite-3.1-1b-a400m-instruct-GGUF',
   revision='940d2e1f9f65330615c7c8e980e6c5ac73d3360c',
   filename='granite-3.1-1b-a400m-instruct-Q4_K_M.gguf',size=821847360,
   sha='3a2ec1c2a78cb29d901e29bbf5162dcd03381e13803d2cbdcff838d4d08142eb'}
}
local function ok(value, err) assert(value, err and err.message); return value end
local function now()
  -- Linux monotonic uptime, 10 ms resolution; these are end-to-end timings.
  local f = assert(io.open('/proc/uptime')); local t = assert(f:read('n')); f:close(); return t
end
local function memory()
  local f = assert(io.open('/proc/self/status')); local text = f:read('a'); f:close()
  return assert(tonumber(text:match('VmRSS:%s+(%d+)'))), assert(tonumber(text:match('VmHWM:%s+(%d+)')))
end
local function save(name, data)
  local f = assert(io.open(work..'/'..name, 'w')); assert(f:write(data)); assert(f:close())
end
for _, model in ipairs(models) do
  local source = {repo=model.repo,revision=model.revision,filename=model.filename,
    sha256={model.sha},cache_root=cache,max_bytes=model.size,allow_download=false,
    connect_timeout_ms=15000,transfer_timeout_ms=300000,max_attempts=2}
  if arg[3] == '--download' then
    source.allow_download = true
    local watermark = 0
    source.progress = function(p)
      if p.received >= watermark then
        io.stderr:write(model.name..' acquired bytes '..p.received..'\n'); watermark = p.received + 67108864
      end
    end
    local fetched = ok(ailo.new{source=source,backend='cpu',context_tokens=256,threads=2})
    save(model.name..'-source.json', ok(fetched:model_card())); fetched:close()
    source.allow_download = false; source.progress = nil
  end
  local reference
  for _, backend in ipairs{'cpu','vulkan'} do
    io.stderr:write('QUALIFY '..model.name..' '..backend..' LOAD\n'); io.stderr:flush()
    local start = now()
    local client = ok(ailo.new{source=source,backend=backend,context_tokens=256,
      batch_tokens=128,threads=2,disable_prompt_cache=true})
    save(model.name..'-'..backend..'-card.json', ok(client:model_card()))
    local startup = now() - start
    local tokens, first_token, last_token, first_text, first_text_token = {}, nil, nil, nil, nil
    local deltas, ended = 0, false
    local request = {items={{role='user',content='Explain why the sky looks blue in two short sentences.'}},
      max_tokens=16,temperature=0,logprobs=true}
    io.stderr:write('QUALIFY '..model.name..' '..backend..' EVALUATE\n'); io.stderr:flush()
    start = now()
    local info = ok(client:generate(request, function(e)
      if e.kind == 'token' then
        assert(#tokens < 16)
        assert(e.token.logprob == e.token.logprob and math.abs(e.token.logprob) < math.huge)
        tokens[#tokens+1] = {id=e.token.token_id,logprob=e.token.logprob}
        first_token = first_token or now(); last_token = now()
      elseif e.kind == 'text_delta' then
        assert(not ended); deltas = deltas + 1
        first_text = first_text or now(); first_text_token = first_text_token or e.usage.completion_tokens
        io.stdout:write(e.data); io.stdout:flush()
      elseif e.kind == 'run_end' then ended = true end
    end))
    local elapsed = now() - start
    assert(info.actual_backend == backend and ended and deltas > 0 and #tokens > 1)
    assert(first_text_token < info.usage.completion_tokens, 'text was held until completion')
    local difference, common = 0, 0
    if reference then
      for i, token in ipairs(tokens) do
        if not reference[i] or token.id ~= reference[i].id then break end
        common = common + 1
        difference = math.max(difference, math.abs(token.logprob-reference[i].logprob))
      end
    else reference = tokens end
    local rss, high = memory()
    local prefill = first_token-start
    local decode = last_token-first_token
    local report = string.format('%s %s: startup=%.2fs first_text=%.2fs first_text_token=%d/%d prompt=%d prefill_inclusive=%.2ftok/s decode=%.2ftok/s elapsed=%.2fs rss=%dKiB process_hwm=%dKiB common_prefix_tokens=%d max_common_prefix_logprob_error=%.6g\n',
      model.name,backend,startup,first_text-start,first_text_token,#tokens,info.usage.prompt_tokens,
      info.usage.prompt_tokens/math.max(prefill,0.01),(#tokens-1)/math.max(decode,0.01),elapsed,rss,high,common,difference)
    io.stdout:write('\n'..report); io.stdout:flush()
    save(model.name..'-'..backend..'-metrics.txt',report)
    client:close()
    io.stderr:write('QUALIFY '..model.name..' '..backend..' COMPLETE\n'); io.stderr:flush()
  end
end
print('Pinned dense/MoE CPU and Vulkan streaming passed; use ailo_qualify_logits for identical-prefix numerical qualification')
