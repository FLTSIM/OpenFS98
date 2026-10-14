# OpenFS98 — orientation for a fresh context

A hybrid decompilation of Microsoft Flight Simulator 98, OpenRCT2-style: the
retail `FLTSIM98.EXE` keeps running and reimplemented modules are swapped in
one at a time. The end goal is native builds for Windows, Linux, Mac and
possibly Android.

## Working agreements

- **Do not commit and do not push.** Stage changes (`git add`) and leave them;
  the user reviews and commits. This holds for every change, including ones
  they asked for.
- **Differential testing is the arbiter**, not careful reading. A service is
  done when it matches retail on vectors, not when the disassembly looks
  understood.
- **Transliterate, do not rewrite**, and **reproduce retail's bugs.** The goal
  is a sim that behaves as FS98 behaves, not one that is arithmetically tidier.
  Omit only dead code with no observable effect, and say that you did.
- Preserve every existing staged, unstaged and untracked change. A dirty
  worktree is another contributor's work, not disposable scratch space.

## Read these, in this order

1. [README.md](README.md) — status table, setup, the full `make` target list,
   and what to do next.
2. [docs/method.md](docs/method.md) — how a service gets from binary to
   verified reimplementation, and the rules that have each caught a real bug.
   **Read this before writing any code.**
3. [docs/architecture.md](docs/architecture.md) — the module system and ABI.
4. `docs/modules/<name>.md` — per-module findings. `host.md` and `util.md` are
   the detailed ones.

## Orienting fast

```sh
make graph                                  # module ids and dependencies
make hostmap                                # the host's 194 service slots
python3 tools/dumplinkage.py orig/Modules/UTIL.DLL   # one module's table + sizes
python3 tools/impscan.py orig/Modules/ATC.DLL        # which imported services it calls
```

All of these read the retail binaries live, so they cannot drift from the truth.
They *can* be buggy — both have under-reported table sizes before, which looks
exactly like a small module. If something looks surprisingly small, hand-count
the table.

## Verifying

```sh
make utildiff-run        # UTIL service by service   -> 6222442 comparisons, 0 mismatches
make utildiff-portable   # the same with x87 compiled out (the ARM path)
make numericdiff-run     # all NUMERIC formats/ranges -> 545778 comparisons, 0 mismatches
make chaindiff-run       # whole-module CHAIN        -> 20000 scenarios, 0 differ
make atcdiff-run         # whole-module ATC          -> 20000 scenarios, 0 differ
make difftest-run        # whole-module SLEW         -> 0/2000 vectors differ
make ofstest-run         # in-process, via the host  -> passed 68, failed 0
make smoke               # ATC, CHAIN, GLOBAL, NUMERIC, SLEW, UTIL installed -> 27/27, 95 gauges, PASS
make util-trace          # which UTIL services the game calls, counted in-game
```

Those exact numbers are the current baseline. If a run reports fewer
comparisons or any mismatch, something regressed — investigate before building
on it. Calls through imports can be tested externally when those dependencies
have small faithful stubs, as NUMERIC demonstrates. Code that needs the real
host process or live game state still needs `ofstest`.

## Current next slice

ATC is done. **LANDME** is next per [docs/preflight.md](docs/preflight.md),
blocked on understanding the records FE `+0x20`/`+0x54` hand back -- see the
README's "What to do next". `tools/difftest/atcdiff.c` is now the template for
a module with unfinished imports: retail's finished modules loaded for real,
logging stubs for the rest, and the module's whole `.data` image compared.

## Two things not to rediscover the hard way

- **Argument counts come from the callee, never the call site.** The optimiser
  interleaves pushes across calls, so counting pushes before a `call` gives the
  wrong arity — and the wrong version still compiles. Confirmed host prototypes
  live in [include/fs98/host.h](include/fs98/host.h); add to it from the
  callee's `ret N`.
- **The wine 3D-viewport bug is parked, deliberately.** See
  [docs/wine.md](docs/wine.md). It reproduces with the retail modules, so it is
  not a decompilation defect, and native renderers make it moot. Do not spend
  time on it unless asked.
