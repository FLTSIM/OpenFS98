# Remaining work

What is still retail, and what is left before a native build. The 27 simulator modules in
`Modules\` are done and in `MODULES` (two more module DLLs, SIM1 and the `.MDL`s, live
outside it and are not), and `English\` World, Alerts, Help, Sim and Views
are in `ENGLISH_MODULES`; `make install` puts them all in the game. Everything below is not. Sizes are `.text` from the reference CD. Differences
and gaps in the finished modules are in [open-differences.md](open-differences.md).
Results here are from 2026-11-01.

## Binaries

| Binary | `.text` | Status | Where |
| --- | --- | --- | --- |
| `English/Options.DLL` | 84,381 | **Not started.** Inventory only: 15 registrations, 31 templates | [options.md](modules/options.md) |
| `FLTSIM98.EXE` | 82,016 | **No executable.** All 194 services are reimplemented and verified in-process as `HOST.DLL`; retail's still runs the game | [host.md](modules/host.md) |
| `SIMSUI.DLL` | 79,850 | **Not started.** 540 named exports, the common dialog and control widgets. Every dialog imports it | [dialogs.md](modules/dialogs.md) |
| `Sim/SIM1.sim` | 78,216 | **Not started.** The flight model: every aircraft's `.AIR` names `sim1`. A module DLL, id 25, 15-entry Linkage, wanting 10 modules. Not in `orig/` | [below](#sim1-and-the-mdls) |
| `Gauges/*.GAU` (120) | 63,472 | **Not started.** No `src/gauges/` and no survey. PANELS, their runtime, is ours | below |
| `English/Flights.DLL` | 51,122 | **In progress.** All 13 registrations transliterated and verified in their harness (`tools/flights.mk`); no DLL yet | [flights.md](modules/flights.md) |
| `English/Sim.DLL` | 33,239 | **Installed** 2026-11-02 | [sim.md](modules/sim.md) |
| `English/mpdialog.dll` | 25,401 | **Partial,** being worked on separately: registries `0x110a`..`0x110c` | [mpdialog.md](modules/mpdialog.md) |
| `Aircraft/*/model/*.MDL` (9) | 25,014 | **Not started.** The aircraft's visual models, module id 31, 9-entry Linkage. Mostly `.data`. Not in `orig/` | [below](#sim1-and-the-mdls) |
| `English/Views.DLL` | 13,090 | **Installed** 2026-11-02 | [views.md](modules/views.md) |
| `fltsim95.exe` | 11,464 | **Not looked at.** A stub that ships alongside; imports only KERNEL32. Probably not needed | [architecture.md](architecture.md) |
| `English/Help.DLL` | 10,657 | **Installed** 2026-11-02 | [help.md](modules/help.md) |
| `English/Alerts.DLL` | 7,649 | **Installed** 2026-11-02 | [alerts.md](modules/alerts.md) |
| `DIALOG.DLL`, `ENGLISH.FLL` | 406, 438 | **Resources:** bitmaps; strings, a menu and the key tables. To be carried over, not decompiled | [dialogs.md](modules/dialogs.md) |

The four installed dialogs still build standalone in `tools/<name>.mk`, with their own
harnesses; the shared build copies each to `bin/<NAME>.DLL`, and `make <name>-install` /
`<name>-revert` do one alone. All of them import retail SIMSUI and MFC42.

**Third-party dependencies, not ours to decompile but to replace for a native port:**
`MFC42` (every dialog), Win32, GDI, DirectDraw, Direct3D, DirectSound, DirectInput,
DirectPlay.

## The gauges

120 files, 12,973,568 bytes, of which only 63,472 are `.text`, from 208 bytes (`737WARNG`)
to 2,902 (`LEARPFD`). The rest is not surveyed, but is presumably bitmaps and gauge tables,
so these may be far less work than their size suggests. Survey them before planning: how
much code they share, and how much is data a generator could carry over.

## SIM1 and the MDLs

Both are ordinary module DLLs with their own Linkage, loaded by ACONTAIN through host
`+0x26c` rather than from `Modules\` ([acontain.md](modules/acontain.md)), so the module
harness patterns apply to them. Neither is in `orig/` yet: the copies in the installed game
(`game/Sim`, `game/Aircraft/*/model`, dated 10 August 1997) come from the CD's setup. Copy
them into `orig/` and check them against the reference CD before writing tools against them.

- **`SIM1.sim`** (base `0x22000000`, `.text` 78,216, `.data` 17,400): imports only KERNEL32
  and MSVCRT, and links to modules 1, 2, 3, 4, 10, 16, 20, 23, 32 and 39. It calls host
  `+0x21c` 77 times and reads and writes GLOBAL widely, like DYNAMIC. As large as SIMSUI and
  in every flight, so it should not wait behind the dialogs.
- **The `.MDL`s** (one per aircraft and a crash model for the C182 and the Extra): `.text`
  678 to 982 bytes for the seven normal models and 9,350 and 9,894 for the two crash ones,
  `.data` 5,800 to 96,488. Presumably a little shared code around model data for G3D;
  survey how much the code differs between files before planning, as for the gauges.

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
2. **Flights, then Options.** The largest code still used in play. Scripted menu tests come
   with them: see [open-differences.md](open-differences.md#not-yet-tested).
3. **SIM1, then the MDLs.** Simulator code in every flight, testable like the modules.
   Copy them into `orig/` first.
4. **SIMSUI.** After the dialogs, because they show which of its 540 exports matter.
5. **Survey the gauges early, do them late.** The survey may change this order.
6. **The executable around `HOST.DLL`.**
7. **Native builds.** Moving off x86 ends comparison with retail, so the earlier steps come
   first. Register contracts between modules ([open-differences.md](open-differences.md#a-class-of-bug-to-look-for))
   are x86 behaviour that a port has to remove on both sides of each call.
