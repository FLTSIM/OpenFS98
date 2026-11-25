---
title: 'Story 1.8, slice 6a: MAIN uses arena.h'
type: 'refactor'
created: '2026-11-25'
status: 'done'
route: 'dispatch'
baseline_commit: '1d710e40dbdbe70071f16bd276257e795b8b1e8b'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-1-8e-demo-landme-use-arena-h.md', '{project-root}/OpenFS98/docs/implementation/spec-1-8d-host-uses-arena-h.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** After slice 5, MAIN still holds 3 banned `(uint32_t)(uintptr_t)` casts and 4 private definitions of `arena.h`'s names (`tools/arena_lint_allowed.txt` rows `src/main/main.c 2 4`, `src/main/main_frame.inc 1 0`), and 4 reverse casts (`main.c:42-44`, `main_engines.inc:7`).

**Approach:** Convert MAIN to `include/fs98/arena.h` as slices 1–5 did, delete the two rows, give `tools/main_breaks.py` a `--build-only` mode, and prove the change by byte identity with the packaged gate, including every one of the runner's mutants.

**Split (decided 2026-11-25):** slice 6 is four slices: 6a MAIN (this spec), 6b FLIGHTS + MPDIALOG + WORLD, 6c OPTIONS, 6d SIMSUI. VIEWS has no rows and needs nothing.

## Boundaries & Constraints

**Always:**
- **`main.c`:** defines `FS98_DATA_IMAGE main_image.bytes`, not `main_image`, immediately after the `extern` declaration, and includes `fs98/arena.h` there. A `-S` probe gave identical assembly for `.bytes`, while `&main_image`, the whole struct, changes code generation. It drops `main_v8`/`main_v16`/`main_v32`, `D` and `M8`…`M32`. `ADDR(p)` becomes `fs98_handle(p)`. `V`, `HOST`/`G`/`CHAIN`/`G3D`, `CALL` and every other module-local macro keep their names and bodies, since the break anchors use them.
- **`main_frame.inc:56`:** `RECORD_LISTENER` becomes `fs98_handle(main_record_listener)`.
- **`main_engines.inc:7`:** `M64F(a)` becomes `MD(a)`, keeping the name `M64F` because anchor `engines/t-store` uses it.
- **Reverse casts:** exactly 27 → 23. **Allowlist:** both rows go, leaving 100 casts and 27 definitions across 41 files, checked with `--prior` on slice 5's list (`git show 1497734:tools/arena_lint_allowed.txt`).
- **`tools/main_breaks.py --build-only DIR [SUITE...]`:**
  - covers all 44 suites, 2180 cases, by default;
  - builds each case's mutant `MAIN_ours.dll` from a mutated copy of `src/main` in a temporary directory outside the tree, into `DIR/<suite>.<name>/mutant.dll` with `meta.json`, whose `name` is `<suite>.<name>`. Fifteen bare names recur across suites;
  - calls the compiler directly with the `main-break-build` recipe's arguments (`tools/module_flags.py` for the module flags), not through `tools/ofs` or CMake, which write bookkeeping under the tree;
  - writes nothing under the tree and runs no wine;
  - exits 0 when every case built; 1 when a named suite does not exist, an anchor does not match exactly once, or a build fails (the first two before anything is made); 2 when DIR is an empty argument, or an existing directory that is not empty. A missing or empty directory is accepted;
  - SIGTERM and SIGHUP kill the compiler, clean up and exit non-zero;
  - the existing wine mode is unchanged.
- **Anchor repair:** `session/create-fail` (`main_kneeboard.inc`) and `spot/main-cmp1` (`main_spot.inc`) match nothing at HEAD, because Story 1.5 put RETAIL BUG and `Not reproduced` comments inside their anchors. Repair them as slice 4 did: the anchors take in the comment and the mutation stays the same. Log it as a pre-existing defect.
- **The gate** (`~/openfs98-verify/gate/`): `compare.py --require-set p18f`:
  - requires `MAIN.DLL` and `difftest/maindiff.exe` in `art.pe`;
  - requires exactly one replay of `src/main/main.c` per side, with equal flags and a byte-identical object;
  - requires exactly one `--mut` pair, `mut-main`, pinned at 2180: every `mutant.dll` identical after normalisation, and each `meta.json` name equal to its directory.
- **Snapshots** go at `~/openfs98-verify/p18f`:
  - before is `git archive` of HEAD plus `orig/`, with `tools/main_breaks.py` overlaid from the working tree (HEAD's has neither `--build-only` nor the anchor repairs). It is the only tools-only overlay and is recorded in the notes. After is the working tree plus `orig/`;
  - both are built at one absolute root by `build_side.sh`, which recreates the root, with the prior at `p18f/prior.txt`;
  - the order is strictly serial: build the before side, then its `--build-only` run into a fresh `p18f/before/mut-main`, then the same for the after side. Everything runs under `nice -n 19`;
  - a `gate.sh` checks that both runs logged exit 0, then runs `compare.py`.
- **Earlier gates:** after the `compare.py` change, p18b, p18c, p18d and p18e must still pass, with the same commands as slice 5's spec.
- **Negative checks** (on `cp -al` copies whose edited files are replaced, never written through), each of which must fail:
  - `MAIN.DLL` missing on both sides;
  - the `main.c` replay entry removed on both sides;
  - one mutant case removed on both sides;
  - a stray second `--mut`.
- **The acceptance rule** is Story 1.7's, as in slice 5.
  - The 2180 mutants must be identical, with no fallback: without wine, nothing else settles a changed mutant's verdict.
  - Only an explained content difference in the ordinary artifacts may use the fallback: `tools/ofs maindiff-run` for all 44 suites at their documented counts, and the 74 flights before and after, run serially from a frozen copy.
  - The two repaired cases are new evidence: they now build on both sides.
- Builds run under `nice -n 19`, `-j 2`, `CMAKE_BUILD_PARALLEL_LEVEL=2`. No staging, commits or status changes to this spec.

**Never:**
- No other module is converted. `arena.h` and the lint change only to fix a defect this slice finds, and that is logged.
- No change to what MAIN computes or exports, or to any break's mutation.
- The break counts in `docs/tools.md` are not re-run under wine. Identical mutants stand in for that.

</frozen-after-approval>

## Code Map

- **`src/main/main.c`** (single translation unit; the `main_*.inc` files are `#include`d):
  - `:23-24` includes; `:26` `MAIN_DATA_VA`; `:30-32` typedefs; `:34-38` `struct main_image` and `extern`; `:40` `D`; `:41` `V`; `:42-44` `M8`…`M32`; `:45` `ADDR`; `:56` `CALL`; `:258` the definition of `main_image`.
  - `D` is an `fs98_addr` of `main_image.bytes`. `fs98_handle(&main_image)` has the same value but different code generation (7874 lines of `-S` diff); `&main_image.bytes` gives identical assembly. arena.h's `fs98_v8/16/32` have the same qualifiers as MAIN's. `MD` adds `may_alias` to `M64F`'s volatile double, and the recipe already disables strict aliasing.
  - `--build-only` cost: about 3.3 s per build, 2180 cases, about 2 h per side.
- **`src/main/main_frame.inc:56`**, **`src/main/main_engines.inc:7`**.
- **Recipes:**
  - `cmake/steps/all.cmake:162` (MAIN.DLL, already `-Iinclude -Isrc/main`);
  - `cmake/targets.cmake:1442` `main-break-build` (`-Iinclude -I${MAIN_BREAK_DIR}`, using `bin/main.res` and `src/main/main.def`);
  - `maindiff.exe` (`cmake/steps/difftests.cmake:155-158`) loads the DLL and includes no MAIN source.
- **`tools/main_breaks.py`:** `BREAKS` maps suite → `(name, file, old, new[, why])` (`:31`), and `main()` is at `:2347`. One whole-DLL build takes about 3.3 s, so 2180 cases take about 2 h per side.
- **`tools/break_build_only.py`:**
  - the shared helper (`run` for DLLs, `run_compile` for objects). Its restore and signal handling can be reused;
  - MAIN differs in copying a directory and choosing the file per case, so it may get a third entry point or local code.
- **Gate:** `~/openfs98-verify/gate/compare.py` (the `p18d` block is the pattern for pinned `--mut` sets) and `~/openfs98-verify/p18d/gate.sh`.

## Tasks & Acceptance

**Execution:**
- [x] `tools/main_breaks.py` -- add `--build-only` and repair the two anchors -- the mutant set.
- [x] Gate: `--require-set p18f` and `p18f/gate.sh`. Build the before snapshot and its mutant set from HEAD.
- [x] `src/main/main.c`, `main_frame.inc`, `main_engines.inc` -- convert.
- [x] `tools/arena_lint_allowed.txt` -- delete the two rows.
- [x] Docs:
  - `docs/tools.md`: a `# Story 1.8 slice 6a` note for `--build-only`;
  - `docs/modules/main.md`, where it names removed macros;
  - `deferred-work.md`: the anchor defect.
- [x] Build the after snapshot and its mutant set; run the gate and the four negative checks; rerun p18b to p18e.
- [x] This spec: `## Implementation Notes` covering the counts, the gate result, the negative checks and the earlier gates.

**Acceptance Criteria:**
- Given the slice, when `check_arena.py --self-test`, the lint, and the lint with `--prior` on slice 5's list run, then all pass with 41 files, 100 casts, 27 definitions, no MAIN row, and reverse casts at exactly 23.
- Given `main_breaks.py --build-only DIR` on either snapshot, then it exits 0 with 2180 case directories. Given a nonexistent suite name it exits 1, and given an empty argument or a non-empty DIR it exits 2; in both cases it makes nothing. Given a SIGTERM during a build, it leaves no compiler running and exits non-zero. In every case the tree's files (a hash inventory, since the snapshot has no git) are unchanged afterwards.
- Given the gate with `--require-set p18f`, then it prints `GATE PASS`: the artifact sets match, every PE is identical after normalisation, `main.c` is replayed once per side with an identical object, and all 2180 mutants are identical. All four negative checks fail, and p18b to p18e still pass.

## Review Triage Log

Spec review (Codex gpt-6.2-sol, 2026-11-25), before approval:

| # | Finding | Verdict | Evidence / action |
|---|---|---|---|
| 1 | `FS98_DATA_IMAGE main_image` changes code generation | high | Reproduced: `-S` differs; `main_image.bytes` is identical. Spec now uses `.bytes`. |
| 2 | The before side cannot run `--build-only` (HEAD lacks it, and two anchors fail); `build_side.sh` recreates the root | high | Before overlays `tools/main_breaks.py`; the order is strictly serial. |
| 3 | A count-preserving substitution or identical wrong DLLs could pass; reconstruct each mutated input | false | General distrust of the gate's own evidence (rejected in slices 3–5). The runner, not hand-copied files, produces both sides. |
| 4 | Stale output; rehash retained PEs | medium (part) | DIR must be fresh or empty, and a non-empty one exits 2. Rehashing is general gate trust: rejected. |
| 5 | CMake route writes under the tree; signals; DIR definition | medium | Direct compiler call; the compiler is killed on a signal; DIR is defined; the tree is checked by hash inventory. |
| 6 | The fallback contradicts the mutant identity | medium | Mutants have no fallback; the fallback covers only ordinary artifacts. |
| 7 | Counts are correct; split the verification commands | low | Commands split. |

## Verification

**Commands:**
- `python3 -B tools/check_arena.py --self-test` -- expected: pass.
- `python3 -B tools/check_arena.py` and `python3 -B tools/check_arena.py --prior ~/openfs98-verify/p18f/prior.txt` -- expected: both pass, 41 files, 100 casts, 27 defines, reverse 23.
- `python3 -B tools/check_abi_headers.py` -- expected: pass.
- `bash ~/openfs98-verify/p18f/gate.sh` -- expected: `GATE PASS`; p18b to p18e -- expected: `GATE PASS`.

## Implementation Notes

**Counts.** `check_arena.py --self-test` (37 cases), the plain run, and `--prior` on slice 5's list (`~/openfs98-verify/p18f/prior.txt`) report 41 files, 100 casts, 27 definitions, reverse casts 23 (27 before); both MAIN rows are gone. `check_abi_headers.py` passes.

**Conversion.** `main.c` defines `FS98_DATA_IMAGE main_image.bytes` right after the `extern` and includes `fs98/arena.h` there; `main_v8/16/32`, `D` and `M8`...`M32` are gone; `ADDR(p)` is `fs98_handle(p)`; `V`, `HOST`/`G`/`CHAIN`/`G3D` and `CALL` are unchanged. `RECORD_LISTENER` is `fs98_handle(main_record_listener)`; `M64F(a)` is `MD(a)`. `docs/modules/main.md` names none of the removed macros, so it is unchanged.

**`--build-only`.** Local code in `tools/main_breaks.py` (not `break_build_only.py`): a temporary copy of `src/main` per case, the `main-break-build` arguments called directly. Exit codes checked on the p18f root: empty argument 2, non-empty DIR 2, unknown suite 1 (nothing made), SIGTERM 143 with no compiler left, and a full-tree sha256 inventory identical before and after all of them plus a 98-case `create` run. Both anchors (`session/create-fail`, `spot/main-cmp1`) were repaired and now build on both sides; logged in `deferred-work.md`. `tools/ofs` and wine were never used.

**Gate** (`compare.py --require-set p18f`, backup `compare.py.bak-pre-p18f`; `~/openfs98-verify/p18f/gate-result.txt`): GATE PASS. 1129 artifacts per side, 320 PEs and 206 objects identical (MAIN.DLL and maindiff.exe present), 522 replayed units cmp-identical with equal flags, `src/main/main.c` replayed once per side, all 2180 mutants identical with meta names equal to their directories. Before is HEAD plus the overlaid `tools/main_breaks.py` (the only tools overlay); both sides were built serially at one root under `nice -n 19`. No fallback was needed.

**Negative checks** (`p18f/negative.sh`, output in `negative-checks.txt`; scratch copies, files replaced never written through): MAIN.DLL missing on both sides, the main.c replay entry removed on both sides, one mutant case removed on both sides, and a stray second `--mut` each give GATE FAIL. p18b, p18c, p18d and p18e still print GATE PASS.

Code review (three Codex gpt-6.2-sol layers, 2026-11-25; B blind, E edge-case, V verification-gap):

| # | Finding | Verdict | Evidence / action |
|---|---|---|---|
| B1/E8/V3 | SIGTERM kills only the gcc driver; `cc1` survives | medium | Reproduced by review; breaks "SIGTERM kills the compiler". Patch: process group. |
| B2/E1 | A repeated suite dies with FileExistsError | low | Real. Patch: rejected with exit 1. |
| B3/E2/E3/E7 | DIR (or TMPDIR) inside the tree is accepted | medium | Breaks "nothing written under the tree". Patch: realpath check, exit 2. |
| B4 | `bin/main.res` prerequisite unchecked and undocumented | low | Patch: checked and documented. |
| E4 | Partial case directories remain after a failure | low | Rejected: the exit is non-zero, and DIR must be fresh for a rerun. |
| E5 | Strict decoding of compiler output | low | Patch: `errors="replace"`. |
| E6 | No compile timeout | low | Rejected: unlikely, and it would fail loudly by hanging. |
| V1 | Both sides use the same new runner, so a runner that skipped mutations would pass | medium | Real (the review substituted unmutated DLLs and got GATE PASS); spec triage #3 rejected a broader form. Patch (evidence only): every non-equivalent mutant must differ from the unmutated build, and three cases are cross-built through the legacy path. |
| V2 | Negative check n4 fails for an unrelated reason | medium | Real (the guard disabled in memory still failed). Patch: a valid pair passed twice, the p18f diagnostic required. |

**Review fixes.** `--build-only` now: starts each compiler in its own session and kills its whole process group on SIGTERM/SIGHUP (re-tested mid-build: exit 143, `ps` shows no gcc/cc1/ld of the run); resolves DIR with realpath and exits 2 if it, or the temp directory, is inside the repository; exits 1 on a repeated suite and, before creating DIR, when `bin/main.res` is missing (named in the usage line and the `docs/tools.md` note); decodes compiler output with `errors="replace"`. The compiler argv moved verbatim into `cc_argv()`. These edits touch only argument checks and signal handling, not the mutation text or the compiler argv, so the existing mutant sets stand; `gate.sh` was re-run at the end: GATE PASS. Negative check n4 now passes the valid `mut-main` pair twice and fails with `--require-set p18f needs exactly one pair of mut-main at 2180` (`negative-checks.txt` updated).

**Mutation evidence** (scratch, `p18f/evidence1.py`, `evidence2.py`): (1) an unmutated DLL built with `cc_argv` from the after root differs, after normalisation, from every one of the 2092 after-side `mutant.dll` whose meta has `equivalent: false` (none identical; the other 88 are the runner's own equivalent-marked cases). (2) `session.create-fail`, `spot.main-cmp1` and `radiodlg.st-none` built through the legacy path (copytree, replace, `tools/ofs main-break-build`, no wine) are normalised-identical to the `--build-only` output.
