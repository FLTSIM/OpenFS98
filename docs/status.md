# Module status

Moved out of the README: one row per module, with the evidence. Results are as
of 2026-11-06; later work is in the per-module documents. What is still retail
is in [remaining-work.md](remaining-work.md), known gaps in
[open-differences.md](open-differences.md).

Retail baseline and the OpenFS98 build -- ACONTAIN, ADVDRV, ANALYSIS, ATC,
BOOSTER, CHAIN, CONTROLS, DEMO, DYNAMIC, FACIL, FE, GLOBAL, HG2D, LANDME, NUMERIC,
PANELS, REALITY, SLEW, SOUND, UTIL, WEATHERM, WINDOW and WINMSG installed in
`Modules/`, with World, Alerts, Help, Sim, Views, Flights and MP_DIALOG installed in `English/`,
both reach the panel: 27/27 runtime modules, 95 gauge trace mentions, `make smoke` PASS.
MP_DIALOG post-install private startup also passes with the private Wine SEHFIX
shim; its 95 mentions contain 58 load records for 29 distinct gauge paths.
(MAIN, G3D, MULTIP and MPCHAT are also in `MODULES`; see their documents.)

| module | id | `.text` | state |
| --- | --- | --- | --- |
| `GLOBAL` | 2 | 54 | **reimplemented and running in-game** |
| `SLEW` | 9 | 1,270 | **reimplemented and verified** — 200,000 differential vectors, byte-identical |
| `ATC` | 24 | 3,840 | **reimplemented, verified, and running in-game** — ATIS and the tower's clearance exchange; 20,000 whole-module differential scripts comparing its whole `.data` image, zero differences — see [modules/atc.md](modules/atc.md) |
| `CHAIN` | 4 | 966 | **reimplemented, verified, and running in-game** — the event bus; 20,000 whole-module differential scripts, zero differences — see [modules/chain.md](modules/chain.md) |
| `WEATHERM` | 27 | 8,774 | **reimplemented, verified, and running in-game** — the weather: areas, blending, temperature and pressure by altitude, wind; 20,000 whole-module differential scripts comparing GLOBAL and its whole `.data` image, zero differences — see [modules/weatherm.md](modules/weatherm.md) |
| `ACONTAIN` | 23 | 5,504 | **reimplemented, verified, and running in-game** — the aircraft container: finds aircraft by title, resolves their paths, loads the flight and visual models, lists them; 20,000 whole-module differential scripts against generated aircraft trees, zero differences — see [modules/acontain.md](modules/acontain.md) |
| `SOUND` | 39 | 15,174 | **reimplemented, verified, and running in-game** — engine, warning, marker and Morse ident sounds, driven through the host's voice engine; 20,000 whole-module differential scripts comparing its whole `.data` image, zero differences — see [modules/sound.md](modules/sound.md) |
| `DYNAMIC` | 6 | 12,300 | **reimplemented, verified, and running in-game** — dynamic scenery: an interpreter for the airport traffic programs in scenery files, three of whose opcodes write programs of their own; 20,000 whole-module differential scripts comparing GLOBAL, the heap and its whole `.data` image, zero differences — see [modules/dynamic.md](modules/dynamic.md) |
| `FE` | 18 | 49,016 | **reimplemented, verified, and running in-game** — the grid conversions, assembly engine, compiled-C file services (20,000 whole-module scenarios) and the Scenery Library (20,000 scenarios through a World.DLL stand-in), zero differences — see [modules/fe.md](modules/fe.md) |
| `WORLD` | `0x80000006` | 115,275 | **reimplemented and integrated at `English/World.DLL`** — all 19 registrations have native acceptance; real MAIN/FE/FACIL owner flows and both weather modes pass the installed-game UI runner — see [modules/world.md](modules/world.md) |
| `MP_DIALOG` | `0x8000000a` | 25,401 | **complete and integrated at `English/MP_DIALOG.DLL`** — three registrations/seven pages; native UI/lifetime/exception acceptance, 89 negative comparisons, paired actual-owner session with peer removal and capacity/OK callbacks; private startup — see [modules/mpdialog.md](modules/mpdialog.md#completion-and-integration-2026-11-05) |
| `LANDME` | 26 | 4,094 | **reimplemented, verified, and running in-game** — nearest-runway selection and all eleven nonzero landing-guidance phases; 20,000 whole-module scenarios over retail FE and real scenery, zero differences — see [modules/landme.md](modules/landme.md) |
| `BOOSTER` | 34 | 8,024 | **reimplemented, verified, and running in-game** — the performance boosters: CD scenery copied to a disk cache on a worker thread, nearest first, and evicted out of reach; 20,000 whole-module scenarios, zero differences — see [modules/booster.md](modules/booster.md) |
| `DEMO` | 7 | 16,864 | **reimplemented, verified, and running in-game** — flight videos (record and play), instant replay and the four dialogs' callbacks; 20,000 whole-module scenarios, zero differences — see [modules/demo.md](modules/demo.md) |
| `FACIL` | 37 | 23,045 | **done** — verified (index, queries and the three dialogs' callbacks) and running in-game — see [modules/facil.md](modules/facil.md) |
| `NUMERIC` | 17 | 14,684 | **reimplemented, verified, and running in-game** — 545,778 differential comparisons, zero mismatches — see [modules/numeric.md](modules/numeric.md) |
| `WINDOW` | 10 | 24,992 | **reimplemented, verified, and running in-game** — the view windows, their titles, status lines and layout, and the eyepoint for every view mode with the transitions between them; 20,000 whole-module scenarios against a model of the host's window services, zero differences — see [modules/window.md](modules/window.md) |
| `MPCHAT` | -1 | 3,104 | **reimplemented, verified, and running in-game** — the multiplayer chat window: transcript, player list, line and Send, driven by MULTIP on CHAIN list 0x1d; 20,000 whole-module scenarios against a model of the window system, zero differences — see [modules/mpchat.md](modules/mpchat.md) |
| `WINMSG` | 22 | 4,416 | **reimplemented, verified, and running in-game** — the message bar: queued messages in seven slots, plain or scrolling, split to fit, appended to; 20,000 whole-module scenarios against a model of the host's window record, zero differences — see [modules/winmsg.md](modules/winmsg.md) |
| `REALITY` | 32 | 4,822 | **reimplemented, verified, and running in-game** — crashes, stress damage, the cracked windscreen and the crash analysis's graph; 20,000 whole-module scenarios, zero differences — see [modules/reality.md](modules/reality.md) |
| `ANALYSIS` | 29 | 8,086 | **reimplemented, verified, and running in-game** — landing and manoeuvre analysis and course tracking: the samples, the trail's display list, the three dialogs; 20,000 whole-module scenarios, zero differences — see [modules/analysis.md](modules/analysis.md) |
| `CONTROLS` | 16 | 48,406 | **reimplemented, verified, and running in-game** — the keyboard, joysticks and mouse: 342 command handlers, the joystick poll and configs, the key bindings, force feedback over DirectInput 5 and the key-assignment dialog's callbacks; 20,000 whole-module scenarios, zero differences — see [modules/controls.md](modules/controls.md) |
| `PANELS` | 15 | 50,038 | **reimplemented, verified, and running in-game** — the instrument panel and the gauges' runtime: bitmap scalers, eight element types, the gauge variables; 3,000 whole-module scenarios through the CD's gauges and 20,000 cases per image routine, zero differences — see [modules/panels.md](modules/panels.md) |
| `Gauges/*.GAU` | 19 | 63,472 total | **120 of 120 rebuilt from C:** all exposed callbacks, data and bitmap resources; 26,687,688 standalone vectors at preferred addresses and under forced relocation, 348 mutations caught, 1,000 strict PANELS scenarios and 74/74 matching flight comparisons. Included in build/install/revert — see [modules/gauges.md](modules/gauges.md) |
| `HG2D` | 13 | 389,734 | **reimplemented, verified, and in `MODULES`** — all 40 public services and 18 GPU dispatch cells; 59 suites pass, 40.64 million large-run operations match retail, and 247 new mutations are caught; in private copies of the game, eight flights match retail frame for frame on the software renderer and six on Direct3D (`tools/hg2d_frames.sh`); not yet flown by hand, non-x86 validation remains — see [modules/hg2d.md](modules/hg2d.md#in-the-game) |
| `UTIL` | 20 | 11,062 | **reimplemented, verified, and running in-game** — all 47 services; 19 seen called in a hands-on session, the modal alert among them — see [modules/util.md](modules/util.md) |
| `FLTSIM98.EXE` | 1 (host) | 82,016 | 194 services mapped; **all 194 reimplemented and verified in-process** against the real host (`make hostdiff-run`) — see [modules/host.md](modules/host.md) |
| `MAIN` | 3 | 119,648 | **complete** — slices 1 to 44 verified, booting and every menu command opening in the game; in `MODULES`, not yet flown by hand — see [modules/main.md](modules/main.md) |
| `G3D` | 14 | 92,518 | **reimplemented, verified, and running in-game** — the whole bytecode machine, its depth queue, services and CHAIN callbacks, 20,000 `g3ddiff` scenarios, 0 differ; a portable C mesh compiler checked against retail's instructions — see [modules/g3d.md](modules/g3d.md) |
| `MULTIP` | 35 | 43,318 | **done** — whole-module acceptance and startup smoke passed: all eleven public entries, workers, protocols, CHAIN callbacks and dialogs, 43 suites of 20,000 scenarios plus eight dispatch-family reruns, 0 differ, all 456 mutations caught; in `MODULES`, installed as `MULTIPLAYER.DLL` — see [modules/multip.md](modules/multip.md) |
| `ADVDRV` | 21 | 29,330 | **reimplemented, verified, and running in-game** — adventures and lessons; 20,000 scenarios, 0 differ — see [modules/advdrv.md](modules/advdrv.md) |

## Notes from the module rounds

Findings that came out of the work and are easy to lose; the full accounts are
in the module documents and [preflight.md](preflight.md) (which has the rounds).

Round 2 of the [preflight](preflight.md) is done: `WEATHERM` and
`ACONTAIN` are reimplemented and verified -- see
[modules/weatherm.md](modules/weatherm.md) and
[modules/acontain.md](modules/acontain.md). `REALITY`, set aside then
for `ofstest`, was done externally in round 7.

Round 3 picked `SOUND`, then `DYNAMIC`, and both are reimplemented and
verified -- see [modules/sound.md](modules/sound.md) and
[modules/dynamic.md](modules/dynamic.md). `BOOSTER` works on its
own thread and is left for later.

Round 4 surveyed FE, LANDME, FACIL and BOOSTER -- see the
[preflight](preflight.md) -- and **FE is done, verified and running in-game**
([modules/fe.md](modules/fe.md)). Its grid conversions
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
[modules/landme.md](modules/landme.md).
`FACIL` is done, verified and running in-game; its dialogs are callbacks for
World.DLL and were verified externally. Its harness found that FE `+0x84` and
`+0x88` return stack leftovers; FE now reproduces them, and
`make facildiff-fe-run` checks OpenFS98 FE and FACIL together. `BOOSTER`,
which waited for FE, is done too: `make boosterdiff-run` runs both sides in
turn and drains its worker thread after every operation (20,000 scenarios,
zero differences, see [modules/booster.md](modules/booster.md)).
That finishes round 4. Round 5 picked `DEMO`, the flight videos, which
ADVDRV imports, and it is done too (`make demodiff-run`, 20,000 scenarios,
zero differences, see [modules/demo.md](modules/demo.md)). `ADVDRV`,
the adventure and lesson interpreter that DEMO feeds, is done too
(`make advdrvdiff-run`, 20,000 scenarios, zero differences, see
[modules/advdrv.md](modules/advdrv.md)). Round 6 picked `WINDOW`,
the view windows and the eyepoint, and it is done too (`make windowdiff-run`,
20,000 scenarios, zero differences, see
[modules/window.md](modules/window.md)). Its harness models the
host's window services and the host's integer-divide repair, which retail
relies on. `WINMSG`, the message bar, was set aside for `ofstest` with
`REALITY`, but turned out to be testable externally: what it shows is the
host calls it makes, and the host's side is four fields of the window record
(`make winmsgdiff-run`, 20,000 scenarios, zero differences, see
[modules/winmsg.md](modules/winmsg.md)). It found that a scrolling
message at an ordinary frame rate can start before its buffer and overwrite a
slot's text pointer, a retail heap corruption now reproduced. `REALITY`,
crashes and the crash analysis, was externally testable for the same reason:
it draws only lines (`make realitydiff-run`, 20,000 scenarios, zero
differences, see [modules/reality.md](modules/reality.md)).
`ANALYSIS` had the same shape and is done too (round 8 of
[preflight.md](preflight.md), `make analysisdiff-run`, 20,000
scenarios, zero differences, see [modules/analysis.md](modules/analysis.md)).
It found that retail's sample compaction depends on what MSVCRT `memmove`
leaves in ecx; OpenFS98 reproduces the CD runtime's.
Round 9 surveyed the six modules left before HG2D and picked `CONTROLS`,
which is done, verified and running in-game. The command dispatcher and all
342 handlers, the joysticks (the poll, the watcher, their configs and
defaults), the key bindings, force feedback over DirectInput 5 and the
key-assignment dialog are reimplemented, and `make controlsdiff-run` drives
them against retail over 20,000 scenarios with zero differences. The
dialog, `English\Options.DLL`'s `0x140a`, calls back into CONTROLS through a
parameter block, so it is verified against a stand-in for that dialog built
from its listing (see [modules/controls.md](modules/controls.md)).
**`MULTIP` has passed whole-module acceptance and startup smoke testing**:
all eleven public entries, workers, protocols, CHAIN callbacks and dialogs
are covered by 43 suites of 20,000 scenarios each, plus eight dispatch-family
reruns, with zero differences. All 456 mutations are caught. The game smoke
test confirms the replacement, 27/27 modules and 95 gauges; it restores the
previous game DLL afterward. MULTIP is now in `MODULES` and `make install`
maps it to `MULTIPLAYER.DLL`. See [modules/multip.md](modules/multip.md)
for the evidence and remaining portability/provider limits.
**`MPCHAT` is done, verified and running in-game**: the chat window, against
a model of the window system hooked into its import table, 20,000 scenarios
with zero differences (see [modules/mpchat.md](modules/mpchat.md)).
**`G3D` is done, verified and running in-game**: the whole bytecode machine,
its depth queue, services and CHAIN callbacks, 20,000 `g3ddiff` scenarios
with zero differences, and a portable C mesh compiler checked against
retail's instructions (see [modules/g3d.md](modules/g3d.md)).

**Next: Options.** The whole game, all retail against all ours, matches
frame for frame on all 74 flights with nobody flying, and flown by script on eight flights, on
both renderers. Alerts, Help, Sim, Views and Flights were installed on 2026-11-02. What is still open
is listed in [open-differences.md](open-differences.md). All 120 gauges are now rebuilt. SIM1, the aircraft models, SIMSUI
and an executable around `HOST.DLL` remain: [remaining-work.md](remaining-work.md).
