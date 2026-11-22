---
title: 'Story 1.4, slice 7: ANALYSIS, REALITY, LANDME and ATC data decompiled'
type: 'refactor'
created: '2026-11-20'
status: 'done'
route: 'dispatch'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43' # worktree held uncommitted Stories 1.2-1.4; file snapshot at ~/openfs98-verify/baseline-1-4g
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4f-window-acontain-dynamic-data.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** ANALYSIS, REALITY, LANDME and ATC still build their `.data` (and for ANALYSIS and REALITY, `.rdata`) images from `orig/`. Six hex dumps sit in `src/`: `analysis_data.inc`, `analysis_rdata.inc`, `atc_data.inc`, `landme_data.inc`, `reality_data.inc` and `reality_rdata.inc`.

`tools/check_data_image.py` cannot verify any of the four yet:
- ANALYSIS, REALITY and LANDME keep `ImportTable` partway into `.data` (+0x88, +0x48, +0x7f8), while the tool assumes it is at +0.
- ATC keeps retail's absolute code addresses verbatim, without relocations, and translates them on use (`ATC_PTR`), while the tool compares our bytes there raw against retail's zeroed relocation targets.

**Approach:**
- **Tool:**
  - (A) Locate our image start as `ours_ImportTable - (retail_ImportTable - retail .data start)`, reading retail's own `ImportTable` export. All twelve retail DLLs export it. If one doesn't, or its `ImportTable` lies outside `.data`, exit with an error.
  - (B) Opt-in only, through a `--kept` flag used for ATC alone. In that mode, every one of retail's relocation targets that is not one of ours must hold retail's raw, unzeroed 4 bytes ("kept"), and the output counts them as `(N relocated, M ours, K kept)`. A zeroed or changed kept slot fails. Without the flag, behaviour is today's: non-ours targets are zeroed on both sides. So the eleven other modules are not weakened.
- **Modules:** convert the four the way slices 5 and 6 did:
  - typed structs at retail's offsets;
  - `.rdata` constants as typed `static const` objects (doubles as hex floats);
  - the generators, dumps and build lines deleted.
- **Gate:** each whole DLL normalized-identical to `~/openfs98-verify/s14/bin` (COFF stamp, CheckSum and export-directory stamp zeroed; all four match today), and the extended tool prints identical for all twelve converted modules. If a DLL's code changes, report the per-function normalized `objdump -d` diff and stop for a human decision.
- **Queued until Story 1.3's Baselines run finishes:** `atcdiff-run`, `landmediff-run`, `realitydiff-run`, `reality-breaks`, `analysisdiff-run` and `analysis-breaks`.

## Boundaries & Constraints

**Always:**
- Use slices 5 and 6's conventions.
  - Packed structs with `_Static_assert`s on `sizeof` and key offsets. Each object keeps its current type alignment and section; check with `objdump -h`/`-t`.
  - Hex-float literals for doubles, and "meaning not yet known" for unexplained bytes.
  - `D`, `ATC_PTR` and every offset-based access stay working unchanged. Where `D` names an array member, redefine it through the first member's array, as in slice 6. LANDME's `D` is a `uint8_t *` (`#define D (landme_image.bytes)`, used as `D + offset`), so its first member stays a `uint8_t` array or `D` is cast to `(uint8_t *)`; a wider first member would scale every offset.
- The existing `.set _ImportTable, _<m>_image + off` aliases stay as they are.
- REALITY's `.rdata` object is 0x1000 bytes. Its first 0x19c bytes are retail's whole `.rdata`:
  - the feet table at +0x00;
  - the metric table at +0x60 (stride 0x30 in use);
  - 15 doubles at +0xc0;
  - retail's export directory words at +0x138..+0x19c, timestamp 0x33e94d61 and RVAs included, which the graph's overrunning scale search reads.

  The rest is zero to the page end, deliberately: shrinking the pad changed the DLL in the review's prototype. Keep the object whole, in order, at its size, and use designated initialisers, because a positional initialiser with an unnamed zero tail warns under `-Wextra`.
- ANALYSIS's 9 doubles are read by offset (`RD(o)`). They stay one object, alone at `.rdata` +0, in order (`static const double analysis_rdata[9]`, `aligned(8)`, hex floats; prototyped identical).
- Check (a) must not flag the new C, and no new allowlist entries are allowed. Write tables as one brace-wrapped `{…}` record per line, and strings as strings. REALITY's 0x138-byte crack table written 8 values per line trips rule 1, as the review found.
- Each generator's asserts and docstring give the field list. Read them before deleting.
- Never `rm -rf bin`. No staging or commits. Record the files in `~/openfs98-verify/story-files.md`.

**Never:**
- No module code or behaviour changes beyond the data definitions.
- No other module is converted.

</frozen-after-approval>

## Code Map

- `tools/check_data_image.py` -- add (A) and (B).
  - (A): read retail's export table the same way ours is read now. Compute `off = retail_ImportTable_rva - retail_data_rva`, then `start = ours_ImportTable - off`.
  - (B): in the compare, for each of retail's relocation targets `t` that is not one of ours, compare `mine[t:t+4]` with retail's raw (unzeroed) bytes. Keep the "ours ⊆ retail" relocation guard.
  - Print the kept count, and update the docstring.
  - A survey ran both changes as scratch variants: ANALYSIS, REALITY and LANDME passed with (A), and ATC passed with (A)+(B). The eight already-converted modules must still print identical.
- `cmake/steps/all.cmake`, the generator lines to remove (re-read the line numbers): `:27` (ANALYSIS, two outputs), `:37` (ATC), `:142` (LANDME), `:203` (REALITY, two outputs).
- `src/analysis/analysis.c` -- the image object, with `.set _ImportTable, _analysis_image + 0x88` at `:1236`. `.data` is 0x32c bytes, about 83 non-zero. `analysis_rdata.inc` holds 9 doubles.
- `src/reality/reality.c` -- `_reality_image + 0x48` (`:811`). `.data` is 0x510 bytes, about 250 non-zero. `reality_rdata.inc` is the whole `.rdata`.
- `src/landme/landme.c` -- `_landme_image + 0x7f8` (`:789`). `.data` is 0x854 bytes, about 1,070 non-zero, so look for strings and tables. The records at +0x683 (stride 0x10 per pair) are unaligned, so they need packed record types.
- `src/atc/atc.c` -- `atc_data.inc` holds the image from +0x70, with ImportTable and Linkage defined in C; the compared size is 0x1a00. Retail's code addresses are kept as values and translated by `ATC_PTR` (see `tools/gen_atc_data.py`'s docstring). Those values stay exact retail addresses; (B) checks them.
  - The 55 kept slots are:
    - 21 template pointers at +0x4a6..+0x4f6;
    - two handler tables, 14 and 6 code addresses, at +0x739 and +0x771;
    - 6 pointers at +0x789..+0x79d;
    - 8 at +0x7c4..+0x7e0.

    Write them as `ATC_VA(off)`, or retail's literal addresses, with a comment.
  - The review's prototype of the tool printed `ATC: … (62 relocated, 7 ours, 55 kept)`.
  - `atc.c:786` says "the generator checks them". Point it at `check_data_image.py --kept`.
  - `ATC_DATA_SIZE` is 0x2000, while the check compares 0x1a00. The zero tail stays inside the `.data` object.
- These `.inc` files are included only by their own module `.c` and written only by their generators. `analysis-break-build` and `reality-break-build` use the same flags as `all.cmake`. Their runners copy the `.c` to `bin/difftest/*_break.c` and patch only macro lines (`R + …`, `AN_MEMMOVE_ECX`), so keep those macro lines textually unchanged. The other docs that name these generators are `docs/modules/{analysis,reality,landme,atc}.md`.
- `tools/raw_dumps_allowed.txt` -- remove the four `gen:` entries and the six dump entries. `check_raw_dumps.py` should then print `21 allowed dump files, 50 scripts reading orig/ (25 gen, 10 resource, 15 oracle)`; confirm against its output.
- `docs/tools.md` Baselines -- add the four `check_data_image.py` lines, update the eight existing ones if the output format changes, and update the count line.
- Docs -- the module docs and `docs/retail-bugs.md` entries that name these generators or dumps; grep for them.

## Tasks & Acceptance

**Execution:**
- [x] Extend `tools/check_data_image.py`. Before any C change, all twelve modules must print identical: the eight done ones, plus ANALYSIS, REALITY, LANDME and ATC against today's dump-built DLLs.
- [x] For each module in turn:
  - typed definitions, with `_Static_assert`s;
  - delete the generator, its dumps and its `all.cmake` line;
  - `tools/ofs all`;
  - whole-DLL normalized identity with s14, and `check_data_image.py` identical.
- [x] Update the allowlist, the Baselines lines and the docs. In `deferred-work.md`, queue the six runs, and note that the `.rdata` objects (ANALYSIS's doubles, REALITY's whole `.rdata`) have no retail oracle except s14 identity and the difftests, because the tool compares `.data` only.

**Acceptance Criteria:**
- Given a clean `tools/ofs all`, when each of the four DLLs is compared with s14 after normalization, then it is identical, and `check_data_image.py` prints identical for all twelve converted modules.
- Given the tree, when `check_raw_dumps.py` runs, then it exits 0 with the counts above.
- Given a temp copy that never writes the repo's `bin/`, when one `.data` byte of each module's new initialiser is changed and rebuilt, then `check_data_image.py` exits 1 and names the offset. For ATC, also change one kept code address, and separately zero one: with `--kept`, both must fail.

## Implementation Notes

- `tools/ofs all` builds with 0 warnings.
- The whole DLLs (ANALYSIS, REALITY, LANDME, ATC) are normalized-identical to `~/openfs98-verify/s14/bin`. Checked by the subagent, then independently, and again after the review patches.
- `check_data_image.py` prints identical for all twelve converted modules; ATC (`--kept`) prints `62 relocated, 7 ours, 55 kept`. `check_raw_dumps.py` prints `21 allowed dump files, 50 scripts reading orig/ (25 gen, 10 resource, 15 oracle)`.
- Negative cases were run by patching one `.data` byte in a scratch copy of each built DLL. That exercises the same tool check as an initialiser edit, but is not the rebuild the AC describes. Each module exits 1 and names the offset: ANALYSIS +0x108, REALITY +0xd0, LANDME +0xd7, ATC +0x4fb. With `--kept`, ATC exits 1 for a changed kept address and for a zeroed one (+0x739).
- Every break pattern still applies exactly once to the new sources: REALITY 67 of 67 and ANALYSIS 103 of 103, checked by loading each runner's `BREAKS` list without running it.
- Not run: the six queued runs (`deferred-work.md`).

## Spec Change Log

## Review Triage Log

| # | Finding | Verdict | Evidence / route |
|---|---|---|---|
| 1 | LANDME's `D` is `landme_image.state` (0x8c bytes) but reaches +0x854: out of bounds for that member, undefined behaviour the optimiser may exploit (BH) | medium | Patched: `D` is now `((uint8_t *)(uintptr_t)&landme_image)`, tested first in a scratch build; LANDME.DLL is still identical to s14. |
| 2 | Our image start is not checked to lie inside our .data (BH, EC) | low | Patched: the tool exits naming both ranges. |
| 3 | A kept slot that is zero in both images passes (EC) | low | Patched: a kept slot must hold a non-zero retail address. ATC still gives 55 kept. |
| 4 | `--kept` parsing accepts typos as paths (BH, EC) | low | Patched: unknown `--` arguments give the usage message (exit 1). |
| 5 | `.rdata` objects (ANALYSIS's doubles, REALITY's whole .rdata) have no retail comparison (VG, BH, EC) | medium | Real, and recorded in the slice's deferred entry. An `.rdata` mode is new tool surface. Deferred, folded into the `tools/ofs data-images` proposal: compare retail's `.rdata` with ours, located by a symbol or export. |
| 6 | Typed initialisers guarded only by a manually run script (VG) | medium | Same root as the 1-4e/1-4f `data-images` deferral. |
| 7 | Runtime-installed pointers and ATC's step arrays are not tied to retail (VG) | medium | Covered by the 1-4f deferral (symbol-per-slot check) and the queued difftests. |
| 8 | No self-test for the tool's `--kept` and edge paths (VG, BH) | low | Exercised by hand above; a mutate-and-fail check goes with `data-images`. Deferred. |
| 9 | Partial `_Static_assert` coverage in atc.c, analysis.c, reality.c (BH, EC) | low | Every byte is checked by the image comparison, and the DLLs are identical. Rejected. |
| 10 | Three spellings of the import-table type (BH) | low | Cosmetic. Rejected. |
| 11 | Raw retail code addresses in ATC with no names (BH) | low | They are kept retail values by design (`ATC_PTR`), each commented. Rejected. |
| 12 | Table style: unsigned hex negatives, sparse designated rest, repeated comments (BH, EC) | low | `check_raw_dumps` exits 0. Cosmetic. Rejected. |
| 13 | Docs elsewhere may name the deleted generators; epics.md mentions "26 committed *_data.inc" (BH, EC) | low | The module docs are updated. The epics line states the story's starting point. Rejected. |
| 14 | Spec ticked before the behavioural runs (BH, EC) | low | By design: the runs are queued until Story 1.3 finishes. Recorded in Implementation Notes. |

## Verification

**Commands:**
- Whole-DLL normalized comparison against `~/openfs98-verify/s14/bin` for ANALYSIS, REALITY, LANDME and ATC -- expected: identical.
- `check_data_image.py` for all twelve (MPCHAT as `orig/Modules/mpchat.DLL bin/MPCHAT.DLL`, ATC with `--kept`) -- expected: identical. The ATC line prints `55 kept`, and the Baselines line states that count so a drift is visible.
- `python3 tools/check_raw_dumps.py` -- expected: exit 0 with the new counts.
- Queued for after Story 1.3: the six runs above.
