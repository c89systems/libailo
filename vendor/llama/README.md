# llama.cpp integration and upgrades

The authoritative inputs are the `llama` recipe in
[`cmake/dependencies.json`](../../cmake/dependencies.json), the ordered
[`patches/series`](patches/series), and its patch files. The recipe pins the
upstream source archive by SHA-256. Sources and build outputs live under
`.cache/deps-build/TARGET/llama/`; edit the recorded patches when changing the
integration. A changed recipe replaces that owned extraction/build directory.

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

## Audit on 2026-11-03

The current pin is `b11430`. Reconstructing its verified archive and applying all
ten recorded patches with zero fuzz exactly reproduces both the GNU and musl
source inputs. Patch backup/reject artifacts were preserved separately in the
audit directory.
There are no unrecorded source changes in either prepared tree.

GitHub's latest stable release is
[`v0.8.0`](https://github.com/ggml-org/llama.cpp/releases/tag/v0.8.0), commit
`8642541b255bfb5dc39cd351a178be09013a55d6`, published 2026-10-28. It is 530
commits ahead of our pin. The latest rolling release checked was
[`b12078`](https://github.com/ggml-org/llama.cpp/releases/tag/b12078), marked
prerelease. Recommend the stable release as the next upgrade candidate.

The scratch stable archive has SHA-256
`51b1e025d7b12e83fa55736e65bc254acd9d23e49f546be7531368fef82210ee`.
The production pin has not changed.

| Patches | Stable-release trial |
| --- | --- |
| 0001–0005 and 0008 | Apply unchanged, with zero fuzz |
| 0006: worker logging scope | Needs rebase |
| 0007: Vulkan physical identity | Needs rebase |
| 0009: diagnostics routing | Needs rebase; the largest patch, touching 21 files |
| 0010: Raphael activation policy | Feature-selection hunks still match; insertion depends on rebasing 0009's logging include |

The existing API inventory guard rejects this candidate until its API changes
are reconciled. In particular, `mtmd` adds experimental streaming-audio APIs.
The stable source already contains the AMD `kv_raw_strided` attention path in
our Halo backlog. Check and reuse that implementation during the upgrade.

Audit inputs and logs: `build/raphael-followup/vendor-audit/`. This is a patch/API
assessment; the stable candidate has not been built or qualified as libailo.

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
