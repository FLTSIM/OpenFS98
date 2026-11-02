# Remaining work

What is still retail, and what is left before a native build. The 27 simulator modules in
`Modules\` and `English\World.DLL` are done and in `MODULES`; `make install` puts them in
the game. Everything below is not. Sizes are `.text` from the reference CD. Differences
and gaps in the finished modules are in [open-differences.md](open-differences.md).
Results here are from 2026-11-01.

## Binaries

| Binary | `.text` | Status | Where |
| --- | --- | --- | --- |
| `English/Options.DLL` | 84,381 | **Not started.** Inventory only: 15 registrations, 31 templates | [options.md](modules/options.md) |
| `FLTSIM98.EXE` | 82,016 | **No executable.** All 194 services are reimplemented and verified in-process as `HOST.DLL`; retail's still runs the game | [host.md](modules/host.md) |
| `SIMSUI.DLL` | 79,850 | **Not started.** 540 named exports, the common dialog and control widgets. Every dialog imports it | [dialogs.md](modules/dialogs.md) |
| `Gauges/*.GAU` (120) | 63,472 | **Not started.** No `src/gauges/` and no survey. PANELS, their runtime, is ours | below |
| `English/Flights.DLL` | 51,122 | **Not started.** Inventory only: 13 registrations, 17 templates. Caller descriptors need recovering first | [flights.md](modules/flights.md) |
| `English/Sim.DLL` | 33,239 | **Built, not installed** | [sim.md](modules/sim.md) |
| `English/mpdialog.dll` | 25,401 | **Partial,** being worked on separately: registries `0x110a`..`0x110c` | [mpdialog.md](modules/mpdialog.md) |
| `English/Views.DLL` | 13,090 | **Built, not installed** | [views.md](modules/views.md) |
| `fltsim95.exe` | 11,464 | **Not looked at.** A stub that ships alongside; imports only KERNEL32. Probably not needed | [architecture.md](architecture.md) |
| `English/Help.DLL` | 10,657 | **Built, not installed** | [help.md](modules/help.md) |
| `English/Alerts.DLL` | 7,649 | **Built, not installed** | [alerts.md](modules/alerts.md) |
| `DIALOG.DLL`, `ENGLISH.FLL` | 406, 438 | **Resources:** bitmaps; strings, a menu and the key tables. To be carried over, not decompiled | [dialogs.md](modules/dialogs.md) |

"Built, not installed" means a standalone replacement in `bin/<module>-diff/` with its own
harness, outside `MODULES`, so `make install` does not copy it. All of them import retail
SIMSUI and MFC42.

**Third-party dependencies, not ours to decompile but to replace for a native port:**
`MFC42` (every dialog), Win32, GDI, DirectDraw, Direct3D, DirectSound, DirectInput,
DirectPlay.

## The gauges

120 files, 12,973,568 bytes, of which only 63,472 are `.text`, from 208 bytes (`737WARNG`)
to 2,902 (`LEARPFD`). The rest is not surveyed, but is presumably bitmaps and gauge tables,
so these may be far less work than their size suggests. Survey them before planning: how
much code they share, and how much is data a generator could carry over.

## Order

1. **Install the four finished dialogs** (Alerts, Help, Sim, Views): the cheapest gain, and
   they have only run alone. Add them to the game comparison in the same step.
2. **Flights, then Options.** The largest code still used in play. Scripted menu tests come
   with them: see [open-differences.md](open-differences.md#not-yet-tested).
3. **SIMSUI.** After the dialogs, because they show which of its 540 exports matter.
4. **Survey the gauges early, do them late.** The survey may change this order.
5. **The executable around `HOST.DLL`.**
6. **Native builds.** Moving off x86 ends comparison with retail, so the earlier steps come
   first. Register contracts between modules ([open-differences.md](open-differences.md#a-class-of-bug-to-look-for))
   are x86 behaviour that a port has to remove on both sides of each call.
