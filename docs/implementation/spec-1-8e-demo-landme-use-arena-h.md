---
title: 'Story 1.8, slice 5: DEMO and LANDME use arena.h'
type: 'refactor'
created: '2026-11-24'
status: 'done'
route: 'dispatch'
baseline_commit: 'a7103d44d08b9aacfebfb8863ceac62ac1249281'
review_loop_iteration: 1
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-1-8d-host-uses-arena-h.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** After slice 4, DEMO and LANDME still hold 133 banned `(uint32_t)(uintptr_t)` casts and 5 private definitions of `arena.h`'s names (`tools/arena_lint_allowed.txt` rows `src/demo/demo.c 128 4`, `src/landme/landme.c 5 1`).

**Approach:** Convert both to `include/fs98/arena.h` as slices 1–4 did, delete the two rows, and prove the change by byte identity with the packaged gate.

## Boundaries & Constraints

**Always:**
- **DEMO:**
  - defines `FS98_DATA_IMAGE demo_image` (its `D` is already `&demo_image`) before including `fs98/arena.h`, after `fs98/host.h`;
  - drops `demo_v8`/`demo_v16`/`demo_v32`, `D` and `M8`…`M32`;
  - every banned cast becomes `fs98_handle(…)`: host-call arguments, callbacks and Linkage stores. Module-local `CALL`, `IMPORT` and the call macros stay.
- **LANDME:**
  - its `D` is a byte pointer (`(uint8_t *)(uintptr_t)&landme_image`), not an `fs98_addr`, so it is renamed to the module-local `LM_D`, with the same definition and every use. It does not become `arena.h`'s `D`, and `FS98_DATA_IMAGE` stays undefined there;
  - it includes `fs98/arena.h`, and its five casts (`:907-917`: two callback tables, three Linkage stores) become `fs98_handle(…)`.
- **Reverse conversions:** dropping DEMO's `M8`…`M32` takes the inventory exactly 30 → 27. LANDME has none.
- **Allowlist:** the two rows go, leaving 103 casts and 31 definitions across 43 files, checked with `--prior` on slice 4's accepted list (`git show a7103d4:tools/arena_lint_allowed.txt`).
- **No module-specific mutant sets:** no break runner mutates or compiles either source (OPTIONS' `demo` cases are `src/options/options_demo.c`). The ordinary build, the gate's replay, and the read-only scans (`check_arena.py`, `check_abi_headers.py`, `check_raw_dumps.py`) are the only consumers. The evidence is the two DLLs, their harnesses and the four replayed units.
- **The gate** (`~/openfs98-verify/gate/`): `compare.py --require-set p18e`:
  - accepts no `--mut`;
  - requires `DEMO.DLL`, `LANDME.DLL`, `difftest/demodiff.exe` and `difftest/landmediff.exe` in `art.pe`, as a subset of the full artifact set. PEs are compared after normalisation;
  - requires exactly one replay of each of `src/demo/demo.c`, `src/landme/landme.c`, `tools/difftest/demodiff.c` and `tools/difftest/landmediff.c` on each side, with equal flags and byte-identical objects. No replayed compiler flag may differ: both DLL recipes already pass `-Iinclude`.
- **Snapshots** go at `~/openfs98-verify/p18e`:
  - before is `git archive` of HEAD plus the reference `orig/`; after is the working tree, with the same `orig/`;
  - both are built at one absolute root, before then after, by `build_side.sh`;
  - the prior is `git show a7103d4:tools/arena_lint_allowed.txt`, saved as `p18e/prior.txt`, outside the root that `build_side.sh` recreates.
- **Earlier gates:** after the `compare.py` change, p18b, p18c and p18d must still pass:
  - p18b: in `~/openfs98-verify/p18b`, `python3 -B ../gate/compare.py before after --require-set p18b --manifest after-src/src/gauges/gauges_manifest.json`, plus `--mut before/mut-X after/mut-X COUNT` for controls 81, panels 276, warning 15, suction 26, collection 67;
  - p18c: in `~/openfs98-verify/p18c`, `--require-set p18c` with window 109, winmsg 64, mpchat 117, mpchat-ui 12;
  - p18d: `bash ~/openfs98-verify/p18d/gate.sh`.
- **Negative checks** (on scratch copies), each of which must fail:
  - a required PE missing on both sides;
  - one required replay entry removed from both sides;
  - a stray `--mut`.
- **The acceptance rule** (Story 1.7's): byte identity stands for every difftest and the 74 flights. Only an explained content difference, left after trying for identity and recorded, may use the fallback. Missing evidence, a failed build or replay, and a flag difference are gate failures. The fallback:
  - `tools/ofs demodiff-run N=20000` and `tools/ofs landmediff-run N=20000` against the frozen candidate, each 20,000 scenarios with 0 differ;
  - the 74 flights, before then after, with `FRAMES_MANY_JOBS=1 FRAMES_MANY_PASSES=900 tools/frames_many.sh -`, using each snapshot's DLLs. Each flight's verdict is kept separately, and every flight must pass the same way;
  - all of it serially, from a frozen copy, never beside another wine run.
- Builds run under `nice -n 19`, `-j 2`, `CMAKE_BUILD_PARALLEL_LEVEL=2`. No staging, commits or status changes to this spec.

**Never:**
- No other module is converted (`src/options/options_demo.c` belongs to OPTIONS' slice). `arena.h` and the lint change only to fix a defect this slice finds, and that is logged.
- The Story 1.7 bridge stays.
- No change to what either module computes or exports.

</frozen-after-approval>

## Code Map

- **DEMO:** `src/demo/demo.c` (3413 lines).
  - `:32-33` includes; `:39-41` typedefs; `:92` `extern struct demo_image demo_image`; `:94-97` `D`, `M8`…`M32`; `:99` `IMPORT` (on `M32`, kept); `:124` `CALL` (keep).
  - Of the 128 casts, `D` holds one, leaving 127 `fs98_handle` sites. They are mostly host-call arguments: `path` 14, `dir` 10, five `&block`… (two whole-block addresses, three member addresses), and callbacks such as `demo_paused`, `demo_tick` and `replay_message`.
  - Recipe: `cmake/steps/all.cmake:80` (`-Iinclude`), followed by `check_data_image.py --hash DEMO`. Harness: `tools/difftest/demodiff.c` (`cmake/steps/difftests.cmake:79`), which loads the DLL and does not include `demo.c`.
- **LANDME:** `src/landme/landme.c` (928 lines).
  - `:18-19` includes; `:56` `D`, used by `D8`/`D16`/`D32`/`DPTR`/`IMPORT` (`:57-62`), the `LAND_*` macros (`:106-128`) and 10 direct uses on nine lines (`:202`, `:249`, `:341`, `:557`, `:616`, `:642-646`, `:802`).
  - The casts are at `:907-917`: two callback-table sites and three Linkage stores.
  - Recipe: `all.cmake:150` (`-Iinclude`), followed by `check_data_image.py --hash LANDME`. Harness: `tools/difftest/landmediff.c` (`difftests.cmake:151`).
- **Gate:** `~/openfs98-verify/gate/compare.py` (`--require-set p18d` is the pattern; `KEY` lists, the `art.pe` check); `~/openfs98-verify/p18d/gate.sh`.

## Tasks & Acceptance

**Execution:**
- [x] Gate: `--require-set p18e` with its three negative checks. Build the before snapshot at `p18e` from HEAD.
- [x] `src/demo/demo.c`, `src/landme/landme.c`: convert.
- [x] `tools/arena_lint_allowed.txt`: delete the two rows.
- [x] Docs: `docs/modules/demo.md` and `landme.md` where they name removed macros or LANDME's `D`.
- [x] Build the after snapshot; run the gate and the negative checks; rerun p18b, p18c and p18d.
- [x] This spec: `## Implementation Notes` (counts, inventory, the LANDME rename, gate result, negative checks, earlier gates).

**Acceptance Criteria:**
- Given the slice, when `check_arena.py --self-test`, the lint, and the lint with `--prior` on slice 4's list run, then all pass with 103 casts and 31 definitions across 43 files, no row for either file, and reverse casts at exactly 30 → 27.
- Given the gate with `--require-set p18e`, then the artifact sets, the four required PEs and the four required replays match. Every PE is identical after normalisation and every replayed object byte for byte, with no replayed flag differing; otherwise each explained difference is settled by the fallback above. All three negative checks fail, and p18b, p18c and p18d still pass.

## Verification

**Commands:**
- `python3 -B tools/check_arena.py --self-test && python3 -B tools/check_arena.py` -- expected: pass, 43 files, 103 casts, 31 defines, reverse 27.
- `python3 -B tools/check_abi_headers.py` -- expected: pass.
- the gate at `~/openfs98-verify/p18e` with `--require-set p18e` -- expected: `GATE PASS`; p18b, p18c, p18d -- expected: `GATE PASS`.

## Implementation Notes

**Counts.** `check_arena.py --self-test` (37 cases), the plain run, and `--prior` on slice 4's list (`~/openfs98-verify/p18e/prior.txt`) report 43 files, 103 casts, 31 definitions, reverse casts 27 (30 before). The two rows are gone. `check_abi_headers.py` passes.

**Conversion.** `demo.c` defines `FS98_DATA_IMAGE demo_image` and includes `fs98/arena.h` where its private `D`/`M8`...`M32` were; `demo_v8/16/32` are gone; all 127 banned casts (one more was `D`'s) became `fs98_handle(...)`. `landme.c` includes `fs98/arena.h`, its `D` is renamed `LM_D` (same definition, every use; `FS98_DATA_IMAGE` stays undefined), and its five casts became `fs98_handle(...)`. `docs/modules/demo.md` and `landme.md` name none of the removed macros, so they are unchanged.

**Gate** (`compare.py --require-set p18e`, backup `compare.py.bak-pre-p18e`; `~/openfs98-verify/p18e`, result `gate-result.txt`): GATE PASS. 1129 artifacts per side, 320 PEs and 206 objects identical (DEMO.DLL, LANDME.DLL, demodiff.exe, landmediff.exe present), 522 replayed units `cmp`-identical with equal flags; each of the four required sources is replayed exactly once per side. `--mut` is rejected. No difftest or flight was run (identity stood in).

**Negative checks** (current evidence: `p18e/rebuild.log`, `neg3-valid-mut.txt`): on scratch copies whose edited files are replaced, never written through, DEMO.DLL removed from `art.pe` on both sides and the landme.c replay entry removed on both sides each give GATE FAIL (2 differences); a stray `--mut` with a valid p18c WINDOW pair gives GATE FAIL with exactly the diagnostic `--require-set p18e takes no --mut`. p18b, p18c and p18d still print GATE PASS. `check_arena-self-test.txt` and `check_abi_headers.txt` in `p18e` hold the self-test and ABI outputs (rc 0).

**Incident.** A first negative-check attempt wrote through a hard link and emptied both sides' `units/manifest.txt`; they were regenerated with the compile stubbed. That evidence was discarded: `p18e` was rebuilt from scratch by `p18e/rebuild.sh` (both sides `replay.out`: `units 522 fails 0`), and the gate result and negative checks above are from that rebuild.

**Review hardening.** `compare.py`'s p18e replay count normalises the unit path against its working directory, so `./src/demo/demo.c` counts as `src/demo/demo.c`. Not done (general gate trust, not this slice): reconciling the manifest with the logged compiler invocations, rehashing retained binaries, checking the candidate's lint inventory inside the gate (checked by hand: 43/103/31/27).
