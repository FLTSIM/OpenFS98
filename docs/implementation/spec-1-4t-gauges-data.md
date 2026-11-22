---
title: 'Story 1.4, slice 18b: the 120 gauges data decompiled'
type: 'refactor'
created: '2026-11-22'
status: 'done'
baseline_commit: 'c1f577c4048f2492cbb9ab099284072aedc8c835' # plus uncommitted 1.3 close-out, 17b and 18a; file snapshot at ~/openfs98-verify/baseline-1-4t
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4s-test-gauges-data.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4r-simsui-data.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** `tools/gen_gauges.py` runs in the `bin/gauges-all/built.stamp` step (`cmake/steps/all.cmake:402`). For each of the 120 CD gauges it reads `orig/Gauges/<NAME>.GAU` and writes, under `bin/gauges-all/<NAME>/`:
- **`image.inc`:**
  - `.data` as a hex `uint8_t gauge_data[]`, with the relocated slots zeroed;
  - `.rdata` as a hex `static const uint8_t gauge_rdata[]`;
  - `gauge_rebind()`;
  - the `ImportTable`/`Linkage` `__asm__` aliases;
  - a `DllMain` with one `{ uint32_t bound = gauge_rebind(0x<target>); memcpy(gauge_data + 0x<off>, &bound, 4); }` per relocated `.data` slot, in relocation order.
- **`gauge.def`** and **`gauge.res`**, the bitmaps.
- **`manifest.json`:** harness metadata from disassembling retail's code (routines' arguments and x87 returns, float, int, print and indirect slots, relocs, bitmaps, the installed name, the sha256).

The aggregate `bin/gauges-all/manifest.json` is read by:
- `tools/build_gauges.py:54`, which builds each gauge from `src/gauges/<name>.c` plus `src/gauges/gauge.h` with `-I<that directory>` and writes `all_gauges.inc`/`all_panels.inc`;
- `tools/check_gauges.py:39`;
- `tools/stage_gauges.py:18`.

`tools/raw_dumps_allowed.txt` still lists `gen_gauges.py` as `gen:`. No gauge has a manifest row.

**Approach:**
- **Typed images, committed.** For each gauge, `src/gauges/images/<NAME>/` gets three files:
  - **`data.inc`:** `.data` in `src/host/host_image.inc`'s style:
    - `AT(off)` at the `.data` offset, `U8`/`U16`/`U32`, `F64` at the manifest's float slots, and `STRn`. `F64(bits)` emits the double's 8 little-endian bytes from its bit pattern, with the value in a hex-float comment;
    - relocated slots zero, and zero runs of 16 bytes or more left out;
    - named: the gauge header (`Linkage` +0x18), the elements of the tree, ImportTable and Linkage. Each element's common header is type at +0, parent +0x24 and children +0x28; text elements extend to their buffer pointer at +0xb8 (`tools/difftest/gauges_panels.inc:44`). Record boundaries come from the tree walk, not from 0x2c steps. The rest is marked "meaning not yet known".
  - **`rdata.inc`:** `.rdata` in the same style.
  - **`rebind.inc`:** one `GAUGE_REBIND(off, rva)` per relocated `.data` slot, in relocation order. The slot holds base + `rva`, an RVA in `.data`, `.rdata` or `.text`.

  A scratch script outside the repo writes the files. Their compiled bytes must equal the generated arrays.
  - `data.inc` and `rdata.inc` `#undef` their helper macros.
  - `rebind.inc` holds only entries and comments: `gauge.h` defines and undefines `GAUGE_REBIND` around its include, because `ptrs=`'s parser accepts nothing else.
- **The common code moves into `gauge.h`.**
  - `gauge.h` includes the three files from `-Isrc/gauges/images/<NAME>`.
  - Retail's per-gauge values come from a committed `src/gauges/images/<NAME>/layout.h`, which holds `GAUGE_DATA_RVA`, `GAUGE_DATA_SIZE`, `GAUGE_RDATA_RVA`, `GAUGE_RDATA_SIZE`, `GAUGE_BASE`, `GAUGE_IMPORT_OFFSET` and `GAUGE_LINKAGE_OFFSET`.
  - From these, `gauge.h` defines the arrays, `gauge_rebind()`, the aliases and `DllMain`. `GAUGE_REBIND` expands to exactly today's statement.
  - Each gauge still compiles as one translation unit from its `<name>.c`, with the same flags. Everything `image.inc` declares today keeps its order, storage and type:
    - the prototypes and the external mutable `uint8_t gauge_data[]`;
    - `static const uint8_t gauge_rdata[]`;
    - the aliases and the attach statements' order.

    The initialisers' bytes must equal today's. The binary gate decides the rest.
- **The metadata, committed.** `src/gauges/gauges_manifest.json` holds today's aggregate, one gauge per line.
  - The gauges' order, every list's order and each `code_pointers` object's key order are kept, with no `sort_keys`: `all_panels.inc` iterates `code_pointers` in insertion order (`build_gauges.py:74`).
  - `build_gauges.py`, `check_gauges.py` and `stage_gauges.py` read it in place of `bin/gauges-all/manifest.json`.
- **`gen_gauges.py` splits in two.**
  - **Default (the build step):** it writes only `gauge.def`, `gauge.res` and the per-gauge `manifest.json` copies, the copies from the committed aggregate.
    - Without disassembling, it keeps the layout asserts:
      - the imports allowed;
      - the single PANELS import words;
      - the element tree's validity;
      - relocation records of type 3 only;
      - the installed name from `docs/setup_map.tsv`.
    - It reads the resources, exports, sections, `.data` and relocations. It needs all 120 retail files.
  - **`--check` (new target `tools/ofs gauges-tables-check`):** it runs today's full analysis in memory, with `objdump`, and writes nothing. It exits 1, naming the gauge, if any of these differs from retail:
    - a committed `layout.h` or `rebind.inc`;
    - the manifest's serialized line;
    - the host-compiled bytes of any `data.inc` or `rdata.inc`. This covers the 25 gauges whose `gauge_rdata` the compiler drops.
  - In the allowlist it moves from `gen:` to `resource:`.
- **The manifest check.**
  - One row per gauge in `tools/data_images.txt`, written with `UPDATE=1`: `<NAME> Gauges/<NAME>.GAU bin/gauges-all/Gauges/<installed> ptrs=src/gauges/images/<NAME>/rebind.inc`.
    - `rdata=<rva>:<len>` is added only for the 95 gauges whose `.rdata` the compiler keeps.
    - 25 drop it as unused: `737FLAPS`, `737NWS`, `737START`, `737STROB`, `737WARNG`, `737XFEED`, `BELLCTRL`, `BKADF`, `BKRNC1`, `BKRNC2`, `BKROPEN`, `BKRXPNDR`, `CESSWTCH`, `EXCON`, `EXCSURF`, `EXDG`, `EXIGNIT`, `EXSWITCH`, `LEARAOA`, `LEARCNSF`, `LEARWARN`, `LRELTRIM`, `LRSPOIL`, `SCHCOM` and `SOPMAGS`. Their `rdata.inc` is checked by `--check`.
  - `check_data_image.py` accepts `GAUGE_REBIND` the way slice 18a accepts `GAUGE_*`:
    - slots count from `.data`'s start;
    - the target is an RVA (target base zero, unlike `GAUGE_DATA`);
    - each slot and target must equal retail's relocation, in retail's order;
    - the target must lie in retail's `.text`, `.data` or `.rdata`.
  - `--hash` takes several `MODULE DLL` pairs: one pair behaves as today; an odd count is an error; it reports every failing pair. The step runs it once for all 120 after `build_gauges.py`.
- **Incremental builds.** `ofs_step` gains `DEPENDS_GLOB` (recursive, `CONFIGURE_DEPENDS`). The gauges step lists:
  - `src/gauges/*.c` and `*.h`;
  - `src/gauges/images/**`;
  - `src/gauges/gauges_manifest.json`;
  - `tools/data_images.txt`, so a hash rewritten by `UPDATE=1` reruns the step and `build_gauges.py`'s own mtime check rebuilds the zeroed DLL.

**Gate:**
- All 120 `bin/gauges-all/Gauges/*.GAU` must be identical to their pre-slice builds, unstripped and stripped, under `peh2.py`. The copies and their two hashes each are in the scratchpad's `pre18b/` and `pre18b/hashes.txt`.
- `allgaugesdiff.exe` (beec0a2bbb934702) and `allpaneldiff.exe` (7af5524795850a31), under the scratchpad's `peh2.py`, must be identical, and `all_gauges.inc` and `all_panels.inc` byte-equal.
- `data-images` must pass with 150 rows.
- If any gauge's code or layout changes, report it and stop.

**Queued with the held-back runs:** the Baselines' gauge-collection lines (`gauges-check`, `gauges-breaks`, `gauges-panels`, `all-gauges-*`, `gauges-game`).

## Boundaries & Constraints

**Always:**
- The 120 `src/gauges/<name>.c` stay as they are, and so do the text `tools/gauge_breaks.py` patches and the gauges' image bases (`build_gauges.py`'s index order).
- `gen_gauges.py --check` keeps every assert the generator has.
- Check (a) must flag none of the new files.
- Never `rm -rf bin`. No staging or commits. Record the files in `~/openfs98-verify/story-files.md`.

**Never:**
- Don't touch the test gauges (slice 18a) or the MDLs (18c).
- Don't change behaviour.
- Don't run wine or any queued run.

## I/O & Edge-Case Matrix

| Scenario | Input | Expected |
|---|---|---|
| Rows pass | the 120 rows | `data-images` passes 150 rows |
| Image byte | an in-range non-pointer byte of one gauge's `data.inc` or of a retained `rdata.inc` changed | `data-images` and the step's `--hash` exit 1, naming the gauge; for one of the 25 dropped `rdata.inc`, `gauges-tables-check` does |
| Rebind | one `GAUGE_REBIND` target or slot changed, a line dropped, duplicated or reordered, or a target outside the three sections | `data-images` fails, naming the slot |
| Metadata | one value in `gauges_manifest.json` or a `layout.h` changed | `gauges-tables-check` exits 1, naming the gauge |
| Clean | in a scratch copy, `bin/gauges-all/` removed | the step rebuilds all 120 identical gauges and the two `.inc` files from the committed files |
| Incremental | one `data.inc` touched; `UPDATE=1` after a hash mismatch | the next build reruns the step and rebuilds that gauge |
| Batch hash | `--hash` with one pair, with a later failing pair, with an odd count | as today; exit 1 naming that pair; usage error |

</frozen-after-approval>

## Code Map

- `tools/gen_gauges.py:56-99` (`Gauge.__init__`: disassembly, function discovery), `:203-345` (`emit()`: the image, `.def`, `.res`, manifest), and `:349-362` (`main`). `relocations()` at `:34` is imported by `tools/build_mdls.py`; keep it.
- `tools/build_gauges.py:13-60` -- the per-gauge build (`-I{directory}`, the up-to-date check's dependencies: add the committed files) and the harness includes.
- `src/gauges/gauge.h:1-30` -- its include of `image.inc` and what the hand files expect from it (`gauge_routine`, `gauge_data`).
- `tools/check_gauges.py:39`, `tools/stage_gauges.py:18` -- the manifest readers.
- `tools/check_data_image.py` -- the `GAUGE_*` support added in slice 18a (`PTR_RES`, the off≠0 path, the range checks, the target-base dispatch) and `--hash`'s argument parsing.
- `cmake/OpenFS98.cmake:21-62` (`ofs_step`: add `DEPENDS_GLOB`); `cmake/steps/all.cmake:400-406`, `cmake/targets.cmake` (add `gauges-tables-check`), `tools/data_images.txt`, `tools/raw_dumps_allowed.txt`.
- `docs/modules/gauges.md`, `docs/tools.md` (the target, `data-images`' and `check_raw_dumps`' Baselines lines), `docs/implementation/deferred-work.md` (close 18b; queue the runs).

## Tasks & Acceptance

**Execution:**
- [x] `src/gauges/images/<NAME>/{layout.h,data.inc,rdata.inc,rebind.inc}` ×120 and `src/gauges/gauges_manifest.json` (new, committed) -- from a scratch script.
- [x] `src/gauges/gauge.h`, `tools/build_gauges.py`, `tools/check_gauges.py`, `tools/stage_gauges.py` -- the common code, the include path and dependencies, the manifest path.
- [x] `tools/gen_gauges.py`, `cmake/OpenFS98.cmake`, `cmake/steps/all.cmake`, `cmake/targets.cmake` -- the two modes, `DEPENDS_GLOB`, the step's `--hash`, `gauges-tables-check`.
- [x] `tools/check_data_image.py`, `tools/data_images.txt` -- `GAUGE_REBIND`, multi-pair `--hash`, the rows, self-test cases for the matrix's image, rebind and batch rows (a synthetic `.rdata` target included, since no real one exists).
- [x] `tools/raw_dumps_allowed.txt`, the Code Map's docs, `~/openfs98-verify/story-files.md`.

**Acceptance Criteria:**
- Given the slice is built, when all 120 gauges are normalized with `peh2.py`, stripped and unstripped, then each equals `pre18b/hashes.txt`. The two harness executables and `.inc` files equal theirs.
- Given the slice, when `tools/ofs data-images` runs, then 150 rows and the self-test pass.
- Given the slice, when `check_raw_dumps.py` runs, then it reports 3 `gen`, 15 `resource` and 19 `oracle` scripts, and none of the new files is flagged.
- Given `orig/`, when `tools/ofs gauges-tables-check` runs, then it passes, and with one manifest value changed it exits 1.

## Implementation Notes

- Scratch generator (outside the repo): `gen18b.py` in the session scratchpad, over `tools/gen_gauges.py`'s `Gauge.analyze()`, `layout_text()` and `rebind_text()`.
- Rows: 95 gauges carry `rdata=`, the 25 listed carry none (confirmed by running every row with it: exactly those 25 fail "found 0 times").
- Gate: all 120 `.GAU` equal `pre18b/hashes.txt` (both columns); `allgaugesdiff.exe` beec0a2bbb934702 and `allpaneldiff.exe` 7af5524795850a31 unchanged; `all_gauges.inc`, `all_panels.inc` byte-equal; clean rebuild in a scratch copy identical.
- Self-test 159 cases (cases 140-156 and the synthetic `.rdata` target, the batch `--hash` cases).

- **Gate re-run (orchestrator, 2026-11-22):**
  - All 120 gauges equal `pre18b/hashes.txt`, stripped and unstripped.
  - `allgaugesdiff.exe` beec0a2bbb934702 and `allpaneldiff.exe` 7af5524795850a31; both `.inc` byte-equal.
  - `data-images`: 150 rows, 130 pointer lists (5931 entries), 105 `.rdata` ranges, self-test 159.
  - Dumps: 3 gen, 15 resource, 19 oracle. `gauges-tables-check` passes.

## Spec Change Log

## Review Triage Log

Code review, pass 1 (Codex, high; Blind Hunter 10, Edge Case Hunter 5, Verification Gap 2 others).

| # | Layer | Finding | Verdict | Evidence | Route |
|---|---|---|---|---|---|
| 1 | blind, edge, gap | `--check` pairs gauges with manifest lines by `zip`, so a reformatted manifest checks 0 gauges and passes | medium | reproduced in memory by the gap layer: "0 gauges … match" | patch |
| 2 | blind, edge | `--check NO_SUCH_GAUGE` checks nothing and exits 0 | low | `gen_gauges.py:437`; a direct guard | patch |
| 3 | blind, edge | Batch `--hash` aborts on the first exception (missing DLL), so later pairs go unreported | medium | `check_data_image.py:1716`; the spec promises every failing pair | patch |
| 4 | blind, edge, gap | Under `UPDATE=1` a mismatch zeroes the DLL's mtime but the step still touches `built.stamp`, so the next build skips it | medium | `all.cmake:418-419`; `UPDATE=1` does not rewrite `data_images.txt` in `--hash` | patch |
| 5 | blind, edge | The step's hash pairs come from the rows, so a deleted or tab-separated row drops a gauge silently | medium | `all.cmake:404-408` | patch |
| 6 | blind | The default generator does not check that the manifest names exactly the retail collection, in order | low | a direct guard | patch |
| 7 | blind | The harness executables do not depend on `all_gauges.inc`/`all_panels.inc` | low | pre-existing; already in `deferred-work.md` (18b spec review) | reject (deferred) |
| 8 | blind | `--check` host-compiles with `-w`, so an excess initialiser passes | medium | `gen_gauges.py:400`; reproduced | patch |
| 9 | blind | The step's globs omit `orig/Gauges/*.GAU` and `docs/setup_map.tsv` | low | both are read by the default mode; a direct addition | patch |
| 10 | blind | `Courier New` and other strings are written as integer fragments | low | `737ADI/data.inc:59`; the spec says strings as literals | patch |

## Verification

**Commands:**
- `cmake --build build/win32-retail` and `--target difftests` -- expected: success, the gauges `--hash` passing.
- `peh2.py` on the 120 gauges (stripped and not) and the two executables; `cmp` on the two `.inc` -- expected: equal to `pre18b/`.
- `tools/ofs data-images`, `tools/ofs gauges-tables-check` -- expected: pass.
- `python3 -B tools/check_raw_dumps.py` -- expected: 3 gen, 15 resource, 19 oracle.
