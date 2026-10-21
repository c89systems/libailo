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
production networking and general model tool-selection quality remain outside
this deterministic profile.

## Trained-model serving acceptance

`make test-openai-inference` defaults to `x86_64-linux-gnu-release` and runs an
independent six-case CTest suite under `build/serving-inference-PRESET-BACKEND/`.
Ordinary `make test` remains offline and deterministic. The trained suite also
defaults to offline acquisition; use `INFERENCE_ACQUIRE=download` explicitly
once. Acquisition uses the public libailo model-cache facility, not a separate
downloader. Text weights stay in `build/inference-models/`; media pairs reuse
`build/media-model-cache/` and the verified speech sample in
`build/media-fixtures/`. All are owned build caches.

The default backend is CPU. `INFERENCE_BACKEND=vulkan` requires a Vulkan-enabled
native preset and real hardware. Each native model-card observation must report
the requested backend; Vulkan additionally requires every repeating layer and
the output layer on selected devices, a native GPU context allocation, and the
selected media encoder on a GPU. The suite disables external ICD discovery and
Mesa's disk shader cache. It cannot pass through silent CPU fallback.

```sh
make test-openai-inference INFERENCE_BACKEND=vulkan
make test-openai-inference PRESET=x86_64-linux-musl-release INFERENCE_BACKEND=vulkan
```

The musl preset links the trained listener and consuming socket executable fully
statically. Before execution the driver rejects an ELF interpreter or `NEEDED`
dependency. Every listener lifetime, including Pouch restart, runs under host
`strace` with file/process tracing. A passing case requires exactly its initial
executable invocation, no helper execution and no shared-object path access.
Host curl, Codex and assertion tools are outside that native process tree.

The fixed model is `bartowski/Qwen2.5-0.5B-Instruct-GGUF`, revision
`c6f6d6134e86f8c53897f3d892b3a3da0143e8bc`, file
`Qwen2.5-0.5B-Instruct-Q4_K_M.gguf`, SHA-256
`6eb923e7d26e9cea28811e1a8e852009b21242fb157b26149d3b188f3a8c8653`.
The fixture checks that hash and runs inference with two CPU threads, greedy
sampling and a 32,768-token context. The simulated and trained executables share
the same HTTP transport. Only the simulated executable defines a replacement
generator; the trained one links production sampling, tool parsing and summaries.
The tested Codex version omits temperature; the product retains its API default of
one. A test-only linker wrapper sets the trained fixture's native temperature
sampler to zero, so this tiny model is tested greedily. It calls the actual native
sampler and never fabricates tokens, tool arguments or answers. Codex requests
reach the product parser unchanged; no product sampling defaults are modified.

This older Qwen ChatML template ignores developer messages. The trained fixture
supplies an explicit template override, derived from the pinned GGUF template,
which maps developer messages to system instructions while preserving their
content. It fails if the expected template no longer matches. This is a declared
test application configuration, not an implicit product rewrite of client roles.
The original template's unsupported-role rejection remains intact.

The cases require observable trained behavior:

- Chat and Responses automatically select a read tool, produce JSON arguments,
  consume the actual owned file's contents and stream the identifier in their
  answer. Responses submits only the new function result with
  `previous_response_id`, then recalls the identifier after listener restart
  using encrypted Pouch and the original generated key. Chat replays its full
  history. Stored deletion and missing-ID errors are also checked.
- Standalone compaction performs a real summary pass, preserves the system
  instruction exactly, seals eligible complete tool groups, and answers from
  opaque replay. A test-only observer inspects authenticated incoming replay
  facts and requires the identifier's tool-result provenance. Tampered replay
  must fail. Summary generation explicitly asks for literal identifier values
  and concrete findings; vague statements that a tool returned a value are
  insufficient.
- A consuming socket receives the first native text delta while the transport
  callback is held on a FIFO. A release completes the stream; an RST at the same
  point cancels generation, leaves no retrievable retained response, and permits
  the next request to complete. The observer forwards bytes before inspecting
  bounded SSE frames. It never captures a generated answer on the server.
- Codex runs two actual `cat` tool turns, remotely compacts eligible history,
  replays the opaque item, and answers a third turn from the compacted finding
  without rereading the file. Each turn's answer is checked independently.
  Two different files confirm one identifier. This is a retention smoke test,
  not a benchmark for distinguishing multiple changing facts.
  The note contains repetitive routine history so reduction is necessary and
  the compaction target leaves room for protected instructions and tool schemas.
  A successful no-op checkpoint cannot satisfy this check.
- The pinned SmolVLM pair receives a real JPEG through Responses and Chat and
  streams a semantic color answer. Responses replays the retained image using
  only a new question plus `previous_response_id` after a Pouch listener restart.
- The pinned Qwen3-ASR pair receives the verified JFK WAV through Chat and
  streams a transcript. After restart, the test retrieves stored `content_parts`,
  resubmits the retained audio and requires the same transcript. Both media cases
  reject malformed input, then prove successful recovery and consume a real text
  delta while the producer is held before completion. Responses audio remains
  explicitly unsupported under the declared wire profile and must return 400.

Media model/projector revisions and hashes match the
[multimodal reference pairs](multimodal.md). Their context is 4096 positions;
they use the stock model templates. ASR is tested as transcription, not general
audio conversation.

Readiness, request completion and backpressure use FIFO notifications, not
polling sleeps. Timeouts are failure deadlines, never minimum test durations.
Trained inference is intentionally more expensive than the default unit suite.
Run an individual case with `ctest --test-dir build/serving-inference-PRESET-BACKEND
--output-on-failure -R '^trained-serving-compaction$'`, for example.

Evidence stays in `build/PRESET/openai-serving.*`: requests, wire responses,
native usage/placement receipts, authenticated replay observations, Codex events
and, for musl, native file/process traces.
The native generator is silent without a logger; transport diagnostics belong
to the test fixture. These cases qualify the pinned model/configuration,
not general model intelligence or instruction-following. In particular, the
0.5B model can ignore answer-style instructions even when those instructions
are retained exactly. A two-file experiment with different identifiers also
confused the first and latest values after compaction; its failing trace is
`build/x86_64-linux-gnu-release/openai-serving.bpFMzG/`. AMD/multi-GPU hardware
qualification and production network server integration remain separate work.

Trained Codex qualification on 2026-10-21 passed with CLI
**0.162.0-alpha.20**, two real tool executions, one native summary pass and
six successful streamed HTTP requests. The post-compaction request contains
neither plaintext identifier nor raw tool results. Evidence:
`build/x86_64-linux-gnu-release/openai-serving.5wcfic/`;
CTest log: `build/serving-codex-final.log` (171.92 seconds, actual CPU inference).
The three direct trained cases passed in 50.37 seconds
(`build/serving-direct-final.log`). The standard native suite passed 77/77,
including packaging/install-tree checks; installed Lua passed 5/5 and the
existing simulated Codex acceptance also passed. Logs:
`build/serving-standard-final.log`, `build/serving-lua-final.log`,
`build/serving-simulated-final.log`. The build's strict C89 public-header probes,
format check, Bash syntax and shell lint also passed.

### Intel Vulkan and static serving qualification, 2026-10-21

All six trained serving cases passed on Intel RPL-U with the built-in ANV
provider in both GNU and fully static musl executables. Native placement receipts
confirmed all 24 Qwen decoder layers plus its output layer on Vulkan, allocated
GPU context buffers, and GPU selection for both media encoders. No external ICD
was available through the suite's configured discovery paths.

| Configuration | Passed cases | Logs |
| --- | --- | --- |
| GNU / Vulkan | Protocol, compaction, transport, Codex, vision, audio | `build/serving-vulkan-direct.log`, `build/serving-codex-vulkan.log`, `build/serving-media-vulkan.log` |
| Static musl / Vulkan | All six, with native file/process tracing | `build/serving-musl-vulkan.log` |
| Static musl / CPU | Protocol, compaction, transport, vision, audio, with tracing | `build/serving-musl-cpu.log` |
| GNU / CPU media | Vision and audio; text/Codex evidence is recorded above | `build/serving-media-cpu.log` (vision), `build/serving-audio-cpu.log` (repaired audio case) |

Static musl's six Vulkan cases took 410.79 seconds, including actual inference,
model/encoder loading, Pouch reopening and tracing; its five CPU cases took
222.16 seconds. Neither suite adds minimum-duration waits. GNU's Vulkan Codex
case took 78.11 seconds. The musl executable and consuming client had neither
`PT_INTERP` nor dynamic dependencies, and every native trace passed the no-helper,
no-shared-object checks. Representative musl artifacts are
`build/x86_64-linux-musl-release/openai-serving.sPuD4W/` (Codex),
`openai-serving.gFgdGV/` (vision) and `openai-serving.jvUQ6R/` (audio).

The standard GNU suite passed 77/77, including package and installed-consumer
checks; installed Lua passed 5/5. The simulated Codex gate also passed with the
shared transport/ownership changes. Logs: `build/serving-gpu-standard.log`,
`build/serving-gpu-lua.log` and `build/serving-gpu-simulated.log`.

AMD, mixed/multi-GPU and AArch64 GPU execution remain unqualified. These checks
exercise the test-only loopback transport; a production Vectis/Kore listener
remains a separate integration.

## Earlier deterministic qualification

Qualification on 2026-10-16 passed with Codex CLI **0.162.0-alpha.20**:
three real tool round trips and one semantic compaction pass across seven
successful streamed HTTP requests. The final run is recorded in
`build/openai-serving.JqAtRv/`. Full native suites passed 74/74 glibc and 71/71
musl CTests; a final focused glibc run also passed the added malformed-trigger
and short-window cases. Installed Lua passed 4/4 checks, including LuaRocks.
All four Linux builds passed with strict C89 public-header probes; AArch64
build evidence does not establish hardware execution. Format and shell syntax
checks passed. Logs use the `build/e2e-final-*` prefix.
