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
[docs/facade-coverage.json](docs/facade-coverage.json). The receiver, model
download/cache helpers, and redistributable SDK are still pending.

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
