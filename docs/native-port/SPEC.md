---
id: SPEC-openfs98-native-port
companions:
  - decisions.md
  - future-backlog.md
  - ../retail-bugs.md
sources:
  - ../../../_bmad-output/forge/openfs98-native-port/forged-idea.md
---

> **Canonical contract.** This SPEC and the files in `companions:` are the complete, preservation-validated contract for what to build, test, and validate. Source documents listed in frontmatter are for traceability — consult them only if you need narrative rationale or prose color this contract intentionally omits.

# OpenFS98 native port

## Why

A vision to realize: FS98 running natively (no wine) on Linux, Windows x64 and Mac, with retail behaviour intact. The decompilation is complete and verified against retail under wine; wine was scaffolding. 32-bit x86 is fading (macOS, ARM), so the port goes 64-bit, while staying a platform for third-party FS98 aircraft, gauges and add-ons.

## Capabilities

- **CAP-1**
  - **intent:** Native Linux i386 build runs FS98 without wine, as a temporary step that keeps in-process comparison with retail.
  - **success:** All 74 flights frame-identical to retail, kept retail bugs included. Removed once CAP-2 matches.
- **CAP-2**
  - **intent:** Linux x86-64 build runs FS98.
  - **success:** Bit-identical to CAP-1 with the retail-bug fixes in, 74 flights frame for frame; any difference from retail is a fix listed in `retail-bugs.md`.
- **CAP-3**
  - **intent:** Windows x64 (mingw) and Intel Mac builds run FS98.
  - **success:** Same bit-identical 74-flight check as CAP-2.
- **CAP-4**
  - **intent:** Apple Silicon build flies the same as retail.
  - **success:** Within documented per-routine tolerances, plus bounded flight-path checks on the 74 flights.
- **CAP-5**
  - **intent:** Retail's dialogs look and behave exactly as in retail on every native build.
  - **success:** `main-dialogs` and `flights-play` pass on native builds; every used MFC42 ordinal (240) passes differential tests against the CD's `MFC42.DLL`.
- **CAP-6**
  - **intent:** New options and dialogs for new features look like they belong in FS98; add-ons can show them in retail through retail's MFC42/SIMSUI.
  - **success:** One new dialog, built on the decompiled MFC toolkit, opens and works in a native build and in retail FS98.
- **CAP-7**
  - **intent:** Third-party FS98 aircraft and gauges (binaries, no source) run on every native platform.
  - **success:** Retail's 120 gauge and crash-MDL binaries, run through the guest, match the decompiled natives; the 22 aircraft in `~/source/fs98stuff` (230 unique gauge and MDL binaries) load and fly with working panels, `generic.gps_dm.gau` steering the autopilot included.
- **CAP-8**
  - **intent:** Add-ons are native shared libraries in `Modules/`, loaded through retail's `Linkage`/`ImportTable`; an add-on can import an OpenFS98 extension module optionally.
  - **success:** A module that imports an unknown id gets a null slot and still loads and inits, in both the retail and the native host.
- **CAP-9**
  - **intent:** An add-on that can work with retail is a standalone GitHub repo, a git submodule of OpenFS98, built from one source as a 32-bit retail DLL and a native module.
  - **success:** One such add-on builds both ways and runs in retail FS98 and in a native build.
- **CAP-10**
  - **intent:** Keyboard, joysticks with force feedback, sound and timing work on every native platform, drawn by the software HG2D renderer.
  - **success:** A hand-flown session on each platform with joystick, haptics and sound working.
- **CAP-11**
  - **intent:** Native builds play multiplayer with each other, after the first native releases.
  - **success:** Two native builds join one session over UDP through MULTIP's DirectPlay calls.

## Constraints

- The user supplies the FS98 CD; OpenFS98 ships no Microsoft files.
- One module contract: retail `Linkage`/`ImportTable`, pointer-width fields in native builds. Native-only capabilities arrive as new module ids (an OpenFS98 extensions module), never as a second API. Our own modules may link statically through the same tables.
- No maintained 32-bit build of the port; i386 exists only as CAP-1.
- Port order is fixed: i386 → x86-64 → Windows x64 / Intel Mac → Apple Silicon. Never straight to 64-bit.
- x86-64 GCC/Clang builds are bit-identical (CAP-2); ARM and MSVC builds are the tolerance tier. No soft 80-bit float in hot paths.
- G3D's mesh compiler runs at 24-bit precision (`g3d_mesh_precision = 24`, or x87 precision control 24 where the x87 is used). Retail compiles meshes only on Direct3D, always at 24-bit (`decisions.md`).
- Machine layer is SDL3 (window, input, joysticks/haptics, audio, timers). Software HG2D first.
- UI is retail's dialogs over our own USER32 (≈140 functions) and GDI32 (≈51) subset drawn through SDL3, plus the used part of the CD's `MFC42.DLL` decompiled with the differential method. Native MFC must match retail class layouts, since decompiled code reads them by offset.
- Third-party code runs in one embedded x86-32 interpreter with a PE loader, the same on every platform. Imports thunk to native PANELS, the host and a bounded Win32 shim: the statically linked MSVC CRT's KERNEL32 set, `MSVCRT.DLL`, resources, WINMM `mmio`, `DirectSoundCreate`, `FindWindowA` (`decisions.md`). The PE loader also resolves imports between guest images and their resources.
- `GLOBAL` and all sim state stay native. The guest arena holds only gauge/MDL images and the blocks PANELS allocates for them; PANELS reads elements through guest-pointer accessors. The variable pointer PANELS `+0x98` writes into a guest MODULE_VAR points to a copy of that variable in the guest arena, synced with native `GLOBAL` before and after each guest call, because one third-party gauge reads and writes sim state through it.
- The native host keeps retail's unknown-import behaviour: null slot, module still loads.
- Kept retail bugs (`retail-bugs.md`) stay matched until CAP-1 is done, then are fixed in the i386 build; CAP-2 and CAP-3 are checked against that fixed build.
- Device support (CAP-10) is judged by hand-flying; there is no retail baseline for devices.

## Non-goals

- A new UI toolkit (Nuklear, ImGui) or a hand-written MFC lookalike.
- A 32-bit gauge-host process, or recompiling gauges ahead of time.
- Retail `GLOBAL` layout in the guest arena, or copying native structs at each crossing.
- Multiplayer in the first native releases; mixed sessions with retail FS98.
- Android (a separate future project with its own UI).
- A GPU renderer (its own later work).
- FS9 support and the backlog in `future-backlog.md`.

## Success signal

- On Linux x86-64, Windows x64 and Intel Mac, a user with only the FS98 CD installs OpenFS98, flies any of the 74 flights frame-identical to retail, uses every retail dialog, and loads a retail gauge binary and a native add-on, with no wine. Apple Silicon does the same within its documented tolerances.

## Assumptions

- The corpus survey is static. A dynamic run (each aircraft flown with its panel showing) may still turn up reaches the disassembly missed; that run is pinned for later.
