---
title: 'Story 1.4, slice 17: SIM1 data decompiled'
type: 'refactor'
created: '2026-11-21'
status: 'done'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43' # worktree held uncommitted Stories 1.2-1.4; file snapshot at ~/openfs98-verify/baseline-1-4q
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4p-host-data.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** two build steps extract SIM1's data from the CD's `SIM1.sim` into untracked `bin/`, and the C includes the result:
- **`tools/gen_sim1_native.py`** runs in the `bin/SIM1.sim` step (`cmake/steps/all.cmake:421`). It writes:
  - `bin/sim1-native/sim1_data.inc`: retail's `.data` from +0x94 to its 0x1200 raw bytes;
  - `bin/sim1-native/sim1_rdata.inc`: retail's 0x759-byte `.rdata`;
  - `bin/sim1-native/sim1.res`: retail's resources.

  `src/sim1/sim1_native.c:47,59` includes the two `.inc` files.
- **`tools/gen_sim1_data.py`** runs in the `sim1airdiff.exe` difftests step (`cmake/steps/difftests.cmake:348`). It verifies the CD's `SIM1.sim` sha256, copies it to `orig/Sim/SIM1.sim`, and writes `.data` from +0x94 to `bin/sim1-diff/sim1_data.inc` plus `survey.json`. Its slice runs to +0x1204: four zero-fill bytes more than `gen_sim1_native.py`'s 0x116c. As initialised members the two are equal. `src/sim1/sim1.c:43` includes that file (through `-Ibin/sim1-diff`) for the harness DLLs.

SIM1 has no manifest row. A trial row, `SIM1 Sim/SIM1.sim bin/SIM1.sim rdata=0x15000:0x759`, passes today: the 0x43f8-byte `.data` is identical (11 relocated, all 11 ours: the Linkage's eleven pointers, nine to code and two to +0x1150/+0x1158), and the `.rdata` window is identical.

**Approach:**
- **One committed image.** `src/sim1/sim1_data.inc` and `src/sim1/sim1_rdata.inc` become committed typed C, in the style of slices 13-16.
  - They are found before the `bin/` copies: a quoted include searches the including file's directory first. So `sim1.c` and `sim1_native.c` need no change, and both builds use the same source.
  - `sim1_data.inc` initialises the `data` member, which starts at `.data`+0x94. Its `AT()` therefore takes retail's `.data` offset and subtracts 0x94, so every comment and `AT()` shows retail's offset.
  - Regions are named and typed from `docs/modules/sim1.md`:
    - the flags at +0x7c0;
    - the conversion constants at +0x690 and +0x720;
    - the rest of the file-backed bytes (+0x94..+0x1200) as typed values marked "meaning not yet known".

    Regions past +0x1200 are zero-filled by the loader and have no initialiser bytes. They are documented in a closing comment, not initialised: the serialized state layout (`sim1.md:173`), the publication block at +0x1730 (0x24 bytes, then four words making the 0x2c-byte descriptor), the flag at +0x1778, and the restore dword at +0x1c80 (`uint32_t`, `sim1_state.inc:141`).

    `.rdata` holds the constants RF64 reads; doubles are written as typed values with hex floats in comments, as in slice 12.
- **`gen_sim1_native.py`** keeps only the resource extraction (`sim1.res`) and its layout and relocation asserts. It moves from `gen:` to `resource:`.
- **`gen_sim1_data.py`** stops writing `sim1_data.inc` and leaves the difftests step. It becomes a setup check, `tools/ofs sim1-reference`, which verifies the CD's sha256, copies it to `orig/Sim/SIM1.sim` and writes `survey.json`. It moves from `gen:` to `oracle:`. `docs/building.md` says to run it once when setting up `orig/`, before the first build: the native step's resource extraction and the harness runs read `orig/Sim/SIM1.sim`. Its place in the difftests step becomes `mkdir -p bin/sim1-diff`, since it was what created that directory before the first harness link.
- **The manifest row.** `SIM1 Sim/SIM1.sim bin/SIM1.sim rdata=0x15000:0x759`, written with `UPDATE=1`, plus a SIM1 `--hash` line in the `bin/SIM1.sim` step.

**Gate:** the following are identical to their pre-slice builds after normalizing with `peh2.py`:
- `bin/SIM1.sim` (21306e24da974a3b, equal to s14);
- `bin/sim1-diff/SIM1_PARTIAL.DLL`, `SIM1_STATE_TEST.DLL` and `SIM1_AIR_TEST.DLL`;
- `bin/sim1-diff/sim1rdiff.exe`.

The three test DLLs date from 2026-11-14 and may be stale. Where a saved copy is not reproducible, build it from `~/openfs98-verify/baseline-1-4q` with the same command, compare with that, and say so. `data-images` must pass with 27 rows. If code changes, report the per-function diff and stop.

**Queued until Story 1.3's Baselines run finishes:** `sim1diff-run`, `sim1rdiff-run`, `sim1-breaks`, `sim1r-breaks`, `sim1air-run`, `sim1air-breaks`, `sim1state-run`, `sim1state-breaks` and `sim1native-flights`.

## Boundaries & Constraints

**Always:**
- `struct sim1_state` and `struct sim1_native_image` keep their layout and the `typedef char …_layout_check` asserts.
- The Linkage initialisers in `sim1.c:37-41` and `sim1_native.c:40-44` stay as they are.
- `sim1_rdata` stays a `const uint8_t[SIM1_RDATA_SIZE]`.
- Carry the generators' asserts into comments or `_Static_assert`s: the base 0x22000000, the section sizes 0x759/0x43f8/0x1200, the ImportTable ids, the 15 Linkage words, the zero constant at 0x152e8, the scale at 0x152f0, the reference sha256, and the fact that `.data`'s only pointers are the Linkage's.
- The text the SIM1 break runners patch stays as it is.
- Check (a) must flag none of the new C.
- Never `rm -rf bin`. No staging or commits. Record the files in `~/openfs98-verify/story-files.md`.

**Never:**
- Don't change any behaviour or touch another module.
- Don't do the heavy runs on the queued list.

## I/O & Edge-Case Matrix

| Scenario | Input | Expected |
|---|---|---|
| Row passes | the SIM1 row | `data-images` passes; it prints 11 relocated, 11 ours |
| Data byte | one value in `src/sim1/sim1_data.inc` changed | `data-images` fails against retail; the build's SIM1 `--hash` step fails |
| Rdata byte | one value in `src/sim1/sim1_rdata.inc` changed | the `rdata=` check fails, and so does `--hash` |
| Clean build | `bin/sim1-native/*.inc` and `bin/sim1-diff/sim1_data.inc` deleted | the build and the harness builds succeed from the committed files |

</frozen-after-approval>

## Code Map

- `tools/gen_sim1_native.py` -- the relocation and layout asserts, `resources()`, the two dump writers to remove.
- `tools/gen_sim1_data.py` -- the CD sha256, the copy to `orig/`, the ImportTable/Linkage/constant asserts, the `.inc` and `survey.json` writers.
- `src/sim1/sim1.c:30-50` -- `sim1_image`, its Linkage, the include and the layout check. `src/sim1/sim1_native.c:35-62` -- the same for the native image, `sim1_rdata`, `_ImportTable`/`_Linkage`.
- `docs/modules/sim1.md:22-60,153-245,318-360,570-590` -- the layout and named offsets.
- `cmake/steps/all.cmake:416-425` -- the `bin/SIM1.sim` step. `cmake/steps/difftests.cmake:345-370` -- the sim1 harness steps (`-Ibin/sim1-diff`). `cmake/targets.cmake:88-110,1805-1905` -- the sim1 targets that build the test DLLs. Add `sim1-reference`.
- `tools/check_data_image.py` (no new mode); `tools/data_images.txt`; `tools/raw_dumps_allowed.txt:18-19`.
- `docs/tools.md` (the `tools/ofs sim1` line and the Baselines lines), `docs/building.md` (setting up `orig/Sim/SIM1.sim`), `docs/modules/sim1.md:584-590`, `docs/implementation/deferred-work.md` (close slice 17; queue the runs).

## Tasks & Acceptance

**Execution:**
- [x] `src/sim1/sim1_data.inc`, `src/sim1/sim1_rdata.inc` (new, committed) -- the typed image, written by a scratch script outside the repo; host compiles must equal the generators' initialised members: retail's raw bytes +0x94..+0x1200, the rest zero.
- [x] `tools/gen_sim1_native.py` -- resources only.
- [x] `tools/gen_sim1_data.py`, `cmake/steps/difftests.cmake`, `cmake/targets.cmake` -- drop the `.inc` writer; replace the difftests line with `mkdir -p bin/sim1-diff`; add the `sim1-reference` target; add build-only targets `sim1state-build` and `sim1air-build` with the STATE and AIR DLL compile commands that today exist only inside `sim1state-run`/`-breaks` and `sim1air-run`/`-breaks` (`targets.cmake:1859-1899`), and have those run targets use them.
- [x] `tools/data_images.txt`, `cmake/steps/all.cmake`, `tools/raw_dumps_allowed.txt` -- the row, the `--hash` line, the two allowlist moves.
- [x] `tools/check_data_image.py` self-test -- one case each for the data and rdata rows of the matrix, if the existing generic cases don't already cover a new row.
- [x] The docs in the Code Map, and `~/openfs98-verify/story-files.md`.

**Acceptance Criteria:**
- Given the slice is built, when the five gate binaries are normalized with `peh2.py`, then each equals its pre-slice hash (copies in the scratchpad `pre17/`: `SIM1_PARTIAL.DLL` 93920e4f0b6c16b8, `SIM1_STATE_TEST.DLL` 4f7ef0cc93942a08, `SIM1_AIR_TEST.DLL` 3309dd17579047f3, `sim1rdiff.exe` 5c673d738c8dd3e3) or a baseline rebuild's.
- Given the slice is built, when `tools/ofs data-images` runs, then 27 rows and the self-test pass.
- Given the slice is built, when `check_raw_dumps.py` runs, then it reports 8 `gen`, 11 `resource` and 18 `oracle` scripts.

## Implementation Notes

- **Hashes (peh2.py):** `bin/SIM1.sim` 21306e24da974a3b, equal to `pre17/` and s14; `sim1rdiff.exe` 5c673d738c8dd3e3, equal to `pre17/`. The three test DLLs were stale in `pre17/`: rebuilt from `baseline-1-4q`, they give `SIM1_PARTIAL.DLL` 5906fc5cfa22eae2, `SIM1_STATE_TEST.DLL` 3ebb9615940f0791 and `SIM1_AIR_TEST.DLL` 7e34d73a5cb4e08c, equal to the new builds.
- **Clean build:** the old `bin/sim1-native/*.inc` and `bin/sim1-diff/sim1_data.inc` were moved to the scratchpad `oldinc/`; the build and the harness DLLs built from the committed files.
- **Checks:** `data-images`: 27 images, 10 `.rdata` ranges; SIM1 11 relocated, 11 ours. Dumps: `2 allowed dump files, 37 scripts reading orig/ (8 gen, 11 resource, 18 oracle)`.

## Spec Change Log

## Review Triage Log

Code review, pass 1 (Codex gpt-6.1-sol, medium; Blind Hunter 10, Edge Case Hunter none, Verification Gap none). Also the orchestrator's matrix audit: no self-test ran for the "Data byte" and "Rdata byte" rows.

| # | Layer | Finding | Verdict | Evidence | Route |
|---|---|---|---|---|---|
| 0 | audit | The matrix's data and rdata rows have no self-test that runs | medium | the implementer mutated by hand only | patch |
| 1 | blind | `sim1_table_lookup()`'s interpolation tables are anonymous | medium | callers name +0x138, +0xb60, +0xc08 | patch |
| 2 | blind | Words typed as doubles from their bit patterns alone | medium | e.g. 0x4000400040004000 shown as 2.03; misleading | patch |
| 3 | blind | Export names in `.rdata` are dwords | low | direct correction | patch |
| 4 | blind | Named `.rdata` constants lack consumer comments | low | the code already names them | patch |
| 5 | blind | `sim1state-build`/`sim1air-build` depend on all of `difftests` | low | the same as the existing `sim1-partial`; no harm named | reject |
| 6 | blind | `docs/tools.md` lists the targets with a comma | low | the wrapper reads it as part of the name | patch |
| 7 | blind | `docs/tools.md` Baselines lines not updated | medium | the Baselines rule needs current numbers | patch |
| 8 | blind | `docs/modules/sim1.md:63` misdescribes the harness image | low | it includes only `sim1_data.inc`; `.data`'s only pointers are the Linkage's | patch |
| 9 | blind | The spec records no observed results | low | Implementation Notes were empty; the orchestrator fills them | reject (spec edit) |
| 10 | blind | The gate's `peh2.py` is a private scratchpad tool | medium | every slice since 10 relies on it | defer |

## Verification

**Commands:**
- `cmake --build build/win32-retail` -- expected: success, with the SIM1 `--hash` line passing.
- `tools/ofs sim1-partial`, `sim1state-build`, `sim1air-build` (build only) -- expected: the three test DLLs built.
- `tools/ofs data-images` -- expected: 27 rows; the self-test passes.
- `python3 <scratchpad>/peh2.py` on the five gate binaries -- expected: equal to `pre17/` or the baseline rebuild.
- `python3 -B tools/check_raw_dumps.py` -- expected: 8 gen, 11 resource, 18 oracle.
