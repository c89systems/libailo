# Forward work

Updated 2026-11-03. This is the implementation backlog, with observable completion
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
- [ ] **Upgrade and qualify the embedded Mesa stable branch.**
  Assess 26.2.6 before 26.3 release candidates. Rebase the static headless patches,
  preserve ACO and built-in RADV/ANV, and recheck the archive closure and embedded
  identities. 26.2.6 includes an ACO branch-optimization correctness fix affecting
  RDNA2–RDNA4; this is a candidate, not an identified cause of our failure.
  Inspect its applicability to the failing compiled shader. Source:
  [Mesa 26.2.6 release notes](https://docs.mesa3d.org/relnotes/26.2.6.html).
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
  effects are documented in `docs/persistence.md`. Disk headroom covers the
  retained source plus the replacement root, not only one segment. Only
  disposable test roots have been used.
- [ ] **Resolve the push writer's staging contract before provider adoption.**
  liblockdc 0.26.0 supplies push begin/write/commit/abort and atomic parent
  renewal, but its writer stages the encoded entry in up to 64 KiB of memory,
  spills larger encoded entries to an anonymous file, and copies at commit.
  This is staged/spooled append, not our required end-to-end streaming. Reads
  deliver bounded chunks directly. Obtain a suitable upstream direct streaming
  contract, or an explicit product decision accepting staging; do not hide the
  spool, add a producer thread, or implement a private dependency bypass.
  [Released writer contract](https://github.com/sa6mwa/liblockdc/blob/v0.26.0/docs/collection-writer.md).
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
- [ ] **Enforce 90 days of conversation inactivity.**
  Configure the parent collection, not an age limit for individual entries.
  Renew by 7,776,000 seconds on newly committed activity; reads, duplicate
  retries, aborts and failed publication must not grant another logical
  lifetime. Reconcile native append renewal with later libailo marker failure;
  native physical publication alone is not logical conversation publication.
  Preserve the documented lifetime of item archives after explicit conversation
  deletion. Pouch expiry is lazy: decide and test the API-visible expiry boundary,
  startup/recovery, maintenance races, snapshots and orphan reclamation using
  controlled clocks. This policy is not active in today's chunk provider.
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

Most upstream versions were checked 2026-11-02; llama.cpp was rechecked 2026-11-03.
Versions below are verified candidates, not promises
that an upgrade fixes Raphael. Keep SDK compression/XML dependencies private;
never substitute workstation libraries for cpkt/cpktmisc.

| Dependency | Current pin after this upgrade | Candidate / action |
| --- | --- | --- |
| liblockdc | 0.26.0, ABI 9 | Keep exact released SDKs; no adjacent-source build. Collection API verification is separate from provider adoption. |
| liblql | 0.4.0, ABI 0 | Required by the new lockdc SDK; upgraded together. |
| Mesa | 26.2.4 | Qualify 26.2.6 first; see P0 above. |
| llama.cpp | v0.8.0 | Stable release adopted; ten integration patches retained, four rebased, plus one new test-only patch. Includes the AMD KV fix. See the [integration record](vendor/llama/README.md). Rolling b12078 is prerelease. |
| cpkt | 0.5.2 | Upgrade to [0.6.1](https://github.com/c89systems/cpkt/releases/tag/v0.6.1); includes a libssh2 AES-GCM handshake leak repair. Static users must relink. |
| cpktmisc | 0.2.1 | Upgrade with core to [0.3.0](https://github.com/c89systems/cpktmisc/releases/tag/v0.3.0); requires cpkt 0.6.1 and removes LZO. Audit codec/target and notice changes. |
| SPIRV-LLVM-Translator | 23.1.0 | Assess [23.1.3](https://github.com/KhronosGroup/SPIRV-LLVM-Translator/compare/v23.1.0...v23.1.3); includes a leak repair and translation/rounding corrections. Keep LLVM/Clang 23 compatibility. |
| SPIRV-Tools / glslang | 2026.4 / 16.6.0 | Match the published [Tools](https://github.com/KhronosGroup/SPIRV-Tools/releases/tag/v2026.4) / [glslang](https://github.com/KhronosGroup/glslang/releases/tag/16.6.0) releases checked; no upgrade justified by age alone. |
| lonejson / libpslog | 0.45.0 / 0.12.0 | Match the published [lonejson](https://github.com/sa6mwa/lonejson/releases/tag/v0.45.0) / [pslog](https://github.com/sa6mwa/libpslog/releases/tag/v0.12.0) releases checked. |
| libdrm | 2.4.134 | Matches the latest [archive](https://dri.freedesktop.org/libdrm/) checked; retain unless a relevant fix is identified. |
| Vulkan headers/loader/SPIR-V headers | SDK 1.4.363.0 | Check a coherent SDK/tag set when changing shader or loader requirements; no newer release was established by this audit. |

- [ ] **Upgrade cpkt/cpktmisc as a coordinated foundation change.**
  Update exact asset hashes, package requirements, host-generator SDK checks and
  notices. Re-prove static/shared libffi callbacks on all four Linux targets,
  encrypted/compressed persistence, host LLVM/Mesa generators and relocatable
  consumers. Check transitive SDK compatibility rather than assuming latest
  versions compose.
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
- [ ] **Re-prove the final static deployment after GPU/foundation changes.**
  Build all four Linux SDKs and relocated C/C++/Lua consumers. On actual GPU
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
