# Remaining work

What is still retail, what is not yet decompiled, and what is left before a native build. Every
binary now has a replacement: the 27 simulator modules in `Modules\` are in `MODULES`;
`English\` World, Alerts, Help, Sim, Views, Flights, MP_DIALOG and Options are in
`ENGLISH_MODULES`; SIMSUI is in `SHARED_MODULES`; SIM1, the gauges and the MDLs have their own
targets. `make install` puts them all in the game. Sizes are `.text` from the reference CD.
Differences and gaps in the finished modules are in [open-differences.md](open-differences.md).
Results updated through 2026-11-08; individual rows retain their acceptance dates.

## Lifted, not decompiled

Found 2026-11-07. Four replacements made by other agents are **lifted**: a Python generator reads
retail's instructions and writes C that emulates them one by one, with the x86 registers and flags
in a struct and the jumps as `goto`s (`f->eax = mdl_read(f, ...); if (f->flags & 0x40) goto
label_437f;`). The C is generated into ignored `bin/` at build time from the CD's binaries, so the
repository holds no readable source for this code. It matches retail and passes every test listed
for it, but it is not a decompilation: nobody can read or change it, and it cannot leave x86. Each
is to be decompiled by the usual method (hand-written C, a differential harness, deliberate breaks),
keeping the lifted build as a second oracle until the hand-written one replaces it.
SIM1, the first, was replaced on 2026-11-08; three remain.

| What | Lifted part | Generator | Hand-written already | Order |
| --- | --- | --- | --- | --- |
| `Sim/SIM1.sim` | AIR decoder, aircraft setup, tick/frame simulation: 190 routines, 17,836 instructions (most of 78,216 bytes) | `tools/gen_sim1_lift.py` | six services and the state lifecycle (`src/sim1/sim1.c`, `sim1_state.inc`, `sim1_air.inc`); **all of it, done 2026-11-08**: all 186 routines decompiled and verified one by one (`src/sim1/sim1_small.c`, 186 operations, 0 differ, 882 breaks), built as the native SIM1.sim over retail's data (`src/sim1/sim1_native.c`, `make sim1`), 74 scripted flights frame for frame; installed, the lift kept only as an oracle ([sim1.md](modules/sim1.md#the-native-build)) | **1, done 2026-11-08** |
| `Gauges/*.GAU` (120) | every gauge's code, 1,318 routines, 63,472 bytes | `tools/gen_gauges.py` | `src/gauges/warning.c`, `suction.c` | 2 |
| Crash MDLs (3) | the three crash models, about 28 KB | `tools/gen_mdl_crash.py` | the eight normal models (`src/mdl/mdl_normal.c`) | 3 |
| `FLTSIM98.EXE` | startup, window procedures, timer, message loop, shutdown: 402 routines, 13,081 instructions | `tools/gen_host_runtime.py` | all 194 services (`src/host/host.c`) | 4 |

G3D's mesh compiler (`src/g3d/g3d_mesh.S`, `tools/gen_g3d_mesh.py`) is a transcription too, but a
deliberate one for x87 rounding, with a portable C version checked against it.

## Binaries

| Binary | `.text` | Status | Where |
| --- | --- | --- | --- |
| `English/Options.DLL` | 84,381 | **Done and installed** (2026-11-07): all 15 registrations and 13 tab pages verified (0 differ, 859 breaks caught, `tools/options.mk`); in `ENGLISH_MODULES`; `main-dialogs` opens 23 of its dialogs and pages, identical | [options.md](modules/options.md) |
| `FLTSIM98.EXE` | 82,016 | **Executable built** (2026-11-06): `OPENFS98.EXE`, all 194 services decompiled; startup/windows/loop/shutdown lifted (see above); 74 matching scripted flights, runtime differential and UI/Direct3D acceptance. Retail remains available; native platform work remains | [host.md](modules/host.md) |
| `SIMSUI.DLL` | 79,850 | **Done and installed** (2026-11-07): every routine transliterated, 444 operations verified (0 differ, `tools/simsui.mk`), deliberate breaks per class; native DLL with all 540 exports at retail's ordinals; in `SHARED_MODULES`, installed beside `FLTSIM98.EXE`; `main-dialogs` with it: same controls, 41 of 47 pictures identical (as before) | [simsui.md](modules/simsui.md) |
| `Sim/SIM1.sim` | 78,216 | **Decompiled and installed** (2026-11-08): every routine hand-written C (`src/sim1/sim1_small.c`), 186 operations against retail over in-flight snapshots (0 differ, 882 breaks); the native DLL over retail's data matches retail on all 74 scripted flights, numeric state and frames. The lifted build (installed 2026-11-06) stays as a second oracle (`tools/stage_sim1.py --lifted`). x87 and the 32-bit ABI are still assumed, as for every module | [sim1.md](modules/sim1.md) |
| `Gauges/*.GAU` (120) | 63,472 | **All 120 lifted** (see above), included in build/install/revert; standalone callback/data/resource/FP, PANELS and frame comparisons | [gauges.md](modules/gauges.md) |
| `English/Flights.DLL` | 51,122 | **Installed** 2026-11-02; used in play by `make flights-play` 2026-11-03 | [flights.md](modules/flights.md) |
| `English/Sim.DLL` | 33,239 | **Installed** 2026-11-02 | [sim.md](modules/sim.md) |
| `English/MP_DIALOG.DLL` | 25,401 | **Installed** 2026-11-05; all three registrations, native exception/UI acceptance and paired actual-owner peer session | [mpdialog.md](modules/mpdialog.md) |
| `Aircraft/*/{model,MODEL.RG}/*.MDL` (11) | 35,074 | **Built and verified; included in install/revert** 2026-11-06; the eight normal models decompiled, the three crash models lifted (see above). Eight normal models: 1,680,000 module scenarios and eight matching scripted flights. Three crash models: 12,000 lifecycle scenarios, all 32 functions/model reached, eight caught defects plus a rendered-pose mutation, and three matching 1,200-pass crash flights. Shared install/revert integration verified in a disposable game tree | [mdl.md](modules/mdl.md) |
| `English/Views.DLL` | 13,090 | **Installed** 2026-11-02 | [views.md](modules/views.md) |
| `fltsim95.exe` | 11,464 | **Not looked at.** A stub that ships alongside; imports only KERNEL32. Probably not needed | [architecture.md](architecture.md) |
| `English/Help.DLL` | 10,657 | **Installed** 2026-11-02 | [help.md](modules/help.md) |
| `English/Alerts.DLL` | 7,649 | **Installed** 2026-11-02 | [alerts.md](modules/alerts.md) |
| `DIALOG.DLL`, `ENGLISH.FLL` | 406, 438 | **Resources:** bitmaps; strings, a menu and the key tables. To be carried over, not decompiled | [dialogs.md](modules/dialogs.md) |

The installed dialogs still build standalone in `tools/<name>.mk`, with their own
harnesses; the shared build copies each to `bin/<NAME>.DLL`, and `make <name>-install` /
`<name>-revert` do one alone. All of them import SIMSUI (ours once installed) and MFC42.

**Third-party dependencies, not ours to decompile but to replace for a native port:**
`MFC42` (every dialog), Win32, GDI, DirectDraw, Direct3D, DirectSound, DirectInput,
DirectPlay.

## The gauges

All 120 reference-CD modules are lifted to generated C (see above), with extracted data and
bitmap resources. The shared build, install and revert targets include them. Standalone
verification covers every exposed callback, all x87 precision/rounding modes,
full data/resource comparisons and forced relocation; PANELS and real-game
checks exercise the collection together. See [gauges.md](modules/gauges.md).
Their current 32-bit ABI still needs adaptation with the rest of the native port.

## SIM1 and the MDLs

Both are ordinary module DLLs with their own Linkage, loaded by ACONTAIN through host
`+0x26c` rather than from `Modules\` ([acontain.md](modules/acontain.md)), so the module
harness patterns apply to them. SIM1 has been verified against the reference
CD and copied to `orig/Sim/` (2026-11-06). All eleven MDLs, including
`C182/MODEL.RG`, have been byte-compared with their mapped CD files and copied
to `orig/Aircraft/` (2026-11-06).

- **`SIM1.sim`** (base `0x22000000`, `.text` 78,216, `.data` 17,400): imports only KERNEL32
  and MSVCRT, and links to modules 1, 2, 3, 4, 10, 16, 20, 23, 32 and 39. It calls host
  `+0x21c` 77 times and reads and writes GLOBAL widely, like DYNAMIC. As large as SIMSUI and
  in every flight. It is now implemented and installed: six services and the
  state lifecycle have isolated comparisons; the real AIR decoder, aircraft
  setup and simulation callbacks add standalone and game acceptance. All 74
  scripted flights match numeric state and frames. Native-platform ABI and FP
  work remains; see [SIM1](modules/sim1.md).
- **The `.MDL`s**: eight normal models (`.text` 678–982 bytes) and three crash
  models (9,110–9,894 bytes), mostly `.data`. Shared Linkage scaffolding wraps
  distinct header update callbacks; crash models also register with CHAIN.
  All eleven now have C implementations and standalone differential verification;
  see [MDL verification](modules/mdl.md) for game acceptance and remaining limits.

## Order

1. **Done 2026-11-02: the four finished dialogs are installed** (Alerts, Help, Sim, Views).
   `make smoke` loads all five English DLLs from `English\`: 27/27 modules, 95 gauges, PASS.
   `make main-dialogs` now runs retail MAIN with the retail English DLLs against ours with
   ours, and reaches Help's About, Credits and Lessons, View Options, Sim's Aircraft Settings
   and crash analysis, and Alerts' Exit confirmation: PASS, every control and its text the
   same, 14 of 20 pictures identical, the rest only in the clock, a live preview or the
   crash's real-time curve. All 74 flights still match frame for frame with every module
   and English DLL ours. Not yet reached in play: Alerts' dialogs 3 and 4, Help's lesson
   graph and lesson completion; see [open-differences.md](open-differences.md#not-yet-tested).
2. **Options: done and installed 2026-11-07.** Not reached in play: the analysis graphs, Save
   video and the video messages, and using controls beyond OK and Cancel
   ([open-differences.md](open-differences.md#not-yet-tested)).
3. **SIM1 decompiled and installed 2026-11-08** (it was lifted 2026-11-06); **the MDLs built 2026-11-06** (the crash
   models still lifted). Both are included in shared build/install/revert.
4. **SIMSUI: done and installed 2026-11-07.** All 540 exports; the English dialogs, retail's and ours, run on it in `main-dialogs`.
5. **Gauges completed independently of Options.** All 120 are in the shared build.
6. **Host executable built 2026-11-06** (startup and windows lifted). Build/run and verification are in [host.md](modules/host.md); native platform replacement remains.
7. **Decompile the lifted code**, in the order of the table above.
8. **Native builds.** Moving off x86 ends comparison with retail, so the earlier steps come
   first. That is also when the retail bugs we reproduce get fixed: they are listed in
   [retail-bugs.md](retail-bugs.md), which must first be completed from every module's doc. Register contracts between modules ([open-differences.md](open-differences.md#a-class-of-bug-to-look-for))
   are x86 behaviour that a port has to remove on both sides of each call.

## Other useful work, in no particular order

1. **Reach UTIL's menu paths.** A hands-on session exercised 19 of its 47 services
   in-game, but the flight, lesson and options screens -- where the file listings and the
   exchange engine run -- were not reachable under wine. Worth retrying with
   `make util-trace UTIL_TRACE_SECS=600` whenever the sim gets further, or on Windows.
2. **Teach `make graph` about the `English\` modules.** `Flights`, `Options`, `Alerts` and
   the rest own 64 of the host's 65 dialogs; the graph does not see them yet.
3. **The host's 84 services that call no platform API.** Portable logic, and the
   specification a native host has to satisfy ([portability.md](portability.md)).
