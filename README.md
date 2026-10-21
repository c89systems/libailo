# libailo

Ailo is a local GGUF inference library with a strict C89 public boundary and a
receiver interface for text generation. The reviewed implementation
contract is in [docs/architecture.md](docs/architecture.md).

## Implementation state

The dependency/build foundation is present: pinned llama.cpp b11430, CPU and
Vulkan backends, public multimodal libraries, cpkt v0.5.0 and cpktmisc v0.2.0, lonejson
v0.45.0, libpslog v0.12.0, and pinned Vulkan/SPIR-V components. The static/shared
project libraries
provide the portable C89 scalar/error boundary and generated native bindings
described in [docs/c89-boundary.md](docs/c89-boundary.md) and
[docs/facade-bindings.md](docs/facade-bindings.md). The current bridge implements
1,083 upstream functions and 306 record fields, including native-backed arrays,
the public mtmd memory map, variadic formatting and C89 macro adapters. It is
complete for the pinned public C inventory, including independently owned
callback adapters and callable native views; no gaps remain in
[docs/facade-coverage.json](docs/facade-coverage.json). Standalone model
download/cache helpers are implemented in [include/ailo/model.h](include/ailo/model.h).
The initial inference receiver is implemented in [include/ailo/client.h](include/ailo/client.h):
model acquisition/loading, Jinja templates, real token streaming, sampling,
multiple candidates, log probabilities, constrained JSON, generated function
calls, bounded prompt/KV reuse, opt-in encrypted snapshots, cancellation, model
cards, owned canonical continuation/forks, qualified reasoning deltas and
capability queries.
[docs/client.md](docs/client.md) describes its supported surface and verification.
Chat Completions and Responses JSON/SSE adapters, retention repositories and
conversation/compaction operations are implemented; [docs/openai.md](docs/openai.md)
records their supported profile. The in-process `handle` dispatcher supplies
model discovery and routes these operations without an output buffer. HTTP
response lifecycle is supplied by the strict C89
[serving handler](docs/serving.md), with a matching Lua receiver. Network
server integration and release acceptance
remain subsequent work. Native install-tree CMake/pkg-config consumers are available.

`make test-openai-agent PRESET=x86_64-linux-gnu-release` exercises the handler
through a test-only loopback HTTP bridge and real Codex multi-turn function-tool
round trips, remote compaction and opaque replay. Ordinary CTest also exercises
every implemented route over HTTP, including stateful Responses and stateless
Chat history. The deterministic producer emits token pieces through the production
adapters; this gate tests agent interoperability separately from model quality.
`make test-openai-inference` runs a separate trained text/media serving and Codex
CTest suite, including tool results, durable continuation, semantic compaction,
consumer cancellation and image/audio replay. Select `INFERENCE_BACKEND=vulkan`
for actual GPU placement checks or `PRESET=x86_64-linux-musl-release` for fully
static execution with file/process tracing. It uses pinned cached models;
explicitly select `INFERENCE_ACQUIRE=download` for their first acquisition. See
[serving qualification](docs/serving.md#trained-model-serving-acceptance).
The C89/Lua compactor key owner, authenticated envelopes and opaque-item replay
through generation/continuations, standalone model-driven semantic compaction
and explicit automatic preflight/in-flight compaction are implemented.

The required receiver includes normal-use Hugging Face acquisition, configurable
XDG model caching and a structured JSON model card for the loaded model. The
common in-process llama receiver, tools/schema support, typed events, prompt reuse,
opt-in persistent snapshots and authenticated compaction contract is in
[docs/inference-backend.md](docs/inference-backend.md). Chat Completions and
Responses adapters use that same backend; API-specific output follows
the documented wire contract.

The complete static RADV/ANV deployment and AMD/Intel/mixed multi-GPU layer splitting
contract is in [docs/static-vulkan.md](docs/static-vulkan.md). The built-in static
driver/loader integration and explicit placement controls are implemented. Focused
static musl trained inference has passed on Intel RPL-U; AMD, AArch64 GPU and
multi-GPU execution still require hardware qualification.

[docs/upstream-api.json](docs/upstream-api.json) records the pinned public C API
inventory. The generated coverage gate checks every declaration against the
C89 facade and requires an empty gap list.

## Local commands

Stored OpenAI Responses and Conversations use encrypted embedded Pouch storage
by default, under `~/.local/state/ailo`, with its generated key under
`~/.config/ailo/pouch.key`. XDG variables and C/Lua path overrides are supported;
see [persistence](docs/persistence.md).

Use `make help` for the current command index. The default native build requires
CMake 3.24+, Ninja with its `inputs`/`deps` inspection tools, Bash, ripgrep,
coreutils, clang-format, patch, and glslc.
Vulkan builds use the host `glslc` executable from shaderc (for example, the
`glslc` package on Debian/Ubuntu). It compiles GLSL into SPIR-V for every target;
no target-specific shader compiler is needed. Its shaderc/glslang/SPIRV-Tools
libraries are host build tools and are not linked into libailo. The Vulkan
headers and loader still come from our pinned sources. Set
`Vulkan_GLSLC_EXECUTABLE` in CMake to select an explicit host executable.
The static Mesa profile additionally requires Meson 1.4+, Mako, PyYAML,
LLVM 20.1.8 development tools and the matching Clang shared runtime. Matching
Mesa generators, SPIR-V tools and Clang headers are prepared inside the checkout;
they are host tools, not target libraries. See the exact prerequisites and
commands in [docs/static-vulkan.md](docs/static-vulkan.md#dependency-and-build-profile).
The CPU preset does not require shader tooling. CUDA is excluded from supported
backends; see the support policy below.
Dependency preparation, builds, formatting and export checks use Bash/CMake.
Python 3.9+ is confined to source/data generation, compiler AST inventory
processing and fixture servers; host Clang is needed for the inventory check.
CTest, Bash and compiled tests own test execution and assertions, including the
local archive-cache and model-download failure cases.
The lifecycle resolver selects the pinned Bootlin
C/C++ collection; it never falls back to a host compiler for compiled targets.
The host Clang executable inspects declarations; Mesa's host generator uses the
matching Clang/LLVM runtime. Valgrind is a host prerequisite
for memory checks. Lua product tests/packaging additionally require host
LuaRocks, zip and unzip; these tools do not supply target runtime libraries.
Owned C targets use explicit `-std=c89 -pedantic-errors -Werror` and disable
C extensions in CMake. Every public header, including the generated version
header, compiles independently under those flags in the normal test build and
again from the extracted SDK. Private C++ and upstream targets keep their own
language requirements.

```sh
make deps
make build
make test
make upstream-inventory
make finalize-slice
make test-scalar-matrix
make facade-coverage
```

`make build PRESET=cpu` is the explicitly CPU-only configuration. The default
`debug` configuration includes Vulkan and CPU. Cross-release presets build the
same selected static GPU closure. Current target/package evidence and remaining
hardware acceptance are recorded in [docs/static-vulkan.md](docs/static-vulkan.md).
Shipments cover x86_64 and aarch64 with glibc or musl. ARM hard-float is retired;
its presets, dependency assets and SDK artifact names are no longer accepted.
`AILO_JOBS` controls workers inside one build (default 2); lifecycle command
entry points run serially.

`make build PRESET=library` excludes inventory/test-tool discovery; the Vulkan
dependency and attribution generators still require Python. The CPU build needs
no binding regeneration. `make test-scalar-matrix` executes scalar and native-width
bridge checks on all four Linux targets; ARM execution requires host
`qemu-aarch64`. Those checks prove the scalar contract, not full model inference
or facade coverage. All four static GPU SDKs pass package checks; full model and
hardware qualification remains pending. Native fixture evidence is described
below. `make valgrind` covers
current native executable tests.
`make facade-coverage` requires every coverage gap to remain resolved;
ordinary unit checks do not substitute for that completeness gate. Generated
bindings are checked in and require no Python invocation for ordinary builds.
`make generate-native` performs the separate compiler-inventory regeneration.

Verified external archives use the shared
`${CPKT_DEPENDENCY_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/cpkt/deps}`
cache. Toolchains use its sibling `toolchains` cache. Disposable extracted and
built dependencies stay under `.cache/`; other generated work stays under
`build/`. None is source or a release artifact.

## Backend support policy

libailo will not compromise its deployment requirements to accommodate NVIDIA's
proprietary runtime dependencies. NVIDIA's official CUDA userspace driver is
closed source and supplied as `libcuda.so`, without a static `libcuda.a`.
Its [open kernel modules](https://github.com/NVIDIA/open-gpu-kernel-modules)
do not provide an open userspace CUDA driver; NVIDIA explicitly distinguishes
these components in its
[driver source policy](https://developer.nvidia.com/blog/nvidia-releases-open-source-gpu-kernel-modules/).
The official CUDA stack cannot satisfy deployment without shared-object loading.
CUDA support is therefore excluded. Vendor market share does not override our
requirements for reproducible builds and independent distribution.

Independent open implementations exist.
[LibreCUDA](https://github.com/mikex86/LibreCuda) offers a static driver library,
but its authors describe rudimentary functionality unsuitable for production.
It has not been qualified for libailo or the pinned llama.cpp backend and is
outside our supported dependency stack.

A musl SDK or a static facade archive does not establish a fully static GPU
executable. Vulkan normally loads userspace driver libraries. Fully static musl
cannot use `dlopen`; a static glibc executable loading a glibc driver still needs
compatible shared glibc. The selected GPU qualification paths are headless Mesa
RADV for AMD Radeon and Mesa ANV for Intel GPUs, with permissive userspace
dependencies and explicit exclusion of libudev/libelf auto-discovery. The
maintained Linux ROCm/HIP stack is excluded because it requires LGPL libnuma.
The [userspace dependency assessment](docs/gpu-userspace-dependencies.md)
records the source evidence, accepted GCC runtime exception, feature losses
from omitting optional dependencies and required static integration work.
Permissive implementations of optional APIs may be qualified separately.
Licensing eligibility is not proof of a working static deployment. Fully static
CPU inference and focused Intel GPU inference have executable evidence recorded
in the static deployment contract. Proving final executables and the wider
hardware matrix remains separate from SDK archive verification.

## Upstream patches

`vendor/llama/patches/series` contains the tracked patches applied to the pinned
archive during preparation:

- Route the Vulkan shader generator through the pinned native toolchain.
- Own and destroy Vulkan device descriptors and their contexts at shutdown.
- Implement the declared upstream threadpool-count query that lacked a body.
- Give CPU repack traits static ownership rather than an unfreed allocation.
- Format Vulkan buffer handles through their native type on 32-bit targets.
- Route receiver-scoped logs through CPU workers and tensor-validation jobs;
  add `GGML_LOG_LEVEL_TRACE = 6` without changing existing enum values.
- Expose private Vulkan device identity to the receiver's placement controls.
- Track the host shader compiler as a shader-generation dependency.
- Route host diagnostics through the logging hooks, with silent native defaults
  and frequent Vulkan details at TRACE; preserve returned errors and explicit
  file output. Fatal GGML diagnostics use the logger without debugger subprocesses.

The ownership patches address allocations observed by Valgrind during backend
enumeration and CPU graph execution. The trace enum and private device-identity
hook preserve existing native enum values. C++ namespace RAII wrappers are
excluded explicitly from the API inventory; the global public mtmd memory-report
operation remains covered.

Mesa/libdrm patches embed required identity data and select the headless static
profile. The Khronos loader patch fixes its providers to the linked RADV/ANV
entrypoints and disables external driver/layer discovery. The driver stack's
host diagnostics also use the scoped native logging bridge, preserving
WARN/ERROR severity and assigning routine driver detail to TRACE. These private
details do not expand the exported libailo C ABI.

No HTTP server, Kore integration, release, tag or push is part of this work.

## Native model evidence

The test suite builds a complete one-layer, 26,976-parameter Llama GGUF using
public native GGUF/tensor APIs. Repeated writes must match SHA-256
`e32806b9651b70b20c6109e4f4aab9e8ef0099108b69a1214e937f8ef1a77668`.
This is an untrained deterministic execution fixture, not a text-quality model.
All fixture files stay under the selected `build/` directory; no download is
needed. Static and shared C89 tests cover model metadata, native chat templates,
tokenization/detokenization, actual decode, greedy sampling, independent contexts,
context/sequence state restoration, and native load failures for missing,
truncated and unsupported model files.

The Vulkan CTest runs real decode when a Vulkan device exists and reports an
explicit skip otherwise. `make inference-smoke-vulkan` requires hardware and
asserts native model/KV offload evidence, so absence cannot pass that gate.
The native Intel RPL-U device has passed this gate. CPU tests explicitly disable
both GPU layers and operation/KV offload.

`make valgrind-model` builds the native CPU-only configuration and checks both
static and shared model tests with all leak kinds fatal. The broader
`make valgrind` includes it. A separate Vulkan-enabled process check exposed
loader/driver retained allocations; CPU-only memory evidence does not claim
Vulkan driver leak freedom. Callback/receiver and trained text inference evidence
is recorded in the linked contracts. [Image/audio input](docs/multimodal.md) now
uses matched model/projector pairs, bounded decoders, streamed text and durable
media replay. Its explicit trained-model suite remains separate from fast offline
regressions; comprehensive GPU lifetime and hardware acceptance remain separate gates.

## Model acquisition

`ailo_model_cache_path` validates a zero-initialized `ailo_model_source` and
resolves its primary path without creating files or making network requests.
`ailo_model_acquire` validates local/cached files or downloads explicitly
permitted missing Hugging Face files. Both return an owned path released with
`ailo_string_free`. Local files stay in place. Cache precedence is an absolute
override, `$XDG_CACHE_HOME/ailo`, then `$HOME/.cache/ailo`; nonempty relative
environment paths fail. Repo/revision identities use a SHA-256 directory key,
with nested filenames below it. Revisions are explicit; pin a commit for a
stable model identity.
Filename components beginning `.ailo-` are reserved for implementation state.

Downloads stream through bounded curl buffers into same-directory exclusive
partial files. HTTP byte counts, GGUF v2/v3 headers, per-file size limits and
optional SHA-256 values are checked before atomic no-replace publication.
Existing invalid files fail for explicit caller repair. Verified cache hits
make zero network requests. Split acquisition validates every requested sibling
before returning; complete shards survive a later shard failure and are reused.
Publication locks also serialize individually requested split siblings. A
subsequent permitted attempt removes only that target's reserved partial files
left by process death. Unrelated files/partials are preserved.
The TLS fixtures also acquire a real split GGUF and load it through the C89
facade for token decode and state restoration.

Transport allows five verified HTTPS redirects, with per-attempt connect/transfer
timeouts and bounded retries. Explicit bearer tokens stay on their original
origin. The implementation uses curl's
[peer verification](https://curl.se/libcurl/c/CURLOPT_SSL_VERIFYPEER.html) and
[redirect URL resolution](https://curl.se/libcurl/c/CURLINFO_REDIRECT_URL.html);
no insecure transport or alternate origin is exposed by the public API.
The test-only alternate origin/CA remains private and enforces TLS verification.
The supported Linux cache filesystem must provide atomic no-replace rename.
These helpers validate acquisition integrity; actual model loading remains a
separate native operation.

Model cards accept copied supplemental `model_metadata` JSON and expose claim
origins/conflicts. Their first lookup can acquire selected publisher metadata
for the same immutable Hugging Face revision, respecting `allow_download` and
reusing a matching sidecar offline. Generation does not wait for this optional
enrichment. The schema and limits are in [docs/client.md](docs/client.md#model-cards-and-current-capabilities).

`ailo_model_cache_example --local MODEL` shows local use. Hugging Face use takes
`--repo OWNER/REPO --revision COMMIT --file FILE [--cache ABSOLUTE-PATH]`;
`--download` is required for a miss, and `--sha256 HEX` pins a single-file checksum.
`make valgrind-acquisition` checks C89 static/shared ownership plus local TLS
success, error, cancellation, callback-exception and concurrent-client cases.
Curl/OpenSSL archives remain separate static dependencies; the shared library
links SDK shared libraries. No model weights or test trust certificates ship.

## Installed consumers

`make test-install-tree PRESET=cpu` builds, installs into a temporary prefix,
moves that prefix to a path containing spaces, then compiles and runs C-only
and C++ consumers through static/shared CMake targets and pkg-config metadata.
It also checks isolated header inclusion, real model decode/state restoration,
the exact installed dynamic export set, private-symbol link rejection, missing
external SDK diagnostics, and relative installed runtime paths.

The install component `ailo` contains only project headers and libraries,
separate native archives under `lib/ailo/native`, selected GNU runtime archives
under `lib/ailo/runtime`, package metadata and documentation/licenses. Curl and
OpenSSL remain external requirements. No native upstream headers are installed.
`share/ailo/dependencies.json` records target-specific dependency URLs/checksums
and bundled/external roles. Untagged Git builds resolve to `0.0.0` by default;
install-tree verification does not constitute a release or completed SDK.

For CMake, use `find_package(ailo CONFIG REQUIRED COMPONENTS static)` or
`COMPONENTS shared`, then link `ailo::static` or `ailo::shared`. Static consumers
supply a compatible cpkt core SDK (baseline v0.5.0) through `CMAKE_PREFIX_PATH` or
`ailo_EXTERNAL_ROOT`. The metadata owns native and GNU runtime archive ordering,
so C-only consumers do not need to enable C++ or write private library lists.
`pkg-config --cflags --libs ailo` selects the shared library;
`pkg-config --static --cflags --libs ailo-static` selects the facade/native/runtime
archives explicitly. Provide cpkt, cpktmisc and lonejson `lib/pkgconfig` paths
on `PKG_CONFIG_PATH`, or their SDK prefixes on `CMAKE_PREFIX_PATH`.
Builds also prepare cpktmisc v0.2.0 in a separate prefix and verify its libffi
3.8.0 closures. The installed facade links its callback adapters against the
separate libffi library, which is a consumer dependency. cpktdb is not
required. Toolchain and archive defaults use `~/.cache/cpkt`; the retired cache
is not searched or modified.
Shared consumers must make the external SDK libraries and compatible GNU C++
shared runtime available to the dynamic loader. Installed libraries contain
only `$ORIGIN`-relative lookup paths; test executables alone receive local paths.
The install-tree gate strips verification copies, creates and extracts an SDK
archive, and scans its text/binary payload and loader metadata before running
consumers. Compilation maps diagnostic/debug filenames to logical names;
the built-in Vulkan loader disables external driver/layer discovery. No linked
binary is rewritten to hide a path. Negative archive fixtures prove rejection of
repository/home paths, local file URLs, foreign workstation paths, absolute
RPATHs, and escaping/dangling symlinks. Both CPU-only and Vulkan-enabled extracted
trees pass. Development checksum sets and all four Linux SDKs have been verified.
The actual tag-mutation release contract, nested release-payload verification,
and comprehensive receiver/hardware acceptance remain pending.

## Versions and source reconstruction

`make print-release-version` reads an exact lightweight `vX.Y.Z` tag on HEAD,
then an explicit `AILO_VERSION_OVERRIDE`, then defaults to `0.0.0` in Git.
Annotated/signed or ambiguous exact release tags fail. Git worktrees ignore
`VERSION`. Extracted sources read their staged `VERSION` even when extracted
inside another checkout. `include/ailo/version.h` is generated at build time;
its project version agrees with CMake/pkg-config metadata, while shared ABI 0
remains independently controlled. Candidate overrides never create tags.

`make package-source` requires an exact release version and complete facade.
The explicit `cmake/source-manifest.txt` controls inclusion; staging injects
`VERSION` and `SOURCE-MANIFEST.sha256`. Archives are deterministic and verification
rejects extra/missing files, generated directories, duplicate/escaping entries,
links, checksum corruption and local source paths. `make test-source-archive`
tests this with development archives under `build/`, without producing releases.

`make package-source-smoke SOURCE_ARCHIVE=path` extracts, checks and reconstructs
the full CPU build/test/install surfaces without `.git`. A development candidate
at version `2.3.4` has passed this proof with generated-header/package version
agreement. This candidate is a test value, not a selected or published release.
Read-only tag-record parsing and current-worktree checks run in ordinary tests;
actual tag creation/signing checks remain outside the authorized implementation.

## Checksummed SDKs

`make package` requires an exact lightweight release tag and a complete facade.
The development `scripts/package.sh --development --preset NAME --no-build
--output-directory build/PATH` surface stages a configured build for verification.
It creates a deterministic stripped archive containing payload and symlink
ledgers plus explicit development/capability metadata; it cannot write to `dist/`.

`make package-checksums ARTIFACT_DIRECTORY=path ARTIFACT_VERSION=X.Y.Z
ARTIFACT_LIST=path` derives the sole `libailo-X.Y.Z-CHECKSUMS` file from an explicit
list of artifact basenames. Stale/unlisted/duplicate/missing files fail.
`make package-verify CHECKSUM_MANIFEST=path` verifies the complete listed set,
then checks each extracted SDK's payload, target-correct archive ABI, required
layout, private-header/dependency exclusion, loader metadata and exact exports.
`scripts/release_upload_files.sh MANIFEST` prints only the verified listed files
plus the manifest itself; it does not upload or publish anything.

The serialized development Linux matrix has built and verified Vulkan+CPU SDKs
for x86_64 and aarch64 with both glibc and musl. It also executes the
existing four-target scalar/bridge checks, including ARM QEMU coverage. The
cross-link helper supplies dependency paths only at link time; native test
executable runtime paths remain private and do not enter SDK metadata.
This matrix proves the current foundation, not facade or receiver completion.

## Receiver logging and Lua

`ailo_client_new_with_logger` accepts a libpslog v0.12.0 logger and derives an
owned child. Native trace/debug/info/warn/error records are scoped per receiver,
including CPU workers, without replacing global hooks or serializing independent
clients. Borrowed output resources must live until receiver destruction. Prompt
and generated text are not logged by the bridge. Unattributable shared device
messages can use an explicitly configured native global hook. Without a logger,
native defaults are silent; errors remain available through returned values.

`make lua-valgrind` runs the full CPU inference/ownership suite under Valgrind.
The normal Lua test selection uses a separate model-free Pouch unload memory
regression, with no sleeps or inference.

`make lua-test PRESET=cpu` builds the Lua 5.5.1 facade against an installed shared
SDK and tests real token callbacks, cancellation/error recovery, allocation
failure, logger retention and finalizers. Configure pslog in Lua and pass its
logger as `ailo.new{source={local_path="model.gguf"},logger=log}`. Native sinks
are required for worker logging. See [the Lua API](lua/README.md) for the full
receiver workflow and [the C API](docs/client.md) for the underlying contract.
`make package-lua` emits separate Lua source, rockspec and source-rock products;
they are included in release checksum inventories and excluded from target SDKs.
