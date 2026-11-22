---
title: 'Story 1.4, slice 8: the data images guarded on every build, and re-proved against retail by one target'
type: 'feature'
created: '2026-11-20'
status: 'done'
route: 'dispatch'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43' # worktree held uncommitted Stories 1.2-1.4; file snapshot at ~/openfs98-verify/baseline-1-4h
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md', '{project-root}/OpenFS98/docs/implementation/spec-1-4g-analysis-reality-landme-atc-data.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Twelve modules' data images are now hand-typed C: WINMSG, MPCHAT, BOOSTER, WEATHERM, DEMO, WINDOW, ACONTAIN, DYNAMIC, ANALYSIS, REALITY, LANDME and ATC. The deleted generators asserted their values against retail on every build. Now only `tools/check_data_image.py`, run by hand or from the Baselines block, proves them, and it compares `.data` only, so ANALYSIS's doubles and REALITY's `.rdata` have no retail check at all. A later edit can ship wrong data with a green build.

A build-time comparison with retail is not acceptable: it would make the build depend on the reference CD again (the November pressing rebuilt WINMSG, for one).

**Approach:** Two layers, both driven by one committed manifest, `tools/data_images.txt`, with one row per module: the retail path, ours, flags (`--kept`, `.rdata` ranges), and the SHA-256 of our normalized image.

1. **Every build, needing no `orig/`:** after each of the twelve modules links, its step runs `check_data_image.py --hash <MODULE> bin/<M>.DLL`. It fails the build if a hash differs from the manifest. No retail bytes are stored, only hashes of our own output. It hashes:
   - our `.data` image, normalized exactly as for the retail comparison, together with the sorted list of our relocation offsets in it, so a NULL slot that gains a static pointer is caught;
   - for each of the row's `.rdata` ranges, its length and sha256, found by sliding a window over our `.rdata` and requiring exactly one match.

   The failure message says to run `tools/ofs data-images UPDATE=1` once the change is intended. With `UPDATE=1` in the environment (`tools/ofs` exports it for the whole `cmake --build`, so the dependency steps see it too), `--hash` warns instead of failing. Without that, a failed hash makes make delete the DLL (`.DELETE_ON_ERROR`, Unix Makefiles generator), and `data-images`, which `DEPENDS openfs98`, could never update the hashes.

   When it warns, `--hash` also sets the DLL's mtime to (0, 0). The next build then reruns that step against whatever manifest is there, so a row that the update did not change fails the next ordinary build.
2. **`tools/ofs data-images` (a new target, needing the reference CD):** for every manifest row, runs today's retail comparison (with `--kept` where flagged) plus a new `.rdata` comparison.
   - The `.rdata` check takes retail's `.rdata` range from the manifest and requires those exact bytes to appear exactly once in our `.rdata`, with none of our relocations overlapping the match. This covers relocation-free constant tables such as ANALYSIS's 9 doubles and REALITY's 0x19c bytes.
   - `tools/ofs data-images UPDATE=1`, which makes the build's `--hash` lines only warn, rewrites the manifest's hashes (`.data` and `.rdata`) row by row. A row is rewritten only if its `.data` and all of its `.rdata` ranges pass the retail check. `data-images` still exits 1 if any row fails.
   - The script reads `UPDATE == "1"` from the environment for both `--hash` warn mode and `--update`, so `UPDATE=0` means neither. An `UPDATE=1` left exported in a shell silently turns the build guard into warnings; the docs say so.

Also add `--self-test`: on temp copies of built DLLs it checks that each failure path fails. It runs as part of `data-images`, with `UPDATE` removed from the environment of its own `--hash` cases, so it also passes during `UPDATE=1` runs. The cases are listed in the Code Map.

Pointer slots checked by symbol stay out of scope: our DLLs carry no symbols. That deferral stands.

## Boundaries & Constraints

**Always:**
- The build-time `--hash` mode never opens `orig/`. Build output stays byte-identical: the check never changes the DLL's bytes, and in warn mode it only resets the DLL's mtime.
- The manifest holds hashes and offsets only, never retail bytes.
- `tools/ofs data-images` replaces the twelve `check_data_image.py` Baselines lines with one line. Its summary uses decimal numbers of 100 or more, so `cmake_verify.py` judges it rather than leaving it UNCHECKED: `data-images: 12 .data images identical (24350 bytes, 385 relocated), 2 .rdata ranges identical (484 bytes), self-test passed (11 cases)`. 24350 and 385 are today's totals: the sum of the twelve images' sizes, and retail's relocation counts as the current lines print them. Keep hex out of the expectation. Replacing twelve lines with one renumbers the later Baselines lines, so old `cmake_verify.py --lines` records won't match.
- `check_raw_dumps.py` must list `check_data_image.py` as `oracle:`. Rule (c) matches `orig` on the command line or in the script's source. `grep -w orig tools/check_data_image.py` finds nothing today, so the `data-images` command names it, `--orig orig`, which is enough on its own. Expected: `51 scripts reading orig/ (25 gen, 10 resource, 16 oracle)`.
- `AGENTS.md` is not edited: it is a managed block.
- Every converted module's DLL stays normalized-identical to `~/openfs98-verify/s14/bin`. This slice changes no module source.
- Never `rm -rf bin`. No staging or commits. Record the files in `~/openfs98-verify/story-files.md`.

**Never:**
- No retail comparison in the build steps.
- No change to module C sources.

</frozen-after-approval>

## Code Map

- `tools/check_data_image.py`:
  - Refactor `main` so the retail comparison yields our normalized image bytes: ours with our relocation targets zeroed, and in `--kept` mode the kept slots left raw. That is exactly what was compared.
  - `--hash` hashes those bytes, plus our sorted relocation offsets, without retail. It needs our image's start and length without retail, so the manifest row also records the image's offset from our `ImportTable` export and its length (today: `start = ours_ImportTable - off`, `len = vs`).
    - The review confirmed this for all twelve: ImportTable sits at 0x88 in ANALYSIS, 0x48 in REALITY and 0x7f8 in LANDME, and LANDME's image starts 0x40 into our `.data`. `len` is retail's `.data` size (WEATHERM 0x1c5, ATC 0x1a00), inside our `.data`.
    - Every one of our relocations in the images points into `.text`, so with those zeroed the hash survives code moving.
    - In `--kept` mode, take the bytes before the kept slots are zeroed for the comparison; the kept slots hold retail's fixed addresses.
    - The `.rdata` window: store each range's length and sha256. The review found ANALYSIS's 0x48 bytes once, at our `.rdata` +0x0, and REALITY's 0x19c once, at +0x40, with no relocation overlapping either.
  - `--manifest tools/data_images.txt` runs every row against retail.
  - `--rdata RVA:LEN` (a retail RVA range, as in the row format) does an exact-bytes search in our `.rdata`.
  - `--update` rewrites hashes row by row, for rows that pass both `.data` and every `.rdata` range.
  - `--self-test` builds mutated temp copies of built DLLs. It runs these 11 cases, each expected to fail (k = 11 in the summary):
    1. retail mode: a changed `.data` byte;
    2. retail mode: a foreign relocation, made by retargeting an existing relocation entry's offset;
    3. retail mode with `--kept`: a zeroed ATC kept slot;
    4. retail mode with `--kept`: a changed ATC kept slot;
    5. retail mode: a changed `.rdata` byte;
    6. retail mode: a relocation of ours overlapping the `.rdata` match;
    7. retail mode: a `.rdata` range found twice;
    8. `--hash`: a changed `.data` byte;
    9. `--hash`: a relocation added on a NULL slot;
    10. `--hash`: a changed `.rdata` byte;
    11. `--update`: a failing row, which must stay unchanged, with exit 1.

    The `--hash` cases run with `UPDATE` removed from their environment.
  - Keep the existing two-argument form working.
- `tools/data_images.txt` (new). One row per module: `MODULE retail ours [--kept] [rdata=RVA:LEN:SHA256 ...] off=0x.. len=0x.. sha256=…`.
  - The retail path is relative to `--orig`, e.g. `Modules/mpchat.DLL` for MPCHAT.
  - `--update` rewrites both the `.data` sha256 and each `rdata` SHA256.
  - The overlap test for a `.rdata` match uses `targets(ours)` within our `.rdata` (`r + 4 > m and r < m + len`); it needs no manifest data. The review measured: ANALYSIS's `.rdata` is 0x5c bytes, one match at +0x0, no relocations; REALITY's is 0x1054, one match at +0x40, with 8 relocations, none overlapping. The sliding sha256 takes about 6 ms. ANALYSIS has `rdata` = its first 0x48 bytes (the 9 doubles); REALITY has its first 0x19c. Read the retail RVAs from `objdump -h`.
- `cmake/steps/all.cmake`: in each of the twelve steps (OUTPUTs at `:6, :25, :34, :43, :71, :80, :138, :159, :198, :236, :245, :256`; re-read them), add one line after the link: `python3 -B tools/check_data_image.py --hash <MODULE> bin/<M>.DLL`.
  - `ofs_step` scans a `python3` line only for `tools/*.py` and `orig/…` tokens, so an edit to the manifest alone does not re-run a step. That is acceptable, because `data-images` re-checks everything. An edit to `check_data_image.py` re-links all twelve modules, which is harmless. Note both in the docs.
- `cmake/targets.cmake`: add `ofs_target(data-images DEPENDS openfs98 COMMANDS [==[python3 -B tools/check_data_image.py --orig orig --manifest tools/data_images.txt --self-test]==])`. The script reads `UPDATE == "1"` from the environment for both warn mode and `--update`, so no shell expansion is needed. No other target uses `UPDATE`.
- `tools/raw_dumps_allowed.txt`: add `oracle:check_data_image.py  # re-proves the converted data images against retail (tools/ofs data-images)`.
- `docs/tools.md`:
  - a `data-images` row in the targets table;
  - replace the twelve Baselines lines with `tools/ofs data-images` and its expected output;
  - update the `check_raw_dumps.py` count line to `51 scripts reading orig/ (25 gen, 10 resource, 16 oracle)`.
- `docs/building.md`: one paragraph on the two layers. The build guards our images by hash with no CD; `data-images` re-proves them against the reference CD; and to update after an intended change, edit the C, then run `tools/ofs data-images UPDATE=1`.
- `docs/implementation/deferred-work.md`:
  - mark resolved the 1-4e entry (the `data-images` target), the 1-4g entry (the `.rdata` mode and self-test) and the guard parts of the 1-4f entry;
  - keep the symbol-per-slot pointer check open, with "no symbols in our DLLs" as the reason.

## Tasks & Acceptance

**Execution:**
- [x] `tools/check_data_image.py`: the four new modes and the refactor. The two-argument form prints exactly what it prints today for all twelve modules.
- [x] `tools/data_images.txt`: generate the twelve rows with `--manifest … --update` from the current build. Before writing, confirm every row passes against retail.
- [x] `cmake/steps/all.cmake`: the twelve `--hash` lines. `cmake/targets.cmake`: the `data-images` target.
- [x] Allowlist, docs, `deferred-work.md`.

**Acceptance Criteria:**
- Given a clean `tools/ofs all`, when it finishes, then it succeeds, and the twelve DLLs are normalized-identical to `~/openfs98-verify/s14/bin`.
- Given `tools/ofs data-images`, when it runs, then it prints identical for twelve images and two `.rdata` ranges, the self-test passes, and it exits 0.
- Given a temp copy of the tree with `orig` symlinked, when one value in ANALYSIS's typed `.data` initialiser is changed, then `tools/ofs all` fails at the ANALYSIS step's `--hash` line.
- Given the same copy, when ANALYSIS's typed `.rdata` doubles are changed instead, then `tools/ofs all` fails at the ANALYSIS `--hash` line (the `.rdata` window). And `python3 -B tools/check_data_image.py --orig orig --manifest tools/data_images.txt`, run directly on the DLL built with `UPDATE=1`, fails at the `.rdata` check.
- Given a copy of the tree with no `orig/`, when each of the twelve `--hash` lines is run under `strace -f -e trace=openat,stat,newfstatat`, then each passes, and no traced path lies under `orig/` or is the row's retail path.
- Given an intended change (a temp copy with ANALYSIS's initialiser edited), when `tools/ofs data-images UPDATE=1` runs, then the build warns at the `--hash` line instead of failing, the retail check fails for ANALYSIS, and that row's hash is not updated, and `data-images` exits 1; the next `tools/ofs all` then fails at ANALYSIS's `--hash` line. A change that keeps the retail check passing (a no-op reformat) updates nothing either, because the hash stays the same.
- Given a temp copy where one retail-relocated slot that ours leaves NULL gets a static code pointer (ANALYSIS: `.linkage = { 29, (uint32_t)(uintptr_t)analysis_init },`, the slot `analysis_install` writes at `AN_LINKAGE+0x04` with the same value; the review built it, and it prints `23 relocated, 1 ours, identical`), when `tools/ofs data-images UPDATE=1` runs, then the retail check passes, that row's hash is rewritten, and the next `tools/ofs all` passes.
- Given the tree, when `check_raw_dumps.py` runs, then it exits 0, with `check_data_image.py` listed as `oracle:`.

## Implementation Notes

- `tools/ofs all` succeeds with the twelve `--hash` lines. `tools/ofs data-images` prints `data-images: 12 .data images identical (24350 bytes, 385 relocated), 2 .rdata ranges identical (484 bytes), self-test passed (19 cases)` and exits 0. `cmake_verify.py`'s `need()` finds 24350, 385 and 484 in it, so the line is judged PASS.
- `check_raw_dumps.py` prints `21 allowed dump files, 51 scripts reading orig/ (25 gen, 10 resource, 16 oracle)`.
- Deviation from the frozen Boundaries: the self-test has 19 cases, not 11. The review added 8: edge-straddling relocations in both modes, a `.rdata` overlap in `--hash`, `--hash` on ATC's kept slot and on LANDME's `off != 0` row, and `UPDATE=1` warn mode with its mtime reset. `docs/tools.md` states 19.
- Deviation from AC 1: DEMO.DLL is not identical to s14. That is slice 5's known `demo_init` reordering (the same 10 normalized lines), not this slice. Eleven DLLs are identical, and `demodiff-run` stays the confirming run for DEMO.
- Temp-tree acceptance runs, all as specified:
  - a changed ANALYSIS `.data` value or `.rdata` double fails at the `--hash` line;
  - under `strace`, with no `orig/`, the twelve `--hash` runs pass and touch no `orig/` path;
  - `UPDATE=1` with a retail-failing change warns, leaves the manifest unchanged, `data-images` exits 1, and the next `all` fails;
  - the `.linkage` static-pointer change gives `23 relocated, 1 ours`, `UPDATE=1` rewrites only ANALYSIS's row, and the next `all` passes.

  The no-op reformat case was not run.

## Spec Change Log

## Review Triage Log

| # | Finding | Verdict | Evidence / route |
|---|---|---|---|
| 1 | `--hash` skips the `.rdata` relocation-overlap test (BH) | medium | Patched: a shared `rdata_overlap` helper is used in both modes, and self-test case 13 covers it. |
| 2 | Self-test misses `UPDATE=1` warn mode, ATC kept slots and LANDME's `off != 0` in `--hash`, and edge-straddling relocations (VG, BH, EC) | medium | Patched: 8 new cases (19 in all). |
| 3 | `--update` rewrites the manifest in place, even unchanged, and reformats failing rows (BH, EC) | medium | Patched: it writes only on a change, through a temp file and `os.replace`, keeping failing rows' text as read. |
| 4 | Malformed manifest rows and missing option arguments give tracebacks (BH, EC) | low | Patched: `file:line: reason` messages, and usage. |
| 5 | A relocation 1..3 bytes before the image start is not caught (EC) | low | Patched. |
| 6 | Self-test leaks temp dirs on a raise, and is silently skipped when a row fails (EC) | low | Patched: try/finally, and "self-test skipped (N rows failed)". |
| 7 | relocate_to shifts a whole relocation block (EC) | low | Patched: one-entry mutations (`add_reloc` appends a single-entry block; a retarget changes one entry). |
| 8 | Dead code (BH) | low | Patched. |
| 9 | Editing only the manifest does not rerun the build's `--hash` checks (VG, BH) | low | Documented in `docs/building.md`; `data-images` re-checks everything, and an `UPDATE=1` run resets the DLLs it warned on. Making the manifest a step input is a build-system change. Deferred. |
| 10 | `UPDATE=1` left exported silently weakens the guard (BH) | low | Decided in the spec and documented. Rejected. |
| 11 | The per-build guard proves "unchanged since the last proof", not "equals retail" (BH) | false | By design (no CD at build time); `data-images` is the retail proof. |
| 12 | The sliding `.rdata` hash is wasteful (BH) | low | About 6 ms. Rejected. |
| 13 | CLI: one `--rdata` only, flags ignored in other modes (BH, EC) | low | The manifest carries several ranges. Rejected. |
| 14 | `--hash` with `UPDATE=1` and a missing DLL gives a traceback (EC) | low | The step that built it would already have failed. Rejected. |
| 15 | Twelve Baselines lines removed (BH) | false | Replaced by the single `data-images` line by design. |

## Verification

**Commands:**
- `tools/ofs all` -- expected: success, twelve `--hash` lines pass.
- `tools/ofs data-images` -- expected: twelve identical, two `.rdata` identical, self-test passed, exit 0.
- The normalized whole-DLL comparison against s14 -- expected: identical for all twelve.
- `python3 tools/check_raw_dumps.py` -- expected: exit 0.
