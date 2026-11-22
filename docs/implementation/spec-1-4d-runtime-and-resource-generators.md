---
title: 'Story 1.4, slice 4: runtime generators reduced to resources; two table generators retired'
type: 'refactor'
created: '2026-11-19'
status: 'done'
route: 'dispatch'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43' # worktree held uncommitted Stories 1.2-1.4; file snapshot at ~/openfs98-verify/baseline-1-4d
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4c-retail-table-generators.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** 45 `gen:` scripts are left. Seven `*_runtime` generators each write a tracked typed table into `src/` from retail vtables and message maps by RVA, and also build resources. Two table generators wait only on them: `gen_options_tables`, which `gen_options_runtime` parses, and `gen_sim_tables`, which `gen_sim_runtime` imports. The resource-only scripts also stop the build on any CD other than the reference one, with an image-base or module-id check that feeds none of their output.

**Approach:**
- **The seven runtime scripts** (alerts, flights, help, mpdialog, options, sim, views): delete each one's table half and commit its `*_runtime_tables.inc` as source. The remainder (resources by type/id, the SIMSUI `ui_new` export by name) becomes `resource:`.
- **Retire** `gen_options_tables` and `gen_sim_tables`; their outputs are all tracked.
- **Relabel** `gen_host_resources` and `gen_main_bitmap` as `resource:`.
- **`gen_simsui_native`**: cut its write of the tracked `simsui_native_tables.inc`. It stays `gen:`, because it embeds retail `.data` into `bin/`.
- **Remove the image-base and module-id checks** from every `resource:` script, including `gen_world_runtime`; a resource looked up by type and id needs no particular pressing. `gen_main_bitmap` has none, so it is relabelled only.
- **Move `gen_host_resources`' SHA-256 pin on `FLTSIM98.EXE`** into `gen_host_data.py`, which stays `gen:` and is where the host's hard-coded addresses live. The host build still depends on the reference EXE, so its check stays where that dependence is.
- **Not this slice:** `gen_host_rt_tables` writes `host_rt_rdata.inc`, a listed hex dump. It stays `gen:`, and so does `gen_simsui_tables`, which it imports (`:40`). Both go with the dump slices.

## Boundaries & Constraints

**Always:**
- Use slices 2 and 3's method (`spec-1-4c`).
  - Before any edit: record normalized hashes of every affected output (step outputs, their `bin/<mod>-diff/` and `bin/host-rt/` files, and the difftest exes listed below).
  - After the edits: force a rebuild by deleting each step's OUTPUT and the exes, then rebuild with `tools/ofs all` and `tools/ofs difftests`. Every hash must match.
  - Save `git diff -- src` after the header edits; it must be unchanged after the rebuild.
- Move the one shared `resource_name` (now in `gen_views_runtime.py`, imported by `gen_sim_runtime` and `gen_world_runtime`) into `tools/dialogmap.py`. The five scripts with identical copies (alerts, help, views, flights, options) import it too and drop their copies; MPDIALOG (no WORD guard) and `gen_simsui_native` keep their own.
- Committed outputs get their header reworded to name the retired script or half, with the same line count and no value changes.
- `gen_sim1_native`, `gen_gauges`, `build_mdls` and `gen_mdl_crash` are not touched. They put retail data into `bin/` and stay `gen:` for the dump slices.
- Never `rm -rf bin`. No staging or commits. Record the files in `~/openfs98-verify/story-files.md`.

**Never:**
- No change to what any resource script writes for the reference CD.
- No `resource:` script has a hard-coded RVA or VA, image-base arithmetic, or code reads. Walking the resource and export directories is allowed, through `Fs98Pe` or, as in `gen_main_bitmap`, raw `struct` code.

</frozen-after-approval>

## Code Map

- `cmake/steps/all.cmake`, the step OUTPUTs:
  - `bin/MAIN.DLL` (:154; `gen_main_bitmap`, :158)
  - `bin/WORLD.DLL` (:273)
  - `bin/ALERTS.DLL` (:284)
  - `bin/HELP.DLL` (:296)
  - `bin/SIM.DLL` (:308)
  - `bin/VIEWS.DLL` (:321)
  - `bin/FLIGHTS.DLL` (:333)
  - `bin/MP_DIALOG.DLL` (:345)
  - `bin/OPTIONS.DLL` (:363)
  - `bin/SIMSUI.DLL` (:382; `gen_simsui_native` runs at :385 and :389, and both runs write the `.inc`)
  - `bin/OPENFS98.EXE` (:425; `gen_host_resources` at :431)

  The runtime scripts run at :285-370. Re-read the line numbers before editing.
- What each runtime script keeps: the resource and `.res` code plus the `ui_new` export lookup (for example `gen_alerts_runtime.py:96-123`). Each drops the base and linkage check and the table code: the imports, vtables, message maps, factories and the `.inc` write. In alerts that is :24-25 and :28-95; the review ran a copy with exactly those lines removed, and it gave identical `alerts.res` and `simsui-anchor.def`.
- `gen_options_runtime.py` reads `tools/difftest/optionseventsdiff.c` and parses `gen_options_tables.py` with `TABLE_ROW` (:24, :32, :68). Both go with the table half.
- `gen_simsui_native.py` reads `src/simsui/simsui_tables.inc` as text (:37, :92). That file stays tracked, so this keeps working.
- Tracked outputs that become source:
  - `src/{alerts,flights,help,mpdialog,options,sim,views}/*_runtime_tables.inc`
  - `src/options/options_tables.inc`, `options_objects_tables.inc`
  - `src/sim/sim_tables.inc`, `sim_hooks.inc`
  - `src/simsui/simsui_native_tables.inc`

  Check each with check (a) first. If one is flagged as a dump, report it and leave its generator alone.
- Other files to hash, besides each OUTPUT:
  - Under `bin/<mod>-diff/`: each `.res`, `-res.o`, `simsui-anchor.def`, `lib<mod>-simsui.a` and `<MOD>_ours.dll` (WORLD's is `WORLD_area.dll`). MPDIALOG has 8 `.res` and 8 `-res.o`.
  - Under `bin/simsui-native/`: `simsui.res`, `simsui-res.o`, `simsui_image.bin`, `simsui_image.S`, `simsui.def`, the unit `.o` files and `SIMSUI_ours.dll`.
  - `bin/host-rt/host.res` and `host_resources.o`.
- Difftest exes to hash and force: `simdiff.exe` (`difftests.cmake:372`) and `optionseventsdiff.exe` (:339). The review checked that no other exe compiles these outputs.
- Seven of these scripts can import the shared `resource_name`: the copies in alerts, help, views, flights and options are identical, and MPDIALOG's lacks the WORD guard, so it keeps its own. `dialogmap` imports only the standard library, so there is no cycle.
- `tools/raw_dumps_allowed.txt`:
  - Move the seven runtime scripts plus `gen_host_resources` and `gen_main_bitmap` to `resource:`.
  - Remove the two retired scripts.
  - The `docs/tools.md` Baselines line should then read `34 allowed dump files, 59 scripts reading orig/ (34 gen, 10 resource, 15 oracle)`. Confirm against the script's output.
- Docs and comments that name these scripts: `docs/modules/*.md`, `src/options/options_objects.h:31`, the `gen_simsui_native.py:18` docstring and `deferred-work.md:48,50`. Edit the worktree as it stands, keeping every uncommitted change, and don't rewrite unrelated text. Also reword `src/flights/flights_runtime.c:3` and `src/options/options_runtime.c:3`, which say the runtime script "lists" the imports, before the `git diff -- src` snapshot.

## Tasks & Acceptance

**Execution:**
- [x] Record the hashes. Confirm that each script reproduces its tracked outputs in a temp copy (the survey says yes for all).
- [x] `tools/dialogmap.py`: take `resource_name` from `gen_views_runtime.py`. Point its two importers at it, and the five scripts with identical copies too (delete those copies).
- [x] The seven runtime scripts: cut the table halves and the identity checks, and update the docstrings. Remove the identity check from `gen_world_runtime.py`, and move `gen_host_resources.py`'s SHA pin to the top of `gen_host_data.main()`, hashing its `SRC` before anything is written, so a different EXE stops the build before `src/host/host_data.inc` is rewritten.
- [x] Retire the two table scripts and their `all.cmake` lines. In `gen_simsui_native.py`, cut the `.inc` write from both invocations.
- [x] Reword the output headers. Update the allowlist, `docs/tools.md`, the module docs, `docs/building.md` if needed, and `deferred-work.md` (the `gen:` scripts that stay, and why).
- [x] Force the rebuild, compare the hashes, and check `git diff -- src`.

**Acceptance Criteria:**
- Given the record, when the forced rebuild finishes, then every normalized hash matches. The MAIN step is not forced, because `gen_main_bitmap` is only relabelled.
- Given the tree, when `check_raw_dumps.py` runs, then it exits 0 and every `resource:` script's code has no RVA-based read. A grep of those scripts for `va_uint32`, `uiBase`, `linkage()`, `objdump`, `\bBASE\b`, `disassemble`, `read_message_maps` and `imports()` finds nothing.
- Given a real-file copy of `Alerts.DLL` with its ImageBase field (optional header +28) patched, passed as `argv[1]` (`gen_alerts_runtime.py:23`), and run in a temp directory (never write into `orig/`, which may be a symlink):
  - before the edits, the script must stop with its "expected the reference Alerts image" error;
  - after them, it must write `alerts.res` and `simsui-anchor.def` byte-identical to the unpatched run.

## Implementation Notes

- The subagent forced the rebuild and compared it against a baseline built from `~/openfs98-verify/baseline-1-4d` in a scratch copy: 108 files, all identical after normalization, with `git diff -- src` unchanged. Its first normalizer misread the section header; it fixed that and rebuilt the baseline before the final comparison.
- Checked independently against the build in `~/openfs98-verify/s14` (before slices 2-4): 68 outputs, none different. They cover 10 module builds and `OPENFS98.EXE`, both difftests, every `_ours.dll`, `.res`, `-res.o` and anchor file, `host.res`, `host_resources.o`, `simsui.res` and `simsui_image.bin`.
- `check_raw_dumps.py`: 34 allowed dump files, 59 scripts reading orig/ (34 gen, 10 resource, 15 oracle). The AC2 grep finds nothing. AC3 behaved as required: with a copy of Alerts.DLL patched to ImageBase 0x10000000, the old script stops with its identity error, and the new one writes `alerts.res` and `simsui-anchor.def` identical to the unpatched run.

## Spec Change Log

## Review Triage Log

| # | Finding | Verdict | Evidence / route |
|---|---|---|---|
| 1 | With the identity checks gone, a pressing whose resources differ builds cleanly against tables committed for one image, and fails only at run time (VG, EC, BH) | maybe-false (medium if true) | Removing the checks was the spec's decision. Whether any pressing's resources differ is unknown until the pressing comparison is run. Deferred. |
| 2 | gen_mpdialog_runtime keeps its own resource_name (VG, BH) | false | Kept by the spec: it lacks the WORD guard, and the shared one raises on ids above 0xffff. |
| 3 | Unused imports (SurveyError, struct, sys) left in the trimmed scripts (BH, VG) | false | An AST scan of the eleven scripts finds no unused import. |
| 4 | gen_host_resources run on its own is no longer pinned (EC, BH) | low | In the build `gen_host_data` runs first (all.cmake:425 before :429) and stops on another EXE. The spec moved the pin deliberately. Rejected. |
| 5 | gen_host_data gives a raw FileNotFoundError when the EXE is missing (EC) | low | It fails loudly. Rejected. |
| 6 | gen_simsui_native no longer catches drift between its committed table and the image relocations (EC) | low | Same as every retired generator: the committed C is the source now. Rejected. |
| 7 | options.md still describes a constructor/destructor check that no longer runs (EC) | low | Patched: the doc now says the retired script ran it and nothing re-checks. |
| 8 | world.md double parenthetical (BH) | low | Patched. |
| 9 | No hash evidence recorded (BH) | low | Recorded in Implementation Notes. |
| 10 | Dependency edges for the committed .inc files may go stale (BH) | false | `cmake/OpenFS98.cmake:54` globs `*.inc` in each named source directory. |
| 11 | No regeneration path or provenance for the committed tables (BH) | false | By design (AD-12), as in slices 2-3. |
| 12 | gen_simsui_native's output and naming are odd now that it is mostly verify-only (BH) | low | It still writes `bin/simsui-native/`. Cosmetic. Rejected. |
| 13 | Hand-kept counts can drift (BH) | low | The Baselines line is checked by `cmake_verify.py`. Rejected. |
| 14 | gen_main_bitmap's relabel is unverified (BH) | false | It has no identity check (second spec review), and the AC2 grep is clean. |
| 15 | No check that only dialogmap defines resource_name (VG) | low | Two copies stay on purpose (2). Rejected. |

## Verification

**Commands:**
- The normalized-hash comparison -- expected: no difference.
- `python3 tools/check_raw_dumps.py` -- expected: exit 0 with the new counts.
- The identity-check removal test above -- expected: identical output.
- `dialogmap.py` is not a tracked step input: steps list only the scripts on their command line. The forced rebuild is what proves the `resource_name` move.
