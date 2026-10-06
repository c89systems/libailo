# libailo

Ailo is a local GGUF inference library with a strict C89 public boundary and a
receiver interface for text Chat Completions. The reviewed implementation
contract is in [docs/architecture.md](docs/architecture.md).

## Implementation state

The dependency/build foundation is present: pinned llama.cpp b11430, CPU and
Vulkan backends, public multimodal libraries, c.pkt.systems v0.11.0, lonejson
v0.45.0, and pinned Vulkan/SPIR-V components. The static/shared project libraries
provide the portable C89 scalar/error boundary and generated native bindings
described in [docs/c89-boundary.md](docs/c89-boundary.md) and
[docs/facade-bindings.md](docs/facade-bindings.md). The current bridge implements
1,055 upstream functions and 281 record fields, including native-backed arrays,
the public mtmd memory map, variadic formatting and C89 macro adapters. It is
incomplete: 83 callback adapter gaps remain explicit in
[docs/facade-coverage.json](docs/facade-coverage.json). Standalone model
download/cache helpers are implemented in [include/ailo/model.h](include/ailo/model.h).
The receiver and release SDK gates are still pending. Native install-tree
CMake/pkg-config consumers are available.

[docs/upstream-api.json](docs/upstream-api.json) records the pinned public C API
inventory. An inventory is the baseline for the forthcoming facade coverage
gate; it does not itself prove facade completeness.

## Local commands

Use `make help` for the current command index. The default native build requires
CMake 3.24+, Ninja, Bash, ripgrep, coreutils, clang-format, patch, and glslc.
Dependency preparation, builds, formatting and export checks use Bash/CMake.
Python 3 is confined to compiler AST inventory processing and test fixtures;
host Clang is needed for the inventory check. The lifecycle resolver selects the pinned Bootlin
C/C++ collection; it never falls back to a host compiler for compiled targets.
Host Clang is used only to inspect declarations. Valgrind is a host prerequisite
for memory checks.

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
`debug` configuration includes Vulkan and CPU. Cross-release presets are
declared; their full build, execution and packaging evidence remains pending.
`AILO_JOBS` controls workers inside one build (default 2); lifecycle command
entry points run serially.

`make build PRESET=library` configures and builds without test/tool Python
discovery. `make test-scalar-matrix` executes scalar and native-width bridge
checks on all six Linux targets; ARM execution requires host `qemu-arm` and
`qemu-aarch64`. Those checks prove the scalar contract, not full model inference
or facade coverage. Full model, backend and packaging matrix evidence remains
pending; native fixture evidence is described below. `make valgrind` covers
current native executable tests.
`make facade-coverage` intentionally fails until every coverage gap is resolved;
ordinary unit checks do not substitute for that completeness gate. Generated
bindings are checked in and require no Python invocation for ordinary builds.
`make generate-native` performs the separate compiler-inventory regeneration.

Verified external archives use the shared
`${CPKT_DEPENDENCY_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/c.pkt.systems/deps}`
cache. Toolchains use its sibling `toolchains` cache. Disposable extracted and
built dependencies stay under `.cache/`; other generated work stays under
`build/`. None is source or a release artifact.

## Upstream patches

`vendor/llama/patches/series` contains the tracked patches applied to the pinned
archive during preparation:

- Route the Vulkan shader generator through the pinned native toolchain.
- Own and destroy Vulkan device descriptors and their contexts at shutdown.
- Implement the declared upstream threadpool-count query that lacked a body.
- Give CPU repack traits static ownership rather than an unfreed allocation.

The ownership patches address allocations observed by Valgrind during backend
enumeration and CPU graph execution. Public upstream header declarations remain
unchanged. C++ namespace RAII wrappers are excluded explicitly from the API
inventory; the global public mtmd memory-report operation remains covered.

No HTTP server, Kore integration, Responses API, stored completions, release,
tag, or push is part of the current foundation.

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
Vulkan driver leak freedom. Full callback, receiver, multimodal inference,
trained-model smoke and packaging acceptance are still pending.

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
and bundled/external roles. Version `0.0.0` remains the development placeholder;
install-tree verification does not constitute a release or completed SDK.

For CMake, use `find_package(ailo CONFIG REQUIRED COMPONENTS static)` or
`COMPONENTS shared`, then link `ailo::static` or `ailo::shared`. Static consumers
supply the matching c.pkt.systems v0.11.0 SDK through `CMAKE_PREFIX_PATH` or
`ailo_EXTERNAL_ROOT`. The metadata owns native and GNU runtime archive ordering,
so C-only consumers do not need to enable C++ or write private library lists.
`pkg-config --cflags --libs ailo` selects the shared library;
`pkg-config --static --cflags --libs ailo-static` selects the facade/native/runtime
archives explicitly. Provide c.pkt.systems's `lib/pkgconfig` on `PKG_CONFIG_PATH`.
Shared consumers must make the external SDK libraries and compatible GNU C++
shared runtime available to the dynamic loader. Installed libraries contain
only `$ORIGIN`-relative lookup paths; test executables alone receive local paths.
Release archives, complete binary privacy scans, exact-tag versioning, source
reconstruction, and cross-target package gates remain pending.
