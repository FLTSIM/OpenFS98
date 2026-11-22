---
title: 'Verify every baseline through CMake'
type: 'chore'
created: '2026-11-16'
status: 'done'
route: 'dispatch'
review_loop_iteration: 0
baseline_commit: '27371fb6412439f035a43c168790f46c7c4d6d43'
context: ['/OpenFS98/docs/tools.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Story 1.2 piece 2: every verification command in `docs/tools.md` (Baselines, 154 lines, about 42 h of documented wine time plus untimed ones) must meet its recorded baseline when run through CMake.

**Approach:** `tools/cmake_verify.py` runs each Baselines line through `cmake --build --target` (script lines as written), one at a time, from a frozen copy of the tree, and records verdict, log and tail per line. Resumable.

## Boundaries & Constraints

**Always:** Run from a frozen copy (break suites rewrite `src/`). Judge against the documented number; a lower count or any mismatch is a regression to investigate, never to rebase. Report faithfully: a CHECK or FAIL stays in the report.

**Never:** Edit `docs/tools.md` baselines to match a run. Run two lines at once. Touch `game/` beyond what the targets themselves do.

</frozen-after-approval>

## Tasks & Acceptance

**Execution:**
- [x] `tools/cmake_verify.py` -- the runner and report
- [x] Run all 154 lines in `~/openfs98-verify`; investigate every CHECK/FAIL

**Acceptance Criteria:**
- Given the frozen copy, when every line has run, then each is PASS, or its CHECK/FAIL is explained in the report.

## Implementation Notes

Run 2026-11-16..19 from `~/openfs98-verify/tree` (report `results/report.md`), with the follow-ups
from the Makefile-free copy `p3` (`p3-results/`). 154 lines: 128 PASS, 5 CHECK, 21 FAIL; each non-PASS:

- **CHECK, the line's number not in the output, fine:** `numericdiff-run` (prints 505440 + 40338 = 545778);
  `mdl-crash-diff-run` (6 x 2000 = 12000). `fesldiff-run`, `panelsdiff-run`, `panelsunit-run` quoted counts for
  an N the line did not pass; the lines now name it (`N=20000`, `N=3000`, `N=20000`) and give 0 differ.
- **Break counts stale under the crash-is-a-miss rule** (main_breaks Nov 1, g3d_breaks 0cc637b): `main-breaks`
  menu, commands, dialogs, viewdlg; `host-breaks`, `mpchat-breaks`, `winmsg-breaks`; `g3d-breaks` (278 caught,
  `plane-facet-skip` crashing). The Makefile tree misses the same breaks. Counts updated (user's say-so).
- **Gaps closed:** `main-breaks` flights (95/99, 2 proven equivalent), natives (95+3 -> 94+4: `col-ecx` proven
  equivalent), radios (115+5 -> 114+6: `n1-cx` proven equivalent; `com-ok` rewritten so it is caught), kneeboard (61/61),
  aircraft (37/39, `b-limit` and `open-size` proven equivalent); maindiff aircraft/create/kneeboard at N=20000
  0 differ.
- **Timeouts:** `controlsdiff-run`, `hg2ddiff-run` hit the runner's limit; run alone, 0 differ.
- **Setup only:** `smoke`, `main-smoke` in the tree (no staged game); PASS on a game copy. `host_rt_breaks.py`
  matches its documented result. `flights-play` exited 2 after printing PASS (trap under `set -e`, fixed).
- **SIMSUI breaks:** the old misses partly fixture gaps; fixture widened, 3 proven equivalent, now 708 breaks, 698
  caught at N=400, all but `init-far` and `char-digit-end` (reached, too rarely) at N=2000; simsuidiff N=2000
  0 differ. **`panels-breaks`:** the documented N=800/N=3000 follow-up does not reproduce,
  with the Makefile either, nor with the CD's gauges; 12 of the 200 left missed at N=3000, recorded as open in
  `docs/modules/panels.md`.

Code review (2026-11-19) found that the runner had passed lines it never checked (no count of 100 or more to look
for; "67," read as a count). With UNCHECKED and anchored matching, the ~50 such lines were read by hand: all match
their documentation except `tools/reg_contracts.sh`, whose runs had all ended NO-VERDICT (FRAMELOCK.DLL had no
target yet; re-run: 140 runs `same`, 0 BREAKS), and `gauge_breaks.py` / `gauges-breaks`, 66 of 67 (`chrn-minutes`,
also through make and at N=20000; open in `docs/remaining-work.md`; the script now exits 1 on a miss).

Bugs found and fixed: break runners missing `-Itools/difftest` (4ac3e38's `argcount.h`; wtime/wareas did not
build); `panels_breaks.py` reusing stale worker copies of `panelsdiff.exe`; no CMake target for
`hg2d_frames.sh`'s `FRAMELOCK.DLL`; `flights_play.sh` exit code; CMake steps not rebuilding a harness when its
`tools/` `.inc`/`.h` changed (`cmake/OpenFS98.cmake`; the Makefile tracked them). Retail bug SUI-13 logged.

## Verification

**Commands:**
- `python3 tools/cmake_verify.py build ../results` (from the frozen copy) -- expected: `report.md` with no unexplained non-PASS
