---
title: 'Story 1.4 follow-up: diagnose, then fix, the two pre-existing harness differences'
type: 'bugfix'
created: '2026-11-25'
status: 'done'
baseline_commit: '27a9e496c2495c57b970c4e265054fce528b76e1'
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Story 1.4's final run left two harnesses differing from retail, both already at `27371fb` and logged in `docs/open-differences.md` ("Unsettled questions"). Read-only analysis points to harness artifacts, not module bugs, but the evidence is not yet enough to choose a fix.

**`g3ddiff-meshc-run`:** 43 of 20000 scenarios differ. Scenarios 9 and 192 differ in the program; in scenario 747, op 14, ours emits `line(...)`.
- Commit `48756cd` set the portable mesh compiler's `g3d_mesh_precision` to 24, the game's Direct3D precision (`src/g3d/g3d_mesh.c:113`).
- In `g3ddiff`, retail's mesh compiler runs at whatever the process left: MinGW's `__fpreset` (`fninit`, about `0x40122a` in the built EXE) gives control word `0x037f`, 64-bit, before any measurement at mesh entry.
- The portable arithmetic lives in `G3D_ours.dll` (`g3d_mesh_portable.o`, `FS98_PORTABLE_FP`). It never reads the control word, and the variable is not exported, so the harness EXE cannot set it.
- The precision mismatch is the leading hypothesis. Which `winds_round`/`mesh()` decision flips is unproven.

**`gauges-panels-run N=1000`:** 143 of 1000 differ. The three reported in detail (6, 20 and 28) each differ in one 0x80-byte K_HEAP block hash. The log has no raw words, and no evidence that all 143 share a cause. There are two hypotheses:
- **H1:** pointers into a rebuilt gauge's image outside `.data` normalise differently;
- **H2:** a stale `gauge_instances` window claims a re-used address.

`all-gauges-panels`' 0 differ used different fixtures and extra typed exceptions, so it does not validate H1.

**Approach:** Two phases with a checkpoint between them:
- **Phase 1** measures under wine, with no fixes, and reports.
- **Phase 2** implements the fix design the user approves at that checkpoint, and proves it.

## Boundaries & Constraints

**Always — phase 1 (diagnosis; scratch builds only, nothing in the repo changed except this spec's notes):**
- **G3D:**
  - **Measure precision.** In a scratch harness build, record the x87 control word at retail's mesh compiler entry, and at ours where it applies, for scenarios 9, 192 and 747.
  - **Instrument.** In a scratch `G3D_ours.dll` build, log which `winds_round` decision and which `mesh()` rejection fires, at the same points, for ours and for retail's walk, where the harness can observe it.
  - **Decisive check.** Run the scratch build with `g3d_mesh_precision` forced, through a scratch DLL setter or a build define, to the measured retail precision. Run `tools/ofs g3ddiff-meshc-run` at N=20000 and report whether all 43 go and nothing new appears.
  - **Staging.** `WINDOW_retail.dll` is staged explicitly, for a clean run.
- **PANELS:**
  - **Capture the first divergence.** For seeds 6, 20 and 28, a scratch `gaugespanelsdiff.exe` captures the first divergent checkpoint, with the differing block's allocation site and kind, its raw words on both sides, and the normalisation rule `norm()` (`tools/difftest/panelsdiff.c:218`) applied to each word. It also records each word's module identity, if any, and the gauges' load and unload history. Fixture files (`game/Gauges`) are pinned by hash.
  - **Discriminate H1 from H2.** With the same binaries, two controls:
    - hold lifetime rules fixed and check whether the words resolve into a rebuilt gauge's image outside `.data`, and in which section (H1);
    - change only stale-window matching in `gauge_pointer()` (`tools/difftest/gauges_panels.inc`) and see whether the difference goes (H2).
  - **Classify all 143** by their first differing block's provenance, so that "one cause" is shown, not assumed.
- **Running:** wine runs go under `flock ~/openfs98-verify/wine.lock`, one at a time, from a frozen copy, with no `wineserver -k` in `~/.wine`. Builds use `nice -n 19` and `-j 2`. Everything is done in a worktree off HEAD with the gitignored fixtures copied in.
- **Report, then CHECKPOINT 2:** `## Implementation Notes` holds the measurements and a proposed fix for each harness: what file and what change, and a bound on what it normalises. The implementer stops there. The user approves or changes the fix design, which is then recorded in this block as a decision.

**Always — phase 2 (only the fixes approved at checkpoint 2):**
- **G3D:**
  - the harness runs the portable path at retail's measured precision through a test-only setter;
  - `src/g3d/g3d_mesh.c`'s default of 24 and its game behaviour are unchanged;
  - the setter is not reachable from the game;
  - a portable-path break check is added, with one mutation of the portable path that must still be caught.
- **PANELS:**
  - any new normalisation is restricted to proven pointer fields;
  - it compares module and load identity, logical target and byte offset, and keeps unknown words exact;
  - targeted checks show that a changed resource, a changed row offset and a changed numeric word still differ;
  - for H2, address reuse and retained-after-unload pointers are tested separately.
- **Pass:** `g3ddiff-meshc-run` (N=20000) and `gauges-panels-run N=1000` give 0 differ. Their coverage and load counts are recorded for both gauge-panel modes, and the harness gets the time it needs (the final 1.4 run timed out).
- **Unchanged:** at their documented baselines and counts, these must be unchanged:
  - `g3ddiff-run`;
  - `g3d-breaks` at its documented N=1000 (the target defaults to 2000) from a frozen copy;
  - `all-gauges-panels`;
  - `gaugesdiff-run` and `suctiondiff-run` (their standalone baselines lack post-18a wine runs, so these runs become their first);
  - if shared PANELS harness code changes, `panelsdiff-run N=3000`, `panelsunit-run N=20000` and `panels-breaks` under its strict verdict rule.
- **Docs:**
  - `docs/open-differences.md` drops or rewrites the "Two harnesses differ" entry;
  - `docs/tools.md` updates its notes on both runs;
  - `docs/modules/g3d.md` re-dates its meshc claim;
  - the 1.4 entry in `deferred-work.md` moves to `deferred-work-resolved.md`.

**Never:**
- No change to G3D's mesh compiler behaviour or its default precision, to PANELS, or to any gauge.
- No normalisation broader than the proven pointer fields.
- No commits or staging.

**Decided (2026-11-27, user):** the entry control word and mesh() rejection traces are not required; 0x037f at the +0x64 call on both sides and 0 differ over 20000 scenarios with the forced 64-bit build stand in.

</frozen-after-approval>

## Code Map

- **G3D:**
  - `src/g3d/g3d_mesh.c:113` (`g3d_mesh_precision`), `:523` (portable arithmetic), `:782`; `winds_round` → `is_ear` → `take_ear` → `ear_triangles` → `mesh()`;
  - `src/g3d/g3d.def:2`, where the variable is not exported;
  - `cmake/steps/difftests.cmake:119` and `cmake/targets.cmake:566` (the meshc build: `G3D_ours.dll` with `g3d_mesh_portable.o`);
  - `tools/difftest/g3ddiff.c:1223`, `:4126`;
  - `tools/difftest/g3dmeshdiff.c:227`, `:270` (it links both mesh objects into the EXE and patches retail's walker, so its 100,000-polygon pass does not settle the whole-module question);
  - `docs/modules/g3d.md:705`.
- **PANELS:**
  - `tools/difftest/panelsdiff.c:171`, `:218` (`norm`), `:1383`, `:2055-2069` (`hash_words`, K_HEAP, the detail dump), `:3341`, `:3474`;
  - `tools/difftest/gauges_panels.inc:79`, `:97`, `:165`, `:207` (`gauge_pointer`, `gauge_instances`, the reverse scan that keeps retained pointers);
  - `src/panels/panels.c:2285`, `:4626` (`resource_bitmap` copies into a new surface; `surface_over` wraps its caller's bits);
  - `cmake/targets.cmake:519`; `tools/run_gaugesdiff.sh:33`.
- **Evidence:**
  - `~/openfs98-verify/p14-final/g3ddiff-meshc-run.log:180,199` and `gauges-panels-run.log:275`;
  - `~/openfs98-verify/p14-bisect/new-panels-1000.log:166-262`;
  - `all-gauges-panels.log:163`.
- **Breaks:**
  - `tools/g3d_breaks.py:407` uses the shared EXE with the hardware DLL;
  - `panels_breaks.py:191` uses plain `panelsdiff.exe`;
  - `gauges_breaks.py:117` and `gauge_breaks.py:3` use standalone EXEs.

## Tasks & Acceptance

**Execution:**
- [ ] Phase 1: worktree and fixtures; G3D measurements and the decisive check; PANELS captures, the H1/H2 controls, and the classification of all 143; the notes; then stop at checkpoint 2.
- [ ] Phase 2 (after approval): the approved harness fixes, the reruns and the docs.

**Acceptance Criteria:**
- **Phase 1:** each difference has a measured cause with its evidence, and each proposed fix states what it normalises and why that is bounded.
- **Phase 2:** both target runs give 0 differ, every listed baseline and break count holds, and the targeted checks show that real changes still differ.

## Verification

**Commands:**
- `tools/ofs g3ddiff-meshc-run` -- expected: 0 of 20000 differ (phase 2).
- `tools/ofs gauges-panels-run N=1000` -- expected: 1000 scenarios, 0 differ (phase 2).

## Review Triage Log

Spec review (Codex gpt-6.2-sol, 2026-11-25), on the first draft, which assumed the diagnoses:

| # | Finding | Verdict | Evidence / action |
|---|---|---|---|
| 1 | Retail's g3ddiff precision is `0x037f` (64-bit, via `__fpreset`), not 53 | blocker | It comes from the binary's disassembly. The spec now measures at mesh entry before choosing. |
| 2 | The portable variable lives in the DLL, unexported, and never reads the control word | blocker | A test-only setter, found and decided in phase 2. |
| 3 | The ear-selection diagnosis is unproven; staging | high | Instrumentation in phase 1; `WINDOW_retail.dll` staged. |
| 4 | The PANELS runs cannot discriminate H1 from H2 | blocker | First-divergence captures, two controls, and all 143 classified. |
| 5 | The evidence supports neither the probabilities nor a whole-image fix | high | The probabilities are dropped; the fix is decided at checkpoint 2. |
| 6 | Whole-image matching can hide real changes | high | Proven pointer fields only, unknown words exact, targeted checks. |
| 7 | The break and baseline matrix | medium | Pinned N, a portable break check, strict PANELS verdicts, the target names, the run-time limit. |

## Implementation Notes

Phase 1 (diagnosis, 2026-11-25/26). Scratch builds and outputs only, under the session scratchpad (`g3d/`, `panels/`); nothing in the repo changed except this block. Wine runs went under `flock ~/openfs98-verify/wine.lock`, from copies of the staged files (`WINDOW_retail.dll` staged explicitly; `game/Gauges` tree hash `ca0f03a5...beb7`, 120 files).

### G3D (`g3ddiff-meshc-run`)

- **Precision measured.** A scratch `g3ddiff` reads the x87 control word (`fnstcw`) around every `+0x64` service call, for scenarios 9, 192 and 747, on both sides: `0x037f` (64-bit) before and after, retail and ours. The harness is the caller that retail's mesh compiler inherits. Retail does not change the word. It was read at the service call, not inside retail's mesh entry.
- **Reproduced.** Scenarios 9 and 747 differ with the stock portable build (default 24): in scenario 9 the program parts at op 8, and in 747 ours emits `line(...)` at op 14. Scenario 192 differs at its snapshot after op 35 ("programs"). The 43 at N=20000 was not re-counted in phase 1.
- **Instrumented.** A scratch `G3D_ours.dll` logs each `winds_round` result and its final angle-sum vector. At 24 and 64 bits the sums differ in their low float bits (e.g. `c0be9fc0` against `c0be9fc2`), and the `winds_round` outcomes then diverge in the logs. Retail's own walk is not observable from the harness except through the emitted program, so no per-decision retail log exists.
- **Decisive check.** The scratch portable build with `g3d_mesh_precision` initialised to 64 gives **`g3ddiff: 20000 scenarios, 0 differ`** (exit 0), and nothing new appears. Cause: the harness runs retail at 64 bits (`__fpreset`) while the portable arithmetic runs at its game default of 24.
- **Proposed fix.**
  - `src/g3d/g3d_mesh.c`: `int g3d_mesh_precision = G3D_MESH_PRECISION_DEFAULT;` under `#ifndef G3D_MESH_PRECISION_DEFAULT / #define G3D_MESH_PRECISION_DEFAULT 24 / #endif`. The game's value and behaviour are unchanged.
  - Only the `g3d_mesh_portable.o` compile line of `g3ddiff-meshc-run` (`cmake/targets.cmake`) gets `-DG3D_MESH_PRECISION_DEFAULT=64`. No setter is exported, and the game DLL and `g3dmeshdiff` objects never see the define, so nothing is reachable from the game.
  - A portable-path break (one mutation, e.g. the portable `fx_round` or `winds_round` threshold) is added to `tools/g3d_breaks.py` against this build.
  - Bound: it changes one build define for the one harness object, and states the precision the harness itself runs retail at.

### PANELS (`gauges-panels-run N=1000`)

- **Reproduced.** A scratch `gaugespanelsdiff` (a probe that dumps, per checkpoint, each K_HEAP block's allocation site, hash and tagged words, and the gauge load/unload history) gives 1000 scenarios, 143 differ, with equal loads (warning 493/493, suction 382/382).
- **First divergence, scenarios 6, 20 and 28.** The differing block is 0x80 bytes, allocated at PANELS RVA `0x1ad2`, on the same site on both sides.
  - Offset `+0x4c` holds a pointer into the **retail-only** gauge `737-400.Engine-Start.GAU`'s `.data` (`+0x2430`), retained after that gauge was unloaded.
  - Retail sees it at `0x32002430` and the rebuilt side at `0x13b82430`, `0x13912430` and `0x13662430`. The rebuilt warning gauge's image takes `0x32000000`, the base retail's Engine-Start prefers, so Wine relocates Engine-Start on that side.
  - `norm()` leaves the word raw because `gauge_pointer()` knows only the two rebuilt kinds, and `region_of()` skips unloaded regions.
  - `+0x2c` of the same block is already paired by `gauge_pointer()` (`GP:`), so it is not part of the difference.
- **Neither H1 nor H2 as stated.** Pointers into a rebuilt gauge's image outside `.data` (H1) were not the first difference in any scenario. The cause is a third one (H3): a retained pointer into a *non-replaced* gauge that loads at a different address.
- **All 143 classified** by their first differing block: **142 are H3** (pointer into a retail-only gauge image, differing by base). **1 (scenario 520) is H2**: no heap block differs, but gauge 36's own data (retail-only Engine-Start) holds pointers into its own `.data`, and on the rebuilt side `gauge_pointer()` matches them to the *stale* unloaded rebuilt-warning window at the reused address `0x32002000`. They become `ff003000`/`ff003018` against retail's `e8002000`.
- **Controls** (same binaries, one change each, N=1000):

  | Control | Result |
  |---|---|
  | H3: map any pointer into a loaded or retained gauge image to (load ordinal, offset) | 143 to **52** differ |
  | H2: match only live gauge instances in `gauge_pointer()` | 143 to **178** differ (worse: retained pointers need the stale window; stale matching needs a generation check, not removal) |

- **Residual 52.** In the three checked (6, 59, 66) a pair of consecutive heap blocks holds, at `+0x18`, the HMODULE (image base) of Engine-Start on the rebuilt side. The retail side's word at that slot is not a gauge-image address, so the pairing is not understood. Needs another capture (raw word of untagged slots) in phase 2 before a rule is written.
- **Proposed fix (to approve or change).** In `tools/difftest/gauges_panels.inc` and the K_HEAP hash only:
  1. Record every gauge load (all kinds) with a load ordinal, name and image range. In `gauge_heap_hash`, a word in the proven window-record block (allocation site `0x1ad2`, field `offset % 0x80`, starting with `+0x4c`) that falls in a recorded image maps to (ordinal, offset from the image base). All other words stay exact. Compared across sides: gauge name, load ordinal and offset. Targeted checks show that a different gauge, a different offset and a plain numeric word still differ.
  2. Make `gauge_pointer()` skip an unloaded instance whose window a later load has re-used (generation check). Retained-after-unload pointers are still matched. Address reuse and retained pointers are tested separately.
  3. Resolve the residual `+0x18` word first (open).
- **Not done in phase 1:** `all-gauges-panels` was not rerun, and the 52 residual scenarios were not classified beyond three.

Stopped at checkpoint 2 as specified; no repo files other than this spec were changed (worktree fixtures `orig/`, `game/`, `cd/`, `build/`, `bin/` are gitignored).

### Checkpoint 2 decision (user, 2026-11-26)

"G3D now, PANELS after residual": phase 2 for G3D goes ahead as designed above. For PANELS, the 52-scenario residual is explained first (capture only, no harness change), then a complete rule aimed at 0 differ is reported for approval before it is implemented.

### Phase 2, G3D (done 2026-11-26)

- `src/g3d/g3d_mesh.c`: `g3d_mesh_precision` initialised from `G3D_MESH_PRECISION_DEFAULT` (`#ifndef`, default 24: the game build is unchanged). Only the `g3d_mesh_portable.o` compile of `g3ddiff-meshc-run` passes `-DG3D_MESH_PRECISION_DEFAULT=64`, so no setter exists and nothing is reachable from the game.
- `cmake/targets.cmake`: `g3ddiff-run` and `g3ddiff-meshc-run` now stage `WINDOW_retail.dll` (neither did; both failed with "cannot load WINDOW_retail.dll" from a clean tree). New target `g3d-portable-breaks`.
- `tools/g3d_breaks.py --portable [N]`: two mutations of the portable software float (`mul64-high`, `mul-normalise`), each built as the meshc harness is. At N=2000 both are caught (58 and 37 differ); the real build goes back afterwards. A first pair (`mul64` carry, `winds_round`'s `>` to `>=`) was missed at N=2000 and replaced: the harness cannot see errors below the float's rounding.
- Docs: `docs/tools.md`, `docs/modules/g3d.md`, `docs/open-differences.md` (entry rewritten; PANELS part remains).
- Verified from a frozen copy (`~/openfs98-verify/p14z-g3d`), under the wine lock: `g3ddiff-meshc-run` 20000 scenarios, **0 differ**; `g3ddiff-run` 20000 scenarios, **0 differ** (unchanged baseline).
- Not run: `g3dmeshdiff-run` (killed after ~45 min with the lock held; it is not on the spec's list and its objects are unchanged). `g3d-breaks N=1000` (about 4 h) was queued from a second frozen copy (`~/openfs98-verify/p14z-g3d-breaks`, log `r_breaks.log`); its result is not yet in. `all-gauges-panels`, `gaugesdiff-run` and `suctiondiff-run` wait for the PANELS fix.

### PANELS residual explained, and the complete rule (for approval)

Scratch controls only (a probe `gaugespanelsdiff` with the rule below applied inside `norm()`, same binaries and fixtures, N=1000 each):

| Control | differ |
|---|---|
| baseline | 143 |
| map any pointer into any recorded gauge image to (load ordinal, offset), first | 52 |
| rule below, but also applied to gauge data words | 6 |
| rule below (heap blocks only) | **0** (`IDENTICAL`) |

- **The 52 (an over-broad control).** A table of 16 row pointers in a pair of heap blocks (allocation sites `0xc6e8`/`0xc794`) points into a DIB. Retail's DIB sits at `0x13200000`. On the rebuilt side the same DIB sits at `0x13b80000`, which is inside the *stale* image range of the unloaded Engine-Start gauge (it was loaded there earlier). My control checked stale images before DIBs and so mapped the rebuilt side's DIB pointers to Engine-Start offsets, while retail's stayed DIB-normalised. In the baseline both sides already normalise these as the same DIB. So address reuse by a DIB is real, and stale-image matching must come last.
- **The 6 left** (scenarios 233, 352, 677, 689, 713, 890). A word in a *retail-only* gauge's data (`Extra-300.Control_Surfaces.GAU +0x248`) holds `0x32000000` on both sides. That address belongs to different unloaded gauges on the two sides (retail's Engine-Start, the rebuilt warning gauge), because the rebuilt warning gauge takes Engine-Start's preferred base. The raw words are equal. The stale mapping made them differ. So the stale-image mapping must not touch gauge data words.
- **Rule** (`tools/difftest/gauges_panels.inc`, `norm()`, the live-only `gauge_pointer()` of step 1 applies to heap blocks and gauge data alike, the stale steps of step 2 to heap blocks only):
  1. Resolve as today, except that `gauge_pointer()` matches **live** gauge instances only. Live regions, DIBs and the arena resolve first, as today.
  2. Only for a word that is at least `0x100000`, resolves to nothing above (not inside a live region or DIB), and is in a heap block: try `gauge_pointer()` over unloaded instances, latest first (retained rebuilt-gauge pointers; the generation check is that a later load or DIB owning the address wins), then the recorded image ranges of all gauge loads of this run, latest first. The word maps to (load ordinal, offset from the image base). Compared across sides: load ordinal and offset. Load ordinals pair because both sides load the same gauges in the same order.
  3. Everything else, including numeric words and gauge-data words, stays exact.
- **Bound.** A recorded range is a gauge image of this run (at most 0x4f000 bytes each). Only words in heap blocks that point into such an image and no live owner are mapped. Targeted checks for approval, each must still differ: a different gauge (ordinal), a different offset in the same image, a numeric word that falls in an image range of one side only, and a pointer whose live owner changed. Address reuse (a DIB over an unloaded gauge, the 52 case) and retained-after-unload pointers are tested separately.
- **Caveats.** In gauge-panel mode the same fix needs the harness to keep a per-run table of gauge image ranges (load ordinal, name, range, live flag); it is reset with the arena. The control did not run the targeted checks or the break counts, and a rerun of `panelsdiff-run`, `panelsunit-run` and `panels-breaks` is needed if shared PANELS code changes (the rule is under `GAUGES_DIFF` only). The probe's load counts (warning 336, suction 348) are lower than the earlier baseline's (493/382), which counted the extra detail reruns.

### Checkpoint 2, PANELS (user, 2026-11-26)

The user approved the PANELS rule above as written. Implementation: `tools/difftest/gauges_panels.inc` (the per-run table of gauge image ranges and the stale mapping) with hooks in `panelsdiff.c` under `GAUGES_DIFF` only.

### Phase 2 results (2026-11-26, frozen copies under `~/openfs98-verify/p14z-*`, one run at a time under the wine lock)

| Run | Result |
|---|---|
| `g3ddiff-meshc-run` N=20000 | 0 differ |
| `g3d-portable-breaks` N=2000 | 2 of 2 caught |
| `g3ddiff-run` N=20000 | 0 differ (unchanged) |
| `g3d-breaks N=1000` | 317 breaks, 278 caught, 7 proven equivalent, 32 missed, exit 2 (misses): equals the documented baseline |
| `gauges-panels-run N=1000` | 1000 scenarios, 0 differ, `IDENTICAL`; loads warning 336/336, suction 348/348; the targeted self-test passes at every start |
| `all-gauges-panels` | 1000 scenarios, 0 differ, exit 0 (unchanged) |
| `gaugesdiff-run` | 737WARNG 20000 scenarios, 0 differ, at its base and relocated (first run since 18a) |
| `suctiondiff-run` | CESSSUC 20000 scenarios and 240288 clamp vectors, 0 differ, both bases (first run since 18a) |
| `panelsdiff-run`, `panelsunit-run`, `panels-breaks` | not rerun: the plain (non-`GAUGES_DIFF`) build of `panelsdiff.c` compiles to identical assembly before and after |
| `g3dmeshdiff-run` | not completed: killed by me after about 45 minutes to free the lock (exit 143); not on the spec's list, and its objects are unchanged |

Docs updated: `docs/tools.md` baselines, `docs/open-differences.md` (entry removed), `docs/modules/g3d.md`; the 1.4 and 1.4z entries moved to `deferred-work-resolved.md`.

### Review triage (Codex code review of the first implementation, 2026-11-26; `~/openfs98-verify/p14z-code-review.out`)

| # | Finding | Verdict | Action and evidence |
|---|---|---|---|
| 1 | Stale lookup ran on every heap word; a raw word could equal a generated tag | accepted | Gated to the data of the host list at PANELS's `.data` `+0x16000` (a pointer array; `blk.origin` set in `h_list_add`). Every word is hashed as (class, identity, offset), so a raw word and a mapped pointer cannot collide. **Correction to the approved rule's wording:** the "record allocated at RVA `0x1ad2`, `+0x4c`" was a sticky site value in the first probe (the last `h_alloc` caller), not the allocator. The 0x80-byte block is that list's data, and its `+0x4c` entry is the retained one in scenarios 6, 20 and 28. With the gate narrowed by site/size alone the 142 returned, which is how it was found |
| 2 | `g3d_breaks.py --portable` used the shared harness directory, no timeout | accepted | Private `bin/difftest-portable`, removed afterwards; 1800 s timeout, a timeout counts as a miss; the unmodified portable build must pass first (`baseline passes ... 0 differ`) |
| 3 | `g3ddiff` target did not stage `WINDOW_retail.dll` | accepted | `g3ddiff` stages it; `g3ddiff-run`, `g3ddiff-meshc-run`, `g3d-breaks` and `g3d-portable-breaks` depend on `g3ddiff` instead of repeating the copies |
| 4 | Older unloaded gauge won over a newer one; `0xe4000000` collision between regions 4 and 36 | accepted | The latest unloaded image holding the address is chosen first, then decoded; identities are 64-bit keys (class, id, offset) with no shared bit-field (`gauge_word_key` in `gauges_panels.inc`); live gauge, live region, live DIB, stale gauge, deleted DIB and exact are separate classes |
| 5 | DIB rows compared by index only; deleted DIB could claim an address | accepted | `gauge_dib_owner`: exact bounds and the live flag; key is (DIB id, byte offset); a deleted DIB is tried only after a stale image, in its own class |
| 6 | Self-test did not drive the production path | accepted | `gauge_stale_selftest()` registers images with `gauge_image_loaded/_unloaded`, DIBs with `hk_CreateDIBSection/hk_DeleteObject`, blocks with `arena_get`, and compares the production `gauge_block_hash`. Cases (each a real difference unless marked same): retained pointer (same gauge and offset: same), changed offset, changed gauge, numeric word inside a recorded image outside the proven place, raw word equal to a tag, changed live owner, address reuse, same DIB row (same), changed row offset, live DIB over an unloaded gauge, live DIB in any block (same), deleted DIB under an unloaded gauge (same) |
| 6b | Show the self-test can fail | done | Six broken rules, each built from the tree with one edit and run under the lock: all exit 2 with the case named (`~/openfs98-verify/p14z-panels/r_selftest.log`): `no-stale-offset`, `no-gauge-name`, `stale-anywhere`, `dib-dead-claims`, `dib-no-offset`, `stale-first`. The real build prints nothing to stderr there |
| 7 | 278 caught includes ten 900 s timeouts (strict: 268 / 7 / 42) | accepted, not this story | Deferred-work entry added (strict verdict in `g3d_breaks.py`, then rerun); 278 stays this story's equality check |
| 8 | Entry-point control word and `mesh()` rejection traces for scenarios 9, 192 and 747 | partly | Control word `0x037f` at the `+0x64` service call, before and after, both sides (phase 1). Not obtained: a reading inside retail's mesh entry (needs a retail breakpoint) and a trace of which `mesh()` rejection fires (needs instrumenting the rebuilt module's rejection returns and a retail comparison); the `winds_round` sums are logged. For the user to accept or ask for |
| 9 | Logical gauge name missing; approved negative cases | accepted | The image record carries the logical gauge name (hash of the requested path's file name) and the load ordinal; both are in the compared key |
| 10 | Outer invocation records | done | Below |

Reruns after the fixes (frozen copy `~/openfs98-verify/p14z-panels`, each under `flock ~/openfs98-verify/wine.lock nice -n 19 ...`; builds `nice -n 19 cmake --build ... -j 2`; `invocations.txt` there has the lines):

| Run | Result |
|---|---|
| `gauges-panels-run N=1000` | 1000 scenarios, 0 differ, `IDENTICAL`; loads warning 336/336, suction 348/348; self-test passes |
| `all-gauges-panels N=1000` | 1000 scenarios, 0 differ |
| `g3ddiff-meshc-run N=20000` | 0 differ |
| `g3ddiff-run N=20000` | 0 differ |
| `g3d-portable-breaks N=2000` | baseline passes; 2 of 2 caught (58 and 37 differ) |

The gauges-panels-run difference after the first tightening (142 of 1000) was the gate narrowed to the wrong place; dumped for scenarios 6, 20 and 28, the retained word sat in the `+0x16000` list's data. `gaugesdiff-run`, `suctiondiff-run` and the G3D break baseline were not rerun: neither the standalone gauge harnesses (`gaugesdiff.exe`, `suctiondiff.exe`) nor the G3D module changed since their recorded runs. The plain `panelsdiff.c` still compiles to identical assembly (line markers ignored).

Outer invocation records for the reruns (from `invocations.txt` in the frozen copy; the earlier phase-2 runs of 2026-11-26 used the same `flock ~/openfs98-verify/wine.lock nice -n 19` wrapper, from `~/openfs98-verify/p14z-g3d` and `p14z-g3d-breaks`, built with `nice -n 19 cmake --build build/win32-retail --target openfs98 difftests -j2`):

```
build: nice -n 19 cmake --build build/win32-retail --target openfs98 difftests test-gauges -j 2
20:46:16 flock ~/openfs98-verify/wine.lock nice -n 19 { selftest.sh; tools/ofs gauges-panels-run N=1000; tools/ofs all-gauges-panels N=1000 }
20:59:59 flock ~/openfs98-verify/wine.lock nice -n 19 tools/ofs g3ddiff-meshc-run N=20000
21:20:51 flock ~/openfs98-verify/wine.lock nice -n 19 tools/ofs g3ddiff-run N=20000
21:25:28 flock ~/openfs98-verify/wine.lock nice -n 19 tools/ofs g3d-portable-breaks N=2000
```

### Review triage, round 2 (Codex follow-up, 2026-11-27; `~/openfs98-verify/p14z-rereview.out`; the last round)

| # | Finding | Verdict | Action and evidence |
|---|---|---|---|
| 1 | The rebuilt-gauge stale branch skipped the `+0x16000` gate | accepted | One gate for the latest stale image, rebuilt or not (`gauge_block_hash`'s `gauge_word_key`); outside the list a word is a number. Self-test: numeric word in a recognised gauge's data window outside the list differs across a relocation; inside the list it pairs |
| 2 | A rejected newest image returned exact before the deleted-DIB fallback | accepted | A rejected match stops the scan and falls through to the bounds-checked deleted-DIB fallback; self-test "deleted DIB row outside the proven place, image over one side (same)" |
| 3 | Self-test bypassed `h_list_add`, the load and release hooks and `put_state` | accepted | `gauge_stale_selftest_real` loads a gauge with `h_module_load`, releases it with `h_module_release`, grows the proven list with `h_list_add`, and compares the `blocks=` hash `put_state` writes; the second side has the gauge's preferred base reserved so Wine relocates it (retail-only gauge, and a recognised rebuilt one). It runs after the side images are staged and clears the load counters. Not run in `all-gauges-panels` for retail-only gauges (none exist there) |
| 3b | Show it fails when that path breaks | done | Twelve one-edit builds under the lock all exit 2, naming the case (`r_selftest.log`): the earlier six, plus `no-list-origin` (`h_list_add` no longer sets `cur_origin`), `no-load-hook`, `no-release-hook`, `no-put-state-rule` (put_state hashes with the plain words), `rebuilt-ungated` and `no-fallthrough`. The real build prints nothing |
| 4 | `--portable` shared a directory; the target restaged the shared DLL | accepted | `tempfile.mkdtemp` per invocation (removed afterwards); `g3d-portable-breaks` depends on `openfs98 difftests`, not `g3ddiff` |

Reruns (frozen copy `~/openfs98-verify/p14z-panels`; each under `flock ~/openfs98-verify/wine.lock nice -n 19`): `gauges-panels-run N=1000`: 1000 scenarios, 0 differ, IDENTICAL, loads 336/336 and 348/348 (a first rerun showed 340 rebuilt: the self-test's loads were counted, now cleared); `all-gauges-panels N=1000`: 0 differ; `g3d-portable-breaks N=2000`: baseline passes, 2 of 2 caught, the temporary directory gone. A first `all-gauges-panels` rerun stopped on the new self-test (it assumed a retail-only gauge); the retail-only cases now skip there.
