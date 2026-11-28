---
title: 'Story 1.10, slice b1: FE, DYNAMIC, ADVDRV and BOOSTER divides use divide.h'
type: 'refactor'
created: '2026-11-25'
status: 'done'
baseline_commit: '27a9e496c2495c57b970c4e265054fce528b76e1'
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/implementation/spec-1-10a-divide-helpers-match-the-host-filter.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** FE, DYNAMIC, ADVDRV and BOOSTER divide with inline `div`/`idiv` asm and rely on the host's trap repair (`0x4119a0`):
- **FE:** `udiv32`, `sdiv32` and `udiv16` (`src/fe/fe.c:251-280`);
- **DYNAMIC:** `udiv`, `sdiv` and `sdiv16` (`src/dynamic/dynamic.c:178-205`, DYN-2);
- **ADVDRV:** `idiv` (`src/advdrv/advdrv.c:567`, ADV-7);
- **BOOSTER:** two inline divides (`src/booster/booster.c:788,797`, BST-4).

They may also hold C `/` or `%` that retail compiles to a divide able to fault. Story 1.10 requires every faulting divide to go through `include/fs98/divide.h` (slice 1.10a).

**Approach:**
- Each wrapper keeps its name and signature and calls the matching helper. BOOSTER's two sites become helper calls.
- The C divides that retail can fault on, found by an audit of each module against its retail disassembly, are converted too.
- The harnesses repair retail's faults exactly as the host does, through our verified filter.
- **The DIVIDE-* quirks:**
  - DYNAMIC has 11 unsigned `div [ebp-disp8]` sites (for example `0x206a254a`, `0x206a2be4` and `0x206a3147`). The host's unsigned path never reads the divisor, so DIVIDE-DISP8-UNSIGNED has no effect there. No `idiv` in the four DLLs uses a negative disp8, a byte-register divisor, or `r/m=4` with `mod=3` (Codex decoded all 58 divides).
  - DYNAMIC's `idivw [esi+1]` (`0x206a1ae0`) is the one quirk left. The filter reads four bytes for a two-byte operand, so a faulting word at the end of a readable page makes retail's repair itself fault. Ours saturates instead. That is the deferred 1.10a filter-read entry, which this slice does not reproduce; the notes say so, and the harness guards that read.
- Proof is behavioural, plus artifact identity everywhere else.

**Split (decided 2026-11-25):** Story 1.10b is three slices: **b1**, this one; **b2**, PANELS (its asm, its C divides such as `edges_24`, and three DIVIDE-DISP8-UNSIGNED sites); **b3**, DEMO (the restructure and two DISP8 sites). The user's decisions for b2 and b3 are in `deferred-work.md`.

## Boundaries & Constraints

**Always:**
- **Helper equivalents** (with `sx(a) = 0u - ((uint32_t)a >> 31)`; typed temporaries for the helpers' `uint32_t *`/`uint16_t *` remainders, converted back to each wrapper's types):
  - **FE:**
    - `udiv32(hi,lo,d)` = `fs98_div32(hi,lo,d,0)`;
    - `sdiv32` = `fs98_idiv32(hi,lo,d,0)`;
    - `udiv16(hi,lo,d,rem)` = `fs98_div16(hi,lo,d,rem)`, whose remainder is a `uint16_t *`.
  - **DYNAMIC:**
    - `udiv(hi,lo,d,rem)` = `fs98_div32(hi,lo,d,rem)`;
    - `sdiv` = `fs98_idiv32(hi,lo,d,0)`;
    - `sdiv16` = `fs98_idiv16(hi,lo,d,0)`.
  - **ADVDRV:** `idiv(a,b,rem)` keeps its `int32_t` quotient and always writes the signed remainder, from `fs98_idiv32(sx(a),a,b,&r)`.
  - **BOOSTER:**
    - `lat_step` = `fs98_idiv32(sx(n-s), n-s, (uint32_t)(int32_t)rows_signed, 0)`, with `n-s` the wrapped `uint32_t` difference;
    - the column step = `fs98_div32(0, e-w, columns, 0)`.
- **The C divides:** Codex's review found none in these four modules that can fault in retail:
  - BOOSTER's `0x20651468` is guarded by a non-zero divisor with a bounded dividend;
  - ADVDRV's COM/BCD divides are by 10 or 100 with EDX 0, and its other divisions are `/2` and a `sizeof` ratio;
  - FE (including `fe_library.c`) and DYNAMIC have no integer C division.

  Confirm each in the notes. A newly found faultable one is converted the same way.
- **Markers:** DYN-2, BST-4 and ADV-7 in `docs/retail-bugs.md`, and their source markers, say ours now returns the host's repaired value instead of trapping. This includes ADV-7's "Ours lets the idiv fault". `tools/check_retail_bugs.py` passes.
- **Links:**
  - Each converted module includes `fs98/divide.h`.
  - Every `-nostdlib` recipe linking a converted module gains `-lgcc`, for the helpers' `___udivmoddi4`/`___divdi3`: `FE.DLL` (`cmake/steps/all.cmake:111`), `DYNAMIC.DLL` (`:90`), and both `FE_sl_ours.dll` recipes (`cmake/targets.cmake:639,1186`).
  - `include/fs98/divide.h` is a declared dependency of those steps.
- **Harnesses** (`tools/difftest/dynamicdiff.c`, `advdrvdiff.c`, `boosterdiff.c`; and FE's `fediff.c`, `fegriddiff.c` and `fesldiff.c` only if the audit finds a site their vectors reach):
  - Edited harnesses that include `fs98/` headers gain `-Iinclude` (ADVDRV, BOOSTER, DYNAMIC and the FE grid harness lack it).
  - **Retail's repair:** an integer-divide fault in the retail module's executable section is repaired by calling `fs98_host_rt_divide_filter` (stdcall, declared and called as `tools/difftest/dividediff.c` does), linked from `src/host/runtime/host_rt_divide.c`, on the live `EXCEPTION_POINTERS`.
  - **Bounds:** the module's section bounds are tracked, and repairs are counted per retail VA outside the compared transcript.
  - **Failures:** each of these fails the run, reported apart from behavioural mismatches:
    - a divide fault outside that section;
    - a fault the filter does not repair;
    - any divide fault on our side after conversion.
  - **Existing verdicts:** non-divide fault handling and existing verdicts are unchanged.
- **Proof** in a worktree `~/source/OpenFS98-110b1` off HEAD, with `orig/`, `cd/` and the other gitignored fixtures the harnesses read copied in. Wine runs go under `flock ~/openfs98-verify/wine.lock`, one at a time, from frozen copies:
  1. **Old baseline:** with the old modules and harnesses, run every covering difftest at its documented baseline (`docs/tools.md`) and pin each result:
     - `fediff-run`, `fegriddiff-run`, `fesldiff-run N=20000`, `facildiff-fe-run`;
     - `dynamicdiff-run`, `advdrvdiff-run`, `boosterdiff-run`;
     - `dividediff-run N=20000`.
  2. **Harness change and directed vectors, with the old modules,** in two controls:
     - **(2a) raw old:** today's harnesses plus the directed vectors. Record each harness's handling of every directed fault, as today: DYNAMIC logs `FAULT(div)` and unwinds; ADVDRV poisons the scenario; BOOSTER exits 3.
     - **(2b) repaired old:** the new repair, applied to both sides, so the old asm on our side is repaired too, scoped to each module's section. DYNAMIC's CHAIN/UTIL faults keep today's handling, and that is stated explicitly.
     - **What both controls pin:** the input stream and the comparison counts, and an account of every old fault that changes the transcript or the continuation.
     - **Harness change:** add the repair.
     - **A manifest** maps every changed retail divide instruction (FE's, all of DYNAMIC's 38 behind the shared wrappers and loops, ADVDRV's, BOOSTER's) to its C caller and its reachable fault cases.
     - **Directed vectors:** each manifest entry gets a fixture that reaches it with a zero divisor (positive and negative dividends), with overflow at both signs where the instruction allows it, and with an in-range signed remainder case:
       - ADVDRV: `x_idiv`/`x_mod` by 0 with both dividend signs, `INT_MIN/-1`, and negative remainders;
       - DYNAMIC: the `udiv`, `sdiv` and `sdiv16` faults;
       - BOOSTER: one dimension zero and the other one (both zero would run 2^32 cell iterations), and negative signed rows with one column and observable cells. Neither BOOSTER site needs an overflow fixture: the 24-bit latitude inputs cannot reach `INT_MIN/-1`, and the column divide starts with EDX 0.
       - Where downstream code can mask the result (FE's strength clamp), the quotient or remainder is asserted directly.
     - **FE harnesses:** they follow the actual reachability. `fediff` reports unhandled faults, `fegriddiff` exits on a divide fault, and `fesldiff` unwinds access violations only. Each gets the repair only if a manifest site is reachable from it; the same goes for `facildiff-fe-run`.
     - **Each directed vector's record:** the retail VAs it must hit, and the expected old and new outcomes for every value it compares.
     - **Pass:** the random vectors keep their pinned results, and each directed vector hits its VAs and shows its recorded old outcome.
  3. **Modules:** convert them, with any divide fault on our side now fatal. Every covering difftest gives its documented count with 0 differ, every directed vector hits the same retail VAs and shows its new outcome, and our side has zero repairs.
  4. **Breaks:**
     - `advdrv-break BREAK=N` (`cmake/targets.cmake:916-918`), N = 1 to 10, at N=200 scenarios, before and after. The recipe prints "caught" for any non-zero exit, so a wrapper captures wine's own exit status and output inside it: only exit 1 with non-zero `differ` counts as caught. The expected result is nine caught and `BREAK=1` equivalent (`docs/modules/advdrv.md:203`). The break fixtures are kept apart from the new directed vectors.
     - FE, DYNAMIC and BOOSTER have documented historical mutation results (`docs/modules/fe.md:88`, `dynamic.md:328`, `booster.md:266`) but no runner. Say so in the notes, and rerun BOOSTER's signed-row mutation by hand if its source still exists.
  5. **Identity:**
     - A `--require-set p110b1` mode in `~/openfs98-verify/gate/compare.py` pins the expected changed outputs by path: the four modules' DLLs, `difftest/FE_sl_ours.dll`, ADVDRV's coverage build, and every harness EXE that compiles their sources or an edited harness.
     - It also pins the expected changed replayed-object keys (the converted modules' units and the edited harnesses), their exact compile-option deltas (`-Iinclude` where added), and the after-only `host_rt_divide.c` keys.
     - `replay.py` drops link flags, so each link recipe's `-lgcc` is verified from the raw compiler log.
     - Every other PE and object must be identical, with equal inventories and the after-side arena `--prior` log present.
     - Both snapshots are built at one root, `~/openfs98-verify/p110b1/root`.
     - After the `compare.py` change, p18b to p18f still pass.
  6. **In the game:**
     - the 74 flights, before and after, with `FRAMES_MANY_JOBS=1 FRAMES_MANY_PASSES=900 tools/frames_many.sh -`, each snapshot's DLL directory pinned. All 74 named flights must produce valid, matching verdicts; `frames_many.sh`'s own exit status is not enough;
     - `main-dialogs` (rerun once on a failure, since it is known to be flaky) and `flights-play`, each with that snapshot's four DLLs explicitly staged;
     - one flight that exercises the real FE–DYNAMIC pair. The two once passed stubbed tests separately and failed together (`docs/modules/dynamic.md:234`).
- **The asm lint:** a new `tools/check_div_asm.py`, run on every build like the arena lint.
  - **The allowlist:** a shrink-only `tools/div_asm_allowed.txt` of per-site records (file, function, mnemonic, the normalised asm text), checked against `--prior`. It starts with PANELS' and DEMO's remaining sites.
  - **No preprocessing:** the lint preprocesses nothing, since generated headers do not exist on a clean build. It scans source text: every `asm`/`__asm`/`__asm__` statement's string literals, with adjacent literals joined and escapes decoded, plus `#define` bodies that hold asm templates. A template it cannot resolve to literals (one taken from an unexpanded macro argument) is reported as an error unless allowlisted.
  - **Checks:** a clean build and a lint-only rebuild.
  - **The scan:**
  - every `.S` file, parsed as assembly;
  - it fails on any bare or suffixed `div`/`idiv` mnemonic, in AT&T or Intel syntax;
  - x87 `fdiv`/`fidiv` are out of scope;
  - `--self-test` covers comments, labels, escaped tabs, adjacent literals, `cltd; idivl`, macro-fed templates and the x87 exclusions.
- **Builds:** `nice -n 19`, `-j 2`. No `wineserver -k` in `~/.wine`.
- **Docs:** `docs/modules/{fe,dynamic,advdrv,booster}.md` where they describe the trap; `docs/tools.md` (the asm lint, the harness repair counters).

**Never:**
- No change to `divide.h`, the filter, PANELS, DEMO or any other module.
- No change to any break mutation.
- No baseline count lowered: any new difference is investigated, not accepted.
- No commits or staging.

**Decided (2026-11-27, user):** the 11 feasible DYNAMIC sites and FE 0x206c2f7f without directed fixtures are deferred (deferred-work.md); 1.10a's helper tests and the arithmetic review stand in.

</frozen-after-approval>

## Code Map

- **Sites:**
  - `src/fe/fe.c:251` (callers `2963`, `3613`), `:259` (`3493`), `:274` (`2027`, `2030`, `3589`, `4105`);
  - `src/dynamic/dynamic.c:178` (24 callers), `:189` (`965`, `1185`), `:198` (`1022`; retail `idivw [esi+1]` at `0x206a1ae0`, disp8 +1);
  - `src/advdrv/advdrv.c:564-571` (retail `idiv esi` at `0x20613029/69`; callers `x_idiv`/`x_mod` `1916`/`1925`);
  - `src/booster/booster.c:776-803`.
- **Harnesses:**
  - `tools/difftest/dynamicdiff.c:1287-1315` (`in_reach` also covers CHAIN and UTIL: bound it to DYNAMIC's section), `:1659`, `:1831`, `:1946`;
  - `advdrvdiff.c:2303-2329` (no IP guard today), `:2800`;
  - `boosterdiff.c:47`, `:1374` (the generator excludes zero dimensions), `:1993` (`on_fault` exits);
  - `fegriddiff.c:154`, `fesldiff.c:1635-1661`.
- **The filter:** `src/host/runtime/host_rt_divide.c:28-143`, which emits `_fs98_host_rt_divide_filter@4` with no link dependencies. `tools/difftest/dividediff.c:36,138` shows how to call it.
- **Gate:** `~/openfs98-verify/gate/compare.py` (pinned replay changes as in `p18d`), `replay.py:25`, `ondemand.py:11`.
- **Cost:** about 3–4 h of wine for steps 1–3, plus about 2 h for the flights.

## Tasks & Acceptance

**Execution:**
- [ ] Worktree and fixtures. Step 1: the old baseline, pinned.
- [ ] The C-divide audit, recorded.
- [ ] The harnesses and directed vectors (step 2), run with the old modules.
- [ ] The four modules, their recipes (`-lgcc`, `divide.h` dependency) and the markers.
- [ ] `tools/check_div_asm.py` and its allowlist, wired into the build.
- [ ] Steps 3–6; `## Implementation Notes` with every count, counter, VA and differing artifact.

**Acceptance Criteria:**
- **Step 2:** the random vectors keep their pinned results, and every directed vector hits its VAs with its recorded old outcome.
- **Step 3:** every covering difftest gives its documented count with 0 differ, every directed vector has its new outcome, and our side has zero repairs.
- **Steps 4–6:** `advdrv-breaks` holds under the strict rule, the gate shows only the pinned outputs differing, and the flights, `main-dialogs` and `flights-play` pass.
- **The asm lint:** `check_div_asm.py` passes, its allowlist names only PANELS and DEMO, and `check_retail_bugs.py` passes.

## Verification

**Commands:**
- `python3 -B tools/check_div_asm.py --self-test && python3 -B tools/check_div_asm.py` -- expected: pass.
- `python3 -B tools/check_retail_bugs.py` -- expected: pass.
- The step-3 difftest list -- expected: the documented baselines, 0 differ.

## Review Triage Log

Two Codex gpt-6.2-sol rounds (2026-11-25) ran on the combined 1.10b draft before it was split. The items that apply to b1:

| # | Finding | Verdict | Evidence / action |
|---|---|---|---|
| 1 | `-nostdlib` recipes need `-lgcc` (the helpers emit libgcc calls) | blocker | A compiler probe showed it. Added; round 2 confirmed the list. |
| 2 | "0 differ" with the old modules is the wrong control | high | Now a pinned old baseline, then directed vectors with recorded old and new outcomes, zero repairs on our side after conversion, and counters outside the transcript. |
| 3 | Repair scope: `div_fixup` differs from the host; handlers lack IP bounds | high | Our filter, bounded to the module's section; unexpected faults fail the run. |
| 4 | ADVDRV and BOOSTER contracts (signed remainder, sign-extended rows) | high | Corrected. |
| 5 | The identity gate needs a pinned mode and the `host_rt_divide.c` replay keys | high | `--require-set p110b1`. |
| 6 | An AC grep misses `\tidivl`; macro-fed asm | high | `tools/check_div_asm.py` with a shrink-only allowlist. |
| 7 | BOOSTER's zero-dimension fixtures need repair and storage | high | Added. |
| 8 | ADVDRV breaks accept any non-zero exit | high | Driven case by case under the strict rule. |
| 9 | C divides can fault where no asm exists (PANELS `edges_24`) | blocker (b2) | b1 audits its four modules' C divides; PANELS moved to b2. |
| 10 | DISP8 sites in DEMO and PANELS | blocker (b2, b3) | Moved out with the user's decision recorded in `deferred-work.md`. |

Round 3 (Codex gpt-6.2-sol, 2026-11-25), on this split spec:

| # | Finding | Verdict | Evidence / action |
|---|---|---|---|
| R1 | The "no negative-disp8" premise is false (DYNAMIC has 11 unsigned ones), and `idivw [esi+1]` has the dword read | blocker | Corrected: the unsigned sites are unaffected; the dword read is the deferred 1.10a quirk, not reproduced and guarded in the harness. |
| R2 | The old-module control contradicts today's fault policy | blocker | Two controls: raw old, then repaired old on both sides; the converted run treats our faults as fatal. |
| R3 | The directed proof can miss a bad substitution | high | A manifest of every changed instruction; both dividend signs and remainders; direct asserts where masked; FE reachability. |
| R4 | The gate needs object and option exceptions too | high | Pinned object keys, option deltas, and `-lgcc` from the raw log. |
| R5 | The lint cannot preprocess on a clean build; per-file counts allow swaps | high | A source-text scan with per-site records; templates it cannot resolve are errors. |
| R6 | The ADVDRV break recipe hides wine's exit | high | Captured inside the recipe; 9 caught and BREAK=1 equivalent pinned; FE/DYNAMIC/BOOSTER history noted. |
| R7 | BOOSTER fixture bounds | medium | One dimension zero and the other one; no overflow fixture needed. |
| R8 | The in-game runs must pin what they load | medium | DLL dirs pinned, 74 named verdicts, staged DLLs, an FE–DYNAMIC flight. |
| — | C-divide audit | verified | Codex found no faultable integer C divide in the four modules. |

## Implementation Notes

Worktree `~/source/OpenFS98-110b1`. Evidence under `~/openfs98-verify/p110b1/` (`verdict.sh` prints the pass/fail of steps 3 to 6 from the logs).

**Changed.** `src/{fe,dynamic,advdrv,booster}/*.c` call `fs98/divide.h` (FE `udiv32`/`sdiv32`/`udiv16`, DYNAMIC `udiv`/`sdiv`/`sdiv16`, ADVDRV `idiv` as `fs98_idiv32(sx(a), a, b, &r)`, BOOSTER's two sites); `-lgcc` on `FE.DLL`, `DYNAMIC.DLL` and both `FE_sl_ours.dll` recipes (ADVDRV and BOOSTER already had it; `include/fs98/` headers are already declared inputs of every step); DYN-2, BST-4 and ADV-7 and their source markers say ours returns the host's repaired value; `tools/difftest/divrepair.h` (the harness repair, per-VA counters outside the transcript, the failures above, `DIVREPAIR=off` raw control, `DIVREPAIR_OURS=repair` repaired-old control) hooked into `advdrvdiff.c`, `boosterdiff.c`, `dynamicdiff.c`, which now build with `-Iinclude -Isrc/host/runtime` and `src/host/runtime/host_rt_divide.c`; directed vectors in `advdrvdiff.c` (38) and `boosterdiff.c` (9); `tools/check_div_asm.py` (+ `--self-test`) and `tools/div_asm_allowed.txt` (PANELS' four sites, DEMO's one), run by `ofs_arena_lint` on every build; docs (`tools.md`, the four module notes, `retail-bugs.md`). `check_retail_bugs.py` passes (335 entries, 327 marked, 8 no longer apply).

**C-divide audit.** Confirmed by reading the four modules and `fe_library.c`: no integer C `/` or `%` that can fault. The harness's own `index_boxes` divides by `rows`/`cols`; guarded for the zero directed vectors.

**Step 1, old baseline** (old modules and harnesses, frozen HEAD tree, wine under the lock): dividediff 6 helpers 168374 comparisons 0 differ; fediff 20000, 0 differ; fegriddiff 7742499 comparisons 0 mismatches; fesldiff N=20000 0 differ; facildiff-fe 2000+2000, 0 differ; dynamicdiff 20000, 0 differ (faults 4039, hangs 395); advdrvdiff 20000, 0 differ; boosterdiff 20000, 0 differ.

**Step 2, new harness with the old modules.** *2a raw* (`DIVREPAIR=off`): ADVDRV directed: 38 vectors, both sides `FAULT(div)` and unwind on every zero divisor and on INT_MIN/-1, in-range values agree, 0 differ. BOOSTER directed, one vector a run: vectors 0 to 3 (column or row count 0) exit 3 at retail `0x2065258d` (column `div`) or `0x20652549` (row `idiv`); vectors 4 to 8 (negative rows, in range) complete, transcripts 14809, 10990, 8976, 22599, 17921 bytes, the same bytes as under repair. *2b repaired old* (`DIVREPAIR_OURS=repair`, both sides repaired, scoped to each module's section; DYNAMIC's CHAIN/UTIL faults keep today's handling): advdrv 20000 scenarios 0 differ, directed 38 vectors 0 differ, retail repairs `0x20613029` 39284 (first at scenario 1) and `0x20613069` 83475; booster 20000, 0 differ, directed 9 vectors 0 differ, `0x2065258d` x21 and `0x20652549` x16, all in the directed scenarios (none in the random ones); dynamic 20000, 0 differ, 26879 repairs, 0 outside the section, at 12 VAs (below). Recorded old outcomes (retail, repaired): `x_idiv(5,0)` 2147483647, `(-5,0)` -2147483647, `(INT_MIN,-1)` 2147483647; `x_mod` by 0 or `INT_MIN/-1` 2147483647; `(-7,2)` q -3 r -1, `(7,-2)` q -3 r 1, `(-7,-2)` q 3 r -1; BOOSTER copies observed 0/7/10/4/0/0/5/11/2 for vectors 0 to 8.

**Manifest (retail VA, C caller, fault cases, fixture).** Every changed retail divide instruction. Fixtures are directed (ADVDRV, BOOSTER) or pinned scenarios (DYNAMIC: `dyn_pins` in `dynamicdiff.c`, run by `DYN_DIRECTED=1`, each asserting the retail VAs it repairs and the crc of retail's transcript). "Impossible" means the instruction cannot fault, with the reason.

ADVDRV (`idiv esi` after `cdq`): `0x20613029` `x_idiv` and `0x20613069` `x_mod` through `idiv()`: divisor 0 and INT_MIN/-1 fault; 38 directed vectors (19 per operator: zero with both dividend signs, INT_MIN/-1, INT_MIN/1, INT_MAX/-1, negative remainders), each asserting the VA, the fault or repair outcome and the exact result.

BOOSTER: `0x20652549` `idiv` of the latitude difference (sign-extended) by the signed row word, `lat_step` in `booster_grid`, faults for rows 0 only (the dividend is 24-bit scaled, no overflow); `0x2065258d` `div` of the longitude difference (EDX 0) by the column word, faults for columns 0 only. Directed vectors 0 to 3 (columns 0; rows 0 with both orientations) assert the VA; vectors 4 to 8 (negative rows, in range) assert no repair.

DYNAMIC (38, `0x206a....`; caller, fault cases, fixture):
- Impossible, a non-zero constant divisor with EDX 0 (or one the code makes at least 1): `12a9` (`dyn_frame_ecx`, / 0x100), `12c2` (/ 0x6000), `12cc` (/ (quotient + 1)), `245e`, `27ee`, `2915`, `2d50`, `30f7` (`/ 3` after `xor edx,edx`), `2831`, `28a0`, `315c` (`/ n`, `n >= 1` from `arc_steps`' `jne; inc eax`, EDX 0), `281c`, `288b`, `3147` (the arc unit `0x10000:0 / ecx` can only be zero or saturate to a non-zero value, EDX 0), `2baf` (EDX at most 0xffff against a divisor of at least 0xa0000).
- Observed and pinned (12): `1f6c` (`path_point`, `div ebx`, duration) scenario 6123; `254a` (`op_to_aircraft`, steering loop 1, divisor the distance, 0 or smaller than `F32(0x14)`) and `26b8` (loop 2) scenario 48; `2b7d` (`op_refuel`, `0x3c * total / [esi+1]`, program word 0 or overflow) 2439; `2b9a` (`/ 100`, EDX >= 100 after `mul`) 884; `2be4`, `2c0a`, `2c30`, `2c56`, `2c7c` (the five `div [ebp-0x20]`, the proportional total 0 or less than room/2) 2492, 1384, 7407, 1384, 2966; `2c89` (the fifth tank's `div [capacity]`) 19373; `2fb4` (`op_turn_to`, `div [ebp-0x68]`, a distance) 46.
- Feasible, not observed in the random stream: `2bf1`, `2c17`, `2c3d`, `2c63` (the other four tanks' `div [capacity]`, capacity 0 in proportional mode; `DYN_FORCE=cap0..cap4` forces them), `185a` (`op_origin`, `0x10000:0 / S_M_TO_LON`, a metres-to-longitude scale 0 or at most 0x10000), `1981` (`approach`, `idiv [S_LEFT]`) and `1c2e` (`op_turn`, `idiv [S_LEFT]`: needs `S_LEFT` 0 or a quotient past 31 bits), `1ae0` (`ramped`, `idivw [esi+1]`, the program word 0 or a quotient past 16 bits, with the page-edge filter read the deferred 1.10a entry), `2469` (`op_to_aircraft`, `0x10000:0 / ecx`, `ecx = [ebx+0x44] * 0x6487e / 3` zero for a zero field), `2d5b` (`op_turn_to`, the same with the word at `[edi+0x2c]`), `35b3` (`op_circuit`, `div [S_SPEED_TO]`, 0).

FE (7, `0x206c....`; the C caller in `fe.c`, fault cases, reachability):
- `2c83` `block_scale` (`udiv32(0, ecx, table[idx])`): the divisor is a word of the cosine table at `+0x1320` (64 entries, the smallest `0xf61d`, none zero) and EDX is 0: impossible.
- `2f7f` `strength` (`sdiv32(0, range, ebx)`, `idiv ebx`, `ebx` the 16-bit hypot of the aircraft's offset from the navaid): zero when the aircraft is exactly on the navaid (the quotient `0x7fffffff` is then clamped to `0x7fff` by the next test, so the repaired quotient cannot be seen in FE's output); also a quotient past 31 bits for `ebx` 1 with a large range. The only feasible FE site, and not reached by fediff's random positions (its baseline is fault-free: a divide fault stops it). No directed fixture was built: it needs a navaid record's coordinates fed back as the aircraft position.
- `30c6` `dist_level` (`udiv16(eax >> 16, eax, 1000)`): overflows only past 65,535 km (the source comment): impossible on the globe (at most about 20,000 km).
- `30da` `dist_level` (`udiv16(0, ax, 10)`): impossible. `50c4` `channel` (`udiv16(0, ax, 5)`): impossible.
- `4daf` `gate` (`udiv32(h, 0, d)`): reached only with `0x10 < h <= 0x98` and `d > 0x15b4`, so `h:0 / d < 2^32`: impossible.
- `5233` `height_sample` (`udiv16(hi, lo, M16(D + 0x117))`): the divisor is the image constant `0x989d` (never written); the overflow needs `((lat + 0x989d56) << 7) >> 16 >= 0x989d`, i.e. `lat + 0x989d56 >= 0x1313a00`, and the largest latitude (10,000,000, the pole) gives `0x130ee56`: impossible.
So fediff, fegriddiff and fesldiff gain no repair (their baselines are fault-free and the one feasible site was not reached); the 1.10a helper tests carry the arithmetic.

**Final results** (evidence in `~/openfs98-verify/p110b1/`; `verdict.sh`). All serial, under the lock, harness exes and DLLs identified in `ctl-results/*.identities` and `final-results/*.identities`.
- *Step 3, new modules:* dividediff 6 helpers 168374 comparisons 0 differ; fediff 20000, 0 differ; fegriddiff 7742499 comparisons 0 mismatches; fesldiff N=20000 and facildiff-fe IDENTICAL, 0 differ; dynamicdiff 20000, 0 differ, retail 26879 repairs at 12 VAs, ours 0, outside/unrepaired/guard 0; advdrvdiff 20000, 0 differ, directed 38 vectors 0 differ 0 faults, retail 122759 repairs (`20613029` x39284, `20613069` x83475), ours 0; boosterdiff 20000, 0 differ, directed 9 of 9 run, retail 37 repairs (`2065258d` x21, `20652549` x16), ours 0. Counts equal the step-1 baselines.
- *Directed DYNAMIC fixtures:* `DYN_DIRECTED=1` against the new modules: 10 pinned scenarios, every one repairing its pinned VAs and matching its pinned retail transcript crc, 19374 scenarios generated, 0 differ (`final-results/dyn-directed-new.log`; the old modules, repaired, the same). `DYN_FORCE=cap0..cap4` (600 scenarios each, `ctl-results/dyn-force-cap*.log`) repaired only `254a`, `26b8` and `1f6c`: the forced scenarios never reach the refuel's tank divides, so no new fixture could be pinned.
- *Controls at baseline scale (step 2):* ADVDRV raw: 20000 scenarios 0 differ, directed 38 vectors 0 differ, 24 faults unwound (both sides fault on every zero divisor and INT_MIN/-1), no repairs; repaired old: 20000, 0 differ, directed 0 faults, the same 122759 repairs as the new run, ours' repairs equal. BOOSTER raw: vectors 0 to 3 exit 3 at `2065258d` (columns 0) or `20652549` (rows 0), vectors 4 to 8 complete, 20000 random scenarios 0 differ; repaired old: 20000 and 9 of 9 directed, 0 differ. DYNAMIC raw: faults 4036, hangs 395; repaired: faults 1831, hangs 404, 0 differ both.
- *Accounting for DYNAMIC (`ctl-results/dyn-raw.scen`, `dyn-repaired.scen`, `dyn-oldharness.scen`):* raw to repaired, 443 scenarios change their transcript (all had a raw fault) and 9 of them (2271, 2668, 3049, 13129, 14208, 14736, 14939, 15517, 19665) become hangs because the run now continues after the divide into a spin: 395 + 9 = 404, and the faults fall by 2205 to 1831. The step-1 count of 4039 against 4036: the old harness rebuilt with a per-scenario log gives 4039 again, and differs from the new harness's raw run in exactly one scenario, 14102 (11 faults against 8, no hang in either, 0 differ retail against ours in both); the other 19999 scenarios have identical fault and hang counts. Run alone (`DYNONLY`) 14102 hangs in both exes, so the cause could not be isolated; it is a scenario that depends on the harness's address-space layout (the new exe links `host_rt_divide.c` and `divrepair.h`, moving what a stray address lands on), not on a wine load: the step-2 runs, which were parallel, gave the same 4036 and 395. The step-2 hang count of 404 came from the repaired control, not the raw one (395 in the serial raw run).
- *Breaks after:* BREAK 1 missed (the documented equivalence), 2 to 10 caught (3, 50, 27, 148, 33, 7, 132, 78, 110 differ), strict rule, no `DIVREPAIR FAIL`: 9 caught, equal to before.
- *BOOSTER signed-row mutation:* the source is gone (no `BREAK` in `booster.c` in any commit, no runner); the hand mutation (`rows` unsigned) at N=300: `new-results/boo-mut-run.log` shows 0 differ in the random scenarios (the generator has no signed-negative rows), and the directed run exits 1 (`boo-mut-directed.log`): the directed vectors catch it.
- *Lint evidence:* lint-only rebuild and `check_div_asm.py --prior` (pinned copy of the allowlist): `new-results/lint-only-build.log`, `div-lint-prior.log`, `gate/after/div-lint-prior.log`, all rc 0, 5 allowlisted sites.
- *Identity gate, rebuilt on the final sources at `~/openfs98-verify/p110b1/root`:* `compare.py --require-set p110b1` GATE PASS (1130 artifacts, 321 PEs, 206 objects, 524 replayed units); p18b, p18c, p18d, p18e, p18f, p18g, p18h and 4b `--strict` all GATE PASS (`final-results/gate-all.txt`).
- *In the game (`ingame-results/`):* before and after each 74 of 74 named flights `match` (checked by name), `NEWYORK` (NEWYORK5.BGL's oversized block, FE `+0x40` feeding DYNAMIC's probe) match with 1556 distinct frames and 0 faults, main-dialogs rc 0, flights-play rc 0.

**Final round results** (`final2-results/`, `breaks-after2/`, `gate/`, `newyork-trace/`; all wine runs under the lock). DYNAMIC `DYN_DIRECTED=1`: converted modules and old repaired both `10 pinned fixtures executed of 10, 19374 scenarios generated`, 0 differ, every exact repair count, transcript crc and continuation (image, GLOBAL, arena use) equal to its pin; `DYNONLY=0 DYN_DIRECTED=1` exits 1 (0 of 10 ran). ADVDRV directed: converted and old repaired 38 vectors, 0 differ, 0 faults (12 retail repairs, ours 0 / equal in the control); raw 38 vectors, 24 faults unwound, each an integer-divide exception at the pinned retail VA. BOOSTER directed: converted and old repaired 9 of 9 run, 0 differ, 37 retail repairs (`2065258d` x21, `20652549` x16), every vector equal to its pinned count, length, copies and crc; raw: vectors 0 to 3 exit 3 at the pinned VA, vectors 4 to 8 complete at their pinned crcs. Strict ADVDRV breaks after: BREAK 1 missed (equivalence), 2 to 10 caught (3, 50, 27, 148, 33, 7, 132, 78, 110). `check_div_asm.py --self-test` pass, tree: 5 allowlisted sites, `--prior` rc 0, lint-only rebuild rc 0. Gate rebuilt at `~/openfs98-verify/p110b1/root`: `--require-set p110b1` PASS (1130 artifacts, 321 PEs, 206 objects, 524 replayed units), p18b to p18h and 4b `--strict` PASS. The saved 74-flight results re-checked with the new `verdict.sh`: 74 of 74 exactly one valid row each, old and new, main-dialogs and flights-play rc 0; NEWYORK flown once more per side with `+file`: NEWYORK5.BGL opened 18 times in each of the three games, equal. The trace shows the file the oversized block comes from was read the same number of times by retail and ours; it cannot show the probe's register hand-off itself, which `docs/modules/dynamic.md` documents.

**Without a directed fixture** (every one is either proven unable to fault, above, or listed here). DYNAMIC, feasible: `2bf1`, `2c17`, `2c3d`, `2c63` (the other four tanks' capacity divides; `DYN_FORCE` did not reach them), `185a` (`S_M_TO_LON`), `1981` and `1c2e` (`S_LEFT`), `1ae0` (`idivw [esi+1]`, also the page-edge filter read, the deferred 1.10a entry), `2469` and `2d5b` (`0x10000:0 / ecx` with a zero field), `35b3` (`S_SPEED_TO` 0). FE, feasible: `0x206c2f7f` (`strength`, aircraft exactly on a navaid). The FE, DYNAMIC and BOOSTER historical mutation results have no runner; FE and DYNAMIC were not rerun.

**ADVDRV breaks, before** (old harness and module, N=200, exit 1 and `differ > 0` required, `~/openfs98-verify/p110b1/advdrv_breaks_strict.sh`): BREAK 2 to 10 caught (3, 45, 26, 140, 30, 6, 125, 77, 106 differ), BREAK 1 missed, the documented equivalence: 9 caught.

**Step 5, identity** (`~/openfs98-verify/gate/compare.py --require-set p110b1`, both snapshots built at `~/openfs98-verify/p110b1/gate/root`, the after-side arena `--prior` log present): GATE PASS, 1130/1130 artifacts, 321 PEs, 206 objects, 524 replayed units. The differing PEs are exactly `ADVDRV.DLL`, `BOOSTER.DLL`, `DYNAMIC.DLL`, `FE.DLL`, `difftest/ADVDRV_ours.dll`, `difftest/FE_sl_ours.dll`, `difftest/{advdrvdiff,boosterdiff,dynamicdiff}.exe`; no object file differs; the differing replayed objects are `fe.c`, `dynamic.c`, `booster.c`, `advdrv.c` (twice) and the three harness units; the harness units carry one added `-Iinclude -Isrc/host/runtime`; `host_rt_divide.c` is replayed once after only (the three identical harness compiles fold); `-lgcc` was added to exactly the three recipes. p18b, p18c, p18d, p18e and p18f still pass.

**Review round** (Codex, 2026-11-26, `~/openfs98-verify/p110b1/code-review.out`; the 49 helper substitutions were confirmed correct):

| # | Finding | Verdict | Action |
|---|---|---|---|
| 1 | Directed tests judged by transcript equality, `%.0f`, unchecked stack depth, a fixed vector count | high, fixed | ADVDRV asserts per vector the fault case, the retail VA and its repair count (exactly one at `0x20613029` or `0x20613069`), ours' repairs by control, and the exact result (`double ==`, printed `%.17g`, expected values written apart from `divide.h`); the stack depth is checked before the read; BOOSTER asserts the VA set per vector, reports `n vectors run of 9` and fails when `BOOSTER_ONLY` selects none |
| 2 | `DIVREPAIR FAIL` counted as a caught break | high, fixed | exit 4 (`DIVREPAIR_EXIT`) apart from 1; `advdrv_breaks_strict.sh` rejects exit 4 and the text |
| 3 | The 49 substitutions | pass | none |
| 4 | The guard rejected a repair the host performs | medium, fixed | the divisor read is guarded only for `0x66 F7 /7` memory forms; the instruction bytes (12) are checked readable before the filter is called |
| 5 | The lint missed split literals, dialect alternatives, numeric labels, stringified macros, duplicate rows | high, fixed | adjacent literals are joined, `{a\|b}` alternatives and `1:` labels handled, any name in a template must expand to literals (a stringified parameter only where no mnemonic can stand), an `#include` in a template is skipped, the allowlist is compared with multiplicity (also against `--prior`); each case is in `--self-test` |
| 6 | No FE or DYNAMIC proof | blocker, partly fixed | the manifest above (all 38 DYNAMIC and 7 FE VAs with caller and fault analysis, 15 DYNAMIC and 6 FE sites proven unable to fault); pinned fixtures for the 12 observed DYNAMIC VAs; FE's one feasible site and 11 DYNAMIC sites remain open (see above) |
| 7 | Controls not at the same scale, not under the lock, unexplained fault and hang counts | high, fixed | both old controls rerun at 20000 under the lock with `DIVREPAIR_SCENLOG` per-scenario records and input identities (`ctl-results/`); the hang and fault changes are accounted for scenario by scenario (Final results) |
| 8 | In-game preflight missing `FRAMELOCK.DLL` and `SEHFIX.DLL`; verdicts aggregated; no FE-DYNAMIC interaction | blocker, fixed | `tools/ofs framelock sehfix` built in both trees; `verdict.sh` checks every one of the 74 named flights; `NEWYORK` (NEWYORK5.BGL's oversized dynamic block) is flown first on each side |
| 9 | The `host_rt_divide.c` exception skipped status and object checks; root differs from the spec | medium, fixed | `compare.py` requires the full key (cwd, options), replay rc 0 and a non-empty object; the snapshots are rebuilt at `~/openfs98-verify/p110b1/root`; after-side `div-lint-prior.log` is required |
| 10 | BOOSTER signed-row mutation | pending, resolved | source not found (above); hand mutation run |
| 11 | Lint-only build, divide `--prior`, `nice -n 19 -j 2` | medium, fixed | `new-results/lint-only-build.log` and `div-lint-prior.log`; `build_side.sh` runs the divide lint `--prior` (`DIV_PRIOR`); every proof script sets `nice -n 19` and `CMAKE_BUILD_PARALLEL_LEVEL=2` |

**Final review round** (Codex, 2026-11-27, `rereview.out`; the user's decisions of 2026-11-27 are in the frozen block and `deferred-work.md`: the 11 DYNAMIC sites and FE `0x206c2f7f` without fixtures are deferred, scenario 14102 is logged as unexplained):

| # | Finding | Verdict | Action |
|---|---|---|---|
| 1 | FE and DYNAMIC fixtures incomplete | blocker, decided | deferred by the user (frozen block); each VA, caller and what its fixture needs is in `deferred-work.md` |
| 2 | Lint missed macro halves, `" " #op`, a newline inside a dialect alternative, five labels | high, fixed | `check_div_asm.py` now expands macros itself at each asm statement (object and function-like, `#`, `##`, across files), joins the template, expands dialect alternatives before splitting, strips any number of labels, and reports an unresolved template as a site; a case for each in `--self-test` |
| 3 | DYNAMIC pins accepted extra repairs, could all be skipped | high, fixed | exact per-VA repair counts (and no other VA), plus the continuation (retail image crc, GLOBAL crc, arena use) beside the transcript crc; the run fails unless every pin ran; "executed of requested, scenarios generated" are reported apart; `DYNONLY=0 DYN_DIRECTED=1` now exits 1 (`final2-results/dyn-negative.log`) |
| 4 | BOOSTER outcomes unpinned | high, fixed | per vector the retail repair count, the transcript length and crc and the copies are pinned from the repaired-old control, asserted in the raw (vectors 4 to 8), old-repaired and converted runs |
| 5 | ADVDRV raw fixture accepted any fault | medium, fixed | the exception code and IP are recorded; raw requires an integer-divide code at the pinned retail VA (ours: in its own image) |
| 6 | Read guard probed 12 bytes | medium, fixed | staged: first byte, opcode and ModR/M, then the SIB/displacement bytes the filter fetches by rm; the divisor read only for signed 16-bit memory forms |
| 7 | Strict breaks inherited the control variables; watchdog labelled as a repair failure | medium, fixed | the wrapper clears `DIVREPAIR`, `DIVREPAIR_OURS`, `DIVREPAIR_SCENLOG`; repair failure exits 5 (`DIVREPAIR_EXIT`), the watchdog's 4 is its own verdict |
| 8 | After-only replay object not validated | medium, fixed | `compare.py` runs `coff_ok()` on it (`coff_ok` moved above `man()`); a one-byte object fails the gate (checked) |
| 9 | In-game scripts accepted stale or conflicting evidence; no NEWYORK5.BGL evidence | medium, fixed | `ingame.sh` takes a fresh output directory and exits on staging or preflight errors; `verdict.sh` requires exactly one valid row for each of the 74 names and exits non-zero on any failure; `HG2D_FRAMES_WINEDEBUG=+file` (new in `hg2d_frames.sh`) counted the opens of NEWYORK5.BGL in each NEWYORK game: 18 in retail-a, retail-b and ours, on both sides (`newyork-trace/*.counts`) |

**Deviations.** The first runs of step 2 used private wine prefix copies (three in parallel) while the old baseline held the lock, before the lock rule was restated; every step-3 run and every later run is under `flock ~/openfs98-verify/wine.lock`. Step 2 used a tree copy (`ctl`) of the old modules with the new harness. The "FE-DYNAMIC pair" flight is every flight of the 74-flight run, which loads all-ours FE and DYNAMIC together.
