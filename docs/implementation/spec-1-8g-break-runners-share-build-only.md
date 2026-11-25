---
title: 'Story 1.8, slice 6b0: the FLIGHTS, MPDIALOG and WORLD break runners share --build-only'
type: 'refactor'
created: '2026-11-25'
status: 'in-progress'
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
- [ ] After snapshot, the runs and `check.sh`; this spec's `## Implementation Notes` gets the per-runner manifests, counts, trace hashes and run times.

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
