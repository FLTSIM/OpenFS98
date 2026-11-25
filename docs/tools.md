# Tools and CMake targets

Every harness and analysis script, moved out of the README. The full list of
verification commands with their expected results and run times is in
[Baselines](#baselines) below; this is what each one is. Setup is in
[building.md](building.md).

`tools/ofs TARGET VAR=value ...` runs a CMake target (`cmake/targets.cmake`) with make-style parameters:
`VAR=value cmake --build "$OFS_BUILD" --target TARGET`, `OFS_BUILD` defaulting to `build/win32-retail`. The
targets are the retired Makefile's and `tools/*.mk`'s, same names and defaults (`clean`, `help`, `install` and
`revert` are `ofs_clean`, `ofs_help`, `ofs_install` and `ofs_revert`); `tools/modules.sh` answers the module-list
queries (`modules-of`, `installed-path-of NAME`, ...).

Every gcc invocation takes the one semantic flag set, `OFS_MODULE_FLAGS` in `cmake/toolchains/win32-retail.cmake`:
`@MODULE_FLAGS@` in `cmake/steps/*.cmake` and `cmake/targets.cmake`, `module_flags.FLAGS` (`tools/module_flags.py`)
in Python. A new harness or break build adds it and does not spell those flags.

| | |
| --- | --- |
| `tools/ofs difftest-run` | drive a rebuilt module and retail over identical synthetic state and diff the result; `N=...` sets the vector count |
| `tools/ofs utildiff-run` | compare individual rebuilt UTIL services against retail's, service by service |
| `tools/ofs utildiff-portable` | the same with the x87 assembly compiled out, as an ARM build would use |
| `tools/ofs optionseventsdiff-run` | Options.DLL's dialogs and tab pages: retail's routines and ours over the same logged stubs of MFC, SIMSUI, Win32 and HOST, with randomised fixtures, mid-call mutations and (`MODE=unwind`) a throw at every call; `tools/ofs options-breaks` breaks it on purpose; `tools/ofs options` builds the native DLL |
| `tools/ofs numericdiff-run` | compare all NUMERIC formatter/parser ids and ranged parses against retail |
| `tools/ofs test-gauges` | build-only: the two hand-written test gauges (`737-400.WARNING_ENUNCIATORS.GAU`, `CESSNA_182.SUCTION.GAU`) into `bin/gauges-diff/` from the committed `src/gauges/{warning,suction}_{data,pointers}.inc` and the `.res` from `orig/`, then `--hash` on both; every target that runs them (`gaugesdiff-run`, `suctiondiff-run`, `gauges-panels-run`, `gauges-game-run`, `suction-game-run`, `suction-breaks`) and `data-images` depend on it |
| `tools/ofs data-images` | re-prove the 161 converted data images (`.data`, the `.rdata` ranges of ANALYSIS, REALITY, MULTIP, PANELS, CONTROLS, HG2D, HOST and SIM1, MULTIP's switch table, MAIN's text tail via `tail=`, the ADVDRV, FACIL, FE, HG2D, G3D, MAIN, MULTIP and PANELS pointer lists via `ptrs=` (the two test gauges' `GAUGE_DATA`/`GAUGE_CODE` lists, and the 120 gauges' `GAUGE_REBIND` lists of `src/gauges/images/<NAME>/rebind.inc`, count slots from `.data`'s start although the image starts at the ImportTable export; a `GAUGE_REBIND` target is an RVA in retail's `.text`, `.data` or `.rdata`, and the lines are retail's relocations in retail's order; 95 of the gauge rows also pin `.rdata=`, the other 25 drop it as unused and `gauges-tables-check` covers their `rdata.inc`; `--hash` takes several `MODULE DLL` pairs, which the gauges step runs once for all 120 and the MDL step for the 11 MDLs; the 11 MDL rows are `--linked at=_mdl_data`: ours and retail's relocations in the window are the same offsets, data targets the same place relative to the image, code targets in our `.text`, `--hash` also hashing the relative targets; Sopwith's literal is an extra `rdata=` window), and HOST's `HOST_RT_CODE_POINTER`/`HOST_RT_DATA_POINTER` rows via `rawptrs=`, for an image without relocations, and SIMSUI's 0xc400-byte image bound at load via `bound=`: retail's unrelocated window plus the 2,129 `SIMSUI_RELOC_*` rows of `src/simsui/simsui_native_tables.inc` classified as the generator does) against the reference CD, row by row from `tools/data_images.txt`, then run its self-test; `UPDATE=1` rewrites the manifest's hashes for rows that pass (docs/building.md) |
| `tools/ofs atcdiff-run` | drive retail and OpenFS98 ATC side by side over random scripts of ATIS, clearances and frames, comparing every import call and the whole `.data` image; `N=...` sets the count |
| `tools/ofs acontaindiff-run` | the same for ACONTAIN, against generated aircraft trees: loads, lists, queries and models, with file I/O passed through and fake `.SIM`/`.MDL` modules, comparing every import call, GLOBAL, the heap and the `.data` image |
| `tools/ofs sim1` / `sim1-install` / `sim1-revert` | build SIM1 into `bin/SIM1.sim`, install at `Sim/SIM1.sim`, or restore the verified original; included in shared build/install/revert |
| `tools/ofs sim1-regression-run N=20000` | `src/sim1/sim1.c`'s six services and fifteen state operations (its isolated test builds), including forced relocation |
| `tools/ofs sim1rdiff-run` | the decompiled SIM1 routines (`src/sim1/sim1_small.c`) against retail one at a time, over in-flight snapshots; `sim1r-breaks` breaks them on purpose, the routine contracts are committed in `src/sim1/sim1_tables.inc`, `sim1-snapshots` the snapshots |
| `tools/ofs sim1`, `tools/ofs sim1native-flights` | the native SIM1.sim (`src/sim1/sim1_small.c` over retail's data, `src/sim1/sim1_native.c`, data in `src/sim1/sim1_data.inc` and `sim1_rdata.inc`, resources from `tools/gen_sim1_native.py`) and its game acceptance, the 74 scripted flights against retail; all 74 stock flights, 1,200 passes with controls, comparing per-pass SIM1 numeric state and rendered frames in disposable game copies; `tools/ofs sim1-install` installs it |
| `tools/ofs mdls` / `tools/ofs mdls-install` / `tools/ofs mdls-revert` | build, install or restore all eleven normal/crash MDLs; included in shared build/install/revert targets and used by `tools/ofs run` |
| `tools/ofs mdl-normal-diff-run` | all eight normal MDL update callbacks against retail, at preferred and forced-relocation addresses: state, entire `.data`, GLOBAL, copy calls and preserved registers; `N=...` sets inputs per model (70,000 by default); callback slice only, no MDL installation |
| `tools/ofs mdl-normal-breaks` | six isolated callback mutations, each required to produce its expected behavioral mismatch |
| `tools/ofs mdl-normal-modules` / `mdl-normal-modules-run` | build all eight drop-in normal MDLs and compare their loaded lifecycle, update callbacks, relocated data and resources against retail (70,000 scenarios per model in three address modes by default) |
| `tools/ofs mdl-normal-module-breaks` | six isolated broken DLL builds; require the expected pointer/data/lifecycle/resource mismatch |
| `tools/ofs mdl-normal-game` / `mdl-normal-render-break` | compare scripted view/control frames on eight flights in private otherwise-retail games (`JOBS=1..4`), or prove that turning the rendered RG aircraft changes the frames; primary game remains untouched |
| `tools/ofs mdl-crash-modules` / `mdl-crash-diff-run` | build the three crash MDLs from the decompiled `src/mdl/mdl_crash.c`; compare lifecycle, CHAIN callbacks, state, data, live registers and import events against retail (2,000 scenarios per model in two address modes) |
| `tools/ofs mdl-crash-breaks` | the decompiled crash models broken on purpose (`tools/mdl_crash_breaks.py`, run from a frozen copy) |
| `tools/ofs hostrtdiff-run N=...` | the decompiled host runtime (`src/host/runtime/`) routine by routine against retail FLTSIM98.EXE (`tools/difftest/hostrtdiff.c`, its models in `hostrt_semantics.inc` and `hostrt_windows.inc`; `HOSTRT_COVER=op,text` counts coverage); tables, `.rdata` and the image's pointers from `src/host/runtime/host_rt_tables.inc` (committed; `tools/ofs host-rt-tables-check` re-derives it with `tools/gen_host_rt_tables.py` over `tools/host_rt_survey.py`'s routine list) |
| `tools/ofs host-exe` (`tools/ofs host-rt-exe`) | `bin/OPENFS98.EXE`, the executable from the decompiled runtime and the services (`src/host/host_rt_exe.c`); fly it with `tools/frames_many.sh` and `HG2D_FRAMES_HOST`, drive its dialogs with `tools/host_runtime_ui.sh` and `HOST_UI_EXE`; its resources from `tools/gen_host_resources.py` (the `.rsrc` tree only) |
| `python3 tools/host_rt_breaks.py [N]` | the decompiled host runtime broken on purpose (run from a frozen copy) |
| `tools/ofs mdl-crash-game` / `mdl-crash-render-break` | three private 1,200-pass crash flights (`JOBS=1..3`) requiring the intended crash MDL, or a valid pose mutation required to change rendered frames; source flights and primary game untouched |
| `tools/ofs sounddiff-run` | the same for SOUND, against generated `SOUND.CFG` trees and the CD's seven: frames, events, wind, faders and CHAIN messages, with a model of the host's voice engine, comparing every import call, GLOBAL, `FLTSIM98.CFG`, the heap and the `.data` image |
| `tools/ofs dynamicdiff-run` | the same for DYNAMIC, against the dynamic-scenery programs of the CD's twelve airport BGLs, often mutated, and three synthetic blocks: frames, the three CHAIN lists, the user's aircraft and MULTIP's players steered onto what the programs test; faults and hangs compared as outcomes |
| `tools/ofs fegriddiff-run` | FE's grid conversions (`+0x60`..`+0x6c`), retail and OpenFS98 loaded side by side and called directly over every table edge at every level and random values; a fault in either side counts as a difference |
| `tools/ofs fediff-run` | FE's assembly engine and compiled-C file services, retail and OpenFS98 loaded side by side over the extracted CD scenery/texture tree plus self-checking synthetic compressed-texture and BGL-section fixtures, comparing every import call and output, GLOBAL, the heap and the whole `.data` image |
| `tools/ofs landmediff-run` | LANDME's seven-state landing assistant against retail, each side backed by its own retail FE over the real scenery tree; compares every import call, GLOBAL, the heap and both modules' whole `.data` images |
| `tools/ofs facildiff-run` | whole-module FACIL against retail, each side backed by a separate retail FE: a synthetic five-level rebuild, 20,000 seeded synthetic caches through the `+0x18`/`+0x1c`/`+0x20` queries, then the real CD index (with all sixteen boosters) and 10,000 dialog scenarios driven by a stand-in for World.DLL; compares the caches, call transcript (with string comparisons), returned records, GLOBAL, heap and both modules' state |
| `tools/ofs boosterdiff-run` | whole-module BOOSTER, worker thread included: retail and OpenFS98 run in turn over identical cache trees (four CD boosters, synthetic ones with every record type), the thread drained after every operation by counting posted 0x401s against SetEvents; MessageBoxA and the file calls hooked and logged; compares the transcript, the whole `.data` image, both heaps, GLOBAL, FE's records and the cache trees |
| `tools/ofs windowdiff-run` | whole-module WINDOW: retail and OpenFS98 in turn over scripts of windows created, closed, maximised, painted frame after frame through every eye mode and transition, and laid out in situations; a model of the host's window services and z-order, and of its filter that repairs integer divide faults; compares the transcript (with each frame's eye), the whole `.data` image, the arena and GLOBAL. `tools/ofs window-breaks` breaks the module on purpose, one place at a time, and checks the harness notices |
| `tools/ofs mpchatdiff-run` | whole-module MPCHAT, the chat window: retail and OpenFS98 in turn over scripts of sessions begun and ended, player lists, chat lines sent and received, the menu command and keys, window messages and the user's typing and selection, against a model of USER32 (windows, ListBox, Edit, RichEdit, scroll state) hooked into both import tables; compares every call, the whole `.data` image, the heap and the model. `tools/ofs mpchat-breaks` breaks it on purpose |
| `tools/ofs winmsgdiff-run` | whole-module WINMSG, the message bar: retail and OpenFS98 in turn over scripts of messages shown, appended, cleared and run frame after frame (plain and scrolling, split to fit), with a model of the host's window record and heap, a CHAIN that answers the done message and re-enters, and situation records; compares the transcript with every string drawn, the whole `.data` image, the arena and GLOBAL. `tools/ofs winmsg-breaks` breaks it on purpose |
| `tools/ofs realitydiff-run` | whole-module REALITY, crashes and damage: retail and OpenFS98 in turn through every crash gate, the countdown, the reload and the crash analysis (a stand-in for its dialog, retail UTIL and NUMERIC), the stress rolls and the cracked windscreen; HG2D's lines logged with their colour and view; compares the transcript, the whole `.data` image, the arena, GLOBAL and UTIL's and NUMERIC's state. `tools/ofs reality-breaks` breaks it on purpose |
| `tools/ofs analysisdiff-run` | whole-module ANALYSIS, landing and manoeuvre analysis and course tracking: retail and OpenFS98 in turn over the frames, the landing thresholds, the compaction, the trail and all three dialogs (a stand-in for the dialog engine, retail UTIL and NUMERIC), with the register leftovers retail reads (CHAIN's, memset's, the CD MSVCRT memmove's) modelled; HG2D's lines and G3D's display list logged; compares the transcript, the whole `.data` image, the arena, GLOBAL and UTIL's and NUMERIC's state. `tools/ofs analysis-breaks` breaks it on purpose |
| `tools/ofs controlsdiff-run` | whole-module CONTROLS: retail and OpenFS98 side by side over commands (through the dispatcher and CHAIN list 0xb), the axis, engine and pilot-event entries, the CHAIN frames, the joystick poll and watcher over scripted sticks, the key bindings, force feedback and the key-assignment dialog; the host's profile services passed through to fixture CFG files, models of its accelerator tables and joysticks, a DirectInput 5 stand-in and a stand-in for Options.DLL's dialog; compares GLOBAL, the whole `.data` image, the import-call transcript and the profile files. `tools/ofs controls-breaks` breaks it on purpose |
| `tools/ofs panelsdiff-run` | whole-module PANELS: retail and OpenFS98 in turn through the gauges installed in `game/` (OpenFS98's rebuilt ones since 2026-11-14) and the CD's panel files over scripts of aircraft loaded, frames, resets, situations, commands, windows and paints, against stubs of the host, GLOBAL, CHAIN, MAIN, WINDOW and the HG2D model, with GDI virtualised; compares the transcript, the `.data` image, every block and each gauge's data. `tools/ofs panelsunit-run` calls its 28 image routines alone; `tools/ofs panels-breaks` breaks it on purpose |
| `tools/ofs gauges` / `tools/ofs gauges-install` / `tools/ofs gauges-revert` | build, install or restore all 120 gauges, also included in the shared targets |
| `tools/ofs mdl-images-check` | the committed MDL images against retail (`tools/gen_mdl_images.py --check`, needs `orig/`, `objdump` and `cc`; writes nothing): each model's `layout.h`, the host-compiled bytes of `data.inc` and `src/mdl/mdl_manifest.json`; exit 1 naming the file and the first differing line; `--self-test` checks that a moved, extra, missing or duplicated slot in `.data` is rejected |
| `tools/ofs gauges-tables-check` | the committed gauge tables against retail's full analysis (`tools/gen_gauges.py --check`, needs `orig/`, `objdump` and `cc`; writes nothing): each gauge's `layout.h`, `rebind.inc`, line of `src/gauges/gauges_manifest.json`, and the host-compiled bytes of `data.inc` and `rdata.inc` (the 25 gauges whose `.rdata` the compiler drops included); exit 1 naming the gauge |
| `tools/ofs gauges-check N=20000` | all 1,317 exposed entries: complete data/resources, PANELS transcript, raw x87 results/flags; add `GAUGES_RELOCATE=1` to force relocation |
| `tools/ofs gauges-breaks` | 348 independent collection mutations; requires explicit mismatches |
| `tools/ofs gauges-panels N=1000` | same rebuilt PANELS on both sides with retail/rebuilt gauge collection; strict state/heap/surface/transcript comparisons |
| `tools/ofs all-gauges-hangs` | 16 watchdog comparisons for the two compass callbacks' retail nontermination |
| `tools/ofs gaugesdiff-run` | first gauge, `737WARNG`: retail/rebuilt callbacks, complete data image and bitmap; 20,000 scenarios both at its preferred base and forcibly relocated. `gauges-breaks` requires explicit failures from 15 isolated mutations |
| `tools/ofs suctiondiff-run` | Cessna suction: full callback/data/two-bitmap comparison; 20,000 scenarios plus 240,288 numeric vectors across x87 precision/rounding modes, repeated under forced relocation. `suction-breaks` checks 26 isolated mutations |
| `tools/ofs gauges-panels-run N=1000` | same PANELS on both sides, retail/rebuilt warning and suction gauges, other gauges retail; whole-panel scripts and strict fault-outcome comparison in a private Wine prefix; N defaults to 1000 (the harness stops at 1800 s). Differs: 143 of 1000, as at 27371fb before Story 1.x (`docs/open-differences.md`) |
| `tools/ofs gauges-game-run` | 737 Hong Kong flight, retail twice then rebuilt warning gauge, 900 FRAMELOCK passes; requires actual selected-gauge loads and identical frames in private game copies |
| `tools/ofs suction-game-run` | Cessna St George flight, retail twice then rebuilt suction gauge, 900 FRAMELOCK passes; requires actual gauge loads and identical frames in private game copies |
| `tools/ofs g3ddiff-run` | whole-module G3D, the 3D pipeline: retail and OpenFS98 in turn over generated scenery programs (every opcode, with operands steered onto the boundaries they are compared with), the depth queue, the Linkage services and the CHAIN callbacks run frame after frame over FE's scene stream (stubs of FE, HG2D, CHAIN, WINDOW and the host; retail UTIL and WINDOW), the host's divide repair modelled; compares every import call, every register, the whole `.data` image, GLOBAL and the arena after every operation. 20,000 scenarios, 0 differ (~5 min). `tools/ofs g3d-breaks` breaks it on purpose |
| `tools/ofs g3dmeshdiff-run` | the mesh compiler (`0x38`) three ways -- retail's own G3D.DLL, the C on the x87, the C on its software 80-bit float -- over generated polygons under 24-, 53- and 64-bit precision: 100,000 polygons, 0 differ. `tools/ofs g3ddiff-meshc-run` runs `g3ddiff` with the portable compiler in the module: 20000 scenarios, 43 differ, as at 27371fb before Story 1.x (`docs/open-differences.md`) |
| `tools/ofs multipdiff-run` | whole MULTIP harness: all eleven public entries, transport/workers, profiles, protocols, dialogs and CHAIN callbacks; 43 suites of 20,000 scenarios plus dispatch-family reruns, guarded packet boundaries and mixed lifecycle scheduling |
| `tools/ofs multip-breaks` | verify MULTIP differential coverage with deliberate mutations in isolated DLLs in `/tmp`; send, receive, worker and DirectPlay slice targets are also available; crashes and fixture errors do not count |
| `tools/ofs multip-smoke` | temporarily boot FS98 with the rebuilt MULTIP as `MULTIPLAYER.DLL`, require module/gauge loading and the exact replacement, then restore the previous DLL |
| `tools/ofs multip-acceptance` | run the 20,000-case harness, all 456 mutations, whitespace check and temporary game smoke test serially |
| `tools/ofs demodiff-run` | whole-module DEMO: retail and OpenFS98 in turn over the CD's six videos and synthetic ones with every record class, recording, playback, instant replay and the four dialogs' callbacks (retail UTIL's catalogue, a stand-in for the host's dialog engine); faults compared as outcomes, runaway ops cut at a fixed count; compares the transcript, the whole `.data` image (code pointers through a learnt pairing), the arena, GLOBAL and the trees |
| `tools/ofs fesldiff-run` | FE's `+0x78`, the Scenery Library, retail against OpenFS98 over a writable fixture with sandboxed drive letters, driven by a stand-in for World.DLL's nine dialogs; compares the call transcript (with string comparisons and the world files left on disk), FE's `.data`, GLOBAL and the heap; faults are compared as outcomes |
| `tools/ofs facildiff-fe-run` | the same harness with OpenFS98 FE behind OpenFS98 FACIL, against retail FE behind retail FACIL: both reimplementations together over the real CD index and the dialogs (`Q=`/`D=` set the phase sizes, 2,000 each by default) |
| `tools/ofs weathermdiff-run` | the same for WEATHERM: updates, frames, resets, scenery effects and list operations, comparing every import call, GLOBAL, the heap and the `.data` image |
| `tools/ofs chaindiff-run` | drive retail and OpenFS98 CHAIN side by side over random re-entrant callback scripts; `N=...` sets the count |
| `tools/ofs ofstest-run` | run assertions *inside* the sim, where the host is reachable; `ofstest-remove` takes the module back out |
| `tools/ofs world` | build World.DLL into `bin/WORLD.DLL` as part of the shared module build |
| `tools/ofs world-install` / `tools/ofs world-revert` | install World at `English/World.DLL` (saving the retail file as `English/World.DLL.orig`) or restore the retail DLL; `alerts-`, `help-`, `sim-` and `views-install`/`-revert` do the same for the other English DLLs |
| `tools/ofs world-game-ui` | drive the installed World module through the retail game's menus and owner callbacks on a disposable copy; see [modules/world.md](modules/world.md#shared-buildinstall-and-game-owner-acceptance) |
| `tools/ofs sehfix-install` | put `SEHFIX.DLL` in `Modules\`, which hands the divide faults wine swallows back to the host's repair, so the 3D view does not freeze; `sehfix-remove` takes it out, `sehfix-log` shows what it repaired (see [wine.md](wine.md)) |
| `tools/ofs util-trace` | boot the sim with a UTIL that counts every service call, and list what ran; `UTIL_TRACE_SECS=` for a longer session to play in |
| `tools/hg2d_frames.sh` | the running game on a virtual clock, every frame checksummed: retail HG2D twice, then ours, in private copies, compared frame for frame, on a private Xvfb; `HG2D_FRAMES_ALL=1` compares the whole game, all retail against all ours; on Direct3D by screenshots (see the script) |
| `tools/ofs references` (`tools/record_references.sh`) | record retail's reference logs once: 131 cases (74 flights, 56 scripted, 1 GLOBAL dump), each flown twice on a game staged fresh from the CD, no sound, software renderer, 900 passes, into `bin/references/` with a committed manifest `tools/references.txt`; `CASES="flight/PARIS ..."` and `REFERENCES_OUT=` for a subset; see [Retail reference logs](#retail-reference-logs) |
| `tools/ofs graph` | module id and dependency table, read live from `orig/` |
| `tools/impscan.py FILE` | which imported services a module calls, by import and slot |
| `tools/ofs hostmap` | the host's service table: location, classification, and which libraries each service calls |
| `tools/ofs compare` | section sizes of each built module vs. the retail one |
| `tools/pesurvey.py FILE...` | sections, linker version, exports, debug dir |
| `tools/dumplinkage.py FILE [n]` | dump a module's `Linkage` / `ImportTable` |
| `tools/pressing_catalogue.py catalogue SOURCE OUT.json` / `compare REF.json OTHER.json` | catalogue a CD pressing (ISO, MODE1 BIN/CUE or folder; images through `7z`): each file's size and SHA-256, and each PE's link date, image base and per-resource SHA-256; `compare` prints a Markdown summary (exit 1 on any difference) of changed files, split into rebuilt code with identical resources and changed resources. The reference's catalogue is `bin/pressings/reference.json`; the ISO, the BIN/CUE made from it and `cd/` catalogue identically (Story 6.1) |
| `tools/guest/x86_record.c` | ground truth for the guest interpreter (Story 7.1): a freestanding `-m32` program (build line in the file, no multilib, no wine) that records integer, flag, `setcc` and x87 results on this CPU into `bin/guest/x86_results.txt` (182,359 lines, the same bytes every run); only SDM-defined flags and x87 condition bits are kept |
| `tools/guest/check_x86_results.py FILE` | checks the recorder's output against an independent model of the integer ops and the exact x87 loads: 102,825 lines agree, 0 disagree; a wrong `adc` or an unmasked shift flag is caught |

## Baselines

Every verification command with its expected result and run time. These exact numbers are the
current baseline: a run reporting fewer comparisons or any mismatch has regressed.

A break that crashes the harness (a harness guard, an unstubbed call) is a miss, not a catch; the break suites'
counts were re-run on 2026-11-17 under that rule, and the misses are named. The same misses come out through
the old Makefile (MAIN's `menu` and `aircraft` suites checked at the commits their counts were recorded at), so
the older, higher counts were crashes counted as catches. "Missed" means the harness reported 0 differ: a gap in
what it compares.

`python3 -B tools/cmake_verify.py build/win32-retail RESULTS [-j N] [--in-place] [--baselines FILE] [--clean-slots] [--only S] [--lines N,N] [--factor F] [--list]`
runs this block (168 lines) and records each as `RESULTS/NNN.json`/`.log`, with `report.md`. By default every line runs in a
**slot**: a disposable copy of the tree it was started in (`RESULTS/slots/K`, outside the start tree), with its own `bin`,
a copy of `game`, a self-contained copy of `~/.wine` (`WINEPREFIX`, `FS98_DIR` and `OFS_BUILD` point into it) and a
template (`K.template`) it is rsync-reset from after every line, so a break runner's mutant source, a rebuilt module, an
installed module or a game write never reaches a later line. `-j N` (default 1) runs up to N lines at once, one per slot;
`--in-place` keeps the old behaviour (the start tree, serial). **Solo lines** (those needing an X display, writing the
game directory or running `FLTSIM98.EXE`) run with no other line running; lines of one **conflict group** (`host-breaks`
with `hostdiff-run`, `g3d-breaks` with `g3ddiff-run`/`g3ddiff-meshc-run`, whose runners `pkill -f` their harness
machine-wide) never overlap. `main-smoke`, `smoke`, `ofstest-run` and `util-trace` are preceded by `ofs_install` into the
slot's game. After a line the processes the slot owns are stopped; a changed file under the slot's `cmake`, `docs`,
`include`, `src` or `tools` fails the line (`tree_changed`). A timeout sends SIGTERM, then SIGKILL 60 s later.
`--clean-slots` deletes `RESULTS/slots` at the end. `--bin-skip-mb N` (default 256; 0 copies everything) is the size above which
a `bin/` subdirectory of the start tree (run output) is not copied into a slot: a live `bin` is about 106 GB, and the
required fixtures inside a skipped directory are still copied. `RESULTS/meta.json` ties the records to the Baselines list; another
list is refused unless `--lines` names what to re-run. The tables and the disk use are in the script's docstring.
**Parallel break runners.** `host-breaks`, `g3d-breaks`, `options-breaks` and `sim1r-breaks` run their breaks on a worker pool
when `BREAK_JOBS=N` is set (unset or 1: serial in the tree, exactly as before; `main-breaks` stays out, its suites are separate
Baselines lines). `tools/break_pool.py` makes up to N workers (never more than there are breaks) under
`bin/break-pool/<runner>-<pid>/<k>` in the runner's tree: a copy of its top level (not `bin`, `build`, `game`, `cd`, `.git`), the
`bin/` paths the runner declares (`POOL_BIN`/`POOL_LINK`), its own `cmake` configure and a private copy of the runner's wine
prefix (`WINEPREFIX`, default `~/.wine`; Options' own `bin/options-diff/wineprefix`), so a hang is killed with `wineserver -k` in
that prefix, never `pkill`. The verdict lines, their order and the summary equal the serial run's. `BREAK_POOL_KEEP=1` keeps the
workers' files (their processes are still stopped); `BREAK_POOL_RECORD=FILE` writes one JSON object per break (index, name, unit,
build exit, harness exit, the harness's summary line, the first `DIFFER` line) on the serial path too, which is how a parallel run
is compared with a serial one. There is no setting for the pool's directory. Budget: about 3-4 GB of disk per worker (the prefix,
the copied `bin` paths; sim1r also builds `openfs98 difftests` once per worker), and wine processes are about `-j` slots times
`BREAK_JOBS`: at most 6 in all on a 12-core host. The break's own files (Options' `<name>.c|.exe|.log`) reach the runner's tree in
serial order; files a break writes under a fixed name (g3d's `g3d_retail.txt` and `g3d_ours.txt`) stay in the worker and are not
copied back. A SIGINT or SIGTERM stops the queue and SIGKILLs the workers' processes within 30 s; any survivor gives exit 2.
Self-tests: `python3 tools/slots.py --self-test`, `python3 tools/break_pool.py --self-test`.
`--factor` scales only the verifier's own limit; two harnesses also stop themselves, and in parallel slots they run slower,
so raise their limits for a `-j` run: `CONTROLS_TIMEOUT=SECONDS` (`controlsdiff-run`, default 3600) and `HG2D_TIMEOUT=SECONDS`
(`hg2ddiff-run`, default 7200) in the environment.
**Load limits** (12-core host). The 1.4 run (`-j 3`, `BREAK_JOBS=2`, load average about 11) failed these lines by load alone:
nothing differed before the limit. The first four are SOLO in `cmake_verify.py`, since their verdict depends on timing; the
others are only slow, and their line timeout comes from the documented time, so keep that time measured alone.

| Line | Under load ~11 | Alone (load ~1) |
|---|---|---|
| `acontaindiff-run` | its own `timeout 1800` | PASS, 1587 s |
| `hostdiff-run N=20000` | its own `timeout 600` at 606 s | 0 differ, 602 s with niced builds beside it (549 s in 1.3's run): close to its own 600 s limit, keep the machine quiet |
| `host-breaks` | `fs-same`, proven equivalent, "caught" by a 120 s hang | PASS alone, 8431 s (`BREAK_JOBS=2`, `-j 1`) |
| `panels-breaks` | `e_be10:5111:lt29`, a known miss, "caught" by a hang | not yet rerun |
| `main-breaks SUITE=situations` | 2 h line limit (documented ~40 min) | PASS, 9973 s; its documented time needs ~2.8 h |
| `controlsdiff-run` | 2 h line limit at 19,000 of 20,000 | PASS alone, 5869 s (~98 min; give it 2.5 h) |
| `controls-breaks` | 1 h line limit | PASS alone, 2874 s (~48 min, just under 1 h: give it 1.5 h) |
| `hg2ddiff-run` | 2 h line limit at 38 of 59 suites (also in 1.3's run) | all 59 suites, 0 differ, 19854 s alone (~5.5 h): its limit needs `HG2D_TIMEOUT` over 20000 and a line timeout of 8 h |

```sh
tools/ofs utildiff-run        # UTIL service by service   -> 6452880 comparisons, 0 mismatches
tools/reg_contracts.sh   # every module alone: what each slot leaves in the registers
tools/ofs utildiff-portable   # the same with x87 compiled out (the ARM path)
tools/ofs numericdiff-run     # all NUMERIC formats/ranges -> formatter 505440 comparisons and parser 40338, 0 mismatches
tools/ofs chaindiff-run       # whole-module CHAIN        -> 20000 scenarios, 0 differ
tools/ofs atcdiff-run         # whole-module ATC          -> 20000 scenarios, 0 differ
tools/ofs weathermdiff-run    # whole-module WEATHERM     -> 20000 scenarios, 0 differ
tools/ofs acontaindiff-run    # whole-module ACONTAIN     -> 20000 scenarios, 0 differ (~25 min)
tools/ofs sounddiff-run       # whole-module SOUND        -> 20000 scenarios, 0 differ (~40 min)
tools/ofs dynamicdiff-run     # whole-module DYNAMIC      -> 20000 scenarios, 0 differ (~15 min)
tools/ofs fegriddiff-run      # FE grid conversions        -> 7742499 comparisons, 0 mismatches
tools/ofs fediff-run          # FE engine + C file services -> 20000 scenarios, 0 differ
tools/ofs landmediff-run      # whole-module LANDME       -> 20000 scenarios, 0 differ
tools/ofs facildiff-run       # whole-module FACIL        -> 0 differences; 20000 query, 10000 dialog scenarios, 0 differ (~27 min)
tools/ofs facildiff-fe-run    # OpenFS98 FE+FACIL together -> 2000 query, 2000 dialog scenarios, 0 differ (~5 min)
tools/ofs fesldiff-run N=20000 # FE +0x78, the Scenery Library -> 20000 scenarios, 0 differ (~6 min)
tools/ofs boosterdiff-run     # whole-module BOOSTER, its thread drained -> 20000 scenarios, 0 differ (~75 min)
tools/ofs demodiff-run        # whole-module DEMO, videos and instant replay -> 20000 scenarios, 0 differ (~25 min)
tools/ofs advdrvdiff-run      # whole-module ADVDRV, adventures and lessons -> 20000 scenarios, 0 differ (~40 min)
tools/ofs windowdiff-run      # whole-module WINDOW, windows and the eye -> 20000 scenarios, 0 differ (~2 min)
tools/ofs window-breaks       # WINDOW broken on purpose -> 109 breaks, 106 caught, 3 proven equivalent (~25 min)
tools/ofs maindiff-run SUITE=records  # MAIN slice 1, situation records -> 20000 scenarios, 0 differ (~15 min)
tools/ofs main-breaks SUITE=records   # its breaks -> 40, 38 caught, 2 proven equivalent (~35 min)
tools/ofs maindiff-run SUITE=situations  # MAIN slice 2, the reader and writer -> 20000 scenarios, 0 differ (~20 min)
tools/ofs main-breaks SUITE=situations   # its breaks -> 69, 63 caught, 6 proven equivalent (~40 min)
tools/ofs maindiff-run SUITE=nav          # MAIN slice 3, range and bearing (+0x70) -> 20000 scenarios, 0 differ (~3 min)
tools/ofs main-breaks SUITE=nav           # its breaks -> 14, 13 caught, 1 proven equivalent (~6 min)
tools/ofs maindiff-run SUITE=navio        # MAIN slice 4, the nav read-outs (+0x78,+0x80) -> 20000 scenarios, 0 differ (~3 min)
tools/ofs main-breaks SUITE=navio         # its breaks -> 12, all caught (~5 min)
tools/ofs maindiff-run SUITE=navdesc      # MAIN slice 5, the station descriptor (+0x7c) -> 20000 scenarios, 0 differ (~3 min)
tools/ofs main-breaks SUITE=navdesc       # its breaks -> 12, all caught (~5 min)
tools/ofs maindiff-run SUITE=season       # MAIN slice 6, the season/zone/clock (+0x74) -> 20000 scenarios, 0 differ (~3 min)
tools/ofs main-breaks SUITE=season        # its breaks -> 24, 23 caught, 1 proven equivalent (~10 min)
tools/ofs maindiff-run SUITE=clock        # MAIN slice 7, the clock as digits (+0xa4) -> 20000 scenarios, 0 differ (~3 min)
tools/ofs main-breaks SUITE=clock         # its breaks -> 6, all caught (~3 min)
tools/ofs maindiff-run SUITE=view         # MAIN slice 8, the view cycler (+0xc8) -> 20000 scenarios, 0 differ (~3 min)
tools/ofs main-breaks SUITE=view          # its breaks -> 10, all caught (~5 min)
tools/ofs maindiff-run SUITE=menu         # MAIN slice 9, analysis menu greying/enabling (+0x30,+0x34) -> 20000 scenarios, 0 differ (~3 min)
tools/ofs main-breaks SUITE=menu          # its breaks -> 6, 5 caught; slot crashes (~2 min)
tools/ofs maindiff-run SUITE=obj          # MAIN slice 10, an object slot release (+0x8c) -> 20000 scenarios, 0 differ (~3 min)
tools/ofs main-breaks SUITE=obj           # its breaks -> 6, all caught (~3 min)
tools/ofs maindiff-run SUITE=clamp        # MAIN slice 11, the latitude clamp (+0x1c) -> 20000 scenarios, 0 differ (~3 min)
tools/ofs main-breaks SUITE=clamp         # its breaks -> 6, 5 caught, 1 proven equivalent (~3 min)
tools/ofs maindiff-run SUITE=sun          # MAIN slice 12, the sun direction (+0xa0) -> 20000 scenarios, 0 differ (~5 min)
tools/ofs main-breaks SUITE=sun           # its breaks -> 14, 12 caught, 2 proven equivalent (~6 min)
tools/ofs maindiff-run SUITE=objalloc     # MAIN slice 13, allocate an object slot (+0x84) -> 20000 scenarios, 0 differ (~5 min)
tools/ofs main-breaks SUITE=objalloc      # its breaks -> 15, 14 caught, 1 proven equivalent (~6 min)
tools/ofs maindiff-run SUITE=eye          # MAIN slice 14, the eye's reticle (+0xc0) -> 20000 scenarios, 0 differ (~4 min)
tools/ofs main-breaks SUITE=eye           # its breaks -> 13, 12 caught, 1 proven equivalent (~5 min)
tools/ofs maindiff-run SUITE=band         # MAIN slice 15, the altitude band (+0x9c) -> 20000 scenarios, 0 differ (~4 min)
tools/ofs main-breaks SUITE=band          # its breaks -> 33, all caught (~12 min)
tools/ofs maindiff-run SUITE=objstep      # MAIN slice 16, the smoke trails (+0x88) -> 20000 scenarios, 0 differ (~4 min)
tools/ofs main-breaks SUITE=objstep       # its breaks -> 34, 32 caught, 2 proven equivalent (~12 min)
tools/ofs maindiff-run SUITE=quicksave    # MAIN slice 17, the quick save (+0xb0) -> 20000 scenarios, 0 differ (~20 min)
tools/ofs main-breaks SUITE=quicksave     # its breaks -> 7, 6 caught, 1 proven equivalent (~15 min)
tools/ofs maindiff-run SUITE=prefs        # MAIN slice 18, the preferences (+0xb4,+0xb8,+0xbc) -> 20000 scenarios, 0 differ (~5 min)
tools/ofs main-breaks SUITE=prefs         # its breaks -> 28, 26 caught, 2 proven equivalent (~25 min)
tools/ofs maindiff-run SUITE=clouds       # MAIN slice 19, the cloud layers' draw (+0x98) -> 20000 scenarios, 0 differ (~4 min)
tools/ofs main-breaks SUITE=clouds        # its breaks -> 53, 51 caught, 2 proven equivalent (~50 min)
tools/ofs maindiff-run SUITE=cloudtex     # MAIN slice 20, the cloud layers' textures (+0x94) -> 20000 scenarios, 0 differ (~5 min)
tools/ofs main-breaks SUITE=cloudtex      # its breaks -> 34, 33 caught, 1 proven equivalent (~30 min)
tools/ofs maindiff-run SUITE=spot         # MAIN slice 21, the spot plane's eye (+0xa8), over retail UTIL -> 20000 scenarios, 0 differ (~4 min)
tools/ofs main-breaks SUITE=spot          # its breaks -> 50, 47 caught, 3 proven equivalent (~45 min)
tools/ofs maindiff-run SUITE=frame        # MAIN slice 22, init/deinit and the frame callbacks (+0x04,+0x08), over retail UTIL -> 20000 scenarios, 0 differ (~5 min)
tools/ofs main-breaks SUITE=frame         # its breaks -> 60, 58 caught, 2 proven equivalent (~90 min)
tools/ofs maindiff-run SUITE=tick         # MAIN slice 23, the timer tick (+0x24) -> 20000 scenarios, 0 differ (~1 min)
tools/ofs main-breaks SUITE=tick          # its breaks -> 11, all caught (~10 min)
tools/ofs maindiff-run SUITE=step         # MAIN slice 24, the frame (+0x18) -> 20000 scenarios, 0 differ (~3 min)
tools/ofs main-breaks SUITE=step          # its breaks -> 40, 39 caught, 1 proven equivalent (~40 min)
tools/ofs maindiff-run SUITE=commands     # MAIN slice 25, the menu command dispatcher (+0x20) -> 20000 scenarios, 0 differ (~4 min)
tools/ofs main-breaks SUITE=commands      # its breaks -> 73, 64 caught; 9 crash: case-1105 -1181 -1303 -1304 -1305 -130a -1405 -140a -2000d (~50 min)
tools/ofs maindiff-run SUITE=dialogs      # MAIN slice 26, the first dialogs, over retail UTIL and NUMERIC -> 20000 scenarios, 0 differ (~3 min)
tools/ofs main-breaks SUITE=dialogs       # its breaks -> 40, 37 caught, 2 proven equivalent; 1607-child crashes (~25 min)
tools/ofs maindiff-run SUITE=flights      # MAIN slice 27, +0x50, +0xac, +0xc4 and command 0x1106 -> 20000 scenarios, 0 differ (~5 min)
tools/ofs main-breaks SUITE=flights       # its breaks -> 99, 95 caught, 2 proven equivalent; open-id, open-answer crash (~80 min)
tools/ofs maindiff-run SUITE=natives      # MAIN slice 28, the routines its G3D programs call -> 20000 scenarios, 0 differ (~4 min)
tools/ofs main-breaks SUITE=natives       # its breaks -> 98, 94 caught, 4 proven equivalent (~65 min)
tools/ofs maindiff-run SUITE=start        # MAIN slice 29, the menu state and the clock -> 20000 scenarios, 0 differ (~3 min)
tools/ofs main-breaks SUITE=start         # its breaks -> 93, all caught (~65 min)
tools/ofs maindiff-run SUITE=radios       # MAIN slice 30, CHAIN lists 3 and 5 -> 20000 scenarios, 0 differ (~4 min)
tools/ofs main-breaks SUITE=radios        # its breaks -> 120, 114 caught, 6 proven equivalent (~75 min)
tools/ofs maindiff-run SUITE=weather      # MAIN slice 31, the weather's layers at start -> 20000 scenarios, 0 differ (~4 min)
tools/ofs main-breaks SUITE=weather       # its breaks -> 50, all caught (~30 min)
tools/ofs maindiff-run SUITE=kneeboard    # MAIN slice 32, the kneeboard -> 20000 scenarios, 0 differ (~3 min)
tools/ofs main-breaks SUITE=kneeboard     # its breaks -> 61, all caught (~45 min)
tools/ofs maindiff-run SUITE=session      # MAIN slice 33, +0x28 and +0x2c -> 20000 scenarios, 0 differ (~15 min)
tools/ofs main-breaks SUITE=session       # its breaks -> 53, all caught (~65 min)
tools/ofs maindiff-run SUITE=aircraft # MAIN slice 34, Select Aircraft and the kneeboard pages -> 20000 scenarios, 0 differ
tools/ofs main-breaks SUITE=aircraft # its breaks -> 39, 37 caught, 2 proven equivalent
tools/ofs maindiff-run SUITE=clockdlg # MAIN slice 35, Time and Season -> 20000 scenarios, 0 differ
tools/ofs main-breaks SUITE=clockdlg # its breaks -> 85, 84 caught, 1 proven equivalent
tools/ofs maindiff-run SUITE=engines  # MAIN slice 36, the aircraft's engines -> 20000 scenarios, 0 differ
tools/ofs main-breaks SUITE=engines  # its breaks -> 78, 66 caught, 12 proven equivalent
tools/ofs maindiff-run SUITE=limits   # MAIN slice 37, the aircraft's limits -> 20000 scenarios, 0 differ
tools/ofs main-breaks SUITE=limits   # its breaks -> 63, 55 caught, 8 proven equivalent
tools/ofs maindiff-run SUITE=areas    # MAIN slice 38, the weather areas -> 20000 scenarios, 0 differ
tools/ofs main-breaks SUITE=areas    # its breaks -> 67, 62 caught, 5 proven equivalent
tools/ofs maindiff-run SUITE=prefdlg  # MAIN slice 39, General Preferences -> 20000 scenarios, 0 differ
tools/ofs main-breaks SUITE=prefdlg  # its breaks -> 69, 68 caught, 1 proven equivalent
tools/ofs maindiff-run SUITE=viewdlg  # MAIN slice 40, View Options -> 20000 scenarios, 0 differ
tools/ofs main-breaks SUITE=viewdlg  # its breaks -> 69, 65 caught, 3 proven equivalent; p-fn crashes
tools/ofs maindiff-run SUITE=logbook  # MAIN slice 41, the logbook and +0x90 -> 20000 scenarios, 0 differ
tools/ofs main-breaks SUITE=logbook  # its breaks -> 111, 105 caught, 6 proven equivalent
tools/ofs maindiff-run SUITE=wxdlg    # MAIN slice 42, the weather dialogs -> 20000 scenarios, 0 differ
tools/ofs main-breaks SUITE=wxdlg    # its breaks -> 99, 94 caught, 5 proven equivalent
tools/ofs maindiff-run SUITE=radiodlg # MAIN slice 43, the radios dialog -> 20000 scenarios, 0 differ
tools/ofs main-breaks SUITE=radiodlg # its breaks -> 118, all caught
tools/ofs maindiff-run SUITE=create   # MAIN slice 44, Create a Flight -> 20000 scenarios, 0 differ
tools/ofs main-breaks SUITE=create   # its breaks -> 98, 97 caught, 1 proven equivalent
tools/ofs main-smoke                      # boots the game on our MAIN.DLL, then restores the previous DLL byte for byte
tools/ofs flights-play                    # Flights' dialogs used in the game (loads, settings, default, delete, quit), retail twice and ours twice, saves and files compared -> PASS (~10 min)
tools/ofs optionseventsdiff-run   # Options.DLL, all 15 dialogs and 13 tab pages -> 20000 scenarios, 9860000 comparisons, 0 differ (~1 h)
tools/ofs options-breaks N=400    # its breaks -> 859, all caught, 17 proven equivalent (~8 h)
tools/ofs options                 # the native Options.DLL (bin/options-diff/OPTIONS_ours.dll); main-dialogs compares it in the game
tools/ofs simsuidiff-run N=2000    # SIMSUI.DLL, every routine -> 444 operations, 0 differ (~10 min)
tools/ofs simsui-breaks N=400      # its breaks, per class (tools/simsui_breaks.py; run from a frozen copy) -> 708, 698 caught; of the 10 missed, 8 are caught at N=2000, init-far and char-digit-end are reached but not caught (re-run 2026-11-19; docs/modules/simsui.md)
tools/ofs sim1rdiff-run             # SIM1 decompiled routines against retail over in-flight snapshots -> 186 operations, 372000 comparisons, 0 differ (~10 min)
tools/ofs sim1r-breaks               # their breaks -> 881 of 882 caught (gpass-steer missed: a coverage gap, see its docstring); proven equivalents listed in its docstring (tools/sim1r_breaks.py; run from a frozen copy)
tools/ofs sim1native-flights        # the native SIM1.sim (tools/ofs sim1, installed) against retail on the 74 scripted flights -> 74 match (~15 min)
tools/ofs simsui                   # the native SIMSUI.DLL (bin/simsui-native/SIMSUI_ours.dll), installed beside FLTSIM98.EXE
tools/ofs main-dialogs                    # 47 menu commands and dialog steps in the real game (with Options' dialogs and tab pages), retail against ours -> same controls, 41 of 47 pictures identical, PASS (~30 min); flaky: in 4 runs on 2026-11-23/24 two cancel passes listed Display options' (0x1409) pages in another z-order, and a run crossing sunrise differs on Time of day's combo (deferred-work.md), so rerun a FAIL before investigating
tools/ofs hostdiff-run N=20000            # HOST (id 1, FLTSIM98.EXE) slices 1-28, all 194 services, real files in bin/difftest/hostfs -> 0 differ (~11 min)
tools/ofs host-breaks                     # HOST broken on purpose -> 505 breaks (498 ran until the 2026-11-24 anchor repair), 467 caught, 15 proven equivalent, 21 crash, 2 rare display paths missed at N=2000 (caught at N=3000) (~5 h)
python3 tools/check_raw_dumps.py  # no raw retail dump in git beyond tools/raw_dumps_allowed.txt (may only shrink, bar an entry newly detected by a stricter rule, with its reason on its line; Story 1.4) -> raw dumps: 2 allowed dump files, 38 scripts reading orig/ (1 gen, 17 resource, 20 oracle), none new, exit 0
python3 -B tools/check_retail_bugs.py  # docs/retail-bugs.md against the RETAIL BUG markers in src/, both ways; every entry that still applies is marked and the allowlist file is an error (Story 1.5 slices 1-10 marked MAIN, Flights, Options, Gauges, SIM1, the runtime, HOST, UTIL, CHAIN, NUMERIC, FACIL, FE, WINDOW, WINMSG, MPCHAT, REALITY, ANALYSIS, DEMO, ADVDRV, DYNAMIC, CONTROLS, SOUND, ATC, ACONTAIN, BOOSTER, WEATHERM, SLEW, PANELS, G3D, HG2D, MULTIP, Views, Help, Alerts, World and MP_DIALOG); `--self-test` covers its failure cases -> retail bugs: 331 entries, 323 marked, 8 no longer apply
tools/ofs data-images  # every converted data image against retail, and the .rdata ranges, then the self-test of its own failure paths (needs orig/; UPDATE=1 rewrites tools/data_images.txt) -> data-images: 161 .data images identical (1587303 bytes, 9105 relocated), 106 .rdata ranges identical (18159 bytes), 1 switch tables, 1 tail windows (49005 bytes), 130 pointer lists (5931 entries), 1 raw pointer lists (295 rows), 1 bound images (2129 rows: 130 DATA, 486 CODE, 1234 IMPORT, 279 NONE), self-test passed (173 cases)
tools/ofs gauges-tables-check  # tools/gen_gauges.py --check: the committed gauge tables (layout.h, rebind.inc, gauges_manifest.json, host-compiled data.inc and rdata.inc) against retail's full analysis, writes nothing (needs orig/, objdump, cc) -> gauges-tables-check: 120 gauges: layout.h, rebind.inc, manifest line and the host-compiled data.inc and rdata.inc match retail, exit 0
tools/ofs mdl-images-check  # tools/gen_mdl_images.py --check: the committed MDL images (layout.h, host-compiled data.inc, mdl_manifest.json) against retail, writes nothing (needs orig/, objdump, cc) -> mdl-images-check: 11 models: layout.h, data.inc (host-compiled) and src/mdl/mdl_manifest.json match retail, exit 0
tools/ofs simsui-tables-check  # tools/gen_simsui_tables.py as an oracle: regenerates simsui_tables.inc in memory, writes nothing, exits 1 if the committed file differs (needs orig/SIMSUI.DLL, the CD's cd/mfc42.dll, i686-w64-mingw32-nm with the mingw import libraries; ~3 min) -> simsui-tables-check: src/simsui/simsui_tables.inc matches: 295 imports, 830 routines, 31 vtables
tools/ofs sim1-reference        # setup check (tools/gen_sim1_data.py): verifies the CD's SIM1.sim sha256, copies it to orig/Sim/SIM1.sim, writes bin/sim1-diff/survey.json
tools/ofs sim1state-build  # build only: SIM1_STATE_TEST.DLL (the run and break targets build it through this)
tools/ofs sim1air-build  # build only: SIM1_AIR_TEST.DLL (the run and break targets build it through this)
tools/ofs host-rt-tables-check  # tools/gen_host_rt_tables.py as an oracle: regenerates host_rt_tables.inc (outside its header comment) and retail's raw .rdata in memory, writes nothing, and fails on any difference from the committed file or from the image host_rt_rdata.inc compiles to (`python3 -B tools/gen_host_rt_tables.py --self-test` runs just its negative checks, which the check runs first); stops on any EXE but the reference (sha256). Needs orig/FLTSIM98.EXE, the CD's cd/mfc42.dll, objdump, i686-w64-mingw32-nm with the mingw import libraries (gen_simsui_tables.py:33) and a host cc; it does not run in a clean checkout alone -> host-rt-tables-check: 316 imports, 165 routines, 191 services match; .rdata 0x200 bytes match; self-test 5 cases
tools/ofs mpchatdiff-run      # whole-module MPCHAT, the chat window -> 20000 scenarios, 0 differ (~20 s)
tools/ofs mpchat-breaks       # MPCHAT broken on purpose -> 117 breaks, 107 caught, 4 proven equivalent, 6 crash (~8 min)
tools/ofs winmsgdiff-run      # whole-module WINMSG, the message bar -> 20000 scenarios, 0 differ (~2 min)
tools/ofs winmsg-breaks       # WINMSG broken on purpose -> 64 breaks, 58 caught, 4 proven equivalent; limit-margin, word-empty crash (~13 min); before Story 1.8 slice 3 only 61 of the 64 anchors matched (`word-empty`, `split-fits` and `close-title` sat across retail-bug comments, repaired in the runner, so those three never ran; the counts here are from before and not re-run)
tools/ofs realitydiff-run     # whole-module REALITY, crashes and the analysis -> 20000 scenarios, 0 differ (~1 min)
tools/ofs reality-breaks      # REALITY broken on purpose -> 67 breaks, 61 caught, 6 proven equivalent (~7 min)
tools/ofs analysisdiff-run    # whole-module ANALYSIS, landing/manoeuvre/course -> 20000 scenarios, 0 differ (~2 min)
tools/ofs analysis-breaks     # ANALYSIS broken on purpose -> 103 breaks, 93 caught, 10 proven equivalent (~20 min)
tools/ofs controlsdiff-run    # whole-module CONTROLS: commands, joysticks, key bindings, force feedback, the dialog -> 20000 scenarios, 0 differ (~40 min)
tools/ofs controls-breaks     # CONTROLS broken on purpose -> 81 breaks, 78 caught, 3 proven equivalent (~20 min)
tools/ofs g3ddiff-run         # whole-module G3D, the 3D pipeline -> 20000 scenarios, 0 differ (~5 min)
tools/ofs g3d-breaks N=1000   # G3D broken on purpose -> 317 breaks, 278 caught, 7 proven equivalent, 32 missed: plane-facet-skip crashes (19 missed at N=4000; ~4 h)
tools/ofs mdl-crash-diff-run  # the three decompiled crash MDLs against retail -> 2000 scenarios per MDL and mode, three MDLs in two modes, 0 differences (~3 min)
python3 tools/mdl_crash_breaks.py # (from a frozen copy) the crash MDLs broken on purpose -> 82, 78 caught, 4 proven equivalent (~4 min)
tools/ofs hostrtdiff-run N=20000 # the decompiled host runtime (src/host/runtime/) -> 161 operations, 3220000 comparisons, 0 differ (~30 min)
python3 tools/host_rt_breaks.py # (from a frozen copy) its breaks -> 569, 557 caught (2 more at N=20000), 10 proven equivalent
tools/ofs mdl-crash-game  # the three crash flights, retail against the decompiled MDLs -> identical frames
tools/ofs gauges-check N=20000 # all 120 hand-written gauges against retail, every exposed callback/data/resource/FP result -> 120 checked, 0 differ
python3 tools/gauge_breaks.py # (from a frozen copy) the hand-written gauges broken on purpose -> 67, 66 caught; `chrn-minutes` missed, also at N=20000 and through the Makefile (re-run 2026-11-19; docs/remaining-work.md)
tools/ofs gauges-breaks       # tools/gauge_breaks.py again (it rewrites src/gauges/: from a frozen copy) -> 67, 66 caught; `chrn-minutes` missed
# Story 1.8 slice 2: `--build-only DIR` on controls_breaks.py, panels_breaks.py (with `--freeze FILE` / `--cases FILE`, the sampled cases kept or replayed at the same operation and site), gauges_breaks.py (`[--suction]`) and gauge_breaks.py builds every case one at a time into DIR/<case>/mutant.dll with meta.json, runs nothing, restores the baseline source and build, and exits non-zero when nothing matches; `tools/build_gauges.py --jobs N` sets its parallelism
# Story 1.8 slice 3: `--build-only DIR` on window_breaks.py, winmsg_breaks.py, mpchat_breaks.py (pinned 109, 64 and 117 cases) and mpchat_ui_breaks.py (12 cases, no baseline) builds every case one at a time into DIR/<case>/mutant.dll (the UI runner also keeps harness.exe) with meta.json, runs nothing, restores the temporary source and DLL (bytes, mtimes, existence) and exits 1 on no matching case, a non-unique anchor or a build failure; run it directly after the main build, not through the break targets. It needs `bin/window.res` with `bin/window/window_text_tail.inc`, `bin/winmsg.res`, `bin/mpchat.res`, and for the UI runner `bin/mpchat-ui/mpchat.res` (windres of `src/mpchat/mpchat.rc`); the shared helper is `tools/break_build_only.py`. `mpchat_ui_breaks.py` now copies and hashes `abi.h`, `arena.h` and `tools/difftest/argcount.h` beside `linkage.h` and `host.h` and uses a fresh directory per case. Hardening: a missing or empty `--build-only` value exits 2; every named filter must match a case (a typo exits 1 before anything is made); SIGTERM and SIGHUP still run the restore, and one failed restore does not stop the others; the UI runner builds in a temporary directory beside DIR (removed afterwards, `bin/mpchat-ui/breaks` untouched) and rejects a DIR in `bin/mpchat-ui/breaks`; the gate rejects an unknown `--require-set`, a before/after or `--mut` pair that is one directory, a `meta.json` name that is not its directory, and a `mut-mpchat-ui` case without `harness.exe`
# Story 1.8 slice 4: `python3 tools/host_breaks.py --build-only DIR [NAME...]` (505 cases) and `python3 tools/host_rt_breaks.py --build-only DIR [NAME...]` (569) compile every case's mutated unit (`host.c`; the one runtime file) with `-c`, from a same-basename copy in a temporary directory, into DIR/<case>/mutant.o with meta.json (`name` is the case), and DIR/baseline/ holds the unmutated units the runner links (host.o plus the five `-DHOST_RT_STANDALONE` companions; hostrtdiff.o plus the 19 runtime objects). Nothing under the tree is written and nothing runs. Run it directly after `build_side.sh`, from the snapshot root. Exit 0 when all built, 1 when a name matches no case, an anchor does not match exactly once, or a compile fails or leaves no object (the first two before anything is made), 2 for a missing or empty DIR; SIGTERM and SIGHUP clean up and exit.
tools/ofs gauges-panels N=1000 # same PANELS, retail/rebuilt gauge collection, strict full-state comparison
tools/ofs gaugesdiff-run      # the warning gauge, retail against rebuilt, callbacks, data image and bitmap -> 20000 scenarios at its preferred base, then 20000 under forced relocation (GAUGES_RELOCATE=1), 0 differ (no wine run since slice 18a moved its data to committed C; first run still to record)
tools/ofs suctiondiff-run     # Cessna suction, callbacks, data image and two bitmaps -> 20000 scenarios plus 240288 numeric vectors, repeated under forced relocation, 0 differ (no wine run since slice 18a; first run still to record)
tools/ofs panelsdiff-run N=3000 # whole-module PANELS through the installed gauges -> 3000 scenarios, 0 differ (~15 min)
tools/ofs panelsunit-run N=20000 # PANELS's image routines alone -> 20000 cases each, 0 differ (~3 min)
tools/ofs panels-breaks       # PANELS broken on purpose -> 200 sampled, 164 caught, 34 missed, 2 do not build; the 34 rerun with N=800 leave 30, those with N=3000 leave 12 (re-run 2026-11-18; the Makefile gives the same, docs/modules/panels.md)
tools/ofs g3dmeshdiff-run     # the mesh compiler: instructions vs portable C, x87 and soft float -> 100000 polygons, 0 differ
tools/ofs multipdiff-run      # whole-module MULTIP: mixed and worker lifecycles, the whole module -> 20000 scenarios per suite, 0 differences (~6 min; first recorded 2026-11-24)
tools/ofs difftest-run        # whole-module SLEW         -> 0/2000 vectors differ
tools/ofs ofstest-run         # in-process, via the host  -> passed 68, failed 0
tools/ofs hg2ddiff-run  # whole-module HG2D, a suite per slice -> 59 suite summaries, 0 differ (N=20000 takes ~5.5 h alone, 19854 s on 2026-11-24; docs/modules/hg2d.md)
tools/hg2d_frames.sh 'Situatio\PARIS.STN' 900  # the running game, retail HG2D twice then ours -> identical frames, PASS, on a private Xvfb; HG2D_FRAMES_ALL=1 for the whole game (needs tools/ofs framelock sehfix)
o=$(mktemp -d -u) && REFERENCES_OUT=$o tools/ofs references CASES="global/STGEORGE flight/PARIS script/controls/PARIS" && test "$(grep -cP '^case\t(global/STGEORGE|flight/PARIS|script/controls/PARIS)\tmatch\t' "$o/manifest.txt")" = 3  # the reference recorder on a subset, into a fresh directory -> three cases of 900 passes, all match (checked here from the manifest), tools/references.txt untouched (~2 min)
tools/ofs smoke               # every module of tools/modules.txt installed (tools/ofs ofs_install) -> 27/27 modules (the 27 of Modules\; a leftover OFSTEST.DLL makes it 28/28), 95 gauges, PASS
tools/ofs util-trace          # which UTIL services the game calls, counted in-game
tools/check_module_flags.sh   # every gcc invocation takes OFS_MODULE_FLAGS (Story 1.3) -> PASS, prints "every site takes OFS_MODULE_FLAGS"
python3 -B tools/check_arena.py   # Story 1.8 (AD-15; also runs on every cmake --build): no (uint32_t)(uintptr_t) cast or #define of arena.h's names in src/ beyond tools/arena_lint_allowed.txt, whose rows may only shrink (also vs HEAD, or `--prior FILE`); `--inventory` prints the rows and the reverse-cast count, `--self-test` runs the matrix -> arena lint: 65 files allowlisted, 472 casts, 71 defines left (reverse casts: 52); self-test 30 cases
python3 -B tools/check_abi_headers.py   # Story 1.7: one FS98_STDCALL (abi.h), no pointer/long/size_t/wchar_t/bitfield/long double in the four ABI headers, and the headers with their _Static_asserts compile on mingw i686, gcc -m32 and gcc; `--self-test` runs failing fixtures for each -> both PASS
```

Manual, not a Baselines line (it takes hours, and `tools/cmake_verify.py` would run it): the full recording, `tools/ofs references`,
is run from a frozen `cp -a` copy of the committed tree (with `cd/`, `game/`, `orig/` and `bin/`), so the manifest has no
`+dirty`. Configure a fresh build directory in the copy, point `OFS_BUILD` at it, and check that its `CMakeCache.txt` names the copy as
`CMAKE_HOME_DIRECTORY`. Then copy `bin/references/` and `tools/references.txt` back with the same marker-and-rename sequence under the live
tree's `bin/references.lock`, run `tools/record_references.sh --verify bin/references` (it reads only that set's `manifest.txt`) and
`cmp bin/references/manifest.txt tools/references.txt`. Story 1.6 closes when that verified mirror is committed.

## Retail reference logs

`tools/record_references.sh` (Story 1.6, AD-8) flies each case twice through `tools/hg2d_frames.sh` with `HG2D_FRAMES_RETAIL_ONLY=1`:
retail only, from a game staged by `tools/stage_game.py` into a fresh work directory under `bin/references-work/`, with SEHFIX and
FRAMELOCK added, `[HARDWARE] Device=` empty (the software renderer), no sound, 900 passes. Every `MZ` file of the stage must be
byte-identical to a file under `cd/`, and `FLTSIM98.EXE` must be the pressing of `REFERENCE_SHA256`, or the recorder stops before wine starts.

- **Cases** (131): `flight/<NAME>` for each of the 74 `.STN` in `Situatio/`, nobody flying; `script/<scenario>/<NAME>` for `controls`,
  `systems`, `views`, `avionics`, `slew`, `toggles` and `keys` (`tools/fly_scripts.py`) on `PARIS`, `STGEORGE`, `KAUAI`, `NEWYORK`, `LEARLON`,
  `HONGKONG`, `BELLWTC` and `CVINSON`; `global/STGEORGE`, nobody flying, with GLOBAL dumped each pass.
- **Outcomes:** `match` (both runs complete and identical), `NONDETERMINISTIC` (both complete, different: both runs' files kept under `a/` and `b/`),
  `FAILED` (a run did not end with `end 00000384`, had under 20 distinct frames, loaded no builtin `msvcrt.dll`, FRAMELOCK or SEHFIX, or a GLOBAL dump
  is not 4,298,400 bytes; or a script is missing, has an event outside passes 1 to 899, or changed nothing against the unscripted flight). Any `FAILED`
  case, or a `global/STGEORGE` that is not a `match`, fails the recording and nothing is published. `delivery unchecked` marks a script case whose
  script check could not be made because one of its two pairs is NONDETERMINISTIC.
- **Published set:** `bin/references/` holds `<case>/frames.log` (and `global.bin`), `config/FLTSIM98.CFG`, `config/devices.cfg` and `manifest.txt`,
  which is authoritative; `tools/references.txt` mirrors it and is the only committed part. The manifest starts with `references v1`, then the
  commit (`+dirty` if a tracked file differs), the pressing (EXE, PE and data digests), the config (wine version, `WINEDLLOVERRIDES`, hashes of the
  CFGs, FRAMELOCK, SEHFIX and the scripts), then one tab-separated line per case: `case`, name, outcome, flight, script sha256 or `-`, passes, delivery,
  and `path=sha256` for each kept file. `tools/record_references.sh --verify DIR` checks a set against its manifest.
- **Config consequence (ANA-1):** retail runs on wine's builtin `msvcrt`, as every comparison does, not the CD's 1997 `MSVCRT.DLL` (under it wine's own
  `advapi32`, `user32` and `gdi32` fail to load). Host `+0x21c`'s `memmove` therefore leaves 0 in ecx, not 7: ANALYSIS's trail compaction
  (`docs/retail-bugs.md`, ANA-1) behaves as with the builtin, and Epic 3 must allow for that when it compares.
- **Publishing:** under `flock bin/references.lock`, the candidate is built and verified in `bin/references.new/`, then `bin/references.publishing` is
  written, `bin/references` becomes `.old`, `.new` becomes `bin/references`, the mirror is written through a temporary file and checked equal, and
  only then are `.old` and the marker removed. If the marker, `.old` or `.new` exists at the start, the recorder refuses and names them. A non-empty
  `bin/references/` needs `REFERENCES_FORCE=1`. A subset publishes to `REFERENCES_OUT` (which must not exist, and lie outside `bin/references*`,
  `game/`, `cd/`, `orig/`, `src/`) and never touches `tools/references.txt`.

### `frames.log` grammar (v1)

One text file, one line per event, `\n` terminated, every number 8 lowercase hex digits (zero padded). A run that reaches its pass limit ends with
`end`. Version 1 compares **all of `frames.log`, byte for byte** (as `hg2d_frames.sh` does), and the GLOBAL dump; `regs.txt` is not compared.

| Line | Fields | Meaning |
|------|--------|---------|
| blit | `pass blit kind x y w h size hash palette` | A `BitBlt`, `StretchBlt` or `StretchDIBits` call, logged before the call is forwarded; the destination window is not checked or named. `pass`: passes counted so far. `blit`: blit counter, from 0, in call order. `kind`: `bitblt`, `stretchblt` or `stretchdibits`. `x y w h`: the call's destination rectangle arguments (signed dwords). `size`: the source DIB's byte count, `(width*bitcount+31)/32*4*abs(height)`, and 0 when there is no DIB or no bits. `hash`: FNV-1a (32 bit, basis `811c9dc5`) over those bytes; `palette`: the same over the colour table. With no DIB bits both stay `811c9dc5`. A recording counts distinct frames from the `hash` of lines with a nonzero `size`. |
| `end` | `end passes` | Written once, last, as the process exits at the pass limit: `end 00000384` for 900 passes. |
| `fault` | `fault pass <pass> eip <eip> stack <dword>...` | An access violation, logged before wine may swallow it: the pass, the faulting address and up to 40 stack dwords. The addresses are retail-specific (retail's image, wine's stack). |
| `sim1` | `sim1 pass <dword pairs>... ` | Only with `FRAMELOCK_SIM1`, which the recorder never sets: not part of v1 recordings. |

`global.bin` (`global/` case only) is 900 records of `0x12a8` bytes, 4,298,400 bytes, the raw bytes of GLOBAL's data, unmasked (masking relocated dwords and
handles is Story 3.6's). `regs.txt` (`FRAMELOCK_REGS`, one line per module slot: name, offset, calls, input hash, then output and kept hashes per register) is a separate
file that the recorder does not produce and v1 does not compare.

Limits of v1, kept as FRAMELOCK writes them today (`framelock.c` is unchanged):

- there is no window identity: a blit line does not say which window it drew to;
- blits are logged in call order, as they are made, not as the last frame presented each pass;
- `fault` lines hold retail-specific addresses;
- GLOBAL record `n` is the state before pass `n` runs, so a native run's state after pass `n` lines up with record `n+1`, through pass 898; the last hook
  writes record 899 and exits without running pass 899's loop;
- script events are dispatched after the pass is counted: an event at pass `p` runs as pass `p` begins, `p >= 1` (an event at 0 first runs at pass 1).

Epic 3's FRAMELOCK in `fs98win32` defines how it maps onto v1 (AD-8).
