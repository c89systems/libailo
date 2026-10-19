# Multimodal receiver design

Status: design review, 2026-10-19. This document tightens the next receiver
contract; it does not claim implemented high-level media inference. The scope
decision between image/audio input with text output and additional generated
speech/video input remains open. No runtime implementation starts on that
decision's dependent work until it is resolved.

## Model evidence and qualification candidates

None of the current fixtures qualifies multimodal inference. The deterministic
one-layer GGUF is an untrained text execution fixture. SmolLM2-135M-Instruct,
SmolLM2-1.7B-Instruct and Granite-3.1-1B-A400M-Instruct are text models. Adding an
unrelated projector does not turn one of these artifacts into a vision model.

The pinned llama.cpp b11430 includes SmolVLM and Qwen3-ASR in its supported model
list. Implementation facts below are checked against that pin's
[mtmd API](https://github.com/ggml-org/llama.cpp/blob/b11430/tools/mtmd/mtmd.h),
[media helpers](https://github.com/ggml-org/llama.cpp/blob/b11430/tools/mtmd/mtmd-helper.h)
and [build configuration](https://github.com/ggml-org/llama.cpp/blob/b11430/tools/mtmd/CMakeLists.txt).
Suitable small candidates are:

| Candidate | Input/output | Language GGUF | Encoder/projector GGUF | Combined download |
| --- | --- | --- | --- | --- |
| SmolVLM-256M-Instruct, Q8_0 | Images and text to text | 175,054,528 bytes | 103,769,856 bytes | 278,824,384 bytes (266 MiB) |
| Qwen3-ASR-0.6B, Q8_0 | Speech to transcription text | 804,749,248 bytes | 214,392,480 bytes | 1,019,141,728 bytes (972 MiB) |

These are artifact sizes, not peak RAM/VRAM requirements. Both are candidates,
not local semantic or static-musl/Vulkan qualification evidence.
[SmolVLM's publisher](https://huggingface.co/HuggingFaceTB/SmolVLM-256M-Instruct)
specifies image/text input and text output under Apache-2.0. Its language
component descends from SmolLM2, but the qualified multimodal pair is required.
[Qwen's publisher](https://huggingface.co/Qwen/Qwen3-ASR-0.6B) specifies Apache-2.0
speech recognition. ASR success does not establish general audio conversation,
tool use, semantic compaction or speech generation.

Pin these observed candidate manifests before writing download tests:

| Repository/revision | File | SHA-256 |
| --- | --- | --- |
| `ggml-org/SmolVLM-256M-Instruct-GGUF` / `b9e4379657e1450d04d02eec8e345667265b0a00` | `SmolVLM-256M-Instruct-Q8_0.gguf` | `2a31195d3769c0b0fd0a4906201666108834848db768af11de1d2cef7cd35e65` |
| Same revision | `mmproj-SmolVLM-256M-Instruct-Q8_0.gguf` | `7e943f7c53f0382a6fc41b6ee0c2def63ba4fded9ab8ed039cc9e2ab905e0edd` |
| `ggml-org/Qwen3-ASR-0.6B-GGUF` / `928ab958557df9aa2ef1c93e0e83c7ad0933fae2` | `Qwen3-ASR-0.6B-Q8_0.gguf` | `bca259818b50ca7c4c05e9bdb35a5dc04fa039653a6d6f3f0f331f96f6aa1971` |
| Same revision | `mmproj-Qwen3-ASR-0.6B-Q8_0.gguf` | `41a342b5e4c514e968cb756de6cd1b7be39eff43c44c57a2ef5fc6522e36603d` |

Sizes/digests were read from the repositories' Hugging Face metadata, not from
downloaded files: [vision metadata](https://huggingface.co/api/models/ggml-org/SmolVLM-256M-Instruct-GGUF/revision/b9e4379657e1450d04d02eec8e345667265b0a00?blobs=true)
and [audio metadata](https://huggingface.co/api/models/ggml-org/Qwen3-ASR-0.6B-GGUF/revision/928ab958557df9aa2ef1c93e0e83c7ad0933fae2?blobs=true).
Acquisition must verify the actual bytes. The Qwen GGUF card links its original
model but omits a license field; carry the upstream license and conversion
provenance explicitly rather than inventing GGUF metadata.

## Receiver and acquisition boundary

Keep one high-level inference receiver, with the existing C89 shell/free-function
and Lua contracts. Add optional multimodal constructor configuration containing
an explicit projector `ailo_model_source`, encoder CPU/Vulkan selection, an
optional stable Vulkan device ID, and resource limits. Reuse the model source's
checksums, immutable revisions, explicit download permission, cancellation,
credentials and configurable `$XDG_CACHE_HOME/ailo` cache.

Copy configuration at construction; lazy construction performs no acquisition.
Loading validates both artifacts before publishing a usable receiver. A failed
pair may leave independently validated files cached, but never a partially
initialized receiver. Never infer a projector from an arbitrary filename, select
a different model silently, or download unrequested weights. Pair identity
includes both content digests, template and preprocessing configuration.

Use the existing image/audio content kinds for finite encoded payload bytes,
with explicit size and borrowed-call lifetime. Internal canonical parts also
carry validated format, detail policy, immutable content digest and source
provenance. Continuations own their retained bytes; they cannot retain pointers
into caller memory. Lua strings are binary-safe and follow the same ownership
rules. URL/file IDs use a separately configured application resolver which
returns bounded bytes; the inference engine performs no arbitrary URL fetching
or local-path interpretation of an untrusted request.

Changing an existing public struct layout requires the shared ABI change
prescribed by the lifecycle: inspect the latest released artifact and advance
that released ABI by one if necessary. Do not change an independent storage or
wire version merely because this pre-1.0 implementation changes.

## Execution and streaming

The in-process path is:

```text
ordered text/media parts
  -> validate capabilities, resolve explicitly, decode/preprocess media
  -> render the model's chat template with tracked media positions
  -> mtmd text/media chunks -> encoder/projector embeddings
  -> llama mixed text/embedding evaluation -> ordinary sampling
  -> existing text/tool/reasoning deltas -> lonejson -> caller sink
```

Initialize mtmd against the loaded language model. Query its actual vision/audio
support; a file header or architecture name alone is insufficient. Report
configured, loaded and qualified capabilities distinctly. Lazy capability
queries must not download weights. Model cards identify both artifacts,
per-artifact licensing/source, preprocessing, supported input/output modalities,
limits and requested/actual encoder placement.

Preserve ordered parts across turns, including multiple images and interleaved
text. Use `mtmd_tokenize_from_parts` after resolving template media spans. Never
interpret a user's literal `<__media__>` as an attachment. Template integration
must prove a one-to-one mapping between each retained media part and its rendered
position; missing, duplicated, ambiguous or reordered spans fail explicitly.
Trusted temporary sentinels must be collision-checked against untrusted content
and converted to typed parts, not passed to a fixed-marker scanner. Template
qualification remains model-specific.

Use mtmd's actual token counts, position counts, M-RoPE coordinates and causal
mask requirements. An image is not one ordinary token. Check the complete
mixed prompt plus requested generation reservation against context capacity
before text generation. Usage reports local evaluated media tokens and actual
cache reuse, without borrowing OpenAI's proprietary image billing formula.

Finite encoded media and decoded RGB/audio arrays are explicitly bounded,
materialized inputs required by this native API. This is not live camera/audio
streaming. Bound memory across concurrent receivers as well as per request.
The generated answer continues to stream each available delta synchronously;
media processing must not introduce answer/tool-argument accumulation. Report
preprocessing separately from time to first generated token.

Serialize mutable encoder/context access using the receiver's busy rules.
Cancellation applies to acquisition, resolution, preprocessing, encoder graph
evaluation and llama evaluation. mtmd exposes loading progress and a scheduler
evaluation callback; connect those to the existing cancellation state, checking
at safe boundaries. In-flight device kernels need not be interruptible.
Cancellation/failure before publication aborts staged continuation/cache/
repository updates and leaves the receiver reusable. A transport failure after
durable commit retains the existing committed-record semantics. Worker
diagnostics use the existing logger
scope, with no unsolicited standard-stream output or payload logging.

## Dependencies, formats and GPU placement

The pinned mtmd helper already compiles stb_image and miniaudio into the native
library. Both offer permissive license choices. Image/audio decoding introduces
no new host target libraries and does not need an external decoder executable.
Keep their upstream notices in the existing dependency inventory.

Qualify JPEG and PNG image inputs and WAV/MP3 audio inputs end to end; the direct
receiver may also accept FLAC after decoder qualification. Normalize audio to
the sample rate required by the loaded encoder, with explicit channel/sample
validation. Reject corrupt/truncated files, contradictory declared formats,
unsupported animation/formats, overflow dimensions and expansion beyond limits.
Decoder support alone does not establish a public supported-format claim.

Limits must cover encoded bytes per part and request, media-part count, decoded
pixels/samples and aggregate allocation, audio duration, media tokens, context
positions and resolver deadline. Preserve the serving layer's current 8 MiB
body ceiling unless a separate limit change is agreed; inline base64 consumes
that ceiling. A resolver/file-ID path can supply a larger bounded input without
raising the JSON body limit. Exact media defaults remain an implementation
contract to settle before coding; native soft batch limits are not admission
limits (mtmd can admit its first image beyond `batch_max_tokens`).

The text model retains its required multi-GPU layer splitting. mtmd selects one
encoder device through `mtmd_context_params.device`; do not imply that splitting
language layers splits the encoder too. Encoder selection must use the existing
stable device identities. Explicit selection fails on unavailable/offload failure;
automatic placement reports any CPU fallback. Both allocations and execution
need hardware evidence. CPU and static musl paths remain required.

## OpenAI mapping

Map media into the same canonical parts as direct C/Lua calls. Preserve public
wire views separately from private replay data; do not leak digests, resolver
credentials or internal fields into stored-message/item projections.

| Endpoint | Media mapping |
| --- | --- |
| Chat Completions | User `image_url` parts (URL or data URL) and `input_audio` parts with base64 `data` and `format` (`wav`/`mp3`). |
| Responses | `input_image` with `image_url` (including data URLs) or application-resolved `file_id`. |

These mappings follow the [Chat request reference](https://developers.openai.com/api/reference/resources/chat/subresources/completions/methods/create)
and [vision guide](https://developers.openai.com/api/docs/guides/images-vision).
Preserve `detail` and qualify its local preprocessing behavior; reject a detail
mode the selected adapter cannot honor instead of silently ignoring it.

The [official audio guide](https://developers.openai.com/api/docs/guides/audio-chat-completions)
currently directs this audio-chat pattern to Chat Completions, while describing
Responses text/image input. Do not invent Responses `input_audio` compatibility.
The direct receiver can still expose its qualified audio capability independently.
File/document extraction and a hosted Files API are separate application services;
accepting a file ID does not claim those services exist in libailo.

## Retention, replay, caches and compaction

Retained canonical input/state/item views must contain the actual bounded media
bytes and preprocessing identity required for replay, alongside original wire
references. Do not depend on a URL staying unchanged or a resolver still being
available after restart. Write those bytes incrementally, with bounded base64
framing where JSON is required, through the existing encrypted Pouch repository
sections and atomic transaction. No separate unencrypted media cache or database
is introduced. Charge encoded framing and all stored copies against admission
quotas before generation. Custom repositories receive the same canonical contract.

Media inherits the lifetime and trusted namespace of its retaining record/item.
Response expiry, explicit deletion, retained conversation items and open readers
keep their existing semantics. Stateless/ZDR operation creates no hidden durable
media files; attached conversations retain their separate documented lifetime.

Bind KV reuse/snapshots to model, projector, template, media bytes/order/detail,
preprocessing, native build and actual device layout. A media boundary must be
restorable as a complete compatible evaluation unit; do not match prefixes on
text tokens alone or reuse half an image. Fall back to recomputation on a safe
cache miss. mtmd chunk serialization saves metadata only: loaded placeholder
chunks cannot re-encode the original media. KV snapshots are never a replacement
for durable canonical inputs, and a cache miss must still replay correctly.

Protected/recent/pinned media-bearing turns retain their original parts. Semantic
compaction of eligible older media must give the compaction context the actual
media; filenames, marker strings or a fabricated transcript are not substitutes.
Preserve source digests/provenance in authenticated compaction state. A semantic
summary does not promise lossless recovery of summarized inputs. A model lacking
qualified media-summary capability returns unsupported/context-limit explicitly,
without altering history. Opaque replay retains the established authentication,
namespace and key-lifetime rules.

## Verification and remaining decisions

Before implementing, resolve the modality scope and exact admission defaults.
Generated speech needs its own audio-delta, voice/codec, cancellation and replay
contract; the pinned mtmd audio-generation API is experimental. Video needs a
timestamp/frame contract and a decoder choice: upstream's `MTMD_VIDEO` helper
launches `ffmpeg`, while this project disables subprocesses. WebP also follows
that external helper path in this pin. Neither facility is an implicit runtime
dependency or a supported high-level claim.

Implement the agreed scope completely before acceptance qualification. Keep
ordinary offline regression tests fast and separate trained-model suites:

- Unit/integration checks: ordered typed parts and marker injection, capability
  rejection, mismatched artifacts, acquisition cancellation/checksums, malformed
  base64/media, dimension/duration/allocation limits, busy/reentry and failure
  recovery. Use small deterministic inputs; no timing sleeps.
- Protocol/persistence checks: fragmented base64 parsing/serialization, C/Lua
  parity, immediate generated deltas, sink rejection, both wire image mappings,
  Chat audio, explicit unsupported fields, restart/offline continuation, tenant
  isolation, expiry/deletion, quota/commit failure and interrupted publication.
- Cache/compaction checks: projector/media/detail mismatch, complete media-boundary
  reuse, safe recomputation, protected turns, actual-media summary input,
  authenticated replay and cancellation without history mutation.
- Explicit trained-model suites: acquire the pinned pairs through the production
  cache facility, then reopen offline. Check several distinct image/question
  pairs and short speech transcripts, using semantic assertions appropriate to
  model quality. A text-only reply or successful decoder call is insufficient.
  Qualify CPU, fully static musl, Intel Vulkan and later AMD/multi-GPU hardware;
  report unavailable hardware separately. Record peak allocations, time to first
  delta and leak/sanitizer evidence in this suite, not unload regression tests.

Specification verification does not require downloading these weights or running
inference. Runtime qualification is deliberately still outstanding.
