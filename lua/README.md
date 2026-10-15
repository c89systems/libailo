# lua-ailo

Lua 5.5.1 receiver facade over the installed libailo shared C ABI. The native
module does not embed a second llama, libailo, Lua or pslog runtime.

```lua
local ailo = require("ailo")
local pslog = require("pslog")
local log = pslog.new_json("inference.log", { min_level = "debug" })
local client, err = ailo.new {
  source = { local_path = "model.gguf" },
  backend = "auto", logger = log,
}
assert(client, err and err.message)
local info, failure = client:generate({
  items = {{ role = "user", content = "Explain gravity briefly." }},
  max_tokens = 64,
}, function(event)
  if event.kind == "text_delta" then
    io.write(event.data)
    io.flush()
  end
end)
assert(info, failure and failure.message)
client:close()
log:close()
```

`new` copies configuration through the C constructor. Choose `source.local_path`
or explicit Hugging Face `repo`, `revision`, `filename`. `cache_root`, `sha256`
(dense array, false skips one checksum), `allow_download`, `split_shards`,
`bearer_token`, `max_bytes`, timeouts and attempts mirror the public C source.
No implicit downloads or token environment lookup. Default cache is
`$XDG_CACHE_HOME/ailo` or `$HOME/.cache/ailo`. Full unsigned 64-bit `max_bytes`
accepts `{hi=...,lo=...}`; other integers are checked exactly.

Constructor options also include `model_id`, `chat_template`, `lazy`, backend
`auto`/`cpu`/`vulkan`, `gpu_layers`, `context_tokens`, `batch_tokens`, `threads`
and an optional pslog `logger`. Defaults match the C factories. Unknown keys,
wrong types, holes and out-of-range values return `nil, error`.

Each client has a private prompt/KV cache (64 MiB/eight entries/300 seconds idle).
Set `disable_prompt_cache=true` to disable it, or attach an explicit owner:

```lua
local cache = assert(ailo.new_cache {namespace_id="trusted-tenant", max_entries=4})
local client = assert(ailo.new {source={local_path="model.gguf"}, cache=cache})
cache:close() -- client retains the underlying owner
local stats = assert(client:cache_stats())
assert(client:cache_clear())
client:close()
```

`new_cache` also accepts `max_memory_bytes` and `idle_ttl_seconds`; zero in any
RAM limit disables memory storage. Its `stats`, `clear`, `prune` and idempotent `close`
methods never download weights. Requests can supply `prompt_cache_key` to
partition entries; identity/configuration checks still apply. Statistics expose
hits/misses/evictions, cached/evaluated token counts, bytes and microseconds;
`usage.cached_tokens` counts actually skipped evaluation in the first candidate.
The default memory cache does not retain response records or create files.
For encrypted snapshots, add explicit disk limits and a 32-byte binary key:

```lua
local cache = assert(ailo.new_cache {
  namespace_id="trusted-tenant", key_id="deployment-key-1", key=secret_key,
  max_disk_bytes=256*1024*1024, disk_ttl_seconds=3600,
  max_disk_entries=8, max_snapshot_bytes=64*1024*1024,
  -- directory="/absolute/custom-cache/ailo/inference",
})
local client = assert(ailo.new {source={local_path="model.gguf"}, cache=cache})
assert(client:generate(request, function(event) consume(event) end))
assert(client:snapshot_export("warm-prompt"))
-- A reconstructed client/cache with the same config can import the slot:
assert(client:snapshot_import("warm-prompt", request.prompt_cache_key))
client:close()
cache:close()
```

Omit `directory` for `$XDG_CACHE_HOME/ailo/inference` or
`$HOME/.cache/ailo/inference`. Automatic reuse also survives reconstruction.
Slots share the disk quota and absolute TTL; export can fail after eviction.
Import authenticates and validates before seeding the cache, without replacing
the active native sequence. There is no conversation/generation resume here.
Key rotation uses a new `key_id`; old domains require their own prune/clear.
Lua key strings remain the caller's responsibility. Snapshot buffers are
explicitly materialized within `max_snapshot_bytes`; output deltas remain
immediate. See [snapshot semantics](../docs/client.md#encrypted-prompt-snapshots).

Explicit canonical history uses its own receiver:

```lua
local history = assert(client:new_continuation {max_items=1024, max_bytes=4*1024*1024})
assert(history:append {{role="system", content="Answer briefly.", pinned=true}})
local fork = assert(history:clone())
assert(client:generate({continuation=history, items={{role="user", content="Hello"}},
  max_tokens=32}, function(event) consume(event) end))
assert(history:visit(function(item) inspect(item) end))
local info = assert(history:info())
assert(info.item_count == 2) -- streamed output was never collected/appended
fork:close()
history:close()
```

Generation commits only supplied input after success, including final callback
acceptance. Failure/cancellation leaves history unchanged. Append generated
assistant messages or function calls explicitly if full replay is needed.
Pending tool groups may be stored, but their results must be complete before
generation. `clone` forks immutable prior items; appends diverge. Owners survive
client destruction and are separate from KV eviction. `visit` supplies copied
Lua item/content tables, so modifying them does not alter history. Reentry and
close during a callback return `busy`. The C ABI semantics and bounds are in
[continuation state](../docs/client.md#owned-continuation-state); this receiver
does not create retained OpenAI response records.

Requests also accept `reasoning="auto"` (default), `"enabled"` or `"disabled"`,
optional `reasoning_effort` and `max_reasoning_tokens`. Controls require a
qualified template; ignored effort or unsupported disabling fails before
generation. `reasoning_delta` events carry `item_kind="reasoning"`, a separate
item ID and the ordinary lifecycle/usage fields. Answer text and function
arguments keep their existing delta kinds. `usage.reasoning_tokens` is part of
the total completion count. The optional reasoning cap stops LENGTH while an
open block is incomplete, without fabricating an answer. Explicit history uses
`kind="reasoning"` with text content before its assistant message/call group.
See [reasoning semantics](../docs/client.md#reasoning-channels-and-controls) for
supported layouts, bounded carry and accounting details.

Requests contain ordered `items` (kind, role, id, call_id, name, arguments and
content), `max_tokens`, `candidates`, `temperature`, `top_p`, `seed`,
`frequency_penalty`, `presence_penalty`, `stop`, `max_output_bytes`, `output_format`
(`text`, `json_object`, `json_schema`), `json_schema` (JSON string), `logprobs`,
`top_logprobs` (0-20, requires logprobs), and `logit_bias` (a dense array of
`{token_id=...,bias=...}` with unique IDs and finite biases in [-100,100]).
Content is a string or a
dense array of `{kind="text",data="..."}` parts. Core unsupported kinds retain
the C `unsupported` outcome. Token payloads are never assembled into a complete
answer. Events have `kind`, monotonic `sequence`, zero-based `candidate`,
`model_id`, `data`, `finish_reason`, and `usage`. Kinds are `run_begin`,
`choice_begin`, `item_begin`, `content_begin`, `text_delta`, `content_end`,
`item_end`, `choice_end`, `usage`, `run_end`, `function_name_delta`,
`function_arguments_delta`, and optional `token`.
Every event has a stable `run_id`, also returned as `info.id`. Active items have
`item_id`, `item_kind`, `item_index`, and `content_index`; payload and identifier
strings are copied into that event's Lua table. No output history is retained.
Token events carry `token={token_id,data,logprob,top_logprobs,index,byte_offset,
end_of_generation}`; each alternative has token_id/data/logprob. Pieces are raw
bytes and may split UTF-8; text_delta stays valid UTF-8. Probabilities normalize
logits after bias/penalties/grammar and before top-p/temperature. A grammar may
allow fewer alternatives than requested. Token events include
stop/EOG tokens and arrive before their corresponding text delta. The default
sampled-byte budget is 16 MiB across candidates; exceeding it fails without a
successful terminal event. No whole answer is retained.

Constrained JSON output uses the C receiver's declared schema subset and limits;
see [the receiver contract](../docs/client.md#constrained-output).
For example, `output_format="json_schema",json_schema='{"const":{"answer":4}}'`
constrains the response while still emitting immediate `text_delta` events.
A `length` finish can leave incomplete JSON;
interruption by a stop sequence returns `model_failed`. No full JSON reply is
collected by this binding.

Completed tool histories use items with `kind="function_call"`, `call_id`,
`name` and `arguments` (a JSON-object string), followed by
`kind="function_result"`, matching `call_id` and text `content`. Calls may also
have an independent item `id`. Complete parallel result groups can arrive in a
different order. Incomplete groups, duplicate identities and orphan results fail
before events. A compatible Jinja `chat_template` is required; unsupported
templates fail instead of dropping fields. The library never executes functions.

For generation, supply `tools={{name="lookup",description="...",
parameters='{"type":"object"}',strict=true}}`. Parameter schemas are JSON
strings; strict mode uses the declared C schema subset. `tool_choice` is `auto`
(default), `none`, `required` or `named`; named also needs `tool_name`.
`allowed_tools={"lookup"}` restricts calls while keeping all definitions in the
prompt. `parallel_tool_calls` defaults true; false allows one call.
`max_tool_calls` is 0-128, with zero selecting 128 (one when parallel is false).
Limits are per candidate; named always allows at most one call.

The template must qualify for an unambiguous marked JSON envelope; unsupported
formats fail before events. Function items have `item_kind="function_call"`,
stable independent `item_id`/`call_id`, and a `name` prefix that is final at
`item_end`. `function_name_delta` supplies decoded name fragments;
`function_arguments_delta` supplies original JSON bytes immediately, before
the call or run completes. These events are not message content. A successful
call-producing choice reports `tool_calls`; length may leave a partial call.
Only execute a call after its successful completion.

`generate` is synchronous: the sink receives each available delta immediately,
including before the next decode. Only boolean false cancels; nil continues.
Sink errors (including attempted yields) return `sink_failed` with the original
`cause` and partial run `info`. Callbacks run on the invoking Lua thread and
must not yield. All Lua allocation/user code in native callbacks is protected
by pcall so errors cannot longjmp through native C++ frames. A separate client
may be used recursively; reentry/close on the active client returns `busy`.
`cancel()` is allowed inside a callback. Native methods remain single-operation
receivers.

`source.progress` optionally receives `{shard,shards,received,total}`; false
cancels, errors return `sink_failed`. Both eager construction and lazy loading
use the active Lua thread. No coroutine pointer is retained between operations.
`model_card()`, `capabilities()` and `devices()` return explicitly buffered JSON
strings from the C ABI. `generate()` returns usage, completed_candidates,
finish_reason and actual_backend (auto means unselected in failure info); failures return nil and a table with code,
status, message, and partial info where applicable. `close()` is idempotent;
GC destroys native producers before releasing the logger.

Logging uses the public libpslog Lua/C interop ABI1. Only live loggers positively
qualified for native sinks are accepted; Lua function/unknown outputs are
rejected before loading. The provider module is resolved through Lua's public
package.searchpath/loadlib facilities; private userdata layouts are never read.
The logger stays retained even after the constructing coroutine is collected.
Do not close borrowed files or explicitly close logger-owned outputs until the
client is closed. Trace/debug/info/warn/error native records carry component,
model_id, phase and continuation fields. Shared device diagnostics that cannot
be attributed to a receiver use the existing process-global fallback.

Build with CMake 3.24+, the installed libailo/pslog SDKs, Lua 5.5.1 SDK CMake
config, and the public pslog_lua.h interop header from lua-pslog 0.12.0. Supply
these prefixes in CMAKE_PREFIX_PATH and PSLOG_LUA_INCLUDE_DIR; select your target
toolchain explicitly. The rockspec exposes AILO_DIR, PSLOG_DIR, CPKTLUA_DIR and
PSLOG_LUA_INCDIR for the same dependency selection. Configure the normal system
library search path for the public shared libraries. Test provider code and the
local embedding runner are only built with AILO_LUA_TESTS=ON. ailo.core is an
internal normalized-input bridge; require("ailo") is the public facade.

In the main checkout, `make lua-test PRESET=cpu` builds and installs the facade
and its separate pslog test module against pinned Bootlin/cpkt inputs, then runs
the installed-layout tests. `PRESET=debug` selects the Vulkan-enabled core.
`make package-lua` produces a minimal source archive, release rockspec and source
rock; those products are separate from per-target C SDKs.

The CPU-only local memory gate checks all lost/reachable allocation kinds. A narrowly
matched suppression covers loader metadata and libstdc++'s emergency exception
pool retained after module loading (87,320 bytes in 22 blocks on the pinned
x86_64 Bootlin runtime). `tests/load_only.lua` reproduces exactly that baseline
without constructing a client or logger. The focused callback/finalizer/OOM suite
retains the same baseline, with zero lost allocations; Lua, libailo and pslog
allocation sites are not suppressed.

LuaRocks uses its per-rock lib/lua staging directories, supplied explicitly to
CMake by the rockspec. The direct CMake layout keeps the normal versioned Lua
search paths. LuaRocks tracks Lua's major/minor virtual package; CMake enforces
the exact 5.5.1 SDK contract. The local lua-rocks gate installs both packages
into an owned tree and loads their deployed modules with the pinned runner.
