---
title: 'Story 1.4, slice 6: WINDOW, ACONTAIN and DYNAMIC data decompiled'
type: 'refactor'
created: '2026-11-20'
status: 'done'
route: 'dispatch'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43' # worktree held uncommitted Stories 1.2-1.4; file snapshot at ~/openfs98-verify/baseline-1-4f
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4e-booster-weatherm-demo-data.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** WINDOW, ACONTAIN and DYNAMIC still build their `.data` images from `orig/` through `gen_window_data.py`, `gen_acontain_data.py` and `gen_dynamic_data.py`. The results are hex dumps in `src/`:
- `window_data.inc`;
- `window_text.inc`, the 0x804-byte `text_constants` object (`window.c:202`, in `.rdata`, read through `CONST_AT()`/`M16`): 0x74 bytes of masks and limits, then 0x790 bytes copied from retail's `.text+0x3504..+0x3c94`, machine code that retail bug WIN-2 reads past the masks into. So retail code is in git today;
- `acontain_data.inc`, plus an already-typed colour table, `acontain_colours.inc`;
- `dynamic_data.inc`: 68 handler names, then bytes from +0x170.

**Approach:** Do what slice 5 did (`spec-1-4e`):
- typed structs and tables at retail's offsets replace the dumps;
- the generators and their build lines go;
- `acontain_colours.inc` stays as committed C, with its header reworded;
- the DYNAMIC handler table stays the list of C function names it is today, now in the source;
- **WINDOW's text (decision):** the 0x74 constants become typed C. `gen_window_data.py` shrinks to copying only the 0x790-byte code tail from `orig/` at build time, into `bin/` instead of `src/`, so retail code leaves git. It stays `gen:` until WIN-2 is fixed at porting time. The constants and the tail stay one contiguous 0x804-byte object, in that order.

The gate is stronger than slice 5's. Each whole DLL must be identical to the pre-slice build (`~/openfs98-verify/s14/bin`, with the COFF TimeDateStamp at pe+8, the optional-header CheckSum at opt+64 and the export directory's TimeDateStamp zeroed; WINDOW's `.edata` is stamped on every link), and `check_data_image.py` must still print identical. Today all three already match s14.

If a DLL's code changes anyway, as DEMO's `demo_init` did: report the normalized `objdump -d` diff per function, confirm `check_data_image.py` still prints identical, and stop for a human decision.

The difftests and `window-breaks` are queued until Story 1.3's Baselines run has finished.

## Boundaries & Constraints

**Always:**
- Use slice 5's conventions:
  - packed structs with `_Static_assert` on `sizeof` and the key offsets. Each object keeps its current alignment and section (check with `objdump -t`/`-h`), so `aligned(4)` goes only where it already holds. `window_image` and `text_constants` are byte-aligned today; `ac_image` and `dyn_image` are 4-aligned;
  - hex-float literals for any floating-point value;
  - "meaning not yet known" fields where a byte's meaning is unknown;
  - `D` and every offset-based access keep working unchanged. WINDOW's `D` names `window_image.bytes` (`:49`). When the typed struct replaces `bytes`, redefine `D` as `(uint32_t)(uintptr_t)window_image.import`, the first member's array, so M8, M16 and M32 are unchanged. The third review found that `&window_image` and `(uint8_t *)&window_image` change WINDOW's code: gcc hoists D+0x400 into a register. `window_image.import` gives an identical DLL. `CONST_AT()` call sites stay unchanged, because `tools/window_breaks.py:130` matches them as text.
- Each generator's asserts and docstring are the field list. Read them before deleting.
- Queue `windowdiff-run`, `window-breaks` (from a frozen copy), `acontaindiff-run` and `dynamicdiff-run` in `deferred-work.md` for after Story 1.3.
- Never `rm -rf bin`. No staging or commits. Record the files in `~/openfs98-verify/story-files.md`.

**Never:**
- No change to module code or behaviour beyond the data definitions.
- No other module is converted.

</frozen-after-approval>

## Code Map

- `cmake/steps/all.cmake:8` (ACONTAIN), `:85` (DYNAMIC), `:253` (WINDOW) -- the generator lines to remove. Re-read the line numbers first.
- `src/window/window.c`, `src/acontain/acontain.c`, `src/dynamic/dynamic.c` -- the only files that include the `.inc` files (repo-wide grep). Read how each defines its image and accesses it before changing it.
- `tools/gen_window_data.py` -- ImportTable at +0, Linkage at +0x68, state in a 0x8ac-byte `.data` (0x400 bytes on disk). Its only relocations are the Linkage's 32 code pointers, installed by `window.c`.
  - The layout, which the third review prototyped (it gave an identical DLL and no warnings): a packed, 1-aligned struct of
    - imports `{id, link}[13]`;
    - `linkage[36]`, zero, installed by `window.c`;
    - `stars[0x18]` at +0xf8;
    - list headers at +0x110 (8 u32) and +0x130 (7 u32);
    - a u32 table at +0x14c..+0x288 ("meaning not yet known");
    - profile keys `[0x28]` at +0x288;
    - bytes +0x2b0..+0x400 ("meaning not yet known");
    - zeros to 0x8ac.
  - `text_constants`:
    - the first 0x74 bytes: 22 u32 masks of the form 2^n-1, 0x18 zero bytes, then the map's pitch and zoom limit (0x4000, 0x0400);
    - then the 0x790-byte tail, from `bin/window/window_text_tail.inc`. Add `-Ibin/window` to WINDOW's gcc line;
    - the form, which the review built in a scratch copy with WINDOW's exact gcc line plus `-Ibin/window` (it gave a normalized-identical `WINDOW.DLL` and no warnings):
      ```c
      static const struct __attribute__((packed)) {
          uint32_t masks[22]; uint8_t snap_zeros[0x18];
          uint16_t pitch_limit, zoom_limit; uint8_t code_tail[0x790];
      } text_constants = { {0xF, ..., 0x7FFFFFF}, {0}, 0x4000, 0x0400, {
      #include "window_text_tail.inc"
      } };
      #define CONST_AT(a) ((uint32_t)(uintptr_t)&text_constants + ((a) - T_MASKS))
      ```
      Add `_Static_assert`s on `sizeof` (0x804) and on `offsetof` 0x58, 0x70 and 0x74. Only the `CONST_AT` body changes; its call sites stay the same;
    - the index can reach 0x803: `(ebx & 0xff80) >> 5` is at most 0x7fc, then +6, +1 byte.
  - `gen_window_data.py` keeps only the tail copy. The image half and the constants half go, but it keeps its existing mask, zero-block and pitch asserts. They check retail's `.text` against the script's own literals, not against the typed C, so the typed constants still have no retail oracle beyond s14 identity.
    - Its command becomes `python3 -B tools/gen_window_data.py orig/Modules/WINDOW.DLL bin/window/window_text_tail.inc`, with `mkdir -p bin/window` before it in the step.
    - The step regenerates the tail before it compiles, and still depends on the script and on `orig/Modules/WINDOW.DLL`. Rule (c) still lists it as `gen:`.
  - `window-break-build` (`cmake/targets.cmake:2092`) compiles `bin/difftest/window_break.c` with `-Isrc/window`. Add `-Ibin/window`.
    - `tools/window_breaks.py` writes `window_break.c` in the tree it runs in (:23, :185) and calls `tools/ofs window-break-build` (:188). `window-breaks` and `windowdiff` both `DEPENDS openfs98` (`targets.cmake:290`, `:2106`), so the tail is generated first.
    - `window-break-build` itself has no DEPENDS. Make `bin/window/window_text_tail.inc` a second OUTPUT of the WINDOW step, so a deleted tail is rebuilt.
- `tools/gen_acontain_data.py` -- the C defines ImportTable and Linkage. `acontain_data.inc` covers +0x78 to the end of the section: an aircraft.cfg key table, strings and two string pools, which start null at +0x39c/+0x3a0.
  - The image from +0x78:
    - 12 key rows of `{u32 id, int32 type, u32 alias}`; some types are -2, so the field is signed;
    - nine strings at +0x108..+0x134 and at +0x38c, +0x394 and +0x3a4;
    - 53 colour names at +0x13c..+0x38c, each padded to 4;
    - the pools at +0x39c/+0x3a0, an empty string `""` at +0x3a4, then 8 bytes of padding to +0x3b0.
  - Prototyped by the third review (`scratchpad/p3/src/acontain`): identical DLL, 12/12 relocations, no warnings.
  - `acontain_colours.inc` is the `.rdata` colour table (`{ name offset, id }` rows) and is already typed. Its literal offsets must keep pointing at those names, so add `_Static_assert`s on a few name offsets.
  - Our 12 relocations equal retail's.
- `tools/gen_dynamic_data.py` -- a 0x250-byte image. From Linkage+0x20, 68 handler slots name C functions, keeping retail's sharing (one function where retail repeats an address).
  - The 68 handler names are C identifiers already (`#ifdef DYN_OPS`).
  - Then 0xe0 bytes from +0x170: six `{u32 id 0xfe0b+k, GUID}` rows, whose GUIDs differ only in byte 12; then state, all zero except +0x228 = 1.
  - The form, needing no packing: `{u32 id, u32 d1, u16 d2, d3, u8 d4[8]}[6]`, then 0x40 bytes, then `uint32_t` = 1 at +0x228, then 0x24 bytes. Prototyped by the third review (`scratchpad/p3/src/dynamic`): identical DLL, 71/71 relocations.
  - Our 71 relocations equal retail's.
- `tools/raw_dumps_allowed.txt` -- remove `gen:gen_acontain_data.py`, `gen:gen_dynamic_data.py` and the four `.inc` dump entries (lines 62, 89, 90, 92). Keep `gen:gen_window_data.py`, with its reason changed to "copies WINDOW's .text tail into bin/ for WIN-2". `check_raw_dumps.py` should then print `27 allowed dump files, 54 scripts reading orig/ (29 gen, 10 resource, 15 oracle)`; confirm against its output.
- `docs/tools.md` Baselines -- add `check_data_image.py` lines for WINDOW, ACONTAIN and DYNAMIC, and update the dump-check count line. Docs:
- `docs/modules/window.md:24-26`, which says the generator zeroes the Linkage pointers and extracts the `.text` bytes. Now `window.c` installs the pointers, and only the tail is extracted.
- `docs/retail-bugs.md:255` (WIN-2): the constants are C, and the code tail is copied into `bin/window/` at build time. Add to its fix column: when the zoom is clamped (Epic 4), delete the tail, the trimmed `gen_window_data.py`, its build line and its `gen:` entry.
- Slice 1's spec note, if it names these.
- `windowdiff`, `acontaindiff` and `dynamicdiff` load the DLLs from `bin/`. The only other build of `src/window` is `window-break-build`, covered above.

## Tasks & Acceptance

**Execution:**
- [x] Record the normalized hashes of `bin/WINDOW.DLL`, `ACONTAIN.DLL` and `DYNAMIC.DLL` against s14, all equal today. Zero all three timestamp and checksum fields, using a normalizer kept outside the repo.
- [x] For each module in turn:
  - write the typed definitions;
  - delete the generator, its `.inc` dumps and its `all.cmake` line. ACONTAIN keeps `acontain_colours.inc`, with its header reworded. WINDOW keeps the generator, reduced to the tail, with its output in `bin/`, and `src/window/window_text.inc` is deleted;
  - run `tools/ofs all`;
  - check that the whole DLL is normalized-identical to s14 and that `check_data_image.py` prints identical.
- [x] WINDOW's cmake:
  - in the step, change the generator line to the tail copy, add `mkdir -p bin/window` before it, add `-Ibin/window` to the gcc line, and add the tail as a second OUTPUT;
  - in `window-break-build`, add `-Ibin/window`.
- [x] Update the allowlist, the Baselines lines, the module docs, and `deferred-work.md` (the four queued runs).

**Acceptance Criteria:**
- Given a clean `tools/ofs all`, when each of the three DLLs is compared with `~/openfs98-verify/s14/bin` after normalization, then it is identical, and `check_data_image.py` prints identical for all eight converted modules.
- Given the tree, when `check_raw_dumps.py` runs, then it exits 0 with the counts above.
- Given a temp copy that never writes the repo's `bin/`, when one byte of each module's new `.data` initialiser is changed and the module is rebuilt there, then `check_data_image.py` exits 1 and names the offset.
- Given the tree, when `git ls-files src/window` runs, then no file holds the code tail.
- After this slice, WINDOW's `.rdata` masks and tail have no retail oracle except s14 identity and `windowdiff`. Record that in `deferred-work.md`.

## Implementation Notes

- `tools/ofs all` builds with no warnings.
- The whole DLLs (WINDOW, ACONTAIN, DYNAMIC) are normalized-identical to `~/openfs98-verify/s14/bin`, with the COFF stamp, the CheckSum and the export-directory stamp zeroed. Checked by the subagent, then again independently, and again after the review patches. BOOSTER and WEATHERM are still identical too.
- `check_data_image.py` prints identical for all eight converted modules:
  - WINDOW 32/0, ACONTAIN 12/12, DYNAMIC 71/71;
  - BOOSTER 4/0, WEATHERM 20/20, DEMO 98/0, WINMSG 6/0, MPCHAT 4/0.
- `check_raw_dumps.py` prints `27 allowed dump files, 54 scripts reading orig/ (29 gen, 10 resource, 15 oracle)`.
- Negative cases, in a scratch copy that never wrote the repo's `bin/`: a one-byte change exits 1 and names the offset (WINDOW +0x16c, ACONTAIN +0x121, DYNAMIC +0x1d0).
- Deleting `bin/window/window_text_tail.inc` and running `tools/ofs all` regenerates it.
- `src/window/window_text.inc` and `window_data.inc` are deleted in the worktree, but still in the index until the user stages the deletion. Retail code then stays only in git history.
- Not run: `windowdiff-run`, `window-breaks`, `acontaindiff-run`, `dynamicdiff-run` (queued in `deferred-work.md`).

## Spec Change Log

## Review Triage Log

| # | Finding | Verdict | Evidence / route |
|---|---|---|---|
| 1 | A swapped handler in DYNAMIC's 68-slot table, or a wrong Linkage pointer, passes check_data_image, which zeroes relocated slots on both sides (VG, BH, EC) | medium | Today's tables are proven by whole-DLL identity with s14, whose generators asserted them against retail. Future edits are uncovered until `dynamicdiff-run`/`windowdiff-run` run. Deferred: extend the `data-images` proposal to resolve ours' relocated pointers to symbols and compare them with a committed retail-address map, or run the module difftests in the normal path. |
| 2 | The generators' value asserts are gone, and nothing runs check_data_image on every build (VG, BH, EC) | medium | Same root as the 1-4e `tools/ofs data-images` deferral; the entry is extended. |
| 3 | The trimmed gen_window_data.py would zero-fill a short `.text` slice (EC) | low | Patched: `assert len(text) == TEXT_HI - TEXT_LO`. |
| 4 | window_image field offsets are unasserted (EC, BH) | low | Patched: `rest` at 0x2b0 and `bss` at 0x400 are asserted. ACONTAIN already has a whole-size check (`ac_layout_check` typedef, :162), and DYNAMIC asserts +0x170 and +0x228. |
| 5 | deferred-work says WINDOW's masks are `.rdata` (BH) | low | Both are right: retail keeps them in `.text`, ours in `.rdata`. Reworded to say both, and that the generator checks its own literals. |
| 6 | Module docs only half updated (BH) | false | A repo-wide grep finds no reference to the deleted generators or `.inc` files outside `docs/implementation`; acontain.md and dynamic.md never named them. |
| 7 | The new check_data_image lines are not run by cmake_verify.py (BH) | false | `cmake_verify.py` reads every line of the `docs/tools.md` Baselines block, so the new lines run. |
| 8 | window-break-build fails on a tree without `all` (BH, EC) | low | `window-breaks` and `windowdiff` both DEPENDS `openfs98`, and the tail is a step OUTPUT. A bare `window-break-build` is internal to the runner. Rejected. |
| 9 | acontain_colours.inc keeps raw offsets instead of offsetof (BH) | low | Committed C, pinned by three asserts and by the image check. Rejected. |
| 10 | gen_window_data.py kind should be oracle:, not gen: (BH) | false | It feeds retail bytes into our DLL, so it is `gen:` by definition, until WIN-2 is fixed. |
| 11 | No check forces the WIN-2 cleanup (BH) | low | The register entry's fix column names it, and the `gen:` entry stays visible. Rejected. |
| 12 | Formatting: indentation, struct ac_key at file scope (BH) | low | Cosmetic. Rejected. |
| 13 | Spec boxes ticked without recorded evidence (BH) | low | Recorded in Implementation Notes. |

## Verification

**Commands:**
- The normalized whole-DLL comparison against s14 -- expected: identical for all three.
- `check_data_image.py` for WINDOW, ACONTAIN, DYNAMIC, BOOSTER, WEATHERM, DEMO and WINMSG (`orig/Modules/<M>.DLL bin/<M>.DLL`), and MPCHAT as `orig/Modules/mpchat.DLL bin/MPCHAT.DLL` -- expected: identical.
- `python3 tools/check_raw_dumps.py` -- expected: exit 0 with the new counts.
- Queued for after Story 1.3: `windowdiff-run`, `window-breaks`, `acontaindiff-run`, `dynamicdiff-run`.
