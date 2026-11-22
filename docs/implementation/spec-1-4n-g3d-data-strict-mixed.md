---
title: 'Story 1.4, slice 14: G3D data decompiled, strict mixed slot check'
type: 'refactor'
created: '2026-11-21'
status: 'done'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43' # worktree held uncommitted Stories 1.2-1.4; file snapshot at ~/openfs98-verify/baseline-1-4n
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4m-global-hg2d-data.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** G3D still carries its 0x18450-byte `.data` image as an allowlisted hex dump (`src/g3d/g3d_data.inc`, 24,117 non-zero bytes; retail's image has 24,821 before its relocated slots are zeroed) made at build time from `orig/` by `tools/gen_g3d_data.py`, which also writes `g3d_pointers.inc`. G3D has no manifest row. And `--mixed`, which G3D needs (as MAIN will in slice 15), accepts zero in any slot not in `ptrs=`, so `UPDATE=1` would bless a deleted raw value or a deleted pointer entry (slice 13's deferred entry).

**Approach:**
- **Tighten `--mixed`** (`tools/check_data_image.py`). A new manifest field `cset=` lists the zero slots the module sets in C: comma-separated offsets, and `A..B` for every 4th offset from A to B inclusive. Under `--mixed`, each retail relocated slot that isn't ours must be one of:
  - retail's raw value;
  - zero, and listed in `ptrs=` (value-checked as now);
  - zero, and listed in `cset=`.

  A `cset=` slot that retail does not relocate, that is also in `ptrs=` or ours, or that is non-zero, fails. `cset=` is only valid with `--mixed`. `--hash` hashes the parsed `cset=` list too, so editing it changes the build's hash. HG2D's row gets `cset=0x3c,0x40,0x50..0xc0,0xd4..0xf4` (its 40 slots).
- **G3D's image:**
  - `g3d_image` stays a `uint8_t[0x18450]` byte array, initialised from a committed typed `src/g3d/g3d_image.inc`, replacing `g3d_data.inc`. Slice 12/13 conventions apply: `AT()`, one typed value per macro, strings as literals, comments naming each region from `docs/modules/g3d.md` (ImportTable, Linkage, state, the opcode table of 148 `{handler, length}` pairs, the dispatch tables, the variable table at +0x6950 (9,124 entries of GLOBAL offsets), the scenery names and textures, the bytecode programs).
  - Retail's 202 raw code addresses stay raw `U32` values.
  - The 154 data pointers and the 22 Linkage slots (+0x5c, +0x60, +0x70..+0xbc) stay zero.
  - **Bytecode as instructions (decided 2026-11-21).** The built-in BGL programs (0xf840..0x15c22 parses as 989 instructions, 0x16180..0x16e12 as 1,301, by the opcode table's length rule, service +0x64) are written one instruction per line: `OP(0x0a)` for the opcode word, `W()` for each operand word, and `U32` for an embedded code address. Where a parse stops (tails, tables), the data is written as typed values marked "meaning not yet known". The self-test re-parses each program region from the committed file and fails if an `OP()` line's length disagrees with the rule.
- **`g3d_pointers.inc`** becomes committed source, with its header reworded. `ptrs=` learns `G3D_DATA_POINTER(at, target)` (image start + target). The checker also requires the file's `G3D_CODE_TARGET` set to equal the set of distinct raw values in the image's kept slots (193), and rejects a duplicate `G3D_CODE_TARGET` line.
- **Removed:** `gen_g3d_data.py`, `g3d_data.inc`, their allowlist lines, and the generator line in `cmake/steps/all.cmake`.
- **Added:** a G3D manifest row (`--mixed ptrs=src/g3d/g3d_pointers.inc cset=0x5c,0x60,0x70..0xbc`, written by `UPDATE=1`) and a G3D `--hash` line in `all.cmake`.

**Gate:**
- `G3D.DLL` and `HG2D.DLL` are identical to `~/openfs98-verify/s14/bin` after normalizing with `peh2.py`.
- `bin/difftest/G3D_ours.dll` and `g3ddiff.exe` are identical to the pre-slice builds.
- `data-images` passes with 24 rows.

If a DLL's code changes, report the per-function diff and stop for a human decision.

**Queued until Story 1.3's Baselines run finishes:** `g3ddiff-run`, `g3ddiff-meshc-run`, `g3dmeshdiff-run` and `g3d-breaks`.

## Boundaries & Constraints

**Always:**
- `g3d.c`'s `D`/`V()`/`M*` access, `data_pointers[]`, `code_targets[]` and `retail_byte()` stay as they are.
- The text that `tools/g3d_breaks.py` patches stays as it is.
- Check (a) must flag none of the new C.
- Carry the generator's asserts (ImportTable ids, module id 14, palette and runway names, 154/224 counts, image base and section addresses) into comments or `_Static_assert`s.
- Never `rm -rf bin`. No staging or commits. Record the files in `~/openfs98-verify/story-files.md`.

**Never:**
- Don't change any module's or harness's behaviour, or touch any module other than G3D (HG2D gets only its manifest field).
- Don't do the heavy runs on the queued list.

## I/O & Edge-Case Matrix

| Scenario | Input | Expected |
|---|---|---|
| Strict mixed passes | HG2D and G3D rows with `cset=` | `data-images` passes; the counts are printed |
| Raw value turned zero | a kept slot zeroed in ours | fails, naming the slot as neither raw nor listed |
| Deleted pointer entry | one `ptrs=` line removed | fails, naming the slot |
| Bad `cset=` | a slot retail doesn't relocate, a slot also in `ptrs=`, a non-zero slot, or `cset=` without `--mixed` | each fails, with its reason |
| Code targets drift | a `G3D_CODE_TARGET` line added, removed or duplicated | fails |
| Bytecode drift | an `OP()` line with a wrong operand count, a program region not fully covered by `OP()` lines, or an embedded code address moved off its operand | the self-test's re-parse fails |

</frozen-after-approval>

## Code Map

- `tools/gen_g3d_data.py` -- the generator: section addresses, asserts, the relocation split (154 data pointers zeroed and listed; 224 code pointers, of which the 22 relocated Linkage slots are zeroed and the other 202 kept raw, with 193 distinct targets).
- `src/g3d/g3d.c:44-50` (struct, `D`), `:15424-15470` (the image initialiser, the `_ImportTable`/`_Linkage` symbols, `data_pointers[]`, `retail_byte()`, `code_targets[]`), `:15480-15500` (`g3d_install`, which sets the Linkage slots).
- `src/g3d/g3d_pointers.inc` -- 154 `G3D_DATA_POINTER` and 193 `G3D_CODE_TARGET` entries.
- `docs/modules/g3d.md:24-90` -- the image layout and the opcode length rule. `src/g3d/g3d.c:15142` -- `next_instruction`, the length rule in code.
- `tools/check_data_image.py:230-262` (`ptrs_check`, its form → base table), `:293-375` (`compare`, the `--mixed` branch at 323), `:381-440` (manifest parse/format), `:510` (row dispatch), the self-test near 813 (cases 47-51 are HG2D `--mixed`).
- `tools/data_images.txt:25` -- HG2D's row. `tools/raw_dumps_allowed.txt:6,45` -- G3D's allowlist lines.
- `cmake/steps/all.cmake:117-121` -- the generator and G3D build lines. `cmake/targets.cmake:533-566,1223-1240` -- the g3d targets (`g3ddiff` builds `G3D_ours.dll` and `g3ddiff.exe`).
- `docs/tools.md` (the Baselines `data-images` and raw-dumps lines), `docs/building.md` (the module count, the `--mixed` text), `docs/modules/g3d.md`, `docs/implementation/deferred-work.md` (close the `--mixed` entry and the slice 14 entry; queue the runs; add an entry to revisit HG2D's `U8` regions the same way).

## Tasks & Acceptance

**Execution:**
- [x] `tools/check_data_image.py` -- `cset=` parse/format/hash; strict `--mixed`; the `G3D_DATA_POINTER` form; `G3D_CODE_TARGET` set and duplicate check; a re-parser for `g3d_image.inc`'s `OP()` lines (each line's length equals the opcode-table rule computed from retail's opcode table, the lines cover 0xf840..0x15c22 and 0x16180..0x16e12 exactly, and each of the 12 embedded code addresses is the `U32` operand at instruction +2 of an opcode `0x8c` line, six of them not 4-aligned); self-test cases for each row of the I/O matrix -- the tighter guard comes first, so G3D lands under it.
- [x] `tools/data_images.txt` -- add `cset=` to HG2D; add the G3D row via `UPDATE=1`.
- [x] `src/g3d/g3d_image.inc` (new), `src/g3d/g3d.c` (include it), `src/g3d/g3d_pointers.inc` (header) -- the typed image, written by a scratch script outside the repo and then committed as source.
- [x] `cmake/steps/all.cmake`, `tools/raw_dumps_allowed.txt` -- drop the generator and dump lines; add the `--hash` line.
- [x] Delete `tools/gen_g3d_data.py` and `src/g3d/g3d_data.inc`.
- [x] The docs listed in the Code Map, and `~/openfs98-verify/story-files.md`.

**Acceptance Criteria:**
- Given the slice is built, when `G3D.DLL`, `HG2D.DLL`, `G3D_ours.dll` and `g3ddiff.exe` are normalized with `peh2.py`, then each equals its pre-slice hash.
- Given the slice is built, when `tools/ofs data-images` runs, then 24 rows pass and the self-test passes with its new cases.
- Given the slice is built, when `check_raw_dumps.py` runs, then it reports 13 `gen` scripts and no G3D allowlist line.
- Given one value changed in `g3d_image.inc` or `g3d_pointers.inc`, when the build runs, then the G3D `--hash` step fails.
- Given HG2D's or G3D's `cset=` edited (a slot dropped or a wrong one added), when `tools/ofs data-images` runs, then it fails. The manifest is not a build dependency (`cmake/OpenFS98.cmake:34` keeps only `tools/*.py` and `orig/` for a python step), so a manifest edit alone does not rerun the build's `--hash` step. That is unchanged from slices 8-13.
- Given the pre-slice normalized hashes (`G3D.DLL` fb6e904484344827, `HG2D.DLL` 516a0e6d028dc6f3, `G3D_ours.dll` b219e8cc73433a93, `g3ddiff.exe` b4daf261eb8e9f1d; copies in the scratchpad `pre14/`), when the slice is built, then each matches.

## Implementation Notes

- **Hashes (peh2.py):** `G3D.DLL` fb6e904484344827 and `HG2D.DLL` 516a0e6d028dc6f3, equal to s14 and `pre14/`; `g3ddiff.exe` b4daf261eb8e9f1d, unchanged.
- **`G3D_ours.dll`:** the recorded b219e8cc73433a93 is the `g3ddiff-meshc-run` variant (portable mesh object), left in `bin/difftest` on 2026-11-14. `tools/ofs g3ddiff` only copies `bin/G3D.DLL` there (fb6e9044…). The implementer compiled `g3ddiff-meshc-run`'s two compile commands from both `baseline-1-4n` and the new tree: both 4f4a9ce7a675f10d. The old file was restored from `pre14/`.
- **`data-images`:** `24 .data images identical (498935 bytes, 2293 relocated), 8 .rdata ranges identical (2317 bytes), 1 switch tables, 7 pointer lists (1373 entries), self-test passed (71 cases)`. G3D: 378 relocated, 202 kept, 22 set in C, 347 list entries. HG2D: 112 kept, 40 set in C; its manifest hash changed because the hash now includes `cset=`.
- **Dumps:** `6 allowed dump files, 39 scripts reading orig/ (13 gen, 10 resource, 16 oracle), none new`.
- **Image file:** `src/g3d/g3d_image.inc`, 3,822 lines, 2,290 `OP()` lines; a long instruction goes on over `W()` lines. The scenery models (+0x4f22..+0x64d2), the tails after the programs and the bytecode from +0x17b1e on are typed words marked "meaning not yet known". A host compile of the new file equals the old `g3d_data.inc` image byte for byte.
- **Break checks:** a one-byte edit in `g3d_image.inc` fails the build's G3D `--hash` step; dropping a `cset=` slot (G3D 0xbc, HG2D 0xc0) fails `data-images`.

- **After review:** the `G3D_CODE_TARGET` check also runs when every target line is gone; `opcode_length()` wraps to 32 bits as service +0x64 does; a third built-in program, +0x17b1e..+0x18448 (183 instructions, the zero tail implicit), is written as `OP()` lines and re-parsed. `G3D.DLL` is still fb6e904484344827, the manifest hashes are unchanged, and the self-test has 72 cases.

## Spec Change Log

## Review Triage Log

Code review, pass 1 (Codex gpt-6.1-sol, medium; Blind Hunter 10 findings, Edge Case Hunter 3, Verification Gap none).

| # | Layer | Finding | Verdict | Evidence | Route |
|---|---|---|---|---|---|
| 1 | blind, edge | Deleting every `G3D_CODE_TARGET` line skips the set-equality check | medium | `ptrs_check` runs it only `if code_targets:` | patch |
| 2 | blind | The build's `--hash` step does not depend on `tools/data_images.txt` | medium | `cmake/OpenFS98.cmake:34`; the same for every row since slice 8 | defer (pre-existing) |
| 3 | blind | `opcode_length()` does not wrap to 32 bits as service +0x64 does | low | `g3d.c:15142` uses 32-bit registers; the fix is a direct correction | patch |
| 4 | blind, edge | `parse_cset()` takes negative, >32-bit or huge ranges | low | the manifest is committed source; it fails loudly (`struct.error`/memory), and the fix adds guards | reject |
| 5 | blind | An oversized `W()` value raises `OverflowError` | low | a loud failure on malformed source; the fix adds a guard | reject |
| 6 | blind | The re-parse never proves the source matches the checked DLL | false | `data-images` depends on `openfs98` (`cmake/targets.cmake:56-57`), so the DLL is rebuilt from the source, and its bytes are compared with retail | reject |
| 7 | blind | `cset=` doesn't prove C installs the slot | medium | this is the symbol-per-slot check, already deferred (`deferred-work.md`, slice 6 entry) | defer (pre-existing) |
| 8 | blind | No self-test for a `cset=` slot that is ours, or for the rejections under `UPDATE=1` | low | G3D and HG2D have no relocations of their own, so the case needs a synthetic row; `UPDATE=1` rewrites only passing rows | reject |
| 9 | blind | The scenery models (+0x4f22..+0x64d2) are anonymous `U16` runs | low | allowed by the frozen decision (typed values marked "meaning not yet known" where the parse stops) | reject |
| 10 | blind | The bytecode from +0x17b1e is outside the re-parse | medium | it parses cleanly as 187 instructions to the end of the image, so it is a third built-in program the spec missed | patch |

## Verification

**Commands:**
- `cmake --build build/win32-retail` -- expected: success, with the G3D `--hash` line passing.
- `tools/ofs data-images` -- expected: 24 rows pass; self-test passes.
- `tools/ofs g3ddiff` (build only) -- expected: `G3D_ours.dll` and `g3ddiff.exe` built.
- `python3 <scratchpad>/peh2.py <dll>` against `~/openfs98-verify/s14/bin` and the pre-slice copies -- expected: equal hashes.
- `python3 -B tools/check_raw_dumps.py` -- expected: `6 allowed dump files, 39 scripts reading orig/ (13 gen, 10 resource, 16 oracle)`.
