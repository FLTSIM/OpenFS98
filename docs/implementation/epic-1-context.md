# Epic 1 Context: The wine build follows the port rules

<!-- Compiled from planning artifacts. Edit freely. Regenerate with compile-epic-context if planning docs change. -->

## Goal

Move the retail-ABI build (the decompiled game running under wine) onto the shared rules every later native target will use: CMake instead of the Makefile, one compiler flag set, 32-bit ABI headers, one arena header, explicit parameters instead of register contracts, saturating divides, every kept retail bug marked, no Microsoft bytes in git, and retail reference logs recorded. All 74 flights must stay frame-identical to retail throughout. This enables the native i386, x86-64, Windows x64 and Mac ports, which would otherwise fork the code per platform.

## Stories

- Story 1.1: CMake builds everything the Makefile builds
- Story 1.2: CMake runs every verification target, and the Makefile is retired
- Story 1.3: One flag set for every unit
- Story 1.4: Module data decompiled, no raw dumps in git
- Story 1.5: Every kept retail bug is marked in the code
- Story 1.6: Retail reference logs recorded
- Story 1.7: The module ABI headers are 32-bit
- Story 1.8: Every module uses the shared `arena.h`
- Story 1.9: Register contracts become explicit parameters
- Story 1.10: Divides go through the saturating helpers

## Requirements & Constraints

- Regression gate for every story: all 74 flights frame-identical to retail, the difftest baselines in `docs/tools.md`, and `main-dialogs`/`flights-play` pass. Behaviour must not change; this epic is restructuring.
- No Microsoft bytes in the repo or releases: no raw `.data`/`.rdata` dumps, no retail resources. Module initial data is decompiled into typed C initialisers, verified byte for byte against retail's image. Only resources still come from `orig/` at build time (`win32-retail`); a build without `orig/` stops with a pointer to `docs/building.md`.
- Kept retail bugs stay matched (not fixed) in this epic. Each register entry (289) is marked `RETAIL BUG (kept): <id>` in `src/`, with a consistency check in both directions (every id appears in `src/`; every marker names a register id). Obsolete entries are marked so in the register. Fixing is Epic 4.
- Reference logs are recorded once under wine, kept outside git under `bin/references/`, with a committed manifest (flight, commit, CD pressing, config, checksum). Recording twice must give identical checksums; nondeterministic flights are listed. The frame-log grammar is frozen in `docs/tools.md`.
- ABI types describing FS98 memory never use C pointers, `long`, `size_t`, `wchar_t`, bitfields or `long double`; 80-bit values are `uint8_t[10]`.

## Technical Decisions

- Build: CMake with toolchain file `cmake/toolchains/win32-retail.cmake` (mingw i686). Install and revert copy to a temporary name then `mv`. SDL3 and libcdio come from checksum-pinned tarballs, never system packages (for later targets).
- One flag set via the `OpenFS98::module` interface target: `-fwrapv -fno-strict-aliasing -fno-delete-null-pointer-checks -mfpmath=387 -ffp-contract=off -fexcess-precision=standard`. Any remaining per-module override is listed in the toolchain file with its reason.
- Headers in `include/fs98/` (`abi.h`, `linkage.h`, `host.h`, `global.h`) use `fs98_addr` (32-bit arena offset) and `fs98_code` (32-bit code handle). Structs have explicit padding, `#pragma pack(push, 4)` where retail is 4-packed, and `_Static_assert` size/offset checks. GLOBAL is 4,776 bytes. `FS98_STDCALL` is defined only in `abi.h`.
- `include/fs98/arena.h` holds the only definitions of `M8`..`MD`, `R8`..`RDB`, `DA`, `D`, `RDATA`, `fs98_ptr`, `fs98_handle`, `fs98_call`, base-relative (base 0 on i386). A lint check fails on any `(uint32_t)(uintptr_t)` cast or macro definition of those names outside `src/platform/arena/`. Private copies are deleted module by module.
- Register contracts (the 14 `keep.h` entries, callers reading registers, ANALYSIS's use of the `memmove` value 7) become explicit parameters/results. `keep.h` wrappers and `.S` stubs survive only under `FS98_RETAIL_ABI`; `tools/reg_contracts.sh` must still pass in the retail build.
- Divides: every faulting divide calls the `include/fs98/divide.h` helpers (`fs98_div32`, `fs98_idiv32`, `fs98_div16`, `fs98_idiv16`, `fs98_div8`, `fs98_idiv8`), which return what retail's host filter `0x4119a0` writes (zero and overflow both saturate). Replace SIM1's `sim1_idiv32`, HG2D's inline saturation and BOOSTER's `idivl`/`divl`; no inline `div`/`idiv` asm may remain in `src/`. A helper test checks them against the host filter.
- Comments keep citing retail VAs and service offsets.

## Cross-Story Dependencies

- Order within the epic: 1.1 then 1.2 (Makefile removal needs every target through CMake), then 1.3; 1.4 builds on 1.1; 1.7 then 1.8; 1.8 and 1.9 and 1.10 each need all difftests and flights to keep passing.
- Stories 1.4, 1.8 are dispatchable module by module.
- Story 1.6's recorded flight with a per-pass GLOBAL dump feeds Story 3.6's headless smoke run; its references are the baseline for Epic 3 (FR1).
- Story 1.5's complete marker map is the starting point for Epic 4 (fixes).
- Stories 1.7 to 1.9 prepare Epic 3 (native i386 target) and Epic 5 (64-bit move, which should change only `arena.h`).
