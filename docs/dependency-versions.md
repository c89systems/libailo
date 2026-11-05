# Released dependency versions

Audit on 2026-11-03. `cmake/dependencies.json` records the authoritative URLs and
SHA-256 values. Select stable releases and published version tags; exclude
prereleases and development branches. SDKs come from release assets, including
liblockdc; no adjacent dependency repository is built here.

| Component | Previous pin | Selected release / evidence |
| --- | --- | --- |
| cpkt | 0.5.2 | [0.6.1](https://github.com/c89systems/cpkt/releases/tag/v0.6.1) |
| cpktmisc | 0.2.1 | [0.3.0](https://github.com/c89systems/cpktmisc/releases/tag/v0.3.0) |
| liblockdc | 0.26.0 | [0.27.0](https://github.com/c89systems/liblockdc/releases/tag/v0.27.0), ABI 9 |
| liblql | 0.4.0 | [0.4.1](https://github.com/c89systems/liblql/releases/tag/v0.4.1), ABI 0 |
| libpslog and Lua provider | 0.12.0 | [0.13.0](https://github.com/c89systems/libpslog/releases/tag/v0.13.0) |
| Mesa | 26.2.4 | [26.2.6](https://docs.mesa3d.org/relnotes/26.2.6.html); 26.3 is still a release candidate |
| Vulkan headers | SDK 1.4.363.0 | [1.4.367](https://github.com/KhronosGroup/Vulkan-Headers/tree/v1.4.367) |
| Vulkan loader | SDK 1.4.363.0 | [1.4.367](https://github.com/KhronosGroup/Vulkan-Loader/tree/v1.4.367) |
| SPIRV-LLVM-Translator | 23.1.0 | [23.1.3](https://github.com/KhronosGroup/SPIRV-LLVM-Translator/releases/tag/v23.1.3) |
| llama.cpp | v0.8.0 | Already current: [stable v0.8.0](https://github.com/ggml-org/llama.cpp/releases/tag/v0.8.0) |
| lonejson | 0.45.0 | Already current: [0.45.0](https://github.com/c89systems/lonejson/releases/tag/v0.45.0) |
| SPIRV-Tools | 2026.4 | Already current: [2026.4](https://github.com/KhronosGroup/SPIRV-Tools/releases/tag/v2026.4) |
| SPIR-V headers | SDK 1.4.363.0 | Latest SDK tag; Tools uses its own exact [released DEPS revision](https://github.com/KhronosGroup/SPIRV-Tools/blob/v2026.4/DEPS), `496543121ce6419f23d6fa5d7194ba66c36212d2` |
| Translator SPIR-V headers | Shared the Tools input | Exact revision `cb42dec3830d3ac67fa449ecdc0c0f73d5e74498` required by [Translator 23.1.3](https://github.com/KhronosGroup/SPIRV-LLVM-Translator/blob/v23.1.3/spirv-headers-tag.conf) |
| glslang | 16.6.0 | Already current: [16.6.0](https://github.com/KhronosGroup/glslang/releases/tag/16.6.0) |
| libdrm | 2.4.134 | Already current: [published archives](https://dri.freedesktop.org/libdrm/) |
| Linux compiler collections | stable 2026.08-1 | Already current: Bootlin [x86-64](https://toolchains.bootlin.com/downloads/releases/toolchains/x86-64/tarballs/) and [AArch64](https://toolchains.bootlin.com/downloads/releases/toolchains/aarch64/tarballs/) stable collections |

The SDK-owned transitive libraries, including Lua, curl, OpenSSL, zlib, XML,
libffi and Zstandard, follow the selected SDK releases. They are not replaced
individually with workstation libraries or privately rebuilt copies. Meson,
glslc and matching LLVM/Clang 23 development tools remain workstation
prerequisites; this refresh does not modify the workstation installation.

## Integration changes

All active source and release references for liblockdc, liblql, lonejson and
libpslog use `c89systems`. Public package requirements and the LuaRocks provider
version match the new pins. cpktmisc 0.3 removes LZO; libailo does not use it.
The core update includes the libssh2 AES-GCM handshake leak fix, which requires
static consumers to relink.

Mesa retains the Raphael full-LDS and explicit-wait repair; its cache-key hunk
now preserves upstream's new `force_exact_glsl_fma` field. The Vulkan static
provider patch follows the upstream move of library-loading implementations
from the header into `vk_loader_platform.c`, including the new instance/OOM
arguments. External ICD discovery, shared-driver loading and standard-stream
diagnostics remain disabled in the embedded profile. Forward patch application
requires zero fuzz for Mesa, Vulkan loader and libdrm.

Project-owned objects depend on all extracted SDK contracts as well as the
llama.cpp contract. An archive upgrade with older member timestamps therefore
rebuilds consumers. The regression checks two independent contracts.

The translator and Tools require different SPIR-V header revisions. They now
have separate source inputs, each taken from its released component's dependency
declaration. Pointing Translator 23.1.3 at the older Tools headers fails to
compile its subgroup-bitcast instruction support.

liblockdc's compression conversion now duplicates one segment or snapshot at a
time, rather than the retained root, and resumes interrupted work before open
returns. See [persistence](persistence.md). Collection push writers still stage
encoded entries; this release does not resolve the streaming-provider adoption
gate. Logical conversation expiry is implemented separately in libailo's
current chunk-backed provider; see [persistence](persistence.md).

## Verification

Qualification logs and verified input archives are under
`build/dependency-refresh/`.

| Gate | Result |
| --- | --- |
| Four Linux SDK configurations | 16/16 static/shared libffi and encrypted/zlib collection tests; AArch64 through QEMU |
| GNU offline application / installed Lua | 81/81 and 5/5 |
| Musl offline application | 78/78 |
| Strict Qwen F16-reference logits | Both pass unchanged tolerances; max/RMS/KL `0.0704010414 / 0.0115375858 / 0.0000117941872`, zero argmax differences |
| Matrix / attention per native target | 400 fresh matrix processes across four storage/precision configurations; 48 attention cases |
| Trained Vulkan serving | Six cases per native target: text, compaction, transport, Codex, vision and audio |
| Static-musl shared-memory isolation | 1,500 fresh processes / 12,000 dispatches |
| Relocated static RADV/ANV and Vulkan loader | Both native targets pass, including poisoned discovery paths and the native offset-generator ELF check |
| Musl qualifier/serving ELF files | All six lack `PT_INTERP` and `DT_NEEDED` |
| Mesa/Vulkan source reconstruction | Fresh archives plus zero-fuzz patches match both prepared targets; build adapters match, with generated identities and Python bytecode accounted for separately |
| Host shader generators | GLSL validation, LLVM translation/reverse translation, Mesa OpenCL compilation/binding generation and invalid-input checks pass; compression/XML providers remain SDK-owned |

Musl trained serving uses file/process traces and poisoned external ICD paths,
rejecting shared-object loading and runtime helper execution. Application storage
checks preserve payloads and the encryption key through conversion/reopen and
verify completed conversion leaves no recovery intent/work directory. The build
regression proves two independent extraction contracts invalidate stale objects.

GPU evidence is limited to Raphael. QEMU SDK execution does not qualify an
AArch64 GPU, Halo, Intel or other hardware. Removing Raphael workarounds remains
a separate experiment; this refresh retains them.

## Follow-up qualification, 2026-11-04

The complete development matrix now builds, packages and verifies all four Linux
SDKs after the refresh. This is an untagged development campaign, not a release
commitment or new hardware acceptance. Evidence is under
`build/raphael-revalidation/`, principally `logs/linux-release-matrix-final.log`.

| Gate | Result |
| --- | --- |
| Full GNU application / installed Lua | 81/81 and 5/5 |
| Full native musl application | 79/79, including the newly enabled install-tree gate |
| Relocated C/C++ consumers | 32 configurations across four targets: CMake/pkg-config, static/shared and C/C++; both API and inference/state executables run in each |
| Static musl deployment consumers | All 16 executable instances across the two architectures lack `PT_INTERP` and `DT_NEEDED` |
| SDK archives | Four checksummed, extracted/stripped payloads pass target architecture, header/privacy, exact-export and private-symbol negative-link checks |
| Source/Lua artifacts | Exact source manifest, inner/outer checksums, version/privacy and separate Lua product verification pass |
| Native source reconstruction | Verified v0.8.0 archive plus all twelve zero-fuzz patches exactly matches all four prepared trees; patch backups and Python bytecode excluded |
| Collection SDK contract | All 16 foundation tests pass; both ARM suites also pass twice consecutively to prove cleanup/reopen after the QEMU runner repair |

Archive builds had incorrectly reported `0.8.0-dev` and the enclosing libailo
Git commit. The root integration now supplies the declared release and the
upstream revision recorded in the API inventory. One-line vendor patch 0012
makes GGML respect that supplied revision. Runtime assertions in native and
installed consumers check `0.8.0` and the full
`8642541b255bfb5dc39cd351a178be09013a55d6` revision. The preserved pre-fix SDK
fails the new assertion with the expected identity diagnostic. The local patch
queue remains a separate declared input; a base commit does not imply pristine
upstream source. No public API or ABI changes are required.

The current stable releases still need both Raphael protections. Controlled
ablation results are in [static Vulkan qualification](static-vulkan.md#upstream-raphael-ablations-2026-11-04).
The collection tests qualify leases, atomic edits and read admission, but do not
adopt staged conversation writes. The production decision retains strict
streaming; [persistence](persistence.md#collection-adoption) records the remaining
minimal upstream writer requirement and snapshot/expiry integration checks.
