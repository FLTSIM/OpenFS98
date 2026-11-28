# Resolved deferred work

Entries moved from `deferred-work.md` once resolved (2026-11-25), unchanged and in their original order; open items stay in `deferred-work.md`. Line references like `deferred-work.md:NNN` in older specs point at the file before this move (see git history).

- source_spec: none
  summary: Story 1.2 piece 2: re-verify every difftest, break suite, smoke, frames and dialog target through CMake against its baseline in docs/tools.md. (Done 2026-11-19 as spec-1-2b-verify-baselines-through-cmake.md.)
  evidence: Split from Story 1.2 on 2026-11-16: about 340 targets, some run 25-40 minutes under wine; hours of wall time and separable by module family.
- source_spec: none
  summary: Story 1.2 piece 3: update docs/tools.md, docs/building.md and AGENTS.md for CMake, then delete the Makefile and tools/*.mk. (Taken up 2026-11-18 as spec-1-2c-retire-the-makefile.md.)
  evidence: Split from Story 1.2 on 2026-11-16: only safe once the targets are ported and re-verified (pieces 1 and 2).
- source_spec: `docs/implementation/spec-1-4e-booster-weatherm-demo-data.md`
  summary: RESOLVED by the held-back Story 1.4 run (2026-11-23/24: ~/openfs98-verify/p14-results, reruns p14-rerun, p14-rerun2; docs/tools.md "Load limits"): Run `tools/ofs boosterdiff-run` (~75 min), `tools/ofs weathermdiff-run` and `tools/ofs demodiff-run` (~25 min) after Story 1.3's Baselines run has finished.
  evidence: expected 20000 scenarios, 0 differ each. Slice 5 proved only the .data images byte for byte; a wrong function pointer in a correct slot is seen only by the difftests.

## Deferred from: code review of spec-1-2-cmake-targets-for-every-tools-md-target.md (2026-11-19)

- source_spec: spec-1-2-cmake-targets-for-every-tools-md-target.md
  summary: RESOLVED by Story 1.4 (2026-11-24): no generated file is left in `src/` (`git ls-files -o src` is empty); the generators read committed sources and write only into `bin/`, and every committed `.inc`/`.h` beside a named source is a step input (`cmake/OpenFS98.cmake`). One gap remains, deferred from spec-1-5d: `fe.c` includes `fe_library.c`, which is not globbed. Generated src/**/*.inc files are not CMake step inputs (shortcut in cmake/OpenFS98.cmake); World's and the slice-3 table files (MULTIP, ALERTS, HELP, VIEWS, FLIGHTS, SIM1) are plain sources now. Story 1.4.
  evidence: cmake/OpenFS98.cmake header comment.

## Deferred from: Story 1.4, slice 1 (WINMSG and MPCHAT data decompiled, 2026-11-19)

- source_spec: `docs/implementation/spec-1-4-module-data-decompiled-no-raw-dumps-in-git.md`
  summary: RESOLVED by Story 1.4 slice 2. Check (c) only saw generators whose command line names orig/Modules and src/; gen_world_advanced.py and other *_tables/*_runtime generators read orig/ inside the script and were not listed.
  evidence: check (c) now covers every tools/*.py that CMake runs and that reads orig/, directly or by import (`tools/raw_dumps_allowed.txt`); World's generators are gone.
- source_spec: `docs/implementation/spec-1-4c-retail-table-generators.md`
  summary: RESOLVED by slice 13 (spec-1-4m-global-hg2d-data.md): GLOBAL's 194 default bytes are typed values in `global.c`, and `global_state.inc` and `gen_global_state.py` are gone.
  evidence: `tools/raw_dumps_allowed.txt`, tagged `(a)`.
- source_spec: `docs/implementation/spec-1-4e-booster-weatherm-demo-data.md`
  summary: RESOLVED by slice 8 (spec-1-4h-data-images-guard.md, 2026-11-20): Add `tools/ofs data-images`, run after `all`, that runs every `check_data_image.py` line and fails on any difference. With the generators' value asserts gone, it is the only guard on the typed initialisers outside the Baselines run. List it as `oracle:` in the allowlist.
  evidence: today the five checks are Baselines lines only (`cmake_verify.py`), and nothing runs them on an ordinary build.
- source_spec: `docs/implementation/spec-1-4f-window-acontain-dynamic-data.md`
  summary: RESOLVED by the held-back Story 1.4 run (2026-11-23/24: ~/openfs98-verify/p14-results, reruns p14-rerun, p14-rerun2; docs/tools.md "Load limits"): Run `tools/ofs windowdiff-run`, `tools/ofs window-breaks` (from a frozen copy of the tree), `tools/ofs acontaindiff-run` and `tools/ofs dynamicdiff-run` after Story 1.3's Baselines run has finished.
  evidence: not run in slice 6. WINDOW.DLL, ACONTAIN.DLL and DYNAMIC.DLL are normalized-identical to the pre-slice build (`~/openfs98-verify/s14/bin`) and `check_data_image.py` prints identical, so these are confirming runs.

## Deferred from: Story 1.4, slice 7 (ANALYSIS, REALITY, LANDME and ATC data decompiled, 2026-11-20)

- source_spec: `docs/implementation/spec-1-4g-analysis-reality-landme-atc-data.md`
  summary: RESOLVED by the held-back Story 1.4 run (2026-11-23/24: ~/openfs98-verify/p14-results, reruns p14-rerun, p14-rerun2; docs/tools.md "Load limits"): Run `tools/ofs atcdiff-run`, `landmediff-run`, `realitydiff-run`, `reality-breaks`, `analysisdiff-run` and `analysis-breaks` (breaks from a frozen copy of the tree) after Story 1.3's Baselines run has finished.
  evidence: not run in slice 7. The four DLLs are normalized-identical to the pre-slice build (`~/openfs98-verify/s14/bin`) and `check_data_image.py` prints identical for all twelve converted modules, so these are confirming runs.
- source_spec: `docs/implementation/spec-1-4g-analysis-reality-landme-atc-data.md`
  summary: RESOLVED by slice 8 (spec-1-4h-data-images-guard.md, 2026-11-20): The `.rdata` objects (ANALYSIS's nine doubles, REALITY's whole `.rdata`) have no retail oracle other than s14 identity and the difftests, because `check_data_image.py` compares `.data` only.
  evidence: the generators that asserted them against retail are gone.
- source_spec: `docs/implementation/spec-1-4g-analysis-reality-landme-atc-data.md`
  summary: RESOLVED by slice 8 (spec-1-4h-data-images-guard.md, 2026-11-20): Give `check_data_image.py` an `.rdata` mode, part of the planned `tools/ofs data-images`: compare retail's `.rdata` (ANALYSIS's first 0x48 bytes, the 9 doubles; REALITY's whole 0x19c bytes plus its zero pad) with ours, located by a symbol or export. Add a small mutate-and-expect-fail self-test for `--kept` and the edge paths.
  evidence: since slice 7 the generators' `.rdata` asserts are gone, so only s14 identity and the difftests check those objects.
- source_spec: `docs/implementation/spec-1-4h-data-images-guard.md`
  summary: RESOLVED by cleanup 1 (spec-cleanup-1-deps-retail-ids-norm-pe.md, 2026-11-25): Make `tools/data_images.txt` an input of the module steps (now fifteen), so a hand edit of the manifest alone reruns their `--hash` checks.
  evidence: `ofs_step` scans `python3` lines only for `tools/*.py` and `orig/…` tokens; until then `tools/ofs data-images` is the check after a manifest edit.
- source_spec: `docs/implementation/spec-1-4i-facil-multip-fe-data.md`
  summary: RESOLVED by the held-back Story 1.4 run (2026-11-23/24; p14-results: the Baselines lines among these passed) and its final run (2026-11-24/25; ~/openfs98-verify/p14-final, p14-bisect): `multipdiff-run` 20000 scenarios per suite, 0 differences (~6 min), now MULTIP's Baselines line; `multip-breaks` 456/456 caught. After Story 1.3's Baselines run, run `facildiff-run`, `facildiff-fe-run`, `fediff-run`, `fegriddiff-run`, `fesldiff-run`, `multipdiff-run` and `multip-breaks` (break suites from a frozen copy). Record `multipdiff-run`'s output as MULTIP's first Baselines line, since none exists yet.
  evidence: slice 9 was proved by normalized identity of FACIL/FE/MULTIP.DLL, facildiff.exe, fediff.exe, MULTIP_test.dll and FE_sl_ours.dll with the pre-slice build, and by `tools/ofs data-images`; the difftests were not run.
- source_spec: `docs/implementation/spec-1-4i-facil-multip-fe-data.md`
  summary: RESOLVED by slice 10 (spec-1-4j-pointer-target-guard.md, 2026-11-20; `ptrs=` and `switch=` fields, plus an `rdata=` range for `s_nametag`; the symbol-per-slot handler check stays open above): Fold the pointer targets of `facil_relocs.inc`, `fe_relocs.inc` and `multip_pointers.inc`, and the contents of `s_nametag` and `s_frame_actions`, into a guard. Either hash them with the image in `tools/data_images.txt`, or cover them by the symbol-per-slot pointer check.
  evidence: `--hash` covers image bytes and relocation offsets only; a changed target alters our DLL outside the hashed image.

## Deferred from: Story 1.4, slice 11 (ADVDRV, SOUND, PANELS and CONTROLS data decompiled, 2026-11-20)

- source_spec: `docs/implementation/spec-1-4k-advdrv-sound-panels-controls-data.md`
  summary: RESOLVED by the held-back Story 1.4 run (2026-11-23/24: ~/openfs98-verify/p14-results, reruns p14-rerun, p14-rerun2; docs/tools.md "Load limits"): After Story 1.3's Baselines run, run `advdrvdiff-run`, `sounddiff-run`, `controlsdiff-run`, `panelsdiff-run N=3000`, `panelsunit-run`, `controls-breaks` and `panels-breaks` (break suites from a frozen copy).
  evidence: slice 11 was proved by normalized identity of ADVDRV/SOUND/PANELS/CONTROLS.DLL, advdrvdiff.exe, the ADVDRV coverage build, an `-DBREAK=2` ADVDRV build and PANELS_unit.dll with the pre-slice build, and by `tools/ofs data-images`; none of the difftests was run.
- source_spec: `docs/implementation/spec-1-4k-advdrv-sound-panels-controls-data.md`
  summary: RESOLVED by cleanup 1 (spec-cleanup-1-deps-retail-ids-norm-pe.md, 2026-11-25): The difftests step depends only on `tools/difftest/*.inc` and `*.h` (`cmake/OpenFS98.cmake:43-47`), so a later edit to `src/advdrv/advdrv_relocs.inc` will not rebuild `bin/difftest/advdrvdiff.exe`, which compiles it. Add the file as a dependency of that step, or rebuild by hand.
  evidence: `tools/difftest/advdrvdiff.c` includes `../../src/advdrv/advdrv_relocs.inc` twice (the code and data slot tables).

## Deferred from: Story 1.4, slice 12 (NUMERIC and UTIL data decompiled, 2026-11-20)

- source_spec: `docs/implementation/spec-1-4l-numeric-util-data.md`
  summary: RESOLVED by the held-back Story 1.4 run (2026-11-23/24: ~/openfs98-verify/p14-results, reruns p14-rerun, p14-rerun2; docs/tools.md "Load limits"): After Story 1.3's Baselines run, run `utildiff-run`, `utildiff-portable`, `numericdiff-run` and `ofstest-run`.
  evidence: slice 12 was proved by normalized identity of NUMERIC.DLL, UTIL.DLL, numericdiff.exe, utildiff.exe, utildiff_portable.exe, slewdiff.exe, OFSTEST.DLL and UTIL_TRACE.DLL with the pre-slice build, and by `tools/ofs data-images`; none of the difftests was run.
- source_spec: `docs/implementation/spec-1-4l-numeric-util-data.md`
  summary: RESOLVED by slice 12, without changing module code paths (the data initialisers did change): NUMERIC and UTIL were thought to need code changes before their data could be checked, because their `.data` copies are not found through the `ImportTable` export. The `at=SYMBOL` manifest field finds each image by its COFF symbol instead, and both DLLs are normalized-identical to the pre-slice build.
  evidence: `tools/data_images.txt` rows NUMERIC (14 kept slots) and UTIL (83 kept slots).
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
  summary: RESOLVED by the held-back Story 1.4 run (2026-11-23/24: ~/openfs98-verify/p14-results, reruns p14-rerun, p14-rerun2; docs/tools.md "Load limits"): After Story 1.3's Baselines run, run `hostdiff-run`, `host-breaks` (from a frozen copy), `hostrtdiff-run` and `host_rt_breaks.py` (from a frozen copy) (HOST's data is now typed C; slice 16 built it and ran none of them; `HOST.DLL` is identical to a build of the baseline tree, `bin/OPENFS98.EXE`, `hostrtdiff.exe` and `hostdiff.exe` to their pre-slice builds).
  evidence: queued by the frozen intent.
- source_spec: `docs/implementation/spec-1-4m-global-hg2d-data.md`
  summary: RESOLVED by the held-back Story 1.4 run (2026-11-23/24; p14-results: the Baselines lines among these passed) and its final run (2026-11-24/25; ~/openfs98-verify/p14-final, p14-bisect): `hg2dmodeldiff-run` 20000 scenarios, 0 differ; `tools/hg2d_breaks.py` 1564 mutations, 1564 caught (~75 min). After Story 1.3's Baselines run, run `hg2ddiff-run`, `hg2dmodeldiff-run` and `hg2d-breaks` (HG2D's data is now typed C; slice 13 built it but ran none of them).
  evidence: queued by the frozen intent.
- source_spec: none
  summary: RESOLVED for SIM1 by slice 17 (spec-1-4q-sim1-data.md, 2026-11-21): SIM1's `.data` and `.rdata` are typed C (`src/sim1/sim1_data.inc`, `sim1_rdata.inc`, shared by the harness DLLs and the native build), `gen_sim1_native.py` only extracts the resources, `gen_sim1_data.py` is the setup check `tools/ofs sim1-reference`, and row SIM1 is in `tools/data_images.txt`. SIMSUI (`gen_simsui_native.py`, `gen_simsui_tables.py`) is slice 17b below.
  evidence: split from "remaining generators" (2026-11-21).
- source_spec: `docs/implementation/spec-1-4q-sim1-data.md`
  summary: RESOLVED by the held-back Story 1.4 run (2026-11-23/24; p14-results: the Baselines lines among these passed) and its final run (2026-11-24/25; ~/openfs98-verify/p14-final, p14-bisect): `sim1diff-run`, `sim1air-run`, `sim1state-run` 0 differences; `sim1-breaks` 16/16, `sim1air-breaks` 20/20, `sim1state-breaks` 41/41 caught. After Story 1.3's Baselines run, run `sim1diff-run`, `sim1rdiff-run`, `sim1-breaks`, `sim1r-breaks`, `sim1air-run`, `sim1air-breaks`, `sim1state-run`, `sim1state-breaks` and `sim1native-flights` (SIM1's data is now typed C; slice 17 built it and ran none of them; `bin/SIM1.sim` and `sim1rdiff.exe` are identical to their pre-slice builds, the three test DLLs to a build of the baseline tree).
  evidence: queued by the frozen intent.
- source_spec: none
  summary: Story 1.4 slice 18: the gauges, MDLs and test gauges (`gen_gauges.py`, `build_mdls.py`, `gen_mdl_crash.py`, `gen_warning_data.py`, `gen_suction_data.py`); then revisit `gen_window_data.py`, which the user chose to keep as a build step in slice 6.
  evidence: split from "remaining generators" (2026-11-21).
- source_spec: `docs/implementation/spec-1-4m-global-hg2d-data.md`
  summary: RESOLVED by slice 14 (spec-1-4n-g3d-data-strict-mixed.md, 2026-11-21: `cset=` and the new self-test cases). Was: tighten `--mixed` before G3D and MAIN use it. Give the row an explicit list of the zero slots set in C (HG2D's 40 at +0x3c..+0xf4), so that a zero slot must be listed in `ptrs=` or in that list. Add self-test cases for a raw slot turned zero and a deleted pointer entry.
  evidence: slice 13 Codex review: `--mixed` accepts zero in any slot not listed, so `UPDATE=1` would bless a deleted raw initialiser or pointer entry. The per-build hash still catches the change.
- source_spec: `docs/implementation/spec-1-4n-g3d-data-strict-mixed.md`
  summary: RESOLVED by the held-back Story 1.4 run (2026-11-23/24; p14-results: the Baselines lines among these passed) and its final run (2026-11-24/25; ~/openfs98-verify/p14-final, p14-bisect): `g3ddiff-meshc-run` 20000 scenarios, 43 differ, the same 43 at 27371fb (before any Story 1.x change), so not caused by this story; logged in docs/open-differences.md (Unsettled questions) and below. After Story 1.3's Baselines run, run `g3ddiff-run`, `g3ddiff-meshc-run`, `g3dmeshdiff-run` and `g3d-breaks` (G3D's data is now typed C; slice 14 built it, matched `G3D.DLL` and `HG2D.DLL` normalized, and ran none of them).
  evidence: queued by the frozen intent.
- source_spec: `docs/implementation/spec-1-4n-g3d-data-strict-mixed.md`
  summary: RESOLVED by cleanup 1 (spec-cleanup-1-deps-retail-ids-norm-pe.md, 2026-11-25): Make each module's build-time `check_data_image.py --hash` step depend on `tools/data_images.txt`, so that editing a row's hash, `cset=` or `ptrs=` alone reruns the check in an incremental build.
  evidence: slice 14 code review; `cmake/OpenFS98.cmake:34` keeps only `tools/*.py` and `orig/` as a python step's inputs. It has been the same for every row since slice 8; `tools/ofs data-images` still catches the edit.
- source_spec: `docs/implementation/spec-1-4o-main-data.md`
  summary: RESOLVED by the held-back Story 1.4 run (2026-11-23/24: ~/openfs98-verify/p14-results, reruns p14-rerun, p14-rerun2; docs/tools.md "Load limits"): After Story 1.3's Baselines run, run `maindiff-run`, `main-breaks` (from a frozen copy), `main-smoke` and `main-dialogs` (MAIN's data is now typed C; slice 15 built it and matched `MAIN.DLL` and `maindiff.exe` normalized, and ran none of them).
  evidence: queued by the frozen intent.
- source_spec: none
  summary: RESOLVED by slice 17b (spec-1-4r-simsui-data.md, 2026-11-22): SIMSUI's image is typed C (`src/simsui/simsui_image.inc`, `simsui_image.c`), row SIMSUI (`bound=`) is in `tools/data_images.txt`, `gen_simsui_native.py` writes only the `.def`, labels header and resources, and `gen_simsui_tables.py` is the oracle `tools/ofs simsui-tables-check`. Was: turn SIMSUI's native image (`gen_simsui_native.py`'s binary payload of retail's `.rdata` 0xa768 and `.data` 0xc00, with every relocated pointer bound at load) into committed typed C with checked bindings, and make `gen_simsui_tables.py` an oracle check as `gen_host_rt_tables.py` became in slice 16.
  evidence: split from slice 17 (SIM1 and SIMSUI) on 2026-11-21. The two are independent, and SIMSUI needs its own manifest mode for a pointer image bound at load.
- source_spec: `docs/implementation/spec-1-4q-sim1-data.md`
  summary: RESOLVED by cleanup 1 (spec-cleanup-1-deps-retail-ids-norm-pe.md, 2026-11-25): Commit the identity gate's normalizer (the scratchpad `peh2.py`: it zeroes the COFF timestamp, the checksum and the export timestamp, and masks `_CSWTCH.N` digits) as a repository tool with a documented command, so another reviewer can reproduce slices 10-17's "normalized-identical" claims.
  evidence: slice 17 code review; every slice's gate depends on a private scratchpad script.
- source_spec: none
  summary: DONE 2026-11-22 (`docs/implementation/spec-verify-parallel-b.md`: `tools/break_pool.py`, `BREAK_JOBS` on host, g3d, options and sim1r; `main_breaks.py` stays out by decision). Parallel verification B. Give the break runners (`tools/*_breaks.py`, starting with `host_breaks.py` and `main_breaks.py`) a worker pool, each worker mutating its own tree copy with its own build directory and `WINEPREFIX`, so one long suite runs in parallel. Reuse A's tree-copy helper.
  evidence: split from the parallel-verification request (2026-11-21) on the user's choice. A (`cmake_verify.py -j N` with disposable per-job trees) comes first. `host-breaks` takes 4.2 h and `options-breaks` 2.1 h on their own.
- source_spec: `docs/implementation/spec-1-4r-simsui-data.md`
  summary: RESOLVED by the held-back Story 1.4 run (2026-11-23/24: ~/openfs98-verify/p14-results, reruns p14-rerun, p14-rerun2; docs/tools.md "Load limits"): After Story 1.3's Baselines run, run `simsuidiff-run`, `simsui-breaks` (from a frozen copy), `main-dialogs` and `flights-play` (SIMSUI's image is now typed C; slice 17b built it and ran none of them; `bin/SIMSUI.DLL` and `simsuidiff.exe` are identical to their pre-slice builds after `strip` and `peh2.py`).
  evidence: queued by the frozen intent.
- source_spec: `docs/implementation/spec-1-4r-simsui-data.md`
  summary: RESOLVED by cleanup 1 (spec-cleanup-1-deps-retail-ids-norm-pe.md, 2026-11-25): No `--hash` build step depends on `tools/data_images.txt`, so changing a pinned hash does not rerun a module's step (28 rows, pre-existing; the 120 gauge rows are covered since slice 18b: the gauges step lists `tools/data_images.txt` in `DEPENDS_GLOB`).
  evidence: slice 17b review, Blind Hunter; `UPDATE=1` works around it by zeroing the DLL's mtime.
- source_spec: `docs/implementation/spec-1-4s-test-gauges-data.md`
  summary: RESOLVED by the held-back Story 1.4 run (2026-11-23/24; p14-results: the Baselines lines among these passed) and its final run (2026-11-24/25; ~/openfs98-verify/p14-final, p14-bisect): `gauges-game-run`, `suction-game-run` identical, PASS; `suction-breaks` 26 all caught; `tools/gauges_breaks.py` (warning) 15 all caught; `gauges-panels-run N=1000` 1000 scenarios, 143 differ, the same 143 at 27371fb, so not caused by this story (logged in docs/open-differences.md and below). Its default N was 20000, which cannot finish inside the harness's 1800 s wine timeout; now 1000, as documented. After Story 1.3's Baselines run, run `gaugesdiff-run`, `suctiondiff-run`, `gauges-panels-run`, `gauges-game-run`, `suction-game-run`, `suction-breaks` and `tools/gauges_breaks.py` (warning) (the test gauges' data is now typed C; slice 18a ran none of them, no wine; both `.GAU` files are identical to their pre-slice builds, stripped and unstripped, after `peh2.py`).
  evidence: queued by the frozen intent. `gauges_breaks.py` now passes `-Isrc/gauges` so its break copies find the committed `.inc` files.
- source_spec: `docs/implementation/spec-1-4t-gauges-data.md`
  summary: RESOLVED by the held-back Story 1.4 run (2026-11-23/24; p14-results: the Baselines lines among these passed) and its final run (2026-11-24/25; ~/openfs98-verify/p14-final, p14-bisect): `all-gauges-check` 120/120; `all-gauges-breaks` 67, 66 caught (`chrn-minutes` missed, the documented baseline); `all-gauges-panels` load counts equal; `all-gauges-hangs` matched retail; `gauges-game` is a Baselines line and passed in p14-results. After Story 1.3's Baselines run, run `gauges-check`, `gauges-breaks`, `gauges-panels`, `all-gauges-check`, `all-gauges-breaks`, `all-gauges-panels`, `all-gauges-hangs` and `gauges-game` (the 120 gauges' images are now committed typed C in `src/gauges/images/`; slice 18b ran none of them, no wine; all 120 `.GAU` files are identical to their pre-slice builds, stripped and unstripped, under `peh2.py`, and `allgaugesdiff.exe`, `allpaneldiff.exe`, `all_gauges.inc` and `all_panels.inc` are unchanged).
  evidence: queued by the frozen intent.
- source_spec: none
  summary: Story 1.4 slice 18c: the 8 normal and 3 crash MDL models' data (`tools/build_mdls.py`, `tools/gen_mdl_crash.py`, about 600 KB, mostly geometry); all of it, geometry included, converted to committed typed C (the user's decision, 2026-11-22: "convert the MDL data to C for now").
  evidence: split from slice 18 on 2026-11-22; the user asked what the content option means before choosing.
- source_spec: `docs/implementation/spec-1-4t-gauges-data.md`
  summary: RESOLVED by cleanup 1 (spec-cleanup-1-deps-retail-ids-norm-pe.md, 2026-11-25): The harness steps that compile with `-Ibin/gauges-all` (`allgaugesdiff.exe`, `allpaneldiff.exe`, difftests.cmake:258-268) do not depend on the generated `all_gauges.inc`/`all_panels.inc`, so a manifest change does not rebuild them (pre-existing).
  evidence: slice 18b spec review (Codex); ofs_step discovers only src/ and tools/ inputs.
- source_spec: `docs/implementation/spec-1-5d-facil-fe-retail-bugs-marked.md`
  summary: RESOLVED by cleanup 1 (spec-cleanup-1-deps-retail-ids-norm-pe.md, 2026-11-25): The FE build step does not depend on `src/fe/fe_library.c`, which `fe.c` includes (`fe.c:4814`), so editing only the Scenery Library leaves `bin/FE.DLL` stale; only FE.DLL needed the edge, since `fesldiff`/`fesldiff-run` rebuild `FE_sl_ours.dll` every run (`cmake/targets.cmake:633`).
  evidence: `cmake/OpenFS98.cmake:46`/`:54` glob only `*.h` and `*.inc` siblings as dependencies; `cmake/steps/all.cmake:110` names only `src/fe/fe.c`. Fix by adding `fe_library.c` to the step's DEPENDS (or globbing included `.c`).

- source_spec: `docs/implementation/spec-1-5f-reality-analysis-demo-retail-bugs-marked.md`
  summary: RESOLVED as not a bug by cleanup 1 (spec-cleanup-1-deps-retail-ids-norm-pe.md, 2026-11-25): host +0x150 zeroes both outputs before GetDiskFreeSpaceA, so retail never reads uninitialised outputs; the comment is corrected and no id is added. Was: DEMO's `disk_check` (`src/demo/demo.c:427`) documents a retail bug with no register id: when the free-space call fails, retail uses its uninitialised outputs, while ours zeroes them and still ignores the return.
  evidence: resolved as not a bug: host `+0x150` (`src/host/host.c:2772-2787`; retail `0x40a921`/`0x40a928`) zeroes both output words before `GetDiskFreeSpaceA`, so retail never reads uninitialised outputs and no DEMO id is needed. Found by slice 6's spec review.

- source_spec: `docs/implementation/spec-1-5h-atc-acontain-booster-weatherm-slew-retail-bugs-marked.md`
  summary: RESOLVED by cleanup 1 (spec-cleanup-1-deps-retail-ids-norm-pe.md, 2026-11-25): `docs/modules/weatherm.md:133` describes three WEATHERM effect-list quirks with no register id (a type-width mismatch, freeing non-members, and losing the predecessor during free-by-type); each needs a WXM entry in `docs/retail-bugs.md` and a marker.
  evidence: Found by slice 8's spec review; adding ids would change the slice's fixed 37-id scope.

- source_spec: spec-1-2-cmake-targets-for-every-tools-md-target.md
  summary: RESOLVED by cleanup 2 (spec-cleanup-2-tooling-guards.md, 2026-11-25): tools/check_modules_ofs.py with tools/modules_golden.txt. No regression test for tools/modules.sh output or tools/ofs argument handling.
  evidence: modules.sh matched `make -s` for every query before the Makefile was deleted; a table test would guard later edits.
- source_spec: `docs/implementation/spec-1-3-one-flag-set-for-every-unit.md`
  summary: RESOLVED by cleanup 2 (spec-cleanup-2-tooling-guards.md, 2026-11-25): tools/check_codegen_overrides.py reads the `#@` rows of the toolchain file and runs with the arena lint on every build. Nothing fails if a codegen override (-fno-ipa-icf, -fno-tree-loop-distribute-patterns, -fno-tree-vectorize, -fno-math-errno) is later dropped from one of its sites; the toolchain table is documentation only.
  evidence: medium, unverified as a harm. Without -fno-ipa-icf, DEMO's code changes but demodiff still gives 0 differ, so a dropped override may go unnoticed. A section comparison against a reference build per site would settle it (Story 1.4's image checks are the natural home).
- source_spec: `docs/implementation/spec-1-5j-multip-english-dialogs-retail-bugs-marked-allowlist-gone.md`
  summary: RESOLVED by cleanup 2 (spec-cleanup-2-tooling-guards.md, 2026-11-25): all six gaps fixed, each with a self-test fixture; the check passes on 335 entries. `tools/check_retail_bugs.py` comment parsing and file listing have gaps that predate slice 10: if `git ls-files` fails for any reason (e.g. a corrupt index) the checker silently falls back to a filesystem walk, which also counts git-ignored files; in `.S` files a spaced directive (`# define X "RETAIL BUG (kept): ID"`) is taken as a comment; `;` after an instruction is accepted as a marker comment (the self-test relies on it); a backslash-newline inside `*/` is not followed; an empty register with no markers passes with `0 entries`; a malformed duplicate marker passes when a valid one exists elsewhere.
  evidence: Slice 10 code review (Codex gpt-6.2-sol, blind and edge-case layers, 2026-11-24). None affects the current tree: the check passes on 331 entries.
- source_spec: `docs/implementation/spec-1-10a-divide-helpers-match-the-host-filter.md`
  summary: SPLIT 2026-11-25 into Story 1.10b1 (FE, DYNAMIC, ADVDRV, BOOSTER; spec-1-10b1-fe-dynamic-advdrv-booster-divides.md), 1.10b2 (PANELS) and 1.10b3 (DEMO), whose entries are in deferred-work.md; the combined entry follows unchanged. Story 1.10b: the six modules whose inline `div`/`idiv` asm relies on the trap move onto `include/fs98/divide.h`: FE (`fe.c:254,262,277`), DYNAMIC (`dynamic.c:183,193,202`), PANELS (`panels.c:75,87,96,105`), BOOSTER (`booster.c:788,797`), ADVDRV (`advdrv.c:571`), plus DEMO's deliberate fault (`demo.c:2256` `fault_div`, DEMO-10, a design decision). The proof is their difftests under wine, after checking each harness's repair model against the filter.
  evidence: Story 1.10 split at its Checkpoint 1 (2026-11-25) into a foundation and two waves; asm becoming C changes code by construction, so these need behavioural proof.
- source_spec: `docs/implementation/spec-cleanup-1-deps-retail-ids-norm-pe.md`
  summary: RESOLVED by `tools/check_build_wiring.py` (Cleanup 3a, 2026-11-25): Nothing re-checks the new rebuild edges automatically: a recurring incremental-build check is wanted (touch `tools/data_images.txt`, `advdrv_relocs.inc`, `fe_library.c`, a gauges input; delete a generated header; a wrong pinned hash; assert the exact recipe sets and a lint-only second build).
  evidence: the touch, delete and bad-hash proofs were one-off runs (the spec's Implementation Notes); removing the `data_images.txt` edge from `ofs_step` would pass every recurring check.
  review (cleanup 2 spec review, Codex gpt-6.2-sol, 2026-11-25): it needs a scratch configure and build (a copied build is tied to its source path), stable recipe identifiers, the absent-`bin/gauges-all` case, and the bad-hash restore.

- source_spec: `docs/implementation/spec-cleanup-2-tooling-guards.md`
  summary: RESOLVED by `tools/check_build_wiring.py` (Cleanup 3a, 2026-11-25): Nothing recurring asserts that the codegen check stays wired into `ofs_arena_lint`: removing its `COMMAND` in `cmake/OpenFS98.cmake` passes every recurring check. Wanted: a scratch-CMake test that drops a flag and requires a command-only target such as `graph` to fail first.
  evidence: cleanup 2 review; the wiring proof was a one-off run (the spec's Implementation Notes).

## Resolved by spec-cleanup-3b (2026-11-25)

- source_spec: none
  summary: RESOLVED by spec-cleanup-3b (cmake_verify tree guards, GUARD.json, session quiesce, resume rule): A break runner killed by `cmake_verify.py`'s timeout leaves its mutation in the tree. The timeout uses `killpg(…, SIGKILL)`, so no cleanup runs. Story 1.3's p13 kept `sim1r-breaks`' `>=` mutant in `src/sim1/sim1_small.c:843`, and every later SIM1 build in p13 used it (`sim1rdiff-run`, `sim1native-flights` and both smoke runs). Fix it with the parallel-verification work: each break runner mutates its own tree copy, never the tree it was started from. Until then, `cmake_verify.py` should send SIGTERM and wait before SIGKILL, the runners should restore on SIGTERM, and the verifier should check the tree is unchanged after each break step.
  evidence: found 2026-11-21 when re-running p13's `sim1rdiff-run`. It differed in `op_crash_count` (GLOBAL +0xb7c) because of the leftover mutant; the current tree gives 0 differ. Restoring the line in p13 was enough. `main`, `g3d`, `hg2d` and `controls` in p13 were clean.
  review (cleanup 2 spec review, Codex gpt-6.2-sol, 2026-11-25): a leak must stop the run and block resuming; protected paths must be told apart from the expected `bin/` outputs; the break-line classification needs fixing; and `cmake_verify.py --self-test` currently raises `IndexError`.
- source_spec: `docs/implementation/spec-1-4-module-data-decompiled-no-raw-dumps-in-git.md`
  summary: RESOLVED by spec-cleanup-4-raw-data-guards.md (2026-11-25) (the allowlist ratchet against HEAD in tools/check_raw_dumps.py; the image-extent half moved to 4b and stays in deferred-work.md): Nothing stops an entry from being added to tools/raw_dumps_allowed.txt.
  evidence: once the allowlist is committed, the check can fail when an entry is not in `git show HEAD:tools/raw_dumps_allowed.txt`; later modules should keep the sizeof assert or the tool should check the export's section bounds.
- source_spec: `docs/implementation/spec-1-4b-generators-reading-orig-and-world-tables.md`
  summary: RESOLVED by spec-cleanup-4-raw-data-guards.md (2026-11-25): Scripts reached only through a `tools/*.sh` wrapper are not seen by check (c).
  evidence: the rule matches `tools/<name>.py` named in `cmake/steps/*.cmake` and `cmake/targets.cmake` only.
- source_spec: `docs/implementation/spec-cleanup-3b-cmake-verify-tree-guards.md`
  summary: RESOLVED by spec-cleanup-4-raw-data-guards.md (2026-11-25): `cmake_verify.py`'s `break_verdict` selects a break suite by `'breaks' in cmd`. Classify parsed target names and direct runner invocations (`tools/*_breaks.py`) instead, with a test that an unrelated command containing `breaks` (a path, an argument) is not one.
  evidence: cleanup 2 and 3b spec reviews (Codex gpt-6.2-sol, 2026-11-25).

## Resolved by spec-cleanup-4b (2026-11-25)

- source_spec: `docs/implementation/spec-1-4-module-data-decompiled-no-raw-dumps-in-git.md`
  summary: RESOLVED by spec-cleanup-4b-image-extents.md (2026-11-25) (`obj=` and `extent=` in the manifest, `FS98_IMAGE_END` end symbols; was 4b, split from the raw-data-guards cleanup): check_data_image.py does not compare our image's own size (the structs' _Static_assert does for WINMSG and MPCHAT).
  evidence: later modules should keep the sizeof assert or the tool should check the export's section bounds. Spec review (Codex gpt-6.2-sol, 2026-11-25): COFF symbols cannot give object extents (alignment, aliases such as `_Linkage` inside `winmsg_image`, intentional sub-windows such as NUMERIC's `0x321` of `0x400` and SIMSUI's would fail 147 of 161 rows); use measured sizes or end labels with per-row declared sub-windows and padding; define scope for `tail=`, `.rdata`, `switch` and `bound`; a layout digest cannot establish extent, so `layout=` needs a manifest bootstrap and verification contract; editing `check_data_image.py` or `tools/data_images.txt` reruns the 30 hash-bearing build recipes and configure, with new PE timestamps; mutant proofs must get past the compile-time asserts and the existing hash.

- source_spec: `docs/implementation/spec-1-4-module-data-decompiled-no-raw-dumps-in-git.md`
  summary: Two harnesses differ from retail, both already before Story 1.x: `g3ddiff-meshc-run` (G3D with the portable mesh compiler, `FS98_PORTABLE_FP`) 43 of 20000 scenarios, some polygons emitted as `line(...)` where retail emits `v(...)` (e.g. scenario 747, op 14); `gauges-panels-run N=1000` (PANELS_STRICT, retail/rebuilt warning and suction gauges) 143 of 1000, heap block hashes and full state (first scenarios 6, 20, 28). Not investigated.
  evidence: ~/openfs98-verify/p14-final/g3ddiff-meshc-run.log, p14-bisect/new-panels-1000.log; the same scenario lists from 27371fb in p14-bisect/old-meshc-b.log and old-panels-1000b.log (tree ~/openfs98-verify/p14-old).

- source_spec: `docs/implementation/spec-1-4z-two-harness-differences-fixed.md`
  summary: Fix the two pre-existing 1.4 harness differences, both probably harness artifacts. `g3ddiff-meshc-run` (43/20000): `g3ddiff.c` never matches `g3d_mesh_precision` (24 since `48756cd`) to retail's 53-bit default. `gauges-panels-run` (143/1000): `gauge_pointer()` does not normalise pointers into a rebuilt gauge's image outside `.data` (different `.rsrc` RVA and image base). The draft spec is ready for Checkpoint 1.
  evidence: read-only analysis 2026-11-25; `48756cd` is an ancestor of `27371fb` (same scenario lists); `all-gauges-panels` (which normalises those words) gave 0 differ in ~/openfs98-verify/p14-final. Each needs one confirming wine run (in the spec) when the wine lock is free.
  resolved: Story 1.4z, 2026-11-26 (both harness differences fixed; `docs/implementation/spec-1-4z-two-harness-differences-fixed.md`).
