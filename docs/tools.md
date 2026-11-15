# Tools and make targets

Every harness and analysis script, moved out of the README. The full list of
verification commands with their expected results and run times is in
[Baselines](#baselines) below; this is what each one is. Setup is in
[building.md](building.md).

| | |
| --- | --- |
| `make difftest-run` | drive a rebuilt module and retail over identical synthetic state and diff the result; `N=...` sets the vector count |
| `make utildiff-run` | compare individual rebuilt UTIL services against retail's, service by service |
| `make utildiff-portable` | the same with the x87 assembly compiled out, as an ARM build would use |
| `make -f tools/options.mk optionseventsdiff-run` | Options.DLL's dialogs and tab pages: retail's routines and ours over the same logged stubs of MFC, SIMSUI, Win32 and HOST, with randomised fixtures, mid-call mutations and (`MODE=unwind`) a throw at every call; `make -f tools/options.mk options-breaks` breaks it on purpose; `make -f tools/options.mk options` builds the native DLL |
| `make numericdiff-run` | compare all NUMERIC formatter/parser ids and ranged parses against retail |
| `make atcdiff-run` | drive retail and OpenFS98 ATC side by side over random scripts of ATIS, clearances and frames, comparing every import call and the whole `.data` image; `N=...` sets the count |
| `make acontaindiff-run` | the same for ACONTAIN, against generated aircraft trees: loads, lists, queries and models, with file I/O passed through and fake `.SIM`/`.MDL` modules, comparing every import call, GLOBAL, the heap and the `.data` image |
| `make sim1` / `sim1-install` / `sim1-revert` | build SIM1 into `bin/SIM1.sim`, install at `Sim/SIM1.sim`, or restore the verified original; included in shared build/install/revert |
| `make -f tools/sim1.mk sim1-regression-run N=20000` | `src/sim1/sim1.c`'s six services and fifteen state operations (its isolated test builds), including forced relocation |
| `make -f tools/sim1.mk sim1rdiff-run` | the decompiled SIM1 routines (`src/sim1/sim1_small.c`) against retail one at a time, over in-flight snapshots; `sim1r-breaks` breaks them on purpose, `sim1-tables` regenerates the routine contracts, `sim1-snapshots` the snapshots |
| `make sim1`, `make -f tools/sim1.mk sim1native-flights` | the native SIM1.sim (`src/sim1/sim1_small.c` over retail's data, `src/sim1/sim1_native.c`, bytes from `tools/gen_sim1_native.py`) and its game acceptance, the 74 scripted flights against retail; all 74 stock flights, 1,200 passes with controls, comparing per-pass SIM1 numeric state and rendered frames in disposable game copies; `make sim1-install` installs it |
| `make mdls` / `make mdls-install` / `make mdls-revert` | build, install or restore all eleven normal/crash MDLs; included in shared build/install/revert targets and used by `make run` |
| `make mdl-normal-diff-run` | all eight normal MDL update callbacks against retail, at preferred and forced-relocation addresses: state, entire `.data`, GLOBAL, copy calls and preserved registers; `N=...` sets inputs per model (70,000 by default); callback slice only, no MDL installation |
| `make mdl-normal-breaks` | six isolated callback mutations, each required to produce its expected behavioral mismatch |
| `make mdl-normal-modules` / `mdl-normal-modules-run` | build all eight drop-in normal MDLs and compare their loaded lifecycle, update callbacks, relocated data and resources against retail (70,000 scenarios per model in three address modes by default) |
| `make mdl-normal-module-breaks` | six isolated broken DLL builds; require the expected pointer/data/lifecycle/resource mismatch |
| `make mdl-normal-game` / `mdl-normal-render-break` | compare scripted view/control frames on eight flights in private otherwise-retail games (`JOBS=1..4`), or prove that turning the rendered RG aircraft changes the frames; primary game remains untouched |
| `make mdl-crash-modules` / `mdl-crash-diff-run` | build the three crash MDLs from the decompiled `src/mdl/mdl_crash.c`; compare lifecycle, CHAIN callbacks, state, data, live registers and import events against retail (2,000 scenarios per model in two address modes) |
| `make mdl-crash-breaks` | the decompiled crash models broken on purpose (`tools/mdl_crash_breaks.py`, run from a frozen copy) |
| `make -f tools/host_rt.mk hostrtdiff-run N=...` | the decompiled host runtime (`src/host/runtime/`) routine by routine against retail FLTSIM98.EXE (`tools/difftest/hostrtdiff.c`, its models in `hostrt_semantics.inc` and `hostrt_windows.inc`; `HOSTRT_COVER=op,text` counts coverage); tables, `.rdata` and the image's pointers from `tools/gen_host_rt_tables.py` over `tools/host_rt_survey.py`'s routine list |
| `make host-exe` (`make -f tools/host_rt.mk host-rt-exe`) | `bin/OPENFS98.EXE`, the executable from the decompiled runtime and the services (`src/host/host_rt_exe.c`); fly it with `tools/frames_many.sh` and `HG2D_FRAMES_HOST`, drive its dialogs with `tools/host_runtime_ui.sh` and `HOST_UI_EXE`; its resources from `tools/gen_host_resources.py` (the `.rsrc` tree only) |
| `python3 tools/host_rt_breaks.py [N]` | the decompiled host runtime broken on purpose (run from a frozen copy) |
| `make mdl-crash-game` / `mdl-crash-render-break` | three private 1,200-pass crash flights (`JOBS=1..3`) requiring the intended crash MDL, or a valid pose mutation required to change rendered frames; source flights and primary game untouched |
| `make sounddiff-run` | the same for SOUND, against generated `SOUND.CFG` trees and the CD's seven: frames, events, wind, faders and CHAIN messages, with a model of the host's voice engine, comparing every import call, GLOBAL, `FLTSIM98.CFG`, the heap and the `.data` image |
| `make dynamicdiff-run` | the same for DYNAMIC, against the dynamic-scenery programs of the CD's twelve airport BGLs, often mutated, and three synthetic blocks: frames, the three CHAIN lists, the user's aircraft and MULTIP's players steered onto what the programs test; faults and hangs compared as outcomes |
| `make fegriddiff-run` | FE's grid conversions (`+0x60`..`+0x6c`), retail and OpenFS98 loaded side by side and called directly over every table edge at every level and random values; a fault in either side counts as a difference |
| `make fediff-run` | FE's assembly engine and compiled-C file services, retail and OpenFS98 loaded side by side over the extracted CD scenery/texture tree plus self-checking synthetic compressed-texture and BGL-section fixtures, comparing every import call and output, GLOBAL, the heap and the whole `.data` image |
| `make landmediff-run` | LANDME's seven-state landing assistant against retail, each side backed by its own retail FE over the real scenery tree; compares every import call, GLOBAL, the heap and both modules' whole `.data` images |
| `make facildiff-run` | whole-module FACIL against retail, each side backed by a separate retail FE: a synthetic five-level rebuild, 20,000 seeded synthetic caches through the `+0x18`/`+0x1c`/`+0x20` queries, then the real CD index (with all sixteen boosters) and 10,000 dialog scenarios driven by a stand-in for World.DLL; compares the caches, call transcript (with string comparisons), returned records, GLOBAL, heap and both modules' state |
| `make boosterdiff-run` | whole-module BOOSTER, worker thread included: retail and OpenFS98 run in turn over identical cache trees (four CD boosters, synthetic ones with every record type), the thread drained after every operation by counting posted 0x401s against SetEvents; MessageBoxA and the file calls hooked and logged; compares the transcript, the whole `.data` image, both heaps, GLOBAL, FE's records and the cache trees |
| `make windowdiff-run` | whole-module WINDOW: retail and OpenFS98 in turn over scripts of windows created, closed, maximised, painted frame after frame through every eye mode and transition, and laid out in situations; a model of the host's window services and z-order, and of its filter that repairs integer divide faults; compares the transcript (with each frame's eye), the whole `.data` image, the arena and GLOBAL. `make window-breaks` breaks the module on purpose, one place at a time, and checks the harness notices |
| `make mpchatdiff-run` | whole-module MPCHAT, the chat window: retail and OpenFS98 in turn over scripts of sessions begun and ended, player lists, chat lines sent and received, the menu command and keys, window messages and the user's typing and selection, against a model of USER32 (windows, ListBox, Edit, RichEdit, scroll state) hooked into both import tables; compares every call, the whole `.data` image, the heap and the model. `make mpchat-breaks` breaks it on purpose |
| `make winmsgdiff-run` | whole-module WINMSG, the message bar: retail and OpenFS98 in turn over scripts of messages shown, appended, cleared and run frame after frame (plain and scrolling, split to fit), with a model of the host's window record and heap, a CHAIN that answers the done message and re-enters, and situation records; compares the transcript with every string drawn, the whole `.data` image, the arena and GLOBAL. `make winmsg-breaks` breaks it on purpose |
| `make realitydiff-run` | whole-module REALITY, crashes and damage: retail and OpenFS98 in turn through every crash gate, the countdown, the reload and the crash analysis (a stand-in for its dialog, retail UTIL and NUMERIC), the stress rolls and the cracked windscreen; HG2D's lines logged with their colour and view; compares the transcript, the whole `.data` image, the arena, GLOBAL and UTIL's and NUMERIC's state. `make reality-breaks` breaks it on purpose |
| `make analysisdiff-run` | whole-module ANALYSIS, landing and manoeuvre analysis and course tracking: retail and OpenFS98 in turn over the frames, the landing thresholds, the compaction, the trail and all three dialogs (a stand-in for the dialog engine, retail UTIL and NUMERIC), with the register leftovers retail reads (CHAIN's, memset's, the CD MSVCRT memmove's) modelled; HG2D's lines and G3D's display list logged; compares the transcript, the whole `.data` image, the arena, GLOBAL and UTIL's and NUMERIC's state. `make analysis-breaks` breaks it on purpose |
| `make controlsdiff-run` | whole-module CONTROLS: retail and OpenFS98 side by side over commands (through the dispatcher and CHAIN list 0xb), the axis, engine and pilot-event entries, the CHAIN frames, the joystick poll and watcher over scripted sticks, the key bindings, force feedback and the key-assignment dialog; the host's profile services passed through to fixture CFG files, models of its accelerator tables and joysticks, a DirectInput 5 stand-in and a stand-in for Options.DLL's dialog; compares GLOBAL, the whole `.data` image, the import-call transcript and the profile files. `make controls-breaks` breaks it on purpose |
| `make panelsdiff-run` | whole-module PANELS: retail and OpenFS98 in turn through the CD's own gauges and panel files over scripts of aircraft loaded, frames, resets, situations, commands, windows and paints, against stubs of the host, GLOBAL, CHAIN, MAIN, WINDOW and the HG2D model, with GDI virtualised; compares the transcript, the `.data` image, every block and each gauge's data. `make panelsunit-run` calls its 28 image routines alone; `make panels-breaks` breaks it on purpose |
| `make gauges` / `make gauges-install` / `make gauges-revert` | build, install or restore all 120 gauges, also included in the shared targets |
| `make gauges-check N=20000` | all 1,317 exposed entries: complete data/resources, PANELS transcript, raw x87 results/flags; add `GAUGES_RELOCATE=1` to force relocation |
| `make gauges-breaks` | 348 independent collection mutations; requires explicit mismatches |
| `make gauges-panels N=1000` | same rebuilt PANELS on both sides with retail/rebuilt gauge collection; strict state/heap/surface/transcript comparisons |
| `make -f tools/gauges.mk all-gauges-hangs` | 16 watchdog comparisons for the two compass callbacks' retail nontermination |
| `make -f tools/gauges.mk gaugesdiff-run` | first gauge, `737WARNG`: retail/rebuilt callbacks, complete data image and bitmap; 20,000 scenarios both at its preferred base and forcibly relocated. `gauges-breaks` requires explicit failures from 15 isolated mutations |
| `make -f tools/gauges.mk suctiondiff-run` | Cessna suction: full callback/data/two-bitmap comparison; 20,000 scenarios plus 240,288 numeric vectors across x87 precision/rounding modes, repeated under forced relocation. `suction-breaks` checks 26 isolated mutations |
| `make -f tools/gauges.mk gauges-panels-run N=1000` | same PANELS on both sides, retail/rebuilt warning and suction gauges, other gauges retail; whole-panel scripts and strict fault-outcome comparison in a private Wine prefix |
| `make -f tools/gauges.mk gauges-game-run` | 737 Hong Kong flight, retail twice then rebuilt warning gauge, 900 FRAMELOCK passes; requires actual selected-gauge loads and identical frames in private game copies |
| `make -f tools/gauges.mk suction-game-run` | Cessna St George flight, retail twice then rebuilt suction gauge, 900 FRAMELOCK passes; requires actual gauge loads and identical frames in private game copies |
| `make g3ddiff-run` | whole-module G3D, the 3D pipeline: retail and OpenFS98 in turn over generated scenery programs (every opcode, with operands steered onto the boundaries they are compared with), the depth queue, the Linkage services and the CHAIN callbacks run frame after frame over FE's scene stream (stubs of FE, HG2D, CHAIN, WINDOW and the host; retail UTIL and WINDOW), the host's divide repair modelled; compares every import call, every register, the whole `.data` image, GLOBAL and the arena after every operation. 20,000 scenarios, 0 differ (~5 min). `make g3d-breaks` breaks it on purpose |
| `make g3dmeshdiff-run` | the mesh compiler (`0x38`) three ways -- retail's own G3D.DLL, the C on the x87, the C on its software 80-bit float -- over generated polygons under 24-, 53- and 64-bit precision: 100,000 polygons, 0 differ. `make g3ddiff-meshc-run` runs `g3ddiff` with the portable compiler in the module |
| `make multipdiff-run` | whole MULTIP harness: all eleven public entries, transport/workers, profiles, protocols, dialogs and CHAIN callbacks; 43 suites of 20,000 scenarios plus dispatch-family reruns, guarded packet boundaries and mixed lifecycle scheduling |
| `make multip-breaks` | verify MULTIP differential coverage with deliberate mutations in isolated DLLs in `/tmp`; send, receive, worker and DirectPlay slice targets are also available; crashes and fixture errors do not count |
| `make multip-smoke` | temporarily boot FS98 with the rebuilt MULTIP as `MULTIPLAYER.DLL`, require module/gauge loading and the exact replacement, then restore the previous DLL |
| `make multip-acceptance` | run the 20,000-case harness, all 456 mutations, whitespace check and temporary game smoke test serially |
| `make demodiff-run` | whole-module DEMO: retail and OpenFS98 in turn over the CD's six videos and synthetic ones with every record class, recording, playback, instant replay and the four dialogs' callbacks (retail UTIL's catalogue, a stand-in for the host's dialog engine); faults compared as outcomes, runaway ops cut at a fixed count; compares the transcript, the whole `.data` image (code pointers through a learnt pairing), the arena, GLOBAL and the trees |
| `make fesldiff-run` | FE's `+0x78`, the Scenery Library, retail against OpenFS98 over a writable fixture with sandboxed drive letters, driven by a stand-in for World.DLL's nine dialogs; compares the call transcript (with string comparisons and the world files left on disk), FE's `.data`, GLOBAL and the heap; faults are compared as outcomes |
| `make facildiff-fe-run` | the same harness with OpenFS98 FE behind OpenFS98 FACIL, against retail FE behind retail FACIL: both reimplementations together over the real CD index and the dialogs (`Q=`/`D=` set the phase sizes, 2,000 each by default) |
| `make weathermdiff-run` | the same for WEATHERM: updates, frames, resets, scenery effects and list operations, comparing every import call, GLOBAL, the heap and the `.data` image |
| `make chaindiff-run` | drive retail and OpenFS98 CHAIN side by side over random re-entrant callback scripts; `N=...` sets the count |
| `make ofstest-run` | run assertions *inside* the sim, where the host is reachable; `ofstest-remove` takes the module back out |
| `make world` | build World.DLL into `bin/WORLD.DLL` as part of the shared module build |
| `make world-install` / `make world-revert` | install World at `English/World.DLL` (saving the retail file as `English/World.DLL.orig`) or restore the retail DLL; `alerts-`, `help-`, `sim-` and `views-install`/`-revert` do the same for the other English DLLs |
| `make world-game-ui` | drive the installed World module through the retail game's menus and owner callbacks on a disposable copy; see [modules/world.md](modules/world.md#shared-buildinstall-and-game-owner-acceptance) |
| `make sehfix-install` | put `SEHFIX.DLL` in `Modules\`, which hands the divide faults wine swallows back to the host's repair, so the 3D view does not freeze; `sehfix-remove` takes it out, `sehfix-log` shows what it repaired (see [wine.md](wine.md)) |
| `make util-trace` | boot the sim with a UTIL that counts every service call, and list what ran; `UTIL_TRACE_SECS=` for a longer session to play in |
| `tools/hg2d_frames.sh` | the running game on a virtual clock, every frame checksummed: retail HG2D twice, then ours, in private copies, compared frame for frame, on a private Xvfb; `HG2D_FRAMES_ALL=1` compares the whole game, all retail against all ours; on Direct3D by screenshots (see the script) |
| `make graph` | module id and dependency table, read live from `orig/` |
| `tools/impscan.py FILE` | which imported services a module calls, by import and slot |
| `make hostmap` | the host's service table: location, classification, and which libraries each service calls |
| `make compare` | section sizes of each built module vs. the retail one |
| `tools/pesurvey.py FILE...` | sections, linker version, exports, debug dir |
| `tools/dumplinkage.py FILE [n]` | dump a module's `Linkage` / `ImportTable` |

## Baselines

Every verification command with its expected result and run time. These exact numbers are the
current baseline: a run reporting fewer comparisons or any mismatch has regressed.

```sh
make utildiff-run        # UTIL service by service   -> 6452880 comparisons, 0 mismatches
tools/reg_contracts.sh   # every module alone: what each slot leaves in the registers
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
make main-breaks SUITE=navdesc       # its breaks -> 12, all caught (~5 min)
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
make maindiff-run SUITE=objstep      # MAIN slice 16, the smoke trails (+0x88) -> 20000 scenarios, 0 differ (~4 min)
make main-breaks SUITE=objstep       # its breaks -> 34, 32 caught, 2 proven equivalent (~12 min)
make maindiff-run SUITE=quicksave    # MAIN slice 17, the quick save (+0xb0) -> 20000 scenarios, 0 differ (~20 min)
make main-breaks SUITE=quicksave     # its breaks -> 7, 6 caught, 1 proven equivalent (~15 min)
make maindiff-run SUITE=prefs        # MAIN slice 18, the preferences (+0xb4,+0xb8,+0xbc) -> 20000 scenarios, 0 differ (~5 min)
make main-breaks SUITE=prefs         # its breaks -> 28, 26 caught, 2 proven equivalent (~25 min)
make maindiff-run SUITE=clouds       # MAIN slice 19, the cloud layers' draw (+0x98) -> 20000 scenarios, 0 differ (~4 min)
make main-breaks SUITE=clouds        # its breaks -> 53, 51 caught, 2 proven equivalent (~50 min)
make maindiff-run SUITE=cloudtex     # MAIN slice 20, the cloud layers' textures (+0x94) -> 20000 scenarios, 0 differ (~5 min)
make main-breaks SUITE=cloudtex      # its breaks -> 34, 33 caught, 1 proven equivalent (~30 min)
make maindiff-run SUITE=spot         # MAIN slice 21, the spot plane's eye (+0xa8), over retail UTIL -> 20000 scenarios, 0 differ (~4 min)
make main-breaks SUITE=spot          # its breaks -> 50, 47 caught, 3 proven equivalent (~45 min)
make maindiff-run SUITE=frame        # MAIN slice 22, init/deinit and the frame callbacks (+0x04,+0x08), over retail UTIL -> 20000 scenarios, 0 differ (~5 min)
make main-breaks SUITE=frame         # its breaks -> 60, 58 caught, 2 proven equivalent (~90 min)
make maindiff-run SUITE=tick         # MAIN slice 23, the timer tick (+0x24) -> 20000 scenarios, 0 differ (~1 min)
make main-breaks SUITE=tick          # its breaks -> 11, all caught (~10 min)
make maindiff-run SUITE=step         # MAIN slice 24, the frame (+0x18) -> 20000 scenarios, 0 differ (~3 min)
make main-breaks SUITE=step          # its breaks -> 40, 39 caught, 1 proven equivalent (~40 min)
make maindiff-run SUITE=commands     # MAIN slice 25, the menu command dispatcher (+0x20) -> 20000 scenarios, 0 differ (~4 min)
make main-breaks SUITE=commands      # its breaks -> 73, all caught (~50 min)
make maindiff-run SUITE=dialogs      # MAIN slice 26, the first dialogs, over retail UTIL and NUMERIC -> 20000 scenarios, 0 differ (~3 min)
make main-breaks SUITE=dialogs       # its breaks -> 40, 38 caught, 2 proven equivalent (~25 min)
make maindiff-run SUITE=flights      # MAIN slice 27, +0x50, +0xac, +0xc4 and command 0x1106 -> 20000 scenarios, 0 differ (~5 min)
make main-breaks SUITE=flights       # its breaks -> 99, 97 caught, 2 proven equivalent (~80 min)
make maindiff-run SUITE=natives      # MAIN slice 28, the routines its G3D programs call -> 20000 scenarios, 0 differ (~4 min)
make main-breaks SUITE=natives       # its breaks -> 98, 95 caught, 3 proven equivalent (~65 min)
make maindiff-run SUITE=start        # MAIN slice 29, the menu state and the clock -> 20000 scenarios, 0 differ (~3 min)
make main-breaks SUITE=start         # its breaks -> 93, all caught (~65 min)
make maindiff-run SUITE=radios       # MAIN slice 30, CHAIN lists 3 and 5 -> 20000 scenarios, 0 differ (~4 min)
make main-breaks SUITE=radios        # its breaks -> 120, 115 caught, 5 proven equivalent (~75 min)
make maindiff-run SUITE=weather      # MAIN slice 31, the weather's layers at start -> 20000 scenarios, 0 differ (~4 min)
make main-breaks SUITE=weather       # its breaks -> 50, all caught (~30 min)
make maindiff-run SUITE=kneeboard    # MAIN slice 32, the kneeboard -> 20000 scenarios, 0 differ (~3 min)
make main-breaks SUITE=kneeboard     # its breaks -> 61, all caught (~45 min)
make maindiff-run SUITE=session      # MAIN slice 33, +0x28 and +0x2c -> 20000 scenarios, 0 differ (~15 min)
make main-breaks SUITE=session       # its breaks -> 53, all caught (~65 min)
make maindiff-run SUITE=aircraft # MAIN slice 34, Select Aircraft and the kneeboard pages -> 20000 scenarios, 0 differ
make main-breaks SUITE=aircraft # its breaks -> 39, 38 caught, 1 proven equivalent
make maindiff-run SUITE=clockdlg # MAIN slice 35, Time and Season -> 20000 scenarios, 0 differ
make main-breaks SUITE=clockdlg # its breaks -> 85, 84 caught, 1 proven equivalent
make maindiff-run SUITE=engines  # MAIN slice 36, the aircraft's engines -> 20000 scenarios, 0 differ
make main-breaks SUITE=engines  # its breaks -> 78, 66 caught, 12 proven equivalent
make maindiff-run SUITE=limits   # MAIN slice 37, the aircraft's limits -> 20000 scenarios, 0 differ
make main-breaks SUITE=limits   # its breaks -> 63, 55 caught, 8 proven equivalent
make maindiff-run SUITE=areas    # MAIN slice 38, the weather areas -> 20000 scenarios, 0 differ
make main-breaks SUITE=areas    # its breaks -> 67, 62 caught, 5 proven equivalent
make maindiff-run SUITE=prefdlg  # MAIN slice 39, General Preferences -> 20000 scenarios, 0 differ
make main-breaks SUITE=prefdlg  # its breaks -> 69, 68 caught, 1 proven equivalent
make maindiff-run SUITE=viewdlg  # MAIN slice 40, View Options -> 20000 scenarios, 0 differ
make main-breaks SUITE=viewdlg  # its breaks -> 69, 66 caught, 3 proven equivalent
make maindiff-run SUITE=logbook  # MAIN slice 41, the logbook and +0x90 -> 20000 scenarios, 0 differ
make main-breaks SUITE=logbook  # its breaks -> 111, 105 caught, 6 proven equivalent
make maindiff-run SUITE=wxdlg    # MAIN slice 42, the weather dialogs -> 20000 scenarios, 0 differ
make main-breaks SUITE=wxdlg    # its breaks -> 99, 94 caught, 5 proven equivalent
make maindiff-run SUITE=radiodlg # MAIN slice 43, the radios dialog -> 20000 scenarios, 0 differ
make main-breaks SUITE=radiodlg # its breaks -> 118, all caught
make maindiff-run SUITE=create   # MAIN slice 44, Create a Flight -> 20000 scenarios, 0 differ
make main-breaks SUITE=create   # its breaks -> 98, 97 caught, 1 proven equivalent
make main-smoke                      # boots the game on our MAIN.DLL, then restores the previous DLL byte for byte
make flights-play                    # Flights' dialogs used in the game (loads, settings, default, delete, quit), retail twice and ours twice, saves and files compared -> PASS (~10 min)
make -f tools/options.mk optionseventsdiff-run   # Options.DLL, all 15 dialogs and 13 tab pages -> 20000 scenarios, 9860000 comparisons, 0 differ (~1 h)
make -f tools/options.mk options-breaks N=400    # its breaks -> 859, all caught, 17 proven equivalent (~8 h)
make -f tools/options.mk options                 # the native Options.DLL (bin/options-diff/OPTIONS_ours.dll); main-dialogs compares it in the game
make -f tools/simsui.mk simsuidiff-run N=2000    # SIMSUI.DLL, every routine -> 444 operations, 0 differ (~10 min)
make -f tools/simsui.mk simsui-breaks N=400      # its breaks, per class (tools/simsui_breaks.py; run from a frozen copy)
make -f tools/sim1.mk sim1rdiff-run             # SIM1 decompiled routines against retail over in-flight snapshots -> 186 operations, 372000 comparisons, 0 differ (~10 min)
make -f tools/sim1.mk sim1r-breaks               # their breaks -> 882, all caught; proven equivalents listed in its docstring (tools/sim1r_breaks.py; run from a frozen copy)
make -f tools/sim1.mk sim1native-flights        # the native SIM1.sim (make sim1, installed) against retail on the 74 scripted flights -> 74 match (~15 min)
make -f tools/simsui.mk simsui                   # the native SIMSUI.DLL (bin/simsui-native/SIMSUI_ours.dll), installed beside FLTSIM98.EXE
make main-dialogs                    # 47 menu commands and dialog steps in the real game (with Options' dialogs and tab pages), retail against ours -> same controls, 41 of 47 pictures identical, PASS (~30 min)
make hostdiff-run N=20000            # HOST (id 1, FLTSIM98.EXE) slices 1-28, all 194 services, real files in bin/difftest/hostfs -> 0 differ (~11 min)
make host-breaks                     # HOST broken on purpose -> 505 breaks, 488 caught, 15 proven equivalent, 2 rare display paths missed at N=2000 (caught at N=3000) (~5 h)
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
make mdl-crash-diff-run  # the three decompiled crash MDLs against retail -> 12000 scenarios, 0 differ (~3 min)
python3 tools/mdl_crash_breaks.py # (from a frozen copy) the crash MDLs broken on purpose -> 82, 78 caught, 4 proven equivalent (~4 min)
make -f tools/host_rt.mk hostrtdiff-run N=20000 # the decompiled host runtime (src/host/runtime/) -> 161 operations, 3220000 comparisons, 0 differ (~30 min)
python3 tools/host_rt_breaks.py # (from a frozen copy) its breaks -> 569, 557 caught (2 more at N=20000), 10 proven equivalent
make mdl-crash-game  # the three crash flights, retail against the decompiled MDLs -> identical frames
make gauges-check N=20000 # all 120 hand-written gauges against retail, every exposed callback/data/resource/FP result -> 120 checked, 0 differ
python3 tools/gauge_breaks.py # (from a frozen copy) the hand-written gauges broken on purpose -> 67, all caught
make gauges-breaks       # tools/gauge_breaks.py again (it rewrites src/gauges/: from a frozen copy)
make gauges-panels N=1000 # same PANELS, retail/rebuilt gauge collection, strict full-state comparison
make panelsdiff-run      # whole-module PANELS through the CD's gauges -> 3000 scenarios, 0 differ (~15 min)
make panelsunit-run      # PANELS's image routines alone -> 20000 cases each, 0 differ (~3 min)
make panels-breaks       # PANELS broken on purpose -> 200 sampled, 2 do not build, the rest caught (two of them only at N=3000)
make g3dmeshdiff-run     # the mesh compiler: instructions vs portable C, x87 and soft float -> 100000 polygons, 0 differ
make difftest-run        # whole-module SLEW         -> 0/2000 vectors differ
make ofstest-run         # in-process, via the host  -> passed 68, failed 0
make -f tools/hg2d.mk hg2ddiff-run  # whole-module HG2D, a suite per slice -> 59 suite summaries, 0 differ (N=20000 takes hours; docs/modules/hg2d.md)
tools/hg2d_frames.sh 'Situatio\PARIS.STN' 900  # the running game, retail HG2D twice then ours -> identical frames, PASS, on a private Xvfb; HG2D_FRAMES_ALL=1 for the whole game (needs make -f tools/hg2d.mk bin/hg2d-diff/FRAMELOCK.DLL)
make smoke               # ACONTAIN, ADVDRV, ANALYSIS, ATC, BOOSTER, CHAIN, CONTROLS, DEMO, DYNAMIC, FACIL, FE, G3D, GLOBAL, HG2D, LANDME, MPCHAT, NUMERIC, REALITY, SLEW, SOUND, UTIL, WEATHERM, WINDOW, WINMSG installed -> 27/27, 95 gauges, PASS
make util-trace          # which UTIL services the game calls, counted in-game
```
