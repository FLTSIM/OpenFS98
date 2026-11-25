---
title: 'Story 1.8, slice 3: WINDOW, WINMSG and MPCHAT use arena.h'
type: 'refactor'
created: '2026-11-24'
status: 'done'
route: 'dispatch'
baseline_commit: 'ac3dbf551cc62488a685f017471c2e8358972d67'
review_loop_iteration: 1
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-1-8b-controls-panels-gauges-use-arena-h.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** After slice 2, WINDOW, WINMSG and MPCHAT still hold 78 banned `(uint32_t)(uintptr_t)` casts and 11 private definitions of `arena.h`'s names (`tools/arena_lint_allowed.txt` rows `src/mpchat/mpchat.c 8 3`, `src/window/window.c 68 4`, `src/winmsg/winmsg.c 2 4`).

**Approach:** Convert the three to `include/fs98/arena.h` as slices 1 and 2 did, delete their rows, and prove the change by byte identity with the packaged gate.

## Boundaries & Constraints

**Always:**
- **Each module** defines `FS98_DATA_IMAGE` before including `fs98/arena.h` (after `fs98/host.h`), and drops its private access typedefs (`win_v*`, `wm_v*`, `mc_v*`), `D` and `M8`…`M32`:
  - WINDOW: `FS98_DATA_IMAGE window_image.import`, today's anchor (the member at offset 0). `&window_image` changes `load_record`'s object code, so the whole object is not used here;
  - WINMSG: `winmsg_image`; MPCHAT: `mpchat_image`.
- `ADDR(p)` (WINMSG 21 call sites, MPCHAT 17) and every other banned cast become `fs98_handle(…)`: WINDOW's host-call arguments (`&count`, `&type`, `buf`, `path`, …), `CONST_AT`, callback arguments and Linkage stores; MPCHAT's five `CreateWindowExA` results and the `LoadLibraryA` result.
- **Reverse conversions:** dropping the private access macros removes 8 from the inventory. MPCHAT's `PTR(a)` and `STR(a)` become `fs98_ptr(a)` and `((const char *)fs98_ptr(a))` under their own names, removing 2 more: exactly 43 → 33. `WND(a)` (a Win32 handle) and every other reverse conversion stay; the slice adds its retained sites to slice 2's deferred-work entry.
- Module-local names that are neither banned nor casts stay: `CALL`, the import and slot macros, `PTR`, `STR`, `WND`.
- **Allowlist:** the three rows go, leaving 279 casts and 41 definitions across 55 files, checked with `--prior` on slice 2's accepted list.
- **Break runners** (`tools/window_breaks.py`, `winmsg_breaks.py`, `mpchat_breaks.py`, `mpchat_ui_breaks.py`):
  - each mutation matches exactly once and hits the same operation at the same site on both snapshots; strings the rewrite changed are updated;
  - WINMSG's `word-empty`, `split-fits` and `close-title` anchors already fail on HEAD (retail-bug comments inside them), so the suite runs 61 of 64. They are repaired on both sides, keeping their original change (the return value, the boundary, the first free removed), and the fix is recorded as a pre-existing runner defect;
  - pinned case counts: 109 WINDOW, 64 WINMSG, 117 MPCHAT model, 12 MPCHAT UI (the UI baseline excluded);
  - each gains `--build-only DIR`, run directly after `build_side.sh` (not through the break targets, which stage binaries or start wine first). The filter is checked before any directory is made; with no match it exits 1 and touches nothing. One case at a time, `DIR/<case>` with metadata; `finally` restores the original temporary source and outputs, their bytes, mtimes and existence. Any build failure is an error unless it is on an explicit expected list;
  - prerequisites the build-only path needs are built first and named in the notes (`bin/window/window_text_tail.inc`, `bin/mpchat-ui/mpchat.res`, the resources the model runners' `tools/ofs` compiles use);
  - `mpchat_ui_breaks.py` is broken on HEAD: it copies only `linkage.h` and `host.h`, which include `fs98/abi.h` since Story 1.7. It copies and hashes `abi.h`, `arena.h` and `tools/difftest/argcount.h` too, compiles the harness from the copy, and uses a fresh directory per case. This is recorded as a Story 1.7 regression fixed here; both sides use the fixed runner. Its harness binary is part of each case's evidence.
- **The gate** (`~/openfs98-verify/gate/`): `compare.py` gains `--require-set p18c`: the three production DLLs, `bin/difftest/{windowdiff,winmsgdiff,mpchatdiff}.exe`, `bin/mpchat-ui/mpchatuidiff.exe` and `bin/mpchat-ui/MPCHAT_ours.dll`, and exactly one before/after pair for each of the four mutant sets at the pinned counts. Snapshots at `~/openfs98-verify/p18c` at one absolute root, before then after, `build_side.sh` with the allowlist of `ac3dbf5` as prior. No replayed compiler flag may differ (the gate replays compiler flags, not linker options).
- **The acceptance rule** (Story 1.7's): byte identity stands for every difftest and the 74 flights. A difference left after trying needs:
  - an unmutated output: its model difftest at 20,000 with 0 differ, or the UI harness's 24 fixtures clean; and the 74 flights with Story 1.7's pinned settings and per-flight baseline comparison;
  - a differing mutant: its verdict before and after, at 20,000 for the model harnesses and on the same two fixtures for the UI runner; caught means the harness's mismatch exit 1 (`N differ` non-zero, or `MPCHAT UI DIFFER`), never a crash, timeout or guard exit, and a case documented as a miss must stay a miss;
  - serial, from a frozen copy, never beside another wine run.
- Builds run under `nice -n 19`, `-j 2`, `CMAKE_BUILD_PARALLEL_LEVEL=2`. No staging, commits or status changes to this spec.

**Never:**
- No other module is converted; `arena.h` and the lint change only to fix a defect this slice finds, logged in Implementation Notes.
- The Story 1.7 bridge stays (its own story).
- No change to what any module computes or exports.

</frozen-after-approval>

## Code Map

- **WINDOW:** `src/window/window.c:39-41` typedefs, `:43-57` `struct window_image` (`import[13]` at `+0x000`), `:59-62` `D`, `M8`…`M32`, `:88` `CALL` (keep); casts throughout (`&count` 10, `&type` 8, `&list` 8, `buf` 8, `path` 5, `&rec` 3, …); image definition `:3891`.
- **WINMSG:** `src/winmsg/winmsg.c:34-36` typedefs, `:53-57` `D`, `M8`…`M32`, `ADDR`; `:74` `CALL` (keep).
- **MPCHAT:** `src/mpchat/mpchat.c:41-42` typedefs (`mc_v8`, `mc_v32` only), `:75-81` `D`, `M8`, `M32`, `ADDR`, `PTR`, `STR`, `WND`; `:94` `CALL` (keep).
- Each module is one `.c` with no header, includes `fs98/linkage.h` and `fs98/host.h`, and its recipes already pass `-Iinclude`.
- **Runners:** `tools/window_breaks.py` (218 lines), `winmsg_breaks.py` (177), `mpchat_breaks.py` (209), `mpchat_ui_breaks.py` (107; copy list `:19-20`, compile `:60-68`); targets in `cmake/targets.cmake:325-358`, `:1493`, `:1517`, `:2116-2137`. Slice 2's `--build-only` in `tools/controls_breaks.py` is the pattern.
- **Gate:** `~/openfs98-verify/gate/compare.py` (`--require-set p18b` is the pattern), usage in slice 2's Implementation Notes.

## Tasks & Acceptance

**Execution:**
- [x] Gate: `--require-set p18c`; build the before snapshot at `p18c` from HEAD with every runner's `--build-only` mutants (the runner changes made first and used on both sides).
- [x] `src/window/window.c`, `src/winmsg/winmsg.c`, `src/mpchat/mpchat.c`: convert.
- [x] `tools/arena_lint_allowed.txt`: delete the three rows.
- [x] The four runners: sites, the three WINMSG anchors, `--build-only`, and `mpchat_ui_breaks.py`'s copy list.
- [x] Docs: `docs/modules/window.md`, `winmsg.md`, `mpchat.md` where they name removed macros; `docs/tools.md` for runner usage and the WINMSG count; `docs/implementation/deferred-work.md` (retained reverse sites).
- [x] Build the after snapshot; run the gate with all four mutant sets.
- [x] This spec: `## Implementation Notes` with the counts, the reverse-cast inventory, the mutant counts, the `mpchat_ui_breaks.py` finding and the gate result.

**Acceptance Criteria:**
- Given the slice, when `check_arena.py --self-test`, the lint, and the lint with `--prior` on slice 2's list run, then all pass with 279 casts and 41 definitions across 55 files, no row for the three files, and reverse casts at exactly 33.
- Given the gate with `--require-set p18c`, then the artifact sets and the required set match, every PE, replayed object and mutant DLL is identical after normalisation, and no replayed compiler flag differs; otherwise each difference is settled by the fallback above.
- Given each runner, when `--build-only` runs with a filter matching nothing, then it exits 1 and no source, DLL or mtime changed.

## Verification

**Commands:**
- `python3 -B tools/check_arena.py --self-test && python3 -B tools/check_arena.py` -- expected: pass, 55 files, 279 casts, 41 defines, reverse 33.
- `python3 -B tools/check_abi_headers.py` -- expected: pass.
- the gate at `~/openfs98-verify/p18c` with `--require-set p18c` -- expected: `GATE PASS` for the build and all four mutant sets.

## Implementation Notes

**Counts.** `check_arena.py --self-test` (37 cases), the plain run, and `--prior` on slice 2's accepted list (`~/openfs98-verify/p18c/prior.txt`, a copy of the allowlist before this slice) report 55 files, 279 casts, 41 definitions, reverse casts 33 (43 before). The three rows are gone. `check_abi_headers.py` passes. WINMSG converts 21 `ADDR` sites, MPCHAT 17 plus its five `CreateWindowExA` results and the `LoadLibraryA` result, WINDOW 58 `fs98_handle` sites (the host-call arguments, `CONST_AT`, the callback arguments and Linkage stores).

**Reverse-cast inventory.** The three modules' private `M8`/`M16`/`M32` (WINDOW 3, WINMSG 3, MPCHAT 2) leave the 43, and MPCHAT's `PTR`/`STR` (now `fs98_ptr`) 2 more: 43 to 33. Kept and logged in `deferred-work.md`: `WND(a)`, the `CALL` slot loads, WINDOW's `paint_fn` call and the call through `eax`, MPCHAT's `HINSTANCE` casts, WINMSG's `CharNextA` operand.

**Conversion.** `FS98_DATA_IMAGE` is `window_image.import` (WINDOW), `winmsg_image`, `mpchat_image`, defined after `fs98/host.h` and before `fs98/arena.h`. The private access typedefs, `D`, `M8`...`M32` and `ADDR` are gone; `PTR(a)` is `fs98_ptr(a)` and `STR(a)` is `((const char *)fs98_ptr(a))`.

**Break runners.** `--build-only DIR` in `window_breaks.py`, `winmsg_breaks.py` and `mpchat_breaks.py` through the new `tools/break_build_only.py` (the filter and every anchor are checked before a directory is made; a miss exits 1; one case at a time; the temporary source and `WINDOW_ours.dll`/`WINMSG_ours.dll`/`MPCHAT_ours.dll` are restored by bytes, mtimes and existence in `finally`; any build failure is an error, the expected list is empty), and in `mpchat_ui_breaks.py` (DLL and `harness.exe` per case, no baseline). Pinned counts: 109, 64, 117, 12. Strings rewritten: `ADDR(` to `fs98_handle(` in the WINMSG, MPCHAT and UI runners, the two `(uint32_t)(uintptr_t)` mutation anchors in `mpchat_breaks.py`, and `create-instance` (its call now ends `0));`). Prerequisites: `bin/window.res`, `bin/window/window_text_tail.inc`, `bin/winmsg.res`, `bin/mpchat.res`, `bin/mpchat-ui/mpchat.res`; `run_side.sh` checks them.

**Pre-existing runner defects, fixed on both sides.** `winmsg_breaks.py` `word-empty`, `split-fits` and `close-title` could not apply on HEAD (retail-bug comments sit inside their anchors; 61 of 64 ran). Repaired with the same change (the return value, the boundary, the first free removed). `mpchat_ui_breaks.py` has been broken since Story 1.7: it copied only `linkage.h` and `host.h`, which include `fs98/abi.h`. It now copies and hashes `abi.h`, `arena.h` and `tools/difftest/argcount.h`, compiles the harness from the copy, and uses a fresh directory per case; both sides used the fixed runner.

**Gate** (`~/openfs98-verify/gate/`): `compare.py --require-set p18c` (the three DLLs, the three difftests, `mpchatuidiff.exe`, `bin/mpchat-ui/MPCHAT_ours.dll`; exactly four `--mut` pairs at 109, 64, 117, 12; `harness.exe` compared as a normalised PE where a case has one). Built at `~/openfs98-verify/p18c` (`before-src` is the tree with the runner fixes only, `after-src` the converted tree, `build_side.sh` with the prior list, `lint-prior.log` rc 0 on the after side). `gate-result.txt`: **GATE PASS**: 1129 artifacts on both sides, 320 PEs and 206 objects identical, 522 replayed units with equal flags (no `-Iinclude` allowance was used) and `cmp`-identical objects, retail hashes equal, all 302 mutant DLLs normalised-identical with equal metadata. So no difftest, break verdict or flight was run; the fallback was not needed.

**Limits.** `before-src`'s `src/` was checked identical to `git archive ac3dbf5` (HEAD) and `after-src` to the working tree; the runners differ between the sides only in their mutation anchors. The 74 flights and the 20,000-scenario difftests were not run (identity stood in). The WINMSG and MPCHAT UI break verdict counts in `docs/tools.md` predate the runner repairs and were not re-run. Nothing was staged.

**Review hardening.** A missing or empty `--build-only` value exits 2 and every named filter must match a case (all four runners); `break_build_only.py` and the UI runner turn SIGTERM and SIGHUP into an exit so `finally` restores, and a failed restore does not skip the rest; the UI runner builds in a temporary directory beside DIR instead of `bin/mpchat-ui/breaks` and rejects a DIR inside it. `compare.py` rejects an unknown `--require-set`, identical before/after or `--mut` paths (realpath), a `meta.json` name that differs from its case directory, and a p18c `mut-mpchat-ui` case without `harness.exe`. Re-checked without rebuilding the snapshots: no-match, typo, empty and missing values rc 1 or 2 with `src`, `bin` and `tools` unchanged; a SIGTERM mid-run restored the tree; `sender-color` built into a scratch directory has the same `meta.json` and normalised-identical `mutant.dll` and `harness.exe` as the p18c after snapshot (raw bytes differ only in the PE timestamp); `compare.py` on the p18c snapshots still prints GATE PASS and fails on each negative case (unknown set, same-directory pair, a missing harness on both sides, a wrong meta name). The meta-name check is limited to p18c: slice 2's sets name their directories with a prefix or suffix (`sopcomp-comp-park`, `…-module`/`-unit`), and with it applied to p18b the slice 2 evidence failed 686 times; after the fix `compare.py` prints GATE PASS on both p18b (all five sets) and p18c.
