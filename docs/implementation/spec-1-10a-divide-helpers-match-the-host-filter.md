---
title: 'Story 1.10, slice a: divide.h helpers match the host filter'
type: 'feature'
created: '2026-11-25'
status: 'done'
route: 'dispatch'
baseline_commit: '0f8e49e12f345913116964b6aead145e6b8c520d'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/native-port/architecture/ARCHITECTURE-SPINE.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Story 1.10 needs every retail divide that can fault to call shared helpers in `include/fs98/divide.h`. Those helpers must return what retail's host filter `0x4119a0` writes. The header does not exist, and nothing checks a C divide against the filter.

**Approach:** Add the six helpers (spine rule, `ARCHITECTURE-SPINE.md:242`) and a helper test. The test executes each real `div`/`idiv` on chosen operands and lets **retail's** filter, from the mapped `FLTSIM98.EXE`, repair any fault. It does the same with **our** filter (`fs98_host_rt_divide_filter`), and requires all three results to agree: the helper's, retail's and ours. No module changes in this slice.

**Split (decided 2026-11-25):** Story 1.10 is three slices:
- **1.10a**, this one: the helpers and their test.
- **1.10b:** the trap-reliant asm modules FE, DYNAMIC, PANELS, BOOSTER, ADVDRV and DEMO.
- **1.10c:** the private-helper modules SIM1, HG2D, G3D, WINDOW, CONTROLS and MAIN.

The specs for 1.10b and 1.10c are written after this slice lands.

## Boundaries & Constraints

**Always:**
- **`include/fs98/divide.h`:** `static inline`, C99, `<stdint.h>` only, no asm, no undefined behaviour (so no C division that can trap or overflow).
- **The helpers mirror the instruction's true operands.** Every input and output is an unsigned bit pattern, as the registers hold it; callers cast. `remainder` may be NULL.
  - `uint32_t fs98_div32(uint32_t high, uint32_t low, uint32_t divisor, uint32_t *remainder)` and `uint32_t fs98_idiv32(...)` with the same parameters: edx:eax by r32.
  - `uint16_t fs98_div16(uint16_t high, uint16_t low, uint16_t divisor, uint16_t *remainder)` and `fs98_idiv16(...)` with the same parameters: dx:ax by r16.
  - `uint8_t fs98_div8(uint16_t ax, uint8_t divisor, uint8_t *remainder)` and `fs98_idiv8(...)` with the same parameters: ax by r8, giving the quotient (al) and the remainder (ah).
  - A `cltd` form is `fs98_idiv32(-(x >> 31), x, d, r)` on the bit patterns, and a zero-extended `div` passes `high = 0`. The header shows both in a comment.
- **Decoded operands only.** The helper contract is for the operands the CPU used. The filter's decode-time quirks belong to whatever executes guest code (the AD-18 guest), not to a C helper:
  - DIVIDE-BYTE-REGISTER: an AH/CH/DH/BH divisor is read as a dword register;
  - DIVIDE-DISP8-UNSIGNED: a negative disp8;
  - the four-byte read of a narrow memory divisor;
  - `r/m=4` with `mod=3` decoded as SIB, which steps EIP one byte too far for `esp`/`sp`/`ah`.
  - Each is cited by its `docs/retail-bugs.md` id where one exists (add the EIP one if missing). The test excludes these encodings and lists them in a comment.
- **In range**, a helper returns the instruction's quotient and remainder: truncation toward zero, and the remainder takes the dividend's sign.
- **When the instruction would fault** (divisor 0, or quotient out of range, including the most negative dividend divided by −1), a helper returns what the filter writes:
  - **div:** quotient and remainder all ones at the width.
  - **idiv:** the quotient is `0x80000001`/`0x8001`/`0x81` when the sign bit of the dividend's top half (edx, dx, ah) differs from the divisor's sign bit at the operand width; otherwise it is `0x7fffffff`/`0x7fff`/`0x7f`. The remainder is the largest positive value at the width.
  - The rule uses the top half's sign, not the true quotient's sign, exactly as `src/host/runtime/host_rt_divide.c` documents.
- **The test:** `tools/difftest/dividediff.c`, built in `cmake/steps/difftests.cmake` as `bin/difftest/dividediff.exe`, with a `tools/ofs dividediff-run N=...` target.
  - **Retail oracle:**
    - The harness is a 32-bit EXE linked away from `0x400000`. It checks the reference `FLTSIM98.EXE`'s sha256, as `tools/check_data_image.py` does, and maps it at `0x400000` as `hostrtdiff.c` does.
    - It executes a real `div`/`idiv` from an executable buffer, padded so the filter's dword reads of code and memory operands stay in bounds.
    - A vectored handler, gated on that stub's exact EIP, calls retail `0x4119a0` (one-argument `__stdcall`, `ret 4`) with the live `EXCEPTION_POINTERS`. No host startup or SEH frame is needed.
    - The divisor is in a register (not `esp`/`sp`/`ah`) or in memory (modrm, disp8 ≥ 0 and disp32, and SIB forms), at 8, 16 (0x66) and 32 bits, for both div and idiv.
  - **Our filter:** the same, with `fs98_host_rt_divide_filter`.
  - **Exception codes:** wine may report every `#DE` as `0xc0000094`, so either live code is accepted and faults are classified by operands. Both filters are also called directly with synthetic records for `0xc0000094`, `0xc0000095` and an unrelated code, and their return value and context are checked.
  - **Per vector it checks:**
    - the helper's quotient and remainder (both fields, at their declared types) and a call with a NULL `remainder`, against the registers after retail's repair or the clean result, and against ours;
    - on a repair: EAX and EDX replaced (32-bit); both upper words kept (16-bit); EAX[31:16] and all of EDX kept, with al the quotient and ah the remainder (8-bit);
    - the other registers and EFLAGS untouched, the exact EIP step, and the handler hit exactly once on a fault and never otherwise.
  - **Vectors:** a fixed set for each helper, width and encoding:
    - zero divisor;
    - the exact quotient limits and one past each;
    - the full most-negative dividend (`0x80000000:0`, `0x8000:0`, `0x8000`) and the sign-extended minimum, each divided by −1;
    - all four sign combinations of the top half and the divisor, including a divisor whose operand-width sign bit differs from its dword's;
    - ±1 divisors, maximum dividends, and in-range remainders of both signs.
    - Plus N seeded random operands, half of them biased towards faults.
  - **Output:** each mismatch is printed and the run exits 1. It ends with `<k> helpers, <n> comparisons, <d> differ`.
- **Baseline:** run at `N=20000`, then add the result line to the Baselines block of `docs/tools.md`. Check it with `python3 -B tools/cmake_verify.py <build> <new results dir> --only dividediff-run`, and require its PASS record plus the exact summary `6 helpers, <n> comparisons, 0 differ` in the log.
- **Docs:** `docs/architecture.md` (or the numeric section of `docs/method.md`) gets a short "Integer divides" note naming the header and the test. `docs/tools.md` gets the target rows.
- **Running:** wine runs go under `flock ~/openfs98-verify/wine.lock`, never overlapping another run, with a prefix copy rather than the shared `~/.wine` for any `wineserver -k`. Builds use `nice -n 19` and `-j 2`.
- **Isolation:** the work happens in a git worktree off HEAD (`~/source/OpenFS98-110a`), because 6a and 6b0 are still verifying in the main tree and `~/source/OpenFS98-6b0`. The gitignored retail fixtures (`orig/`, per `docs/building.md`) are copied in. This spec stays in the main tree.

**Never:**
- No module calls the helpers yet, and nothing under `src/` changes. `include/fs98/divide.h` is new. The filter and `hostrtdiff` are unchanged.
- No helper models the filter's decoding quirks (DIVIDE-BYTE-REGISTER, DIVIDE-DISP8-UNSIGNED). Those concern how the filter reads the instruction, which a C helper does not have. A test vector that exercises such a quirk is excluded and listed in the test's comment.
- No commits or staging.

## I/O & Edge-Case Matrix

| Scenario | Input | Expected | Notes |
|---|---|---|---|
| div32 zero | `0:5 / 0` | q `0xffffffff`, r `0xffffffff` | |
| div32 overflow | `1:0 / 1` | q `0xffffffff`, r `0xffffffff` | |
| idiv32 sign-extended min/−1 | `0xffffffff:0x80000000 / 0xffffffff` | q `0x7fffffff`, r `0x7fffffff` | top half negative, divisor negative: signs equal |
| idiv32 full min/−1 | `0x80000000:0x00000000 / 0xffffffff` | q `0x7fffffff`, r `0x7fffffff` | |
| idiv32 zero, signs differ | `0xffffffff:0x00000005 / 0` | q `0x80000001`, r `0x7fffffff` | divisor 0 counts as nonnegative |
| idiv16 in range | `dx:ax = 0xffff:0xfff9 (−7) / 2` | q `0xfffd` (−3), r `0xffff` (−1) | truncation toward zero |
| idiv8 overflow | `ax = 0x7f00 / 1` | q `0x7f`, r `0x7f` | |
| div8 in range | `ax = 0x00fe / 0x10` | q `0x0f`, r `0x0e` | al quotient, ah remainder |

</frozen-after-approval>

## Review Triage Log

Spec review (Codex gpt-6.2-sol, 2026-11-25), before approval:

| # | Finding | Verdict | Evidence / action |
|---|---|---|---|
| 1 | Operands-only helpers cannot reproduce the filter's decoding quirks | high | The contract covers decoded operands; the quirks are assigned to the guest and excluded from the test. |
| 2 | `r/m=4` with `mod=3` steps EIP one byte too far | high | Added to the exclusions; the test checks the exact EIP step. |
| 3 | Wine may never deliver `0xc0000095` | high | Either live code is accepted; synthetic records cover 94, 95 and an unrelated code. |
| 4 | The live oracle's ABI and memory setup are unspecified | high | Specified: image base, sha check, stdcall, EIP gate, padding. |
| 5 | Prototypes for the narrow helpers and the signed returns | medium | All prototypes are given, with bit-pattern types; the `cltd` and zero-extend forms are shown. |
| 6 | Matrix labels; the full minimum; the AL/AH swap | medium | Fixed. |
| 7 | Comparing only low parts can pass | medium | Both fields, NULL remainder, register preservation, hit counts and a fixed coverage set are checked. |
| 8 | `cmake_verify` selection, fixtures, untracked files | medium | Fixed. |

## Code Map

- **`src/host/runtime/host_rt_divide.c:28-125`:** `fs98_host_rt_divide_filter(void **pointers)`. It acts on codes `0xc0000094`/`0xc0000095` with an F6/F7 opcode whose modrm reg field is 6 (div) or 7 (idiv), with an optional 0x66 prefix. It decodes the length (SIB at `0x411fe0`), saturates the CONTEXT and steps EIP.
- **`tools/difftest/hostrtdiff.c`:** maps retail at `BASE 0x400000` (about `:29`, `:754`). Operation `0x119a0` (`hostrt_operations.inc:47`) already calls the filter. Reuse its mapping code, not its operation table.
- **Other harnesses** model the repair the same way and are examples of a vectored handler: `tools/difftest/sim1rdiff.c:715` `repair_divide`, `dynamicdiff.c:1306`, `controlsdiff.c:137`.
- **Recipes:** `cmake/steps/difftests.cmake:306-309` builds hostrtdiff and is the pattern for the new target. The `tools/ofs` targets come from `cmake/targets.cmake`, where the `hostrtdiff-run` entry is the pattern.
- **Existing helpers to match** (not to change): `src/sim1/sim1_internal.h:53-100` `sim1_div32`/`sim1_idiv32`/`sim1_div16`/`sim1_idiv16`, which already saturate in C. 1.10c will check them against the new ones.
- **`include/fs98/numeric.h`** holds no divides. It is the style reference for a static-inline header.

## Tasks & Acceptance

**Execution:**
- [x] `~/source/OpenFS98-110a` -- `git worktree add` off HEAD -- isolation.
- [x] `include/fs98/divide.h` -- the six helpers -- the shared rule.
- [x] `tools/difftest/dividediff.c` -- the three-way test -- proof against retail and ours.
- [x] `cmake/steps/difftests.cmake`, `cmake/targets.cmake` -- `dividediff` and `dividediff-run`.
- [ ] Run `tools/ofs dividediff-run N=20000` under the wine lock. Also run `tools/ofs hostrtdiff-run N=20000` (expected unchanged: 161 operations, 3,220,000 comparisons, 0 differ), and `python3 -B tools/cmake_verify.py` for the new line.
- [x] Docs: `docs/tools.md` (target rows and the baseline), the architecture or method note, and `docs/remaining-work.md` if it lists Story 1.10.
- [x] This spec: `## Implementation Notes` with the counts and run times.

**Acceptance Criteria:**
- Given the worktree, when `dividediff-run N=20000` runs, then it reports 6 helpers and 0 differ, and every matrix row is among its fixed vectors.
- Given a deliberately wrong helper (for example the idiv sign rule taken from the low half), when the test is rebuilt and run on a scratch copy, then it reports differences and exits 1. Record this in the notes.
- Given the worktree, `git status --porcelain --untracked-files=all` shows only `include/fs98/divide.h`, the test, the two CMake files and docs.

## Verification

**Commands:**
- `tools/ofs dividediff-run N=20000` -- expected: `6 helpers, <n> comparisons, 0 differ`.
- `tools/ofs hostrtdiff-run N=20000` -- expected: the baseline in `docs/tools.md:308`.

## Implementation Notes

Worktree `~/source/OpenFS98-110a` (off 0f8e49e, `orig/` copied). Files: `include/fs98/divide.h`, `tools/difftest/dividediff.c`, `cmake/steps/difftests.cmake`, `cmake/targets.cmake`, `docs/tools.md`, `docs/architecture.md`, `docs/retail-bugs.md` (new id DIVIDE-SIB-REGISTER-STEP).

- `dividediff-run N=20000`: `6 helpers, 168374 comparisons, 0 differ` (116,645 of the vectors faulted), about 2 s. `cmake_verify --in-place --only dividediff-run`: PASS. The seed-fixed run is deterministic.
- Deliberate break (idiv32 sign rule from the low half, scratch copy of the header): 1356 differ at N=200, exit 1.
- Wine ran in a private prefix, not under `wine.lock`: another run (flightsdiff breaks) held the lock for hours. cmake_verify's slot mode refuses a worktree (`.git` is a file), so `--in-place` was used.
- `hostrtdiff-run N=20000`: 161 operations, 3220000 comparisons, 0 differ (unchanged), about 30 min.

Code review (three Codex gpt-6.2-sol layers, 2026-11-25; B blind, E edge-case, V verification-gap):

| # | Finding | Verdict | Evidence / action |
|---|---|---|---|
| B1 | The architecture note says every faulting divide already uses the helpers | low | True: SIM1, FE and BOOSTER do not yet. Patch: reworded as pending 1.10b/c. |
| B2/E3 | The handler resumes at the filter's EIP, so a wrong step crashes or refaults | low | Real, but loud, not silent. Patch: record the step, resume at the known continuation. |
| B3 | Remainders start at 0, so a helper that skips the store passes | medium | Real for exact divisions. Patch: nonzero sentinels. |
| B4/V | DIVIDE-SIB-REGISTER-STEP is unmarked, and has no regression case | high | `check_retail_bugs.py` exits 1 (reproduced). Patch: a comment marker (no code change) and a synthetic case. |
| B5 | The filter's dword read of a narrow divisor can fault across a page | medium | Pre-existing filter behaviour, not caused here. Deferred. |
| B6/E6 | The signed overflow generator's positive remainder pulls quotients back in range | low | Real: it loses coverage. Patch: the remainder takes the dividend's sign. |
| B7 | No instruction-cache flush | false | x86 keeps instruction fetch coherent with stores to the same process; wine needs no flush. |
| B8/E8 | A malformed N runs zero random vectors | low | Patch: rejected with exit 2. |
| E1 | calloc failure in sha256 | low | Rejected: 192 KB, negligible. |
| E2 | 0x400000 may hold an unrelated mapping | false | The harness links at 0x13000000; as with hostrtdiff, the range is wine's EXE reservation. |
| E4 | Hits are not checked against the operands' expected fault | medium | A deviation from the spec ("exactly once on a fault, never otherwise"). Patch. |
| E5 | EFLAGS outside the arithmetic mask | low | Patch: EFlags compared around the filter call. |
| E7 | No non-F6/F7 synthetic form with /6 or /7 | low | Patch: one added. |
| E9 | AddVectoredExceptionHandler failure | low | Rejected: it never fails in practice, and the run would fail loudly. |
| E10 | No `dividediff` build target, unlike every other harness | low | Patch: added. |
| V1 | BOOSTER (and the other modules) still divide with asm | false | Out of scope by the frozen intent (slices 1.10b and 1.10c). |
