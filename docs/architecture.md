# Ailo implementation specification

Status: reviewed specification; implementation and acceptance evidence pending.

## Intent and scope

Ailo runs GGUF models locally through llama.cpp. `libailo` supplies a C89
receiver shell and equivalent free functions for applications that ordinarily
call OpenAI Chat Completions. It also supplies a complete C89 facade over the
pinned llama.cpp public C library APIs. Neither public boundary exposes C++
types, C++ headers, upstream headers, or compiler extensions disguised as C89.
There is no HTTP server in this project. Vectis/Kore integration and Responses
API support are later work.

The first receiver implements text Chat Completions, buffered output and real
streaming, model listing and model retrieval, with explicit errors for features
outside that contract. Stored completions and their list/retrieve/update/delete
and messages routes are later work. This is a useful, deliberately bounded
compatibility surface, not a claim of complete OpenAI API compatibility.

## Dependency and implementation boundaries

1. Project-owned C is compiled with explicit `-std=c89`, strict pedantic checks,
   and warnings as errors. Do not express this as CMake `C_STANDARD 90`.
2. Private bridge implementation may use the C++ standard required by upstream.
   C++ exceptions must never cross a C boundary. Public structs, callbacks,
   enums, constants and prototypes must compile as strict C89 and as C++.
3. Pin llama.cpp b11430, revision
   `8345f333951c661d166b00e6f9362e553768f292`. All public C APIs means the public
   llama library, GGML core/backend/allocator/optimizer/GGUF and public backend
   headers, plus public multimodal `mtmd` and `mtmd-helper` APIs. C++ common,
   server utilities, private backend interfaces and CLI programs are excluded.
4. Facade declarations and wrappers use the `ailo_` namespace. Completeness
   includes deprecated declarations, every enum value and public constant,
   callback signatures, public record fields, unions, pointer levels, array
   extents, ownership and returned borrowed views. Renaming or skipping an
   inconvenient declaration does not satisfy completeness.
5. Maintain a checked-in upstream inventory and deterministic facade generation
   where appropriate. Compare inventory to actual pinned public headers.
   Missing functions/types/fields/constants/callbacks, unexplained extras,
   unsupported conversions, or unmapped conditional declarations fail the gate.
   Compile/link coverage alone does not prove callback or value conversion.
6. Native booleans and exact-width integers need explicit C89 representations
   and conversions. Do not cast incompatible callback function pointers, assume
   `long` is always 64 bits, or copy records solely because their sizes match.
   Preserve full 64-bit ranges, array behavior, field layout semantics, and
   callback lifetimes, including custom samplers and optimizer hooks.
7. Backend-specific APIs remain conditional on the corresponding built backend;
   the inventory records their conditions. CPU and Vulkan are required initial
   build capabilities. CUDA is excluded under the
   [backend support policy](../README.md#backend-support-policy). A missing
   optional backend implementation must not be misrepresented as successful
   support. Upstream API inventory does not imply a supported backend.
8. Use released SDK bundles from
   [c89systems/cpkt](https://github.com/c89systems/cpkt/releases/tag/v0.3.0)
   v0.3.0 for curl/OpenSSL, independent
   [cpktmisc](https://github.com/c89systems/cpktmisc/releases/tag/v0.2.0)
   v0.2.0 for private libffi 3.8.0 callback support, and
   [sa6mwa/lonejson](https://github.com/sa6mwa/lonejson/releases/tag/v0.45.0)
   v0.45.0 for JSON. Pin exact per-target asset URLs and SHA-256 digests. Do not
   switch to c89systems/cpkt. No project-owned JSON parser or escaping code.
9. External curl, OpenSSL, lonejson and their dependencies remain independent
   libraries. Never concatenate their object files into `libailo.a`. Keep native
   llama/GGML/mtmd and C++ runtime link dependencies explicit as separate archives
   for static consumers. Public installed headers contain only project APIs.

## Low-level facade

The facade is a separate layer below the receiver and can be consumed without
the high-level JSON handler. It faithfully represents upstream loading,
quantization, vocabulary/tokenization, context/batch evaluation, logits and
embeddings, sampling and custom sampling, memory/state, chat templates, adapter
and control vectors, optimization, tensor/graph construction and execution,
backend scheduling/devices/buffers, GGUF and multimodal operations.

Default parameter factories preserve upstream defaults. Native-owned handles
stay opaque. Records and adapters document allocation/free, borrowed lifetime,
mutation and thread rules. Configurable callbacks have correctly typed native
trampolines and explicit ownership; native callbacks returned to C89 consumers
remain callable without incompatible function-pointer casts. No global
single-callback slot may substitute for independent registrations.

The low-level API preserves native success/error semantics where possible;
additional bridge allocation/conversion failures are explicit. APIs unsafe on
invalid native inputs retain documented preconditions. Tests must never exercise
native assertions by passing arbitrary invalid handles.

## Receiver, errors and concurrency

`ailo_client_new(config, out_client, error)` returns an allocated receiver shell
whose mutable state is private. Methods have the receiver as the first argument:
`client->chat_completions(client, ...)`, `client->models(client, ...)`,
`client->model(client, ...)`, `client->handle(client, ...)`, and
`client->destroy(client)`. Equivalent `ailo_client_*` functions are exported.
Destroy and project-owned string cleanup tolerate NULL. Fallible constructors
set output handles to NULL before validation.

Zero-initializable configuration specifies a local model path or Hugging Face
repo/revision/filename, served model ID, cache override, explicit download
permission, eager or lazy loading, backend policy (`auto`, `cpu`, `vulkan`), GPU
layer count, context size, batch size, threads and default generation limits.
Strings are copied by construction. No default model silently triggers network
access. Backend defaults to auto, CPU remains available, and diagnostics identify
the backend actually used. Explicit Vulkan can fail clearly; auto retries model
and context setup on CPU if GPU setup fails. Unsupported architecture, invalid
GGUF, and CPU failure return actionable errors rather than synthetic output.

Status and detailed error objects distinguish invalid arguments, malformed JSON,
unsupported features, unknown model, download/cache failure, model load/evaluate
failure, context overflow, allocation failure, busy and cancellation. Error text
must not include bearer tokens. Handler errors use OpenAI-shaped error JSON and
appropriate HTTP status metadata; no sockets or Kore types enter the library.

Each client serializes access to its mutable inference context and rejects
overlapping calls with busy. Different clients can operate independently. Lazy
load transitions and download publication are synchronized. Callback reentry
into the same client returns busy; cancellation is safe during generation.
Process-wide backend/curl initialization is synchronized and reference-counted;
destroying one client must not invalidate another client or low-level context.

## Chat Completions contract

The receiver accepts UTF-8 request JSON and uses lonejson for validation,
inspection and response serialization. It supports `model`, ordered `messages`
with `system`, `developer`, `user` and `assistant` text roles, `stream`,
`max_tokens`/`max_completion_tokens`, `temperature`, `top_p`, `seed`, `stop`
(string or up to four strings), and `stream_options.include_usage`.
Text content parts are accepted; nontext parts and tool calls return explicit
unsupported-feature errors. Missing required data, invalid types/ranges,
conflicting limits, unknown fields with meaningful unsupported semantics, and
unknown model IDs fail before generation. `n` defaults to 1 and other values,
tools/tool choice, logprobs, nontext modalities, structured outputs and `store`
true are unsupported initially. Model capabilities are not inferred from its
filename. No silent ignoring of functionality requested by the caller.

Apply the model's chat template through the native library; an explicit template
override may be configured. If the model requires a template the native public
API cannot apply, report that limitation. Prompt tokenization, context-window
checks, prompt batching, sampler setup, EOS handling and per-token decoding use
the actual model. Every request starts independent context state.

Buffered results contain `id`, `object: chat.completion`, `created`, `model`,
one indexed assistant choice with content and `finish_reason`, and token usage.
EOS/stop yields `stop`; exhausted output budget yields `length`. Stop matches
are removed from output. Token counts reflect actual tokenization/evaluation.

Streaming invokes a synchronous sink during generation with SSE bytes, beginning
with an assistant role chunk, then content deltas, a finish chunk, optional usage
chunk, and `data: [DONE]`. Stable ID/time/model apply across all chunks. Sink
return failure cancels promptly, stops evaluation, and does not emit a false
successful finish. Cancellation likewise does not claim successful completion.
The sink consumes all bytes passed before returning; memory is borrowed only
during that call. This provides bounded backpressure and avoids partial-write
ambiguity in the public callback contract.

Streaming may retain bounded token-piece/UTF-8 carry and stop-prefix state; it
must not build the whole completion, collect all SSE frames, spool to disk, or
generate buffered output first. Fragmented UTF-8 and stop strings crossing token
boundaries must be correct. Request JSON and the prompt may be materialized;
the generated streaming output may not.

## Transport-neutral handler and model routes

`handle` dispatches method/path plus JSON bytes into status/content-type metadata
and a response sink. Implement `POST /v1/chat/completions`, `GET /v1/models`, and
`GET /v1/models/{id}`. Unknown paths return 404; unsupported methods return 405.
Do not register placeholder success routes for stored completions or Responses.
Model list/retrieve expose configured served models without leaking local file
paths or forcing a lazy download. Model ID must match the configured identity.
Authentication, HTTP framing, TLS, CORS and connection management belong to the
embedding server. Midstream failures return library status; the embedding server
owns closing an already-started HTTP response.

## Hugging Face acquisition and model cache

Provide standalone C89 model acquisition helpers also usable by construction
and lazy loading. Direct pinned `https://huggingface.co/{repo}/resolve/{revision}/`
URLs suffice; avoid a Hugging Face metadata API when it adds no needed behavior.
Validate/encode repo, revision and filename components, reject traversal and
absolute filenames, and keep distinct repo/revision/file identities separate.
Support nested model filenames and split-GGUF sibling shards when requested.

Default cache root is absolute `$XDG_CACHE_HOME/ailo`, otherwise
`$HOME/.cache/ailo`; an explicit absolute root takes precedence. Invalid or
missing environment paths fail clearly. Offline existing-file use never probes
the network. Missing files fail unless downloads were explicitly permitted.
Local model paths are not copied into the cache.

Use SDK libcurl with certificate and hostname verification, bounded redirects,
HTTPS-only origins/redirects, connect/transfer timeouts, bounded retries,
optional progress/cancellation and size limits. Authentication is explicit and
must not be forwarded to unrelated redirect hosts. Stream download bytes to a
same-directory exclusive partial file. Validate HTTP success, actual byte count,
GGUF signature and an optional caller-supplied SHA-256 before atomic rename.
Checksum-pinned hits are revalidated. File existence alone does not prove a
valid model. Failed, cancelled and interrupted downloads must never publish a
partial final file. Serialize publications per model and never overwrite a
caller-owned model silently. Make resumability explicit if later added.

The model cache and lifecycle dependency archive cache are different contracts.
The library owns only requested files under its model-cache root; build cleanup
never deletes downloaded models or either shared lifecycle cache.

## Lifecycle, packages and verification

Follow the lifecycle skill's local Make/CMake structure, pinned Bootlin C/C++
collection resolution before `project()`, shared checksum-verified dependency
archive cache, disposable per-target extraction/build/install roots, strict
C89 flags, local executable runtime helper, exact lightweight-tag versioning,
static/shared targets, examples, install metadata, packaging and checksum gates.
`make help` is the command index. No remote CI scaffold or automatic push.
Build, dependency, formatting, cleanup and verification orchestration uses Bash
and CMake scripts. Keep Make recipes as simple calls to those scripts, without
embedded escaped shell programs. Python is reserved for tasks that materially
need it, currently compiler AST inventory processing and test HTTP fixtures.

Linux target presets cover x86_64/aarch64 with glibc and musl.
ARM hard-float is outside the supported shipment and verification matrix.
Cross-target ABI proof includes all exact-width conversions. Vulkan uses pinned headers,
loader and SPIR-V headers with a usable host shader compiler, never an implicit
host C/C++ compiler fallback. It is enabled in the default native build and CPU
is always built. Missing Vulkan build prerequisites fail with actionable
diagnostics; an explicit CPU-only build is a separate configuration. Runtime GPU
availability must be measured, not inferred from installed ICD filenames.

Shared exports are restricted to the exact project allowlist, with native C++
symbols and dependency internals hidden. Static archives retain explicit link
closure through relocatable CMake/pkg-config metadata. Initial shared ABI is 0;
later changes follow released-artifact ABI policy. No release/tag/push is
authorized by this implementation request.

Acceptance requires:

- Reviewed spec and an upstream inventory with no unexplained facade omissions.
- Standalone strict-C89/C++ headers, compile/link coverage of all public APIs,
  callback and record conversion tests, boundary-width tests, custom sampler and
  native model/context/state/GGUF/tensor/multimodal lifecycle tests.
- Actual GGUF inference for buffered and incremental streamed text; CPU and
  Vulkan backend probes and fallback tests. A small pinned deterministic fixture
  is appropriate for tests; Qwen2.5-3B-Instruct-Q4_K_M is a supported live smoke
  example, not a mandatory multi-gigabyte unit-test dependency.
- Request validation, model routes, native output shape/usage, limits, UTF-8,
  stop boundaries, cancellation, sink failure, independent clients, busy/reentry
  and repeated destroy/load behavior.
- Offline/local deterministic download tests for cache precedence, traversal,
  wrong content/HTTP errors/checksum mismatch/truncation, cancellation,
  concurrent publication, no-network hits and partial cleanup. Production
  transport remains HTTPS-only; test injection cannot enable insecure downloads
  in the exported API.
- Lifecycle archive cache contract tests, formatting, standard CTest checks,
  native Valgrind, bounded parser fuzzing, examples, static/shared installed
  consumers, exact exports/imports, package privacy/relocatability, and source
  archive reconstruction through named lifecycle gates.

## Specification review

The review resolved the following before implementation:

- “Complete” is relative to a pinned public C API inventory, includes multimodal
  and public transitive GGML APIs, and excludes C++ server/common utilities.
- Chat compatibility is bounded explicitly; Responses and stored-completion
  persistence are not silently promised by a text completion implementation.
- Compiler/link/runtime, dependency archive and model caches have separate
  ownership boundaries. Curl/OpenSSL never become members of `libailo.a`.
- True streaming includes stop/UTF-8 carry, sink backpressure and cancellation;
  full-result buffering cannot pass its acceptance test.
- Exact-width values, custom callbacks, native record fields and optional
  backends require coverage beyond a generated function-name list.
- Missing hardware or build prerequisites limit evidence; they cannot be used
  to claim Vulkan execution or facade completion without proof.

Implementation may refine internal structure and naming within this contract.
Material API, capability, persistence, dependency or release changes require a
production decision. Acceptance evidence must distinguish completed gates from
pending work; an unfinished facade must never be described as complete.
