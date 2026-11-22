---
title: 'Story 1.4, slice 11: ADVDRV, SOUND, PANELS and CONTROLS data decompiled'
type: 'refactor'
created: '2026-11-20'
status: 'done'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43' # worktree held uncommitted Stories 1.2-1.4; file snapshot at ~/openfs98-verify/baseline-1-4k
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4i-facil-multip-fe-data.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4j-pointer-target-guard.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** ADVDRV, SOUND, PANELS and CONTROLS still generate their data from `orig/`. The generators are `gen_advdrv_data.py`, `gen_sound_data.py`, `gen_panels_data.py` and `gen_controls_data.py`, and they write six allowlisted dumps. All four already pass `check_data_image.py` against retail:
- ADVDRV: 0x32a0 bytes, 372 relocated, 0 ours;
- SOUND: 0x177c bytes, 152 relocated, 152 ours;
- PANELS: 0x16580 bytes, 394 relocated, 0 ours;
- CONTROLS: 0xc020 bytes, 70 relocated, 0 ours.

**Approach:** Use slice 9's method, under slices 8 and 10's guard.
- **Images:** each image becomes a typed packed struct at retail's offsets in the module's `.c`.
- **Tables the generators already write as C** become committed source, with their headers reworded so they no longer say "generated, do not edit". These are:
  - `controls_table.inc`, `controls_pointers.inc` and `panels_pointers.inc`;
  - `sound_consts.inc`;
  - ADVDRV's relocation list, split out of `advdrv_data.inc` into `advdrv_relocs.inc` with its macro names and order kept.
- **`.rdata` copies:**
  - PANELS and CONTROLS get `rdata=` ranges.
  - ADVDRV's constant pool becomes typed hex-float doubles: our DLL holds no whole copy of it, because gcc folds the constants into the code.
  - SOUND's doubles are already typed (`sound_consts.inc`).
- **Pointer lists** (decision): `ptrs=` covers the complete lists only:
  - ADVDRV, through `advdrv_relocs.inc`. `ADVDRV_CODE(off, retail VA)` gives target RVA = VA − base; `ADVDRV_DATA(off, target)` gives image start + target.
  - PANELS, through `panels_pointers.inc`. `C(at, to)` gives `.text` RVA + to; `L(at)` gives image start + 0xc, the GLOBAL link.

  Extend `check_data_image.py`'s parser with these forms, plus self-test cases. CONTROLS (44 of 70 slots listed, the rest set in C) and SOUND (static pointers, all ours) get no `ptrs=`. They stay under the deferred symbol-per-slot check.
- **Deleted:** the four generators, the six dumps and their build lines.
- **Added:** four manifest rows (`UPDATE=1`) and four `--hash` lines.

**Gate:**
- each DLL normalized-identical to its pre-slice build;
- these other builds of the same sources normalized-identical too:
  - `bin/difftest/advdrvdiff.exe` (compiles the relocation list);
  - ADVDRV's coverage build (`-DADVDRV_COVER`, the gcc line of `advdrv-cover`, compiled to a scratch path; the target itself also runs `advdrvdiff`, so don't run it);
  - one ADVDRV `-DBREAK=N` build whose break uses `K()`, compiled to a scratch path;
  - `PANELS_unit.dll` (`panelsunit`).

  CONTROLS has no separate harness build: `controlsdiff` copies `bin/CONTROLS.DLL`, and `controls-break-build` compiles mutant copies left in `bin/difftest/`.
- `data-images` passes with 19 modules.

If a DLL's code changes, report the per-function diff and stop for a human decision.

**Queued until Story 1.3's Baselines run finishes:**
- `advdrvdiff-run`, `sounddiff-run`, `controlsdiff-run`, `panelsdiff-run N=3000` and `panelsunit-run`;
- `controls-breaks` and `panels-breaks`.

## Boundaries & Constraints

**Always:**
- Use slice 9's conventions.
  - Packed structs with `_Static_assert`s. Each object keeps its current alignment, section and position:
    - the ADVDRV and PANELS images are aligned(16);
    - `controls_rdata` is aligned(8), and stays writable and in `.data`, because `controls.c` writes its +0x264 slot.
  - Every offset access stays unchanged.
  - `D`, `RDATA`, `RD()` and `K()` keep their values. Where a header defines them through `.bytes` (`panels.h`, `controls.h`, used by `controls_input.c` too), either keep a `bytes` view through a union, or redefine them from the object's address as slice 9 did. A typed constant pool keeps byte-offset `K()`, by casting to `const uint8_t *`.
  - Hex floats for floating-point values, and names for fields whose meaning is known.
  - Check (a) must flag none of the new C. Brace-wrapping scalars to dodge it is not allowed. A run of one repeated value uses a range designator.
  - If check (a) surfaces a numeric table with no known formula, it goes in its own typed file with one allowlist entry giving the reason, as `fe_bands.inc` did.
- Strings stay string literals. `tools/fly_scripts.py:182` parses `controls_table.inc`, so keep its record format.
- Read each generator's docstring and asserts before deleting it, and carry its field knowledge into comments.
- Never `rm -rf bin`. No staging or commits. Record the files in `~/openfs98-verify/story-files.md`.

**Never:**
- No module or harness behaviour change, and no other module.
- No heavy runs (the queued list above).

</frozen-after-approval>

## Code Map

- **Includes to replace with typed definitions:**
  - `src/advdrv/advdrv.c:283` (`ADVDRV_RDATA`, `advdrv_rdata[]` read through `K(va)`), `:4381` (`ADVDRV_IMAGE`) and `:4511-4517` (`ADVDRV_RELOCS`, `code_for(retail)`).
  - `src/sound/sound.c:206` (`sound_consts.inc`, which stays) and `:2355` (`sound_data.inc`: `void *` words from +0x78; image pointers as offsets, code pointers as functions).
  - `src/panels/panels.c:5669-5673` (`panels_data.inc` is 0x1e00 initialised of 0x16580; `panels_rdata.inc` is 0x1fb bytes, doubles and floats to +0x1a0, then retail's export directory), and `:5776-5787` (pointers, which stay).
  - `src/controls/controls.c:4084` (the table, which stays), `:4124` (`controls_data.inc`, 0xc00 initialised of 0xc020), `:4128` (`controls_rdata.inc`, 0x2b8 bytes; the slot at +0x264 is zero and installed at `:4161`) and `:4132` (pointers, which stay).
- **Headers that define the image types and macros:**
  - `src/panels/panels.h:21-28`;
  - `src/controls/controls.h:22-41`, also included by `controls_input.c`, which uses `D` and `RD()` 173 times;
  - ADVDRV's `D` is `advdrv_image.bytes` (`advdrv.c:52`).
- **`K()` users inside `#if BREAK` blocks:** `advdrv.c:1014`, `:2065`, `:2183` and `:2894`.
- **Field layouts:** from each generator's docstring.
  - ADVDRV:
    - opcode table at +0xd8;
    - operator pairs at +0x198;
    - 176 variable descriptors at +0x318;
    - weather table at +0x10f8;
    - skip table at +0x1230.
  - SOUND:
    - sub-sounds at +0x78;
    - 29 templates at +0x378;
    - Morse table at +0xfe8;
    - state from +0x1128.
  - PANELS: strings, then 16-byte gauge-variable records from +0x610 `{fn, GLOBAL link, offset, size}`.
  - CONTROLS:
    - state from +0xe0;
    - section names;
    - DirectInput object table at +0x510;
    - force-feedback key names.
- **Retail layouts:**

  | Module | Base | `.text` | `.rdata` | `.data` |
  |---|---|---|---|---|
  | ADVDRV | 0x20610000 | 0x1000 | 0x9000 | 0xa000 |
  | SOUND | 0x208d0000 | 0x1000 | 0x5000 | 0x6000 |
  | PANELS | 0x10000000 | 0x1000 | 0xe000 | 0xf000 |
  | CONTROLS | 0x20670000 | 0x1000 | 0xd000 | 0xe000 |

  All four images have `off=0`.
- **Probed `rdata=` ranges.** Each matches once in today's build:
  - PANELS `0xe000:0x1fb`;
  - CONTROLS `0xd000:0x264` and `0xd268:0x50`.

  ADVDRV's `0x9000:0x300` is found 0 times, and retail has no relocations in it.
- **`tools/check_data_image.py`:** in `PTR_RES`/`parse_ptrs`, add the forms `ADVDRV_CODE(off, va)`/`ADVDRV_DATA(off, tgt)` and `C(at, to)`/`L(at)`.
  - `ptrs_check:191` takes the target base from `form.split("_")[2]`, which raises IndexError on the new forms. Replace it with a form → base table:
    - `_REL_DATA`, `ADVDRV_DATA` and `L`: `.data` RVA;
    - `_REL_TEXT`: 0;
    - `ADVDRV_CODE`: −base (the value is a VA);
    - `C`: `.text` RVA.
  - `L` carries no target; store it as 0xc.
  - Hash each entry as `(form, off, target)`.
  - Add one self-test `listcase` for a mistyped target on ADVDRV and one on PANELS. `listcase` needs each list to start with a `/* */` header (`body.index("*/")`), and `parse_ptrs` accepts entry lines only. So `advdrv_relocs.inc` is a header plus entries, with no `#ifdef` guard.
- **Retail checks (the review, against the retail DLLs):**
  - ADVDRV has 372 `.data` relocations, all listed: every `ADVDRV_CODE` equals its retail slot, and every `ADVDRV_DATA` equals 0x2061a000 + target.
  - PANELS has 394 = 83 `C` + 311 `L`: every `C` equals 0x10001000 + to, and every `L` equals 0x1000f00c.
  - ADVDRV's pool is 96 finite doubles, all read through `K()` at 8-aligned offsets.
- **`cmake/steps/all.cmake`:**
  - remove the generator lines at `:18` (ADVDRV), `:67` (CONTROLS), `:198` (PANELS) and `:227` (SOUND); re-read the line numbers first;
  - add a `--hash` line after each link.
- **`tools/difftest/advdrvdiff.c:81-92`:** point both includes at `advdrv_relocs.inc`.
- **Harness builds to hash before and after:**
  - `bin/difftest/advdrvdiff.exe` (`cmake/steps/difftests.cmake:37`);
  - the coverage build: the gcc line of `advdrv-cover` (`cmake/targets.cmake:898`), run by hand with `-o` set to a scratch path;
  - a `-DBREAK=N` build of `advdrv.c`, compiled to a scratch path, choosing an N whose block uses `K()`;
  - `PANELS_unit.dll` (`panelsunit`, `:1717`). Today's copy dates from October, so rebuild it from the pre-slice tree first.
- **`tools/raw_dumps_allowed.txt`:** remove the four `gen:` entries and the six dump entries. `check_raw_dumps.py` should then print `12 allowed dump files, 44 scripts reading orig/ (18 gen, 10 resource, 16 oracle)`, plus any new entry from the rule above.
- **Docs:**
  - the `docs/tools.md` Baselines `data-images` line, and its "fifteen" at `:25`;
  - the fifteen in `docs/building.md`;
  - `docs/modules/{advdrv,sound,panels,controls}.md`, where they name the generators;
  - the generator mentions at `advdrv.c:25`, `controls.c:16`, `panels.c:11` and `cmake/OpenFS98.cmake:3-5`;
  - `deferred-work.md`:
    - the queued runs;
    - CONTROLS and SOUND under the symbol-per-slot entry;
    - a new entry: the difftests step depends only on `tools/difftest/*.inc` (`cmake/OpenFS98.cmake:43-47`), so a later edit to `src/advdrv/advdrv_relocs.inc` won't rebuild `advdrvdiff.exe`.

## Tasks & Acceptance

**Execution:**
- [x] Rebuild and record the normalized hashes of the four DLLs and the four harness builds from the pre-slice tree. Use `peh.py` at `/tmp/claude-1000/-home-steve-source/b2e3078b-1d77-426e-816f-2de2772a67dc/scratchpad/peh.py`.
- [x] `tools/check_data_image.py`: add the new list forms and their self-test cases.
- [x] For each module in turn:
  - add the typed definitions, plus the committed tables and splits;
  - delete the generator, its dumps and its build line;
  - add the `--hash` line, together with the module's manifest row, still without hashes. Build with `UPDATE=1` until the hashes are filled in: `run_hash` fails on a missing row, and fails on a row with no hashes unless it is warning;
  - run `tools/ofs all` and the four harness builds;
  - check identity and `check_data_image.py`.
- [x] `tools/data_images.txt`: give the four rows their `ptrs=` and `rdata=` fields as above, then run `tools/ofs data-images UPDATE=1`, then a clean `tools/ofs all` without `UPDATE`.
- [x] Update the allowlist, the docs and `deferred-work.md`.

**Acceptance Criteria:**
- Given `tools/ofs all` and the four harness builds, when the four DLLs and four harness builds are normalized and hashed, then each equals its recorded hash.
- Given `tools/ofs data-images`, when it runs, then it reports 19 images identical, 7 `.rdata` ranges, 5 pointer lists and the self-test passed.
- Given the tree, when `check_raw_dumps.py` runs, then it exits 0 with the counts above.
- Given one target in `src/panels/panels_pointers.inc` seeded wrong, when PANELS.DLL is rebuilt, then the build fails at its `--hash` step. Reverting the edit restores the build.

## Implementation Notes

- Typed packed structs for all four images, emitted from the old dumps by a scratch generator (not committed), then pasted into each `.c`. Pointers in SOUND's image are `&snd_image.<member>` and `(snd_..._fn)function`; ADVDRV's, PANELS' and CONTROLS' slots are zero and installed at attach, as before.
- PANELS: the 512-entry u16 table at +0x1b6 is `(n - 256)^2` truncated to 16 bits, so it is written with `SQ16()` macros, not literals. `panels_rdata` is a typed `const struct` (doubles, two floats, retail's export directory as fields). `panels.h` and `controls.h` redefine `D` and `RDATA` from the object's address and declare the structs incomplete.
- CONTROLS: `controls_rdata` keeps `aligned(8)` on the object, not the type. On the type, gcc's aggregate-alignment bump (32) moved it and the image, and changed the `.text` addresses.
- ADVDRV: `advdrv_rdata` is `static const double[0x60]` with `K()` casting to `const uint8_t *`; gcc still folds it away.
- Deviation: the normalized comparison also masks the number in gcc's local `_CSWTCH.N` symbol names (the COFF string table). Adding struct types shifts gcc's declaration counter, which renumbers them; no section changed. The masking is in the scratch `peh2.py`, not in the repository.
- Normalized sha256 (first 16 hex digits), before and after, all identical: `bin/ADVDRV.DLL` ca38b16739e1593d, `bin/SOUND.DLL` 3dc65efd67ef89ed, `bin/PANELS.DLL` 87ef3b3b8d536bd3, `bin/CONTROLS.DLL` 118d855f8f426eb3, `bin/difftest/advdrvdiff.exe` 568a721e6262ae0c, ADVDRV coverage build b4f4ddc84f5f6fcc, ADVDRV `-DBREAK=2` build bc9bbfce187102b5 (BREAK=1's block is dead code, so that build equals the plain DLL), PANELS_unit.dll 8fdb37d963f25ea3.
- `tools/ofs data-images` prints `data-images: 19 .data images identical (224782 bytes, 1609 relocated), 7 .rdata ranges identical (2221 bytes), 1 switch tables, 5 pointer lists (971 entries), self-test passed (40 cases)`. `check_raw_dumps.py` prints `12 allowed dump files, 44 scripts reading orig/ (18 gen, 10 resource, 16 oracle)`, with no new entry.
- Negative AC: a seeded `C(0x006c, 0x8e70)` in `panels_pointers.inc` failed the PANELS build at `--hash`; reverting restored it.
- Not run: the queued difftests and break suites (`deferred-work.md`).
- Main-session check: under plain `peh.py`, SOUND, PANELS and CONTROLS equal s14 exactly.
  - ADVDRV differs from s14 in 11 bytes: the COFF timestamp, the checksum, the export timestamp, and the digits of one local symbol name, `_CSWTCH.463`, in the COFF string table.
  - Every section, `.text`, `.data`, `.rdata`, `.reloc`, `.idata`, `.rsrc` and `.bss`, is byte-identical, and so is `.edata` once its timestamp is zeroed.

## Spec Change Log

## Review Triage Log

Pass 1 (Blind Hunter, Edge Case Hunter, Verification Gap; Sonnet). BH = Blind Hunter, ECH = Edge Case Hunter, VG = Verification Gap.

| # | Finding | Verdict | Evidence | Route |
|---|---|---|---|---|
| 1 | ECH/BH: no per-build guard covers ADVDRV's 96 typed doubles (or SOUND's `sound_consts.inc`) | medium | gcc folds them into our literal pool, so no `rdata=` window exists (probe: found 0 times); a mistyped literal changes `.text`/`.rdata` outside every hashed range | defer: needs a new mechanism, such as a retail-side check of the parsed pool |
| 2 | ECH: the pool could hold -0.0, NaN or padding that hex floats can't express | false | the spec review counted 96 finite doubles, no -0.0/NaN/subnormal; the DLL is identical | reject |
| 3 | VG/BH/ECH: `advdrvdiff.exe` doesn't rebuild when `advdrv_relocs.inc` changes | low | real (`cmake/OpenFS98.cmake:43-47` globs only the tools dir); the fix is a dependency-scan change, not one line, and it's already in deferred-work | reject (already deferred) |
| 4 | VG/BH/ECH: CONTROLS and SOUND pointer targets are unguarded | medium | real; the frozen intent decided no `ptrs=` for them, under the symbol-per-slot entry | reject (out of scope by intent, deferred) |
| 5 | VG: behaviour difftests and break suites not run | medium | the frozen intent queues them until Story 1.3 finishes | reject (scheduled) |
| 6 | ECH/BH: `C` is a generic form; any module's list accepts every form | low | a wrong-form line still has to match retail's value at a relocated slot, so a mismatch fails | reject |
| 7 | VG/BH: no self-test mutates an `L(...)` line | low | true; case 39 hits a `C` line | patch |
| 8 | BH: `docs/modules/controls.md:70` names the deleted generator | low | true | patch |
| 9 | BH: `docs/modules/sound.md:198` says the constants are generated | low | true | patch |
| 10 | BH: the building.md example swapped to `gen_main_data.py` doesn't fit | false | MAIN is in that paragraph's list of rebased modules, and `gen_main_data.py:60` stops with that message | reject |
| 11 | BH: the OpenFS98.cmake comment is wrong, and the manifest and list aren't step inputs | false | `.inc` files in `src/<m>/` are globbed (slice 10's seeded test); "not when only the manifest changes" is documented since slice 8 | reject |
| 12 | BH: no `_Static_assert` ties the list offsets to struct fields | low | the retail check proves each slot, and the image hash proves the layout | reject |
| 13 | BH: no one ran the retail-side `data-images` | false | it ran against `orig/`: 19 images identical (Implementation Notes) | reject |
| 14 | BH: placeholder field names (`f_*`, `k_*`) aren't tracked | low | naming backlog, no behaviour impact | reject |

## Verification

**Commands:**
- `tools/ofs all`, then `panelsunit`, the coverage line and the `-DBREAK` line, then `peh.py` over the four DLLs and four harness builds -- expected: the recorded hashes.
- `tools/ofs data-images` -- expected: PASS, with 19 images.
- `python3 tools/check_raw_dumps.py` -- expected: exit 0, with 18 gen.
