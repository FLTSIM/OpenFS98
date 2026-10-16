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
sudo apt install gcc-mingw-w64-i686-win32         # 32-bit toolchain
7z x ~/Downloads/MSFlightSimulator98.iso -ocd     # the CD, for staging
./tools/extract.sh ~/Downloads/MSFlightSimulator98.iso   # orig/ reference binaries
make stage                                        # game/ — a real install
make && make smoke                                # build, then boot the game
```

`cd/`, `orig/`, `game/` and `bin/` are gitignored — no game content goes in the
repository. The only derived artifact that is committed is
`docs/setup_map.tsv`, the CD-to-install file map read out of `Setup.exe`.

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
27/27 modules, 95 gauges — and so does the build with OpenFS98's `ACONTAIN`,
`ATC`, `CHAIN`, `DYNAMIC`, `GLOBAL`, `NUMERIC`, `SLEW`, `SOUND`, `UTIL` and
`WEATHERM` modules substituted, with identical counts.

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
| `FE` | 18 | 49,016 | **in progress** — the grid conversions, assembly engine and compiled-C file services are reimplemented and verified (20,000 whole-module scenarios, zero differences); only the Scenery Library dialog remains for `ofstest` — see [docs/modules/fe.md](docs/modules/fe.md) |
| `NUMERIC` | 17 | 14,684 | **reimplemented, verified, and running in-game** — 545,778 differential comparisons, zero mismatches — see [docs/modules/numeric.md](docs/modules/numeric.md) |
| `UTIL` | 20 | 11,062 | **reimplemented, verified, and running in-game** — all 47 services; 19 seen called in a hands-on session, the modal alert among them — see [docs/modules/util.md](docs/modules/util.md) |
| `FLTSIM98.EXE` | 1 (host) | 82,016 | 194 services mapped, 21 with confirmed prototypes, 11 verified in-process, 2 implemented — see [docs/modules/host.md](docs/modules/host.md) |

Retail baseline and the OpenFS98 build -- ACONTAIN, ATC, CHAIN, DYNAMIC, GLOBAL,
NUMERIC, SLEW, SOUND, UTIL and WEATHERM all installed -- both reach the panel: 27/27 modules, 95 gauges,
`make smoke` PASS.

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

Round 2 of the [preflight](docs/preflight.md) is done apart from `REALITY`:
`WEATHERM` and `ACONTAIN` are reimplemented and verified -- see
[docs/modules/weatherm.md](docs/modules/weatherm.md) and
[docs/modules/acontain.md](docs/modules/acontain.md). `REALITY` draws and runs
dialogs, so it belongs in `ofstest`, not an external harness.

Round 3 picked `SOUND`, then `DYNAMIC`, and both are reimplemented and
verified -- see [docs/modules/sound.md](docs/modules/sound.md) and
[docs/modules/dynamic.md](docs/modules/dynamic.md). `BOOSTER` works on its
own thread and is left for later.

Round 4 surveyed FE, LANDME, FACIL and BOOSTER -- see the
[preflight](docs/preflight.md) -- and **FE is in progress**
([docs/modules/fe.md](docs/modules/fe.md)). Its grid conversions
(`+0x60`..`+0x6c`) are reimplemented and verified by vectors (`make
fegriddiff-run`, 7,742,499 comparisons, 0 mismatches). Its whole hand-written
assembly engine and compiled-C file services are also reimplemented and
verified: `make fediff-run` drives 20,000 whole-module scenarios over the
extracted CD scenery/texture tree and synthetic part-3 fixtures, comparing
every import call and output, GLOBAL, the heap and `.data`, with zero
differences. The run includes 7,272 successful texture loads and 3,636 FACIL
section reads. `+0x78`, the Scenery Library dialog, goes to `ofstest`. FE is
not in `MODULES`, so a partial one is never installed.

`LANDME` is next, and is not blocked on FE's record format: its
harness loads FE for real over FE's fixture tree. `FACIL` follows, and
`BOOSTER` still waits for FE. `WINMSG` and `REALITY` draw and run dialogs, and
belong in `ofstest`.

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
11. Then outward: round 4 -- FE's non-dialog engine is done; `LANDME` is next,
   followed by `FACIL`; `WINMSG`, `REALITY` and FE's dialog need `ofstest`.
12. `HG2D` (390 KB, the 2D rasteriser) last. It is 26% of the project on its
   own.

`GLOBAL`'s state block is the shared workspace: every module worked through
names more of its 4776 bytes, and those names are the project's real asset.
