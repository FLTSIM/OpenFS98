---
title: 'Story 1.5, slice 1: the retail-bug marker check, and MAIN, Flights, Options and Gauges marked'
type: 'chore'
created: '2026-11-23'
status: 'done'
route: 'dispatch'
baseline_commit: '71be387e40884677de3a30d00fab3fccf6e09875' # worktree holds uncommitted Stories 1.2-1.4
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** `docs/retail-bugs.md` lists 331 retail bugs (the epic's 289 predates later additions), but only 50 ids are marked `RETAIL BUG (kept): <id>` in `src/` (63 markers). The rest carry the module doc's account and no marker, and nothing checks that markers and register agree, so Epic 4 has no reliable map.

**Approach:** Like Story 1.4's slice 1, this slice adds a two-way consistency check whose allowlist of unmarked ids starts with every current gap and may only shrink. It then marks and checks every entry of the register sections already partly marked: MAIN.DLL, Flights.DLL, Options.DLL and Gauges. The scope is those sections' entries, wherever their code lives (FLT-6's site in `src/advdrv/`, PANEL-ZOOM-ADDRESS's in `src/panels/`). Later slices take the other sections until the allowlist is empty; Story 1.5 stays in progress until then.

## Boundaries & Constraints

**Always:**
- Markers are comments only: no code, data or build change. Every built PE (every `.DLL`, `.EXE`, `.GAU`, `.sim` and `.MDL` under `bin/`, harness exes included) is byte-identical to before: stripped and unstripped, the `peh2.py` hash lists of before and after must `diff` empty, and a file missing on either side fails.
- Marking an entry means reading the code it names and confirming the entry: correct its Where column (retail VA, file) where it is wrong, and put the marker at each site the entry names.
- Entry status comes from the Id cell's annotation, and sets what the check requires:

  | Id cell | Status | Requires |
  |---|---|---|
  | `ID` | kept | a `(kept)` marker, or allowlisted |
  | `ID (suspected)`, `ID (suspected, not used)` | suspected | a `(kept, suspected)` marker, or allowlisted |
  | `ID (fixed, not kept)` | fixed on purpose by us | a `(fixed, not kept)` marker at the fix, or allowlisted |
  | `ID (no longer applies)` | obsolete: the code no longer has the site (deleted, or retail's behaviour proven otherwise) | no marker, not allowlisted, one sentence why in the entry |

  An entry whose bug our code does not reproduce (e.g. OPT-4's larger buffer, `src/options/options_logbook.c:189`) is `(fixed, not kept)` if fixed on purpose, otherwise a finding to report, not a status to pick.
- Markers: `RETAIL BUG (<kind>): ID` inside a comment (`/* */`, `//`, or `#`/`;` in assembly), exactly one id after each prefix, which may wrap onto the comment's next line; a trailing `.` or `,` is not part of the id. A comment may hold several markers. An id in ordinary prose is not a marker.
- The allowlist names each unmarked id once; a listed id that is now marked fails the check, so a finished section must leave it.
- Heavy runs wait until the Story 1.4 rerun in `~/openfs98-verify/p14-rerun` has finished; builds before then run under `nice -n 19` with at most 2 jobs.
- No staging or commits.

**Never:**
- No marker in `tools/` counts, and the harness break lists in `tools/*_breaks.py` are not edited.
- No section beyond the four named here is marked in this slice.
- No retail bug is fixed.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|----------|--------------|---------------------------|----------------|
| Consistent | every register id marked or allowlisted, every marker known | `retail bugs: N entries, M marked, K allowlisted, L no longer apply`, exit 0 | — |
| Unknown marker | `RETAIL BUG (kept): OPT-99` in `src/` | names file:line and the id | exit 1 |
| Unmarked | an id neither marked nor allowlisted | names the id | exit 1 |
| Stale allowlist | an allowlisted id now marked, or not in the register | names the id | exit 1 |
| Wrong kind | `(kept)` marker for a `(suspected)` or `(fixed, not kept)` entry | names file:line, both kinds | exit 1 |
| No longer applies | entry with `(no longer applies)` that still has a marker | names it | exit 1 |
| Wrapped / trailing dot | `RETAIL BUG (kept):\n * FLT-3.` | read as `FLT-3` | — |
| Prefix collision | `RETAIL BUG (kept): MAIN-10` only | `MAIN-1` still unmarked | exit 1 if MAIN-1 not allowlisted |
| Prose only | `/* see MAIN-6, MAIN-18 */` | not markers | — |
| Not in a comment | marker text in a string literal or code | ignored, named as a warning | — |
| Malformed | `RETAIL BUG (kept):` with no id, or `RETAIL BUG (kept): MAIN-6, MAIN-18` | names file:line | exit 1 |
| Two in one comment | `RETAIL BUG (kept): SS-1 ... RETAIL BUG (kept): SS-2` | both read | — |
| Obsolete allowlisted | `(no longer applies)` id in the allowlist | names it | exit 1 |
| Duplicate id | an id twice in the register | names it | exit 1 |

</frozen-after-approval>

## Code Map

- `docs/retail-bugs.md` -- one `## Module` section per module, each a table `| Id | Where | What retail does | When | Fix at porting |`, 331 rows. Id-cell annotations today: 6 ` (suspected)`, 1 ` (suspected, not used)` (WXM-8), 5 ` (fixed, not kept)`. An unknown annotation fails the check. Its preamble (lines 14-24) says the entries from "FLTSIM98.EXE services" on, MAIN-2 on and FLT-4 on are not yet marked; reword it to point at the check.
- Existing marker variants in `src/`: `RETAIL BUG (kept): ID`, `RETAIL BUG (kept, suspected): ID` (`src/sim1/sim1_small.c:1639`, `src/options/options_objects.c:163`), markers wrapped across lines (`src/simsui/simsui_edit.c:70`, `src/main/main_logbook.inc:894`), and trailing dots (`FLT-3.`, `OPT-2.`). Normalise any marker whose kind disagrees with its entry.
- Unmarked in this slice: MAIN-2..MAIN-25 (`src/main/`), FLT-4..FLT-6 (`src/flights/`), and whatever the check reports for Options and Gauges. Module docs `docs/modules/main.md`, `flights.md`, `options.md`, `gauges.md` give each entry's offsets.
- `tools/check_raw_dumps.py` + `tools/raw_dumps_allowed.txt` -- the pattern to copy: shrink-only allowlist, a self-test, a Baselines script line (`docs/tools.md:283`).
- `/tmp/claude-1000/-home-steve-source/b2e3078b-1d77-426e-816f-2de2772a67dc/scratchpad/peh2.py` -- PE compare normalising timestamps and checksum.

## Tasks & Acceptance

**Execution:**
- [x] `tools/check_retail_bugs.py` (new) -- parse the register and every file under `src/` that git tracks or would track (`git ls-files -co --exclude-standard src`), apply the status table and the matrix, print the summary line; `--self-test` covers every matrix row on synthetic inputs.
- [x] `tools/retail_bugs_unmarked.txt` (new) -- one id per line, `#` comments; seed with every id the check finds unmarked, then remove this slice's as they are marked.
- [x] `src/` files holding the four sections' sites (mostly `src/main/`, `src/flights/`, `src/options/`, `src/gauges/`) -- add markers at each named site; normalise existing markers' kind (e.g. OPT-10's marker must become `(kept, suspected)`).
- [x] `docs/retail-bugs.md` -- correct Where cells found wrong, mark entries that no longer apply, reword the preamble.
- [x] `docs/tools.md` -- a Baselines script line for the check with its summary, next to the raw-dump line; mention it in `docs/retail-bugs.md`'s preamble.

**Acceptance Criteria:**
- Given the slice is done, when `python3 tools/check_retail_bugs.py` runs, then it exits 0 and no MAIN, Flights, Options or Gauges id is allowlisted.
- Given the builds before and after, when every DLL and EXE is compared with `peh2.py`, then all are identical.
- Given `python3 tools/check_retail_bugs.py --self-test`, then every matrix row passes.

## Implementation Notes

## Spec Change Log

## Review Triage Log

Spec review 1 (Codex):
1. high -- the identity gate named only DLL/EXE; now every PE under `bin/` incl. `.GAU`, `.sim`, `.MDL`, missing files fail.
2. medium -- `peh2.py` only prints hashes; now the two hash lists must `diff` empty. Its `_CSWTCH` renaming stays: a comments-only change cannot renumber switch tables, and the stripped copies carry no symbol names.
3. medium -- the count was 319/44; it is 331 rows, 50 ids marked. Annotations listed, unknown ones fail.
4. medium -- matrix rows added: prefix collision, prose, non-comment text, malformed, two markers per comment.
5. medium -- status table added; obsolete entries need no marker and may not be allowlisted; OPT-4-like cases are reported, not guessed.
6. medium -- SIM1 assigned a later slice; every seeded id belongs to one.
7. medium -- scope is the four sections' entries wherever their sites are.
8. medium -- the scan covers untracked, non-ignored `src/` files.
9. false -- an immutable baseline for the allowlist is not needed: like `raw_dumps_allowed.txt`, a deleted marker plus a re-added allowlist id is a visible diff in review.

Code review 1 (Codex: blind, edge case, verification gap). Found by me before it: the implementer dropped PANEL-ZOOM-ADDRESS-2 (a PANELS entry) from the allowlist, so the check failed; restored, 254 allowlisted.
- B1 medium, patch -- `main_nav.inc:105` MAIN-8 marked `(kept)` but ours returns `bearing`, not the caller's eax.
- B2 medium, patch -- `main_spot.inc:278` MAIN-15 `(kept)` on the +0xe60 step count its own comment says is not reproduced.
- B3 medium, patch -- `main_flights.inc:379` MAIN-17 `(kept)` on the empty-flight answer; ours returns 0.
- B4 medium, patch -- `main_kneeboard.inc:275` MAIN-24 `(kept)` on the RECT bottom ours sets. B1-B4 share a root (markers on non-reproduced parts): notes instead, and "Not reproduced" in the entries.
- B5 medium, patch -- `main_logbook.inc:11` MAIN-23 marker describes the file-read counts, another (non-reproduced) behaviour.
- B6 medium, patch -- MAIN-9's FE +0x20 state calls (`main_season.inc:89`, `main_radios.inc:351`) unmarked.
- E6 medium, patch -- FLT-6's MAIN sites (`main_flights.inc:392`, `:435`) unmarked though the Where cell names the file.
- B7/E4 medium, patch -- `register()` silently drops a malformed `| Id |` table row; its entry escapes the check.
- B8/E3 low, patch -- `.S` `#define` lines read as comments, so a string there counts as a marker; direct fix.
- B9 low, patch -- bare marker text in code gives no warning, against the matrix row.
- E1 false, carried -- shrink-only baseline: spec review 1 row 9.
- E2 low, rejected -- a `//` comment continued by backslash-newline before a marker: no such comment in `src/`, and the fix adds a splicing pass.
- E5 low, rejected -- `(no longer applies)` without a reason: none exist, review reads the sentence; a prose test adds guesswork.
- V1 low, rejected -- the non-zero exit is the one line `sys.exit(1 if errs else 0)`; a subprocess fixture repo adds more than it guards.

## Design Notes

Later slices, each its own spec, one shrink of the allowlist; every id the check seeds belongs to one of them: SIM1 (SIM1-AIR-FRAME-OVERRUN has only prose), HOST and the runtime; UTIL, CHAIN, NUMERIC; FACIL, FE; WINDOW, WINMSG, MPCHAT; REALITY, ANALYSIS, DEMO; ADVDRV, DYNAMIC, CONTROLS, SOUND; ATC, ACONTAIN, BOOSTER, WEATHERM, SLEW; PANELS, G3D, HG2D; MULTIP and the English dialogs. The last one deletes the allowlist file and makes the check refuse any allowlist.

## Verification

**Commands:**
- `python3 -B tools/check_retail_bugs.py --self-test && python3 -B tools/check_retail_bugs.py` -- expected: self-test passes; summary line, exit 0.
- Before editing, copy every PE under `bin/` aside; after, `nice -n 19 cmake --build build/win32-retail -j 2` (plus `tools/ofs difftests` and the gauge/MDL steps it covers), then `diff` the `peh2.py` hash lists of both sets, stripped and unstripped -- expected: empty diff, same file list.
