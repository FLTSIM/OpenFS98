---
title: 'Story 1.9, slice b: the keep.h entries are audited and pinned'
type: 'chore'
created: '2026-11-28'
status: 'done'
baseline_commit: 'fdc56f7ecefb72537d0738d678b6aa4eb8c09f03'
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/native-port/architecture/ARCHITECTURE-SPINE.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Story 1.9b was planned as giving the 14 `include/fs98/keep.h` entries, plus HG2D's raw `FS98_ENTRY`, explicit values. Investigation found no C body to convert:
- every entry only preserves registers, or (HG2D `+0x24`) leaves an argument in ecx, for retail callers;
- 1.9a already compiled the wrappers out of every non-retail build.

Two slots' targets are asm thunks that do read an input register: HG2D `colour_remap` reads eax through `EAX_ENTRY` (HG2D-2), and DYNAMIC `_dyn_frame` reads ecx (DYN-5). Both are already 1.9e rows. Nothing checks any of this, so a new entry, or asm added to a body, would go unnoticed.

**Approach:** Add a lint that pins all 15 entries to a classification table and fails on any drift. Wire it into the build beside the other lints, document the result, and close 1.9b. No product source changes. Decided 2026-11-28 by the user ("one audit slice").

## Boundaries & Constraints

**Always:**
- **The table:** a new `tools/keep_entries.txt` holds two kinds of row.
  - **Entry rows (15):** `file|macro|entry|symbol|args|kept|target|class|body`.
    - `macro` is the `FS98_KEEP_*` variant or `FS98_ENTRY`, and `args` the remaining arguments, whitespace-normalised. For `FS98_ENTRY`, these include the bytes, count, saves and restores.
    - `kept` is the `FS98_KEPT(entry, function)` binding.
    - The source's invocations and bindings must equal the rows as multisets.
  - **The 15 entries' classes:**
    - **`c`** (12 rows): HG2D `draw_depth_point`, `colour_convert`, `texture_create` and `texture_pixels`; ATC `atc_atis` and `atc_clear`; NUMERIC `fs98_numeric_format`, `fs98_numeric_parse` and `fs98_numeric_parse_internal`; MAIN `clouds_textures` and `nav_eye`; WEATHERM `wx_reset`.
    - **`thunk-out`** (1 row): HG2D `palette_select`, which takes only its stack argument and returns carry.
    - **`thunk-in:<reg>`** (2 rows): HG2D `colour_remap` (`eax`, whose upper 16 bits survive into its result) and DYNAMIC `dyn_frame` (`ecx`).
  - **Thunk rows (3):** `thunk|label|asm`. `asm` is the label's instructions, decoded from the joined literals after macro expansion, whitespace-normalised. It includes the body call, so a register or flag change, or a redirected call, fails.
- **What `tools/check_keep_entries.py` checks:**
  - **Expansion:** it preprocesses each owning translation unit (the `.c` that includes the file) with `i686-w64-mingw32-gcc -E` and that module's flags, which is the win32-retail configuration.
  - **Bodies:** each `c` target and each thunk body resolves to exactly one definition in the expanded unit, matched by balanced braces. That definition must have no `asm`, `__asm` or `__asm__`. A same-named definition in another module is ignored.
  - **Thunks:** each thunk label resolves to exactly one asm definition, which must equal its row.
  - **Failures:** any unresolved row fails, and so does any untabled invocation in `src/` (`.c`, `.inc`, `.h`; `src/util/` is excluded until 1.9c revisits it).
  - **Shrink rule:**
    - The `thunk-in` rows may only shrink, compared by full identity as a multiset.
    - The prior is `--prior FILE` (which must be readable and valid), or HEAD's table. With neither, the lint uses the table itself and prints that it did.
    - `p19b/gate.sh` passes a reviewed two-row prior.
- **Self-test:** `--self-test` has one failing fixture, each with its diagnostic, for each of these cases:
  - an untabled invocation, or a missing row;
  - a swapped KEEP variant, a changed byte count, or a changed raw restore;
  - a miswired `FS98_KEPT`;
  - asm in a `c` body, including asm hidden behind a macro;
  - in a thunk, `%eax` changed to `%ecx`, a register input added to `palette_select`, or the body call redirected;
  - a `thunk-in` row added, together with its source, or a same-count replacement;
  - two definitions in one unit, or an invalid prior.

  It also has two passing fixtures: the real table, and a same-named definition in another module.
- **Wiring:** add one `COMMAND python3 -B tools/check_keep_entries.py` to `ofs_arena_lint` (`cmake/OpenFS98.cmake:28`). That covers every target that depends on it.
- **Docs:**
  - **Tools:** a `docs/tools.md` row and quick-reference line.
  - **Register table:** `docs/open-differences.md` splits it by slot wherever the class changes, and adds a class column.
  - **keep.h:** its comment points to the lint, and nothing else in it changes.
  - **deferred-work.md:**
    - 1.9b is resolved as this audit only, and Story 1.9's own acceptance stays open.
    - 1.9e explicitly names `colour_remap` and `dyn_frame` as input thunks, and `palette_select`'s carry result as still to be made explicit.
    - 1.9c must revisit the UTIL exclusion.
- **Gate:**
  - **Builds:** two frozen snapshots of the same baseline commit with the same retail inputs; after differs only by this slice's files. Both are built with `gate/build_side.sh` at one root (nice 19, `-j 2`), passing the baseline arena inventory as its prior.
  - **Acceptance:** default `compare.py` `GATE PASS`, plus `p19b/verify.py`.
    - It compares live files with exact bytes for non-PEs and replayed objects, and PE bytes outside the named masks (COFF timestamp and checksum).
    - It checks the live `bin/` inventory against `art.paths`.
    - It requires HG2D, ATC, NUMERIC, MAIN, WEATHERM and DYNAMIC's DLLs on both sides.
  - **Negatives**, each on a `cp -al` copy that replaces files rather than writing through:
    - a replayed object replaced: both tools exit 1 naming it;
    - a retained DLL changed outside the masks: `verify.py` exits 1 naming it.
  - **Wiring probe:** in a disposable copy of after, introduce a KEEP mismatch and dirty one module output, then run the build with `OFS_STEP_LOG` set. It must exit non-zero, print the KEEP diagnostic and leave an empty step log. After the fixture is restored, the build must pass.
  - **Earlier sets:** the gate scripts are unchanged. If `compare.py` changes, rerun p18b–p18i, p110b1 and 4b `--strict`.

**Never:**
- Change any `src/`, `include/` code, `.S` or recipe line, except the one lint command and the `keep.h` comment.
- Convert the `thunk-in` thunks. That is 1.9e.
- Touch UTIL's private `KEEP_*` macros. That is 1.9c.
- Edit `AGENTS.md`, stage, or commit.

**Decided (2026-11-28, user):** `verify.py` also masks the export-directory timestamp, the same build-time stamp as the COFF one, which the spec under-named; everything else stays byte-exact.

</frozen-after-approval>

## Code Map

**The entries:**
- **HG2D:** `src/hg2d/hg2d.c:213-221`, slots at `:251-284`.
- **ATC:** `src/atc/atc.c:399-401`, Linkage `:945-946`.
- **NUMERIC:** `src/numeric/numeric.c:1871-1874`, Linkage `:1895-1897`.
- **MAIN:** `src/main/main.c:321-322`, slots `:369,379`.
- **WEATHERM:** `src/weatherm/weatherm.c:1080`, slot `:1599`.
- **DYNAMIC:** `src/dynamic/dynamic.c:2627`, slot `:2642`.

**The targets:**
- **HG2D:** `draw_depth_point` at `hg2d_draw.inc:76`, `colour_convert` at `hg2d_palette.inc:38`, `texture_create` at `hg2d.c:44`, `texture_pixels` at `hg2d.c:130`.
- **ATC:** `atc_atis` at `atc.c:666`, `atc_clear` at `atc.c:408`.
- **NUMERIC:** `numeric.c:511,1847,1448`.
- **MAIN:** `clouds_textures` at `main_clouds.inc:561`, `nav_eye` at `main_eye.inc:22`.
- **WEATHERM:** `wx_reset` at `weatherm.c:1086`.

**The thunks:**
- **`palette_select`:** `hg2d_palette.inc:347-353`, body at `:315`.
- **`colour_remap`:** `EAX_ENTRY` at `hg2d.c:158-162` and `hg2d_palette.inc:54`, body at `:48`.
- **`_dyn_frame`:** `dynamic.c:388-394`.

**To reuse:** `tools/check_div_asm.py`, for the `--prior`/HEAD shrink check and the self-test style (not its tree-wide macro map). `tools/module_flags.py` for the flags. `tools/check_build_wiring.py:298` for the wiring probe. `~/openfs98-verify/p19a/verify.py` and `gate/build_side.sh` for the gate.

**Background:** NUMERIC's own inline asm (`numeric.c:271,292`, the x87 scaling) is outside these bodies. UTIL's `NUMERIC_*` calls (`util_exchange.c:237,350,382`) are C.

## Tasks & Acceptance

**Execution:**
- [x] `tools/keep_entries.txt` -- the 15 rows -- the pinned classification.
- [x] `tools/check_keep_entries.py` -- the lint and `--self-test` -- it catches drift.
- [x] `cmake/OpenFS98.cmake` -- one `COMMAND` -- runs on every build.
- [x] `include/fs98/keep.h`, `docs/tools.md`, `docs/open-differences.md`, `docs/implementation/deferred-work.md` -- docs as above.
- [x] `~/openfs98-verify/p19b/` -- the snapshots, `gate.sh`, `verify.py`, the prior, the negatives and the wiring probe.

**Acceptance Criteria:**
- Given the tree, when the lint runs, then it reports 15 entries (12 c, 1 thunk-out, 2 thunk-in) and 3 thunks, and exits 0.
- Given `--self-test`, when it runs, then every failing fixture fails with its diagnostic and both passing fixtures pass.
- Given the wiring probe, when the build runs with the mismatch, then it exits non-zero with the diagnostic and an empty step log. Once the fixture is restored, the build passes.
- Given both snapshots built, when the gate runs, then it prints `GATE PASS`, `verify.py` passes, and each negative exits 1 naming its file.

## Implementation Notes

- `verify.py` masks three PE fields, not two: the COFF timestamp, the optional-header checksum and the export directory's own timestamp (`world-diff/WORLD_area.dll`'s `.edata` carries a build time). The user accepted this on 2026-11-28, and `verify.py` names all three in its output.
- The lint runs `i686-w64-mingw32-gcc -E` without `GCCLOG`, so the gate's compile wrapper does not log it as a build unit.

## Spec Change Log

## Review Triage Log

## Verification

**Commands:**
- `python3 -B tools/check_keep_entries.py --self-test && python3 -B tools/check_keep_entries.py` -- expected: both pass.
- `python3 -B tools/check_arena.py && python3 -B tools/check_div_asm.py` -- expected: still pass.
- `bash ~/openfs98-verify/p19b/gate.sh` -- expected: `GATE PASS` and `verify.py` passing.
