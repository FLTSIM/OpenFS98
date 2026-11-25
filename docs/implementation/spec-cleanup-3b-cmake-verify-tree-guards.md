---
title: 'Cleanup 3b: cmake_verify stops on a changed tree and will not resume over one'
type: 'chore'
created: '2026-11-25'
status: 'done'
route: 'dispatch'
baseline_commit: '57016e041ad690d4b57bd71629155dfeca517e8b'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-cleanup-3-verification-guards.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** `docs/implementation/deferred-work.md` entry C (about line 88). A break runner killed at `cmake_verify.py`'s timeout can leave its mutant in the tree; in p13 that mutant fed every later SIM1 line. Today:
- **`--in-place` has no tree check:** a mutant left by one line reaches every later line.
- **Slots** reset after every line from a template, and their check (`tree_hash` over `cmake docs include src tools`, untracked files included) fails the line. But:
  - top-level source files such as `CMakeLists.txt` are only in the template-cache manifest, not in the per-line check;
  - a write through a source symlink that leads back to the start tree escapes the reset.
- **Recording a stop needs a resume rule.** A resume skips every recorded line. Once a stopped line is recorded, a naive resume would skip it over a dirty tree.
- **`--self-test` raises `IndexError`:** the argv parsing at `:45` runs before the asserts.

Already done, and kept: SIGTERM, up to 60 s of grace, then SIGKILL to the line's process group (`run_line`, `:340-367`).

**Approach:**
- One protected inventory, used by both modes.
- An in-flight guard marker, written atomically before each in-place line and cleared only after a clean check, so a crash or interrupt anywhere leaves the run blocked until the tree is shown clean.
- In slots, the start tree is also checked after every line.

## Boundaries & Constraints

**Always:**
- **The inventory:** `protected_hash(base, skip)` hashes:
  - `SRC_DIRS` (`tree_hash`, as today: content, symlink targets, untracked files, no `__pycache__`);
  - every top-level regular file and symlink of `base`.

  `skip` holds the verifier's own paths: the results directory when it lies inside `base`, a slot's `READY` and `.slot-manifest.json`, and any `*.ACTIVE`.

  Not protected, as run outputs, fixtures or too large: `bin/`, `build*/`, `game/`, `orig/`, `cd/`, `wineprefix/`, `.git/`. The notes say so. The slot preparation check (`:329`), the template inventory (`tmpl_cache`) and the per-line slot check all use this one function.
- **`--in-place`:**
  1. **Before each line:** compute `protected_hash(root)`, then atomically write `<results>/GUARD.json` (temp file and `os.replace`). It holds the state `running`, the line's `n` and command, the Baselines list hash, and the before inventory.
  2. **After the line** (every exit, timeout included), quiesce, then check:
     - **Quiesce.** Wait for the line's session to be empty: the processes whose `/proc/<pid>/stat` session id is the line's (`start_new_session` makes it the child's pid). That catches descendants that moved to their own process group, such as nested `timeout`.
       - Survivors get SIGKILL. If any remain 10 s later, the tree is unverifiable: the marker becomes `stopped` with that reason, and the run stops.
       - A process that called `setsid` itself is not tracked; the notes say so.
     - **Check.** Compute `protected_hash(root)` again.
       - The same: the row is recorded, then the marker is deleted.
       - Different: the row is FAIL, with `tree_changed` and `reason`. The marker becomes `stopped` with the changed paths. No further line starts, the report is written, and the run exits non-zero: `stopped: line N changed the tree: <paths>`.
- **Atomic records:** rows and `meta.json` are written to a temp file, then `os.replace`d. A record that does not parse, or a `.json` without its `.log`, counts as not recorded. On load it is moved to `earlier/` with a `.partial` suffix, never crashing resume or the report.
- **On every start**, in either mode, before any file moves:
  - **When `GUARD.json` exists:**
    - its list hash must equal the current one. Otherwise the run refuses: `the Baselines list changed since line N stopped; use a new results directory, or --clear-stop`;
    - `protected_hash(root)` must equal its before inventory. Otherwise the run refuses and lists the differing paths: `restore them, or pass --clear-stop after a deliberate edit`;
    - if both match, line N's record and log move to `earlier/`, and only then is the marker deleted. The run goes on, and line N runs when the selection includes it. If it does not, line N stays unrecorded, so a later run picks it up. The tree is known clean either way.
  - **`--clear-stop`** moves line N's record to `earlier/` and deletes the marker, without comparing. It is for a deliberate change, for example to fix the cause.
  - **`--lines` and `--only`** neither bypass nor clear the marker.
  - The protection covers reuse of the same results directory. A new results directory starts clean, and the docs say so.
- **Slots:**
  - Each line's slot check uses `protected_hash(slot)` against the template's. A change is FAIL, as today, and the run goes on: the reset contains it.
  - After the reset, the slot is checked against the template again. A mismatch stops the queue, the same way a cleanup failure does today (`stop.set()`; running lines finish, nothing new starts, the run exits `stopped:`).
  - A reused template is checked the same way before its first line.
  - **The start tree is checked too:** `protected_hash(root)` is taken once before dispatch and again after every line. A change, for example a write through a symlink, stops the queue the same way. `STOPPED`-state `GUARD.json` is written with the start-tree paths, and it blocks the next start as in place.
- **`UPDATE`:** each line's environment drops an inherited `UPDATE`, unless the line sets it itself. `data-images` with `UPDATE=1` rewrites `tools/data_images.txt`; a line that does so on purpose would hit the guard, and the docs say verification runs with it unset.
- **`--self-test`:** it runs every module-level assert plus new ones and exits 0, with no other argv. The argv and Baselines parsing (`:44-63`) moves below the asserts or behind the flag. The new asserts:
  - `protected_hash` on a temporary tree: top-level, `src/` and untracked changes are seen; `bin/`, results and slot metadata changes are not;
  - the start decision: refuse on a dirty tree, refuse on a changed list, clean-and-continue, `--clear-stop`;
  - partial records;
  - the session scan finds a child in its own process group.
- **The in-place proof** (in the notes): synthetic `--baselines` files in a scratch copy of the tree, `--in-place`, no build. Mutators are one-shot: they check a sentinel under `bin/`, so a resumed run can finish.
  - Edits to a `src/` file, to top-level `CMakeLists.txt`, and a new untracked file under `tools/` each stop the run: the next line does not start, the exit is non-zero, and `GUARD.json` is right.
  - A restart without restoring refuses and lists the path. After restoring, the stopped line runs again, then the rest. `--clear-stop` after a deliberate edit goes on. `--lines` on a stopped directory refuses.
  - Changing the list after a stop refuses.
  - A process killed mid-line (SIGKILL to `cmake_verify` itself) leaves `running`, and the next start blocks on a dirty tree and continues on a clean one.
  - Timeouts (small `--factor`): a clean one passes the check; a writer that ignores TERM and edits after it is caught after SIGKILL; a TERM-ignoring child in its own process group, under a nested `timeout`, is found by the session scan.
  - A truncated record is moved aside, and the line re-runs.
  - A clean list's rows and report match a run of the unchanged tool, after normalising timing fields.
- **The slot proof:** from a `cp -a` copy of the worktree with `.git` removed (slot setup refuses a `.git` file). One slot, `-j 1`, a three-line list:
  1. clean;
  2. writes the slot's `CMakeLists.txt`: FAIL with `tree_changed`;
  3. clean, and asserts that line 2's write is gone.

  It completes. A second run with `--lines` reuses the template without a build. A line that writes through a symlink into the start tree stops the queue.
- **Docs and paperwork:**
  - the module docstring of `cmake_verify.py`, and its notes in `docs/tools.md`;
  - `deferred-work.md` entry C moves to `deferred-work-resolved.md`;
  - the parallel-verification entry (about line 68) is rewritten to what is left: the runners restore on SIGTERM, and each runner mutates its own copy, since verifier slots and `-j` exist;
  - a new entry for the break-verdict classification: `break_verdict` selects by `'breaks' in cmd`. It should classify parsed target names and direct runner invocations, with a test that an unrelated command containing `breaks` is not one.
- **Running:** in a worktree `~/source/OpenFS98-cl3b` off HEAD, with fixtures copied in for the slot proof's copy. Builds use `nice -n 19` and `-j 2`; the slot proof's one slot build is the only build. The wine prefix copy is about 2.8 GB. No wine runs.

**Never:**
- No change to any break runner, `tools/slots.py`, `break_pool.py` or the verdict rules. Runner work waits for the parallel-verification entry and for slice 6b0, which edits the runners.
- No change to a Baselines line.
- No commits or staging.

</frozen-after-approval>

## Code Map

- **`tools/cmake_verify.py`:**
  - `:44-63`: argv and Baselines parsing at import, which causes the `--self-test` `IndexError`;
  - `:127-205`: the module-level asserts;
  - `:180`, `:217-233`: `SRC_DIRS`, `tree_hash`, `tree_diff`;
  - `:288-337`: `prepare_slot`, including the `.git` refusal at `:301`, the check at `:329`, and `READY`/the manifest at `:333-336`;
  - `:340`: `run_line`;
  - `:384-417`: `meta.json`, resume and `--lines`;
  - `:450`: `record`;
  - `:459-461`: the `--in-place` loop;
  - `:483-566`: `setup`, `reset` and `worker`, with the existing stop at `:548-550`, `:559-562`;
  - `:578-592`: the report and the stopped exit.
- **`tools/slots.py:46`:** `owned_procs`/`cleanup` (read only, for the slot side).
- **Entry C:** `docs/implementation/deferred-work.md:87-90`, plus the parallel entry at `:68`.
- **Evidence:**
  - `~/openfs98-verify/p14-results/*.json`: 166 slot records, `tree_changed` empty in all, under the old inventory;
  - `cmake/targets.cmake:539` and `tools/check_data_image.py:994`: `UPDATE`.

## Tasks & Acceptance

**Execution:**
- [ ] The worktree; `protected_hash`; the in-place guard, quiesce and marker; atomic records; the start decision and `--clear-stop`; slots on the new inventory with the start-tree check; `UPDATE`; `--self-test`.
- [ ] The in-place proofs and the slot proof.
- [ ] Docs, the deferred-work moves and the new entry, `## Implementation Notes`.

**Acceptance Criteria:**
- `python3 -B tools/cmake_verify.py --self-test` exits 0.
- Every proof case behaves as listed.
- A clean list's normalised rows and report are unchanged.
- Only `tools/cmake_verify.py`, `docs/tools.md`, `deferred-work.md`, `deferred-work-resolved.md` and this spec changed.

## Verification

**Commands:**
- `python3 -B tools/cmake_verify.py --self-test` -- expected: exit 0.

## Review Triage Log

Spec review (Codex gpt-6.2-sol, 2026-11-25), on the first draft:

| # | Finding | Verdict | Action |
|---|---|---|---|
| 1 | The slot proof cannot run from a linked worktree (`.git` file refused, `:301`) | blocker | The slot proof runs from a copy with `.git` removed. |
| 2 | Stop and resume must be crash-safe; partial records | high | An in-flight marker written before launch; atomic rows and meta; partial records moved aside. |
| 3 | `--lines`, `--only` and `meta.json` against the marker | high | The marker is bound to the list hash; both are checked before any move. The stopped line stays unrecorded if not selected (the tree is clean). Other-list reuse via `--lines` is pre-existing and left alone. |
| 4 | Slot inventories: `READY` and the manifest; reused slots; symlink escape | high | Verifier metadata skipped; checks after reset and before reuse; the start tree checked after each line, stopping the queue. |
| 5 | A timeout hash needs a quiescent tree; nested `timeout` changes the process group | high | A session-id scan, SIGKILL, and an unverifiable state that stops. TERM-ignoring and nested cases in the proof. |
| 6 | `UPDATE=1` writes `tools/data_images.txt`; a nested results directory | medium | `UPDATE` dropped unless the line sets it; the results directory skipped. |
| 7 | The break-verdict classification is still open | medium | A new deferred entry; the parallel entry rewritten. |
| 8 | Proof strength: restoration in a slot, template reuse, one-shot mutators, more cases | medium | All added. |
| 9 | Current-state wording | low | Corrected in Intent. |

## Implementation Notes

Done in the worktree `~/source/OpenFS98-cl3b` (changed: `tools/cmake_verify.py`, `docs/tools.md`, `deferred-work.md`, `deferred-work-resolved.md`, this spec). Nothing staged.

- **Code.** `protected_hash` (SRC_DIRS plus top-level files and symlinks, `skip` relative or absolute) used by the slot preparation check, the template inventory and reuse check, the per-line slot check, the after-reset check and the start-tree check. In place: `GUARD.json` before each line, `quiesce` (session scan over `/proc/*/stat`, 2 s grace, SIGKILL, 10 s) after it, row then marker. `guard_start` runs on every start before any move; `--clear-stop`; atomic rows and meta; `.partial` records; `UPDATE` dropped. A reused template that no longer equals the start tree is re-prepared (a build), not stopped. Argv and Baselines parsing moved below the asserts and the `--self-test` exit; `plan()` no longer defaults `build`.
- **Choices.** The slot comparison ignores the top-level `cd` key (a symlink in the slot, a directory in the start tree). A slot that differs from its template after a reset leaves its `.ACTIVE` file so the next start resets it, and its row is not recorded (as for a cleanup failure). The unverifiable path (a survivor of SIGKILL) could not be provoked and is untested. `GUARD.json` is written for slots only when stopped, as specced.
- **Verification.** `python3 -B tools/cmake_verify.py --self-test` exits 0. In-place proof (scratch copy, no build, `~/.../scratchpad/p/proof.py`): edits to `src/fe/fe_library.c`, `CMakeLists.txt` and a new `tools/zz_new.txt` each stop the run (line 3 not started, non-zero exit, FAIL row with `tree_changed`, `GUARD.json` stopped with the path, report written); a restart without restoring refuses naming the path; after restoring the stopped line and the rest run; `--clear-stop` goes on; `--lines` on a stopped directory refuses (dirty) and, on a clean one, clears the marker and leaves line N unrecorded; a changed list refuses (also with `--lines`); SIGKILL of `cmake_verify` mid-line leaves `running`, blocks on a dirty tree and continues on a clean one; timeouts: a clean one passes, a TERM-ignoring writer is caught after the 60 s grace and SIGKILL, a TERM-ignoring writer under a nested `timeout` (own process group) is killed by the session scan; a truncated record and a `.json` without `.log` are moved to `earlier/*.partial` and re-run; rows, report and file set of a clean list equal the unchanged tool's (timing normalised). One draft proof line `echo 100 -> done` wrote a file `done` in the tree through shell redirection and was caught by the guard as FAIL; it was quoted for the comparison. Slot proof (`cp -a` copy without `.git`, one slot, `-j 1`, 3 lines): line 2 writing `CMakeLists.txt` is FAIL with `tree_changed: CMakeLists.txt`, line 3 sees it gone, the run completes (first run 4 min 31 s including the slot build at `nice -n 19`, `CMAKE_BUILD_PARALLEL_LEVEL=2`); `--lines 0` reuses the template (2.4 s, no build, setup 0 s); a line writing through `tools/evil_link` into `src/zz_evil.c` stops the queue (`stopped: line 1 changed the tree: src/zz_evil.c`, stopped `GUARD.json`, line 2 not run), and the next start refuses until the file is removed.
- **Not run.** No Baselines-list run through the full `docs/tools.md`; no wine. The break runners are untouched.

**Review patch (Codex findings 1-13, 2026-11-25).** In `tools/cmake_verify.py`: a `Guard` object (state, covered `lines`, `sids` as pid/launch time/line, list hash, inventory) written atomically before each line, right after the launch (`run_line`'s `on_start`) and before a row is published; the start empties each recorded session (`quiesce`, with a launch-time floor so a reused pid after a reboot is not killed) before the tree is compared or `--clear-stop` honoured, and refuses if it cannot. Slots write a guard before dispatch and delete it only after the final start-tree check; every worker exception checks the start tree and leaves it stopped; the check, guard update and row run under the queue lock, and the guard lists every affected line. `flock` on `<results>/.lock`. `tree_hash` uses `lstat`, records non-regular entries as `special:<mode>`, and `os.walk` errors raise. `/proc` errors other than a vanished process count as live; the post-SIGKILL wait is bounded (30 s); the session is emptied before the log and verdict are read, and in slots before `slots.cleanup()`. `reset()` compares the slot with the template before removing `.ACTIVE`; a reused working slot is compared before its first dispatch and reset when it differs. `meta.json` and the list are checked before the guard is consumed (`veto`), so a refused `--clear-stop` changes nothing; rows are schema-checked, a non-object `meta.json` and an unreadable guard fail closed. Results may not be the start tree or hold a protected directory. `cd` is checked as a symlink to `<root>/cd` instead of being dropped. Finding 12: the `OFS_BUILD` keyword clash (`dict(..., **env, OFS_BUILD=b)` raising when a line sets `OFS_BUILD`) came from this change's first draft, not from the original; fixed with `{**env_base, **env, 'OFS_BUILD': b}` and proved with a line `make nonexistent_target OFS_BUILD=zzz`.

**Review proofs** (commands, exit statuses and logs under `~/openfs98-verify/cl3b/`: `inplace/proof.py`, `proof2.py`, `proof.log`, `proof2.log`, `commands.log`, `commands2.log`; `slots/slotproof.py`, `commands.log`, `slotproof2.log`, `R1.setup.log`, `R2.slot0.setup.log`, `R2.slot1.setup.log`). Every launch is shown by a sentinel file (`sent/<name>.started`), not by the absence of a row. In place, all pass: stops on a `src/` edit, `CMakeLists.txt`, a new `tools/` file and a new FIFO under `src/` (guard `stopped`, `lines [1]`, a recorded session); refuse without restoring, resume after; `--lines`, `--clear-stop`, changed list (also `--clear-stop` on a changed list refuses and leaves the guard and records untouched); a pre-existing FIFO does not hang; live orphan (verifier SIGKILLed while its child sleeps, guard holds the session, the restart empties it, `src/zz_orph.c` never appears, also not after the sleep would have ended); SIGKILL mid-line, dirty and clean; timeouts: clean, a writer that writes only after TERM (caught after the 60 s grace and SIGKILL), a TERM-ignoring writer under a nested `timeout` in its own process group (session scan); a second verifier on the same results directory is refused; results equal to the start tree, `src/` and the tree's parent refused; a line setting `OFS_BUILD`; truncated and wrong-schema records, a record without its log, a non-object `meta.json`; a clean list's rows, report and file set equal the unchanged tool's. Slots (`cp -a` copy without `.git`): a line writing through `tools/evil_link` into the start tree and poisoning its own template stops on the reset mismatch with a stopped guard naming `src/zz_evil.c` and line 1, line 2 never launched, a restart refuses while the file exists, resumes once it and the template are restored (template reused, no build); a reused working slot with a mutant `CMakeLists.txt` is reset before its first line; with `-j 2` two lines writing through the symlink are both recorded FAIL and the guard lists both. Not provoked: the unverifiable path (survivor of SIGKILL, unreadable `/proc` entry) is covered only by the unit asserts that exercise `session_pids` and `quiesce`.

**Accepted limits (rejected review findings):** pid reuse between scan and kill; a zombie thread-group leader with live threads; fsync and power loss, and stale `.tmp` symlinks in the verifier-owned results directory; symlinked slot roots inside the verifier-owned slots directory; nested `*.ACTIVE` under `src/` is hashed (it is a source file); a root-level `READY`; unique archive ids and orphan logs; `--lines` reuse across lists (left alone by spec triage 3).

**Final patch round (follow-up review 1-7).** (1) Every line runs as `read -r _ <&G || exit 1; exec G<&-; <command>` (G a free single-digit fd: dash cannot close fd 10 and up) with the pipe's write end held by the verifier, which writes the byte only after `guard.started()` has published the session; `BrokenPipeError` on the write is ignored (a syntax error ends the shell before the gate). (2) A guard session is `[pid, boot_id, leader starttime, line]`: another boot means the session is gone; the leader's pid in use with another starttime means it was reused (not ours); otherwise every process with that session id counts, which identifies members once the leader is gone (a new session cannot take the id while members remain). (3) `session_pids` returns the unreadable `/proc` entries apart; `quiesce` returns False at once if any exist and never signals them; a start refuses, a line's check stops. (4) A `tainted` event: once a start-tree change is recorded, a line finishing later is marked FAIL, recorded, and left in the guard (not `done`), so resume requeues it. (5) `.ACTIVE`, `guard.begin()` and the rest of the dispatch run inside the worker's try/except/finally, and the thread body is itself wrapped, so any error stops and is reported. (6) The slot guard is written before setup or any recovery reset; a setup failure checks the start tree and leaves a stopped guard if it changed (else removes it). (7) Rows: list fields must be lists of strings, `slot` an int or null, numbers numeric; guard: `before` string maps, sessions `[int, str, int, int]`, `tree_changed` strings; an invalid guard refuses, and `--clear-stop` first empties the sessions it can identify, then removes it. Proofs (logs under `~/openfs98-verify/cl3b/round3/`): the self-test covers 2, 3 (a mocked `PermissionError` entry), 7 and a gate whose `on_start` fails; `inplace/proof.py` is the earlier in-place suite plus the gate case (the verifier SIGKILLs itself between `Popen` and the release through `inject.py die_after_popen`: guard lines `[1]`, no sessions, and the line's sentinel never appears, even after its sleep would have ended); `slots/slotproof.py` holds the symlink stop, the -j 2 case (a second line repairs the start tree before its own check and is still kept in the guard and marked FAIL), an `OSError` injected into `guard.begin` (exit non-zero, line never launched), a setup that writes the start tree then fails (stopped guard naming the file), and a reused slot with a mutant file; the clean-list equivalence is `inplace/proof_eq.py`. Not provoked: no real `/proc` entry was unreadable (mocked only).

Code review (Codex gpt-6.2-sol, three layers, 2026-11-25):

| # | Finding | Verdict | Action |
|---|---|---|---|
| 1 | Running guard has no process scope; an orphan writes after resume (reproduced) | blocker | Session id in the guard; quiesced on start. |
| 2 | Slot reset/cleanup failure skips the start-tree check; no guard (reproduced) | blocker | Slots persist a guard; start tree checked on every stop path. |
| 3 | -j N guard races; only one affected line requeued | high | Check under the queue lock; the guard lists every line. |
| 4 | Two verifiers on one results dir | high | flock. |
| 5 | FIFOs hang the hash; unreadable subtrees skipped | high | lstat, special types recorded, onerror raises. |
| 6 | /proc errors read as exits; unbounded wait; verdict before quiesce; slots ignore the session | high | Fail closed; bounded wait; quiesce first; slots quiesce the session. |
| 7 | Reset/reuse unverified (reproduced) | high | Verified reset; reused slot checked before dispatch. |
| 8 | --clear-stop consumed before list refusal (reproduced) | medium | Meta validated first. |
| 9 | Malformed rows/meta/guard crash | medium | Schema checks; partial/fail closed. |
| 10 | Results dir equal to root or a protected dir | medium | Refused. |
| 11 | `cd` key dropped | medium | Explicit link check. |
| 12 | OFS_BUILD keyword clash | medium | Checked/fixed. |
| 13 | Proof weaknesses | medium | Re-proved with live orphan, post-TERM writer, sentinels, slot cases; logs kept. |
| 14 | pid reuse, zombie leaders, fsync, stale tmp links, symlinked slot roots, nested .ACTIVE, root READY, archive IDs, --lines across lists | rejected | Verifier-owned paths or tiny windows; noted as limits. |

Follow-up review (Codex gpt-6.2-sol, 2026-11-25):

| # | Finding | Verdict | Action |
|---|---|---|---|
| 1 | Popen→guard gap lets an orphan outlive a restart (reproduced) | high | Pipe gate released after the guard is published. |
| 2 | Wall-clock launch floor misidentifies sessions | high | boot_id plus the leader's starttime. |
| 3 | Unreadable /proc entries were SIGKILLed | high | Kept apart; fail closed. |
| 4 | Parallel stop kept a contaminated in-flight row (reproduced) | high | Every in-flight line requeued. |
| 5 | Dispatch error exited 0 with no rows (reproduced) | high | Inside the worker's handling. |
| 6 | Slot setup could dirty the start tree without a guard | medium | Guard before setup; checked on failure. |
| 7 | Row and guard types unchecked | medium | Typed validation. |
