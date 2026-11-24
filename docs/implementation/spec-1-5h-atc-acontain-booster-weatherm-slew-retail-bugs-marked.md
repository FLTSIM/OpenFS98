---
title: 'Story 1.5, slice 8: ATC, ACONTAIN, BOOSTER, WEATHERM and SLEW retail bugs marked'
type: 'chore'
created: '2026-11-24'
status: 'done'
route: 'dispatch'
baseline_commit: '7c354a84c061559cd1341bcf19f2d8d74fbbc558' # slices 6 and 7 committed
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-1-5g-advdrv-dynamic-controls-sound-retail-bugs-marked.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** After slice 7, 109 ids are still allowlisted. 37 of them belong to the eighth slice: ATC-1..12, ACN-1..9, BST-1..5, WXM-1..8 and SLEW-1..3. None of the five modules has a marker yet.

**Approach:** Use slice 7's method (`spec-1-5g`'s Boundaries): read each entry's code, correct its row, mark every site of every part it names, put Not-reproduced notes on parts we don't reproduce, leave a per-part checklist in this spec's Implementation Notes, and remove the ids from the allowlist.

## Boundaries & Constraints

**Always:**
- Statuses follow the user's standing rule:
  - an entry whose bug ours deliberately avoids becomes `(fixed, not kept)`, with a marker at our fix;
  - one whose only site is dead code, or code with no observable effect, that we omitted becomes `(no longer applies)`, with no marker, off the allowlist, and one sentence why;
  - one that is not reproduced for no stated reason stays allowlisted with a `#` comment and is reported;
  - a mixed entry keeps a `(kept)` marker on each reproduced part and a Not-reproduced note on each other part;
  - a `(suspected)` entry keeps its kind: `(kept, suspected)`.

- Statuses this rule settles (spec review 1):
  - ATC-12 `(fixed, not kept)`: ours reads both fields through the imported GLOBAL, at the two reads (`atc.c:650`; `atc.md:111`). MAIN still supplies GLOBAL in EDI for retail ATC; cite that caller and thunk (`main_radios.inc:386`, `main.c:173`).
  - ATC-10 `(no longer applies)`: ours gives NUMERIC 8 bytes and copies at most 6 (`atc.c:178`), and no id ATC uses can reach the overwrite, so leaving it out has no observable effect. Correct Where to NUMERIC's output buffer in `.text+0x6d2`.
  - ACN-8 `(fixed, not kept)`: ours seeds `previous[0]`, `sim[0]` and the first `e.type` (`acontain.c:413`, `:758`, `:793`).
  - WXM-1 and WXM-2 are mixed: the triggers are reproduced, but `wx_fault` raises a non-continuable software exception (`weatherm.c:188`), and the host's filter resumes only at a faulting `div`/`idiv` (`host_rt_divide.c:28`). The repair is a Not-reproduced part. Qualify the rows' fault claims, add one line to `docs/open-differences.md`, and note that the harness avoids these inputs (`tools/difftest/weathermdiff.c:21`).

  An obsolete id's checklist line gives its rationale instead of a file:line, and a site in another module is cited in its checklist line, not marked.
- The per-part checklist is part of acceptance: for each id, each behaviour (part) its row names, and the file:line of its marker or `Not reproduced (ID): ...` note, refreshed to the final line numbers after the last edit.
- Comments only. Before any edit, hash every PE under `bin/` with `peh3.py` (`bin/break-pool` excluded); afterwards they must hash the same. Each PE linked from an edited source must be shown rebuilt after the last edit, by an mtime newer than the source. A fixture that is only a copy of another build (`bin/difftest/*_ours.dll`) is checked to be unchanged, not rebuilt.
- Builds run under `nice -n 19`, with `-j 2` and `CMAKE_BUILD_PARALLEL_LEVEL=2`.
- No staging or commits, no reverting sources to take a baseline, and no changing this spec's status.

**Never:**
- No section outside ATC, ACONTAIN, BOOSTER, WEATHERM and SLEW is marked, with one exception: BST-5's site is UTIL's `+0x44`. If it is marked there (beside UTIL-1's marker, `src/util/util_math.c:363`), `UTIL.DLL` must be shown rebuilt and `OFSTEST.DLL` and `UTIL_TRACE.DLL` scratch-built before and after with the scratchpad's `sb15c.sh`.
- No `tools/` file except the allowlist is edited, and no retail bug is fixed.

</frozen-after-approval>

## Code Map

- `docs/retail-bugs.md`: the ATC, ACONTAIN, BOOSTER, WEATHERM and SLEW sections. `docs/modules/atc.md`, `acontain.md`, `booster.md`, `weatherm.md` and `slew.md` give the offsets.
- `src/atc/atc.c`: ATC-1..12.
  - ATC-9: ours sets `lcid = 0` (`:221`, "retail: uninitialised if sscanf fails"). Likely `(fixed, not kept)`.
  - ATC-6 is unreachable in the game, but it is kept if our code reproduces it.
  - ATC-3: in Where, name the `.data+0x4a2` flag and the finish routine `.text+0x85b`. List the set and test sites (`:152`, `:692`, `:883`), and qualify "every finish" as every finish after the first start.
- `src/acontain/acontain.c`: ACN-1..9.
  - ACN-3: keep it at the unchecked query and read, and qualify `SIM\.SIM` as the empty-name outcome. ACN-7 still inherits a prior type on later entries.
  - ACN-9: give separate checklist lines for:
    - the undersized path destinations (`:24`, where adjacent bytes may differ between the builds);
    - the chunk read and the short-read zeroing (`:583`);
    - the unterminated SIM name (`:694`);
    - the model-state writes and the cleanup read (`:719`, `:794`).

    Correct the conditions: `+0x64` is written only after a successful texture query, and `+0x60` overruns only with a non-zero state size below `0x62`.
- `src/booster/booster.c`: BST-1..5.
  - BST-5 is UTIL's fault, not BOOSTER's. Ours does not reproduce it, the same deliberate choice as UTIL-1, so it becomes `(fixed, not kept)`, marked beside UTIL-1 at `util_math.c:363`. Relink evidence then covers `UTIL.DLL` and `difftests`' `slewdiff.exe`, `utildiff.exe` and `utildiff_portable.exe` (`cmake/steps/difftests.cmake:197`, `:211`), plus `sb15c.sh` scratch links of `OFSTEST.DLL` and `UTIL_TRACE.DLL` before and after.
- `src/weatherm/weatherm.c`: WXM-1..8.
  - WXM-6 is mixed: both areas are still converted before the enabled check (`:768`, `:910`); mark that. UTIL keeps reads past the latitude table within the copied `.data` and falls back to offset 0 beyond it: a Not-reproduced note, citing UTIL-2's site (`util_math.c:315`), which stays unedited.
  - WXM-8 stays `(suspected, not used)`. Ours implements the lists: it passes 0 as the allocation flag and walks the heads and links (`:1196`, `:1218`, `:1262`). Put `(kept, suspected)` markers at the allocator and the list operations. `weatherm.md:133` describes three further list quirks with no register id; they are deferred, not added.
- `src/slew/slew.c`: SLEW-1..3.
  - SLEW-1 is QUIRK 2, omitted as dead (`:307`): `(no longer applies)`.
  - SLEW-2 is QUIRK 3 (`:337`).
  - `:219` notes values left uninitialised as retail leaves them.
- PEs:
  - the default build: `bin/ATC.DLL`, `ACONTAIN.DLL`, `BOOSTER.DLL`, `WEATHERM.DLL`, `SLEW.DLL`;
  - no on-demand link;
  - fixture copies `bin/difftest/{ATC,ACONTAIN,BOOSTER,WEATHERM,SLEW}_ours.dll`.

  No source here includes another `.c`.
- The gate: `/tmp/claude-1000/-home-steve-source/b2e3078b-1d77-426e-816f-2de2772a67dc/scratchpad/peh3.py`, with lists in new `pre15h/` and `post15h/` there.

## Tasks & Acceptance

**Execution:**
- [x] Before any edit, hash every PE under `bin/` into `pre15h/`, and scratch-build OFSTEST/UTIL_TRACE into `pre15h/sb/`.
- [x] The five modules' sources and `src/util/util_math.c` (BST-5, beside UTIL-1): add the markers and notes.
- [x] `docs/retail-bugs.md`: correct the rows, add "Not reproduced" sentences, and apply any status changes.
- [x] `tools/retail_bugs_unmarked.txt`: drop the ids marked or obsolete.
- [x] `docs/tools.md`: update the check's Baselines line.
- [x] This spec: an `## Implementation Notes` section with the per-part checklist.

**Acceptance Criteria:**
- Given the slice is done, when `python3 -B tools/check_retail_bugs.py` runs, then it exits 0 with no ATC, ACN, BST, WXM or SLEW id allowlisted except those reported. With `O` entries made obsolete and `R` reported, the summary is `331 entries, (254 - O - R) marked, (72 + R) allowlisted, (5 + O) no longer apply`; with SLEW-1 and ATC-10 obsolete and nothing reported, that is `252 marked, 72 allowlisted, 7 no longer apply`.
- Given the builds before and after, when the hash lists are diffed, then the diff is empty, and each PE built from an edited source was rebuilt.

## Implementation Notes

Per-part checklist (final line numbers; `a:` = `src/atc/atc.c`, `n:` = `src/acontain/acontain.c`, `b:` = `src/booster/booster.c`, `w:` = `src/weatherm/weatherm.c`, `s:` = `src/slew/slew.c`, `u:` = `src/util/util_math.c`). Status changes: ATC-9, ATC-12, ACN-8, BST-5 `(fixed, not kept)`; ATC-10, SLEW-1 `(no longer applies)`. Nothing was left allowlisted.
- ATC-1: the word copy past the terminator `a:379`. ATC-2: the unconditional registration `a:905`. ATC-3: the test in the finish routine `.text+0x85b` `a:713`; the set site (plain comment, `.data+0x4a2`) `a:903`, the define `a:152`. ATC-4: `a:462`. ATC-5: the taxi post `a:758`; the squawk reminder post `a:802`. ATC-6: the write before an empty field `a:204`. ATC-7: re-init with GLOBAL `+0x128c` clear `a:239`; deinit `a:281`. ATC-8: `a:554` (qualified: an earlier layer at base 0 exits). ATC-11: `a:299`.
- ATC-9 `(fixed, not kept)`: `a:222`. ATC-12 `(fixed, not kept)`: both reads `a:669` (the `+0xb04` and `+0xb68` tests, one comment); MAIN's caller and thunk cited (`main_radios.inc:386`, `main.c:173`).
- ATC-10 `(no longer applies)`: ours gives NUMERIC 8 bytes and copies at most six (`a:188`), and no id ATC uses reaches the overwrite; no marker.
- ACN-1: `n:632`. ACN-2: `n:546`. ACN-3: the unchecked query and read `n:820`. ACN-4: `n:700`. ACN-5: `n:848`. ACN-6: `n:498`. ACN-7: `n:438`.
- ACN-8 `(fixed, not kept)`, three parts: the first `e.type` `n:417`; `previous[0]` and `sim[0]` `n:784` (seeds at `n:797`).
- ACN-9, four parts: the undersized path destinations `n:28` (header), `n:338`, `n:364` (model-folder joins, with the tags at `n:370`), `n:377` (alias branch, with the tags at `n:382`, `n:383`), `n:442`, `n:480`, `n:696`; the chunk read and the short-read zeroing `n:598`; the unterminated SIM name `n:826`; the model-state writes `n:718` and the cleanup read `n:742`.
- BST-1: the unwaited thread `b:322`; the lost post and the unbounded wait `b:536`. BST-2: `b:401`. BST-3: `b:393`. BST-4: the point list count `b:718`; the grid's two divides `b:787`, `b:796`; the thread's two list loops `b:1252`, `b:1272`.
- BST-5 `(fixed, not kept)`: `u:366`, beside UTIL-1 (`u:363`).
- WXM-1: `w:336`, with `Not reproduced (WXM-1)` at `w:337`. WXM-2: `w:761`, with `Not reproduced (WXM-2)` at `w:763`. WXM-3: `w:616`. WXM-4: `w:991`. WXM-5: `w:835`. WXM-6: the conversion before the enabled check `w:777`; both areas weighed `w:925`; `Not reproduced (WXM-6)` `w:779` (UTIL-2's site, `util_math.c:315`, unedited). WXM-7: `w:1153`. WXM-8 `(kept, suspected)`: the allocator `w:1218`, the list walk `w:1290`, the head dereference and walk in `wx_record_append` `w:1322`; the three further list quirks of `weatherm.md:133` are deferred.
- SLEW-1 `(no longer applies)`: QUIRK 2 is omitted as dead (`s:307` comment); no marker. SLEW-2: `s:340`. SLEW-3: `s:313`. The `s:219` comment (values left uninitialised) is unchanged.
- Row and doc changes: ATC-3's Where and When; ATC-8 and ATC-10 qualified; ACN-3, ACN-9 rows; BST-4, SLEW-3, WXM-1, WXM-2, WXM-6 and WXM-8 rows; `weatherm.md` (divide repair); a line in `docs/open-differences.md`.

Gate: PEs hashed with `peh3.py` before and after (`pre15h/`, `post15h/`; same list, 112,129 lines, `diff` empty); `bin/ATC.DLL`, `ACONTAIN.DLL`, `BOOSTER.DLL`, `WEATHERM.DLL`, `SLEW.DLL` and `UTIL.DLL` were rebuilt after the last edit (mtimes after their sources), as were `slewdiff.exe`, `utildiff.exe` and `utildiff_portable.exe` by `tools/ofs difftests`; the `sb15c.sh` scratch links of `OFSTEST.DLL` and `UTIL_TRACE.DLL` hash the same before and after; the five `*_ours.dll` fixtures are unchanged (md5). Check: 331 entries, 252 marked, 72 allowlisted, 7 no longer apply.

## Review Triage Log

Spec review 1 (Codex gpt-6.2-sol):
1. high, applied: WXM-1/2's lost host repair is a Not-reproduced part, also recorded in open-differences.
2. high, rejected: the spec already requires every hash to succeed; slice 7's late pre-edit scratch link was a recorded deviation, and this slice has no on-demand link apart from OFSTEST/UTIL_TRACE, which are taken before any edit.
3. medium, applied: ATC-12 `(fixed, not kept)`.
4. medium, applied: ATC-10 `(no longer applies)`, with Where corrected.
5. medium, applied: ACN-8 `(fixed, not kept)`; ACN-3 qualified.
6. medium, applied: ACN-9's parts and conditions.
7. medium, applied: WXM-6 is mixed.
8. medium, applied: BST-5 pinned beside UTIL-1, with the three difftest relinks.
9. low, applied: WXM-8 `(kept, suspected)`; three unregistered list quirks deferred.
10. low, applied: ATC-3's Where and sites.

Code review 1 (Codex gpt-6.2-sol, blind and edge-case layers; verification-gap skipped for a comment-only slice):
- E7 false -- "docs/tools.md's Baselines hunk is missing": it is in the worktree; I left the file out of the review diff because it also holds 1.4 close-out edits.
- B1 low, patch -- ACN-3: the path tried is `SIM.SIM`.
- B2/E1 medium, patch -- ACN-9: the model-folder and alias joins are marked too.
- B3 low, patch -- ACN-9: the overrun range is 0x100..0x103 characters.
- B4/E2 low, patch -- ACN-9: the state is zero-allocated; the defect is the out-of-bounds read under 0x68 bytes.
- B5/E4 medium, patch -- WXM-2: only a distance of 0 reaches the divide at width 1, and retail's divide is unsigned `div` (`0x208f1633`); open-differences corrected.
- B6 medium, patch -- `weatherm.md` still claimed retail crashes above 500 m; now "mostly reproduced", with the host repair and the harness exclusion.
- B7/E5 medium, patch -- WXM-8's marker moved to `wx_record_append`, which walks the uninitialised head.
- E3 low, patch -- ATC-8 exits when an earlier layer qualifies at base 0.
- B8 low, patch -- ATC-10: the NUL is the seventh byte and lands on saved EBP.
- B9 low, patch -- BST-4: the wrapped count is 0x7ffffff6..0x7fffffff.
- E6 low, patch -- SLEW-3: three squares cannot overflow the high half; the injected-carry claim is dropped.

Verification note: the implementer's hash lists covered `.dll` and `.exe` only (112,129 lines, identical before and after); no `.gau`, `.sim` or `.mdl` links these sources or was rebuilt. After the patches: the check passes (252 marked, 72 allowlisted, 7 no longer apply); ATC, ACONTAIN, BOOSTER, WEATHERM and SLEW were rebuilt after the last edit and hash as before; `util_math.c` was unchanged by the patches, so UTIL.DLL, its three difftest exes and the OFSTEST/UTIL_TRACE scratch links stand as first verified.

## Verification

**Commands:**
- `python3 -B tools/check_retail_bugs.py --self-test && python3 -B tools/check_retail_bugs.py`. Expected: the self-test passes, then the summary line, exit 0.
- `nice -n 19 cmake --build build/win32-retail -j 2 && CMAKE_BUILD_PARALLEL_LEVEL=2 nice -n 19 tools/ofs difftests`, then hash again and `diff` against `pre15h/`. Expected: empty.
