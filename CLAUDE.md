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
make maindiff-run SUITE=records  # MAIN slice 1, situation records -> 20000 scenarios, 0 differ (~15 min)
make main-breaks SUITE=records   # its breaks -> 40, 38 caught, 2 proven equivalent (~35 min)
make maindiff-run SUITE=situations  # MAIN slice 2, the reader and writer -> 20000 scenarios, 0 differ (~20 min)
make main-breaks SUITE=situations   # its breaks -> 69, 63 caught, 6 proven equivalent (~40 min)
make maindiff-run SUITE=nav          # MAIN slice 3, range and bearing (+0x70) -> 20000 scenarios, 0 differ (~3 min)
make main-breaks SUITE=nav           # its breaks -> 14, 13 caught, 1 proven equivalent (~6 min)
make maindiff-run SUITE=navio        # MAIN slice 4, the nav read-outs (+0x78,+0x80) -> 20000 scenarios, 0 differ (~3 min)
make main-breaks SUITE=navio         # its breaks -> 12, all caught (~5 min)
make maindiff-run SUITE=navdesc      # MAIN slice 5, the station descriptor (+0x7c) -> 20000 scenarios, 0 differ (~3 min)
make main-breaks SUITE=navdesc       # its breaks -> 9, all caught (~4 min)
make maindiff-run SUITE=season       # MAIN slice 6, the season/zone/clock (+0x74) -> 20000 scenarios, 0 differ (~3 min)
make main-breaks SUITE=season        # its breaks -> 24, 23 caught, 1 proven equivalent (~10 min)
make maindiff-run SUITE=clock        # MAIN slice 7, the clock as digits (+0xa4) -> 20000 scenarios, 0 differ (~3 min)
make main-breaks SUITE=clock         # its breaks -> 6, all caught (~3 min)
make maindiff-run SUITE=view         # MAIN slice 8, the view cycler (+0xc8) -> 20000 scenarios, 0 differ (~3 min)
make main-breaks SUITE=view          # its breaks -> 10, all caught (~5 min)
make maindiff-run SUITE=menu         # MAIN slice 9, analysis menu greying/enabling (+0x30,+0x34) -> 20000 scenarios, 0 differ (~3 min)
make main-breaks SUITE=menu          # its breaks -> 6, all caught (~2 min)
make maindiff-run SUITE=obj          # MAIN slice 10, an object slot release (+0x8c) -> 20000 scenarios, 0 differ (~3 min)
make main-breaks SUITE=obj           # its breaks -> 6, all caught (~3 min)
make maindiff-run SUITE=clamp        # MAIN slice 11, the latitude clamp (+0x1c) -> 20000 scenarios, 0 differ (~3 min)
make main-breaks SUITE=clamp         # its breaks -> 6, 5 caught, 1 proven equivalent (~3 min)
make maindiff-run SUITE=sun          # MAIN slice 12, the sun direction (+0xa0) -> 20000 scenarios, 0 differ (~5 min)
make main-breaks SUITE=sun           # its breaks -> 14, 12 caught, 2 proven equivalent (~6 min)
make maindiff-run SUITE=objalloc     # MAIN slice 13, allocate an object slot (+0x84) -> 20000 scenarios, 0 differ (~5 min)
make main-breaks SUITE=objalloc      # its breaks -> 15, 14 caught, 1 proven equivalent (~6 min)
make maindiff-run SUITE=eye          # MAIN slice 14, the eye's reticle (+0xc0) -> 20000 scenarios, 0 differ (~4 min)
make main-breaks SUITE=eye           # its breaks -> 13, 12 caught, 1 proven equivalent (~5 min)
make maindiff-run SUITE=band         # MAIN slice 15, the altitude band (+0x9c) -> 20000 scenarios, 0 differ (~4 min)
make main-breaks SUITE=band          # its breaks -> 33, all caught (~12 min)
make hostdiff-run                    # HOST (id 1, FLTSIM98.EXE) slices 1-5, 26 services -> 0 differ (~5 s); N=.. iterations
make host-breaks                     # HOST broken on purpose -> 28 breaks, all caught (~6 min)
make mpchatdiff-run      # whole-module MPCHAT, the chat window -> 20000 scenarios, 0 differ (~20 s)
make mpchat-breaks       # MPCHAT broken on purpose -> 117 breaks, 113 caught, 4 proven equivalent (~8 min)
make winmsgdiff-run      # whole-module WINMSG, the message bar -> 20000 scenarios, 0 differ (~2 min)
make winmsg-breaks       # WINMSG broken on purpose -> 64 breaks, 60 caught, 4 proven equivalent (~13 min)
make realitydiff-run     # whole-module REALITY, crashes and the analysis -> 20000 scenarios, 0 differ (~1 min)
make reality-breaks      # REALITY broken on purpose -> 67 breaks, 61 caught, 6 proven equivalent (~7 min)
make analysisdiff-run    # whole-module ANALYSIS, landing/manoeuvre/course -> 20000 scenarios, 0 differ (~2 min)
make analysis-breaks     # ANALYSIS broken on purpose -> 103 breaks, 93 caught, 10 proven equivalent (~20 min)
make controlsdiff-run    # whole-module CONTROLS: commands, joysticks, key bindings, force feedback, the dialog -> 20000 scenarios, 0 differ (~40 min)
make controls-breaks     # CONTROLS broken on purpose -> 81 breaks, 78 caught, 3 proven equivalent (~20 min)
make g3ddiff-run         # whole-module G3D, the 3D pipeline -> 20000 scenarios, 0 differ (~5 min)
make g3d-breaks N=1000   # G3D broken on purpose -> 317 breaks, 279 caught, 7 proven equivalent, 31 missed (19 at N=4000; ~4 h)
make panelsdiff-run      # whole-module PANELS through the CD's gauges -> 3000 scenarios, 0 differ (~15 min)
make panelsunit-run      # PANELS's image routines alone -> 20000 cases each, 0 differ (~3 min)
make panels-breaks       # PANELS broken on purpose -> 200 sampled, 2 do not build, the rest caught (two of them only at N=3000)
make g3dmeshdiff-run     # the mesh compiler: instructions vs portable C, x87 and soft float -> 100000 polygons, 0 differ
make difftest-run        # whole-module SLEW         -> 0/2000 vectors differ
make ofstest-run         # in-process, via the host  -> passed 68, failed 0
make smoke               # ACONTAIN, ADVDRV, ANALYSIS, ATC, BOOSTER, CHAIN, CONTROLS, DEMO, DYNAMIC, FACIL, FE, G3D, GLOBAL, LANDME, MPCHAT, NUMERIC, REALITY, SLEW, SOUND, UTIL, WEATHERM, WINDOW, WINMSG installed -> 27/27, 95 gauges, PASS
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

FE is in `MODULES`. **LANDME is also done and verified**: its harness gives each
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

**WINMSG is done, verified and running in-game**: the message bar, a queue of
messages in seven slots, plain or scrolling, split at spaces to fit. It was
externally testable after all: what it shows is the host calls it makes, and
the host's side is four record fields (pixel width `+0x38`, surface
`+0x94`/`+0x98`, flags). `make winmsgdiff-run`: 20,000 scenarios, 0
differences; `make winmsg-breaks`: 60 of 64 caught, 4 proven equivalent. It
reproduces a scroller drawn before its buffer at ordinary frame rates (over a
slot's text pointer), a line breaker that reads past its copy's terminator,
and a stale cut. Two findings beyond WINMSG: **a window rectangle is in
0x2000ths of the screen's width and 0x1800ths of its height**, not 0x1000ths
(host `0x402a1e`; window.md and host.h corrected), and a harness must guard
module memory *outside* the compared image, because a retail overrun into its
own import table showed up as faults several scenarios later. See
[docs/modules/winmsg.md](docs/modules/winmsg.md).

**REALITY is done, verified and running in-game** (round 7 of
[docs/preflight.md](docs/preflight.md)): crashes, the countdown through the
reload, stress damage, the cracked windscreen and the crash analysis's graph.
It too was set aside for `ofstest` and turned out externally testable: it
draws only lines, and a line is its arguments. `make realitydiff-run`: 20,000
scenarios, 0 differences; `make reality-breaks`: 61 of 67 caught, 6 proven
equivalent. It reproduces the countdown reading the dialog's result in ah, a
metric scale search that steps 0x30 through an 8-byte table, and a half-width
clamp. The line stubs log the view of the record they draw through: a break
in a block freed before the comparison went unseen until they did. See
[docs/modules/reality.md](docs/modules/reality.md).

**ANALYSIS is done, verified and running in-game** (round 8 of
[docs/preflight.md](docs/preflight.md)): landing and manoeuvre analysis and
course tracking. `make analysisdiff-run`: 20,000 scenarios, 0 differences;
`make analysis-breaks`: 93 of 103 caught, 10 proven equivalent. Its sample compaction counts in ecx across
host `+0x21c`, so the result depends on what MSVCRT `memmove` leaves in ecx
(wine's builtin 0, the CD's 1997 `MSVCRT.DLL` 7). **Decided: reproduce the
CD's 7** on every platform, in the module and in the harness's `+0x21c` stub.
Two findings beyond ANALYSIS: **retail CHAIN calls a callback with ecx set to
the dispatch's first argument**, and OpenFS98's CHAIN does not (a CHAIN fix,
not yet made); and a harness stub must leave the registers the real callee
leaves where retail asm reads them (`analysisdiff.c`'s asm wrappers). See
[docs/modules/analysis.md](docs/modules/analysis.md).

**Round 9 surveyed what is left** ([docs/preflight.md](docs/preflight.md)).
**CONTROLS is done, verified and running in-game**, and in `MODULES`. The
hand-written half (`src/controls/controls.c`: the dispatcher and all 342
handlers, the CHAIN callbacks, init/deinit) and the compiled-C half
(`src/controls/controls_input.c`: the key bindings, the joystick poll and
watcher, the joystick configs and their defaults, force feedback over
DirectInput 5, the key-assignment dialog `+0x58`) are transliterated and
**verified by `make controlsdiff-run`: 20,000 scenarios, 0 differences**, over
a model of the host's profile, accelerator and joystick services, fixture
CFG files, a DirectInput stand-in, and a stand-in for the dialog (`0x140a`
of `English\Options.DLL`, an MFC dialog that calls back into CONTROLS
through a parameter block), built from Options.DLL's listing as FACIL's were
from World.DLL. The verification found and fixed six transliteration bugs (a
repeat counter, three in the keyboard-layout reader, two in the key list).
Two harness lessons: a fault that skips a cleanup leaves heap pointers whose
values depend on the other side's leak (compare them as null or not), and a
break "caught" by a harness artifact is not caught (every break run stopped
at that artifact until it was fixed). See
[docs/modules/controls.md](docs/modules/controls.md).

**G3D is done, verified and running in-game**, and in `MODULES`: the
scenery bytecode machine (all 148 opcodes), the transform and the four
clippers, the depth queue, every Linkage service and the nine CHAIN
callbacks (the scene built from FE's stream, the ground, the frame's draw).
`make g3ddiff-run`: 20,000 scenarios, 0 differences, against stubs of HG2D
(it supplies its own span routines, so HG2D can stay a logging stub), FE,
CHAIN and the host, with retail UTIL and WINDOW. Retail's bugs are
reproduced; its four crash-only bugs are fixed on purpose and listed in
[docs/modules/g3d.md](docs/modules/g3d.md). The mesh compiler (`0x38`,
x87 compiled C whose decisions turn on rounding) runs as retail's own
instructions (`g3d_mesh.S`) on x86; `g3d_mesh.c` is its portable C over a
software 80-bit float, checked against them (`make g3dmeshdiff-run`). Which
x87 precision the game runs G3D under is not established; a native build
has to set `g3d_mesh_precision` to match. MULTIP, PANELS
(docs/modules/panels.md) and **MPCHAT are done**; MPCHAT, the chat window,
is verified against a model of the window system hooked into its import
table (docs/modules/mpchat.md). **MAIN is in progress**, a slice at a time behind
`maindiff`'s suites, as MULTIP was; slices 1-6 are verified (situation
records, the assembly situation reader and writer, the nav services
`+0x70`/`+0x78`/`+0x7c`/`+0x80` -- range and bearing, the read-outs and the
station descriptor -- `+0x74`, the season, zone and clock, `+0xa4`, the clock
as digits, `+0xc8`, the view cycler, `+0x30`/`+0x34`, the analysis menu
greying and enabling, `+0x8c`, an object slot release, and `+0x1c`, the
latitude clamp), the rest to go. See
[docs/modules/main.md](docs/modules/main.md). The CHAIN ecx fix is done: OpenFS98's CHAIN enters callbacks with
all of retail's registers and returns its ecx/edx, and `chaindiff` now
compares them (see [docs/modules/chain.md](docs/modules/chain.md)).

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

## Three things not to rediscover the hard way

- **Argument counts come from the callee, never the call site.** The optimiser
  interleaves pushes across calls, so counting pushes before a `call` gives the
  wrong arity — and the wrong version still compiles. Confirmed host prototypes
  live in [include/fs98/host.h](include/fs98/host.h); add to it from the
  callee's `ret N`.
- **The wine 3D-view freeze is solved -- run with `SEHFIX.DLL`.** Wine
  swallows exceptions raised in window procedures, so the host's integer-divide
  repair (filter `0x4119a0`) never saw HG2D's divides by a zero-length edge,
  and the first one in a frame drawn from a callback stopped the rasteriser
  for good. `make sehfix-install` hands those faults back to the host's
  filter; `make vd-on` is still needed for focus. Not a decompilation defect.
  See [docs/wine.md](docs/wine.md).
- **Not every FS98 CD is the reference.** A later pressing rebuilt
  `FLTSIM98.EXE` and ten modules at base `0x10000000` (CONTROLS among them,
  with 493 commands instead of 383). If a generator stops with `image base
  moved`, check `orig/` against README.md's "Which CD" before touching any
  code.
