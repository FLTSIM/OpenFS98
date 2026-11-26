---
title: 'Story 1.8, slice 6b0: the FLIGHTS, MPDIALOG and WORLD break runners share --build-only'
type: 'refactor'
created: '2026-11-25'
status: 'done'
route: 'dispatch'
baseline_commit: '0f8e49e12f345913116964b6aead145e6b8c520d'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-1-8f-main-uses-arena-h.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Slice 6b (FLIGHTS, MPDIALOG and WORLD convert to `arena.h`) has to show that every break mutant of those modules is unchanged.
- 27 runners compile the files it changes, and none can build its mutants without wine.
- Most are hand-copied loops with no shared code.
- 14 of the WORLD runners cannot build at all: `world_ui`, `world_priority`'s ui suite and 12 `world_*_native` runners hard-code 38 units, while `src/world/sources.txt` lists 45. Their baseline link fails with 58 undefined references.

**Approach:**
- Give these runners one shared case-and-build helper and a `--build-only DIR` mode with 6a's contract.
- Repair the 14 by reading `sources.txt`, as time, advanced and wareas already do.
- Prove nothing else changed:
  - by tracing every compile and wine call, old against new;
  - by every wine suite reporting its documented counts.

No module source changes in this slice.

**Decided (2026-11-25):**
- The user chose to update the runners rather than bolt `--build-only` onto each.
- The 14 broken WORLD runners are repaired here.
- 6b then converts all three modules with a whole-build proof of every case.
- Out of scope: OPTIONS and SIMSUI (6c, 6d), and the 12 runners that already have `--build-only`.

## Boundaries & Constraints

**Always:**
- **Runners (27):**
  - `tools/flights_breaks.py`;
  - `tools/mpdialog_ui_breaks.py` and its six wrappers `mpdialog_{common,construction,create_failure,exception,retained,review}_breaks.py`;
  - `tools/world_{search,search_init,ui,priority,wareas}_breaks.py`;
  - the 14 `tools/world_*_native_breaks.py`, giving 20 WORLD modes counting wareas `--native`.
- **The helper:** a third entry point in `tools/break_build_only.py`.
  - A case is an id plus mutated file texts overlaid on a copy of the inputs, and its compile argvs with named outputs.
  - Each runner builds its cases with the code it has today, moved but not rewritten. Post-edits, replace-all, `before in data`, expected count 2 and MPDIALOG's cumulative `flow` rebinding all keep their effect.
  - The verdict predicates (exit code, markers, unload/fault checks, abort-or-continue) move verbatim.
  - Shared code goes only where runners are near-copies: the WORLD natives, and the MPDIALOG wrappers' `compare`/`build`.
- **Wine mode keeps:** its CLI, run order, wine arguments, timeouts, markers, output paths and logs, and every compile's argv and input bytes. The one exception is the 14 repaired runners' DLL unit list, which becomes `sources.txt` order.
- **`--build-only DIR [CASE...]`**, the same in every runner:
  - **Case ids:** `CASE` is a case id: `<suite>.<name>` where a runner has suites, otherwise `<name>`. Baseline builds are cases named `baseline`.
  - **Defaults:** no `CASE` means every case, and wareas takes `--native` as in wine mode.
  - **Where it builds:** in a temporary directory outside the tree, with `TMPDIR` checked to be outside the tree and `MPD_PROVENANCE_DIR` ignored. It runs no wine, and prebuilt `bin/` inputs are only read.
  - **What it writes:** `DIR/<id>/` holds every PE the case builds, under its built basename, plus `meta.json` with `name` = `<id>`.
  - **Exit 0:** every case built.
  - **Exit 1**, checked before any output is made: an unknown or duplicate `CASE`, or a primary anchor whose count differs from what the runner requires.
  - **Exit 1 after building starts:** a build that fails or exits 0 without its PE. Cases already built stay.
  - **Exit 2:** DIR is empty, is a file, is a non-empty directory, or resolves inside the tree.
  - **Signals:** SIGTERM and SIGHUP kill the compiler's whole process group, remove the temporary copy and exit non-zero.
  - **Nothing at import:** no runner writes anything when imported; directory creation moves into `main`.
- **The proof** lives in `~/openfs98-verify/p18g0`; everything there runs under `nice -n 19`.
  - **Snapshots:** before is `git archive` of the base commit, after is the worktree. Each gets `orig/` and its prerequisites built by the CMake `openfs98` and `difftests` steps, outside the trace.
  - **Wrappers** in `p18g0/trace/` are put first on `PATH`:
    - `i686-w64-mingw32-gcc` logs cwd and argv, the sha256 of every input file and every dependency `gcc -M` resolves with the same arguments (not `-o`), and the sha256 of the output after `tools/norm_pe.py`, then returns the real compiler's status;
    - `wine` logs its argv and cwd, then execs the real wine.
  - **Normalisation:** paths are mapped snapshot-root → `ROOT` and temporary or case directory → `WORK`.
  - **Runs:** every runner's documented wine suite on both sides (FLIGHTS at `400`), serially from frozen copies, with each runner's per-case result lines kept. One `flock` on `~/openfs98-verify/wine.lock` covers every wine run of this slice.
  - **Required results:**
    - **(a) Counts:** per runner, a build manifest (cases built) and a comparison manifest (wine checks, including baselines). The after side's counts match the docs: `docs/modules/flights.md:225` 502/502; `docs/modules/mpdialog.md:48-54` 46, 5, 5, 6, 8, 7, 12; `docs/modules/world.md` per runner, recorded in the notes. The before side matches the after side except for the 14 repaired runners, whose before log must show the baseline link failure.
    - **(b) Traces and verdicts:** for every runner except the 14, the before and after traces are identical after normalisation, and so are the per-case result lines. For the 14, the after trace differs from the before only in the unit list.
    - **(c) Build-only against wine:** each after runner's `--build-only` trace equals its wine-mode compile trace after normalisation, with the same output hashes, and its case count equals its build manifest.
  - **A repaired runner that misses a break** stops the slice. It is logged and nothing is changed to make it pass.
- **Running:** the work happens in a git worktree at `~/source/OpenFS98-6b0` off HEAD. Slice 6a still verifies from the main working tree, which 6b0 must not touch. Builds may run alongside 6a's. MPDIALOG's `wineserver -k` acts only inside its isolated prefix.
- **`deferred-work.md`:** the 14 runners' stale unit list (pre-existing, repaired here), and MPDIALOG's cumulative `flow` rebinding (kept).

**Never:**
- No change to any mutation, anchor, count, predicate, timeout or wine argument, and no change under `src/`.
- No change to the other 12 `--build-only` runners or to `compare.py`.
- No commits or staging.


**Decision (user, 2026-11-25, at the proof):** check (c) is relaxed for DLLs. The 17 WORLD runners' mutant DLLs differ by about 255 bytes of import-directory order between `--build-only` (an absolute temp directory outside the tree, as required above) and wine mode (relative paths under the tree). This is a path-form effect, reproduced at `bin/zz/a` against `/tmp/zzdir/a`. For DLLs, (c) now requires equal argv, inputs and dependencies, plus equal hashes between build-only runs. EXEs, FLIGHTS and MPDIALOG keep exact hash equality.

</frozen-after-approval>

## Code Map

- **`tools/break_build_only.py`:** `run` and `run_compile`. The `Popen` kill at about `:80` kills only the parent, so it needs a process group. 6a's `main_breaks.py` `build_only()` (about `:2348`) is the contract to copy.
- **`tools/flights_breaks.py`:**
  - 10 lists of `(name, before, after)`, with the file implied by the list in `main()`, plus UNWIND `(name, unit, before, after)`: 502 cases.
  - One gcc call each: `flightseventsdiff.c`, the mutant `breaks/{name}.c` and the other 9 units, with `-Isrc/flights`. Headers are resolved beside the source.
  - A failure continues and counts as a miss. It is invoked by `tools/run_flightsdiff.sh:13`.
- **MPDIALOG:**
  - **`mpdialog_ui_breaks.py`:** `build`/`build_flow` (about `:107`/`:143`), `check_mutant` (about `:184`), `isolated_wine_prefix` and `run_wine`.
    - 45 builds and 46 comparisons.
    - Its cases are code-driven: slice edits, multi-edit mutants, one build with several wine arguments, `flow` rebinding at about `:545`/`:556`.
    - Mutations reach `mpdialog_unwind.h` and `mpdialog_runtime_tables.inc`, which no gcc argv names, hence `gcc -M`.
  - **The wrappers** import `ui`, `review.compare` and `construction.replace_once`. The exception runner has 10 builds and 12 comparisons; the wrappers' 16 baselines use the prebuilt DLL and harness.
- **WORLD:**
  - **`world_ui_breaks.py`** (11 builds, including a baseline) and the natives snapshot `src/world` plus `bin/world-diff/{world-res.o,libworld-simsui.a}`. They build the DLL and a harness, then run `wine harness dll N`.
  - **The hard-coded 38-unit tuple** (for example `world_ui_breaks.py:51`, `world_priority_breaks.py:64`, `world_booster_native_breaks.py:68`) is the repair site.
  - **Quirks:** dynamic and search pick N per case; advanced has a 5th column, a count dict and a copied EXE; booster and priority have suites; time, weather, copy and wareas copy a baseline DLL.
  - **`world_search`/`search_init`** are one gcc call each and check stderr only; they currently create their output directory at import.
- **Cost:** about 1,950 wine calls and 3,500 gcc calls. FLIGHTS takes about 4.8 s per case, so about 80 min for its two passes. Pilot one runner first.

## Tasks & Acceptance

**Execution:**
- [ ] `~/source/OpenFS98-6b0` -- `git worktree add` off HEAD -- isolation from 6a.
- [ ] `~/openfs98-verify/p18g0/` -- before snapshot, `trace/` wrappers, the per-side run script and `check.sh` for (a) to (c) -- written and its before run done before the refactor is judged.
- [ ] `tools/break_build_only.py` -- the case entry point with process-group kill -- shared build-only.
- [ ] `tools/flights_breaks.py`, `tools/world_search_breaks.py`, `tools/world_search_init_breaks.py` -- onto the helper.
- [ ] The 14 WORLD natives plus `world_ui`, `world_priority`, `world_wareas` -- onto one shared native module reading `sources.txt`.
- [ ] `tools/mpdialog_ui_breaks.py` and the six wrappers -- onto the helper.
- [ ] Docs: `docs/modules/{flights,mpdialog,world}.md` get `--build-only` usage and the repair; `docs/remaining-work.md:177` is updated; `deferred-work.md` gets two entries.
- [ ] After snapshot, the runs and `check.sh`; this spec's `Code review (Codex gpt-6.2-sol, three layers, 2026-11-25):

| # | Finding | Verdict | Action |
|---|---|---|---|
| 1 | check.py did not enforce the DLL decision; one build-only pass | blocker | A second pass; DLL pass-to-pass equality; exact EXE, FLIGHTS and MPDIALOG equality; PASS/FAIL. |
| 2 | A signal after Popen leaves the compiler running (reproduced) | high | Signals blocked across registration; reaped after unwinding. |
| 3 | .pyc and TMPDIR probes write into the tree | high | Bytecode off before imports; paths validated first. |
| 4 | Retail-side ownership guard counted as caught | high | Deferred (frozen predicates); entry added. |
| 5 | Proof evidence incomplete (WORLD counts, verdict prefixes, gcc -M) | medium | Asserted in check.py. |
| 6 | Destination validated after loading inputs | medium | Validated first. |
| 7 | CRLF anchor regression in advanced weather (reproduced) | medium | Universal newlines restored. |
| 8 | Anchor counts unchecked; secondary zero-matches | medium | Exact counts enforced. |
| 9 | Compiler TMPDIR and cwd; partial cases; missing compiler; PE signatures | medium | Fixed. |
| 10 | Wine lock not held across the proof | medium | The earlier overlap accepted with evidence; any rerun holds an outer lock. |
| 11 | cmake_verify verdicts, shared displays, filters, SUITE, mp_ui anchors | medium/low | Deferred entries. |

## Implementation Notes` gets the per-runner manifests, counts, trace hashes and run times.

**Acceptance Criteria:**
- Given both snapshots' wine runs and traces, when `check.sh` runs, then (a), (b) and (c) hold for all 27 runners.
- Given `--build-only DIR` on any after runner, then it exits 0 with one directory per case and a hash inventory of the tree is unchanged.
  - Given an unknown or duplicate case, it exits 1; given a DIR that is empty, a file, non-empty or inside the tree, it exits 2. In both cases it makes nothing.
  - Given a SIGTERM or SIGHUP mid-build, no compiler process is left and it exits non-zero.
- Given the worktree, `git diff --stat` touches only `tools/*.py` and docs.

## Verification

**Commands:**
- `bash ~/openfs98-verify/p18g0/check.sh` -- expected: `P18G0 PASS`, showing (a), (b) and (c) per runner.
- `python3 -B tools/check_arena.py` -- expected: unchanged.

## Review Triage Log

Spec review (Codex gpt-6.2-sol, 2026-11-25), before approval:

| # | Finding | Verdict | Evidence / action |
|---|---|---|---|
| 1 | 14 WORLD runners link a stale 38-unit list and cannot build | blocker | Reproduced: 58 undefined references. The user chose to repair them here. |
| 2 | A header or `.inc` mutation is invisible to an argv-only trace | high | The trace adds `gcc -M` dependencies and normalised output hashes. |
| 3 | A changed predicate passes on clean runs | high (part) | Per-case result lines and wine argv are compared, and predicates move verbatim. Failure probes rejected: review covers verbatim moves. |
| 4 | Linker inputs, `-l` and the toolchain are outside the trace | high | Normalised output PE hashes cover them. `-o` is excluded from inputs. |
| 5 | Builds and comparisons need separate denominators | high | Separate build and comparison manifests. |
| 6 | Scope is per mode, not per script | medium | 20 WORLD modes named. |
| 7 | Doc claims: wareas 20 native is documented, FLIGHTS N | medium | The wareas defect is dropped; FLIGHTS pinned at 400. |
| 8 | Prebuild, artifacts, selectors, failure and DIR edge cases | medium | Specified. |
| 9 | Kill only the parent, `TMPDIR`, provenance, import-time writes | medium | Process group; `TMPDIR` check; provenance ignored; nothing at import. |
| 10 | Cost and a cross-snapshot wine lock | medium | One `flock`; cost noted; pilot first. |

## Implementation Notes

Code in worktree `~/source/OpenFS98-6b0` (tools and docs only). Proof in `~/openfs98-verify/p18g0` (`check.sh`, output `check.out`; before = `git archive 0f8e49e`, after = the worktree; `cd/`, `game/` and `orig/` copied into both; traces by `trace/`).

**Result:** `bash ~/openfs98-verify/p18g0/check.sh` (output `check2.out`) ends `P18G0 PASS`: (a) counts, (b) traces and verdict lines for all 28 modes, and (c) build-only against wine mode as decided below. Two `--build-only` passes of all 28 modes (1,910 case directories, 2,342 PEs in the two passes together) are equal pass to pass for every DLL, and every PE equals the pre-review build-only run (1,171 PEs, so the review patches changed no mutant; no wine rerun was needed).

**(a) Counts (after):** flights 502/502; mp_ui 46 (45 builds, 48 wine calls), review 5, construction 5, common 6, retained 8, create_failure 7, exception 12 (10 builds); WORLD: search 22, search_init 24, ui 10 + baseline, priority 22, wareas 53, wareas --native 20, advanced 23, airport 14, booster 18, cache 14, complexity 13, copy 14, dynamic 16, facilities 16, files 11, list 13, position 12, search_native 13, time 23, weather 15. Before: identical except the 14 repaired runners (HEAD: `world_ui` and the 12 natives fail at the first link with undefined references; `priority` and `booster` run their non-UI suites, 17 and 12 cases, then fail at the first `ui` link).

**(b):** 14 non-repaired modes: compile traces, wine traces and result lines identical after normalisation. 14 repaired: the failing link differs from after's only in the unit list (38 before, 45 after, `sources.txt` order), earlier compiles identical.

**(c):** build-only case counts equal the wine-mode build manifests (flights 502, mp 45/6/5/7/10/8/5, WORLD as above); argv, input hashes and `gcc -M` dependency hashes equal in every runner; output hashes equal in all FLIGHTS, MPDIALOG, search, search_init and wareas (non-native) cases and in every harness EXE.

**Deviation (decided: relax (c) for DLLs):** the DLLs of `world_ui`, `priority`, `wareas --native`, `advanced` and the 12 natives hash differently between build-only and wine mode. Same inputs and argv; only the form of the source paths differs (wine mode: relative under the tree; build-only: absolute in a scratch directory outside it, as the spec requires). Rebuilding one case at `bin/zz/a` or a sibling relative directory reproduces the wine-mode hash; at `/tmp/zzdir/a` or `../../../tmp/zzdir/a` it gives the build-only hash. The difference is the import-directory order (about 255 bytes); a rebuild at the same path is deterministic. check.py therefore requires equal argv, inputs and dependencies to wine mode, and equal DLL hashes between the two build-only passes; EXEs, FLIGHTS and MPDIALOG keep exact equality with wine mode.

**Environment findings (not runner defects):** the UI harnesses hang without a window manager (`w_ui` baseline idle 416 s on a drawn dialog, 11 s with `metacity`), and `mp_ui`'s baseline hangs near flow case 203 on Xvfb with or without a window manager (never finished in 900 s) but passes in 21 s on the desktop display `:0`; the final `mp_ui` runs used `:0`. The first run of every MPDIALOG and most WORLD runners also lacked `cd/`. No runner or timeout was changed.

**Run times (s, before / after):** flights 4475 / 3778; mp_ui 249/250, common 93/93, construction 80/79, create_failure 76/76, exception 165/168, retained 128/129, review 91/92; search 52/52, search_init 73/75, ui 17(fail)/269, priority 77(fail)/270, wareas 164/176, wareas_native 1045/1055, advanced 634/652, airport 16(fail)/783, booster 61(fail)/200, cache 17(fail)/1101, complexity 18(fail)/506, copy 17(fail)/649, dynamic 18(fail)/386, facilities 18(fail)/416, files 18(fail)/860, list 17(fail)/344, position 18(fail)/432, search_native 17(fail)/378, time 631/664, weather 16(fail)/389.

**Slice 1.10a overlap:** a harness of slice 1.10a ran under wine outside the lock during the FLIGHTS passes (about 09:30 to 10:30); both FLIGHTS logs show 502/502 caught and no TIMEOUT, "not counted" or BUILD FAILED line.

**After the code review:** `break_build_only.py` blocks SIGTERM and SIGHUP across the compiler launch (tested at 0 to 3 s, TERM and HUP: exit 143 and 129, no compiler, partial directory or scratch directory left); gives the compiler `TMPDIR` and the tree root as cwd; stages each case beside DIR and renames it into place; validates PE signatures; treats a missing compiler as a case failure; validates DIR and `TMPDIR` before any probe or input load. Every runner sets `sys.dont_write_bytecode` first. Anchors have exact counts, follow-up replacements included (`core.position-format` is anchored on the whole call, byte-identical mutant). `mpdialog_ui_breaks.py` restores `RECORDING` in a `finally`; the inherited `flow` mutations in `flow-browser-refresh-reentry-guard` are pre-existing in HEAD (deferred-work). `world_native.py` reads `sources.txt` once per run. The earlier unlocked FLIGHTS overlap is accepted (502/502 and no TIMEOUT or not-counted line); `mp_ui`'s final runs used the real display `:0`.
