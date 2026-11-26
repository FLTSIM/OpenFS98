---
title: 'Cleanup 4b: check_data_image checks each image object''s real extent (sizeof-derived end symbols)'
type: 'chore'
created: '2026-11-25'
status: 'done'
route: 'dispatch'
baseline_commit: '41a4a98bdfdf0784cd60f1fa45dcf6d6a5027313'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-cleanup-4-raw-data-guards.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** `docs/implementation/deferred-work.md`, entry R2 (labelled 4b). `tools/check_data_image.py` reads retail's length from our image's start and checks only that the window lies inside our section (`our_image`, `:161-188`). An image object that shrank would compare whatever follows it against retail's tail: zero padding, or the next object. That passes whenever those bytes happen to match. Codex reproduced it: `737ADI`'s `gauge_data` shortened by one byte still compiled and passed the hash, because the zero fill supplied the last byte.

**Facts (Codex design study, 2026-11-25):** every one of the 161 compared windows is a single C object. 148 rows compare the whole object (kind A). 13 compare part of one on purpose (kind B):
- ATC: `atc_image`, a `0x1a00` prefix of `0x2000`;
- NUMERIC: `fs98_numeric_data`, a `0x321` prefix of `0x400`;
- the 11 MDLs: `mdl_image`, a suffix at `+0x3000` in `.data`, or at `+0x5000` in `.mdlimg` for the three `_C` models.

COFF symbols alone cannot give extents (cleanup 4a's review): alignment, aliases such as `_Linkage` and `_mdl_data`, and the sub-windows defeat them.

**Approach (the user's choice at design time, 2026-11-25):**
- A shared macro emits, next to each image object, a global COFF symbol `<obj>_end` equal to `<obj> + sizeof <obj>`. The compiler evaluates `sizeof`; the symbol adds no code or data bytes.
- The tool reads the object's real start and end from the linked binary, and requires the compared window to be exactly the object, or a declared prefix or suffix of it.

## Boundaries & Constraints

**Always:**
- **The macro:** `FS98_IMAGE_END(obj)`, in a new `include/fs98/image_end.h`. GCC 13 rejects file-scope extended asm, so the macro defines a `used`, byte-aligned helper function with a `.L`-style assembler name. Inside it, an extended asm `.set %c0_end, %c0 + %c1` takes the object's own assembler-name operand and `sizeof obj`, then `__builtin_unreachable()`.
  - **Local alias:** the end symbol is a local (not `.globl`) COFF alias, named from the compiler's symbol operand. So `text_tail` gives `_main_text_tail_end`, and static objects keep their actual names. Codex showed a global alias gets exported under automatic export, while a local one is kept in `.data`, `.rdata` and `.mdlimg` and never exported.
  - **No other output change:** the macro must add no bytes to any section. That includes `.text`, `.eh_frame`, `.drectve` and debug data. It must not change any export, and it must survive the link into the PE's COFF symbol table.
  - **HOST and SIMSUI:** the helper changed `.eh_frame` for both in Codex's scratch builds.
    - For HOST, the end directive goes at the start of the existing `DllMain` (or another existing function) instead of a new helper.
    - For SIMSUI, its image TU alone gets `-fno-asynchronous-unwind-tables`.

    Both preserved section bytes in scratch objects. The gate decides.
  - **Placement:** it is used once per image object, in the defining TU, outside initializers and X-macro includes. A name that resolves to two symbols is rejected at lookup.
  - It is used once per image object:
    - the module and host sources that define each row's object;
    - the shared gauge header, once, for `gauge_data` (one image per separately linked gauge);
    - `src/mdl/mdl_image.c`, for `mdl_image`;
    - MAIN's `text_tail`, the `tail=` window, which enters through `main.c`.
  - **Recipe edits allowed, flags only:**
    - an include path for the MDL and SIMSUI image compiles (`tools/build_mdls.py` and the CMake recipes);
    - SIMSUI's one per-TU flag above;
    - the new header in `tools/build_gauges.py`'s freshness list.

    The gate must show none of them changes a code or data byte.
- **The manifest:** all checker metadata goes in `tools/data_images.txt`, which has 30 individual rows (WARNING and SUCTION included), 120 gauges and 11 MDLs. The gauge JSON and MDL JSON are build inputs, not checker rows, and they are not edited.
  - Every row gains `obj=SYMBOL`, naming its image object (`gauge_data` for the gauges). For A rows the window must be the whole object.
  - The B rows declare their sub-window:
    - `extent=prefix` for ATC and NUMERIC: start = object start, `len` ≤ the object size, which covers through retail's last byte;
    - `extent=suffix:OFF` for the MDLs: start = object + OFF, start + `len` = the object end.
  - `read_manifest` requires `obj=` on every row, and validates `extent=` where present.
  - `format_row` preserves both fields.
  - The existing `sha256=` and every other existing field stay byte-identical.
  - The new fields are not hashed into `sha256=`: they are checked structurally.
- **The check:** a function in `check_data_image.py`. W = `[start, start+len)` is first resolved as today, from `ImportTable - off` or `at=`. The check then runs in the retail comparison (`--orig`/manifest), in `--hash` (single and batch), and on the `--linked` rows:
  - `--gauge-pairs` validates row metadata and membership at configure time, and the later batch `--hash` checks the binaries;
  - `coff_symbol` returns the section identity alongside the RVA;
  - `_<obj>` and `_<obj>_end` must each resolve uniquely, in the same section, with `end > obj`. The end may lie at the section boundary;
  - A rows: `start == obj` and `start + len == end`;
  - prefix: `start == obj` and `start + len <= end`;
  - suffix: `start == obj + OFF` and `start + len == end`;
  - `tail=`: in `our_tail()`, `tail.len == _main_text_tail_end - _main_text_tail`;
  - `bound=` (SIMSUI), in both paths: `start == obj` and `row.len == BOUND_END - BOUND_START == end - obj`;
  - the existing section and relocation checks stay for every window.
  - **Failures:** a missing symbol, a missing or mismatched field, or a failed equation fails with a message naming the row, the symbol and the numbers.
  - **Hard errors under `UPDATE=1`:** a structural failure is a hard error there too. Today `run_hash` downgrades every error under `UPDATE`, so structural failures get a separate hard-error path. And `run_manifest` may not rewrite `len=` unless the extent check agrees. `UPDATE` never writes `obj=` or `extent=`: they are edited by hand, once, in this slice.
  - **Out of scope:** the legacy two-file CLI (`<retail.dll> <ours.dll>`) has no object metadata and keeps its behaviour; the notes and docstring say so.
  - `rdata=` and `switch=` are out of scope: they are content searches, already section-bounded, unique and relocation-checked. The notes say so.
- **The identity proof:** the code and data bytes of every built PE are unchanged.
  - **Build:** build before (`HEAD`) and after at one root (`~/openfs98-verify/cl4b/root`) with `~/openfs98-verify/gate/build_side.sh`.
  - **The gate gains one strict mode,** a new option in `pehash.py` and `pehash_norm.py` (outside the repo). Stripping is not enough: `strip --strip-all` on `OPENFS98.EXE` also removed eight debug sections. The mode:
    - parses and validates the COFF symbol records and the length-prefixed string table;
    - omits exactly those byte ranges;
    - masks `PointerToSymbolTable` and `NumberOfSymbols`, plus the existing timestamp and checksum exceptions;
    - validates every section name that references the string table, and preserves trailing bytes.
  - **Symbol diff:** decoded symbols are compared separately. The only allowed difference is the expected `_end` additions, one per image object.
  - **Objects:** objects whose TU carries the macro get the same narrow exception. Their relocations are compared by resolved symbol, their section bytes exactly, and any other object or replay byte for byte, as today.
  - **The verdict:** every PE identical on that basis, with the export tables identical.
  - The notes give the method, the counts and the gate changes.
- **Rebuilds:** configure and every recipe rerun once, and that is accepted. The causes are the source edits, the manifest and the checker; also, a new header under `include/fs98` changes the `CONFIGURE_DEPENDS` glob shared by every `ofs_step`.
  - A warm build afterwards logs no step under `OFS_STEP_LOG`.
  - `tools/ofs data-images` (with `orig/`, no wine) passes on every row.
  - `check_build_wiring.py` still passes, with its expected sets unchanged: they describe isolated touches after a clean build.
- **The self-test:** the existing entry (`--orig … --manifest … --self-test`) keeps its 173 cases and gains synthetic cases:
  - each equation's pass and fail;
  - a missing `_end`;
  - a duplicate symbol;
  - a symbol in the wrong section;
  - a prefix that is too long;
  - a wrong suffix offset;
  - `UPDATE=1` hard errors, in both update paths.

  The Baselines expectation's case count (`docs/tools.md:317`) is updated to the new total. That is the one Baselines edit allowed.
- **Mutant proofs** (in scratch copies, each a full build of the module concerned; the notes record the tool's own exit status and message):
  1. **`737ADI`:** its `gauge_data` array shortened by 1 byte.
  2. **WINMSG:** `WM_IMAGE_SIZE` changed to `0x4a4`, so its assert follows.
  3. **NUMERIC:** its array changed to `0x320`.

     Each of mutants 1–3 must give exit 0 under the old tool and old manifest (Codex confirmed the old checks accept all three), and exit 1 under the new tool, with the extent diagnostic.
  4. **Missing macro:** removing the `FS98_IMAGE_END` line from one module source fails with "no end symbol".
  5. **Suffix offset:** the declared MDL suffix offset changed by 4 in the manifest fails.

     For mutants 4 and 5, the old tool cannot read the new fields. They are proved under the new tool only, and the notes say so.
- **Docs and paperwork:**
  - `docs/tools.md`: the `check_data_image` row and the manifest fields;
  - `docs/method.md` (or `docs/architecture.md`, wherever the image convention lives): one line saying every compared image object carries `FS98_IMAGE_END`;
  - the `check_data_image.py` docstring;
  - `deferred-work.md`: R2 moves to `deferred-work-resolved.md`. The notes list any `rdata=` or `switch=` ownership left open as a new one-line entry.
- **Running:** in a worktree `~/source/OpenFS98-cl4b` off HEAD, with `orig/` copied in. Builds use `nice -n 19` and `-j 2`. No wine. Slice 6b0 is running at the same time; this slice touches no break runner. Its source edits are one macro line per image, and the gate proves them.

**Never:**
- No change to any code or data byte, export, or relocation of any built binary; only symbol-table entries are added.
- No change to any existing `sha256=` or other existing manifest field value.
- No change to any break runner or Baselines line, apart from the self-test count in one expectation.
- No loosening of an existing check.
- No commits or staging.

</frozen-after-approval>

## Code Map

- **`tools/check_data_image.py`:**
  - `:86` `read_rva`;
  - `:134` `coff_symbol`;
  - `:161-188` `our_image`;
  - `:438` `compare_bound`;
  - `:569-602` `tail=`;
  - `:626` `compare`;
  - `:789` `read_manifest`;
  - `:881` `hash_check`;
  - `:954` `run_hash`;
  - `:971` `run_manifest`;
  - `:1806` the `--gauge-pairs` CLI.
- **`tools/data_images.txt`:** 29 rows. The gauge and MDL manifests come from `gen_gauges.py`, `gen_mdl_images.py` and `build_mdls.py` (the implementer finds where their rows and `len` are produced).
- **Image objects:**
  - the table in the Codex study: WINMSG `winmsg_image`, ATC `atc_image`, NUMERIC `fs98_numeric_data`, SIMSUI `fs98_simsui_block`, HOST `host_image`, GLOBAL `Linkage`, and the rest;
  - `src/gauges/gauge.h:23` (`gauge_data`);
  - `src/mdl/mdl_image.c:34`, `:81` (`mdl_image`, the `_mdl_data` alias);
  - `src/main/main_situation.inc:96` (`text_tail`).
- **Gate:** `~/openfs98-verify/gate/` (`build_side.sh`, `compare.py`, `pehash_norm.py`).

## Tasks & Acceptance

**Execution:**
- [ ] The worktree. The macro, with a one-gauge prototype that checks exports and bytes. The macro applied to every image object.
- [ ] The manifest fields, the check in every path, and the self-test.
- [ ] The identity gate with the symbol table excluded, plus the symbol-diff check.
- [ ] `data-images`, the warm build, `check_build_wiring`, the mutant proofs.
- [ ] Docs, deferred-work, `## Implementation Notes`.

**Acceptance Criteria:**
- The gate shows every PE identical apart from the symbol table, with the added symbols the only symbol diff.
- `tools/ofs data-images` passes on every row.
- Every mutant fails with the extent message under the new tool. Mutants 1 and 2 pass the old tool.
- Every existing `sha256=` is unchanged.
- Only these changed: the macro header; one line per image source; the flag-only recipe edits listed above; `tools/data_images.txt`'s new fields; `check_data_image.py`; the docs; and this spec. The gate changes live outside the repo.

## Verification

**Commands:**
- `tools/ofs data-images` -- expected: every row passes, as today.
- `python3 -B tools/check_data_image.py --self-test` (or the tool's existing self-test entry) -- expected: pass.

## Review Triage Log

Spec review (Codex gpt-6.2-sol, 2026-11-25), on the first draft:

| # | Finding | Verdict | Action |
|---|---|---|---|
| 1 | GCC 13 rejects file-scope extended asm; the helper changes `.eh_frame` for HOST and SIMSUI | blocker | A `used` helper with an in-function `.set`; HOST in its existing `DllMain`; SIMSUI's TU without async unwind tables; the gate decides. |
| 2 | A global alias is exported under auto-export; static objects, `.inc` objects and aliases | high | A local alias named from the symbol operand; one per defining TU; duplicates rejected. |
| 3 | The gauge and MDL JSON are build inputs, not checker rows; there are 30 individual rows | high | All metadata in `tools/data_images.txt`; `--gauge-pairs` validates at configure time; the batch `--hash` checks binaries. |
| 4 | The gate hashes PEs and objects strictly; stripping removes debug sections | blocker | A strict symbol-table-aware normaliser; a decoded symbol diff; a narrow object exception. |
| 5 | The equations need resolved windows, section identity, and both paths for `bound=` and `tail=` | high | Specified per path. |
| 6 | `UPDATE=1` downgrades errors; `run_manifest` can rewrite `len=` | high | A hard-error path; `len=` is not rewritten against the extent. |
| 7 | Mutant edits and diagnostics unpinned; the old tool cannot parse the new fields | medium | Pinned edits; old-tool exit 0 for 1–3; 4 and 5 new-tool only; the 173 cases kept. |
| 8 | The new header widens the shared configure glob; recipe include paths | medium | Accepted; flag-only recipe edits listed. |

Code review (Codex gpt-6.2-sol, three layers, 2026-11-25):

| # | Finding | Verdict | Action |
|---|---|---|---|
| 1 | bound= accepted a wrong len and extent=prefix (reproduced) | high | extent= rejected on bound rows; whole-object equations in both paths. |
| 2 | UPDATE=1 downgraded missing or duplicate anchor symbols (reproduced) | high | Typed structural errors, hard in every mode; regressions. |
| 3 | Symbol section tag trusted without its address; uniqueness only within sections | medium | Address bounds; whole-table uniqueness; ordered headers. |
| 4 | Duplicate manifest fields; the checker and the gate parse differently | medium | Duplicates rejected; module names unique; one shared parser. |
| 5 | Malformed COFF raised struct.error | medium | Clean structural errors. |
| 6 | Five prefix rows left slack unchecked (3, 200, 59 bytes) | medium | The object size pinned per prefix row; zero, relocation-free slack for the three padding rows; a MULTIP mutant. |
| 7 | --gauge-pairs not rerun on a checker edit | low | A configure dependency; notes corrected. |
| 8 | The gate trusted saved digests; omitted range unvalidated; loose object exception | high | Rehash at compare time; disjointness checks; the exception narrowed; resolved relocations. |
| 9 | Missing transcripts; overstated notes | low | Saved and corrected. |
| 10 | LTO or -pg could drop in-function markers | rejected | Not used; a drop is caught as "no end symbol"; header note. |

Code re-review of the fixes (Codex gpt-6.2-sol, 2026-11-25):

| # | Finding | Verdict | Action |
|---|---|---|---|
| 11 | A row without off=/len=/sha256= skipped the anchor and extent checks; a bogus obj= exited 0 under UPDATE=1 | high | The symbol and obj= checks run regardless; the missing fields are themselves a hard error. |
| 12 | A stale bound= digest hid an illegal relocation in the image | medium | The relocation check is separate from the digest; only the digest is a Mismatch. |
| 13 | An at= anchor could resolve outside its declared COFF section | medium | The value must lie in its declared section (end allowed), and the window inside it. |
| 14 | Zero or several .rdata or switch= matches counted as a hash mismatch | medium | Structural and hard; only one match with another digest is a Mismatch. |
| 15 | Malformed PE header fields raised struct.error | medium | Bounds-checked: a clean "malformed PE" error. |
| 16 | ptrs= parsing overwrote the manifest path in diagnostics | low | A separate variable; the manifest file:line is kept. |

## Implementation Notes

Worktree `~/source/OpenFS98-cl4b` (off `41a4a98`, `orig/` copied in). Nothing staged or committed. Changed: `include/fs98/image_end.h` (new); one `FS98_IMAGE_END` (or `FS98_IMAGE_END_IN`) line and one include in each of 31 files under `src/` (`main.c` has a second line for `text_tail`; `gauge.h` serves the 120 gauges); `tools/check_data_image.py`; `tools/data_images.txt` (new fields only); the flag-only recipe edits (`tools/build_mdls.py` `-Iinclude`; `cmake/steps/all.cmake` SIMSUI image TU `-fno-asynchronous-unwind-tables -Iinclude`; `tools/build_gauges.py` freshness list); `docs/tools.md`, `docs/method.md`, the two deferred-work files; this spec. The gate changes are outside the repo (`~/openfs98-verify/gate/`: `pehash_norm.py` `norm_strict`/`decode_symbols`, `pehash.py --strict`, `compare.py --strict --manifest-rows`; `.bak-pre-cl4b` copies kept).

**The macro.** `FS98_IMAGE_END(obj)` is the helper function the spec describes (`used`, `aligned(1)`, `.L` assembler name, `.set %c0_end, %c0 + %c1`, `__builtin_unreachable()`). `FS98_IMAGE_END_IN(obj)` is the bare directive. The helper adds an FDE to `.eh_frame` in any TU built with unwind tables, so those TUs use `_IN` at the start of an existing function: HOST (`DllMain`, as the spec said) and, found by the gate's first run, HG2D (`DllMain`), SIM1 (`sim1_native_entry`), NUMERIC (`fs98_numeric_init`), UTIL (`fs98_util_sqrt32`) and the two test gauges (`warning_release`, `suction_release`). These are the TUs whose recipes (module or difftest builds) do not pass `-fno-asynchronous-unwind-tables`. SIMSUI's image TU takes the flag, as specified. The end symbol is a local alias named from the compiler's symbol operand (`_main_text_tail_end` for `text_tail`), kept in the PE's COFF table, never exported.

**Deviations from the spec text (decided here, please confirm).**
1. Five rows are `extent=prefix:SIZE`, not two (the object size is pinned, and DEMO, MULTIP and WEATHERM add `:zero`): ATC and NUMERIC as specified, plus DEMO (`0x1165` of a `0x1168` object, "rounded"), MULTIP (`0xf38` of `0x1000`) and WEATHERM (`0x1c5` of `0x200`, a `spare[]`). Their objects are rounded up on purpose and cannot be made exact without changing the struct (a code risk). Review fix 6 pins each object's size (`_end - obj == SIZE`) and requires the slack of the three rounded rows to be zero bytes with no relocation, so a shrink into the slack is caught again (mutant 6). The Codex study's "148 whole-object rows" was 145.
2. WARNING and SUCTION name `warning_image` and `suction_image` (their own sources), not `gauge_data`; `--gauge-pairs` pins `obj=gauge_data` and no `extent=` on the 120 gauge rows only.
3. `obj=` is written without the COFF underscore (`obj=winmsg_image`); the tool looks up `_winmsg_image` and `_winmsg_image_end`. `obj=` and `extent=` follow `at=` in `format_row`'s order, so `UPDATE=1` leaves the manifest byte-identical (checked: every row round-trips).
4. `docs/tools.md`'s Baselines expectation is `self-test passed (227 cases)`: 173 existing plus 54 new cases (171-224; each extent case covers both `--orig` and `--hash`, the `UPDATE=1` cases the single and batch forms).

**The check** (`extent_problem`, `coff_symbol_in`, `our_tail`, `hash_check`, `compare`, `compare_bound`, `read_manifest`, `--gauge-pairs`). Messages name the file, row, symbol and numbers, e.g. `bin/WINMSG.DLL (WINMSG): obj=winmsg_image: image window RVA 0x3000..0x34a8 (len 0x4a8) is not the whole object of _winmsg_image, RVA 0x3000..0x34a4 (size 0x4a4)`. Extent failures are `Structural` in `hash_check`, so `run_hash` exits 1 under `UPDATE=1` (the stale-hash warning is unchanged); `--orig` returns before it rewrites a row whose extent failed. The legacy two-file command line and `rdata=`/`switch=` are untouched; a one-line deferred entry records that `rdata=`/`switch=` still name no object.

**Self-test** (`tools/ofs data-images`, 227 cases, passed in the gate root and the scratch tree; transcript `transcripts/di-final2.log`): each equation's pass and fail (whole, prefix, suffix, `tail=`, `bound=`), a missing `_end`, a missing object symbol, a duplicate `_end`, a duplicate object symbol, the end in another section, the end not after the start, a prefix longer than its object, a wrong suffix offset (both directions), bad manifest rows (`obj=` missing, bad `extent=`, `suffix:0x0`, bad name), `--gauge-pairs` pass and two fails, and `UPDATE=1` hard errors in `--hash` (single and two pairs, exit 1 not 3, mtime kept) and `--orig --update` (manifest unchanged), with the stale-hash warning still a warning.

**The gate** (build before `41a4a98` and after at `~/openfs98-verify/cl4b/root` with `build_side.sh`, kept in `~/openfs98-verify/cl4b/out-before` and `out-after`). `strict` mode omits exactly the symbol table and string table (parsed and validated; `.file` names and `/N` section names resolved), masks `PointerToSymbolTable`/`NumberOfSymbols` plus the existing timestamp and checksum exceptions, and keeps trailing bytes. Result: `artifacts 1130/1130, PEs 321, objects 206, replayed units 524` and `GATE PASS`: all 321 PEs identical (export tables included, they are section bytes), `retail.sha256` equal, 0 replay failures; 178 PEs gain symbols, 179 in all, every one a local `<X>_end` of an existing symbol in the same section; each of the 161 manifest rows' PEs gains exactly its `_<obj>_end` (MAIN also `_main_text_tail_end`); 184 objects (replay units and `bin/` objects) differ only by added `<X>_end` symbols, their sections, flags and relocations (by resolved symbol) equal; the flag-only edits appear exactly where expected (`-Iinclude` on 11 `mdl_image.c` units, both flags on `simsui_image.c`). The first gate run (helper everywhere) failed on HG2D, SIM1, the test gauges and four harness PEs: the `.eh_frame` growth above. Its output is kept in `out-after-run1`. After the review the strict compare was rerun on the kept before and after outputs (no rebuild; the sources the bytes come from did not change): `compare.py` now rehashes every kept PE and object itself (`pehash_norm.digest_strict`, which validates that the symbol and string tables are contiguous, consistent and disjoint from the headers and every section's raw data, and hashes the overlay and its length), reads the manifest through the checker's own `read_manifest`, applies the object exception only to units whose after-side `.d` lists `image_end.h` and only to `<X>_end` aliases of manifest objects (or the tail symbol), compares each object's body (masking only NumberOfSymbols and relocation symbol indices), line records, trailing bytes, sections and relocations resolved to (name, section, value, class). Result again `GATE PASS` (artifacts 1130/1130, PEs 321, 179 added symbols, 161 manifest rows matched, 184 objects); negative checks: the first, helper-everywhere run still fails (158 differences) and a flipped byte in a kept `WINMSG.DLL` is reported. Transcripts: `~/openfs98-verify/cl4b/transcripts/` (`cmp4.out`, `cmp-neg1.out`).

**Rebuilds.** Configure and every recipe reran once, as the spec accepts. After it: `cmake --build --target openfs98` and `--target difftests` with `OFS_STEP_LOG` log no step (checked in the gate root). `python3 -B tools/check_build_wiring.py` (the final run's result is below; run from a copy without `.git`, since the script copies only tracked files and the new header is untracked): `build wiring: 8 cases, 161 steps, ok`, expected sets unchanged (rerun on the final tree after the review fixes, same result).

**Mutant proofs** (a scratch copy of the tree at `~/openfs98-verify/cl4b/dev`, each module compiled with its recipe's own gcc line, then both tools run on the DLL; old = `HEAD`'s tool and manifest; scripts in `~/openfs98-verify/cl4b/mut/`, full transcripts of all six, with each tool's exit status, in `~/openfs98-verify/cl4b/transcripts/m1.out`..`m6.out`; mutant 1 was built with `tools/build_gauges.py` and its log line is in `m1.out`):
1. `737ADI`, `GAUGE_DATA_SIZE` 0x1078 to 0x1077: old tool `--hash` exit 0 and `--orig` row exit 0; new tool exit 1, `image window RVA 0x3000..0x4078 (len 0x1078) is not the whole object of _gauge_data, RVA 0x3000..0x4077 (size 0x1077)`.
2. WINMSG, `WM_IMAGE_SIZE` 0x4a4 (its assert follows): old exit 0 and 0; new exit 1, `... (len 0x4a8) is not the whole object of _winmsg_image, RVA 0x3000..0x34a4 (size 0x4a4)`.
3. NUMERIC, array `[0x320]`: old exit 0 and 0; new exit 1, `_fs98_numeric_data is 0x320 bytes (RVA 0x4080..0x43a0), the row declares the object size 0x400` (before the review's size pin it read `image window ... (len 0x321) is not a prefix`).
4. WINMSG with the `FS98_IMAGE_END` line removed (new tool only): exit 1, `no end symbol _winmsg_image_end in our DLL (FS98_IMAGE_END(winmsg_image) missing or dropped?)`.
6. MULTIP, `MULTIP_IMAGE_SIZE` 0xf38 and `z_0933[0x605]` (assert follows; added by the review): old tool exit 0 on `--hash` and `--orig`; new tool exit 1, `_s_multip_image is 0xf38 bytes (RVA 0xb200..0xc138), the row declares the object size 0x1000`.
5. `BOEING737-400_N`, manifest `extent=suffix:0x3004` (new tool only): exit 1, `image window RVA 0x5000..0x10ab8 (len 0xbab8) is not the suffix at +0x3004 of _mdl_image, RVA 0x2000..0x10ab8 (size 0xeab8)`.

**Not done or risky.** (a) Correction: the gauges rule already sees `image_end.h` through the shared header glob; the review's fix 7 adds `tools/check_data_image.py` to the configure dependencies that feed `--gauge-pairs` (`cmake/steps/all.cmake`), so a checker edit reruns it. `FS98_IMAGE_END_IN` must sit in an entry or exported function (noted in `image_end.h`); LTO and `-pg` are not used, and a dropped marker is caught as "no end symbol". (b) The `_IN` placements put an `asm volatile` at the top of seven functions; the gate shows their bytes identical today, but a future edit near them should rerun it. (c) No break runner or Baselines line was touched; the break runners that rebuild these sources (`*_breaks.py`) were not run, and `tools/mdl_module_breaks.py` copies `mdl_image.c` and compiles through the edited `image_object`, so its `-Iinclude` comes from there.

**Round 2 (re-review fixes 11-16).** `tools/ofs data-images` (gate root and scratch tree): self-test passed (227 cases). The six mutants and the strict compare were rerun with the same results (`GATE PASS`, 321 PEs, 179 added symbols, 161 rows matched); transcripts with the `r2` suffix in `~/openfs98-verify/cl4b/transcripts/`. Under `UPDATE=1` a changed `.rdata` window no longer warns in `--hash`: it needs `tools/ofs data-images UPDATE=1` run on its own, as the message says.
