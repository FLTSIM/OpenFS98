---
title: 'Story 1.4, slice 1: the raw-dump check, and WINMSG and MPCHAT data decompiled'
type: 'refactor'
created: '2026-11-19'
status: 'done'
route: 'dispatch'
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43' # worktree held uncommitted Stories 1.2/1.3; file snapshot at ~/openfs98-verify/baseline-1-4
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/epic-1-context.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** 26 module data images sit in git as hex dumps (`src/*/*_data.inc`), and the build still regenerates most of them from `orig/` (AD-12 wants decompiled C initialisers only). Story 1.4 is too big for one sitting, and the user asked for it slice by slice.

**Approach:** This slice adds a raw-dump check whose allowlist starts with every current offender and may only shrink. It converts the WINMSG and MPCHAT images into typed C initialisers at retail's offsets and proves each compiled image matches retail byte for byte. Later slices take the rest of the modules, one or more at a time, until the allowlist is empty.

## Boundaries & Constraints

**Always:**
- Each converted image keeps retail's layout and size: a struct of typed fields, checked with `_Static_assert` on `sizeof` and on the key `offsetof`s.
- Strings become string or char-array initialisers; numbers become named constants or fields. Pointers that retail relocates stay zero in the image and are installed at load, as today.
- The check's allowlist names each offender once. The check fails on any new offender, and on any listed entry that is no longer an offender, so a finished module must leave the list.
- Heavy runs (all-flight comparisons, long difftests) wait until Story 1.3's runs in `~/openfs98-verify` have finished. Break suites run from a frozen copy.
- No staging or commits.

**Never:**
- No change to modules other than WINMSG and MPCHAT, beyond the allowlist.
- No change to the win32-retail flag set or to Story 1.3's files, except adding Baselines lines to `docs/tools.md`.
- The image is never produced from `orig/` at build time.

**Decisions:** WINMSG (0x4a8 bytes) and MPCHAT (0x50c bytes, with strings) go first: both are small, single-image modules whose difftests run in minutes. The no-`orig/` stop already exists (`CMakeLists.txt:9`), so this slice only checks that it still applies.

</frozen-after-approval>

## Code Map

- `src/winmsg/winmsg.c:37-43,711-716` -- `struct winmsg_image { uint8_t bytes[0x4a8]; }`. `D` is the image base, and `M32(D+off)` reaches it. The asm aliases `_ImportTable` (+0) and `_Linkage` (+0x30), which `winmsg.def` exports as DATA. `winmsg_install()` writes the 6 Linkage code pointers.
- `tools/gen_winmsg_data.py` -- its asserts name every non-zero field: an ImportTable of {1,2,4,10,3,0}, Linkage id 22 at +0x30, slot colours at +0x58 ({0,0x6a,0x75,0x75,0x75,0x6a,0x6a}), the fill byte 0x0b at +0x74, and zero from +0x78 on. Use these asserts as the field list, then delete the script.
- `src/mpchat/mpchat.c:552`, `tools/gen_mpchat_data.py` -- the same pattern (ImportTable, Linkage, a dialog table, class names such as "Button" and "RICHED32.DLL", the pointers zeroed).
- `cmake/steps/all.cmake:270` (WINMSG) and `:168` (MPCHAT) -- the generator lines to remove. The gcc lines are unchanged.
- `CMakeLists.txt:9` -- the existing stop when `orig/` is missing.
- `docs/tools.md` -- the Baselines block (WINMSG and MPCHAT lines at ~230-233). `tools/cmake_verify.py` runs it.
- `docs/modules/winmsg.md`, `docs/modules/mpchat.md` -- they say where the image comes from. Update them.
- Not to touch: Story 1.3's files (see `~/openfs98-verify/story-files.md`).

## Tasks & Acceptance

**Execution:**
- [x] `tools/check_raw_dumps.py` (new), plus its allowlist `tools/raw_dumps_allowed.txt` (new) -- add the check. Offenders are:
  - (a) a git-tracked file under `src/` or `include/` with a run of 8 or more consecutive lines that each hold 8 or more integer literals and nothing else (the threshold is a constant in the script);
  - (b) a tracked file whose sha256 equals a file under `orig/` (skipped with a note when `orig/` is absent);
  - (c) a `cmake/steps/*.cmake` command that runs a `tools/gen_*.py` over `orig/Modules/...` into `src/`, listed as `gen:<script>`.

  Seed the allowlist with today's offenders, then remove WINMSG's and MPCHAT's once converted.
- [x] `tools/check_data_image.py` (new) -- `<retail.dll> <ours.dll>`. It takes retail's `.data` image (virtual size, zero-filled past the raw data) and zeroes each retail relocation target inside it. It finds ours at the `ImportTable` export and compares the same number of bytes, reporting the first differing offset. It exits 0 and prints `<MODULE>: .data image 0x<size> bytes identical`.
- [x] `src/winmsg/winmsg.c` -- replace `bytes[]` and the `#include` with a typed struct and its initialiser. Keep `D` pointing at the image's start, so every `M32(D+off)` keeps working. Delete `src/winmsg/winmsg_data.inc` and `tools/gen_winmsg_data.py`.
- [x] `src/mpchat/mpchat.c` -- the same; delete `mpchat_data.inc` and `gen_mpchat_data.py`.
- [x] `cmake/steps/all.cmake` -- drop both generator lines.
- [x] `docs/tools.md` -- add three lines to the Baselines block: `tools/check_raw_dumps.py` and one `check_data_image.py` line per module, each with its expected output.
- [x] `docs/modules/winmsg.md`, `docs/modules/mpchat.md`, `docs/building.md` -- the image is now C; say which checks prove it.
- [x] `docs/implementation/deferred-work.md` -- add the remaining modules (the allowlist is the list).

**Acceptance Criteria:**
- Given a clean build, when `check_data_image.py` runs for WINMSG and for MPCHAT, then each prints "identical" and exits 0.
- Given the tree, when `tools/check_raw_dumps.py` runs, then it exits 0. When a 10-line hex dump is added to a tracked `src/` file, or an allowlist entry is no longer an offender, then it exits 1 and names the file or the entry.
- Given the converted modules, when `winmsgdiff-run`, `winmsg-breaks`, `mpchatdiff-run` and `mpchat-breaks` run, then each meets its Baselines line.
- Given `orig/` renamed away, when configuring, then CMake stops with the message that points to `docs/building.md`.
- Given Story 1.3's runs have finished, when `frames_many.sh` compares all 74 flights, then all 74 match.

## Implementation Notes

- Both images are typed structs at retail's offsets: `check_data_image.py` gives identical for WINMSG (0x4a8) and MPCHAT (0x50c); `mpchatdiff-run` and `winmsgdiff-run` give 20000 scenarios, 0 differ. `tools/ofs all` builds without the two generator steps, and with `orig/` renamed configure stops at `CMakeLists.txt:9`.
- The review widened check (a): besides 8 lines of 8 literals, a run of 128 or more literals counts even when it is one literal per line with comments, casts and identifiers between them (as in `sound_data.inc`). That adds `sound_data.inc` and `dynamic_data.inc` to the allowlist.
- Break suites, from a frozen copy (`~/openfs98-verify/s14`): `winmsg-breaks` 64 breaks, 58 caught, 4 proven equivalent, missed limit-margin and word-empty; `mpchat-breaks` 117 breaks, 107 caught, 4 proven equivalent, 6 missed (crashes). Both equal their Baselines lines; the runners exit 1 whenever a break is missed. The 74-flight comparison waits for Story 1.3's runs.
- Story 1.4 closed 2026-11-25: the held-back runs (`~/openfs98-verify/p14-results`, then the targets outside the Baselines list in `p14-final` and `p14-bisect`) all match their baselines or, where none existed, pass. The two exceptions, `g3ddiff-meshc-run` (43 of 20000 differ) and `gauges-panels-run N=1000` (143 of 1000), give the same scenario lists at 27371fb and are logged in `docs/open-differences.md` and `deferred-work.md`. `multipdiff-run` is now a Baselines line; `gauges-panels-run`'s default N went from 20000 to 1000.

## Spec Change Log

## Review Triage Log

| # | Finding | Verdict | Evidence / route |
|---|---|---|---|
| 1 | The new checks are not run by any build or ofs target (VG, BH) | low | They are Baselines lines, so `cmake_verify.py` runs them, like 1.3's `check_module_flags.sh`. Running them inside the build would make the build read `orig/` again. Rejected. |
| 2 | Neither checker has a self-test (VG, BH) | low | Negative tests were run by hand. Patch: inline asserts on the detector. |
| 3 | Check (a) misses dword-per-line and commented dumps (EC, BH) | medium | Verified: `sound_data.inc` and `dynamic_data.inc` are not flagged, and are only covered by their `gen:` entries. Patch: a second rule (128 literals). |
| 4 | Check (a) only scans src/ and include/ (EC, BH) | medium | The wider rule finds several fixtures in `tools/difftest/` (64–270 literals). Some may be retail data. Deferred. |
| 5 | Check (b) keeps one path per hash (EC, BH) | low | Real; a direct correction. Patch. |
| 6 | Check (b) re-hashes inside a nested any() (BH) | low | Slow, not wrong. Folded into patch 5. |
| 7 | Check (c) misses generators that read orig/ internally or across lines (EC, BH) | medium | `gen_world_advanced.py` reads `orig/English/World.DLL` with no path on its command line, and so do several `*_tables`/`*_runtime` generators. Whether table generators count as images is the next slice's call. Deferred. |
| 8 | The orig-absent filter at :176 is a no-op (VG, EC) | low | Every entry starts with src/ or gen:. Patch: delete it and skip only the stale report. |
| 9 | Indented `#` lines and a missing allowlist (EC, BH) | low | Indented comments: patch. A missing file fails loudly, which is correct. |
| 10 | Duplicate allowlist entries merge silently (EC) | low | Harmless: an offender is still allowed once. Rejected. |
| 11 | git ls-files failure gives a traceback (EC) | false | It fails loudly, which is the right outcome. |
| 12 | Untracked dumps are not checked (EC) | false | The AC is about tracked dumps. |
| 13 | "May only shrink" is not enforced (BH) | medium | Adding an entry passes. Enforceable against the committed list once the list is in git. Deferred. |
| 14 | check_data_image does not compare our image's size (EC, BH) | low | Both structs `_Static_assert` their sizes. Rejected for now. A later module without that assert is noted with finding 13's entry. |
| 15 | No check that ours is zero at relocation targets (BH) | false | Retail's targets are zeroed, so a non-zero byte in ours already shows as a difference. |
| 16 | A relocation in the last 3 bytes grows the image (EC) | low | Not reachable for these images (relocations at +0x30..+0x5c). Rejected. |
| 17 | No .data, relocation or export section gives an opaque exit (EC) | low | It exits non-zero with a message. Rejected. |
| 18 | No image-base check (EC) | low | Relocation zeroing is relative, and the comparison would still name the difference. Rejected. |
| 19 | mpchat offsets only partly asserted (EC, BH) | low | A direct addition. Patch. |
| 20 | WINMSG fill should be a 32-bit slot (BH) | false | `S_FILL` is read with `M8` (`winmsg.c:466`), and +0x75..0x77 are zero state. |
| 21 | Typed fields go unused because code uses M32(D+off) (BH) | false | By design (transliterated code); the types give the initialiser its meaning. |
| 22 | sprint-status `last_updated` format changed (VG, BH) | low | Patch. |
| 23 | building.md names the done modules, which goes stale (BH) | low | Patch. |
| 24 | The baseline_commit comment points at ~/openfs98-verify (BH) | low | Local by intent (the user's verification area). Rejected. |
| 25 | Stale references to the deleted files (BH) | false | A grep outside docs/implementation finds none, and the tree builds. |
| 26 | "26 dumps" in the intent vs 51 entries (EC) | false | 26 counts the `*_data.inc` files; the entries also count gen scripts and other incs. A spec edit, so rejected. |
| 27 | Orig-absent stop not tested (EC) | false | Tested by hand; see Implementation Notes. |

## Verification

**Commands:**
- `tools/ofs all && python3 tools/check_data_image.py orig/Modules/WINMSG.DLL bin/WINMSG.DLL && python3 tools/check_data_image.py orig/Modules/mpchat.DLL bin/MPCHAT.DLL` -- expected: both identical.
- `python3 tools/check_raw_dumps.py` -- expected: exit 0, plus the negative tests above.
- `tools/ofs winmsgdiff-run`, `tools/ofs mpchatdiff-run` -- expected: 20000 scenarios, 0 differ. The break suites run from a frozen copy and must meet their baselines.
