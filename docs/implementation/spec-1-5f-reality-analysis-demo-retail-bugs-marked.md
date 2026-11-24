---
title: 'Story 1.5, slice 6: REALITY, ANALYSIS and DEMO retail bugs marked'
type: 'chore'
created: '2026-11-23'
status: 'draft'
route: 'dispatch'
baseline_commit: 'ce8b47b93d362e8c3eeb5054114cac27aa529bc6' # worktree also holds the uncommitted slices 4 and 5
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
- No section outside REALITY, ANALYSIS and DEMO is marked. A site in another module (a CONTROLS caller for DEMO-3/DEMO-4, say) is cited in the row, not marked. No `tools/` file except the allowlist is edited, and no retail bug is fixed.

</frozen-after-approval>

## Code Map

- `docs/retail-bugs.md`: the REALITY, ANALYSIS and DEMO sections. `docs/modules/reality.md`, `analysis.md` and `demo.md` give the offsets.
- `src/reality/reality.c`: REAL-1..8.
  - REAL-8 is dead code the row says was omitted, so likely `(no longer applies)`.
  - REAL-3's reads past 2,545,942,528 are a Not-reproduced part.
  - `:20` says an import call was not reproduced because it has no observable effect.
- `src/analysis/analysis.c`: ANA-1..10.
  - ANA-1 is reproduced through `AN_MEMMOVE_ECX` (`:49`, `:352-363`).
  - ANA-6's row says ours is zeros: decide whether that part is our deliberate initialisation (`(fixed, not kept)` for that part) or a Not-reproduced part of a mixed entry. Its second part, the extents loop from -1, needs its own check.
  - `:19` says an import call was not reproduced because it has no observable effect.
- `src/demo/demo.c`: DEMO-1..11.
  - The refill (`:1341`) and the 0x40 handler (`:2230`) are DEMO-7's and DEMO-3's register carries.
  - DEMO-11's row claims a marker; add one, and drop "marked" from the row if it reads as already done.
- PEs:
  - default build: `bin/REALITY.DLL`, `bin/ANALYSIS.DLL`, `bin/DEMO.DLL`;
  - on-demand links: `bin/difftest/REALITY_ours.dll` and `bin/difftest/ANALYSIS_ours.dll` (`cmake/targets.cmake:1046`, `:1730`; check which is which). Scratch-build them before and after, as `sb15e.sh` does in the scratchpad.

  No source here includes another `.c`.
- The gate: `/tmp/claude-1000/-home-steve-source/b2e3078b-1d77-426e-816f-2de2772a67dc/scratchpad/peh3.py`, with lists in new `pre15f/` and `post15f/` there.

## Tasks & Acceptance

**Execution:**
- [ ] Before any edit, hash every PE under `bin/` into `pre15f/` and scratch-build the on-demand links into `pre15f/sb/`.
- [ ] `src/reality/reality.c`, `src/analysis/analysis.c`, `src/demo/demo.c`: add the markers and notes.
- [ ] `docs/retail-bugs.md`: correct the rows, add "Not reproduced" sentences, and apply any status changes.
- [ ] `tools/retail_bugs_unmarked.txt`: drop the ids marked or obsolete.
- [ ] `docs/tools.md`: update the check's Baselines line.
- [ ] This spec: an `## Implementation Notes` section with the per-part checklist.

**Acceptance Criteria:**
- Given the slice is done, when `python3 -B tools/check_retail_bugs.py` runs, then it exits 0 with no REAL, ANA or DEMO id allowlisted except those reported. With `O` entries made obsolete and `R` reported, the summary is `331 entries, (190 - O - R) marked, (138 + R) allowlisted, (3 + O) no longer apply`.
- Given the builds before and after, when the hash lists are diffed, then the diff is empty, and each PE built from these modules was rebuilt.

## Verification

**Commands:**
- `python3 -B tools/check_retail_bugs.py --self-test && python3 -B tools/check_retail_bugs.py`. Expected: the self-test passes, then the summary line, exit 0.
- `nice -n 19 cmake --build build/win32-retail -j 2 && CMAKE_BUILD_PARALLEL_LEVEL=2 nice -n 19 tools/ofs difftests`, then hash again, scratch-build again, and `diff` against `pre15f/`. Expected: empty.
