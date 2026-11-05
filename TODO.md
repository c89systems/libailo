# Forward work

Updated 2026-11-05. This is the implementation backlog, with observable completion
conditions. GPU qualification is still open. Fix correctness before making
performance or hardware-support claims. Run builds and tests serially.

## P0: Preserve the Raphael repair and qualify numerical precision

- [x] **Recheck Raphael on built-in Mesa 26.2.4 and current Mesa 26.2.6.**
  The workstation has `mesa-vulkan-drivers 26.2.6~kisak1~s`, but our executable
  privately links Mesa 26.2.4. Updating the workstation driver does not upgrade
  that embedded driver. The recheck uses the same validated, padded FP32 SPIR-V
  and two-wave standalone reproducer. A dynamic host-driver control is diagnostic
  evidence, not static-product acceptance.
  Rechecked on 2026-11-02: built-in 26.2.4 passed 402/500 and failed 98/500;
  dynamic host 26.2.6 passed 405/500 and failed 95/500. No setup errors or skips.
  Both ran the same padded two-wave shader, SHA-256
  `9f875beba3c839333f3bcfdddf6c5d444e0c61f73a103f286146002bbb68d2f1`,
  with FP32 arithmetic and memory type 3. This comparison changes the userspace
  driver stack and uses separately compiled probe binaries; it is not a driver
  commit bisect. The host update does not close the failure. Logs and commands:
  `build/gpu-recheck-20261102/`.
- [x] **Upgrade and qualify the embedded Mesa stable branch.**
  Upgraded to 26.2.6, preserving ACO, built-in RADV/ANV and the full Raphael
  repair. Both native targets pass strict logits, 400 matrix processes, 48
  attention cases, all six trained serving cases and relocated static closure.
  Musl passes 1,500 shared-memory processes. Release candidates are excluded.
  The upstream ACO branch fix does not establish the root cause of our LDS
  failure. See the [release audit](docs/dependency-versions.md).
- [x] **Repair the reproduced Raphael shared-memory corruption.**
  The private Mesa patch reserves the full LDS region for compute workgroups
  using shared memory and retains explicit LDS barrier waits, including within
  a single wave. Both measures are necessary. The family/cache-key scope is
  `CHIP_RAPHAEL_MENDOCINO`; only Raphael hardware has been tested. The maintained
  integer-only qualification passes 1,500 fresh processes / 12,000 dispatches;
  the original FP32 tiled shader passes 1,500/1,500, and four GGML configurations
  pass 100/100 each. CPU/GPU F16-model reference logits and all six trained
  Vulkan serving tests pass with FP16 enabled. No kernel setting, external ICD,
  CPU fallback or ACO debug override is required. Details and limitations:
  [Raphael repair](docs/static-vulkan.md#raphael-shared-memory-repair-2026-11-02).
- [x] **Resolve Raphael's quantized-model precision policy.**
  Before this policy, Qwen's integer-dot path quantized activations and exceeded
  the existing
  strict F16-reference tolerances (worst max/RMS/KL 0.469540/0.104982/0.00336607).
  Disabling integer dot for a diagnostic control passes unchanged tolerances
  (0.0722278/0.0115874/0.0000122658) while retaining FP16. The F16 CPU/GPU pair
  also passes. Do not confuse this finite arithmetic-path difference with the
  repaired intermittent corruption, or claim default Q4 probability equivalence.
  Accepted on 2026-11-03: select floating-point activation kernels for PCI
  `1002:164e`, retaining FP16. Both GNU and fully static musl now pass the same
  strict trained-logit tolerances without an environment override. Other device
  IDs retain their existing selection. Broader model/context coverage remains.
  The 2026-11-04 MoE follow-up below extends this accuracy policy to FP32
  arithmetic on the same PCI ID; FP16 weight and KV storage remain supported.
- [x] **Recheck whether upstream can replace the Raphael protections.**
  Isolated v0.8.0 without the activation override fails all 16 strict-logit
  positions (max/RMS/KL 0.476407/0.105655/0.00300542); disabling integer dot in
  that same binary restores the exact passing control. Both serve all six
  trained cases, demonstrating why serving success alone is insufficient.
  Separately rebuilt Mesa 26.2.6 without the LDS repair corrupts 499/500
  integer-only probe processes; the repaired control passes 500/500. Neither
  protection can be retired. See [the controlled ablations](docs/static-vulkan.md#upstream-raphael-ablations-2026-11-04).
- [x] **Measure the LDS workaround and check a smaller reservation.**
  On the fixed Qwen workload, the original unpatched control reaches about
  652 prefill / 29 decode tokens/s; the safe profile with floating activations
  reaches about 332 / 16.4. The unpatched driver is known to be incorrect.
  A 32 KiB minimum plus explicit waits passes 1,500 integer-probe processes but
  fails trained Codex compaction with non-finite logits. Restoring 64 KiB passes
  that same case. Keep the full reservation. Measurements and limits:
  [2026-11-03 validation](docs/static-vulkan.md#raphael-precision-and-static-musl-validation-2026-11-03).
- [x] **Validate the safe Raphael profile in fully static musl.**
  Five qualification/serving executables have no interpreter or shared-library
  dependencies. The hardware campaign passes 1,500 shared-memory processes,
  400 matrix processes, strict trained logits and all six serving cases with
  file/process traces and poisoned external ICD settings.
- [ ] **Upstream and narrow the scoped LDS workaround.**
  Prepare a minimal integer reproducer and ablation results for Mesa, without
  claiming a proven silicon/kernel root cause. Measure the occupancy and barrier
  cost across further workloads, compare kernel/firmware under controlled
  conditions, and retire or narrow the local workaround only after the same
  repeated gates pass. Test actual Mendocino before extending hardware claims.
- [x] **Run a real 7B MoE within a 16 GB qualification budget.**
  OLMoE-1B-7B-0125-Instruct Q4_K_M (4.21 GB, 6.919B total parameters) runs on
  Raphael with all 16 layers/output and expert operations on Vulkan. The final
  campaign caps host memory at 10 GB, reserves 6 GB for GPU allocations/headroom,
  and disables test-group swap. The F32 reference is a 27.68 GB memory-mapped
  file; the cold reference comparison respects the host limit without OOM.
  Explicit FP32 arithmetic passes all 16 strict reference positions. Normal
  serving and FP32 serving with batch 8 both stream 52 tokens; process-owned
  GPU allocations fall from about 4.25 GB to 2.68 MB after close. See the
  [qualification record](docs/static-vulkan.md#raphael-7b-moe-and-the-16-gb-budget-2026-11-04).
- [x] **Resolve MoE precision and the reproduced FP32 prefill timeout.**
  Raphael now prefers FP32 arithmetic with floating activations, retaining F16
  storage. The existing submission work cap was bypassed on the first graph
  and exceeded during early ramp-up; patch 0013 enforces it in both places.
  With no precision, batching or fusion override, all 16 OLMoE F32-reference
  positions pass unchanged tolerances. Three fresh batch-128 Chat runs stream
  52 tokens; a 134-token prompt streams 64 tokens with expert operations on
  Vulkan. Native exceptions now retain their bounded diagnostic at the C
  boundary while callback exception text remains private. GNU passes 82 native
  tests, installed Lua 5, musl 80; both pass 400 fresh matrix processes,
  48 attention cases and six trained serving cases. See the
  [follow-up evidence](docs/static-vulkan.md#raphael-moe-defaults-and-bounded-submissions-2026-11-04).
- [ ] **Expand MoE accuracy/context coverage and recover safe prefill throughput.**
  The complete new profile costs about 27% warm Qwen prefill throughput in a
  short before/after comparison; decode is approximately unchanged. Preserve
  the strict F32 oracle, full LDS reservation and bounded submissions while
  investigating narrower arithmetic choices. Cover further MoE prompts,
  longer contexts, batch sizes, concurrency and sustained load. The work cap
  operates between nodes and cannot divide a single expensive kernel.
  CPU F16 reference weights also round activations and fail 11 OLMoE positions
  against F32; do not mistake that older reference discrepancy for GPU corruption.
  The bounded 2026-11-05 pass recovers about 39% Qwen prefill throughput with
  FP16 enabled and FP32 matrix accumulation while retaining submission limits;
  both Qwen strict pairs pass. The subsequent cached CPU oracle rejects that
  candidate at one MoE position. FP32 attention restores the short gate, but
  longer-prefill regression checks still fail six positions; retaining FP32
  expert and non-quantized matrices fails seven. Keep the qualified default;
  do not change precision from timing or a single short prompt alone. See the
  [short optimization record](docs/static-vulkan.md#bounded-raphael-optimization-pass-2026-11-05).
- [x] **Capture and validate a reusable F32 MoE oracle.**
  The completed 16-position CPU capture occupies 3.2 MB, with source/model,
  producer and artifact checksums plus the exact inference settings. Reuse
  reproduces the current profile's prior strict result exactly and reduces GPU
  comparisons to roughly 4–10 seconds. A separate larger-prefill GPU baseline
  catches regressions that the short CPU corpus misses; it is not a CPU oracle.
  Preserve provenance checks before loading either local artifact. A maintained
  capture/load utility remains separate work; the ad hoc helper is not shipped.
  See the [oracle follow-up](docs/static-vulkan.md#reusable-oracle-and-extended-moe-validation-2026-11-05).
- [x] **Remove redundant persistent-cache lookup for full RAM hits.**
  A maximum safe RAM prefix skips the disk catalog; a partial match still
  searches for a longer disk checkpoint. Regression tests prove both choices
  and identical generated output. FP32 tile trials supply no GPU speedup:
  medium is already selected for the tested two-configuration path, while
  small reduces prefill from 242 to 113 tokens/s. All retained accuracy gates
  pass; production precision, tiles and safeguards are unchanged. See the
  [follow-up evidence](docs/static-vulkan.md#fp32-tile-and-cache-follow-up-2026-11-05).

Existing evidence and reproduction commands are in
[static-vulkan.md](docs/static-vulkan.md). Local investigation inputs and the
Halo assessment are under ignored `stash/`; they must remain uncommitted.

## P0: Fix and qualify Halo attention

- [x] **Adopt the upstream AMD strided KV scratch-copy path.**
  llama.cpp `v0.8.0` contains `kv_raw_strided`, gated to AMD except RDNA4.
  No additional backport of
  [PR #27703](https://github.com/ggml-org/llama.cpp/pull/27703) is needed.
  The opt-in upstream backend-operation tool passes 48 flash-attention cases
  on Raphael in both GNU and musl: contiguous/strided F16 KV, decode/prefill,
  default/F32 precision and Q8_0/Q4_0 KV, plus added dense/padded/strided cases
  at batches 255/256/512 to enter or bypass the new scratch path. Actual Halo,
  BF16 on capable hardware,
  RDNA4/non-AMD selection and long-context stress remain hardware acceptance
  work below. The
  [gfx1151 report](https://github.com/ggml-org/llama.cpp/issues/30364)
  links this path to long-context performance collapse/timeouts; it does not
  establish the cause of Raphael's wrong FP32 matrix results.
- [ ] **Run an exact static-musl acceptance campaign on actual Halo hardware.**
  First record the exact SKU/GPU revision, BIOS/AGESA, firmware, kernel, memory
  and power configuration. No Halo hardware has been tested here. Compare the
  attention fix before/after on the same hardware/model hashes and workload.
  Require repeated arithmetic and trained-logit checks, actual GPU layer/work
  placement, text/media serving, tools, continuation, compaction, cancellation,
  independent clients and teardown. Successful enumeration or CPU fallback is
  insufficient. Keep Halo and Raphael acceptance separate.

## P1: Finish collection-backed conversation persistence

- [x] **Settle compression-on-open policy for the SDK upgrade.**
  0.26 converts an existing root when explicit compression differs; 0.23
  rejected it. Accepted the SDK's conversion on open. The integration test
  proves original payload/key preservation and new-record reopen; startup/space
  effects are documented in `docs/persistence.md`. The subsequent 0.27 upgrade
  limits temporary payload duplication to one segment or snapshot plus recovery
  metadata and indexes. Only disposable test roots have been used.
- [ ] **Resolve the push writer's staging contract before provider adoption.**
  liblockdc 0.27.0 supplies push begin/write/commit/abort and atomic parent
  renewal, but its writer stages the encoded entry in up to 64 KiB of memory,
  spills larger encoded entries to an anonymous file, and copies at commit.
  This is staged/spooled append, not our required end-to-end streaming. Reads
  deliver bounded chunks directly. Obtain a suitable upstream direct streaming
  contract; the 2026-11-04 product decision explicitly retains strict streaming.
  Reuse ordinary append's pending record/encoder with public push control;
  same-collection writer serialization is acceptable. Do not hide the spool,
  add a producer thread, or implement a private dependency bypass. Atomic
  parent-retention renewal already exists; no batch/XA API is requested.
  [Released writer contract](https://github.com/c89systems/liblockdc/blob/v0.27.0/docs/collection-writer.md).
- [x] **Qualify the changed collection SDK surface before adoption.**
  The 16-test four-Linux foundation matrix covers encrypted/zlib push appends,
  lease takeover/fencing, private admission before bytes, release versus close,
  atomic replacement/recovery metadata, stale heads, retry receipts and reopen
  through static/shared consumers. QEMU explicitly invokes the pinned loader
  rather than redirecting secure path traversal into the compiler sysroot.
- [ ] **Replace conversation payload chunks with collection entries.**
  Use one collection incarnation per conversation, exact immutable ID/position
  references, and the existing libailo commit marker for one conversation plus
  optional response and associated item changes. Every read/list/lookup must
  select committed references before emitting bytes. No new batch/XA mechanism
  is required. Keep attachments and inference/KV state separate. Clean up
  physically appended but unpublished entries. Define efficient range reads
  for sparse committed positions; a completed-entry callback arrives too late
  to decide whether to expose that entry's bytes. Cut over cleanly and remove
  the replaced conversation storage path.
  Preserve stable repository snapshots across repeated read calls, edits and
  native collection expiry: an SDK read invocation pins a generation, whereas
  a cursor/open receiver alone does not retain that view. Validate the least
  invasive lifetime design before requesting additional upstream APIs.
- [x] **Enforce logical 90-day conversation inactivity independently of collections.**
  The current Pouch provider publishes expiry with its existing commit marker.
  Creation and newly committed nonempty appends set a 7,776,000-second deadline;
  reads, metadata edits, item deletions, empty appends, aborts and failed commits
  do not renew it. Controlled-clock tests cover the exact boundary, interrupted
  publication/reopen, independent response expiry, pinned readers, parent/item
  cleanup and explicit-deletion archives. Public adapter tests accept finite
  provider lifetimes and reject expired conversations before inference.
- [ ] **Carry the logical deadline into collection retention at cutover.**
  Configure the parent collection, not individual entry age limits. Reconcile
  native append renewal with later libailo marker failure; physical publication
  alone must not renew logical visibility. Preserve pinned snapshots and the
  separate lifetime of explicit-deletion item archives.
- [ ] **Prove collection provider behavior and cost.**
  Test large fragmented items, first-byte delivery/backpressure, bounded memory,
  abort/error/cancellation, retries, commit-marker failure/restart, concurrent
  conversations, item edits/deletes, pagination, compression/encryption and
  retention. Measure append/read/compaction cost against the existing provider
  and a raw JSONL baseline with equivalent durability identified. Count file
  opens, I/O, memory and entries rather than imposing host-sensitive unit-test
  timing. Adopt collection-local atomic edits only if a demonstrated need
  requires them; their presence does not replace our cross-record marker.

## P1: Refresh dependencies deliberately

All direct dependency releases were rechecked 2026-11-03; exact release sources
and the upgrade record are in [dependency versions](docs/dependency-versions.md).
Updating a pin does not establish that upstream fixes Raphael. Keep SDK compression/XML dependencies private;
never substitute workstation libraries for cpkt/cpktmisc.

| Dependency | Current pin after this upgrade | Candidate / action |
| --- | --- | --- |
| liblockdc | 0.27.0, ABI 9 | Latest release; resumable conversion with one segment/snapshot duplicated at a time. Collection provider adoption remains separate. |
| liblql | 0.4.1, ABI 0 | Latest released SDK; no adjacent-source build. |
| Mesa | 26.2.6 | Latest stable; retains the Raphael repair. 26.3 release candidates are excluded. |
| llama.cpp | v0.8.0 | Stable release with 13 ordered patches, including the Raphael precision policy, bounded submissions and regression cases. Includes the upstream AMD KV fix. See the [integration record](vendor/llama/README.md). Rolling b12078 is prerelease. |
| cpkt | 0.6.1 | Latest release; includes the libssh2 AES-GCM handshake leak repair. Static users must relink. |
| cpktmisc | 0.3.0 | Latest release; requires cpkt 0.6.1 and removes LZO. |
| SPIRV-LLVM-Translator | 23.1.3 | Latest release; retains LLVM/Clang 23 compatibility. |
| SPIRV-Tools / glslang | 2026.4 / 16.6.0 | Match the published [Tools](https://github.com/KhronosGroup/SPIRV-Tools/releases/tag/v2026.4) / [glslang](https://github.com/KhronosGroup/glslang/releases/tag/16.6.0) releases checked; no upgrade justified by age alone. |
| lonejson / libpslog | 0.45.0 / 0.13.0 | Latest stable releases, including the matching 0.13.0 Lua logging provider. Canonical owner is c89systems. |
| libdrm | 2.4.134 | Matches the latest [archive](https://dri.freedesktop.org/libdrm/) checked; retain unless a relevant fix is identified. |
| Vulkan headers/loader | 1.4.367 | Latest published stable version tags; static-provider patch rebased to the new platform implementation. |
| SPIR-V headers | SDK 1.4.363.0 | Latest SDK tag; the separate Tools header input follows the exact revision required by its released DEPS. |

- [x] **Upgrade cpkt/cpktmisc as a coordinated foundation change.**
  Adopt 0.6.1/0.3.0, released c89systems storage/logging SDKs and current stable
  GPU components. Update all five target asset pins and package requirements.
  The four Linux SDK configurations pass 16 static/shared libffi and collection
  tests; native application, Lua, encrypted persistence, host generators and
  relocated consumers pass. See the [release audit](docs/dependency-versions.md).
- [x] **Upgrade llama.cpp to stable v0.8.0.**
  Pin commit `8642541b255bfb5dc39cd351a178be09013a55d6` and its verified archive.
  Rebase the ordered patches, regenerate the C89 facade and reconcile the native
  API changes with no legacy shims. All 1,111 functions and 329 fields are covered.
  Project-owned objects now depend on the extraction contract, preventing stale
  template objects when new archive members carry older timestamps. Follow
  [the upgrade sequence](vendor/llama/README.md) for subsequent refreshes.
  GNU passes 81 native and five installed Lua tests; musl passes 78 native tests.
  Both pass strict logits, 400 matrix processes, 48 attention cases and all six
  trained Vulkan serving cases. Static closure and source reconstruction pass.

## P1: Qualify memory limits and shipped artifacts

- [ ] **Measure usable UMA capacity and individual allocation constraints.**
  On each Halo SKU record heaps/types/budgets, GTT/TTM limits, firmware reservation
  and display/OS pressure. Exercise dense and MoE weights plus KV and peak scratch
  together. Our Mesa pin advertises an approximately 4 GiB per-allocation/buffer
  limit; total RAM is not a promise that each tensor or scratch allocation fits.
  Prove advertised placement, actionable allocation failures and reclamation.
  Change heap policy only with evidence in the static profile; runtime XML,
  forced-limit overrides and universal kernel tuning are not established fixes.
- [x] **Re-prove all four complete SDK artifacts and relocated consumers.**
  The 2026-11-04 development matrix builds/packages all four SDKs and passes
  32 relocated C/C++ CMake/pkg-config static/shared configurations, each running
  both API and inference/state consumers. GNU passes 81/81, musl 79/79 and native
  installed Lua 5/5. Static musl consumers have no ELF interpreter or shared
  dependencies. ARM CPU execution uses QEMU and the pinned loader without
  sysroot path redirection. Runtime version/revision now identify the pinned
  upstream base, independent of libailo's enclosing Git identity; the preserved
  old SDK fails the new assertion. All twelve patches reconstruct all four
  prepared source trees exactly. See [the evidence](docs/dependency-versions.md#follow-up-qualification-2026-11-04).
- [ ] **Complete final deployment qualification on actual GPU hardware.** On actual GPU
  hosts inspect final musl ELF interpreter/dynamic entries, exact shared exports,
  archive order, notice completeness and private paths. Trace initialization,
  compilation, inference, failure and teardown with poisoned ICD/layer settings:
  no shared drivers, discovery manifests or runtime compiler process. QEMU can
  verify ARM SDK execution, not AArch64 GPU behavior.

## P2: Performance and remaining hardware coverage

- [ ] After correctness, measure cold pipeline/startup time, prefill/decode,
  context/batch/concurrency, scratch/RSS/GPU memory and sustained thermal/power
  behavior with pinned model hashes. Keep workload-specific results reproducible.
- [ ] Close separately advertised AMD/AMD, Intel/Intel, mixed AMD/Intel and
  AArch64 GPU gates with real hardware. Do not infer these from single-APU proof.
- [ ] Complete the documented release/tag/source-reconstruction acceptance
  campaign before the first release commitment. Historical Intel/static proof
  and today's native regression run do not constitute a new release gate.
