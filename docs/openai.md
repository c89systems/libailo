# OpenAI adapter operations

`ailo/openai.h` exposes Chat Completions and Responses over the existing receiver,
without an HTTP server. `client->chat_completions` and `ailo_client_chat_completions` accept
request JSON, a synchronous byte sink and caller-owned `ailo_api_info`. The
request mapper supports the qualified text, function, schema, sampling and
reasoning controls in [the receiver contract](client.md). Unavailable controls
fail explicitly. Model/HTTP dispatch, stored-record list/update routes,
conversation objects, automatic Responses compaction and encrypted reasoning
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

Explicit compaction items already use the native authenticated replay path.
Conversation objects, automatic context management, standalone Responses compact,
encrypted reasoning replay, input-item pagination and HTTP routes are still
unavailable integrations; requests for those controls fail explicitly. Capability
fields distinguish implemented Responses operations, configured record storage,
and the unavailable conversation/automatic-compaction/reasoning-encryption paths.

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

Configure the same trusted profile through `ailo.new{..., openai={...}}`:
`namespace_id`, `repository`, `zero_data_retention`, `chat_store_opt_in`,
`response_retention_seconds`, `chat_retention_seconds`,
`max_output_memory_bytes` and `max_record_bytes`. Defaults match C. Unsigned
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
