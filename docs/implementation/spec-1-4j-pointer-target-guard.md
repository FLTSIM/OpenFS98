---
title: 'Story 1.4 slice 10: guard the pointer lists and MULTIP small tables'
type: 'feature'
created: '2026-11-20'
status: 'done'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43' # worktree held uncommitted Stories 1.2-1.4; file snapshot at ~/openfs98-verify/baseline-1-4j
route: 'dispatch'
review_loop_iteration: 0
context: []
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** The data-image guard hashes the image bytes and relocation offsets. It doesn't hash the targets of the pointers FACIL, FE and MULTIP install at attach (`facil_relocs.inc`, `fe_relocs.inc` and `multip_pointers.inc`). It doesn't cover MULTIP's `s_nametag` and `s_frame_actions` either. A mistyped target builds cleanly, and only a difftest would catch it.

**Approach:**
- A new `ptrs=PATH[:SHA256]` field on a manifest row names a pointer list.
  - With `orig/` (`--manifest`), every listed slot and target is checked against retail's relocated values, and retail's relocations inside the image must all be accounted for.
  - The per-build `--hash` check needs no `orig/`. It hashes the parsed entries and compares that with the manifest.
- `s_nametag` gets an existing `rdata=` range, which already reads retail `.text`.
- `s_frame_actions` gets a new `switch=` field. With `orig/`, it rebuilds the table from retail's compressed switch: the byte table, then the jump table, then the jump target's action. The table must appear exactly once in our DLL. The per-build check hashes that window, as `rdata=` does.

## Boundaries & Constraints

**Always:**
- Every module DLL and harness stays normalized-identical to its pre-slice build. The only C source change is one comment.
- The hash covers the parsed entries, not the file text. Comment or whitespace edits don't trip it; a changed slot, kind or target does.
- `UPDATE=1` can only write a `ptrs=` hash when the retail check passes, as for image hashes today.
- `--hash` keeps working without `orig/`. The list hash is over the parsed tuples, in source order: `(form, off, target)` for FACIL and FE, and `(owner, off, kind, target)` for MULTIP. Slot RVAs need the retail DLL, so they're not hashed.
- The self-test stays green, plus new cases.

**Never:**
- No symbol-per-slot check of handler identity. Mapping a text RVA to our function stays as it is, and the deferred item stays open for it.
- Don't touch modules that install pointers in C code (DYNAMIC and others).
- No CMake changes unless the seeded test shows a `.inc` edit fails to rerun the step.
- No heavy runs: no difftests or break suites.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|----------|--------------|---------------------------|----------------|
| Clean build | lists unchanged | `--hash` passes for FACIL, FE and MULTIP | N/A |
| Mistyped target | one target in a `.inc` edited | `--hash` fails: "pointer list … differs from the manifest". `--manifest` fails, naming the slot, retail's value and ours | exit 1; under UPDATE=1 the row isn't rewritten |
| Dropped entry | one line removed | `--hash` fails. `--manifest` fails: "retail relocation at +0x… is not ours, kept or listed" | exit 1 |
| Foreign slot | entry at a slot retail doesn't relocate | `--manifest` fails | exit 1 |
| Duplicate slot | two entries with the same slot | `--manifest` fails | exit 1 |
| Slot is ours | a listed slot is also one of our own relocations | `--manifest` fails | exit 1 |
| Owner 1 | a MULTIP entry with owner 1 (retail has no `.rdata` relocations, and the list has none today) | rejected as unsupported | exit 1 |
| Switch mismatch | a byte of `s_frame_actions` changed | `--hash` fails on its window; `--manifest` doesn't find the rebuilt table | exit 1 |
| Comment edit | comment text changed | both pass | N/A |
| Unparsable line | a non-comment line in the list matches no entry form | fails with file:line | exit 1 |
| Nametag edited | a byte of `s_nametag` changed | `--hash` fails on its `rdata=` window | exit 1 |

</frozen-after-approval>

## Code Map

- `tools/check_data_image.py`:
  - `read_manifest` / `format_row`: add the `ptrs=` field; keep the field order stable.
  - `hash_check`: add the list hash.
  - `compare` / `run_manifest`: add the retail target check, the completeness check and the switch rebuild. Add `ptrs` and `switch` to the update tuple (line ~315), and make `format_row` write them.
  - `self_test`: has `mutated`, `hashed` and `retail` helpers; reuse them, with a temp copy of the list (absolute path) and a temp manifest. `copy_rva` (line ~444) uses `row["rdata"][0]`, so new `rdata=` fields go after MULTIP's existing `0xc000` one.
- The list formats:
  - **FACIL and FE:** `{FACIL,FE}_REL_DATA(off, target)` and `_REL_TEXT(off, rva)`.
  - **MULTIP:** `{ owner, off, kind, target }`.
  - Skip `/* */` comments, which can span lines; the header names the macros. Every other non-blank line must be one strict entry, or it's an error at file:line.
- How each entry maps to retail's values. The image starts at retail `.data` RVA + the row's `off`, and the raw value retail stores in a slot is image base + target RVA.

  | Entry | Slot RVA | Target RVA |
  |---|---|---|
  | `_REL_DATA` and MULTIP kind 2 | image start + off | image start + target |
  | `_REL_TEXT` | image start + off | target, a code RVA |
  | MULTIP kind 1 | image start + off | `.text` RVA + target, a `.text` offset |
  | MULTIP kind 3 | image start + off | `.rdata` RVA + target |

  The review checked this against the retail DLLs: all 74 FACIL entries match, all 55 FE and all 76 MULTIP. The retail relocations in each image are 74, 86 (31 ours plus 55 listed) and 76, so none are left over.
- `src/multip/multip_pointers.inc:2`: its comment calls kind 1 an "RVA". Correct it to a `.text` offset; a comment-only edit doesn't change the DLL.
- `switch=IDX_RVA:LEN:JT_RVA:T=A,…[:SHA256]`: for MULTIP, that is `switch=0x3c90:0x40:0x3c68:0x3c63=0,0x3c2b=1,0x3c3d=2,0x3c4c=3,0x3c59=4`. Map keys are `0x` hex, as in the other fields, and `format_row` writes them back in the same form. The jump table has max(index)+1 dwords; each holds base + RVA. Rebuilding the table gives exactly the 64 bytes of `s_frame_actions` (checked).
- `tools/data_images.txt`, on the FACIL, FE and MULTIP rows:
  - add `ptrs=src/<m>/<file>.inc`;
  - add `rdata=0x2cf4:0x2a` to MULTIP, for `s_nametag`. It matches once, at our RVA 0xd220, with no relocation over it. Then add the `switch=` field.
  - The hashes are filled in by `tools/ofs data-images UPDATE=1`.
- `cmake/steps/all.cmake`: `--hash` runs from the source root after the link. `ofs_step` globs `src/<m>/*.inc` (`cmake/OpenFS98.cmake:58`), so editing a list reruns it. Leave it alone.
- Retail layouts, for reference:
  - FACIL: base 0x206b0000, `.data` at 0x8000;
  - FE: base 0x206c0000, `.data` at 0xe000;
  - MULTIP: base 0x20850000, `.rdata` at 0xc000, `.data` at 0xd000.

## Tasks & Acceptance

**Execution:**
- [x] `tools/check_data_image.py`:
  - parse `ptrs=`, and parse the list with one regex per form;
  - check the list against retail, using the mapping table: each value, no duplicate slots, no listed slot that is ours, and owner 1 rejected. Completeness: the retail relocations in the image are exactly ours ∪ kept ∪ listed;
  - add `switch=`: rebuild the table, require exactly one match in ours with no relocation over it, and hash the window as `rdata=` does. If a jump-table dword isn't a retail relocation, or its target isn't in the map, fail with a message naming the index, not a traceback;
  - add the switch and pointer-list counts to the summary line from `run_manifest`;
  - make `--hash` verify the list hash and the switch window;
  - add one self-test case for each matrix row. The clean and comment-edit rows expect exit 0. Every edit runs on a temp copy of the list, never the real file;
  - update the usage text and the manifest header comment.
- [x] `src/multip/multip_pointers.inc`: fix the kind-1 comment.
- [x] `tools/data_images.txt`: add the fields, then run `tools/ofs data-images UPDATE=1` to fill in the hashes.
- [x] `docs/building.md` and `docs/tools.md`:
  - describe `ptrs=` in the guard section;
  - refresh the Baselines `data-images` line from the real output.
- [x] `docs/implementation/deferred-work.md`: mark the slice 9 pointer-list entry resolved. Leave the symbol-per-slot entry open.

**Acceptance Criteria:**
- Given the current tree, when `tools/ofs data-images` runs, then it passes. It reports 4 `.rdata` ranges, 1 switch table, and 3 pointer lists with 205 entries, and the self-test passes with its new cases.
- Given a full build, when each module's DLL and harness is normalized-hashed, then all match `~/openfs98-verify/s14/bin`. DEMO keeps its accepted difference.
- Given one target in `src/fe/fe_relocs.inc` seeded wrong in the real tree, when FE.DLL is rebuilt, then the build fails at its `--hash` step. Reverting the edit restores the build.

## Implementation Notes

- Implemented by a Sonnet subagent. `tools/ofs data-images` prints: 15 .data images identical (65106 bytes, 621 relocated), 4 .rdata ranges identical (1022 bytes), 1 switch tables, 3 pointer lists (205 entries), self-test passed (37 cases after the review patches). The new cases are 24-37. The clean row is the main run itself.
- The seeded `FE_REL_DATA(0x1d02, 0x1d12)` in the real `fe_relocs.inc` failed the build at FE's `--hash` step, with "pointer list src/fe/fe_relocs.inc differs from the manifest". Reverting it restored a clean build. No CMake change was needed.
- After the change, the normalized hashes are FACIL.DLL 4494ef6308c3165f, FE.DLL 29aaf9c2da2ba8d9 and MULTIP.DLL 810aadc0c1298d7a. These equal spec-1-4i's.
  - The four harness builds weren't rebuilt. Their sources are unchanged apart from the comment in `multip_pointers.inc`.
- `check_raw_dumps` is unchanged: 18/48, with 22 gen.
- Added after the dispatch: `compare` rejects `ptrs=` when the row's `off` isn't 0. List offsets count from ImportTable, and the image's offsets from `.data`. Today all three rows have off=0.
- The parser strips `/* */` comments only. A `//` line is reported as unparsable.

## Spec Change Log

## Review Triage Log

Pass 1 (Blind Hunter, Edge Case Hunter, Verification Gap; Sonnet). BH = Blind Hunter, ECH = Edge Case Hunter, VG = Verification Gap.

| # | Finding | Verdict | Evidence | Route |
|---|---|---|---|---|
| 1 | VG/BH: `switch_table`'s error paths (not a relocation, not in map) have no self-test case | medium | cases 32-34 only flip our bytes; `mp.get(t, 0)` would pass them | patch: two retail cases |
| 2 | VG/BH: unknown-kind rejection untested | low | no case sets kind outside 1-3 | patch: one listcase |
| 3 | VG/BH: the `ptrs=` + `off!=0` guard untested | low | unreachable in the self-test: a faked `off` fails the image comparison before the guard runs | reject |
| 4 | VG/BH/ECH: `--hash` guards the list source, not what the DLL installs (handler mapping, macros) | medium | real, but the intent's Never excludes the symbol-per-slot check; its deferred entry (deferred-work.md:72) stays open | reject (out of scope, already deferred) |
| 5 | BH/ECH: a `ptrs=` row without a hash reports "differs" instead of "no hash" | low | `None != ptrs_hash(...)` | patch |
| 6 | BH: no doc says an `.inc` edit reruns `--hash` | low | behaviour holds (`ofs_step` globs `*.inc`; seeded FE test failed the build); the doc is silent | patch: doc sentence |
| 7 | BH: owner-1 rejection undocumented | low | true; docs silent | patch: doc sentence |
| 8 | BH/ECH: completeness is skipped for `--kept` rows | false | in kept mode every retail relocation that isn't ours counts as kept, so ours ∪ kept already covers all of them and the skip is equivalent | reject |
| 9 | BH: only the first missing relocation is reported | low | cosmetic; it still fails | reject |
| 10 | BH/ECH: slot offsets aren't bounds- or alignment-checked | false | every slot must be in `rset`, which holds only retail relocations inside the image | reject |
| 11 | BH: unused `vs` parameter in `ptrs_check` | low | true | patch: delete |
| 12 | BH: the parser rejects `//` and trailing comments | false | strict form by design (spec Code Map); a bad line fails loudly, with file:line | reject |
| 13 | BH: deferred-work entry marked RESOLVED while the handler check stays open | false | the handler check is its own entry (deferred-work.md:72), still open | reject |
| 14 | BH: building.md paragraph dense; doesn't say what needs `orig/` | false | it says `--hash` needs no CD and `data-images` checks against retail | reject |
| 15 | ECH: switch LEN 0 makes `max()` raise | low | only a hand-mistyped manifest reaches it, and it fails loudly | reject |
| 16 | ECH: an out-of-section RVA, or an action > 255, gives a traceback | false / low | `read_rva` raises SystemExit with a message; an action > 255 fails loudly in `bytes()` | reject |
| 17 | ECH: duplicate map key keeps the last action | low | the rebuilt table would then not match ours, so it fails | reject |
| 18 | ECH: missing `.text` or `.rdata` in retail gives a KeyError | low | every FS98 module has both; fails loudly | reject |
| 19 | ECH: building.md implies every row gets the completeness check | false | the sentence is scoped to rows that carry `ptrs=` | reject |
| 20 | ECH: UPDATE=1 not rewriting a failing ptrs row is untested | low | it's the shared `rc` → `continue` path, covered by `update_leaves_failing_row` | reject |

## Verification

**Commands:**
- `tools/ofs data-images` -- expected: PASS, with the self-test case count increased.
- `tools/ofs build` (or the FACIL, FE and MULTIP steps), then `peh.py` over the seven builds from slice 9 -- expected: the hashes in spec-1-4i's notes.
- `python3 tools/check_raw_dumps.py` -- expected: unchanged, 18/48.
