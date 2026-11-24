---
title: 'Story 1.5, slice 2: SIM1, HOST and the runtime retail bugs marked'
type: 'chore'
created: '2026-11-23'
status: 'draft'
route: 'dispatch'
baseline_commit: '71be387e40884677de3a30d00fab3fccf6e09875' # worktree holds uncommitted Stories 1.2-1.5 slice 1
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
- A part of an entry that our code does not reproduce gets a plain `Not reproduced (ID): ...` note, not a marker, and a "Not reproduced: ...; ours ..." sentence in the entry (slice 1's B1-B4). An entry none of whose parts is reproduced is reported, not given a status.
- SIM1-AIR-FRAME-OVERRUN gets a `RETAIL BUG (fixed, not kept): SIM1-AIR-FRAME-OVERRUN` marker at the bounded copy in `sim1_air_decode`.
- Builds run under `nice -n 19` with at most 2 jobs while the Story 1.4 rerun in `~/openfs98-verify/p14-rerun` runs.
- No staging or commits.

**Never:**
- No section outside SIM1, the runtime and HOST is marked; no `tools/` file except `tools/retail_bugs_unmarked.txt` is edited; no retail bug is fixed; no HOST-13 is invented.

</frozen-after-approval>

## Code Map

- `docs/retail-bugs.md:117-181`: SIM1 (lines 117-128), runtime (145-152) and HOST (154-181). The HOST Where cells give service slots; check `docs/modules/host.md` for their VAs.
- `src/host/host.c` (5544 lines): every HOST service, as `host_<slot>`, with a `/* +0xNNN (0xVA): ... */` header comment. The service table is at `:5425` onwards. The list code (HOST-4/5/6) is at `:389-530`: `host_2f8` append, `host_list_remove_at`, `host_300`, the walk's queue at `:448`, `host_310`, `host_2fc`. If a site is outside `host.c` (the menu tree, the voices, the tooltips, the modal loop), search `src/host/` (`src/host/runtime/`, `host_rt_exe.c`).
- `src/sim1/sim1_small.c:4855-4864`: `sim1_air_decode`'s header already describes the bounded copy; the marker goes at the copy itself in its 0x302 case.
- `tools/retail_bugs_unmarked.txt`: remove the 23 ids as they are marked.
- `tools/check_retail_bugs.py`: use it, don't edit it.
- `/tmp/claude-1000/-home-steve-source/b2e3078b-1d77-426e-816f-2de2772a67dc/scratchpad/peh3.py`: the hash lister; `pre15/` there holds slice 1's method.

## Tasks & Acceptance

**Execution:**
- [ ] Before any edit, hash every PE under `bin/` with `peh3.py` into a new `pre15b/` in the scratchpad (sorted by path).
- [ ] `src/host/host.c` (and any other `src/host/` file holding a site): add the HOST-1..23 markers, and notes where a part is not reproduced.
- [ ] `src/sim1/sim1_small.c`: add the SIM1-AIR-FRAME-OVERRUN `(fixed, not kept)` marker, and change the header's prose mention to name the marker.
- [ ] `docs/retail-bugs.md`: correct wrong Where cells and add "Not reproduced" sentences.
- [ ] `tools/retail_bugs_unmarked.txt`: drop the 23 ids.
- [ ] `docs/tools.md`: update the check's Baselines summary line (`:284`).

**Acceptance Criteria:**
- Given the slice is done, when `python3 -B tools/check_retail_bugs.py` runs, then it exits 0 with 231 allowlisted (`331 entries, 100 marked, 231 allowlisted, 0 no longer apply`), and no HOST, SIM1 or runtime id is allowlisted. The counts may differ only if the slice reports a reason.
- Given the builds before and after, when the `peh3.py` lists are diffed, then the diff is empty.

## Design Notes

HOST-13 has never existed in the register, the source or the history; the numbering gap stays.

## Verification

**Commands:**
- `python3 -B tools/check_retail_bugs.py --self-test && python3 -B tools/check_retail_bugs.py`. Expected: the self-test passes, then the summary line, exit 0.
- `nice -n 19 cmake --build build/win32-retail -j 2 && nice -n 19 tools/ofs difftests JOBS=2`, then hash again and `diff` against `pre15b/`. Expected: empty.
