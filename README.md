# OpenFS98

Hybrid decompilation of Microsoft Flight Simulator 98 (retail, 1997-08-07).

The approach is OpenRCT2's, not a ROM-style matching decompilation: the retail
game keeps running, and reimplemented code is swapped in a piece at a time so
there is something testable from the first commit.

FS98 makes this unusually easy. It has no static linkage between its own
modules — `FLTSIM98.EXE` enumerates `Modules\`, loads each DLL, and links them
by numeric id through two exported data symbols. So a reimplemented module is a
**drop-in replacement**: same two exports, same id, same service slots, copy it
into `Modules\`, done. No binary patching, no hooking, no injected loader.

Total code to account for across the whole product: **1.44 MiB** of `.text`
across 27 modules plus the host. See [docs/architecture.md](docs/architecture.md)
for the module map, the ABI, and what was found on the disc.

New here? [docs/method.md](docs/method.md) is how work gets done -- the loop
from retail binary to verified reimplementation, and the conventions that have
earned their place. [docs/architecture.md](docs/architecture.md) is what FS98
is. The per-module documents under `docs/modules/` are the detail.

## Setup

```sh
sudo apt install gcc-mingw-w64-i686-win32 p7zip-full   # 32-bit toolchain, 7z
7z x ~/Downloads/MSFlightSimulator98.iso -ocd     # the CD, for staging
./tools/extract.sh ~/Downloads/MSFlightSimulator98.iso   # orig/ reference binaries
make stage                                        # game/ — a real install
make && make smoke                                # build, then boot the game
```

`cd/`, `orig/`, `game/` and `bin/` are gitignored — no game content goes in the
repository. The only derived artifact that is committed is
`docs/setup_map.tsv`, the CD-to-install file map read out of `Setup.exe`.

**Which CD.** FS98 shipped in more than one pressing, and the project is built
against one in particular. In the reference, `FLTSIM98.EXE` and every module
are dated 6 August 1997, except MAIN (11 August) and PANELS (7 August, and
the only one linked at `0x10000000`). One known-good image is a 581,996,544-byte
`MSFlightSimulator98.iso` with SHA-256
`be523a66bf368f8769f86aedcddad6f58fb0185908b6a398c456c0c5a4d07436`. Other
rips of the same CD may hash differently, so go by the dates.

A later pressing, whose ISO files are dated 10 November 1997, rebuilt
`FLTSIM98.EXE` (28 October 1997) and ten modules (17–20 October 1997, all
linked at `0x10000000`): ADVDRV, CONTROLS, FE, SOUND, WINMSG, MAIN, G3D,
HG2D, PANELS and MULTIP. Its other seventeen modules are byte-identical to
the reference's. The rebuilt ones are different programs (that pressing's
CONTROLS has 493 commands to the reference's 383, and the host's services
have moved), so the generators and harnesses do not fit them:
`tools/gen_controls_data.py`, for one, stops with `image base moved`. Check
after extracting:

```sh
objdump -p orig/Modules/CONTROLS.DLL | grep -E 'Time/Date|ImageBase'
# reference: Wed Aug  6 21:46:24 1997, ImageBase 20670000
```

The Makefile's `BASE_*` give the reference's base for each module OpenFS98
builds.

`make stage` exists because the CD layout is not the installed layout: setup
renames 263 files on the way in, and FS98 opens them by their installed names.
Skipping that is why the sim first dies with "Aircraft initialization failure".
See [docs/architecture.md](docs/architecture.md#the-cd-layout-is-not-the-installed-layout).

## Layout

```
orig/       retail binaries, extracted from the CD (not committed)
game/       runnable FS98 tree for testing (not committed)
include/    the module ABI
src/        reimplemented modules, one directory each
tools/      analysis scripts
docs/       findings
bin/        build output
```

Identifiers in the code use the `fs98_` / `FS98_` prefix when they describe the
original game -- its ABI, its module ids, its data layouts -- and `openfs98`
only for this project's own identity. Same split OpenRCT2 draws between `rct2_`
and `openrct2`: most of what gets written here is a description of someone
else's 1997 build, and it should read that way.

## Tools

| | |
| --- | --- |
| `make difftest-run` | drive a rebuilt module and retail over identical synthetic state and diff the result; `N=...` sets the vector count |
| `make utildiff-run` | compare individual rebuilt UTIL services against retail's, service by service |
| `make utildiff-portable` | the same with the x87 assembly compiled out, as an ARM build would use |
| `make numericdiff-run` | compare all NUMERIC formatter/parser ids and ranged parses against retail |
| `make atcdiff-run` | drive retail and OpenFS98 ATC side by side over random scripts of ATIS, clearances and frames, comparing every import call and the whole `.data` image; `N=...` sets the count |
| `make acontaindiff-run` | the same for ACONTAIN, against generated aircraft trees: loads, lists, queries and models, with file I/O passed through and fake `.SIM`/`.MDL` modules, comparing every import call, GLOBAL, the heap and the `.data` image |
| `make sounddiff-run` | the same for SOUND, against generated `SOUND.CFG` trees and the CD's seven: frames, events, wind, faders and CHAIN messages, with a model of the host's voice engine, comparing every import call, GLOBAL, `FLTSIM98.CFG`, the heap and the `.data` image |
| `make dynamicdiff-run` | the same for DYNAMIC, against the dynamic-scenery programs of the CD's twelve airport BGLs, often mutated, and three synthetic blocks: frames, the three CHAIN lists, the user's aircraft and MULTIP's players steered onto what the programs test; faults and hangs compared as outcomes |
| `make fegriddiff-run` | FE's grid conversions (`+0x60`..`+0x6c`), retail and OpenFS98 loaded side by side and called directly over every table edge at every level and random values; a fault in either side counts as a difference |
| `make fediff-run` | FE's assembly engine and compiled-C file services, retail and OpenFS98 loaded side by side over the extracted CD scenery/texture tree plus self-checking synthetic compressed-texture and BGL-section fixtures, comparing every import call and output, GLOBAL, the heap and the whole `.data` image |
| `make landmediff-run` | LANDME's seven-state landing assistant against retail, each side backed by its own retail FE over the real scenery tree; compares every import call, GLOBAL, the heap and both modules' whole `.data` images |
| `make facildiff-run` | whole-module FACIL against retail, each side backed by a separate retail FE: a synthetic five-level rebuild, 20,000 seeded synthetic caches through the `+0x18`/`+0x1c`/`+0x20` queries, then the real CD index (with all sixteen boosters) and 10,000 dialog scenarios driven by a stand-in for World.DLL; compares the caches, call transcript (with string comparisons), returned records, GLOBAL, heap and both modules' state |
| `make boosterdiff-run` | whole-module BOOSTER, worker thread included: retail and OpenFS98 run in turn over identical cache trees (four CD boosters, synthetic ones with every record type), the thread drained after every operation by counting posted 0x401s against SetEvents; MessageBoxA and the file calls hooked and logged; compares the transcript, the whole `.data` image, both heaps, GLOBAL, FE's records and the cache trees |
| `make windowdiff-run` | whole-module WINDOW: retail and OpenFS98 in turn over scripts of windows created, closed, maximised, painted frame after frame through every eye mode and transition, and laid out in situations; a model of the host's window services and z-order, and of its filter that repairs integer divide faults; compares the transcript (with each frame's eye), the whole `.data` image, the arena and GLOBAL. `make window-breaks` breaks the module on purpose, one place at a time, and checks the harness notices |
| `make winmsgdiff-run` | whole-module WINMSG, the message bar: retail and OpenFS98 in turn over scripts of messages shown, appended, cleared and run frame after frame (plain and scrolling, split to fit), with a model of the host's window record and heap, a CHAIN that answers the done message and re-enters, and situation records; compares the transcript with every string drawn, the whole `.data` image, the arena and GLOBAL. `make winmsg-breaks` breaks it on purpose |
| `make realitydiff-run` | whole-module REALITY, crashes and damage: retail and OpenFS98 in turn through every crash gate, the countdown, the reload and the crash analysis (a stand-in for its dialog, retail UTIL and NUMERIC), the stress rolls and the cracked windscreen; HG2D's lines logged with their colour and view; compares the transcript, the whole `.data` image, the arena, GLOBAL and UTIL's and NUMERIC's state. `make reality-breaks` breaks it on purpose |
| `make analysisdiff-run` | whole-module ANALYSIS, landing and manoeuvre analysis and course tracking: retail and OpenFS98 in turn over the frames, the landing thresholds, the compaction, the trail and all three dialogs (a stand-in for the dialog engine, retail UTIL and NUMERIC), with the register leftovers retail reads (CHAIN's, memset's, the CD MSVCRT memmove's) modelled; HG2D's lines and G3D's display list logged; compares the transcript, the whole `.data` image, the arena, GLOBAL and UTIL's and NUMERIC's state. `make analysis-breaks` breaks it on purpose |
| `make controlsdiff-run` | CONTROLS, in progress: retail and OpenFS98 side by side over random commands through the dispatcher and the axis, engine and pilot-event entries, and the per-frame joystick poll over a seeded joystick config, with scripted buttons pressed, held (so repeated) and released as the host's joystick read reports them; compares GLOBAL, the whole `.data` image and the import-call transcript. The key/joystick config, the dialog and force feedback are not driven yet. `make controls-breaks` breaks the dispatcher and the joystick poll on purpose |
| `make demodiff-run` | whole-module DEMO: retail and OpenFS98 in turn over the CD's six videos and synthetic ones with every record class, recording, playback, instant replay and the four dialogs' callbacks (retail UTIL's catalogue, a stand-in for the host's dialog engine); faults compared as outcomes, runaway ops cut at a fixed count; compares the transcript, the whole `.data` image (code pointers through a learnt pairing), the arena, GLOBAL and the trees |
| `make fesldiff-run` | FE's `+0x78`, the Scenery Library, retail against OpenFS98 over a writable fixture with sandboxed drive letters, driven by a stand-in for World.DLL's nine dialogs; compares the call transcript (with string comparisons and the world files left on disk), FE's `.data`, GLOBAL and the heap; faults are compared as outcomes |
| `make facildiff-fe-run` | the same harness with OpenFS98 FE behind OpenFS98 FACIL, against retail FE behind retail FACIL: both reimplementations together over the real CD index and the dialogs (`Q=`/`D=` set the phase sizes, 2,000 each by default) |
| `make weathermdiff-run` | the same for WEATHERM: updates, frames, resets, scenery effects and list operations, comparing every import call, GLOBAL, the heap and the `.data` image |
| `make chaindiff-run` | drive retail and OpenFS98 CHAIN side by side over random re-entrant callback scripts; `N=...` sets the count |
| `make ofstest-run` | run assertions *inside* the sim, where the host is reachable; `ofstest-remove` takes the module back out |
| `make util-trace` | boot the sim with a UTIL that counts every service call, and list what ran; `UTIL_TRACE_SECS=` for a longer session to play in |
| `make graph` | module id and dependency table, read live from `orig/` |
| `tools/impscan.py FILE` | which imported services a module calls, by import and slot |
| `make hostmap` | the host's service table: location, classification, and which libraries each service calls |
| `make compare` | section sizes of each built module vs. the retail one |
| `tools/pesurvey.py FILE...` | sections, linker version, exports, debug dir |
| `tools/dumplinkage.py FILE [n]` | dump a module's `Linkage` / `ImportTable` |

## Testing against the real game

The game does not need installing — the CD's `fs98` tree runs as-is once
`mfc42.dll` and `MSVCRT.DLL` from the CD root sit next to `FLTSIM98.EXE`. That
tree is staged in `game/` (gitignored, 103 MB) and symlinked into the wine
prefix as `C:\FS98`, so no bulk data lands in `~/.wine`.

```sh
make install     # swap rebuilt modules in; backs up to *.orig first
make run         # wine FLTSIM98.EXE
make revert      # put the retail modules back
make trace       # WINEDEBUG=+loaddll, to see which modules got loaded
```

Set `FS98_DIR` if the tree is elsewhere.

Two levels of test. `make smoke` boots the real game; `make difftest-run` drives
a module directly against the retail one, which is the only way to check
anything the game will not exercise on startup — SLEW's tick, for instance, runs
only in slew mode.

`make smoke` asserts the game actually comes up: all 27 modules loaded **and**
at least one `.GAU` loaded. The gauge check is the point — FS98 reports startup
failures in *modal dialogs*, so a game wedged behind "Aircraft initialization
failure" stays alive and looks identical to one that is flying. Gauges only load
once an aircraft has resolved, its visual model has been found and its panel
built, so a gauge load means startup went all the way through.

**Verified working**: under wine 9.0 the retail game reaches its main menu —
27/27 modules, 95 gauges — and so does the build with OpenFS98's `ACONTAIN`, `ADVDRV`, `ANALYSIS`, `ATC`, `BOOSTER`,
`CHAIN`, `DEMO`, `DYNAMIC`, `FACIL`, `FE`, `GLOBAL`, `LANDME`, `NUMERIC`,
`REALITY`, `SLEW`, `SOUND`, `UTIL`, `WEATHERM`, `WINDOW` and `WINMSG` modules substituted, with
identical counts.

## Where this is going

The end state is native builds -- modern Windows, Linux, Mac, maybe Android --
at which point wine stops mattering. Running the retail binary under wine is
scaffolding for the middle of the project, not the destination, and it is
already the only part that misbehaves (see [docs/wine.md](docs/wine.md)).

How far away that is, measured by what each retail module actually calls:

| | modules | `.text` | share |
| --- | --- | --- | --- |
| Portable as-is -- pure logic, or C runtime only | 16 | 213 KB | 21% |
| Needs a host for input and messages (USER32) | 7 | 217 KB | 22% |
| Needs a host for drawing (GDI32) | 4 | 563 KB | 57% |

The platform dependency is concentrated, not spread: `HG2D` (390 KB, the 2D
rasteriser), `MAIN` and `PANELS` account for essentially all of the drawing.
Sixteen modules -- including every one reimplemented so far and every one next
in the queue -- call nothing but the C runtime and can be built natively today.

So the order of work already serves the native goal: finish the portable layer,
then the input layer behind a small host abstraction, and leave the rasteriser
until last, where a native replacement is a renderer rather than a port.

**Floating point is kept portable deliberately.** Retail rounds on the x87, and
the reimplementation uses the same instructions on x86 so that "the same" is
not a matter of opinion. There is no x87 on ARM, so a parallel path goes
through `sqrt` and `nearbyint` instead -- and `make utildiff-portable` holds it
to the same standard, checking it against the retail DLL with the assembly
compiled out. It currently passes all 6,222,442 comparisons, so an ARM build
will compute the same numbers as an x86 one.

One service is known not to survive the move: UTIL's double-to-fixed-point
conversion depends on the x87's 80-bit intermediate precision, and ARM's
`long double` is 128-bit, which computes a *different* -- in fact more exact --
answer. The difference is a unit or two in 1/65536ths of a position -- around a
hundredth of a millimetre -- and the decision is to accept it rather than
emulate 80-bit arithmetic to preserve a rounding artefact. See
[docs/modules/util.md](docs/modules/util.md).

## Status

| module | id | `.text` | state |
| --- | --- | --- | --- |
| `GLOBAL` | 2 | 54 | **reimplemented and running in-game** |
| `SLEW` | 9 | 1,270 | **reimplemented and verified** — 200,000 differential vectors, byte-identical |
| `ATC` | 24 | 3,840 | **reimplemented, verified, and running in-game** — ATIS and the tower's clearance exchange; 20,000 whole-module differential scripts comparing its whole `.data` image, zero differences — see [docs/modules/atc.md](docs/modules/atc.md) |
| `CHAIN` | 4 | 966 | **reimplemented, verified, and running in-game** — the event bus; 20,000 whole-module differential scripts, zero differences — see [docs/modules/chain.md](docs/modules/chain.md) |
| `WEATHERM` | 27 | 8,774 | **reimplemented, verified, and running in-game** — the weather: areas, blending, temperature and pressure by altitude, wind; 20,000 whole-module differential scripts comparing GLOBAL and its whole `.data` image, zero differences — see [docs/modules/weatherm.md](docs/modules/weatherm.md) |
| `ACONTAIN` | 23 | 5,504 | **reimplemented, verified, and running in-game** — the aircraft container: finds aircraft by title, resolves their paths, loads the flight and visual models, lists them; 20,000 whole-module differential scripts against generated aircraft trees, zero differences — see [docs/modules/acontain.md](docs/modules/acontain.md) |
| `SOUND` | 39 | 15,174 | **reimplemented, verified, and running in-game** — engine, warning, marker and Morse ident sounds, driven through the host's voice engine; 20,000 whole-module differential scripts comparing its whole `.data` image, zero differences — see [docs/modules/sound.md](docs/modules/sound.md) |
| `DYNAMIC` | 6 | 12,300 | **reimplemented, verified, and running in-game** — dynamic scenery: an interpreter for the airport traffic programs in scenery files, three of whose opcodes write programs of their own; 20,000 whole-module differential scripts comparing GLOBAL, the heap and its whole `.data` image, zero differences — see [docs/modules/dynamic.md](docs/modules/dynamic.md) |
| `FE` | 18 | 49,016 | **reimplemented, verified, and running in-game** — the grid conversions, assembly engine, compiled-C file services (20,000 whole-module scenarios) and the Scenery Library (20,000 scenarios through a World.DLL stand-in), zero differences — see [docs/modules/fe.md](docs/modules/fe.md) |
| `LANDME` | 26 | 4,094 | **reimplemented, verified, and running in-game** — nearest-runway selection and all eleven nonzero landing-guidance phases; 20,000 whole-module scenarios over retail FE and real scenery, zero differences — see [docs/modules/landme.md](docs/modules/landme.md) |
| `BOOSTER` | 34 | 8,024 | **reimplemented, verified, and running in-game** — the performance boosters: CD scenery copied to a disk cache on a worker thread, nearest first, and evicted out of reach; 20,000 whole-module scenarios, zero differences — see [docs/modules/booster.md](docs/modules/booster.md) |
| `DEMO` | 7 | 16,864 | **reimplemented, verified, and running in-game** — flight videos (record and play), instant replay and the four dialogs' callbacks; 20,000 whole-module scenarios, zero differences — see [docs/modules/demo.md](docs/modules/demo.md) |
| `FACIL` | 37 | 23,045 | **done** — verified (index, queries and the three dialogs' callbacks) and running in-game — see [docs/modules/facil.md](docs/modules/facil.md) |
| `NUMERIC` | 17 | 14,684 | **reimplemented, verified, and running in-game** — 545,778 differential comparisons, zero mismatches — see [docs/modules/numeric.md](docs/modules/numeric.md) |
| `WINDOW` | 10 | 24,992 | **reimplemented, verified, and running in-game** — the view windows, their titles, status lines and layout, and the eyepoint for every view mode with the transitions between them; 20,000 whole-module scenarios against a model of the host's window services, zero differences — see [docs/modules/window.md](docs/modules/window.md) |
| `WINMSG` | 22 | 4,416 | **reimplemented, verified, and running in-game** — the message bar: queued messages in seven slots, plain or scrolling, split to fit, appended to; 20,000 whole-module scenarios against a model of the host's window record, zero differences — see [docs/modules/winmsg.md](docs/modules/winmsg.md) |
| `REALITY` | 32 | 4,822 | **reimplemented, verified, and running in-game** — crashes, stress damage, the cracked windscreen and the crash analysis's graph; 20,000 whole-module scenarios, zero differences — see [docs/modules/reality.md](docs/modules/reality.md) |
| `ANALYSIS` | 29 | 8,086 | **reimplemented, verified, and running in-game** — landing and manoeuvre analysis and course tracking: the samples, the trail's display list, the three dialogs; 20,000 whole-module scenarios, zero differences — see [docs/modules/analysis.md](docs/modules/analysis.md) |
| `UTIL` | 20 | 11,062 | **reimplemented, verified, and running in-game** — all 47 services; 19 seen called in a hands-on session, the modal alert among them — see [docs/modules/util.md](docs/modules/util.md) |
| `FLTSIM98.EXE` | 1 (host) | 82,016 | 194 services mapped, 71 with confirmed prototypes, 11 verified in-process, 2 implemented — see [docs/modules/host.md](docs/modules/host.md) |

Retail baseline and the OpenFS98 build -- ACONTAIN, ANALYSIS, ATC, BOOSTER, CHAIN,
DEMO, DYNAMIC, FACIL, FE, GLOBAL, LANDME, NUMERIC, REALITY, SLEW, SOUND, UTIL,
WEATHERM, WINDOW and WINMSG all installed -- both
reach the panel: 27/27 modules, 95 gauges, `make smoke` PASS.

`GLOBAL.DLL`'s 4764-byte shared state block is byte-identical to retail, and the
module id matches. The remaining differences from the retail binary are
cosmetic and belong in a future `OpenFS98-matching`, not here: the entry point
sits one byte later because gcc orders the no-op first, mingw emits unused
`__CTOR_LIST__`/`__DTOR_LIST__` tables into `.text`, the PE linker-version field
reads 2.41 rather than 5.00, and the version resource deliberately says
OpenFS98 instead of copying Microsoft's company name and copyright.

### What to do next

Nothing is blocked. The in-process harness (`make ofstest-run`) reaches
anything that calls the host, and the external one
(`make utildiff-run`, `make difftest-run`) covers everything self-contained.

Every module left imports at least one that is not done, so each needs some
imports stubbed or bound to retail in its harness. ATC showed the pattern:
retail's finished modules loaded for real, small logging stubs for the rest,
and the module's whole `.data` image compared when its layout allows it --
see [docs/modules/atc.md](docs/modules/atc.md).

Round 2 of the [preflight](docs/preflight.md) is done: `WEATHERM` and
`ACONTAIN` are reimplemented and verified -- see
[docs/modules/weatherm.md](docs/modules/weatherm.md) and
[docs/modules/acontain.md](docs/modules/acontain.md). `REALITY`, set aside then
for `ofstest`, was done externally in round 7.

Round 3 picked `SOUND`, then `DYNAMIC`, and both are reimplemented and
verified -- see [docs/modules/sound.md](docs/modules/sound.md) and
[docs/modules/dynamic.md](docs/modules/dynamic.md). `BOOSTER` works on its
own thread and is left for later.

Round 4 surveyed FE, LANDME, FACIL and BOOSTER -- see the
[preflight](docs/preflight.md) -- and **FE is done, verified and running in-game**
([docs/modules/fe.md](docs/modules/fe.md)). Its grid conversions
(`+0x60`..`+0x6c`) are reimplemented and verified by vectors (`make
fegriddiff-run`, 7,742,499 comparisons, 0 mismatches). Its whole hand-written
assembly engine and compiled-C file services are also reimplemented and
verified: `make fediff-run` drives 20,000 whole-module scenarios over the
extracted CD scenery/texture tree and synthetic part-3 fixtures, comparing
every import call and output, GLOBAL, the heap and `.data`, with zero
differences. The run includes 7,272 successful texture loads and 3,636 FACIL
section reads. `+0x78`, the Scenery Library, is World.DLL callbacks like
FACIL's dialogs: `make fesldiff-run` drives 20,000 scenarios through a stand-in
for World over a writable fixture, with zero differences.

`LANDME` is also reimplemented, verified and running in-game. Its harness gives
each side a separate retail FE over the same fixture tree and steers the
aircraft through all guidance phases: 20,000 scenarios, 18,881 runways found,
1,119 no-runway paths and zero differences. See
[docs/modules/landme.md](docs/modules/landme.md).
`FACIL` is done, verified and running in-game; its dialogs are callbacks for
World.DLL and were verified externally. Its harness found that FE `+0x84` and
`+0x88` return stack leftovers; FE now reproduces them, and
`make facildiff-fe-run` checks OpenFS98 FE and FACIL together. `BOOSTER`,
which waited for FE, is done too: `make boosterdiff-run` runs both sides in
turn and drains its worker thread after every operation (20,000 scenarios,
zero differences, see [docs/modules/booster.md](docs/modules/booster.md)).
That finishes round 4. Round 5 picked `DEMO`, the flight videos, which
ADVDRV imports, and it is done too (`make demodiff-run`, 20,000 scenarios,
zero differences, see [docs/modules/demo.md](docs/modules/demo.md)). `ADVDRV`,
the adventure and lesson interpreter that DEMO feeds, is done too
(`make advdrvdiff-run`, 20,000 scenarios, zero differences, see
[docs/modules/advdrv.md](docs/modules/advdrv.md)). Round 6 picked `WINDOW`,
the view windows and the eyepoint, and it is done too (`make windowdiff-run`,
20,000 scenarios, zero differences, see
[docs/modules/window.md](docs/modules/window.md)). Its harness models the
host's window services and the host's integer-divide repair, which retail
relies on. `WINMSG`, the message bar, was set aside for `ofstest` with
`REALITY`, but turned out to be testable externally: what it shows is the
host calls it makes, and the host's side is four fields of the window record
(`make winmsgdiff-run`, 20,000 scenarios, zero differences, see
[docs/modules/winmsg.md](docs/modules/winmsg.md)). It found that a scrolling
message at an ordinary frame rate can start before its buffer and overwrite a
slot's text pointer, a retail heap corruption now reproduced. `REALITY`,
crashes and the crash analysis, was externally testable for the same reason:
it draws only lines (`make realitydiff-run`, 20,000 scenarios, zero
differences, see [docs/modules/reality.md](docs/modules/reality.md)).
`ANALYSIS` had the same shape and is done too (round 8 of
[docs/preflight.md](docs/preflight.md), `make analysisdiff-run`, 20,000
scenarios, zero differences, see [docs/modules/analysis.md](docs/modules/analysis.md)).
It found that retail's sample compaction depends on what MSVCRT `memmove`
leaves in ecx; OpenFS98 reproduces the CD runtime's.
Round 9 surveyed the six modules left before HG2D and picked `CONTROLS`,
which is in progress. Its hand-written half is reimplemented, and its command
dispatcher and all 342 handlers (engines, radios, autopilot, the eye, slew,
the mouse yoke) are verified, as is the per-frame joystick poll, axes and
buttons: `make controlsdiff-run` drives them against retail over 20,000
scenarios with zero differences
(see [docs/modules/controls.md](docs/modules/controls.md)). Its per-frame
CHAIN callbacks and deinit are not driven yet. The rest of its compiled-C
half is not verified: the key-binding config is transliterated but
not driven, and the force-feedback setup,
the joystick-config persistence, the assignment dialog and the DirectInput
force dispatch are still placeholders. So CONTROLS is not yet installed.
`MULTIP` and `MPCHAT` follow it, then `G3D`.

Other useful work, in no particular order:

1. **Reach UTIL's menu paths.** A hands-on session exercised 19 of its 47
   services in-game, but the flight, lesson and options screens -- where the
   file listings and the exchange engine run -- were not reachable under wine.
   Worth retrying with `make util-trace UTIL_TRACE_SECS=600` whenever the sim
   gets further, or on Windows.
2. **The `English\` modules** — `Flights`, `Options`, `Alerts` and the rest
   own 64 of the host's 65 dialogs; `make graph` does not see them yet.
3. **The host's 84 services that call no platform API** — portable logic, and
   the specification a native host has to satisfy.

### Order of work

Bottom of the dependency graph first, because everything imports it and
nothing in it imports anything interesting:

1. ~~**`GLOBAL`** (54 bytes)~~ — done. Pure data: the shared state block.
2. ~~**`CHAIN`** (966 bytes)~~ — done, verified and running in-game.
3. ~~**`NUMERIC`** (14,684 bytes)~~ — done, verified and running in-game.
4. ~~**`SLEW`** (1,270 bytes)~~ — done and verified against retail.
5. ~~**`UTIL`** (11,062 bytes)~~ — done, verified and running in-game.
6. ~~**`ATC`** (3,840 bytes)~~ — done, verified and running in-game.
7. ~~**`WEATHERM`** (8,774 bytes)~~ — done, verified and running in-game.
8. ~~**`ACONTAIN`** (5,504 bytes)~~ — done, verified and running in-game.
9. ~~**`SOUND`** (15,174 bytes)~~ — done, verified and running in-game.
10. ~~**`DYNAMIC`** (12,300 bytes)~~ — done, verified and running in-game.
11. ~~**`LANDME`** (4,094 bytes)~~ — done, verified against retail FE over the
    real scenery tree, and running in-game.
12. ~~**`FACIL`** (23,045 bytes)~~ — done, verified and running in-game.
    ~~**`FE`** (49,016 bytes)~~ — done, verified and running in-game.
    ~~**`BOOSTER`** (8,024 bytes)~~ — done, verified and running in-game.
    ~~**`DEMO`** (16,864 bytes)~~ — done, verified and running in-game.
    ~~**`ADVDRV`** (29,330 bytes)~~ — done, verified and running in-game.
    ~~**`WINDOW`** (24,992 bytes)~~ — done, verified and running in-game.
    ~~**`WINMSG`** (4,416 bytes)~~ — done, verified and running in-game.
    ~~**`REALITY`** (4,822 bytes)~~ — done, verified and running in-game.
    ~~**`ANALYSIS`** (8,086 bytes)~~ — done, verified and running in-game.
    Round 9 of the [preflight](docs/preflight.md) surveyed what is left.
    Next: **`CONTROLS`** (48,406 bytes, the keyboard, joystick and force
    feedback), then `MULTIP` (43,318) with `MPCHAT` (3,104) behind it, then
    `G3D` (92,518), `PANELS` (50,038) and `MAIN` (119,648).
13. `HG2D` (390 KB, the 2D rasteriser) last. It is 26% of the project on its
   own.

`GLOBAL`'s state block is the shared workspace: every module worked through
names more of its 4776 bytes, and those names are the project's real asset.
