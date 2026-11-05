# Static Vulkan implementation and acceptance contract

Updated 2026-10-15. This extends [architecture.md](architecture.md).
Implement the complete CPU/RADV/ANV deployment and multi-GPU layer splitting,
then test it thoroughly. The investigation establishes an implementation path;
it does not establish an already working static GPU artifact.

Receiver placement controls are implemented: copied UUID selection and weights,
layer/single-device strategies, explicit no-fallback commitments, positive
participant ownership/residency checks, and detailed model-card allocations.
See [the receiver contract](client.md#gpu-placement). Static RADV/ANV dependency
archives now build across the four Linux targets. Built-in loader dispatch and
the libailo/SDK integration are implemented; qualification evidence remains
distinct from full hardware acceptance.

Focused 2026-10-15 foundation evidence used the existing shared-driver build on
Intel Graphics RPL-U, ANV, PCI `0000:00:02.0`, device UUID
`868021a7040000000002000000000000`. SmolLM2-135M-Instruct Q4_K_M (SHA-256
`2e8040ceae7815abe0dcb3540b9995eaa1fa0d2ca9e797d0a635ae4433c68c2d`)
offloaded all 30 repeating layers and the output layer. The installed Lua/C ABI
exercised explicit weighted layer placement and single-device placement, first
text at token 1, cancellation and a second client surviving first-client teardown,
with Mesa disk shader cache disabled. Native model/context/compute device buffers
were reported separately from host buffers. This is single-Intel foundation
evidence, not static Mesa, AMD or multi-GPU qualification.

The installed-ABI `lua/tests/receiver.lua` has a hardware mode: append
`--vulkan single SHA256` after its normal model/work/repository-fixture arguments
to test one device, or `--vulkan all SHA256` for weighted placement across all
eligible devices. The latter requires at least two devices and fails if hardware
is absent; it is not a successful skip. Use the matching Lua runner/loader/module
paths from `lua-receiver` and a trained model, not the synthetic CPU fixture.

## Deployment boundary

The standalone deployment is an application statically linked with musl,
libailo, its native backend, Vulkan dispatch, Mesa runtime compilers/drivers and
all required userspace dependencies. It must run without a dynamic interpreter,
DT_NEEDED dependencies, runtime shared-object loading, an installed Vulkan ICD,
driver manifest, shader executable or loose shader asset. Model weights and the
model cache remain external data. Linux kernel DRM drivers, GPU firmware and
permission to open the GPU render nodes are host requirements.

Ship SDKs for x86_64/aarch64 with glibc/musl; CPU remains supported everywhere
in this matrix. Build the selected providers for these targets and expose actual
GPU capabilities. Advertise a target/device combination only with hardware
evidence; cross compilation or QEMU execution cannot establish GPU support.
Static glibc is not a substitute for the static musl acceptance executable.
Shared libailo is still an SDK product, with private GPU code statically linked
into it and no subsequent driver/plugin loading. Existing shared-loader builds
are foundation evidence, not a second shipped GPU architecture to retain.

RADV selects supported AMD Radeon/graphics-capable devices; it does not imply
support for compute-only AMD Instinct. ANV selects supported Intel GPUs.
Enumerate actual capabilities rather than deriving support from a vendor name.
The pinned GGML implementation needs Vulkan 1.2 and storageBuffer16BitAccess;
also validate its queue, memory and shader requirements at device setup.
Optional fp16, integer dot product and cooperative matrix features enable
specific kernels; their absence is not automatically an inference failure.

## Compile and execute shaders

| Stage | AMD RADV | Intel ANV |
| --- | --- | --- |
| Application build | Host glslc compiles GGML GLSL to embedded SPIR-V | Same |
| Runtime shader translation on CPU | SPIR-V to NIR, then ACO produces GPU instructions | SPIR-V to NIR, then Mesa BRW/JAY produces GPU instructions |
| Submission | RADV uploads code/buffers and submits through DRM | ANV uploads code/buffers and submits through DRM |
| Inference execution | GPU executes compiled kernels | GPU executes compiled kernels |

NIR/ACO/Intel compiler code is part of the static userspace link closure. Runtime
shader compilation is in process and uses memory; it launches no compiler
program. SPIR-V is an intermediate shader representation, not an executable
that can be uploaded unchanged to every GPU. Disable disk shader caches in the
selected profile; retain in-memory pipeline reuse. Model-cache writes are a
separate, configured facility.

The existing x86_64 glibc release build embeds 1,336 shader variants totalling
36,175,948 bytes (34.50 MiB). These bytes live in the separate native
libggml-vulkan archive; dependencies are not concatenated into libailo.a.
That archive measured 52,268,128 bytes, including code/object overhead. Neither
measurement is a bound on the final executable or the added Mesa compilers.
Record stripped executable size, selected shader bytes, compiler/driver archive
sizes, startup time, pipeline compilation latency and peak RSS in qualification.

## Static driver dispatch

Retain the pinned Khronos loader's existing Vulkan handle dispatch and multi-ICD
machinery. Apply a focused static integration rather than implementing a new
Vulkan loader. Mesa 26.2.4 normally builds its final RADV and ANV drivers with
shared_library. The selected profile changes these to private static PIC
archives suitable for both standalone executables and shared libailo. Common
runtime/compiler archives are installed separately and linked once; the two ICD
entrypoints are namespaced as `ailo_radv_icdGetInstanceProcAddr` and
`ailo_anv_icdGetInstanceProcAddr`.

The loader has a fixed built-in registry of independently namespaced RADV and
ANV provider entrypoints. It reuses the loader's
[direct driver loading](https://docs.vulkan.org/refpages/latest/refpages/source/VkDirectDriverLoadingListLUNARG.html)
function-pointer registration and negotiation helper for every ICD scan,
including global version/extension queries before instance creation. It never
scans installed ICDs, layers or loader settings. External direct-driver injection
is rejected and its extension is not advertised. Driver/layer/settings environment
overrides are ignored; `VK_LOADER_DEBUG` remains available for diagnostics.
Dynamic library helpers fail closed and ELF DSO introspection is removed.

GGML uses the linked loader normally; it needs no provider-list injection. The
pinned Vulkan-Hpp setting VULKAN_HPP_DISPATCH_LOADER_DYNAMIC means function
pointer dispatch; its dispatcher starts from the linked vkGetInstanceProcAddr.
It does not by itself require opening a shared object. Preserve that distinction
while auditing the complete loader/backend call graph.

RADV and ANV each publish ICD entrypoints; their common Mesa runtime also owns
ICD negotiation and physical-device dispatch. Namespace provider entrypoints,
keep negotiation state coherent per provider, and resolve common utility/runtime
symbols deliberately. Do not duplicate a whole Mesa object graph accidentally,
resolve collisions with linker allow-multiple-definition, or use a global
vendor selector. Separate clients and mixed-vendor physical devices need correct
handle ownership through instance, physical device, logical device and teardown.
Hide private Vulkan/Mesa symbols from libailo's exported C ABI and document any
static symbol isolation constraints for applications linking another GPU stack.
The shared library's exact export allowlist hides these symbols. Static consumers
use the global Vulkan/Mesa symbol namespace; linking another Vulkan loader or
Mesa stack into the same executable is unsupported.

Retain both complete ICD archives and the four common runtime archives exactly
once. Mesa's dispatch tables use weak entrypoint references, so ordinary archive
extraction can silently omit implementations such as ANV's event creation even
when device enumeration works. The device/event probe and actual inference
regression cover this failure; linker multiple-definition overrides are not used.

Relevant pinned loader code is in loader/loader.c: loader_add_direct_driver,
loader_scan_for_direct_drivers, loader_scan_for_icds and layer discovery.
See its [driver interface documentation](https://github.com/KhronosGroup/Vulkan-Loader/blob/v1.4.367/docs/LoaderDriverInterface.md).
The registry and global discovery adaptation are project work, not behavior
provided by an unmodified upstream static archive.

## Dependency and build profile

Use Mesa 26.2.6 and libdrm 2.4.134 for the current integration;
archive identities and license evidence are in
[gpu-userspace-dependencies.md](gpu-userspace-dependencies.md). These
inputs are now checksum-pinned in `cmake/dependencies.json` and use the existing
verified global archive cache. Meson source readiness is checked independently
of CMake sources.

`make gpu-deps TARGET=<id>` currently builds libdrm core and amdgpu as static PIC
archives. `make test-gpu-deps TARGET=<id>` also checks a relocated static consumer
and its ELF dependencies. The four supported Linux targets use the same pinned
Bootlin resolver as the CMake build, explicit compiler sysroots and target-only
pkg-config roots. Meson/Ninja/pkg-config are host prerequisites; ambient compiler
flags and include/library search overrides are cleared for target builds.
`make test-static-mesa TARGET=<id>` additionally prepares the matching host tools,
builds both Mesa drivers and checks a relocated, fully static dependency
consumer. Mesa's installed `ailo-mesa.pc` supplies the complete archive order,
including one copy of its common runtime. The probe directly creates an instance
for each provider and enumerates device properties; it does not execute shaders
or use the libailo dispatcher. `make test-static-vulkan TARGET=<id>` instead checks
the built-in loader, device/event dispatch, instance lifetimes and rejection of
external drivers/layers. On x86-64, strace verifies that poisoned environment
settings cause no shared-library or manifest discovery. AArch64 checks link only.
The main Vulkan build now adds this closure to the SDK as separate archives,
with link ordering supplied by its CMake/pkg-config metadata.
The libdrm patch embeds its pinned marketing-name table as constant data, with
no runtime `amdgpu.ids` file or environment override. The existing kernel
`/proc/cpuinfo` fallback for integrated devices is retained. A CMake source
generator checks the table format and PCI device/revision ranges before emitting
the header; Meson tracks both the input and generator. Tests cover known and
unknown IDs, revision-specific names, duplicate-row ordering, ignored external
overrides and malformed generator inputs. The metadata tests execute on both
x86-64 libc profiles; AArch64 currently has compile/link evidence only.

Keep the already pinned Khronos headers/loader/SPIR-V headers coherent with
the chosen Mesa version and prove generation/compilation compatibility.

Build libdrm core and amdgpu, Mesa Vulkan runtime/utilities, NIR/SPIR-V translation,
ACO/address libraries and Intel BRW/JAY/device/layout dependencies. Select the
permissive licenses for bundled third-party code, including BLAKE3. Disable
target LLVM, libelf, libudev, OpenGL/Gallium/OpenCL, X11/Wayland/display WSI,
disk shader cache, tracing/profiling/plugin integrations and automatic optional
dependency discovery. Minimal headless inference must work without XML/cache
compression libraries; retain a permissive optional dependency only if an actual
selected code path requires it and document that closure explicitly.
Omission of libelf loses RADV ELF/RTLD and RGP export paths; normal ACO emits a
non-ELF native binary. These consequences are documented in the dependency audit.

Host tools have a separate closure. glslc produces portable SPIR-V for each
application target; no CPU-target GLSL cross compiler is required. ANV internal
shader generation also needs matching host mesa_clc and vtn_bindgen2, plus host
glslangValidator for internal GLSL shaders. The selected Mesa host tools use
LLVM/Clang, SPIR-V LLVM translator and SPIR-V Tools; this Intel profile requires
neither libclc nor Rust. LLVM is a host prerequisite, not a target dependency.
Use Mesa's system `mesa-clc` mode in the target build, with explicit tool
paths and matching source versions. Document Meson/Ninja, Mako/PyYAML and any
generator prerequisites resolved by this selected upstream build, with actionable
failures.
Host tooling may be installed on the workstation; target libraries may not be
taken from its package database or library directories.

`make gpu-tools` builds pinned host SPIR-V Tools 2026.4, LLVM/SPIR-V translator
23.1.3, glslang 16.6.0 and Mesa 26.2.6 `mesa_clc`/`vtn_bindgen2`. These builds
require installed LLVM 23 development tools, matching Clang development headers
and static LLVM/Clang libraries in the same SDK prefix, Meson >= 1.4, Ninja, Mako
and PyYAML. The translator and Mesa host tools link LLVM statically.
Zlib and libxml2 come from the pinned native cpkt v0.6.1 SDK; Zstandard comes
from cpktmisc v0.3.0. CMake receives explicit static archive/header paths and
Meson uses a scoped `llvm-config` adapter for those same provider paths. Unknown
LLVM system-library dependencies fail rather than falling back to host packages.
Both SDK manifests are validated before LLVM configuration. The installed LLVM
SDK is left untouched, including any workstation paths in its original metadata.
Clang headers must match the exact LLVM version reported by `llvm-config-23` or
`llvm-config`; the host compiler installation is not modified. Native sources/builds/installs
have separate per-component `host` roots.
SPIR-V Tools and glslang share the headers revision selected by their upstream
dependency declarations. `make test-gpu-tools` exercises actual GLSL/SPIR-V
compilation, validation/optimization, invalid inputs and an LLVM kernel round
trip, plus OpenCL through Mesa CLC and SPIR-V binding generation. Invalid GLSL,
OpenCL, missing SPIR-V, mismatched LLVM/Clang and missing SDK prerequisites have
explicit failure checks. The installed Mesa generators run with their dependency
paths resolved and bundle the LLVM OpenCL resource headers. These tools and host
libraries are excluded from target pkg-config/CMake roots and the SDK.

Project sequencing stays in Make/Bash/CMake/CTest. Invoke Mesa's native Meson
build as a dependency build with an explicit cross file and prepared host tools;
do not create a competing Python build controller. Target C/C++ comes from the
pinned Bootlin collections. Isolate pkg-config and library/header discovery to
the selected target roots, disable fallback downloads, and check the resolved
graph: Mesa probes libelf/libudev even with auto_features disabled.
No inferred host library is allowed to enter a native or cross target.

Keep driver/compiler/libdrm/native runtime archives separate from libailo.a.
Installed CMake and pkg-config metadata must carry their complete static order
and private dependency paths after SDK relocation. Check exports, source lists,
generated shader notices and shipped licenses. Existing libstdc++/libgcc remain
accepted under the GCC Runtime Library Exception; host compiler license does
not determine the additional GPU userspace license policy.

## Static musl adaptations

ANV initializes device UUIDs by locating its ELF build-ID note through Mesa's
build_id_find_nhdr_for_addr. That uses dladdr/dl_iterate_phdr and cannot simply
be assumed to work in static musl. ANV initialization fails if the note is absent
or too short. Supply a deterministic embedded per-driver build identity and
adapt UUID/cache identity generation to use it. This adaptation is implemented
for the selected profile. RADV already has a
RADV_BUILD_ID_OVERRIDE path. Include source revision, patches and relevant
compiler/configuration identity in the input; preserve the upstream device and
configuration contributions to cache UUIDs. Disabling disk cache does not remove
this initialization requirement. Prove reproducibility and invalidation when
shader/compiler inputs change, without runtime DSO introspection. Focused tests
verify unchanged-input identity and file timestamp stability, distinct provider
identities, and invalidation on compiler or target changes. Binding this closure
to libailo's inference-cache identity is implemented by additionally hashing the
actual selected GPU archives into its native-state compatibility identity.
The compiled GGML Vulkan archive is hashed after its CMake target builds and
before either facade compiles. This also binds state to the generated shader
bytes. The shader compiler executable is an explicit generation dependency;
changes to its installed runtime/tool collection still require normal native
build invalidation under the lifecycle contract.

ANV also conditionally supports flushing noncoherent host-visible memory under
SUPPORT_INTEL_INTEGRATED_GPUS. Otherwise such a memory configuration fails
initialization. AArch64 needs explicit compilation/coherency validation and
hardware proof for each advertised Intel/device combination. Do not turn that
failure into false support or assume PCIe attachment solves architecture limits.

Audit optional Mesa loading helpers, RenderDoc, backend plugins and libc support
paths. Explicitly disable GGML_BACKEND_DL. A static archive alone proves none of
these runtime properties; no embedded shared object extraction workaround is
allowed. Preserve direct host/kernel interfaces needed for normal DRM operation.

## Required multi-GPU behavior

Support layer splitting across AMD-only, Intel-only and mixed AMD/Intel devices
in one process. Device enumeration exposes provider, device name, vendor/device
IDs, Vulkan device UUID and PCI identity when available, capability/availability
reason, and known memory values. Unknown memory availability is explicit;
integrated/shared memory is not reported as dedicated VRAM. Identify selected
devices by stable identities, not persisted enumeration indices.

Configuration supplies an ordered selected-device list and optional nonnegative
layer-distribution weights with at least one positive entry. Validate list
uniqueness, matching counts, finite weights and device usability before loading.
Normalize weights into the native tensor_split inputs used for layer allocation;
document native layer rounding. Without weights, retain the pinned native
free-memory distribution heuristic across the selected devices. Exclude devices
not selected, and fail clearly if an explicit participating device cannot receive
the requested placement. Record requested and actual per-device layer ownership.

Layer splitting is the required Vulkan strategy. The pinned Vulkan registry does
not supply the split-buffer procedure used for native row splitting; reject a
receiver request for that unsupported mode instead of silently interpreting it
as a layer split. Preserve the full native facade and its documented native
semantics. This requirement does not introduce a promise of tensor-parallel,
distributed network or concurrent execution of layers on separate GPUs.

Distributed coordinator/participant routing is a separate future backend,
explicitly outside the first few versions, as recorded in
[the inference backend contract](inference-backend.md#future-distributed-coordinatorparticipant-backend).

Memory planning considers weights, KV cache, graph/scratch buffers, staging and
host RAM, not just the GGUF file size or the sum of advertised VRAM. MoE active
parameter counts do not remove inactive expert storage. Report CPU-resident
layers and operation fallback honestly. Any fallback policy must honor explicit
placement commitments. Allocation/device loss/evaluation failures end the request
without a false successful completion or finish chunk; auto CPU setup retry is
limited to uncommitted setup, not resuming a partially streamed answer.

Separate clients can select different devices. Synchronize process-wide native
initialization and teardown; destruction of one client must not invalidate
another. Preserve busy/reentry/cancellation rules and reclaim driver, context,
pipeline, mapped memory and model resources on failure and repeated load/destroy.

## Verification after the full implementation

Implement both providers, dispatch, receiver acquisition/cards and multi-GPU
placement before running the comprehensive acceptance campaign. Focused checks
during implementation remain useful. Do not replace required capabilities with
test-only stubs or reduce delivery to the hardware presently on this host.

Required evidence includes:

- Strict C89/C++ public header tests, observable device/card/configuration
  behavior, independent provider dispatch, invalid selection/split errors and
  complete facade/callback coverage under the main architecture contract.
- All four target builds and relocated installed static/shared consumers.
  A fully static musl consumer must acquire/cache a model over verified HTTPS,
  then perform GPU inference offline. Inspect final stripped bytes for PT_INTERP
  and DT_NEEDED and audit the resolved archive/source/license closure.
- Initialization, enumeration, pipeline creation, trained prompt evaluation and
  decode with no installed userspace ICD/layer libraries or manifests. Trace
  file/library access and mapped objects throughout execution, including error
  and teardown paths; deny forbidden shared-object loads and prove no attempts
  are needed. Repeat with misleading ICD/layer environment settings.
- Actual AMD and Intel trained inference, comparison with CPU using documented
  numerical/logit tolerances, bounded buffered and true streaming output, usage,
  cancellation, error behavior and model-card facts. Synthetic tests do not
  substitute for the pinned trained instruct fixture in architecture.md.
- AMD/AMD, Intel/Intel and AMD/Intel layer splitting, with evidence that every
  participating selected GPU owns layers and executes work. Include explicit
  weights, automatic distribution, partial offload, unequal memory, bad device
  identities, insufficient memory and repeated load/destroy. Enumeration or a
  log stating Vulkan is enabled is insufficient execution evidence.
- At least one larger dense and one MoE GGUF workload, with exact model revision,
  file hashes/license, architecture/quantization, template, context and resource
  limits recorded. Select those fixtures to fit the qualification machines;
  verify their operations actually run on the intended backend and report CPU
  fallback. Do not claim every upstream architecture or model is qualified.
- Memory/lifetime checks, driver-build UUID reproducibility, shader-cache-disabled
  operation, privacy/relocatability, package/export/source archive checks, and
  measured binary/model memory, prefill/decode throughput and pipeline startup.

Missing hardware or tools leaves the corresponding gate pending and prevents a
support claim; it is not a successful skip. Keep required multi-GPU coverage
explicit even if this workstation supplies only one Intel GPU. Record device,
kernel, firmware and CPU architecture with hardware results. Neither open
weights nor aggregate memory establishes frontier-level accuracy or throughput.

## Current evidence

The existing foundation compiled CPU/Vulkan SDKs across the four targets and
ran native synthetic GPU inference through a shared loader and host Intel driver.
The source audit identified permissible RADV/ANV dependency profiles, the
embedded shader measurement and the static integration changes above. libdrm
core/amdgpu and the selected static RADV/ANV profile now build for all four Linux
targets. Their relocated static consumers pass archive, metadata and ELF checks:
no dynamic interpreter or `DT_NEEDED`, no shared driver or ICD manifest payload,
one copy of selected common runtime/compiler symbols, and no unresolved dynamic
loading, ELF, LLVM or OpenMP dependency in the Mesa archives. Both x86-64 libc
profiles create instances for each provider and enumerate one Intel Graphics
RPL-U device through ANV and zero devices through RADV. AArch64 evidence is
compile/link only.

The x86-64 glibc integration now passes synthetic libailo GPU inference and the
exact shared ABI export check through the built-in loader and static ANV driver.
The x86-64 musl release build also passes the relocated built-in loader/device
probe. Its incremental build reuses shader and library objects. A standalone
strict C89 consumer of the installed CMake static target runs the existing
`examples/infer.c` without an ELF interpreter or `DT_NEEDED` entries. Its stripped
size is 95,010,784 bytes (90.61 MiB).

On Intel Graphics RPL-U, PCI `0000:00:02.0`, Linux `6.17.0-41-generic`, that
executable downloaded the pinned SmolLM2 fixture through libailo's Hugging Face
facility into `$XDG_CACHE_HOME/ailo`. SHA-256 verification succeeded. All 30
repeating layers and the output layer were offloaded; the model card records
Vulkan model/context/compute buffers of 103,668,480 / 47,185,920 / 26,873,856
bytes at a 2,048-token context, with host buffers reported separately.

The same stripped executable then generated offline from the cache with socket
creation denied, misleading ICD/layer environment settings and disk shader
caching disabled. The trace records one application exec, no socket/connect,
shared-library or manifest attempts, and successful i915 GPU submissions. A
write trace confirms incremental stdout fragments during decoding. Its 12-token
greedy output matches both the download run and a CPU run. This is a text-output
comparison, not a numerical/logit qualification. The download/inference run took
7.17 seconds and reported peak RSS of 131,364 KiB; neither number isolates
prefill/decode throughput or complete GPU memory use.

All four release configurations now build the integrated library and strict C89
header probes. Their extracted development SDKs pass target ABI, privacy and
exact-export checks. The x86-64 glibc release suite passes 72/72 CTests; the
installed Lua/shared C ABI passes its three Vulkan-profile tests, including
LuaRocks installation. The same trained Lua hardware scenario described above
also passes with the built-in static ANV provider: first text at token 1,
cancellation, explicit placement and independent-client teardown. Scalar/native
bridge tests pass across all four targets, using QEMU for AArch64 execution.
These checks do not establish AArch64 GPU execution.

The refreshed static musl executable, including the compiled-shader cache
identity, repeats the offline GPU/CPU output comparison and incremental output
trace successfully. It performs 488 successful i915 submissions, with socket
creation denied and no shared-library or manifest attempts. The executable size
remains 95,010,784 bytes.

The final core refresh passes 72/72 glibc and 69/69 musl CTests; the different
counts reflect three glibc-only packaging/install gates. All four refreshed
development SDKs pass extraction, target ABI, privacy and exact-export checks.
The current strict C89 static musl executable is 95,113,184 bytes and again has
no interpreter or dynamic dependencies. Its offline Intel SmolLM2 run makes
488 successful i915 submissions, no network/shared-library/manifest attempts,
and writes output fragments between decode submissions. Its 12-token output
matches the CPU run. Component parser/writer and snapshot RSS bounds are
measured separately in [the receiver tests](client.md#encrypted-prompt-snapshots).

The comprehensive hardware campaign remains pending: firmware identification,
AMD, AArch64 GPU and multi-GPU execution, and numerical/performance acceptance.
Larger dense/MoE execution evidence and numerical reference comparisons are
recorded below. The earlier host-driver receiver proof
remains separate from this static-driver evidence. These are development
checks, not a signed or tagged release.

### Raphael qualification, 2026-10-27

This section records the failing baseline. The
[2026-11-02 repair](#raphael-shared-memory-repair-2026-11-02) below supersedes
its CPU-only recommendation for the tested GNU workloads.

The LLVM host generator stack now supports LLVM/Clang 23, with translator
23.1.0 and matching installed Clang development headers. This host uses LLVM
23.1.4. The pinned SDKs are cpkt 0.5.2 and cpktmisc 0.2.1, including zlib,
libxml2 and Zstandard for the host generators. No host compression/XML library
is selected. The target RADV/ANV profile still builds without LLVM.

The machine is a Ryzen 5 7600 running Ubuntu 26.10 and Linux
`7.3.0-9-generic`. Its integrated Radeon is PCI `0000:0f:00.0`, vendor/product
`1002:164e`, revision `c6`, with `amdgpu` and an accessible render node. Built-in
Mesa 26.2.4 RADV reports `RAPHAEL_MENDOCINO`, UUID
`000000000f0000000000000000000000`, UMA, subgroup size 32, FP16/dot2 and integer
dot support, 64 KiB shared memory and no matrix cores. The 512 MiB firmware
reservation is part of shared memory, not dedicated VRAM. Firmware identification
records DMUB `0x05003500`, VCN ENC 1.33/DEC 4/revision 15 and SMU 84.79.226.
The shader compiler is host shaderc/glslc 2026.3.

The upgraded SDK foundation matrix passes eight static/shared libffi tests
across x86-64 and AArch64, each with GNU and musl; AArch64 uses QEMU. The native
GNU build passes 78 tests, including archive/install contracts, and installed
Lua passes five. The LLVM kernel round trip and Mesa OpenCL/binding generators
pass, including rejected invalid inputs and checks for SDK compression/XML
providers. These are build/regression results, separate from GPU qualification.

Synthetic GPU inference and the installed SmolLM2-135M receiver smoke test pass
with actual model and KV offload. Trained serving is **not qualified**: CPU
passes all six cases, while Vulkan passes protocol, transport, vision and audio
but fails compaction and Codex tool execution. Both failures report
`Native sampling logits are not finite`. Compaction returns HTTP 500; the Codex
case fails during an already-started SSE response. Select `backend="cpu"` on
this host while the numerical failure remains unresolved.

`ailo_qualify_matrix` is a manual native GGML probe, excluded from ordinary
builds and CTest like `ailo_qualify_logits`. It selects the first Vulkan device
and returns 77 when none is available, 2 for invalid arguments and 1 for a
numerical/compute failure. Each process runs eight repetitions of three matrix
cases: K=256, M=16, N=128/121, with explicit FP32 accumulation for the large
values, plus a small default-precision case. Inputs and expected results are
exact powers of two; it checks every output for finiteness and absolute error
less than 1. Storage can be F16 or F32. All allocated bytes start poisoned, then
every input element is uploaded. The large expected result is exactly 262144.

One final campaign used 100 fresh processes per configuration, the built-in
loader, forbidden external ICD paths and disabled disk shader caching:

| Stored left operand | Native FP16 computation | Passed processes | Failed processes |
| --- | --- | ---: | ---: |
| F16 | enabled | 33 | 67 |
| F32 | enabled | 89 | 11 |
| F16 | disabled | 80 | 20 |
| F32 | disabled | 68 | 32 |

Failures frequently affect the first matrix dispatch, with later graphs in the
same process passing. For example, an output is 262209 instead of 262144 with
F32 storage and FP16 computation disabled. Earlier short controls passed;
the repeated failures rule out treating those passes, disabling FP16, or a
single shader-optimizer experiment as qualification. The cause has not been
isolated to GGML, the compiler, Mesa, kernel/firmware or hardware. No experimental
arithmetic override or shader change is retained.

Follow-up isolation on 2026-10-28 reproduces incorrect outputs in a standalone
Vulkan program running the extracted FP32 GGML matrix shader, without libailo,
GGML's scheduler, model loading or liblockdc. The SPIR-V validates. A padded-input
case fails 19/300 fresh processes; a two-wave case with required subgroup size
32 fails 42/500. The inputs remain unchanged. Separate constant-write, copy and
simple scalar-matrix controls each pass 100 processes. These controls narrow
the investigation to the tiled kernel's compilation/execution on this stack;
they do not identify the ultimate shader, driver or hardware cause. Disabling
ACO scheduling passes an initial 150 runs but fails 3/1500 on repetition, so it
is not a qualified workaround. These raw probes check exact results, a stricter
criterion than the original matrix gate. Local handoff sources, shader bytes,
counts and caveats are in `stash/raphael-vulkan/`, with full logs under
`build/raphael-investigation/`. No GPU implementation change is accepted.

Reproduce against the normal build, repeating each command in fresh processes:

```sh
cmake --build build/x86_64-linux-gnu-release --target ailo_qualify_matrix --parallel 2
MESA_SHADER_CACHE_DISABLE=true build/x86_64-linux-gnu-release/ailo_qualify_matrix f16
MESA_SHADER_CACHE_DISABLE=true GGML_VK_DISABLE_F16=1 build/x86_64-linux-gnu-release/ailo_qualify_matrix f32
scripts/test_openai_inference.sh x86_64-linux-gnu-release offline cpu
ctest --test-dir build/serving-inference-x86_64-linux-gnu-release-vulkan --output-on-failure -j1
```

The serving commands require the fixtures prepared by the documented explicit
download step. Local evidence is under `build/llvm23-upgrade/`:
`foundation-matrix.log`, `gpu-tools-sdk.log`, `gpu-tools-check.log`,
`gnu-final-test.log`, `matrix-final-summary.txt`, `matrix-final-*.log`,
`matrix-fp32-compute-*.log`, `cpu-serving-final.log` and `gpu-serving-final.log`.
Larger dense/MoE numerical and performance acceptance, the new host's static
musl hardware campaign, AArch64 GPU and multi-GPU execution remain pending.
The next qualification step is to isolate this matrix/serving failure before
collecting performance claims or accepting a device-specific workaround.

A 2026-11-02 recheck compares the padded FP32, required-subgroup-32 two-wave
standalone case across 500 fresh processes per stack. The built-in Mesa 26.2.4
passes 402 and produces incorrect results in 98; the workstation's dynamic
Mesa 26.2.6 (`26.2.6~kisak1~s`) passes 405 and fails 95. Neither stack reports
setup errors or skips, and inputs remain unchanged. Both use memory type 3 and
the same validated shader, SHA-256
`9f875beba3c839333f3bcfdddf6c5d444e0c61f73a103f286146002bbb68d2f1`.
The probe binaries are compiled separately; this compares userspace stacks,
not individual driver commits. The kernel remains `7.3.0-9-generic`.
Commands, binaries and logs are under `build/gpu-recheck-20261102/`.
The host Mesa upgrade does not change libailo's privately linked Mesa pin,
and the dynamic control does not establish static deployment acceptance.
This recheck preceded the repair below. [TODO.md](../TODO.md) tracks remaining
precision, upstream, performance and separate Halo acceptance work.

### Raphael shared-memory repair, 2026-11-02

The failure reduces to integer shared-memory transfers: no model, GGML graph
scheduler or floating-point arithmetic is needed. Global-load/FMA and global
bit-copy controls pass, while small shared-memory allocations fail. For example,
the 1 KiB integer sentinel fails 99/100 processes and the 8 KiB sentinel fails
73/100. Wrong XOR results sometimes differ by a workgroup tag. This implicates
LDS execution and synchronization on this stack; it does not prove a particular
silicon, firmware or kernel defect.

`vendor/mesa/patches/0003-raphael-lds-isolation.patch` applies two measures to
Mesa's `CHIP_RAPHAEL_MENDOCINO` family. RADV reserves the complete 64 KiB LDS
region for compute shaders using shared memory; ACO retains explicit LDS waits
at barriers, including subgroup/single-wave cases otherwise optimized away.
RADV retains the family in its shader cache key so affected binary configurations
cannot share the generic ACO family identity. Other families retain their
existing code generation and allocation. Only Raphael has hardware proof here.

Allocation alone passes the original tiled shader but fails the maintained
single-wave integer probe. Explicit waits alone fail that probe too. A 32 KiB
reservation passes early arithmetic probes but still produces non-finite logits
in trained FP16 compaction. The accepted combination uses the full region and
explicit waits. It reduces occupancy and adds synchronization; its performance
cost has not been benchmarked. No host GPU settings, FP16 disabling, CPU fallback,
external shared driver or ACO/RADV debug setting is part of the repair.

The final native GNU build, private Mesa 26.2.4 and unchanged kernel pass:

| Gate | Result |
| --- | --- |
| Maintained integer LDS probe | 1,500 fresh processes, eight dispatches each; exact outputs and untouched guards |
| Original padded FP32 tiled shader, required wave32, memory type 3 | 1,500/1,500 fresh processes; exact 262144, unchanged inputs |
| GGML matrix, F16/F32 storage and FP16 enabled/disabled | 100/100 fresh processes per configuration; 9,600 graphs total |
| F16 Qwen CPU versus Vulkan, 16 identical prefixes, full vocabulary | max logprob error 0.0625267, RMS 0.00966180, KL 0.0000115439; no argmax differences |
| Trained Vulkan serving | 6/6: protocol, compaction, transport, real Codex tool turns/opaque replay, vision, audio; FP16 enabled |

The final native offline suite passes 80/80 and installed Lua passes 5/5.
Formatting, shell syntax, source/archive/install contracts and relocated static
RADV/ANV plus built-in-loader checks pass. The loader check runs with poisoned
external ICD/layer paths and traces file/process access; no shared GPU driver or
discovery manifest is required.

Logit tolerances remain max 0.25 / RMS 0.025 / KL 0.001 nats. The reference is
the F16 representation of the stored Qwen2.5-0.5B Q4_K_M weights, generated by
`ailo_qualify_logits dequantize`; SHA-256
`f5f90b6b937fb032591a76e87d277566ada3d965510b58750ce80de78efe2e8e`.
The original Q4 file has SHA-256
`6eb923e7d26e9cea28811e1a8e852009b21242fb157b26149d3b188f3a8c8653`.
This is a representation of the quantized weights, not recovered training
weights. All serving cases use the original pinned quantized fixtures.

Default quantized Qwen probability equivalence was **unqualified at this baseline**. Against
that F16 reference, the default Q4 Vulkan path has worst max/RMS/KL errors
0.469540/0.104982/0.00336607 with finite outputs and no differing argmax. Its
integer-dot kernels use quantized Q8_1 activations. The diagnostic
`GGML_VK_DISABLE_INTEGER_DOT_PRODUCT=1` passes unchanged tolerances:
0.0722278/0.0115874/0.0000122658. Disabling FP16 or dot2 alone does not pass.
The diagnostic switch was not installed as a default then. The 2026-11-03
profile below adopts floating activations for Raphael. Neither these
finite differences nor the passing F16 pair establish universal model accuracy.

The installed Codex client here is `0.166.0-alpha.6`. The trained test now gives
protected instructions and a full 512-token summary a 3,072-token target, with
twice the removable tool output to keep compaction mandatory. The old 2,450-token
budget passes a shorter CPU summary but rejects a longer finite Vulkan summary.
The revised test still requires two actual tool executions, compaction, opaque
replay with raw tool history removed, and exact recovery of the deployment code.
No product compaction limit or protection rule was relaxed.

Run the maintained hardware regression explicitly; missing hardware is a failure:

```sh
make test-vulkan-memory PRESET=x86_64-linux-gnu-release GPU_MEMORY_RUNS=1500
scripts/test_openai_inference.sh x86_64-linux-gnu-release offline vulkan
```

`ailo_qualify_shared` is excluded from normal offline CTest. Its shader is compiled
and embedded at build time; the executable directly links the private Vulkan
stack and needs no runtime shader file or compiler. It dispatches 32 workgroups
at 64/128 invocations, using 1/8/32/64 KiB shared allocations where supported,
with eight write/barrier/read/barrier rounds per dispatch. It checks every result
and the untouched output tail. Disk shader caching is disabled for repeated
fresh-process qualification. Logs and experimental ablations are under
`build/raphael-reduction/`; local reduction sources are under ignored
`stash/raphael-reduction/`. Static-musl hardware acceptance, Mendocino, Halo,
other GPUs, sustained workloads and performance remain separate gates.

### Raphael precision and static-musl validation, 2026-11-03

The accepted Raphael policy now selects floating-point activation kernels for
PCI `1002:164e`. `vendor/llama/patches/0010-prefer-float-activations-on-raphael.patch`
disables this device's effective integer-dot capability in both device setup
and diagnostic reporting. FP16 remains enabled. This is a precision policy:
the alternative Q8_1 activation conversion changes probabilities. It is not a
claim that the hardware's integer-dot instructions are defective. Other device
IDs retain the existing selection, including the existing disable environment
variable. This baseline used upstream `b11430`; the subsequent `v0.8.0` upgrade
retains this policy and the full Mesa LDS reservation.

Eight synthetic matvec/matrix cases cover Q4_K, Q6_K, Q5_0 and Q8_0 weights,
one/128 RHS columns and controlled activation outliers. With integer dot enabled,
affected kernels match a CPU Q8_1 activation reference much more closely than
the floating-activation reference. For Q8_0 matrix multiplication, normalized
RMS errors are about 0.009 against floating activations and 0.00000013 against
Q8_1. Selecting floating activations reverses that relationship. This supports
activation quantization as the source of the finite logit difference; it is
separate from the LDS corruption.

The real Qwen2.5-0.5B Q4_K_M candidate now passes the unchanged F16 CPU-reference
gate in both GNU and musl with no environment override: maximum normalized
logprob error 0.0722278 nats, RMS 0.0115874 and KL 0.0000122658, with no differing
argmax across 16 identical prefixes and the full vocabulary. The original and
reference hashes are recorded in the preceding section. Both builds also pass
all six trained Vulkan serving cases with the new default, retaining FP16.

The musl campaign uses fully static shared-memory, matrix, logits, serving and
stream-client executables: no `PT_INTERP` or `DT_NEEDED` entries. Its hardware
checks pass 1,500 fresh shared-memory processes / 12,000 dispatches, plus 100
fresh matrix processes for each F16/F32 storage and FP16 enabled/disabled
combination (9,600 graphs). The six serving cases run with poisoned external
ICD paths and file/process tracing. Their native traces contain one initial
execution, no helper execution and no shared-object load.

The fresh musl build exposed a build-time portability defect: Vulkan's native
`asm_offset` generator requested a workstation `/lib/ld-musl-x86_64.so.1`.
The loader build now statically links its executable helpers with the pinned
collection. The static-loader integration check rejects a generator containing
an interpreter or dynamic dependency. No host musl installation is required.
The manual matrix and logits tools also link fully statically for musl.

The final offline suites pass 80/80 native GNU tests, 77/77 musl tests and 5/5
installed Lua tests. Formatting and shell syntax checks pass. Relocated static
RADV/ANV and built-in-loader checks pass for both GNU and musl, including the
offset-generator ELF assertion. GNU package/install/export and source-archive
checks are included in the offline suite. This is development acceptance on
Raphael; a fresh release build and the full multi-target SDK campaign remain
separate work.

A smaller reservation is still insufficient. A 32 KiB minimum with explicit
LDS waits passes 1,500 integer-probe processes and the first three trained
serving cases, then fails Codex remote compaction with non-finite logits.
Restoring the complete 64 KiB reservation passes the same Codex case. The
production LDS patch remains unchanged.

#### Scoped performance measurements

The native GNU benchmark fixes 256 prompt tokens and 32 subsequent tokens, a
512-token context, a 256-token batch / 128-token microbatch and two CPU threads.
Each process warms once, then performs five iterations with KV cleared each
time. Explicit synchronization bounds the prefill/decode timers; logit-finiteness
scans and loading are outside those timers. Inputs are fixed across profiles;
the GPU logs confirm 25/25 layers offloaded. Power settings were unchanged.

| Profile | Prefill tokens/s | Decode tokens/s | Warm samples |
| --- | ---: | ---: | ---: |
| Original Mesa, integer-dot activations; known incorrect driver | 651.7 | 29.1 | 5 |
| Full LDS repair, integer-dot activations | 347.2 | 18.2 | 5 |
| Full LDS repair, accepted floating activations | 332.0 | 16.4 | 15 across three processes |
| CPU control, two threads | 216.2 | 69.5 | 5 |

Rates use total tokens divided by accumulated time. The original driver is a
diagnostic performance control, not a usable correctness baseline. With the
LDS repair, floating activations reduce prefill throughput by about 4–5% on this
fixture. Decode variation in the integer-dot control spans 16.2–20.1 tokens/s;
the final profile spans 16.2–16.6. These samples do not establish a precise
activation-policy decode penalty. The full LDS reservation has a substantial
cost, and the smaller reservation failed acceptance.

For this short interactive workload on the tiny Raphael GPU, the two-thread
CPU control decodes much faster. GPU prefill is faster. Choose a backend using
the actual workload; these measurements are not broader model/hardware or
sustained-performance claims. Timing/reduction sources remain under ignored
`stash/raphael-reduction/`; data, binary hashes and ablations are under
`build/raphael-followup/`.

### llama.cpp v0.8.0 qualification, 2026-11-03

The dependency now pins stable `v0.8.0`, commit
`8642541b255bfb5dc39cd351a178be09013a55d6`. The ten integration patches are
retained, with four rebased; one additional patch adds attention tests only.
The upstream memory-budget clamp supersedes our previous underflow guard.
Raphael still uses floating activations, FP16, the full 64 KiB LDS reservation
and explicit LDS barrier waits. The earlier throughput measurements above used
`b11430` and have not been repeated for this pin.

On the same Ryzen 5 7600/Raphael host:

| Gate | GNU | Fully static musl |
| --- | --- | --- |
| Offline native suite | 81/81 | 78/78 |
| Installed Lua suite | 5/5 | Not run on musl |
| Qwen strict F16-reference logits | Pass | Pass |
| Fresh matrix processes, four storage/precision configurations | 400/400 | 400/400 |
| Selected upstream attention cases, including new scratch-path cases | 48/48 | 48/48 |
| Trained text/compaction/transport/Codex/vision/audio serving | 6/6 | 6/6, with file/process traces |
| Relocated static RADV/ANV and built-in loader isolation | Pass | Pass |

Both trained-logit runs have worst max/RMS/KL errors
`0.0704010414 / 0.0115375858 / 0.0000117941872`, zero differing argmax positions
and zero tolerance violations. Model/reference hashes, identical-prefix protocol
and tolerances are unchanged. The static-musl integer-only isolation campaign
also passes 1,500 fresh processes / 12,000 dispatches. Shared-memory, matrix,
logits, backend-operation, serving-fixture and stream-client executables all
have no `PT_INTERP` or `DT_NEEDED`. Trained musl serving uses poisoned ICD paths
and rejects shared-object loading or runtime helper execution.

The pin includes AMD's `kv_raw_strided` attention scratch-copy implementation,
gated to AMD except RDNA4. The original 24 selected upstream cases cover decode,
prefill, F16 layouts and Q8_0/Q4_0 KV. They do not cross the new path's 256-token
threshold. Patch 0011 adds 24 cases at batches 255/256/512, both precisions and
contiguous/permuted layouts with and without padded cache views. The dense
permuted cases at 256/512 enter the new path; the other cases test its bypasses.
All compare actual Vulkan computation with CPU using upstream's unchanged
`5e-4` normalized mean-square-error tolerance. Reproduce the new cases after
preparing the selected build:

```sh
build=build/x86_64-linux-musl-release
cmake --build "$build" --target ailo_qualify_backend_ops --parallel 2
params='hsk=64,hsv=64,nh=4,nr23=\[1,1\],kv=512,nb=(255|256|512),mask=1,sinks=0,max_bias=0\.000000,logit_softcap=0\.000000,.*type_K=f16,type_V=f16'
MESA_SHADER_CACHE_DISABLE=true timeout 300 "$build/ailo_qualify_backend_ops" \
  test -b Vulkan0 -o FLASH_ATTN_EXT -p "$params"
```

Require `24/24 tests passed`; selecting zero cases is not a successful gate.
This tool is excluded from normal builds and installation.

The upgrade exposed stale project-owned template objects after archive
replacement: upstream timestamps were older than the existing objects. Those
objects now depend on the extraction contract. A regression replaces a header
with an older timestamp and proves that changing the contract rebuilds the
executable with the new layout. Fresh archive reconstruction plus the eleven
zero-fuzz patches exactly matches both prepared source trees. The regenerated
C89 facade covers 1,111 functions and 329 fields with zero gaps; native API
cutovers are recorded in the [integration record](../vendor/llama/README.md).

Logs and reconstruction inputs are under `build/llama-upgrade/`. These results
qualify this development profile on Raphael. Actual Halo, Mendocino, RDNA4,
non-AMD hardware, BF16 and long-context stress remain separate gates.

### Upstream Raphael ablations, 2026-11-04

The upgrade does **not** replace either local protection. These isolated GNU
builds use llama.cpp v0.8.0 and Mesa 26.2.6 on the same Ryzen 5 7600, PCI
`1002:164e` revision `c6`, gfx1036, two compute units, kernel `7.3.0-9`.
Production sources, cached SDKs and the shipped precision policy are unchanged.

For the activation comparison, copy the prepared GGML source, reverse only
llama patch 0010, compile `ggml-vulkan.cpp` with its original compile-database
arguments and replace that member in a copy of `libggml-vulkan.a`. Relink the
maintained qualifiers and serving fixtures against the installed static SDK,
replacing only that imported archive. Keep the repaired Mesa in every activation
variant. No `GGML_VK_DISABLE_F16` override is used.

| Activation variant | Worst max / RMS / KL error | Differing argmax / failing positions | Trained serving |
| --- | --- | --- | --- |
| Shipped floating activations | 0.0704010414 / 0.0115375858 / 0.0000117941872 | 0 / 0 | 6/6 |
| Remove patch 0010 | 0.476406959 / 0.105654873 / 0.00300542071 | 1 / 16 | 6/6 |
| Same experimental binary, disable integer dot by environment | Identical to shipped control | 0 / 0 | Not repeated |
| F16 CPU model versus F16 GPU model, experimental binary | 0.0650553487 / 0.00927953292 / 0.0000127726757 | 0 / 0 | Not repeated |

The unchanged strict limits are max `0.25`, RMS `0.025`, KL `0.001`, over the
same 16 identical-prefix full-vocabulary positions. The Q4 candidate SHA-256 is
`6eb923e7d26e9cea28811e1a8e852009b21242fb157b26149d3b188f3a8c8653`;
the F16 reference is
`f5f90b6b937fb032591a76e87d277566ada3d965510b58750ce80de78efe2e8e`.
Successful generation is insufficient evidence of this accuracy policy.
These results isolate the quantized-model activation path; they do not prove a
hardware integer-dot defect or accuracy equivalence for other models.

For LDS, extract a fresh Mesa 26.2.6 archive, verify SHA-256
`b9436b3e1a8be76fbd4823dc884f90596ab7c6ac5e1867a305e66ffccfb31a92`,
and apply only patches 0001 and 0002. Omit 0003 in full. Build/install with the
same static headless cross/native files and Meson options as production, using
two build jobs. Assign distinct driver build UUIDs to the trial so it cannot
reuse production pipeline identities. Replace all matching imported Mesa
archives in the installed-SDK fixture; keep the shipped GGML archive. Rebuild
the maintained integer-only `ailo_qualify_shared` against each profile.

| Mesa profile | Fresh processes | Correct | Corruption | Setup errors/timeouts |
| --- | --- | --- | --- | --- |
| Full shipped LDS repair | 500 | 500 | 0 | 0 |
| Upstream LDS behavior, no patch 0003 | 500 | 1 | 499 | 0 |

Each invocation has a 60-second timeout and disables the shader disk cache.
The maintained shader covers eight workgroup/shared-size configurations; failed
processes stop at their first mismatch. The first failing upstream run reports
incorrect integer XOR results with 1 KiB shared memory and 64 lanes. This probe
does not involve llama.cpp, floating-point arithmetic or model quantization.
The control covers 4,000 correct dispatches. This experiment removes the whole
repair, not its two components separately; the earlier component and 32 KiB
reduction evidence remains applicable. Retain both protections. Neither a
silicon/kernel root cause nor actual Mendocino/Halo acceptance is established.

Local fixture sources, exact compilation command, driver/source inputs and
named logs are under ignored `build/raphael-revalidation/`. The completed
serving logs are `logs/serving-control-final.log` and
`logs/serving-integer-dot-valid.log`; earlier harness runs with missing companion
executables are not acceptance results. The shared-memory summaries and first
upstream failure are in `logs/*shared-summary.txt` and
`logs/upstream-lds-shared-first-failure.log`.

### Larger dense and MoE execution probes

`tests/qualify_models.lua` is a manual installed-Lua/C-ABI probe, separate from
the offline CTest suite. Use the matching runner/module paths from `lua-receiver`
and supply an absolute cache root and existing output directory, followed by
`--download` only to permit acquisition. It acquires through the normal receiver,
validates these pinned SHA-256 values, then runs both backends offline with
256 context tokens, a 128-token batch, two CPU threads, prompt caching disabled,
greedy sampling and 16 output tokens. Both publishers identify Apache-2.0;
the loaded GGUF cards agree. These are Q4_K_M files with mixed Q4_K/Q6_K tensors.

| Fixture | Repository / immutable revision | File / SHA-256 |
| --- | --- | --- |
| Dense, 1,711,376,384 parameters, `llama` | [bartowski/SmolLM2-1.7B-Instruct-GGUF](https://huggingface.co/bartowski/SmolLM2-1.7B-Instruct-GGUF/tree/1f03464768bfcc0319fc50da8ff5fb20b6417ba2) / `1f03464768bfcc0319fc50da8ff5fb20b6417ba2` | `SmolLM2-1.7B-Instruct-Q4_K_M.gguf`, 1,055,609,824 bytes / `77665ea4815999596525c636fbeb56ba8b080b46ae85efef4f0d986a139834d7` |
| MoE, 1,334,628,352 parameters, `granitemoe` | [bartowski/granite-3.1-1b-a400m-instruct-GGUF](https://huggingface.co/bartowski/granite-3.1-1b-a400m-instruct-GGUF/tree/940d2e1f9f65330615c7c8e980e6c5ac73d3360c) / `940d2e1f9f65330615c7c8e980e6c5ac73d3360c` | `granite-3.1-1b-a400m-instruct-Q4_K_M.gguf`, 821,847,360 bytes / `3a2ec1c2a78cb29d901e29bbf5162dcd03381e13803d2cbdcff838d4d08142eb` |

Both GGUF templates are used unchanged, with identities
`872be49dbb638044ad01b60388f48d469ff2980e5f0dccdc22ec907db54d0788`
and `22da301945ac6b617a8835d435cbbdc780c0ac08d69c07e1e17e6c6799eed800`,
respectively. On the same Intel RPL-U/i915, x86-64 and Linux
`6.17.0-41-generic`, built-in ANV placed all 24 repeating layers and the output
layer on Vulkan. Scheduler diagnostics assign Granite's `MUL_MAT_ID` expert
gate/up/down operations to Vulkan; input embedding `GET_ROWS` remains on CPU.
Both complete 16-token generation and deliver first text at token 1. An offline
repeat with socket creation denied makes no socket/connect attempts; misleading
ICD environment settings and disabled Mesa disk shader caching remain active.

One short offline run, under network syscall tracing, measured:

| Fixture/backend | Load/context setup s | First text s | Prompt tokens | Inclusive prefill tokens/s | Decode tokens/s |
| --- | ---: | ---: | ---: | ---: | ---: |
| Dense CPU | 1.56 | 0.81 | 41 | 50.62 | 16.30 |
| Dense Vulkan | 1.33 | 1.61 | 41 | 25.47 | 16.13 |
| MoE CPU | 1.09 | 0.38 | 53 | 139.47 | 48.39 |
| MoE Vulkan | 1.23 | 1.23 | 53 | 43.09 | 45.45 |

The Linux uptime clock has 10 ms resolution. Prefill includes rendering,
first-use pipeline work and first-token sampling; decode measures token events
after the first. These short diagnostic timings do not isolate shader pipeline
compilation or establish sustained throughput. Native Vulkan model/context/compute
bytes are 1,053,827,072 / 50,331,648 / 29,429,760 for dense and
820,107,736 / 12,582,912 / 27,334,144 for MoE. Process RSS after each GPU run is
149,444 and 107,408 KiB; driver allocations and mapped GPU memory are accounted
separately by the cards, with additional driver memory still unknown.

Direct quantized CPU/Vulkan equality **does not pass** the initial conservative
gate. A common dequantized reference below separates that native arithmetic
difference from GPU accuracy.
`ailo_qualify_logits` is an explicit, strict C89 target excluded from ordinary
builds and CTest. It compares the entire vocabulary at 16
identical prefixes by feeding each reference-selected token to both models. Both
files must represent the same model and tokenizer. It uses
the raw prompt `The sky appears blue because` and native defaults apart from
explicit placement, 256 context/128 batch tokens and two threads. Each position
requires maximum absolute normalized-logprob error <= 0.25 nats, RMS <= 0.025
nats and reference-to-candidate KL divergence <= 0.001 nats. All comparisons
are reported;
any violation exits nonzero. The deterministic fixture passes this tool. The
following comparison uses the original Q4_K_M file on both backends:

| Trained fixture | Worst absolute logprob error | Worst RMS | Worst KL | Differing argmax positions |
| --- | ---: | ---: | ---: | ---: |
| Dense | 3.24414 | 1.21621 | 0.0268033 | 1/16 |
| MoE | 1.71170 | 0.34109 | 0.0301260 | 0/16 |

The dense model's first-position discrepancy remains when disabling Vulkan
FP16 or CPU repacking independently. Native CPU Q4_K arithmetic uses
[Q8_K activation dot products](https://github.com/ggml-org/llama.cpp/blob/b11430/ggml/src/ggml-cpu/ggml-cpu.c#L312).
Cross-backend equality is therefore distinct from evaluating against a common
weight representation. Matching MoE greedy text also does not establish
probability equivalence.

The probe can produce an F16 representation of the **same stored quantized
weights** through the public native quantization API. This does not recover
pre-quantization training weights. It uses `allow_requantize=true`, `pure=true`,
F16 output and two threads, and rejects existing output files and symlinks.
Reference and candidate inputs each undergo SHA-256 verification. Reproduce with:

```sh
cmake --build BUILD --target ailo_qualify_logits --parallel 2
BUILD/ailo_qualify_logits dequantize MODEL SHA256 NEW_REFERENCE
sha256sum NEW_REFERENCE
BUILD/ailo_qualify_logits compare NEW_REFERENCE REFERENCE_SHA256 MODEL SHA256 vulkan
```

The reference always runs on CPU; the final argument selects the candidate's
backend (`cpu` or `vulkan`). Both use native defaults with the context, batch,
thread limits and unchanged numerical thresholds above. The resulting reference
files are 3,424,736,224 bytes (dense), SHA-256
`74d240e5689f87492f331bc4d4e124b0fc09266d579c0ed3c981dddedacd7759`,
and 2,672,669,536 bytes (MoE), SHA-256
`02feea575fa88510bc7228a6afe3b60121788e08a23521b8cc2ccbee70214522`.

Both quantized Vulkan candidates **pass** against those CPU F16 references:

| Fixture | Worst absolute logprob error | Worst RMS | Worst KL | Differing argmax positions |
| --- | ---: | ---: | ---: | ---: |
| Dense | 0.0442889 | 0.00958911 | 0.0000113948 | 0/16 |
| MoE | 0.0219071 | 0.00748924 | 0.00000730324 | 0/16 |

The dense quantized CPU candidate against the same F16 reference fails the
unchanged thresholds (worst KL 0.0819430). The dequantized dense CPU/Vulkan pair
passes (worst KL 0.00000900462). This isolates the large discrepancy in these
probes to the native quantized CPU path rather than the static GPU integration;
the original quantized CPU/Vulkan equality failures remain recorded above.
It establishes scoped numerical evidence for these two Vulkan workloads, not
bitwise equivalence between native arithmetic paths or a model-quality score.
Broader models, contexts, sustained performance and hardware acceptance remain
subject to the campaign above.

### Raphael 7B MoE and the 16 GB budget, 2026-11-04

A real larger MoE is feasible on this Ryzen 5 7600/Raphael host. The fixture is
[Allen AI's OLMoE-1B-7B-0125-Instruct-GGUF](https://huggingface.co/allenai/OLMoE-1B-7B-0125-Instruct-GGUF/tree/2ac1d27317927518c9ef7bd99f91f3f1e4ee288d),
immutable revision `2ac1d27317927518c9ef7bd99f91f3f1e4ee288d`, file
`OLMoE-1B-7B-0125-Instruct-Q4_K_M.gguf`, 4,213,512,192 bytes, SHA-256
`4ddc0e53159ed512b8dd67914a66e27bc618f694672ba43a9a0454eabd9c684f`.
The loaded card reports `olmoe`, 6,919,161,856 total parameters and 16 layers.
The graph routes eight of 64 experts. Both existing Raphael protections remain.

#### Memory accounting

The final acceptance profile caps charged host memory at 10,000,000,000 bytes
(page-aligned by Linux to 9,999,998,976), disables swap in the test scope and
reserves another 6 GB for GPU allocations and headroom. A host cgroup limit alone
is not a combined CPU/GPU allocation limit. DRM counters are collected from the
actual process, alongside native model/context/compute buffers; duplicate file
descriptors must be grouped by DRM client ID. These measurements do not claim
an exact accounting of every driver allocation or machine-wide swap activity.

The successful cold F32-reference run reaches the 10 GB host ceiling, with no
OOM/kill or test-group swap. File-cache reclamation at that ceiling is expected.
Its maximum process RSS is 9,756,152 KiB. Only this private reference's cached
pages were evicted before the cold run; no machine-wide cache flush was used.
The 27,678,429,184-byte reference is memory mapped, not fully resident. File
size and virtual address space are not physical working-set requirements.

With FP32 arithmetic and batch 8, the serving card reports 4,153,780,224 GPU
model bytes, 33,554,432 context bytes and 1,876,224 compute bytes. Host-visible
native buffers are reported separately. The process's DRM client has 4,116,676
KiB resident GTT plus 35,084 KiB VRAM after inference: 4,251,402,240 bytes total,
within the GPU reservation. After client close these fall to 2,296 and 320 KiB,
respectively, totaling 2,678,784 bytes. Residual device infrastructure is retained;
the model allocations are released. Host RSS and UMA mappings can alias GPU
memory, so these counters must not be presented as disjoint physical pools.

Earlier exploratory probes used a 16 GB **host-only** cgroup limit, including a
successful 13,844,566,016-byte F16 model load/decode on Vulkan. Those probes are
not combined-budget acceptance; the final 10 GB host / 6 GB GPU reservation
supersedes their memory-accounting method. No persistent workstation setting,
kernel tuning, adjacent repository edit or dependency source build was used.

#### Reference precision and numerical results

The qualifier now accepts `dequantize MODEL SHA256 NEW_OUTPUT [f16|f32]`.
Omitting the representation retains the existing F16 behavior. F32 is needed
to distinguish this model's routing sensitivity from the CPU F16 arithmetic
path: upstream's CPU F16 dot-product trait converts activations to F16 as well.
An F16 representation also rounds dequantized weights. It is not a full-FP32
oracle. Neither representation recovers original pre-quantization training
weights.

The reference files generated from the pinned Q4 weights are:

| Representation | Bytes | SHA-256 |
| --- | ---: | --- |
| F16 | 13,844,566,016 | `d84ad9cdf1150e5264a0dc318ddea6c695a6888250e8a01bbcf0499cb64bac71` |
| F32 | 27,678,429,184 | `70bac7eab4179e79fcdfaaec67eb5f15419fef6e8a301679a8c209102f744400` |

All comparisons retain the same 16 identical-prefix full-vocabulary positions,
256-token context, 128-token batch, two CPU threads and max/RMS/KL thresholds
`0.25 / 0.025 / 0.001`. A failed gate remains failed.

| CPU reference / candidate | Worst max / RMS / KL | Violating positions |
| --- | --- | ---: |
| F16 / Q4 Vulkan default | 0.614853 / 0.156832 / 0.00164927 | 11 |
| F16 / Q4 Vulkan, FP16 disabled | 0.614276 / 0.157621 / 0.00164802 | 11 |
| F16 / F16 Vulkan | 0.616159 / 0.159483 / 0.00163448 | 11 |
| F32 / F16 CPU | 0.618118 / 0.159100 / 0.00160965 | 11 |
| F32 / Q4 Vulkan default | 0.170671 / 0.0360701 / 0.000179551 | 1 |
| F32 / Q4 Vulkan, FP16 disabled | 0.00870603 / 0.00260335 / 0.000000613316 | 0 |

All GPU comparisons above retain matching greedy argmax at every position.
That alone is not numerical acceptance. Disabling fusion, Flash Attention,
changing KV storage to F32, and disabling FP16 on the F16 candidate did not
resolve the F16-reference failures. F32 KV storage alone also leaves the one
F32-reference/default-GPU failure.

An observational router trace locates the first F16-reference/GPU expert-set
change at position 5, layer 0. The CPU's eighth/ninth router scores differ by
0.00156257; a maximum CPU/GPU router difference of 0.00270032 swaps experts 15
and 44. Each backend sorts its own scores correctly. The reciprocal CPU F16
versus F32 failure demonstrates that the initial 11-position discrepancy is
not sufficient evidence of a GPU defect. The remaining default-GPU failure
against F32 is still open. No tolerances or shipped precision defaults changed.

#### Streaming and the prefill boundary

`tests/qualify_models.lua` adds the pinned `moe7b` fixture and optional
`--model`, `--tokens`, `--batch` and `--trace` arguments. Defaults remain 16 output
tokens and a 128-token batch; the fixture catalog now includes the 7B model.
Use the installed Lua runner/module paths from `lua-receiver`. For example,
with an existing cache/output directory:

```sh
# Run under an explicit host-memory limit; reserve GPU memory separately.
RUNNER tests/qualify_models.lua LUA_PATH LUA_CPATH CACHE OUTPUT \
  --model moe7b --tokens 64
GGML_VK_DISABLE_F16=1 RUNNER tests/qualify_models.lua LUA_PATH LUA_CPATH CACHE OUTPUT \
  --model moe7b --tokens 64 --batch 8
```

`RUNNER` denotes the selected loader/runtime/installed Lua runner command, not
an additional executable. Acquisition requires explicit `--download`; cached
runs remain offline. Native trace output requires the matching pslog Lua
provider; use `--trace` with `GGML_SCHED_DEBUG=2` for per-operation assignments.

Both CPU and Vulkan stream text with logprobs. GPU placement covers all 16
repeating layers and the output layer. During actual `generate`, scheduler
traces place expert gate/up/down `MUL_MAT_ID` operations on Vulkan; input
embedding lookup remains on CPU. The request has 25 rendered prompt tokens;
the GPU emits 52 tokens, with first text at token 1. Native cards, process status
and DRM counters are saved before close and after reclamation.

Short diagnostic observations on the same request:

| GPU profile | First text s | Inclusive prefill tokens/s | Decode tokens/s | Generation s |
| --- | ---: | ---: | ---: | ---: |
| Shipped arithmetic, batch 128 | 4.44 | 5.63 | 11.33 | 8.95 |
| FP16 disabled, batch 8 | 1.74 | 14.37 | 11.31 | 6.26 |
| Same batch-8 control, with tracing | 1.75 | 14.29 | 11.26 | 6.28 |

These are single short runs, not sustained throughput claims. Full-FP32
reference comparisons above use a different raw prompt; they do not prove all
52 Chat-token probability comparisons or long-context accuracy.

Disabling FP16 with batch 128 **fails** the Chat prefill. GDB identifies the
terminal exception as `vk::DeviceLostError` in Vulkan queue submission, propagated
through decode. Kernel records correlate both failing process IDs with compute-ring
timeouts and successful ring resets. That does not distinguish excessive dispatch
duration from a stuck kernel. The C boundary currently reports only `Inference receiver
raised an exception`. No OOM or test-group swap is recorded. Batch 8 completes
in independent normal/traced runs. It is an explicit qualification workaround,
not an automatically adopted production policy. Reduce/fix the larger-prefill
failure and improve native error reporting before changing defaults or claiming
this model's unrestricted FP32 serving acceptance.

Evidence, exact pinned metadata, memory controls and exploratory sources are
under ignored `build/raphael-16gb/`. Final budget logs are
`logs/combined-strict.*`, `logs/stream-default.*`,
`logs/stream-fp32-batch8.*` and `logs/stream-trace-fp32-batch8.*`.
The earlier host-only-limit probes and failed controls remain separately named.

### Raphael MoE defaults and bounded submissions, 2026-11-04

This follow-up supersedes the unresolved default-precision and batch-128
observations above. The underlying Mesa LDS repair remains required. Raphael
PCI `1002:164e` now selects FP32 arithmetic and floating-point activations by
default, retaining FP16 weight and KV storage. Other PCI IDs retain their
arithmetic selection. This implements the accepted accuracy-first policy.

The 25-token OLMoE Chat prefill was isolated from individual expert kernels.
Both Q4_K/F32 and Q6_K/F32 projection shapes pass the upstream backend-operation
comparison, including batches 8, 9, 25 and 128. Before the repair, either
`GGML_VK_MAX_NODES_PER_SUBMIT=1` or disabling fusion allows the failing full
request to complete. These are diagnostic controls, not adopted defaults.

The Vulkan scheduler had two submission-budget defects: the first graph's
missing work estimate yielded zero, disabling the FLOP limit; the first three
submissions could then double the limit without retaining the device cap.
Patch 0013 uses the existing cap until a previous graph supplies an estimate,
and clamps early growth to that cap. Raphael's existing two-CU cap is 4 GFLOP;
no kernel timeout, fusion policy, batch size or node-count default changes.
The cap limits accumulated estimated work between nodes; it cannot divide a
single expensive kernel. This resolves the reproduced timeout without proving
all larger workloads safe.

With only the submission fix and explicit FP32 arithmetic, three fresh
batch-128 processes complete the previously failing request. Folding the
precision choice into patch 0010 then passes three more fresh installed-Lua
runs with no precision/batching/fusion override: 25 prompt tokens, 52 generated
tokens, first text at token 1, about 6.35–6.48 seconds to first text and
11.06–11.28 decode tokens/s. A separate traced run handles 134 prompt tokens
and streams 64 output tokens, first text at token 1, at about 21.07 inclusive
prefill / 11.21 decode tokens/s. All 16 repeating layers, the output layer and
all traced expert `MUL_MAT_ID` operations are on Vulkan. These timings include
startup pipeline work and are short observations, not sustained benchmarks.

Both default profiles pass the same 16 identical-prefix full-vocabulary
OLMoE positions against the pinned F32 reference above: worst max/RMS/KL
`0.00870603431 / 0.00260335092 / 6.13316045e-7` for GNU and
`0.00876819228 / 0.00260335092 / 5.88252204e-7` for musl, zero differing
argmax and zero violations. The unchanged limits are `0.25 / 0.025 / 0.001`.
The Qwen F16-reference/Q4 and F16-reference/F16 Vulkan pairs also pass, with
worst max/RMS/KL respectively `0.0728120567 / 0.0106818219 / 1.16679257e-5`
and `0.0611722021 / 0.00978808224 / 8.84632828e-6`. Free-running CPU Q4 Chat
outputs are not an F32 oracle; agreement of their text is not the strict gate.

All final model runs retain the 10 GB host limit, zero test-group swap and
6 GB GPU/headroom reservation described above. The cold F32 reference run
reaches the kernel-rounded host cap of 9,999,998,976 bytes with reclaim,
without OOM or swap; it takes about 71 seconds in GNU and 128 seconds in musl.
The larger Chat run records
about 4.29 GB of process-owned resident GTT/VRAM, falling to 2.69 MB after
close. Its model card accounts for 4,153,780,224 model, 33,554,432 context and
30,019,584 compute bytes on Vulkan; host buffers remain separately reported.

The C receiver now copies `std::exception::what()` into its existing bounded
error buffer, returning `AILO_INTERNAL_ERROR`; an empty/unknown exception
retains the generic fallback. Typed failures and allocation failures retain
their status mappings. Callback exceptions are sanitized at the event sink.
The new boundary test proves owned diagnostics after exception destruction,
truncation/NUL termination, guard integrity, status mappings, null error
receivers and stale-error clearing. The existing callback integration test
also proves that private callback text does not escape.

Both native targets pass the remaining regression campaign:

| Gate | GNU | Fully static musl |
| --- | ---: | ---: |
| Native CTest suite, including relocated install consumers | 82/82 | 80/80 |
| Installed Lua suite (GNU host module) | 5/5 | — |
| OLMoE F32-reference positions | 16/16 | 16/16 |
| OLMoE expert projection cases | 8/8 | 8/8 |
| Qwen strict reference pairs | 2/2 | 2/2 |
| Flash Attention/layout/precision/quantized-KV cases | 48/48 | 48/48 |
| Fresh matrix processes, F16/F32 storage with fusion enabled/disabled | 400/400 | 400/400 |
| Trained text/protocol/compaction/Codex/vision/audio serving | 6/6 | 6/6 |

All six musl qualification/serving executables have no ELF interpreter or
`NEEDED` entries. Serving runs retain disabled shader disk caching and poisoned
external ICD paths; musl additionally traces file/process access. Fresh
extraction plus all 13 zero-fuzz patches exactly matches all four prepared
source trees. ARM source reconstruction is not new ARM binary/GPU acceptance.
Repeated normal builds pass the patch-queue check. The updated precision policy
belongs wholly to patch 0010, avoiding a later patch that rewrites its helper.

The unchanged `examples/infer.c`, linked with `-static` against the final
installed musl SDK, also completes the OLMoE request at its default 2,048-token
context and 512-token batch (128-token microbatch). Its output passes the JSON
and expected-content checks, and its ELF has no interpreter or `NEEDED` entry.
This is an installed-consumer check: the ordinary in-tree example links shared
runtime libraries even when it uses the static libailo archive. Configuring a
larger context does not prove accuracy at longer input prefixes.

The complete profile change has a measurable cost. A fixed Qwen Q4 workload
uses the same upstream base, Mesa repair, 512-token context, 128-token microbatch,
256 prefill tokens and 32 teacher-forced decode tokens. Each SDK runs one warmup
and five measured repetitions, with finite full-vocabulary logits throughout:

| SDK profile | Mean warm prefill tokens/s | Mean decode tokens/s |
| --- | ---: | ---: |
| Preserved prior SDK: floating activations, FP16 arithmetic | 299.47 | 14.71 |
| Current SDK: FP32 arithmetic and enforced submission cap | 218.15 | 14.81 |

This is about 27.2% lower prefill throughput with no clear decode penalty.
The comparison changes the complete profile, not just arithmetic, and is not a
thermal/concurrency campaign. Recover throughput without weakening numerical
acceptance or the LDS repair. Longer MoE contexts, more prompts/models,
sustained load and other GPU hardware remain open.

Exact controls, model hashes, per-process cards/DRM counters, cgroup/time
records, source reconstruction and commands are under ignored
`build/raphael-moe-fix/`; the large pinned fixtures remain under
`build/raphael-16gb/`. The historical failed controls above remain evidence.
To replay the added expert cases with the opt-in backend-operation tool:

```sh
params='n_mats=64,n_used=8,b=0,m=(1024,n=(8|9|25|128),k=2048|2048,n=(8|9|25|128),k=1024),'
BUILD/ailo_qualify_backend_ops test -b Vulkan0 -o MUL_MAT_ID -p "$params"
```

Require `8/8 tests passed`; zero selected cases do not qualify the kernels.

### Bounded Raphael optimization pass, 2026-11-05

A short isolated trial compares the current profile with FP16 enabled while
retaining floating activations, the Mesa LDS repair and patch 0013's submission
limits. A second setting forces FP32 matrix accumulation with FP16 enabled.
One private backend build supplies all three settings; experimental controls
exist only under ignored `build/raphael-opt-short/`. The recorded vendor queue,
normal prepared dependency sources and installed SDK are unchanged.

The same pinned Qwen Q4 workload as above runs one warmup and five measured
repetitions per setting, with finite full-vocabulary logits throughout:

| Trial setting | Mean warm prefill tokens/s | Mean decode tokens/s |
| --- | ---: | ---: |
| Current FP32 profile and enforced submission cap | 241.66 | 15.98 |
| FP16 enabled, same enforced cap | 331.22 | 16.07 |
| FP16 enabled, FP32 matrix accumulation, same enforced cap | 336.14 | 16.05 |

The last setting is about 39.1% faster than this pass's FP32 control, with no
clear decode change. These are contemporaneous short observations; compare
rows within this pass rather than absolute rates across different campaigns.
Because the cap remains enforced in every row, it does not prevent recovering
most of the precision-related throughput. Its separate cost was not measured
against an otherwise identical uncapped binary.

The FP32 matrix-accumulation setting passes both existing 16-position Qwen
strict reference pairs, with unchanged max/RMS/KL limits `0.25/0.025/0.001`:

| F16 CPU reference / Vulkan candidate | Worst max / RMS / KL |
| --- | --- |
| Q4 stored weights | 0.0704010414 / 0.0115375858 / 1.17941872e-5 |
| F16 stored weights | 0.0650553487 / 0.00927953292 / 1.27726757e-5 |

Both have zero differing argmax and zero violating positions. That result does
not qualify OLMoE. Its full F32-reference comparison times out after 300 seconds
under the 10 GB host cap with 6 GB reserved for GPU/headroom, without an accuracy
result. A separate CPU-only reference capture, with no GPU model loaded, also
hits its 150-second limit under a 15 GB host cap and 1 GB headroom. The latter
log remains inside the CPU reference phase, records about 940,000 major page
faults and roughly 66 GB of filesystem input, and produces no reusable oracle.
Neither run records OOM or swap use. The reduced-precision candidate's MoE
accuracy is therefore unknown, not a pass or a numerical failure.

No production optimization is adopted from this pass. Retain the qualified FP32
Raphael default. The next precision trial needs a reusable F32 oracle with the
same model/source identity, token prefixes, context and numerical limits, so
that disk paging does not consume the short GPU test budget. Attention already
requests FP32 accumulation in the upstream model graph; changing matrix
accumulation alone does not establish that FP16 operands meet the MoE gate.

Sources, exact commands, timings, strict Qwen results and bounded timeout
records are under `build/raphael-opt-short/`. The trial's reference-cache helper
is an unfinished experiment and is not a shipped test tool or storage format.

### Reusable oracle and extended MoE validation, 2026-11-05

The follow-up completes the CPU oracle capture and rejects the faster candidate
rather than adopting it. The qualified production precision policy, vendor
patches, normal prepared source and installed SDK remain unchanged.

The original F32 reference model, prompt, 256-token context, batch/microbatch
128, two CPU threads and 16 identical-prefix positions are retained. A CPU-only
capture under a 15 GB cgroup cap, zero swap and no GPU model takes 268.35 seconds
and completes without OOM. It saves 3,219,524 bytes: vocabulary size, selected
prefix tokens and 16 full float32 logit arrays. Artifact SHA-256:
`cfeae022b2d679ee421a628a4a9e83ae8bdd1d34b100e5e8b15035853f4b53fb`.
The reference GGUF remains
`70bac7eab4179e79fcdfaaec67eb5f15419fef6e8a301679a8c209102f744400`.
This is the F32 expansion of the pinned Q4 stored weights, not original
training weights.

`build/raphael-oracle/provenance.json` records model identity, prompt/settings,
upstream base and producer executable/CPU/native archive digests. The producer
binary and source are frozen separately. The checksum ledger is checked before
reuse. This is a local x86_64 fixture, not a supported interchange format;
the experimental reader is not a shipped qualification API. Cached GPU runs
retain the 10 GB host cap, zero swap and separate 6 GB GPU/headroom reservation.
They take about 4–10 seconds rather than rerunning the large CPU reference.

The cached corpus reproduces the current profile's earlier strict result
exactly: max/RMS/KL `0.00870603431/0.00260335092/6.13316045e-7`, zero
violations or differing argmax. FP16 with the enforced submission cap, with
or without forced FP32 matrix accumulation, instead fails one position:
`0.170670933/0.0360701333/0.000179550887`. Thus the previously timed-out
candidate now has a measured numerical rejection.

A narrower trial retains FP32 attention while enabling FP16 elsewhere. Both
that setting and one also preserving FP32 matrix accumulation pass the short
CPU oracle with the same worst errors as the current profile. That alone is
insufficient for adoption. A second corpus uses eight repetitions of
`The sky appears blue because sunlight scatters in the atmosphere. ` before
16 compared positions, with the same context/batch/thread settings. This corpus
is generated by the existing full-FP32 Vulkan profile on the Q4 model; it is a
**GPU regression baseline, not another CPU accuracy oracle**. Its checksum,
exact prompt, model identity and producer source are recorded separately.
All comparisons retain max/RMS/KL limits `0.25/0.025/0.001`.

| Longer-prefill candidate versus FP32 GPU baseline | Worst max / RMS / KL | Violating positions |
| --- | --- | ---: |
| Full FP32 control | 0.00221483268 / 0.000581032655 / 3.83896833e-8 | 0 |
| FP32 attention and accumulation; FP16 other operands | 1.47630318 / 0.31324629 / 5.11985394e-5 | 6 |
| Additionally keep expert and non-quantized matrices in FP32 | 1.47715362 / 0.354351177 / 0.000839983943 | 7 |

All retain matching greedy argmax. That does not override the unchanged
numerical gate. The last variant allows FP16 only for ordinary quantized dense
matrices, yet still exceeds the regression bounds. The control's small
variation is far below those failures. These observations do not identify a
unique faulty instruction or prove that other GPUs have the same behavior.

The paired short Qwen benchmark reaches 242.67 prefill / 17.87 decode tokens/s
for the current profile versus 330.31 / 15.96 for the attention-plus-accumulation
trial, over five warm repetitions each. Prefill improves about 36.1%, but this
pair also shows a decode reduction; do not describe the candidate as an
unchanged-decode production speedup. Earlier short timings vary. None of these
rates qualifies a candidate that fails the larger-prefill accuracy gate.

No runtime optimization is adopted from this follow-up. Further precision work
must cover multiple prefixes and real prefill matrix paths before changing
defaults. The CPU oracle, separate GPU regression corpus, frozen producers,
provenance/checksum ledgers, source trials and bounded logs remain under ignored
`build/raphael-oracle/` and `build/raphael-opt-short/`. Their source/format helpers
remain experiments; standard native oracles and released SDK contracts are
unchanged. The source archive contains this report, not these local artifacts.

### FP32 tile and cache follow-up, 2026-11-05

A bounded tile-selection experiment keeps FP32 arithmetic, floating-point
activations, the full LDS reservation and submission limits. The isolated
backend replaces only the ordinary dense matrix selector; expert selection is
unchanged. Instrumentation confirms that the tested Qwen prefill shape
`m=896,n=128` has two available configurations and already selects index 1
(medium). Forcing medium is therefore a control for this shape; forcing index 0
(small) changes the selected kernel. An earlier three-configuration-only trial
did not exercise this two-configuration path and supplies no optimization evidence.

The same pinned Qwen Q4 model, 256-token prefill, 32 teacher-forced decode tokens,
one warmup and five measured repetitions give these mean rates:

| FP32 tile selection | Prefill tokens/s | Decode tokens/s |
| --- | ---: | ---: |
| Current | 242.287 | 15.994 |
| Medium control | 242.025 | 16.172 |
| Small | 112.643 | 15.990 |

Both trials pass the cached 16-position OLMoE CPU oracle with max/RMS/KL
`0.00870603431/0.00260335092/6.13316045e-7`. They also pass the separate
longer-prefill GPU regression corpus: medium
`0.00216390073/0.000627446228/3.42675239e-8`, small zero differences. All retain
zero violating positions and matching argmax. Artifact checksums are validated
before reuse. Each run uses the existing 10 GB host cgroup cap, no swap and
6 GB GPU/headroom reserve; none records OOM. Commands, isolated source, logs and
memory records are in ignored `build/raphael-fp32-tuning/`.

No tile or precision change is adopted: medium supplies no gain and small loses
more than half the prefill throughput. The production cache instead avoids a
disk catalog lookup when RAM already has the maximum safe reusable prefix.
That removes lock acquisition and matching token-metadata decryption from this
lookup case; capture/publication keeps its existing behavior. A regression
holds the disk lock and proves the full RAM hit does not incur a lookup BUSY
skip, while another proves a longer disk checkpoint still beats a partial RAM
match and reproduces the cold output. This is a lookup-cost improvement, not a
claim of faster uncached GPU arithmetic. See [cache behavior](client.md).
