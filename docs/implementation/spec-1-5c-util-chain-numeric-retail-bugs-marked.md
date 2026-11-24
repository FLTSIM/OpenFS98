---
title: 'Story 1.5, slice 3: UTIL, CHAIN and NUMERIC retail bugs marked'
type: 'chore'
created: '2026-11-23'
status: 'done'
route: 'dispatch'
baseline_commit: '21c827a657a34d68fef78d576b8e6c720c921a81' # worktree also holds the uncommitted slice 2
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-1-5b-host-sim1-retail-bugs-marked.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** After slice 2, 231 ids are still allowlisted. 21 of them belong to the third slice: UTIL-1..4 and UTIL-6..17 (there is no UTIL-5), CHAIN-1..2 and NUM-1..3. None of the three modules has a marker yet.

**Approach:** Use slice 2's method (`spec-1-5b`'s Boundaries): read each entry's code, correct its row, mark every site of every part it names, put Not-reproduced notes on parts we don't reproduce, leave a per-part checklist in Implementation Notes, and remove the ids from the allowlist.

## Boundaries & Constraints

**Always:**
- Comments only. Every PE under `bin/` must hash the same before and after (the `peh3.py` lists, `bin/break-pool` excluded). Each PE that links `src/util/`, `src/chain/` or `src/numeric/`, including those not in `bin/` (`OFSTEST.DLL` from target `ofstest`, `UTIL_TRACE.DLL`), must be shown rebuilt after the edits, either by its mtime or by a scratch build of its recipe; a stale binary proves nothing.
- Decision (user, 2026-11-23): UTIL-1 becomes `UTIL-1 (fixed, not kept)`, with a `RETAIL BUG (fixed, not kept): UTIL-1` marker at our 64-bit divide and guard (`src/util/util_math.c:358-363`), saying retail raises #DE there. UTIL-8 becomes `UTIL-8 (no longer applies)`, with no marker, not allowlisted, and one sentence in the entry: ours omits the dead compare, which has no observable effect. Decision (user, 2026-11-23, after spec review): UTIL-14 (ours returns nothing, `util_exchange.c:395`), UTIL-16 (ours zeroes the answer, `util_alert.c:117`) and CHAIN-2 (ours returns -1 on a null list, `chain.c:153`) become `(fixed, not kept)` likewise, with a marker at each fix. Any other entry found wholly unreproduced stays allowlisted with a `#` comment and is reported.
- The checker counts ids, not sites, so the per-part checklist in Implementation Notes is part of acceptance: for each id, each behaviour (part) its row names, and the file:line of its marker or `Not reproduced (ID): ...` note.
- Builds run under `nice -n 19`, with `-j 2` and `CMAKE_BUILD_PARALLEL_LEVEL=2`, while the Story 1.4 rerun runs.
- No staging or commits.

**Never:**
- No section outside UTIL, CHAIN and NUMERIC is marked, no `tools/` file except the allowlist is edited, no retail bug is fixed, and no UTIL-5 is invented.

</frozen-after-approval>

## Code Map

- `docs/retail-bugs.md:183-219`: UTIL (Where cells give service slots; `docs/modules/util.md` has the VAs), CHAIN and NUMERIC.
- `src/util/`:
  - `util_math.c`: `+0x44`, UTIL-1..4 (`:284-363`), `+0x80`/`+0x84`, `+0xbc`/`+0xc0`;
  - `util_exchange.c`: `+0x8c`/`+0x90`, UTIL-12..14 (the dispatch's header at `:32-40` already describes UTIL-12);
  - `util_files.c`: `+0x94`/`+0xa0` and the file lister, UTIL-9..11;
  - `util_alert.c`: UTIL-15..17.
- `src/chain/chain.c`: CHAIN-1, the callback register contract (probably in asm or inline asm), and CHAIN-2's null list.
- `src/numeric/numeric.c`: NUM-1..3.
- Which PEs link these sources: grep `cmake/targets.cmake` and `cmake/steps/*.cmake` for `src/util/`, `src/chain/` and `src/numeric/`. Slice 2 found that `bin/HOST.DLL` is built only by its harness targets, so check whether `bin/UTIL.DLL` and the others relink in the default build, and if not, scratch-build them with the expanded flags from `build/win32-retail`.
- Known row corrections and site gaps (spec review 1):
  - UTIL-1 cites BST-4; its duplicate is BST-5. Correct the reference and leave BST-5 to its own slice.
  - UTIL-2/3: the lookup is `fs98_util_lat_scale` (`util_math.c:295`), called from `+0x44` and `+0x48` (`:1259`). Ours falls back to offset 0 where retail's two reads would leave the copied `.data`: a Not-reproduced part.
  - UTIL-9 names `+0x94`, but `+0xa0` shares the entry, and the type comes from HOST `+0x160` (`host.c:2214`), which leaves it untouched when the lookup fails. Mark both consumers (`util_files.c:51`, `:119`) and record ours seeding 0 for the first entry.
  - UTIL-11's lister is `+0x98`'s catalogue, at the empty-count return (`util_files.c:245`).
  - UTIL-12: mark the dispatch in both directions (`util_exchange.c:161`, `:206`, `:279`, `:291`, `:319`, `:402`), with notes saying what ours does in place of retail's re-entry and off-table jumps, not "inert".
  - UTIL-15: mark the separator appended before the realloc (`util_alert.c:58`/`:72`); put a note at ours initialising `block = 0`; name both alert callers.
  - UTIL-17: retail (`0x208e339b`) and ours both allocate with zero = 1, so the "sixteen bytes left as malloc returned them" claim is wrong. Correct the row and the `util_alert.c:141` header. Mark the early `*out` store and the layout. ALERT-1 makes the same claim; note that for its slice and don't edit it.
  - NUM-3: only `fs98_numeric_store_s48` (`numeric.c:1120`) rejects equality; the longitude and latitude parsers (`:1430`, `:1609`) accept it. Narrow the row, and mark the helper's upper-bound comparison.
  - CHAIN-1: mark each part: the callback thunk, edx carried between callbacks, the dispatch's return paths and compaction, and the final register reload (`chain.c:189-297`). Put Not-reproduced notes for the add and remove services' leftover registers.
- PEs outside the default build: `difftests` builds `numericdiff`, `slewdiff`, `utildiff` and `utildiff_portable`; `ofstest` (`cmake/targets.cmake:1664`) and the `UTIL_TRACE.DLL` recipe (`:710`) are built only on demand. Scratch-build those two from their expanded recipes before the edits and after, and compare.
- Reuse slice 2's gate: `/tmp/claude-1000/-home-steve-source/b2e3078b-1d77-426e-816f-2de2772a67dc/scratchpad/peh3.py`, with the hash lists in a new `pre15c/` there.

## Tasks & Acceptance

**Execution:**
- [ ] Before any edit, hash every PE under `bin/` into `pre15c/`, and scratch-build `OFSTEST.DLL` and `UTIL_TRACE.DLL` into it.
- [ ] `src/util/*`, `src/chain/chain.c` and `src/numeric/numeric.c`: add the markers and notes.
- [ ] `docs/retail-bugs.md`: correct the rows, and add "Not reproduced" sentences.
- [ ] `tools/retail_bugs_unmarked.txt`: drop the ids marked.
- [ ] `docs/tools.md`: update the check's Baselines line (`:284`).

**Acceptance Criteria:**
- Given the slice is done, when `python3 -B tools/check_retail_bugs.py` runs, then it exits 0 with no UTIL, CHAIN or NUM id allowlisted and the summary `331 entries, 120 marked, 210 allowlisted, 1 no longer apply`, and the per-part checklist covers every part of every row (lower only for entries reported as wholly unreproduced).
- Given the builds before and after, when the hash lists are diffed, then the diff is empty, and each PE built from these modules was rebuilt.

## Review Triage Log

Spec review 1 (Codex gpt-6.2-sol):
1. high, applied (user decision): UTIL-14, UTIL-16 and CHAIN-2 are deliberately not reproduced, so they become `(fixed, not kept)`; the counts stay at 120/210 (Codex's 117/213 assumed they would stay allowlisted).
2. high, applied: UTIL-17's malloc claim is wrong (zero = 1 on both sides).
3. high, applied: OFSTEST.DLL and UTIL_TRACE.DLL scratch-built before and after.
4-8, 10. medium/low, applied: UTIL-2/3, 9, 11, 12, 15, NUM-3 and BST-5 corrections, listed in the Code Map.
9. medium, applied: the per-part checklist is an acceptance item, with CHAIN-1's parts named.

Code review 1 (Codex gpt-6.2-astra: blind, edge case, verification gap):
- B1/E1/V1 false -- "23 HOST/SIM1 ids dropped without their markers": the review diff left out slice 2's uncommitted `src/host`/`src/sim1` edits on purpose; those markers are in the worktree, and the check passes.
- B2/E3 medium, patch -- CHAIN-1's entry gave ecx as the last callback's; `chain.c:299` returns the decremented depth (edx is the callback's).
- B3/E6 low, patch -- UTIL-12 index range is 4..7, not 4..8 (0x2001 is the callback, taken first).
- B4 low, patch -- UTIL-12's get side: `+0x90` returns nothing and the kind handler still clears status.
- B5/E2 medium, patch -- UTIL-13's `X_STRING` copy writes in validation mode and had no marker.
- E7 low, patch -- UTIL-13 flag marker: valid widths write whatever the mode; width 0 only clears status.
- B6/E4 low, patch -- UTIL-15: the services forward `p1` unchanged; zero `p1` with a later piece reaches strcat with null.
- B7 low, patch -- UTIL-9: duplicate same-path records are one per match; the stale type only steers recursion.
- B8/E5 low, patch -- UTIL-10's drive-root claim needs HOST-1's 0x5c exception.
- B9/E8 low, patch -- HOST-11 (slice 2's text): `%f` of a double is up to 318 bytes, and one value overruns.

After the patches: the check passes (120 marked, 210 allowlisted, 1 no longer applies). Every PE relinked since (UTIL.DLL, both OPENFS98.EXE, utildiff, utildiff_portable), and scratch-built HOST.DLL, OFSTEST.DLL and UTIL_TRACE.DLL, hash as in `pre15c/`.

## Verification

**Commands:**
- `python3 -B tools/check_retail_bugs.py --self-test && python3 -B tools/check_retail_bugs.py`. Expected: the self-test passes, then the summary line, exit 0.
- `nice -n 19 cmake --build build/win32-retail -j 2 && CMAKE_BUILD_PARALLEL_LEVEL=2 nice -n 19 tools/ofs difftests`, then hash again and `diff` against `pre15c/`. Expected: empty.

## Implementation Notes

Markers, file:line (`util_math.c` = `src/util/util_math.c`, and so on). No entry was wholly unreproduced beyond the user's decisions.

- UTIL-1 (fixed, not kept): util_math.c:363 (divide and guard).
- UTIL-2: kept, util_math.c:321 (unclamped index); Not reproduced, :315 (fallback to offset 0 past the copied .data). Called from +0x44 and +0x48.
- UTIL-3: util_math.c:333. UTIL-4: util_math.c:374.
- UTIL-6: util_math.c:624. UTIL-7: util_math.c:1053 (+0xc0), :1068 (+0xbc).
- UTIL-8: no longer applies; no marker; sentence in the entry.
- UTIL-9: util_files.c:51 (+0x94), :123 (+0xa0); both note ours seeding 0.
- UTIL-10: util_files.c:127 (dot in name, and bare name against the drive root). UTIL-11: util_files.c:254.
- UTIL-12: util_exchange.c:164 (put_sub), :304 (get_sub), :288 (put kind), :416 (get kind); each with its Not-reproduced note (re-entry, off-table jumps).
- UTIL-13: util_exchange.c:176 (callback numbering), :186 (bits always write), :234 (NUMERIC scratch), :254 (width skip).
- UTIL-14 (fixed, not kept): util_exchange.c:407.
- UTIL-15: separator before realloc, util_alert.c:80; Not reproduced (garbage block pointer, ours null), :64 and header :47; both callers (+0xa4, +0xa8) named.
- UTIL-16 (fixed, not kept): util_alert.c:124.
- UTIL-17: early `*out` store and layout, util_alert.c:161; header corrected (zero = 1 in both). ALERT-1's same claim is left for its slice.
- CHAIN-1: thunk chain.c:250; edx carried between callbacks :285; return paths :272; compaction :204; ecx/edx after the walk :299; final reload :312; Not reproduced (add, remove leftovers) :143, :181.
- CHAIN-2 (fixed, not kept): chain.c:154.
- NUM-1: numeric.c:924. NUM-2: numeric.c:1639. NUM-3: numeric.c:1121 (store_s48 only; the row is narrowed).

Verification: self-test passes; check prints `331 entries, 120 marked, 210 allowlisted, 1 no longer apply`; `cmake --build` and `tools/ofs difftests` exit 0 (CHAIN.DLL, NUMERIC.DLL, UTIL.DLL and the four difftest exes were relinked, by mtime); scratch-built OFSTEST.DLL and UTIL_TRACE.DLL hash the same before and after; `peh3.py` lists of every PE under `bin/`, stripped and unstripped, diff empty.
