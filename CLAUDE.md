# OpenFS98 — orientation for a fresh context

A hybrid decompilation of Microsoft Flight Simulator 98, OpenRCT2-style: the
retail `FLTSIM98.EXE` keeps running and reimplemented modules are swapped in
one at a time. The end goal is native builds for Windows, Linux, Mac and
possibly Android.

## Working agreements

- **Do not stage, commit or push.** Leave changes in the worktree and say
  which files changed; the user stages, reviews and commits. This holds for
  every change, including ones they asked for.
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
make weathermdiff-run    # whole-module WEATHERM     -> 20000 scenarios, 0 differ
make acontaindiff-run    # whole-module ACONTAIN     -> 20000 scenarios, 0 differ (~25 min)
make sounddiff-run       # whole-module SOUND        -> 20000 scenarios, 0 differ (~40 min)
make dynamicdiff-run     # whole-module DYNAMIC      -> 20000 scenarios, 0 differ (~15 min)
make fegriddiff-run      # FE grid conversions        -> 7742499 comparisons, 0 mismatches
make fediff-run          # FE engine + C file services -> 20000 scenarios, 0 differ
make landmediff-run      # whole-module LANDME       -> 20000 scenarios, 0 differ
make facildiff-run       # FACIL index + queries     -> 0 differences, 20000 query scenarios, 0 differ
make difftest-run        # whole-module SLEW         -> 0/2000 vectors differ
make ofstest-run         # in-process, via the host  -> passed 68, failed 0
make smoke               # ACONTAIN, ATC, CHAIN, DYNAMIC, GLOBAL, LANDME, NUMERIC, SLEW, SOUND, UTIL, WEATHERM installed -> 27/27, 95 gauges, PASS
make util-trace          # which UTIL services the game calls, counted in-game
```

Those exact numbers are the current baseline. If a run reports fewer
comparisons or any mismatch, something regressed — investigate before building
on it. Calls through imports can be tested externally when those dependencies
have small faithful stubs, as NUMERIC demonstrates. Code that needs the real
host process or live game state still needs `ofstest`.

## Current next slice

WEATHERM, ACONTAIN, SOUND and DYNAMIC are done. **FE's externally testable
engine is done and verified**
(round 4 of [docs/preflight.md](docs/preflight.md), survey there; details in
[docs/modules/fe.md](docs/modules/fe.md)):

1. **done and verified** -- the grid conversions `+0x60`..`+0x6c`, by vectors
   (`make fegriddiff-run`, 7,742,499 comparisons, 0 mismatches);
2. **done and verified** -- the whole
   hand-written assembly engine (the scene stream, the nav/ground lookups, the
   listing, object and dynamic-block lookups, world-file loading, BGL
   registering, and the boosters), by 20,000 whole-module scenarios over the
   real CD scenery tree (`make fediff-run`, 0 differences). The harness
   compares every import call and output plus GLOBAL, the heap, and `.data`.
   The CD's `world.vis` has no booster records, so only the registered
   callback's empty path is directly covered; a nonempty booster cache still
   needs a synthetic fixture if that code changes.
3. **done and verified** -- the compiled-C texture loader `+0x1c` and FACIL
   BGL services `+0x7c`..`+0x88`, in the same 20,000-scenario run. The latest
   baseline has 11 operation classes, 7,272 successful texture loads, 3,636
   successful FACIL section reads, and 0 differences. Synthetic fixtures
   self-check the compressed texture output and the BGL selector/read paths.

`+0x78`, the Scenery Library dialog, goes to `ofstest`, as do REALITY and
WINMSG. FE is deliberately **not** in `MODULES`, so `make install` cannot ship
a partial one. **LANDME is also done and verified**: its harness gives each
side a separate retail FE instance, steers the aircraft through all eleven
nonzero guidance phases, and reports zero differences across 20,000 scenarios
(`make landmediff-run`).

**FACIL is now in progress; everything but its dialogs is verified.** Its
complete `.data` image, profile init/deinit, `+0x2c` index validation/rebuild,
`+0x20` serialized-record copy and the mapped-cache queries `+0x18`/`+0x1c` are
reimplemented. `make facildiff-run` first rebuilds a synthetic five-level
facility block against separate retail FE instances (a 0x195-byte cache, a
0xbcd0-byte arena, zero differences), then runs 20,000 seeded synthetic caches
through the query services with comparisons logged (zero differ; 17 of 18
deliberate breaks caught, the eighteenth a proven equivalence). The next slice
is FACIL's three dialog slots in `ofstest`. FACIL stays out of `MODULES` until
those are done. See [docs/modules/facil.md](docs/modules/facil.md).

FE's `+0x40` calling-convention break is now confirmed by `fediff`: the
one-byte probe writes through the caller's edi and returns with ebx/esi/edi
shifted exactly as the disassembly suggested. `+0x18` has a second, newly
confirmed preserved-register rotation when its scene state passes through 4.
Both are reproduced by entry shims; see [docs/modules/fe.md](docs/modules/fe.md).
`tools/difftest/acontaindiff.c`, `weathermdiff.c` and `atcdiff.c` are the
templates for a module with unfinished imports: retail's finished modules
loaded for real, logging stubs for the rest, and the module's whole `.data`
image compared. `acontaindiff.c` adds the pattern for a module that touches
files: a stub host passing file I/O through to generated fixture trees, behind
transliterations of the host's own path and find services. `sounddiff.c` adds
three more: comparing an image and heap that hold pointers into the module
(as offsets, and code through a learnt pairing), recovering from a fault
retail and ours both take, and running each scenario under a random x87
precision control. `dynamicdiff.c` adds the pattern for a module that runs
unchecked data: guard-paged heap and GLOBAL, faults and watchdog-cut hangs
compared as outcomes, mutated real fixtures, and synthetic ones built to
reach a boundary.

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
