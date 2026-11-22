---
title: 'Story 1.4, slice 16: HOST data decompiled'
type: 'refactor'
created: '2026-11-21'
status: 'done'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43' # worktree held uncommitted Stories 1.2-1.4; file snapshot at ~/openfs98-verify/baseline-1-4p
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4o-main-data.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** the host's data still comes from `orig/FLTSIM98.EXE` at build time, through two generators run by the `bin/OPENFS98.EXE` step (`cmake/steps/all.cmake:430-431`). HOST has no manifest row.
- **`tools/gen_host_data.py`** writes:
  - `src/host/host_data.inc`: the 0x55cc-byte `.data` image (0x1600 on disk), a hex dump that `host.c:39-41` includes as `host_image`;
  - `src/host/host_pointers.inc`: 200 `HOST_SLOT` lines, which nothing includes.
- **`tools/gen_host_rt_tables.py`** (with `tools/host_rt_survey.py`, `gen_simsui_tables.py` and the CD's `MFC42.DLL`) writes:
  - `src/host/runtime/host_rt_rdata.inc`: retail's `.rdata`, a hex dump of its 0x200-byte raw size (0x190 virtual) included by `host.c:46` and `host_rt_exe.c:28`;
  - `src/host/runtime/host_rt_tables.inc`: 1,315 lines of ABI facts from disassembly (`HOST_RT_IMPORT`, `_FUNCTION`, `_SERVICE`, `_THUNK`, `_CODE_POINTER`, `_DATA_POINTER`, `_DIRECT`), used by the runtime and `hostrtdiff.c`.

The EXE has no relocations. Its `.data` holds retail's absolute addresses, and our image keeps them raw: `host_rt_exe.c` rebases them at start-up. So the existing checker already compares HOST with no new mode. A trial manifest row, `HOST FLTSIM98.EXE bin/OPENFS98.EXE at=_host_image rdata=0x16000:0x200`, passes today: 0x55cc bytes identical, 0 relocated, and `.rdata` identical.

**Approach:**
- **The image.** `src/host/host_image.inc` is committed typed C, in the style of slices 13-15, replacing `host_data.inc`.
  - `AT()`, one typed value per macro, strings as literals.
  - Named regions from `docs/modules/host.md` and `host.c`:
    - the palette at 0x4171a4: 256 entries of four bytes (`host.c:203`);
    - the 0x1c-byte list headers at 0x417b30 (module records, 0x14-byte elements), 0x417ee0 (dialogs, 0x10) and 0x417f00 (modeless list, 8) (`host.c:350`);
    - the service table at 0x417ba0: a dword 1, then 199 words, of which 194 are retail's code addresses (raw `U32`s) and 5 are reserved zeros;
    - the 37-entry extension table at 0x417f20 with its strings;
    - the path buffer at 0x41bca0, empty at start and filled with a 0x104-byte bound (`host_rt_main.c:102`).
  - The rest typed values marked "meaning not yet known".
- **`.rdata`.** `host_rt_rdata.inc` keeps all 0x200 bytes (the arrays keep their size) and becomes typed C in the same style: code addresses as `U32`, strings as literals, numbers as typed values.
- **The runtime tables.** `host_rt_tables.inc` becomes committed source with its header reworded. `gen_host_rt_tables.py` leaves the build and becomes an oracle check, `tools/ofs host-rt-tables-check`: it regenerates both outputs into memory, writes nothing, and fails if `host_rt_tables.inc` differs from the committed file outside its header comment (the header is skipped on both sides) or if the `.rdata` bytes differ from the image `host_rt_rdata.inc` compiles to. Its prerequisites are documented in `docs/tools.md`: `orig/FLTSIM98.EXE`, the CD's `cd/mfc42.dll`, `objdump`, `i686-w64-mingw32-nm` and the mingw import libraries (`gen_simsui_tables.py:33`). It does not run in a clean checkout alone. Its disassembly and MFC analysis are kept because they are not recoverable from the file.
- **The pointer rows.** `check_data_image.py` gets a new `rawptrs=PATH` field, for an image without relocations. It parses `HOST_RT_CODE_POINTER(site, target)` and `HOST_RT_DATA_POINTER(site, target)`, and requires them to equal exactly the set the generator's rule gives on retail:
  - every aligned word of `.rdata` and `.data` (raw size) whose value lies in `.text`, or in 0x416000..0x41f000;
  - each row's target equals that word minus 0x400000.

  `--hash` hashes the parsed rows.
- **The manifest row.** `HOST FLTSIM98.EXE bin/OPENFS98.EXE at=_host_image rdata=0x16000:0x200 rawptrs=src/host/runtime/host_rt_tables.inc`, written with `UPDATE=1`, plus a HOST `--hash` line in the `OPENFS98.EXE` step.
- **Removed:**
  - `gen_host_data.py`, `host_data.inc` and the unused `host_pointers.inc`;
  - the generator lines in `all.cmake`;
  - the allowlist lines for `gen_host_data.py`, `host_data.inc` and `host_rt_rdata.inc`. `gen_host_rt_tables.py` moves from `gen:` to `oracle:`.

**Gate:** the following are identical to their pre-slice builds after normalizing with `peh2.py`:
- `bin/OPENFS98.EXE` (also equal to s14);
- `bin/HOST.DLL` (built by `tools/ofs hostdiff`, which only builds);
- `bin/host-rt/hostrtdiff.exe`;
- `bin/difftest/hostdiff.exe`.

`data-images` must pass with 26 rows, and `host-rt-tables-check` must pass. If code changes, report the per-function diff and stop.

**Queued until Story 1.3's Baselines run finishes:** `hostdiff-run`, `host-breaks`, `hostrtdiff-run` and `host_rt_breaks.py`.

## Boundaries & Constraints

**Always:**
- `struct host_image { uint8_t bytes[0x55cc]; } host_image` keeps its wrapper, member, symbol, size, layout and section, and every `V()`/`D` access stays as it is. So do `fs98_host_rt_rdata`, `s_rdata` and the `HOST_IMAGE_SHARED` paths.
- Keep the reference-EXE sha256 check: the oracle keeps it, and the image header names it.
- Carry `gen_host_data.py`'s facts into comments: base 0x400000, `.data` at 0x417000, the table at 0x417ba0, 0xc8 dwords.
- The text `tools/host_breaks.py` and `tools/host_rt_breaks.py` patch stays as it is.
- Check (a) must flag none of the new C.
- Never `rm -rf bin`. No staging or commits. Record the files in `~/openfs98-verify/story-files.md`.

**Never:**
- Don't change any behaviour or touch another module.
- Don't edit `host_rt_tables.inc`'s rows, only its header.
- Don't do the heavy runs on the queued list.

## I/O & Edge-Case Matrix

| Scenario | Input | Expected |
|---|---|---|
| Row passes | the HOST row | `data-images` passes; it prints 0 relocated and the pointer-row counts (244 code, 51 data) |
| Pointer row drift | a `HOST_RT_CODE_POINTER`/`_DATA_POINTER` row removed, added, repeated, or with a wrong target | fails |
| Image byte | one value in `host_image.inc` or `host_rt_rdata.inc` changed | `data-images` fails against retail; the build's HOST `--hash` step fails |
| Oracle | one analysis row of `host_rt_tables.inc` changed | `host-rt-tables-check` fails |

</frozen-after-approval>

## Code Map

- `tools/gen_host_data.py` -- the reference sha256, the layout constants, the zero fill.
- `tools/gen_host_rt_tables.py:150-175` -- the pointer-row rule; `:131-140` -- both outputs. `tools/host_rt_survey.py` and `tools/gen_simsui_tables.py` -- what it imports.
- `src/host/host.c:28-58` -- `host_image`, `fs98_host_rt_rdata`, `D`/`V()`. `src/host/host_rt_exe.c:20-110` -- `s_rdata`, the table uses, the rebase.
- `src/host/runtime/host_rt.h:33,57`, `tools/difftest/hostrtdiff.c:48-60` -- the other `host_rt_tables.inc` users.
- `docs/modules/host.md` -- the named addresses (lines 166, 206, 262, 287, 406-407, 504-527).
- `tools/check_data_image.py` -- `at=`, `rdata=`, `parse_ptrs`/`ptrs_check` (a model for `rawptrs=`), the manifest parse/format, the self-test.
- `cmake/steps/all.cmake:427-440` -- the `OPENFS98.EXE` step. `cmake/steps/difftests.cmake:141-144,306-309` -- `hostdiff.exe` and `hostrtdiff.exe`. `cmake/targets.cmake:184,817-850,1389-1410` -- the host targets. Add `host-rt-tables-check` beside them.
- `tools/raw_dumps_allowed.txt:12,14,43,44`; `tools/data_images.txt`.
- `docs/tools.md` (the Baselines lines), `docs/building.md`, `docs/modules/host.md`, `docs/implementation/deferred-work.md` (close the slice 16 entry; queue the runs).

## Tasks & Acceptance

**Execution:**
- [x] `tools/check_data_image.py` -- `rawptrs=` (parse, format, `UPDATE=1`, hash, the retail set check), and self-test cases for the matrix's pointer and image rows.
- [x] `tools/gen_host_rt_tables.py`, `cmake/targets.cmake` -- an in-memory compare mode, and the `host-rt-tables-check` target; drop its build line.
- [x] `src/host/host_image.inc` (new), `src/host/host.c`, `src/host/runtime/host_rt_rdata.inc`, `src/host/runtime/host_rt_tables.inc` (header) -- written by a scratch script outside the repo; host compiles must equal the old bytes.
- [x] `tools/data_images.txt`, `tools/raw_dumps_allowed.txt`, `cmake/steps/all.cmake` -- the row, the `--hash` line, the allowlist changes.
- [x] Delete `tools/gen_host_data.py`, `src/host/host_data.inc` and `src/host/host_pointers.inc`.
- [x] The docs in the Code Map, and `~/openfs98-verify/story-files.md`.

**Acceptance Criteria:**
- Given the slice is built, when the four gate binaries are normalized with `peh2.py`, then each equals its pre-slice hash (copies in the scratchpad `pre16/`: `OPENFS98.EXE` 3059d94f59843151, also s14; `HOST.DLL` 3f657cdc5ed99cef; `hostrtdiff.exe` 77ff5c721ea81bc8; `hostdiff.exe` 00be334e728a9f2e). `HOST.DLL` dates from 2026-11-14 and may be stale, as slice 14's `G3D_ours.dll` was. Where a copy is not reproducible, build that binary from `~/openfs98-verify/baseline-1-4p` with the same command and compare with that instead, and say so.
- Given the slice is built, when `tools/ofs data-images` and `tools/ofs host-rt-tables-check` run, then both pass.
- Given the slice is built, when `check_raw_dumps.py` runs, then it reports 10 `gen` and 17 `oracle` scripts, with no HOST file allowlisted.

## Implementation Notes

- **Hashes (peh2.py):** `bin/OPENFS98.EXE` 3059d94f59843151, equal to `pre16/` and s14; `bin/host-rt/hostrtdiff.exe` 77ff5c721ea81bc8 and `bin/difftest/hostdiff.exe` 00be334e728a9f2e, equal to `pre16/`. `HOST.DLL` is 232969785351a0a5, not `pre16/`'s 3f657cdc5ed99cef (the 2026-11-14 copy is stale): a build of `~/openfs98-verify/baseline-1-4p/src` with the same command gives 232969785351a0a5.
- **Checks:** `data-images`: 26 images, 9 `.rdata` ranges, 1 raw pointer list (295 rows: 244 code, 51 data), self-test passed (111 cases; 95-107 and the two hash round-trips are new; the rawptrs hash also covers the text of `host_rt_tables.inc` after its header). `host-rt-tables-check` passes. `check_raw_dumps.py`: 10 gen, 10 resource, 17 oracle, no HOST file allowlisted.
- **Image:** host compiles of `host_image.inc` and `host_rt_rdata.inc` equal the old files' bytes. `host_rt_rdata.inc` defines no `AT()` (its values run from +0, and `host_rt.h`'s `AT(object, offset)` would clash); `host.c` brackets its include of `host_image.inc` with `push_macro`/`pop_macro("AT")` for the same reason.
- **Departure:** the build no longer stops on a different `FLTSIM98.EXE` by itself (the pin lived in `gen_host_data.py`); the pin is in the oracle and the image headers, and `docs/building.md` says so.

- **HOST.DLL baseline rebuild (orchestrator):** built from `~/openfs98-verify/baseline-1-4p` with the `hostdiff` target's commands: 232969785351a0a5, equal to the new build; `pre16/`'s copy (2026-11-14) is stale.

## Spec Change Log

## Review Triage Log

Code review, pass 1 (Codex gpt-6.1-sol, medium; Blind Hunter 10, Edge Case Hunter 3, Verification Gap 1).

| # | Layer | Finding | Verdict | Evidence | Route |
|---|---|---|---|---|---|
| 1 | blind | The build no longer pins the reference EXE, but `gen_host_resources.py` still reads it | low | resources are taken by type and id; the frozen intent moves the pin to the oracle; whether another pressing's resources differ is unchecked | defer |
| 2 | blind, edge ×2 | Unmatched pointer invocations (whitespace, expressions) are skipped though they compile | medium | `rawptrs` parse uses one regex per row and ignores other lines | patch |
| 3 | blind, edge | `#if 0` around a pointer block keeps rows and hash | low | needs a deliberate edit of the block directives; the fix adds a guard | reject |
| 4 | blind | The oracle skips everything before the first `*/` | low | `body()`; the fix is a direct correction | patch |
| 5 | blind | The build hash leaves the import/function/service/thunk/direct rows unguarded | medium | the generator used to overwrite them every build; now only the oracle sees an edit | patch |
| 6 | blind | Recognizable strings are integers | low | direct correction | patch |
| 7 | blind | The palette headers are opaque `U32`s | low | `CreatePalette` callers give version 0x300, 0x100 entries | patch |
| 8 | blind | Four GUIDs in `.rdata` are anonymous | low | `host.c` names them | patch |
| 9 | blind | The accelerator, menu and string-module headers are unmarked | low | host code and harness constants give their layout | patch |
| 10 | blind | The oracle has no negative test | medium | only a manual mutation was run | patch |
| 11 | gap | No round-trip test of a `rawptrs` hash update | medium | pre-verified | patch |

## Verification

**Commands:**
- `cmake --build build/win32-retail` -- expected: success, with the HOST `--hash` line passing.
- `tools/ofs hostdiff`, then `tools/ofs hostrtdiff` (both build only) -- expected: built.
- `tools/ofs data-images`; `tools/ofs host-rt-tables-check` -- expected: both pass.
- `python3 <scratchpad>/peh2.py` on the four gate binaries -- expected: equal to `pre16/`.
- `python3 -B tools/check_raw_dumps.py` -- expected: 10 gen, 17 oracle.
