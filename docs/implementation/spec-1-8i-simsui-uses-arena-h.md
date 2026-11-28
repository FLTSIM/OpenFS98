---
title: 'Story 1.8, slice 6d: SIMSUI uses arena.h'
type: 'refactor'
created: '2026-11-25'
status: 'in-progress'
baseline_commit: '3dae7cd27375a77f9ff2bd202015373fae183d0a'
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-1-8h-options-uses-arena-h.md', '{project-root}/OpenFS98/docs/implementation/spec-1-8g-break-runners-share-build-only.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** SIMSUI still holds 11 banned `(uint32_t)(uintptr_t)` casts (on 10 lines) in five files (`tools/arena_lint_allowed.txt` rows `src/simsui/simsui_{dib 2, dibsection 2, edit 2, native 4, shadow 1}.c`, no defines).

**Approach:**
- Convert them to `include/fs98/arena.h`, as slices 1–6c did, and delete the five rows.
- Give `tools/simsui_breaks.py` a `--build-only` mode on slice 6b0's shared helper.
- Prove the change by byte identity with the packaged gate:
  - SIMSUI's objects, byte for byte;
  - the DLL and `simsuidiff.exe`, after PE normalisation;
  - every one of the runner's 708 mutants.

**Order:** implemented after 6c is in the main tree. It uses the same helper and gate support.

## Boundaries & Constraints

**Always:**
- **The cast sites:**
  - `dib.c:137,138`, `dibsection.c:67,96`, `edit.c:140` (two casts on one line) and `shadow.c:51`;
  - the three macro bodies `SIMSUI_RELOC_DATA`, `SIMSUI_RELOC_CODE` and `SIMSUI_RELOC_IMPORT` (`native.c:76-78`);
  - `native.c:124`.
  - Every one becomes `fs98_handle(x)`, and each changed file includes `fs98/arena.h`.
- **No data image:** SIMSUI defines no `FS98_DATA_IMAGE`. Its data is retail's window, reached through the biased pointer `fs98_simsui_image` and `IMAGE`/`G32`/`G_PTR` (`simsui_internal.h:40-45`), and those stay. So does `native.c:75` (`(uintptr_t)fs98_simsui_block - SIMSUI_IMAGE_START`).
- **Lint:** reverse casts are unchanged (SIMSUI has none). The five rows go, checked with `--prior` on the list at this slice's baseline.
- **`-Iinclude`** is added, as the last compile flag (so the 24 shared units replay once), to:
  - the per-unit compile at `cmake/steps/all.cmake:382`;
  - `simsui_native.c` at `:390`;
  - `simsuidiff.exe` at `cmake/steps/difftests.cmake:389`;
  - the mutant argv in `tools/simsui_breaks.py`.
  - `simsui_image.c` (`:392`) is unchanged. `check_data_image.py --hash SIMSUI` (`:399`) and its `SIMSUI_RELOC_` regex (`check_data_image.py:340`) must still pass.
- **The runner change, in two steps:**
  - **Step 1** adds `--build-only DIR [CASE...]` through 6b0's helper and changes nothing else.
    - Case ids are `<unit>.<name>`: 708 cases in 23 units (`simsui_textart.c` has none). Each case directory holds exactly the built `<name>.exe` and `meta.json`.
    - The mutation text and the argv come from the code the wine path uses.
    - Parity with the legacy path: the baseline's runner in wine mode, with a fake `wine` that exits 0 and 6b0's gcc trace wrapper, gives the legacy compile trace. Step 1's `--build-only` trace must equal it after path normalisation.
    - The ordered 708 ids from the baseline runner are saved as `p18i/cases.txt`.
  - **Step 2** repairs anchor `dib-valid-low` (`if ((uint32_t)(uintptr_t)pointer < bits)`), both its strings, to the converted text. Its mutation, `<` to `<=`, stays the same. A check shows that old mutation then conversion gives the same text as conversion then the repaired mutation.
  - **Wine mode** is unchanged, apart from `-Iinclude`.
- **The gate:** `compare.py --require-set p18i` requires:
  - `SIMSUI.DLL` and `simsui-diff/simsuidiff.exe`, identical after PE normalisation;
  - the 27 objects in `simsui-native/`, the 24 units, `simsui_native.o`, `simsui_image.o` and `simsui-res.o`, each byte-identical;
  - the 26 replayed units (24 units, `simsui_native.c`, `simsuidiff.c`) replayed exactly once per side, each with exactly one trailing `-Iinclude` added on the after side and the other flags equal; `simsui_image.c` replayed unchanged;
  - exactly one `--mut` pair, `mut-simsui`: its case directories exactly the ids in `p18i/cases.txt` on both sides, each holding `<name>.exe` and a `meta.json` whose name is its directory, with every `<name>.exe` identical after normalisation;
  - the lint delta: the after allowlist is the prior minus exactly the five rows.
  - After the `compare.py` change, p18b to p18h still pass.
- **Snapshots** go at `~/openfs98-verify/p18i`:
  - before is `git archive` of the baseline plus `orig/`, with step 1's runner overlaid (the only overlay, recorded in the notes);
  - after is the working tree plus `orig/`;
  - both are built at one absolute root by `build_side.sh`, with the prior at `p18i/prior.txt`. Each side's mutants are built from its populated root before the root is recreated, with `python3 -B` and DIR outside the root. The baseline commit and gate revision are recorded. The order is strictly serial: the before side, then its `--build-only` into a fresh `p18i/before/mut-simsui`, then the same for the after side, under `nice -n 19`;
  - `gate.sh` checks the run logs, then runs `compare.py`.
- **Negative checks** on `cp -al` copies whose edited files are replaced, never written through. Each must fail:
  - `SIMSUI.DLL` missing on both sides;
  - one object missing on both sides;
  - one changed unit's replay entry removed on both sides;
  - one mutant case removed on both sides;
  - one case's `<name>.exe` removed on both sides;
  - one case directory renamed to an id not in `cases.txt` on both sides;
  - a stray second `--mut`.
- **The acceptance rule:**
  - The 708 mutants must be identical, with no fallback.
  - `GATE PASS` is required unless the user accepts an explained difference in an ordinary artifact. That fallback is:
    - `tools/ofs simsuidiff-run` at its documented N=2000 (444 operations, 0 differ). It does not cover `simsui_native.c`, so a difference in `SIMSUI.DLL` itself also needs the next two;
    - `main-dialogs` and `flights-play` with each side's `bin/SIMSUI.DLL` installed beside `FLTSIM98.EXE` (SHARED_MODULES), its hash checked in the installed copy before each run;
    - all run serially from a frozen copy.
- Builds run under `nice -n 19`, `-j 2`, `CMAKE_BUILD_PARALLEL_LEVEL=2`.

**Never:**
- No other module is converted.
- No change to what SIMSUI computes or exports, or to its data window or relocation tables.
- No change to any break's mutation.
- The plain pointer casts through `uintptr_t` (24 explicit, plus 14 `I32((uintptr_t)…)` arguments; the image bias at `native.c:75` is separate, including those in anchors `draw-length`, `combo-compare-sign`, `error-mark`, `char-limit`, `char-sign-empty` and `text-get-length`) are left alone and logged in `deferred-work.md` for Epic 5.
- The 708/698 counts in `docs/tools.md:276` are not re-run under wine.
- No commits or staging.

**Decided (2026-11-27, user):** the p18i gate applies the hardening from the 6b and 6c code reviews (reuse `gate/pecheck.py`, the p18g/p18h `bin/` walk and `--trees` binding): both kept `bin/` trees are required and rehashed, and the saved digests must agree with the files; every PE is validated; each case directory holds exactly `<name>.exe` and `meta.json` as regular files, with the completed-pair count reported; each kept mutant exe is bound to its side's compile record (case id, mutated-source hash, input hashes, normalised output hash) and `cases.txt` equals the baseline runner's ordered ids; every replay object is non-empty valid COFF and tied to its logged recipe; a file inventory of the populated root is taken just before and after each mutant run and any change fails; `gate-revision.txt` records the gate scripts' sha256. The negative checks add 6c's set: a case replaced by a file, a symmetric swap of two mutants' exes, a changed retained `SIMSUI.DLL`, deleted `bin/` trees, symmetric empty replay objects and a swapped pair in `cases.txt`; each must exit 1 with `GATE FAIL` and its diagnostic, and the unmodified control must pass. Earlier sets to rerun are p18b to p18h, 4b `--strict` and p110b1 if present.

</frozen-after-approval>

## Review Triage Log

Spec review (Codex gpt-6.2-sol, 2026-11-25), before approval:

| # | Finding | Verdict | Evidence / action |
|---|---|---|---|
| 1 | `-Iinclude` changes 26 replay entries, not five | blocker | Confirmed by the recipes: all 26 are allowed one trailing `-Iinclude`. |
| 2 | Kept PEs are trusted through manifests | false | General distrust of the gate's own evidence (rejected in slices 3–6a). |
| 3 | The mutant inventory and legacy parity are unproven | high | `cases.txt` pinned, `<name>.exe` required, legacy-trace parity, and negative checks. |
| 4 | The fallback misses `simsui_native.c` and how SIMSUI is loaded | high | `main-dialogs` and `flights-play` with each side's DLL installed and hash-checked; the user accepts any fallback. |
| 5 | The object glob matches 27; raw vs normalised identity | medium | 27 objects pinned; raw objects and normalised PEs; deletions on both sides. |
| 6 | Same root, prior, `-B` | medium | Specified. |
| 7 | 11 casts on 10 lines; 708 cases in 23 units | low | Corrected. |
| 8 | The generator description was wrong | low | Corrected. |

## Code Map

- **`src/simsui/`:**
  - 24 units in `sources.txt`, plus `simsui_native.c`.
  - `simsui_image.c`, `simsui_image.inc` and `simsui_native_tables.inc` are committed. `tools/gen_simsui_native.py` classifies retail relocation targets and uses object symbols for export names.
  - The hash check's regex reads the table rows, not the macro bodies: the gate's object identity covers those.
  - `simsui_internal.h` defines `TC SC D32 I32 D16 D8 PTR AT IMAGE G32 G_PTR CALL_*` and `mfc_*`, none of which collides with `arena.h`.
  - Nothing includes `arena.h` yet.
- **Recipes:**
  - `cmake/steps/all.cmake:380-399`: per-unit objects, `simsui_native.o`, the image, the link to `SIMSUI_ours.dll`, the copy to `bin/SIMSUI.DLL`, and the hash check;
  - `cmake/steps/difftests.cmake:389`: `simsuidiff.exe` from `@SIMSUI_SOURCES@`;
  - `cmake/OpenFS98.cmake:8-34` expands the source list.
- **`tools/simsui_breaks.py`** (864 lines):
  - `BREAKS[unit]` maps each unit to a list of `(name, before, after)` tuples: 708 cases in 24 units.
  - Each case requires its anchor once, writes the mutant to `bin/simsui-diff/breaks/<name>.c`, and builds `simsuidiff.c` with the mutant and the other 23 units. The flags are `-Isrc/simsui -Wno-unused -fno-tree-loop-distribute-patterns -fno-tree-vectorize -Wl,--image-base=0x400000`.
  - The verdict is exit 1 plus `DIFFER scenario=`. Runs go through `tools/run_simsuidiff.sh`.
- **Shared code:** 6b0's helper in `tools/break_build_only.py`, and 6c's `options_breaks.py` step 1 as the closest example.
- **Cost:** about 4 s per mutant, so about 50 min per side.

## Tasks & Acceptance

**Execution:**
- [ ] `tools/simsui_breaks.py` -- step 1, `--build-only` -- the before overlay and the mutant set.
- [ ] Gate: `--require-set p18i` and `p18i/gate.sh`. Build the before snapshot and its mutant set.
- [ ] `src/simsui/simsui_{dib,dibsection,edit,native,shadow}.c` -- convert.
- [ ] Recipes (`all.cmake:382,390`, `difftests.cmake:389`, the runner argv) get `-Iinclude`; then step 2's anchor repair.
- [ ] `tools/arena_lint_allowed.txt` -- delete the five rows.
- [ ] Docs:
  - `docs/modules/simsui.md`: the `--build-only` usage;
  - `deferred-work.md`: the plain casts.
- [ ] Build the after snapshot and its mutant set; run the gate, the four negative checks, and p18b to p18h.
- [ ] This spec: `## Implementation Notes`.

**Acceptance Criteria:**
- **The lint:** given the slice, when the lint self-test, the lint, and the lint with `--prior` run, then all pass with no SIMSUI row, and `check_data_image.py --hash SIMSUI` passes.
- **The runner:** given `simsui_breaks.py --build-only DIR` on either snapshot, it exits 0 with 708 case directories, and the tree's hash inventory is unchanged.
- **The gate:** given `--require-set p18i`, it prints `GATE PASS`, with the 25 objects and all 708 mutants identical. All four negative checks fail, and p18b to p18h still pass.

## Verification

**Commands:**
- `python3 -B tools/check_arena.py --self-test`, `python3 -B tools/check_arena.py`, `python3 -B tools/check_arena.py --prior ~/openfs98-verify/p18i/prior.txt` -- expected: pass.
- `bash ~/openfs98-verify/p18i/gate.sh` -- expected: `GATE PASS`; earlier sets -- expected: `GATE PASS`.
