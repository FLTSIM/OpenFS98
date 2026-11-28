---
title: 'Story 1.9, slice c: UTIL''s register wrappers move onto keep.h'
type: 'refactor'
created: '2026-11-28'
status: 'in-progress'
baseline_commit: 'd4a079d67859a37ec772f7f33c5275c84fb80d76'
baseline_note: 'HEAD plus the uncommitted 1.9b slice (spec-1-9b), applied in the worktree before implementation'
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/native-port/architecture/ARCHITECTURE-SPINE.md', '{project-root}/OpenFS98/docs/implementation/spec-1-9b-keep-entries-audit.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** UTIL's 31 register wrappers use a private copy of the keep macros, not `include/fs98/keep.h`, and are outside the 1.9b lint.
- **Where they are:** `util_math.c:493-504` (`rand`, `rand32`) and `:1646-1726` (19 `KEEP_ALL`/`KEEP_ECX_EDX` and 10 raw `ENTRY_NAME` stubs). They are selected by a private `ENTRY()` macro in `util.c:59-95`.
- **What they do:** as in 1.9b, every one only preserves or leaves registers for retail callers. WIN-8's eax counts here, along with the operand and result values the raw stubs reproduce. No UTIL C body reads a register, so "explicit values" holds already.

**Approach:**
- Move every wrapper onto `keep.h` and bind every slot through `FS98_KEPT`.
- Pin all 31 in `tools/keep_entries.txt`, and extend the lint to cover `src/util/`.
- Prove by whole-build byte identity that win32-retail is unchanged, and by the off-branch probe that other builds still compile.

## Boundaries & Constraints

**Always:**
- **Wrappers** (`src/util/util_math.c`): these replace the private macros, all under the existing `FS98_RETAIL_ABI` gate, and keep the same entry names (`fs98_util_<name>_entry`), call symbols, byte counts and instructions.
  - Each `KEEP_ALL`/`KEEP_ECX_EDX` becomes `FS98_KEEP_ALL`/`FS98_KEEP_ECX_EDX`.
  - Each raw stub becomes `FS98_ENTRY(entry, symbol, bytes, count, saves, restores)`. Its explicit stack pushes become the macro's `.rept`, which assembles to the same instructions. Its after-call instructions become `restores`.
  - `ENTRY_NAME` and the private macros are deleted. Their comments, including WIN-8's `RETAIL BUG (kept)` mark, stay beside the converted entries.
- **No-pop entries:**
  - **The problem:** `rand` and `rand32` end in `ret`, while `FS98_ENTRY` always emits `ret $bytes`, which encodes differently.
  - **The new macro:** add `FS98_ENTRY_NOPOP(entry, symbol, count, saves, restores)` to `keep.h`, plus an empty non-retail branch. It is identical to `FS98_ENTRY` with no `.rept` and a plain `ret`.
  - **What stays unchanged:** existing macros and their expansions. ATC's, DYNAMIC's, WEATHERM's and MAIN's `ret $0` entries keep their bytes.
- **Selector** (`src/util/util.c`): include `fs98/keep.h`, delete the `ENTRY()` macro, and write each of the 31 slots as `SLOT(off, FS98_KEPT(fs98_util_<name>_entry, fs98_util_<name>))`.
  - The `_entry` declarations stay under `FS98_RETAIL_ABI`.
  - The slot order and the other 19 slots (16 populated, 3 null) are unchanged.
- **Lint** (`tools/check_keep_entries.py`, `tools/keep_entries.txt`):
  - **Scope:** drop the `src/util/` exclusion.
  - **Table columns:** two trailing columns, `bindfile|slot`. They are required for `src/util/` rows (`src/util/util.c` and the Linkage byte offset) and empty elsewhere.
  - **UTIL binding check:** in the expanded `util.c`, the `Linkage` initializer must hold each tabled entry at exactly its tabled offset. The set of UTIL slots bound through `FS98_KEPT` must equal the 31 UTIL rows. For these rows this check replaces the active-use heuristic.
  - **`FS98_ENTRY_NOPOP`:** recognised in the raw scan. In the expanded scan, its signature is the label's own `.globl`, its saves, the call to the tabled symbol, its restores and a terminal plain `ret`. A NOPOP row whose expansion emits `ret $N` fails.
  - **New class `c-asm`:** the target is a C body whose asm statements are pinned exactly, in order, both ways, by new rows `asm|target|n|kind|statement`.
    - **Statement:** the normalised template, operands and clobbers.
    - **Kind `x87`:** every operand is `"m"`/`"=m"`, the only clobber is `"st"`, and the template uses only `fild*`, `fist*`, `fld*`, `fsqrt`, `fwait` and `%N` placeholders that name declared operands.
    - **Kind `image-end`:** the exact expansion of `FS98_IMAGE_END_IN` from `include/fs98/image_end.h`.
    - **Missing, extra, duplicate or malformed statements fail.**
  - **The 31 UTIL rows:**
    - 27 are class `c`.
    - 4 are class `c-asm`: `fs98_util_sqrt16`, `double_to_int` and `double_to_int64_low`, each with one `x87` statement, and `fs98_util_sqrt32`, with one `image-end` statement.
    - Called helpers, such as `fs98_fsqrt_round`, are not checked, as in 1.9b.
    - The lint reports 46 entries: 39 c, 4 c-asm, 1 thunk-out and 2 thunk-in.
  - **Self-test,** besides the 1.9b fixtures:
    - **Positive fixtures:** `c-asm` and NOPOP.
    - **`c-asm` negatives:** missing, extra, duplicate and malformed statements. Three more fail one rule each: a register constraint, a non-`st` clobber, and a literal `%%eax` in the template. Each of those has its pin updated, so the failure comes from the rule and not from a pin mismatch.
    - **NOPOP negatives:** an untabled `FS98_ENTRY_NOPOP` hidden behind a macro, which must fail in the expanded scan, and a tabled NOPOP expanding to `ret $0`.
    - **UTIL slot negatives:** a slot bound without `FS98_KEPT`, a binding moved under `#if 0` with its `_entry` declaration still present, and two offsets swapped.
  - **Joint removal:** removing a wrapper, its binding and its row together is caught by the gate's byte identity, not by the lint.
- **Docs:**
  - `docs/modules/util.md`'s register section and `docs/tools.md`'s lint row (the counts and UTIL coverage);
  - the `keep.h` comment, for the new macro;
  - in `deferred-work.md`, 1.9c resolved (no register inputs; the C bodies are already explicit) and the 1.9b UTIL-exclusion note updated.
- **Gate** (`~/openfs98-verify/p19c`):
  - **Snapshots:** before is the 1.9b commit, or `d4a079d` plus the reviewed 1.9b delta if 1.9b is still uncommitted. After is before plus only this slice's files. Record both snapshots' identities.
  - **Build:** both sides by `gate/build_side.sh` at one root (nice 19, `-j 2`), with the arena prior. Pass the reviewed 1.9b `keep_prior.txt` to the lint explicitly.
  - **Acceptance:**
    - default `compare.py` `GATE PASS`, plus the 1.9b `verify.py` (exact bytes for non-PEs and replayed objects, and PEs outside the three masks: COFF timestamp, checksum, export timestamp);
    - `gate.sh` uses `pipefail` and checks exit statuses;
    - present on both sides: `UTIL.DLL`, `UTIL_TRACE.DLL`, both `utildiff` executables, and the baseline replay keys for `src/util/util.c` and `src/util/util_math.c`. The whole-build comparison also covers consumers (`slewdiff`, `OFSTEST`).
  - **Negatives,** each on a `cp -al` copy (replace, never write through), each first confirming that its mutation changed the target:
    - a replayed UTIL object replaced: both tools exit 1 naming it;
    - `UTIL.DLL` changed outside the masks: `verify.py` exits 1 naming it;
    - `UTIL.DLL` and its rows removed from both sides: the presence check exits 1 naming it.
  - **ABI-off probe:**
    1. In a disposable copy of after, empty `include/fs98/retail_abi.h`, as 1.9a's probe did.
    2. MinGW preprocesses and compiles (`-c`) `util.c` and `util_math.c` with UTIL's recipe flags.
    3. Both must compile, and neither object may define or reference any `fs98_util_*_entry` symbol.
    4. `util.o`'s `Linkage` relocations must map each of the 31 offsets to its plain function. The expected list is written out independently in `p19c/slots.txt`.
  - **Fallback:** if UTIL's bytes differ at all, stop and report (no runtime fallback). Identical bytes make `utildiff-run`, `reg_contracts.sh` and the flights unnecessary.
  - **Earlier sets:** rerun p18b–p18i, p110b1 and 4b `--strict` only if `compare.py` changes.

**Never:**
- Change any instruction, entry name, call symbol or byte count, or any non-UTIL source, except `keep.h`'s new macro and comment.
- Change the `FS98_UTIL_TRACE` thunks (`util.c:189-198`), a trace-only build.
- Edit `AGENTS.md`, stage, or commit.

</frozen-after-approval>

## Code Map

- **Wrappers:** `src/util/util_math.c:491-504`, `rand`/`rand32`, with no pop.
  - `:1640-1690`: the WIN-8 comment, `ENTRY_NAME`, `KEEP_ALL`, `KEEP_ECX_EDX` and the 19 uses.
  - `:1680-1726`: the raw stubs:
    - `hypot` (count 1, bytes 8);
    - `v48_equal` (0, 8);
    - `V48_ENTRY` add and sub (0, 12);
    - `copy24` (2, 8, saves eax and edx);
    - `shr48` (0, 12);
    - `FIXED48_ENTRY` ×2 (1, 8);
    - `fixed48_to_double` and `_signed` (0, 8).
  - The `FS98_ENTRY` stack offset `4*count+bytes` equals UTIL's hand-written offsets in every case.
- **asm in bodies:** `fs98_util_sqrt32`'s `FS98_IMAGE_END_IN(fs98_util_data)` (`include/fs98/image_end.h:11`); the x87 ones in `fs98_util_sqrt16` (`util_math.c:442-452`), `double_to_int` (`:535-545`) and `double_to_int64_low` (`:652-662`), all under `FS98_FP_X87`. `sqrt64` and `sqrt32` call `fs98_fsqrt_round` (`:170`), and their own bodies hold no asm.
- **Selector:** `src/util/util.c:58-95` (declarations and `ENTRY`), slots `:118-175`.
- **keep.h:** `include/fs98/keep.h:14-33`.
- **Lint:** `tools/check_keep_entries.py` has `CLASS` at `:29`, the UTIL exclusion at `:2,127`, and the wrapper signature and the self-test at the end. Table format: `tools/keep_entries.txt:1-10`.
- **Recipe:** `cmake/steps/all.cmake:242` (UTIL has `-Iinclude -ffreestanding`). The UTIL difftests are in `cmake/targets.cmake:14-30,2036`.
- **Gate reuse:** `~/openfs98-verify/p19b/{gate.sh,verify.py,negatives.sh,keep_prior.txt}` and `p19a/probe.sh:47` (the MinGW probe with `retail_abi.h` emptied). Host gcc cannot compile UTIL (`<windows.h>`, `host.h:716`).

## Tasks & Acceptance

**Execution:**
- [ ] `include/fs98/keep.h` -- `FS98_ENTRY_NOPOP` and its comment -- the no-pop entries.
- [ ] `src/util/util_math.c` -- the 31 wrappers onto `keep.h` -- one copy of the macros.
- [ ] `src/util/util.c` -- `FS98_KEPT` slots, `ENTRY()` deleted -- the AD-4 selector.
- [ ] `tools/check_keep_entries.py`, `tools/keep_entries.txt` -- UTIL coverage, `c-x87`, `NOPOP`, fixtures -- pins them.
- [ ] `docs/modules/util.md`, `docs/tools.md`, `docs/implementation/deferred-work.md` -- docs as above.
- [ ] `~/openfs98-verify/p19c/` -- snapshots, `slots.txt`, `keep_prior.txt`, `gate.sh`, `verify.py`, the presence check, the negatives and the probe.

**Acceptance Criteria:**
- **Lint:** given the after tree, when `check_keep_entries.py --prior p19c/keep_prior.txt` runs, then it reports `46 entries (39 c, 4 c-asm, 1 thunk-out, 2 thunk-in)` and its thunk count, and exits 0.
- **Self-test:** given `--self-test`, when it runs, then every positive fixture passes and every negative exits non-zero with its own diagnostic.
- **Gate:** given both snapshots built, when `p19c/gate.sh` runs, then:
  - `compare.py` and `verify.py` exit 0 with `GATE PASS` and `VERIFY PASS`;
  - the presence check finds every required artifact and replay key;
  - each negative exits 1 naming its file.
- **Probe:** given the ABI-off copy, when the probe runs, then both objects compile, no `_entry` symbol is defined or referenced, and the 31 Linkage relocations equal `p19c/slots.txt`.
- **Other lints:** given the after tree, when `check_abi_headers.py`, `check_arena.py` and `check_div_asm.py` run, then all three exit 0.

## Implementation Notes

## Spec Change Log

## Review Triage Log

## Verification

**Commands:**
- `python3 -B tools/check_keep_entries.py --self-test && python3 -B tools/check_keep_entries.py --prior ~/openfs98-verify/p19c/keep_prior.txt` -- expected: both pass, 46 entries.
- `python3 -B tools/check_abi_headers.py && python3 -B tools/check_arena.py && python3 -B tools/check_div_asm.py` -- expected: pass.
- `bash ~/openfs98-verify/p19c/gate.sh` -- expected: `GATE PASS`, `VERIFY PASS`, the presence check, `NEGATIVES PASS` and `PROBE PASS`.
