---
title: 'Story 1.8, slice 4: HOST uses arena.h'
type: 'refactor'
created: '2026-11-24'
status: 'done'
route: 'dispatch'
baseline_commit: '9b951693c0090cbce63e52b26757b848cee5be8c'
review_loop_iteration: 1
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-1-8c-window-winmsg-mpchat-use-arena-h.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** After slice 3, HOST still holds 43 banned `(uint32_t)(uintptr_t)` casts and 5 private definitions of `arena.h`'s names, over 10 allowlist rows: `src/host/host.c 3 5`, `src/host/host_rt_exe.c 3 0`, and eight `src/host/runtime/host_rt_*.c` rows (child 9, common 1, cursor 1, floating 2, frame 2, main 6, misc 1, mouse 15).

**Approach:** Convert HOST (the services and the runtime) to `include/fs98/arena.h` as slices 1–3 did, delete the 10 rows, and prove the change by byte identity with the packaged gate.

## Boundaries & Constraints

**Always:**
- **`host.c`:**
  - defines `FS98_DATA_IMAGE` before including `fs98/arena.h` (after `runtime/host_rt.h`): `*fs98_host_data` under `HOST_IMAGE_SHARED` (`bin/host-rt/host_services.o`), `host_image.bytes[0]` otherwise. A probe showed that the whole `host_image` changes `host_290`;
  - drops `host_v8`/`host_v16`/`host_v32`, both `D`s, `M8`…`M32` and `ADDR`;
  - keeps `V(a)` as module-local, on `D`;
  - turns all 290 `ADDR(…)` sites and the other banned casts into `fs98_handle(…)`.
- **The runtime:** the eight runtime files with a row, and `host_rt_exe.c`, include `fs98/arena.h` themselves, not through `host_rt.h` (which `tools/difftest/hostrtdiff.c` also includes). Their banned casts become `fs98_handle(…)`. `host_rt.h`'s macros stay.
- **`-Iinclude`** is added only where a converted file is compiled without it:
  - `host_rt_common.c`'s four standalone compiles (`cmake/targets.cmake:830/850/1342/1414`);
  - the `hostrtdiff.exe` recipe (`cmake/steps/difftests.cmake:309`);
  - `tools/host_rt_breaks.py`'s `CC`.
- **Reverse conversions:** dropping `host.c`'s `M8`…`M32` takes the inventory exactly 33 → 30. `host.c:1242` and the cursor's outer `(uint8_t *)(uintptr_t)(…)` stay, and are added to the deferred-work entry.
- **Allowlist:** the 10 rows go, leaving 236 casts and 36 definitions across 45 files, checked with `--prior` on slice 3's accepted list (`9b95169`).
- **Break runners** (`tools/host_breaks.py`, `tools/host_rt_breaks.py`):
  - every mutation, old and new text alike, matches exactly once and hits the same operation at the same site on both snapshots. That includes the 38 HOST cases that hold `ADDR` (`tx-bad-kind` ends at a bare `ADDR`) and the runtime's `child-command-float`, `spot-hit` and `modeless-took`;
  - `host_breaks.py`'s `ab-ret`, `sd-stop`, `vw-rows-skip`, `vw-block-grow`, `tx-font0`, `tt-right` and `tt-dup` already fail to match on HEAD. They are repaired on both sides with their original change, and recorded as a pre-existing runner defect;
  - pinned counts: 505 HOST, 569 runtime; the expected-build-failure list is empty;
  - **`--build-only DIR`**, run directly after `build_side.sh` from the snapshot root:
    - a case's evidence is the mutated unit's object (`DIR/<case>/mutant.o` plus `meta.json`). It is compiled with `-c`, from a copy with the same basename in a temporary directory, so nothing under the tree is written;
    - the profiles are pinned. HOST: `OFS_MODULE_FLAGS -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/host`. Runtime: the runner's `CC` without link options, plus `-I src/host/runtime -Iinclude`;
    - `DIR/baseline/` holds every unmutated unit the runner links, compiled with the runner's own profile: `host.c` and the five `-DHOST_RT_STANDALONE` companions; `hostrtdiff.c` and the 19 runtime files. The runners' link inputs are gated that way. Linking 505 DLLs and 569 harnesses per side would add nothing over this;
    - `tools/break_build_only.py` gains an output name and a temporary-copy mode, and keeps its argument checks (an empty or missing value exits 2; an unmatched name exits 1 before anything is made). Slice 3's runners still produce byte-identical outputs, checked on one case each.
- **The gate** (`~/openfs98-verify/gate/`), `compare.py --require-set p18d`:
  - **Required:** `OPENFS98.EXE`, `HOST.DLL`, `difftest/hostdiff.exe`, `host-rt/OPENFS98.EXE` and `host-rt/hostrtdiff.exe` in `art.pe`; `host-rt/host_services.o` and `host-rt-standalone/host_rt_common.o` in `art.obj`.
  - **Mutant sets:** exactly one pair each of `mut-host` (505) and `mut-host-rt` (569). In each case `mutant.o` is compared byte for byte (PE normalisation is for PEs only), `meta.json` is equal and names its directory, and both baselines are identical.
  - **Flags:** one added `-Iinclude` is allowed on each of exactly 21 replay entries, matched by recipe, source and the other flags, and required on both sides: the 20 units of the `hostrtdiff.exe` command and the standalone `host_rt_common.c` entry. No other replayed flag may differ.
  - **Negative checks:** each of these must fail: a required object missing on both sides, a mutant missing on both sides, a wrong meta name on both sides, and a misplaced `-Iinclude`.
  - **Snapshots:** `~/openfs98-verify/p18d`, before then after at one absolute root, via `build_side.sh` with slice 3's list as prior. After any `compare.py` change, the p18b and p18c gates must still pass.
- **The acceptance rule** (Story 1.7's): byte identity stands for every difftest and the 74 flights. Only an explained content difference may use the fallback; missing evidence, a flag violation or a runner failure is a gate failure. The fallback is:
  - `hostdiff` at 20,000 with 0 differ, and `hostrtdiff` at 20,000 with its baseline of 161 operations, 3,220,000 comparisons and 0 differ;
  - the 74 flights with Story 1.7's settings and per-flight comparison, with each snapshot's `OPENFS98.EXE` selected through `HG2D_FRAMES_HOST`;
  - for a differing mutant, the same verdict before and after, one named case at a time. A catch is the harness's mismatch exit 1 with its mismatch record (`N differ` non-zero, or `DIFFER`); crashes, timeouts and guard exits are recorded apart;
  - all of it serially, from a frozen copy, never beside another wine run.
- Builds run under `nice -n 19`, `-j 2`, `CMAKE_BUILD_PARALLEL_LEVEL=2`. No staging, commits or status changes to this spec.

**Never:**
- No other module is converted; `arena.h` and the lint change only to fix a defect this slice finds, logged.
- The Story 1.7 bridge stays.
- No change to what HOST computes, exports or links.

</frozen-after-approval>

## Code Map

- **`src/host/host.c`** (5580 lines):
  - `:31-33` typedefs, `:35-45` `host_image` (`AT` push/pop around the `.inc`), `:54-66` `D` (shared, else standalone), `V`, `M8`…`M32`, `ADDR`; `:1242` reverse cast (keep); `host_290` `:4309` is where a whole-object `D` differs;
  - standalone recipes at `cmake/targets.cmake:831/851/1415`. `:1343` is the break build of `${HOST_BREAK:-}` (`-Isrc/host`); all have `-Iinclude`;
  - shared recipe: `cmake/steps/all.cmake:462`.
- **The runtime:** `src/host/runtime/host_rt.h` (`D32`, `PTR`, `AT`, `VA`, `G32` …).
  - Casts: `host_rt_common.c:33`; `child.c:48,431-438`; `mouse.c:101-227,334`; `main.c:60,68,302,346,437,442`; `floating.c:63,66`; `frame.c:98,207`; `misc.c:53`; `cursor.c:147`.
  - `src/host/host_rt_exe.c:84` is a statement; `:85` and `:102` are macro definitions.
  - Compiled in `OPENFS98.EXE` (`all.cmake:465`, which already has `-Iinclude`, then copied to `bin/OPENFS98.EXE`), in `hostrtdiff.exe` (`difftests.cmake:309`), and as five standalone objects (`targets.cmake:822-830`, `842-850`, `1334-1342`, `1406-1414`).
  - `tools/host_rt_survey.py` and `gen_host_rt_tables.py`'s probe are untouched.
- **Runners:**
  - `tools/host_breaks.py`: `break_pool`, `one()` `:614`, `tools/ofs host-break-build HOST_BREAK=…`; 505 cases.
  - `tools/host_rt_breaks.py`: `BREAKS` rows `(name, file, before, after)`, `CC` `:43`, `build()` `:661` (sorted copied sources plus `tools/difftest/hostrtdiff.c`); 569 cases.
  - The helper is `tools/break_build_only.py`, which today hard-codes `mutant.dll` and one in-place source.
- **Gate:** `~/openfs98-verify/gate/compare.py`:
  - required-artifact check `:46` (only `art.pe` today); mutant comparison `:105`;
  - slice 2's `-Iinclude` allowance is the pattern;
  - replay deduplication is in `replay.py:25`, and break targets are skipped by `ondemand.py:8`.
  - The p18b and p18c rerun commands are in spec-1-8c's notes.

## Tasks & Acceptance

**Execution:**
- [x] Gate and runners first: `--require-set p18d`, the helper extension, and both runners' `--build-only` with the anchor repairs. Then build the before snapshot at `p18d` from HEAD.
- [x] `src/host/host.c`, `src/host/host_rt_exe.c` and the eight runtime files: convert.
- [x] `cmake/targets.cmake`, `cmake/steps/difftests.cmake`, `tools/host_rt_breaks.py`: `-Iinclude`.
- [x] `tools/arena_lint_allowed.txt`: delete the 10 rows.
- [x] Docs: `docs/modules/host.md` where it names removed macros; `docs/tools.md` (runner usage, and that the HOST count predates the anchor repair); `docs/implementation/deferred-work.md` (the retained reverse sites, the anchor defect).
- [x] Build the after snapshot; run the gate with both sets and the negative checks; rerun p18b and p18c.
- [x] This spec: `## Implementation Notes` (counts, inventory, mutant counts, gate result).

**Acceptance Criteria:**
- Given the slice, when `check_arena.py --self-test`, the lint, and the lint with `--prior` on slice 3's list run, then all pass with 236 casts and 36 definitions across 45 files, no HOST row, and reverse casts at exactly 30.
- Given the gate with `--require-set p18d`, then the artifact sets and the required set match, and every PE, replayed object, baseline object and mutant object is identical. No flag differs beyond the 21 counted `-Iinclude`, each negative check fails, and p18b and p18c still pass.
- Given each runner, when `--build-only` runs with a filter matching nothing, then it exits 1 and no source, binary or mtime under the tree changed.

## Verification

**Commands:**
- `python3 -B tools/check_arena.py --self-test && python3 -B tools/check_arena.py` -- expected: pass, 45 files, 236 casts, 36 defines, reverse 30.
- `python3 -B tools/check_abi_headers.py` -- expected: pass.
- the gate at `~/openfs98-verify/p18d` with `--require-set p18d` -- expected: `GATE PASS`; p18b and p18c -- expected: `GATE PASS`.

## Implementation Notes

**Counts.** `check_arena.py --self-test` (37 cases), the plain run, and `--prior` on slice 3's accepted list (HEAD's `arena_lint_allowed.txt`, saved as `~/openfs98-verify/p18d/prior.txt`; note that `~/openfs98-verify/p18c/prior.txt` is slice 2's list, not slice 3's) report 45 files, 236 casts, 36 definitions, reverse casts 30 (33 before). The 10 rows are gone. `check_abi_headers.py` passes.

**Conversion.** `host.c` defines `FS98_DATA_IMAGE` as `(*fs98_host_data)` under `HOST_IMAGE_SHARED` and `host_image.bytes[0]` otherwise (the whole `host_image` changes `host_290`), after `runtime/host_rt.h` and before `fs98/arena.h`; `V(a)` stays module-local on `D`. `host_v8/16/32`, `D`, `M8`...`M32` and `ADDR` are gone, and every `ADDR(` site (254 lines) became `fs98_handle(`. The nine runtime files (eight with a row, and `host_rt_exe.c`) include `fs98/arena.h` themselves; their 40 banned casts became `fs98_handle(...)`. `-Iinclude` was added to the four standalone `host_rt_common.c` compiles, the `hostrtdiff.exe` recipe and `tools/host_rt_breaks.py`'s compile.

**Reverse-cast inventory.** `host.c`'s private `M8`/`M16`/`M32` leave it: 33 to 30. Kept and logged in `deferred-work.md`: `host.c:1242`, the other `(T *)(uintptr_t)` conversions, the Win32 handle and callback casts, and the cursor's outer `(uint8_t *)(uintptr_t)(...)`.

**Runners.** `--build-only DIR` in `host_breaks.py` (505 cases) and `host_rt_breaks.py` (569) through `break_build_only.run_compile`: each case's unit is compiled with `-c` from a same-basename copy in a temporary directory to `DIR/<case>/mutant.o` with `meta.json` (`name` = the directory); `DIR/baseline/` holds `host.o` plus the five `-DHOST_RT_STANDALONE` companions (6 files) and `hostrtdiff.o` plus the 19 runtime files (20). A filter matching nothing exits 1 and an empty or missing value exits 2, before anything is made; nothing under the tree is written. Pinned profiles as in the spec. Slice 3's runners (window, winmsg, mpchat) were rerun on one case each in the p18d after tree: `meta.json` byte-equal, `mutant.dll` normalised-identical to `p18c/after` (raw bytes differ only in the PE header timestamp and checksum, as the roots and times differ). Mutation anchors: `ADDR(` to `fs98_handle(` in `host_breaks.py` (38 cases, `tx-bad-kind`'s bare `ADDR` included) and the three runtime anchors (`child-command-float`, `spot-hit`, `modeless-took`).

**Pre-existing runner defect, fixed on both sides.** `host_breaks.py` `ab-ret`, `sd-stop`, `vw-rows-skip`, `vw-block-grow`, `tx-font0`, `tt-right` and `tt-dup` could not apply on HEAD (retail-bug comments sit inside their anchors, and `tt-right`'s also matched twice); 498 of 505 ran. Repaired with the same change by including the comments. The 505 in `docs/tools.md` predates it.

**Gate** (`~/openfs98-verify/gate/compare.py --require-set p18d`, backup `compare.py.bak-pre-p18d`; `~/openfs98-verify/p18d/gate.sh`, result `gate-result.txt`): **GATE PASS**. 1129 artifacts on both sides, 320 PEs and 206 objects identical (the five required PEs and the two required objects present), 522 replayed units with `cmp`-identical objects, retail hashes equal, `lint-prior.log` rc 0. One trailing `-Iinclude` was added on exactly 21 replayed units (the 20 of the `hostrtdiff.exe` command and the standalone `host_rt_common.c`), matched by recipe tail and source. All 505 `mut-host` and 569 `mut-host-rt` `mutant.o` are byte-identical with equal `meta.json` naming their directories, and the baselines (6 and 20 objects) are identical. So no difftest, flight or break verdict was run; the fallback was not needed. `before-src` is `git archive 9b95169` plus `orig/` and the runners (`break_build_only.py`, `host_breaks.py`, `host_rt_breaks.py`), which differ from `after-src`'s only in the mutation anchor strings; both built at `~/openfs98-verify/p18d/root` before then after.

**Negative checks** (hard-linked scratch copies, never the snapshots): each fails with GATE FAIL: `host-rt/host_services.o` removed from `art.obj` on both sides (2 differences); `mut-host/tt-right/mutant.o` removed on both sides; `meta.json` name `wrong` on `const-one` on both sides; a misplaced `-Iinclude` (before `-Isrc/host/runtime` on `host_rt_mouse.c`) on the after side (41 differences). The compare.py changes did not disturb slices 2 and 3: p18b (five sets: 81, 276, 15, 26, 67) and p18c (109, 64, 117, 12) both print GATE PASS.

**Limits.** The 74 flights and the 20,000-scenario difftests were not run (identity stood in). The HOST and runtime break verdict counts in `docs/tools.md` were not re-run. The `before` and `after` `src/` were not hashed against `git archive` beyond the file list above (they differ only in the 13 source/recipe files plus runners). Nothing was staged.

**Anchor rule.** "Matches exactly once" is applied to the old text (the anchor). The replacement text need not be unique: `spot-hit`'s, `return fs98_handle(element);`, occurs 13 times in the mutated source by nature.

**Review hardening.** `compare.py --require-set p18d` now matches the `-Iinclude` allowance by name: `tools/difftest/hostrtdiff.c` and the 19 named `src/host/runtime/host_rt_*.c` under the `hostrtdiff.exe` profile, and `host_rt_common.c` under the `-DHOST_RT_STANDALONE` profile, each exactly once and present on both sides; any missing or extra entry fails. The baselines must hold exactly the expected file names (`host.o` plus the five companions; `hostrtdiff.o` plus the 19 runtime objects): a missing, extra or renamed file fails. A malformed `meta.json` is a DIFF, not a crash. `run_compile` fails a compile that exits 0 without an object, turns SIGTERM and SIGHUP into an exit (the temporary directory is removed) and kills and reaps the compiler on interruption. `p18d/run_side.sh` exits with the runners' status and `gate.sh` fails unless all four `--build-only` runs are logged with exit 0 (checked against the existing logs). The normal paths still build from a scratch copy of `after-src` (`p18d/normal-build/`): `host-break-build` with an unmutated and a mutated `host.c` (with `HOST_BREAK=src/host/host.c` the DLL is normalised-identical to `HOST.DLL`), and `host_rt_breaks.build()` for `spot-hit`. Slice 3's runners rebuilt one case each (`p18d/slice3-compat/`): `meta.json` byte-equal, `mutant.dll` normalised-identical to p18c. The p18d, p18b and p18c gates were rerun and print GATE PASS; `p18d/negative.sh` runs seven negative checks (the four above, a replay entry swapped for another runtime filename, a renamed baseline file, a malformed `meta.json`), each fails (`negative-checks.txt`). The no-match, missing and empty-value tests on both HOST runners exit 1, 2, 2 with the tree unchanged. The `p18d` snapshots and their mutant sets were not rebuilt; the snapshots' `break_build_only.py` copies were refreshed to this version.

