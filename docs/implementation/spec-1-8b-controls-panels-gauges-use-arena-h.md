---
title: 'Story 1.8, slice 2: CONTROLS, PANELS and the gauges use arena.h'
type: 'refactor'
created: '2026-11-24'
status: 'done'
route: 'dispatch'
baseline_commit: '430772c056488da73484d931b836a0b53eaad344'
review_loop_iteration: 1
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-1-8-every-module-uses-the-shared-arena-h.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** After slice 1, seven files of CONTROLS, PANELS and the gauges still hold 115 banned `(uint32_t)(uintptr_t)` casts and 19 private definitions of `arena.h`'s names (`tools/arena_lint_allowed.txt` rows 9-10, 22-24, 51-52).

**Approach:** Convert them to `include/fs98/arena.h` as slice 1 converted REALITY and ANALYSIS, delete their seven allowlist rows, and prove the change by byte identity with the packaged gate, extended where slice 2 needs it.

## Boundaries & Constraints

**Always:**
- **CONTROLS:** `controls.h` defines `FS98_DATA_IMAGE controls_image` and `FS98_RDATA_IMAGE controls_rdata`, includes `fs98/arena.h` after `fs98/host.h`, and drops its `ct_v*` typedefs, `D`, `RDATA`, `M8`…`MD` and `ADDR`. Every `ADDR(p)` (39 in `controls.c`, which has no row, plus `controls_input.c`), `controls_input.c`'s private `P(x)`, its `h_str*` macros and its forward casts (`:101`, `:891`, `:896`) become `fs98_handle(…)`.
- **PANELS:** the same opt-in with `panels_image` and `panels_rdata`, plus `FS98_DATA_VA PN_DATA_VA` for the shared `DA`. `panels.h` drops `pn_v*`, `D`, `RDATA`, `M8`…`MD`, `DA`, `R8`…`R32`, `RDB`, `ADDR` and `P`. The renames:
  - the one-argument `.rdata` readers `RF(va)` and `RD(va)` become `PN_RF(va)` and `PN_RD(va)`, same bodies;
  - the record float `RFL(p, o)` becomes the shared `RF(p, o)`;
  - `ADDR` and `P` become `fs98_handle`, the 83 casts of `routine` (`panels.c:6411-6493`) included.
- **Gauges:** `gauge.h`'s casts (`gauge_rebind` and `GAUGE_ADDRESS`), `suction.c`'s and `warning.c`'s become `fs98_handle(…)`; each includes `fs98/arena.h`. `-Iinclude` is added to the test-gauge recipes (`cmake/targets.cmake:61,64`), the direct compile in `tools/gauges_breaks.py`, and `tools/build_gauges.py`. `build_gauges.py` also adds `arena.h` and `abi.h` to its freshness inputs and is run with `--jobs 2`.
- **Reverse conversions:** deleting the private access macros takes the reverse-cast inventory from 52 to 43; that is this slice's required drop. Other address-to-pointer and callback conversions in these files stay as they are, as do Win32 handle conversions and the `CALL`/`SVC`/`COM` slot loads. They are logged in `deferred-work.md` for the story that needs them off i386.
- Module-local names that are neither banned nor casts stay: `IMPORT`, `CALL`, `SVC`, `G8`…`G32`, `D8`…`D32`, the slot macros and the gauges' `GD*`/`GAUGE_*`.
- **Allowlist:** the seven rows are deleted, leaving 357 casts and 52 definitions across 58 files, checked with `--prior` on slice 1's accepted list.
- **Break runners** (`controls_breaks.py`, `panels_breaks.py`, `gauges_breaks.py` both modes, `gauge_breaks.py`):
  - each mutation hits the same operation at the same site on both snapshots. Strings the rewrite changed are updated; an anchor that matches more than once today keeps its first-occurrence site, made unique where it is rewritten;
  - `panels_breaks.py` freezes its sampled case list on the before side and the after side maps each case to the same operation and site; the two `var_lookup` duplicate-`case` mutations are expected compile failures on both sides;
  - each gains `--build-only DIR` on slice 1's pattern (one case at a time, `DIR/<case>` with metadata, the baseline source and built output restored in `finally`, non-zero exit on no match). `gauge_breaks.py` saves and restores the installed `.GAU` its manifest names;
  - expected counts, confirmed on the before side: 15 warning, 26 suction, 67 collection, the CONTROLS cases and the PANELS sample.
- **The gate** (`~/openfs98-verify/gate/`), extended:
  - `compare.py` accepts repeated `--mut B A COUNT`, and allows exactly one added `-Iinclude` on the 122 gauge compile units while still comparing their objects byte for byte;
  - `retail_hash.sh` also hashes `orig/Gauges/*.GAU`;
  - a required-output set for this slice: CONTROLS, both PANELS builds, both test gauges, the 120 manifest gauges and their harness executables.
  - Snapshots at `~/openfs98-verify/p18b`, before then after, `build_side.sh` with slice 1's list as prior.
- **The acceptance rule** (Story 1.7's): byte identity stands for every difftest and the 74 flights. A difference left after trying needs:
  - an unmutated output: its module's difftest in every affected mode (PANELS unit mode; the test gauges at preferred base and relocated) at 20,000 with 0 differ, and the flights;
  - a mutant: that case's verdict compared before and after at `N=20000`, where caught means the harness's mismatch exit with `N differ` non-zero, never a crash, timeout or guard exit;
  - both serial, from a frozen copy, with the same staged `game/`, never beside another wine run.
- Builds run under `nice -n 19`, `-j 2`, `CMAKE_BUILD_PARALLEL_LEVEL=2`. No staging, commits or status changes to this spec.

**Never:**
- No other module is converted; `arena.h` and the lint change only to fix a defect this slice finds, logged in Implementation Notes.
- The Story 1.7 bridge stays (its own story).
- No change to what any module computes or exports.

</frozen-after-approval>

## Code Map

- **CONTROLS:** `src/controls/controls.h:37-43` (`D`…`ADDR`, typedefs above), `:45-61` imports and `SVC`, `:77` `CALL`; `controls.c:186` on (`ADDR`); `controls_input.c:45-49` (`h_str*`, `P`), forward casts `:101`, `:891`, `:896`; reverse conversions such as `:206`, `:897`, `:1033` stay.
- **PANELS:** `src/panels/panels.h:13-47` (typedefs, image and access macros, `D8`…`D32`, `RD`/`RF` on `.rdata` at `0x1000e000`, `R8`…`R32`, `RFL`, `RDB`), `:49-82` imports and slot macros (keep). Only `panels.c` includes `panels.h`; its forward casts are all in `routine` (`:6411-6493`).
- **Gauges:** `src/gauges/gauge.h` (`gauge_rebind` `:30-32`, `GAUGE_ADDRESS` `:156`; included by the 120 sources, none of which uses an `arena.h` name); `suction.c:24-92`, `warning.c:25-77`. The harnesses in `cmake/steps/difftests.cmake:258-288` do not include `gauge.h`.
- **Builders and runners:** `tools/build_gauges.py:19-37,94`; `tools/gauges_breaks.py:30-85`; `tools/gauge_breaks.py:147-167`; `tools/panels_breaks.py:74-153`; `tools/controls_breaks.py`; slice 1's `--build-only` in `tools/reality_breaks.py`.
- **Gate:** `~/openfs98-verify/gate/replay.py:18` (keeps flags), `compare.py:11` (required outputs), `:37`, `:54` (one `--mut`), `retail_hash.sh:4`; usage in slice 1's Implementation Notes.

## Tasks & Acceptance

**Execution:**
- [x] Gate: extend `compare.py` and `retail_hash.sh`; build the before snapshot at `p18b` from HEAD with every runner's `--build-only` mutants (the runners' flag added first and used on both sides, as slice 1 did).
- [x] `src/controls/controls.h`, `controls.c`, `controls_input.c`: convert.
- [x] `src/panels/panels.h`, `panels.c`: convert and rename.
- [x] `src/gauges/gauge.h`, `suction.c`, `warning.c`: convert; `-Iinclude` and freshness in `cmake/targets.cmake`, `tools/build_gauges.py`, `tools/gauges_breaks.py`.
- [x] `tools/arena_lint_allowed.txt`: delete the seven rows.
- [x] The four break runners: sites, frozen PANELS cases, `--build-only`.
- [x] Docs: `docs/modules/controls.md`, `panels.md` and the gauges' doc where they name removed macros; `docs/tools.md` for runner usage; `docs/implementation/deferred-work.md` for the reverse conversions left.
- [x] Build the after snapshot; run the gate and every mutant set.
- [x] This spec: `## Implementation Notes` with the counts, the reverse-cast inventory, the mutant counts and the gate result.

**Acceptance Criteria:**
- Given the slice, when `check_arena.py --self-test`, the lint, and the lint with `--prior` on slice 1's list run, then all pass with 357 casts and 52 definitions across 58 files, no row for the seven files, and reverse casts at 43 or fewer.
- Given the gate, then the artifact sets and the required-output set match, every PE, replayed object and mutant DLL is identical after normalisation, retail hashes match, and the only command-line difference is the allowed `-Iinclude`; otherwise each difference is settled by the fallback above.
- Given each runner, when `--build-only` runs with a filter matching nothing, then it exits non-zero and the baseline source and DLL are untouched.

## Verification

**Commands:**
- `python3 -B tools/check_arena.py --self-test && python3 -B tools/check_arena.py` -- expected: pass, 58 files, 357 casts, 52 defines, reverse ≤ 43.
- `python3 -B tools/check_abi_headers.py` -- expected: pass.
- the gate at `~/openfs98-verify/p18b` -- expected: `GATE PASS` for the build and every mutant set.

## Implementation Notes

**Counts.** The lint (`check_arena.py --self-test`, the plain run, and `--prior` on slice 1's committed list, `~/openfs98-verify/p18b/prior.txt`) reports 58 files, 357 casts, 52 definitions, reverse casts 43 (52 before). The seven rows are gone. `check_abi_headers.py` passes.

**Conversion.** CONTROLS and PANELS define `FS98_DATA_IMAGE`/`FS98_RDATA_IMAGE` (PANELS also `FS98_DATA_VA PN_DATA_VA`) and include `fs98/arena.h` in their headers; `ADDR`, `P` and the `h_str*` casts, `controls_input.c`'s three forward casts and the 83 casts of PANELS' `routine` are `fs98_handle`. PANELS' `RF(va)`/`RD(va)` are `PN_RF`/`PN_RD` and `RFL(p, o)` is the shared `RF(p, o)`. `gauge.h`, `suction.c` and `warning.c` use `fs98_handle` and include `arena.h`. `-Iinclude` is in the two test-gauge recipes, `build_gauges.py` and `gauges_breaks.py`; `build_gauges.py` has `arena.h` and `abi.h` as freshness inputs and runs with `--jobs 2` (the `all` step and `gauge_breaks.py`; the `all` step's glob also lists the two headers).

**Break runners.** `--build-only DIR` in all four. Strings changed: `controls_breaks.py` (`P(`/`ADDR(` to `fs98_handle(`; every anchor still matches once, checked on both sources), `gauges_breaks.py` (`failure-root-address`). `panels_breaks.py` adds `--freeze FILE` and `--cases FILE`: the 200-case SEED 1 sample is frozen on the before side as (function, line, operation, nth match on the line) with its name, and the after side maps each to the same site (its candidate list is identical, 3611 on both sources, once the `off4` pattern accepts `RF` beside `RFL`). Cases are kept as `DIR/<case>-module` and `-unit`; the two `var_lookup` duplicate-`case` mutants fail to compile on both sides and are recorded as `"unbuilt": true` (4 dirs). `gauge_breaks.py` saves and restores the installed `.GAU`. No-match filters exit 1 in all four with the baseline untouched (tried).

**Gate** (`~/openfs98-verify/gate/`): `compare.py` takes repeated `--mut`, `--require-set p18b --manifest` (CONTROLS, `PANELS.DLL` and `PANELS_unit.dll`, both test gauges, the 120 manifest gauges, their harness executables) and allows exactly one added `-Iinclude` on the 122 `src/gauges` units (counted, objects still compared byte for byte); `retail_hash.sh` hashes `orig/Gauges/*.GAU`. Built at `~/openfs98-verify/p18b` (`before-src` from HEAD, `after-src` from the tree, `build_side.sh` with slice 1's list as prior, `lint-prior.log` rc 0). Result, `gate-result.txt`: **GATE PASS**: 1129 artifacts on both sides, 320 PEs and 206 objects identical, 522 replayed units with flags equal (bar the 122 `-Iinclude`) and objects `cmp`-identical, retail hashes equal, mutants: 81 CONTROLS, 276 PANELS dirs (200 cases), 15 warning, 26 suction, 67 collection, all normalised-identical with equal metadata. So no difftest, break verdict or flight was run; the fallback was not needed.

**Process note.** The first PANELS pass failed on both sides (`PANELS_retail.dll` is made by `tools/ofs panelsdiff panelsunit`, which the runner's usage names); PANELS was rerun after that on the after tree and on a fresh build of `before-src` at the same root (`run_panels.sh`), using the frozen sample. Every other set came from the first full builds.

**Not done / limits.** Reverse conversions left are logged in `deferred-work.md`. The flights and 20,000-scenario difftests were not run (identity stood in, per the acceptance rule). Nothing was staged.

**Review patches.**
- `gauge_breaks.py` restores the saved `.GAU` with its original mtime. `controls_breaks.py` and `gauge_breaks.py` check that the filter matches a case (and, for `gauge_breaks.py`, that each gauge is known) before touching anything; the no-match runs of all four runners left `src/`, `bin/difftest`, `bin/gauges-all/Gauges` and `bin/gauges-diff` unchanged (a hash of names, mtimes and sizes before and after), each exiting 1.
- `panels_breaks.py`: only `var_lookup:3508:const9` and `:const31` may fail to build (their metadata keeps the compiler's stderr); any other build failure exits non-zero. `--build-only` runs one case at a time. `--freeze` stores each site's line text, and `--cases` fails if the line differs from it as the conversion rewrote it (checked on all 6,545 lines of the two sources). The frozen file was regenerated from the HEAD source with the same 200 names.
- `compare.py`: `-Iinclude` is allowed on exactly the 122 gauge sources (the 120 manifest names plus warning and suction), each replayed once on both sides; `--require-set p18b` needs all five mutant sets; an unbuilt pair must be one of the four expected directories on both sides (diagnostics compared, not the echoed source excerpt, which shows the renamed `PN_RD`). `retail_hash.sh` hashes every file under `orig/`, symlinks followed, and fails unless the 120 gauges and `ENGLISH.FLL` are in it; both sides' `retail.sha256` were regenerated from their snapshots.
- Re-run: only the two `var_lookup` PANELS cases (4 directories, now with stderr), on a fresh before build and a fresh after build; the other 272 PANELS directories and the other four sets are unchanged, as no runner change alters their DLLs or metadata. `compare.py` with all five sets: GATE PASS. The lint, its self-test and `--prior` pass at 58 files, 357 casts, 52 defines, reverse 43.
- `deferred-work.md` logs that the PANELS and warning/suction verdicts cannot supply the fallback as written.
