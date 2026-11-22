---
title: 'Story 1.4, slice 12: NUMERIC and UTIL data decompiled'
type: 'refactor'
created: '2026-11-20'
status: 'done'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43' # worktree held uncommitted Stories 1.2-1.4; file snapshot at ~/openfs98-verify/baseline-1-4l
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4k-advdrv-sound-panels-controls-data.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** NUMERIC and UTIL still carry their data as allowlisted dumps:
- `src/numeric/numeric_data.inc`, rewritten each build by `gen_numeric_tables.py`;
- `src/util/util_data.inc` and `src/util/cos_table.inc`, both rewritten each build by `gen_util_tables.py`.

`check_data_image.py` can't check either module. It finds our image at our `ImportTable` export. But each module keeps retail's whole `.data` as a separate buffer, a verbatim copy that includes retail's own ImportTable, Linkage and their raw relocated pointer values. The buffers are `fs98_numeric_data[0x400]` (writable, in `.data`) and `fs98_util_data[2048]` (const, in `.rdata`). The module's real ImportTable and Linkage are other objects.

**Approach:**
- **Locating the image** (decision): don't restructure the modules. A new manifest field `at=SYMBOL` locates our image by the DLL's COFF symbol table (`_fs98_numeric_data`, `_fs98_util_data`) instead of by the export. Each row is `--kept`: the copied pointer slots must hold retail's raw values, as ATC's do. With `at=`, the image may lie in `.data` or `.rdata`. `--hash` uses the same lookup, still with no `orig/`.
- **Buffers** (decision): each stays the same byte array, with the same name, type, constness, size and section, so the symbol and every `+ off` access are unchanged.
  - Its initialiser is written as typed values through byte-emitting macros at retail's offsets: `U16(x)`, `U32(x)`, `F64(hexfloat)`, strings and range designators.
  - A struct isn't possible here: it breaks `fs98_util_data + off` and changes the symbol. The macros carry one typed value each, a field, not a byte dump.
  - Retail's copied pointers are `U32` numbers, with a comment naming what each pointed to. UTIL's slot 0x44 can read them (up to `.data+0x7fc`), so they're live data.
- **UTIL's trig tables** (decision): exact formulas exist, but C99 can't evaluate `cos`/`atan` in a static initialiser, so the tables stay as numbers.
  - They go in one typed file, `src/util/util_trig.inc`, as X-macro lists:
    - the Q32 cosine at +0xf4: 129 entries, exactly `min(0xffffffff, trunc(2^32 cos(pi i/256)))`;
    - the atan table at +0x3ac: 257 u16, `round(atan(i/256) 0x8000/pi)`, except i=227, which is 7567;
    - the arcsine table at +0x2f8: 61 u16 (i = 0..60), `round(asin(i/64) 0x8000/pi)`, except i=54, which is 10474;
    - the second arcsine table at +0x372..+0x3ac: 29 u16, ending 16384, 16384. The first 12 entries are `asin((241+i)/256)` and the next 17 are `asin((1009+i)/1024)`, same scaling. Its index 22 is an exception: 15352.
  - Each formula and its exceptions are stated in a comment.
  - Check (a) doesn't flag X-macro or `U16(...)` lines (verified with its own rules), so the file needs no allowlist entry.
  - `fs98_util_cos_q16[130]` is built from the same cosine list (the high halves), plus its entry 129, 0x00a3, which is arcsine entry 1, at +0x2fa. A mistyped value therefore changes the hashed image too.
  - The latitude-circle table at 0x5b4: 78 entries, ending at 0x6ec, the last −505888. It has no exact formula, so it's typed numbers.
  - Two regions are still undescribed: 0x5ae..0x5b4 (after the atan table) and 0x6ec..0x800 (276 bytes, non-zero up to 0x7e8). Slot 0x44 can read the second. Type both by what they hold, marking unknown fields "meaning not yet known".
- **NUMERIC's `fs98_numeric_rdata`:** it is already `static const uint64_t[7]` bit patterns, read through `fs98_numeric_rdata_double()` (`numeric.c:64`), with only indices 0 and 3 used. Keep that array as it is, adding hex-float comments; no code change. gcc folds it, so no window exists. It goes under slice 11's deferred constant-pool entry.
- **Removed:** `gen_numeric_tables.py`, `gen_util_tables.py`, the three dumps and the two build lines.
- **Added:** two manifest rows (`UPDATE=1`) and two `--hash` lines.

**Gate:**
- `NUMERIC.DLL` and `UTIL.DLL` normalized-identical to the pre-slice build. Normalize gcc's `_CSWTCH.N` numbering in the COFF string table too, as slice 11 found necessary.
- These other builds of the same sources normalized-identical:
  - `numericdiff.exe`, `utildiff.exe`, `utildiff_portable.exe` and `slewdiff.exe` (the difftests step);
  - `OFSTEST.DLL` and `UTIL_TRACE.DLL`. Their targets (`ofstest-run`, `util-trace`) start the sim, so compile their gcc lines by hand to scratch paths.
- `data-images` passes with 21 modules.

If a DLL's code changes, report the per-function diff and stop for a human decision.

**Queued until Story 1.3's Baselines run finishes:** `utildiff-run`, `utildiff-portable`, `numericdiff-run` and `ofstest-run`.

## Boundaries & Constraints

**Always:**
- Use slices 9 and 11's conventions.
  - Packed structs with `_Static_assert`s on size and key offsets.
  - Hex floats for floating-point values.
  - Strings as string literals.
  - Check (a) must flag none of the new C. No brace-wrapping to dodge it. If a table has no exact formula and trips (a), it goes in its own typed file with one allowlist entry giving the reason.
- Every symbol and access stays unchanged: `fs98_numeric_data + off`, `FS98_NUMERIC_U32(...)`, `fs98_util_data + FS98_UTIL_LATTABLE + off`, `fs98_util_cos_q16[i]`, `FS98_UTIL_DATA_SIZE`, and the rest. Keep `(int16_t)` reads of the cosine table working.
- Read each generator's docstring and asserts before deleting it, and carry its field knowledge into comments.
- Never `rm -rf bin`. No staging or commits. Record the files in `~/openfs98-verify/story-files.md`.

**Never:**
- No change to module or harness code paths, ImportTable or Linkage objects, or exports.
- No other module.
- No heavy runs (the queued list).

</frozen-after-approval>

## Code Map

- **The buffers and their includes:**
  - `src/numeric/numeric.c:25` includes `numeric_data.inc`, which defines `static uint8_t fs98_numeric_data[0x400]`: 0x200 initialised bytes, then zeros.
    - Content, per the generator: powers of ten, the degree/minute/second division tables, sprintf formats, and FLTSIM98.CFG settings.
    - Accesses are at `numeric.c:27-31`, `:318` (formats at 0x94 − 12n), `:434` (0x220) and `:778` (0x1cc).
  - `src/util/util_math.c:22-23` includes `cos_table.inc` (`static const uint16_t fs98_util_cos_q16[130]`, read at `:111-112`) and `util_data.inc` (`static const unsigned char fs98_util_data[2048]`, `FS98_UTIL_DATA_SIZE`, `FS98_UTIL_LATTABLE 0x5b4`). UTIL's tables:
    - atan at 0x3ac and quadrant at 0x5b0 (`util_math.c:729-807`);
    - latitude-circle lengths at 0x5b4 (40075000 m at the equator), which slot 0x44 may index past.
- **Retail layout:**
  - NUMERIC: base 0x20870000; `.data` at 0x6000, vsize 0x321, 0x200 on disk; 14 relocations in `.data`, from +0x24 to +0x12c. Our buffer is 0x400: the image check covers 0x321, and the rest is zero.
  - UTIL: base 0x208e0000; `.data` at 0x5000, 0x800; 83 relocations.
  - Our buffers start at RVA 0x4080 (`.data`) and 0x5060 (`.rdata`) (`nm`). Each equals retail's whole `.data` byte for byte, raw pointer values included. No kept slot is zero in retail, and none of our relocations falls inside either buffer.
- **`tools/check_data_image.py`:**
  - `read_manifest`/`format_row` gain `at=SYMBOL`.
  - Add a small COFF symbol lookup. The symbol table pointer and count are at `pe+12`/`pe+16`, in 18-byte records; skip each record's aux records (count at byte 17). Names are inline, or `\0\0\0\0` plus an offset into the string table, which these longer names use. RVA = section RVA + value.
  - `our_image` takes the start from that lookup when `at=` is set. The containment check then accepts `.data` or `.rdata`.
  - `compare` and `hash_check` pass `at=` through.
  - Self-test cases: a missing symbol fails cleanly; a kept slot flipped in UTIL's `.rdata` copy fails, both retail-side and hash-side. `self_test.mutated()` (`:520`) computes the start from `export()`, so give it the symbol lookup too.
- **Builds:**
  - `cmake/steps/all.cmake:188` (NUMERIC) and `:237` (UTIL): remove the generator lines (re-read the numbers first) and add `--hash` lines after the links.
  - The harness builds are in `cmake/steps/difftests.cmake:179-221`, `cmake/targets.cmake:675` (ofstest-run, two `-o bin/OFSTEST.DLL` lines) and `:719` (util-trace).
- **`tools/raw_dumps_allowed.txt`:** remove `gen:gen_numeric_tables.py`, `gen:gen_util_tables.py` and the three dump entries. Expect `9 allowed dump files, 42 scripts reading orig/ (16 gen, 10 resource, 16 oracle)`.
- **Docs:**
  - `docs/modules/numeric.md` and `util.md`, where they name the generators;
  - `docs/tools.md` and `docs/building.md`, the module count and the `data-images` Baselines line;
  - `docs/building.md` also gets a sentence on `at=`;
  - `deferred-work.md`:
    - mark the "NUMERIC and UTIL need code changes" item resolved, without code changes;
    - add the queued runs;
    - add `fs98_numeric_rdata` to slice 11's constant-pool entry.

## Tasks & Acceptance

**Execution:**
- [x] Record the normalized hashes of `NUMERIC.DLL`, `UTIL.DLL` and the six harness builds from the pre-slice tree. Use `/tmp/claude-1000/-home-steve-source/b2e3078b-1d77-426e-816f-2de2772a67dc/scratchpad/peh2.py`, which also zeroes the `_CSWTCH.N` digits.
- [x] `tools/check_data_image.py`: add `at=`, the symbol lookup, and the self-test cases.
- [x] For each module in turn:
  - add the typed definitions;
  - delete the generator, its dumps and its build line;
  - add the `--hash` line and the manifest row (`MODULE retail ours --kept at=SYMBOL`), without hashes, and build with `UPDATE=1` until the hashes are filled in;
  - check identity.
- [x] Run `tools/ofs data-images UPDATE=1`, then a clean `tools/ofs all difftests` without `UPDATE`.
- [x] Update the allowlist, the docs and `deferred-work.md`.

**Acceptance Criteria:**
- Given `tools/ofs all difftests` and the two hand-compiled builds, when the eight outputs are normalized and hashed, then each equals its recorded hash.
- Given `tools/ofs data-images`, when it runs, then it reports 21 images identical, with 14 and 83 kept slots, and the self-test passes.
- Given the tree, when `check_raw_dumps.py` runs, then it exits 0 with 16 gen.
- Given one value changed in a temp copy, in each of NUMERIC's image, UTIL's image, the cosine list and the latitude table, when the module rebuilds, then its `--hash` step fails. `fs98_numeric_rdata` is exempt (deferred).

## Implementation Notes

- Implemented by a Sonnet subagent; checked in the main session.
- Normalized (`peh2.py`, first 16 hex digits), identical before and after:
  - NUMERIC.DLL 3aa12dac170da4f9 and UTIL.DLL 4aa50544723863e4, both also equal to s14;
  - numericdiff.exe df608efbe3dfa6b0, slewdiff.exe 93e19f4c8ed8fce4, utildiff.exe bc750dcf8987e3ad and utildiff_portable.exe 7983e2060c52a6d4;
  - OFSTEST.DLL 1d67c672c42f0696 and UTIL_TRACE.DLL a7bb1b25a7c39c09, compiled by hand to scratch paths.
- `tools/ofs data-images` prints `21 .data images identical (227631 bytes, 1706 relocated), 7 .rdata ranges identical (2221 bytes), 1 switch tables, 5 pointer lists (971 entries), self-test passed (46 cases)`; NUMERIC has 14 kept slots and UTIL 83.
- `check_raw_dumps.py` prints `9 allowed dump files, 42 scripts reading orig/ (16 gen, 10 resource, 16 oracle), none new`.
- Negative AC: one value changed in each of NUMERIC's image, UTIL's image, the cosine list and the latitude table failed `--hash` (temp copy).
- Deviations:
  - NUMERIC's one double at +0x1a8 is `U64(0x4059000000000000ull)` with a hex-float comment; C99 can't byte-emit a double in a constant expression.
  - There was no verbatim "NUMERIC and UTIL need code changes" entry in `deferred-work.md`, so a RESOLVED entry was added.
- Still "meaning not yet known":
  - UTIL +0x5ae, +0x6ec..+0x700 and +0x780..+0x7e8;
  - NUMERIC +0x158..+0x198.
- Not run: the queued difftests.

## Spec Change Log

## Review Triage Log

Pass 1 (Blind Hunter, Edge Case Hunter, Verification Gap; Sonnet). BH = Blind Hunter, ECH = Edge Case Hunter, VG = Verification Gap.

| # | Finding | Verdict | Evidence | Route |
|---|---|---|---|---|
| 1 | VG/ECH: `fs98_util_cos_q16`'s entry 129 (`0x00a3`) and the `>> 16` aren't covered by the image hash | medium | the array is a separate `.rdata` object; the 129 cosine values come from the hashed list, but entry 129 is a literal | patch: derive it from the arcsine list's named entry. The `>> 16` is code; `utildiff-run` (queued) covers it |
| 2 | ECH: a duplicate symbol name makes the first match win | low | trivial to fail on more than one match | patch |
| 3 | ECH: `at=` with a non-zero `off=` is silently accepted | low | trivial row check | patch |
| 4 | ECH: a `None` from `coff_symbol` in the UPDATE and `mutated()` paths gives a TypeError | low | possible on a stripped DLL | patch |
| 5 | BH: `at=` depends on the COFF symbol table, and nothing says so | low | true | patch: one comment |
| 6 | ECH: no static assert ties `FS98_UTIL_DATA_SIZE`/`LATTABLE` to the array | low | direct | patch |
| 7 | BH: the trig formula comments lack `*` | low | true | patch |
| 8 | BH: the NUMERIC comment says 0x200; the guard covers 0x321 | low | the initialiser ends at +0x1f0, and retail's vsize is 0x321 | patch: comment |
| 9 | BH: the tool docstring and building.md word the `at=` buffers differently | low | true | patch |
| 10 | BH: stale "34 remaining gen:" in deferred-work; RESOLVED entry says "without code changes" | low | true | patch |
| 11 | ECH: a missing NUL in the string table gives a ValueError | low | loud failure on a malformed DLL only | reject |
| 12 | ECH: `STRn` truncates silently | low | any change to the bytes fails the image hash | reject |
| 13 | ECH: a short X-macro list leaves a silent zero gap | low | the image hash catches it (the seeded tests showed one value fails) | reject |
| 14 | ECH: the image tail beyond the raw size is read as zeros | false | same semantics as every other row; retail's tail is zero too | reject |
| 15 | ECH: the spec claims `F64(hexfloat)` | false | the deviation is recorded in Implementation Notes (U64 with a hex-float comment) | reject |
| 16 | BH: `coff_symbol` relies on dict order for sections | false | `load()` builds the dict in header order (Python 3.7+ insertion order); section names are unique in these DLLs | reject |
| 17 | VG/BH: NUMERIC's +0x321..+0x400 tail is unhashed | low | beyond retail's vsize, zero-initialised; only a deliberate edit could change it | reject |
| 18 | BH: self-test ordering; no stripped-DLL or round-trip cases | low | cosmetic; the missing-symbol case covers the failure path | reject |
| 19 | BH/VG: difftests not run | medium | queued by the frozen intent until Story 1.3 finishes | reject (scheduled) |
| 20 | BH: helper macros duplicated across modules; counts hard-coded in docs | low | no behaviour impact | reject |

## Verification

**Commands:**
- `tools/ofs all difftests`, the OFSTEST and UTIL_TRACE gcc lines by hand (`cmake/targets.cmake:675` and `:719`), then `peh2.py` over the eight outputs -- expected: the recorded hashes.
- `tools/ofs data-images` -- expected: PASS, with 21 images.
- `python3 tools/check_raw_dumps.py` -- expected: exit 0, with 16 gen.
