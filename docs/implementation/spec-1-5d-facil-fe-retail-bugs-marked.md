---
title: 'Story 1.5, slice 4: FACIL and FE retail bugs marked'
type: 'chore'
created: '2026-11-23'
status: 'done'
route: 'dispatch'
baseline_commit: 'ce8b47b93d362e8c3eeb5054114cac27aa529bc6' # slices 2 and 3 committed
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-1-5c-util-chain-numeric-retail-bugs-marked.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** After slice 3, 210 ids are still allowlisted. 18 of them belong to the fourth slice: FACIL-1..8 and the ten FE entries (FE-TYPE4, FE-18-PUSH, FE-40-CONVENTION, FE-GRID-TABLE, FE-84-LEFTOVER, FE-88-LEFTOVER, FE-LIB-EMPTY-DIR, FE-LIB-PRIORITY, FE-LIB-NEXT, FE-LIB-LEAKS). Neither module has a marker yet.

**Approach:** Use slice 3's method (`spec-1-5c`'s Boundaries): read each entry's code, correct its row, mark every site of every part it names, put Not-reproduced notes on parts we don't reproduce, leave a per-part checklist in Implementation Notes, and remove the ids from the allowlist.

## Boundaries & Constraints

**Always:**
- Statuses follow the user's standing rule from slice 3:
  - an entry whose bug ours deliberately avoids becomes `(fixed, not kept)`, with a marker at our fix;
  - one whose only site is dead code we omitted becomes `(no longer applies)`, with no marker and one sentence why;
  - one that is not reproduced for no stated reason stays allowlisted with a `#` comment and is reported;
  - a mixed entry keeps a `(kept)` marker on each reproduced part and a Not-reproduced note on each other part;
  - an id made `(no longer applies)` is removed from the allowlist too, since the check rejects one left there.
- The checker counts ids, not sites, so the per-part checklist in Implementation Notes is part of acceptance: for each id, each behaviour (part) its row names, and the file:line of its marker or `Not reproduced (ID): ...` note.
- Comments only. Every PE under `bin/` must hash the same before and after (the `peh3.py` lists, `bin/break-pool` excluded). Each PE that links `src/facil/` or `src/fe/` must be shown rebuilt after the edits, by its mtime or by a scratch build of its recipe. `fe.c` includes `fe_library.c` (`fe.c:4814`), and the build tracks only `.h`/`.inc` siblings, so after any edit to `fe_library.c`, `touch src/fe/fe.c` before building.
- Builds run under `nice -n 19`, with `-j 2` and `CMAKE_BUILD_PARALLEL_LEVEL=2`, while the Story 1.4 rerun runs.
- No staging or commits.

**Never:**
- No section outside FACIL and FE is marked. Two exceptions: FACIL-8 and FE-88-LEFTOVER describe one behaviour and are marked on both sides, and FE-40-CONVENTION's caller, DYNAMIC's `fe_probe` (`src/dynamic/dynamic.c:481`), gets that entry's marker too. No `tools/` file except the allowlist is edited, and no retail bug is fixed.

</frozen-after-approval>

## Code Map

- `docs/retail-bugs.md`: the FACIL and FE sections; `docs/modules/facil.md` and `fe.md` give the offsets.
- `src/facil/facil.c`: all FACIL entries. FACIL-2's unmap is `.text+0x57e0`. Corrections known from the spec review:
  - FACIL-7 overstates the record-copy leak: the airport row leaks its record copy, the successful navaid row frees it (`facil.c:1609`, `:1960`), and the row-name results leak either way. The sorted `+0x1c` copy (`:2168`) and the second mode-`0x1002` free (`:3112`) are separate parts, and the second-match allocation (`:2239`) overlaps FACIL-4. Split the checklist by behaviour.
  - FACIL-8: the consumer is `facil_row_runways` (`:2128`); the Airport dialog's `facil_list_runways` (`:2821`) calls `+0x84`.
- `src/fe/fe.c` and `fe_bands.inc`:
  - FE-TYPE4: `fe_register`.
  - FE-18-PUSH: `fe.c:480`, `fe_scene_entry`.
  - FE-40-CONVENTION (`fe.c:4508`, `:4580`): the row's read length is wrong. The oversized path reads the caller's byte count through edi, so DYNAMIC's probe reads one byte, and the full length goes out through the size output.
  - FE-GRID-TABLE: only `+0x60`'s latitude search (`fe.c:303`) and `+0x64`'s band lookup (`:349`) read the table; `+0x68`/`+0x6c` don't. Narrow the Where cell, and note that the vectors exclude out-of-table cases.
  - FE-84/88-LEFTOVER: ours reads the incoming stack word on purpose (`fe.c:1811`), and `tools/difftest/facildiff.c:1835` seeds it the same on both sides. Limit the leftover return to found records, and record that the natural carry-over from `+0x84` is unverified by the seeded test.
- `src/fe/fe_library.c`: the Scenery Library, FE-LIB-*.
  - FE-LIB-EMPTY-DIR has two sites (`:198`, `:247`, writer `:787`). Ours compares a padded local buffer, so a match zeroes the padding, not retail's saved esi: a Not-reproduced part. Document the writer's separate match condition.
  - FE-LIB-LEAKS also has the always -1 return in `fe_lib_free_blocks` (`:885`) and the unused joined path in `fe_lib_create_temp` (`:929`).
- PEs: three link outputs:
  - `bin/FACIL.DLL` and `bin/FE.DLL`, from the default build (`cmake/steps/all.cmake:95`, `:105`);
  - `bin/difftest/FE_sl_ours.dll`, an on-demand link at `0x20c00000` (`targets.cmake:638`, `:1173`). Scratch-build it before and after, like slice 3's `sb15c.sh` in the scratchpad.

  `FE_ours.dll`, `FACIL_ours.dll` and the temporary `FE_facil_ours.dll` are copies of the builds; `FE_retail.dll` and `FE_landme_ours.dll` come from retail. They stay in the hash inventory but aren't relink evidence.
- The gate: `/tmp/claude-1000/-home-steve-source/b2e3078b-1d77-426e-816f-2de2772a67dc/scratchpad/peh3.py`, with the hash lists in a new `pre15d/` there.

## Tasks & Acceptance

**Execution:**
- [x] Before any edit, hash every PE under `bin/` into `pre15d/` and scratch-build `FE_sl_ours.dll` into it.
- [x] `src/facil/facil.c`, `src/fe/*`: add the markers and notes.
- [x] `docs/retail-bugs.md`: correct the rows, add "Not reproduced" sentences, and apply any status changes.
- [x] `tools/retail_bugs_unmarked.txt`: drop the ids marked.
- [x] `docs/tools.md`: update the check's Baselines line.

**Acceptance Criteria:**
- Given the slice is done, when `python3 -B tools/check_retail_bugs.py` runs, then it exits 0 with no FACIL or FE id allowlisted except those reported. The summary is `331 entries, 138 marked, 192 allowlisted, 1 no longer apply`, adjusted only for each entry reported or made `(no longer applies)`. The per-part checklist covers every part of every row.
- Given the builds before and after, when the hash lists are diffed, then the diff is empty, and each PE built from these modules was rebuilt.

## Implementation Notes

Per-part checklist (markers `RETAIL BUG (kept)`; lines as of the final sources). No entry was wholly unreproduced, and none changed status.
- FACIL-1: `facil.c:1805`, deinit's missing ALLOW_GRAY write.
- FACIL-2: `facil.c:958`, left mapped and read unmapped by `+0x1c`/`+0x20`; `:1076`, a failed `MapViewOfFile` still marked mapped; `:1091`, view copies stale after the unmap.
- FACIL-3: `facil.c:1695`, the unmap's result returned, and the queue and filter copy leak.
- FACIL-4: `facil.c:1613`, the second match overwrites the first; `:1711`, the sorted copy is unused.
- FACIL-5: `facil.c:3068`. FACIL-6: `facil.c:2981`.
- FACIL-7: row names `:1974`; frequency section `:2034`; airport record copy `:2184` (the navaid row frees its own); leading empty record `:2627`; `0x1309` array `:2880`; second free in mode 0x1002 `:3132`; second-match name `:1614`; sorted copy `:1712`.
- FACIL-8: `facil.c:2142` (`facil_row_runways`), and FE-88's sites.
- FE-TYPE4: `fe.c:926` (the signed test), `:932` (the write at `13 * count`), the header near `:850`.
- FE-18-PUSH: `fe.c:484`, `:3138`, `:3371`.
- FE-40-CONVENTION: `fe.c:4537`/`:4545`, `:4604`, `:4641` (the rotation); `dynamic.c:481`.
- FE-GRID-TABLE: `fe.c:303`, `:351`, with the vector exclusion noted.
- FE-84-LEFTOVER: `fe.c:1796` (with the Not-reproduced note on the natural carry-over), `:1856`.
- FE-88-LEFTOVER: `fe.c:1823` (its Not-reproduced note), `:1845`, `:1867`.
- FE-LIB-EMPTY-DIR: `fe_library.c:199`, `:251` (with Not-reproduced notes on the padded buffer, and the writer's condition), `:791`.
- FE-LIB-PRIORITY: `fe_library.c:1268`. FE-LIB-NEXT: `:1965`, `:1994`.
- FE-LIB-LEAKS: `fe_library.c:891`, `:921`, `:1310`, `:2536`.

Verification deviation: the implementer did not take a full `bin/` hash before the edits. It rebuilt the four sources at HEAD, hashed `FACIL.DLL`, `FE.DLL`, `DYNAMIC.DLL` and a scratch `FE_sl_ours.dll`, then restored its edits (`cmp` clean). I cross-checked those PEs and their difftest copies against slice 3's full after-list (`post15c/unstripped.txt`, the state before this slice): all the same. The edited DLLs are newer than the sources.

## Review Triage Log

Spec review 1 (Codex gpt-6.2-sol):
1. high, applied: `fe.c` includes `fe_library.c`, which the build doesn't track; touch `fe.c`. The build bug itself is deferred (`deferred-work.md`).
2. high, applied: FACIL-8/FE-88 consumer and the seeded-test caveat.
3. medium, applied: FE-40-CONVENTION's read length, and DYNAMIC's `fe_probe` site allowed.
4. medium, applied: FE-LIB-EMPTY-DIR's sites and padding.
5. medium, applied: FACIL-7 split.
6. medium, applied: FE-GRID-TABLE narrowed.
7. medium, applied: the PE inventory, link outputs vs copies.
8. medium, rejected: `peh3.py` writes `ERR` lines into the path-sorted list, so a PE turning unreadable, or a file missing, shows in the diff; slice 3 compared stripped and unstripped lists the same way.
9. low, applied: mixed entries, obsolete ids off the allowlist, baseline now `ce8b47b`.
10. low, applied: FE-LIB-LEAKS's two further sites.

Code review 1 (Codex gpt-6.2-astra: blind, edge case, verification gap; the last found no gaps):
- B1/E1 medium, patch -- FACIL-4/7: the displaced first name leaks; the second goes to the caller.
- B2/E2 medium, patch -- FACIL-2/3: only paths reaching `fail` unmap and return the unmap's result; a queue failure returns -1 with the map live; map errors precede the queue and filter allocations.
- B3/E3 low, patch -- FACIL-7's sorted copy is heap (`facil_list_copy`), freed on success.
- B4/E5 medium, patch -- the stack-word wording was inconsistent: the seeded test shows the same leftover for the same incoming word; the natural carry-over is unverified.
- B5/E9 medium, patch -- FE-LIB-EMPTY-DIR: ours zeroes local padding, not the saved esi; added to `docs/open-differences.md`, `fe.md`'s claim narrowed, and any block address with top byte 0x5c matches.
- B6/E8 low, patch -- FE-LIB-NEXT: the read is at `(cursor + 1) * 0x108` when `cursor + 1 < count`.
- B7/E7 low, patch -- FE-GRID-TABLE: a negative band's magnitude is shifted.
- B8/E4/E6 low, patch -- FE-88 and FACIL-8 When cells narrowed.

After the patches: the check passes (138 marked, 192 allowlisted, 1 no longer applies). FACIL.DLL, FE.DLL and DYNAMIC.DLL were rebuilt (after `touch src/fe/fe.c`) and hash as in slice 3's after-list; the scratch `FE_sl_ours.dll` is unchanged (`638d3e03bd1c374b`).

## Verification

**Commands:**
- `python3 -B tools/check_retail_bugs.py --self-test && python3 -B tools/check_retail_bugs.py`. Expected: the self-test passes, then the summary line, exit 0.
- `nice -n 19 cmake --build build/win32-retail -j 2 && CMAKE_BUILD_PARALLEL_LEVEL=2 nice -n 19 tools/ofs difftests`, then hash again, scratch-build again, and `diff` against `pre15d/`. Expected: empty.
