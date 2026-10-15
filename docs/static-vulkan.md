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
See its [driver interface documentation](https://github.com/KhronosGroup/Vulkan-Loader/blob/vulkan-sdk-1.4.363.0/docs/LoaderDriverInterface.md).
The registry and global discovery adaptation are project work, not behavior
provided by an unmodified upstream static archive.

## Dependency and build profile

Use Mesa 26.2.4 and libdrm 2.4.134 as the investigated integration baseline;
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
20.1.18, glslang 16.6.0 and Mesa 26.2.4 `mesa_clc`/`vtn_bindgen2`. These builds
require installed LLVM 20.1.8 development tools, the matching Clang shared runtime,
Meson >= 1.4, Ninja, Mako and PyYAML. Clang 20.1.8 development headers are generated
and installed inside the repository using upstream's `clang-headers` target;
the host compiler installation is not modified. Native sources/builds/installs
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
exact-export checks. The x86-64 glibc release suite passes 71/71 CTests; the
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

The comprehensive hardware campaign remains pending: firmware identification,
AMD, AArch64 GPU and multi-GPU execution, larger dense/MoE workloads and
numerical/performance qualification. The earlier host-driver receiver proof
remains separate from this static-driver evidence. These are development
checks, not a signed or tagged release.
