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
make facildiff-run       # whole-module FACIL        -> 0 differences; 20000 query, 10000 dialog scenarios, 0 differ (~27 min)
make facildiff-fe-run    # OpenFS98 FE+FACIL together -> 2000 query, 2000 dialog scenarios, 0 differ (~5 min)
make fesldiff-run        # FE +0x78, the Scenery Library -> 20000 scenarios, 0 differ (~6 min)
make boosterdiff-run     # whole-module BOOSTER, its thread drained -> 20000 scenarios, 0 differ (~75 min)
make demodiff-run        # whole-module DEMO, videos and instant replay -> 20000 scenarios, 0 differ (~25 min)
make advdrvdiff-run      # whole-module ADVDRV, adventures and lessons -> 20000 scenarios, 0 differ (~40 min)
make windowdiff-run      # whole-module WINDOW, windows and the eye -> 20000 scenarios, 0 differ (~2 min)
make window-breaks       # WINDOW broken on purpose -> 109 breaks, 106 caught, 3 proven equivalent (~25 min)
make difftest-run        # whole-module SLEW         -> 0/2000 vectors differ
make ofstest-run         # in-process, via the host  -> passed 68, failed 0
make smoke               # ACONTAIN, ADVDRV, ATC, BOOSTER, CHAIN, DEMO, DYNAMIC, FACIL, FE, GLOBAL, LANDME, NUMERIC, SLEW, SOUND, UTIL, WEATHERM, WINDOW installed -> 27/27, 95 gauges, PASS
make util-trace          # which UTIL services the game calls, counted in-game
```

Those exact numbers are the current baseline. If a run reports fewer
comparisons or any mismatch, something regressed — investigate before building
on it. Calls through imports can be tested externally when those dependencies
have small faithful stubs, as NUMERIC demonstrates. Code that needs the real
host process or live game state still needs `ofstest`.

## Current next slice

WEATHERM, ACONTAIN, SOUND and DYNAMIC are done. **FE is done, verified and
running in-game**
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

4. **done and verified** -- `+0x78`, the Scenery Library (`src/fe/fe_library.c`).
   Like FACIL's dialogs it is World.DLL callbacks, so `make fesldiff-run`
   verifies it externally: a World stand-in drives all nine dialogs over a
   writable fixture with sandboxed drive letters (the CD's Boosters as `K:`),
   20,000 scenarios, 0 differences. It found a retail leftover byte (the top
   byte of a saved esi read below a path buffer), now reproduced.

FE is in `MODULES`. REALITY and WINMSG still go to `ofstest`. **LANDME is also done and verified**: its harness gives each
side a separate retail FE instance, steers the aircraft through all eleven
nonzero guidance phases, and reports zero differences across 20,000 scenarios
(`make landmediff-run`).

**FACIL is done, verified and running in-game.** Its "dialogs" turned out to
be callbacks that `English\World.DLL`'s dialogs call through parameter blocks,
so they were verified externally rather than in `ofstest`. `make facildiff-run`
has three phases: a synthetic rebuild, 20,000 synthetic caches through the
queries, and 10,000 dialog scenarios over the index rebuilt from the real CD
scenery plus all sixteen boosters' AFD files (3,791 airports), driven by a
stand-in for World.DLL. All report zero differences. See
[docs/modules/facil.md](docs/modules/facil.md).

FE `+0x84`/`+0x88` now reproduce retail's stack-leftover returns (FACIL's
runway lists depend on them) through entry shims, proven at the owner:
`fediff` paints the slots and goes red on each deliberate break.

**BOOSTER is done, verified and running in-game**, which finishes round 4.
`make boosterdiff-run` runs retail and OpenFS98 in turn over identical cache
trees: four of the CD's boosters, and synthetic ones carrying all five record
types. It drains the worker thread after every operation: posted 0x401s are
counted against the SetEvents that answer them. 20,000 scenarios, 0
differences, and all 25 deliberate breaks are caught. See
[docs/modules/booster.md](docs/modules/booster.md), which also records a
retail UTIL `+0x44` fault (a reach of a whole latitude circle) that
OpenFS98's UTIL does not reproduce.

**DEMO is done, verified and running in-game** (round 5 of
[docs/preflight.md](docs/preflight.md)). It covers the flight-video recorder
and player, instant replay, and the four dialogs' callbacks. `make
demodiff-run` runs both sides in turn over the CD's six videos and synthetic
ones, with retail UTIL's catalogue and a stand-in for the host's dialog
engine. Faults are compared as outcomes, and runaway ops are cut at a fixed
count: 20,000 scenarios, 0 differences, all 25 deliberate breaks caught or
proven equivalent. It reproduces a flush that loses edx, a table read past its
end into code pointers, and callee-left ecx; see
[docs/modules/demo.md](docs/modules/demo.md).

**ADVDRV is done, verified and running in-game** (round 5 of
[docs/preflight.md](docs/preflight.md)). It runs the compiled adventures,
lessons and challenges: a bytecode interpreter with an x87 expression stack
machine over 176 GLOBAL-backed system variables. `make advdrvdiff-run` runs
both sides over the CD's forty-four programs and synthetic ones from an
assembler in the harness, with 20,000 scenarios, 0 differences, all deliberate
breaks caught or proven equivalent. It reproduces an `op_error` use-after-free
(the harness poisons comparison past it), a dead `0xffffd8f1` sentinel check,
sign-and-magnitude 48-bit positions, and displaced-weather-layer field reuse;
see [docs/modules/advdrv.md](docs/modules/advdrv.md).

**WINDOW is done, verified and running in-game** (round 6 of
[docs/preflight.md](docs/preflight.md)): the view windows, titles, status
lines and situation layout, and the eye for every view mode with the
transitions between them. `make windowdiff-run` runs both sides over the same
scripts against a model of the host's window services (z-order, `WM_SIZE`
sizes, menus): 20,000 scenarios, 0 differences; `make window-breaks` applies
its deliberate breaks (all caught but three proven equivalences). Two
findings matter beyond WINDOW: **the host repairs integer divide faults**
(a `__try` filter at `0x4119a0` saturates the result; retail modules divide
into overflow and rely on it, so a harness must model it), and
`dumplinkage.py` stopped at the `+0x10` flag word of CHAIN/FE/WINDOW's
Linkage (fixed). See [docs/modules/window.md](docs/modules/window.md) and
[docs/modules/host.md](docs/modules/host.md).

**Next:** survey `WINMSG` (4,416 bytes) against WINDOW's host model -- it
may be externally testable after all. `REALITY` needs `ofstest`.

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
