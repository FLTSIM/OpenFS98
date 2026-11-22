---
title: 'Story 1.4, slice 18c: the eleven MDL models data decompiled'
type: 'refactor'
created: '2026-11-22'
status: 'done'
baseline_commit: 'c1f577c4048f2492cbb9ab099284072aedc8c835' # plus uncommitted 1.3 close-out, 17b, 18a and 18b; file snapshot at ~/openfs98-verify/baseline-1-4u
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4t-gauges-data.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** The `bin/mdl-diff/modules.json bin/mdl-crash/modules.json` step (`cmake/steps/all.cmake:423`) runs `tools/build_mdls.py` (8 normal models) and `tools/gen_mdl_crash.py` (3 crash models). At build time they read each `orig/Aircraft/**/<NAME>.MDL` and copy its whole `.data` to `bin/.../<NAME>/model.data.bin`: 5.8–96 KB per model, about 600 KB in all, mostly geometry. `model_data.S` then `.incbin`s it, with `.long` at the 7 relocated slots. The scripts also take the survey fields, the ImportTable RVA, the image base and (crash) the CHAIN callback from retail. Story 1.4 allows only resources to come from `orig/` when the modules are built. The user decided (2026-11-22): "for 18c we will convert the MDL data to C for now", geometry included.

**Approach:**
- Commit each model's image as typed C under `src/mdl/images/<NAME>/`, built by one shared `src/mdl/mdl_image.c` in place of `model_data.S`.
- Commit the module metadata as `src/mdl/mdl_manifest.json`.
- A new oracle, `tools/gen_mdl_images.py`, writes these files from `orig/` and checks them.
- The two builders read `orig/` only for resources.
- Every MDL must come out identical, and so must the harness inputs.

## Boundaries & Constraints

**Always:**
- **Relocation shape.** Inside retail `.data` every model has exactly 7 HIGHLOW relocations, all 4-aligned. Retail has more elsewhere: 35 to 323 per model, outside `.data`.
  - In the header (`.data+0x10`, 0x24 bytes):
    - `+0x4` is a data pointer, `mdl_image + target`;
    - `+0x10` is update code.
  - In Linkage, `+0x4`, `+0x8`, `+0x18`, `+0x1c` and `+0x20` are code.

  The bindings are those of `build_mdls.py:32-39` and `gen_mdl_crash.py:40-44`. The generator requires exactly these sites inside `.data`. It keeps today's checks of relocation type, duplicates, bounds and targets, and fails otherwise.
- **Pointer slots.** They stay statically initialised address constants, so the linker emits the same relocations. Code symbols go through `__asm__` labels for the decorated names. No rebinding at attach.
- **Image layout.** A packed struct, with `_Static_assert` on its size and on every member's offset:
  1. `lead[.data RVA]`, with Sopwith's 14-byte literal at `0x1037`;
  2. `prefix[0x10]`;
  3. the header, 0x24 bytes with its 2 pointers;
  4. `body` from `.data+0x34` up to Linkage, `#include "data.inc"`, where `AT(off)` is `[(off) - 0x34] =` and `off` is the retail `.data` offset;
  5. Linkage, 0x24 bytes with its 5 pointers;
  6. a 0x14-byte tail.

  The non-pointer bytes of members 2, 3, 5 and 6 come from `layout.h`.
- **Labels.** `_mdl_image`, `_ImportTable`, `_Linkage` and `_mdl_header` keep their names and values. Add `_mdl_data = _mdl_image + <.data RVA>`. All of them are file-scope `__asm__` `.globl`/`.set`, written into the committed `layout.h` or `mdl_image.c`.
- **Compile and link.** `mdl_image.c` is compiled on its own with `-fno-ident`, as SIMSUI's step does (`cmake/steps/all.cmake:392`). Its object takes `model_data.S`'s place on every link line. Unchanged:
  - the flags of the other units;
  - the image's 16-byte alignment;
  - the crash models' `.mdlimg` (`"w"` and `"dw"` give the same flags) and `--section-alignment=65536`.
- **`data.inc` format.** Use the macros of `src/gauges/images/737ADI/data.inc`:
  - `U8`/`U16`/`U32` values;
  - strings as `STRn`;
  - zero runs of 16 bytes or more omitted;
  - comments naming the ImportTable, the rest marked "meaning not yet known".

  The emitter is new; `gen_gauges.py` has none.
- **Metadata.**
  - `modules.json`, `modules.inc` and `crashes.inc` are projected from the manifest and come out byte-identical. That keeps field order, list order and number types.
  - Absolute paths are made for the current checkout and `--output`.
  - The manifest stores the ImportTable RVA, the kind or crash index and `chain_rva`. `survey()` gives only import IDs.

**Never:**
- Change `mdl_normal.c`, `mdl_crash.c`, `mdl_module.c`, `mdl_crash_entry.S` or any harness `.c`.
- Change a break runner's mutations' meaning or verdicts.
- Run heavy runs: the MDL diff runs, the breaks and the MDL game runs. They wait for the 1.4 runs.
- Change `gen_mdl_fixtures.py` or `gen_window_data.py`. The fixtures are the harness's retail side, an `oracle:` read in `difftests`, not a module build.
- `rm -rf bin`.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected | Error Handling |
|---|---|---|---|
| Build | committed `src/mdl`, `orig/` resources | 11 MDLs identical (stripped `peh2`); harness inputs `cmp`-equal | N/A |
| `.data` not read | the build run against a copy of `orig/` whose MDLs have `.data` overwritten with `0xcc` | the same 11 MDLs | the builders read only `.rsrc` |
| Check | `tools/ofs mdl-images-check` | regenerated text equals the committed files | exit 1, naming the file and the first differing line |
| Shape drift | a moved or extra relocation in `.data` | the generator fails | ValueError naming the model and the slot |
| Edited image | a `data.inc` byte, a pointer target or Sopwith's literal | `data-images` fails; `UPDATE=1` warns, and the next build reruns | 18b's exit-3 handling |

</frozen-after-approval>

## Code Map

- `tools/build_mdls.py`:
  - `generate()` (`:23-102`) is the oracle logic and moves into `gen_mdl_images.py`;
  - `build()` (`:105`) links `model_data.S`;
  - the metadata writer is at `:122-154`, and it reopens retail for the ImportTable RVA at `:148`.
- `tools/gen_mdl_crash.py`: `chain_callback` (objdump of retail), `hand_record` (`:40-47`) and `build_hand`; it reopens retail at `:82`.
- Break runners:
  - `tools/mdl_module_breaks.py:30-58` mutates `model_data.S` (`.long` targets) and `model.data.bin`;
  - `tools/mdl_render_break.py:28` rewrites the `.incbin` paths;
  - `tools/mdl_crash_breaks.py:26,165` and `tools/mdl_crash_render_break.py:10,26` call `build_hand`.

  Give `build()` and `build_hand()` an image-directory argument, so a mutant builds a copy of `src/mdl/images/<NAME>/`, never the committed one.
- Consumers of `modules.json`: `stage_mdls.py`, `mdl_frames.py` and `mdl_crash_frames.py`. `gen_mdl_fixtures.py` writes its own `manifest.json`.
- To reuse:
  - `tools/gen_simsui_native.py:190` (the generated `.set` labels);
  - 18b's `data.inc` macros and its `--check`, which host-compiles and compares the bytes (`gen_gauges.py:399`);
  - the `DEPENDS_GLOB` and exit-3 stamp lines (`all.cmake:402-420`);
  - `ofs_step`, which sees only command-line tokens (`cmake/OpenFS98.cmake:33`).
- `tools/check_data_image.py`:
  - `at=` needs a section-defined COFF symbol (`:150`) and today allows only `.data`/`.rdata`;
  - both modes zero relocated words, and `--hash` keeps only the sites (`:627`, `:844`).
- `tools/raw_dumps_allowed.txt:7,14`.
- Docs: `docs/modules/mdl.md:166,239,299`, `docs/remaining-work.md:21-24`, `docs/tools.md`.

## Tasks & Acceptance

**Execution:**
- [x] `tools/gen_mdl_images.py` (new, `oracle:`):
  - `--write` emits each model's `src/mdl/images/<NAME>/{layout.h,data.inc}` and `src/mdl/mdl_manifest.json`;
  - `--check`, the default, regenerates and compares;
  - `--self-test` asserts that a moved slot and an extra slot are rejected.
- [x] `src/mdl/mdl_image.c` (new): the struct described above, chosen by `-Isrc/mdl/images/<NAME>`.
- [x] `tools/build_mdls.py`, `tools/gen_mdl_crash.py`:
  - read the manifest; read `orig/` only for `model.res`; no `survey()`, `Fs98Pe` `.data` reads or objdump;
  - compile `mdl_image.c` separately and link its object;
  - project the metadata;
  - change their `gen:` lines to `resource:`.
- [x] The four break runners: build their mutants from a copy of the image directory. Each mutation keeps its meaning: a pointer target, a `body` byte, a code symbol.
- [x] `tools/check_data_image.py`:
  - let `at=` read `.mdlimg`;
  - add a `linked` row flag: each of ours and retail's relocations in the window sits at the same offset; data targets match relative to the image; code targets fall in our `.text`; `--hash` also hashes the relative data targets;
  - add a negative self-test case for each.

  In `tools/data_images.txt`, add 11 `linked at=_mdl_data` rows, with SOPWITH's literal as an extra checked window (161 rows).
- [x] `cmake/steps/all.cmake`:
  - the MDL step's `DEPENDS_GLOB` covers `src/mdl/*`, `src/mdl/images/*/*`, the manifest, `tools/data_images.txt` and `orig/Aircraft/*/*.MDL`;
  - add a multi-pair `--hash` for the 11; exit 3 ages both `modules.json` files.

  In `cmake/targets.cmake`, add `mdl-images-check`.
- [x] Docs:
  - `docs/modules/mdl.md` and `docs/remaining-work.md`: where the data now lives;
  - `docs/tools.md`: `mdl-images-check`, `data-images` at 161, and raw dumps at 1 gen, 17 resource and 20 oracle.

**Acceptance Criteria:**
- Given the pre-slice outputs (`scratchpad/pre18c/`), when the tree builds (`all`, `difftests`), then:
  - the 11 stripped MDLs equal `pre18c/hashes.txt` column 2;
  - the unstripped ones equal column 1, or differ only in the COFF symbol table (say which);
  - `modules.inc`, both `modules.json`, `crashes.inc` and `fixtures.inc` are `cmp`-equal;
  - `mdldiff.exe`, `mdldlldiff.exe` and `mdlcrashdiff.exe` keep `peh2` hashes 2eee26d6636cb767, e9855b063f1562db and 0349144071c208d1.
- Given the built tree:
  - touching `mdl_image.c` or a `data.inc` reruns the MDL step;
  - editing that model's hash fails it;
  - deleting a DLL rebuilds it.
- Given the tree, `check_raw_dumps.py` reports 1 gen, 17 resource and 20 oracle; no `model.data.bin` or `.incbin` remains under `bin/mdl-*/<NAME>/`.

## Implementation Notes

- 2026-11-22, implemented by a Sonnet subagent and re-verified independently after a fresh rebuild of all 11 models:
  - All 11 stripped MDLs are equal to the pre-slice build.
  - The unstripped ones differ only in the COFF symbol table, which gains one symbol, `_mdl_data`.
  - `modules.inc`, both `modules.json`, `crashes.inc` and `fixtures.inc` are `cmp`-equal.
  - The three harness hashes are unchanged.
  - `mdl-images-check` and its self-test pass (5 cases).
  - `data-images` reports 161 rows, with a 173-case self-test after the review patches.
  - The raw-dump count is 1/17/20.
- The `.data` overwrite test (orig MDLs' `.data` set to 0xcc gives the same 11 DLLs) and the incremental-build ACs were run by the implementer.
- The break runners' mutants build, but have not been run under wine; they are held back for the 1.4 runs.
- `DEPENDS_GLOB src/mdl/*` is recursive (`GLOB_RECURSE`), so it covers `images/*/*` and the manifest.
- The 11 DLLs are now step outputs.
- Only `mdl_module_breaks.py` copies the image directory: the other three runners mutate C or asm and build the committed image read-only.

## Spec Change Log

## Review Triage Log

| # | Finding (Codex, spec review) | Verdict | Change |
|---|---|---|---|
| 1 | C adds `.ident` | high | `-fno-ident`, separate object |
| 2 | fixtures dump `.data`; `resource:` relabel proves nothing | medium (partly false) | fixtures stay `oracle:` (harness side, `difftests`), the "`.data` not read" scenario added, consumer list fixed |
| 3 | mutations edit `.S`/`.bin` | high | keep meaning and verdicts; image-dir override |
| 4 | incremental deps | medium | `DEPENDS_GLOB` widened; rebuild ACs. Harness `bin/` includes are already deferred (18b) |
| 5 | ImportTable RVA not in survey | medium | in manifest; projection rules |
| 6 | header is 0x24 at `+0x10`; `AT` ambiguous; `_mdl_data` | medium | exact members, `AT` defined, asserts, relative label |
| 7 | hash ignores targets, Sopwith | medium | `linked` flag, Sopwith window |
| 8 | "exactly 7" only inside `.data` | low | qualified |
| 9 | wrong reuse refs | low | corrected |

Code review (Codex: Blind Hunter, Edge Case Hunter, Verification Gap), 2026-11-22:

| # | Finding | Verdict | Evidence | Route |
|---|---|---|---|---|
| B1 | `--linked` does not tell code targets apart (swapping update and state passes) | low | `mdl_image.c` is source; `mdl_module_breaks` `update-pointer` is exactly this swap, and the harness catches it. Pinning a symbol per slot adds a COFF-symbol lookup | reject |
| B2 | with `at=`, the export offsets are not validated | low | the `.set` values come from `layout.h` (`MDL_IMPORT_RVA`/`MDL_LINKAGE_RVA`), which `mdl-images-check` re-proves against retail; the harness binds through the exports | reject |
| B3 / E1 | duplicate relocations pass (sets) | low | real at `check_data_image.py:660`; the compiler cannot emit one, but the fix is one line | patch |
| B4 | Sopwith's window matches anywhere | low | `rdata=` is "exactly once" by design; the position is fixed by `layout.h` `MDL_LEAD [0x1037]` (re-proved by `--check`) and `_Static_assert` of the struct | reject |
| B5 | manifest `kind` not checked by the builders | low | `mdl-images-check` re-proves the manifest against retail | reject |
| B6 | `modules.inc`/`crashes.inc` are not step outputs | low | real: deleting one does not regenerate it. Trivial: add them to `OUTPUTS`. The harness dependencies on them are already deferred (18b) | patch |
| B7 | the image section's writable flag is not checked | false | the flags are the compiler's, and the gate shows the stripped DLLs identical; no reachable path changes them | reject |
| B8 | old `model.data.bin`/`model_data.S` are not removed in other build dirs | low | stale files are never read (no `.incbin` source remains); this tree is clean | reject |
| B9 / E3 | only `mdl_module_breaks` copies images | low | the other three mutate C or asm, not the image, so they need no copy; the crash runners rewrite `mdl_image.o` in the build dir with identical content (they already wrote `resource.o` there) | reject (claim corrected in the notes) |
| B10 | `data.inc` does not name its generator | low | `layout.h` does, and `data.inc` doesn't; a one-line fix | patch |
| E2 | an extra `*_N.MDL`/`*_C.MDL` in `orig/` is silently ignored | medium | `gen_mdl_images.py:paths()` filters to the known names; the old `build_mdls.py` required the stems to equal `NAMES` exactly | patch |
| G1 | the "`.data` not read" build is a manual check only | medium | real; Story 1.4's `resource:` category is self-declared in every slice (pre-existing, 16-18b) | defer |
| G2 | the incremental hash-gate rerun is a manual check only | low | the same pattern as 18b, done by hand here; automating it is new test infrastructure | reject |

## Verification

**Commands:**
- `cmake --build build/win32-retail && cmake --build build/win32-retail --target difftests`: expected success.
- `tools/ofs mdl-images-check`, `tools/ofs data-images`, `python3 tools/check_raw_dumps.py`: expected pass, 161 rows, 1/17/20.
- `python3 tools/gen_mdl_images.py --self-test`: expected pass.
