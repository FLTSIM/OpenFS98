---
title: 'Story 1.10, slice b1: FE, DYNAMIC, ADVDRV and BOOSTER divides use divide.h'
type: 'refactor'
created: '2026-11-25'
status: 'in-progress'
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
