---
title: 'cmake_verify.py -j N: Baselines lines in parallel, each in its own tree'
type: 'feature'
created: '2026-11-21'
status: 'done'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43' # worktree held uncommitted Stories 1.2-1.4; file snapshot at ~/openfs98-verify/baseline-verify-parallel
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** `tools/cmake_verify.py` runs the 160 lines of `docs/tools.md`'s Baselines block one at a time, in the tree it was started in. Story 1.3's run (p13) took about 44 h on a 12-core machine that sat at a load of about 1.8. Five lines hit their timeout.

Worse, a timeout `killpg(…, 9)`s the line. `sim1r-breaks` rewrites `src/sim1/sim1_small.c` in place and restores it only in `finally`, so the kill left its `>=` mutant in p13. Every later SIM1 line ran against the mutant, and nothing noticed.

**Approach:** every run uses slots: disposable copies of the tree it was started in. `-j N` (default 1) runs up to N lines at once, one per slot. `--in-place` keeps today's behaviour: the start tree, serial, no slots. Lines are still recorded as `NNN.json`/`NNN.log`. `report.md`, `--only`, `--lines`, `--factor`, resume and the verdict rules are unchanged. A new `--baselines FILE` (default `docs/tools.md`) lets tests feed it their own lines.

- **A slot** is `<slots>/K/`, under `<results>/slots`. The results directory must lie outside the resolved start tree, or the run refuses to start. The run may create, delete and overwrite only paths under `<results>/slots`.
- **Each slot has a template** at `<slots>/K.template/`, prepared once at the slot's own path and then copied aside, so the absolute paths that builds embed (MDL manifests, `fixtures.inc`, harness executables) name the slot path. Preparing a slot:
  1. Copy the start tree's sources: every top-level entry except `bin`, `build`, `game` and `cd`, including `.gitignore`, `.git` and `orig`. `orig` is copied because `sim1-reference` may create `orig/Sim/SIM1.sim`. A `.git` that is a file, or points outside the start tree, stops setup. Symlink `cd`, which no Baselines line writes.
  2. Copy the start tree's `game` as it is. It is prepared state the Baselines rely on: the rebuilt gauges PANELS loads (`docs/tools.md:57`, `panelsdiff.c:679`), `FLTSIM98.CFG`, and SEHFIX for retail HG2D (`docs/wine.md:10,38`). A serial run in the start tree would see the same game.
  3. Copy `~/.wine` to `<slot>/wineprefix`, then make it self-contained:
     - `drive_c/FS98` points to `<slot>/game`, because OFSTEST needs that `C:` mapping (`docs/building.md:103`, `src/test/ofstest.c:387`);
     - `dosdevices/c:` points to `../drive_c`;
     - every other symlink in the prefix that resolves into `~/.wine` is rewritten to the same place in the slot's prefix;
     - links to `/` (`z:`) and to devices stay.

     Setup fails if any symlink still resolves into `~/.wine` or into the start tree.
  4. Configure `<slot>/build/win32-retail` (`cmake -S <slot> -B <slot>/build/win32-retail -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/win32-retail.cmake`) and build `--target openfs98 difftests` from scratch into an empty `<slot>/bin`.
  5. Fill in the fixtures no build makes by copying the start tree's `bin` without overwriting (`rsync -a --ignore-existing`). Everything the fresh build produced keeps the slot's own paths. Everything it didn't (`sim1-diff/snapshots.bin`, `difftest/replay.bin`, `pressings/reference.json`, `FRAMELOCK.DLL`, `SEHFIX.DLL`, logs) comes from the start tree, as a serial run would see it. Setup lists, in `<slots>/K.setup.log`, the copied text files that name the start tree's path; it does not delete them. A short commented list of fixtures known to be required (the five above) must be present afterwards, or setup stops.
  6. Check the sources against the start tree's by content, ignoring `__pycache__`. The build may legitimately rewrite a source (`gen_simsui_tables.py` writes `src/simsui/simsui_tables.inc`), but a rewrite with different content stops setup.
  7. Copy `<slots>/K` to `<slots>/K.template` with `cp -a`, never hardlinks. Write a manifest to both: the sha256 of the start tree's source directories, of the fixture files, of `cmake_verify.py`, and the canonical slot path. Then write `READY`.

  Slots are prepared in parallel. Any setup failure stops the run before a line starts.
- **The environment for every line in a slot:**
  - `cwd=<slot>`;
  - `plan()`'s `cmake --build` pointed at `<slot>/build/win32-retail`, per job, with no shared global changed, and `OFS_BUILD` the same;
  - `PYTHONDONTWRITEBYTECODE=1`;
  - `WINEPREFIX=<slot>/wineprefix`;
  - `FS98_DIR=<slot>/game`.
- **Scheduling.** There is one work queue in file order; a free READY slot takes the next unrecorded line, and gets an `ACTIVE` marker before dispatch.
  - **Conflict groups:** lines in one group never run at once, in any slot. They are a commented table. There is one group for each harness a break runner kills machine-wide with `pkill -f`:
    - `host_breaks.py:635`: `host-breaks` with `hostdiff-run`;
    - `g3d_breaks.py:434`: `g3d-breaks` with `g3ddiff-run`/`g3ddiff-meshc-run`.
  - **Solo lines** run with no other line running: the queue waits for every slot to be free, runs the line, then continues. A line is solo when it needs an X display, writes the game directory, or runs `FLTSIM98.EXE`. `smoketest.sh:30` refuses to start while any `FLTSIM98.EXE` exists. Reading the slot's own game files is not solo. The table is commented, one pattern per command, covering at least:
    - `reg_contracts.sh`, `main-smoke`, `smoke`, `flights-play`, `main-dialogs`, `sim1native-flights`;
    - `hg2d_frames.sh`, `mdl-crash-game`, `gauges-panels`, `mdl_crash_frames`, `mdl_frames`;
    - `ofstest-run`, `util-trace`.
  - **Install hooks:** `main-smoke`, `smoke`, `ofstest-run` and `util-trace` test our installed build, so `ofs_install` into `<slot>/game` runs just before each, as part of the line. A failed install fails the line. Lines that swap modules themselves (`sim1native-flights`, `flights-play`, `main-smoke`'s own swap) are left to do so.
- **Timeouts.** SIGTERM to the line's process group, then SIGKILL 60 s later. The record keeps `exit: "timeout"`.
- **After every line:**
  1. **Cleanup.** Every process whose `/proc/<pid>/cwd`, executable, or `WINEPREFIX` (from `/proc/<pid>/environ`) lies under the slot path is a process the slot owns, whether or not its prefix still exists. Run `wineserver -k` for each owned prefix that still exists. Send SIGTERM to every owned process, wait up to 30 s, then SIGKILL, and repeat the scan until none is left or 120 s pass. A survivor marks the slot broken, and the run stops with a message. Never touch `~/.wine` or a process outside the slot.
  2. **The tree check.** Compare the slot's `cmake`, `docs`, `include`, `src` and `tools` with its template's by content, ignoring `__pycache__`. If any file was added, removed or changed, the record gets `tree_changed: [paths]` and the verdict becomes FAIL, with the reason in the record and in `report.md`.
  3. **Reset.** Run `rsync -a --delete --checksum <slots>/K.template/ <slots>/K/` (no `-H`; the template shares no inodes with the slot). Checksums cover every file, since a runner may restore a file's mtime over changed content. This restores sources, `bin`, `game`, the prefix and the build directory exactly as prepared, so no mutant source, mutant output, installed module or game write reaches a later line. The duration is recorded as `reset_seconds`.
  4. Clear `ACTIVE`.
- **Resume.** A slot is reused only when its template has `READY` and its manifest matches the current start tree, fixtures, `cmake_verify.py` and slot path. One with `ACTIVE` gets the cleanup and reset first. Anything else is prepared again. The results directory holds `meta.json` with the sha256 of the parsed Baselines list. A results directory whose `meta.json` differs, or that has none, is refused with a message unless `--lines` names what to re-run.
- **Records** gain `slot`, `tree_changed`, `reset_seconds` and `start`/`end` wall-clock timestamps. `report.md` shows `slot` and `tree_changed`.

## Boundaries & Constraints

**Always:**
- Keep the existing pure functions (`need`, `found`, `break_counts`, `break_verdict`, `minutes`, `plan`) and their asserts.
- `--clean-slots` removes `<results>/slots` after a run.
- The docstring gives the disk use, measured during the matrix runs: a slot and its template each hold a fresh `bin`, a game (about 0.3 GB) and a wine prefix (2.8 GB).
- Document `-j`, `--in-place`, slots, solo lines, conflict groups, the tree check and `--clean-slots` in `docs/tools.md`, beside the Baselines block. `AGENTS.md` is a managed block; leave it.
- Never `wineserver -k` in `~/.wine`. Never `pkill -f` a pattern that appears in the same command line. Never `rm -rf bin`; deletion is limited to `<results>/slots`.
- No staging or commits. Record the files in `~/openfs98-verify/story-files.md` under "parallel verification".

**Never:**
- Don't change any break runner, harness or script, except these targeted fixes:
  - `ofstest-run` and `util-trace` (`cmake/targets.cmake:682,725`) read and delete their logs under `$HOME/.wine`; they use `${WINEPREFIX:-$HOME/.wine}`;
  - the Baselines line `sim1state-build / sim1air-build` (`docs/tools.md:234`) is shorthand, of which `plan()` runs only the first target; it becomes two lines.

  That gives 161 lines and shifts later indices; `meta.json` (above) refuses an old results directory.

  B, the runners' own worker pools, is deferred.
- Don't change the verdict rules.
- Don't run the full Baselines block as part of this work.

## I/O & Edge-Case Matrix

| Scenario | Input | Expected |
|---|---|---|
| One slot | `-j 1` on three short lines | verdicts equal to an `--in-place` run of the same lines |
| Parallel | `-j 3` on six short lines, including one break suite | all six recorded, at most 3 overlapping (by `start`/`end`), verdicts equal to the `-j 1` run's |
| Solo | a solo line in the selection | no other line overlaps it |
| Conflict group | two lines of one group in a `-j 3` selection | they never overlap |
| Mutant source | a test line that edits a compiled file under `src/`, rebuilds that module and exits 0 | its record has `tree_changed` and FAIL; after the reset, the next line in that slot sees the original module (normalized hash equal to the pre-test one) |
| Mutant output | a test line that restores its source but leaves a rebuilt, changed module under `bin/`, and exits 1 | after the reset, the next line sees the original module |
| Game write | a test line that adds `Modules/OFSTEST.DLL` to the slot's game and writes a settings file | after the reset, neither is there |
| Same-size mutant | a test line that changes a byte of a built module and restores its mtime | after the reset, the module is the original |
| Prefix isolation | the start prefix's `dosdevices/c:` is an absolute link into `~/.wine` | in the slot it points to `../drive_c`, and a test line's write to `C:\\x` lands in the slot |
| Fixtures | a slot after setup | `difftest/replay.bin` and the other listed fixtures are present; `panelsunit-run` tests the replay case |
| Timeout | a test line that traps SIGTERM, logs it and keeps sleeping past a `--factor`-shrunk limit | the trap logs SIGTERM; SIGKILL follows 60 s later; `exit: "timeout"` |
| Escaped process | a test line that starts a wineserver in `<slot>/x/prefix`, deletes that prefix directory, and exits | no process with that `WINEPREFIX` remains after cleanup |
| Resume | a run interrupted mid-line, then restarted with the same results dir | recorded lines are skipped; the interrupted line's slot is cleaned and reset; the others are reused |
| Cost | the matrix runs themselves | the records show the setup time per slot, `reset_seconds`, and the peak disk use |
| Old results | a results dir from the 160-line list | refused with a message |

</frozen-after-approval>

## Code Map

- `tools/cmake_verify.py` -- argument parsing (`sys.argv.index`, :22-28), the Baselines parse (:30-37), `plan()`, the serial loop and `Popen(..., start_new_session=True)`/`killpg(..., 9)` (:135-170), the `report.md` writer (:172-181), the import-time asserts.
- `tools/smoketest.sh:20,30`, `tools/main_smoke.sh:5` -- `FS98_DIR`/`$PWD/game` and the machine-wide `FLTSIM98.EXE` refusal.
- `tools/hg2d_frames.sh`, `tools/reg_contracts.sh`, `tools/flights_play.sh`, `tools/main_dialogs.sh`, `tools/frames_many.sh`, `tools/mdl_crash_frames.py`, `tools/mdl_frames.py`, `cmake/targets.cmake:523,535` -- the hard-coded displays (`:62`, `:150+`, `:81`, `:97`, `:80+`, `:86+`, `:76`, `:176/177`) that make those lines solo.
- `cmake/targets.cmake:117,128,439,681-731` -- the `FS98_DIR` default `$HOME/.wine/drive_c/FS98`, which the slot environment overrides.
- `docs/tools.md` -- the Baselines block and its prose (where `-j` is documented).

## Tasks & Acceptance

**Execution:**
- [x] `tools/cmake_verify.py` -- `-j N`, `--in-place`, `--baselines`, slots (setup, markers, reuse, discard, `--clean-slots`), the solo and conflict tables, the install hooks, the scheduler, SIGTERM-then-SIGKILL, prefix cleanup, the tree check, the old-results refusal, the new record fields and `report.md` columns, and asserts for the tables.
- [x] `cmake/targets.cmake`, `docs/tools.md` -- the two targeted fixes.
- [x] A small test harness in the scratchpad (not in the repo) -- throwaway Baselines files for the matrix rows, fed through `--baselines`.
- [x] `docs/tools.md` -- the `-j` documentation; `~/openfs98-verify/story-files.md`.

**Acceptance Criteria:**
- Given a frozen copy of the tree, when each matrix row is run, then it behaves as the matrix says (records and logs kept in the scratchpad as evidence).
- Given `--in-place`, when the same short selection is run before and after the change, then the verdicts are equal.
- Given the change, when `python3 -B tools/cmake_verify.py x y --list` runs, then the asserts pass and it lists 161 lines, the old list with `sim1state-build` and `sim1air-build` as two.

## Implementation Notes

## Spec Change Log

## Review Triage Log

Code review, pass 1 (Codex gpt-6.1-sol, medium; Blind Hunter 8, Edge Case Hunter 14, Verification Gap 2 plus 1). Grouped by root cause.

| # | Layer | Finding | Verdict | Evidence | Route |
|---|---|---|---|---|---|
| 1 | all three | `--clean-slots` always raises AssertionError | high | it calls `rm_slot_path(slots_dir)`, which forbids that path; reproduced | patch |
| 2 | edge | `wineserver -k` could follow a slot-local prefix symlink into `~/.wine` | high | no realpath check before the kill | patch |
| 3 | blind, edge | A `wineserver -k` timeout escapes before the fallback signals; no overall deadline | medium | `TimeoutExpired` is uncaught | patch |
| 4 | edge | `killpg` can raise ProcessLookupError | medium | race between the timeout and the kill | patch |
| 5 | blind, edge | An invalid template's slot is deleted before its processes are cleaned up | medium | order in setup | patch |
| 6 | blind, edge | The manifest omits `CMakeLists.txt`, `game`, `orig`, the prefix and the `bin` fixtures | medium | a stale template could be reused | patch |
| 7 | blind, edge | `drive_c/FS98` is repointed only if it is already a symlink | medium | a real directory or a missing entry slips through | patch |
| 8 | blind, edge | `tree_hash` misses directory symlinks | low | `os.walk` puts them in dirnames; a direct correction | patch |
| 9 | blind, edge | A `slots` symlink, or `orig`/`game` symlinks out of the tree, could redirect writes or deletion | low | no check | patch |
| 10 | edge | A `VAR=value` naming a slot variable raises TypeError | low | duplicate keyword arguments | patch |
| 11 | blind, edge | `--lines` on a mismatched results directory mixes records | medium | the meta handling | patch |
| 12 | edge | Writing `ACTIVE` failing leaves workers blocked | low | needs a disk failure; the fix adds a handler | reject |
| 13 | edge | The 256 MB `bin` skip departs from the spec's full fill-in | medium | a documented deviation, needed for a 106 GB live `bin` | patch: a `--bin-skip-mb` flag, 0 copies everything |
| 14 | gap ×2 | The matrix tests print hashes and timestamps but assert nothing | medium | pre-verified | patch: a checker script |

## Verification

**Commands:**
- `python3 -B tools/cmake_verify.py build/win32-retail /tmp/x --list` -- expected: asserts pass; 161 lines listed.
- The matrix runs above, from a frozen copy under `~/openfs98-verify/` (never in the live tree), with `--baselines` test files -- expected: as the matrix says.
