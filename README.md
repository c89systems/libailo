# libailo

Ailo is a local GGUF inference library with a strict C89 public boundary and a
receiver interface for text Chat Completions. The reviewed implementation
contract is in [docs/architecture.md](docs/architecture.md).

## Implementation state

The dependency/build foundation is present: pinned llama.cpp b11430, CPU and
Vulkan backends, public multimodal libraries, c.pkt.systems v0.11.0, lonejson
v0.45.0, and pinned Vulkan/SPIR-V components. The static/shared project libraries
currently provide the portable C89 scalar and error boundary described in
[docs/c89-boundary.md](docs/c89-boundary.md). The complete llama.cpp facade,
receiver, model download/cache helpers, and redistributable SDK are still pending.

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
or facade coverage. Native model, Vulkan and packaging matrix evidence remains
pending. `make valgrind` covers current native executable tests.

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

The second patch addresses a definite upstream leak observed by Valgrind during
backend enumeration. Public upstream header declarations remain unchanged.

No HTTP server, Kore integration, Responses API, stored completions, release,
tag, or push is part of the current foundation.
