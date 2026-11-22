# Decisions and the evidence behind them

Why the locked choices in SPEC.md hold, so the rejected options are not proposed again.

## Rejected

| Option | Why rejected |
| --- | --- |
| Maintain a 32-bit build of the port | 32-bit x86 is fading (macOS, ARM); add-ons are built twice instead (CAP-9). |
| 32-bit gauge-host process | Fails on Mac and ARM. |
| Recompile gauges ahead of time | That is lifting, which the project has removed. |
| (B) Copy native structs at each crossing | Not needed: retail gauges never touch `GLOBAL`, and the one third-party gauge that does is served by the shared arena. |
| New UI (Nuklear/ImGui) | Loses retail behaviour and the differential checks. |
| Hand-written MFC lookalike | No ground truth for each function. |
| Go straight to 64-bit | Loses in-process comparison with retail during the move. |
| Module images generated from the CD at build time or setup | The November 1997 pressing rebuilt `FLTSIM98.EXE` and ten modules as different programs, so images would need per-pressing tables. Module data is decompiled into C instead (AD-12). |
| Raw `.data`/`.rdata` dumps committed | Microsoft bytes in the repo and releases. |
| Our own multiplayer wire format | Retail FS98 could not join; the DirectPlay wire protocol is published ([MC-DPL4CS]). |
| Cooperative fibers for module threads | Rejected for real OS threads (AD-9). |
| Keeping any retail bug as FS98 behaviour | Every bug is fixed after CAP-1; a player toggle is backlog work. |

Option (A), retail `GLOBAL` layout in the guest arena, was rejected at the forge and adopted by the architecture (AD-1): one arena serves native modules and the guest alike.

## Measurements

- **Retail gauge memory reach (2026-11-15).** 120 retail gauges: 4,076 absolute memory operands, 0 outside each gauge's own image. Dereferenced pointers are the element (137), the mouse point (9), or a PANELS-made structure stored in the image (6). PANELS evaluates variables through its 38-function table and writes the value into the element (+0xc) before calling the gauge. `tools/difftest/allgaugesdiff.c` verifies all 1,317 entries with no `GLOBAL`.
- **Retail gauge Win32 imports.** `DisableThreadLibraryCalls`, `_ftol` (41), `wsprintfA` (36), `lstrcpyA` (1). Everything else goes through the PANELS/`GLOBAL` linkage. This sizes the guest's Win32 shim.
- **UI surface** (retail EXE, English DLLs, SIMSUI, modules; distinct imports): USER32 140, GDI32 51, MFC42 240 ordinals (SIMSUI 189, World 105, Options 102), COMCTL32 1, IMM32 5.
- **MFC42.DLL** (`OpenFS98/cd/mfc42.dll`): 941,840 bytes, `.text` 615 KB, 214,046 instructions. Reachable from the 240 ordinals: ~13,300 instructions by direct calls, ~18,300 with vtables; message-map dispatch is undercounted, so 20–40k is the estimate. For scale: SIM1 was 17,836, host runtime 13,081.
- **G3D mesh precision (2026-11-15).** The mesh compiler (`0x206e35b0`) runs only when GLOBAL `+0x12a4` bit 4 is set, which only HG2D's Direct3D device scene does; the software renderer never compiles meshes. A probe at mesh entry in a scratch copy of our G3D, all-ours game, Direct3D `640x480x16`, 500 passes: PARIS, HONGKONG, NEWYORK, KAUAI and STGEORGE read control word `0x007f` (24-bit) at every compile; CVINSON compiled none; all six frame-identical to retail. HG2D's batch init (slice 46) also leaves 24 bits. `g3d_mesh.c`'s default, 53 until then, is now 24.
- **Third-party corpus (2026-11-15).** `~/source/fs98stuff`: 22 aircraft zips, 230 unique binaries (22 MDLs, 208 gauges), all exporting `Linkage`. Static survey:
  - 106 plain absolute operands fall outside their own image. None lands in any retail module or the EXE (`GLOBAL.DLL` included); all are data decoded as code.
  - No gauge calls `+0x9c` (named variables), and none dereferences an element's variable pointer. Retail's 120 gauges give 0 on the same checks.
  - **One exception:** `ultim757/generic.gps_dm.gau` dereferences MODULE_VAR `+4`, the pointer PANELS `+0x98` fills. It writes var `0x6b` (GLOBAL `+0x7cc`, autopilot heading target, 16-bit) and reads var `0x6a` (`+0x7c8`, heading hold). The GPS steers the autopilot through it.
  - **Win32 imports:** 143 `DisableThreadLibraryCalls`, 59 `wsprintfA`, 57 `_ftol`; 47 with a statically linked MSVC CRT (~60 KERNEL32 functions: heap, TLS, critical sections, environment, files, `LoadLibraryA`/`GetProcAddress`); 105 import `MSVCRT.DLL`; resource APIs (20); WINMM `mmio*` (2); `DirectSoundCreate` (2); `FindWindowA` (2); `CreateFileA`/`ReadFile` (1). `abrakedis_v2.gau` imports `autobrakes`/`ab_in_use` from `Autobrake_V2.GAU`.
  - **MDLs:** all PE DLLs (module 31) with ~1 KB code, like retail's normal models; none carries crash-MDL-sized code (~9 KB). 21 share one 968-byte converter stub (`MDL610.MOD`, 71,680 bytes each) that loads the model from `.rsrc` through `FindResourceA`/`LoadResource`/`LockResource`. `beavmp.mdl` has retail `CESSNA182S_N.MDL`'s layout.
  - `PNW.ISO` is data only (13 BGL, ~11,800 R8 textures, 9 flights, installers): a scenery and texture test, not a guest test.
- **Unknown import id in the retail host** (`src/host/runtime/host_rt_modules.c`, 0x407ac0/0x407fe0): `module_with_id` finds nothing, acquire returns 0, the slot stays 0, the module still loads and inits. An add-on can list an OpenFS98 extension module and test for null with no `#ifdef` in the retail build.
- **DirectPlay protocol scope (2026-11-15).** [MC-DPL4CS] documents dialects DX6VERSION (9) to DX9VERSION (14); `IDirectPlay3`/`4` interfaces do not change the wire. Retail FS98 on a DirectX 6+ runtime speaks a documented dialect; DirectX 5-only peers are undocumented. Port 47624 (UDP broadcast, TCP to a named host), sessions TCP/UDP 2300–2400. DirectPlay is still an optional Windows 10/11 feature.
- **Module threads (2026-11-15).** MULTIP (send and receive), BOOSTER and UTIL (trace) call `CreateThread`.

