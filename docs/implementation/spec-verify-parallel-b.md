---
title: 'Parallel verification B: a worker pool inside the long break runners'
type: 'feature'
created: '2026-11-22'
status: 'in-progress'
baseline_commit: 'c1f577c4048f2492cbb9ab099284072aedc8c835' # plus uncommitted Stories 1.3-1.4; file snapshot at ~/openfs98-verify/baseline-verify-parallel-b
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-verify-parallel.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Part A (`tools/cmake_verify.py -j N`) runs Baselines lines at once, but one break suite still runs its breaks serially in one tree. The held-back Story 1.4 runs are bounded by four lines: `options-breaks N=400` (~8 h), `host-breaks` (~5 h), `g3d-breaks N=1000` (~4 h) and `sim1r-breaks` (past 2 h).

**Approach:** Add `tools/break_pool.py`. With `BREAK_JOBS=N`, it runs a runner's breaks on N workers. Each worker has:
- a copy of the runner's tree, in its `bin/break-pool/`;
- its own build directory;
- a private copy of the runner's wine prefix.

Part A's prefix and process code moves into a shared module. The pool is adopted in `host_breaks.py`, `options_breaks.py`, `g3d_breaks.py` and `sim1r_breaks.py`. Per-break verdict lines, their order and the summary must equal the serial run's. With `BREAK_JOBS` unset or 1, everything is exactly as today.

Decision: `main_breaks.py` stays out. Its 34 suites are separate Baselines lines, which part A already runs at once, and the longest is ~90 min.

## Boundaries & Constraints

**Always:**
- **Shared code: `tools/slots.py`** (new):
  - `under`;
  - a guarded `remove(path, base)`;
  - `owned_procs`;
  - `cleanup` (bounded; it returns False on survivors);
  - `copy_prefix(src, slot, game, forbid)`, the prefix block of `prepare_slot` (`cmake_verify.py:323-347`), with the source prefix as a parameter. Its destination must not exist yet. It keeps Part A's rule exactly: links into the source prefix are rewritten, and a link that still resolves into the source prefix or into a `forbid` tree outside the new slot is rejected. System links stay as they are (`z: -> /`, devices, `d:`). With `game=None`, `drive_c/FS98` is removed and not mapped, because workers never run the game.

  `cmake_verify.py` imports these and behaves identically.
- **A worker:**
  - **Location:** `bin/break-pool/<runner>-<pid>/<k>` in the runner's tree, so Part A's `owned_procs(slot)` and cleanup cover it.
    - `prepare_slot`'s `bin` rsync excludes `bin/break-pool`.
    - Deletion is limited to that run's own `<runner>-<pid>` directory, never `bin`.
    - A `<runner>-<pid>` directory counts as stale only when its `owner` marker names a dead PID. Only stale directories are cleaned up and removed; a live run's directory is never touched.
  - **Contents:**
    - a `cp -a` of the tree's top level, except `bin`, `build`, `game`, `cd`, `.git` and `wineprefix` (a Part A slot has one);
    - `cd` as a symlink; no `game`;
    - the runner's declared `POOL_BIN` paths, copied from its `bin/` after its target's preamble has staged them (host: `difftest/` and `host-rt-standalone/`; g3d: `difftest/`, `G3D.DLL` and `g3d.res`, because `g3d-break-build` links it (`targets.cmake:1230`); options: `options-diff/` minus `wineprefix/`; sim1r: nothing copied, because its per-break `tools/ofs sim1rdiff` depends on `openfs98 difftests`, so its `prepare` builds those once in the worker, as part A's slots do), plus `POOL_LINK` read-only files as symlinks (`sim1-diff/snapshots.bin`). Extend a list where a runner's build or run needs more of `bin/` (an inner `tools/ofs` target's inputs), and record why. Never copy all of `bin/`;
    - a fresh `cmake` configure into `build/win32-retail`;
    - a `copy_prefix(<runner's WINEPREFIX, default ~/.wine>, worker, game=None, forbid=<runner tree>)`.
  - **Prerequisites:** each runner asserts them in every worker before the first break (exit 2 naming the missing file).
  - **A hook:** `prepare(worker)` holds per-worker setup, such as g3d's `g3ddiff_break.exe` copy and Options' `wineserver -p60`. `g3ddiff.c:1225` loads `WINDOW_retail.dll`, which the `g3d-breaks` preamble (`targets.cmake:535`) does not stage. Add `cp orig/Modules/WINDOW.DLL bin/difftest/WINDOW_retail.dll` to that preamble, so the oracle, serial and parallel runs all have it. Workers copy it with `difftest/`, and its absence is a setup failure.
- **Environment:** a worker's `env` is `os.environ` plus `WINEPREFIX`, `OFS_BUILD` and `FS98_DIR` for that worker. A runner layers its own per-call variables on top, as today (host's `WINEDEBUG` and `HOSTDIFF_SKIP_FILES`; Options' `WINEARCH`). No `os.chdir` and no `os.environ` change in threads.
- **Runs:**
  - Each break calls `one(entry, worker) -> (result, text)`, which is today's loop body rooted at `worker.root`.
  - The source is mutated from originals captured before dispatch, and restored in a per-break `finally` (sim1r restores after every break, a build failure included).
  - The main thread prints texts in the serial order as soon as everything before them is printed.
  - Results return in that order, so the summary code is unchanged.
- **Timeouts and verdicts** stay per runner:
  - host and g3d count a hang as caught;
  - Options counts a timeout as not counted;
  - sim1r stays unbounded.

  In a worker, the hang kill is `wineserver -k` in the worker's prefix plus `cleanup(worker)`. `pkill -f` stays on the serial path only.
- **Artifacts:** a break's own files (Options' `<name>.c`, `<name>.exe` and `<name>.log`) have their state recorded right after the break, by comparing each file's mtime and existence before and after. A file is either written (snapshot it), deleted by the break (Options unlinks the exe before building; a mark) or untouched (no change). The changes are published to the runner's tree when the break's text is printed, that is in serial order: a written file is copied, a deleted one is removed, and an untouched one leaves the runner's current file alone. Names repeat across Options' units (`accept-state`), so the same file ends as the serial run leaves it. Fixed-name transcripts (g3d's `g3d_retail.txt`/`g3d_ours.txt`) are not copied back; this is documented.
- **Records:** with `BREAK_POOL_RECORD=<file>`, set or not on either path, each break's line goes to a JSON file in serial order. The line holds the entry index, its name, the unit or suite, the build exit, the raw harness exit, the harness's own summary line (host and g3d's last `hostdiff:`/`g3ddiff:` line, sim1r's `SIM1:` line, Options' last line) and the first `DIFFER` line. It writes nothing to stdout.
- **Teardown:**
  - On normal end, an exception or SIGINT/SIGTERM, every worker is cleaned up in `finally`. A signal stops the queue and SIGKILLs the workers' processes within 30 s, inside Part A's 60 s grace (`cmake_verify.py:446`).
  - Survivors give exit 2 and name the PIDs.
  - Workers are deleted (`remove`) unless `BREAK_POOL_KEEP=1`, which keeps files only.
- **The final step:** each runner's final restore and rebuild runs once, in its own tree.

**Never:**
- Change a break's text, a verdict rule or a summary line.
- Write to `~/.wine` or to the runner's prefix.
- Run `wineserver -k` outside a worker prefix.
- Adopt the pool in other runners.
- Run the full suites. They are the held-back 1.4 runs.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected | Error Handling |
|---|---|---|---|
| Serial | `BREAK_JOBS` unset or 1 | today's commands, cwd and output | N/A |
| Parallel | `BREAK_JOBS=3` over a subset | the verdict lines and summary equal the serial run byte for byte, in order; the `BREAK_POOL_RECORD` files are equal (any difference in a timing-dependent count is reported) | N/A |
| Cross-suite reuse | sim1r subset spanning two suites, 2 workers | equal to serial; worker sources pristine after each break | per-break `finally` |
| Setup fails | `bin/break-pool` unwritable, or a prerequisite missing | exit 2 naming the worker log or file; no break run | teardown |
| Interrupted | SIGINT during a parallel run | no process under the pool survives; exit non-zero | teardown |
| More jobs than breaks | `BREAK_JOBS=8`, 3 breaks | 3 workers | N/A |

</frozen-after-approval>

## Code Map

- `tools/cmake_verify.py`:
  - `under` (`:229`) and `rm_slot_path` (`:233`), which is tied to `slots_dir`;
  - `prepare_slot` (`:291`); its prefix block is at `:323-347`, and its check that rejects links under `root` is at `:343-347`;
  - `owned_procs` (`:375`) and `cleanup` (`:397`);
  - `slot_env` (`:252`).
- `tools/host_breaks.py:605-655`:
  - writes `bin/difftest/host_break.c` and builds with `tools/ofs host-break-build`;
  - runs `wine hostdiff.exe` with `cwd=bin/difftest`;
  - sets `env` (`:629-631`);
  - kills a hang with `pkill` (`:635`);
  - the final step is `tools/ofs hostdiff`.

  Its target (`cmake/targets.cmake:830`) builds `host-rt-standalone`, stages the retail aliases and depends on `openfs98 difftests`. `hostdiff` loads `FLTSIM98.EXE`, `ENGLISH.FLL`, `SIMSUI.DLL` and `DIALOG.DLL` from `bin/difftest`.
- `tools/g3d_breaks.py:401-450`:
  - copies the exe once (`:403`) and writes `TMP`;
  - builds with `g3d-break-build`;
  - runs with `cwd=bin/difftest`;
  - kills a hang with `pkill` (`:434`).

  Its target (`targets.cmake:535`) stages `G3D_retail`, `UTIL_retail` and `G3D_ours`. `g3ddiff.c:4137` also loads `WINDOW_retail.dll`.
- `tools/options_breaks.py:1068-1128`:
  - mutants go to `<directory>/<name>.c|.exe` without touching `src`;
  - it builds with gcc directly;
  - the run has `timeout=300`.

  Run via `tools/run_optionsdiff.sh --breaks`, which sets `WINEPREFIX=bin/options-diff/wineprefix` and `WINEARCH`, takes `flock` and starts `wineserver -p60`.
- `tools/sim1r_breaks.py:1017-1056`:
  - rewrites `src/sim1/<suite>` under `root` and builds with `tools/ofs sim1rdiff`, which depends on `openfs98 difftests`;
  - runs `wine bin/sim1-diff/sim1rdiff.exe`;
  - restores at the end of each suite.

  `sim1rdiff.c:933` reads `bin/sim1-diff/snapshots.bin`. `bin/sim1-diff` is 27 GB, mostly run output.
- `cmake_verify.py:164-169`: the `CONFLICT` groups stay, because the serial path still uses `pkill`.

## Tasks & Acceptance

**Execution:**
- [ ] `tools/slots.py`, `tools/cmake_verify.py`: extract the code and import it. Add a self-test (`--self-test`) covering:
  - `under`;
  - `remove` refusing paths outside its base;
  - `copy_prefix` on a tiny fake prefix (a `dosdevices/c:` link and an absolute link into the source get rewritten, and nothing resolves outside).
- [ ] `tools/break_pool.py`: `workers`, `run`, `kill` and `teardown`. Add a self-test with a fake `one` and no wine:
  - 9 entries on 3 workers give results in order;
  - an exception in `one` tears down and is reported;
  - jobs greater than entries works;
  - an unwritable `bin/break-pool` gives exit 2;
  - a published deletion and an untouched file behave as in serial order.
- [ ] The four runners:
  - `POOL_BIN`/`POOL_LINK`;
  - prerequisite asserts;
  - `prepare` hooks;
  - `one`;
  - `break_pool.run`.
- [ ] `docs/tools.md`:
  - `BREAK_JOBS`, `BREAK_POOL_KEEP` and `BREAK_POOL_RECORD` on the four targets (there is no pool-directory setting);
  - the budget: about 3–4 GB of disk per worker (prefix, copied `bin`), and wine processes ≈ `-j` slots × `BREAK_JOBS`, at most 6 on this 12-core host;
  - the fixed-name transcripts that are not copied back.

  `docs/implementation/deferred-work.md`: mark part B done.

**Acceptance Criteria:**
- Given a frozen copy, the pre-change serial runner is the oracle. A subset of 10–14 breaks is run per runner, serially (the new code with `BREAK_JOBS` unset) and with `BREAK_JOBS=3`. The verdict lines must be identical to the oracle's, including their "N differ" counts. The subsets are listed in Implementation Notes and cover:
  - host: both `HOSTDIFF_SKIP_FILES` paths and a proven equivalent;
  - g3d: a hang or crash;
  - Options: an `unwind` break and a name repeated across units;
  - sim1r: two suites.

  The new serial run and the parallel run also write `BREAK_POOL_RECORD` files, and those must be equal. sim1rdiff drops comparisons whose retail side times out (`sim1rdiff.c:905`), so its `SIM1:` counts show load; if they differ, rerun with fewer jobs and record it. The commands, `N`, seeds and staged inputs are pinned in Implementation Notes. Any verdict decided by a timeout is noted. Wall times, including setup, are recorded.
- Given each parallel run, when it ends:
  - `~/.wine` and the runner's prefix are unchanged (listing hash);
  - `owned_procs` of every worker is empty;
  - the runner's tree passes `tree_hash`.
- Given the interrupted and setup-failure scenarios, they are run once each and recorded. One of them is a SIGTERM to the runner's process group, after which no process under `bin/break-pool` survives.
- Given part A, when `cmake_verify.py -j 1 --lines <one cheap line>` runs, then it gives the same verdict as before the extraction.

## Implementation Notes

## Spec Change Log

## Review Triage Log

| # | Finding (Codex, spec review) | Verdict | Change |
|---|---|---|---|
| 1 | workers inside the tree break the prefix checks | high | workers beside the tree; `copy_prefix` checks the links against the new slot (location superseded by 2.4) |
| 2 | a fresh build lacks the staged retail aliases; g3d's exe copy | high | `POOL_BIN` copied after the target's preamble, prerequisite asserts, `prepare` hook |
| 3 | sim1r mutations leak across suites in a reused worker | high | originals captured once; per-break `finally` restore |
| 4 | the hang policy differs per runner | medium | kept per runner |
| 5 | cleanup on failure is unspecified | medium | teardown in `finally`, signals, survivors give exit 2, stale workers |
| 6 | `env=worker.env` drops per-call variables; Options' wrapper | medium | layered `env`; prefix copied from the runner's own |
| 7 | the artifact copy-back is nondeterministic | medium | per-break names copied right after each break; fixed names documented |
| 8 | nested concurrency and disk | medium | budget documented; host, g3d and Options copy `bin` paths (their inner targets have no DEPENDS); only sim1r builds once per worker |
| 9 | the extraction is underspecified | medium | only the named functions; `prepare_slot` calls `copy_prefix`; a part A rerun AC |
| 10 | the ACs are weak | medium | the pre-change oracle, byte-equal lines with counts, the required coverage, the failure scenarios |
| 2.1 | the prefix containment rule escapes through game and `z:` | high | Part A's rule kept; `game=None`, so there is no FS98 mapping |
| 2.2 | g3d lacks `g3d.res` and `WINDOW_retail.dll` | high | added to `POOL_BIN`; `prepare` stages WINDOW |
| 2.3 | the stale cleanup can kill a live pool | high | per-run `<runner>-<pid>` with an owner marker; only dead owners cleaned |
| 2.4 | sibling pools escape Part A's cleanup | medium | pools back inside the tree (`bin/break-pool`), rsync exclusion, 30 s SIGTERM teardown |
| 2.5 | Options' names repeat, so the copy-back order matters | medium | snapshot, then publish in serial order |
| 2.6 | the verdict lines lack counts; sim1r drops comparisons under load | medium | `BREAK_POOL_RECORD` with exits, summary lines and the first `DIFFER`; load-dependent counts reported |
| 3.1 | a slot's `wineprefix` is copied, then nested | high | excluded from the tree copy; `copy_prefix` needs a new destination |
| 3.2 | WINDOW staged only in workers changes verdicts | medium | staged in the `g3d-breaks` preamble for all paths |
| 3.3 | Options' deletions and untouched files | medium | per-break changes (written, deleted, untouched) published in order |
| 3.4 | `BREAK_POOL_DIR` escapes containment | medium | removed; row 1 marked superseded |

## Verification

**Commands:**
- `python3 tools/slots.py --self-test && python3 tools/break_pool.py --self-test`: expected pass.
- The four subset trios (oracle, serial and `BREAK_JOBS=3`) from a frozen copy under `~/openfs98-verify/`: expected identical lines.
