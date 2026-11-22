<!-- bmad:context -->
<!-- Verified 2026-11-14 against 4ac3e38. Managed by bmad-project-context; edits inside this block are replaced on refresh. Keep anything you want preserved outside the markers. -->

## OpenFS98

A hybrid decompilation of Microsoft Flight Simulator 98, OpenRCT2-style: the retail `FLTSIM98.EXE` keeps running and reimplemented modules are swapped in one at a time. The end goal is native builds for Windows, Linux, Mac and possibly Android. C and x86 assembly built with CMake (`cmake -S . -B build/win32-retail -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/win32-retail.cmake`, then `cmake --build build/win32-retail`), run under wine. Every other target runs as `tools/ofs <target> VAR=value` (the retired Makefile's names and parameters); module lists come from `tools/modules.sh`. Deeper docs are in `docs/`; what is left is in `docs/remaining-work.md`.

## Policy

- Do not stage, commit or push. Leave changes in the worktree and say which files changed; the user stages, reviews and commits. This holds for every change, including ones they asked for.
- Preserve every existing staged, unstaged and untracked change. A dirty worktree is another contributor's work, not disposable scratch space.
- Differential testing is the arbiter, not careful reading. A service is done when it matches retail on vectors, not when the disassembly looks understood.
- Transliterate, do not rewrite, and reproduce retail's bugs: the goal is a sim that behaves as FS98 behaves. Omit only dead code with no observable effect, and say that you did. Log every kept bug in `docs/retail-bugs.md` with an id and mark its code `RETAIL BUG (kept): <id>`; they are fixed at porting time, not before.
- Never generate C that emulates retail's instructions (registers and flags in a struct, a `goto` per jump) and call it decompiled. SIM1, the gauges, the crash models and the host's startup were lifted that way and had to be redone (`docs/remaining-work.md`, "Lifted code"). The oracle is retail itself, loaded beside ours.
- Reproduce the CD's 1997 `MSVCRT.DLL` `memmove` leaving 7 in ecx (wine's builtin leaves 0) on every platform, in modules and in harness stubs; ANALYSIS depends on it.

## Where things are

- Read `docs/method.md` before writing any code: how a service goes from binary to verified reimplementation.
- Module system and ABI: `docs/architecture.md`. Per-module findings: `docs/modules/<name>.md` (`host.md` and `util.md` are the detailed ones).
- Every CMake target and script: `docs/tools.md`. Setup, which CD, layout and testing in the game: `docs/building.md`.
- Known differences from retail and what is untested: `docs/open-differences.md`. Keep it current when you change a finished module.
- Confirmed host prototypes: `include/fs98/host.h`.
- Harness templates in `tools/difftest/`: `acontaindiff.c`, `weathermdiff.c`, `atcdiff.c` for a module with unfinished imports (retail's finished modules loaded for real, logging stubs for the rest, the whole `.data` compared); `acontaindiff.c` for one that touches files; `sounddiff.c` for images holding pointers into the module, faults both sides take, and random x87 precision; `dynamicdiff.c` for a module that runs unchecked data (guard pages, faults and hangs compared as outcomes).

## Running and verifying

- Orient with `tools/ofs graph` (module ids, dependencies), `tools/ofs hostmap` (the host's 194 slots), `python3 tools/dumplinkage.py orig/Modules/<X>.DLL` and `python3 tools/impscan.py orig/Modules/<X>.DLL`. They read the retail binaries live but have under-reported table sizes before; if a module looks surprisingly small, hand-count the table.
- Before running or judging a verification target, look up its baseline in `docs/tools.md` (Baselines). Fewer comparisons or any mismatch is a regression: investigate before building on it.
- Run final numbers at the full N=20000, and rerun the unbroken baseline at full scale after any harness edit. A run once reported 61/61 breaks caught while the baseline itself differed.
- Run break suites (`*-breaks`, `tools/*_breaks.py`) from a frozen copy of the tree: they copy or rewrite live `src/`, so editing during a run corrupts it.
- A break counts as caught only when the harness exits 1 with a non-zero `N differ`; a crash or a harness guard is a miss (`CRASHED (exit N)`). The runners for MAIN, HOST, WINDOW, WINMSG, REALITY, ANALYSIS, MPCHAT, G3D, PANELS, CONTROLS, SIM1R and the gauges apply it, and their counts in `docs/tools.md` were re-run under it on 2026-11-18/19; the other `tools/*_breaks.py` are not yet checked for it (`docs/implementation/deferred-work.md`), so read a runner's verdict test before trusting its count.
- Code that needs the real host process or live game state is tested with `tools/ofs ofstest-run`; everything else externally, with small faithful stubs for its imports.

## Conventions that differ from defaults

- Use snake_case identifiers and `<stdint.h>` types such as `uint32_t`. No Hungarian notation, no `types.h`; this overrides any general instruction to use them.
- Take argument counts from the callee's `ret N`, never by counting pushes at the call site: the optimiser interleaves pushes across calls, and the wrong arity still compiles. Add confirmed prototypes to `include/fs98/host.h`.
- A harness stub must leave the registers the real callee leaves wherever retail asm reads them (`tools/difftest/analysisdiff.c`'s asm wrappers).
- A harness must model the host's integer-divide repair (the `__try` filter at `0x4119a0` saturates the result): retail modules divide into overflow and rely on it.
- A window rectangle is in 0x2000ths of the screen's width and 0x1800ths of its height, not 0x1000ths (host `0x402a1e`).

## Known pitfalls

- Where a verified module's quirk has a reimplemented caller, test the pair together: FE and DYNAMIC each passed against a stub of the other and failed together (`docs/modules/dynamic.md`).
- Guard module memory outside the compared image: a retail overrun into its own import table showed up as faults several scenarios later.
- A fault that skips a cleanup leaves heap pointers that depend on the other side's leak; compare them as null or not. A break caught only by a harness artifact is not caught.
- Not every FS98 CD is the reference: a later pressing rebased `FLTSIM98.EXE` and ten modules at `0x10000000`. If a generator stops with `image base moved`, check `orig/` against `docs/building.md` ("Which CD") before touching code.
- Retail HG2D freezes the 3D view under wine (exceptions in window procedures are swallowed, so the divide repair never runs). Run retail HG2D with `tools/ofs sehfix-install`, plus `tools/ofs vd-on` for focus; ours does not need it. See `docs/wine.md`.
- G3D's mesh compiler runs at 24-bit x87 precision in the game (Direct3D only; measured 2026-11-15). `g3d_mesh_precision` defaults to 24 for builds without the x87; x86 uses the caller's control word.
- Never run `wineserver -k` in the shared prefix while difftests run; use a prefix copy.
- Never `pkill -f` a pattern that also appears in the same command line: it kills its own shell (`docs/wine.md`).
- In `tools/drive/`: SIMSUI buttons are owner drawn, so `BM_CLICK` does nothing under wine; click by position, and post clicks (a sent `BN_CLICKED` blocks on the modal dialog it opens). Run a crash with `WINEDLLOVERRIDES=dsound=d`, since a crash waits for its sound.

<!-- /bmad:context -->
