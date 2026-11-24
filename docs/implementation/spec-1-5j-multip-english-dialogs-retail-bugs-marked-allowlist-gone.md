---
title: 'Story 1.5, slice 10: MULTIP and the English dialogs marked, and the allowlist gone'
type: 'chore'
created: '2026-11-24'
status: 'done'
route: 'dispatch'
baseline_commit: '7c354a84c061559cd1341bcf19f2d8d74fbbc558' # worktree also holds the uncommitted slices 8-9 and the 1.4 close-out
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-1-5i-panels-g3d-hg2d-retail-bugs-marked.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** After slice 9, 35 ids are still allowlisted, the last of them: MULTI-1..15, VIEW-1..3, HELP-1..5, ALERT-1, WLD-1..7 and MPD-1..4. None of these modules has a marker. While the allowlist exists, a new register entry can be added without its marker and the check still passes.

**Approach:**
1. Mark these sections with slice 9's method (`spec-1-5i`'s Boundaries).
2. Then delete `tools/retail_bugs_unmarked.txt`.
3. Make `tools/check_retail_bugs.py` require every non-obsolete entry to be marked, and fail if the allowlist file exists.

This finishes Story 1.5.

## Boundaries & Constraints

**Always:**
- Statuses follow the user's standing rule:
  - an entry whose bug ours deliberately avoids becomes `(fixed, not kept)`, with a marker at our fix;
  - one whose only site is dead code, or code with no observable effect, that we omitted, or whose claimed behaviour retail does not have, becomes `(no longer applies)`, with no marker and one sentence why;
  - a mixed entry keeps a `(kept)` marker on each reproduced part and a Not-reproduced note on each other part;
  - a `(suspected)` entry keeps its kind.

  There is no allowlist left to hold an entry not reproduced for no stated reason: report it to the user and stop before deleting the allowlist.
- ALERT-1 becomes `(no longer applies)`. Slice 3 showed that UTIL `+0xa8` allocates the descriptor zero-filled on both sides, and writes 0 to `+0x10` (UTIL-17, `src/util/util_alert.c:155`), so the uninitialised progress fields it claims do not exist. Cite UTIL-17 in the sentence, and correct `docs/modules/alerts.md:227` to match.
- MPD-4's two behaviours live in MULTIP and overlap MULTI-14. Mark MPD-4 at `multip_details_kick` and `multip_details_refresh` (`src/multip/multip_details.inc:58`) alongside MULTI-14's markers, and cite the MP_DIALOG callback boundary in Where. The marker is set in the future, so a refresh can be due at once on an ordinary tick, not only near the wrap (`mpdialog.md:1261`).
- The per-part checklist in this spec's Implementation Notes is part of acceptance, refreshed to the final line numbers. An obsolete id's line gives its rationale, and a site in another module is cited, not marked.
- Module sources: comments only. Every PE under `bin/` (`.dll`, `.exe`, `.gau`, `.sim`, `.mdl`; `bin/break-pool` excluded) must hash the same with `peh3.py` before and after.
  - Each default-build PE linked from an edited source must be shown rebuilt after the last edit.
  - For the on-demand links (the `*-diff` DLLs and harness exes, about 50 under `bin/world-diff/` alone), first write a manifest of each recipe's translation units, with their exact flags and defines. That includes:
    - MULTIP with and without `-DMULTIP_DIFFTEST` (`cmake/targets.cmake:1653`);
    - each harness unit that includes an edited module header or `.inc` (e.g. `tools/difftest/viewseventsdiff.c:11`).

    Compile each affected unit before and after, under the same relative name, and `cmp` the objects. Identical objects prove every link built from them. `.def` and resource inputs are not edited.
  - WORLD and the four English dialog DLLs link under `bin/*-diff/` and are copied to `bin/` only when the bytes differ (`cmake/steps/all.cmake:284`, `:361`). The link outputs need rebuild evidence; the `bin/` copies are checked by hash.
- The checker:
  - The summary becomes `retail bugs: N entries, M marked, L no longer apply`.
  - An existing `tools/retail_bugs_unmarked.txt` is an error.
  - An unmarked non-obsolete entry is an error, as now.
  - `--self-test` drops the allowlist cases and gains one for the refused file, exercised through the same path as the CLI with a temporary root. The prose and outside-comment cases keep a valid marker elsewhere, so an unmarked error cannot mask what they test.
- Builds run under `nice -n 19`, with `-j 2` and `CMAKE_BUILD_PARALLEL_LEVEL=2`.
- No staging or commits, no reverting sources to take a baseline, and no changing this spec's status.

**Never:**
- No section outside MULTIP, Views, Help, Alerts, World and MP_DIALOG is marked. No `tools/` file is edited except the checker and the allowlist's deletion. No retail bug is fixed.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|----------|--------------|---------------------------|----------------|
| Consistent | every non-obsolete entry marked, no allowlist file | `retail bugs: 331 entries, M marked, L no longer apply`, exit 0 | — |
| Allowlist back | `tools/retail_bugs_unmarked.txt` exists (even empty) | names the file | exit 1 |
| Unmarked | a non-obsolete entry with no marker | `unmarked: ID` | exit 1 |
| Earlier rows | unknown, wrong-kind, malformed and obsolete-marked markers; duplicate register ids | as before | exit 1 |
| Same id, several sites | one entry marked at two sites | passes | — |

</frozen-after-approval>

## Code Map

- `docs/retail-bugs.md`: the MULTIP, Views, Help, Alerts, World and MP_DIALOG sections. The preamble (`:16`) names the allowlist; reword it so it says every entry is marked and the check enforces it. Module docs: `docs/modules/multip.md`, `views.md`, `help.md`, `world.md`, `mpdialog.md`, `alerts.md`.
- Sources: `src/multip/` (44 files), `src/views/`, `src/help/`, `src/alerts/`, `src/world/` (105 files) and `src/mpdialog/`.
  - VIEW-1 is kept, at both ESP-16 shims (`src/views/views_stack.S:1`).
  - MULTI-6 and MULTI-15: mark the x86 capture and consumption sites. The non-i386 branches that supply zeros are Not-reproduced parts; the DPNAME comment explains that native targets lack the caller's stack (`multip_players.inc:228`). The checklist covers state publication (`multip_control_updates.inc:118`), weather replies (`multip_weather.inc:77`) and maintenance sends (`multip_tick.inc:104`).
  - MULTI-9: separate checklist lines for construction from the aircraft size (`multip_state_messages.inc:22`) and receipt from the packet field (`multip_updates.inc:2`; `multip.md:1334`).
  - MULTI-7 is `(suspected)`.
  - MPD-3: Choice is `0x22345a40`; `0x22342bad` and `0x22342c2f` belong to Host Options. Mark both error handlers and tell their branches apart. Record Choice's null-block guard as an existing divergence whose intent is not established (`mpdialog.md:2080`, `:2263`; `mpdialog_flow.c:639`).
  - WLD-1: mark both the List and the Priority population (`world_list.c:26`, `:45`; `world_priority.c:89`).
  - WLD-3: replace "files list" with the scenery-layer list's selection and double-click. Double-click discards the fresh selection and rereads protection from the row at the stale descriptor index (`world_list.c:128`).
  - WLD-6: `_itoa` runs on the numeric runway branch with a supported suffix; the bit-priority test belongs to navaid output (`world_facilities.c:57`, `:187`).
  - WLD-7: negative counts skip the row-field frees but still free the arrays, and large positive counts walk past the rows (`:232`).
- `tools/check_retail_bugs.py`: the docstring (`:8`), `check(reg_text, files, allow)` (`:88-129`), the self-test (`:136-166`; the stale-allowlist and obsolete-allowlisted rows go), and `main` (`:184-185`).
- `tools/retail_bugs_unmarked.txt`: delete it once all 35 ids are settled.
- `docs/tools.md:284`: the Baselines line, with the new summary and no allowlist mention.
- PEs:
  - default build: `bin/MULTIP.DLL`, and the English DLLs (`bin/MP_DIALOG.DLL` at `cmake/steps/all.cmake:349`, World built through `@WORLD_SOURCES@` at `:284`; find the Views, Help and Alerts outputs);
  - on demand: `bin/difftest/MULTIP_test.dll`, `bin/{views,help,alerts,mpdialog,world}-diff/*`. These use the object gate above.
- The gate: `/tmp/claude-1000/-home-steve-source/b2e3078b-1d77-426e-816f-2de2772a67dc/scratchpad/peh3.py`, with lists in new `pre15j/` and `post15j/` there, and the objects under `pre15j/obj/` and `post15j/obj/`.

## Tasks & Acceptance

**Execution:**
- [ ] Before any edit, hash every PE under `bin/` into `pre15j/`, and compile every source file in the six modules to `pre15j/obj/`.
- [ ] The six modules' sources: add the markers and notes.
- [ ] `docs/retail-bugs.md`: correct the rows, add the Not-reproduced sentences, apply the status changes (ALERT-1 included), and reword the preamble.
- [ ] `tools/check_retail_bugs.py`: no allowlist, refuse the file, new summary, self-test updated to the matrix.
- [ ] `tools/retail_bugs_unmarked.txt`: delete it.
- [ ] `docs/tools.md`: the Baselines line.
- [ ] This spec: an `## Implementation Notes` section with the per-part checklist.

**Acceptance Criteria:**
- Given the slice is done, when `python3 -B tools/check_retail_bugs.py` runs, then it exits 0 with `331 entries, (324 - O) marked, (7 + O) no longer apply`, where `O` counts the entries made obsolete here (ALERT-1 at least), and the allowlist file is gone.
- Given `touch tools/retail_bugs_unmarked.txt`, when the check runs, then it exits 1 naming the file. Remove the file afterwards.
- Given `--self-test`, then every matrix row passes.
- Given the builds before and after, when the hash lists are diffed, then the diff is empty, every default-build PE from an edited source was rebuilt, and every edited source's object is byte-identical.

## Implementation Notes

Per-part checklist (final line numbers). `m:` = `src/multip/`, `v:` = `src/views/`, `h:` = `src/help/`, `w:` = `src/world/`, `d:` = `src/mpdialog/`. `NR` = a `Not reproduced (ID)` note. Every entry is `(kept)` except MULTI-7 `(kept, suspected)` and ALERT-1 `(no longer applies)`; nothing is left unexplained.

MULTIP:
- MULTI-1: the leaked name `m:multip_session.inc:242`.
- MULTI-2: request constructors set pending/time after the enqueue `m:multip_messages.inc:9`; the four allocated constructors leak on enqueue failure `m:multip_messages.inc:52`; the transfer messages 101a/101b/101e/101d `m:multip_transfer_messages.inc:3`.
- MULTI-3: destruction returns -1 `m:multip_system.inc:27`; migration `m:multip_system.inc:49`.
- MULTI-4: `m:multip_texture_receive.inc:52`.
- MULTI-5: 101a's unused total, GLOBAL total and ignored results `m:multip_transfer_messages.inc:7`; 101d's raw name `m:multip_transfer_messages.inc:63`.
- MULTI-6: full-state snapshot/tail `m:multip_state_messages.inc:30`; 1018 padding `:52`; 1019 padding `:86`; x86 capture of 1000/1018/1019 `:91`; publication's reads `m:multip_control_updates.inc:95`, `:102`, `:115`; publication capture `:122`; DPNAME reserved word consumed `m:multip_players.inc:163`, captured (create_local ESP-0x400, start ESP-0x404) `:224`. NR (non-i386 zeros): `m:multip_state_messages.inc:107`, `m:multip_control_updates.inc:169`, `m:multip_players.inc:238`.
- MULTI-7 (suspected): the reader's destination `m:multip_profile_fields.inc:23`; the validated field `m:multip_profile.inc:58`.
- MULTI-8: queue append lock `m:multip.c:590`; queue take `:614`; receive `:719`; receive callback (x86 and C twin) `m:multip_dispatch.inc:119`.
- MULTI-9: construction from the aircraft size `m:multip_state_messages.inc:37`; receipt from the packet field `m:multip_updates.inc:8`.
- MULTI-10: `m:multip_players.inc:9`, `:82` (local removal), `:94` (player removal).
- MULTI-11: cleanup `m:multip_providers.inc:9`; enumerate `:47`.
- MULTI-12: session callback `m:multip_browser.inc:48`; refresh's walkers `:69`.
- MULTI-13: second query consumed `m:multip_lobby.inc:10`; lobby creation/settings/release `:21`; host copy failure `:43`; explicit start `:90`; create's first query `m:multip.c:1037`; host's vtable read `m:multip_session.inc:91`.
- MULTI-14: kick's leak `m:multip_details.inc:58`; refresh's unsigned test `:80`.
- MULTI-15: maintenance tick `m:multip_tick.inc:122`; weather-button entry `m:multip_weather.inc:17` (the packet word), `:31` (the capture); host's reply (1013) `:91`. NR (non-i386 zeros): `m:multip_tick.inc:131`, `m:multip_weather.inc:38`, `:96`.

Views: VIEW-1 `v:views_events.c:311` (map), `:407` (zoom), `v:views_stack.S:10` (map shim), `:22` (zoom shim). VIEW-2 `v:views_events.c:150`. VIEW-3 `v:views_events.c:118` (validate), `:151` (report), `:378` (select, radio and exact flag).

Help: HELP-1 `h:help_events.c:392`. HELP-2 `h:help_events.c:393`, `h:help_stack.S:1`. HELP-3 `h:help_events.c:442`. HELP-4 `h:help_events.c:98` (stream callback), `:348` (unchecked library and window). HELP-5 `h:help_events.c:144` (signed compare), `:157` (XOR toggle).

Alerts: ALERT-1 is `(no longer applies)`: UTIL `+0xa8` allocates the descriptor zero-filled on both sides and writes 0 to `+0x10` (UTIL-17, `src/util/util_alert.c:155`). `docs/modules/alerts.md` corrected. No source edited.

World: WLD-1 `w:world_list.c:50` (List), `w:world_priority.c:93` (Priority). WLD-2 `w:world_area.c:101`. WLD-3 `w:world_list.c:27` (selection), `:134` (double-click). WLD-4 `w:world_facilities.c:260`. WLD-5 `w:world_facilities.c:279` (load), `:401` (Close). WLD-6 `w:world_facilities.c:201` (`_itoa`), `:239` (navaid priority test). WLD-7 `w:world_facilities.c:59`.

MP_DIALOG: MPD-1 `d:mpdialog_flow.c:163` (Create unchecked), `:179` (factory result). MPD-2 `d:mpdialog_flow.c:166` (Intro), `d:mpdialog_session.c:438` (Settings). MPD-3: Choice `d:mpdialog_flow.c:533` (with the null-block guard recorded as an existing divergence, intent not established); Host Options `:653` (exits `0x22342bad`, `0x22342c2f`). MPD-4: MULTIP `m:multip_details.inc:59` (kick, through MP_DIALOG's `+0x0c` entry) and `:82` (refresh, `+0x10`); MP_DIALOG's own callback boundary is `d:mpdialog_details.c:371` and `:379`, cited not marked.

Rows corrected: MULTI-6, MULTI-9, MULTI-15, WLD-1, WLD-3, WLD-6, WLD-7, MPD-3, MPD-4, ALERT-1; the preamble no longer names the allowlist.

Verification: the check passes (331 entries, 323 marked, 8 no longer apply) and its self-test; the PE hashes, the rebuilt link outputs and the object gate match pre15j and post15j (scratchpad).

## Review Triage Log

Spec review 1 (Codex gpt-6.2-sol), all applied:
1. high: a recipe manifest for the object gate (MULTIP_DIFFTEST, harness units including edited files).
2. high: the English DLLs' link outputs vs their `bin/` copies.
3. high: MPD-4 is marked in MULTIP.
4. medium: MPD-3's two handlers.
5. medium: VIEW-1 kept; MULTI-6/15's x86 sites, with non-i386 zeros as Not-reproduced parts.
6. medium: MULTI-9's two subtractions.
7. medium: WLD-1's and WLD-3's sites.
8. medium: WLD-6's and WLD-7's triggers.
9. medium: the checker matrix (duplicate register ids; same id at several sites passes; the CLI-path test).
10. low: `alerts.md` corrected with ALERT-1.

Code review 1 (Codex gpt-6.2-sol: blind, edge-case and verification-gap layers):
- Patched:
  - the allowlist path is tested with `lexists`, so a dangling symlink is refused (self-test case added);
  - the prose, string, directive and bare-code self-test cases assert a single error;
  - VIEW-1 is marked at each shim;
  - MULTI-6 is marked at the DPNAME consumption site;
  - comments corrected from the code: 1018/1019 (only the `+0x26` padding word is unwritten), 1000 (the 0x6c-byte uninitialised tail), MULTI-9 (oversized counts, not wraps), and MULTI-14/MPD-4 (the future kick marker makes the next refresh due at once; a real tick rollover is not a bug). The `multip.md` and `mpdialog.md` lines are corrected to match.
- Deferred (they predate this slice, and the current tree passes): the checker's file-listing fallback, `.S` directive and `;` parsing, the backslash-newline `*/`, the empty register and malformed duplicates (`deferred-work.md`).
- Dismissed:
  - the hash lists' `ERR` rows are non-PE or unreadable files, identical on both sides;
  - the per-recipe manifest finding concerns units that were not edited;
  - the `tools.md` line names the allowlist only as an error;
  - the read-only sandbox could not create the self-test's temporary directory.
- After the patches: the check and self-test pass, all 134 objects are byte-identical to `pre15j/obj`, and `bin/MULTIP.DLL`, `VIEWS_ours.dll` and `bin/VIEWS.DLL` were rebuilt after the last edit, with hashes equal to `pre15j`.

## Verification

**Commands:**
- `python3 -B tools/check_retail_bugs.py --self-test && python3 -B tools/check_retail_bugs.py`. Expected: the self-test passes; the summary line has no allowlisted count; exit 0.
- `touch tools/retail_bugs_unmarked.txt; python3 -B tools/check_retail_bugs.py; echo $?; rm tools/retail_bugs_unmarked.txt`. Expected: an error naming the file, then 1.
- `nice -n 19 cmake --build build/win32-retail -j 2 && CMAKE_BUILD_PARALLEL_LEVEL=2 nice -n 19 tools/ofs difftests`, then hash again and recompile the objects, and `diff` against `pre15j/`. Expected: empty.
