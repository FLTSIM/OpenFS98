---
title: 'Story 1.4, slice 17b: SIMSUI data decompiled'
type: 'refactor'
created: '2026-11-22'
status: 'done'
baseline_commit: 'c1f577c4048f2492cbb9ab099284072aedc8c835' # plus uncommitted 1.3 close-out; file snapshot at ~/openfs98-verify/baseline-1-4r
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4p-host-data.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4q-sim1-data.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** SIMSUI's build step (`cmake/steps/all.cmake:383-399`) runs two generators that read `orig/SIMSUI.DLL`:
- **`tools/gen_simsui_tables.py`** rewrites the committed `src/simsui/simsui_tables.inc` on every build. That file holds the imports, functions and vtables. Its output is identical to the committed file.
- **`tools/gen_simsui_native.py`** writes `bin/simsui-native/simsui_image.bin`: retail's bytes from RVA 0x15000 to 0x21400, about 50 KB of `.rdata`, `.data` and `.data`'s zero tail. It writes `simsui_image.S`, which `.incbin`s that blob as `_fs98_simsui_block` in `.data`, aligned to 4096, and gives each exported datum a `.set` label. It also writes `simsui.def` and `simsui.res`.

At attach, `bind_image()` (`src/simsui/simsui_native.c:72`) rewrites each of the image's relocated words from the committed `src/simsui/simsui_native_tables.inc`. Each row is one of `SIMSUI_RELOC_DATA`, `_CODE`, `_IMPORT` or `_NONE`. Nothing checks that table against retail; `gen_simsui_native.py` only counts the rows.

**Approach:**
- **The image becomes committed typed C:**
  - The new `src/simsui/simsui_image.inc` is in `src/host/host_image.inc`'s style:
    - `AT(off)` designated initialisers at retail's RVA minus 0x15000, so every comment and `AT()` shows the RVA;
    - `U8`/`U16`/`U32` values and `STRn` strings;
    - zero runs of 16 bytes or more left out.
  - Each relocated word stays retail's raw VA as a `U32`, commented with what `simsui_native_tables.inc` binds it to, since `bind_image()` overwrites it.
  - Regions are named from:
    - the export table (each exported datum under its export name);
    - the `SIMSUI_VTABLE` rows;
    - `docs/modules/simsui.md`.
  - The rest is typed values marked "meaning not yet known".
  - A scratch script outside the repo writes the file. Its host-compiled bytes must equal `simsui_image.bin`.
  - The new `src/simsui/simsui_image.c` defines `uint8_t fs98_simsui_block[0xc400]` in `.data`, aligned to 4096, and includes the file.
    - It also includes `simsui_labels.h` (compiled with `-Ibin/simsui-native`), the file `bin/simsui-native/simsui_labels.h` written by `gen_simsui_native.py` from the export table. That header holds the `.globl`/`.set` datum labels as file-scope `__asm__`, so they are assembled in the same unit as the block; COFF cannot alias an undefined symbol plus an offset.
    - It is compiled with `-fno-ident`, which is required: the old assembly emitted no `.ident`.
    - It is compiled explicitly in the native DLL's step, to `bin/simsui-native/simsui_image.o`, which keeps the old object's place in the link after `simsui_native.o`. It is not added to `src/simsui/sources.txt`, which also feeds `simsuidiff.exe` and every break build.
    - The object's allocatable sections must match the old one's: `.data` 0xd000 bytes aligned to 4096, and nothing else allocatable.
- **`gen_simsui_native.py`** stops writing `simsui_image.bin` and `simsui_image.S`, and writes `simsui_labels.h` instead; the step's assembly line goes. The script keeps the `.def` (exports by name and ordinal), `simsui.res` and its layout and relocation asserts. In the allowlist it moves from `gen:` to `resource:`.
- **`gen_simsui_tables.py`** becomes a check, `tools/ofs simsui-tables-check`, as `gen_host_rt_tables.py` became in slice 16:
  - it writes nothing and exits 1 if the committed `simsui_tables.inc` differs;
  - it leaves the build step;
  - `gen_host_rt_tables.py` still imports its helpers;
  - in the allowlist it moves from `gen:` to `oracle:`.
- **A new `check_data_image.py` mode, `bound=PATH[:SHA256]`**, for a pointer image bound at load. With `at=SYMBOL`, it is its own early branch, not the `.data`-zeroing comparison:
  - **The window.** Retail's window is mapped section by section, as `gen_simsui_native.py` maps it: `.rdata` 0x15000, virtual size 0xa768; the gap; `.data` 0x20000, virtual size 0x13f8; zero past raw data; up to 0x21400. It is unrelocated and nothing is zeroed. Our symbol's 0xc400 bytes must equal it, with none of our relocations in the window.
  - **The table.** PATH is parsed strictly. Every line must be one of:
    - the two `#define`s, which must equal 0x15000 and 0x21400;
    - a well-formed `SIMSUI_RELOC_*` row;
    - a comment.

    Sites must be unique, ascending, and four bytes inside the window. The site list must equal retail's relocations in the window.
  - **Each row's kind**, checked with the generator's classifier and its precedence, against retail's raw target:
    - **DATA:** the target is inside the window plus 0x10, and the row's target equals it.
    - Otherwise the target must be in `.text`. Then, in this order:
      - **IMPORT:** a `jmp [slot]` thunk whose `SIMSUI_IMPORT` index equals the row's;
      - **CODE:** the routine's set of `OP`/`OPV` RVAs in `tools/difftest/simsui_operations.inc` contains the target (one routine may stand for several RVAs);
      - **NONE:** the funclet set (at or above 0x13560, or one of the four listed).
    - Anything else fails, naming the site.
  - **`--hash`** hashes the window plus the canonical table: the bounds, then the rows in order.
  - The new row is `SIMSUI SIMSUI.DLL bin/SIMSUI.DLL at=_fs98_simsui_block bound=… …`, written with `UPDATE=1`, plus a SIMSUI `--hash` line in the step.

**Gate:** each of these is identical to its pre-slice copy after `i686-w64-mingw32-strip` and normalizing with `peh2.py`. The DLL carries a COFF symbol table, and the C unit adds a file symbol where the assembly had `fake`; stripping removes only that unmapped table. The pre-slice copies are in the scratchpad's `pre17b/`:
- `bin/SIMSUI.DLL` = `SIMSUI_ours.dll`;
- `bin/simsui-diff/simsuidiff.exe`.

The stripped hashes are computed once from `pre17b/` and recorded in Implementation Notes before any change. The unstripped `peh2.py` hashes are 714fa51ee817b4a7 for the DLL and efd26444a15c5d60 for `simsuidiff.exe`, and the latter must still hold unstripped.

Rebuild `simsuidiff.exe` explicitly (`difftests` is not in the default build) before comparing. `data-images` must pass with 28 rows. If code or layout changes, report the per-section diff and stop.

**Queued with the other held-back runs:** `simsuidiff-run`, `simsui-breaks`, `main-dialogs` and `flights-play`.

## Boundaries & Constraints

**Always:**
- `bind_image()`, `simsui_native_tables.inc`'s rows and `simsui_tables.inc`'s content stay as they are. So do the text that `tools/simsui_breaks.py` patches and the exports' names and ordinals.
- Carry the generators' asserts into the checker or into comments: the base 0x20470000, the image bounds and the export count.
- Check (a) must flag none of the new C.
- Never `rm -rf bin`. No staging or commits. Record the files in `~/openfs98-verify/story-files.md`.

**Never:**
- Don't move binding from load time to link time.
- Don't change any behaviour or touch another module's output.
- Don't do the queued runs.

## I/O & Edge-Case Matrix

| Scenario | Input | Expected |
|---|---|---|
| Row passes | the SIMSUI row | `data-images` passes and prints 130 DATA, 486 CODE, 1234 IMPORT, 279 NONE (2129) |
| Image byte | one value in `simsui_image.inc` changed | `data-images` fails against retail; the build's SIMSUI `--hash` fails |
| Binding | one `SIMSUI_RELOC_CODE` routine swapped, a row dropped or duplicated, a DATA/IMPORT/CODE row made NONE, or a bound changed | `data-images` fails, naming the site |
| Tables | one pop count in `simsui_tables.inc` changed | `simsui-tables-check` exits 1; the build is unaffected |
| Clean build | in a scratch copy, `bin/simsui-native/` intermediates and `bin/SIMSUI.DLL` deleted | the step reruns and builds the same DLL from the committed files (a build check, not a checker self-test) |

</frozen-after-approval>

## Code Map

- `tools/gen_simsui_native.py` -- `main()`: the image builder (to remove), the relocation classifier (reuse in the checker), the export, `.def`, `.S` and resource writers.
- `tools/gen_simsui_tables.py` -- `main()` writes `OUTPUT`; turn it into a comparison. Its helpers are imported by `tools/gen_host_rt_tables.py:52`; keep their names.
- `tools/gen_host_rt_tables.py`, `cmake/targets.cmake:184` (`host-rt-tables-check`) -- the pattern for the check and its target.
- `src/simsui/simsui_native.c:20-35,72-85` -- `fs98_simsui_block`, the table include and `bind_image()`.
- `src/host/host_image.inc:1-40`, `src/host/host.c:35-45` -- the typed-image style and its macros.
- `cmake/steps/all.cmake:378-399` -- the SIMSUI step: drop the tables line; compile `simsui_image.c` (add it to the link); add the `--hash` line.
- `cmake/OpenFS98.cmake:16`, `src/simsui/sources.txt` -- the SIMSUI objects list; it also feeds `cmake/steps/difftests.cmake:389` and `tools/simsui_breaks.py:836`. Leave it alone.
- `tools/check_data_image.py` (`rawptrs=`/`tail=` are the nearest modes; add `bound=` and its self-test cases), `tools/data_images.txt`, `tools/raw_dumps_allowed.txt`.
- `docs/modules/simsui.md`, `docs/tools.md` (the `simsui` line, the new check and the Baselines), and `docs/implementation/deferred-work.md` (close 17b; queue the four runs).

## Tasks & Acceptance

**Execution:**
- [x] `src/simsui/simsui_image.inc`, `src/simsui/simsui_image.c` (new, committed) -- the typed image and its unit -- from a scratch script, byte-equal to `simsui_image.bin` on a host compile.
- [x] `tools/gen_simsui_native.py`, `cmake/steps/all.cmake` -- stop writing the image; keep the labels, `.def`, `.res` and asserts; build and link the new unit.
- [x] `tools/gen_simsui_tables.py`, `cmake/targets.cmake` -- make it a check; add `simsui-tables-check`.
- [x] `tools/check_data_image.py`, `tools/data_images.txt` -- the `bound=` mode and the row, plus self-test cases for the matrix's image and binding rows (a duplicate-routine CODE row passing included).
- [x] `tools/raw_dumps_allowed.txt`, the Code Map's docs, `~/openfs98-verify/story-files.md`.

**Acceptance Criteria:**
- Given the slice is built, when the two gate binaries are stripped and normalized with `peh2.py`, then they equal `pre17b/` stripped (and `simsuidiff.exe` equals unstripped too).
- Given the slice is built, when `tools/ofs data-images` runs, then 28 rows and the self-test pass.
- Given the slice, when `check_raw_dumps.py` runs, then it reports 6 `gen`, 12 `resource` and 19 `oracle` scripts.
- Given `orig/` and `cd/`, when `tools/ofs simsui-tables-check` and `tools/ofs host-rt-tables-check` run, then both pass.

## Implementation Notes

- Pre-slice stripped `peh2.py` hashes (2026-11-22): `SIMSUI.DLL` 64a923265c69b21f, `simsuidiff.exe` 7f3b506c090e40d7.
- **Gate (orchestrator, 2026-11-22):** the leftovers `bin/simsui-native/simsui_image.{bin,S,o}`, `simsui_labels.h`, `SIMSUI_ours.dll` and `bin/SIMSUI.DLL` were moved to the scratchpad's `old17b/`. Then `cmake --build build/win32-retail` regenerated them; the stripped `peh2.py` hash is 64a923265c69b21f, equal. `--target difftests` rebuilt `simsuidiff.exe`: efd26444a15c5d60 unstripped, equal.
- **Checks:**
  - `data-images`: 28 images; SIMSUI 130 DATA, 486 CODE, 1234 IMPORT, 279 NONE (2129); self-test 131 cases.
  - `host-rt-tables-check` and `simsui-tables-check` pass; the latter exits 1 with a pop count mutated.
  - Dumps: 6 gen, 12 resource, 19 oracle.
  - `objdump -h simsui_image.o`: `.data` 0xd000, 2**12.
- **Hash split:** `sha256=` hashes the window; `bound=`'s sha256 hashes the bounds and the rows in order.

## Spec Change Log

## Review Triage Log

Code review, pass 1 (Codex, high; Blind Hunter 10, Edge Case Hunter 1, Verification Gap 2 plus 1 other).

| # | Layer | Finding | Verdict | Evidence | Route |
|---|---|---|---|---|---|
| 1 | blind | The SIMSUI step does not depend on `tools/data_images.txt` | low | every module's `--hash` step is the same (pre-existing); `UPDATE=1` zeroes the DLL's mtime | defer |
| 2 | blind | The `.set` label offsets are not checked against retail's exports | low | the labels are written from retail's export table each build; the gate's identical DLL covers today's | reject |
| 3 | blind | The spec file is not in the diff | false | it is untracked and on disk; the diff was assembled by path | reject |
| 4 | blind, edge, gap | IMPORT index `0122` parses as 122 (Python) but binds 82 (C octal) | medium | `check_data_image.py:343`, `arg.isdigit()` then `int(arg)` | patch |
| 5 | blind | Retail relocations overlapping the window's start are not detected | low | retail is fixed and its sites equal the table's 2129; adds a guard | reject |
| 6 | blind | Duplicate retail relocations collapse in a set | low | the same; retail has none | reject |
| 7 | blind | Import names and ordinals are not checked by `bound=` | low | by design `simsui-tables-check` re-proves them; the two oracles are separate | reject |
| 8 | blind | `shared_routine()` does not test one routine for distinct RVAs | low | it counts repeated names only; a direct test correction | patch |
| 9 | blind | The initialiser table and raw DllMain are "meaning not yet known" | low | `simsui_native.c` names `INITIALISERS` 0x20000..0x2000c and `RAW_DLL_MAIN` 0x20020 | patch |
| 10 | blind | Two `STR8`s in the square-distance table are incidental bytes | low | `simsui_image.inc:3924`, inside a numeric table | patch |
| 11 | gap | No test changes an IMPORT row's index | medium | the IMPORT cases drop, duplicate or make NONE only; a weakened check passes | patch |
| 12 | gap | `simsui-tables-check`'s mismatch path has no recurring negative test | medium | only a hand mutation; `gen_host_rt_tables.py:self_test()` is the pattern | patch |

## Verification

**Commands:**
- `cmake --build build/win32-retail` and `cmake --build build/win32-retail --target difftests` -- expected: success, the SIMSUI `--hash` line passing.
- `i686-w64-mingw32-objdump -h bin/simsui-native/simsui_image.o` -- expected: `.data` 0xd000, 2**12; no other allocatable section with contents.
- `tools/ofs data-images`, `tools/ofs simsui-tables-check`, `tools/ofs host-rt-tables-check` -- expected: pass.
- `i686-w64-mingw32-strip -o` each gate binary and its `pre17b/` copy, then `python3 <scratchpad>/peh2.py` -- expected: equal.
- `python3 -B tools/check_raw_dumps.py` -- expected: 6 gen, 12 resource, 19 oracle.
