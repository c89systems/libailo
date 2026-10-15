# Inference receiver

`include/ailo/client.h` provides the first implemented shared inference core.
It calls the pinned llama library in the embedding process. The
[Chat Completions adapter](openai.md) provides public JSON/SSE operations and
retained-record hooks. Responses and remaining handler work follow the broader
[backend contract](inference-backend.md).

Construct an `ailo_client` with `ailo_client_new`, then call its `generate`,
`model_card`, `capabilities`, `devices`, `cancel` and `destroy` methods.
Equivalent `ailo_client_*` functions are exported. Configuration strings and
checksum arrays are copied; callback user state remains borrowed until destroy.
Requests are borrowed until the synchronous call returns. Each callback borrows
its event and payload only until it returns.

The constructor uses the existing `ailo_model_source` acquisition facility,
including explicit download permission, checksums, sharded files, credentials,
progress, limits and configurable XDG caching. Lazy construction validates and
copies configuration without downloading or loading weights. Model cards and
generation force a lazy load; device and capability queries do not acquire files.

## GPU placement

Set `config.gpu` to an `ailo_gpu_config` with an ordered device-ID array and
optional matching layer weights. The constructor copies the configuration.
IDs come from `devices()` as `vulkan:<32 lowercase UUID hex digits>`; enumeration
indices and names such as `Vulkan0` are not accepted selection identities.
Device metadata includes UUID/driver UUID, vendor/product IDs, RADV/ANV provider,
PCI address when present, eligibility/reason and memory kind. Integrated memory
is shared, with no dedicated-VRAM claim; unavailable free-memory budgets are null.
CPU free memory is also null: the pinned native Linux backend estimates all
physical RAM as free, which is not an available-memory measurement.
UUIDs identify the device reported by the driver, not a permanently guaranteed
hardware serial number. Khronos documents possible changes after hardware moves
or driver changes; rediscover if a saved identity becomes unavailable. See
[Vulkan identity properties](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceIDProperties.html).

The default strategy is layer splitting. Empty selection uses eligible devices;
an explicit configuration requires GPU placement and disables automatic CPU
retry. Nonnegative finite weights require an explicit selection and matching
counts, with at least one positive value. They are normalized safely into the
native float tensor-split representation, including very large input weights;
unrepresentable positive values fail. Without weights the native free-memory
heuristic remains active. Native cumulative fractions allocate whole layers;
small shares may round to no layer. Every positive explicit participant must
receive a repeating or output layer and own device-resident model buffers,
otherwise loading fails. An assignment label alone does not satisfy explicit
placement. Zero-weight
devices need not participate. An unavailable, ambiguous or unusable selected
device fails before model acquisition; it is never silently substituted.

`AILO_GPU_SPLIT_NONE` selects one device; `AILO_GPU_SPLIT_ROW` returns unsupported
because the pinned Vulkan backend has no row-split buffer procedure. The raw
native facade keeps its upstream semantics. `backend=CPU` cannot accompany GPU
configuration. Only Mesa RADV and ANV are eligible receiver drivers.

Model cards include `placement`: requested/actual backend, ordered devices,
requested/normalized weights, per-layer device and buffer type, output device,
and native model/context/compute buffer allocations with a host-memory flag.
These are native assignments/allocations, not proof that every operation executes
on that device or a complete process/driver peak-memory measurement. Additional
driver memory is explicitly unknown. Actual placement and driver/device identity
also bind prompt/KV cache compatibility. The static driver integration and
multi-GPU hardware acceptance remain governed by [static-vulkan.md](static-vulkan.md).

Lua uses `gpu={devices={id1,id2},weights={3,1},split="layer"}` in `ailo.new`.
`split` defaults to `layer`; `none` and rejected `row` map to the C enum. Keep
the normal automatic backend selection by omitting GPU placement. Select
`backend="cpu"` explicitly for CPU inference.

## Generation

Supply ordered message items with UTF-8 text parts. The model's stored Jinja
template is interpreted by selected private llama helpers; named native
templates such as `chatml` and explicit overrides are also supported. Model
files and templates are trusted executable inputs: the interpreter is not a
sandbox. The rendered prompt has an 8 MiB acceptance limit, applied after
template execution. Request text is limited to 4 MiB, 4096 items and 4096 parts
per message. Embedded NUL text currently returns unsupported because the
private message boundary uses C strings.

Generation emits run begin, choice begin, item/content begin, text deltas,
content/item end, choice end, usage and run end. Every event has a stable run ID;
item/content events and deltas carry a stable item ID and zero-based item/content
indices. Run info owns its run ID. Candidate item IDs are distinct, and each run
uses a fresh random identity. Payload pointers remain borrowed for the callback.
Each delta goes to the sink before the next native token evaluation. There is
no retained complete answer, output spool or whole-answer JSON conversion.
Only an incomplete UTF-8 character and a possible stop-string prefix can delay
text. Four stop strings of up to 4096 bytes each are supported; matched stops
and subsequent bytes are excluded from output. The token-piece safety limit
is 1 MiB. A consumer can forward each delta directly to lonejson's incremental
string writer, as demonstrated in [examples/infer.c](../examples/infer.c).

The sink returns positive to accept, zero to cancel or negative to report sink
failure. Cancellation and failures return their status immediately, without a
synthetic successful terminal event. Run info retains usage observed so far.
Sampled EOS and stop-producing tokens count toward completion usage. Prompt
usage counts the input once; completion usage totals all candidates. Up to 16
candidates are generated sequentially, with independent sampler/context state.

Sampling supports greedy or temperature/top-p selection, seeds, frequency and
presence penalties, and at most 4096 unique token-ID logit biases in [-100,100].
Vocabulary IDs are validated before events/evaluation. Optional `logprobs`
emits a token event before its text delta, with the chosen probability and up to
20 alternatives. These probabilities normalize logits after bias, penalties and
any output grammar,
before top-p and temperature; greedy selection still reports model uncertainty.
Alternatives use descending probability, then token ID for ties. Token pieces
are arbitrary bytes, distinct from the UTF-8 text path. Per-candidate indices,
byte offsets and EOG flags let adapters exclude suppressed stop/EOG tokens from
user-visible content probabilities. Token events are bounded metadata, not a
retained transcript.

Text, reasoning and function-argument deltas carry `raw_offset_valid` and
`raw_offset`: their bytes occupy `[raw_offset, raw_offset + size)` in the sampled
byte stream. These coordinates match token `byte_offset`, include hidden
framing and stop bytes, reset per candidate and survive context rebuilds.
Decoded function names are metadata and have no source span. With `logprobs`
enabled, `AILO_EVENT_OUTPUT_PROGRESS` reports an exclusive `raw_offset`
watermark: all earlier source-mapped deltas have been delivered, and later
mapped deltas cannot start before it. Watermarks may repeat while bounded
UTF-8, delimiter or stop-prefix carry is unresolved; the final watermark
classifies all sampled bytes, including suppressed framing and stops. A sink
can release settled token metadata without retaining the reply. Adapters must
use these spans to associate visible bytes with actual sampled-token
probabilities, rather than matching substrings or exposing hidden framing as
content. A progress event is control metadata, not an empty text delta.

`max_output_bytes` bounds non-EOG sampled bytes across candidates, including
stop/UTF-8 carry; zero selects 16 MiB. Exceeding the budget fails without emitting
a successful terminal event. Cancellation/sink failures in token events stop
before their text delta and before evaluating another token.

Zero configuration selects auto backend, eager loading,
two CPU threads, up to 4096 context tokens and up to 512 batch tokens. Native
context padding can increase the effective context; the model card reports
requested and effective values separately. A request exceeding the effective
context fails before events or decoding. There is no silent input truncation.

CPU selection forces CPU model placement. Vulkan selection requires an eligible
built-in Vulkan device. Auto selects available Vulkan devices and retries on
CPU if GPU model/context setup fails. Native layer splitting uses all selected
Vulkan devices. Explicit per-device placement and full static RADV/ANV deployment
remain required work; this change does not qualify them.

One operation owns a client at a time. Concurrent calls and callback reentry
return busy. Cancellation is allowed from a callback or another thread during
an active operation. It is reset for the next operation. Join all calls before
destroying the receiver. Process backend ownership is shared with balanced
native-facade initialization; destroying one receiver preserves other owners.

## Model cards and current capabilities

Model cards, capabilities and devices return explicitly buffered, bounded owned
JSON strings. Free them with `ailo_string_free`. Cards report loaded GGUF/native
identity, source, license, architecture, parameter count, actual tensor types,
context limits, template fingerprint and file/checksum provenance. They do not
infer facts from filenames or expose local paths or credentials. Unknown facts
remain null. Publisher metadata enrichment is not yet implemented. Template
support means the named helper recognizes it or the interpreter can parse it;
rendering can still reject a conversation the template does not accept.

Capabilities currently advertise text, streaming, multiple candidates, logit
bias, log probabilities, constrained output, qualified tool generation and
bounded prompt/KV reuse, encrypted persistent snapshots and canonical
continuation state, qualified reasoning separation and authenticated compaction
replay and standalone semantic compaction, with their limits. Media remains unavailable.
Media items fail rather than losing semantics. Completed function histories are
supported as described below. Protocol handlers remain implementation requirements
in the full specification.

## Prompt/KV cache

Every client gets a private in-memory cache by default: 64 MiB, eight entries,
and 300 seconds idle expiry. `disable_prompt_cache` disables it. An explicit
`ailo_cache` receiver can be attached with `config.cache`; the client retains
its owner so the caller can destroy the shell afterwards. Explicit owners
require a trusted application `namespace_id`; there is no global tenant cache.
Use `ailo_cache_default_config()` then change bounds as needed. Zero memory,
entry or idle TTL limits disable memory storage. Expiry is checked on cache operations;
there is no background worker.

This cache stores prompt checkpoints, not generated answers or retained API
records. The default memory owner never writes files; cache operations never
delete model weights. `prompt_cache_key`
adds a request partition within the owner; it does not authorize access or
override identity. Identity binds actual model/shard bytes (verified digests
or hashes), tokenizer/template, native patches/build/settings, effective
context and actual device/layer placement. Loaded model files must remain
immutable for the client's lifetime, as required by native memory mapping.

Checkpoints stop before the last prompt token. A hit restores host-portable
sequence data into a disposable matching context and replays the final token
to obtain logits. Full-attention models can trim to a shorter common prefix;
sliding-window, hybrid and recurrent models reuse only a whole saved prefix,
never a rewind of partial recurrent state. A failed native restore is evicted
and recomputed using the unchanged live context. Busy shared owners are
bypassed. Samplers, grammar and output parsers are always fresh per candidate.

`usage.cached_tokens` reports skipped evaluation in the first candidate;
`prompt_tokens` still reports the complete logical prompt. Owner statistics
count cache operations and evaluated/cached tokens across candidates/clients,
with entries, charged memory, evictions, oversize/invalid/busy skips, lookup,
capture and total prefill microseconds. Timing components overlap; they are
not billing data. Read them through the cache receiver or client `cache_stats`;
`clear`/`prune` affect only KV entries. Client operations retain the normal busy
contract. Cache methods return busy on overlap; inference never waits for a
cache lock.

The memory charge includes serialized native buffers, tokens and entry metadata. Oversized
states are skipped before allocating their payload; eviction happens before a
new buffer is allocated. Native state serialization materializes one bounded
state buffer before generation and is not described as streaming export.
The disposable restore context temporarily coexists with the live context;
model/context allocations are separate from this cache budget. If allocation
fails, inference recomputes with its live context. Buffers and tokens are wiped
on eviction. Default cache hits/identity/trim/
expiry/limits/corruption fallback are tested on the deterministic full-attention
fixture; other architecture and GPU qualification remains separate.

The cached SmolLM2-135M instruct model was also checked through the installed
Lua ABI: a warm request reused 40 of 41 prompt tokens, evaluated only the final
prompt token, and matched the cold run's generated token IDs and log
probabilities (tolerance 1e-6). The serialized cache entry charged 923,207 bytes.

## Encrypted prompt snapshots

Persistence is opt-in on an explicit cache owner. Supply a 32-byte `key`,
`key_size=32`, nonempty `key_id`, nonzero `max_disk_bytes` and
`disk_ttl_seconds`. `max_disk_entries` defaults to eight (maximum 4096), and
`max_snapshot_bytes` defaults to 64 MiB. The latter bounds the combined
materialized tokens and native state of one snapshot. Zero RAM limits allow
a disk-only owner. Incomplete persistence configuration fails at construction.

`directory` is an absolute root; NULL selects `$XDG_CACHE_HOME/ailo/inference`
or `$HOME/.cache/ailo/inference`. A custom model-cache root does not implicitly
redirect it: supply that root's `inference` subdirectory explicitly. Files are
scoped by hashes of the namespace and key ID, use private 0700 directories and
0600 regular files, and reject symlinks and hard links. A nonblocking namespace
lock protects publication, eviction and reads across processes. Explicit
operations return BUSY on contention; generation can use RAM or recompute.

Captured prompt checkpoints are published automatically and reused across
cache/client reconstruction. `client->snapshot_export(client, slot, error)`
also saves the last successfully cached prompt to a named slot. A cancelled
request or rejected final event does not replace that reference. Export can
fail if its checkpoint was evicted. `snapshot_import(client, slot,
prompt_cache_key, error)` authenticates a named slot and validates its native
state in a disposable context, then seeds the owner for subsequent requests.
Import leaves the active sequence unchanged, even on success. Slots and
partitions are bounded UTF-8 strings of at most 256 bytes. Neither operation
resumes a transcript, sampler, generated reply or RNG state.

The envelope uses AES-256-GCM with random 96-bit nonces, separate HKDF-SHA256
derived keys for token metadata and native state, and authenticated identity,
partition, lengths, checksums, creation/expiry times, key ID and filename.
Tokens and native state are encrypted. Header counts, times and hashes are
visible. Payload authentication completes before tokens are used or native
restore is called; identity/configuration mismatches fail. An automatic bad or
expired entry is evicted and recomputed; explicit import reports the failure.
Copies into a different slot, namespace or key-ID domain cannot be replayed.
Key IDs provide rotation domains: a new ID does not read or clean an older
ID's files. Keep the old owner's key available to prune/clear that domain.
The constructor derives and owns its keys, erasing them on destruction; the
caller is responsible for its original key bytes, including immutable Lua strings.

Publication writes only encrypted temporary files, fsyncs, then atomically
renames and syncs the directory. Interrupted owned temporary files are pruned.
Named and automatic snapshots share the byte/count quota and absolute TTL;
newest publications are preferred, and reads do not extend lifetime. Publication
can temporarily use the disk budget plus one bounded encrypted snapshot
(256-byte header plus `max_snapshot_bytes`). Import/export materializes a
bounded state buffer; this is separate from the resident RAM cache budget and
live/disposable context allocations. Generation deltas still flow immediately.

`clear` removes memory entries and snapshots in the owner's namespace/key-ID
domain; `prune` enforces expiry and quotas. Statistics include snapshot reads,
writes, errors, skipped automatic publications, disk evictions, file count and
bytes. These slots are evictable performance data, not OpenAI retained response
records. Response storage and its retention contract remain separate required
adapter work.

The focused snapshot test covers reconstruction reuse, longer disk prefixes,
named round trips, tampered headers/ciphertext/tags, wrong keys/namespaces/key
IDs/partitions/configuration, truncation, bounded import, disk-only use,
contention, symlinks, quotas, expiry and last-successful-prompt behavior after
a sink failure. Failed imports preserve the existing native sequence byte for
byte. Lua exercises the same C ABI round trip and cleanup under Valgrind.

A separate-process check with the cached SmolLM2-135M instruct model exported
a named snapshot, exited, then imported it through a fresh installed Lua/C ABI
process. It reused 40 of 41 prompt tokens and matched the cold run's token IDs
and log probabilities within 1e-6. The automatic and named encrypted files
totaled 1,846,496 bytes. This qualifies the CPU full-attention path; GPU and
other architecture qualification remain separate.

## Owned continuation state

`ailo_continuation_new(client, config, &history, error)` creates a separate
receiver for explicit canonical input history; include
[`ailo/continuation.h`](../include/ailo/continuation.h). NULL config selects
4096 items and 4 MiB of charged canonical storage. Explicit limits must be
nonzero, at most 4096 items/64 MiB; all content parts together are capped at
4096. The existing 4 MiB combined text/arguments and supported-item validation
also apply. Unsupported media still fails. Reasoning history
requires a compatible template as described below.

Construction binds the owner to actual model/shard content and the effective
tokenizer/template, acquiring a lazy model if needed. The owner does not retain
the client or a KV cache. A compatible client can use it after the original
client is destroyed. Canonical items can be re-rendered with another context
size or backend; native cache/snapshot compatibility remains stricter. Changed
model content or template is rejected before generation events.

The receiver exposes `append`, `clone`, `info`, `visit` and `destroy`, with
equivalent free functions. Append copies ordered items, preserving IDs,
roles/names, original argument bytes, content-part boundaries and `pinned`.
System/developer roles represent protected instructions; an explicit pinned
flag records application protection for compaction. Ordinary rendering is
unchanged by this flag. Validation is atomic over the complete resulting
history. Duplicate IDs, orphan/duplicate results and interrupted pending groups
leave the prior history unchanged. A final unresolved tool group or partial
results may be stored between turns; generation requires all its results.

`clone` is the fork operation: owners initially share immutable items, then
diverge on append. Destroying either owner does not invalidate the other.
`info` reports item count, pending calls, charged bytes, identity and revision.
Each successful nonempty append increments the revision once; empty append
does not. `visit` borrows one item at a time, with positive/zero/negative sink
results meaning continue/cancel/failure. Every operation is try-only; overlap
or callback reentry on the same owner returns BUSY. Independent forks can be
used independently. Join all operations before destroying a C shell; Lua also
rejects explicit close while the owner is active.

Set `request.continuation=history` to generate from stored items followed by
the request's new items. New items are staged before inference and committed
only after the final run event is accepted. Preflight failure, cancellation,
model/sink failure or rejection of the final event leaves canonical state and
revision unchanged. Already-delivered deltas cannot be recalled. A request
without new items replays history without advancing its revision. Stateless
requests keep their existing behavior. Sampling controls, tools and other
request configuration remain request-scoped.

This operation never collects or appends generated output. The application or
protocol adapter explicitly owns any transcript collection, and appends its
completed assistant messages/function calls itself. A tool-call finish can
therefore be appended as a pending group before later results arrive. Input
history is explicitly materialized; generation deltas continue immediately.
Charged bytes include item/content records, text and metadata strings, not
allocator overhead, the model/native contexts or temporary validation views.
Staging temporarily retains old and new windows; immutable shared items avoid
duplicating the old payload. Canonical payload is wiped when its last owner
releases it. No background storage, files or automatic transcript collector is
introduced, and KV eviction/clearing does not remove history.

This supplies local canonical state, not an OpenAI response repository.
Retention, authorization, retrieval/deletion, top-level instruction inheritance
and `previous_response_id` semantics remain protocol adapter responsibilities.
The focused continuation test proves copied/forked ownership, pending parallel
tools, success/failure transactions, immediate output, busy reentry, compatible
client reconstruction/context resizing, incompatible templates and budgets.
Lua covers the installed C ABI, copied visitor values, callback errors,
cancellation and ownership under Valgrind.

The cached SmolLM2-135M instruct model was also exercised through the installed
Lua/C ABI. Its first run delivered four immediate text deltas; canonical history
contained only the system/user inputs until the proof explicitly appended the
assistant text. A replacement client with context enlarged from 256 to 512
continued the four-item history, while its fork retained only the original
instruction. Equivalent lowercase/uppercase verified model checksums produced
the same canonical identity. This is CPU integration evidence, not a model
quality benchmark or GPU qualification.

## Compaction key ownership

The key-owner foundation is available through
[`ailo/compaction.h`](../include/ailo/compaction.h). Authenticated opaque items
can be replayed through generation and canonical continuations, advertised by
`compaction_replay=true`. Standalone model-driven summary production is advertised
by `compaction=true`; explicit automatic preflight/in-flight compaction is
advertised as `automatic_compaction=true`.

`ailo_compactor_new(config, &owner, error)` creates an immutable receiver with
`info` and `destroy` methods. NULL configuration selects an ephemeral owner.
`ailo_compactor_default_config()` selects a 1 MiB decoded-envelope limit and
no automatic expiry. Explicit limits are 256 bytes through 4 MiB. Stable keys
require an application namespace, current key ID and exactly 32 key bytes;
up to eight distinct old decryption keys support rotation. Constructor inputs
are borrowed only during construction; owned derived keys are erased on final
release. The caller remains responsible for original key bytes.

Attach the owner through `client_config.compactor`. Clients retain the underlying
owner, so destroying its receiver shell does not invalidate them. Continuations
also retain the owner for authenticated append/clone validation. Without an
explicit owner, each client gets a separate private ephemeral key/namespace.
An explicit owner can be shared across clients; a replacement owner with a new
current key and old decryption keys is the rotation boundary. No key file or
record store is created. A zero TTL avoids silently imposing a lifetime on
future retained API records; response retention remains a separate contract.

The private codec uses purpose-separated HKDF-SHA256 and AES-256-GCM with a fresh
96-bit nonce. Its authenticated envelope binds item ID, namespace, key ID, model/
template identity, issue/expiry times and payload length. The encrypted payload
contains a summary and ordered retained-item fingerprints/cut boundary. Decryption
must authenticate before any payload field is parsed. Canonical unpadded base64url
and encoded/decoded length checks bound decoding. Retained-role/content/identity/
pinned changes, missing/reordered context and duplicate compaction items fail;
appending new items after the bound prefix and authorized replay/forks are allowed.
This is bounded compaction materialization, not a generated-reply collector.

The focused crypto test covers tampering, wrong keys/namespaces/models/IDs,
rotation, expiry, bounds, retained-context substitution/order/removal, duplicate
items, replay/appending, cancellation and owner lifetime. It is evidence for the
envelope foundation, not evidence that semantic summarization is implemented.

## Authenticated compaction replay

An `AILO_ITEM_COMPACTION` has required `id` and `encrypted_content` strings.
Role, content and function fields are zero/NULL. It is accepted only with the
compatible model/template, namespace and keys that issued it. Its canonical
owned form retains only the opaque token; `visit` never reveals the plaintext
summary. Appending the entire retained window is atomic, and replay failure
cannot commit a continuation update. Compatible explicit key owners support
fork/restart replay; a fresh default client cannot decrypt a prior client's
private token. Generation authenticates with the receiving client's own keys,
even when the continuation was originally created by another client.

After envelope authentication and retained-context binding, lonejson validates
the structured summary: at most 32 facts, each with bounded source index, original
kind/role/IDs/name, category and nonempty text up to 4096 bytes. Unknown/duplicate
fields, malformed JSON, invalid source indices and instruction-role provenance
are rejected. This is explicitly bounded summary materialization. The renderer
receives an assistant message labeled as untrusted historical context, preserving
the source metadata as data. It does not create a system/developer instruction.
A template that drops this context fails before generation events.

One focused integration test uses a signed deterministic scenario containing
the user goal `Finish ticket T-1.`. It proves provenance, byte-preserved retained
instructions, copied opaque storage, forks and client lifetimes, key isolation,
malformed signed summaries and immediate first-token cancellation/recovery.
The same generated fixture runs through the installed shared C ABI in Lua,
including the Valgrind ownership gate. These tests qualify replay and streaming,
not model-driven summary quality or automatic compaction.

## Standalone semantic compaction

`client->compact(client, request, policy, &window, &info, error)` returns a new
owned continuation containing retained items and one opaque summary item. Use
`ailo/compaction.h` for the policy/info types and free-function form
`ailo_client_compact`. The input continuation is unchanged on success and failure.
A window already below its target produces an independent no-op window. Both
outputs are cleared on failure. Destroy the result with `ailo_continuation_destroy`.

The default policy retains two recent complete user turns and reserves 128 main-context
tokens for output. Its target is 75% of the remaining main context. It allows
256 summary generation tokens and 32 KiB of answer/normalized summary bytes, in
a separate context bounded by `min(main context, 32768)` tokens. Explicit summary
contexts span 128..32768 tokens and output budgets span 1..4096 tokens, strictly
smaller than that context. Summary byte limits span 256 bytes through 1 MiB.
`target_tokens`, `recent_turns`, `output_reserve_tokens`, `summary_context_tokens`,
`summary_tokens` and `max_summary_bytes` are configurable through the policy.
Use the factory for defaults; unlike the target/context selectors, zero is a
literal recent-turn count or reserve. Thresholds that cannot fit fail explicitly.

Complete older user/assistant groups are eligible. Instructions and their groups,
pinned groups, recent turns, incomplete turns and pending tool-call dependencies
remain byte-preserved in canonical order. Parallel call/result groups are never
split. A previous opaque summary is authenticated and included as historical
data when replacing it; exactly one new token enters the result, while the old
window remains replayable. Authenticated facts retain their original roles and
identities through replacement; their carrier does not turn them into assistant
claims. Incomplete turns do not consume the recent-complete-turn allowance.
A pinned old token prevents replacement explicitly.
Tools and rendering controls on the request determine exact before/after token
counts. They remain caller-owned and must accompany later generation as usual.

The summary context shares the already-loaded weights and uses independent KV,
sampler and constrained JSON generation. It observes client cancellation and
routes logging through libpslog with the compaction phase. Summary work never
replaces the main context or its prompt checkpoint. The model generates only
source-indexed facts, categories and text; libailo reconstructs role/kind/identity
provenance from eligible original items. Invalid or protected source references,
malformed/oversized summaries, exhausted token budgets, missing task/source text
in the template, lack of real token reduction and unmet targets all fail without
adopting a window. Eligible input larger than the bounded single summary pass
fails with a context error; there is no hidden truncation or retry loop.

`info` reports changed/no-op, rendered before/after tokens, retained/removed source
item counts, summary prompt/completion tokens and pass count. Summary generation
is explicitly bounded materialization, distinct from streaming ordinary replies.
Model quality is model-dependent. The deterministic one-layer compaction fixture
qualifies selection, goal/identifier preservation, authenticated replay, original
cache preservation, replacement/forks, pending parallel tools, cancellation and
policy/work/byte/context failures through C89/Lua. It is not a general summary
quality benchmark. Protocol compact endpoints remain separate implementation work.

A trained CPU integration case also passed through the installed Lua/shared C
ABI with the pinned Qwen2.5-0.5B-Instruct Q4_K_M model used in the tool-generation
proof above (SHA-256 `6eb923e7d26e9cea28811e1a8e852009b21242fb157b26149d3b188f3a8c8653`).
Compaction reduced 418 rendered input tokens to 153 using 525 summary prompt
tokens and 70 summary completion tokens. A subsequent question through the
opaque window answered `T-1` in three text deltas, starting at completion token
1 of 4. The manual proof's answer collector was explicitly bounded to 64 tokens
and 1024 bytes; ordinary library generation still collects no reply. This is one
trained-model semantic/streaming integration observation, not a general quality
or performance qualification.

## Automatic preflight and in-flight compaction

Generation accepts an explicit `request.auto_compaction` pointer to an
`ailo_auto_compaction_policy` from `ailo/compaction.h`. NULL leaves ordinary
generation unchanged. An owned continuation is required. Standalone `compact`
rejects this generation-only option. Use `ailo_auto_compaction_default_policy()`
for the standalone policy defaults plus four passes; zero trigger selects main
context minus the output reserve. Explicit policies require a nonzero reserve,
`target < trigger <= context - reserve`, and 1..16 passes. Invalid policies and
model-specific sampling errors fail before summary inference/events.

The trigger counts actual rendered input and unfinished native output tokens.
Large input is compacted before generation; when generation crosses the trigger,
the backend compacts eligible canonical input, rebuilds a disposable context and
continues. A bounded token-ID window replays unfinished output into the model
without re-emitting deltas. Sampler/RNG/grammar, stop/UTF-8 carry, reasoning and
tool/JSON parsers stay alive, with stable run/item/call IDs. The input target is
tightened when needed to leave space for the protected unfinished output.
Impossible protected spans, exhausted passes or non-reducing summaries fail
explicitly. No full generated reply is collected or spooled. Output budgets may
exceed a single context only with this explicit policy; a protected unfinished
span still cannot grow beyond its bounded native window.

`AILO_EVENT_COMPACTION` carries borrowed `compaction` work metadata and the complete
`compacted_items` canonical INPUT window (including its opaque item). It is a
control event; it does not restart the active output item. Preflight emits it
after RUN_BEGIN, before CHOICE_BEGIN; in-flight emits it between token steps.
The staged canonical window is committed only after the final RUN_END sink
acceptance. Cancellation, sink failure, later context failure or failed rebuild
leaves the caller's original continuation unchanged; already-emitted deltas and
control events cannot be recalled.

Successful adoption also retains the issuing client's key owner with the new
canonical window. Later append remains valid after that client is destroyed;
the old client does not gain authority to decrypt a different private issuer's
token. Failed adoption preserves the old key owner along with the old history.

Usage separates `compaction_prompt_tokens`, `compaction_completion_tokens`,
`compaction_passes` and `context_replay_tokens` from ordinary generation tokens.
`prompt_tokens` and `cached_tokens` describe the initial effective generation
prompt, after any preflight compaction. Control metadata also reports actual
`replayed_tokens` and `reused_tokens` for the rebuild. Prefix cache matching uses
the new rendered tokens; unrelated entries follow normal quota/TTL policy without
an explicit whole-cache clear. Later candidates
start with the latest staged canonical input and retain independent samplers.

One local integration test covers preflight and three-token in-flight checks,
transactional cancellation/sink failure, bounded/pass-exhaustion errors,
independent candidate RNG streams, constrained JSON and checkpoints inside tool
arguments and reasoning, withheld stop prefixes and issuing-key ownership after
client destruction. These are deterministic protocol/state tests; trained
standalone summary quality remains model-dependent.

## Reasoning channels and controls

Generation separates one initial reasoning block where template probes establish
an unambiguous `<think>...</think>` or `[THINK]...[/THINK]` layout. Probes use the
actual Jinja renderer, including tool definitions and requested effort. Delimiters
include inferred suffix whitespace, at most 128 ASCII bytes each; whitespace
before a closing tag remains reasoning content. The closing tag commits the
grammar to its separator and answer instead of returning to reasoning. A
generation prefix can already contain the opening delimiter, or the eager
grammar generates it. Unsupported reasoning layouts fail before events when
reasoning is requested or the template advertises one; ordinary text templates
retain their existing behavior. This is conditional template support, not a
claim that every model can reason or obey every control.

`request.reasoning` selects AUTO (zero), ENABLED or DISABLED. AUTO enables and
separates a qualified initial block. DISABLED passes the template's thinking
control as false and requires an effective changed prefix for reasoning models;
mandatory-thinking templates cannot silently ignore it. `reasoning_effort`
accepts `none`, `minimal`, `low`, `medium`, `high`, `xhigh` or `max` only when the
template actually consumes the effort control. The native effort/strength
aliases receive the requested label. This does not emulate effort through
temperature, promise provider-equivalent computation, or claim every level is
meaningful to every model. Contradictory disabled/effort/budget settings reject.

`max_tokens` includes all newly sampled reasoning, delimiters and answer tokens.
Optional `max_reasoning_tokens` is a per-candidate hard bound; zero leaves only
the total limit. Exhaustion while a reasoning block remains open ends LENGTH
with an incomplete reasoning item. It does not force a closing message, invent
an answer or summarize a trace. `usage.reasoning_tokens` counts sampled tokens
beginning in the reasoning phase, including its delimiter tokens. A token
crossing into answer text counts once there; already-prefilled opening tokens
are prompt tokens. Counts aggregate candidates as completion usage does and
remain local accounting rather than provider billing claims.

Reasoning has its own item ID (`rs_...`), item/content lifecycle and immediate
`AILO_EVENT_REASONING_DELTA`; answer text remains `AILO_EVENT_TEXT_DELTA` in a
separate message item, and generated function calls retain their own IDs. Raw
local reasoning is not relabeled as a supported summary or encrypted provider
state. UTF-8 and delimiter carry are bounded; no complete generated item is
retained. Eager GBNF composes the reasoning phase with the existing answer/tool
grammar without a lazy trigger buffer or full-output parser. Constrained JSON
and function arguments apply after reasoning closes. Independent parser and
sampler state are created for each candidate. Token logprob events retain their
raw sampled byte offsets; channel deltas determine user-visible spans.

Byte quotas include the complete sampled reasoning/answer/syntax bytes. An
explicit stop interrupting an open reasoning block reports a constrained-output
failure; normal length truncation remains an incomplete LENGTH result. Invalid
or incomplete UTF-8 fails, and a partial delimiter is not leaked as text.
Cancellation, sink errors and backpressure remain synchronous and immediate.

Explicit reasoning input uses `AILO_ITEM_REASONING`, default/assistant role and
text content, immediately before its assistant message or function-call group.
It may have an independent item ID and pinned flag. Empty completed reasoning
is valid; a dangling or interrupted item is not. Rendering preserves the supplied
reasoning through the template's reasoning field and preservation controls;
templates that discard it reject the history. Canonical state stores the original
typed items independently of prompt rendering. No hidden generation transcript
is appended to continuation state.

One focused CTest covers split delimiters/UTF-8, incremental delivery of a 1 MiB
reasoning body, recursive answer-grammar composition, actual native inference
through reasoning into constrained JSON and named tools, multiple candidates,
usage, disabling, budget truncation, effort consumption, explicit history,
empty history, stop/byte limits, sink failure/cancellation and recovery. The
deterministic fixture adds one vocabulary token in a separate model variant;
existing pinned fixture bytes remain unchanged. Lua uses the same installed
C ABI, including reasoning callback recovery under Valgrind.

A controlled CPU integration check used
[Qwen3-0.6B Q4_K_M](https://huggingface.co/bartowski/Qwen_Qwen3-0.6B-GGUF/blob/f9184d8b4c371cb52af5b8cb3d38e55e3cf0f4a0/Qwen_Qwen3-0.6B-Q4_K_M.gguf),
acquired through libailo's own Hugging Face facility: revision
`f9184d8b4c371cb52af5b8cb3d38e55e3cf0f4a0`, SHA-256
`9acfc1e001311f34b4252001b626f2e466d592a42065f66571bff3790d4e1b14`.
The installed Lua/C ABI delivered the first reasoning delta at completion token
3, then correctly returned LENGTH at a 16-token reasoning cap without an answer.
A separate closing-tag/EOG-biased case exercised an empty completed reasoning
block followed by constrained JSON; disabling reasoning produced JSON with zero
reasoning tokens and its first answer delta at token 1. These deliberately bounded
protocol checks used a constant-answer schema and sampled decoding; they are
not quality benchmarks or evidence of a natural complete reasoning answer.
A preceding natural 128-token attempt remained in reasoning at its limit.

## Conversation and function history

Input supports participant names and complete function-call/result histories
through compatible Jinja templates. A function-call item carries an independent
item ID, call ID, function name and original JSON-object argument string. A
result refers to the call ID and supplies text content; its optional name must
match that call. Call/result roles may be zero (implied by kind) or explicitly
assistant/tool. Participant and function names use 1-64 ASCII letters, digits,
underscores or hyphens; IDs are UTF-8 strings of 1-256 bytes when supplied.
Argument bytes count toward the shared 4 MiB input-text budget.

Adjacent calls form one assistant group, including any preceding assistant
message text. Every call needs exactly one result before another message or
generation. Results may arrive in a different order, with their call identities
preserved. Duplicate item/call IDs, orphan/duplicate results, mismatched names,
interrupted or unresolved groups and malformed arguments fail before inference
events. Each request is independently validated; failures do not replace history.

Named native text templates cannot represent these richer fields and reject
them. Jinja rendering uses the selected pinned capability helper to choose
object/string arguments and check parallel-call representation. Actual rendering
tracks field use and rejects templates that discard supplied participant names,
call names/arguments or nonempty message/result content. A template can format
arguments for its model, while the caller's original input bytes stay unchanged.
For parallel-call formats that omit explicit result IDs, the prompt presents
results in the original call order so the model cannot swap their association.
The caller's ordered items stay unchanged; explicit-ID formats retain that order.
The core never executes a function.

## Generated function calls

`tools` supplies up to 128 unique `ailo_function_tool` definitions, with names,
optional descriptions and parameter-schema JSON. Each schema/description is at
most 64 KiB; definitions total at most 4 MiB. Original schema annotations remain
in the prompt. `strict` compiles the published subset into argument constraints;
non-strict schemas guide the model while permitting any JSON-object arguments.
The callable `allowed_tools` subset leaves every definition in the prompt.
Unknown/duplicate subset names and contradictory controls fail before inference.

`tool_choice` is auto, none, required or named; named requires `tool_name` and
returns at most one call. Required generates at least one call when generation
finishes normally. `parallel_tool_calls` defaults true in the factory; false
allows one call per candidate. `max_tool_calls` bounds each candidate at 1-128
(zero selects 128, or one when parallel calls are disabled).

Compatible Jinja templates are qualified from small rendered text/call probes,
including a parallel probe when enabled. Qualification requires an unambiguous
literal opening marker and JSON call envelopes: flat, function-wrapped,
function-name-keyed or array-wrapped. Unsupported/tagged argument formats and
templates that discard tool definitions fail before events. This qualification
establishes format fidelity, not the model's ability to choose useful functions.

An eager grammar reserves the opening marker for constrained call syntax and
bounds the number of calls. It never uses the native lazy grammar's retained
pre-trigger output. A public lonejson push visitor validates the envelope and
object arguments. Name/ID metadata is bounded; argument bytes flow directly,
unchanged, through `FUNCTION_ARGUMENTS_DELTA` as they arrive, with at most 4 KiB
chunks and a partial UTF-8 character as carry. No entire call is collected.

Each function item has stable, independent receiver-owned `item_id` and `call_id`.
`ITEM_BEGIN` precedes decoded `FUNCTION_NAME_DELTA` and original JSON argument
deltas; `ITEM_END` carries the final name. Function items have no message content
events. Text between calls remains ordinary message items, indexed in output
order. Names can be partial until item end; consumers must never execute a call
before successful completion. A normal choice containing calls finishes with
`TOOL_CALLS`. Length exhaustion may leave a partial call and reports `LENGTH`;
an unfinished forced opening marker is not exposed as answer text. Cancellation,
sink failure, quota exhaustion and an interrupted/invalid envelope cannot produce
a successful run end. JSON response formats constrain the ordinary answer branch
separately from function argument schemas.

A trained CPU check used
[`bartowski/Qwen2.5-0.5B-Instruct-GGUF`](https://huggingface.co/bartowski/Qwen2.5-0.5B-Instruct-GGUF/blob/c6f6d6134e86f8c53897f3d892b3a3da0143e8bc/Qwen2.5-0.5B-Instruct-Q4_K_M.gguf),
revision `c6f6d6134e86f8c53897f3d892b3a3da0143e8bc`, Q4_K_M file SHA-256
`6eb923e7d26e9cea28811e1a8e852009b21242fb157b26149d3b188f3a8c8653`.
Libailo acquired it through its Hugging Face cache facility. The installed Lua
facade generated a named strict call through the stored template: five argument
deltas, first at completion token 12 of 18. Replaying that call and its result
produced 25 immediate text deltas. This proves that model/template integration;
it is not a qualification of every model, tool format or GPU backend.

## Constrained output

`output_format` selects `AILO_OUTPUT_TEXT` (default),
`AILO_OUTPUT_JSON_OBJECT`, or `AILO_OUTPUT_JSON_SCHEMA`. The last requires
`json_schema`, a UTF-8 JSON document of at most 64 KiB. Validation/compilation
happens before generation events or prompt evaluation. Native grammar filtering
constrains each sampled token; a push-fed lonejson writer validates emitted JSON
incrementally using a discard sink. The text sink still receives every available
delta immediately, without a retained answer or a second serialization pass.
This path preserves special-token text matched by the grammar; EOG remains a
structural terminator. Suppressing matched text could invalidate a constant.

The accepted schema subset is explicit: object schemas with `type`,
`properties`, `required`, boolean/schema `additionalProperties`, arrays with
`items`/`minItems`/`maxItems`, scalar/null types, `enum`, `const`, `anyOf`, and
acyclic same-document `$ref` (including escaped JSON Pointer names) using
`$defs`/`definitions`. Nullable type arrays are supported. Structural keywords
require an explicit corresponding object/array type. Required names must have
property schemas. `anyOf` and `$ref` allow only annotation siblings; `enum` and
`const` allow only a single type name alongside annotations.
Type-array/enum combinations are rejected. `title`, `description`, `default`
and `examples` are annotations; recognized dialect declarations are draft
2020-12 and draft-07. Other dialects fail explicitly.

Schema limits are 4096 expanded nodes, 32 levels, 256 properties per object,
128 enum values and 32 alternatives. Array size bounds are integers in
[0,4096]. Inclusive `minimum`/`maximum` apply to integer schemas only, with
integer bounds within +/-10^15. Floating-point bounds, exclusive bounds,
string lengths/patterns/formats, `oneOf`, `allOf` and unknown constraints are
rejected rather than ignored. Capabilities and cards publish the keyword set
and limits. Plain number/string/boolean/null schemas remain supported.
The pinned helper's primitive integer/fraction/exponent components use up to
16 digits; exact `const`/`enum` literals can be longer. This is part of the
current generation subset, not an arbitrary-precision numeric guarantee.

Successful `stop` completion has complete validated JSON. `length` denotes an
incomplete generation and does not promise schema-complete output. Cancellation,
sink failure and byte-budget exhaustion cannot claim success. Matching a stop
sequence during constrained output returns `model_failed` without a successful
terminal event, because removing those bytes can invalidate the schema. The next
request starts with independent grammar/parser state, including each candidate.

Output validation permits 64 nested levels, 1024 decoded bytes per object key
and 1024 bytes per number token. Total/string bytes follow the output-byte
budget; parser-owned live allocation is capped at 4 MiB. These are resource
limits, independent of a schema's accepted values.

## Usage and verification

Build with `make build PRESET=cpu`, then run:

```sh
build/cpu/ailo_infer_example --local /absolute/model.gguf --backend cpu \
  --prompt 'Hello' --tokens 32
```

The example writes one JSON string incrementally through lonejson and flushes
each sink write. An error after output begins leaves an incomplete JSON value
and a nonzero exit status. `--card` prints the model card instead. Use `--help`
for Hugging Face source and cache options.

Deterministic static/shared C89 tests prove first-token delivery through
lonejson before token two, lifecycle ordering, multiple candidates, cross-token
stops, cancellation, sink errors, busy/reentry and independent backend owners.
Additional tests cover cross-thread cancellation, callback exception recovery,
stored Jinja semantics, EOS termination/usage, split UTF-8 and immediate delivery
of long output.
Installed C89 and C++ consumers execute generation through CMake/pkg-config.

A real CPU smoke run used `bartowski/SmolLM2-135M-Instruct-GGUF`, revision
`09816acd5d99df7be770d85ea30822623dab342c`, file
`SmolLM2-135M-Instruct-Q4_K_M.gguf`, SHA-256
`2e8040ceae7815abe0dcb3540b9995eaa1fa0d2ca9e797d0a635ae4433c68c2d`.
The receiver acquired and validated the 105,454,432-byte file under an owned
`build/` cache, generated through the stored Jinja template, and reopened the
cached model offline to obtain its card. This proves the integration, not model
answer quality or GPU qualification. Routine tests require no network download.

The same cached instruct model also generated successfully through a fully
static x86_64 musl executable. `readelf -l` showed no interpreter and `readelf -d`
reported no dynamic section. The build used the pinned musl Bootlin compiler
and cpkt/cpktmisc/lonejson/pslog static archives, with no host target libraries:

```sh
cmake -S . -B build/cpu-musl-static -G Ninja \
  -DAILO_TARGET_ID=x86_64-linux-musl -DAILO_VULKAN=OFF \
  -DAILO_BUILD_SHARED=OFF -DAILO_BUILD_TESTS=OFF -DBUILD_TESTING=OFF \
  -DAILO_INSTALL=OFF -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXE_LINKER_FLAGS=-static
cmake --build build/cpu-musl-static --target ailo_infer_example --parallel 2
```

This is CPU executable evidence; it does not qualify static GPU inference or
replace the required release matrix. Dependency preparation must precede this
direct CMake invocation.

The private Jinja helper uses the MIT-licensed nlohmann JSON implementation
already vendored in the pinned llama source. It is restricted to request/template/
schema handling; generated text passes directly through the event sink. Public JSON
serialization uses the separate pinned lonejson SDK. Template/native/compiler
runtime archives remain separate from `libailo.a` in the installed static SDK.

## Logger ownership

The additive `ailo_client_new_with_logger` constructor takes a forward-declared
`struct pslog_logger *`. Include the separate libpslog public header to configure
it. The client derives an owned child carrying component/model_id; the parent
logger may be destroyed afterwards. Borrowed output/palette/user resources must
remain live and support native worker threads until client destruction. Native
callbacks must not throw or longjmp. Existing constructor and receiver/config
layouts are unchanged, so this addition does not break the shared C ABI.

Trace/debug/info/warn/error are mapped to pslog. Each native hook fragment emits
one record with phase and continuation fields, preserving streaming without
accumulating a complete native log message. Continuations inherit the preceding
level on that receiver/thread (info before any level). The private native patch
propagates scopes into CPU graph workers and asynchronous tensor validation;
process-global native setters remain untouched as fallback. Raw facade logging
setters retain their native semantics. Shared device diagnostics without an
active scope cannot be attributed to a receiver; high-level media inference is
still unavailable and its separate raw hooks remain caller-controlled.

The [Lua facade](../lua/README.md) uses this same installed shared ABI and the
public pslog Lua interop ABI1. Logger qualification/retention happens before
native handoff; finalization stops native producers before unreferencing Lua.

## Local Vulkan integration evidence

The updated receiver and installed Lua facade both generated from the same
cached SmolLM2-135M instruct model on Intel(R) Graphics (RPL-U), using the Mesa
Intel Vulkan driver. Native records captured by a pslog logger configured in
Lua showed all 31/31 layers offloaded and an 11.25 MiB Vulkan KV buffer. The Lua
sink emitted eight text deltas for the test prompt and reported backend=vulkan.
This qualifies the local shared-loader/driver integration. It does not qualify
a fully static Mesa GPU executable or multi-GPU hardware not present on this
host. The standalone musl CPU evidence remains separate.

Model-card file metadata is resolved from the acquired absolute model path.
Changing cwd or the default XDG cache environment after loading does not redirect
a card to another file. Requested Hugging Face identities/nested filenames are
preserved independently of that resolved local path.
