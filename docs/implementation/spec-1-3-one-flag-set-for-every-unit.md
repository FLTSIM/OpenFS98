---
title: 'One flag set for every unit'
type: 'refactor'
created: '2026-11-19'
status: 'done'
route: 'dispatch'
review_loop_iteration: 0
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43' # plus uncommitted 1.2 work; diff against the snapshot /tmp/claude-1000/-home-steve-source/b2e3078b-1d77-426e-816f-2de2772a67dc/scratchpad/baseline-1-3 (cmake CMakeLists.txt tools docs include src AGENTS.md)
context: ['{project-root}/OpenFS98/docs/implementation/epic-1-context.md', '{project-root}/OpenFS98/docs/tools.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Every compile line spells out its own semantic flags. Some units get `-fno-strict-aliasing` or `-fno-delete-null-pointer-checks` and others don't, and none gets `-fwrapv`. So two units can treat the same C differently: signed overflow, aliasing and null checks are exactly what retail's MSVC code relies on.

**Approach:** `cmake/toolchains/win32-retail.cmake` owns the set as `OFS_MODULE_FLAGS` (AD-11, AD-20). CMake publishes it as the `OpenFS98::module` interface target and substitutes it into every build command. Python scripts read the same `set()` from the toolchain file. Every gcc invocation takes the set from there instead of spelling its own.

## Boundaries & Constraints

**Always:** The set is exactly `-fwrapv -fno-strict-aliasing -fno-delete-null-pointer-checks -mfpmath=387 -ffp-contract=off -fexcess-precision=standard`. It reaches every gcc invocation, harnesses, break builds and test drivers included, because a harness compiles the module sources it tests. All baselines in `docs/tools.md` hold, and the 74 flights stay identical. Any baseline that moves is investigated, not re-recorded.

**Never:** Edit baselines to make them pass. Change module source to suit a flag. Run break suites from the live tree.

**Decisions (user, 2026-11-19):**
1. Only the six semantic flags move into the set. Optimisation levels, freestanding, `-nostdlib`, link flags and `-std` stay in their rules. The codegen overrides `-fno-ipa-icf`, `-fno-tree-loop-distribute-patterns`, `-fno-tree-vectorize` and `-fno-math-errno` stay in their rules too, and the toolchain file lists each one with where it is used and why. Each reason is found by removing the flag and seeing what breaks.
2. Re-verify the whole Baselines block of `docs/tools.md` through `tools/cmake_verify.py` from a frozen copy, break suites included, since their harnesses rebuild with the new flags. Measured: the six flags change the code of 41 of the 65 units in `all.cmake` (`-fwrapv` 27, aliasing 14, null checks 2). The three FP flags change nothing for `-std=c99` units, but can change code in the `-std=gnu99` harnesses.

</frozen-after-approval>

## Code Map

- `cmake/toolchains/win32-retail.cmake` -- add `set(OFS_MODULE_FLAGS …)` (one line, parsed by Python) and the overrides table. Rewrite the header (it says flags were "copied from the Makefile" and promises Story 1.3).
- `CMakeLists.txt` -- `add_library(openfs98_module INTERFACE)`, `add_library(OpenFS98::module ALIAS …)`, `target_compile_options(… INTERFACE ${OFS_MODULE_FLAGS})`.
- `cmake/OpenFS98.cmake:12,62` -- beside the `@SEMI@` replace, `string(REPLACE "@MODULE_FLAGS@" "<flags joined by spaces>" …)`; `ofs_step` (19-43) also DEPENDS on the toolchain file, so a flag change rebuilds every step.
- `cmake/steps/all.cmake`, `cmake/steps/difftests.cmake`, `cmake/targets.cmake` -- 289 literal `[==[i686-w64-mingw32-gcc …]==]` strings: drop the six, insert `@MODULE_FLAGS@` after the compiler, in every gcc invocation (asm-only `all.cmake:433` and link-only lines included: gcc ignores them there).
- Python, own gcc lists, no shared helper (73 files): `grep -l mingw32-gcc tools/*.py`. Shared lists: `host_rt_breaks.py:42` `CC`, `mpdialog_ui_breaks.py:15-19` `FLAGS` (imported by `mpdialog_timeout_check.py`). `build_gauges.py:20-35` (gnu99; its up-to-date check also gets the toolchain file). No set at all today, so code may change: `build_mdls.py`, `mdl_breaks.py`, `mpdialog_owner_acceptance.py`, `multip_breaks.py`.
- gnu99 / `-O1` harnesses where the FP flags can change code: `panelsdiff`, `allpaneldiff`, `allgaugesdiff`, `gaugespanelsdiff` (`difftests.cmake:186,261,267,281`), `fsdialogs.exe` (`:255`, no `-std`).
- 12 runners build through `tools/ofs *-break-build` and inherit from `targets.cmake`: no edit.
- `tools/cmake_verify.py` -- runs the Baselines block (cwd = root); reuse 1-2b's `~/openfs98-verify/` layout.
- `tools/frames_many.sh` (no `HG2D_FRAMES_ONLY`) -- all ours against all retail, every flight; `sim1native-flights` covers SIM1 alone.

## Tasks & Acceptance

**Execution:**
- [x] `cmake/toolchains/win32-retail.cmake`, `CMakeLists.txt`, `cmake/OpenFS98.cmake` -- the set, the target, the placeholder, the toolchain-file dependency.
- [x] `tools/module_flags.py` -- `FLAGS` parsed from the toolchain file's `set(OFS_MODULE_FLAGS …)`, path from `Path(__file__).resolve().parents[1]`; stop if the line is missing.
- [x] `cmake/steps/*.cmake`, `cmake/targets.cmake` -- scripted rewrite of every gcc string.
- [x] every Python site -- `*module_flags.FLAGS` in place of its own semantic flags; `build_gauges.py` adds the toolchain file to its dependencies.
- [x] `cmake/toolchains/win32-retail.cmake` -- overrides table: for each override, remove it in a frozen copy, rebuild that target and run its difftest; record the sites and the build error or baseline that moved.
- [x] `docs/tools.md`, `docs/building.md`, `docs/architecture.md` -- the set and where it lives.
- [x] verify (decision 2) from a frozen copy with `bin/` removed (`tools/ofs ofs_clean`), then rebuild; then all ours installed in a disposable game copy and flown with `tools/frames_many.sh`. Record moved numbers and causes in Implementation Notes.

**Acceptance Criteria:**
- Given the change, when `grep -rnE -- '-f(no-)?(wrapv|strict-aliasing|delete-null-pointer-checks)|-mfpmath|-ffp-contract|-fexcess-precision' cmake CMakeLists.txt tools/*.py tools/*.sh tools/ofs` runs, then only `cmake/toolchains/win32-retail.cmake` matches.
- Given a configured build, when `cmake --build build/win32-retail -- VERBOSE=1` and the `difftests` target run, then every gcc command printed carries the six; and `grep -L module_flags $(grep -l mingw32-gcc tools/*.py)` prints nothing.
- Given only the toolchain file is touched, when the build runs, then every step rebuilds.
- Given the overrides table, when it is read, then each override names its sites and the build error or baseline seen without it.
- Given the frozen-copy verification, when `tools/cmake_verify.py` finishes, then every line is PASS or as 1-2b recorded it, `tools/frames_many.sh` reports 74 match, and `sim1native-flights` 74 match.

## Implementation Notes

Done 2026-11-19: `OFS_MODULE_FLAGS` in the toolchain file, `OpenFS98::module`, `@MODULE_FLAGS@` in all 289 gcc strings, `tools/module_flags.py` in all 75 Python sites (`mpdialog_ui_breaks.FLAGS` and `host_rt_breaks.CC` carry it; `build_gauges.py` also depends on the toolchain file), docs. The acceptance greps pass. A frozen copy (`~/openfs98-verify/p13`) builds `all` and `difftests`: the 65 and 122 gcc commands each carry the six; 160 build rules depend on the toolchain file.

Override experiments (copies `~/openfs98-verify/x-<name>`, WINEPREFIX `~/.wine-x`, log `~/openfs98-verify/ovr.log`): removing the flag changes code, as the table in the toolchain file lists. Baselines without `-fno-ipa-icf`: demodiff 20000, 0 differ; advdrvdiff 20000, 0 differ. controlsdiff, g3ddiff (its first run failed on a missing `WINDOW_retail.dll`, a setup dependency on an earlier target), the loop-distribute / vectorize / math-errno runs: see `ovr.log` (the table in the toolchain file is to be completed from it).

Superseded by the 2026-11-22 notes below. Was NOT finished: the full Baselines run (`python3 tools/cmake_verify.py build/win32-retail ../p13-results` from `~/openfs98-verify/p13`, running detached, results in `~/openfs98-verify/p13-results/report.md`; 1-2b's took three days), `tools/frames_many.sh` with all ours installed (74 match) and `sim1native-flights` (74 match).

Review patches (2026-11-19, after `~/openfs98-verify/p13` was frozen): `tools/check_module_flags.sh` (new, a Baselines line), the sentinel compile in `tools/mpdialog_timeout_check.py` takes the set, and `tools/simsui_breaks.py` is added to the overrides table. None changes a unit the running Baselines run builds; the p13 copy's mpdialog timeout sentinel still builds without the set, and it is a probe, not a unit under test. Run `tools/check_module_flags.sh` once in p13 at the end.

Verification finished 2026-11-22 (frozen copy `~/openfs98-verify/p13`, results in `p13-results`, then the reruns `p13-group2`..`p13-group2d` through `cmake_verify.py -j 3`):
- Baselines: all 161 lines PASS or read by hand and OK. Earlier false results fixed along the way: `panelsunit-run` lacked `bin/difftest/replay.bin` (now a real PASS with the replay case); `reg_contracts` ran before FRAMELOCK and SEHFIX were built; `ofstest-run` started the game from `Z:` in a slot (now 68 passed, 0 failed); a killed `sim1r-breaks` left a mutant in p13 (restored; slots now check the tree after every line).
- `hg2ddiff-run`: 59 suites, 0 differ (needed `HG2D_TIMEOUT=40000`; 19309 s). `controlsdiff-run`: 20000 scenarios, 0 differ (`CONTROLS_TIMEOUT`).
- `sim1r-breaks`: 881 of 882 caught. 16 breaks never ran since before 1.2 (their text was not unique; 1-2b's run recorded 0 of 882 because every mutant crashed or was not found): re-anchored in `tools/sim1r_breaks.py`, all caught. `filter-kept` and `jet-flowsat` are rare edges, caught at N=2000 (now their own count). `gpass-steer` is missed at N=2000 with the 1-3 flags and with the pre-1-3 ones alike (tested 2026-11-22): a coverage gap, recorded in the runner's docstring and `docs/tools.md`.
- `tools/frames_many.sh` from p13 with all ours: 74 match, 0 differ. `sim1native-flights`: 74 of 74.
- Overrides table complete (every removal run 0 differ). Toolchain-touch rebuild: all 40 steps reran (5 m 36 s), every output identical after normalizing. `tools/check_module_flags.sh` passes.

## Spec Change Log

## Review Triage Log

Review 1 (2026-11-19; Blind Hunter, Edge Case Hunter, Verification Gap on Sonnet at the user's request), while the Baselines run was in progress (user's choice). The diff was taken against a file snapshot of the tree, because the 1.2 work is uncommitted.

| # | Finding | Verdict | Evidence / route |
|---|---------|---------|------------------|
| 1 | No check that every gcc invocation carries the set (VG, BH) | medium | Only convention held it. Patch: `tools/check_module_flags.sh` is now a Baselines line; it fails on a copy with one step and one runner stripped |
| 2 | `mpdialog_timeout_check.py:21` compiles its sentinel without the set (VG) | low | Real: the Always rule includes test drivers, and the fix is a direct correction. Patch: `*module_flags.FLAGS` |
| 3 | Overrides table omits `tools/simsui_breaks.py` as a vectorize/loop-distribute site (ECH) | low | Real (`simsui_breaks.py:839`). Patch: added to the table |
| 4 | Overrides have no regression pin (VG) | medium (unverified) | Defer: overrides predate this story; entry in deferred-work.md |
| 5 | `OpenFS98::module` is unused (BH) | false | Design Notes: it is for add-ons (Epic 9, packaged by a later story) and fed from the same variable |
| 6 | `module_flags.py` parse is fragile: `$`, `)`, empty or repeated `set()` (BH, ECH) | low, rejected | Unlikely in everyday use, and the fix adds guards. It already stops when the line is missing |
| 7 | Assert the parsed FLAGS equal the six (ECH) | rejected | It would create a second source of truth, the harm the story removes |
| 8 | `@MODULE_FLAGS@` empty if OFS_MODULE_FLAGS is unset (ECH) | false | `CMakeLists.txt:7` stops unless the win32-retail toolchain (which sets it) is loaded |
| 9 | Python generators' own up-to-date checks skip a flag change (ECH) | false | Only `build_gauges.py` has one, and it now depends on the toolchain file; `build_mdls.py` and `gen_mdl_crash.py` always rebuild |
| 10 | Hand-merged long lines may have dropped flags (BH) | false | A token comparison of every changed `tools/*.py` against the snapshot shows only the semantic flags removed and `module_flags` added |
| 11 | `-fno-math-errno` kept with no effect (BH, VG) | rejected | Decision 1 keeps the overrides in their rules; the table records that it has no effect |
| 12 | "Four codegen overrides" vs three groups in docs (BH) | false | Four flags; loop-distribute and vectorize are listed together because they share sites |
| 13 | Verification and override reasons unfinished (BH, ECH) | not a defect | Open tasks, still running |
| 14 | Spec says 73 files, notes say 75 (ECH) | rejected | The fix edits this spec |
| 15 | `pressing_catalogue.py`, `tools/guest/*`, sprint change proposal, epic renumbering in the diff, and their own defects (BH, ECH, VG) | rejected | Not this story: untracked files created 12:18-12:24 by another contributor after the snapshot; they belong to Stories 6.1/7.1 and the course correction |
| 16 | sprint-status.yaml absent from the diff (BH) | false | `docs/implementation/` was left out of the review diff on purpose |

## Design Notes

The set is a configure-time variable, so a plain `string(REPLACE)` beside `@SEMI@` does the substitution. The interface target exists for add-ons (Epic 9) and is fed from the same variable. Python parses the toolchain file rather than reading a generated file, because `ofs_clean` removes `bin/` and a generated copy would stay gone until the next configure. `-Werror` is no risk: `-fwrapv` and `-fno-strict-aliasing` only remove warnings, and there is no C++.

## Verification

**Commands:**
- `tools/ofs ofs_clean && cmake --build build/win32-retail` (frozen copy) -- builds.
- `python3 tools/cmake_verify.py build ../results` (frozen copy) -- every line PASS or as in 1-2b.
- `tools/frames_many.sh` with all ours installed -- 74 match.
