---
title: 'Cleanup 1: build-dependency gaps, three retail-bug ids, a committed PE normalizer'
type: 'chore'
created: '2026-11-25'
status: 'in-progress'
route: 'dispatch'
baseline_commit: '0f8e49e12f345913116964b6aead145e6b8c520d'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Eight open `docs/implementation/deferred-work.md` entries are small and need no wine.
- **(A) Build dependencies.** Editing an input does not rebuild what uses it:
  - `tools/data_images.txt` is not an input of the single-module `check_data_image.py --hash` steps (three entries, at about lines 87, 159 and 203);
  - `src/advdrv/advdrv_relocs.inc` does not rebuild `advdrvdiff.exe` (about line 105);
  - the generated `bin/gauges-all/all_gauges.inc` and `all_panels.inc` do not rebuild `allgaugesdiff.exe` or `allpaneldiff.exe` (about line 218);
  - `src/fe/fe_library.c`, `#include`d at `fe.c:4839`, does not rebuild `FE.DLL` (about line 225).
- **(B) Retail-bug register.**
  - WEATHERM's three effect-list quirks have no register id or marker (`docs/modules/weatherm.md:141-146`, about line 237): word types compared as dwords, freeing a non-member, and free-by-type losing its predecessor.
  - DEMO's `disk_check` entry (about line 229) rests on a false premise. Host `+0x150` (`src/host/host.c:2772`; retail `0x40a921`/`0x40a928`) zeroes both output words before `GetDiskFreeSpaceA`, so retail never reads uninitialised outputs, and `demo.c`'s comment saying otherwise is wrong.
- **(C) PE normaliser.** The identity gate's normaliser lives only outside the repo, so no reviewer can reproduce the "normalised-identical" claims (about line 191). There were two algorithms:
  - Story 1.4 slices 10–17 used `peh2.py` (now saved as `~/openfs98-verify/gate/peh2-legacy.py`);
  - Story 1.7 and the Story 1.8 gate use `~/openfs98-verify/gate/pehash_norm.py`.
  - The two give different hashes; on `bin/SIM1.sim` they are `21306e24…` and `c12f2271…`.

**Approach:** Fix each in place, prove it without wine, and mark the eight entries RESOLVED. The DEMO entry is resolved as "not a bug", with its comment corrected.

## Boundaries & Constraints

**Always:**
- **(A) Changes:**
  - `ofs_step` (`cmake/OpenFS98.cmake:26-83`) adds `tools/data_images.txt` to the dependencies of every step with a `tools/check_data_image.py` command. That covers the 28 single-pair `--hash` steps, SIM1 and HOST included. The gauges and MDL steps already list the file.
  - The gauges step (`cmake/steps/all.cmake:412`, `tools/build_gauges.py:74,89`) declares `bin/gauges-all/all_gauges.inc` and `all_panels.inc` as outputs. `ofs_step`'s token scan ignores `bin/`, so `allgaugesdiff.exe` and `allpaneldiff.exe` each get an unconditional file dependency on their header (a new `ofs_step` argument, or an explicit `DEPENDS` on the custom command), not a configure-time glob.
    - `build_gauges.py` rewrites both headers on every run. So every run of the gauges step rebuilds both harnesses on the next `difftests` build. That is accepted, and `build_gauges.py` is not changed.
  - `advdrvdiff.exe`'s step depends on `src/advdrv/advdrv_relocs.inc`.
  - The FE DLL step gets `DEPENDS_GLOB src/fe/fe_library.c`. `FE_sl_ours.dll` is rebuilt by every `fesldiff` run already, so it needs no change.
  - The comment at `cmake/OpenFS98.cmake:4-5` that names the FE gap is updated.
- **(A) Proof:** in a configured, fully built worktree, run `cmake --build <dir>`, then `cmake --build <dir> --target difftests`:
  - **touching `tools/data_images.txt`** reconfigures. It reruns exactly the 28 single-pair `--hash` recipes, the gauges step (with its `--gauge-pairs` check) and the MDL step, and then `allgaugesdiff.exe` and `allpaneldiff.exe`;
  - **touching `advdrv_relocs.inc`** rebuilds exactly `ADVDRV.DLL` and `advdrvdiff.exe`;
  - **touching `fe_library.c`** rebuilds exactly `FE.DLL`;
  - **a gauges-producer input edit** (touching one `src/gauges/*.c`) reruns the gauges step and both harnesses in the same invocation;
  - **deleting one header** reruns the gauges step, which rewrites both headers, and both harnesses;
  - **a clean `difftests` build** from an empty `bin/gauges-all` succeeds;
  - **a second build after each case** runs only the arena lint;
  - **a wrong pinned hash:** in the worktree itself, change one valid single-pair `sha256=` value in `tools/data_images.txt`, leave `UPDATE` unset, and the incremental build must fail with that module's hash-mismatch diagnostic, not a configure-time parse error. Then restore the line.
  - Each run's recipe list goes in the notes.
- **(B) WEATHERM:**
  - `docs/retail-bugs.md` gets `WXM-9`, `WXM-10` and `WXM-11`. Their Id cells carry `(suspected, not used)`, as WXM-8 does, and their markers read `RETAIL BUG (kept, suspected): WXM-n`.
  - **Marker sites** in `src/weatherm/weatherm.c`, confirmed by reading:
    - **WXM-9:** both walk comparisons, the copy-size dispatch and the free-by-type comparison (about `:1296`, `:1304`, `:1393`, `:1440`; `:1363` is a constructor case, not a site);
    - **WXM-10:** both removal paths (about `:1429`, `:1536-1537`);
    - **WXM-11:** the predecessor loss (about `:1444-1447`).
  - **Triggers**, stated exactly in each entry. For WXM-11 that is the *immediately* following matching non-head node after a free that is not at the head; an intervening non-match restores `prev`.
  - The existing "a second such free" wording in the source and `docs/modules/weatherm.md:141-146` is corrected to that, and the module doc names the ids.
- **(B) DEMO:** `src/demo/demo.c`'s `disk_check` comment says the host zeroes the outputs on failure. The deferred entry is resolved as not a bug, and no DEMO id is added.
- **(B) Proof:**
  - `python3 -B tools/check_retail_bugs.py` passes with 3 more entries: 334 in this worktree, from HEAD's 331. The combined tree with 1.10a gives 335.
  - **No build output changes,** proved with the identity gate:
    - snapshots at `~/openfs98-verify/cl1`: before is `git archive` of the baseline plus `orig/`, after is the worktree plus `orig/`;
    - built sequentially at one absolute root of its own, `~/openfs98-verify/cl1/root` (never another slice's), by `~/openfs98-verify/gate/build_side.sh`, since generated MDL includes embed absolute paths;
    - each recipe keeps its own flags, and the build covers the default and `difftests` targets;
    - `python3 ~/openfs98-verify/gate/compare.py` with no `--require-set` prints `GATE PASS`: the same relative artifact set, every normalised PE equal and every `.o`/`.a` equal.
- **(C) `tools/norm_pe.py`:**
  - **Default mode** reproduces `pehash_norm.py` exactly. For a PE32 image it zeroes:
    - the COFF timestamp;
    - the optional-header checksum;
    - the export directory's timestamp, found through `opt+96` with section bounds `max(VirtualSize, SizeOfRawData)`;
    - the digits of every `CSWTCH.<n>` match, width-preserving, in the whole tail from `PointerToSymbolTable + 18*NumberOfSymbols` to EOF (when both fields are non-zero).
  - **`--peh2` mode** reproduces `peh2-legacy.py` exactly, for Story 1.4's evidence:
    - the same timestamp and checksum edits;
    - then a whole-file replacement of every `_CSWTCH\.[0-9]+` with the literal `_CSWTCH.N`, which can shorten the image;
    - its output is the first 16 lowercase hex digits of the SHA-256, one space, and the path as given.
  - **CLI:** `python3 -B tools/norm_pe.py [--peh2] FILE...` prints `<sha256>  <path>` per file in default mode, and peh2's format in `--peh2`.
    - Both modes first check that the file is a readable PE32: an MZ header, the PE signature in bounds, a PE32 optional header, and section headers in bounds. Any file failing that, unreadable or truncated, prints an error naming it.
    - Every file is processed. The exit code is 0 if all succeeded and 2 if any failed.
  - **`--self-test`** uses synthetic PEs:
    - export and symbol tables absent;
    - digit widths preserved in default mode;
    - a match past the declared string table's end;
    - a truncated file and a PE32+ file, both rejected;
    - pinned expected digests for both modes.
  - **Agreement check (scratch, recorded in the notes):** across every PE in a built `bin/`, the default mode's digests equal `pehash.py`'s per-file digests, and `--peh2` equals `peh2-legacy.py`'s.
  - **Docs:** `docs/tools.md` gets a row saying which evidence used which mode.
- **Isolation and merge:**
  - The work happens in a worktree off HEAD (`~/source/OpenFS98-cl1`), with `orig/` and this approved spec copied in.
  - Slice 1.10a's worktree also edits `cmake/steps/difftests.cmake`, `docs/tools.md` and `docs/retail-bugs.md`, on different lines. Both merge into the main tree after slice 6a closes, and any textual conflicts are resolved then.
- **Running:** builds use `nice -n 19` and `-j 2`; no wine.
- **Paperwork:** the eight entries in `deferred-work.md` are marked RESOLVED, each with a pointer to this spec.

**Never:**
- No change to any build output, only to rebuild triggers.
- No source edits other than comments.
- No change to the gate, `compare.py` or `cmake_compare.py`.
- No commits or staging.

</frozen-after-approval>

## Code Map

- **`cmake/OpenFS98.cmake:26-83`** is `ofs_step`. Its token scan keeps only `tools/*.py` and `orig/…` for `python3` lines (globs at `:51`, `:59` and `:68`). `DEPENDS_GLOB` is configure-time and recursive.
- **`cmake/steps/all.cmake`:**
  - the `--hash` steps are at `:12…274`, `:399`, `:455` and `:467`;
  - the gauges step is at `:404-420` (`built.stamp` its only declared output today) and the MDL step at `:438-445`;
  - the FE step is at `:104-111`.
- **`cmake/steps/difftests.cmake`:** `advdrvdiff.exe` is at `:34-37` (`advdrvdiff.c:83` includes the relocs twice); `allgaugesdiff.exe` and `allpaneldiff.exe` are at `:258-268`.
- **`tools/build_gauges.py:74,89`** writes the two headers.
- **WEATHERM:**
  - `src/weatherm/weatherm.c`: WXM-8's markers are at `:1218` and `:1290`, and `:1322` is its third. The new sites are listed above.
  - `tools/difftest/weathermdiff.c:574` already exercises non-member frees.
- **`src/host/host.c:2772`** is `host_150` ("0, or -1 (with *out 0)"). `src/demo/demo.c:423-435` is `disk_check`.
- **`tools/check_retail_bugs.py:15,37`** defines the id and marker forms.
- **The normalisers:** `~/openfs98-verify/gate/pehash_norm.py`, `pehash.py:5-22` (which filters on MZ and skips `break-pool*`), and `peh2-legacy.py`.

## Tasks & Acceptance

**Execution:**
- [ ] `~/source/OpenFS98-cl1` -- a worktree off HEAD, with `orig/` and this spec copied in.
- [ ] `cmake/OpenFS98.cmake`, `cmake/steps/{all,difftests}.cmake` -- (A).
- [ ] `docs/retail-bugs.md`, `src/weatherm/weatherm.c`, `docs/modules/weatherm.md`, `src/demo/demo.c` (comment) -- (B).
- [ ] `tools/norm_pe.py`, `docs/tools.md` -- (C).
- [ ] The proofs; `deferred-work.md` (the eight entries RESOLVED); this spec's `## Implementation Notes`.

**Acceptance Criteria:**
- **(A):** given the proof runs, each touched input reruns exactly its listed recipes, and the bad-hash case fails the build.
- **(B):** `check_retail_bugs.py` passes with 334 entries, and every after-build PE and object equals the baseline build's.
- **(C):** `tools/norm_pe.py --self-test` passes, and both modes agree with their reference scripts on every PE under `bin/`.
- **Scope:** `git status --porcelain --untracked-files=all` in the worktree shows only the files listed above.

## Verification

**Commands:**
- `python3 -B tools/check_retail_bugs.py` -- expected: pass, 334 entries.
- `python3 -B tools/norm_pe.py --self-test` -- expected: pass.
- `python3 -B tools/check_arena.py` -- expected: unchanged.

## Review Triage Log

Spec review (Codex gpt-6.2-sol, 2026-11-25), before approval:

| # | Finding | Verdict | Evidence / action |
|---|---|---|---|
| 1 | DEMO-12 rests on a false premise | blocker | Confirmed: `host_150` zeroes `*out` on failure. No DEMO id; the comment is corrected and the entry resolved. |
| 2 | The generated-header edges are underspecified (globs, clean build) | blocker | The headers become declared outputs with file dependencies; clean, delete and producer cases added. |
| 3 | "Exactly one step reruns" is false | high | Exact recipe sets per input; `--target difftests`; a bad-hash test. |
| 4 | Two historical normalisers | high | A default and a `--peh2` mode, documented by evidence. |
| 5 | The WXM annotation would fail the checker | high | `(suspected, not used)` cells and `kept, suspected` markers. |
| 6 | WXM marker sites and triggers | medium | Listed. |
| 7 | The exact gate algorithm and the CLI edge cases | medium | Specified. |
| 8 | Only FE.DLL needs the edge | medium | Corrected. |
| 9 | Comment-only identity is a hypothesis | medium | A whole-`bin/` comparison of fresh builds. |
| 10 | Line refs, the shared files with 1.10a, the spec copy | low | Corrected. |

Second round (Codex gpt-6.2-sol, 2026-11-25), on the revision:

| # | Finding | Verdict | Evidence / action |
|---|---|---|---|
| R1 | The rerun sets are wrong: the headers are rewritten every run, `--gauge-pairs` reruns, and `bin/` is ignored by the token scan | blocker | Exact sets restated; explicit file dependencies; spurious harness rebuilds after a gauges run accepted. |
| R2 | Separate worktree paths break whole-`bin/` identity (MDL includes embed absolute paths) | blocker | Now the identity gate: `build_side.sh` at one root, sequential, then `compare.py`. |
| R3 | `--peh2`'s contract depends on a private script | high | The contract is spelled out; validation and failure behaviour defined; digests pinned. |
| R4 | A scratch manifest copy has no effect | medium | The worktree's own manifest is mutated, then restored; the mismatch diagnostic is required. |
| R5 | WXM sites and the WXM-11 trigger | medium | Sites corrected; "immediately following"; the existing wording fixed. |
| R6 | The 334 and 335 counts | low | Both stated. |
