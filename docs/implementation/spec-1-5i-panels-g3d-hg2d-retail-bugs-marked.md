---
title: 'Story 1.5, slice 9: PANELS, G3D and HG2D retail bugs marked'
type: 'chore'
created: '2026-11-24'
status: 'done'
route: 'dispatch'
baseline_commit: '7c354a84c061559cd1341bcf19f2d8d74fbbc558' # worktree also holds the uncommitted slice 8 and 1.4 close-out
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-1-5h-atc-acontain-booster-weatherm-slew-retail-bugs-marked.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** After slice 8, 72 ids are still allowlisted. 37 of them belong to the ninth slice: the 13 PANELS ids (every PANEL-* except the already marked PANEL-ZOOM-ADDRESS), G3D-1..14 and HG2D-1..10. PANELS holds one marker (PANEL-ZOOM-ADDRESS); G3D and HG2D have none.

**Approach:** Use slice 8's method (`spec-1-5h`'s Boundaries): read each entry's code, correct its row, mark every site of every part it names, put Not-reproduced notes on parts we don't reproduce, leave a per-part checklist in this spec's Implementation Notes, and remove the ids from the allowlist.

## Boundaries & Constraints

**Always:**
- Statuses follow the user's standing rule:
  - an entry whose bug ours deliberately avoids becomes `(fixed, not kept)`, with a marker at our fix;
  - one whose only site is dead code, or code with no observable effect, that we omitted becomes `(no longer applies)`, with no marker, off the allowlist, and one sentence why;
  - one that is not reproduced for no stated reason stays allowlisted with a `#` comment and is reported;
  - a mixed entry keeps a `(kept)` marker on each reproduced part and a Not-reproduced note on each other part;
  - an entry already annotated keeps its kind, so G3D-11..14 `(fixed, not kept)` get `(fixed, not kept)` markers at our fix.

  An obsolete id's checklist line gives its rationale instead of a file:line, and a site in another module is cited in its checklist line, not marked.
- The per-part checklist is part of acceptance: for each id, each behaviour (part) its row names, and the file:line of its marker or `Not reproduced (ID): ...` note, refreshed to the final line numbers after the last edit.
- Comments only. Before any edit, hash every PE under `bin/` (`.dll`, `.exe`, `.gau`, `.sim`, `.mdl`, `bin/break-pool` excluded) with `peh3.py`; afterwards they must hash the same. Each PE linked from an edited source must be shown rebuilt after the last edit, by an mtime newer than the source, or by a scratch link of its recipe before and after under the same relative name. The link and the hash must succeed. A fixture that is only a copy of another build is checked to be unchanged, not rebuilt.
- Builds run under `nice -n 19`, with `-j 2` and `CMAKE_BUILD_PARALLEL_LEVEL=2`.
- No staging or commits, no reverting sources to take a baseline, and no changing this spec's status.

**Never:**
- No section outside PANELS, G3D and HG2D is marked. G3D-3's MAIN sky address is cited, not marked. No `tools/` file except the allowlist is edited, and no retail bug is fixed.

</frozen-after-approval>

## Code Map

- `docs/retail-bugs.md`: the PANELS, G3D and HG2D sections. `docs/modules/panels.md`, `g3d.md` and `hg2d.md` give the offsets.
- `src/panels/panels.c`: the 13 PANEL ids. PANEL-ZOOM-ADDRESS's existing marker stays.
  - PANEL-DIGITS is mixed. `e_ae40` zeroes `RECT.right`/`.bottom` before the first measurement (`:4576`), with no stated reason: a Not-reproduced note. `e_be10` keeps the icon pointer as target and latch (`:5088`): a kept marker.
  - PANEL-DIB-NEW is mixed. Ours substitutes a null write for the indeterminate pointer (`:2786`, `:2839`), and the invalid write remains: a kept marker at the write, and a Not-reproduced note for the indeterminate address.
  - PANEL-LOAD-WINDOW is mixed. The zero fallback for retail's stale `ebp` applies only before a panel record is seen (`:1011`): a Not-reproduced note. The record reuse and the `SIZE_MM` scaling (`:1153`, `:1250`) get kept markers, and the sizing gets its own trigger in the row.
  - PANEL-QUAD-CLAMP: the shape setup clamps the first x (`+0xec`) on both sides and omits the lower clamp on the second (`+0xf4`, `:5440`, `:5576`); the needle draw is `:4432`. The strip uses clipped rectangular blits (`:5313`, `:2713`). Name the needle and the shape in Where, and drop the strip unless it has its own defect.
- `src/g3d/g3d.c`, `g3d_mesh.c`: G3D-1..14.
  - G3D-3 is `(kept)`: the mapping keeps the absolute-address opcode. Sites: `op_native` (`g3d.c:1017`), the dispatcher and thunk (`:15412`), and G3D's raw bytecode operands (`g3d_image.inc:2392`). Cite MAIN's raw operands and native table (`main_image.inc:702`, `main_natives.inc:800`) in the checklist; don't mark them.
  - G3D-7 and G3D-9 are `(kept)`: ours keeps both unreachable branches (`:412`, `:1589`, `:3020`). G3D-7: the point decoder shifts the flag byte but never enables the two-word form. G3D-9: list each clipper's bypass; the shaded clipper has none (`g3d.md:154`).
  - G3D-11..14 are already `(fixed, not kept)`; mark our fix at each.
- `src/hg2d/hg2d.c` and its many `.inc` files: HG2D-1..10.
  - HG2D-3 is "preserved on purpose", so it is kept.
  - HG2D-2: give checklist lines for `texture_release`, `texture_destroy_body`, `texture_pixels` and `texture_update_body`, including the eax entry shim (`hg2d.c:89`, `:107`, `:122`, `:131`, `:146`). The count it decrements is a table indexed by texture level from `0x2079a800`.
  - HG2D-10: the uninitialised fields belong to the device disc's stack fan; ordinary device points write specular (`hg2d_device_points.inc:35`). Pin the marker to `hg2d_device_disc` (`hg2d_device_disc.inc:50`), limit When to a submitted disc fan, and note that the harness excludes those bytes (`hg2d.md:1607`).
  - Most HG2D entries have several parts (HG2D-2, -4, -6, -7 especially), and each needs a checklist line.
- PEs:
  - default build: `bin/PANELS.DLL`, `bin/G3D.DLL`, `bin/HG2D.DLL`;
  - default `difftests`: `bin/difftest/g3d_mesh_hw.o` and `g3d_mesh_soft.o` (`cmake/steps/difftests.cmake:128-129`), with the harness exes they link;
  - on-demand links:
    - `bin/difftest/PANELS_unit.dll` (`cmake/targets.cmake:434`);
    - `bin/difftest/G3D_ours.dll` and `g3d_mesh_portable.o` (`:556-557`);
    - the `g3d-break-build` recipe (`:1231`), which also writes `G3D_ours.dll`, so it gets its own scratch directory;
    - `bin/hg2d-diff/HG2D_ours.dll` (`:861`).

    Scratch-link each, before and after, as the scratchpad's `sb15g.sh` does.

  `tools/difftest/hg2ddiff.c:150` includes `src/hg2d/hg2d_pointers.inc`, so if that file is edited, `hg2ddiff.exe` (`targets.cmake:862`) must be shown rebuilt too.

  Find any other consumers with a grep of `cmake/` and `tools/` for the three source directories. No source here includes another `.c`, and HG2D's `.inc` files are steps' inputs through the glob.
- The gate: `/tmp/claude-1000/-home-steve-source/b2e3078b-1d77-426e-816f-2de2772a67dc/scratchpad/peh3.py`, with lists in new `pre15i/` and `post15i/` there.

## Tasks & Acceptance

**Execution:**
- [x] Before any edit, hash every PE under `bin/` into `pre15i/`, and scratch-link the on-demand recipes into `pre15i/sb/`.
- [x] The three modules' sources: add the markers and notes.
- [x] `docs/retail-bugs.md`: correct the rows, add "Not reproduced" sentences, and apply any status changes.
- [x] `tools/retail_bugs_unmarked.txt`: drop the ids marked or obsolete.
- [x] `docs/tools.md`: update the check's Baselines line.
- [x] This spec: an `## Implementation Notes` section with the per-part checklist.

**Acceptance Criteria:**
- Given the slice is done, when `python3 -B tools/check_retail_bugs.py` runs, then it exits 0 with no PANEL, G3D or HG2D id allowlisted except those reported. With `O` entries made obsolete and `R` reported, the summary is `331 entries, (289 - O - R) marked, (35 + R) allowlisted, (7 + O) no longer apply`; with nothing obsolete or reported, that is `289 marked, 35 allowlisted, 7 no longer apply`.
- Given the builds before and after, when the hash lists are diffed, then the diff is empty, and each PE built from these modules was rebuilt.

## Implementation Notes

Per-part checklist (final line numbers; `p:` = `src/panels/panels.c`, `g:` = `src/g3d/g3d.c`, `gi:` = `src/g3d/g3d_image.inc`, `h:` = `src/hg2d/hg2d.c`, `ha:` = `hg2d_affine.inc`, `hd:` = `hg2d_draw.inc`, `hp:` = `hg2d_palette.inc`, `hf:` = `hg2d_perspective_fill.inc`, `hpr:` = `hg2d_device_prepare.inc`, `hpt:` = `hg2d_device_points.inc`, `hdd:` = `hg2d_device_disc.inc`). Every id is `(kept)` except G3D-11..14 `(fixed, not kept)`. Nothing is obsolete or left allowlisted.

PANELS:
- PANEL-SMOOTH: `smooth_8` `p:1610`; `smooth_24` `p:1683`. PANEL-SHRINK-GREY: `shrink_8_to_8` `p:1972`; `shrink_8_to_8_plain` `p:2128`. PANEL-555-TEST: `p:2350`. PANEL-EDGES-AND: `p:2596`. PANEL-WINDOW-IDENT: `p:804`. PANEL-FCLOSE: `p:873`. PANEL-FREE-USE: `p:1479`. PANEL-ZOOM-ADDRESS-2: `p:3153`.
- PANEL-NAMED-CLEAR: the clear's free of a local `p:3547`; `named_remove` of an empty list `p:3527`.
- PANEL-DIGITS (mixed): the RECT right/bottom Not reproduced `p:4599`; the icon pointer as target and latch `p:5113`.
- PANEL-DIB-NEW (mixed): the invalid write `p:2854`; Not reproduced (the indeterminate address) `p:2802`.
- PANEL-LOAD-WINDOW (mixed): Not reproduced (stale `ebp`) `p:1018`; the `SIZE_MM` sizing `p:1156`; the record reuse `p:1259`.
- PANEL-QUAD-CLAMP: the needle's second x `p:4458`; the shape's second x `p:5467`; the strip dropped (clipped blits, no defect).

G3D:
- G3D-1 `g:966`. G3D-2 `g:1551`. G3D-5 `g:1253`. G3D-6 `g:2353`. G3D-8 `g:5700`. G3D-10 `g:10673`.
- G3D-3: `op_native` `g:1024`; the dispatcher's external-address branch in `routine()` `g:15450`; `native_thunk`'s indirect call `g:15237`; the raw operands `gi:2392` (the image stores `0x207e9d47`; `main_install()` rebinds MAIN's cells, `main.c:460`, cited not marked); MAIN's raw operands and native table cited (`src/main/main_image.inc:702`, `main_natives.inc:800`), not marked.
- G3D-4: the one-shot word exchanged in `object_check` `g:6852`; the texture name rewritten in `op_texture` `g:8914`.
- G3D-7: the point decoder `g:410`.
- G3D-9, each bypass: `line` `g:1602`; the flat polygon `g:1866`, `g:1950`, `g:2048`; the textured clipper `g:3759`, `g:3838`, `g:3925`; the shaded-textured clipper `g:4560`, `g:4642`, `g:4729`. The shaded clipper has no bypass (`g3d.md:154`).
- G3D-11 `(fixed, not kept)`: `g:4253`. G3D-12: `g:5011`. G3D-13: `g:6975`. G3D-14: `g:9959`.

HG2D:
- HG2D-1: `texture_inside` `h:85`.
- HG2D-2: `texture_release` (clear `+0x1c`, eight scratch dwords, per-level count table at `0x2079a800`, `0x207ba9ec`, discarded Release result) `h:93`; `texture_destroy_body` out-of-range eax `h:115` and the four-dword clear `h:123`; `texture_pixels` `h:134`; `texture_update_body` out-of-range eax `h:142`; the eax entry shim `h:156`.
- HG2D-3: `ha:105`.
- HG2D-4: the signed divides and no width cap `ha:87`; the unshifted second coordinate `ha:93`; four samples before four writes `ha:126`.
- HG2D-5: the +0x4e00 phase read `hf:18`.
- HG2D-6: the `+0x1298` span remap that then faults `hd:147`; point `hd:70`; depth point `hd:80`; horizontal span `hd:171`; vertical span `hd:197`; line `hd:230`; 16.16 line `hd:254`; zero frame dimension `hd:290`.
- HG2D-7: gradient doubling `hp:22`; inverse table by the full word (remap) `hp:189`; the `0xa5` toggle `hp:202`; divisor 16 and full-word inverse (shades) `hp:206`; refresh's saved EBX `hp:243`; sky ramp final byte `hp:285`.
- HG2D-8: leaked texture interface `hpr:29`; unchecked staged upload `hpr:31`; unchecked direct upload `hpr:39`; final lookup's HRESULT `hpr:48`.
- HG2D-9: the lookup index in EBX `hpt:19`; the shim that loads it `hpt:42`.
- HG2D-10: `hdd:52` (the disc fan's stack array; the harness excludes those bytes, `hg2d.md:1607`).

Rows corrected: PANEL-WINDOW-IDENT, PANEL-SHRINK-GREY, PANEL-ZOOM-ADDRESS-2, G3D-5, G3D-8, HG2D-5, HG2D-6, HG2D-8, PANEL-DIGITS, PANEL-DIB-NEW, PANEL-LOAD-WINDOW, PANEL-QUAD-CLAMP, G3D-3, G3D-7, G3D-9, HG2D-2, HG2D-10.

## Review Triage Log

Spec review 1 (Codex gpt-6.2-sol):
1. high, applied: the `g3d-break-build` link, in its own scratch directory, and `hg2ddiff.exe` if `hg2d_pointers.inc` is edited.
2. high, rejected: the spec already requires every link and hash to succeed under the same relative name. The `ERR` lines so far are non-PE files with spaces in their names, identical on both sides; the inventory keeps all five extensions.
3. medium, applied: PANEL-DIGITS is mixed.
4. medium, applied: PANEL-DIB-NEW is mixed.
5. medium, applied: PANEL-LOAD-WINDOW is mixed, and its sizing gets a trigger.
6. medium, applied: PANEL-QUAD-CLAMP's corners; the strip is dropped.
7. medium, applied: G3D-3 is kept, with its sites.
8. medium, applied: G3D-7 and G3D-9 are kept, with G3D-9's bypasses listed.
9. medium, applied: HG2D-10 is pinned to the disc fan.
10. low, applied: HG2D-2's owners and the counter table.

Code review 1 (Codex gpt-6.2-sol, blind and edge-case layers; verification-gap skipped for a comment-only slice):
- B1 false -- "ATC/ACONTAIN/BOOSTER/WEATHERM/SLEW ids dropped without markers": slice 8's uncommitted markers, outside the review diff.
- E8 false -- "docs/tools.md's Baselines line is missing": it is in the worktree, and the file was left out of the review diff.
- B2/E1 medium, patch -- HG2D-5: the phase overruns the +0x4e00 remap rows, not the +0xd70 distance rows.
- B3/E7 medium, patch -- HG2D-8: the direct upload is marked too; the HRESULT note is qualified.
- B4 medium, patch -- HG2D-6: with GLOBAL+0x1298 a valid span faults on retail's clobbered EBP; a separate part, with the fault-context limit noted.
- B5 low, patch -- HG2D-2: pixel access gets its own trigger.
- B6/E6 medium, patch -- G3D-3: the dispatcher's external branch and `native_thunk` are marked; MAIN's rebinding at install is cited.
- E5 low, patch -- G3D-5: the operand is compared as int16_t.
- B7/E4 low, patch -- G3D-7: the decoder writes the byte back shifted, which can never set the bit.
- B8 low, patch -- G3D-8's base caveat covers the one-point row only.
- B9/E3 low, patch -- PANEL-WINDOW-IDENT faults on reaching a null entry before a match.
- B10/E2 low, patch -- PANEL-SHRINK-GREY names both shrink functions and the interior-pixel condition.
- B11 low, patch -- PANEL-ZOOM-ADDRESS-2: only op 1 reaches the scaling; the variable's address case is PANEL-ZOOM-ADDRESS's.
- B12 low, patch -- PANEL-DIGITS: the icon pointer is the target and latch; the frame moves by one.

After the patches: the check passes (289 marked, 35 allowlisted, 7 no longer apply). I rebuilt and ran `tools/ofs difftests`: PANELS/G3D/HG2D.DLL and the one relinked harness (`g3dmeshdiff.exe`) hash as in `pre15i/`; the implementer's rerun of `sb15i.sh` matches `pre15i/sb`.

## Verification

**Commands:**
- `python3 -B tools/check_retail_bugs.py --self-test && python3 -B tools/check_retail_bugs.py`. Expected: the self-test passes, then the summary line, exit 0.
- `nice -n 19 cmake --build build/win32-retail -j 2 && CMAKE_BUILD_PARALLEL_LEVEL=2 nice -n 19 tools/ofs difftests`, then hash again, scratch-link again, and `diff` against `pre15i/`. Expected: empty.
