---
title: 'Story 1.8, slice 6c: OPTIONS uses arena.h'
type: 'refactor'
created: '2026-11-25'
status: 'ready-for-dev'
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-1-8f-main-uses-arena-h.md', '{project-root}/OpenFS98/docs/implementation/spec-1-8g-break-runners-share-build-only.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** OPTIONS still holds 27 banned `(uint32_t)(uintptr_t)` casts in six files (`tools/arena_lint_allowed.txt` rows `src/options/options_{book 4, demo 2, keys 12, logbook 5, replay 2, videos 2}.c`, no defines). It also has four reverse casts `(uint8_t *)(uintptr_t)(uint32_t)`, at `options_book.c:231,346` and `options_videos.c:275,407`.

**Approach:** Convert them to `include/fs98/arena.h`, as slices 1–6a did, and delete the six rows. Give `tools/options_breaks.py` a `--build-only` mode on slice 6b0's shared helper. Prove the change by byte identity with the packaged gate, including every one of the runner's 859 mutants.

**Order:** this slice is implemented after 6b0 and 6b are in the main tree. It uses 6b0's helper and 6b's gate support for case directories holding named PEs.

## Boundaries & Constraints

**Always:**
- **Sources:** every banned cast becomes `fs98_handle(x)`, and every reverse cast `fs98_ptr(x)` with its existing pointer cast kept in front (`options_book.c:346` keeps `const char *`; the whole `send_message(...)` result is the operand). Operands keep their extent: `keys.reset-args` keeps `LIST(page)` as the operand; `demo.populate-add-string` keeps `+ 0x100` outside `fs98_handle(record)`. Each changed file includes `fs98/arena.h`. The module has no data image, so it defines no `FS98_DATA_IMAGE`.
- **Lint:** reverse casts go down by exactly 4. The six rows go, checked with `--prior` on the list at this slice's baseline. `gate.sh` also asserts the delta: the after allowlist equals the prior minus exactly those six rows, and the reverse count is the prior's minus 4.
- **Recipes:** `-Iinclude` is added, as the last compile flag in both CMake recipes so that shared units replay once, to `cmake/steps/all.cmake:373` (OPTIONS_ours.dll, 15 units), `cmake/steps/difftests.cmake:342` (optionseventsdiff.exe, 14 of those plus `optionseventsdiff.c`) and the mutant argv in `tools/options_breaks.py`.
- **The runner change, in two steps:**
  - **Step 1** adds `--build-only DIR [CASE...]` through 6b0's helper and changes nothing else. Its case ids are `<unit>.<name>`, where unit is the unit list or `unwind`: 831 normal and 28 unwind, 859 cases. Each case directory holds exactly the built `<name>.exe` and `meta.json`.
    - The mutation text and the compiler argv come from the same code `one()` uses, not a copy of it.
    - Parity with the legacy path: the baseline's runner, run in wine mode with a fake `wine` that exits 0 and 6b0's gcc trace wrapper, gives the legacy compile trace. Step 1's `--build-only` trace must equal it after path normalisation.
    - The ordered list of the 859 ids, taken from the baseline runner, is saved as `p18h/cases.txt`.
  - **Step 2** repairs the anchors that hold a converted cast. The cases are `populate-item-data`, `error-day-value`, `error-night-value`, `error-instrument-value`, `fill-data`, `show-row-data`, `error-block`, `populate-add-string`, `populate-data-index`, `populate-data-record` and `reset-args`.
    - Each anchor, and each replacement that holds the cast (seven of them), changes only where the converted text appears.
    - For each of the 11, a check shows that applying the old mutation to the old source and then converting gives the same text as applying the repaired mutation to the converted source.
  - **Wine mode** is unchanged in both steps, except that its argv gains `-Iinclude`.
- **The gate:** `compare.py --require-set p18h` requires:
  - `OPTIONS.DLL` and `options-diff/optionseventsdiff.exe`;
  - all 16 replayed units (the 15 DLL units and `optionseventsdiff.c`) replayed exactly once per side, each with exactly one trailing `-Iinclude` added on the after side and the other flags equal, as in slice 4; both original invocations present in each side's `gcc.log`;
  - exactly one `--mut` pair, `mut-options`: its case directories exactly the ids in `p18h/cases.txt` on both sides, each holding `<name>.exe` and a `meta.json` whose name is its directory, with every `<name>.exe` identical after normalisation.
  - After the `compare.py` change, every earlier set (p18b to p18g) must still pass.
- **Snapshots** go at `~/openfs98-verify/p18h`.
  - **Before** is `git archive` of the baseline plus `orig/`, with step 1's `tools/options_breaks.py` overlaid. That is the only overlay, and it is recorded in the notes.
  - **After** is the working tree plus `orig/`.
  - Both are built at one absolute root by `build_side.sh`, with the prior at `p18h/prior.txt`. Each side's mutants are built from its populated root before the root is recreated, with `python3 -B` and DIR outside the root.
  - The baseline commit and the gate revision (with 6b's `p18g` support) are recorded in the notes. The earlier sets are rerun with the commands recorded in their specs (6b0's `p18g0/check.sh` is not a gate set).
  - **Order:** strictly serial: build the before side, then its `--build-only` into a fresh `p18h/before/mut-options`, then the same for the after side, all under `nice -n 19`.
  - **`gate.sh`** checks that both runs logged exit 0, then runs `compare.py`.
- **Negative checks** run on `cp -al` copies whose edited files are replaced, never written through. Each must fail:
  - `OPTIONS.DLL` missing on both sides;
  - one changed unit's replay entry removed on both sides;
  - one mutant case removed on both sides;
  - one case's `<name>.exe` removed on both sides;
  - one case directory renamed to an id not in `cases.txt` on both sides;
  - a stray second `--mut`.
- **Acceptance:** the 859 mutants must be identical, with no fallback. `GATE PASS` is required unless the user accepts an explained difference in an ordinary artifact. That fallback is:
  - `tools/ofs optionseventsdiff-run` at its documented baselines (normal N=20000: 9,860,000 comparisons, 0 differ; and its unwind baseline);
  - `main-dialogs`, which opens 23 OPTIONS dialogs;
  - the 74 flights with the same command as slice 5's spec.
  - All run serially from a frozen copy.
- **Builds:** `nice -n 19`, `-j 2`, `CMAKE_BUILD_PARALLEL_LEVEL=2`.

**Never:**
- No other module is converted.
- No change to what OPTIONS computes or exports, or to any break's mutation.
- The plain `(T *)(uintptr_t)x` casts are left alone: `options_graphs.c:56`, `options_demo.c:89,118`, `options_keys.c:337`, `options_runtime.c:363`, `options_controls.c:349-351`. The lint does not ban them, and they are logged in `deferred-work.md` for Epic 5.
- The 859/17 count in `docs/tools.md:273` is not re-run under wine. The identical mutants stand in for that.
- No commits or staging.

</frozen-after-approval>

## Review Triage Log

Spec review (Codex gpt-6.2-sol, 2026-11-25), before approval:

| # | Finding | Verdict | Evidence / action |
|---|---|---|---|
| 1 | `-Iinclude` changes 16 replay entries, not six | blocker | Confirmed by the recipes: all 16 are allowed one trailing `-Iinclude`, in the same position in both recipes. |
| 2 | A mutant can vanish, or be swapped with another case, and still pass | high | Case ids pinned in `cases.txt`, `<name>.exe` required, and two negative checks added. |
| 3 | Build-only could build a different mutation from the one wine runs | high | Shared code with `one()`, a legacy-trace parity check, and a per-anchor commutation check. |
| 4 | Kept PEs are trusted through manifests | false | General distrust of the gate's own evidence; the manifests are written from the files by `build_side.sh` (rejected in slices 3–6a). |
| 5 | The fallback target is wrong and coverage unclear | high | Now `optionseventsdiff-run` at its documented baselines, plus `main-dialogs` and the flights; the user accepts any fallback. |
| 6 | The lint delta is not enforced | medium | `gate.sh` asserts the six rows and the reverse delta. |
| 7 | Reproducibility: baseline, root, `-B`, regression commands | medium | Specified. |
| 8 | Cast type and operand extent | low | Specified. |

## Code Map

- **`src/options/`:**
  - Cast sites: book `119,120,198,199`; demo `104,105`; keys `105,106,136,281,293,338,369,377,411,423,451,455`; logbook `170,266,320,322,330`; replay `171,178`; videos `300,301`.
  - Helpers: `PTR`, `AT`, `D32`, `SERVICES`, `BLOCK`, `LIST`, `WHOLE`, `PART` and `CHOSEN`. None collides with an `arena.h` name.
  - `options_internal.h` includes only `<stdint.h>`.
- **`tools/options_breaks.py`** (1166 lines):
  - **Cases:** 15 lists. The unit lists are `(name, before, after)` and UNWIND is `(name, unit, before, after)`. `main()` expands them to `(name, unit, mode, before, after)`, 859 in all.
  - **One case** (`one()`, about `:1094-1130`): it requires each anchor once, writes `bin/options-diff/breaks/NAME.c`, then compiles `optionseventsdiff.c`, the mutant and the other 13 units with `-Isrc/options -Wno-unused`. The verdict is exit 1 plus `DIFFER scenario=`.
  - **Pool:** there is a pool of worker copies (`w.root`). Build-only does not use it.
- **`tools/break_build_only.py`:** 6b0's `build_only(argv, cases, inputs)` and `Case`, with `tools/flights_breaks.py` as the closest example (the same harness-plus-units shape).
- **The gate:** `~/openfs98-verify/gate/compare.py`. The `p18d` block is the pattern for allowing `-Iinclude` on replayed units; the `p18f` block is the pattern for one pinned `--mut`.
- **Cost:** about 3 s per mutant, so about 45 min per side.

## Tasks & Acceptance

**Execution:**
- [ ] `tools/options_breaks.py` -- step 1, `--build-only` -- the before overlay and the mutant set.
- [ ] The gate: `--require-set p18h` and `p18h/gate.sh`. Build the before snapshot and its mutant set.
- [ ] `src/options/options_{book,demo,keys,logbook,replay,videos}.c` -- convert.
- [ ] Recipes: `cmake/steps/all.cmake:373`, `cmake/steps/difftests.cmake:342` and the runner argv get `-Iinclude`; then step 2's anchor repair.
- [ ] `tools/arena_lint_allowed.txt` -- delete the six rows.
- [ ] Docs:
  - `docs/modules/options.md`: the `--build-only` usage;
  - `deferred-work.md`: the plain casts.
- [ ] Build the after snapshot and its mutant set; run the gate and the four negative checks; rerun p18b to p18g.
- [ ] This spec: `## Implementation Notes` covering the counts, the gate result, the negative checks and the earlier gates.

**Acceptance Criteria:**
- **Lint:** given the slice, when `check_arena.py --self-test`, the lint, and the lint with `--prior` run, then all pass with the six OPTIONS rows gone and reverse casts down by exactly 4.
- **The runner:** given `options_breaks.py --build-only DIR` on either snapshot, it exits 0 with 859 case directories, and the tree's hash inventory is unchanged.
- **The gate:** given `--require-set p18h`, it prints `GATE PASS`, with all 859 mutants identical. All four negative checks fail, and p18b to p18g still pass.

## Verification

**Commands:**
- `python3 -B tools/check_arena.py --self-test`, `python3 -B tools/check_arena.py`, `python3 -B tools/check_arena.py --prior ~/openfs98-verify/p18h/prior.txt` -- expected: pass.
- `bash ~/openfs98-verify/p18h/gate.sh` -- expected: `GATE PASS`; earlier sets -- expected: `GATE PASS`.
