---
title: 'Story 1.8, slice 1: arena.h, its lint with a shrink-only allowlist, and REALITY and ANALYSIS converted'
type: 'refactor'
created: '2026-11-24'
status: 'in-progress'
route: 'dispatch'
baseline_commit: '1932b40b08c0aa6cfa0d5f91f36170129b658c99'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/native-port/architecture/ARCHITECTURE-SPINE.md', '{project-root}/OpenFS98/docs/implementation/spec-1-7-the-module-abi-headers-are-32-bit.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Modules define their own arena macros (81 definitions) and convert pointers with 478 `(uint32_t)(uintptr_t)` casts, across 67 files of `src/`. All of that works only where the arena's base is 0, so the 64-bit move would have to edit every module (AD-15).

**Approach:** Story 1.8 comes in slices, as Story 1.5 did. This slice adds:
- `include/fs98/arena.h`, the one home of those macros;
- `tools/check_arena.py`, a lint that runs on every build and fails on a banned cast or macro definition in `src/` outside `src/platform/arena/`, with a per-file allowlist of today's sites that may only shrink;
- the conversion of REALITY and ANALYSIS.

Later slices convert the rest (Design Notes), and the last deletes the allowlist.

## Boundaries & Constraints

**Always:**
- **`arena.h`.** It includes only `fs98/abi.h` and `<stdint.h>`. A module opts in directly, after its system and ABI includes, and removes its private copies in the same slice. No shared header includes it during the migration, because unconverted modules' private names would collide under `-Werror`. It holds the only definitions of these:
  - **Access types:** `fs98_v8`, `fs98_v16`, `fs98_v32`, `fs98_vf` and `fs98_vd`, each `volatile`. Like REALITY's and ANALYSIS's byte types today, `fs98_v8` has no `may_alias`; the others carry it.
  - **The base:** `FS98_ARENA_BASE` is the constant `0` on `__i386__`, retail and native `linux-i386` alike. Elsewhere it is declared `extern uintptr_t fs98_arena_base`, not defined yet (Epic 3).
  - **`fs98_ptr(a)`** turns an address into a pointer: `((void *)(FS98_ARENA_BASE + (uintptr_t)(uint32_t)(a)))`.
  - **`fs98_handle(p)`** turns a pointer into an address. On i386 it is exactly `((fs98_addr)(uintptr_t)(p))`, where any 32-bit pointer is in the arena. It accepts struct, array, volatile and function pointers. Off i386 it is not defined yet: the membership check needs Epic 3's arena bounds.
  - **`fs98_call(code, fn_type)`** turns a code value into a pointer: `((fn_type *)(uintptr_t)(code))`. It exists only on `_WIN32 && __i386__`, for a real code value such as a stored callback. It is not used for slot loads.
  - **Memory access:** `M8`, `M16`, `M32`, `MF` and `MD(a)`, each `(*(fs98_vN *)fs98_ptr(a))`.
  - **Record fields:** `R8`, `R16`, `R32`, `RF` and `RDB(p, o)`, each `MN((uint32_t)(p) + (o))`.
  - **Module images:** `D` is `fs98_handle(&FS98_DATA_IMAGE)`, defined only if the module defined `FS98_DATA_IMAGE`; `RDATA` is the same with `FS98_RDATA_IMAGE`; and `DA(va)` is `(D + ((uint32_t)(va) - FS98_DATA_VA))`, defined only with `FS98_DATA_VA`.
  - The short names `R`, `RD`, `P` and `ADDR` are not defined in the shared namespace.
- **The lint, `tools/check_arena.py`.**
  - **Scope:** `src/` only (the user's decision). It scans `.c`, `.h`, `.inc` and `.S` files, and files under `src/platform/arena/` are exempt.
  - **Tokenising:** line splices are joined first. Then a tokenizer skips comments and string and character literals, and maps every hit back to its physical file and line.
  - **What it finds:**
    - each `(uint32_t)(uintptr_t)` cast, with any spacing, several to a line;
    - each `#define` of `M8`, `M16`, `M32`, `MF`, `MD`, `R8`, `R16`, `R32`, `RF`, `RDB`, `DA`, `D`, `RDATA`, `fs98_ptr`, `fs98_handle` or `fs98_call`, `# define` and `#define/**/` forms included, within one logical directive.
  - **Reverse casts:** the reverse `(uintptr_t)(uint32_t)` form isn't banned, since it isn't AD-15's rule. It is counted, though, in a separate inventory so no conversion is stranded: 60 today, reported by `--inventory`.
  - **The allowlist, `tools/arena_lint_allowed.txt`:**
    - It holds one row per file: normalised path, casts, defines. Counts are non-negative. A `(0, 0)` row and a stale row are errors, though one zero, such as `(0, 1)` or `(4, 0)`, is valid. The rows are seeded by the scanner itself.
    - A count above its row fails, and so does a hit in an unlisted file. A count below its row also fails, saying to lower the row.
    - When `git show HEAD:tools/arena_lint_allowed.txt` exists, any row higher than HEAD's, or any row not in HEAD, also fails. In a tree with no git, or with no allowlist in HEAD, the lint says it checked only the working tree.
    - `--prior FILE` compares with a given earlier allowlist instead. The gate passes the before snapshot's inventory for this first seed, and the previous accepted list in later slices, so a raise can't hide in a commit.
    - Its stated limit: swapping one banned site for another in the same file keeps the count, so that's caught only in review.
    - The last slice deletes the file, and from then on the lint refuses it.
  - **Running it:**
    - an always-run target, named in the `DEPENDS` of every output-producing `add_custom_command` (`cmake/OpenFS98.cmake:67`), so no module recipe can start before it passes, and a violation fails `cmake --build` even on an up-to-date tree (AD-15's CMake check);
    - a Baselines line in `docs/tools.md`;
    - `--self-test`, which runs every matrix row through the CLI's path with a temporary root.
- **REALITY and ANALYSIS.** Each:
  - drops its private access typedefs (`reality.c:36-39`; `analysis.c:51-54`) and `M8`…`MD`;
  - defines `FS98_DATA_IMAGE` and `FS98_RDATA_IMAGE` as the whole objects (`reality_image` and `reality_rdata`; `analysis_image` and `analysis_rdata`, a `const double[9]`). The fields `D` and `R` point at today, `jump`, `feet` and `state`, all sit at offset 0;
  - replaces `R` with `RDATA`, REALITY's four scale-table reads (`:767`) included;
  - renames `RD(o)` to a module-local name defined as `MD(RDATA + (o))`;
  - replaces `ADDR(p)` with `fs98_handle(p)`;
  - keeps its module-local `CALL` slot load (`reality.c:153`, `analysis.c:108`) exactly as it is, since it's neither a banned name nor a banned cast.

  Their allowlist rows are deleted, which leaves 472 casts and 71 definitions across 65 files.
- **The break runners.** The renames break exact-match strings in `tools/reality_breaks.py` (11 cases) and `tools/analysis_breaks.py` (5). Those strings are updated, each mutation kept, and each must match exactly once.
  - Each runner gains a build-only path (`--build-only DIR`). It builds every case's linked mutant DLL, one case at a time, and keeps it under `DIR/<case>` with its case metadata, so no case overwrites another.
    - Both snapshots run it, and all pairs (67 REALITY and 103 ANALYSIS, the 11 and 5 changed ones included) must be normalised-identical. The harness and the retail DLLs each mutant loads are covered by the main gate.
    - Identical mutants with unchanged metadata prove each verdict unchanged without a wine run.
    - A differing mutant needs that case's break verdict compared in wine at `N=20000`, under the fallback's scheduling rules.
- **The gate:**
  - **Packaging:** Story 1.7's gate becomes a parameterised tool in `~/openfs98-verify/gate/`: the build driver, the compiler wrapper, the replayer, the on-demand relinker, the PE hasher and the object comparison.
    - It takes snapshot and output paths, not `p17` or scratchpad paths.
    - It compares the two frozen replay manifests unit by unit, with their flags.
    - Every failure exits non-zero: a missing artifact, a compile or relink failure, a hash `ERR`.
  - **What it keeps from Story 1.7:** the on-demand coverage, the named exclusions, the PE normalisation, and the pinned per-flight comparison.
  - **This slice:** build at `~/openfs98-verify/p18`, before then after.
  - **The acceptance rule (Story 1.7's precedent):** byte identity stands for "every module's difftest and all 74 flights still pass".
    - A difference left after trying needs `realitydiff-run` and `analysisdiff-run` at 20,000 scenarios with 0 differ, and the 74 flights as Story 1.7 states.
    - Those runs are serial, from a frozen copy, and never while Story 1.6's recording or the 1.4 overnight targets run.
- Builds run under `nice -n 19`, with `-j 2` and `CMAKE_BUILD_PARALLEL_LEVEL=2`. No staging or commits, no reverting sources to take a baseline, and no changing this spec's status.

**Never:**
- No other module is converted.
- The Story 1.7 bridge stays (the user's decision): its removal is its own story. Only the stale "Story 1.8 removes it" comments in `abi.h:31` and `host.h:38` are reworded to point at that story.
- No `tools/` change except the lint, its allowlist, and the two break runners' strings and `--build-only` path. No register-contract change (Story 1.9).

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|----------|--------------|---------------------------|----------------|
| Clean | counts equal their rows, nothing higher than HEAD | `arena lint: N files allowlisted, C casts, M defines left (reverse casts: K)`, exit 0 | — |
| New cast | a banned cast in a listed or unlisted file, also in `.S`, or several on one line | each site named by file and line | exit 1; the build fails, even on an up-to-date tree |
| New definition | `#define M32(a) …`, `# define D x`, `#define/**/D x`, split by a backslash | named | exit 1 |
| Not a site | the same inside a comment or a string literal | ignored | exit 0 |
| Shrunk, not lowered | a file below its row | says to lower it | exit 1 |
| Raised | a row higher than HEAD's, or new | names it | exit 1 |
| Bad row | a `(0, 0)` row, a stale path, a duplicate or a negative count; one-zero rows such as `(0, 1)` and `(4, 0)` pass | names it | exit 1 |
| Exempt | anything in `src/platform/arena/` | ignored | exit 0 |
| Reverse cast | `(uintptr_t)(uint32_t)` | counted in the inventory, not failed | exit 0 |

</frozen-after-approval>

## Code Map

- **REALITY:** the typedefs at `src/reality/reality.c:36-39` (`re_v8` without `may_alias`); `D`, `R`, `M8`…`MD` and `ADDR` at `:123-129`; `CALL` at `:153`; `RD` at `:229`; volatile arguments at `:724`; scale reads at `:767`.
- **ANALYSIS:** the typedefs at `src/analysis/analysis.c:51-54`; the macros at `:81-87`; `CALL` at `:108`; `RD` at `:189`.
- **PANELS** (`src/panels/panels.h:13-47`) has the full family. Its record float macro is `RFL` and its one-argument `.rdata` float is `RF`; the PANELS slice renames the latter and moves `RFL` to the shared `RF`.
- **Byte-pointer `D`, for later slices:** `sim1.c:51`, `booster.c:70`, `acontain.c:171`, `atc.c:110`, `facil.c:242`, `weatherm.c:113`, `landme.c:56`, `sound.c:413` and `advdrv.c:153`. ADVDRV's `DA` (`:159`) also returns a pointer, unlike the shared `DA`. `ADDR(&image)`: `fe.c:217` and `dynamic.c:96`. SIM1's unrelated local `RD` and `RF` (`sim1_small.c:4852-4853`) are renamed in its slice.
- **Break runners:** `tools/reality_breaks.py:37` and `tools/analysis_breaks.py:134`.
- **Build:** `ofs_step` (`cmake/OpenFS98.cmake:21`) only reruns on stale outputs, so the lint needs an always-run target ahead of the default target's steps (`CMakeLists.txt:20`).
- **Gate sources:** `/tmp/claude-1000/-home-steve-source/b2e3078b-1d77-426e-816f-2de2772a67dc/scratchpad/pre17/`, where `build_side.sh` hardcodes `p17` and `wrap/`, and `objcmp.py` uses the live tree. The usage is in `spec-1-7`'s Implementation Notes.
- **Baselines:** `realitydiff-run` and `analysisdiff-run` in `docs/tools.md:298` and nearby.

## Tasks & Acceptance

**Execution:**
- [ ] Package the gate in `~/openfs98-verify/gate/`. Build the before snapshot at `p18`, recording hashes, the replay manifest and objects, and the break mutations' objects.
- [ ] `include/fs98/arena.h` (new).
- [ ] `tools/check_arena.py` (new) with `--self-test` and `--inventory`, and `tools/arena_lint_allowed.txt` seeded by it.
- [ ] `cmake/`: the always-run lint target ahead of the default build.
- [ ] `src/reality/reality.c` and `src/analysis/analysis.c`: convert, and delete their rows.
- [ ] `tools/reality_breaks.py` and `tools/analysis_breaks.py`: update the strings, and add `--build-only DIR`.
- [ ] Docs:
  - `docs/tools.md`: the Baselines line;
  - `docs/architecture.md`: an `arena.h` paragraph;
  - `docs/implementation/deferred-work.md`: the bridge-removal story and the lint's scope are already logged (2026-11-24), so nothing to add unless the slice finds more;
  - the comments at `abi.h:31` and `host.h:38`.
- [ ] Build the after snapshot, then run the gate and the mutation-object comparison.
- [ ] This spec: `## Implementation Notes` with the seed counts, the inventory, the gate result and any difference left.

**Acceptance Criteria:**
- Given the slice, when `python3 -B tools/check_arena.py` and `--self-test` run, then both pass, the summary shows 472 casts and 71 definitions across 65 files, and REALITY and ANALYSIS have no row.
- Given an up-to-date build and a new banned cast in a new `.S` file, when `cmake --build` runs, then it fails, naming the site. Given a failing lint and a stale module output, when `cmake --build -j 2` runs, then that module's recipe never starts. Remove the file after each test.
- Given the gate, then the artifact sets match, and every PE, replayed object and break-mutation object is identical. Otherwise any explained difference has its difftests and the flights passing.

## Design Notes

Later slices, each its own spec and one shrink of the allowlist:
1. CONTROLS and PANELS (with GAUGES; PANELS' `RF`/`RFL` renames).
2. WINDOW, WINMSG and MPCHAT.
3. HOST.
4. DEMO and LANDME.
5. MAIN and the English dialogs: OPTIONS, FLIGHTS, SIMSUI, MPDIALOG, VIEWS and WORLD.
6. G3D, HG2D and MDL.
7. SIM1, FE, FACIL and DYNAMIC (the byte-pointer and `ADDR` shapes; SIM1's `RF`/`RD`).
8. The rest: ADVDRV, SOUND, WEATHERM, BOOSTER, ATC, ACONTAIN, CHAIN, UTIL, MULTIP, GLOBAL and `src/test`. This slice deletes the allowlist and makes the lint refuse it.

Each slice also lowers the reverse-cast inventory for its modules. Byte identity, slice by slice, is the evidence for the story's difftest and flight criterion, as in Story 1.7.

## Verification

**Commands:**
- `python3 -B tools/check_arena.py --self-test && python3 -B tools/check_arena.py` -- expected: both pass, with the counts above.
- `nice -n 19 cmake --build build/win32-retail -j 2` on an up-to-date tree -- expected: the lint runs and passes.
- the gate at `~/openfs98-verify/p18` -- expected: same artifact sets, empty diffs, identical mutation objects.

## Review Triage Log

Spec review 1 (Codex gpt-6.2-sol), 12 findings, all applied:
1. high: `CALL` is a slot load, so `fs98_call` would call the slot's address. `CALL` stays module-local, and `fs98_call` is for real code values.
2. high: `fs98_handle`'s i386 expansion is pinned (volatile and function pointers too). The images are whole objects, `R` becomes `RDATA`, and the byte types keep no `may_alias`.
3. high: the break runners' strings are updated, with mutation-object identity in place of wine runs.
4. high: the lint is an always-run target, verified on an up-to-date tree.
5. high: the gate is packaged and parameterised, and it fails on every error.
6. medium: an opt-in inclusion rule; `D`, `RDATA` and `DA` are guarded; the PANELS and SIM1 renames are assigned.
7. medium: the base is 0 on all i386, `fs98_call` is retail-only, and off-i386 `fs98_handle` is deferred.
8. medium: reverse casts are inventoried, not banned; `.S` and several sites per line are covered.
9. medium: a tokenizer that skips comments and literals and maps hits to lines.
10. medium: allowlist rules (HEAD comparison, row validity, the stated swap limit), with corrected counts: 478 casts, 81 definitions, 67 files, leaving 472, 71 and 65.
11. medium: byte identity is accepted as the evidence, per Story 1.7; the fallback is named.
12. low: Code Map anchors; WEATHERM and LANDME added; the stale bridge comments reworded.

Spec review 2 (Codex gpt-6.2-sol), 6 findings, all applied:
1. high: a HEAD comparison can't catch a raise that is already committed, and the seed has no HEAD list. Added `--prior FILE`, fed by the gate (the before inventory now, the last accepted list later); a no-git tree is reported as such.
2. high: the replayer collapsed the mutation cases, which share a temporary file. The runners get `--build-only`, keeping each case's mutant DLL apart: 67 and 103 pairs.
3. high: the proof compares the linked mutant DLLs and their metadata, not just objects. A differing mutant gets a wine verdict comparison at N=20000.
4. medium: the lint target sits in every output recipe's `DEPENDS`, tested with a stale output under `-j 2`.
5. medium: only `(0, 0)` rows are invalid.
6. low: SOUND and ADVDRV byte-pointer `D` and ADVDRV's pointer `DA` added; the SIM1 citation is `:4852-4853`.
