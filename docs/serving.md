# OpenAI serving handler

The serving handler is a strict C89 receiver above `client->handle`. It owns
response lifecycle and error serialization; the embedding application owns
sockets, authentication, trusted namespace selection, TLS and scheduling.
Kore and Vectis are not dependencies. The test HTTP bridge is an acceptance
fixture, not a supported production listener.

## Contract

`ailo_handler_new` borrows one client until handler destruction. Join all calls
before destroying either object. A handler admits one call at a time, without
queuing; overlap returns HTTP 503. Distinct handlers/clients may run concurrently.
The client's own busy contract still applies. A handler does not create a second
model, inference path, repository or tenant authorization mechanism.

Each call supplies an origin-form method/target, optional JSON body and typed
content headers. JSON bodies require `application/json`, optionally with UTF-8
charset. Unsupported media/encoding returns 415. Default request limits are
8 MiB body and 16 KiB target; configuration can lower them. The network adapter
must enforce its own ingress limits before materializing a request body.

The transport supplies four synchronous callbacks: begin, write, finish, abort.
Begin receives status, content type, Allow, Cache-Control, optional Retry-After
and a generated request ID, before the first byte. Write consumes ALL borrowed
bytes before returning. Positive accepts; zero cancels; negative fails. Bounded
transport buffers are allowed; collecting the full answer is not. A blocking
write provides bounded backpressure. Event-loop integrations must run the
synchronous handler on an appropriate worker and bound their outbound queue.

The handler calls finish once after complete output. Any failure after begin
was attempted calls abort once, never finish or a replacement JSON error. A
failed stream has no fabricated successful terminal event. The first status is
immutable even if a later native failure changes adapter diagnostic metadata.
Disconnects return cancellation; failed writes return sink failure. External
cancellation uses the borrowed client's existing cancellation operation.

Preflight errors become OpenAI-shaped JSON errors with message, type, param and
code. Internal errors use a generic public message; private diagnostics remain
in the returned `ailo_error`. No repository paths or credentials are exposed in
server-error bodies. Cancellation before begin emits nothing. HTTP error output
may complete successfully at the transport level while the C call returns its
original non-OK application status. Missing callbacks/invalid C arguments return
without invoking transport. No callback may throw or longjmp across the C ABI.

Retained CRUD, pagination, metadata, conversation transactions and streaming
semantics are those in [openai.md](openai.md). Durable storage defaults to
[encrypted embedded Pouch](persistence.md); applications can supply a custom
repository provider. Admission failures occur before
inference. Output and repository quotas are not network queue limits.

## Qualification

Use the existing production dispatcher/adapters in all handler tests. A mock
transport verifies header-before-body, incremental output during generation,
bounded forwarding, begin/write/finish failure, disconnect, busy reentry,
preflight JSON errors, 404/405, content limits and immutable started status.
Exercise C89 public headers and installed shared-ABI Lua lifecycle parity.

Last, use a loopback HTTP fixture and the installed Codex CLI. Simulate model
output token by token, including function arguments, through production adapters;
verify a real tool call, returned tool result and final answer. This establishes
agent protocol interoperability, not trained-model tool competence. Keep a
separate small real-model inference smoke test. The fixture has no external
credentials, production listener or new target dependency.

The authoritative wire references are the official
[Chat Completions reference](https://developers.openai.com/api/reference/resources/chat/subresources/completions/methods/create),
[Chat streaming events](https://developers.openai.com/api/reference/resources/chat/subresources/completions/streaming-events),
[Responses reference](https://developers.openai.com/api/reference/resources/responses/methods/create),
[Responses streaming events](https://developers.openai.com/api/reference/resources/responses/streaming-events)
and [error guidance](https://developers.openai.com/api/docs/guides/error-codes).
The implemented capability profile is explicit in openai.md; a local model does
not acquire hosted tools or modalities merely because they appear in a schema.

## Reproduction and evidence

`openai-pouch` verifies the default provider's encryption, restart persistence,
real-model streaming, stable readers, concurrent access, quota/cancellation
failures and interrupted atomic publication. `openai-pouch-http` runs stored
Chat CRUD, response continuations and conversation archives through the same
HTTP fixture with production Pouch storage. The Codex acceptance harness also
selects that provider. Every fixture request reconstructs its client, exercising
durable state independently of a long-lived receiver.

Ordinary CTest's `openai-http` runs the loopback fixture without Codex or external
credentials. It covers every implemented method/path in the route table, including
stored Chat CRUD/filtering, response retrieval/deletion/input pages, conversation
CRUD/item pagination, and these state transitions:

- Responses chaining with `previous_response_id` and **only new input**, including
  a function-call/result round trip, changed instructions, parent deletion,
  missing/unstored/expired IDs and namespace isolation.
- Chat history replay with assistant function calls and tool results supplied by
  the client. Chat does not acquire server-managed continuation semantics.
- Standalone, automatic and explicitly triggered compaction; real authenticated
  envelopes, tampering rejection, stateful continuation and stateless replay.
- Provider reconstruction and file reload on **every HTTP request**, with stable
  test keys. The application-owned test repository implements the public callback
  contract; it is serial, bounded test storage, not a production provider.
- Reserve/write/commit/open/read failures, abort/close balance, failed-stream
  termination and private-error redaction. A live consumer receives a text delta
  while generation is held; disconnecting then aborts the unfinished record.

`make test-openai-agent PRESET=x86_64-linux-gnu-release` drives the installed
Codex CLI's `app-server --stdio` with explicit `thread/start`, `turn/start`,
`turn/completed` and `thread/compact/start` messages. It completes two user turns
with real shell tools, requires a successful remote compaction model pass, then
runs a third tool round trip after replay. The generation observer requires both
a remembered identifier and an earlier tool finding to survive compaction.
Matched function-call/result groups reach the compactor together. The observed
current CLI sends `compaction_trigger` to `/v1/responses`; standalone
`/v1/responses/compact` is covered independently by HTTP and native tests.

The gate declares a custom function-tool HTTP Responses provider with
`capabilities.remote_compaction="v2"`, no reasoning summary, no multi-agent
namespace, no freeform apply-patch tool and no Responses Lite/WebSocket transport.
Requests reach the product parser unchanged. A successful local summarization
fallback or a no-op checkpoint cannot satisfy the remote-compaction assertion.
Codex's state root, logs, catalog and ephemeral thread are confined to the owned
build evidence directory; no workstation config, MCP services or credentials are
used. Host `codex`, `timeout`, `curl`, `jq`, `sed` and `rg` are test tools, not
product dependencies. The test repository uses llama's already-pinned vendored
JSON header for explicitly materialized fixture storage only; all product HTTP
serialization still uses lonejson and generated bytes are forwarded directly.

The producer substitutes only generation at static link time. Summary generation
is simulated, while selection, authenticated checkpoint construction, replay,
retention, protocol serialization and transport are production code. The automatic
scenario invokes the native compaction controller; the actual sampling-loop
preflight/in-flight behavior has separate real-model native tests. This qualifies
protocol and state interoperability, not trained-model semantic quality.

Each invocation preserves requests, provider receipts, resolved generation input,
Codex events and diagnostics in an ignored `build/.../openai-serving.*` directory.
The scripts clean up their owned processes. The fixture is not installed or shipped
in SDKs and is not a supported production listener. Unsupported hosted tools,
namespace/freeform tools, reasoning summaries, background execution, WebSockets,
production networking and actual model tool-selection quality remain outside this
declared profile.

Qualification on 2026-10-16 passed with Codex CLI **0.162.0-alpha.20**:
three real tool round trips and one semantic compaction pass across seven
successful streamed HTTP requests. The final run is recorded in
`build/openai-serving.JqAtRv/`. Full native suites passed 74/74 glibc and 71/71
musl CTests; a final focused glibc run also passed the added malformed-trigger
and short-window cases. Installed Lua passed 4/4 checks, including LuaRocks.
All four Linux builds passed with strict C89 public-header probes; AArch64
build evidence does not establish hardware execution. Format and shell syntax
checks passed. Logs use the `build/e2e-final-*` prefix.
