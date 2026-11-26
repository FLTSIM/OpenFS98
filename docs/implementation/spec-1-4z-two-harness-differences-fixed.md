---
title: 'Story 1.4 follow-up: diagnose, then fix, the two pre-existing harness differences'
type: 'bugfix'
created: '2026-11-25'
status: 'in-progress'
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
