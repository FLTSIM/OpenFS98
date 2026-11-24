---
title: 'Story 1.5, slice 5: WINDOW, WINMSG and MPCHAT retail bugs marked'
type: 'chore'
created: '2026-11-23'
status: 'done'
route: 'dispatch'
baseline_commit: 'ce8b47b93d362e8c3eeb5054114cac27aa529bc6' # worktree also holds the uncommitted slice 4
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-1-5d-facil-fe-retail-bugs-marked.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** After slice 4, 192 ids are still allowlisted. 25 of them belong to the fifth slice: WIN-1..8, WMS-1..9 and MPC-1..8. None of the three modules has a marker yet.

**Approach:** Use slice 4's method (`spec-1-5d`'s Boundaries): read each entry's code, correct its row, mark every site of every part it names, put Not-reproduced notes on parts we don't reproduce, leave a per-part checklist in this spec's Implementation Notes, and remove the ids from the allowlist.

## Boundaries & Constraints

**Always:**
- Statuses follow the user's standing rule:
  - an entry whose bug ours deliberately avoids becomes `(fixed, not kept)`, with a marker at our fix;
  - one whose only site is dead code, or code with no observable effect, that we omitted becomes `(no longer applies)`, with no marker, off the allowlist, and one sentence why;
  - one that is not reproduced for no stated reason stays allowlisted with a `#` comment and is reported;
  - a mixed entry keeps a `(kept)` marker on each reproduced part and a Not-reproduced note on each other part.
- Statuses this rule settles (spec review 1):
  - WIN-4 `(fixed, not kept)`: `update_title` starts the buffer empty (`window.c:705`).
  - MPC-7 `(fixed, not kept)`: ours zeroes the structures (`mpchat.c:225`).
  - WMS-7 `(no longer applies)`: the C call passes only the fill byte, and HOST keeps only the low byte (`winmsg.c:461`).
  - MPC-8 `(no longer applies)`: ours omits the ignored return, which CHAIN discards (`mpchat.c:423`).

  An obsolete id's checklist line gives its rationale instead of a file:line.
- The per-part checklist is part of acceptance: for each id, each behaviour (part) its row names, and the file:line of its marker or `Not reproduced (ID): ...` note.
- Comments only. Before any edit, hash every PE under `bin/` with `peh3.py` (`bin/break-pool` excluded); afterwards they must hash the same. Each PE that links an edited source must be shown rebuilt after the last edit, by an mtime newer than the source or by a scratch build of its recipe before and after. A scratch build writes to the same relative name in `pre15e/` and `post15e/`; the link and the hash must succeed, and any `ERR` line fails the gate.
- Builds run under `nice -n 19`, with `-j 2` and `CMAKE_BUILD_PARALLEL_LEVEL=2`, while the Story 1.4 rerun runs.
- No staging or commits, and no reverting sources to take a baseline.

**Never:**
- No section outside WINDOW, WINMSG and MPCHAT is marked, with three exceptions:
  - WIN-8's UTIL sites (`src/util/util_math.c:400`, `:1639`). Editing them also requires relink evidence for `UTIL.DLL` and the difftest exes that compile it, plus `sb15c.sh` scratch builds of `OFSTEST.DLL` and `UTIL_TRACE.DLL`.
  - WMS-8's second free, in WINDOW (`window.c:334`).
  - MPC-6's producer side in MULTIP (`src/multip/`), which is cited in the row as evidence but not marked: MULTIP is slice 10's. No `tools/` file except the allowlist is edited, and no retail bug is fixed.

</frozen-after-approval>

## Code Map

- `docs/retail-bugs.md`: the WINDOW, WINMSG and MPCHAT sections; `docs/modules/window.md`, `winmsg.md` and `mpchat.md` give the offsets.
- `src/window/window.c`: WIN-1..8.
  - WIN-3's Where covers creation only. Add removal (`:329`, the record's high half, for 3D and map views) and the title update (`:738`, the new title pointer's high half), alongside creation (`:284`), and say which pointer supplies the high word at each.
  - WIN-8: WINDOW restores its modelled eax after the calls (`:1674`, `:2082`); UTIL holds the register-preserving wrappers and the bare `+0x5c` stub (`util_math.c:400`, `:1639`). Name the slots.
  - WIN-1's saturation is computed in C; its divides are named in the row.
  - WIN-2's code tail is copied by `tools/gen_window_data.py`; mark the zoom indexing in C, and don't edit the tool.
  - WIN-8 is about which registers UTIL's services leave behind, seen from WINDOW's calls.
- `src/winmsg/winmsg.c`: WMS-1..9.
  - WMS-1: the line starts at `+0x98` and slot 6's pointer ends at `+0x93`, so speed 9 starts one byte early but reaching the pointer needs at least five. Correct the trigger, and mark the copy destination (`:483`) and the later frees separately (`:108`).
  - WMS-2: both the unchecked copy and the trailing NUL remain (`:496`). What differs is the PE layout (retail's import-table corruption), and the harness excludes the widths that trigger it (`winmsg.md:138`). Mark both writes, and describe the layout difference in a note; it is not a bounds check.
  - WMS-8: the first free is `:558`; the second is WINDOW's (`window.c:334`).
  - WMS-9: add `situation` to Where; the two Linkage returns (`:573`, `:591`) and the two list `0x14` callback returns (`:646`, `:680`) each need a checklist line.
- `src/mpchat/mpchat.c`: MPC-1..8.
  - MPC-3: a `0x3fe`-byte name plus `:` and the NUL fits exactly; the overrun starts at `0x3ff` (`:279`, `:584`). Correct the row, and record that the harness keeps names under `0x300`.
  - MPC-6: the receive and display path is `:526`.
- PEs:
  - default build: `bin/WINDOW.DLL`, `bin/WINMSG.DLL`, `bin/MPCHAT.DLL`;
  - on-demand links: `bin/difftest/WINDOW_ours.dll` (`cmake/targets.cmake:2111`), `bin/difftest/WINMSG_ours.dll` (`:2132`), `bin/difftest/MPCHAT_ours.dll` (`:1488`), and `bin/mpchat-ui/MPCHAT_ours.dll` (one recipe, repeated at `:1497`-`:1532`).

  The three break recipes take their source arguments from variables that default to empty; use the modules' own sources.

  Scratch-build each on-demand link before and after, as `sb15d.sh` does in the scratchpad, with the flags expanded from `build/win32-retail`. None of the three sources includes another `.c`.
- The gate: `/tmp/claude-1000/-home-steve-source/b2e3078b-1d77-426e-816f-2de2772a67dc/scratchpad/peh3.py`, with lists in a new `pre15e/` and `post15e/` there.

## Tasks & Acceptance

**Execution:**
- [x] Before any edit, hash every PE under `bin/` into `pre15e/` and scratch-build the on-demand links into it.
- [x] `src/window/window.c`, `src/winmsg/winmsg.c`, `src/mpchat/mpchat.c`: add the markers and notes.
- [x] `docs/retail-bugs.md`: correct the rows, add "Not reproduced" sentences, and apply any status changes.
- [x] `tools/retail_bugs_unmarked.txt`: drop the ids marked or obsolete.
- [x] `docs/tools.md`: update the check's Baselines line.
- [x] This spec: an `## Implementation Notes` section with the per-part checklist.

**Acceptance Criteria:**
- Given the slice is done, when `python3 -B tools/check_retail_bugs.py` runs, then it exits 0 with no WIN, WMS or MPC id allowlisted except those reported. The summary is `331 entries, 161 marked, 167 allowlisted, 3 no longer apply`; each further entry reported moves one from marked to allowlisted.
- Given the builds before and after, when the hash lists are diffed, then the diff is empty, and each PE built from these modules was rebuilt.

## Implementation Notes

Per-part checklist (lines as of the final sources; `w:` = `src/window/window.c`, `m:` = `src/winmsg/winmsg.c`, `c:` = `src/mpchat/mpchat.c`, `u:` = `src/util/util_math.c`). Statuses changed: WIN-4 and MPC-7 `(fixed, not kept)`; WMS-7 and MPC-8 `(no longer applies)`. Nothing was left allowlisted.
- WIN-1: `w:1616` (`div16`'s saturation), `:1632` (`idiv16`'s), `:1861` (aspect's two divides; they saturate only through the 16-bit truncation), `:3211` (`look_vector`, `.text+0x50fa`), `:3446` (`matrix_angles`). `zoom_factor`'s `div16(BP)` is guarded by `DX < BP`, so is not a site.
- WIN-2: `w:216` (the code tail, copied by the unedited `gen_window_data.py`), `:2185` (`zoom_height`'s index, the reads 0x7ac bytes past the masks), `:2254` (`eye_map`'s other branch: the signed clamp a zoom with bit 15 set passes), `:2264` (the unclamped GLOBAL `+0x390` 2 path).
- WIN-3: creation `w:287`, removal `:335` (the record's high half), title update `:750` (the new title pointer's).
- WIN-4: `w:720` (`(fixed, not kept)`: the buffer starts empty).
- WIN-5: `w:1391`.
- WIN-6: `w:2940`.
- WIN-7: `w:1303` (the stored pointers), `:1315` (the stale reads; cases 11 and 0x1e use the same pointers).
- WIN-8: WINDOW `w:1701` (the model), the `eax = UTIL;` lines `:1809` (sincos), `:1985` (`+0x4c`), `:2008` (`+0x44`), `:2113`, `:2149`, `:2794` (`+0x5c`), `:2361` (sincos), `:2403` (`+0x40`), `:3230` (`+0x48`); UTIL `u:404` (the `+0x5c` stub) and `:1640` (the `KEEP_ALL` wrappers).
- WMS-1: start `m:318`; the copy destination `:523`; the frees `:396` (`slot_text`), `:675` (deinit), `:700` (`show`), `:726` (`clear_slot`). Row's trigger corrected: one byte early stays in `+0x94..+0x97`; slot 6's pointer needs `at == -5` (speed 41 and over, 78 frames a second).
- WMS-2: `m:514` (the unsigned room, with the Not-reproduced note on the PE layout, which is not a bounds check, and the length dependency), `:529` (the trailing NUL); the unchecked copy is the WMS-1 line `:523`, which the note covers too.
- WMS-3: `m:281`. WMS-4: `m:355`. WMS-5: `m:357`, `:370`. WMS-6: `m:348`, `:425`.
- WMS-7 `(no longer applies)`: the C call passes only the fill byte and HOST keeps only the low byte, so there is nothing to reproduce (`winmsg.c`'s `blank`); no marker.
- WMS-8: `m:592` (the first free); the second free is WINDOW's `destroy_record`, marked at `w:339`.
- WMS-9: Linkage returns `m:627` (`clear_all`) and `:741` (`clear_slot`); list 0x14 callback returns `:606` (`data`) and `:686` (`message - 0x37`).
- MPC-1: `c:357`. MPC-2: `c:275`. MPC-3: `c:290` (`0x3ff` is the first overrun; the harness keeps names under 0x300). MPC-4: `c:292`. MPC-5: `c:239`.
- MPC-6: `c:539`; MULTIP's side is cited in the marker and row, not marked.
- MPC-7: `c:233` (`(fixed, not kept)`: both structures zeroed).
- MPC-8 `(no longer applies)`: ours omits the return CHAIN discards (the note at `c:438`); no marker.

Verification: `check_retail_bugs.py --self-test` passes and the check reports `331 entries, 161 marked, 167 allowlisted, 3 no longer apply`. All `bin/` PEs (`peh3.py`, stripped and unstripped) hash the same before and after (`pre15e/`, `post15e/`). Rebuilt after the last edit: `WINDOW.DLL`, `WINMSG.DLL`, `MPCHAT.DLL`, `UTIL.DLL`, and `slewdiff.exe`, `utildiff.exe`, `utildiff_portable.exe` (mtimes newer than the sources); scratch builds of `WINDOW_ours.dll`, `WINMSG_ours.dll`, `MPCHAT_ours.dll`, `mpchat-ui/MPCHAT_ours.dll`, `OFSTEST.DLL` and `UTIL_TRACE.DLL` (`sb15e.sh`) are identical before and after. Caveat: the inventory lists carry the same `ERR` lines before and after for files whose names contain spaces (wine prefixes under `bin/`), which `xargs` splits; none is a built PE.

## Review Triage Log

Spec review 1 (Codex gpt-6.2-sol), all applied:
1. high: WIN-4 is deliberately fixed, so `(fixed, not kept)`.
2. high: WMS-1's trigger, and the copy and frees as separate sites.
3. high: scratch builds use the same relative names; ERR fails; the break recipes take the modules' own sources; mpchat-ui is one recipe.
4. medium: WIN-8's sites are in WINDOW and UTIL, with UTIL's relink and scratch obligations.
5. medium: WMS-7 and MPC-8 `(no longer applies)`, MPC-7 `(fixed, not kept)`; the counts become 161/167/3.
6. medium: WMS-2: both writes remain; the note describes the layout difference.
7. medium: WIN-3: creation, removal and title update.
8. medium: WMS-8's WINDOW site is marked; MPC-6's MULTIP side is cited, not marked.
9. low: WMS-9's four return paths.
10. low: MPC-3's threshold is `0x3ff`.

Code review 1 (Codex gpt-6.2-astra: blind, edge case, verification gap):
- B1/E1/V1 false -- "18 FACIL/FE ids dropped without markers": the review diff left out slice 4's uncommitted sources; their markers are in the worktree, and the check passes.
- B2/E2 medium, patch -- WIN-2: the other `eye_map` branch's signed clamp lets a zoom with bit 15 set reach the tail; now marked.
- B3/E3 low, patch -- WIN-2: 0x803 is the offset from the masks' start; the overread is 0x7ac past them.
- B4/E4 medium, patch -- WIN-1: the aspect divides the shorter side by the longer; only 16-bit truncation saturates.
- B5/E5 low, patch -- WIN-3: a different id only when the high halves differ.
- E11 low, patch -- WIN-8: one `eax = UTIL`, after the third `U_SINCOS`.
- B6/E6/E7/E8 low, patch -- WMS-1: the bounds of the countdown; slot 6 is hit from `at == -5`, i.e. 78 frames a second, not "hundreds".
- B7/E9 low, patch -- WMS-2: the write's reach depends on the message length.
- B9/E10 medium, patch -- WMS-8's second free in WINDOW was unmarked, against the spec; now marked (`window.c:339`).
- B8/E12 low, patch -- FACIL-2 (slice 4's text) is restricted to failures reaching the common exit.

After the patches: the check passes (161 marked, 167 allowlisted, 3 no longer apply). WINDOW.DLL and WINMSG.DLL were relinked after the last edit; they, MPCHAT.DLL and UTIL.DLL hash as in `pre15e/`; the six scratch links match. The checklist was refreshed to the final lines.

## Verification

**Commands:**
- `python3 -B tools/check_retail_bugs.py --self-test && python3 -B tools/check_retail_bugs.py`. Expected: the self-test passes, then the summary line, exit 0.
- `nice -n 19 cmake --build build/win32-retail -j 2 && CMAKE_BUILD_PARALLEL_LEVEL=2 nice -n 19 tools/ofs difftests`, then hash again, scratch-build again, and `diff` against `pre15e/`. Expected: empty.
