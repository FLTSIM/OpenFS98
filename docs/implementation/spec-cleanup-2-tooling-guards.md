---
title: 'Cleanup 2: tooling guards (modules/ofs test, codegen-override check, retail-bug checker gaps)'
type: 'chore'
created: '2026-11-25'
status: 'done'
route: 'dispatch'
baseline_commit: 'f15ee6a9ec0500f84dac1233348bfdedc0cb087f'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Three open `docs/implementation/deferred-work.md` entries are tooling guards that need no wine.
- **(A)** No regression test covers `tools/modules.sh`'s output or `tools/ofs`'s argument handling (entry at about line 11).
- **(B)** Nothing fails if a codegen override (`-fno-ipa-icf`, `-fno-tree-loop-distribute-patterns`, `-fno-tree-vectorize`, `-fno-math-errno`) is dropped from one of its sites. The table in `cmake/toolchains/win32-retail.cmake:19-32` is documentation only (about line 17).
- **(D)** `tools/check_retail_bugs.py` has six parsing gaps (about line 111).

**Approach:** One tool or fix per item, each with a self-test, no wine. The three entries move to `deferred-work-resolved.md`.

**Scope decided (2026-11-25):** the tree check after each break step (C, about line 97) and the recurring rebuild-edge check (E, about line 197) stay deferred. Their entries gain this spec's review findings. The superseded combined "Story 1.10b" entry (about line 181) moves to the resolved file, marked as split into 1.10b1, 1.10b2 and 1.10b3.

## Boundaries & Constraints

**Always:**
- **(A) `tools/check_modules_ofs.py`, with `--self-test`:**
  - **`tools/modules.sh`:** all eight query forms in use, compared against a committed golden file, `tools/modules_golden.txt`, generated once from the current tree. A one-line update command is documented.
    - the five CMake uses: `modules-of`, `english-modules-of`, `shared-modules-of`, `installed-path-of` and `orig-module-of` (`cmake/targets.cmake:159` and on);
    - the two candidate queries used by `tools/main_dialogs.sh:30`;
    - `installed-name-of`, used by `tools/reg_contracts.sh:45`.
    - The golden file covers case-sensitive mappings such as FACIL and MP_DIALOG (`tools/modules.txt:13,37`). `cmake/install.cmake:6` reads `modules.txt` on its own, so its parse of the same rows is compared too.
  - **`tools/ofs`:** it has no dry-run mode and never calls `modules.sh` (`tools/ofs:5`). The test runs it against a fake build directory (a `CMakeCache.txt` stub) with a `cmake` shim first on `PATH`, and asserts:
    - the argv it passes, the exported `VAR=value` values, and the target order;
    - that a shim-simulated unknown-target diagnostic and its exit status pass through unchanged;
    - the message for a missing build directory.
  - Nothing is built.
- **(B) `tools/check_codegen_overrides.py`**, run on every build like the arena lint, by the same wiring paths (`ofs_arena_lint` for output steps and before `ofs_target` targets, `cmake/OpenFS98.cmake:24,82,110`). `ofs_revert` bypasses both paths, as it does for the arena lint, and that is stated.
  - **The table** becomes machine-readable comment rows: flag, site, reason. A site is an individual compiler recipe, identified by its file and the recipe's output and source, so duplicate-output recipes such as the `PANELS_unit.dll` ones are separate sites.
  - **Coverage:** the checker parses every compile command in `cmake/steps/*.cmake` and `cmake/targets.cmake`, including loop-generated ones (SIMSUI's 24 objects plus its two explicit compiles), and the break runners that compile those units themselves (`tools/simsui_breaks.py:837`). Runners that delegate to CMake recipes (SIM1R via `difftests`; ANALYSIS, CONTROLS, G3D, PANELS, REALITY and WINDOW via break-build recipes) are covered through those recipes, and the notes say so.
  - **Failures:** a listed site missing its flag fails, and so does a listed flag found nowhere. An unlisted extra use is allowed.
  - **`--self-test`** covers:
    - the flag removed from one duplicate-output recipe while its siblings keep it;
    - a renamed site;
    - a loop-generated site;
    - an allowed extra use.
  - **Wiring proof:** the check runs on an up-to-date build, and a deliberately broken scratch copy stops a command target before anything compiles. Editing the toolchain comments rebuilds every output step, so the notes record that the rebuilt outputs are identical, checked with `tools/norm_pe.py` on the PEs and `cmp` on the objects against a pre-edit build.
- **(D) `tools/check_retail_bugs.py`**, each gap fixed with its own self-test fixture:
  - **Git listing:** a `git ls-files` failure exits 2 with a message, with no fallback. The existing allowlist fixture moves into a temporary Git repo.
  - **Spaced `#` in `.S`:** a spaced `# define` line is a directive. Genuine `#` comments stay comments.
  - **`;` in `.S` and asm strings:** in GAS x86, `;` separates statements and is not a comment. `.def` keeps its own comment syntax, and C asm strings keep today's string-warning path. Codex disabled `;` recognition in memory and the tree still gave 335 entries, 327 marked, 8 no longer apply, with no warnings.
  - **Line splices:** a `*\` before a newline followed by `/` is spliced before lexing, with physical line numbers kept.
  - **Empty register:** a register with no entries and no markers fails.
  - **Malformed duplicates:** a malformed near-marker such as `RETAIL BUG kept: SS-1` fails even when a valid marker for the same id exists elsewhere (reproduced at HEAD). The boundary of what counts as an attempted marker is defined in the code. Valid repeated markers stay valid.
  - **Baseline:** the check passes on the tree with 335 entries, 327 marked and 8 no longer apply. `docs/tools.md`'s Baselines line for it is updated from 334/326, or `cmake_verify.py` reports CHECK.
- **Docs:**
  - `docs/tools.md` rows for the new tools;
  - the toolchain table's row format, described in the file;
  - `deferred-work.md`: entries C and E get this spec's review findings appended (C: a leak must stop the run and block resuming, protected paths must be distinguished from expected `bin/` outputs, the break-line classification needs fixing, and `cmake_verify --self-test` currently raises `IndexError`; E: a scratch configure and build is needed, with stable recipe identifiers, the absent-`bin/gauges-all` case, and the bad-hash restore).
- **Running:** in a worktree `~/source/OpenFS98-cl2` off HEAD, with `orig/` and `cd/` copied and this approved spec copied in. Builds use `nice -n 19` and `-j 2`. No wine.
- **Merge:** `docs/tools.md` is also edited by slice 6b0, so the two merge into the main tree one at a time.

**Never:**
- No change to build outputs (B changes only comments and adds a check).
- No change to `tools/modules.sh`'s or `tools/ofs`'s behaviour, or to any break runner.
- No commits or staging.

</frozen-after-approval>

## Code Map

- **The query tools:** `tools/modules.sh` over `tools/modules.txt`; `tools/ofs` (`:5`, a wrapper of `cmake --build $OFS_BUILD`); callers `cmake/targets.cmake:159…`, `tools/main_dialogs.sh:30`, `tools/reg_contracts.sh:45`, `cmake/install.cmake:6`.
- **The toolchain table:** `cmake/toolchains/win32-retail.cmake:19-32`. `-fno-ipa-icf` appears in 8 commands in `cmake/steps/all.cmake` and 12 in `cmake/targets.cmake` (`:444`, `:1727` and others). SIMSUI's loop is at `all.cmake:379`.
- **Break-runner compiles:** `tools/simsui_breaks.py:837`, `tools/sim1r_breaks.py:1045`.
- **The lint wiring:** `cmake/OpenFS98.cmake:24,82,110`; `cmake/install.cmake:29` (`ofs_revert`).
- **`tools/check_retail_bugs.py`:** `:15`, `:19`, `:37`, `:51`, `:94`, `:122`, `:149`, `:165`, `:181`; Baselines line `docs/tools.md:287`.

## Tasks & Acceptance

**Execution:**
- [x] The worktree. (A) `tools/check_modules_ofs.py` and `tools/modules_golden.txt`.
- [x] (B) the table rows, `tools/check_codegen_overrides.py`, the wiring, and the identity check of the rebuilt outputs.
- [x] (D) `tools/check_retail_bugs.py` and the Baselines line.
- [x] Docs; `deferred-work.md` (A, B and D and the combined 1.10b entry moved; C and E updated); `## Implementation Notes`.

**Acceptance Criteria:**
- **Self-tests:** each tool's `--self-test` passes, and each deliberately broken case fails with its own message.
- **The worktree:**
  - a full build runs the codegen check and passes;
  - an induced failure stops a command target;
  - rebuilt outputs are identical to the pre-edit build;
  - `check_retail_bugs.py` passes with 335/327/8;
  - only the files listed above changed.

## Verification

**Commands:**
- `python3 -B tools/check_modules_ofs.py --self-test && python3 -B tools/check_modules_ofs.py` -- expected: pass.
- `python3 -B tools/check_codegen_overrides.py --self-test && python3 -B tools/check_codegen_overrides.py` -- expected: pass.
- `python3 -B tools/check_retail_bugs.py --self-test && python3 -B tools/check_retail_bugs.py` -- expected: pass, 335 entries, 327 marked, 8 no longer apply.

## Review Triage Log

Spec review (Codex gpt-6.2-sol, 2026-11-25), on the five-item draft:

| # | Finding | Verdict | Evidence / action |
|---|---|---|---|
| 1–3 | C: leaks must stop the run; protected paths against expected `bin/` writes; the break-line rule; `--self-test` raises `IndexError` | blocker/high | C deferred by the user; the findings go onto its entry. |
| 4–5 | E: a copied build is tied to its source path; the replay misses the absent-`bin/gauges-all` case; recipe identifiers | high | E deferred by the user; the findings go onto its entry. |
| 6 | B: key sites to individual recipes; loop-generated and runner compiles | high | Adopted. |
| 7 | B: wiring proof and output identity after the comment edit | medium | Adopted. |
| 8 | D: precise lexer fixtures; the malformed-duplicate boundary | medium | Adopted. |
| 9 | D: strict Git listing breaks the fixture; Baselines line 334/326 is stale | medium | Adopted. |
| 10 | A: `ofs` has no dry-run mode; eight query forms; the install parser | medium | Adopted (a `cmake` shim). |

## Implementation Notes

Built in the worktree `~/source/OpenFS98-cl2` (detached at `f15ee6a`, `orig/` and `cd/` copied), builds `nice -n 19 -j 2`, no wine.

- **(A)** `tools/check_modules_ofs.py` and `tools/modules_golden.txt` (114 answers: the five list queries, and installed-path-of, orig-module-of, installed-name-of for each of the 36 modules, plus the unknown-name error). Update with `python3 -B tools/check_modules_ofs.py --update`. `install.cmake`'s parse is cut out of the file (`set(OFS_MODULES "")` to the first `endforeach()`) and run with `cmake -P`. `tools/ofs` runs against a fake build dir through `OFS_BUILD` and a `cmake` shim; the default `build/win32-retail` path is not exercised. The self-test breaks copies of modules.sh, modules.txt, install.cmake and ofs (seven breaks).
- **(B)** 22 `#@` rows in `cmake/toolchains/win32-retail.cmake`, covering 279 gcc recipes. A row matches every recipe with that file, output and source (fnmatch), so the three `PANELS_unit.dll` recipes and the two `ADVDRV_ours.dll` ones are each checked; there is no ordinal. The SIMSUI loop is expanded from `src/simsui/sources.txt` (24 units plus the two explicit compiles). `tools/simsui_breaks.py` is parsed from its argv list. The other break runners compile units that carry none of the four flags and are not listed; SIM1R and the ANALYSIS/CONTROLS/G3D/PANELS/REALITY/WINDOW runners go through CMake recipes. The check is a second `COMMAND` of the `ofs_arena_lint` target in `cmake/OpenFS98.cmake`, so it takes the arena lint's paths and `ofs_revert` bypasses both.
- **Wiring proof:** the full build in the worktree ran it (`codegen overrides: 22 rows, 279 compile commands, ok`), and so does an up-to-date build. In a scratch copy with `-fno-ipa-icf` removed from `all.cmake:21`, `cmake --build --target mdls-revert` stopped at the lint with `missing -fno-ipa-icf (bin/ADVDRV.DLL <- src/advdrv/advdrv.c)` and no `bin/` was created.
- **Identity:** the comment edit rebuilt all 41 generating steps (1m55s). Against a copy of the pre-edit build, `tools/norm_pe.py` gave the same digest for every .dll/.exe/.sim in `bin/`, and `cmp` found no difference in any of the 187 `.o` files.
- **(D)** all six gaps fixed in `tools/check_retail_bugs.py`, each with a fixture. An attempted marker is "retail bug" (any case) followed by `(`, `:`, `kept`/`suspected`/`fixed` or an id; prose such as "a RETAIL BUG in X" is not. A spaced `# define` is a directive only when a known directive name follows the `#`, so `# define the thing` in prose-comment form is also taken as code (a limit). `;` is a comment only in `.def`. The git-failure self-test prints the checker's message to stderr. Tree result: 335 entries, 327 marked, 8 no longer apply, no warnings.
- **Docs:** `docs/tools.md` (two rows, the retail bugs Baselines line to 335/327), `deferred-work.md` (A, B, D and the combined 1.10b entry moved to `deferred-work-resolved.md`; C and E carry the review findings; the now-empty spec-1-3 heading removed). The main tree's uncommitted `deferred-work.md` adds entries at the end (1.10b2, 1.10b3, E); those hunks do not overlap this change, but the file must be merged, not overwritten.

### Review fixes (2026-11-25)

- **check_codegen_overrides.py:** rows gained a count column (`flag | file | output | source | count | reason`); a different match count fails, so one `PANELS_unit.dll` recipe renamed and stripped no longer passes. The SIMSUI template counts only if its list variable is referenced inside an `ofs_step(`. Python runners are parsed with `ast` (a commented-out flag is gone). Tokens keep their order, and a `-fX` after a required `-fno-X` fails. Commented CMake lines are blanked before parsing, and `--inventory` prints each recipe's own file. Self-test cases added for each.
- **check_modules_ofs.py:** the shim logs one argument per line and the test compares the exact argv list with a build dir containing a space (combined-argument mutations are in the self-test); the unknown-target check is exact. A fixture `modules.txt` with one module of every kind asserts all five list queries, so the two empty candidate answers are covered. `--update` collects and validates first, then writes a temp file and `os.replace`s it.
- **check_retail_bugs.py:** the attempted-marker matcher needs `bug` at a word boundary and a separator before a kind word or (case-sensitive) id, so "retail bugfix-123" and "retail bug in-flight" are prose, `RETAIL BUG kept: SS-1` is still malformed. Directives are recognised through block comments before and after the `#`. The not-a-repo fixture asserts the stderr message.
- **Docs:** `docs/tools.md` Baselines gained the three self-test lines (`cmake_verify.py --only <name> --list` selects each one); `deferred-work.md` gained the entry on the unasserted `ofs_arena_lint` wiring.

Verification after the review fixes (2026-11-25):
- The three self-tests and plain runs pass: 114 golden answers; 22 rows over 279 commands; 335/327/8.
- The worktree's full build ran both lints.
- The toolchain edit is comments only (0 non-comment lines; `module_flags.FLAGS` identical). Every step was rebuilt.
- 167 of the 170 rebuilt PEs are normalised-identical to the main tree's build of the same sources.
- The other three are `mdl-crash/mdlcrashdiff.exe`, `mdl-diff/mdldiff.exe` and `mdl-diff/mdldlldiff.exe`. They embed the build root's absolute path (`/home/steve/source/OpenFS98-cl2` against `/home/steve/source/OpenFS98`, 22, 8 and 16 occurrences). That is the known MDL-include path dependence (cleanup 1's review). The implementer's same-root comparison after the first comment edit found them identical.

Code review (three Codex gpt-6.2-sol layers, 2026-11-25; B blind, E edge-case, V verification-gap):

| # | Finding | Verdict | Evidence / action |
|---|---|---|---|
| B1 | A renamed PANELS_unit recipe with its flag dropped passes, because its siblings still match | medium | Real. Patch: expected match counts. |
| B2 | The SIMSUI template is counted even if it is disconnected | medium | Real. Patch. |
| B3/E3/V5 | Flags inside Python comments count | medium | Reproduced. Patch: AST. |
| B4 | A later `-fipa-icf` cancels the override unseen | medium | Real. Patch: order-aware check. |
| E2/E5 | Commented CMake recipes count; the inventory prints the wrong filename | low | Patch (trivial). |
| B7/E7/E9/V4 | The `ofs` shim loses argv boundaries; substring target check | medium | Real. Patch. |
| B8 | The candidate queries have empty golden answers | medium | Real. Patch: a fixture. |
| B9/E10 | `--update` truncates before collecting | low | Patch: atomic write. |
| B6 | The malformed-marker matcher flags prose | low | A false-positive risk. Patch. |
| B5 | Block comments around `#` in `.S` directives | low | Patch with fixtures. |
| V3 | The git-failure fixture checks only the exit code | low | Patch. |
| V2 | The self-tests are not recurring | medium | Patch: Baselines lines. |
| V1 | No recurring check that the codegen lint stays wired into the build | medium | Real, but needs a scratch-CMake harness. Deferred. |
| E1 | Shell backslash continuations | low | Rejected: none exist in the recipes today; the fix would be a speculative guard. |
| E6 | `--root` without an argument raises IndexError | low | Rejected: developer-only, fails loudly. |
| E8 | Install-parse comparison order | low | Rejected: only a false failure, and the current table passes. |
| B10 | The 1.10b successors are missing from deferred-work.md | false | They are in the main tree's uncommitted copy; the merge keeps them. |
