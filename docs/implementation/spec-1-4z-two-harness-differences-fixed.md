---
title: 'Story 1.4 follow-up: the two pre-existing harness differences fixed'
type: 'bugfix'
created: '2026-11-25'
status: 'draft'
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Story 1.4's final run left two harnesses differing from retail, both already at `27371fb` and logged in `docs/open-differences.md` ("Unsettled questions") and `deferred-work.md`. A read-only analysis (2026-11-25) points to harness artifacts in both, not module bugs:
- **`g3ddiff-meshc-run`:** 43 of 20000 scenarios differ. Commit `48756cd` (2026-11-15) changed `g3d_mesh_precision`'s default from 53 to 24 bits (`src/g3d/g3d_mesh.c:113`), the game's Direct3D precision. `tools/difftest/g3ddiff.c` sets neither it nor the x87 control word, so retail meshes at the process default of 53 bits while ours meshes at 24. Polygons near `winds_round`'s pi threshold pick other ears: `mesh()` fails and the original instruction draws as `line(...)` (scenario 747, op 14). `g3dmeshdiff.c:270` matches the two precisions and gives 0 differ over 100,000 polygons.
- **`gauges-panels-run N=1000`:** 143 of 1000 differ, in every case one 0x80-byte K_HEAP block of the PANELS heap model (scenarios 6, 20 and 28: B66/B43, next after the "Minicontrols" block). The transcript, `data=`, `dibN=`, `global=` and `gN=` all agree. The probable cause (about 55%) is that the block holds pointers into a rebuilt gauge's image outside its `.data` window. Our gauges place `.rsrc` at RVA 0x7000 against retail's 0x5000, with other image bases. `norm()` (`tools/difftest/panelsdiff.c:218`) through `gauge_pointer()` (`tools/difftest/gauges_panels.inc`) knows only the `.data` windows and callbacks, so those words normalise differently. `all-gauges-panels`, which normalises such words (`gauge_heap_hash`, `gauge_data_word`), gave 1000 scenarios, 0 differ in the same run. The alternative (about 30%) is a stale `gauge_instances` window claiming a re-used address on one side.

**Approach:** Confirm each cause with one wine run before changing anything, then fix the harness, not the module. Both difftests must reach 0 differ. The docs then stop listing them as differences.

## Boundaries & Constraints

**Always:**
- **Confirm before fixing.**
  - **G3D:** a scratch build with `g3d_mesh_precision = 53;` set in `g3ddiff.c` before the scenarios, running `tools/ofs g3ddiff-meshc-run` (N=20000). It must give 0 differ, with scenarios 9, 192 and 747 gone. If it does not, stop and report.
  - **PANELS:** run scenario 6 twice, under the same environment as `tools/run_gaugesdiff.sh:33-36`: `PANELS_STRICT=1 PANELS_SCENARIO=6`, once as is and once with `GAUGES_ONLY=zz`, which loads no rebuilt gauge. Then a scratch dump of raw and normalised words for blocks over 0x48 bytes, to find which address range the differing words fall in. Pick the fix from that evidence (H1, `.rsrc`/image pointers, or H2, stale instances). If neither explains it, stop and report.
- **Fixes:**
  - **G3D:** `tools/difftest/g3ddiff.c` sets `g3d_mesh_precision` to the precision retail runs at in that harness, reading the control word rather than hard-coding 53 if that is no more code. `src/g3d/` is unchanged.
  - **PANELS:** `gauge_pointer()` (or `norm()`) maps any address inside a replacement gauge's whole image to a layout-independent key, as the all-gauges path does. For H2, unloaded gauges' windows are dropped. `src/panels/` and `src/gauges/` are unchanged.
- **After the fixes:**
  - `g3ddiff-meshc-run` gives 0 differ at N=20000, and the plain `g3ddiff-run` baseline is unchanged.
  - `gauges-panels-run N=1000` gives 0 differ, and `all-gauges-panels`, `gaugesdiff` and `suctiondiff` are unchanged at their baselines.
  - Every harness edit gets its unbroken baseline rerun at full scale (AGENTS.md).
- **Breaks:** if a break suite uses either harness (the panels, warning and suction break runners, `g3d-breaks`), rerun it from a frozen copy and confirm its documented count. A fix that hides differences must not lose catches.
- **Docs:**
  - `docs/open-differences.md` drops or rewrites the "Two harnesses differ" entry;
  - `docs/tools.md` fixes its "43 differ" note (about line 72) and the gauges-panels note;
  - `docs/modules/g3d.md`'s "3,000 scenarios, 0 differences" claim for meshc is re-dated;
  - the 1.4 entry in `deferred-work.md` is marked RESOLVED with a pointer here.
- **Running:**
  - Wine runs under `flock ~/openfs98-verify/wine.lock`, never overlapping another run, with no `wineserver -k` in the shared `~/.wine`.
  - Builds under `nice -n 19`, `-j 2`.
  - Done in a worktree off HEAD if other slices are still verifying in the main tree.

**Never:**
- No change to G3D's mesh compiler, its default precision, PANELS or any gauge.
- No masking that would also hide a real gauge or PANELS state difference: only address-layout words are normalised.
- No commits or staging.

</frozen-after-approval>

## Code Map

- **G3D:**
  - `src/g3d/g3d_mesh.c:113` (`g3d_mesh_precision`); `winds_round` → `is_ear` → `take_ear` → `ear_triangles` → `mesh()`.
  - `tools/difftest/g3ddiff.c`, which has no control-word or precision setting. `tools/difftest/g3dmeshdiff.c:37,85,270` (`extern`, `fldcw`, the precision matched per run) is the pattern.
- **PANELS:**
  - `tools/difftest/panelsdiff.c:218-243` (`norm`) and `:2064` (`hash_words(..., K_HEAP)`, with the detail dump just after);
  - `tools/difftest/gauges_panels.inc` (`gauge_pointer`, `gauge_instances`; the `ALL_DIFF` path's `gauge_heap_hash`/`gauge_data_word`);
  - `src/panels/panels.c:2870` (`surface_over`: a `rows*4` row table, 0x80 for 32 rows), `:3688` (`resource_bitmap`) and `:210` (`surface_from_bitmap`).
- **Recipes:**
  - `cmake/steps/difftests.cmake:281` builds `gaugespanelsdiff.exe` with `-DGAUGES_DIFF`;
  - `cmake/targets.cmake:513` → `tools/run_gaugesdiff.sh:33`.
- **Evidence:**
  - `~/openfs98-verify/p14-final/g3ddiff-meshc-run.log` and `gauges-panels-run.log`;
  - `~/openfs98-verify/p14-bisect/new-panels-1000.log` (lines 166-262) and the old-tree logs `old-meshc*.log` and `old-panels-1000*.log`;
  - `all-gauges-panels.log` (0 differ).

## Tasks & Acceptance

**Execution:**
- [ ] The two confirming runs, recorded in the notes.
- [ ] `tools/difftest/g3ddiff.c` -- the matched precision.
- [ ] `tools/difftest/gauges_panels.inc` / `panelsdiff.c` -- the normalisation, chosen from the evidence.
- [ ] Reruns: `g3ddiff-meshc-run`, `g3ddiff-run`, `gauges-panels-run N=1000`, `all-gauges-panels`, `gaugesdiff`, `suctiondiff`, and the affected break suites.
- [ ] Docs as listed; `deferred-work.md` 1.4 entry RESOLVED.

**Acceptance Criteria:**
- Given the fixed harnesses, when `g3ddiff-meshc-run` (N=20000) and `gauges-panels-run N=1000` run, then both report 0 differ.
- Given the other runs, every documented baseline and break count is unchanged.
- Given the worktree, only `tools/difftest/` files and docs changed.

## Verification

**Commands:**
- `tools/ofs g3ddiff-meshc-run` -- expected: 0 of 20000 differ.
- `tools/ofs gauges-panels-run N=1000` -- expected: 1000 scenarios, 0 differ.
