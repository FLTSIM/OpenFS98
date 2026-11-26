---
title: 'Cleanup 4a: raw-data guards (allowlist ratchet, wrapper scripts, break-line classification)'
type: 'chore'
created: '2026-11-25'
status: 'done'
route: 'dispatch'
baseline_commit: '72192c1492d2a6ebd755b88ac371ffba19c7804f'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-cleanup-2-tooling-guards.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** three open `docs/implementation/deferred-work.md` entries, about checks that can pass when they shouldn't.
- **(R1)** Nothing stops a new entry from being added to `tools/raw_dumps_allowed.txt`. Its header says dump-file and `gen:` entries "may only be removed", but nothing enforces it.
- **(R3)** Check (c) of `tools/check_raw_dumps.py` sees only the `tools/<name>.py` scripts named in `cmake/steps/*.cmake` and `cmake/targets.cmake`. Scripts that CMake reaches through the 25 `tools/*.sh` wrappers are not seen. The spec review found 81 newly visible Python entry points, and 10 of them read `orig/` with no allowlist entry: `world_game.py`, and the nine in `run_mpdialogdiff.sh` that import `mpdialog_provenance.py`.
- **(R4)** `cmake_verify.py`'s `break_verdict` treats any command containing `breaks` as a break suite.

**Split (approved at Checkpoint 1, 2026-11-25):** the image-extent check (R2, `check_data_image.py` not checking our image's own size) moves to its own later slice, 4b. The spec review showed that COFF symbols cannot give object extents: alignment, aliases and intentional sub-windows would fail 147 of 161 rows. It also showed that editing `check_data_image.py` or `tools/data_images.txt` reruns all 30 hash-bearing build recipes. R2's deferred entry gains these findings.

## Boundaries & Constraints

**Always:**
- **R1, the ratchet (`tools/check_raw_dumps.py`):**
  - **The base** is `HEAD`'s allowlist (`git show HEAD:tools/raw_dumps_allowed.txt`), read from the repository whose top level is the tool's own root (`git rev-parse --show-toplevel` must equal it).
  - **No base:** when there is no `HEAD`, or `HEAD` has no allowlist, the ratchet is skipped with a printed note. Any other git error fails.
  - **What it guards:** pending edits, not history. The notes say so.
  - **Added entries:** a dump-file entry or a `gen:` entry whose key is not in the base fails: `allowlist entry added (dump-file and gen: entries may only be removed): <entry>`. A rename is an added key.
  - **The stricter-rule exception:** a new dump-file entry passes only when all of these hold:
    - its file exists at `HEAD` with the same blob;
    - `tools/check_raw_dumps.py` differs from `HEAD`, so a stricter rule is pending in the same change;
    - the entry's reason starts with `stricter rule:`;
    - `HEAD`'s detector does not flag the file. To show it, run `HEAD`'s `tools/check_raw_dumps.py` rule functions on the file's text: extract them from the `HEAD` blob, or keep the rules in a small importable module so `HEAD`'s copy can be loaded. The implementer picks one and says which.
  - **New `gen:` entries** are never allowed. `resource:` and `oracle:` entries are not ratcheted.
  - **Parse checks:** a duplicate key fails, where today a later one silently overwrites the reason. On a dump-file entry, the reason tag must be `(a)`, `(b)` or `stricter rule:`.
  - The success line is unchanged.
- **R3, wrappers (`tools/check_raw_dumps.py`):**
  - `cmake_scripts()` follows executable invocations: from each `tools/<name>.sh` named in a scanned CMake file, the lines that run `python3 [options] tools/x.py`, `"$D"/x.py`, `$(dirname "$0")/x.py` and the like, or that run another `tools/*.sh`. It recurses, visiting a cycle once.
  - **The `orig` test** for a script reached this way is its own invocation line (its arguments) plus its source and imports, as today. It is not the whole wrapper's text, so other commands, branches and comments in a shared wrapper do not count.
  - **New entries:** each newly seen script that reads `orig/` gets an allowlist entry with its reason. The review expects `oracle:` for all 10 named above; the implementer confirms each by reading it. A `gen:` entry would mean it feeds module data into a build, and that is stopped and reported. These are R1-exempt by kind (`oracle:` and `resource:` are not ratcheted).
  - **The Baselines expectation** for `check_raw_dumps.py` (`docs/tools.md:308`, "38 scripts reading orig/ (1 gen, 17 resource, 20 oracle)") is updated to the new counts. This is the one Baselines edit allowed.
- **R4, the break-line classification (`tools/cmake_verify.py`):**
  - `is_break(cmd)` is true when the target that `plan()` parses (shared with `plan()`, `make -f` included) ends in `-breaks`, or when the executed script is `tools/<name>_breaks.py` or `.sh`.
  - The executed script is the first word for a script line. For `python3`, it is the first word after its options: skip `-B`, `-u`, `-I` and the like; `-m` and `-c` mean no script, so the line is not a break.
  - `break_verdict` uses `is_break`, and the rest of the rule is unchanged.
  - **Equivalence:** a self-test-time check (or a recorded proof) shows that `is_break` and the old test select the same 60 of the 179 current Baselines lines.
  - Asserts cover `python3 tools/check.py src/breaks/x.c`, `echo breaks`, `tools/ofs graph BREAKS=1`, `python3 -m x_breaks` and `make -f X.mk foo-breaks`.
- **Self-tests:** each tool's asserts or `--self-test` cover its new rule on synthetic inputs:
  - R1: an added key, a removed key, a reason-only edit, a duplicate, the stricter-rule exception with each of its conditions missing in turn, and no `HEAD`;
  - R3: a two-level wrapper, a cycle, and a shared wrapper whose sibling branch mentions `orig`;
  - R4: the cases above.
- **Proof (in the notes):**
  - **R1:** in a scratch clone at `HEAD`:
    - adding a `gen:` line fails;
    - adding a dump-file line for a new file fails;
    - a reason-only edit passes;
    - a pending stricter `RUN` that newly catches an unchanged tracked file passes once its `stricter rule:` entry is added, and fails without the detector change.
  - **R3:**
    - the list of newly seen scripts and the entries added;
    - a scratch wrapper calling an `orig/`-reading script without an entry fails;
    - a sibling branch's `orig` mention does not flag its neighbour.
  - **R4:** the equivalence, and `cmake_verify.py --self-test`.
  - **Rebuilds:** none of the edited files is a build dependency. The notes confirm it with a warm `openfs98` build under `OFS_STEP_LOG`, which must log no step.
- **Docs and paperwork:**
  - `docs/tools.md`: the tool rows and the one Baselines expectation;
  - the allowlist header, if its wording changes;
  - `deferred-work.md`:
    - R1, R3 and R4 move to `deferred-work-resolved.md`;
    - R2's entry stays, gains the spec review's findings below, and is labelled for 4b.

    The R2 findings to record:
    - symbols cannot give extents: alignment, aliases (`_Linkage` inside `winmsg_image`), intentional sub-windows (NUMERIC's `0x321` of `0x400`, SIMSUI);
    - use measured sizes or end labels and per-row declared sub-windows and padding;
    - define scope for `tail=`, `.rdata`, `switch` and `bound`;
    - a manifest bootstrap and verification contract for `layout=`;
    - editing `check_data_image.py` or `data_images.txt` reruns 30 recipes and configure, with PE timestamps;
    - mutant proofs must get past compile-time asserts and the existing hash.
- **Running:** in a worktree `~/source/OpenFS98-cl4` off HEAD, with `orig/` copied in. Builds use `nice -n 19` and `-j 2`. No wine. Slice 6b0 is running at the same time and edits break runners and `docs/tools.md`; this slice touches no runner.

**Never:**
- No change to `tools/check_data_image.py`, `tools/data_images.txt`, any build recipe, any module source, or any build output.
- No change to any break runner. No Baselines change beyond the one expectation.
- No loosening of an existing check.
- No commits or staging.

</frozen-after-approval>

## Code Map

- **`tools/check_raw_dumps.py`:**
  - `:63`: `git ls-files`;
  - `:58-82`: `tools_imports`, `reads_orig` and `cmake_scripts`;
  - `:84-116`: the allowlist parse (entries overwrite on a duplicate key) and the verdicts.
- **`tools/raw_dumps_allowed.txt:1-6`:** the header rules.
- **Wrappers:**
  - `cmake/targets.cmake`: the 25 `tools/*.sh` references;
  - `tools/run_mpdialogdiff.sh:18`, `tools/mpdialog_provenance.py:65`;
  - `tools/world_game.py:26`;
  - `tools/run_worlddiff.sh`, which leads to 51 entry points.
- **`tools/cmake_verify.py`:** `plan` (about `:87`), `break_verdict` (about `:127`), the Baselines parser (about `:899`).
- **`docs/tools.md:308`:** the `check_raw_dumps` Baselines line.

## Tasks & Acceptance

**Execution:**
- [ ] The worktree. R4 with its equivalence. R3, the new entries and the count. R1.
- [ ] The proofs, the docs, the deferred-work moves and the R2 entry update, `## Implementation Notes`.

**Acceptance Criteria:**
- The self-tests pass.
- The proofs behave as listed.
- `check_raw_dumps.py` passes with only R3's additions.
- `is_break` matches the old selection on all 179 lines.
- The warm build logs no step.
- Only `tools/check_raw_dumps.py` (plus a rules module if R1 needs one), `tools/raw_dumps_allowed.txt` (R3 entries), `tools/cmake_verify.py`, `docs/tools.md`, the deferred-work files and this spec changed.

## Verification

**Commands:**
- `python3 tools/check_raw_dumps.py` -- expected: `raw dumps: 2 allowed dump files, N scripts reading orig/ (...), none new`, where N is 38 plus R3's additions.
- `python3 -B tools/cmake_verify.py --self-test` -- expected: exit 0.

## Review Triage Log

Spec review (Codex gpt-6.2-sol, 2026-11-25), on the first draft (R1–R4):

| # | Finding | Verdict | Action |
|---|---|---|---|
| 1 | The R2 symbol-extent rule would reject 147 of 161 rows (alignment, aliases, sub-windows) | blocker | R2 moved to 4b with the findings. |
| 2 | A layout digest cannot establish extent; auxiliary windows are unscoped | high | Moved to 4b. |
| 3 | R2 has no manifest bootstrap or verification contract | high | Moved to 4b. |
| 4 | R1's exception admits any unchanged HEAD file; renames, reason edits, duplicates | high | The exception needs a pending detector change, a `stricter rule:` reason, and HEAD's detector not flagging the file; duplicates fail; tags validated. |
| 5 | R1's no-git behaviour; parent repos; HEAD without the allowlist | medium | The repo root is checked; no-base cases skip with a note; other git errors fail. |
| 6 | R3: 81 new entry points, 10 missing entries; whole-wrapper text gives false positives | high | Invocation-level attribution; 10 `oracle:` entries expected; the Baselines count updated. |
| 7 | R4 matches 60 of 179; share `plan`'s parsing; python options | low | Shared parsing, option handling, an equivalence check. |
| 8 | Editing the R2 files reruns 30 recipes | high | Moved to 4b; 4a edits no build dependency, proved with a warm build. |
| 9 | R2's mutant proofs could pass for the wrong reason | high | Moved to 4b. |

Code review (Codex gpt-6.2-sol, three layers, 2026-11-25):

| # | Finding | Verdict | Action |
|---|---|---|---|
| 1 | The wrapper scan misses quoted, braced, optioned and sourced forms, and attributes orig across commands and comments | high | Shell-aware parsing per command; orig tested on the command's own arguments; simple assignments resolved. |
| 2 | gen: can be relabelled to oracle:; loose key matching | high | gen: cannot change category; exact keys. |
| 3 | HEAD's detector executed in-process (sys.exit, shape changes) | high | Subprocess; fail closed. |
| 4 | Root mismatch and corrupt HEAD skip the ratchet | high | Both fail; only a genuinely unborn HEAD skips; fixture added. |
| 5 | The exception accepted (b)-only files; a missing file crashed | high | The current text rule must flag it; early rejection. |
| 6 | is_break option parsing; IndexError on short argv | medium | Options handled; short argv False. |
| 7 | The rename self-test did not remove the old key | low | Fixed. |
| 8 | Tool rows and the discovered-script inventory missing | low | Added. |
| 9 | Checking that new oracle:/resource: entries qualify; xargs or eval invocations | rejected | Noted as limits. |

Follow-up review (Codex gpt-6.2-sol, 2026-11-25):

| # | Finding | Verdict | Action |
|---|---|---|---|
| 1 | A dangling HEAD symref was read as unborn (reproduced) | high | `--no-recurse` and a no-commits check; otherwise fail. |
| 2 | gen: escapes by rename plus relabel | high | The category is carried across renames. |
| 3 | `<<<` read as a here-doc; stn_compare.py missed (the 81st) | medium | Here-string handled; fixture; inventory 81. |
| 4 | Refusing GIT_* env vars breaks pre-commit hooks | medium | They are stripped from git subprocesses; the root is validated. |
| 5 | `__file__` prepended breaks `from __future__` | low | Passed in the globals instead. |

## Implementation Notes

Worktree `~/source/OpenFS98-cl4` (detached at 72192c1), `orig/` copied in. Nothing staged or committed.

- **R4 (`tools/cmake_verify.py`):** `make_target(words)` is the parse `plan()` and the new `is_break(cmd)` share. `is_break` is true for a `-breaks` target, or an executed `tools/<name>_breaks.py|.sh` (first word, or the first word after `python3`'s options; `-m`/`-c` mean no script). `break_verdict` uses it. Module-level asserts cover the five listed cases and more. `--self-test` parses the live Baselines and asserts `is_break` and the old `'breaks' in cmd` select the same lines: 60 of 179.
- **R3 (`tools/check_raw_dumps.py`):** `wrapper_scripts()` reads a `tools/<name>.sh` named in a scanned CMake file, skips comment lines, joins continuations, takes `python3 [opts] tools|"$D"|$(dirname "$0")/x.py`, `exec` forms and other `tools/*.sh` runs, and recurses with a shared `seen` set. The `orig` test for such a script is its own wrapper line plus its source and imports. Newly seen readers of `orig/`: exactly the 10 expected (`world_game.py`, `mpdialog_provenance.py` and the 8 other `mpdialog_*` runners that import it). I read `world_game.py:26,455` (hashes retail's FLTSIM98.EXE and MAIN.DLL) and `mpdialog_provenance.py:65` (hashes retail's mpdialog.dll); the others are break/smoke runners with no build output. All 10 are `oracle:`, none `gen:`. Baselines count now 48 scripts (1 gen, 17 resource, 30 oracle), updated at `docs/tools.md` (the one expectation).
- **R1 (ratchet):** HEAD's detector is taken by extracting the rules part of HEAD's `tools/check_raw_dumps.py` (everything before `def sha(`) and `exec`ing it; a rules module was not possible because HEAD has none. The marker comment above `def sha(` says to keep it first. Base: `git rev-parse --show-toplevel` must equal the tool's root, else skipped with a note; no HEAD or no allowlist at HEAD skips with a note; other git failures exit. Duplicate keys and a dump-file reason without `(a)`, `(b)` or `stricter rule:` fail. Exception conditions are in `stricter_ok()`, fed by `real_stricter()` (same blob at HEAD, `git diff --quiet HEAD` on the tool, HEAD's `is_dump`). `--self-test` covers added, removed, reason-only, duplicate, rename, the exception with each condition missing, wrappers (two levels, a cycle, a sibling branch mentioning orig). "No HEAD" is covered by the pure code path only, not by a self-test fixture.
- **Proofs (scratch clone at HEAD, new tool and allowlist copied in):** a new `gen:` line fails; a dump-file line for a new file fails; a reason-only edit passes; with `RUN, PER_LINE = 4, 4` five tracked files are newly flagged, all pass with `stricter rule:` entries (7 allowed dump files), fail with the tag removed (parse check plus ratchet), and fail as stale once the detector change is gone. A scratch wrapper calling an unlisted `orig/`-reading script fails, and its sibling branch's script is not flagged. The old-HEAD-tool "fails without the detector change" leg was shown as stale entries rather than the exception condition (the new tool always differs from HEAD in this change); the missing-condition cases are in the self-test.
- **Rebuilds:** configure plus `openfs98` build, then a warm rebuild under `OFS_STEP_LOG`: 0 steps logged. `check_raw_dumps.py`, `--self-test` and `cmake_verify.py --self-test` pass.
- **Paperwork:** R1 and R3 and R4 entries moved to `deferred-work-resolved.md`; R2's entry stays in `deferred-work.md` as 4b with the review findings; allowlist header notes the enforcement.
- **Not done / risk:** R2/4b is untouched. Only `python3`-prefixed, `exec`, and `$D`/`dirname` forms in wrappers are followed; a script run another way (a variable interpreter, `xargs`) would be missed. Direct `tools/x.py` invocation without python is not matched.

### After code review (2026-11-25)

- **Wrapper parser:** `shell_commands()` (shlex with punctuation, backslash-newline removed, here-doc bodies and comments dropped, split on `; & && || | ( )`), `python_script()` (options incl. `-W`/`-X` values, `-c`/`-m`/`-` mean no script, `--`), `wrapper_scripts()` (quoted paths, `$D`, `${D}`, `$(dirname "$0")`, direct `tools/x.py|sh`, `exec`, `nice`, `bash`, `source`/`.`; simple literal assignments resolved; wrappers visited once). The `orig` test uses only that command's own arguments, plus source and imports. Known limits: a `#` inside a word and an unterminated quote end the scan of that wrapper; `xargs`, a variable interpreter and `eval` are not followed; a new `oracle:`/`resource:` entry is not checked for really being one (review judgement).
- **Ratchet:** a `gen:` script cannot be relabelled; script keys must be exactly `<name>.py` (shared by the category and stale checks); HEAD's rules run in a subprocess with a timeout and fail closed; the tool's root must equal `git rev-parse --show-toplevel` (realpaths) or it fails; only an unborn HEAD (symbolic ref that does not exist) or HEAD without the allowlist skips; `GIT_DIR`, `GIT_WORK_TREE`, `GIT_INDEX_FILE` set fails; the exception also needs the current rule to flag the file, and a missing file is rejected early. Self-test has unborn, root-mismatch and corrupt-HEAD fixtures.
- **`is_break`:** python options with values, attached/bundled `-c`/`-m`, `--`, `-`, `./tools/x_breaks.py` normalised; empty or short argv (``, `tools/ofs`, `make -f rules.mk`) is False. Still 60 of 179.
- **Proofs (scratch clone with the new tool committed as HEAD):** an entry for an unchanged file with the checker identical to HEAD fails as added; relabelling `gen:gen_window_data.py` to `oracle:` fails; `oracle:foo` fails as malformed; scratch wrapper cases (`python3 a.py; cp orig/x y` not flagged, `"$r"` assigned to orig flagged, comment and `-cpass` not flagged) behave as intended; `GIT_WORK_TREE` set fails; the tool in a subdirectory of the repo fails on the top-level check; the stricter-rule proof (five files, `RUN, PER_LINE = 4, 4`) passes with `stricter rule:` entries. Warm build again logs 0 steps; both self-tests and `check_raw_dumps.py` pass (48 scripts, 1 gen, 17 resource, 30 oracle).
- **Wrapper-reached inventory (final, 81):** the parser finds 140 scripts run by CMake, 59 directly and 81 only through wrappers (stn_compare.py, behind flights_play.sh's `<<<` here-string, was the 81st); the reader count stays 48. The list, to diff against: alerts_breaks.py, alerts_object_breaks.py, alerts_smoke.py, alerts_ui_breaks.py, check_gauges.py, flights_breaks.py, gauges_breaks.py, help_breaks.py, help_smoke.py, help_ui_breaks.py, mdl_crash_fixture.py, mpchat_ui_breaks.py, mpdialog_common_breaks.py, mpdialog_construction_breaks.py, mpdialog_create_failure_breaks.py, mpdialog_exception_breaks.py, mpdialog_provenance.py, mpdialog_retained_breaks.py, mpdialog_review_breaks.py, mpdialog_smoke.py, mpdialog_ui_breaks.py, options_breaks.py, sim_breaks.py, sim_smoke.py, sim_ui_breaks.py, simsui_breaks.py, stn_compare.py, views_breaks.py, views_smoke.py, views_ui_breaks.py, world_advanced_core_breaks.py, world_advanced_life_breaks.py, world_advanced_native_breaks.py, world_airport_breaks.py, world_airport_life_breaks.py, world_airport_native_breaks.py, world_booster_breaks.py, world_booster_life_breaks.py, world_booster_native_breaks.py, world_breaks.py, world_cache_breaks.py, world_cache_life_breaks.py, world_cache_native_breaks.py, world_complexity_breaks.py, world_complexity_init_breaks.py, world_complexity_life_breaks.py, world_complexity_native_breaks.py, world_copy_breaks.py, world_copy_life_breaks.py, world_copy_native_breaks.py, world_dynamic_breaks.py, world_dynamic_init_breaks.py, world_dynamic_life_breaks.py, world_dynamic_native_breaks.py, world_facilities_breaks.py, world_facilities_life_breaks.py, world_facilities_native_breaks.py, world_files_breaks.py, world_files_life_breaks.py, world_files_native_breaks.py, world_game.py, world_init_breaks.py, world_life_breaks.py, world_list_breaks.py, world_list_life_breaks.py, world_list_native_breaks.py, world_position_breaks.py, world_position_life_breaks.py, world_position_native_breaks.py, world_priority_breaks.py, world_search_breaks.py, world_search_init_breaks.py, world_search_life_breaks.py, world_search_native_breaks.py, world_smoke.py, world_time_breaks.py, world_time_native_breaks.py, world_ui_breaks.py, world_wareas_breaks.py, world_weather_breaks.py, world_weather_native_breaks.py.

### Follow-up review fixes

- Dangling symref: unborn only when `symbolic-ref --no-recurse` names a ref that `show-ref` says is absent and `rev-list -n1 --all` is empty; else fail. Rename carry: `head_renames()` builds a temporary index (`read-tree HEAD`, `add -A tools`, `diff -M --name-status --cached`; the real index is untouched) and a relabelled or renamed `gen:` script fails. Here-string `<<<` is no longer a here-doc (fixture after `read -r x <<<"$y"`). All git calls run `git -C <root>` with `GIT_DIR`, `GIT_WORK_TREE` and `GIT_INDEX_FILE` stripped; the top level is still validated. HEAD's rules are compiled unchanged from a temporary file and `__file__` is passed in the globals (`rules_flags`, tested with `from __future__` and a prefix that calls `sys.exit(0)`).
- Proofs (scratch clone, new tool committed): under `GIT_DIR` and `GIT_INDEX_FILE` set to a bogus index the check passes, and with an added `gen:` entry it still fails (the ratchet runs); `git mv` of `gen_window_data.py` with CMake updated and `oracle:` for the new name fails as relabelled or renamed.
