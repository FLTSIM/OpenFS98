---
title: 'Story 1.4, slice 5: BOOSTER, WEATHERM and DEMO data decompiled'
type: 'refactor'
created: '2026-11-19'
status: 'done'
route: 'dispatch'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43' # worktree held uncommitted Stories 1.2-1.4; file snapshot at ~/openfs98-verify/baseline-1-4e
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4-module-data-decompiled-no-raw-dumps-in-git.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** The rest of Story 1.4 is real dumps. Each one is a module `.data` image written into `src/` as hex bytes by a `gen_*_data.py` from `orig/` at build time. The three smallest single-image ones are BOOSTER (0x904 bytes), WEATHERM (the initialised part from +0x98, 0x1c5 bytes compared) and DEMO (0x1165). Each sits at its retail offsets behind the `ImportTable` export.

**Approach:** Do what slice 1 did for WINMSG and MPCHAT:
- each image becomes a typed struct at retail's offsets, with `_Static_assert`s and a designated initialiser;
- the `.inc` dump, its generator and its build line are deleted;
- `tools/check_data_image.py` proves each compiled image equals retail's byte for byte.

The tool also learns to zero our own relocation targets as well as retail's. WEATHERM's C initialises its Linkage code pointers statically, and those can never equal retail's addresses. As a guard, it fails if any of our relocation targets is not also one of retail's.

The whole-module difftests are queued, not run, until Story 1.3's Baselines run has finished. The byte-for-byte image check is the proof for now.

## Boundaries & Constraints

**Always:**
- Use slice 1's pattern (`spec-1-4`, and `src/winmsg/winmsg.c`, `src/mpchat/mpchat.c`).
  - Strings become string initialisers, numbers become named fields, and tables become typed arrays.
  - Pointers that retail relocates stay zero where the C installs them at load, as today. Where the C already initialises them statically (WEATHERM's Linkage), they stay as they are.
  - `D` and every offset-based access keep working unchanged.
- Each generator's asserts are the field list: they name what is non-zero. Read them before deleting.
- Where a byte's meaning is unknown, it becomes a named `uint8_t` or `uint32_t` field or array at its offset, commented "meaning not yet known".
- Floating-point values (BOOSTER's ten doubles at +0x60..+0xaf) are written as hex-float literals, so no decimal literal can round differently.
- The image object's size and the offsets the harnesses rely on do not change (WEATHERM: `WX_IMAGE_SIZE` 0x200, `ImportTable == Linkage - 0x38`).
- Queue `boosterdiff-run` (~75 min), `weathermdiff-run` and `demodiff-run` (~25 min) in `docs/implementation/deferred-work.md` as "run after Story 1.3's Baselines run". They are not run in this slice.
- Never `rm -rf bin`. No staging or commits. Record the files in `~/openfs98-verify/story-files.md`.

**Never:**
- No change to any module's code or behaviour beyond the image definition.
- No other module is converted.

</frozen-after-approval>

## Code Map

- `tools/check_data_image.py` -- after reading ours at the `ImportTable` export (`mine = …`), walk our own `.reloc`. Zero each HIGHLOW target inside `[start, start+vs)` in both `mine` and `img`.
  - Fail, naming the offset, if one of our targets is not among retail's relocation targets in the image. The review found ours always a subset: WEATHERM 20/20, ACONTAIN 12/12, DYNAMIC 71/71; the rest have none.
  - Print the counts, e.g. `WEATHERM: .data image 0x1c5 bytes identical (20 relocated, 20 ours)`.
  - A scratch prototype gave identical for BOOSTER, WEATHERM, DEMO, WINDOW, ACONTAIN, DYNAMIC, WINMSG and MPCHAT. Today, unchanged, only WEATHERM fails (+0x3c).
  - A wrong function pointer in a correct slot cannot be seen by a byte check. The queued difftests cover that.
  - Update the docstring, and the `docs/tools.md` lines for WINMSG and MPCHAT to the new output.
- BOOSTER:
  - `src/booster/booster.c:1342` includes `booster_data.inc`. The image is `uint8_t bytes[0x904]`, with `D` as `booster_image.bytes` (`:47`). With a typed struct, redefine `D` as `((uint8_t *)&booster_image)`; `D8`, `D16`, `D32` and `DPTR` keep working.
  - Layout per `tools/gen_booster_data.py`'s docstring: ImportTable at +0, Linkage at +0x38, then strings, level factors and state; its 4 Linkage code pointers are relocated.
  - `cmake/steps/all.cmake:48` is the generator line.
- WEATHERM:
  - `src/weatherm/weatherm.c:1549` includes `weatherm_data.inc`.
  - The image is `struct wx_image wx_image` (`weatherm.c:80-89`, `:1531-1556`): `import[7]`, the typed `wx_linkage` (0x60 bytes, at +0x38), then `uint8_t data[0x168]`, which the `.inc` fills. `_ImportTable` aliases `_wx_image` and `_Linkage` is `_wx_image+0x38`.
  - Only `data[]` becomes typed. Keep `WX_IMAGE_SIZE` at 0x200 (`weathermdiff.c:128`) and `ImportTable == Linkage - 0x38` (`:882`).
  - The three tables sit at odd offsets (+0xf3, +0x103, +0x145, +0x181), so their u16/u32 fields need `__attribute__((packed))` or byte arrays.
  - The existing size check is a `typedef char[...]` trick; replace it with `_Static_assert`.
  - `all.cmake:246` is the generator line.
- DEMO:
  - `src/demo/demo.c:3225` includes `demo_data.inc`.
  - ImportTable at +0, Linkage at +0x50, then state and tables.
  - Its 98 relocations are the Linkage, the reader table at +0x610, three handler tables, the dialog callbacks, two more code pointers (+0xe84, +0xed4) and six pointers into `.data` (+0xea4, +0xea8, +0xec0, +0xec4, +0xeec, +0xf00). All are installed by `demo_install` (`demo.c:3260-3267`).
  - The non-zero content is small, about 94 bytes. The biggest tables are 20 u32 values at +0x760 and 17 bytes at +0x7d5.
  - `all.cmake:77` is the generator line.
- `tools/raw_dumps_allowed.txt` -- remove `gen:gen_booster_data.py`, `gen:gen_weatherm_data.py`, `gen:gen_demo_data.py` and their `.inc` entries where listed. Update the `docs/tools.md` Baselines count line to what the script prints.
- `docs/tools.md` Baselines -- add one `check_data_image.py` line per module, next to the WINMSG and MPCHAT ones.
- Docs: `docs/modules/{booster,weatherm,demo}.md` say where the image comes from (`booster.md:22` and `demo.md:18` name the generators). Update those passages only.
- No one else uses these files. Only `all.cmake` runs the three generators. `boosterdiff`, `demodiff` and `weathermdiff` (`difftests.cmake:58,79,228`) load the DLLs from `bin/` and compile no module source. No break runner covers these modules.
- Check (a) is not a risk here: the three `.inc` files hold 272, 368 and 301 non-zero bytes. A `.field = …,` line resets the checker's run count.

## Tasks & Acceptance

**Execution:**
- [x] `tools/check_data_image.py` -- mask our relocations as well. Then, with the masking tool and before any C change, check that all five (BOOSTER, WEATHERM, DEMO, WINMSG, MPCHAT) print identical.
- [x] Each module, in turn: the typed struct and initialiser, `_Static_assert`s on `sizeof` and the key offsets, then delete the `.inc`, the generator and the `all.cmake` line. After each module: `tools/ofs all`, then its `check_data_image.py` line prints identical.
- [x] Update the allowlist, the Baselines lines, the module docs, and `deferred-work.md` (the three queued difftests).

**Acceptance Criteria:**
- Given a clean `tools/ofs all`, when `check_data_image.py` runs for BOOSTER, WEATHERM, DEMO, WINMSG and MPCHAT, then each prints "identical" and exits 0.
- Given the tree, when `check_raw_dumps.py` runs, then it exits 0 and prints `31 allowed dump files, 56 scripts reading orig/ (31 gen, 10 resource, 15 oracle)`.
- Given a temp copy of the tree (configured afresh, or just the module's gcc line from `all.cmake` run there, never writing the repo's `bin/`), when one byte outside the relocated slots is changed in each module's initialiser and the module is rebuilt and checked, then `check_data_image.py` exits 1 and names that offset. Use BOOSTER +0x58 (`bstinfo`), WEATHERM +0x103 and DEMO +0x7c0.
- Given ours with a relocated pointer at an offset retail does not relocate (simulated in the copy), when the tool runs, then it exits 1 and names the offset.

## Implementation Notes

- After `tools/ofs all`, all five print identical (relocated/ours): BOOSTER 4/0, WEATHERM 20/20, DEMO 98/0, WINMSG 6/0, MPCHAT 4/0. `check_raw_dumps.py`: 31 allowed dump files, 56 scripts reading orig/ (31 gen, 10 resource, 15 oracle).
- Negative cases, in a scratch copy that never wrote the repo's `bin/`:
  - a one-byte change exits 1 and names the offset: BOOSTER +0x5e (inside the `bstinfo` string), WEATHERM +0x105 (inside the +0x103 table), DEMO +0x7c0;
  - a relocated pointer of ours at BOOSTER +0xb0, where retail has none, exits 1.
- Beyond the spec, each whole DLL was compared with the build in `~/openfs98-verify/s14` (before slices 2-5), timestamps and checksum zeroed:
  - BOOSTER.DLL and WEATHERM.DLL are identical, code and static pointers included.
  - DEMO.DLL's `.data` is identical, but `demo_init` compiles 2 bytes shorter: the same stores and loop, reordered, with `%edx` for `%eax`. Everything after it shifts by 2 bytes (the `.rdata` jump tables, one `.reloc` entry).
  - It is semantically equivalent, so `demodiff-run` (queued) is the confirming run for DEMO. BOOSTER and WEATHERM need no difftest to be sure, though theirs stay queued.
- The queued runs (`boosterdiff-run`, `weathermdiff-run`, `demodiff-run`) are not yet done; they are in `deferred-work.md` for after Story 1.3's Baselines run.

## Spec Change Log

## Review Triage Log

| # | Finding | Verdict | Evidence / route |
|---|---|---|---|
| 1 | check_data_image.py is not run by any build or ofs target, and the generators' value asserts are gone (VG, BH) | medium | The Baselines lines are run by `cmake_verify.py`, but nothing runs them on an ordinary build. An `ofs` target is new public surface, so it is deferred as a proposal: `tools/ofs data-images` run after `all`, listed as `oracle:`. |
| 2 | A wrong pointer in a correct relocated slot is masked (VG, BH, EC) | false for BOOSTER and WEATHERM, low for DEMO | Their whole DLLs are identical to the pre-slice build, static pointers included. DEMO installs its pointers in code, and `demodiff-run` (queued) covers it. |
| 3 | Our relocation straddling the image end grows the buffer (EC) | low | Patched: it fails, naming the offset. |
| 4 | Our relocations outside the compared window are skipped (EC, BH) | low | Outside the window is outside retail's image too, so there is nothing to compare. Rejected. |
| 5 | A retail DLL with no reloc directory yields no targets (EC) | low | Correct: no relocations means nothing to mask. Rejected. |
| 6 | Generator asserts on image base, module ids and relocation layout are lost (BH, EC) | low | The byte comparison against retail covers the ids and layout, and a rebased CD fails it. Rejected. |
| 7 | The booster fmt_d4 comment is misleading (BH, EC) | low | Patched; the bytes were right. |
| 8 | DEMO comment "six pointers" conflicts with docs and the old generator (BH, VG, EC) | false | Six is right (+0xea4, ea8, ec0, ec4, eec, f00), as the spec review found; the old generator's "five" was the slip. |
| 9 | Packed struct alignment is inconsistent (BH) | low | Patched: `demo_image` gets `aligned(4)` like BOOSTER. DEMO's code diff is unchanged (the same 10 lines). |
| 10 | No `_Static_assert` that the size macros are at least the minimum (BH) | low | A wrong macro gives a negative array size, which is already a compile error. Rejected. |
| 11 | Failure reports only the first difference (BH) | low | Rejected. |
| 12 | Spec ticked while the difftests are unrun (BH) | low | Recorded in Implementation Notes; they are queued by design. |

## Verification

**Commands:**
- `tools/ofs all && for m in BOOSTER WEATHERM DEMO WINMSG; do python3 tools/check_data_image.py orig/Modules/$m.DLL bin/$m.DLL; done && python3 tools/check_data_image.py orig/Modules/mpchat.DLL bin/MPCHAT.DLL` -- expected: five lines saying identical.
- `python3 tools/check_raw_dumps.py` -- expected: exit 0.
- Queued for after Story 1.3: `tools/ofs boosterdiff-run`, `weathermdiff-run` and `demodiff-run` -- expected: 20000 scenarios, 0 differ each.
