---
title: 'Story 1.5, slice 2: SIM1, HOST and the runtime retail bugs marked'
type: 'chore'
created: '2026-11-23'
status: 'done'
route: 'dispatch'
baseline_commit: '21c827a657a34d68fef78d576b8e6c720c921a81' # Story 1.5 slice 1 committed; worktree holds uncommitted 1.4 load-limit docs
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-1-5-every-kept-retail-bug-is-marked-in-the-code.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** After slice 1, `tools/check_retail_bugs.py` passes with 254 ids allowlisted. 23 of them belong to the second slice: HOST-1..HOST-23 (22 entries; there is no HOST-13) and SIM1-AIR-FRAME-OVERRUN, which has only a prose mention. The runtime section's four entries are already marked.

**Approach:** Read each entry's code, confirm or correct its register row, mark every site it names, and remove its id from the allowlist. This follows slice 1's rules (its spec's Boundaries, status table and marker grammar), unchanged.

## Boundaries & Constraints

**Always:**
- Comments only. Every PE under `bin/` (`.dll`, `.exe`, `.gau`, `.sim`, `.mdl`, harness exes included, `bin/break-pool` excluded) must hash the same before and after: the `peh3.py` lists, stripped and unstripped, must `diff` empty, with the same file list.
- Marking an entry means reading the code it names. Fix the Where cell when it is wrong (service slot, retail VA, file); for example, HOST-4 says `+0x2f8 (list remove)`, but `+0x2f8` is append (`host.c:389`). Then put a marker at each site the entry names, which for a multi-part entry means each part's site.
- A part of an entry that our code does not reproduce gets a plain `Not reproduced (ID): ...` note, not a marker, and a "Not reproduced: ...; ours ..." sentence in the entry (slice 1's B1-B4). An entry none of whose parts is reproduced stays allowlisted, with a `#` comment saying why, and is reported for the user to decide its status; the counts in the acceptance criteria drop by one for each.
- The check counts ids, not sites, so one marker satisfies a multi-part entry. The slice therefore leaves a checklist in Implementation Notes: for each id, each behaviour (part) it names, and the file:line of its marker or Not-reproduced note.
- In SIM1, only Where cells that are found wrong are corrected; SIM1-AIR-SIZE-HANDLE's must also name `src/sim1/sim1_small.c`, the native loader (its marker is at `:5546`).
- SIM1-AIR-FRAME-OVERRUN gets a `RETAIL BUG (fixed, not kept): SIM1-AIR-FRAME-OVERRUN` marker at the bounded copy in `sim1_air_decode`.
- Builds run under `nice -n 19` with at most 2 jobs while the Story 1.4 rerun in `~/openfs98-verify/p14-rerun` runs.
- No staging or commits.

**Never:**
- No section outside SIM1, the runtime and HOST is marked; no `tools/` file except `tools/retail_bugs_unmarked.txt` is edited; no retail bug is fixed; no HOST-13 is invented.

</frozen-after-approval>

## Code Map

- `docs/retail-bugs.md:117-181`: SIM1 (lines 117-128), runtime (145-152) and HOST (154-181). The HOST Where cells give service slots; check `docs/modules/host.md` for their VAs.
- `src/host/host.c` (5544 lines): every HOST service, as `host_<slot>`, with a `/* +0xNNN (0xVA): ... */` header comment. The service table is at `:5425` onwards. The list code (HOST-4/5/6) is at `:389-530`: `host_2f8` append, `host_list_remove_at`, `host_300`, the walk's queue at `:448`, `host_310`, `host_2fc`. If a site is outside `host.c` (the menu tree, the voices, the tooltips, the modal loop), search `src/host/` (`src/host/runtime/`, `host_rt_exe.c`).
- Where cells already known wrong or incomplete:
  - HOST-4 names `+0x2f8`, but the removal is `host_list_remove_at`/`host_300` (`:404`, `:429`);
  - HOST-3's trampoline also serves `+0x234` bsearch (`:550`);
  - HOST-8's finder `host_opt_find` also serves `+0xa4` (`:703`).

  Where an entry's bug lives in a shared helper, mark the helper and name its callers in the cell.
- HOST-18's last part (a start that fails at the primary buffer) is in `src/host/runtime/host_rt_common.c:172` (`fs98_host_rt_start_sound`); its duplicate and append parts are around `host_2d4`/`host_2d8` (`host.c:3496`, `:3602`).
- `src/sim1/sim1_small.c:4855-4864`: `sim1_air_decode`'s header already describes the bounded copy; the marker goes at the copy itself in its 0x302 case.
- `tools/retail_bugs_unmarked.txt`: remove the 23 ids as they are marked.
- `tools/check_retail_bugs.py`: use it, don't edit it.
- `/tmp/claude-1000/-home-steve-source/b2e3078b-1d77-426e-816f-2de2772a67dc/scratchpad/peh3.py`: the hash lister; `pre15/` there holds slice 1's method.

## Tasks & Acceptance

**Execution:**
- [x] Before any edit, hash every PE under `bin/` with `peh3.py` into a new `pre15b/` in the scratchpad (sorted by path).
- [x] `src/host/host.c` (and any other `src/host/` file holding a site): add the HOST-1..23 markers, and notes where a part is not reproduced.
- [x] `src/sim1/sim1_small.c`: add the SIM1-AIR-FRAME-OVERRUN `(fixed, not kept)` marker, and change the header's prose mention to name the marker.
- [x] `docs/retail-bugs.md`: correct wrong Where cells and add "Not reproduced" sentences.
- [x] `tools/retail_bugs_unmarked.txt`: drop the 23 ids.
- [x] `docs/tools.md`: update the check's Baselines summary line (`:284`).

**Acceptance Criteria:**
- Given the slice is done, when `python3 -B tools/check_retail_bugs.py` runs, then it exits 0 with 231 allowlisted (`331 entries, 100 marked, 231 allowlisted, 0 no longer apply`), and no HOST, SIM1 or runtime id is allowlisted. The counts may differ only if the slice reports a reason.
- Given the builds before and after, when the `peh3.py` lists are diffed, then the diff is empty.

## Review Triage Log

Spec review 1 (Codex):
1. high, applied: the check counts ids, not sites; added the per-part checklist.
2. medium, applied: named the HOST-3, 4 and 8 Where corrections, and the rule for shared helpers.
3. medium, applied: HOST-18's primary-buffer site in `host_rt_common.c`.
4. medium, applied narrowly: the SIM1-AIR-SIZE-HANDLE cell gains `sim1_small.c`; other marked SIM1 entries are corrected only if found wrong.
5. medium, applied: an entry with no reproduced part stays allowlisted and is reported.
6. low, rejected: `peh3.py` is the session's gate tool, and the gate already fails on missing files.
7. low, applied: `CMAKE_BUILD_PARALLEL_LEVEL=2` instead of `JOBS=2`.

Code review 1 (Codex gpt-6.2-astra: blind, edge case, verification gap). Before it, I checked that `host.c`/`host_rt_common.c` recompiled: `bin/host-rt/OPENFS98.EXE` was relinked with the same hash; `bin/HOST.DLL` (built only by the HOST harness targets) was rebuilt in the scratchpad and matched too.
- B1/E1 low, patch -- HOST-8 "every command line": the byte before is read only when the command line starts with `/` or `-`.
- B2/E2 low, patch -- HOST-3's trigger covers bsearch too: a sort or bsearch inside either's comparator.
- B3 low, patch -- HOST-5: requeued entries stay until another removal sets the flag, not until "a later walk".
- B4 low, patch -- HOST-6 marker missed `+0x300`'s queue-entry return during a walk.
- B5 low, patch -- HOST-9 "gives 0" is `*out`, not the return.
- B6 low, patch -- HOST-11's writer: retail's format at `0x417ed8` is `%f` (implementer read it from `orig/FLTSIM98.EXE`), so a large double overruns the 0x24-byte temporary; added to the entry.
- B7/E4 low, patch -- HOST-17: no submenu reads through null; an unresolvable parent returns -1; a wrong resolvable one overruns.
- B8 low, patch -- HOST-22's pump marker: both records must be arrays.
- E3 low, patch -- HOST-1: a heap pointer whose top byte is 0x5c leaves the search relative.
- E5 medium, patch -- HOST-18's copy from null is `host_voice_buffer`'s `host_21c`, which had no marker.
- E6 low, patch -- HOST-22: an array factory returning null faults at `M32(w)` before the -1.
- V1 false, carried -- shrink-only baseline: slice 1's spec review row 9.

After the patches: the check passes (100 marked, 231 allowlisted). The rebuild relinked only the two OPENFS98.EXE copies, and they and a scratch-built HOST.DLL hash as before.

## Design Notes

HOST-13 has never existed in the register, the source or the history; the numbering gap stays.

## Verification

**Commands:**
- `python3 -B tools/check_retail_bugs.py --self-test && python3 -B tools/check_retail_bugs.py`. Expected: the self-test passes, then the summary line, exit 0.
- `nice -n 19 cmake --build build/win32-retail -j 2 && CMAKE_BUILD_PARALLEL_LEVEL=2 nice -n 19 tools/ofs difftests`, then hash again and `diff` against `pre15b/`. Expected: empty.

## Implementation Notes

Markers (all `RETAIL BUG (kept)` except FRAME-OVERRUN, `(fixed, not kept)`), file:line in `src/host/host.c` unless named. No part was found unreproduced, so no Not-reproduced note was needed.

- HOST-1: empty dir reads the byte before buf, 2193.
- HOST-2: unchecked 0x208 work buffer, 1043.
- HOST-3: global comparator and uncleaned trampoline (+0x22c, +0x234), 541.
- HOST-4: count drop for any index, 417 (queued removals reach it through `host_list_flush`).
- HOST-5: depth not consulted at the walk's end, 443.
- HOST-6: capacity left by +0x2fc and its return, 495; +0x300's eax return, 426; +0x310 zero length, 529.
- HOST-7: non-fatal second failure, 314.
- HOST-8: byte before the command line, 635 (`host_opt_find`, for +0x98/+0x9c/+0xa0/+0xa4).
- HOST-9: return values, 941 (+0x1d0) and 960 (+0x1d4: past 64/16 gives 0, axis prefix after a button prefix).
- HOST-10: unchecked `len - n - 1`, 1448 and 1450.
- HOST-11: 15/30-byte-per-entry block, read 1590, write 1621.
- HOST-12: leaked path copy, 2124.
- HOST-14: strcpy test the wrong way round, 3911.
- HOST-15: 32-bit product, 2798.
- HOST-16: folded flag not set for Enter, 2653.
- HOST-17: no submenu 3017 (+0x244) and 3032 (+0x248); parent-id lookup 3048 (+0x24c), 3070 (+0x250), 3094 (+0x254); rebuild before redraw 3115; +0x25c's lookup and unfreed submenu 3146.
- HOST-18: duplicate fails and frees shared sound 3580; copy from null 3563 (+0x2d4), 3648 (+0x2d8); append to a duplicate frees the shared sound 3652; unchecked buffer 3716 (+0x2e4), 3755 (+0x2f0), 3777 (+0x2f4), 3797 (+0x2e8, voice too); primary-buffer failure `host_rt_common.c:173`.
- HOST-19: block not freed 3955 (+0x074); lock reads block unchecked 3968 (+0x06c); row table refill 4003; +0x068 5079; restack `host_rt_common.c:204`.
- HOST-20: tools before check 5138; outermost bounds 5248; top-level nodes as groups 5270.
- HOST-21: font per call, 4192.
- HOST-22: reused register 4851; failed factory without release 4825.
- HOST-23: 16-bit axis count, 2465.
- SIM1-AIR-FRAME-OVERRUN: `src/sim1/sim1_small.c:4918`; the header prose now names it.

Unverified: HOST-19 says the locks read the block unchecked; ours reads it in +0x06c only (+0x070 touches no block), so the entry's plural stands on the register's word.

Verification: self-test passes; check prints `331 entries, 100 marked, 231 allowlisted, 0 no longer apply`; `cmake --build` and `tools/ofs difftests` (both exit 0); `peh3.py` lists of every PE under `bin/` (115446 files, `bin/break-pool` excluded), stripped and unstripped, diff empty before and after. Stripped hashes were taken once per distinct unstripped hash (the 5196 distinct PEs), since stripping 106 GB of per-run copies is impractical.
