# Ailo implementation specification

Status: reviewed specification, updated 2026-10-14; full implementation followed
by thorough acceptance testing. Pending capabilities are not release claims.

## Intent and scope

Ailo runs GGUF models locally through llama.cpp. `libailo` supplies a C89
receiver shell and equivalent free functions for applications that ordinarily
call OpenAI Chat Completions. It also supplies a complete C89 facade over the
pinned llama.cpp public C library APIs. Neither public boundary exposes C++
types, C++ headers, upstream headers, or compiler extensions disguised as C89.
There is no production HTTP listener in this project. Vectis/Kore integration
is later work. Chat/Responses wire adapters and in-process routing are implemented;
the [serving handler](serving.md) adds a framework-independent transport boundary.
The inference receiver supports the
shared generation, state, caching and compaction contract in
[inference-backend.md](inference-backend.md), so future API adapters do not
reimplement inference or force a role/content-only backend redesign.

The receiver implements Chat Completions generation, buffered output and real
streaming, tools, constrained output, model listing and model retrieval, with
actual model capabilities and explicit errors for unavailable behavior. Stored
completions and their list/retrieve/update/delete and messages routes use an
explicit application-owned repository provider. Backend prompt caching and
semantic compaction are separate from stored completion records. Public API
compatibility claims follow verified wire contracts, not the native inventory.

Distributed coordinator/participant inference over authenticated TCP/IP is a
future direction outside the first few versions. Its direct activation-routing
architecture is recorded in [inference-backend.md](inference-backend.md#future-distributed-coordinatorparticipant-backend);
it does not defer the required local multi-GPU implementation.

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
   server utilities, private backend interfaces and CLI programs are excluded
   from the public facade inventory. Selected private template/schema helper
   sources may be used internally under the inference backend contract.
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
   GPU userspace dependencies must satisfy the
   [permissive dependency assessment](gpu-userspace-dependencies.md); headless
   RADV and ANV are the selected qualification paths. Linux ROCm/HIP is excluded
   by its required LGPL libnuma dependency. Fully static GPU support requires
   final-executable and hardware evidence without shared-object loading.
   The required implementation is described in
   [static Vulkan and multi-GPU deployment](static-vulkan.md), including both
   providers and multi-GPU layer splitting. Single-GPU qualification does not
   replace the multi-GPU requirement.
8. Use released SDK bundles from
   [c89systems/cpkt](https://github.com/c89systems/cpkt/releases/tag/v0.5.0)
   v0.5.0 for curl/OpenSSL, independent
   [cpktmisc](https://github.com/c89systems/cpktmisc/releases/tag/v0.2.0)
   v0.2.0 for private libffi 3.8.0 callback support, and
   [sa6mwa/lonejson](https://github.com/sa6mwa/lonejson/releases/tag/v0.45.0)
   v0.45.0 for JSON. Pin exact per-target asset URLs and SHA-256 digests. Do not
   replace lonejson with a different JSON implementation when adopting cpkt.
   No project-owned JSON parser or escaping code.
9. External curl, OpenSSL, lonejson and their dependencies remain independent
   libraries. Never concatenate their object files into `libailo.a`. Keep native
   llama/GGML/mtmd, selected private helpers and C++ runtime link dependencies
   explicit as separate archives for static consumers. Public installed headers
   contain only project APIs.

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
`client->model(client, ...)`, `client->model_card(client, ...)`,
`client->devices(client, ...)`, `client->handle(client, ...)`, and
`client->destroy(client)`. Equivalent `ailo_client_*` functions are exported.
The neutral `generate`, `compact`, `capabilities`, `cache_stats` and `cache_clear`
methods are specified in the inference backend contract. Chat JSON/SSE is an
adapter over that neutral generation path.
Destroy and project-owned string cleanup tolerate NULL. Fallible constructors
set output handles to NULL before validation.

Zero-initializable configuration specifies a local model path or Hugging Face
repo/revision/filename, served model ID, cache override, explicit download
permission, eager or lazy loading, backend policy (`auto`, `cpu`, `vulkan`), GPU
layer count, selected device identities and layer distribution, context size,
batch size, threads and default generation limits. Acquisition options use the
same `ailo_model_source` contract as the standalone helper, including checksums,
shards, progress/cancellation, credentials, timeouts and byte limits.
Configuration also owns explicit memory/disk cache limits, namespace/key policy,
compaction thresholds/retention/work limits and request resource ceilings.
Configuration strings and arrays are copied by construction; callback user
state remains caller owned until client destruction. No default model silently
triggers network access. Backend defaults to auto, CPU remains available, and
diagnostics identify the backend actually used. Explicit Vulkan can fail clearly; auto retries model
and context setup on CPU if GPU setup fails. Unsupported architecture, invalid
GGUF, and CPU failure return actionable errors rather than synthetic output.
An explicit device selection or layer distribution is a placement commitment:
fail clearly rather than silently changing devices or falling back to CPU.
Report actual layer placement, CPU fallback operations and KV-cache placement
separately from requested settings. The `devices` method and equivalent free
function return an owned JSON snapshot of available built-in providers and
usable devices; release with `ailo_string_free`. Enumeration needs no model
download. Selection and memory-reporting rules are in the static Vulkan spec.

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
with `system`, `developer`, `user`, `assistant` and `tool` roles, `stream`,
`max_tokens`/`max_completion_tokens`, `temperature`, `top_p`, `seed`, `stop`
(string or up to four strings), and `stream_options.include_usage`.
The expanded contract includes `n`, participant names, function-tool calls and
results, `tools`/`tool_choice`/`parallel_tool_calls`, `response_format`,
frequency/presence penalties, `logit_bias`, `logprobs`/`top_logprobs` and the
cache controls whose semantics are implemented. Reasoning/media controls are
model/adapter dependent, not silently flattened into text. Missing required
data, invalid types/ranges, conflicting limits, unknown fields with meaningful
unsupported semantics and unknown model IDs fail before generation. Stored
completion records require a configured handler repository; prompt cache state
does not implement `store`. Model capabilities are not inferred from its
filename. No silent ignoring of functionality requested by the caller.

Apply the model's chat template through the private native helper adapter;
an explicit template override may be configured. The limited public template
helper alone does not fulfill tool/Jinja behavior. Prompt tokenization,
context-window checks, prompt batching, sampler setup, EOS handling and per-token decoding use
the actual model. Each logical request has independent generation/sampling state;
exact compatible prompt prefixes may reuse native evaluation without inheriting
another request's answer or mutable sampler. No silent context truncation.

Buffered results contain `id`, `object: chat.completion`, `created`, `model`,
indexed assistant choices with content or tool calls and `finish_reason`, and
token usage.
EOS/stop yields `stop`; exhausted output budget yields `length`. Stop matches
are removed from output; tool-call completion has its own documented outcome.
Token counts reflect actual tokenization/evaluation, including actual cached
prompt counts. Compaction work is reported separately from answer generation.

Streaming invokes a synchronous sink during generation with SSE bytes, beginning
with an assistant role chunk per choice, then content/tool deltas, finish chunks,
an optional usage chunk, and `data: [DONE]`. Stable ID/time/model apply across all chunks. Sink
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

`client->handle` dispatches method/path plus JSON bytes into status/content-type
metadata and a response sink. The complete implemented route table, including
stored completions, Responses and conversations, is in [openai.md](openai.md).
Unknown paths return 404; unsupported methods return 405.
Model list/retrieve expose configured served models without leaking local file
paths or forcing a lazy download. Model ID must match the configured identity.
Authentication, HTTP framing, TLS, CORS and connection management belong to the
embedding server. Midstream failures return library status; the embedding server
owns closing an already-started HTTP response. `ailo_handler` supplies the
begin/write/finish/abort lifecycle above this dispatcher without a listener.

## Hugging Face acquisition and model cache

Automatic acquisition is a normal library capability, not a testing utility or
an external downloader prerequisite. `ailo_client_new` accepts a local source or
an explicit Hugging Face source. Eager construction acquires and loads it; lazy
construction copies/validates configuration and the first operation needing
weights acquires and loads it. Both use the standalone C89 acquisition facility;
tests exercise the same path as applications.

Direct pinned `https://huggingface.co/{repo}/resolve/{revision}/`
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
Inference snapshots use a separate bounded, encrypted opt-in subdirectory and
are evicted independently; they must never delete model weights. Canonical
conversation records, if configured by a future handler, are not cache entries.

## Structured model card

The receiver exposes structured JSON metadata for its actual loaded model,
through `client->model_card(client, out_json, error)` and
`ailo_client_model_card(client, out_json, error)`, both returning `ailo_status`.
Output is an owned NUL-terminated UTF-8 string, released with `ailo_string_free`;
set it to NULL on failure. This call performs the configured acquisition/load
when lazy, follows the client's busy/cancellation rules, and cannot describe an
unloaded file as a verified model. Model list/retrieve retain their existing
OpenAI-compatible shape and do not force loading. No new HTTP route or original
Hugging Face Markdown card is required.

Use lonejson to construct and validate the card. Its documented fields are:

| Field | Meaning and evidence |
| --- | --- |
| `id`, `name` | Configured served identity and GGUF model name, respectively |
| `source` | Local or Hugging Face origin; remote repo, requested revision and resolved immutable revision when known |
| `license` | Identifier/name/link when present, with the source of the claim; unknown is null |
| `architecture` | GGUF architecture actually accepted by the native loader |
| `parameter_count` | Native llama_model_n_params count: total tensor elements, including stored experts, not active MoE parameters |
| `quantization` | GGUF file type and tensor-type distribution; mixed precision is represented explicitly |
| `context` | `trained_tokens`, `original_tokens`, `scaling`, `configured_tokens` and `effective_tokens` distinguish declarations from the loaded limit |
| `chat_template` | Presence, selected template identity and whether the receiver can apply it; an override is identified as caller supplied |
| `capabilities` | Receiver-supported text/modality/features for this model and implementation; declarations alone do not establish support |
| `files` | Ordered shard sizes and validated digests when available |
| `provenance` | Origin of supplemental claims and any conflicts or unavailable metadata |

Parameter counts and file byte counts use decimal strings for lossless 64-bit
representation across C89 and JSON clients. Context limits are JSON integers.
`source` is an object with `kind`, `repo`, `requested_revision` and
`resolved_revision`; remote identifiers are null for local models. `license` is
an object with `id`, `name`, `url` and `origin`, or null when unknown.
`architecture` and `name` are strings or null. `files` is an ordered array of
objects with `filename`, `bytes`, `sha256` and `checksum_validated`; expose a
local basename only. `quantization` describes the native file type and an array
of tensor types/counts, without treating mixed tensors as uniformly quantized.
`provenance` records claim origins and conflicting values. Other nested fields
must have documented types in the installed API documentation.

Unknown scalar values are null; unknown collections are null rather than an
invented empty result. A checksum is labelled validated only after bytes were
actually checked. No host paths, bearer tokens or credential-bearing URLs enter
the card. A filename is never evidence for architecture, size, quantization,
license, instruction tuning or supported capabilities.

GGUF/native facts are authoritative about the loaded artifact. Publisher license
and lineage are claims with provenance, not a legal conclusion or proof of model
quality. Configuration may supply a bounded supplemental JSON metadata object
for local or remote models; copy and validate it at construction. It may provide
name, license and source lineage, but cannot override loaded-model facts.

For Hugging Face sources where GGUF metadata is insufficient, the acquisition
facility attempts to retrieve structured repository metadata for the same
immutable revision when downloads are permitted. Cache only the selected bounded
JSON metadata alongside the model, with atomic publication and identity checks. Never combine
a mutable branch's current metadata with earlier cached weights. If an immutable
identity cannot be established, leave publisher fields unknown and expose that
limitation. Metadata HTTP failure/missing fields do not invalidate successfully
loaded weights; malformed caller-supplied metadata fails configuration.
Existing cached metadata and GGUF facts must remain usable offline. Card access
never bypasses download permission to fill missing fields; no YAML/Markdown
parser or additional target dependency is introduced.

Card generation reads native metadata without inference. Limit each supplemental
or remote metadata input to 1 MiB and the resulting card to 4 MiB, with actionable
errors for oversized caller input/output. Verify all shard identities,
and preserve conflicts in provenance rather than silently replacing facts.

## Trained inference fixture

Use a small trained instruct model through the receiver's ordinary acquisition
and cache path for CPU and available Vulkan hardware checks:

- Repository: `bartowski/SmolLM2-135M-Instruct-GGUF`.
- Immutable revision: `09816acd5d99df7be770d85ea30822623dab342c`.
- File: `SmolLM2-135M-Instruct-Q4_K_M.gguf`.
- Size: 105,454,432 bytes (about 100.57 MiB).
- SHA-256: `2e8040ceae7815abe0dcb3540b9995eaa1fa0d2ca9e797d0a635ae4433c68c2d`.

The [base instruct model](https://huggingface.co/HuggingFaceTB/SmolLM2-135M-Instruct)
and [GGUF publisher](https://huggingface.co/bartowski/SmolLM2-135M-Instruct-GGUF/tree/09816acd5d99df7be770d85ea30822623dab342c)
declare Apache-2.0. The revision, size and checksum were inspected in repository
metadata. Trained CPU and static Intel ANV inference with this pin are recorded
in [the static deployment evidence](static-vulkan.md#current-evidence). This replaces
the earlier Qwen2.5-3B smoke recommendation, whose
[publisher declares the Qwen research license](https://huggingface.co/Qwen/Qwen2.5-3B-Instruct).
Model licensing is independent of library licensing.

Keep tiny generated fixtures for deterministic boundary tests. They cannot
establish trained chat-template/inference behavior. Ordinary tests remain local;
an explicit live integration gate acquires this pin, generates bounded text on
CPU and Vulkan, reads its card, then proves cache reuse with networking disabled.
On this host use the detected Vulkan GPU when available, recording its actual
identity. An absent GPU is reported as unavailable evidence, never a passed
GPU test. Larger dense and MoE model qualification is additional coverage;
this small fixture cannot prove those workloads or frontier-level quality.

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
Cross-target ABI proof includes all exact-width conversions. Vulkan uses pinned
headers, SPIR-V headers and the static loader/provider integration specified in
[static-vulkan.md](static-vulkan.md), with declared host shader/source generators
and no host target-library or implicit C/C++ compiler fallback. It is enabled
in the default native build and CPU is always built. Missing Vulkan build
prerequisites fail with actionable diagnostics; an explicit CPU-only build is
a separate configuration. Runtime GPU
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
  Vulkan backend probes and fallback tests, trained SmolLM2 receiver/cache/card
  integration, both static providers, and AMD/Intel/mixed multi-GPU layer splits
  under the detailed static Vulkan acceptance contract.
- Request validation, model routes, native output shape/usage, limits, UTF-8,
  stop boundaries, cancellation, sink failure, independent clients, busy/reentry
  and repeated destroy/load behavior.
- The complete inference-backend acceptance contract: multiple candidates,
  tool/schema/reasoning capabilities, typed events/state, real prefix reuse,
  persistent snapshot integrity/isolation, semantic compaction and authenticated
  opaque replay, with API-specific wire fixtures and bounded streaming memory.
- Offline/local deterministic download tests for cache precedence, traversal,
  wrong content/HTTP errors/checksum mismatch/truncation, cancellation,
  concurrent publication, no-network hits and partial cleanup. Production
  transport remains HTTPS-only; test injection cannot enable insecure downloads
  in the exported API.
- Model-card identity, exact counts, quantization, declared versus effective
  context, unknown/absent metadata, conflicting publisher/caller claims,
  shard consistency, lazy loading, offline cached metadata, corrupt/oversized
  metadata, ownership and privacy. Changing the configured source cannot reuse
  a different artifact's card.
- Lifecycle archive cache contract tests, formatting, standard CTest checks,
  native Valgrind, bounded parser fuzzing, examples, static/shared installed
  consumers, exact exports/imports, package privacy/relocatability, and source
  archive reconstruction through named lifecycle gates.

## Specification review

The review resolved the following before implementation:

- “Complete” is relative to a pinned public C API inventory, includes multimodal
  and public transitive GGML APIs, and excludes C++ server/common utilities from
  that public inventory while allowing selected private helper dependencies.
- A common typed inference backend supports API adapters, caching, state and
  compaction. Responses wire integration and stored-record handlers remain
  explicit integration work, not implicit features of a text-only wrapper.
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
