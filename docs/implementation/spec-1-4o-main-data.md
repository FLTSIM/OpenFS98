---
title: 'Story 1.4, slice 15: MAIN data decompiled'
type: 'refactor'
created: '2026-11-21'
status: 'done'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43' # worktree held uncommitted Stories 1.2-1.4; file snapshot at ~/openfs98-verify/baseline-1-4o
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4n-g3d-data-strict-mixed.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** MAIN still has three allowlisted dumps.
- `tools/gen_main_data.py` reads `orig/Modules/MAIN.DLL` at build time and writes:
  - `src/main/main_data.inc`: the 0x2ee00-byte `.data` image, 0x8000 of it initialised;
  - `src/main/main_pointers.inc`: 455 `MAIN_DATA_POINTER`, 92 `MAIN_CODE_TARGET` and 51 `MAIN_LINKAGE_RETAIL` entries;
  - `src/main/main_text_tail.inc`: retail's 0xbf6d bytes from 0x207f4093 up to `.data`.

  The text tail is the season table (355, 81, 172, 264) followed by retail's own code and `.rdata`. `set_season()` (`main_situation.inc:639`) indexes the season table by GLOBAL `+0x248` without a bound and reads on through those bytes (MAIN-6 and MAIN-18 in `docs/retail-bugs.md`). The daylight table at 0x207f8eb0 (five `{start, end, offset}` words; `main_season.inc:24,105-109`) is also in the tail, read with a bounded index. It is exported as `main_text_tail` for the harness.
- `src/main/main_start.inc` is hand-written C. Check (a) flags it only because of `menu_state()`'s menu-id lists (lines 39-49).
- MAIN has no manifest row.

**Approach:**
- **The image.** `src/main/main_image.inc` is committed typed C, in the style of slices 13 and 14, replacing `main_data.inc`:
  - `AT()`, one typed value per macro, strings as literals;
  - tables written by their known structure, from `docs/modules/main.md` (for example the 0x12-byte situation records at +0x4456), with region comments. These regions at least are named and typed:
    - the situation records (+0x4456, 77 records);
    - the two month-length tables at +0x2c8 and +0x2e0 (12 words each), read in `main_clockdlg.inc`;
    - the season boundaries at +0x2f8 (five words, ending 0xffff), also read in `main_clockdlg.inc`;
    - the five-word boundary table at +0x6e10, read in `main_start.inc`;
  - typed values marked "meaning not yet known" where the structure isn't known;
  - the 133 raw code addresses kept as raw `U32`;
  - the 455 data pointers and 47 Linkage slots kept zero.
- **The pointer list.** `main_pointers.inc` becomes committed source with its header reworded.
  - `ptrs=` learns `MAIN_DATA_POINTER`.
  - `MAIN_CODE_TARGET` gets the same set-equality, duplicate and non-empty rules as `G3D_CODE_TARGET`.
  - `MAIN_LINKAGE_RETAIL(at, v)` lines must be exactly 51, at ascending offsets 0xc0..0x188 in steps of 4, with no duplicates, and each `v` must equal retail's raw word at `at`. `main.c:276` uses only `v`, in order. They are metadata, not installed slots, so they don't count against `cset=` or `ptrs=` overlap.
- **The manifest row.** `MAIN … --mixed ptrs=src/main/main_pointers.inc cset=0xc4..0xc8,0xd8..0x188`, written with `UPDATE=1`, plus a MAIN `--hash` line in `cmake/steps/all.cmake`.
- **`main_start.inc`.** The menu-id lists are regrouped one menu per group, each group under its own comment line naming the menu. Check (a) strips comments, so trailing comments would not clear it, but a comment-only line ends a run. The last line of `clear_all`, `0x1625, 0x1520, 0x1180, 0x1181, 0x1304, 0x1303,`, stays as one line under a "remaining items, retail order" comment, because `main_breaks.py`'s `all-order` break (`:1250`) matches it. Values, order and every string `tools/main_breaks.py` matches stay the same, each present exactly once. Both dump rules must pass, and the allowlist line goes.
- **The text tail (decided 2026-11-21: keep, labelled and guarded).**
  - The four season words and the five daylight records become typed `U16`s at their offsets in `text_tail`.
  - The rest stays in a committed `main_text_tail.inc`. It keeps its allowlist line with the reason "retail's own code and .rdata around the season and daylight tables; MAIN-6/MAIN-18's unbounded season index reads through it; goes when the index is bounded at porting".
  - A new manifest field, `tail=SYMBOL:VA:LEN`, makes `data-images` compare our symbol's bytes with retail's mapped image (`.text`, the gap and `.rdata`) from VA. `--hash` hashes the window.
  - The `text_tail` array, its size and its export are unchanged.
- **Removed:** `gen_main_data.py`, `main_data.inc`, their allowlist lines, and the generator line in `all.cmake`. `gen_main_bitmap.py` (a resource script) stays.

**Gate:**
- `bin/MAIN.DLL` is identical to `~/openfs98-verify/s14/bin` and to the pre-slice copy after normalizing with `peh2.py`.
- `bin/difftest/maindiff.exe` is identical to its pre-slice build.
- `data-images` passes with 25 rows.

If code changes, report the per-function diff and stop.

**Queued until Story 1.3's Baselines run finishes:** `maindiff-run`, `main-breaks` (from a frozen copy), `main-smoke` and `main-dialogs`.

## Boundaries & Constraints

**Always:**
- `main.c`'s `D`/`V()`/`M*` access stays as it is, as do the `MAIN_*` macro uses at `main.c:259-280` and the `text_tail` export.
- The text that `tools/main_breaks.py` patches stays as it is.
- Carry the generator's asserts into comments or `_Static_assert`s: the ImportTable ids, module id 3, the 455/180 counts, the season days, and the section addresses.
- Check (a) must flag none of the new C.
- Never `rm -rf bin`. No staging or commits. Record the files in `~/openfs98-verify/story-files.md`.

**Never:**
- Don't change any behaviour or touch another module.
- Don't bound the season index: MAIN-6 and MAIN-18 are kept bugs.
- Don't do the heavy runs on the queued list.

## I/O & Edge-Case Matrix

| Scenario | Input | Expected |
|---|---|---|
| Row passes | the MAIN row | `data-images` passes and prints 455 listed, 47 set in C and 133 kept |
| Linkage drift | a `MAIN_LINKAGE_RETAIL` value changed, a line removed, repeated or out of order | fails |
| Code targets | a `MAIN_CODE_TARGET` line removed, added or repeated, or all of them removed | fails |
| Image byte | one value in `main_image.inc` changed | the build's MAIN `--hash` step fails |
| Tail drift | one byte of `main_text_tail.inc` or a season word changed | `data-images` fails against retail, and the build's `--hash` step fails |

</frozen-after-approval>

## Code Map

- `tools/gen_main_data.py` -- the asserts, the relocation split and the tail extraction (`TAIL_VA` 0x207f4093).
- `src/main/main.c:255-282` -- the image initialiser and the two `#include "main_pointers.inc"` uses (data pointers; retail Linkage values).
- `src/main/main_situation.inc:84-95` -- the `text_tail` export. `src/main/main.def:5` -- `main_text_tail DATA`.
- `src/main/main_start.inc:36-60` -- `menu_state()`'s `clear_all`, `cover` and `busy` lists.
- `src/main/main_season.inc:24,105-109` -- `DST_TABLE` 0x207f8eb0 and its reader. `src/main/main_situation.inc:639` -- `set_season()`'s unbounded read.
- `docs/modules/main.md:82-135` -- the binary, the Linkage and the image's known tables. Per-slice sections name more offsets.
- `docs/retail-bugs.md:80,92` -- MAIN-6 and MAIN-18.
- `tools/check_data_image.py` -- `PTR_RES`, the form → base table in `ptrs_check`, the `G3D_CODE_TARGET` logic to generalise, the self-test cases.
- `tools/data_images.txt`; `tools/raw_dumps_allowed.txt:16,46-48`.
- `cmake/steps/all.cmake:154-165` -- the MAIN step. `cmake/steps/difftests.cmake:155` -- `maindiff.exe`. `cmake/targets.cmake:756-811,1429-1440` -- the MAIN targets.
- `docs/tools.md` (the Baselines lines), `docs/building.md`, `docs/modules/main.md`, `docs/implementation/deferred-work.md` (close the slice 15 entry; queue the runs).

## Tasks & Acceptance

**Execution:**
- [x] `tools/check_data_image.py` -- the `MAIN_DATA_POINTER` form; `MAIN_CODE_TARGET` with `G3D_CODE_TARGET`'s rules (shared, not copied); the `MAIN_LINKAGE_RETAIL` value and completeness check; the `tail=` field: parse, format, `UPDATE=1`, a persisted sha256, the `--hash` window, and a retail window rebuilt from the mapped image (`.text`, the zero gap, `.rdata`, as `gen_main_data.py` builds it; `read_rva()` alone doesn't overlay `.rdata`). Our side is found by COFF symbol (`_main_text_tail` is at RVA 0x1e000 today). Both windows must be positive and wholly inside mapped sections, and no relocation, ours or retail's, may overlap either, in the comparison and in `--hash` (as `rdata=` rejects overlap, `check_data_image.py:184`). Add negative self-tests for an uncontained window and an overlapping relocation, and self-test cases for each matrix row.
- [x] `src/main/main_image.inc` (new), `src/main/main.c`, `src/main/main_pointers.inc` (header) -- the typed image, written by a scratch script outside the repo; a host compile must equal the old `main_data.inc` image.
- [x] `src/main/main_situation.inc`, `src/main/main_text_tail.inc` -- the season words and the five daylight records (tail +0x4e1d, 0x1e bytes) typed; the rest kept with a reworded header that names MAIN-6/MAIN-18; the `tail=` row field (`tail=_main_text_tail:0x207f4093:0xbf6d`).
- [x] `src/main/main_start.inc` -- the lists regrouped by menu under comment lines.
- [x] `tools/data_images.txt`, `tools/raw_dumps_allowed.txt`, `cmake/steps/all.cmake` -- the row and the `--hash` line; the generator and dump lines go.
- [x] Delete `tools/gen_main_data.py` and `src/main/main_data.inc`.
- [x] The docs in the Code Map, and `~/openfs98-verify/story-files.md`.

**Acceptance Criteria:**
- Given the slice is built, when `MAIN.DLL` and `maindiff.exe` are normalized with `peh2.py`, then each equals its pre-slice hash (recorded in the scratchpad `pre15/`).
- Given the slice is built, when `tools/ofs data-images` runs, then 25 rows and the self-test pass.
- Given the slice is built, when `check_raw_dumps.py` runs, then it reports 12 `gen` scripts, and `main_text_tail.inc` is MAIN's only allowlisted file.

## Implementation Notes

- **Hashes (peh2.py):** `MAIN.DLL` 8630d3dfaaf513e6, equal to s14 and `pre15/`; `maindiff.exe` 7d0a22ca3072b9cf, equal to `pre15/`. `maindiff.exe` compiles only its harness and loads the copied DLL, so it did not relink. `G3D.DLL` and `HG2D.DLL` are unchanged.
- **`data-images`:** `25 .data images identical (690935 bytes, 2928 relocated), 8 .rdata ranges identical (2317 bytes), 1 switch tables, 1 tail windows (49005 bytes), 8 pointer lists (1971 entries), self-test passed (95 cases)`. MAIN: 635 relocated, 133 kept, 47 set in C, 598 list entries.
- **Dumps:** `4 allowed dump files, 38 scripts reading orig/ (12 gen, 10 resource, 16 oracle), none new`. `main_text_tail.inc` is MAIN's only allowlisted file.
- **Departure from the spec (tail relocations):** retail's tail window is code with 2,309 relocations of its own, so "no retail relocation may overlap" can never pass. Only our own relocations over our window are rejected. Our window must lie inside one of our sections. Retail's window runs from 0x207f4093 to `.data`'s start, 0x1f9 bytes into zero padding past `.rdata`'s mapped size, as `gen_main_data.py` built it.
- **Image:** a host compile of `main_image.inc` equals the old `main_data.inc` image. Typed by structure: the ImportTable, the Linkage, the month-length tables (+0x2c8, +0x2e0, and the pair at +0x6de0/+0x6df8), the season boundaries (+0x2f8), the five-word table (+0x6e10) and the 77 situation records with their field lists. The other ~30 kB is typed words and strings marked "meaning not yet known", queued in `deferred-work.md`.

- **After review:** a self-test now covers a successful tail-hash update (96 cases). 3-character strings ("Foo", "Bar" and 27 more) are `STR4` literals. The header separates dialog callbacks `main_install()` replaces from addresses interpreted at call time. `MAIN.DLL` is still 8630d3dfaaf513e6 and the manifest's MAIN image hash is unchanged.

## Spec Change Log

## Review Triage Log

Code review, pass 1 (Codex gpt-6.1-sol, medium; Blind Hunter 10, Edge Case Hunter 2, Verification Gap 1).

| # | Layer | Finding | Verdict | Evidence | Route |
|---|---|---|---|---|---|
| 1 | blind | The DLL's compiled `data_pointers`/`retail_linkage` arrays are not checked against the parsed list | low | they compile from the checked `.inc`; only a patched DLL differs, and the same holds for every pointer list since slice 10 | defer (with 2) |
| 2 | blind | Nothing proves `main_install()` sets the 47 `cset=` slots | medium | `cset=` checks only that they are zero; it is slice 14's deferred install-proof entry, which named only G3D and HG2D | defer |
| 3 | blind | The tail check follows the COFF symbol, not the `main_text_tail` export | low | a redirect needs a hand-edited `main.def`; the fix adds a second lookup | reject |
| 4 | blind | `--hash` ignores the tail's manifest VA | low | `data-images` compares with retail at that VA and fails; the build hash covers the bytes | reject |
| 5 | blind | The manifest is not a build dependency | medium | already deferred in slice 14 | reject (already deferred) |
| 6 | blind | MAIN's G3D command blocks (+0x2386..+0x34fa) are packed words | medium | real structure, but a linear parse from +0x2386 stops at once (opcode 0x83d7); the blocks start at entry points tabled in `main_natives.inc:770-822`, which must be worked out first | defer |
| 7 | blind | The radio retuning records are not written by their fields | medium | `radio_retune()` gives the layout; but the region is labelled "meaning not yet known" | defer (with 8) |
| 8 | blind | The known dialog descriptors are labelled unknown | medium | `main_clockdlg.inc`/`main_dialogs.inc` give their layouts | defer |
| 9 | blind | "Foo"/"Bar" and similar buffers are words, not string literals | low | direct correction | patch |
| 10 | blind | The header says all 133 raw addresses are only called through the mapping | low | some are dialog callbacks `main_install()` replaces; a comment correction | patch |
| 11 | edge | A MAIN list using `G3D_DATA_POINTER` would skip the Linkage checks | low | needs a hand-written wrong family; the fix adds a guard | reject |
| 12 | edge | Retail relocations overlap the tail window, against the spec | false | the spec's rule cannot pass (2,309 retail relocations in code); the departure is recorded in Implementation Notes; the fix would edit the spec | reject |
| 13 | gap | No test of a successful tail-hash update | medium | pre-verified: no self-test writes and re-reads a tail hash | patch |

## Verification

**Commands:**
- `cmake --build build/win32-retail` -- expected: success, with the MAIN `--hash` line passing.
- `tools/ofs data-images` -- expected: 25 rows; the self-test passes.
- `tools/ofs maindiff` (build only) -- expected: `maindiff.exe` built.
- `python3 <scratchpad>/peh2.py` on `bin/MAIN.DLL` and `bin/difftest/maindiff.exe` -- expected: equal to `pre15/` and s14.
- `python3 -B tools/check_raw_dumps.py` -- expected: 12 gen.
