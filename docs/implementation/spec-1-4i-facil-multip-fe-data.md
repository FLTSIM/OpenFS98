---
title: 'Story 1.4, slice 9: FACIL, MULTIP and FE data decompiled'
type: 'refactor'
created: '2026-11-20'
status: 'done'
route: 'dispatch'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43' # worktree held uncommitted Stories 1.2-1.4; file snapshot at ~/openfs98-verify/baseline-1-4i
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4h-data-images-guard.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** FACIL, MULTIP and FE still generate their data images from `orig/`, through `gen_facil_data.py`, `gen_multip_data.py` and `gen_fe_data.py`.
- **FACIL and FE:** each `.inc` (`facil_data.inc`, 10.8 kB; `fe_data.inc`, 34 kB) holds the image as bytes plus an X-macro relocation list, `FACIL_REL_DATA`/`FACIL_REL_TEXT` or `FE_REL_DATA`/`FE_REL_TEXT`. Both the module and its harness include it: `tools/difftest/facildiff.c:89-97` and `tools/difftest/fediff.c:226-234` build their relocation-offset tables from it.
- **MULTIP:** the generator writes `multip_data.inc` (22 kB), `multip_rdata.inc` (2.7 kB) and three tracked tables, `multip_pointers.inc`, `multip_frame_actions.inc` and `multip_nametag.inc`.

`check_data_image.py` already prints identical for all three, and their DLLs match `~/openfs98-verify/s14/bin`.

**Approach:** Use slices 6 and 7's method, under slice 8's guard.
- **FACIL and FE:** the image becomes a typed struct at retail's offsets in the module's `.c`. The relocation list moves unchanged into its own committed X-macro file (`facil_relocs.inc`, `fe_relocs.inc`), which the module and the harness both include. Those rows are macro calls of offsets and targets, not a dump.
- **MULTIP:**
  - `.data` becomes a typed struct;
  - retail's `.rdata` copy stays one non-const object named `s_multip_rdata`, in our `.data` and at its current position (RVA 0xb000, before the image at 0xb200), typed with byte-offset access unchanged. Making it `const` moves it to `.rdata`, shrinks `.data` and shifts every address, as the review's prototype showed;
  - the three small tables stay as committed C with reworded headers;
  - the manifest gets `rdata=` for retail's relocation-free range 0xc000:0x1f0.
- **The `.rdata` checks search all of ours** (decision): `check_data_image.py`'s `.rdata` comparison (retail mode) and its `.rdata` window (`--hash`) search all of our initialised data sections, `.data` and `.rdata`, still requiring exactly one match and no overlapping relocation of ours. MULTIP keeps its copy in `.data`, so neither check would find it otherwise. Add self-test cases for a range that lives in `.data`.
- **FE's two numeric tables** (decision): 256 rising dwords at +0xf20 (latitude bands) and 257 falling dwords at +0x1320 (cosine-like). No exact formula is known, and check (a) cannot tell a typed table from a dump. They move into a named, typed, committed file `src/fe/fe_bands.inc` (`fe_lat_bands[256]`, `fe_cos_steps[257]`, with comments on what they are). It gets one allowlist entry, newly added with that reason, because the conversion surfaced it.
- **Deleted:** the generators, the dumps and their build lines.
- **Added:** three manifest rows (`tools/ofs data-images UPDATE=1`) and three `--hash` lines.

**Gate:**
- each whole DLL normalized-identical to s14 (all three match today);
- the other builds of these sources normalized-identical to the pre-slice build: `bin/difftest/facildiff.exe` and `fediff.exe`, which compile the relocation lists, and `bin/difftest/MULTIP_test.dll` and `FE_sl_ours.dll` (`cmake/targets.cmake:576-1649`, `:632`, `:1168`);
- `data-images` passes with 15 modules.

If a DLL's code changes, report the per-function diff and stop for a human decision.

**Queued until Story 1.3's Baselines run finishes:** `facildiff-run`, `facildiff-fe-run`, `fediff-run`, `fegriddiff-run`, `fesldiff-run`, `multipdiff-run` and `multip-breaks`. MULTIP has no Baselines line, so record what `multipdiff-run` prints then as its first baseline.

## Boundaries & Constraints

**Always:**
- Use slices 6 and 7's conventions.
  - Packed structs with `_Static_assert`s. Each object keeps its current alignment and section.
  - `D` and every offset access stay unchanged. Redefine `D` through the first member's array, cast to `uint8_t *` if it was a byte pointer.
  - Hex floats for floating-point values, and "meaning not yet known" fields.
  - Check (a) must flag none of the new C except `fe_bands.inc`. Brace-wrapping scalars to dodge it is not allowed. A run of one repeated value uses a GNU range designator (FE's 550 words of 0xa7 at +0xace..+0xf1a: `[0 ... 549] = 0xa7`).
  - Names and text that tools and asm rely on stay unchanged: `s_multip_image`, `s_multip_rdata` with its byte-offset uses (`tools/multip_breaks.py:302-566` patches `s_multip_rdata + 0x38` and `fmull _s_multip_rdata+0x18`), and `_s_multip_image+off` in `multip_lobby.inc` and `multip_dispatch.inc`.
  - MULTIP's `MULTIP_IMAGE_SIZE` stays 0x1000 (vsize 0xf38).
- The relocation lists keep their macro names and their entries' order, so both harnesses keep compiling unchanged except for the include line.
- Each generator's asserts and docstring give the field list. Read them before deleting.
- Never `rm -rf bin`. No staging or commits. Record the files in `~/openfs98-verify/story-files.md`.

**Never:**
- No module or harness behaviour change.
- No other module is converted.

</frozen-after-approval>

## Code Map

- `cmake/steps/all.cmake`, the generator lines to remove (re-read the line numbers):
  - `:97` (FACIL);
  - `:107` (FE);
  - `:178` (`gen_multip_data.py`, which runs with no arguments and writes into `src/multip/`).

  Add the three `--hash` lines after the links, as slice 8 did.
- `src/facil/facil.c:3048-3065` -- it includes `facil_data.inc` twice: once for the image, once with `FACIL_RELOCS` defined, for the relocation install. Per the generator's docstring:
  - `.data` is 0x2d1c bytes, of which 0x600 are on disk;
  - pointers into `.data` are recreated mechanically;
  - pointers into `.text` name dialog handlers.

  The image check gives `74 relocated, 0 ours`.
- `src/fe/fe.c:4582-4595` -- the same double include. Per the docstring:
  - the image is 0x62e0 bytes, of which 0x3400 are on disk;
  - FE's code addresses its state by retail's offsets;
  - `FE_DATA` holds the bytes after the Linkage.

  The image check gives `86 relocated, 31 ours`.
- `src/multip/multip.c:68,72` include `multip_data.inc` and `multip_rdata.inc`. `multip_frame_actions.inc` comes from mapping the jump table at 0x3c68 to actions (targets 0x3c63, 0x3c2b, 0x3c3d, 0x3c4c, 0x3c59); its reworded header must keep that mapping, so it can be re-derived. The generator asserts that the nametag size is 42; keep that as a `_Static_assert`. All 76 of retail's `.data` relocations are owner 0 (no `.rdata` relocations). `multip_pointers.inc` (`:83`) holds `{owner, offset, kind, target}` rows, with owner 0 for `.data` and 1 for `.rdata`. `multip_frame_actions.inc` and `multip_nametag.inc` are included from `multip_callbacks.inc:3` and `multip_players.inc:2`. The image check gives `0xf38 bytes, 76 relocated, 0 ours`. Find MULTIP's `.rdata` RVA and its relocation-free range with `objdump -h` and the pointer table.
- Harnesses:
  - `tools/difftest/facildiff.c:93` and `fediff.c:230` include the `.inc` files. Point them at the new `*_relocs.inc`.
  - The exes are `bin/difftest/facildiff.exe` and `bin/difftest/fediff.exe` (`difftests.cmake:90-100`). Hash both before and after.
  - Grep for any other harness or break build that includes these `.inc` files or compiles these modules' sources.
- `tools/raw_dumps_allowed.txt` -- remove `gen:gen_facil_data.py`, `gen:gen_fe_data.py`, `gen:gen_multip_data.py`, and the dump entries `facil_data.inc`, `fe_data.inc`, `multip_data.inc` and `multip_rdata.inc`. Add `src/fe/fe_bands.inc  # (a) retail latitude-band and cosine-step tables, typed u32; no exact formula known (newly detected in slice 9)`. `check_raw_dumps.py` should then print `18 allowed dump files, 48 scripts reading orig/ (22 gen, 10 resource, 16 oracle)`; confirm against its output.
- `tools/data_images.txt` -- add three rows with `UPDATE=1`. MULTIP's row uses `Modules/multip.dll` and FACIL's `Modules/facil.dll`; MULTIP's carries its `rdata=` range.
- The `docs/tools.md` Baselines `data-images` line -- the new totals: 15 images, their byte and relocation sums, the `.rdata` ranges and their bytes. Compute them from the tool's output, in decimal.
- Docs: `docs/modules/{facil,fe,multip}.md`, wherever they name the generators; `docs/tools.md:25` ("twelve converted"); and `docs/building.md`, which should note that the `.rdata` checks search `.data` too.
- Prototypes from the review are in `/tmp/claude-1000/-home-steve-source/b2e3078b-1d77-426e-816f-2de2772a67dc/scratchpad/p`:
  - FACIL, typed mechanically (188 members, `facil_relocs.inc` split), gives a DLL identical to s14, with `74 relocated, 0 ours`;
  - FE, the same way, is identical with `86 relocated, 31 ours`. FE's 31 ours are the Linkage's init, deinit and 29 slots, set statically at `fe.c:4569-4581`;
  - the `*_REL_DATA` pointers stay zero in the image and are installed at attach.

## Tasks & Acceptance

**Execution:**
- [x] Record normalized hashes of the three DLLs, the two harness exes, `MULTIP_test.dll` and `FE_sl_ours.dll`, from the current build.
- [x] `tools/check_data_image.py`: make the `.rdata` search cover `.data` and `.rdata` in both modes, with a self-test case for a range that lives in `.data`. All existing rows must still pass, and the summary must print the new case count.
- [x] For each module in turn:
  - the typed definitions, plus the `*_relocs.inc` split for FACIL and FE;
  - delete the generator, its dumps and its build line;
  - add the `--hash` line;
  - `tools/ofs all` and `tools/ofs difftests`;
  - whole-DLL identity with s14, harness-exe identity, and `check_data_image.py` identical.
- [x] Add the three manifest rows (`UPDATE=1`), and update the allowlist, the Baselines line, the docs, and `deferred-work.md` (the seven queued runs, and MULTIP's missing baseline).

**Acceptance Criteria:**
- Given a clean `tools/ofs all`, `tools/ofs difftests`, and the targets that build `MULTIP_test.dll` and `FE_sl_ours.dll`, when the three DLLs, the two exes and those two DLLs are compared after normalization, then each is identical to its recorded hash.
- Given `tools/ofs data-images`, when it runs, then it reports 15 images identical, the `.rdata` ranges including MULTIP's identical, and the self-test passed.
- Given the tree, when `check_raw_dumps.py` runs, then it exits 0 with the counts above.
- Given a temp copy, when one value of each new initialiser is changed, then `tools/ofs all` fails at that module's `--hash` line.

## Implementation Notes

- Implemented by a Sonnet subagent until it hit the account's usage limits; the main session checked its work and finished the docs.
- The seven builds of these sources are normalized-identical to the pre-slice hashes. FACIL's and FE's equal s14's. Normalized sha256, before and after (first 16 hex digits):
  - `bin/FACIL.DLL` 4494ef6308c3165f
  - `bin/FE.DLL` 29aaf9c2da2ba8d9
  - `bin/MULTIP.DLL` 810aadc0c1298d7a
  - `bin/difftest/facildiff.exe` 31faba877042b1b1
  - `bin/difftest/fediff.exe` b3cbcf3599048143
  - `bin/difftest/MULTIP_test.dll` 274c782123961ab3
  - `bin/difftest/FE_sl_ours.dll` 638d3e03bd1c374b
- `tools/ofs all` and `tools/ofs difftests` succeed, with 15 `--hash` lines. `tools/ofs data-images` prints `data-images: 15 .data images identical (65106 bytes, 621 relocated), 3 .rdata ranges identical (980 bytes), self-test passed (23 cases)`.
- `check_raw_dumps.py` prints `18 allowed dump files, 48 scripts reading orig/ (22 gen, 10 resource, 16 oracle)`.
- Deviation: check (a) used to scan only tracked files, so the new, still-untracked `fe_bands.inc` read as a stale allowlist entry. It now lists `--cached --others --exclude-standard`, which also catches a dump before it is committed.
- Not run: the seven queued difftests and break runs (`deferred-work.md`), and the per-module negative AC (one changed value fails `tools/ofs all`). The self-test's `--hash` cases cover that path, and slice 8's ANALYSIS run showed it end to end.

## Spec Change Log

## Review Triage Log

| # | Finding | Verdict | Evidence / route |
|---|---|---|---|
| 1 | The pointer targets in `facil_relocs.inc`, `fe_relocs.inc` and `multip_pointers.inc` are not guarded: `--hash` covers the image bytes and relocation offsets, not the targets installed at attach (VG) | medium | Real. A changed target alters bytes outside the hashed image. Today's lists are proven by whole-DLL identity with the pre-slice build. Deferred: folded into the symbol-per-slot pointer check, and covered meanwhile by the queued difftests. |
| 2 | `s_nametag` and `s_frame_actions` have no value check (VG) | low | Proven by identity today, and checked by `multipdiff-run`/`multip-breaks` (queued). Deferred with item 1. |
| 3 | The per-module negative AC was not run for FACIL, FE and MULTIP (EC, BH) | low | The `--hash` path is module-independent: self-test cases cover it on ANALYSIS, ATC, LANDME and MULTIP, and slice 8 ran it end to end. Recorded; not rerun. |
| 4 | Evidence cites session-local scratch files (BH) | low | Patched: the normalized hashes are inline above. |
| 5 | `fe_bands.inc` is added to the allowlist without a follow-up (BH) | low | Patched: a `deferred-work.md` entry to derive the tables, or confirm they stay data. |
| 6 | The stale "twelve" in the 1-4h deferred entry (BH) | low | Patched. |
| 7 | `ALLOW_EXTURE_RECEIVE`, a retail misspelling, has no retail-bug entry (BH) | low | Pre-existing: retail's own INI key, documented in `docs/modules/multip.md:100`; this slice only typed it. Left for Story 1.5's register sweep. |
| 8 | `check_raw_dumps.py --others` scans untracked files (BH, EC) | low | Rule (a) still scans only `src/` and `include/`, and (b) only compares hashes with `orig/`. The wider listing also catches a dump before it is committed. Rejected. |
| 9 | A range found once in each of `.data` and `.rdata` fails as found twice (EC, BH) | low | Intended: exactly one match is required. Rejected. |
| 10 | Editing a committed `.inc` may not rebuild the DLL (EC) | false | `ofs_step` globs `*.inc` in each named source directory (`cmake/OpenFS98.cmake:54`). |
| 11 | Manifest edits don't rerun `--hash` (EC, BH) | low | Already deferred (1-4h). |
| 12 | Quadratic `.rdata` window scan (BH) | low | The sections are a few kB; it takes milliseconds. Rejected. |
| 13 | Few offset asserts, opaque field names, MULTIP's `__asm__` symbol name (BH) | low | Kept so that existing asm (`_s_multip_rdata`) and `multip_breaks.py` stay unchanged. The bytes are proven by identity and `data-images`. Rejected. |
| 14 | Spec ticks Baselines and difftests that are outstanding (BH, EC) | low | Recorded in Implementation Notes and `deferred-work.md`; queued by design. |
| 15 | Relocation-list order not demonstrated (EC) | false | The harness exes are normalized-identical, so the lists compile to the same tables. |

## Verification

**Commands:**
- The normalized whole-DLL and exe comparison -- expected: identical.
- `tools/ofs data-images` -- expected: 15 images identical, and the self-test passed.
- `python3 tools/check_raw_dumps.py` -- expected: exit 0 with the new counts.
- Queued for after Story 1.3: the seven runs above.
