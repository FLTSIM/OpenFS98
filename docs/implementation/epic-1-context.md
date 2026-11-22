# Epic 1 Context: The wine build follows the port rules

<!-- Compiled from planning artifacts. Edit freely. Regenerate with compile-epic-context if planning docs change. -->

## Goal

Move the retail-ABI build (running under wine) onto the shared rules the native port needs: CMake instead of the Makefile, one compiler flag set, no raw retail data in git, 32-bit ABI headers, one shared arena header, no register contracts, no faulting divides, and every kept retail bug marked in code. All 74 flights must stay frame-identical to retail throughout, so later epics (native i386, x86-64, Windows, macOS) start from a clean, checkable base.

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

- Every story is verified with today's tools: the 74 frame-identical flights, the difftest baselines recorded in `docs/tools.md`, and `main-dialogs` / `flights-play`.
- Kept retail bugs stay matched (bug-for-bug) in this epic; they are only fixed later in the i386 native build. Each is logged in `docs/retail-bugs.md` and marked `RETAIL BUG (kept): <id>` in code (57 markers exist today for 289 register entries; a consistency check must tie register and code together).
- No Microsoft files, raw dumps or retail resources in git: module data becomes decompiled C initialisers (typed tables, constants, strings); only resources still come from `orig/` at build time for `win32-retail`, and a build without `orig/` must stop with a pointer to `docs/building.md`.
- ABI headers describe FS98 memory with fixed-width 32-bit types only (no C pointers, `long`, `size_t`, `wchar_t`, bitfields, `long double`); GLOBAL is 4,776 bytes.
- Retail reference logs are recorded once under wine, kept outside git with a committed manifest; recording must be repeatable (non-deterministic flights are listed).
- Docs (`docs/tools.md`, `docs/building.md`, `AGENTS.md`) must describe CMake commands once the Makefile is gone.

## Technical Decisions

- CMake replaces the Makefile at parity; toolchain file `cmake/toolchains/win32-retail.cmake` for this epic (others come later). Install and revert copy to a temp name, then `mv`. Binaries should match the Makefile output byte for byte, or differences are listed with reasons.
- One `OpenFS98::module` interface target carries `-fwrapv -fno-strict-aliasing -fno-delete-null-pointer-checks -mfpmath=387 -ffp-contract=off -fexcess-precision=standard`; any remaining per-module override is listed in the toolchain file with its reason. (The host's x87 control word value is to be measured, not assumed.)
- `include/fs98/arena.h` is the only home of the address macros (`M8`..`MD`, `R8`..`RDB`, `DA`, `D`, `RDATA`, `fs98_ptr`, `fs98_handle`, `fs98_call`), base 0 on i386; a lint check rejects `(uint32_t)(uintptr_t)` casts or macro definitions outside `src/platform/arena/`. Types use `fs98_addr` / `fs98_code`, with `_Static_assert` size and offset checks; `FS98_STDCALL` is defined only in `abi.h`.
- Register contracts (14 `keep.h` entries, callers reading registers, ANALYSIS's `memmove` value of 7) become explicit parameters/results; `keep.h` and `.S` stubs survive only under `FS98_RETAIL_ABI`; `tools/reg_contracts.sh` must pass in the retail build.
- Divides never trap: use `include/fs98/divide.h` saturating helpers, returning what host filter `0x4119a0` writes. Replace SIM1 `sim1_idiv32`, HG2D's inline saturation and BOOSTER's `idivl`/`divl`; no inline `div`/`idiv` asm remains in `src/`.
- Module and data conversions are dispatchable module by module.
- Retail reference logs go to `bin/references/`; one deterministic flight also records a GLOBAL dump each pass (used later by the headless smoke run in Story 3.6).

## Cross-Story Dependencies

- 1.1 builds on nothing; 1.2 needs 1.1; 1.3 needs 1.2; 1.4 needs 1.1; 1.8 needs 1.7.
- Story 1.3 was in progress when the 2026-11-19 resequencing was planned; Story 1.6 gained an acceptance criterion for a headless smoke run flying its GLOBAL-dump flight.
- Story 1.5 feeds Epic 4 (retail bug fixes); 1.6 feeds Epic 3 (native i386 baseline and smoke run); 1.7-1.10 prepare Epic 5 (x86-64 move changes one header).
