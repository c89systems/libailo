# GPU userspace dependency selection

Investigation date: 2026-10-13. This is a source-based dependency and license
assessment, not a completed static build or hardware qualification.

## Decision and scope

Select headless Mesa **RADV for AMD Radeon** and **ANV for Intel GPUs** as the
GPU implementation paths to qualify. Their additional userspace dependencies
can be kept permissively licensed. Do not adopt the default distribution Mesa
package or its automatically discovered dependencies as our shipped stack.

The maintained Linux ROCm/HIP stack does not pass this criterion: both static
HIP and the HSA thunk require LGPL-licensed libnuma. NVIDIA's official CUDA
userspace driver remains excluded under the README support policy.

This assessment concerns GPU code and libraries linked into the application.
Kernel drivers and host GPU firmware are host requirements. Host shader and
source-generation tools are distinct from shipped runtime libraries.

The compiler executable's license does not affect this selection. The linked
C++ runtime is a different component: the current foundation includes
libstdc++/libgcc under GPL with the GCC Runtime Library Exception. The
[exception](https://gcc.gnu.org/onlinedocs/libstdc++/manual/license.html)
permits proprietary applications to incorporate the runtime through eligible
compilation, including static linking, but is not a purely permissive license.
The project accepts these existing runtime libraries under that exception.
Additional GPU userspace libraries must use permissive licensing. LLVM libc++
is a different C++ library with permissive licensing; it is not the GNU runtime
discussed here, and no runtime substitution is required by this decision.

## Sources inspected

- Mesa **26.2.4**, the latest stable archive available during this investigation:
  [release notes and published checksum](https://docs.mesa3d.org/relnotes/26.2.4.html),
  [source archive](https://archive.mesa3d.org/mesa-26.2.4.tar.xz).
  Downloaded SHA-256 matched
  `bce5f7fbebb934373b86c999a064d52fb5065878dc57f287f95346648ec832e9`.
- libdrm **2.4.134**:
  [source archive](https://dri.freedesktop.org/libdrm/libdrm-2.4.134.tar.xz).
  Downloaded SHA-256 was
  `ac5e74d157830eb8bee44c6a6bf3ad49774ef0dd2a72bdad74a8f20308b52a95`.
  This is a locally recorded digest, not an independently verified release
  signature. Mesa RADV 26.2.4 requires libdrm_amdgpu >= 2.4.133.
- ROCm systems revision
  [`f44204e18ebf8ace9b1534973df0ca9a69c10651`](https://github.com/ROCm/rocm-systems/tree/f44204e18ebf8ace9b1534973df0ca9a69c10651),
  including the static HIP and HSA-thunk dependency declarations.

These are investigation inputs. They do not add dependency pins or artifacts
to the current libailo SDK.

## Permissive Mesa runtime profile

| Component | RADV | ANV | License basis |
| --- | --- | --- | --- |
| Driver, common Vulkan runtime and Mesa utilities | Required | Required | Predominantly MIT; preserve individual file notices |
| NIR and SPIR-V translation | Required | Required | Mesa permissive source notices |
| ACO and AMD address library | Required | Unused | MIT source notices |
| Intel BRW/JAY compiler, device and layout libraries | Unused | Required | MIT source notices |
| libdrm core | Required | Required | MIT |
| libdrm_amdgpu | Required | Unused | MIT |
| BLAKE3 embedded in Mesa | Required | Required | Select Apache-2.0 from upstream dual licensing |
| Expat for XML configuration | Optional | Optional | MIT |
| zlib for shader-cache compression | Optional | Optional | Zlib |
| Zstandard for shader-cache compression | Optional | Optional | Select BSD-3-Clause from upstream dual licensing |
| SPIRV-Tools, if enabled | Optional | Optional | Apache-2.0 |
| Vulkan headers and an application dispatch layer | Required | Required | Existing Khronos permissive licensing; implementation still needs qualification |

Evidence comes from the release's `src/amd/vulkan/meson.build`,
`src/amd/common/meson.build`, `src/amd/compiler/meson.build`,
`src/amd/addrlib`, `src/intel/vulkan/meson.build`,
`src/intel/compiler/{brw,jay}`, `src/intel/{dev,isl,perf,blorp,shaders}`,
`src/compiler/{nir,spirv}`, `src/util` and `src/vulkan/{runtime,util,wsi}`.
The libdrm core and amdgpu source lists and notices were inspected separately.
A copyleft-notice search over these Mesa C/C++/OpenCL/Rust source trees found no
GPL/LGPL notices; this is supporting evidence, not a substitute for checking
the final compiled source list and bundled third-party notices.

Mesa's embedded BLAKE3 README identifies version 1.8.2 and local modifications.
Its upstream Apache license must be included with the shipped notices rather
than treating the entire Mesa archive as uniformly MIT.

Primary license references:
[Mesa](https://docs.mesa3d.org/license.html),
[BLAKE3 1.8.2](https://github.com/BLAKE3-team/BLAKE3/blob/1.8.2/LICENSE_A2),
[Expat](https://github.com/libexpat/libexpat/blob/master/expat/COPYING),
[zlib](https://zlib.net/zlib_license.html),
[Zstandard BSD license](https://github.com/facebook/zstd/blob/dev/LICENSE),
[SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools/blob/main/LICENSE).

## Required configuration boundaries

1. Build only the chosen Vulkan drivers and their internal dependencies. Exclude
   Gallium/OpenGL/OpenCL drivers, display integrations, tools and optional
   profiling integrations from the runtime payload. Headless inference does
   not require an X11 or Wayland client stack.
2. Use ACO for RADV. Keep target LLVM disabled. For ANV, use Mesa's native Intel
   runtime compilers; its internal shaders still require host mesa_clc and
   vtn_bindgen2 build tools. A host LLVM-backed shader tool does not establish
   a target LLVM dependency. Check generated shader notices too.
3. Explicitly prevent unqualified libelf and libudev implementations from
   entering the dependency graph. The initial minimal profile omits both.
   Mesa probes both with `required: false`, independent of a general
   `auto_features=disabled` setting. RADV lists dep_elf, and shared WSI lists
   dep_libudev. The absence of a hard requirement makes their exclusion
   possible; relying on whatever happens to be installed is insufficient.
   Use controlled target package discovery and explicit exclusions in the
   dependency integration, then inspect the resolved link graph. The libelf
   interface itself is not excluded: a permissive implementation may be
   qualified separately to retain the functionality described below.
4. Use only libdrm core plus the amdgpu module when needed. ANV does not require
   the separate libdrm_intel module or its libpciaccess dependency. libdrm's
   compiler-atomic path avoids an external libatomic_ops dependency.
5. Keep optional compression/XML libraries within the table's permissive
   choices. Disable debugging, RenderDoc, Perfetto and sysprof integrations
   unless separately qualified; RenderDoc integration calls dlopen/dlsym.
6. Preserve each applicable license and attribution. Confirm the selected
   source list, generated code and complete static link dependency graph
   before claiming that an artifact has a permissive GPU userspace stack.

## Consequences of omitting libelf and libudev

ELF is also an object-file format for GPU programs. Those objects can contain
GPU instructions, named sections, symbols and address relocations. A userspace
driver may need an ELF parser to extract code and resolve addresses before
uploading it to GPU memory. That operation is distinct from loading a host
shared library through dlopen.

In the inspected Mesa release:

| Omission | Concrete consequence | Relevance to headless inference |
| --- | --- | --- |
| libelf from RADV | LLVM/RTLD ELF shader binaries cannot be processed or uploaded | Use the normal ACO compiler and its non-ELF binary path |
| libelf from RADV | Radeon GPU Profiler capture files cannot be exported | Loses detailed RGP profiling for tuning and debugging |
| libelf from ANV | No direct ANV dependency or corresponding ELF shader path was found | No identified ANV inference feature depends on this library |
| libudev from shared Vulkan WSI | Display-output hotplug listener is omitted | Normal DRM-based GPU enumeration is separate; inference has no display output |

The evidence is explicit in `radv_shader.c`: RTLD handling is gated by
USE_LIBELF, while `radv_aco_build_shader_binary` creates RADV_BINARY_TYPE_LEGACY
binaries with GPU code embedded directly. `src/amd/common/ac_rtld.c` reads
ELF sections and handles AMD GPU relocations. `ac_rgp.c` reports an error when
asked to export an RGP capture without libelf. Shared Vulkan `vk_instance.c`
enumerates devices through drmGetDevices2; libudev calls are guarded in the
display hotplug listener in `wsi_common_display.c`.

This supports an expectation that omitting these libraries preserves ordinary
ACO/ANV inference. It does not prove equal performance or hardware compatibility;
actual shader execution, model output and performance still need validation.
Profiling capability is a real loss, even when inference remains possible.

The API name libelf does not determine its license. The
[ELF Tool Chain implementation used by FreeBSD](https://github.com/freebsd/freebsd-src/blob/main/contrib/elftoolchain/libelf/elf.c)
has BSD-2-Clause source terms, unlike elfutils' copyleft implementation. It is
an available source candidate for restoring ELF-based functionality under our
policy. Its complete Linux/musl port, dependency closure, static build and Mesa
compatibility have not been qualified here; the FreeBSD sources include BSD
platform headers. Do not substitute it and claim compatibility from the common
API name alone.

## Why ROCm is not selected

HIP/CLR is MIT and ROCr/HSA uses the permissive University of Illinois/NCSA
license. The rejection comes from their transitive Linux requirements:

- [Static HIP dependency declaration](https://github.com/ROCm/rocm-systems/blob/f44204e18ebf8ace9b1534973df0ca9a69c10651/projects/clr/hipamd/src/CMakeLists.txt)
  links numa alongside COMGR and HSA.
- [HSA thunk dependency declaration](https://github.com/ROCm/rocm-systems/blob/f44204e18ebf8ace9b1534973df0ca9a69c10651/projects/rocr-runtime/libhsakmt/CMakeLists.txt)
  requires NUMA on Unix; its fallback also requires a library named numa.
- [libnuma](https://github.com/numactl/numactl#license-copyrights-acknowledgements)
  is LGPL-2.1. Static availability does not make that license permissive.
- ROCr also requires libelf. The
  [elfutils implementation](https://sourceware.org/elfutils/) is dual
  GPLv2+/LGPLv3+. An alternative libelf implementation could have a different
  license, but that alone would not remove the required libnuma dependency.

Replacing those dependencies would be a new port, not an existing permissive
ROCm configuration. No such port was implemented or qualified here.
This leaves AMD Radeon/RADV as the selected AMD direction; it does not establish
support for compute-only Instinct hardware through RADV.

## Static deployment remains a separate gate

The reviewed RADV and ANV build definitions explicitly use shared_library for
their final driver. Both therefore need static-driver integration and an
application dispatch path that avoids dynamic ICD loading. A static Khronos
loader archive alone would not remove shared-driver loading.

Qualification must prove a musl executable with no dynamic interpreter or
DT_NEEDED dependencies, no required runtime shared-object loading, and actual
GPU model inference. Multiple linked drivers also need coherent entrypoint
dispatch and symbol ownership. Driver build identification, optional loader
helpers, shader generation, shutdown and error handling require validation.
The existing Intel Vulkan smoke test uses the shared loader/driver path and
does not prove this deployment model. This investigation ran no Mesa build,
host-library link or GPU compatibility test.
