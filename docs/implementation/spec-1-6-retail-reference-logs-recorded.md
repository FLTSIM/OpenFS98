---
title: 'Story 1.6: Retail reference logs recorded'
type: 'feature'
created: '2026-11-24'
status: 'done'
route: 'dispatch'
baseline_commit: '7c354a84c061559cd1341bcf19f2d8d74fbbc558' # worktree also holds the uncommitted Story 1.5 slices 8-10 and the 1.4 close-out
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/native-port/architecture/ARCHITECTURE-SPINE.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Native builds (Epic 3) need a fixed record of what retail does. Today every frame comparison reruns retail beside ours and keeps no retail logs, so the baseline moves with each run (AD-8).

**Approach:** Add `tools/record_references.sh`, run as `tools/ofs references`.
- It stages a pure retail game from the CD.
- It records every case twice through the existing `tools/hg2d_frames.sh`.
- It publishes the logs to `bin/references/` and writes a committed manifest of commit, CD pressing, config and checksums.
- `docs/tools.md` gets a section that documents the FRAMELOCK log grammar.

## Boundaries & Constraints

**Always:**
- **Cases.** Every run is without sound, on the software renderer, for 900 passes. Sound is left out because a crash then waits on its sound in real time and does not repeat (`open-differences.md:25-31`).
  - `flight/<NAME>`: every `.STN` in the staged game's `Situatio/` only (other directories hold more), nobody flying. There must be exactly 74, or the recorder stops.
  - `script/<scenario>/<NAME>`: the seven scenarios `controls`, `systems`, `views`, `avionics`, `slew`, `toggles` and `keys` (`tools/fly_scripts.py` also writes `models` and `crash_models`, which are not used), on `PARIS`, `STGEORGE`, `KAUAI`, `NEWYORK`, `LEARLON`, `HONGKONG`, `BELLWTC` and `CVINSON` (`open-differences.md:15`).
  - `global/STGEORGE`: nobody flying, with `FRAMELOCK_GLOBAL` writing one `0x12a8`-byte GLOBAL record per pass. Record `n` is written as pass `n` begins, from 0 to 899 (`framelock.c:532`). Each dump must be exactly 4,298,400 bytes.
    - The case must be `match`, with both logs and both dumps identical and of exact size. Anything else, a NONDETERMINISTIC pair included, fails the recording: stop and report, and don't choose another flight.
    - The raw bytes are kept as they are. Masking relocated dwords and handles is Story 3.6's job.
- **The staged game.**
  - It is built fresh, per invocation, with `tools/stage_game.py --cd cd --out <work>/game`, which installs from the CD with Setup's own renames, the regional scenery, and the CD's `mfc42.dll` and `MSVCRT.DLL`.
  - Add `game/FLTSIM98.CFG` and `devices.cfg`. Stop unless `[HARDWARE] Device=` selects the software renderer. Copy both files into `bin/references/config/`.
  - **MSVCRT (user's decision, revised after the probe).** Retail runs on wine's builtin `msvcrt`, as every existing comparison has.
    - The probe showed the CD's 1997 `MSVCRT.DLL` cannot be used. Under `msvcrt=n`, wine's own `advapi32`, `user32` and `gdi32` bind to it and fail, so the game never starts. Under `msvcrt=n,b`, retail hung with no frames.
    - `WINEDLLOVERRIDES` is exactly `dsound=d`.
    - Every run uses `+loaddll`, and both runs of every case must show that `msvcrt.dll` loaded as builtin and that FRAMELOCK and SEHFIX loaded.
    - The manifest config and the grammar section state the consequence: host `+0x21c`'s `memmove` leaves 0 in ecx, not the CD runtime's 7. Epic 3 must allow for this at ANA-1, ANALYSIS's trail compaction (`docs/retail-bugs.md:307`).
  - Then check it: every file whose first bytes are `MZ`, whatever its name, must be byte-identical to some file under `cd/`. `orig/FLTSIM98.EXE` and the staged EXE must both match `REFERENCE_SHA256` (`tools/gen_host_rt_tables.py:60`), the pressing FRAMELOCK's addresses assume.
  - Anything else stops the recording, before wine starts. `FRAMELOCK.DLL` and `SEHFIX.DLL` are added per run by `hg2d_frames.sh` and are hashed into the config.
- **`hg2d_frames.sh` switches.** Without them, its behaviour is unchanged.
  - `HG2D_FRAMES_RETAIL_ONLY=1`:
    - it runs retail-a and retail-b only, from `FS98_DIR` as given, and copies no module over it;
    - both runs use SEHFIX;
    - it skips the `$ours` check and every option that copies a file in. The recorder unsets every other `HG2D_FRAMES_*` and every `FRAMELOCK_*`, and sets only what each run needs.
    - In the private prefix copy, it checks that the copy succeeded and removes the `drive_c/FS98` link, so nothing can reach the live `game/`.
    - It does not judge. Each run writes `<tag>/status`: its exit code, and whether its log ends with exactly `end <passes>`. It exits 0 once both runs have finished, and the recorder classifies the pair.
  - `HG2D_FRAMES_GLOBAL=1` writes `<tag>/global.bin`.
- **Outcomes per case.**
  - The checks run in order:
    1. completion: the saved exit code is 0, and a fresh check of the log's last line gives exactly `end 00000384`;
    2. frames: each run has at least 20 distinct pixel hashes, from blit lines with a nonzero DIB byte count (`framelock.c:160-191`);
    3. dump size: an exact match, neither short nor long.

    Any failure here is FAILED, before the logs are compared.
  - `match`: the checks pass and the logs (and dumps) are identical.
  - `NONDETERMINISTIC`: the runs are complete but differ; both runs' files are kept.
  - `FAILED`: a failed check or a missing script. A FAILED case makes the recording fail.
  - **Script delivery.** The script file must exist, and every event's pass must lie within 1 to 899: FRAMELOCK counts the pass before dispatching, so an event at 0 first runs at pass 1. As a check that it reached the sim, a scripted case is compared with its unscripted `flight/` case, which the recorder adds to any subset that lacks it.
    - If both are `match` and the logs are identical, the case is FAILED.
    - If either pair is NONDETERMINISTIC, a separate manifest field records `delivery unchecked`, and the case keeps its outcome.
  - **Case names.** Case ids use the canonical uppercase flight name, and the launch uses the staged file's actual name (the CD has `StGeorge.STN` and `paris.STN`). A flight needed by several cases is recorded once, so the full run is exactly 131 cases.
- **The manifest** is `tools/references.txt`, which starts with a format line `references v1`. Its header records:
  - the full commit id, and `+dirty` if `git status --porcelain` lists any tracked change;
  - the pressing: the EXE's sha256, plus one sha256 over the sorted `path sha256` lines of every staged PE, and another over every other staged file (flights, scenery and data);
  - the config: sha256 of the `.CFG` files, `FRAMELOCK.DLL`, `SEHFIX.DLL` and the script files; the output of `wine --version`; and the effective `WINEDLLOVERRIDES`.

  Then one line per case: name, outcome, flight, script sha256 or `-`, passes, and the relative path and sha256 of every kept file.
- **Safety.**
  - All work goes in a fresh `mktemp -d` under `bin/references-work/`, and every `HG2D_FRAMES_OUT` is inside it.
  - Each job's Xvfb display must be free before use. The recorder confirms that its own Xvfb process is the one answering.
  - Publishing happens only after every case is match or NONDETERMINISTIC (`global/STGEORGE` match), and it holds a `flock` on `bin/references.lock` throughout.
    - `bin/references/manifest.txt` is authoritative, and `tools/references.txt` is its mirror.
    - First the candidate and its manifest are verified in `bin/references.new/`, and then `bin/references.publishing` is written. After that:
      1. `bin/references` becomes `.old`, and `.new` becomes `bin/references`;
      2. the mirror is written through a temporary file and `mv`, and checked equal to the manifest;
      3. only then are `.old` and the `.publishing` marker removed.
    - If the marker, `.old` or `.new` exists at the start, an earlier publish was interrupted: the recorder refuses and names them.
    - A failure before the renames leaves the old pair untouched. A non-empty `bin/references/` needs `REFERENCES_FORCE=1`.
  - Subset runs (case arguments) publish to `REFERENCES_OUT` and write their manifest there, never to `tools/references.txt`.
    - `REFERENCES_OUT` must not exist yet, and its resolved path must be outside `bin/references*`, `game/`, `cd/`, `orig/` and `src/`, and must not contain them.
  - `REFERENCES_JOBS` is 4 by default.
- **When the full recording runs (user's decision).**
  - The implementer verifies the tool on subsets only.
  - After the user commits 1.6, the full recording runs from a frozen `cp -a` copy of the tree, which includes `cd/`, `game/`, `orig/` and `bin/`, so the manifest has no `+dirty`. The copy gets a freshly configured build directory, with `OFS_BUILD` pointing at it, and its `CMakeCache.txt` must name the copy as its source root.
  - Work can continue in the live tree meanwhile, but no other wine verification. Then copy back into the live tree with the same marker-and-rename sequence, under the live tree's lock, and check every hash against the manifest. 1.6 is done when the user has committed the copied-back `tools/references.txt`.
- **The grammar section** documents every line kind in `frames.log` with its fields (blit, `end`, `fault`, `sim1`), and `global.bin`'s record layout. It says what v1 compares: all of `frames.log`, byte for byte, as `hg2d_frames.sh` does, and the GLOBAL dump. `regs.txt` is documented on its own, as a separate file that is not compared. The grammar is `references v1`, and it is documented as it is now (the user's decision); `framelock.c` is not changed. Its limits are written down:
  - there is no window identity;
  - blits are logged in call order, as they are made;
  - `fault` lines hold retail-specific addresses;
  - GLOBAL record `n` is the state before pass `n`, so a native run's state after pass `n` lines up with record `n+1`, through pass 898. The last hook writes record 899 and exits without running pass 899's loop;
  - script events are dispatched after the pass is counted (event pass `p` runs as pass `p` begins, `p >= 1`).

  Epic 3's FRAMELOCK in `fs98win32` defines how it maps onto v1 (AD-8).
- Builds run under `nice -n 19`, with `-j 2`. No staging or commits, and no changing this spec's status.

**Never:**
- No change to `tools/drive/framelock.c`, to module sources or to `game/`, and no reference log in git.
- The full command is not added to the Baselines block (`tools/cmake_verify.py:57` would run it). It goes in a manual note beside it.
- No `wineserver -k` without an explicit private `WINEPREFIX`.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|----------|--------------|---------------------------|----------------|
| Record | full run | 131 cases published, manifest written | exit 0 |
| Not retail | `REFERENCES_TEST_PLANT=bin/MULTIP.DLL` copies a PE not on the CD into the stage, after staging and before the check | names it, no wine run | exit 1 |
| Wrong pressing | `REFERENCES_TEST_EXE=cd/fs98/fltsim95.exe` (a CD PE, so it passes the PE check) replaces the staged EXE | stops before wine, naming the pressing | exit 1 |
| Existing | in a disposable copy, a prepared non-empty `bin/references/`, no force | refuses, nothing touched | exit 1 |
| Nondeterministic | `REFERENCES_TEST_SKEW=1`, subset only: changes one blit hash in run b's log after it returns | both runs kept, listed | exit 0 |
| Failed | `REFERENCES_TEST_TRUNCATE=1`, subset only: removes run b's last line after it returns | FAILED named, nothing published, old set kept | exit 1 |
| Interrupted publish | in a disposable copy of the tree, a prepared `bin/references.publishing` | refuses, names it | exit 1 |
| Dirty | in a disposable copy, a tracked file edited | `+dirty` in the header | — |

</frozen-after-approval>

## Code Map

- `tools/stage_game.py`:
  - `--cd`/`--out` build an install from `cd/fs98`, with the renames (`:69-83`), the runtime DLLs (`:90-93`) and the scenery (`:143-145`);
  - it runs `rmtree` on `--out` (`:65-66`), so only ever pass it a path inside the work directory.
- `tools/hg2d_frames.sh`:
  - the prerequisites at `:88`; `rm -rf "$out"` at `:93`; the prefix copy at `:102`; Xvfb from `:104`; NOSOUND at `:119`, which replaces `WINEDLLOVERRIDES`;
  - `run()` at `:121`, the module overlay at `:124-138`, the HG2D copy at `:139`, FRAMELOCK and SEHFIX at `:179-182`, the completion check at `:205-214`, `wineserver -k` at `:257`;
  - the calls at `:267-269`, the verdict from `:272`.
- `tools/frames_many.sh`: displays `:$((80+SLOT))` at `:39`, `xargs --process-slot-var` at `:75`, the FEW FRAMES count at `:55`. It deletes matched logs (`:68`), so it is not reused.
- `tools/drive/framelock.c`:
  - the environment at `:21-43`;
  - line formats: blit `:160-191`, `end` `:468`, `sim1` `:506-519`, `fault` `:623-642`, `regs.txt` `:428-458`;
  - GLOBAL at `:532-538`; the environment reads at `:790-806`; opening the script at `:741-746`.
  - It is built by `tools/ofs framelock`, and SEHFIX by `tools/ofs sehfix` (`cmake/targets.cmake:1743`, `:1749`).
- `tools/fly_scripts.py DIRECTORY` writes every scenario (`:221-225`).
- `cmake/targets.cmake`: `ofs_target(references ...)` beside `sim1native-flights` (`:116`), depending on the FRAMELOCK and SEHFIX targets. `tools/ofs` forwards the environment.
- `tools/slots.py:77`: an existing helper that copies a prefix and checks the result.
- `game/` carries our modules, SIM1, SIMSUI and gauges, so it is used only for its two `.cfg` files.

## Tasks & Acceptance

**Execution:**
- [x] `tools/hg2d_frames.sh`: the two switches.
- [x] `tools/record_references.sh` (new): stage, check, record, classify, publish and write the manifest. Usage in a header comment, in the style of `frames_many.sh`.
- [x] `cmake/targets.cmake`: the `references` target.
- [x] `docs/tools.md`:
  - a tools row;
  - the grammar section;
  - beside the Baselines block, the subset command as an executable line with a fresh `REFERENCES_OUT` (`$(mktemp -d -u)`), and the full one as a manual note;
  - `tools/cmake_verify.py`: add `references` to the solo targets (`:157`), so its wine runs never overlap other game tests.
- [x] Verify (see Verification).
- [x] This spec: an `## Implementation Notes` section.
- [x] After the user's commit, not by the implementer: the full recording from a frozen copy, then the copy-back and the hash check.

**Acceptance Criteria:**
- Given the full recording, then `bin/references/` holds 131 cases (74 + 56 + 1), and `tools/references.txt` lists each one with its outcome and hashes, with no `+dirty`.
- Given the GLOBAL case, then both dumps are 4,298,400 bytes and identical.
- Given the subset recorded twice into separate outputs, then each `match` case has the same hashes in both.
- Given `tools/hg2d_frames.sh 'Situatio\PARIS.STN' 900` without the switches, then it passes as in Baselines.

## Verification

**Commands:**
- `REFERENCES_OUT=<scratch>/a tools/ofs references CASES="global/STGEORGE flight/PARIS script/controls/PARIS"`, then the same into `<scratch>/b` -- expected: exit 0, three `match` cases with equal hashes, a scratch manifest, `tools/references.txt` untouched.
- each error row of the matrix, by running `tools/record_references.sh` directly (`tools/ofs` returns CMake's status) with the switch or prepared state the row names -- expected: the exit code and diagnostic in the matrix.
- `tools/hg2d_frames.sh 'Situatio\PARIS.STN' 900` -- expected: PASS.

## Review Triage Log

Spec review 1 (Codex gpt-6.2-sol), 14 findings, all applied:
1. high: the grammar is documented as v1 with its limits (the user chose this over changing FRAMELOCK).
2. high: the GLOBAL case's size, pass alignment and acceptance; no silent replacement flight.
3. high: the staged game comes fresh from the CD through `stage_game.py`, and PEs are found by signature against `cd/` (FSHLP.DLL, ENGLISH.FLL and the renamed gauges included).
4. high: retail-only mode copies nothing over the verified stage and unsets the inherited options; SEHFIX is on both runs.
5. high: the config is pinned and recorded (CFG files, wine version, overrides, instrumentation hashes; software renderer).
6. high: distinct match, NONDETERMINISTIC and FAILED outcomes; a scripted case must differ from its unscripted flight.
7. high: a fresh mktemp work directory, free displays checked, a private `WINEPREFIX` for any `wineserver -k`.
8. high: publishing is atomic and keeps the old set on failure; subsets go to scratch; the copy-back checks every hash.
9. medium: a versioned manifest with per-file hashes, the pressing over path/hash pairs, `+dirty` from `git status`, and a pressing check before wine starts.
10. medium: the frozen copy includes `cd/`, `game/`, `orig/` and `bin/`; the stage no longer depends on `orig/` MDLs or SIM1.
11. medium: named case selectors; the 74 flights and seven scenarios are pinned.
12. medium: test switches for each error row; the verification goes through `tools/ofs references`.
13. medium: the full run is a manual note, not a Baselines line.
14. low: Code Map anchors corrected.

Spec review 2 (Codex gpt-6.2-sol), 10 findings, all applied:
1. high: wine loaded its builtin msvcrt, not the CD's. The user chose the CD's runtime (`msvcrt=n;dsound=d`), checked by a `+loaddll` trace.
2. high: the frozen copy gets a freshly configured build directory, and its cache must name the copy.
3. high: retail-only mode writes a status per run and doesn't judge; distinct frames are counted from blit lines on both sides.
4. high: every inherited `FRAMELOCK_*` is unset; the prefix copy is checked and its `drive_c/FS98` link removed.
5. high: script delivery is checked against the unscripted case, with a `delivery unchecked` outcome when that case is nondeterministic.
6. high: publishing holds a lock, keeps the manifest inside the set, renames in order, and refuses when an earlier publish was interrupted.
7. medium: flights come from `Situatio/` only, and the pressing has a data digest.
8. medium: deterministic test fixtures (skew, truncate, a PE not on the CD, a replaced EXE) replace the timing-based ones.
9. medium: the grammar covers `global.bin` and its pass alignment, and sets `regs.txt` apart.
10. low: Code Map anchors.

Spec review 3 (Codex gpt-6.2-sol), 8 findings, all applied:
1. high: publishing writes a marker and treats the set's manifest as authoritative, with the mirror checked equal; copy-back uses the same sequence; 1.6 is done when the user commits the mirror.
2. high: a subset destination must be new and lie outside the protected trees.
3. high: the CD MSVCRT override is kept exactly and traced on every run, and a first-task probe stops for the user if wine's own DLLs can't run on it (`__iob_func`).
4. high: the fixtures are injected where they test the intended check (PLANT and EXE before validation, with a CD PE for EXE; SKEW and TRUNCATE on returned logs, classified from fresh checks).
5. high: checks run in order (completion, frames from nonzero-size blits, exact dump size), and the GLOBAL case must match.
6. medium: canonical case names, each flight recorded once (exactly 131), and `delivery unchecked` as its own field.
7. medium: script events within 1-899; the grammar records the dispatch timing and the final record's boundary.
8. medium: prepared-state rows run in a disposable copy, error rows invoke the recorder directly, the Baselines subset gets a fresh destination, and `references` is a solo target.

Implementation probe (2026-11-24): `msvcrt=n` failed to start the game, and `msvcrt=n,b` hung with no frames. The user chose wine's builtin runtime, and the MSVCRT bullet above was revised to match.

## Implementation Notes

- **`tools/hg2d_frames.sh`:** `HG2D_FRAMES_RETAIL_ONLY` and `HG2D_FRAMES_GLOBAL` as specified. Retail-only always sets `WINEDLLOVERRIDES=dsound=d` and `+loaddll`. One addition beyond the spec: `HG2D_FRAMES_KEEP=1` keeps the game copies (a debugging aid, kept since it is harmless; the recorder does not use it). The no-switch run `tools/hg2d_frames.sh 'Situatio\PARIS.STN' 900` still gives PASS.
- **Scenery index (found while verifying, not in the spec):** a fresh stage has no `INDEX_FILE`, and the game's "Building database for new scenery files" step never ends on FRAMELOCK's virtual clock, so every run sat with no frames. The recorder therefore starts the staged game once in a disposable copy on plain wine (own Xvfb and prefix copy, no FRAMELOCK), takes the `INDEX_FILE` it builds (byte-identical to the one in `game/`), puts it in the stage and records its sha256 in the manifest header (`pressing index_sha256`). The PE check and the pressing digests run before this, on the pure stage.
- **PE check:** compares against every PE under `cd/` (not only `cd/fs98/`): the CD's `MSVCRT.DLL` and `mfc42.dll` are in `cd/`'s root.
- **Manifest:** tab-separated `case` lines (name, outcome, flight, script sha256 or `-`, passes, delivery `-`/`checked`/`delivery unchecked`, then `path=sha256` per kept file) after header lines (`pressing ...`, `config ...`, `file config/...`). A matching case keeps `<case>/frames.log` (and `global.bin`); a NONDETERMINISTIC one keeps `a/` and `b/`. `tools/record_references.sh --verify DIR` checks a set.
- **Verification:** the subset `global/STGEORGE flight/PARIS script/controls/PARIS` recorded twice through `tools/ofs references` into separate directories: three `match`, identical case lines and hashes, `tools/references.txt` untouched, both GLOBAL dumps 4,298,400 bytes. Every matrix row run directly in a disposable tree (PLANT, EXE, existing, interrupted, SKEW with an edited tracked file giving `+dirty`, TRUNCATE) gave the stated exit code and diagnostic. The publish path (marker, renames, mirror, `REFERENCES_FORCE`) was exercised in the disposable tree with a copy of the recorder cut down to one case, since the full run is not run here.
- **Not done here:** the full 131-case recording, the copy-back and the hash check (after the user's commit).

Code review 1 (Codex gpt-6.2-sol: blind, edge-case and verification-gap layers; findings deduplicated across layers):
- medium, patch: a subset's cleanup deletes a pre-existing `$REFERENCES_OUT.tmp`, and a failed `rm` falls through the `&& ||` chain to `rm -rf bin/references.new` (blind, edge, gap).
- medium, patch: the publish marker's creation and the `.old`/marker removals are unchecked (blind, edge, gap).
- medium, patch: `--verify` passes a manifest with only its version line; case count, unique ids and required files are not checked (blind, edge, gap).
- medium, patch: a full run doesn't check it has all 131 rows, GLOBAL included, before publishing (edge).
- medium, patch: the Baselines subset line has no parsed expectation, so a NONDETERMINISTIC or missing case reads UNCHECKED (blind, edge, gap).
- medium, patch: no fixture exercises the GLOBAL dump comparison (gap); `REFERENCES_TEST_SKEW=global` added.
- low, patch: `REFERENCES_FORCE` accepts any non-empty value (edge, gap).
- low, patch: the warm-up's Xvfb and wine survive a `die` (blind, edge).
- low, patch: `script_ok` checks only the first token (blind, edge).
- low, patch: `cmake_verify.py`'s solo assertions don't include the real subset command (gap).
- low, patch: the grammar's blit row omits FRAMELOCK's rules (logged before forwarding, `|biHeight|`, `811c9dc5` with no bits) (blind, edge).
- low, patch: the copy-back note doesn't compare the mirror with the manifest (blind).
- low, rejected: the subset `mv` race with a destination created mid-run; unlikely, and a no-replace rename adds complexity (blind, edge).
- false: prefix links reaching the live game. `~/.wine/dosdevices/c:` is the relative `../drive_c`, so a copy maps its own `drive_c`, and `drive_c/FS98` is removed (blind, edge).
- false: a malformed status or log being accepted. Completion is checked from the log's last line, and a malformed blit field raises, so the run exits non-zero and nothing is published (edge).
- low, rejected: `hg2d_frames.sh:225` now clears an inherited `FRAMELOCK_GLOBAL` in normal mode. No caller sets it, and an inherited value would make both runs append to one file (edge).
- false: two subset recordings giving different hashes for the same case. The two subset recordings' case lines and hashes were identical (edge).

Full recording (2026-11-24, from the frozen copy `~/openfs98-verify/p16` of commit `1932b40`, clean, fresh build directory; `bin/` not copied, since the fresh build made FRAMELOCK and SEHFIX): 131 cases, all `match`, with 56 scripted cases' delivery `checked`. The manifest has no `+dirty`. Copied back with the marker-and-rename sequence under the live lock: `--verify` passed 134 files, and `cmp` of the mirror against the manifest is equal. The set is 43 MB. Story 1.6 closes when the user commits `tools/references.txt`.
