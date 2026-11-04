# llama.cpp integration and upgrades

The authoritative inputs are the `llama` recipe in
[`cmake/dependencies.json`](../../cmake/dependencies.json), the ordered
[`patches/series`](patches/series), and its patch files. The recipe pins the
upstream source archive by SHA-256. Sources and build outputs live under
`.cache/deps-build/TARGET/llama/`; edit the recorded patches when changing the
integration. A changed recipe replaces that owned extraction/build directory.
Project-owned objects also depend on that extraction contract, including the
selected upstream template sources. Archive timestamps cannot leave objects
compiled against a previous upstream revision in the incremental build.

`scripts/build.sh` applies the series in order. Forward application requires
zero fuzz; line offsets are allowed when the context still matches exactly.
The already-applied check tolerates context altered by later patches in the
series. Fresh reconstruction is the authoritative comparison. A conflicting
patch stops the build. A patch edit on an existing pin may require
reconstructing the extraction before
reapplication; editing the cache alone is not a reproducible change.

The application and upstream boundaries are separate:

- `src/ailo_*`, public C89 headers, Lua and serving code belong to libailo.
- The upstream C API inventory is `docs/upstream-api.json`; the generated native
  facade is checked against it. Upstream headers are private to the integration.
- Template code reuses selected upstream `common`/Jinja sources. Model placement
  and cache identity also use `src/llama-model.h`. These are explicit C++ coupling
  points to inspect during an upgrade.
- The static Vulkan loader, Mesa patches and SDK dependencies have independent
  recipes and patch series. The Raphael LDS workaround belongs to Mesa; a
  llama.cpp upgrade alone does not retire it.

## Upgrade on 2026-11-03

The current pin is
[`v0.8.0`](https://github.com/ggml-org/llama.cpp/releases/tag/v0.8.0), commit
`8642541b255bfb5dc39cd351a178be09013a55d6`, published 2026-10-28, 530 commits
after the previous `b11430` pin. The latest rolling release checked was
[`b12078`](https://github.com/ggml-org/llama.cpp/releases/tag/b12078), marked
prerelease; the integration selects the stable release.

The source archive has SHA-256
`51b1e025d7b12e83fa55736e65bc254acd9d23e49f546be7531368fef82210ee`.

| Patches | v0.8.0 integration |
| --- | --- |
| 0001–0005 and 0008 | Apply unchanged, with zero fuzz |
| 0006: worker logging scope | Rebased onto the new worker implementation |
| 0007: Vulkan physical identity | Rebased; removed the local memory-budget guard superseded by upstream's heap clamp and underflow protection |
| 0009: diagnostics routing | Rebased while retaining upstream's multi-device pinned-memory implementation |
| 0010: Raphael precision policy | Floating activations and, after the 2026-11-04 MoE follow-up, FP32 arithmetic for PCI 1002:164e; FP16 weight/KV storage remains supported |
| 0011: strided KV prefill tests | New test-only cases at batches 255/256/512, both precisions and dense/padded/strided layouts |
| 0012: embedded build identity, 2026-11-04 | Respect the parent-supplied GGML commit instead of probing the enclosing libailo Git repository |
| 0013: bounded submissions, 2026-11-04 | Apply the existing work cap to the first graph and early ramp-up; add eight OLMoE expert projection regression cases |

The regenerated C89 facade covers 1,111 upstream functions and 329 record
fields with zero gaps. Upstream adds 28 functions, including experimental
audio-stream and generation-state APIs. Two existing native signatures change:
`mtmd_gen_audio_process` takes an explicit generation state and
`mtmd_helper_support_video` takes a const context. Native video-init accessors
are replaced with video-config accessors. This is a clean cutover; receiver
features are not expanded by generating bindings. There is no prior released
libailo artifact/tag to derive an ABI increment from, so the unreleased initial
shared-library ABI remains 0.

The stable source includes AMD's `kv_raw_strided` attention scratch-copy path;
there is no duplicate backport. The upstream backend-operation tool is available
as the opt-in `ailo_qualify_backend_ops` target. Both GNU and musl pass 48 selected
flash-attention cases on Raphael: 24 upstream decode/prefill/layout/quantized-KV
cases and 24 added cases spanning the scratch path's 256-token threshold,
dense/padded/strided KV and default/F32 precision. BF16, actual Halo,
RDNA4 and non-AMD GPU behavior remain separate hardware qualification work.

At the upgrade, fresh extraction of the verified archive plus all eleven patches
with zero fuzz exactly reproduced both prepared source trees. Offset backup files are preserved
separately; neither tree contains rejected patches or unrecorded source edits.
GNU passes 81/81 native tests and 5/5 installed Lua tests; musl passes 78/78.
Both pass strict trained logits, 400 fresh matrix processes and all six trained
Vulkan serving cases. Static-musl integer isolation passes 1,500 processes.
Relocated static Vulkan checks pass for both targets; all six musl qualification
and serving executables lack an interpreter and dynamic library dependencies.

Upgrade inputs, source reconstruction and qualification logs:
`build/llama-upgrade/`. See [static Vulkan qualification](../../docs/static-vulkan.md)
for the accepted driver and numerical policy.

## Embedded runtime identity, 2026-11-04

Archive builds previously reported `0.8.0-dev` and the enclosing libailo Git
commit through the native version/commit API. The integration now selects the
declared stable version and the revision already recorded in the public API
inventory. Patch 0012 lets GGML honor the revision supplied by its parent, which
upstream llama.cpp already passes to it. The reported commit identifies the
upstream base; the ordered local patches remain separate declared inputs and
participate in libailo's native-state compatibility identity.

Static/shared native tests and relocated C/C++ CMake/pkg-config consumers assert
the observed version and full upstream revision against those declared inputs.
This requires no new facade API or ABI change, no source copy rewrite and no
runtime dependence on the checkout's Git state.

## Upgrade sequence

1. Select an exact release/commit and archive checksum. Trial the ordered series
   in a fresh scratch extraction. Identify upstreamed fixes, remove superseded
   patches, and rebase the remaining minimal changes with zero fuzz.
2. Update the dependency recipe, patch series and source manifest together.
   Prepare/build through the normal scripts so the selected target owns its
   extraction and generated tools.
3. Inspect public header changes and the private coupling points above. Update
   the revision metadata in `scripts/upstream_inventory.py`, then regenerate
   the inventory with `python3 scripts/upstream_inventory.py --write` and the
   facade with `make generate-native`. Require `make facade-coverage` to pass;
   resolve unsupported signatures rather than excluding new public C APIs.
4. Assess changes to libailo's public C ABI against the latest released artifact.
   Make an ABI bump when required by an actual incompatible public change.
5. Run standard native/Lua, export, source/package/install and target checks.
   Re-run exact arithmetic, trained reference logits and text/media/Codex
   serving on Raphael in GNU and static musl. Verify private driver closure and
   final ELF/trace evidence. Qualify other hardware separately.

Keep the rebase distinct from adopting new receiver features. Each new feature
still needs its product specification and observable acceptance tests.

## MoE precision and submission follow-up, 2026-11-04

Patch 0010 now also selects FP32 arithmetic on Raphael PCI 1002:164e, retaining
FP16 storage. Patch 0013 makes the scheduler respect its existing work cap on
the first graph and during early submission growth. Eight expert projection
cases cover the OLMoE vector/matrix boundary. Default batch-128 serving and
strict F32-reference logits pass without environment workarounds; the measured
Qwen prefill cost is about 27% for the complete profile change.

All 13 zero-fuzz patches reproduce all four prepared source trees exactly;
repeated normal builds also pass. GNU passes 82 native tests and five Lua tests;
musl passes 80 native tests. Both pass 400 fresh matrix processes, 48 attention
cases and six trained serving cases. See the [qualification record](../../docs/static-vulkan.md#raphael-moe-defaults-and-bounded-submissions-2026-11-04)
for the unchanged error bounds, resource envelope and remaining coverage.
