---
title: 'Story 1.9, slice a: FS98_RETAIL_ABI gates the register wrappers'
type: 'refactor'
created: '2026-11-25'
status: 'done'
baseline_commit: '27a9e496c2495c57b970c4e265054fce528b76e1'
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/native-port/architecture/ARCHITECTURE-SPINE.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** AD-4 says the register-keeping wrappers and `.S` stubs build only under `FS98_RETAIL_ABI` (win32-retail). No such macro exists yet. Each place keys on its own condition:
- `include/fs98/keep.h`, HG2D's raw `FS98_ENTRY` and UTIL's 30 private wrappers, with UTIL's slot selector in `util.c`, all test `__i386__`, so linux-i386 would get them.
- `abi.h`'s `FS98_STDCALL` and `FS98_RETAIL_PTR` test `_WIN32 && __i386__`.
- The six `src/` `.S` stubs test nothing.

**Approach:** Define `FS98_RETAIL_ABI` once, in a new assembler-safe header, and key exactly those places on it. Prove by whole-build byte identity that win32-retail is unchanged. Story 1.9 is split, as decided on 2026-11-25:
- **1.9a (this slice):** gates only, converting nothing.
- **1.9b–e:** in `deferred-work.md`. They give the entries explicit values, and gate and convert the other register-carrying families. Those families are listed in the 1.9e entry.

## Boundaries & Constraints

**Always:**
- **Definition:** a new `include/fs98/retail_abi.h` holds only preprocessor lines: `#if defined(_WIN32) && defined(__i386__)` / `#define FS98_RETAIL_ABI 1`.
  - Its comment cites AD-4. It says the predicate is an interim proxy for the win32-retail target, valid while the Windows native target is x64. A future Windows-i386 target needs explicit build selection first.
  - Nothing else defines the macro, and no `-D` passes it.
- **Gated places**, each including the header and testing `#if defined(FS98_RETAIL_ABI)`:
  - **Headers:** `include/fs98/abi.h` (`FS98_ABI_STDCALL_`, `FS98_RETAIL_PTR`) and `include/fs98/keep.h` (the wrapper branch; its other branch is kept). Both include it as `"retail_abi.h"`. Update the include rule in `abi.h`'s comment.
  - **HG2D:** `src/hg2d/hg2d.c:213`, the raw `FS98_ENTRY` only. It includes `"../../include/fs98/retail_abi.h"`, as it already does for `keep.h`.
  - **UTIL:** all 30 wrappers, gated together with their slot selection:
    - `src/util/util_math.c:493-504` (`rand`, `rand32`);
    - `src/util/util_math.c:1646-1725` (the `ENTRY_NAME`/`KEEP_*` block and its raw stubs);
    - the `_entry` declarations and selector branch in `src/util/util.c:59`.

    Use the include spelling that resolves in UTIL's recipes, which have `-Iinclude`.
  - **The six `.S` stubs**, whole body wrapped:
    - `src/sim1/sim1_{stack,state_stack,velocity}.S` and `src/mdl/mdl_crash_entry.S`, which include `"fs98/retail_abi.h"`: their recipes and runners pass `-Iinclude`.
    - `src/help/help_stack.S` and `src/views/views_stack.S`, which include `"../../include/fs98/retail_abi.h"`: their DLL recipes have no `-Iinclude`. HELP-2 and VIEW-1 are retail stack bugs, retail-only all the same.
- **No argv changes:** no compile or link command, recipe, toolchain file or runner changes. Every other `__i386__` condition stays.
- **Probe** (`~/openfs98-verify/p19a/probe.sh`). Every preprocessor call must exit 0, and the outputs are saved.
  - **Headers:** a test TU uses every `FS98_KEEP_*` form, `FS98_ENTRY`, `FS98_KEPT(e, f)`, `FS98_STDCALL` and `FS98_RETAIL_PTR`. It is preprocessed twice:
    - **Off:** host `gcc -m32 -ffreestanding -E -Iinclude`. No `__asm__`, `KEPT` gives `f`, `STDCALL` is empty, and `FS98_RETAIL_PTR` is undefined.
    - **On:** `i686-w64-mingw32-gcc -E` with the module flags. Each wrapper definition, `KEPT` giving `e`, the stdcall attribute and `FS98_RETAIL_PTR` are present.
  - **`.S` files:**
    - **Off:** host `gcc -m32 -E` with the file's recipe flags emits no assembly tokens.
    - **On:** the retail compiler keeps each file's expected `.globl` definitions.
  - **Whole C files** (`hg2d.c`, `util_math.c`, `util.c`): retail preprocessing with each recipe's flags gives:
    - HG2D's `draw_depth_point_entry` definition;
    - all 30 UTIL `_entry` definitions;
    - `util.c`'s slot table naming the 30 entries.

    A disposable copy of the tree, whose `retail_abi.h` defines nothing, preprocessed with the same flags, gives none of those definitions. Its slot table names the C functions.
- **Proof:**
  - **Snapshots:** before is `git archive` of the baseline, after is the working tree. Both get the gitignored fixtures. They are built by `build_side.sh` at one absolute root, serially, under `nice -n 19` with `-j 2` and `CMAKE_BUILD_PARALLEL_LEVEL=2`. The baseline's `tools/arena_lint_allowed.txt` is frozen as `PRIOR_INVENTORY`.
  - **Compare:** `compare.py --strict`, which rehashes the kept files at compare time. The gate's revision and the exact commands are recorded.
  - **`p19a/verify.py`** checks:
    - **compiles:** both sides' `gcc.log`s, normalised, have equal full argv and equal invocation counts;
    - **units:** every changed file is compiled on both sides;
    - **artifacts:** the HELP, VIEWS, HG2D, UTIL and MDL artifacts and the SIM1 artifacts that link a stub are present, and their raw bytes are equal apart from what `pehash` already masks, with the masked fields listed.
  - **Break runners that compile a changed file:** one case each is built on the after side, with no wine. That is through `--build-only` where the runner has it, otherwise its mutant compile command on a frozen copy. Each must exit 0 and produce its PE. The runners are those for HG2D, MAIN, ATC, NUMERIC, DYNAMIC, WEATHERM, UTIL, SIM1, MDL crash, HELP and VIEWS that compile one.
- **Negative check:** on a `cp -al` copy, replace one replayed `.S` object with a different valid object. The gate and `verify.py` must exit 1, naming that unit.
- **Acceptance:** `GATE PASS` and `verify.py` passing, with no fallback. Identical bytes make difftests, `reg_contracts.sh` and the flights unnecessary.
- **Running:** in a git worktree off HEAD; copy nothing into the main tree except this spec. Slice 6b is changing `compare.py` concurrently, so run after 6b's gate change lands, and confirm `--strict` still passes on 4b's kept outputs first.

**Never:**
- No entry gets explicit values, no macro is renamed and no caller changes.
- None of the other families is touched; they are 1.9e's: SIM1's inline wrappers, HG2D's texture shims and draw/device entries, DYNAMIC's `_dyn_frame` thunk, CHAIN/ANALYSIS, FE, CONTROLS and MULTIP's tick shims.
- No `tools/difftest/` `.S` file, toolchain or CMake option changes.
- No commits or staging.

**Decided (2026-11-26, user):** `--strict` cannot pass on an identical pair; the default gate plus `verify.py`'s raw comparison of every kept file stands in for it.

</frozen-after-approval>

## Code Map

- **`include/fs98/keep.h:11-33`:** the wrappers are under `__i386__`. The other branch has empty macros and `FS98_KEPT` returning the function. Its users are `hg2d.c:212-220`, `atc.c:400-401`, `dynamic.c:2633`, `numeric.c:1872-1874`, `main.c:321-322` and `weatherm.c:1080`.
- **`include/fs98/abi.h:24-35`:** may include only `<stdint.h>`, and every fs98 header includes it.
- **UTIL:** the wrappers are at `util_math.c:493-504` and `:1646-1725` (28 entries there, 30 in all). The selector is in `util.c:59`. The recipes, `cmake/steps/all.cmake:242` and `cmake/targets.cmake:682,726,1685`, all have `-Iinclude`.
- **`.S` consumers:**
  - SIM1 partial and test builds: `cmake/targets.cmake:105-106,1839-1912`, `tools/sim1_breaks.py:64`, `tools/sim1_state_breaks.py:92`.
  - MDL crash: `tools/gen_mdl_crash.py:20`, `tools/mdl_crash_breaks.py:154`.
  - HELP: `cmake/steps/all.cmake:308`, `difftests.cmake:298`, `tools/help_ui_breaks.py:51`. Its `-Itools/difftest` resolves the relative spelling back to `include/`.
  - VIEWS: `all.cmake:332`, `difftests.cmake:406`.
  - HG2D's mutant (`tools/hg2d_breaks.py:1859`) resolves through `-I…/src/hg2d`.
- **MinGW:** it rejects `-U_WIN32` (`#error Only Win32 target is supported!`), so the off-branch probes use host `gcc -m32 -ffreestanding` or the disposable tree.
- **Gate:** `~/openfs98-verify/gate/`.
  - `build_side.sh` needs `PRIOR_INVENTORY` for `lint-prior.log`.
  - Default mode trusts saved digests, while `--strict` rehashes.
  - `replay.py` deduplicates by (cwd, flags, source), so `verify.py` compares the full `gcc.log`.

## Tasks & Acceptance

**Execution:**
- [ ] `include/fs98/retail_abi.h`: new.
- [ ] `abi.h` and `keep.h`: key on it.
- [ ] `hg2d.c`, `util_math.c` (both blocks), `util.c` and the six `.S` files: include and gate.
- [ ] `docs/method.md` or `docs/architecture.md`: one line on where `FS98_RETAIL_ABI` lives. `deferred-work.md`: mark the `keep.h` entry (line 102) as gated by 1.9a.
- [ ] `~/openfs98-verify/p19a/`: `probe.sh`, `verify.py`, the snapshots, the gate, the break-runner builds and the negative check.
- [ ] `## Implementation Notes`: the probe results, the gate and `verify.py` output, the commands and the gate revision.

**Acceptance Criteria:**
- **Single definition:** given the slice, when `grep -rn 'define FS98_RETAIL_ABI' include src cmake tools` runs, then exactly one line matches, in `retail_abi.h`.
- **Conditions:** given the gated places, when they are read, then each tests `FS98_RETAIL_ABI` and none tests `__i386__`.
- **Probe:** given `probe.sh`, when it runs, then every call exits 0, the off outputs lack every wrapper, and the on outputs have each one.
- **Identity:** given both snapshots, when the gate and `verify.py` run, then both pass, every affected runner case builds, and the negative check fails as specified.

## Review Triage Log

Spec review (Codex gpt-6.2-sol, 2026-11-25), before approval:

| # | Finding | Verdict | Action |
|---|---|---|---|
| 1 | MinGW rejects `-U_WIN32`, so the probe cannot run | blocker | Host `gcc -m32 -ffreestanding` for headers and `.S`; a disposable tree for whole C files; exit 0 required. |
| 2 | UTIL has 30 wrappers in two blocks, plus `util.c`'s selector | high | All three gated together. |
| 3 | Other register families are missing | high | Scope narrowed to the named places; the families are assigned to 1.9e. |
| 4 | The default gate trusts saved digests and the argv check is partial | high | `--strict` rehash, `verify.py` (full argv, units, artifacts), runner builds, and a better negative check. |
| 5 | Include spellings and recipe map | medium | Specified per file; the map corrected. |
| 6 | Probe assertions are not symbol-scoped | medium | Per-symbol on and off checks, slot expansions, every macro form. |
| 7 | `build_side.sh` needs `PRIOR_INVENTORY` | medium | Frozen baseline allowlist. |
| 8 | The predicate is a platform proxy | low | Documented in the header. |

## Verification

**Commands:**
- `bash ~/openfs98-verify/p19a/probe.sh` -- expected: pass.
- The gate and `python3 -B ~/openfs98-verify/p19a/verify.py` -- expected: `GATE PASS`, and the verifier passes.

## Implementation Notes

Changed: new `include/fs98/retail_abi.h`; `abi.h`, `keep.h`, `hg2d.c`, `util_math.c` (both blocks), `util.c`, the six `.S` stubs; one line each in `docs/architecture.md` and `deferred-work.md`. Verification files are in `~/openfs98-verify/p19a/`.

- **Probe:** `probe.sh` passes (every preprocessor call exit 0, outputs in `probe-out/`). The `.S` files on win32-retail compile to objects byte-identical to the baseline's. Whole-file probe: HG2D's `draw_depth_point_entry` and all UTIL `_entry` definitions are present on and absent off, and the off slot table names the C functions. UTIL has 31 wrappers, not 30 (29 in the macro/raw block plus `rand` and `rand32`). `FS98_ENTRY` itself is undefined off (only the `KEEP_*` forms are empty), so `hg2d.c` keeps its `#if`.
- **Snapshots:** `before-src` is `git archive 27a9e49`, `after-src` the working tree, both with `orig/`; built by `run.sh` (`build_side.sh`, root `p19a/root`, serial, nice 19, `-j 2`), `PRIOR_INVENTORY` the baseline's `arena_lint_allowed.txt` (`prior.txt`).
- **Gate:** `compare.py` sha256 b35049b3... at the first run; rerun after 6b's edits with 7ea84a536b6c775bd30f58a87ea31fb77bfafe34c4331656f09964fd20f77077 (`GATE PASS` again). Default mode: `python3 -B compare.py before after` gives `GATE PASS` (1130 artifacts, 321 PEs, 524 replayed units). `--strict --manifest-rows tools/data_images.txt` FAILS on identical sides (185 DIFFs, `gate.out`): it is hard-wired to cl4b's expectations (an added `_image_end` symbol per PE, 13 flag-only recipe edits). It still passes on 4b's kept outputs. So the spec's `--strict` cannot be met as written; `verify.py` rehashes every PE's raw bytes instead.
- **verify.py:** passes. gcc.log (365 invocations; CMake's random-named compiler-probe dirs are collapsed) and gcc.od.log identical; all 9 changed units compiled equally on both sides; 524 replayed objects byte-equal; 1130 bin files, 809 byte-identical and 321 equal after the masks (COFF timestamp, checksum, export timestamp/CSWTCH).
- **Break-runner builds** (after side, frozen copy, no wine, all exit 0 and make a PE): MAIN `--build-only` suite `radiodlg` (118 mutants); HG2D, SIM1 (`sim1_breaks` and `sim1_state_breaks` commands), MDL crash (`build_hand`), HELP (`help_ui_breaks` work-dir layout) and VIEWS (`views_ui_breaks`) by their compile commands on unmodified copies (`rb/rb.py`). ATC, NUMERIC, DYNAMIC, WEATHERM and UTIL have no break runner that compiles their sources; their difftests are built in the gate's `difftests` target.
- **Negative check:** on a `cp -al` copy, `sim1_stack.S`'s object replaced by `sim1_velocity.S`'s: `compare.py` (default) exits 1 `object differs: src/sim1/sim1_stack.S`; `verify.py` exits 1 `replayed object differs: unit src/sim1/sim1_stack.S`.

### Code review triage (Codex, 2026-11-26, `~/openfs98-verify/p19a/code-review.out`)

| # | Finding | Verdict | Action |
|---|---|---|---|
| 1 | No bug in the diff alone | ok | none |
| 2 | `check_abi_headers.py` self-test anchor stale, check skipped | fixed | fixture mutates `retail_abi.h`'s gate, still asserts Linux i386 rejects stdcall; `--self-test` ok |
| 3 | `mpchat_ui_breaks.py` frozen includes lack `retail_abi.h` (and `image_end.h`) | fixed | both added; `--build-only ... sender-color` builds a PE |
| 4 | `build_gauges.py` freshness ignores `retail_abi.h` | fixed | added to the dependencies |
| 5 | Probe names from the candidate; count informational; off check weak | fixed | 31 names and offsets pinned in `probe.sh`; hard count; each `Linkage` initializer and label asserted on and off; `FS98_ENTRY` undefined off checked; a miswired slot makes it fail |
| 6 | `verify.py` drops scratch argv; artifact set not tied to `art.paths` | fixed | only `TryCompile-*` and `cmTC_*` ids normalised; each kept tree must equal its `art.paths`; 29 exact outputs required (`SIM1_AIR_TEST.DLL` included); negative check on that file exits 1 |
| 7 | `pehash_norm` CSWTCH mask runs past the string table | deferred | a gate change; the coordinator handles it after 6b closes |
| 8 | `--strict` criterion open | decided | the user's decision above |
| 9 | Break-runner builds were unmutated and missed three runners | fixed | `rb2/rb2.py` builds one real mutant for SIM1 (`word-eax-high`), SIM1 Air (`query-key`), SIM1 state (`velocity-negation`), MDL crash (`update-edi`), HELP events (`about-string-unwind`), HELP UI (`credits-timer-counter`), VIEWS events (`temporary-unwind`), VIEWS UI (`module-unwind-delete`), HG2D (`descriptor width`) and MPCHAT UI (`sender-color`, `--build-only`); each exits 0 and writes a PE (`rb2/rb2.out`); MAIN as before |
