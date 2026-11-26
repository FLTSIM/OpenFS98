---
title: 'Story 1.8, slice 6b: FLIGHTS, MPDIALOG and WORLD use arena.h'
type: 'refactor'
created: '2026-11-25'
status: 'done'
baseline_commit: '27a9e496c2495c57b970c4e265054fce528b76e1'
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-1-8f-main-uses-arena-h.md', '{project-root}/OpenFS98/docs/implementation/spec-1-8g-break-runners-share-build-only.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** FLIGHTS, MPDIALOG and WORLD still hold 12 banned `(uint32_t)(uintptr_t)` casts in nine files: the `tools/arena_lint_allowed.txt` rows `src/flights/flights_{aircraft 1, autopilot 1, comms 1, events 1, navigation 1, select 2}.c`, `src/mpdialog/mpdialog_{details 1, flow 3}.c` and `src/world/world_search.c 1`, none with definitions. FLIGHTS also has one reverse cast `(uint8_t *)(uintptr_t)(uint32_t)`, at `flights_select.c:50`.

**Approach:** Convert them to `include/fs98/arena.h` as slices 1–6a did, and delete the nine rows. Prove the change by byte identity with the packaged gate. That includes every mutant of the 28 break modes that slice 6b0 (`27a9e49`) gave `--build-only`: 955 cases and 1,171 PEs.

## Boundaries & Constraints

**Always:**
- **Sources:**
  - **Forward casts:** each of the 12 becomes `fs98_handle(x)`, and its operand keeps its extent. `(slice + fields[index][0])` and each `AT(...)` stay whole.
  - **Reverse cast:** it becomes `(uint8_t *)fs98_ptr(x)`, with the whole `send_message(...)` result as `x`.
  - **Includes and macros:** each of the nine files includes `fs98/arena.h`. No module defines `FS98_DATA_IMAGE`, and no module macro is renamed.
- **Diff audit:** `p18g/audit.py` checks the diff against `27a9e49`. It must contain exactly:
  - the 13 substitutions, with operands unchanged;
  - the nine includes;
  - the nine allowlist rows deleted;
  - the `-Iinclude` additions below;
  - the five anchor repairs, plus the `deferred-work.md` and docs entries.

  Anything else fails it, in particular any change to a runner's verdict or predicate code.
- **Lint:** the reverse count goes from 23 to 22, and the nine rows go, checked with `--prior` on `27a9e49`'s list.
- **`-Iinclude`** goes last on every compile of a changed file, in the same position everywhere, so that shared units replay once:
  - **The six CMake recipes:** `cmake/steps/all.cmake:285` (`WORLD_area.dll`), `:344` (`FLIGHTS_ours.dll`), `:362` (`MP_DIALOG_ours.dll`), and `cmake/steps/difftests.cmake:250` (`flightseventsdiff.exe`), `:742` (`worldsearchdiff.exe`), `:749` (`worldsearchinitdiff.exe`).
  - **The seven runner command sites:** `tools/flights_breaks.py` (`:686`), MPDIALOG's shared builder (`tools/mpdialog_ui_breaks.py:128`), `tools/world_search_breaks.py:59`, `tools/world_search_init_breaks.py:59`, `tools/world_native.py`'s `dll_command()`, and the DLL commands of `tools/world_advanced_native_breaks.py:52` and `tools/world_wareas_breaks.py:114`.
  - **DLL compiles only** in the WORLD shared builders: shared `FLAGS` lists stay unchanged, so harness, core, life and init compiles are untouched.
  - **Trace check:** normalised runner compile traces, before against after, differ only by that trailing flag on those compiles.
- **Anchor repair (FLIGHTS):** five cases are repaired: `fill-reload`, `comms-error-pointer`, `ap-error-pointer`, `radios-error-address` and `radios-error-answer`.
  - Anchors and replacements change only where the converted text appears.
  - For each case, a commutation check shows that the old mutation applied to the old source and then converted gives the same text as the repaired mutation applied to the converted source.
- **The gate:** `compare.py --require-set p18g` re-hashes from the kept files and requires both `bin/` trees.
  - **Ordinary artifacts:** it requires these nine:
    - the installed `FLIGHTS.DLL`, `MP_DIALOG.DLL` and `WORLD.DLL`;
    - the link outputs `flights-diff/FLIGHTS_ours.dll`, `mpdialog-diff/MP_DIALOG_ours.dll` and `world-diff/WORLD_area.dll`;
    - the three difftest exes.
  - **Replays:** the 63 exact replay keys, pinned from the before manifest, each replayed once per side. The only allowed flag change is one trailing `-Iinclude`.
  - **Link invocations:** all six original invocations in each side's `gcc.log`, keyed by `-o` and their ordered input list.
  - **Mutants:** exactly 28 `--mut` pairs named `mut-<mode>`, using the mode names of `p18g0/runners.tsv`. `w_wareas_native` is `world_wareas_breaks.py --native --build-only DIR`.
    - **Frozen inventory:** made right after the before build, in `p18g/cases/<mode>.tsv`: one row per case id, listing its PE basenames and their normalised digests.
    - **Both sides** must hold exactly those case ids and basenames: no missing or extra files, every listed file a valid PE, and 1,171 PEs per side. Each `meta.json` name must be its directory. Every PE must equal the frozen digest on both sides.
    - **Run identity:** each run log records its snapshot root and the snapshot's commit or diff hash. `gate.sh` requires that the before runs name the before root and the after runs the after root.
  - **6c and 6d reuse** this case-inventory support.
  - **Regression:** after the `compare.py` change, p18b to p18f and 4b's `--strict` still pass.
- **Snapshots** go in `~/openfs98-verify/p18g`.
  - **Before** is `git archive 27a9e49` plus the gitignored fixtures (`orig/`, `cd/`, and `game/` where needed). It has no overlay. **After** is the working tree plus the same fixtures.
  - **Builds:** both sides are built at one absolute root by `build_side.sh`, with `p18g/prior.txt`. Each side's 28 build-only runs come from its populated root, with `python3 -B` and DIR outside the root.
  - **Order:** strictly serial, under `nice -n 19`.
  - **`gate.sh`** requires exit 0 from every run, then runs `compare.py`.
  - **If a mutant differs,** that case is rebuilt once on each side. A difference that does not reproduce is reported, not passed.
- **Negative checks** run on `cp -al` copies, with edited files replaced, never written through. Each must exit 1 with `GATE FAIL` and the expected diagnostic; a traceback does not count:
  - `WORLD.DLL` missing on both sides;
  - one changed unit's replay entry removed on both sides;
  - one link invocation removed from both `gcc.log`s;
  - one case removed on both sides;
  - a PE removed from a one-PE case on both sides, and from a two-PE case (`mut-w_ui/baseline`) on both sides;
  - a case renamed, with its `meta.json` updated, to an id that is not frozen;
  - two different DLLs swapped symmetrically;
  - a mode's pair missing;
  - a valid duplicate as a 29th pair.
- **Acceptance:** the 955 mutants must be identical, and any mutant or evidence failure fails the slice. Only an explained content difference in an ordinary artifact can be waived, and only with a recorded user decision. The waiver needs the module docs' baselines for the changed units, run serially from a frozen copy under the wine lock:
  - FLIGHTS normal and unwind;
  - MPDIALOG isolated UI, flow and details;
  - WORLD search core, init and interrupt, and native search UI;
  - `flights-play` and `main-dialogs`.
- **Running:** in a git worktree off `27a9e49`. Implementers copy nothing into the main tree except this spec. Builds use `-j 2` and `CMAKE_BUILD_PARALLEL_LEVEL=2`.

**Never:**
- No other module is converted. `arena.h` and the lint change only to fix a defect found here, and that fix is logged.
- No change to what the modules compute or export, or to any break's mutation, count, predicate or wine argument.
- The plain `(T *)(uintptr_t)x` casts stay: 14 arena pointers (12 in `mpdialog_flow.c`, including `char **` at `:741`, plus one each in `mpdialog_session.c` and `world_search.c`) and two `GetProcAddress` ordinals (`flights_runtime.c:279`, `mpdialog_runtime.c:98`). They are enumerated in `deferred-work.md` for Epic 5, with the ordinals marked.
- Break counts are not re-run under wine.
- No commits or staging.

**Decided (2026-11-26, user):** six FLIGHTS anchors are repaired (select-choose-itemdata too), and the after side may hold the plain replay key of a WORLD unit where a pinned unchanged recipe produces it.

</frozen-after-approval>

## Code Map

- **Cast sites:**
  - FLIGHTS: `flights_comms.c:145`, `flights_autopilot.c:502`, `flights_navigation.c:405`, `flights_events.c:177`, `flights_aircraft.c:118` and `flights_select.c:81,82`, with the reverse cast at `:50`.
  - MPDIALOG: `mpdialog_flow.c:189,544,667` and `mpdialog_details.c:343`.
  - WORLD: `world_search.c:49`.
- **`include/fs98/arena.h`:**
  - On i386, `fs98_handle(p)` is `((fs98_addr)(uintptr_t)(p))` and `fs98_ptr(a)` is `(void *)(FS98_ARENA_BASE + (uintptr_t)(uint32_t)(a))`.
  - None of its macros is defined in these modules. `world_advanced_internal.h`'s `AT`/`U32`/`PTR` do not clash.
- **FLIGHTS anchors:** `tools/flights_breaks.py:65-66,187,445,552,555-556`. No MPDIALOG or WORLD anchor holds a converted cast.
- **Gate:** `~/openfs98-verify/gate/compare.py` and `replay.py`, which deduplicates replays by (cwd, flags, source). Existing blocks to copy from:
  - `p18f`: a pinned `--mut`;
  - `p18d`: `-Iinclude` allowed on replays;
  - `p18c`: a second PE in a case;
  - 4b's `--strict`: rehashing at compare time.
- **6b0's proof,** `~/openfs98-verify/p18g0`, has the mode table (`runners.tsv`), the fixture setup and the counts (`check2.out`). Build-only took about 75 min for its two passes run together.

## Tasks & Acceptance

**Execution:**
- [ ] The gate: `--require-set p18g`, `gate.sh`, `audit.py` and the negative script. Build the before snapshot and its 28 mutant sets, then freeze `cases/`.
- [ ] Convert the nine source files.
- [ ] `-Iinclude` on the six recipes and seven runner sites, then the five anchor repairs with their commutation checks.
- [ ] `tools/arena_lint_allowed.txt`: delete the nine rows. `deferred-work.md`: add the plain-cast entry.
- [ ] Build the after side. Run the audit, the trace check, the gate and the negative checks, then rerun the earlier sets.
- [ ] `## Implementation Notes`: counts, the pinned keys, results.

**Acceptance Criteria:**
- **Lint:** given the slice, when `check_arena.py --self-test`, the lint, and the lint with `--prior` run, then all pass with the nine rows gone and the reverse count at 22.
- **Audit:** given `audit.py` and the trace check, when they run, then both pass.
- **Gate:** given `--require-set p18g`, when it runs, then it prints `GATE PASS` with all 955 cases and 1,171 PEs identical to the frozen digests.
- **Negative checks:** each one fails with its expected diagnostic.
- **Earlier sets:** they still pass.

## Review Triage Log

Spec review (Codex gpt-6.2-sol, 2026-11-25), before approval. The review confirmed the casts, rows, recipes and anchors, with the reverse count at 23 → 22.

| # | Finding | Verdict | Action |
|---|---|---|---|
| 1 | A symmetric PE deletion passes | blocker | Frozen per-case basename inventory, 1,171 PEs, and two deletion probes. |
| 2 | Symmetric swaps, and side identity | high | Frozen before digests and run identity in logs. Full trace binding of each case was rejected as general distrust of runner output (as in slices 3–6a); the swap probe covers the case. |
| 3 | Link outputs are not required, and kept files are not rehashed | high | Nine artifacts; rehash; `bin/` required. |
| 4 | A replay count is not exact | high | 63 pinned keys and six link invocations. |
| 5 | The lint and build-only miss a wrong spelling or a changed verdict | high | `audit.py` and the trace check. |
| 6 | Runner sites are implicit | medium | Seven sites, DLL-only in shared builders, the mode table. |
| 7 | The fallback baselines are wrong, and the waiver is inconsistent | medium | Module-doc baselines; waiver limited. |
| 8 | Negative probes may fail for the wrong reason | medium | Exit 1, `GATE FAIL` and the diagnostic; valid probes. |
| 9 | The plain-cast inventory is wrong | low | 14 plus 2 ordinals, enumerated. |

## Verification

**Commands:**
- `python3 -B tools/check_arena.py --self-test`, `python3 -B tools/check_arena.py`, and `python3 -B tools/check_arena.py --prior ~/openfs98-verify/p18g/prior.txt` -- expected: pass.
- `python3 -B ~/openfs98-verify/p18g/audit.py` -- expected: pass.
- `bash ~/openfs98-verify/p18g/gate.sh` -- expected: `GATE PASS`.
- `bash ~/openfs98-verify/p18g/negative.sh` -- expected: every probe fails as specified.
- The earlier sets -- expected: `GATE PASS`.

## Implementation Notes

**Counts.** `check_arena.py --self-test` (37 cases), the plain run, and `--prior` on 27a9e49's list (`~/openfs98-verify/p18g/prior.txt`) pass; the nine rows are gone, reverse casts 22 (23 before), 32 files, 88 casts, 27 definitions. `check_abi_headers.py` passes.

**Conversion.** 12 forward and 1 reverse substitution in nine files, plus nine `#include "fs98/arena.h"`. The parenthesised operand stays whole: `fs98_handle((slice + fields[index][0]))`. `audit.py` (`~/openfs98-verify/p18g`) re-derives the conversion from 27a9e49's text with an operand parser and compares it with the worktree; it also checks the six recipe lines, the six runner lines, the nine allowlist rows, the docs (pure additions) and the file set: AUDIT PASS. `-Iinclude` sits right after `-Werror` on the six recipes and after the flags on the seven runner sites.

**Spec defect, six anchors not five (deviation, approved by the user 2026-11-26).** `select-choose-itemdata` anchors on `0);` at the end of the reverse-cast statement and cannot match once the call is wrapped in `fs98_ptr(...)`. It is repaired like the five (`fill-reload`, `comms-error-pointer`, `ap-error-pointer`, `radios-error-address`, `radios-error-answer`); `p18g/commute.py` shows, over all 502 cases, that the old mutation on the old source then converted equals the repaired mutation on the converted source (PASS), and that only these six changed. Logged in `deferred-work.md`, with the plain-cast inventory (14 arena pointers: the spec's line numbers for the two ordinals, 279 and 98, are 283 and 103 at this base).

**Gate** (`compare.py --require-set p18g`, backup `compare.py.bak-pre-p18g`; `p18g/gate-result.txt`): GATE PASS. 1130 artifacts, 321 PEs and 206 objects identical, rehashed from the kept `bin/` of both sides; the 63 pinned keys (`gate/p18g_replay_keys.json`, from the before manifest) replayed once per side, the after side with one trailing `-Iinclude`, objects cmp-identical; the six link invocations equal in inputs and differing by one `-Iinclude`; 28 `--mut` pairs, 955 cases and 1,171 PEs equal to the frozen digests (`p18g/cases/`, frozen right after the before runs) with the same case ids and file names on both sides. Run identity: each run logs its snapshot root (`before-src`/`after-src`), its commit or diff hash and a hash of the tree it ran on, which `gate.sh` matches to the side's snapshot. No mutant differed, so no rebuild or waiver was needed. Trace check (`tracecheck.py`): 1,171 compiles, 873 with exactly one added `-Iinclude`, every output hash identical, TRACE PASS.

**Shared-unit refinement (deviation, approved by the user 2026-11-26): 42 plain WORLD replays.** The spec expects shared units to replay once. WORLD's DLL also compiles units that unchanged recipes compile without the flag, so after holds both keys for those. The gate pins that: 75 producer records of 42 distinct units (`gate/p18g_producers.json`, by log, recorded cwd, key and output, from the before side) must be identical on both sides; the plain key may remain only where a pinned producer makes it, and the `-Iinclude` key's object must equal before's.

**Negative checks** (`negative.sh`, `negative-checks.txt`): the unmodified pair passes; WORLD.DLL missing, one replay entry removed, one link invocation removed, one case removed, one PE removed from a one-PE case and from `mut-w_ui/baseline`, a renamed case, two mutant PEs swapped, two bin DLLs swapped, a missing mode pair and a valid 29th pair each exit 1 with `GATE FAIL` and the expected diagnostic.

**Earlier sets.** After the `compare.py` changes p18b, p18c, p18d, p18e, p18f and 4b's `--strict` all print GATE PASS.

**Not done.** No wine baselines were run (the waiver was not needed). Break counts were not re-run.

**Code review fixes (Codex, 2026-11-26), all outside the repo.** `compare.py` walks each kept `bin/` itself and compares the path set with `art.paths`, reads files inside the diagnostics path (no `FileNotFoundError`), validates PE32 headers, section table and raw bounds (`gate/pecheck.py`, also used by `freeze.py`, which reproduced the inventory byte for byte), validates inventory rows, and pins the unchanged producers and the six links' keys with the manifest parser (`gate/replaykeys.py`, no `isdir`). `gate.sh` pairs each run header with its exit record and checks the build root; `tracecheck.py` pins per-mode compile coverage, pairs records by position and output, checks cwd and the `gcc -M` status and allows `-Iinclude` only at the approved sites (873); `audit.py` compares each runner's full text with the baseline plus the listed replacements; `negative.sh` fails if the control is not GATE PASS and adds probes n1 (DLL deleted), n1b-n1d, n12-n14 (all 17 pass). Gate, trace check, audit, commutation, lint and p18b-p18f plus 4b `--strict` all pass again. Not redone: `run_side.sh` now serialises on a lock and computes the tree hash per run, but both sides were not rerun (about 4 h each); the saved runs hold one tree hash per side, computed once before the 28 runs, which `gate.sh` still matches to the side's snapshot.

| # | Finding | Verdict | Action |
|---|---|---|---|
| 1 | Rehash relied on `art.paths` | high | Independent walk, path-set check. |
| 2 | Normalisation counted as PE validation | medium | `pecheck.py` in the gate and in `freeze.py`. |
| 3 | `root=` ignored; tree hash once per side; header/exit not paired | high | Root and pairing checked in `gate.sh`; `run_side.sh` fixed for reruns, runs not repeated. |
| 4 | Trace check passed with a dropped compile; flag anywhere | high | Pinned coverage, pairing, cwd, dependency status, approved sites. |
| 5 | Plain-key exception compared key unions; `isdir` on a historical cwd | medium | Pinned producers (75 records, 42 units); manifest parser; no `isdir`. |
| 6 | FLIGHTS audit checked hunk sizes only | medium | Full-text check of every runner. |
| 7 | Malformed or repeated inventory field raised or overwrote | medium | Validated, `GATE FAIL`. |
| 8 | Control status lost in a subshell | low | Control must exit 0 with GATE PASS. |
| 9 | WORLD.DLL probe left the file; missing file raised | high | File deleted in both trees; missing files reported. |
| 10 | 42 plain replays change the once-per-side rule | medium | Approved by the user; producers pinned. |
| 11 | Sixth anchor repair | low | Approved by the user. |
