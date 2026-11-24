---
title: 'Story 1.5, slice 6: REALITY, ANALYSIS and DEMO retail bugs marked'
type: 'chore'
created: '2026-11-23'
status: 'done'
route: 'dispatch'
baseline_commit: '86beaf13644b2c9ed4160992b0d67262c81c1029' # slices 4 and 5 committed
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-1-5e-window-winmsg-mpchat-retail-bugs-marked.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** After slice 5, 167 ids are still allowlisted. 29 of them belong to the sixth slice: REAL-1..8, ANA-1..10 and DEMO-1..11. None of the three modules has a marker yet (DEMO-11's row says "Kept, marked", but `src/demo/` has none).

**Approach:** Use slice 5's method (`spec-1-5e`'s Boundaries): read each entry's code, correct its row, mark every site of every part it names, put Not-reproduced notes on parts we don't reproduce, leave a per-part checklist in this spec's Implementation Notes, and remove the ids from the allowlist.

## Boundaries & Constraints

**Always:**
- Statuses follow the user's standing rule:
  - an entry whose bug ours deliberately avoids becomes `(fixed, not kept)`, with a marker at our fix;
  - one whose only site is dead code, or code with no observable effect, that we omitted becomes `(no longer applies)`, with no marker, off the allowlist, and one sentence why;
  - one that is not reproduced for no stated reason stays allowlisted with a `#` comment and is reported;
  - a mixed entry keeps a `(kept)` marker on each reproduced part and a Not-reproduced note on each other part.

  An obsolete id's checklist line gives its rationale instead of a file:line.
- The per-part checklist is part of acceptance: for each id, each behaviour (part) its row names, and the file:line of its marker or `Not reproduced (ID): ...` note. Refresh it to the final line numbers after the last edit.
- Comments only. Before any edit, hash every PE under `bin/` with `peh3.py` (`bin/break-pool` excluded); afterwards they must hash the same. Each PE that links an edited source must be shown rebuilt after the last edit, by an mtime newer than the source, or by a scratch build of its recipe before and after under the same relative name. The link and the hash must succeed, and any `ERR` line for a built PE fails the gate.
- Builds run under `nice -n 19`, with `-j 2` and `CMAKE_BUILD_PARALLEL_LEVEL=2`, while the Story 1.4 rerun runs.
- No staging or commits, no reverting sources to take a baseline, and no changing this spec's status.

**Never:**
- No section outside REALITY, ANALYSIS and DEMO is marked. A site in another module is cited in the row, not marked, and its checklist line gives that citation: CONTROLS `+0x44` is DEMO-3's callee and supplies the `ecx` left over (`src/controls/controls.c:80`); `pilot_event` is DEMO-4's caller and supplies `edi` (`controls.c:3702`). No `tools/` file except the allowlist is edited, and no retail bug is fixed.

</frozen-after-approval>

## Code Map

- `docs/retail-bugs.md`: the REALITY, ANALYSIS and DEMO sections. `docs/modules/reality.md`, `analysis.md` and `demo.md` give the offsets.
- `src/reality/reality.c`: REAL-1..8.
  - REAL-8 is dead code the row says was omitted, so likely `(no longer applies)`.
  - REAL-3: both the feet and the metric searches run past their tables (`:740`, `:763`); mark both. The reads past 2,545,942,528 depend on the image layout and are a Not-reproduced part (`reality.md:120`).
  - REAL-4: a flat (zero) range forces the scale to 1 even for a unit past 2; qualify the row to non-zero ranges.
  - `:20` says an import call was not reproduced because it has no observable effect.
- `src/analysis/analysis.c`: ANA-1..10.
  - ANA-1 is reproduced through `AN_MEMMOVE_ECX` (`:49`, `:352-363`).
  - ANA-6 has three parts, each with a checklist line:
    - the landing graph's uninitialised last-position local, when index -1 is refused (`l_curve`, `:310`, `:702`);
    - the manoeuvre extents loop from -1, which takes the first sample twice (`m_extents`, `:725`);
    - ours explicitly zeroing the empty manoeuvre case (`:929`).

    The row's "ours is zeros" covers only the manoeuvre.
  - ANA-9 has two unchecked lookups:
    - `an_frame_ecx` indexes the frame-delay table by GLOBAL `+0x824` (`:472`);
    - `build_list` indexes the segment counts by `+0x828` and does the unchecked writes (`:572`).

    Name both routines in Where, and give checklist lines for both lookups, the zero-count loop and the writes.
  - `:19` says an import call was not reproduced because it has no observable effect.
- `src/demo/demo.c`: DEMO-1..11.
  - The refill (`:1341`) and the 0x40 handler (`:2230`) are DEMO-7's and DEMO-3's register carries.
  - DEMO-11's row claims a marker; add one, and drop "marked" from the row if it reads as already done.
  - DEMO-8: view value 4 writes tag `0x84`, which the parser takes as a message; a value such as `0x12` writes `0x92`, which reaches `parse_frame`'s stop-and-recurse. Narrow the trigger, and mark the tag writer (`:2402`), the parser's default (`:1793`) and the review-stop cycle (`:1706`, `:1279`, `:1503`).
  - DEMO-10: the zero-interval divide still faults, but ours divides through an unsigned `divl` helper (`:2203`), so the host's repair saturates unsigned where retail's `idivl` saturates signed. `demodiff` stops at the fault (`tools/difftest/demodiff.c:1703`), so the repair is never compared. Mark the fault and both parser behaviours, put a `Not reproduced (DEMO-10)` note on the signed repair, and add one line to `docs/open-differences.md`.
- PEs:
  - default build: `bin/REALITY.DLL`, `bin/ANALYSIS.DLL`, `bin/DEMO.DLL`;
  - fixture copies: `bin/difftest/REALITY_ours.dll`, `ANALYSIS_ours.dll` and `DEMO_ours.dll`, copied by the `*diff` targets from the default DLLs. They must still hash as the fresh default DLLs do.
  - on-demand links: `analysis-break-build` and `reality-break-build` (`cmake/targets.cmake:1044`, `:1117`, `:1728`). Scratch-link them before and after, with their source variables set to the modules' own sources, as `sb15e.sh` does in the scratchpad.

  No source here includes another `.c`.
- The gate: `/tmp/claude-1000/-home-steve-source/b2e3078b-1d77-426e-816f-2de2772a67dc/scratchpad/peh3.py`, with lists in new `pre15f/` and `post15f/` there.

## Tasks & Acceptance

**Execution:**
- [x] Before any edit, hash every PE under `bin/` into `pre15f/` and scratch-build the on-demand links into `pre15f/sb/`.
- [x] `src/reality/reality.c`, `src/analysis/analysis.c`, `src/demo/demo.c`: add the markers and notes.
- [x] `docs/retail-bugs.md`: correct the rows, add "Not reproduced" sentences, and apply any status changes.
- [x] `tools/retail_bugs_unmarked.txt`: drop the ids marked or obsolete.
- [x] `docs/tools.md`: update the check's Baselines line.
- [x] This spec: an `## Implementation Notes` section with the per-part checklist.

**Acceptance Criteria:**
- Given the slice is done, when `python3 -B tools/check_retail_bugs.py` runs, then it exits 0 with no REAL, ANA or DEMO id allowlisted except those reported. With `O` entries made obsolete and `R` reported, the summary is `331 entries, (190 - O - R) marked, (138 + R) allowlisted, (3 + O) no longer apply`; with REAL-8 obsolete and nothing reported, that is `189 marked, 138 allowlisted, 4 no longer apply`.
- Given the builds before and after, when the hash lists are diffed, then the diff is empty, and each PE built from these modules was rebuilt.

## Implementation Notes

Per-part checklist (lines as of the final sources; `r:` = `src/reality/reality.c`, `a:` = `src/analysis/analysis.c`, `d:` = `src/demo/demo.c`). Status changed: REAL-8 `(no longer applies)`. Nothing was left allowlisted.
- REAL-1: `r:460` (the test for 1 reads the dialog's second byte). REAL-2: `r:772` (metric step 0x30). REAL-3: `r:761` (feet search; stops at `.rdata+0xe8` up to 3,181,878,959), `r:772` (metric search, which skips that limit), `Not reproduced (REAL-3)` note `r:764` (the metric search past 2,545,942,528) (reads past 2,545,942,528). REAL-4: `r:756` (a unit past 2 writes neither S_STEP nor S_SCALE; a zero range forces S_SCALE to 1 for the curve only, the labels keep the stale step). REAL-5: `r:610`. REAL-6: `r:732`. REAL-7: `r:394`.
- REAL-8 `(no longer applies)`: both sides of the `+0xc44` test go the same way, so the omitted test has no observable effect (`reality.c`'s `stress` comment); no marker.
- ANA-1: `a:359`. ANA-2: `a:362`. ANA-3: `a:377`. ANA-4: `a:437`. ANA-5: `a:698`. ANA-7: `a:884`. ANA-8: `a:1205`. ANA-10: `a:1043` (NUMERIC 0x3a reads only the low dword).
- ANA-6, three parts: (1) landing's uninitialised last-position local, `l_curve`'s `get_position(ebx - 1, ...)` with `get_position`'s refusal (`a:310`), marker `a:741` (ours is uninitialised too; also the capacity-only `get_position(0, ...)` slot-0 copy and the harness cut-off, `analysisdiff.c:566`); (2) `m_extents`'s loop from -1, marker `a:976`; (3) ours zeroing the empty manoeuvre case, `Not reproduced (ANA-6)` note `a:958`.
- ANA-9: `an_frame_ecx`'s frame-delay lookup by `+0x824` `a:480`; `build_list`'s segment-count lookup by `+0x828` `a:584`; the zero-count loop `a:599`; the unchecked writes `a:541` (`segment`).
- DEMO-1: `d:409`. DEMO-2: `d:2314`. DEMO-3: `d:2303` (the callee, CONTROLS `+0x44`, is cited: `src/controls/controls.c:344`, where the dispatch is; the spec's `:80` is `brakes_hold`'s tail, not marked).
- DEMO-4: `d:2444` (`e_view`), `:2458` (`e_2007`), `:2470` (`e_mode`), `:2517` (`demo_event_body`, taking the caller's edi); the caller, CONTROLS' `pilot_event`, is cited `src/controls/controls.c:3702`, not marked.
- DEMO-5: `d:1648` (`parse_memory` switches the shared table), `:1237` (the looping rewind), `:3315` (deinit leaves the file open). DEMO-6: `d:2884` (the refusal), `:1239` (the stale loop flag's hang). DEMO-7: `d:1358` (refill's ecx), `:1943` (the 0x84 reader's count).
- DEMO-8: tag writer `d:2445` (two-byte tag, high byte first: 0x8400 / 0x9200 are the values that reach the message parser / `parse_frame`'s stop) (`e_view`), parser `case 0x84` `d:1815`, `parse_frame`'s default `d:1726`, review-stop cycle `d:1289` (`play_review_end`), `:1517` (`stop_playing`). Trigger narrowed: 4 writes 0x84 (a message); 0x12 writes 0x92 (`parse_frame` stops on it).
- DEMO-9: `d:2925`. DEMO-11: `d:741` (marker added; row's "marked" dropped; the scan counts 1 to 0x38 bytes).
- DEMO-10: the fault `d:2205` (zero interval, speeds), `:2236` (vertical speed), `:2256` (`fault_div`, with `Not reproduced (DEMO-10)` on the signed repair and on the resume inside the void helper; one line in `docs/open-differences.md`); misparsed length skipping `d:1378` (`f_peek`'s 0x200-byte refills) and its origin `d:1928` (`record_tag`'s unchecked `length - 4`); second-at-a-time skip `d:1768` (`parse_frame`'s `skip:`).

## Review Triage Log

Spec review 1 (Codex gpt-6.2-sol):
1. high, applied: ANA-6's three parts.
2. high, applied: DEMO-10's signed vs unsigned repair is a Not-reproduced part, recorded in open-differences.
3. medium, applied: the fixture copies and the two break-build links.
4. medium, rejected: slice 5's ERR lines are wine-prefix and aircraft-data files whose names contain spaces; no built PE has one, and both lists carry the same lines.
5. medium, applied: REAL-3's two searches; REAL-4 qualified.
6. medium, applied: ANA-9's two lookups.
7. medium, applied: DEMO-8's trigger and sites.
8. medium, applied: DEMO-3's callee and DEMO-4's caller cited in the checklist.
9. low, deferred: DEMO's `disk_check` (`demo.c:427`) documents an unregistered retail bug (outputs used uninitialised when the free-space call fails; ours zeroes them). Recorded in `deferred-work.md` for a register id.
10. low, applied: baseline is the slices 4-5 commit.

Code review 1 (Codex gpt-6.2-sol, blind and edge-case layers; verification-gap skipped, as the user decided for comment-only slices):
- B1/E1 medium, patch -- DEMO-8's examples: tags are two bytes, high first; 0x8400 / 0x9200 reach the message parser / the stop-and-recurse (the implementer checked against `put_tag` and rejected my 0x9180, since 0x91 is a valid frame tag).
- B2/E5 medium, patch -- DEMO-10: ours resumes inside the void `fault_div` and discards the repaired quotient (a zero interval divides by zero again, an overflow stores the truncated quotient); recorded in the row, the note and open-differences.
- B3/E6 low, patch -- DEMO-10: `record_tag`'s unchecked `length - 4` skip, now marked.
- B4 low, patch -- ANA-6: the empty graph also copies slot 0 (`get_position` checks capacity); the harness cuts off the empty graph first.
- B5/E3 low, patch -- REAL-2 compresses the curve; `.rdata+0xc0` is taken only up to its own limit.
- B6 low, patch -- REAL-4: the zero range forces only the curve scale; the labels keep the stale step.
- B7/E7 low, patch -- DEMO-11: the scan takes 1 to 0x38 bytes, so ecx is already below 0x38 at the test.
- E2 low, patch -- REAL-3: only the metric search skips `.rdata+0xe8`'s limit; I read both limits from retail `REALITY.DLL` (`.rdata+0xc0` = 2,545,942,528, `+0xe8` = 3,181,878,959).
- E4 low, patch -- ANA-10: format 0x3a reads only the low dword (the `_ftol` result).

Deviation: the spec required `bin/difftest/DEMO_ours.dll` to hash as the fresh `DEMO.DLL`. It is a 2026-10-17 copy that only `tools/ofs demodiff` refreshes, so it says nothing about this change; it is unchanged across the slice. My spec was wrong.

After the patches: the check passes (189 marked, 138 allowlisted, 4 no longer apply). REALITY/ANALYSIS/DEMO.DLL were rebuilt after the last edit and hash as in `pre15f/`; the two break-build scratch links match.

## Verification

**Commands:**
- `python3 -B tools/check_retail_bugs.py --self-test && python3 -B tools/check_retail_bugs.py`. Expected: the self-test passes, then the summary line, exit 0.
- `nice -n 19 cmake --build build/win32-retail -j 2 && CMAKE_BUILD_PARALLEL_LEVEL=2 nice -n 19 tools/ofs difftests`, then hash again, scratch-build again, and `diff` against `pre15f/`. Expected: empty.
