---
title: 'Cleanup 3a: a recurring build-wiring check (rebuild edges and lint wiring)'
type: 'chore'
created: '2026-11-25'
status: 'done'
route: 'dispatch'
baseline_commit: '57016e041ad690d4b57bd71629155dfeca517e8b'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-cleanup-1-deps-retail-ids-norm-pe.md', '{project-root}/OpenFS98/docs/implementation/spec-cleanup-2-tooling-guards.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Two open `docs/implementation/deferred-work.md` entries:
- **(E)** Nothing re-checks cleanup 1's rebuild edges automatically (about line 181).
- **(V1)** Nothing re-checks that the codegen check stays wired into `ofs_arena_lint` (about line 200).

File timestamps cannot prove which recipes ran. Some steps run their recipe yet leave the declared output untouched (`cmp -s ... || cp`, `cmake/steps/all.cmake:386,449`).

**Approach:**
- `ofs_step` gains an opt-in execution log, `OFS_STEP_LOG`: each recipe that runs appends its step id. The build outputs are unchanged.
- A new `tools/check_build_wiring.py` builds a scratch copy once, then replays cleanup 1's rebuild cases and the lint-wiring cases. Each case asserts the exact set of step ids that ran.
- It is added as a Baselines line.

**Split (decided 2026-11-25):** cleanup 3 is two slices: 3a, this one, and 3b, the `cmake_verify` tree guards (entry C, about line 88), specced afterwards. C's entry gains this spec review's findings.

## Boundaries & Constraints

**Always:**
- **The hook.** In `ofs_step` (`cmake/OpenFS98.cmake:26-90`), every custom command gets a first command that appends the step id to `$OFS_STEP_LOG` when that variable is set, and does nothing otherwise.
  - The step id is the step's first declared output, relative to the source root, so it is stable and unique.
  - `ofs_target` commands are not logged.
- **The hook's proof:** the identity gate, with `~/openfs98-verify/gate/build_side.sh` building before (the baseline) and after at one root, `~/openfs98-verify/cl3/root`, then `compare.py` with no `--require-set`. It must print `GATE PASS`: every PE and object identical, and replays equal. The changed commands rebuild every step once.
- **`tools/check_build_wiring.py`:**
  - **The scratch tree:** with no arguments, it makes its own scratch directory with `tempfile.mkdtemp`, and fails if that resolves inside the source tree. `OFS_WIRING_SCRATCH` may name a parent instead. Its argv contains no placeholders. It always removes the directory on exit, success or failure. It runs every build in its own process group and kills that group on SIGTERM, SIGHUP or an exception.
  - **What it copies:** the source tree's working-tree files. With git, that is the paths from `git ls-files`, read from disk, with deleted ones skipped. Without git, inside a `cmake_verify` slot, it copies everything except `bin/`, `build*/`, results and fixture directories. It then copies `orig/`, the reference retail files the build reads, including `orig/Sim/SIM1.sim`, the gauges and the aircraft MDLs. Anything else a build turns out to need is named in the notes.
  - **Configure and build:** it configures with the project toolchain and the default generator, then runs `cmake --build <b> --target openfs98` and then `--target difftests`, as cleanup 1 did. It logs to `$OFS_STEP_LOG` throughout. The full first build's log gives the complete step inventory, and its size N is printed.
  - **The cases.** Each is a mutation, then the same two builds, then a second pair of builds. Each pair must log exactly the expected step ids, and the second pair must log none (lints still run, and they are not steps):
    - touch `tools/data_images.txt`: the 28 single-pair `--hash` steps (SIM1 and HOST included), the gauges step and the MDL step, then `allgaugesdiff.exe` and `allpaneldiff.exe`. A reconfigure is expected and asserted separately;
    - touch `src/advdrv/advdrv_relocs.inc`: ADVDRV's DLL step and `advdrvdiff.exe`'s;
    - touch `src/fe/fe_library.c`: FE's DLL step;
    - touch one `src/gauges/*.c`: the gauges step and both all-gauge harnesses;
    - delete `bin/gauges-all/all_panels.inc`: the gauges step and both harnesses;
    - remove `bin/gauges-all`, then build: success, with the gauges step and both harnesses;
    - a wrong pinned hash on WINMSG's single-pair row, with `UPDATE` unset: the build fails with WINMSG's hash-mismatch diagnostic, not a configure error. After the row is restored, WINMSG's step is logged and succeeds, and the next pair logs none;
    - lint wiring: drop `-fno-ipa-icf` from ADVDRV's recipe in the scratch `cmake/steps/all.cmake`. Then:
      - `--target graph`, a command-only target, fails with the codegen check's exact missing-flag message naming ADVDRV's site, and the graph command's own output is absent;
      - with one output step forced dirty (touch its source), `--target openfs98` fails the same way with no step id logged;
      - restoring the flag makes both succeed.
  - **Output:** the expected sets live in the tool, built from cleanup 1's notes. It prints `build wiring: <k> cases, <N> steps, ok`, or names the case with its unexpected and missing ids and exits 1. `--self-test` checks the set comparison and the log parsing on synthetic logs.
  - **Meta-proof,** run once and recorded in the notes. In a scratch copy:
    - removing the codegen `COMMAND` from `ofs_arena_lint` makes the lint-wiring cases fail;
    - removing the `data_images.txt` edge from `ofs_step` makes its case fail;
    - removing `ofs_arena_lint` from output steps' `DEPENDS` makes the `openfs98` lint case fail.
- **The Baselines line:** `python3 -B tools/check_build_wiring.py`, appended at the end of the block so existing `--lines` numbers stay, with its expectation `-> build wiring: <k> cases, <N> steps, ok` (N ≥ 100, so `cmake_verify` checks it) and its measured time at `nice -n 19 -j 2`.
  - Check it through `python3 -B tools/cmake_verify.py <b> <r> --only check_build_wiring --in-place`, and through one slot.
  - The notes say how a failure shows: the row is FAIL, but a failed row does not make `cmake_verify` exit non-zero.
- **Paperwork:**
  - `docs/tools.md`: a row for the tool and the `OFS_STEP_LOG` hook.
  - `deferred-work.md`: E and V1 move to `deferred-work-resolved.md`. C gains this spec review's findings for 3b:
    - resuming skips the line that stopped the run;
    - parallel workers need stop semantics;
    - the slot and in-place inventories protect different paths (`CMakeLists.txt` and untracked files);
    - a deliberate doc edit would halt runs;
    - `--lines` interacts with the marker.
- **Running:** in a worktree `~/source/OpenFS98-cl3` off HEAD, with fixtures and this approved spec copied in. Builds use `nice -n 19` and `-j 2`. No wine.

**Never:**
- No change to any build output or recipe behaviour: the hook only appends to a log when asked.
- No change to `cmake_verify.py` (that is 3b) or to any break runner.
- No commits or staging.

</frozen-after-approval>

## Code Map

- **`cmake/OpenFS98.cmake:26-90`:** `ofs_step`, which adds `COMMAND sh -c` per line, its `DEPENDS`, and `ofs_arena_lint` (with the codegen `COMMAND`) at `:24-27`. The `ofs_target` wiring is at about `:97-110`.
- **`cmake/steps/all.cmake`:** the `cmp -s || cp` recipes at `:386` and `:449`; the gauges step at `:404-420` (three outputs); the MDL step at `:424-445` (`${OFS_MDL_DLLS}`); WINMSG's step at `:268-275`.
- **`cmake/targets.cmake:736`:** `graph`, which is command-only and inherits `ofs_arena_lint`.
- **Cleanup 1's cases and their expected sets:** `docs/implementation/spec-cleanup-1-deps-retail-ids-norm-pe.md:190` onward (Implementation Notes); `~/openfs98-verify/cl1/proof.log`. Its configure plus two initial builds took about 4.5 minutes.
- **Codegen check messages:** `tools/check_codegen_overrides.py:95`.
- **`cmake_verify` timeouts and verdicts:** `tools/cmake_verify.py:66` (`minutes`), `:403`, `:432`, `:439`, `:589`.

## Tasks & Acceptance

**Execution:**
- [ ] The worktree. The `ofs_step` hook, and the identity gate for it.
- [ ] `tools/check_build_wiring.py`, run once standalone and once through `cmake_verify` (`--in-place` and one slot).
- [ ] The meta-proof.
- [ ] Docs, the Baselines line, `deferred-work.md` and the resolved file; `## Implementation Notes` with timings and N.

**Acceptance Criteria:**
- **The hook:** the identity gate prints `GATE PASS` for the `ofs_step` change.
- **The tool:** `check_build_wiring.py --self-test` and the full run pass, standalone and through `cmake_verify`.
- **The meta-proof:** each of the three removals makes its case fail.
- **Scope:** only the files listed changed.

## Verification

**Commands:**
- `python3 -B tools/check_build_wiring.py --self-test` -- expected: pass.
- `python3 -B tools/check_build_wiring.py` -- expected: `build wiring: <k> cases, <N> steps, ok`.

## Review Triage Log

Spec review (Codex gpt-6.2-sol, 2026-11-25), on the combined cleanup 3 draft:

| # | Finding | Verdict | Evidence / action |
|---|---|---|---|
| 1 | `<scratch>` in a Baselines line is shell redirection; cleanup and process groups | blocker | mkdtemp or an env var, always removed, its own process group. |
| 2 | File stats cannot prove which recipes ran (`cmp -s \|\| cp`) | blocker | The `OFS_STEP_LOG` hook; the hook itself proved by the identity gate. |
| 3 | The output inventory and build sequence | high | Step ids from the first full build; the two builds pinned; "nothing" means zero step ids. |
| 4–6 | `cmake_verify` resume, parallel stop, differing inventories | high | Moved to 3b with the user's split; recorded on entry C. |
| 7 | `git ls-files` lists the index; slots may have no `.git` | high | Working-tree reads; a no-git copy rule. |
| 8 | Fixtures and timing | medium | `orig/` pinned; time measured at `nice -n 19 -j 2`. |
| 9 | Stronger negative proofs | medium | The exact message; graph output absent; an output-target case; edges mutated independently. |
| 10 | "Runs every time" overstated; a small count gives UNCHECKED | medium | N ≥ 100 in the expectation; appended at the end; FAIL behaviour noted. |

Code review (Codex gpt-6.2-sol, three layers, 2026-11-25):

| # | Finding | Verdict | Action |
|---|---|---|---|
| 1 | Symlinked mutation inputs could edit files outside scratch | high | Inputs materialised; mutated paths asserted inside scratch. |
| 2 | Signal races at mkdtemp/Popen; new session escapes the caller | high | Signals blocked across registration; children reaped; own process group, same session. |
| 3 | WINMSG case skipped the real manifest-edit path | high | A plain row edit; repeated failure required; exact set after restore. |
| 4 | Expected ids derived from the recipes under test | medium | Pinned by name. |
| 5 | Lint probes: steps not required empty; one dirty step; partial graph check | medium | Empty logs on all probes; many steps dirty; full diagnostic; every graph line absent. |
| 6 | Quiet pairs don't prove lint ran; reconfigure inferred loosely | medium/low | Lint messages required; configure passes counted. |
| 7 | Relative scratch parent; orig copied twice | low | Absolute path; one copy. |
| 8 | Proof transcripts (compare FAIL, head exit statuses) | low | Gate compare and meta-proof re-run with true statuses. |
| 9 | Hook quoting, log-write failure, Ninja restat, NFS, full inventory, `8` unchecked, results names | rejected | Noted as limits. |

Follow-up review (Codex gpt-6.2-sol, 2026-11-25):

| # | Finding | Verdict | Action |
|---|---|---|---|
| 1 | The signal handler deadlocks in p.wait() during communicate() (reproduced) | high | The handler raises; cleanup after unwinding. |
| 2 | Signals left blocked after cleanup | low | The mask restored on every path. |
| 3 | Any `arena lint:` line accepted | low | The full success line; a self-test rejects the advisory. |
| 4 | The lint removals were caught before the named probes | medium | Re-run with the earlier check bypassed in scratch; the lint case fails on its own. |

## Implementation Notes

Worktree `~/source/OpenFS98-cl3` (detached at the baseline); logs in `~/openfs98-verify/cl3` (`gate-*.log`, `compare.out`, `wiring1.log`, `wiring2.log`, `meta.log`, `cv-inplace.out`, `cv-slot.out`).

- **The hook.** `ofs_step` prepends `sh -c '[ -z "$OFS_STEP_LOG" ] || echo <first output> >> "$OFS_STEP_LOG"'` to every step's commands; `ofs_target` is untouched. Identity gate (`build_side.sh` before and after at `~/openfs98-verify/cl3/root`, `compare.py` without `--require-set`): artifacts 1130/1130, PEs 321, objects 206, replayed units 524. As in cleanup 1, `compare.py` first reported only the missing `after/lint-prior.log`; I wrote it by running `check_arena.py --prior <baseline's tools/arena_lint_allowed.txt>` in the after root (rc 0), then `GATE PASS`.
- **The tool.** `tools/check_build_wiring.py`: 8 cases (data_images.txt with its reconfigure, `advdrv_relocs.inc`, `fe_library.c`, one gauges source, `all_panels.inc` deleted, `bin/gauges-all` removed, WINMSG's wrong hash, lint wiring), N = 161 steps (the first build's log has no repeats). Full run `build wiring: 8 cases, 161 steps, ok`; 9m14s at `nice -n 19`, `-j 2` (with the identity gate also running); through `cmake_verify --in-place` PASS in 556 s, and in a slot (a tree copy without `.git`, so the no-git copy path) PASS in 567 s.
  - The first full run found two wrong expectations of mine, not tool bugs: restoring ADVDRV's flag reruns only `bin/ADVDRV.DLL` (`advdrvdiff.exe` does not depend on `advdrv.c`).
  - **WINMSG case.** A plain edit of `data_images.txt` would dirty all 28 hash steps, so the bad row is written with the file's old mtime, and WINMSG's step is dirtied by touching `src/winmsg/winmsg.c` (before the bad row and again after the restore, since the failed step leaves its DLL newer than the source). Only `openfs98` is built in the failing pass (a `difftests` build would then run every step after WINMSG). The failure must contain `bin/WINMSG.DLL: our .data image (or its relocations) differs from the manifest` and no `CMake Error`; the pass after the restore logs only `bin/WINMSG.DLL`.
  - **Lint case.** The `graph` marker is the first line of `tools/modgraph.py orig/Modules` run before the mutation; it must be absent from the failing output. The `openfs98` pass with ADVDRV's source touched fails with the same message and logs no step.
  - The scratch copy needed `orig/` only (no `game/`, `cd/`). Without git, a copy skips top-level `bin`, `build*`, `game`, `cd`, `wineprefix`, `slots`, `RESULTS`, `.git` (a named list, marked `shortcut:` in the code).
- **Meta-proof** (scratch copies of the worktree, one mutation each; `meta.log`): removing the codegen `COMMAND` from `ofs_arena_lint` fails the lint case ("graph with the flag dropped (rc 0) must fail"); removing the `check_data_image` edge from `ofs_step` fails the first case (all 28 hash steps missing); removing `ofs_arena_lint` from output steps' `DEPENDS` fails the `openfs98` lint case (rc 0, `bin/ADVDRV.DLL` ran).
- **A failure shows** as a FAIL row in `cmake_verify` (the tool exits 1 and prints the case with its unexpected and missing ids in the log); `cmake_verify` itself still exits 0.
- **Slot runs need** `game/` and the five `REQUIRED_FIXTURES` in the start tree's `bin` (copied from the main tree; FRAMELOCK.DLL does not exist there), and a `.git` that is a directory or absent (a worktree's `.git` file is refused).
- **Paperwork.** `docs/tools.md`: a row, the Baselines line (appended last, N = 161 >= 100) and the line count (the doc said 168, the block has 179). E and V1 moved to `deferred-work-resolved.md`; entry C (the `cmake_verify` leak entry) gained the five 3b findings.
- Not done: no 3b work (`cmake_verify.py` untouched); the Baselines time is one measurement under load.

### Review fixes (2026-11-25)

Logs in `~/openfs98-verify/cl3/round2`. Only `tools/check_build_wiring.py`, `docs/tools.md` (time) and this spec changed; `cmake/OpenFS98.cmake` is as gated.

- **Tool patches:** files are copied with symlinks followed, and every mutated path must resolve inside scratch; SIGTERM/SIGHUP/SIGINT are blocked across `mkdtemp` and each `Popen` plus its registration; children get their own process group (`setpgid`, same session), are killed and reaped before scratch goes; the scratch path is absolute; `orig/` is copied once; the 28 single-pair ids are pinned by name; the quiet pairs require both lint success lines; configure passes are counted per case (the data_images case's first pair 1, quiet pairs 0).
- **WINMSG case:** the row is edited normally (new mtime, UPDATE unset). The failing pass must show WINMSG's diagnostic, no `CMake Error`, WINMSG's id and nothing outside the data_images set; a second build with the bad pin must fail again (it does: a failed check does not leave an output that passes the next time); after the restore the pair logs exactly the data_images set, then none.
- **Lint case:** both `graph` probes and an `openfs98` probe with `tools/data_images.txt` touched (most steps dirty) must log no step and print no line of `modgraph`'s output; the diagnostic is matched as the full line `error: cmake/steps/all.cmake:21: missing -fno-ipa-icf (...)`. A first run failed because I had not included the `error: ` prefix.
- **Runs:** `--self-test` passes; full run `build wiring: 8 cases, 161 steps, ok` (N unchanged), 12m23s; `cmake_verify --in-place` PASS in 707 s (11m47s wall), so the Baselines time is now ~12 min. Identity gate: `compare-transcript.log` (compare.py on the retained before/after roots, no rebuild) ends `GATE PASS`; `lint-prior.log` is the one written earlier by `check_arena.py --prior <baseline's arena_lint_allowed.txt>` in the after root (build_side.sh writes it only when given a prior inventory).
- **Meta-proof re-run** (tool's own exit status, 1 each, `meta{1,2,3}.log`): removing `data_images.txt` from `ofs_step` fails the first case; removing the codegen `COMMAND` and removing `ofs_arena_lint` from `DEPENDS` are now both caught earlier than the lint case, by the quiet-pair requirement that both lint success lines show ("lints not shown": with the lint off the `openfs98` build prints no lint).
- **Accepted limits:** step ids are echoed unquoted (our own fixed paths); a log write failure fails the build (opt-in); the default Make generator only (no Ninja restat); no NFS append semantics; the inventory is checked by size and the pinned ids, not a full list; `cmake_verify` checks only numbers >= 100 in the expectation (not the `8`); the no-git rule names results directories.

### Follow-up fixes (2026-11-25)

Logs in `~/openfs98-verify/cl3/round2`. The signal handler only raises `SystemExit`; kill, reap and scratch removal run after the stack unwinds, with the signal mask saved and restored around the final cleanup. A SIGTERM 25 s into a run exits 143 in 0.14 s and leaves no scratch directory. The arena lint line must be its full summary (`arena lint: N files allowlisted, N casts, N defines left`); `--self-test` rejects the "no prior allowlist" advisory. Full run `build wiring: 8 cases, 161 steps, ok` (12m42s). The two lint removals, re-run with only the quiet-pair lint-line assertion bypassed in a scratch copy of the tool (`WIRING_BYPASS_LINT`, not in the real tool): `meta2b.log` (codegen `COMMAND` removed) fails the lint-wiring case at the `graph` probe (rc 0, expected failure), `meta3b.log` (`ofs_arena_lint` out of `DEPENDS`) fails it at the `openfs98` probe (rc 0, steps ran); both tool exit status 1.
