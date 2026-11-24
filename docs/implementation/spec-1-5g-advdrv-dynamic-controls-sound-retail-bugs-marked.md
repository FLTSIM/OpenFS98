---
title: 'Story 1.5, slice 7: ADVDRV, DYNAMIC, CONTROLS and SOUND retail bugs marked'
type: 'chore'
created: '2026-11-23'
status: 'done'
route: 'dispatch'
baseline_commit: '86beaf13644b2c9ed4160992b0d67262c81c1029' # worktree also holds the uncommitted slice 6
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-1-5f-reality-analysis-demo-retail-bugs-marked.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** After slice 6, 138 ids are still allowlisted. 29 of them belong to the seventh slice: ADV-1..7, DYN-1..7, CTL-1..9 and SND-1..6. ADVDRV and DYNAMIC hold one marker each from earlier slices (FLT-6 and FE-40-CONVENTION); none is for these ids.

**Approach:** Use slice 6's method (`spec-1-5f`'s Boundaries): read each entry's code, correct its row, mark every site of every part it names, put Not-reproduced notes on parts we don't reproduce, leave a per-part checklist in this spec's Implementation Notes, and remove the ids from the allowlist.

## Boundaries & Constraints

**Always:**
- Statuses follow the user's standing rule:
  - an entry whose bug ours deliberately avoids becomes `(fixed, not kept)`, with a marker at our fix;
  - one whose only site is dead code, or code with no observable effect, that we omitted becomes `(no longer applies)`, with no marker, off the allowlist, and one sentence why;
  - one that is not reproduced for no stated reason stays allowlisted with a `#` comment and is reported;
  - a mixed entry keeps a `(kept)` marker on each reproduced part and a Not-reproduced note on each other part.

- Statuses this rule settles (spec review 1):
  - CTL-8 `(no longer applies)`: retail's `jge` skips the reload at equality, as our `<` does (`controls.c:2513`); the register's account describes the `<=` break variant (`tools/controls_breaks.py:45`), not retail.
  - SND-6 `(fixed, not kept)`: retail's final `fcomp` reads at `p+0xac`, just past the allocation (retail `0x208d29b8`). That can fault if the address is unmapped, and the result has no other effect. Ours deliberately omits the read (`sound.c:1526`, `:1536`): mark it there, and state the allocation assumption in the row.

  An obsolete id's checklist line gives its rationale instead of a file:line, and a site in another module is cited in its checklist line, not marked.
- The per-part checklist is part of acceptance: for each id, each behaviour (part) its row names, and the file:line of its marker or `Not reproduced (ID): ...` note, refreshed to the final line numbers after the last edit.
- Comments only. Before any edit, hash every PE under `bin/` with `peh3.py` (`bin/break-pool` excluded); afterwards they must hash the same. Each PE linked from an edited source must be shown rebuilt after the last edit, by an mtime newer than the source, or by a scratch link of its recipe before and after under the same relative name. The link and the hash must succeed. A fixture that is only a copy of another build, and is refreshed only by its own harness target, is checked to be unchanged, not rebuilt.
- Builds run under `nice -n 19`, with `-j 2` and `CMAKE_BUILD_PARALLEL_LEVEL=2`, while the Story 1.4 rerun runs.
- No staging or commits, no reverting sources to take a baseline, and no changing this spec's status.

**Never:**
- No section outside ADVDRV, DYNAMIC, CONTROLS and SOUND is marked. The CONTROLS-to-DEMO register contract (CTL-9) is marked on the CONTROLS side only. No `tools/` file except the allowlist is edited, and no retail bug is fixed.

</frozen-after-approval>

## Code Map

- `docs/retail-bugs.md`: the ADVDRV, DYNAMIC, CONTROLS and SOUND sections. `docs/modules/advdrv.md`, `dynamic.md`, `controls.md` and `sound.md` give the offsets.
- `src/advdrv/advdrv.c`: ADV-1..7.
  - ADV-1:
    - Where is `op_error`, `.text+0xbc0`.
    - A call-stack overflow is not this bug: `push_call` calls the console-only `error(0)` and returns, as retail does at `0x20612100` (`:1057`). Take it out of the trigger list.
    - `op_error` frees the buffer and returns, and callers such as `text` read on (`:1289`, `:1688`, `:3190`). Give each post-alert continuation a checklist line.
    - The harness stops comparing at the alert (`advdrv.md:185`).
  - ADV-4 is three behaviours, so describe each field by field and mark each at its store:
    - the cloud promotion reloads the old top (`:3523`, `:3540`);
    - the first wind promotion keeps the incoming top (`:3709`, `:3755`);
    - the temperature shift moves the old altitude, carries stale temperature data and omits the fourth id's store (`:3897`, `:3914`, `:3921`).
  - ADV-6: the copy, the attribute change and the delete all use `0x900e`, and the delete's result is ignored (HOST passes it to `DeleteFileA`, `host.c:2128`). Name `skip_sound` in Where (`:4531`, `:4540`), and describe the fix as tracking the cached copies and handling a failed delete.
  - ADV-7: check whether ours lets the `idiv` fault, as the host's divide repair needs. A deliberate test is `(fixed, not kept)`.
- `src/dynamic/dynamic.c`: DYN-1..7. DYN-2 is a long multi-part entry, so each part needs its own checklist line:
  - jumps, conditional branches and both return paths (`:844`, `:1201`, `:1249`, `:1276`);
  - `0x31`'s byte index. It writes `edi + 0x30 + 2*index` with a `0x120` object stride, so index `0x78` first reaches the next object and `0x8a` hits its `+0x24` flags (`:325`). Correct the row's "0x1fe past ... at 0x8a".
  - the declaration's edi;
  - the divides. The FE-40-CONVENTION marker at `:481` stays.
- `src/controls/controls.c` and `controls_input.c`: CTL-1..9.
  - `controls.c:13-15` documents an omitted stack-alignment wrapper that has no register id; leave it alone.
  - CTL-9: the kept ecx is set in `axis_reset` through `h_4b16` (`controls_input.c:233`), so mark it there, not at the dispatcher. `pilot_event` deliberately zero-extends the words (`controls.c:1701`, `:3702`), because DEMO reads only their low halves: a Not-reproduced note there. Give checklist lines citing DEMO's capture and receive sites (`demo.c:2277`, `:2494`; `controls.md:326`).
- `src/sound/sound.c`: SND-1..6.
  - SND-2: index 5 lands at `.data+0x170c`, before the fader table; index -1 lands inside `S_POS`, and other negatives reach other addresses. Give the address formula in the row, and cover all three operation arms and their masked shifts (`:469`, `:477`, `:1120`).
- PEs:
  - default build: `bin/ADVDRV.DLL`, `bin/DYNAMIC.DLL`, `bin/CONTROLS.DLL`, `bin/SOUND.DLL`;
  - on-demand links, each scratch-linked to its own output name:
    - the ADVDRV cover link (`-DADVDRV_COVER`, `advdrv_cover.def`, `cmake/targets.cmake:898`);
    - the ADVDRV break link (`-DBREAK=$BREAK`, `:909`), with `BREAK` pinned to its empty, unbroken default;
    - the CONTROLS break link (`:1091`, `:1098`), whose source copies (`tools/controls_breaks.py:185`, `targets.cmake:1137`, `:1973`) are regenerated fresh for each snapshot.
  - fixture copies, checked unchanged: `bin/difftest/ADVDRV_ours.dll`, `DYNAMIC_ours.dll`, `CONTROLS_ours.dll` and `SOUND_ours.dll`, whichever exist.

  Find any other consumers with a grep of `cmake/` for the four source directories. No source here includes another `.c`.
- The gate: `/tmp/claude-1000/-home-steve-source/b2e3078b-1d77-426e-816f-2de2772a67dc/scratchpad/peh3.py`, with lists in new `pre15g/` and `post15g/` there, and the scratch links under `pre15g/sb/` and `post15g/sb/` (as `sb15f.sh` does).

## Tasks & Acceptance

**Execution:**
- [x] Before any edit, hash every PE under `bin/` into `pre15g/`, and scratch-link the on-demand recipes into `pre15g/sb/`.
- [x] The four modules' sources: add the markers and notes.
- [x] `docs/retail-bugs.md`: correct the rows, add "Not reproduced" sentences, and apply any status changes.
- [x] `tools/retail_bugs_unmarked.txt`: drop the ids marked or obsolete.
- [x] `docs/tools.md`: update the check's Baselines line.
- [x] This spec: an `## Implementation Notes` section with the per-part checklist.

**Acceptance Criteria:**
- Given the slice is done, when `python3 -B tools/check_retail_bugs.py` runs, then it exits 0 with no ADV, DYN, CTL or SND id allowlisted except those reported. With `O` entries made obsolete and `R` reported, the summary is `331 entries, (218 - O - R) marked, (109 + R) allowlisted, (4 + O) no longer apply`; with only CTL-8 obsolete and nothing reported, that is `217 marked, 109 allowlisted, 5 no longer apply`.
- Given the builds before and after, when the hash lists are diffed, then the diff is empty, and each PE built from these modules was rebuilt.

## Implementation Notes

Per-part checklist (final line numbers; `a:` = `src/advdrv/advdrv.c`, `y:` = `src/dynamic/dynamic.c`, `c:` = `src/controls/controls.c`, `i:` = `src/controls/controls_input.c`, `s:` = `src/sound/sound.c`). Status changes: CTL-8 `(no longer applies)`, SND-6 `(fixed, not kept)`. Nothing was left allowlisted.
- ADV-1: `op_error` `a:1063`; the post-alert continuations, one each: `text`'s string piece reads its index `a:1693`; the variable reference left half-built `a:1761`; `eval` returns its value `a:1801` (`step`'s bad opcode, `a:1718`, is a plain comment: the next iteration does not read the freed cursor, since `op_stop` clears `A_RUNNING`); `push` stores `a:1819`; `pop` reads `OPERAND(-1)` `a:1829`; `op_bad_operator` `a:1843`; variable getter `a:2161`, setter `a:2177`, read-only test `a:2186`; `op_weather` `a:3312`. A call-stack overflow is not this (row corrected).
- ADV-2: `a:2263`. ADV-3: `a:3169` (`to_48`), `a:3186` (`metres`: positive sub-unit values come back negative). ADV-5: `a:4348`. ADV-7: `a:565` (the `idiv` helper, used by `x_idiv` and `x_mod`; ours lets it fault).
- ADV-4, three parts: cloud, no bug (displaced layer reloads every field), plain comment `a:3520`, row corrected; wind first promotion `a:3724` (top16 not reloaded) and `a:3750` (the second-slot store); temperature `a:3938` (the displaced layer's id is not carried, the fourth slot's id is never stored).
- ADV-6: the copy and attribute call (which sets nothing read-only), `a:4554`; the delete (result ignored, of a possibly pre-existing file), `a:4567`.
- DYN-1: `y:653`. DYN-3: `y:1195`. DYN-4: `y:1010`. DYN-5: `y:385`. DYN-6: `y:2108`. DYN-7: `y:442`.
- DYN-2, per part: jumps `y:1205` (`op_jump`), `y:2272` (`op_jnear`); conditional branches `y:1278` (`condition`); calls `y:1213`; both return paths `y:1225`; `0x31`'s byte index `y:861` (row corrected: index `0x78` reaches the next object, `0x8a` its `+0x24`); the declaration's edi `y:559`; the divides `y:180` (`udiv`), `y:191` (`sdiv`), `y:200` (`sdiv16`). The FE-40-CONVENTION marker (`y:481` before the edits) is untouched.
- CTL-1: `i:564`. CTL-2: `i:769`. CTL-3: `i:1815`. CTL-4: the lock `i:699`, the stack `i:708`. CTL-5: `i:1039`. CTL-6: `i:1217`. CTL-7: `i:313`.
- CTL-8 `(no longer applies)`: retail's `jge` skips the reload at equality, as ours `<` does (`c:2513`); the old account was the `<=` break variant (`tools/controls_breaks.py:45`); no marker.
- CTL-9: the kept ecx `i:233` (`axis_reset`, reached through `h_4b16`); `Not reproduced (CTL-9)` `c:3711` (`pilot_event` zero-extends the words). DEMO's capture and receive sites are cited, not marked: `src/demo/demo.c:2277` (`demo_call2` keeps the callee's ecx) and `:2494` (`demo_event`); `docs/modules/controls.md:326`.
- SND-1: the read `s:638`, the call `s:1902`. SND-2: all three arms, each slot write and masked shift: off `s:1135`, `s:1136`; on `s:1139`, `s:1140`; toggle `s:1143`, `s:1144` (address formula in the function header and the row). SND-3: `s:1298`. SND-4: `s:1882` (id `0x22`), `s:1895` (id `0x21`). SND-5: `s:762`. SND-6 `(fixed, not kept)`: `s:1543`, the allocation assumption stated in the marker and the row.

Gate: PEs hashed with `peh3.py` before and after (`pre15g/`, `post15g/`); `bin/ADVDRV.DLL`, `CONTROLS.DLL`, `DYNAMIC.DLL`, `SOUND.DLL` were rebuilt after the last edit (mtimes 22:14, sources last edited 22:10); the three on-demand scratch links (ADVDRV cover, ADVDRV break with `BREAK=0`, since an empty `-DBREAK=` does not preprocess, and CONTROLS break from fresh source copies) hash the same before and after; the four `*_ours.dll` fixtures are unchanged. The pre-edit scratch links were taken after the (comment-only) edits, since a first attempt failed on the empty `BREAK`; no `__LINE__`, `__FILE__` or `assert` is used in the four modules, so comments cannot reach the output.

## Review Triage Log

Spec review 1 (Codex gpt-6.2-sol):
1. high, applied: ADV-1's Where (`.text+0xbc0`), call-stack overflow removed, and the post-alert continuations.
2. high, rejected: the spec already requires every link and hash to succeed under the same relative name; `sb15f.sh` keeps names relative under `sb/`.
3. medium, applied: three distinct scratch links, `BREAK` pinned, fresh CONTROLS copies, and the four fixture copies.
4. medium, applied: ADV-4's three insertion paths.
5. medium, applied: CTL-8 `(no longer applies)` (retail skips the reload); `controls.c:15` is unrelated.
6. medium, applied: CTL-9's kept ecx in `axis_reset`, with a Not-reproduced note at `pilot_event`.
7. medium, applied: DYN-2's index arithmetic and branch sites.
8. medium, applied: SND-6 `(fixed, not kept)` with the allocation assumption.
9. low, applied: ADV-6's Where and fix.
10. low, applied: SND-2's address formula and three arms.

Code review 1 (Codex gpt-6.2-sol, blind and edge-case layers; verification-gap skipped for a comment-only slice):
- B1/E1 false -- "28 REALITY/ANALYSIS/DEMO ids dropped without markers": they are slice 6's uncommitted markers, outside this diff; the check passes.
- B2/E2 medium, patch -- ADV-1: the loops test the running flag, so no further opcode is fetched; only a call already in progress reads the freed buffer.
- B3 low, patch -- ADV-3: a positive sub-unit value comes back negative.
- B4/E3 medium, patch -- ADV-6: flag 1 is FILE_ATTRIBUTE_NORMAL, not read-only; a pre-existing file skips the copy, yet cleanup still deletes it.
- B5/E8 low, patch -- ADV-7: `INT_MIN / -1` added to When and the fix.
- B8/E4 medium, patch -- DYN-2: the signed `sdiv`/`sdiv16` helpers marked.
- B9/E9 low, patch -- DYN-6: equality overflows too.
- E5 low, patch -- CTL-1: a terminal minus sign also steps past the NUL.
- B10 low, patch -- SND-6: the omitted `fcomp` can also change the x87 status; equivalence is limited to the stored curve.
- B6/E7, B7, E6 low, patch -- slice 6's DEMO-8 (message misclassification vs the recursion), DEMO-10 (signedness only on the vertical-speed and pitch-rate divides) and REAL-2 (When bounded at 2,545,942,528) rows.

Implementer's reading kept: ADV-4's cloud insertion reloads every field, so it is not a bug; the row was corrected and only the wind and temperature parts are marked. Neither layer disputed it.

After the patches: the check passes (217 marked, 109 allowlisted, 5 no longer apply). The four DLLs were rebuilt after the last edit and hash as in slice 6's list; I reran the three scratch links (ADVDRV cover, ADVDRV break with `BREAK=0`, CONTROLS break from fresh copies), and they match `pre15g/sb`; the four fixture copies are unchanged (md5).

## Verification

**Commands:**
- `python3 -B tools/check_retail_bugs.py --self-test && python3 -B tools/check_retail_bugs.py`. Expected: the self-test passes, then the summary line, exit 0.
- `nice -n 19 cmake --build build/win32-retail -j 2 && CMAKE_BUILD_PARALLEL_LEVEL=2 nice -n 19 tools/ofs difftests`, then hash again, scratch-link again, and `diff` against `pre15g/`. Expected: empty.
