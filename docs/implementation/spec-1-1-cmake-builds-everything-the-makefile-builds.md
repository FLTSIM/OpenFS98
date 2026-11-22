---
title: 'CMake builds everything the Makefile builds'
type: 'chore'
created: '2026-11-15'
status: 'done'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43'
route: 'dispatch'
review_loop_iteration: 0
context: ['/OpenFS98/docs/native-port/architecture/ARCHITECTURE-SPINE.md', '/OpenFS98/docs/native-port/epics.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** The port builds only through a 1682-line Makefile plus 16 `tools/*.mk` sub-makes (recursive `$(MAKE) -f`). AD-11 requires CMake, with one toolchain file per target; nothing exists yet.

**Approach:** Add `CMakeLists.txt` and `cmake/toolchains/win32-retail.cmake` (mingw i686) that build every artifact `make all` builds, with the Makefile's flags, image bases and link options. Build tree and outputs are compared against the Makefile's. The Makefile stays until Story 1.2.

## Boundaries & Constraints

**Always:** Same flags/bases/entry point as the Makefile (`-nostdlib`, `--entry=_DllMain@12`, no dynamicbase/nxcompat, per-module `BASE_*`). Python generators and `orig/` inputs run as CMake custom commands as they do today. `install`/`revert` copy to `<name>.openfs98`, then `mv -f`. Any byte difference from the Makefile output is listed with its reason (paths, timestamps).

**Decisions:** Difftests are built by CMake as a non-default `difftests` target group, byte-compared like the rest. `cmake_minimum_required(VERSION 3.28)`.

**Never:** Change source, delete or edit the Makefile or `tools/*.mk`. Add the shared `OpenFS98::module` flag set (Story 1.3). Port verification targets (Story 1.2). Decompile data images (Story 1.4).

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|----------|--------------|---------------------------|----------------|
| Clean build | clean tree, `orig/` | all of `make all` produced | N/A |
| No `orig/` | `orig/` absent | configure stops with a message | N/A |
| Install | `FS98_DIR` with `Modules/` | each file copied via temp name then `mv` | missing dir: exit with message, as the Makefile |

</frozen-after-approval>

## Code Map

- `Makefile:1-94` -- CC/CFLAGS/LDFLAGS, `BASE_*`, module lists (MODULES, ENGLISH_MODULES, SHARED_MODULES)
- `Makefile:170-181` -- installed names/paths; `all`
- `Makefile:1630-1649` -- `install`/`revert`
- `tools/{world,sim1,mdl,gauges,gauges-all,simsui,host_rt,host_runtime,alerts,help,sim,views,flights,options,hg2d,mpdialog,mpchat}.mk` -- sub-makes for the non-pattern builds
- `src/<module>/`, `src/host/`, `tools/gen_*.py`, `tools/build_gauges.py`, `tools/build_mdls.py`, `tools/stage_sim1.py` -- sources and generators to wrap
- Existing `bin/` output -- byte-compare baseline (build Makefile into a copy first)

## Tasks & Acceptance

**Execution:**
- [x] `cmake/toolchains/win32-retail.cmake` -- mingw i686 compilers/windres, retail flags
- [x] `CMakeLists.txt` + per-area `cmake/*.cmake` -- 27 modules, 8 English DLLs, SIMSUI, SIM1, gauges, MDLs, OPENFS98.EXE, difftests (per Q1)
- [x] `cmake/install.cmake` -- `install`/`revert` targets
- [x] `tools/cmake_compare.py` -- compare every CMake output with the Makefile's; list differences with reasons
- [x] `docs/building.md` -- CMake commands added beside the Makefile ones

**Acceptance Criteria:**
- Given a clean tree and `orig/`, when configured with the toolchain and built, then every binary `make all` builds exists.
- Given both builds, when compared, then each matches byte for byte or is listed with its reason.
- Given `FS98_DIR`, when `install` then `revert` run, then files match the Makefile's results.

## Implementation Notes

- Steps in `cmake/steps/{all,difftests}.cmake` are the `make -n -B` command lists, verbatim, one `ofs_step` per output, ordered by target dependency in Makefile order (no parallelism; an edit rebuilds only its own step). Not idiomatic CMake targets yet: Story 1.3 replaces the per-rule flags.
- Output goes to `bin/` of the source tree (generators and windres hardcode it); build dir holds only CMake state.
- Generators run as custom commands; generated `.inc` are not tracked as inputs (shortcut, see `cmake/OpenFS98.cmake`).
- Install/revert are `ofs_install`/`ofs_revert` (`install` is reserved by CMake). Install and revert of a scratch game tree matched the Makefile's, PE timestamps aside.
- Verified in two scratch copies: 1234 files in `bin/`, 0 unexplained differences (`make all` + the 120 difftest exes vs `cmake --build` + `difftests`). Difftest baseline used `make -f tools/X.mk` for exes defined in sub-makes.
- Two converter slips found and fixed by the compare: a collapsed run of spaces in a `printf`, and `cc` instead of mingw gcc in two gauges-all steps (their sub-make was run standalone).

## Verification

**Commands:**
- `cmake -S . -B build/win32-retail -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/win32-retail.cmake && cmake --build build/win32-retail` -- expected: success
- `python3 tools/cmake_compare.py <makefile-tree> <cmake-tree>` -- expected: zero unexplained differences
