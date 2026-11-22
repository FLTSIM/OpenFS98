- source_spec: none
  summary: Story 1.2 piece 2: re-verify every difftest, break suite, smoke, frames and dialog target through CMake against its baseline in docs/tools.md.
  evidence: Split from Story 1.2 on 2026-11-16: about 340 targets, some run 25-40 minutes under wine; hours of wall time and separable by module family.
- source_spec: none
  summary: Story 1.2 piece 3: update docs/tools.md, docs/building.md and AGENTS.md for CMake, then delete the Makefile and tools/*.mk. (Taken up 2026-11-18 as spec-1-2c-retire-the-makefile.md.)
  evidence: Split from Story 1.2 on 2026-11-16: only safe once the targets are ported and re-verified (pieces 1 and 2).
- source_spec: spec-1-2b-verify-baselines-through-cmake.md
  summary: Open break coverage found by the CMake re-verification (PANELS' 12 mutants missed at N=3000; SIMSUI init-far and char-digit-end; MAIN open-id/open-answer and G3D plane-facet-skip crashing). Not caused by the port.
  evidence: docs/remaining-work.md "Open from the CMake re-verification"; logs in ~/openfs98-verify/p3-results (panels-breaks-3000.log, simsui-3-2000.log, simsui-probe2.log).

- source_spec: `docs/implementation/spec-1-4e-booster-weatherm-demo-data.md`
  summary: Run `tools/ofs boosterdiff-run` (~75 min), `tools/ofs weathermdiff-run` and `tools/ofs demodiff-run` (~25 min) after Story 1.3's Baselines run has finished.
  evidence: expected 20000 scenarios, 0 differ each. Slice 5 proved only the .data images byte for byte; a wrong function pointer in a correct slot is seen only by the difftests.

## Deferred from: code review of spec-1-2-cmake-targets-for-every-tools-md-target.md (2026-11-19)

- source_spec: spec-1-2-cmake-targets-for-every-tools-md-target.md
  summary: Break runners other than the nine already fixed (WINDOW, WINMSG, REALITY, ANALYSIS, MPCHAT, HOST, G3D, PANELS, CONTROLS) may still count a harness crash as a caught break.
  evidence: Unverified; read each remaining tools/*_breaks.py verdict test.
- source_spec: spec-1-2-cmake-targets-for-every-tools-md-target.md
  summary: No regression test for tools/modules.sh output or tools/ofs argument handling.
  evidence: modules.sh matched `make -s` for every query before the Makefile was deleted; a table test would guard later edits.
- source_spec: spec-1-2-cmake-targets-for-every-tools-md-target.md
  summary: Generated src/**/*.inc files are not CMake step inputs (shortcut in cmake/OpenFS98.cmake); World's and the slice-3 table files (MULTIP, ALERTS, HELP, VIEWS, FLIGHTS, SIM1) are plain sources now. Story 1.4.
  evidence: cmake/OpenFS98.cmake header comment.

## Deferred from: code review of spec-1-3-one-flag-set-for-every-unit.md (2026-11-19)

- source_spec: `docs/implementation/spec-1-3-one-flag-set-for-every-unit.md`
  summary: Nothing fails if a codegen override (-fno-ipa-icf, -fno-tree-loop-distribute-patterns, -fno-tree-vectorize, -fno-math-errno) is later dropped from one of its sites; the toolchain table is documentation only.
  evidence: medium, unverified as a harm. Without -fno-ipa-icf, DEMO's code changes but demodiff still gives 0 differ, so a dropped override may go unnoticed. A section comparison against a reference build per site would settle it (Story 1.4's image checks are the natural home).

## Deferred from: Story 1.4, slice 1 (WINMSG and MPCHAT data decompiled, 2026-11-19)

- source_spec: `docs/implementation/spec-1-4-module-data-decompiled-no-raw-dumps-in-git.md`
  summary: Every other module's data image is still a raw dump or is generated from `orig/` by a `tools/gen_*.py`. Later slices convert them and shrink the allowlist to empty.
  evidence: `tools/raw_dumps_allowed.txt` is the list (`python3 tools/check_raw_dumps.py` fails when an entry stops being an offender).
- source_spec: `docs/implementation/spec-1-4-module-data-decompiled-no-raw-dumps-in-git.md`
  summary: tools/check_raw_dumps.py only scans src/ and include/; several harness fixtures in tools/difftest/ (panels_funcs.inc, hg2d_draw_suite.inc, multip_send.inc, g3ddiff.c) hold long literal runs that may be retail data.
  evidence: the review's wider rule found 64–270-literal runs there; whether each is retail data or a hand-made vector needs a look per file.
- source_spec: `docs/implementation/spec-1-4-module-data-decompiled-no-raw-dumps-in-git.md`
  summary: RESOLVED by Story 1.4 slice 2. Check (c) only saw generators whose command line names orig/Modules and src/; gen_world_advanced.py and other *_tables/*_runtime generators read orig/ inside the script and were not listed.
  evidence: check (c) now covers every tools/*.py that CMake runs and that reads orig/, directly or by import (`tools/raw_dumps_allowed.txt`); World's generators are gone.
- source_spec: `docs/implementation/spec-1-4-module-data-decompiled-no-raw-dumps-in-git.md`
  summary: Nothing stops an entry from being added to tools/raw_dumps_allowed.txt, and check_data_image.py does not compare our image's own size (the structs' _Static_assert does for WINMSG and MPCHAT).
  evidence: once the allowlist is committed, the check can fail when an entry is not in `git show HEAD:tools/raw_dumps_allowed.txt`; later modules should keep the sizeof assert or the tool should check the export's section bounds.
- source_spec: `docs/implementation/spec-1-4b-generators-reading-orig-and-world-tables.md`
  summary: Scripts reached only through a `tools/*.sh` wrapper are not seen by check (c).
  evidence: the rule matches `tools/<name>.py` named in `cmake/steps/*.cmake` and `cmake/targets.cmake` only.
- source_spec: `docs/implementation/spec-1-4b-generators-reading-orig-and-world-tables.md`
  summary: The 14 remaining `gen:` entries after slice 13 (45 before slice 4, 34 before slice 5) in `tools/raw_dumps_allowed.txt` still need converting. Slice 4 moved the seven `*_runtime` scripts, `gen_host_resources` and `gen_main_bitmap` to `resource:`; `gen_options_tables` and `gen_sim_tables` are retired.
  evidence: `gen:` by module -- ACONTAIN, ADVDRV, ANALYSIS, ATC, BOOSTER, CONTROLS, DEMO, DYNAMIC, FACIL, FE, G3D, LANDME, PANELS, REALITY, SOUND, WEATHERM, WINDOW: `gen_<m>_data.py`; HOST: `gen_host_data` (also pins the reference `FLTSIM98.EXE` by SHA-256 for the host build), `gen_host_rt_tables`; MAIN: `gen_main_data`; MULTIP: `gen_multip_data`; SIM1: `gen_sim1_data/native`; SIMSUI: `gen_simsui_native` (embeds retail `.data` into `bin/`), `gen_simsui_tables`; MDLs and gauges: `build_mdls`, `gen_mdl_crash`, `gen_gauges`; test DLLs: `gen_warning_data`, `gen_suction_data`.
- source_spec: `docs/implementation/spec-1-4c-retail-table-generators.md`
  summary: One tables generator stays because another script reads it: `gen_simsui_tables` (`gen_host_rt_tables.py:40` imports helpers). `gen_options_tables` and `gen_sim_tables` were retired in slice 4, with the table halves of their readers.
  evidence: `gen_simsui_tables` goes later, together with `gen_host_rt_tables` and its `host_rt_rdata.inc` dump. `gen_sim1_native`, `gen_gauges`, `build_mdls` and `gen_mdl_crash` put retail data into `bin/` and stay `gen:` for the dump slices.
- source_spec: `docs/implementation/spec-1-4c-retail-table-generators.md`
  summary: RESOLVED by slice 13 (spec-1-4m-global-hg2d-data.md): GLOBAL's 194 default bytes are typed values in `global.c`, and `global_state.inc` and `gen_global_state.py` are gone.
  evidence: `tools/raw_dumps_allowed.txt`, tagged `(a)`.
- source_spec: `docs/implementation/spec-1-4d-runtime-and-resource-generators.md`
  summary: With the image checks removed from the resource: scripts, a pressing whose dialogs or other resources differ from the reference builds without error against the vtable and message-map tables committed for the reference image, and would fail only at run time.
  evidence: maybe-false, medium if true. Owned by Story 6.1 (its resources AC now names the committed tables); method and known facts in docs/building.md, "Resources from another pressing". The November pressing rebuilt MAIN (gen_main_bitmap) and FLTSIM98.EXE (gen_host_resources, still guarded by gen_host_data's pin). Settle it with tools/pressing_catalogue.py compare. If resources differ, add a per-module resource-hash check that names the pressing.
- source_spec: `docs/implementation/spec-1-4e-booster-weatherm-demo-data.md`
  summary: RESOLVED by slice 8 (spec-1-4h-data-images-guard.md, 2026-11-20): Add `tools/ofs data-images`, run after `all`, that runs every `check_data_image.py` line and fails on any difference. With the generators' value asserts gone, it is the only guard on the typed initialisers outside the Baselines run. List it as `oracle:` in the allowlist.
  evidence: today the five checks are Baselines lines only (`cmake_verify.py`), and nothing runs them on an ordinary build.
- source_spec: `docs/implementation/spec-1-4f-window-acontain-dynamic-data.md`
  summary: Run `tools/ofs windowdiff-run`, `tools/ofs window-breaks` (from a frozen copy of the tree), `tools/ofs acontaindiff-run` and `tools/ofs dynamicdiff-run` after Story 1.3's Baselines run has finished.
  evidence: not run in slice 6. WINDOW.DLL, ACONTAIN.DLL and DYNAMIC.DLL are normalized-identical to the pre-slice build (`~/openfs98-verify/s14/bin`) and `check_data_image.py` prints identical, so these are confirming runs.
- source_spec: `docs/implementation/spec-1-4f-window-acontain-dynamic-data.md`
  summary: WINDOW's masks and code tail (retail keeps them in `.text`; ours is a `.rdata` copy) have no retail oracle other than s14 identity and `windowdiff`. `gen_window_data.py` checks retail against its own literals, not against the typed C in `window.c`.
  evidence: `gen_window_data.py` still asserts retail's masks, zero block and pitch words against its own literals, but not against the typed C in `window.c`; the tail is copied from retail at build time.
- source_spec: `docs/implementation/spec-1-4f-window-acontain-dynamic-data.md`
  summary: Guard part RESOLVED by slice 8 (`tools/ofs data-images` and the per-build `--hash` check; a changed table now fails the build). STILL OPEN: the symbol-per-slot pointer check, because our DLLs carry no symbols. HG2D (`--mixed`) adds 40 unlisted zero code slots (+0x3c, +0x40, +0x50..+0xf4 apart from the listed +0xc4..+0xd0) that DllMain sets in C, and 112 raw retail code addresses kept as values; both pass the check unchecked. check_data_image.py zeroes relocated slots on both sides, so a swapped handler in DYNAMIC's 68-slot dispatch table, or a wrong Linkage, reader or handler pointer in any converted module, passes it. CONTROLS (44 of its 70 slots are listed in `controls_pointers.inc`, the rest are set in C) and SOUND (its pointers are static, all ours) have no `ptrs=` list and stay under this check too. Extend the planned `tools/ofs data-images`: resolve each of ours' relocated pointers to its symbol, and compare the symbol for each slot against a committed map of retail address to handler name. Alternatively, run the module difftests in the normal verification path.
  evidence: today's tables are proven by whole-DLL identity with the pre-slice build, whose generators asserted them; nothing guards a later edit except the difftests.

## Deferred from: Story 1.4, slice 7 (ANALYSIS, REALITY, LANDME and ATC data decompiled, 2026-11-20)

- source_spec: `docs/implementation/spec-1-4g-analysis-reality-landme-atc-data.md`
  summary: Run `tools/ofs atcdiff-run`, `landmediff-run`, `realitydiff-run`, `reality-breaks`, `analysisdiff-run` and `analysis-breaks` (breaks from a frozen copy of the tree) after Story 1.3's Baselines run has finished.
  evidence: not run in slice 7. The four DLLs are normalized-identical to the pre-slice build (`~/openfs98-verify/s14/bin`) and `check_data_image.py` prints identical for all twelve converted modules, so these are confirming runs.
- source_spec: `docs/implementation/spec-1-4g-analysis-reality-landme-atc-data.md`
  summary: RESOLVED by slice 8 (spec-1-4h-data-images-guard.md, 2026-11-20): The `.rdata` objects (ANALYSIS's nine doubles, REALITY's whole `.rdata`) have no retail oracle other than s14 identity and the difftests, because `check_data_image.py` compares `.data` only.
  evidence: the generators that asserted them against retail are gone.
- source_spec: `docs/implementation/spec-1-4g-analysis-reality-landme-atc-data.md`
  summary: RESOLVED by slice 8 (spec-1-4h-data-images-guard.md, 2026-11-20): Give `check_data_image.py` an `.rdata` mode, part of the planned `tools/ofs data-images`: compare retail's `.rdata` (ANALYSIS's first 0x48 bytes, the 9 doubles; REALITY's whole 0x19c bytes plus its zero pad) with ours, located by a symbol or export. Add a small mutate-and-expect-fail self-test for `--kept` and the edge paths.
  evidence: since slice 7 the generators' `.rdata` asserts are gone, so only s14 identity and the difftests check those objects.
- source_spec: `docs/implementation/spec-1-4h-data-images-guard.md`
  summary: Make `tools/data_images.txt` an input of the module steps (now fifteen), so a hand edit of the manifest alone reruns their `--hash` checks.
  evidence: `ofs_step` scans `python3` lines only for `tools/*.py` and `orig/…` tokens; until then `tools/ofs data-images` is the check after a manifest edit.
- source_spec: `docs/implementation/spec-1-4i-facil-multip-fe-data.md`
  summary: After Story 1.3's Baselines run, run `facildiff-run`, `facildiff-fe-run`, `fediff-run`, `fegriddiff-run`, `fesldiff-run`, `multipdiff-run` and `multip-breaks` (break suites from a frozen copy). Record `multipdiff-run`'s output as MULTIP's first Baselines line, since none exists yet.
  evidence: slice 9 was proved by normalized identity of FACIL/FE/MULTIP.DLL, facildiff.exe, fediff.exe, MULTIP_test.dll and FE_sl_ours.dll with the pre-slice build, and by `tools/ofs data-images`; the difftests were not run.
- source_spec: `docs/implementation/spec-1-4i-facil-multip-fe-data.md`
  summary: `src/fe/fe_bands.inc` (256 latitude-band edges and 257 cosine-like steps, typed u32) is allowlisted as data because no exact formula is known. Either derive a formula that reproduces both tables bit for bit, or confirm they stay data, then settle the allowlist entry.
  evidence: slice 9 decision (the user chose committed and allowlisted); `docs/implementation/spec-1-4i-facil-multip-fe-data.md`.
- source_spec: `docs/implementation/spec-1-4i-facil-multip-fe-data.md`
  summary: RESOLVED by slice 10 (spec-1-4j-pointer-target-guard.md, 2026-11-20; `ptrs=` and `switch=` fields, plus an `rdata=` range for `s_nametag`; the symbol-per-slot handler check stays open above): Fold the pointer targets of `facil_relocs.inc`, `fe_relocs.inc` and `multip_pointers.inc`, and the contents of `s_nametag` and `s_frame_actions`, into a guard. Either hash them with the image in `tools/data_images.txt`, or cover them by the symbol-per-slot pointer check.
  evidence: `--hash` covers image bytes and relocation offsets only; a changed target alters our DLL outside the hashed image.

## Deferred from: Story 1.4, slice 11 (ADVDRV, SOUND, PANELS and CONTROLS data decompiled, 2026-11-20)

- source_spec: `docs/implementation/spec-1-4k-advdrv-sound-panels-controls-data.md`
  summary: After Story 1.3's Baselines run, run `advdrvdiff-run`, `sounddiff-run`, `controlsdiff-run`, `panelsdiff-run N=3000`, `panelsunit-run`, `controls-breaks` and `panels-breaks` (break suites from a frozen copy).
  evidence: slice 11 was proved by normalized identity of ADVDRV/SOUND/PANELS/CONTROLS.DLL, advdrvdiff.exe, the ADVDRV coverage build, an `-DBREAK=2` ADVDRV build and PANELS_unit.dll with the pre-slice build, and by `tools/ofs data-images`; none of the difftests was run.
- source_spec: `docs/implementation/spec-1-4k-advdrv-sound-panels-controls-data.md`
  summary: The difftests step depends only on `tools/difftest/*.inc` and `*.h` (`cmake/OpenFS98.cmake:43-47`), so a later edit to `src/advdrv/advdrv_relocs.inc` will not rebuild `bin/difftest/advdrvdiff.exe`, which compiles it. Add the file as a dependency of that step, or rebuild by hand.
  evidence: `tools/difftest/advdrvdiff.c` includes `../../src/advdrv/advdrv_relocs.inc` twice (the code and data slot tables).
- source_spec: `docs/implementation/spec-1-4k-advdrv-sound-panels-controls-data.md`
  summary: Guard ADVDRV's 96 typed `.rdata` doubles (`advdrv_rdata` in `src/advdrv/advdrv.c`), SOUND's `sound_consts.inc` and NUMERIC's `fs98_numeric_rdata` (seven bit patterns, indices 0 and 3 read; slice 12) against later edits. gcc folds them into our literal pool, so no `rdata=` window exists. A retail-side check could parse the pool and compare it with retail's `.rdata`, with a per-build hash of the parsed values.
  evidence: slice 11 review; `--rdata 0x9000:0x300` finds 0 matches in ours; today the values are proven only by whole-DLL identity with the pre-slice build.

## Deferred from: Story 1.4, slice 12 (NUMERIC and UTIL data decompiled, 2026-11-20)

- source_spec: `docs/implementation/spec-1-4l-numeric-util-data.md`
  summary: After Story 1.3's Baselines run, run `utildiff-run`, `utildiff-portable`, `numericdiff-run` and `ofstest-run`.
  evidence: slice 12 was proved by normalized identity of NUMERIC.DLL, UTIL.DLL, numericdiff.exe, utildiff.exe, utildiff_portable.exe, slewdiff.exe, OFSTEST.DLL and UTIL_TRACE.DLL with the pre-slice build, and by `tools/ofs data-images`; none of the difftests was run.
- source_spec: `docs/implementation/spec-1-4l-numeric-util-data.md`
  summary: RESOLVED by slice 12, without changing module code paths (the data initialisers did change): NUMERIC and UTIL were thought to need code changes before their data could be checked, because their `.data` copies are not found through the `ImportTable` export. The `at=SYMBOL` manifest field finds each image by its COFF symbol instead, and both DLLs are normalized-identical to the pre-slice build.
  evidence: `tools/data_images.txt` rows NUMERIC (14 kept slots) and UTIL (83 kept slots).
- source_spec: `docs/implementation/spec-1-4l-numeric-util-data.md`
  summary: The cos/asin/atan tables are still numbers (`src/util/util_trig.inc`) because C99 cannot evaluate them in a static initialiser. The latitude-circle table (78 entries) has no exact formula. Regions still undescribed, typed as words: UTIL +0x5ae (2 bytes), +0x6ec..+0x700 and +0x780..+0x7e8; NUMERIC +0x158..+0x198 (eight 64-bit words, unread by our code).
  evidence: slice 12; each is marked "meaning not yet known" in `src/util/util_math.c` and `src/numeric/numeric.c`.
- source_spec: none
  summary: RESOLVED by slice 14 (spec-1-4n-g3d-data-strict-mixed.md, 2026-11-21): G3D's data image is typed C (`src/g3d/g3d_image.inc`, `g3d_pointers.inc`), `gen_g3d_data.py` and `g3d_data.inc` are gone, row G3D in `tools/data_images.txt`.
  evidence: split from "remaining generators" (2026-11-21): 378 retail slots, 176 zero and installed, 202 holding retail's raw value; 292 kB dump.
- source_spec: none
  summary: RESOLVED by slice 15 (spec-1-4o-main-data.md, 2026-11-21): MAIN's data image is typed C (`src/main/main_image.inc`, `main_pointers.inc`), `gen_main_data.py` and `main_data.inc` are gone, `main_start.inc`'s menu lists are grouped by menu, row MAIN in `tools/data_images.txt`. `main_text_tail.inc` stays allowlisted (retail's code and `.rdata`) behind `tail=` until MAIN-6/MAIN-18's season index is bounded at porting.
  evidence: split from "remaining generators" (2026-11-21): 635 retail slots, 502 zero, 133 raw; about 480 kB of dumps.
- source_spec: none
  summary: RESOLVED by slice 16 (spec-1-4p-host-data.md, 2026-11-21): HOST's `.data` is typed C (`src/host/host_image.inc`), `.rdata` typed C (`host_rt_rdata.inc`), `host_rt_tables.inc` committed source; `gen_host_data.py`, `host_data.inc` and `host_pointers.inc` are gone, `gen_host_rt_tables.py` is the oracle behind `tools/ofs host-rt-tables-check`, and row HOST in `tools/data_images.txt` uses the new `rawptrs=` field.
  evidence: split from "remaining generators" (2026-11-21).
- source_spec: `docs/implementation/spec-1-4p-host-data.md`
  summary: After Story 1.3's Baselines run, run `hostdiff-run`, `host-breaks` (from a frozen copy), `hostrtdiff-run` and `host_rt_breaks.py` (from a frozen copy) (HOST's data is now typed C; slice 16 built it and ran none of them; `HOST.DLL` is identical to a build of the baseline tree, `bin/OPENFS98.EXE`, `hostrtdiff.exe` and `hostdiff.exe` to their pre-slice builds).
  evidence: queued by the frozen intent.
- source_spec: `docs/implementation/spec-1-4m-global-hg2d-data.md`
  summary: After Story 1.3's Baselines run, run `hg2ddiff-run`, `hg2dmodeldiff-run` and `hg2d-breaks` (HG2D's data is now typed C; slice 13 built it but ran none of them).
  evidence: queued by the frozen intent.
- source_spec: none
  summary: RESOLVED for SIM1 by slice 17 (spec-1-4q-sim1-data.md, 2026-11-21): SIM1's `.data` and `.rdata` are typed C (`src/sim1/sim1_data.inc`, `sim1_rdata.inc`, shared by the harness DLLs and the native build), `gen_sim1_native.py` only extracts the resources, `gen_sim1_data.py` is the setup check `tools/ofs sim1-reference`, and row SIM1 is in `tools/data_images.txt`. SIMSUI (`gen_simsui_native.py`, `gen_simsui_tables.py`) is slice 17b below.
  evidence: split from "remaining generators" (2026-11-21).
- source_spec: `docs/implementation/spec-1-4q-sim1-data.md`
  summary: After Story 1.3's Baselines run, run `sim1diff-run`, `sim1rdiff-run`, `sim1-breaks`, `sim1r-breaks`, `sim1air-run`, `sim1air-breaks`, `sim1state-run`, `sim1state-breaks` and `sim1native-flights` (SIM1's data is now typed C; slice 17 built it and ran none of them; `bin/SIM1.sim` and `sim1rdiff.exe` are identical to their pre-slice builds, the three test DLLs to a build of the baseline tree).
  evidence: queued by the frozen intent.
- source_spec: none
  summary: Story 1.4 slice 18: the gauges, MDLs and test gauges (`gen_gauges.py`, `build_mdls.py`, `gen_mdl_crash.py`, `gen_warning_data.py`, `gen_suction_data.py`); then revisit `gen_window_data.py`, which the user chose to keep as a build step in slice 6.
  evidence: split from "remaining generators" (2026-11-21).
- source_spec: `docs/implementation/spec-1-4m-global-hg2d-data.md`
  summary: RESOLVED by slice 14 (spec-1-4n-g3d-data-strict-mixed.md, 2026-11-21: `cset=` and the new self-test cases). Was: tighten `--mixed` before G3D and MAIN use it. Give the row an explicit list of the zero slots set in C (HG2D's 40 at +0x3c..+0xf4), so that a zero slot must be listed in `ptrs=` or in that list. Add self-test cases for a raw slot turned zero and a deleted pointer entry.
  evidence: slice 13 Codex review: `--mixed` accepts zero in any slot not listed, so `UPDATE=1` would bless a deleted raw initialiser or pointer entry. The per-build hash still catches the change.
- source_spec: `docs/implementation/spec-1-4m-global-hg2d-data.md`
  summary: Guard `src/hg2d/hg2d_projective_tables.inc`, the tail tables from retail's `.text` whose words are retail code addresses, with a check against retail's `.text` at the addresses its header lists, and a per-build hash.
  evidence: slice 13 Codex review; once `gen_hg2d_data.py` is gone, only DLL identity proves them.
- source_spec: `docs/implementation/spec-1-4n-g3d-data-strict-mixed.md`
  summary: After Story 1.3's Baselines run, run `g3ddiff-run`, `g3ddiff-meshc-run`, `g3dmeshdiff-run` and `g3d-breaks` (G3D's data is now typed C; slice 14 built it, matched `G3D.DLL` and `HG2D.DLL` normalized, and ran none of them).
  evidence: queued by the frozen intent.
- source_spec: `docs/implementation/spec-1-4n-g3d-data-strict-mixed.md`
  summary: Revisit HG2D's `U8` regions (`src/hg2d/hg2d_image.inc`: glyph bitmaps, byte ramps, tables marked "meaning not yet known") the way G3D's image was typed in slice 14: by structure (names, counts, dwords, words, instruction lines) rather than by width alone. G3D's remaining untyped regions (the scenery models +0x4f22..+0x64d2 and the tails after the two programs, written as words) wait for their meaning.
  evidence: slice 13 review finding 8; slice 14 left HG2D untouched by its frozen intent.
- source_spec: `docs/implementation/spec-1-4n-g3d-data-strict-mixed.md`
  summary: Make each module's build-time `check_data_image.py --hash` step depend on `tools/data_images.txt`, so that editing a row's hash, `cset=` or `ptrs=` alone reruns the check in an incremental build.
  evidence: slice 14 code review; `cmake/OpenFS98.cmake:34` keeps only `tools/*.py` and `orig/` as a python step's inputs. It has been the same for every row since slice 8; `tools/ofs data-images` still catches the edit.
- source_spec: `docs/implementation/spec-1-4n-g3d-data-strict-mixed.md`
  summary: Prove that each `cset=` slot is installed at attach (the symbol-per-slot check deferred from slice 6 now covers G3D's 22 and HG2D's 40 `cset=` slots).
  evidence: slice 14 code review; `cset=` proves only that the slot is zero in the image, so deleting its assignment in C passes.
- source_spec: none
  summary: Parallel verification, after slice 15. First, give the break runners a worker pool, each worker with its own frozen tree copy, build directory and `WINEPREFIX` (starting with `host-breaks` and `main-breaks`). Then add `cmake_verify.py -j N`, running independent Baselines lines at once, each in its own tree, prefix and Xvfb display. Move the remaining wine steps off the shared `~/.wine`.
  evidence: Story 1.3's Baselines run (p13) took about 44 h serially on a 12-core, 78 GB machine with load around 1.8. Its longest steps are `host-breaks` (4.2 h), `options-breaks` (2.1 h), and three that hit the 2 h timeout (`main-breaks SUITE=situations`, `sim1r-breaks`, `g3d-breaks N=1000`). Docker is not needed: separate prefixes give the same isolation. Requested by the user on 2026-11-21.
- source_spec: `docs/implementation/spec-1-4o-main-data.md`
  summary: After Story 1.3's Baselines run, run `maindiff-run`, `main-breaks` (from a frozen copy), `main-smoke` and `main-dialogs` (MAIN's data is now typed C; slice 15 built it and matched `MAIN.DLL` and `maindiff.exe` normalized, and ran none of them).
  evidence: queued by the frozen intent.
- source_spec: `docs/implementation/spec-1-4o-main-data.md`
  summary: MAIN's `main_image.inc` types only the ImportTable, Linkage, month and season tables and the situation records by structure; the rest (menus, dialogs, strings and state, about 30 kB) is typed words marked "meaning not yet known". Describe them as the slices that read them settle their layout. The `tail=` window's retail-relocation allowance (the spec asked for none overlapping; retail's code there has 2,309) can tighten once the tail goes.
  evidence: slice 15 implementation.
- source_spec: `docs/implementation/spec-1-4o-main-data.md`
  summary: Prove that MAIN's 47 `cset=` Linkage slots are installed by `main_install()`, and that each module's compiled pointer arrays (`data_pointers`, `retail_linkage`) match the checked `.inc`. This extends slice 14's install-proof entry to MAIN.
  evidence: slice 15 code review; `cset=` checks only that the slots are zero in the image.
- source_spec: `docs/implementation/spec-1-4o-main-data.md`
  summary: Type more of `src/main/main_image.inc` by structure:
    - the G3D command blocks at +0x2386..+0x34fa, as `OP()` lines from the entry points in `main_natives.inc:770-822`, with a re-parse as G3D has;
    - the radio retuning records, using `radio_retune()`'s layout;
    - the clock and other dialog descriptors, using `main_clockdlg.inc` and `main_dialogs.inc`.

    Until then, relabel those regions "layout known, not yet typed".
  evidence: slice 15 code review. A linear parse from +0x2386 stops at once, so the block boundaries come from the entry table, not from a single walk.
- source_spec: `docs/implementation/spec-1-4p-host-data.md`
  summary: Check whether `gen_host_resources.py` (which reads `orig/FLTSIM98.EXE` in the `OPENFS98.EXE` build step) gives the same resources from another pressing. Add a pin or a pressing-independent check if it doesn't.
  evidence: slice 16 code review. `gen_host_data.py` used to stop the build on any EXE but the reference; now only `host-rt-tables-check` and the image headers name the reference sha256.
- source_spec: none
  summary: RESOLVED by slice 17b (spec-1-4r-simsui-data.md, 2026-11-22): SIMSUI's image is typed C (`src/simsui/simsui_image.inc`, `simsui_image.c`), row SIMSUI (`bound=`) is in `tools/data_images.txt`, `gen_simsui_native.py` writes only the `.def`, labels header and resources, and `gen_simsui_tables.py` is the oracle `tools/ofs simsui-tables-check`. Was: turn SIMSUI's native image (`gen_simsui_native.py`'s binary payload of retail's `.rdata` 0xa768 and `.data` 0xc00, with every relocated pointer bound at load) into committed typed C with checked bindings, and make `gen_simsui_tables.py` an oracle check as `gen_host_rt_tables.py` became in slice 16.
  evidence: split from slice 17 (SIM1 and SIMSUI) on 2026-11-21. The two are independent, and SIMSUI needs its own manifest mode for a pointer image bound at load.
- source_spec: `docs/implementation/spec-1-4q-sim1-data.md`
  summary: Commit the identity gate's normalizer (the scratchpad `peh2.py`: it zeroes the COFF timestamp, the checksum and the export timestamp, and masks `_CSWTCH.N` digits) as a repository tool with a documented command, so another reviewer can reproduce slices 10-17's "normalized-identical" claims.
  evidence: slice 17 code review; every slice's gate depends on a private scratchpad script.
- source_spec: none
  summary: A break runner killed by `cmake_verify.py`'s timeout leaves its mutation in the tree. The timeout uses `killpg(…, SIGKILL)`, so no cleanup runs. Story 1.3's p13 kept `sim1r-breaks`' `>=` mutant in `src/sim1/sim1_small.c:843`, and every later SIM1 build in p13 used it (`sim1rdiff-run`, `sim1native-flights` and both smoke runs). Fix it with the parallel-verification work: each break runner mutates its own tree copy, never the tree it was started from. Until then, `cmake_verify.py` should send SIGTERM and wait before SIGKILL, the runners should restore on SIGTERM, and the verifier should check the tree is unchanged after each break step.
  evidence: found 2026-11-21 when re-running p13's `sim1rdiff-run`. It differed in `op_crash_count` (GLOBAL +0xb7c) because of the leftover mutant; the current tree gives 0 differ. Restoring the line in p13 was enough. `main`, `g3d`, `hg2d` and `controls` in p13 were clean.
- source_spec: none
  summary: Parallel verification B. Give the break runners (`tools/*_breaks.py`, starting with `host_breaks.py` and `main_breaks.py`) a worker pool, each worker mutating its own tree copy with its own build directory and `WINEPREFIX`, so one long suite runs in parallel. Reuse A's tree-copy helper.
  evidence: split from the parallel-verification request (2026-11-21) on the user's choice. A (`cmake_verify.py -j N` with disposable per-job trees) comes first. `host-breaks` takes 4.2 h and `options-breaks` 2.1 h on their own.
- source_spec: `docs/implementation/spec-1-4r-simsui-data.md`
  summary: After Story 1.3's Baselines run, run `simsuidiff-run`, `simsui-breaks` (from a frozen copy), `main-dialogs` and `flights-play` (SIMSUI's image is now typed C; slice 17b built it and ran none of them; `bin/SIMSUI.DLL` and `simsuidiff.exe` are identical to their pre-slice builds after `strip` and `peh2.py`).
  evidence: queued by the frozen intent.
- source_spec: `docs/implementation/spec-1-4r-simsui-data.md`
  summary: No `--hash` build step depends on `tools/data_images.txt`, so changing a pinned hash does not rerun a module's step (all 28 rows, pre-existing).
  evidence: slice 17b review, Blind Hunter; `UPDATE=1` works around it by zeroing the DLL's mtime.
