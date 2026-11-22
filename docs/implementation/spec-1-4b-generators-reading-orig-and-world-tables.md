---
title: 'Story 1.4, slice 2: every script the build runs that reads orig/ is checked; the World tables are committed C'
type: 'refactor'
created: '2026-11-19'
status: 'done'
route: 'dispatch'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43' # worktree held uncommitted Stories 1.2-1.4; file snapshot at ~/openfs98-verify/baseline-1-4b
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4-module-data-decompiled-no-raw-dumps-in-git.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** All 68 generators in `cmake/steps/` read `orig/`, most of them by retail RVAs and offsets, and so do several scripts run from `cmake/targets.cmake`. That ties the build to one CD pressing, which Story 1.4 exists to remove. Check (c) of `tools/check_raw_dumps.py` sees only the ~20 whose command line names `orig/Modules`.

**Approach:** Widen check (c) to every `tools/*.py` that the CMake files run and that reads `orig/`, and put each one on the allowlist under one of three kinds. Then convert the World batch first. Its 18 `gen_world_*.py` generators write 36 `.inc` files that are already typed C and already tracked, so the committed files become the source and 17 of the generators go.

## Boundaries & Constraints

**Always:**
- The allowlist has three entry kinds. Each entry is one line: `<kind>:<script>  # <reason>`.
  - `gen:` -- feeds module data into a build and is not yet converted. It may only be removed.
  - `resource:` -- audited: it reads from `orig/` only resources by type and id, or export tables by name (the import ABI of a retail DLL it links against). It may stay.
  - `oracle:` -- reads retail only to build test oracles, fixtures, staging copies or orientation output, never data compiled into one of our modules. It may stay: retail is the reference that difftests compare against.

  Any entry fails as stale once its script no longer reads `orig/`, or is no longer run from a scanned CMake file.
- "Unchanged" means byte-identical once the bytes that change on every link are zeroed: the COFF header TimeDateStamp, the export directory's TimeDateStamp, and the optional-header checksum if set. It covers `bin/WORLD.DLL` and the five WORLD-step files in `bin/world-diff/` (`world.res`, `world-res.o`, `simsui-anchor.def`, `libworld-simsui.a`, `WORLD_area.dll`). That comparison replaces the World difftests and break suites for this slice.
- `gen_world_runtime.py` keeps producing `bin/world-diff/world.res` and `simsui-anchor.def` at build time, and becomes `resource:`.
- Never `rm -rf bin`. It holds wine prefixes, break and run directories, and `bin/pressings/reference.json`, which cannot be rebuilt.
- No staging or commits. Story files go in `~/openfs98-verify/story-files.md`.

**Never:**
- No change to the values in any committed World `.inc` file. Header comments may be reworded without changing line counts.
- No other module's generators are converted in this slice.
- No generator is hidden from the check.

</frozen-after-approval>

## Code Map

- `tools/check_raw_dumps.py` (c) -- today: a `cmake/steps/*.cmake` line holding `tools/gen_*.py`, `orig/Modules` and `src/`. The new rule covers every `tools/<name>.py` named in `cmake/steps/*.cmake` or `cmake/targets.cmake`. Read each command whole, with its continuation lines joined. The script must appear under exactly one kind when any of these mentions `orig` as a path segment (regex `\borig\b`, so `orig/`, `'orig'` and `Path('orig')` all count, comments and strings included):
  - the command;
  - the script's source;
  - the source of a `tools/` module it imports, followed transitively.

  Scripts reached only through a `tools/*.sh` wrapper are out of scope here; add them to deferred work. Keep the inline asserts and add one for the new rule.
- How each script is classified. The third review simulated the rule (whole commands, transitive imports, all of `cmake/steps/*.cmake`, which includes `difftests.cmake`, plus `targets.cmake`): 86 matches now and 69 after this slice. `dialogmap.py` defaults to `Path('orig')`, so everything importing it matches.
  - `gen:` (about 53): every data or table generator. Also:
    - `build_mdls.py` (all.cmake:435; its data goes into the shipped MDL DLLs);
    - `gen_hg2d_data.py` (targets.cmake:860; writes the tracked `src/hg2d/hg2d_data.inc`);
    - `gen_warning_data.py` and `gen_suction_data.py` (targets.cmake:466, :468; retail data compiled into test DLLs);
    - `gen_sim1_data.py` (difftests.cmake:348; it reads `cd/` and writes `orig/`).
  - `oracle:` (about 15): `hostmap.py`, `modgraph.py`, `mpdialog_owner_acceptance.py`, `stage_gauges.py`, `stage_sim1.py`, `stage_mdls.py` (targets.cmake:122,128), `pesurvey.py` (targets.cmake:736, given `$(tools/modules.sh orig-module-of …)`) and `gen_mdl_fixtures.py` (difftests.cmake:321). Also the break and frame runners pulled in through imports of `build_mdls`, `mdl_survey`, `gen_gauges` and `dialogmap`: `mdl_module_breaks.py` (targets.cmake:158), `mdl_frames.py` (:164), `mdl_crash_breaks.py` (:175), `mdl_crash_frames.py` (:197), `mdl_crash_render_break.py` (:1442), `mdl_render_break.py` (:1461) and `sim1r_breaks.py` (:797; its only hit is in its docstring).
  - `resource:`: `gen_world_runtime.py` only. The other `*_runtime` and resource generators stay `gen:` until audited, in a later slice.
  - Install and run targets need no entry: their `cp orig/…` and `wine` lines run no script.
- `cmake/steps/all.cmake:278-298` -- the WORLD step. Drop the 17 generator lines. Keep `gen_world_runtime.py` (`:282`) and the windres and dlltool lines.
- `tools/gen_world_{advanced,airport,booster,cache,complexity,copy,dynamic,facilities,files,list,position,priority,search,tables,time,wareas,weather}.py` -- delete them. 16 of them import `gen_world_tables`. Nothing outside `tools/gen_world_*` imports any of them (repo-wide grep).
- `tools/gen_world_runtime.py` (441 lines) -- remove lines 15-414, the table half: `local_bindings`, `imports`, and `CLASSES`/`instructions` from the deleted modules, writing `src/world/world_runtime_tables.inc`. Keep:
  - lines 1-14: World load, base check, `output_dir` and its `mkdir`;
  - lines 415-441: SIMSUI's export ordinal, `world.res`, and the `__main__` guard. These use only `pe`, `output_dir`, `Path`, `struct` and `resource_name`.

  Update the docstring.
- `src/world/*.inc` -- 36 tracked outputs, each starting with `/* Generated by tools/gen_world_*.py ... */`. Reword each to say the values were read from retail World.DLL by the retired generator, with the same number of lines. A temp-copy run of all 18 generators matched the tracked files exactly.
- `cmake/OpenFS98.cmake:3-5` -- the comment that src's generated `.inc` files aren't tracked as inputs, "upgrade with Story 1.4". Say that the World `.inc` files are now plain sources.
- `docs/modules/world.md` (14 mentions of the generators), `docs/tools.md` (the count on the `check_raw_dumps.py` Baselines line), and `docs/implementation/deferred-work.md:37-39` (this slice resolves that check (c) entry; mark it resolved, don't delete it).

## Tasks & Acceptance

**Execution:**
- [x] Before any edit:
  - run `tools/ofs all`;
  - record the normalized sha256 of each file in scope (a throwaway script outside the repo zeroes the three timestamp and checksum fields for the two PE files, `WORLD.DLL` and `WORLD_area.dll`, and hashes the other four with plain sha256: archive members have mtime 0, and `world-res.o`'s COFF stamp is 0);
  - confirm `git diff --quiet -- src/world`.
- [x] `tools/check_raw_dumps.py`, `tools/raw_dumps_allowed.txt` -- the new rule and the three kinds, every match classified. Check (c)'s stale report must not depend on `orig/` being present, since its rule reads scripts, not `orig/`. Only (b)'s stale entries are skipped without `orig/`.
- [x] `tools/gen_world_runtime.py` -- keep lines 1-14 and 415-441, then check it still works: from a temp directory holding a symlink `orig` pointing at the repo's `orig/`, run `python3 <repo>/tools/gen_world_runtime.py`, and check that `bin/world-diff/world.res` and `simsui-anchor.def` appear there (both paths in the script are relative to the working directory).
- [x] Delete the 17 generators. Edit `cmake/steps/all.cmake` and the 36 headers.
- [x] Docs, the `cmake/OpenFS98.cmake` comment and `deferred-work.md`. Add the remaining `gen:` entries to deferred work, grouped by module.
- [x] After the header edits, save `git diff -- src/world` to a file outside the repo. Then delete the six files in scope (deleting `bin/WORLD.DLL`, the step's only OUTPUT, forces the step), rebuild with `tools/ofs all`, and compare normalized hashes with the record.

**Acceptance Criteria:**
- Given the record from before the change, when the files in scope are rebuilt, then all normalized hashes match.
- Given the tree, when `check_raw_dumps.py` runs, then it exits 0 and no `gen_world_*` appears except `resource:gen_world_runtime.py`.
  - Both negative cases run in a temp copy of the tree, never by editing the repo:
    - the copy is a git clone of the worktree state (it needs `.git`, because the script calls `git ls-files`), with `orig` symlinked to the repo's;
    - adding a line to the copy's `cmake/targets.cmake` that runs a script reading `orig/` exits 1;
    - a `resource:` or `oracle:` entry whose script, in the copy, no longer mentions `orig` exits 1.
  - The `docs/tools.md` Baselines line states the count the script then prints.
- Given the `git diff -- src/world` saved after the header edits, when the rebuild has run, then `git diff -- src/world` is unchanged: no step rewrites a World file any more.

## Implementation Notes

- The subagent forced the WORLD step on the unchanged tree and recorded normalized hashes of the six files. After the change it deleted them and rebuilt: all six match. Checked independently by comparing against the World build in `~/openfs98-verify/s14` (slice 1, before this slice). Both DLLs normalize to the same hash, and the four other files are byte-identical. `git diff -- src/world` is unchanged after a forced rebuild.
- `check_raw_dumps.py` lists 33 dump files and 69 scripts (53 gen, 1 resource, 15 oracle). Both negative cases ran in a temp git copy and exited 1. The stale `resource:` case could not use `gen_world_runtime.py`, because it imports `dialogmap`, which defaults to `Path('orig')`; it was tested on an `oracle:` entry instead.

- After the review patches the negative cases were rerun, each in a clean copy with `orig` symlinked: an unknown kind, a new script reading orig/ in `targets.cmake`, and a stale `oracle:` entry (`stage_sim1.py` emptied) each exit 1. Without orig/ the check exits 0. The World build is still identical to the one in s14.

## Spec Change Log

## Review Triage Log

| # | Finding | Verdict | Evidence / route |
|---|---|---|---|
| 1 | Stale path entries are checked only for src/ and only when orig/ exists (EC, me) | medium | Code at :121 gates on `src/` and `have_orig`. Patch: (a) hits get their own set; only entries tagged `(b)` are skipped without orig/. |
| 2 | An unknown kind (`gne:`) is silently ignored (EC, BH) | low | Real; a direct check. Patch. |
| 3 | `import x as y` is not followed (EC, VG) | low | `.replace(" ","")` gives `xasy`. Patch. |
| 4 | No self-check for the import and allowlist logic (VG, BH) | low | Patch: one assert on a known chain. A fixture suite is not added. |
| 5 | The allowlist header contradicts the kinds (BH) | low | Patch. |
| 6 | The world_runtime_tables.inc header calls the live script retired (EC, BH) | low | Patch. |
| 7 | world.md keeps present-tense generator claims (BH) | low | Patch. |
| 8 | deferred-work entries are untidy and mix concerns (BH) | low | Patch. |
| 9 | No proof that the committed .inc files equal the generator output (BH) | false | The second spec review ran all 18 generators into a temp copy and got `diff -r` SAME. The normalized build comparison holds too. |
| 10 | Committed retail-derived tables are invisible to the check (BH) | false | By design (AD-12): typed C tables are the target form. Check (a) flags only untyped runs. |
| 11 | Editing a World .inc may not rebuild WORLD.DLL (BH, EC) | false | `cmake/OpenFS98.cmake:54` globs `*.inc` in each named source directory (confirmed by VG). |
| 12 | No automated run of the checker (VG, BH) | low | Same as slice 1: it is a Baselines line, which `cmake_verify.py` runs. Rejected. |
| 13 | `\borig\b` matches comments and prose (BH, EC) | low | Chosen in the spec: a false match only costs a classified entry. Rejected. |
| 14 | A script named through a CMake variable, or from another CMake file, escapes (EC) | low | None exists today (all 69 are literal names in the scanned files). Rejected. |
| 15 | A missing targets.cmake or non-UTF-8 source crashes the check (EC) | false | It fails loudly, which is correct. |
| 16 | A generator could be relabelled `oracle:` to hide (BH) | low | Every entry carries a reason that review reads. Rejected. |
| 17 | Shallow import scan: dotted imports, importlib, subprocess (BH) | low | None is used to reach orig/ today; `as` is patched (3). Rejected. |
| 18 | gen_world_runtime.py keeps unused imports (BH) | false | `struct` packs world.res and `SurveyError` is raised at line 13. |

## Verification

**Commands:**
- The normalized-hash comparison above -- expected: no difference.
- `python3 tools/check_raw_dumps.py` -- expected: exit 0, plus both negative cases exit 1.
- World has no whole-module difftest. Its break runners (`tools/world_*_breaks.py`) compile `src/world` directly and use `bin/world-diff/world-res.o`, so identical files stand in for them.
