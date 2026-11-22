---
title: 'Retire the Makefile'
type: 'chore'
created: '2026-11-18'
status: 'done'
route: 'dispatch'
review_loop_iteration: 0
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43'
context: ['/OpenFS98/docs/native-port/epics.md', '/OpenFS98/docs/tools.md', '/OpenFS98/docs/building.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Story 1.2 piece 3: `docs/tools.md`, `docs/building.md` and `AGENTS.md` must describe the CMake commands, and the `Makefile` and `tools/*.mk` must go. Scripts still call `make` (break builds, harness builds, `make -s modules-of` and friends) and read `tools/*.mk` for source lists.

**Approach:** Port the rest of the Makefile's and sub-makes' targets the way piece 1 did; give scripts and docs `tools/ofs TARGET VAR=value` (a `cmake --build` wrapper keeping make's parameter syntax) and `tools/modules.sh` for the module queries; then delete the Makefile and `tools/*.mk`. Authorised by the user on 2026-11-17 ("finish 1.2 entirely").

## Boundaries & Constraints

**Always:** Same commands as the Makefile's `make -n -B` (checked before deleting it). Scripts keep their behaviour. Historical records in docs stay as written; present-tense instructions are rewritten.

**Never:** Edit baselines. Change module source or harness logic beyond what the port needs.

</frozen-after-approval>

## Tasks & Acceptance

**Execution:**
- [x] `cmake/targets.cmake` -- the other 308 targets (all phony targets of the Makefile and the 17 `tools/*.mk`; `clean`, `help` become `ofs_clean`, `ofs_help`)
- [x] `tools/ofs` -- `tools/ofs T... VAR=value` = `VAR=value cmake --build "$OFS_BUILD" --target T`
- [x] `tools/modules.txt`, `tools/modules.sh` -- the module lists; `cmake/install.cmake` reads the same file
- [x] `src/world/sources.txt`, `src/simsui/sources.txt` -- the source lists four break runners read from `tools/world.mk` and `tools/simsui.mk`
- [x] 12 break runners and 5 shell scripts call `tools/ofs` / `tools/modules.sh`
- [x] `ofs_install` / `ofs_revert` honour `FS98_DIR` from the environment, as `make install FS98_DIR=` did
- [x] docs: `tools.md`, `building.md`, `AGENTS.md`, and present-tense `make` commands across `docs/` (939 + 61 rewrites, by target name; prose kept)
- [x] delete `Makefile`, `tools/*.mk`, `tools/cmake_targets_check.py` (it needs the Makefile)
- [x] run the rewritten runners and script lines without a Makefile (`~/openfs98-verify/p3-check.sh`)

**Acceptance Criteria:**
- Given the Makefile, when `cmake_targets_check.py` compared all 409 phony targets, then 0 differ (done before deletion).
- Given a tree with no Makefile, when every rewritten break runner runs one break and the script lines run, then each behaves as before.

### Review Findings

Code review of the docs rewrite (AGENTS.md and docs/ except native-port; 2026-11-19; Blind Hunter, Edge Case Hunter,
Verification Gap, Acceptance Auditor on Sonnet 5.5). Patches applied in the review session.

- [x] [Review][Patch] Two Makefile pattern rules were never ported (the 409-target check compared phony targets only): `advdrv-break-%`, which `docs/modules/advdrv.md` uses, and `disasm-%` [cmake/targets.cmake] — now `tools/ofs advdrv-break BREAK=N [N=300]` and `tools/ofs disasm MOD=NAME`; run in the Makefile-free copy (BREAK 1, the documented equivalence, MISSED as recorded).
- [x] [Review][Patch] `building.md`'s setup block ran `tools/ofs stage` before the configure step `tools/ofs` needs [docs/building.md:12]
- [x] [Review][Patch] Present-tense Makefile references left: `make -B all` in multip.md, "The shared Makefile now builds" in alerts/help/sim/views.md, `tools/simsui.mk` and `MODULES`/`ENGLISH_MODULES`/`SHARED_MODULES` in remaining-work.md, the English list in building.md's install line; `ofs_help` now says it is Help.DLL's
- [x] [Review][Patch] `natives` and `radios` counts changed (95+3 -> 94+4, 115+5 -> 114+6) without the reason in 1-2b's notes — added (`col-ecx`, `n1-cx` proven equivalent)
- [x] [Review][Patch] remaining-work.md's "results updated through 2026-11-14"
- [x] [Review][Patch] AGENTS.md still said catch counts for seven suites predate the crash rule [AGENTS.md:32] — all seven were re-run under it in 1-2b; the line now names the twelve runners that apply the rule and points to the unchecked rest (user's say-so).
- [x] [Review][Patch] `tools/ofs smoke` reported 28/28 on the verification game copy against a 27/27 baseline [docs/tools.md:252] — the 28th was an `OFSTEST.DLL` left by an interrupted `ofstest-run`; 27/27 stands, and the line now names the installed set (every module) instead of an old partial list.

Rejected:
- false: `ofs_help` collides with a `help` listing target — the Makefile had none; it is `tools/help.mk`'s `help`, which builds Help.DLL.
- false: "409 targets" contradicts spec 1.2's "102 checked" — 102 were the tools.md targets of piece 1, 409 every phony target before deletion (notes above).
- false: SUI-13 lacks its code marker — `src/simsui/simsui_picture.c:52`.
- low: aliases `install`/`revert`/`clean`/`help` in `tools/ofs`; dated records rewritten to `tools/ofs` names that exist; column alignment in command blocks; building.md's second Building section; no description of a missing build dir or of `tools/modules.txt`'s format; owners for the open items; tools.md not naming the user's authorisation (1-2b records it).

## Implementation Notes

- Checker before deletion: 409 targets, 0 problems (with the two exceptions in its code: `tools/gauges-all.mk` given the mingw CC its caller passes, and `all-gauges-check`'s missing standalone N default).
- `tools/modules.sh` matched `make -s` for every query and module; it refuses an unknown name where make printed a made-up path.
- Install: reverting then installing a game copy through `tools/ofs ofs_revert` / `ofs_install FS98_DIR=<copy>` gave retail, then the build's files; `tools/ofs smoke` and `main-smoke` on that copy PASS.
- `$PWD` replaces the source root in converted commands (absolute paths stay absolute; piece 1 had made four of them relative, which broke `WINEPREFIX`).
- Bugs found on the way, fixed: `;` in commands split by CMake (fixed with `$<SEMICOLON>`), relative `WINEPREFIX`, `tools/smoketest.sh` missing OPTIONS/MP_DIALOG/SIMSUI install names, `panelsdiff unit` broken by 4ac3e38's `arg_count`, `panels_breaks.py` and `controls_breaks.py` counting any non-zero exit as a catch.
- Makefile-free run (`p3`, 2026-11-18/19): every rewritten break runner caught its break; `main-smoke`,
  `main-dialogs`, `flights-play`, `hg2d_frames.sh` and `smoke` PASS. Fixed on the way: a `framelock` target
  (`hg2d_frames.sh` needed the Makefile's file target `bin/hg2d-diff/FRAMELOCK.DLL`), `-Itools/difftest` in 56 break
  runners' harness builds, `panels_breaks.py` refreshing its worker copies, `flights_play.sh`'s exit trap, and
  `ofs_step` tracking a `tools/` harness's `.inc`/`.h` files (the Makefile's prerequisites had them; without it an
  edited fixture ran a stale `simsuidiff.exe`).

- Fixed after done (2026-11-19, from a later review): the port kept each command as `make -n` printed it, so a parameter that was empty at port time, or computed by make, was dropped or baked in. A per-target sweep of every overridable variable in the old recipes against `cmake/targets.cmake` found 19 targets. Now passed as `${VAR:-default}` (make's `$(if ...)`) or `${VAR-default}` (`?=`): `START` (panelsdiff-run, panelsunit-run, gauges-panels-run, gauges-panels), `COUNT`/`SEED` (panels-breaks), `SEED` (optionseventsdiff-run, simsuidiff-run), `SIM1R_N`/`SIM1R_SEED`, `HG2D_TIMEOUT`, `UTIL_TRACE_SECS`, `Q`/`D` (facildiff-fe-run), `VD` (run). `FE_FIXTURE` (orig/, else game/) and `FE_CD` (cd/ if cd/Boosters exists; windowdiff `.`) are computed at run time again, so their "needs ..." guards can fail again. Checked by running each command with `echo` in place of `wine`, with defaults and overrides.
- Also after done (2026-11-19): `src/world/sources.txt` and `src/simsui/sources.txt` are now the only copy of the WORLD and SIMSUI unit lists. `cmake/OpenFS98.cmake` reads them (a configure dependency) into `@WORLD_SOURCES@`, `@SIMSUI_SOURCES@` and `@SIMSUI_OBJECTS@`, and the 24 SIMSUI unit compiles are one loop in `cmake/steps/all.cmake`. All 577 generated build rules are byte-identical to before.

## Verification

- `tools/ofs <target>` for every break build and harness build in a Makefile-free copy -- built.
- `~/openfs98-verify/p3-check.sh` -- results in `~/openfs98-verify/p3-results/`.
