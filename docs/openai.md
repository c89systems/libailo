# OpenAI adapter operations

`ailo/openai.h` exposes Chat Completions over the existing receiver, without an
HTTP server. `client->chat_completions` and `ailo_client_chat_completions` accept
request JSON, a synchronous byte sink and caller-owned `ailo_api_info`. The
request mapper supports the qualified text, function, schema, sampling and
reasoning controls in [the receiver contract](client.md). Unavailable controls
fail explicitly. Responses operations, model/HTTP dispatch, stored-record list/
update routes and the Lua repository-provider bridge remain required work.

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
Responses' configured TTL is already validated to at least 30 days for its
upcoming adapter. The [retention contract](inference-backend.md) records the
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

## Lua

`client:chat_completions(json, sink)` passes raw JSON through the same C ABI and
delivers each byte string immediately. The result contains HTTP metadata,
stored/started flags, ID and native usage. Callback errors return `nil, error`
with the original Lua value in `error.cause`; returning false cancels. Logger
configuration and native logging ownership follow the ordinary Lua constructor.
The current Lua constructor has no repository-provider bridge: use explicit
`store=false`. No model output is assembled by the Lua binding.
