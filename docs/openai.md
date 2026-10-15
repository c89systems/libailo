# OpenAI adapter operations

`ailo/openai.h` exposes Chat Completions and Responses over the existing receiver,
without an HTTP server. `client->chat_completions` and
`ailo_client_chat_completions` accept request JSON, a synchronous byte sink and
caller-owned `ailo_api_info`. The
request mapper supports the qualified text, function, schema, sampling and
reasoning controls in [the receiver contract](client.md). Unavailable controls
fail explicitly. Model/HTTP dispatch and stored-response list/update routes
remain required work.

```c
/* After constructing client; sink consumes every byte before returning. */
ailo_api_info result;
ailo_error error;
const char *json =
    "{\"model\":\"local-model\",\"messages\":[{\"role\":\"user\","
    "\"content\":\"Hello\"}],\"store\":false,\"stream\":true}";
ailo_status status = client->chat_completions(
    client, json, strlen(json), sink, user_data, &result, &error);
```

`stream=true` writes JSON/SSE directly through lonejson: role chunks, immediate
content/function deltas, probability chunks, finish chunks, optional final usage
and `data: [DONE]`. Every frame shares the completion ID, model and creation
time. Each supplied byte range is borrowed only until the sink returns; positive
accepts all bytes, zero cancels, negative fails. No complete SSE frame or reply
is collected for unstored streaming. Native item boundaries and raw reasoning
do not become invented Chat fields. Reasoning usage still counts toward total
completion tokens.

`stream=false` explicitly retains a bounded typed result and writes one
`chat.completion` JSON object after generation. Stored requests retain that same
typed output while forwarding deltas immediately. No second serialized reply
buffer is created. The default charged output/probability memory limit is 32 MiB;
configure `max_output_memory_bytes` through `config.openai`. The charge includes
payload and entry metadata; allocator capacity, temporary single-token Unicode
conversion, the native sampler's one-token alternatives and model/context/cache
allocations are separate. Native
sampled-output limits also apply. A JSON document/frame has a 64 MiB writer
limit; stored sections additionally share the configured record byte limit.

Logprobs use the receiver's raw-byte provenance and classification watermarks.
Content deltas are forwarded first; a probability-only chunk may follow once a
token's visible bytes are classified. The chosen entry contains its projection
into visible content and the original sampled-token probability, not a
fabricated probability from retokenizing the reply. Chosen tokens outside the
native top twenty use the documented -9999 sentinel even when fewer alternatives
are requested. Hidden reasoning, framing,
function syntax and stop bytes are excluded from chosen content entries. Top
alternatives retain the sampled vocabulary bytes. Incomplete UTF-8 token pieces
have a replacement-character display label and exact `bytes` arrays. Very low
probabilities also use the API's -9999 floor. Only unsettled token metadata is kept
for unstored streaming, within the same memory quota.

Stream obfuscation defaults on. Random characters in the `obfuscation` field
pad JSON frame lengths to 256-byte multiples. `include_obfuscation=false`
disables this. This is the local padding algorithm; OpenAI does not publish an
exact padding algorithm or packetization guarantee. `include_usage=true` emits
null usage on ordinary chunks and a final usage chunk with empty choices.

Preflight failures emit no bytes. `http_status`, `content_type`, `started` and
actual native usage are available in `ailo_api_info`; the library returns an
`ailo_status` and diagnostic. The embedding handler builds its error response
before output starts, or closes an already-started response on failure. A
failed/cancelled stream has no successful terminal finish or `[DONE]`. Reentry
and overlapping operations use the client's ordinary busy contract.

## Retained records

Set `config.openai` to an `ailo_openai_config`. Strings and the repository method
table are copied; callback user resources remain borrowed until client destroy.
A configured repository needs every transaction/read/delete method and a trusted
`namespace_id`. Request `user`, `metadata` and `safety_identifier` never authorize
record access. Separate clients may call a shared provider concurrently; the
provider owns durable storage, authorization, atomic publication and expiry
cleanup. These are records, not evictable KV entries.

Chat defaults to storage for the current new-account profile; use
`chat_store_opt_in` for an existing deployment's historical default. An explicit
`store` selects storage except when trusted ZDR policy forces false. No repository
means storage-dependent requests fail before loading a lazy model or sampling.
Use explicit `store=false` for stateless operation. Chat's default local record
policy is retention until deletion; an explicit application TTL is available.
Responses' configured TTL is validated to at least 30 days and applied to each
stored response. The [retention contract](inference-backend.md) records the
official source and the distinction from conversation-item lifetime.

The repository first reserves the entire `max_record_bytes` allowance and
retention deadline. The default is 64 MiB across all sections. Each transaction
receives independent incremental JSON sections:

- `AILO_RECORD_INPUT`: normalized input-message array, including function history.
- `AILO_RECORD_OUTPUT`: complete `chat.completion` object, not SSE frames.
- `AILO_RECORD_METADATA`: request metadata object, separate from the wire result.

Failure while reserving, writing, generating or forwarding pre-terminal output
aborts unpublished state. A successful commit atomically publishes all sections
before finish/usage/`[DONE]` or buffered output is sent. If the transport fails
after this commit, `info.stored` remains true: a completed durable record is not
undone by a later disconnect. Providers must not evict records before their
deadline; a zero deadline means explicit deletion. Reserve failures returning a
transaction still receive abort. Commit failure retains ownership for abort.

`chat_retrieve(client, id, input, sink, user, info, error)` reads the completion
or its normalized input array directly through the provider's sink, without
collecting the stored JSON. The library rejects expired records before sending
bytes, closes read handles on every path, and enforces its read byte limit.
Missing/unauthorized/expired records return `AILO_NOT_FOUND` and HTTP 404.
`chat_delete` emits the documented deletion object after the provider deletes
the record. These are primitives for a later transport handler, not registered
HTTP routes or paginated messages/list/update endpoints.

## Responses

`client->responses` / `ailo_client_responses` accepts the same raw request/sink/
info arguments as Chat. It uses the existing in-process inference producer.
Text, schemas, function tools, qualified reasoning text and logprobs follow the
request mapper's supported controls. A stream sends named SSE lifecycle,
content, reasoning and argument events with monotonic `sequence_number`, stable
item/call IDs and zero-based output/content indices. There is no Chat `[DONE]`
marker. The terminal event is `response.completed` or `response.incomplete`
with `max_output_tokens` details when the native output limit is reached.
An earlier completed function call may coexist with a later incomplete message.
For functions, the added-item event waits only for the bounded name and
precedes the first argument delta. A truncated name-only call is added as an
incomplete item. Arguments do not wait for the function or response to complete.

The [official streaming contract](https://developers.openai.com/api/reference/resources/responses/streaming-events)
repeats finalized text/arguments/items and the full terminal response. Responses
therefore explicitly collects typed output within `max_output_memory_bytes`,
including when `store=false`, while delivering each delta immediately. It does
not spool output or build a complete JSON/SSE buffer. Each wire frame and stored
section is written directly through lonejson. Logprob metadata follows raw byte
provenance: visible text is immediate, with a separate empty text delta when
its probability becomes settled. Hidden framing/reasoning tokens do not become
answer probabilities; split UTF-8 vocabulary pieces use replacement display
labels. Full finalized parts include their collected logprobs.

Stored Responses default to true unless ZDR overrides storage. The repository
reserves the full quota before model loading/inference, then atomically publishes
input lineage, output and user metadata. Successful done/terminal events follow
commit. Output failure before publication aborts the transaction; disconnect
after commit leaves `info.stored=true`. Without storage, completed item events
are sent when their metadata settles. An inference/quota/repository failure
after complete SSE frames can emit `response.failed`; a rejected sink or partial
frame requires the transport to close. Neither failure path emits successful
terminal completion. Buffered `stream=false` returns a JSON response using the
same explicitly retained typed output.

`response_retrieve(id, ...)` streams the stored Response object directly from the
repository; `response_delete(id, ...)` emits `response.deleted` after deletion.
Both use `AILO_RECORD_RESPONSE` and the trusted namespace, with expiry checked
before bytes are sent. Chat records are a separate protocol domain.

`previous_response_id` resolves canonical stored input and output, checks model/tokenizer/
template identity, and prepends the accumulated typed lineage to new input.
Prior top-level `instructions` are excluded; current instructions affect only
the current request. Each child stores its accumulated lineage independently,
so deleting/expiring a parent does not break an already-created child. Expanded
lineage retains the existing 4 MiB / 4096 item-and-part bounds. Repository read
sections are bounded to 8 MiB while decoding lineage. The private input section
is an envelope with `ailo_model_identity`, `input` and diagnostic-free canonical
`output` arrays; it is not an
implemented input-items HTTP endpoint. Output logprobs are validated as diagnostic
metadata on replay and are not rendered into the model prompt or copied into
canonical descendant input. Chaining reads this private section alone, so large
wire logprob diagnostics do not consume the lineage input budget. Each original
stored output retains its logprobs.

Standalone compaction and canonical-window replay are implemented below.
Stored-response input-item pagination and HTTP routes are still
unavailable integrations; requests for those controls fail explicitly. Capability
fields distinguish implemented Responses operations, configured record storage,
and configured conversation support.

## Conversations

The receiver exposes conversation create/retrieve/update/delete and item
create/retrieve/delete/list, through the corresponding
`ailo_client_conversation_*` free functions and receiver methods in
[`ailo/openai.h`](../include/ailo/openai.h). Create/update/item-create accept raw
JSON bodies; item retrieval/list accept an optional raw JSON query object.
No HTTP listener is introduced. Operations return the same `ailo_api_info` and
byte-sink/error contract as the other adapters.

This follows the official [conversation state guide](https://developers.openai.com/api/docs/guides/conversation-state)
and [Conversations reference](https://developers.openai.com/api/reference/resources/conversations).
Conversation roots and item rows have no automatic expiry. Responses create
accepts a conversation ID string or `{ "id": "conv_..." }`, prepends its saved
context, and appends new input/output items on success, including `store=false`
and trusted ZDR. The response record, if requested, retains its separate lifetime.
`previous_response_id` and `conversation` are mutually exclusive. Per-request
top-level instructions are not inherited; explicit system/developer items are.

Create and item-create accept at most 20 items. IDs omitted on input are assigned
before admission; duplicate input IDs are rejected. Easy messages are normalized
to item objects with IDs, type, status and content arrays. Supplied wire diagnostics
are preserved for item retrieval while canonical model input omits diagnostics.
Updates replace the metadata map; null clears it. Query options are `after`,
`limit` (1..100, default 20), `order` (`asc`/`desc`, default `desc`) and `include`
(`reasoning.encrypted_content`, `message.output_text.logprobs`). Pages return
`object:list`, data, first/last IDs and `has_more`. Item bodies and string values
pass incrementally from provider to sink; no complete item or page is assembled
by libailo.

Deleting a [conversation](https://developers.openai.com/api/reference/resources/conversations/methods/delete)
does not delete its item rows. The root becomes unavailable to retrieval,
updates and new responses. A minimal tombstone clears root metadata/context and
preserves the parent identity needed to retrieve or explicitly delete surviving
items. Deleting an item invalidates the inference checkpoint; the next response
reconstructs context from the remaining canonical item rows. Original history
survives automatic compaction. Generated compaction control items are omitted
from reconstruction so deleted history cannot reappear through an old checkpoint.
An imported opaque window still obeys its authentication/binding constraints.

The bounded inference checkpoint is distinct from the full durable item archive.
Context decoding remains capped at 8 MiB serialized / 4 MiB owned item data and
4096 items/parts. Oversized reconstruction fails explicitly; it does not evict or
silently truncate stored items. A changed model/template can rebuild ordinary
canonical history instead of trying to open an incompatible compacted checkpoint.
Conversation reasoning/compaction requires configured stable keys with no expiry,
so persisted replay state does not expire before its conversation.

Configure the optional `amend` and `query` callback pair on the existing record
repository. Both are required to enable conversations. `conversation_api`
advertises implementation; `response_conversations` reports the configured pair.
Existing record-only providers need not implement it. All operations use the
trusted client namespace, not a request metadata/user field.

`amend(change)` admits the whole operation before inference. The change contains
namespace, conversation ID, action, prior revision, byte allowance, new input IDs,
optional item ID and optional response-record metadata. It atomically compares
the revision, checks IDs and holds the conversation until commit/abort; conflicts
return `AILO_BUSY` before inference. `open` readers must be stable snapshots across
all section reads. A response plus its conversation mutation commits atomically,
before successful done/terminal events. Cancellation or failed publication aborts
both. A disconnect after commit does not undo durable state.

Transactions write ordinary response sections when response storage is requested.
`AILO_RECORD_CONVERSATION_STATE` replaces the private context checkpoint/revision.
`AILO_RECORD_CONVERSATION_ITEMS` appends rows shaped `{id,wire,canonical}`: the
original wire item plus its durable native view. Generated compaction control
rows have null canonical views. Without a response record, ordinary OUTPUT writes
the conversation object; an omitted OUTPUT preserves the existing object.
Providers treat checkpoints as opaque library state and maintain revision/ID
constraints, durability, authorization, quota admission and atomic publication.

`query(query)` opens a stable reader of an item array scoped to namespace and
parent conversation, optionally one item or a cursor page. It uses ordinary
`read(OUTPUT, emit)` and `close`. Return at most `limit+1` rows so libailo can compute
`has_more`; internal limit zero requests ascending canonical history, excluding
null canonical rows. Item IDs are namespace-wide unique and parent association
must also be checked. These reads carry no TTL. The per-transaction byte allowance
covers all sections together; total archive capacity is the provider's admission
policy. No database, key file or repository is silently created by the library.

## Standalone Responses compaction

`client->response_compact` / `ailo_client_response_compact` accepts request JSON,
byte sink and `ailo_api_info`, following the official
[`/responses/compact` contract](https://developers.openai.com/api/reference/resources/responses/methods/compact).
Its required `model` selects the served model (null selects the configured
default). Supported fields are `input`, `instructions`, `previous_response_id`,
`prompt_cache_key`, `prompt_cache_retention` and `service_tier`. Generation/store/
stream controls belong to Responses create and are rejected here.

The operation returns `response.compaction` with ID, creation time, canonical
output items and actual model-pass usage. It reserves no response record and
creates no retrievable record, including when the client's normal create policy
defaults to storage. An explicit prior response still uses the trusted repository
for its input. Input must fit the model context before summary generation.

Set the trusted `openai.compaction` policy to choose the native target, recent
turns, output/summary reserves and summary byte limit; the constructor copies it.
Omission uses the native defaults. `instructions` reaches the summary model as a
separate developer message, alongside the fixed provenance/schema task. It does
not become inherited instructions in the returned window. Canonical system and
developer input messages remain protected in that window. A short window can
return unchanged items with zero model-pass usage. Impossible reduction, summary
failure, quota exhaustion and cancellation return explicit errors.

The model's bounded summary and authenticated state are materialized during
compaction; ordinary generation still forwards each available delta immediately.
The compact result uses the endpoint's synchronous JSON shape and is written
directly to the byte sink after validation, without a complete serialized-result
buffer or a temporary-file spool.

Pass the entire returned output array into the next Responses request, then append
new items. Retained items, their order and the compaction item are authenticated;
pruning or substituting them fails before generation. Per-request `instructions`
may change independently. The adapter authenticates the native window, adjusts
its instruction prefix/provenance indices and reseals it without exposing the
summary. Native C89 continuation tokens and Responses window envelopes have
different protocol purposes; use each with its corresponding receiver operation.
OpenAI-issued opaque items are not local libailo items.

Responses envelopes use a separate HKDF-SHA256-derived AES-256-GCM key and random
96-bit nonces. Authentication binds the header, item ID, model/template identity
and trusted API namespace, in addition to the compactor's key domain. The inner
native token authenticates retained roles/content and the semantic summary.
Decryption authenticates before parsing. Derived keys and envelope plaintext
buffers are cleansed; key rotation uses the configured old-key ring. Decoded
envelopes share the compactor's byte limit (default 1 MiB, at most 4 MiB), and
canonical output obeys the API memory quota.

Stateless replay can use the client's ephemeral keys or explicitly configured
stable keys. Reconstruction/restart replay needs compatible configured keys,
namespace and model/template. Compactor TTL applies to these envelopes; reads do
not extend an expired item. Stored compacted Responses require stable keys and
either no compactor expiry or a TTL at least as long as response retention.
Publication reseals the state, covering the record's full promised lifetime.
The application retains those keys
for as long as records remain usable; no plaintext key file is created.

## Automatic Responses compaction

Responses create accepts `context_management: [{"type":"compaction",
"compact_threshold":N}]`, following the official
[compaction guide](https://developers.openai.com/api/docs/guides/compaction).
The trigger counts the rendered input and unfinished native output. Omitting
`compact_threshold` or setting it to null selects the context size minus the
trusted output reserve. A configured
target must be below that trigger; an omitted target selects 75% of the explicit
trigger. The native policy otherwise supplies the target, recent turns, summary
context, token/byte bounds and output reserve. Each request has at most four
compaction passes. Impossible reductions and exhausted bounds fail explicitly.

Preflight and in-flight compaction use the same separate summary context and
native context rebuild as ordinary receiver generation. The active message,
function arguments, reasoning parser, sampler, IDs and token provenance continue
across the rebuild. Available text/reasoning/argument deltas still reach the sink
immediately; context reconstruction never redelivers previously emitted deltas.

A compaction output item is added before its model pass with empty encrypted
content. As specified by the official
[streaming events reference](https://developers.openai.com/api/reference/resources/responses/streaming-events),
new summary output can produce `response.compaction.compacting` at most once per
30 seconds; that progress event carries no summary. Short passes need no progress
event. Use the completed compaction item from `response.output_item.done` or the
terminal response for replay. Its ID and output index remain stable.

Append output to input normally, or drop everything before the most recent
automatic compaction item and append new input. Both forms restore the same
authenticated canonical input. Each completed automatic envelope also carries
the complete generated items preceding it in that response, including a message
that continued across the checkpoint. This bounded state is finalized at response
completion, so the compaction done event is deferred until then. Immediate deltas
continue throughout generation. Standalone windows still require every retained
item; the authenticated envelope distinguishes the two replay contracts.

Automatic envelopes share the existing encryption, purpose/key/model/namespace
binding, TTL and byte limits. Their state and encrypted output count against the
API memory budget. Stored automatic requests require stable keys and a TTL
covering response retention even if the threshold is not reached. The private
record stores the final compacted input and ordinary generated output exactly
once, permitting `previous_response_id` chaining across receiver reconstruction.
Publication remains atomic before successful done/terminal events. Summary work
is included in wire usage; native info keeps its separate compaction counters.
`response_automatic_compaction` now advertises this integration.

## Encrypted reasoning replay

Qualified Responses reasoning items include local `encrypted_content` by default,
following the current [Responses create reference](https://developers.openai.com/api/reference/resources/responses/methods/create)
and [reasoning guide](https://developers.openai.com/api/docs/guides/reasoning).
This also applies to `store=false` and the trusted ZDR profile. The legacy
`include: ["reasoning.encrypted_content"]` is accepted without enabling logprobs
or changing that default. Nonempty reasoning summaries remain unsupported; raw
local reasoning is not relabeled as a summary.

Reasoning deltas reach the sink immediately. The added item has null encrypted
content; its completed `response.output_item.done` item contains the final token.
For an unstored response, that done event can precede subsequent answer tokens.
Stored responses retain the existing atomic-publication rule for done events.
Use a final item for replay, including when its status is `incomplete`.

Pass the reasoning item with its ID, empty `summary` array and encrypted content
into subsequent input. Plain `content` may be omitted or empty. If supplied, it
must match the authenticated content exactly. The adapter restores canonical
reasoning before inference and normalizes a reasoning-only historical turn with
an internal empty assistant message when required by the native template. It
does not emit a fabricated answer item. Raw reasoning input remains supported.

The token contains bounded canonical reasoning state encrypted with AES-256-GCM,
using a separate HKDF-SHA256 purpose, `libailo.responses.reasoning:1`. Model/template,
namespace, key domain, item ID, issue/expiry timestamps and payload length are
authenticated. Compaction tokens and reasoning tokens cannot substitute for each
other. Authentication precedes parsing. The existing compactor byte limit, key
rotation and TTL apply, alongside API input/output budgets; plaintext and derived
secret buffers are cleansed. Model identity is established before generation,
so a done-item encryption step never starts hashing model files between deltas.

Stateless ephemeral keys work within the owning client. Reconstruction or restart
requires compatible configured keys. Stored requests that generate reasoning
require stable configured keys and TTL zero or at least the response retention
period; a missing policy fails before events or inference and aborts any record
reservation. Private retained lineage stores canonical reasoning rather than a
dependency on a public replay token. Public retrieval preserves the encrypted
wire item. Only compatible libailo instances can consume these local tokens;
they are not OpenAI-issued state. `response_reasoning_encryption` advertises the
implemented integration.

## Lua

`client:chat_completions(json, sink)` passes raw JSON through the same C ABI and
delivers each byte string immediately. The result contains HTTP metadata,
stored/started flags, ID and native usage. Callback errors return `nil, error`
with the original Lua value in `error.cause`; returning false cancels. Logger
configuration and native logging ownership follow the ordinary Lua constructor.
No model output is assembled by the Lua binding. Retained operations are
`client:chat_retrieve(id, sink, {input=true})` (omit options for the completion)
and `client:chat_delete(id, sink)`.
Responses uses `client:responses(json, sink)`, `response_retrieve(id, sink)` and
`response_delete(id, sink)`, with the same result/error/cancellation contract.
Standalone compaction is `client:response_compact(json, sink)` and returns the
same info/error shape. Configure its policy under `openai.compaction` using the
ordinary native compaction policy fields.

Configure the same trusted profile through `ailo.new{..., openai={...}}`:
`namespace_id`, `repository`, `zero_data_retention`, `chat_store_opt_in`,
`response_retention_seconds`, `chat_retention_seconds`,
`max_output_memory_bytes`, `max_record_bytes` and `compaction`. Defaults match C.
Unsigned
64-bit TTLs accept an exact nonnegative integer or `{hi=...,lo=...}`.
The repository is an application-owned table or userdata receiver with all
eight methods below; the constructor captures its methods and retains the
receiver until client close. Later method replacement does not change that
client's callbacks. The application supplies the durable implementation.

| Receiver method | Successful return |
| --- | --- |
| `reserve(metadata)` | transaction |
| `write(transaction, section, bytes)` | `true` |
| `commit(transaction)` | `true`, consuming transaction |
| `abort(transaction)` | no return required; must succeed |
| `open(namespace_id, protocol, id)` | reader, expiry |
| `read(reader, section, emit)` | `true` |
| `close(reader)` | no return required; must succeed |
| `erase(namespace_id, protocol, id)` | `true` |

Metadata has `namespace_id`, `protocol`, `id`, `created_at`, `expires_at` and
`max_bytes`. Protocol is `"chat"` or `"response"`; section is `"output"`,
`"input"` or `"metadata"`. Expiry uses Unix seconds with zero meaning until
deletion. `read` calls `emit(bytes)` synchronously for each available chunk and
must stop when it returns false. It runs on the invoking Lua thread. The emitter
rejects calls after `read` returns or throws; retaining it does not extend reader
or client lifetime.

Failure returns `nil, error`, or throws a Lua value. `reserve` may return a
transaction plus an error for subsequent abort; `open` may return reader,
expiry, error for subsequent close. Error tables with a recognized native
`code` preserve that status; other values become `io_failed`. The original
value is exposed as `error.cause`. Cleanup errors are reported separately as
`error.cleanup_cause`, preserving an earlier output/provider failure. Callbacks
must not yield and all calls are protected before crossing native C++ frames.
