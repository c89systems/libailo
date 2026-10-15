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
semantics are those in [openai.md](openai.md). Durable storage remains an
explicit application-owned repository provider. Admission failures occur before
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

Final verification passed 73/73 glibc CTests, 70/70 musl CTests and 4/4 installed
Lua checks, including LuaRocks and public-only shared ABI imports. Full builds
and strict C89 standalone-header probes passed for all four Linux targets:
x86_64/AArch64 with glibc/musl. The glibc suite includes source/package checks,
exact exports, install privacy and relocated C/C++ consumers. Cross compilation
does not establish native AArch64 GPU hardware qualification.

`make test-openai-agent PRESET=x86_64-linux-gnu-release` builds an explicit
test-only target, checks model discovery and a malformed-request HTTP error,
then runs the installed Codex CLI against an ephemeral loopback port. Host
`codex`, `timeout`, `curl`, `jq` and `rg` are test prerequisites; they do not enter
the target dependency closure. Ordinary CTest does not require Codex or start a
listener. The fixture uses a stateless ZDR policy, independently of the retained
record tests. It loads the tiny generated model for real template preflight but
substitutes only generation at link time. Prompt qualification remains production
code, separated from generation in `ailo_prompt.cpp`.

The local model catalog explicitly selects function tools, no reasoning summary,
no multi-agent namespace, no freeform apply-patch tool and no Responses Lite or
WebSocket transport. These are declared capabilities of this test profile.
Requests reach the production parser unchanged. `client_metadata` follows the
documented bounded telemetry extension in [openai.md](openai.md).

Codex CLI **0.162.0-alpha.20** passed the round-trip on 2026-10-16. It executed
the requested `exec_command` printf with exit status zero, returned the real
`AILO_TOOL_OK` output under the original call ID, and received
`AILO_AGENT_DONE`. Generation emitted 91 one-byte token pieces for the function
name/arguments and 15 for the final answer. Both `/v1/responses` requests
completed with HTTP 200; the production serializers forwarded 41,368 and 21,933
bytes respectively, including their required terminal objects and padding.
The fixture never collected those output bytes. Evidence is in the ignored
`build/openai-agent.Qpzq4z/` directory; the script preserves a new evidence
directory on every invocation and cleans up its owned server process.

This establishes function-tool client interoperability. It does not qualify
trained-model tool selection or hosted tools, namespace tools, freeform tools,
reasoning summaries, background execution, WebSockets or production networking.
The supported protocol profile and explicit rejection behavior remain in
openai.md; network-framework acceptance belongs to the eventual embedding app.
