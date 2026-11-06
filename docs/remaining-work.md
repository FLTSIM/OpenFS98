# Remaining work

What is still retail, and what is left before a native build. The 27 simulator modules in
`Modules\` are done and in `MODULES` (two more module DLLs, SIM1 and the `.MDL`s, live
outside it and are not), and `English\` World, Alerts, Help, Sim, Views, Flights and MP_DIALOG
are in `ENGLISH_MODULES`; `make install` puts them all in the game. Installed rows below are retained for context. Sizes are `.text` from the reference CD. Differences
and gaps in the finished modules are in [open-differences.md](open-differences.md).
Results updated through 2026-11-06; individual rows retain their acceptance dates.

## Binaries

| Binary | `.text` | Status | Where |
| --- | --- | --- | --- |
| `English/Options.DLL` | 84,381 | **In progress** (2026-11-03): 10 of 15 registrations (the video, logbook and analysis dialogs) verified in its harness (`tools/options.mk`); no DLL yet | [options.md](modules/options.md) |
| `FLTSIM98.EXE` | 82,016 | **No executable.** All 194 services are reimplemented and verified in-process as `HOST.DLL`; retail's still runs the game | [host.md](modules/host.md) |
| `SIMSUI.DLL` | 79,850 | **Not started.** 540 named exports, the common dialog and control widgets. Every dialog imports it | [dialogs.md](modules/dialogs.md) |
| `Sim/SIM1.sim` | 78,216 | **Not started.** The flight model: every aircraft's `.AIR` names `sim1`. A module DLL, id 25, 15-entry Linkage, wanting 10 modules. Not in `orig/` | [below](#sim1-and-the-mdls) |
| `Gauges/*.GAU` (120) | 63,472 | **All 120 rebuilt**, included in build/install/revert; standalone callback/data/resource/FP, PANELS and frame comparisons | [gauges.md](modules/gauges.md) |
| `English/Flights.DLL` | 51,122 | **Installed** 2026-11-02; used in play by `make flights-play` 2026-11-03 | [flights.md](modules/flights.md) |
| `English/Sim.DLL` | 33,239 | **Installed** 2026-11-02 | [sim.md](modules/sim.md) |
| `English/MP_DIALOG.DLL` | 25,401 | **Installed** 2026-11-05; all three registrations, native exception/UI acceptance and paired actual-owner peer session | [mpdialog.md](modules/mpdialog.md) |
| `Aircraft/*/{model,MODEL.RG}/*.MDL` (11) | 35,074 | **Implemented and verified; private copies only** 2026-11-06; all eleven drop-in MDLs built. Eight normal models: 1,680,000 module scenarios and eight matching scripted flights. Three crash models: 12,000 lifecycle scenarios, all 32 functions/model reached, eight caught defects plus a rendered-pose mutation, and three matching 1,200-pass crash flights. No default installation | [mdl.md](modules/mdl.md) |
| `English/Views.DLL` | 13,090 | **Installed** 2026-11-02 | [views.md](modules/views.md) |
| `fltsim95.exe` | 11,464 | **Not looked at.** A stub that ships alongside; imports only KERNEL32. Probably not needed | [architecture.md](architecture.md) |
| `English/Help.DLL` | 10,657 | **Installed** 2026-11-02 | [help.md](modules/help.md) |
| `English/Alerts.DLL` | 7,649 | **Installed** 2026-11-02 | [alerts.md](modules/alerts.md) |
| `DIALOG.DLL`, `ENGLISH.FLL` | 406, 438 | **Resources:** bitmaps; strings, a menu and the key tables. To be carried over, not decompiled | [dialogs.md](modules/dialogs.md) |

The installed dialogs still build standalone in `tools/<name>.mk`, with their own
harnesses; the shared build copies each to `bin/<NAME>.DLL`, and `make <name>-install` /
`<name>-revert` do one alone. All of them import retail SIMSUI and MFC42.

**Third-party dependencies, not ours to decompile but to replace for a native port:**
`MFC42` (every dialog), Win32, GDI, DirectDraw, Direct3D, DirectSound, DirectInput,
DirectPlay.

## The gauges

All 120 reference-CD modules are rebuilt from C with extracted data and bitmap
resources. The shared build, install and revert targets include them. Standalone
verification covers every exposed callback, all x87 precision/rounding modes,
full data/resource comparisons and forced relocation; PANELS and real-game
checks exercise the collection together. See [gauges.md](modules/gauges.md).
Their current 32-bit ABI still needs adaptation with the rest of the native port.

## SIM1 and the MDLs

Both are ordinary module DLLs with their own Linkage, loaded by ACONTAIN through host
`+0x26c` rather than from `Modules\` ([acontain.md](modules/acontain.md)), so the module
harness patterns apply to them. SIM1 is not in `orig/` yet; verify it against the
reference CD before writing tools against it. All eleven MDLs, including
`C182/MODEL.RG`, have been byte-compared with their mapped CD files and copied
to `orig/Aircraft/` (2026-11-06).

- **`SIM1.sim`** (base `0x22000000`, `.text` 78,216, `.data` 17,400): imports only KERNEL32
  and MSVCRT, and links to modules 1, 2, 3, 4, 10, 16, 20, 23, 32 and 39. It calls host
  `+0x21c` 77 times and reads and writes GLOBAL widely, like DYNAMIC. As large as SIMSUI and
  in every flight, so it should not wait behind the dialogs.
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
2. **Options** (Flights installed 2026-11-02). The largest code still used in play. Scripted menu tests come
   with them: see [open-differences.md](open-differences.md#not-yet-tested).
3. **SIM1, then the MDLs.** Simulator code in every flight, testable like the modules.
   Copy them into `orig/` first.
4. **SIMSUI.** After the dialogs, because they show which of its 540 exports matter.
5. **Gauges completed independently of Options.** All 120 are in the shared build.
6. **The executable around `HOST.DLL`.**
7. **Native builds.** Moving off x86 ends comparison with retail, so the earlier steps come
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
