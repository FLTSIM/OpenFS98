---
title: 'Story 1.4, slice 3: eight table generators retired, their typed C committed; check (a) sees macro-continued dumps'
type: 'refactor'
created: '2026-11-19'
status: 'done'
route: 'dispatch'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43' # worktree held uncommitted Stories 1.2-1.4; file snapshot at ~/openfs98-verify/baseline-1-4c
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4b-generators-reading-orig-and-world-tables.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Story 1.4 has 53 `gen:` scripts left. Eight of them only rewrite tracked, typed C tables from retail at build time. That is the same situation as World before slice 2, and it is the cheapest kind left to retire. Check (a) also misses dumps written as designated initialisers that continue a macro over `\` lines. `src/global/global_state.inc` is one: 194 designated single bytes.

**Approach:** Retire these eight generators the way slice 2 retired World's: their tracked outputs become plain sources, the generators and their build lines go, and every affected step output stays byte-identical after normalization.

| Module | Generators |
|---|---|
| MULTIP | `gen_multip_dispatch`, `gen_multip_profile`, `gen_multip_weather` |
| ALERTS | `gen_alerts_tables` |
| HELP | `gen_help_tables` |
| VIEWS | `gen_views_tables` |
| FLIGHTS | `gen_flights_tables` |
| SIM1 | `gen_sim1_tables` |

Teach check (a) to strip a trailing `\`, so `global_state.inc` becomes a listed dump file. Its decompilation comes in a later slice.

## Boundaries & Constraints

**Always:**
- Use slice 2's method (`spec-1-4b`, Tasks and Verification).
  - Before any edit: build, then record normalized hashes of each affected step's outputs (zero the COFF and export-directory TimeDateStamps and the checksum in PE files; plain sha256 for everything else).
  - Run each generator once into a temp copy and check that its output equals the tracked file.
  - After the edits: delete each step's OUTPUT to force it, rebuild, and compare the hashes. Save `git diff -- src` after the header edits and require it unchanged after the rebuild.
- Each output's header comment is reworded to name the retired generator, with the same line count. No value changes.
- The two tables generators other scripts still read stay: `gen_options_tables` (`gen_options_runtime.py:24,68` parses its source) and `gen_sim_tables` (`gen_sim_runtime.py:6` imports `classes`). So does `gen_simsui_tables` (`gen_host_rt_tables.py:40` imports helpers). Each goes later, together with the script that reads it. Record this in deferred work.
- Never `rm -rf bin`. No staging or commits. Add the files to `~/openfs98-verify/story-files.md`.

**Never:**
- `gen_global_state.py` is not retired here. Its output is a dump to be decompiled, not committed as is.
- No other generator changes.

</frozen-after-approval>

## Code Map

- `cmake/steps/all.cmake` -- the generator lines and the step OUTPUT each forces:

  | Line | Generator | Step OUTPUT |
  |---|---|---|
  | 177 | `gen_multip_dispatch` | `bin/MULTIP.DLL` |
  | 178 | `gen_multip_weather` | `bin/MULTIP.DLL` |
  | 180 | `gen_multip_profile` | `bin/MULTIP.DLL` |
  | 289 | `gen_alerts_tables` | `bin/ALERTS.DLL` |
  | 302 | `gen_help_tables` | `bin/HELP.DLL` |
  | 328 | `gen_views_tables` | `bin/VIEWS.DLL` |
  | 341 | `gen_flights_tables` | `bin/FLIGHTS.DLL` |
  | 425 | `gen_sim1_tables` | `bin/SIM1.sim` |

  Re-read the lines first: slice 2 shifted them.
- Files to hash, besides each OUTPUT. Normalize the PE files as in slice 2; archive member mtimes and `.o` COFF stamps are 0.
  - Each step also writes files under `bin/<mod>-diff/`: `<MOD>_ours.dll`, `<mod>.res`, `<mod>-res.o`, `lib<mod>-simsui.a` and `simsui-anchor.def`, whichever exist.
  - MULTIP also writes `bin/multip.res`.
  - SIM1 also writes `bin/sim1-native/SIM1_ours.sim`, `sim1.res` and `sim1-res.o`, plus `sim1_data.inc` and `sim1_rdata.inc` (`gen_sim1_native.py:82-84`), which the SIM1 compile reads through `-Ibin/sim1-native`.
- Difftest builds that compile these tables, outside `tools/ofs all` (`CMakeLists.txt:24-28`):
  - `bin/sim1-diff/sim1rdiff.exe` (`difftests.cmake:359`), the only consumer of `sim1_tables.inc`, since nothing in `src/sim1` includes it;
  - `helpeventsdiff.exe` (`:292`), `viewseventsdiff.exe` (`:393`), `flightseventsdiff.exe` (`:246`) and `bin/alerts-diff/alertseventsdiff.exe` (`:6-9`, through `src/alerts/alerts_events.c:6`).

  Hash all five, then force them by deleting them and running `tools/ofs difftests`. The multip harness and the `*uidiff` exes compile none of the eleven outputs.
- Expect extra reruns. The header edits put every step and difftest that names a file in those `src/` directories out of date. Steps are chained by target and rerun only when out of date, so this costs only build time.
- The steps also rewrite other tracked files: the `*_runtime_tables.inc` of alerts, help, views and flights, and the MULTIP files from `gen_multip_data.py`. Save the `git diff` over all of `src/`, not only the eleven outputs.
- Tracked outputs:
  - `src/multip/multip_dispatch_cases.inc`, `multip_weather_fields.inc`, `multip_profile_fields.inc`. `gen_multip_dispatch.py` also reads `src/multip/multip.c` and `tools/difftest/multip_send.inc`, but does not write them.
  - `src/alerts/alerts_tables.inc`
  - `src/help/help_tables.inc`, `help_objects_tables.inc`
  - `src/views/views_tables.inc`, `views_objects_tables.inc`
  - `src/flights/flights_tables.inc`, `flights_objects_tables.inc`
  - `src/sim1/sim1_tables.inc`
- If an output is itself an untyped byte or dword dump (check (a) flags it), leave that generator alone and report it.
- Comments that name the retired generators: `gen_flights_runtime.py:16`, `gen_options_tables.py:7` ("as tools/gen_flights_tables.py does"), `src/sim1/sim1_internal.h:10`, `src/flights/flights_objects.h:30`, and `docs/modules/sim1.md:557` (`CRT_START`). Reword them.
- `cmake/OpenFS98.cmake:2-5` says only World's `.inc` files are plain sources, and `deferred-work.md:20` holds the matching shortcut entry. Update both for these modules.
- `cmake/targets.cmake:1850` -- `ofs_target(sim1-tables …)` has no commands. `docs/tools.md:29` says it "regenerates the routine contracts". Delete the target and fix that sentence.
- `tools/check_raw_dumps.py` rule1/rule2 -- strip a trailing `\` (and whitespace) from each line before matching. Then add `src/global/global_state.inc` to the allowlist, tagged `(a)`. This is a one-time addition, of a dump that was there all along and is only now detected. Reword the allowlist header ("may only be removed") and `docs/tools.md:230` ("may only shrink") to allow an entry newly detected by a stricter rule, with the reason on its line. Add an inline assert for the continued-macro form, with 130 one-per-line `[x] = 0x.., \` lines: a 10-line block is below both rules' thresholds. The reviewer's simulation shows this change flags only `global_state.inc`.
- `tools/raw_dumps_allowed.txt` -- remove the eight `gen:` entries and add `global_state.inc`. Update the `docs/tools.md` Baselines line (~230) to print "34 allowed dump files, 61 scripts reading orig/ (45 gen, 1 resource, 15 oracle)".
- Docs that name the generators: `docs/modules/{alerts,flights,help,views,multip,sim1}.md`. Say the outputs are committed and the generator is retired; don't rewrite unrelated text.

## Tasks & Acceptance

**Execution:**
- [x] Record normalized hashes of the affected step outputs, and check that each generator reproduces its tracked output in a temp copy.
- [x] `tools/check_raw_dumps.py`, `tools/raw_dumps_allowed.txt` -- the trailing-`\` fix, `global_state.inc`, and the eight removals.
- [x] Delete the eight generators and their `all.cmake` lines, and the `sim1-tables` target. Reword the output headers and the `gen_flights_runtime.py:16` comment.
- [x] Docs, plus `deferred-work.md`: the three kept table generators and why, and `global_state.inc` as a dump to decompile.
- [x] Force each affected step by deleting its OUTPUT and the five difftest exes, rebuild with `tools/ofs all` and `tools/ofs difftests`, compare the hashes, and check that the saved `git diff -- src` is unchanged.

**Acceptance Criteria:**
- Given the record, when the forced rebuild finishes, then every normalized hash matches.
- Given the tree, when `check_raw_dumps.py` runs, then it exits 0. None of the eight appears in the allowlist, and `global_state.inc` is listed as a dump file.
- Given a temp git copy with `orig` symlinked, when 130 `\`-continued designated byte lines (the `global_state.inc` form) are added to a tracked `src/` file, then the check exits 1.

## Implementation Notes

- The subagent recorded normalized hashes of 39 files before any edit: the six step outputs, the `bin/<mod>-diff` files, `bin/multip.res`, `bin/sim1-native/*` and the five difftest exes. After the change it deleted the outputs and exes and rebuilt with `tools/ofs all` and `tools/ofs difftests`: all 39 match. `git diff -- src` is the same before and after the rebuild. Each of the eight generators reproduced its tracked output in a temp copy.
- Checked independently against the build in `~/openfs98-verify/s14` (before slices 2 and 3): the six module outputs, `SIM1_ours.sim`, the five difftest exes, `bin/multip.res` and the three `bin/sim1-native` support files are all identical (16 compared). The review's comment fix in `flights_objects.h` leaves FLIGHTS.DLL identical too.
- `check_raw_dumps.py`: 34 allowed dump files, 61 scripts reading orig/ (45 gen, 1 resource, 15 oracle). A temp git copy with 130 `\`-continued designated byte lines added exits 1.

## Spec Change Log

## Review Triage Log

| # | Finding | Verdict | Evidence / route |
|---|---|---|---|
| 1 | flights_objects.h points to the wrong file for s_classes (BH) | low | `s_classes` is in `flights_objects_tables.inc`. Patched; FLIGHTS.DLL is still identical. |
| 2 | help.md sentence broken by "metadata;" + "The" (BH) | low | Patched. |
| 3 | multip.md "Neither generator embeds executable code" (BH) | low | One of the two is retired. Patched. |
| 4 | The retail cross-checks go with the generators; nothing validates the committed tables (BH, VG, EC) | false | By design: the committed C is the source now. A different pressing no longer changes it, which is the point of Story 1.4. The modules' difftests compare behaviour against retail. |
| 5 | Provenance lost (no commit or image recorded) (BH) | low | The retired scripts stay in git history once committed, and the headers name them. No hash exists to cite before the commit. Rejected. |
| 6 | No hash evidence attached (BH) | low | Recorded in Implementation Notes above. |
| 7 | Rule 2's backslash branch is untested; a `\` before a `//` comment is mishandled (BH, EC) | false | The 130-line assert has one literal per line, so it can only trip rule 2. The comment is stripped before `rstrip()` and the `\` test. |
| 8 | A `\` inside a string or comment is stripped (EC) | low | No effect on today's tree. Rejected. |
| 9 | .gitignore / other references not checked (BH) | false | The outputs were already tracked. VG's repo-wide grep finds only retired-wording mentions. |
| 10 | The allowlist exception is open-ended (BH) | low | The exception needs a reason on the entry's line, and review sees it. Rejected. |
| 11 | deferred-work entries thin, line refs will rot (BH) | low | Rejected. |
| 12 | Headers lack a "hand-maintained" signal (BH) | low | Rejected: they say "now committed C". |
| 13 | Remaining doc awkwardness: long lines, "was set to", "(now committed)" (BH) | low | Cosmetic. Rejected. |
| 14 | docs/tools.md cell no longer parses (BH) | false | Line 29 is a single valid table row. |

## Verification

**Commands:**
- The normalized-hash comparison -- expected: no difference.
- `python3 tools/check_raw_dumps.py` -- expected: exit 0, and the negative case exits 1.
- The modules' break suites compile the same committed files. With the step outputs and the five difftest exes identical, the files stand in for those runs.
