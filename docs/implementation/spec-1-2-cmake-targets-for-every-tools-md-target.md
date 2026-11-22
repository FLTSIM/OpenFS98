---
title: 'CMake targets for every tools.md target'
type: 'chore'
created: '2026-11-16'
status: 'done'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43'
route: 'dispatch'
review_loop_iteration: 0
context: ['/OpenFS98/docs/native-port/epics.md', '/OpenFS98/docs/implementation/spec-1-1-cmake-builds-everything-the-makefile-builds.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Story 1.2 as written (port every verification target, re-verify baselines, retire the Makefile) is three deliverables. Only the first is this spec; pieces 2 and 3 are in `deferred-work.md`.

**Approach:** Every `make` target named in `docs/tools.md` (about 102) becomes a CMake target of the same name, running the same commands after the same CMake build steps. Variables (`N`, `SUITE`, `JOBS`, `MODE`, `FS98_DIR`, `GAUGES_RELOCATE`, `M`) are read from the environment with the Makefile's defaults.

## Boundaries & Constraints

**Always:** A target's commands equal what `make -n <target>` prints, minus build steps already in `cmake/steps/`, which become target dependencies. Same names, so `cmake --build <dir> --target maindiff-run` with `SUITE=records` in the environment replaces `make maindiff-run SUITE=records`.

**Never:** Run the targets under wine or compare against baselines (piece 2). Edit `docs/tools.md`, `AGENTS.md`, scripts, the Makefile or `tools/*.mk` (piece 3). Tool scripts keep calling `make -s modules-of` and the like (the query targets), and `make` itself, until piece 3.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|----------|--------------|---------------------------|----------------|
| Same commands | any ported target | its command list equals `make -n`'s, after substitution | check fails, naming the target |
| Parameter set | `SUITE=nav` in environment | `nav` appears where `make SUITE=nav` puts it | N/A |
| Parameter unset | variable absent | the Makefile's default is used | N/A |

</frozen-after-approval>

## Code Map

- `docs/tools.md` -- the target list (`make [-f tools/X.mk] <target>`)
- `Makefile`, `tools/*.mk` -- recipes; `make -n` prints them with variables expanded
- `cmake/steps/{all,difftests}.cmake`, `cmake/OpenFS98.cmake` -- the build steps (Story 1.1) and `ofs_step`
- `cmake/install.cmake` -- `ofs_install`/`ofs_revert`: `install`/`revert`/`gauges-install`... map here

## Tasks & Acceptance

**Execution:**
- [x] `cmake/targets.cmake` -- one custom target per tools.md target, depending on the step targets it needs
- [x] `tools/cmake_targets_check.py` -- for each target, compare `make -n` with the CMake recipe (`cmake --build ... --target X` dry), report differences
- [x] `docs/building.md` -- how to pass parameters

**Acceptance Criteria:**
- Given every target in `docs/tools.md`, when the check runs, then each CMake recipe matches `make -n`'s, or its difference is listed with a reason.
- Given a target taking `SUITE`, when `SUITE` is set in the environment, then the command carries it; when unset, the Makefile's default.

### Review Findings

Code review of the story's code (1.2, 1.2b, 1.2c; 2026-11-19; Blind Hunter, Edge Case Hunter, Verification Gap,
Acceptance Auditor on Sonnet 5.5). The patches were applied in the review session.

- [x] [Review][Patch] `run` and `compare` still called `make -s installed-path-of` / `orig-module-of`, gone with the Makefile, over hard-coded module lists [cmake/targets.cmake:133,736] — now `tools/modules.sh`; both commands run.
- [x] [Review][Patch] Ported echoes and an error message still said `make run`, `make ddraw-default`, `make stage` [cmake/targets.cmake:240,1083,2000,2007]
- [x] [Review][Patch] Hand-written `src/` `.inc` files were not step inputs (the Makefile had `MAIN_SOURCES`' 34) [cmake/OpenFS98.cmake:34] — globbed beside `*.h`; touching `main_aircraft.inc` rebuilds MAIN.DLL once, a second build does nothing.
- [x] [Review][Patch] `cmake_verify.py` passed lines it never checked: no number to look for gave PASS on exit 0, `\d[\d,]{2,}` read "67," as a count, numbers matched as substrings, timings read as counts [tools/cmake_verify.py:61,96] — new verdict UNCHECKED, anchored matching, timings skipped. The ~50 lines it had not checked were read by hand: all match except `reg_contracts.sh` (every run NO-VERDICT: FRAMELOCK.DLL had no target yet; re-run) and `gauge_breaks.py` / `gauges-breaks` (66 of 67, `chrn-minutes`, same through make and at N=20000; recorded, open).
- [x] [Review][Patch] A timeout killed only the shell; wine and Xvfb children ran on into the next line [tools/cmake_verify.py:89] — own session, `killpg`.
- [x] [Review][Patch] `panels_breaks.py` returned CRASHED at the unit mode's crash without giving the module mode its run [tools/panels_breaks.py:159] (no mutant of the recorded runs crashed, so their numbers stand)
- [x] [Review][Patch] `ofs_step` lacked `ofs_target`'s `@SEMI@` handling [cmake/OpenFS98.cmake:11]
- [x] [Review][Patch] `smoketest.sh` kept its own module-to-path table, which had already drifted [tools/smoketest.sh:82] — `tools/modules.sh installed-path-of`, old fallback kept.
- [x] [Review][Patch] `gauge_breaks.py` exited 0 with a miss, so its line passed [tools/gauge_breaks.py:180] (found verifying the above)
- [x] [Review][Patch] `.gitignore`'s `build/` matched at any depth [.gitignore] — `/build/`; `modules.sh`'s comment named "the Makefile's tools/modules.sh".
- [x] [Review][Defer] Break runners other than the nine fixed may still count a crash as a catch [tools/*_breaks.py] — deferred, unverified: read each runner's verdict test (`world_*`, `help_ui`, `mpchat_ui`, ...).
- [x] [Review][Defer] No tests for `tools/modules.sh` and `tools/ofs` [tools/modules.sh] — deferred: matched `make -s` for every query before the Makefile went; a table test would guard future edits.
- [x] [Review][Defer] Generated `src/**/*.inc` are still not step inputs [cmake/OpenFS98.cmake:3] — deferred to Story 1.4 (shortcut noted in the file).

Rejected:
- false: `-Itools/difftest` reads the live tree's headers — a quoted include is found beside the work copy's source first.
- false: `install` should keep `.orig` for plain modules — the Makefile did not; `revert` copies from `orig/`.
- false: SIMSUI equivalents should keep a 5th tuple element — that is main_breaks' convention; simsui_breaks lists them in its docstring. SUI-13 is in `docs/retail-bugs.md`.
- false: `M` not honoured — it only parameterised the module queries, now `tools/modules.sh NAME`.
- false: the `trap ... || true` changes are unexplained — under `set -e` a `kill` with nothing to kill exited 2 after PASS (spec-1-2c notes).
- false: ClientToScreen's 10000 overflows — sums stay far from int32 limits; simsuidiff N=2000 0 differ.
- low: concurrent `tools/ofs` in panels' workers and cmake noise in their stderr — results matched the documented N=150 count.
- low: `cmake_verify.py` argument, resume, env-merge and leftover `make`-line handling; `cmake_compare.py` truncated-PE and UTF-16 path edge cases (a Story 1.1 one-off); `install_run.cmake` partial install on a missing DLL, empty lists, quoting in `FS98_DIR`; `modules.sh` duplicate rows; `tools/ofs` odd variable names; `ofs_clean` as `make clean`; no frozen-copy check; break-runner regex strictness; `maindiff.c` unchecked pointers (maindiff N=20000, 0 differ); `panelsdiff.c`'s `unit` test comment.
- spec: 1.2's checker was deleted with the Makefile, as 1.2c authorised; the fix would edit the spec.

## Implementation Notes

- 102 targets: `make -n -B <target>` with parameters as placeholders, minus lines equal to a build step (those become `openfs98` / `difftests` dependencies; `difftests` builds all 120 exes, an over-build kept for correctness since steps share generated files).
- Parameters: `N SUITE JOBS MODE FS98_DIR GAUGES_RELOCATE`, `${VAR:-default}` with the Makefile's default. Repo root becomes relative, `$HOME` stays.
- Shortcut: helper builds inside a target run every time (make -B), not only when stale.
- Checked by `tools/cmake_targets_check.py`: 102 checked, 0 problems, defaults and parameters set. No target was run under wine (piece 2).
- Query targets (`modules-of`, ...) are not ported: scripts still call `make -s`; piece 3 rewrites them.

## Verification

**Commands:**
- `python3 tools/cmake_targets_check.py` -- expected: every target matches or is explained
