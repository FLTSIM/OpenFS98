---
title: 'Story 1.4, slice 13: GLOBAL and HG2D data decompiled, mixed slot check'
type: 'refactor'
created: '2026-11-21'
status: 'done'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43' # worktree held uncommitted Stories 1.2-1.4; file snapshot at ~/openfs98-verify/baseline-1-4m
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4l-numeric-util-data.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** GLOBAL and HG2D still carry their data as allowlisted dumps, from `gen_global_state.py` and `gen_hg2d_data.py`. Neither module passes `check_data_image.py` today:
- **GLOBAL** has no ImportTable, so it has no manifest row: retail exports only `Linkage`, at `.data`+0. The check already handles that: with `at=`, `compare()` uses `off=0` (`check_data_image.py:300-307`). `at=_Linkage` passes today, with 2 relocated and 2 ours.
- **HG2D** matches retail byte for byte outside its relocated slots. But of retail's 207 relocated slots, 95 are zero in ours (installed at attach: 55 data pointers from `hg2d_pointers.inc`, and 40 Linkage and service slots in C at `hg2d.c:233-272`) and 112 hold retail's raw value. The check accepts either all-zero or all-kept, not a mix. G3D (176/202) and MAIN (502/133) have the same mix, in slices 14 and 15.

**Approach:**
- **`tools/check_data_image.py`:**
  - **New row flag `--mixed`:** each retail relocated slot that isn't ours must be either zero or hold retail's raw value. Zero slots named in the row's `ptrs=` list get slice 10's value check. Zero slots not listed are allowed: they're set in C, under the deferred symbol-per-slot check.
  - **`--hash`** already covers the pattern, since raw values are hashed as bytes and zero slots as zero.
  - **`ptrs=`:** learns the `HG2D_DATA_POINTER(at, target)` form (image start + target). Under `--mixed`, completeness is relaxed to the rule above.
- **GLOBAL:** `global_state.inc` becomes typed C in `global.c`: the 194 non-zero state bytes after the 0xc-byte Linkage header (203 counting the header), written as typed fields or `U8`/`U16`/`U32` values at their offsets, named where `global.c` already knows them. The header's id and two function pointers stay as they are.
- **HG2D:**
  - `hg2d_image` stays a `uint8_t[0x28cd0]` byte array, because `hg2d.c` and `tools/hg2d_breaks.py` use `hg2d_image + 0x…` text throughout. Its initialiser is written in slice 12's style, one typed value per macro at retail's offsets.
  - `hg2d_pointers.inc` becomes committed source, with its header reworded. So do the generator's other two outputs, `hg2d_projective_tables.inc` (`gen_hg2d_data.py:64-129`) and `hg2d_device_guids.inc` (`:133-139`). Their headers name where each table comes from, and any invariant the generator asserted becomes a comment or a `_Static_assert`.
- **Removed:** both generators, and the two dumps `global_state.inc` and `hg2d_data.inc`, and the generator lines in `cmake/steps/all.cmake` and in the `hg2ddiff-run` and `hg2d` targets.
- **Added:** two manifest rows (`UPDATE=1`), a `--hash` line for GLOBAL and one for HG2D.

**Gate:**
- `GLOBAL.DLL` and `HG2D.DLL` normalized-identical to the pre-slice build, using `peh2.py`.
- `bin/hg2d-diff/HG2D_ours.dll` and `hg2ddiff.exe` normalized-identical. Build them with the `hg2d` target, which only builds.
- `data-images` passes with 23 modules.

If a DLL's code changes, report the per-function diff and stop for a human decision.

**Queued until Story 1.3's Baselines run finishes:** `hg2ddiff-run`, `hg2dmodeldiff-run` and `hg2d-breaks`.

## Boundaries & Constraints

**Always:**
- Slice 12's conventions:
  - byte arrays keep their names, types, sections and every `+ off` access;
  - one typed value per macro;
  - strings as literals;
  - hex floats in comments for doubles.

  Check (a) must flag none of the new C. A table with no formula that does trip (a) goes in its own typed file with one allowlist entry giving the reason.
- GLOBAL keeps `struct global_module`'s layout, and its Linkage at `.data`+0.
- Read each generator's docstring and asserts before deleting it, and carry what they know into comments.
- The text that `tools/hg2d_breaks.py` patches in `hg2d.c` stays as it is.
- Never `rm -rf bin`. No staging or commits. Record the files in `~/openfs98-verify/story-files.md`.

**Never:**
- No module or harness behaviour change, and no other module.
- No heavy runs (the queued list).

</frozen-after-approval>

## Code Map

- **GLOBAL:**
  - `src/global/global.c`:
    - `:21` includes `global_state.inc`, which defines `FS98_GLOBAL_SIZE 0x12a8`, `FS98_GLOBAL_HEAD 0xc` and `FS98_GLOBAL_DEFAULTS` as `[off - HEAD] = byte` designators;
    - `struct global_module` holds the Linkage header (`id`, `init`, `deinit`) and then the state;
    - its known field is +0x365, the SLEW early-out flag.
  - Retail exports `Linkage` at `.data`+0 and nothing else. Run plainly, the check stops with "retail has no ImportTable export inside .data"; with `at=_Linkage` it passes today. Our `_Linkage` symbol is at 0x20722000, the start of `.data`. Retail's init and deinit slots (+4, +8) are relocations in ours too.
  - `check_raw_dumps.py` mentions `global_state.inc` in its backslash-continuation note. Update that comment.
- **HG2D:**
  - `src/hg2d/hg2d.c:12-14`: `uint8_t hg2d_image[DATA_SIZE]` (0x28cd0), aligned(4), with `hg2d_data.inc` as designated rows. ImportTable and Linkage are `.set` onto it at `:223`.
  - `:229-232` installs `hg2d_pointers.inc` (`HG2D_DATA_POINTER(at, target)`, 55 entries). Every entry equals retail's raw slot value (0x20730000 + 0x62000 + target). The other 40 zero slots (+0x3c, +0x40, and +0x50..+0xf4 apart from the listed +0xc4..+0xd0) are set by hand at `:233-272`.
  - `tools/difftest/hg2ddiff.c:150` includes `hg2d_pointers.inc`.
  - `tools/gen_hg2d_data.py` writes four files: `hg2d_data.inc`, `hg2d_pointers.inc`, `hg2d_projective_tables.inc` and `hg2d_device_guids.inc`. It runs only from the `hg2ddiff-run` and `hg2d` targets (`cmake/targets.cmake:866`, `:1307`); `all.cmake:135-140` builds `bin/HG2D.DLL` from the committed files.
  - Retail: base 0x20730000, the same as ours. Its 207 relocated slots are 95 zero in ours and 112 holding retail's raw value.
- **`tools/check_data_image.py`:**
  - **`--mixed` plumbing:** add the flag to `read_manifest`, `format_row`, `run_manifest`, `compare` and `ptrs_check`; reject `--mixed` together with `--kept`.
    - Today `ptrs_check():257` rejects unlisted zero slots unless `kept_mode` is set, and `compare():320` rejects zero kept slots.
    - Under `--mixed`, every retail relocation that isn't ours may be zero or retail's raw value. Raw values are zeroed only for the comparison, as kept slots are now.
    - Listed slots keep their duplicate, foreign-slot, owned-relocation and target checks; only completeness is relaxed.
    - `hash_check` needs nothing new.
  - **`HG2D_DATA_POINTER`:** add it to the two-argument regex in `PTR_RES` and to the form → base table in `ptrs_check:239`, with base `.data` RVA. The target is then 0x20792000 + target.
  - **Self-test cases:**
    - **pass:** HG2D's real row, with raw and unlisted-zero slots accepted;
    - **fail:** a `--mixed` slot that is neither zero nor raw;
    - **fail:** a listed HG2D slot with a mistyped target;
    - **fail:** the same row without `--mixed`, so non-mixed completeness stays strict;
    - **fail:** a GLOBAL byte flipped, on both the retail side and the hash side.

    `mutated()` and `listcase()` already handle `at=` and image length. Don't write a "slot is ours" case for HG2D, which has no relocations of its own.
- **`tools/raw_dumps_allowed.txt`:** remove `gen:gen_global_state.py`, `gen:gen_hg2d_data.py`, `src/global/global_state.inc` and `src/hg2d/hg2d_data.inc`. Expect `7 allowed dump files, 40 scripts reading orig/ (14 gen, 10 resource, 16 oracle)`.
- **Docs:**
  - `docs/modules/global.md` and `hg2d.md`, where they name the generators;
  - `docs/building.md` (`--mixed`, and twenty-three modules);
  - `docs/tools.md`, the Baselines `data-images` line;
  - `deferred-work.md`: the queued runs, and HG2D's unlisted code slots under the symbol-per-slot entry.

## Tasks & Acceptance

**Execution:**
- [x] Record the `peh2.py` hashes (`/tmp/claude-1000/-home-steve-source/b2e3078b-1d77-426e-816f-2de2772a67dc/scratchpad/peh2.py`) of `GLOBAL.DLL`, `HG2D.DLL`, `bin/hg2d-diff/HG2D_ours.dll` and `hg2ddiff.exe`, from the pre-slice tree.
- [x] `tools/check_data_image.py`: add `--mixed`, the HG2D list form and the self-test cases.
- [x] GLOBAL, then HG2D:
  - add the typed definitions;
  - delete the generator, its dumps and its build lines;
  - add the manifest row (`GLOBAL … at=_Linkage`; `HG2D … --mixed ptrs=src/hg2d/hg2d_pointers.inc`) without hashes, and the `--hash` line; build with `UPDATE=1` until the hashes are filled in;
  - check identity.
- [x] Run `tools/ofs data-images UPDATE=1`, then a clean `tools/ofs all` plus the `hg2d` target without `UPDATE`.
- [x] Update the allowlist, the docs and `deferred-work.md`.

**Acceptance Criteria:**
- Given the rebuilt tree, when the four outputs are hashed with `peh2.py`, then each equals its recorded hash.
- Given `tools/ofs data-images`, when it runs, then it reports 23 images identical, HG2D with 112 kept and 55 listed slots, and the self-test passes.
- Given the tree, when `check_raw_dumps.py` runs, then it exits 0 with 14 gen.
- Given one value changed in a temp copy, in each of GLOBAL's defaults, HG2D's image and `hg2d_pointers.inc`, when the module rebuilds, then its `--hash` step fails.

## Implementation Notes

- Implemented by a Sonnet subagent and checked in the main session. Code review: Codex (`gpt-6.1-sol`).
- Normalized (`peh2.py`), identical before and after:
  - `GLOBAL.DLL` eec0115e3d8aa669 and `HG2D.DLL` 516a0e6d028dc6f3, both equal to s14;
  - `bin/hg2d-diff/HG2D_ours.dll` b53b8b35a10b4ff8 and `hg2ddiff.exe` 8f1f10ac8aa33908.
- `tools/ofs data-images` prints `23 .data images identical (399527 bytes, 1915 relocated), 8 .rdata ranges identical (2317 bytes), 1 switch tables, 6 pointer lists (1026 entries), self-test passed (53 cases)`:
  - HG2D: 207 relocated, 112 kept, 55 listed;
  - GLOBAL: 2 relocated, 2 ours.
- `check_raw_dumps.py` prints `7 allowed dump files, 40 scripts reading orig/ (14 gen, 10 resource, 16 oracle), none new`.
- Negative AC: one GLOBAL default, one HG2D image value and one `hg2d_pointers.inc` target, each changed in a scratch copy, failed `--hash`.
- HG2D's image initialiser lives in a new `src/hg2d/hg2d_image.inc`, included by `hg2d.c`.
  - Its font and many tables are typed by width from their byte patterns, marked "meaning not yet known".
  - There is no `docs/modules/global.md`.
- Not run: the queued difftests.

## Spec Change Log

## Review Triage Log

Pass 1: Codex (`gpt-6.1-sol`). Blind Hunter (low effort), Edge Case Hunter and Verification Gap (medium). The edge-case layer returned `[]` and the gap layer "No verification gaps found"; all ten findings below are the Blind Hunter's.

| # | Finding | Verdict | Evidence | Route |
|---|---|---|---|---|
| 1 | `--mixed` accepts zero in slots meant to stay raw, so a deleted raw initialiser passes the retail check and `UPDATE=1` would re-hash it | medium | true: zero is accepted for any non-listed slot. The per-build hash still catches the edit; only a deliberate `UPDATE=1` would bless it | defer: give `--mixed` an explicit list of the slots set in C, before G3D and MAIN reuse it |
| 2 | Completeness is off, so a deleted `HG2D_DATA_POINTER` entry passes the retail check | medium | same root cause as #1; the per-build list hash catches it | defer, with #1 |
| 3 | Self-tests don't cover raw-to-zero or entry deletion | low | follows from #1 and #2 | defer, with #1 |
| 4 | 40 C-installed slots unverified | medium | the symbol-per-slot entry, already deferred and noted by this slice | reject (already deferred) |
| 5 | The six GUIDs in `hg2d_device_guids.inc` have no guard | medium | retail `.rdata` 0x61050:0x60 matches once in ours (RVA 0x3b040, checked) | patch: `rdata=` range |
| 6 | The projective tail tables (from retail `.text`, words that are retail code addresses) have no guard | medium | true; the tables aren't a contiguous copy of retail bytes in our image | defer: needs a table-against-`.text` check |
| 7 | Difftests deferred | medium | queued by the frozen intent until Story 1.3 finishes | reject (scheduled) |
| 8 | Large `U8(...)` runs of unknown meaning are dumps by another name | low | 5593 `U8` values vs 323 `U16`/`U32`. Much is glyph bitmaps and byte ramps (genuinely byte data), each marked "meaning not yet known". This is slice 12's accepted pattern | reject; raised to the user |
| 9 | `building.md` says "all twenty-one relink" | low | stale count | patch |
| 10 | Comments say the build proves the image against retail | low | the build checks our hash; `data-images` compares with retail | patch |

## Verification

**Commands:**
- `tools/ofs all` and `tools/ofs hg2d`, then `peh2.py` over the four outputs -- expected: the recorded hashes.
- `tools/ofs data-images` -- expected: PASS, with 23 images.
- `python3 tools/check_raw_dumps.py` -- expected: exit 0, with 14 gen.
