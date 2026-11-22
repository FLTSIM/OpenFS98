---
id: SPEC-openfs98-native-port
companions:
  - decisions.md
  - future-backlog.md
  - ../retail-bugs.md
  - architecture/ARCHITECTURE-SPINE.md
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
  - **intent:** Windows x64 (mingw-w64) and Intel Mac builds run FS98.
  - **success:** Windows x64 passes the same bit-identical 74-flight check as CAP-2. Intel Mac builds in CI, passes the CD-free checks (unit tests, per-routine tolerances), and ships in every release.
- **CAP-4**
  - **intent:** Apple Silicon build flies the same as retail, and Mac users can download it from each release.
  - **success:** Builds in CI and ships in every release as a downloadable app; stays within documented per-routine tolerances and passes the guest interpreter suite, all without the CD. Flight-path checks come from a user-run script, added only if Mac users report real issues.
- **CAP-5**
  - **intent:** Retail's dialogs look and behave exactly as in retail on every native build.
  - **success:** `main-dialogs` and `flights-play` pass on native builds; every used MFC42 ordinal (240) passes differential tests against the CD's `MFC42.DLL`.
- **CAP-6**
  - **intent:** New options and dialogs for new features look like they belong in FS98; add-ons can show them in retail through retail's MFC42/SIMSUI.
  - **success:** The EFB add-on's settings dialog (CAP-9), built on the decompiled MFC toolkit, opens and works in a native build and in retail FS98.
- **CAP-7**
  - **intent:** Third-party FS98 aircraft and gauges (binaries, no source) run on every native platform.
  - **success:** Retail's 120 gauge and crash-MDL binaries, run through the guest, match the decompiled natives; the 22 aircraft in `~/source/fs98stuff` (230 unique gauge and MDL binaries) load and fly with working panels, `generic.gps_dm.gau` steering the autopilot included.
- **CAP-8**
  - **intent:** Add-ons are native shared libraries in `Modules/`, loaded through retail's `Linkage`/`ImportTable`; an add-on can import an OpenFS98 extension module optionally.
  - **success:** A module that imports an unknown id gets a null slot and still loads and inits, in both the retail and the native host.
- **CAP-9**
  - **intent:** An add-on that can work with retail is a standalone GitHub repo, a git submodule of OpenFS98, built from one source as a 32-bit retail DLL and a native module.
  - **success:** The EFB data-output add-on builds both ways and, in retail FS98 and in a native build, streams GDL 90 over UDP that Garmin Pilot shows as the aircraft moving.
- **CAP-10**
  - **intent:** Keyboard, joysticks with force feedback, sound and timing work on every native platform, drawn by the software HG2D renderer.
  - **success:** A hand-flown session on Linux and Windows with joystick, haptics and sound working. On Mac, users' hand-flown reports are tracked as issues.
- **CAP-11**
  - **intent:** Native builds play LAN multiplayer with each other and with retail FS98, after the first native releases.
  - **success:** Two native builds share a LAN session, and a native build shares one with retail FS98 on Windows DirectPlay (DirectX 6+ runtime), each side hosting in turn.
- **CAP-12**
  - **intent:** On first launch, the user points OpenFS98 at their FS98 CD (an ISO 9660 or BIN/CUE image, a mounted folder, or a drive) in a setup dialog that looks like FS98's own, and the data is installed.
  - **success:** Starting from a fresh release zip plus the CD image of any catalogued pressing (the November 1997 one included), setup completes and a flight starts. The data goes beside the binaries when that folder is writable, else into the user root.

## Constraints

- The user supplies the FS98 CD. Neither releases nor the git repo contain Microsoft files, raw dumps or retail resources. Module `.data`/`.rdata` is decompiled into C initialisers like the code; resources and game files are read at runtime from what setup copies off the CD.
- One memory model: a 32-bit arena per process holds module images, `GLOBAL` in retail's layout, the FS98 heap and guest images. Addresses are 32-bit offsets and service slots hold code handles. Native add-ons use a typed SDK, never raw arena offsets.
- One module contract: retail `Linkage`/`ImportTable`, 32-bit in every build. Native-only capabilities arrive as new module ids (an OpenFS98 extensions module), never as a second API. Our own modules may link statically through the same tables.
- No maintained 32-bit build of the port; i386 exists only as CAP-1.
- Port order is fixed: i386 → x86-64 → Windows x64 / Intel Mac → Apple Silicon. Never straight to 64-bit.
- Tier 1 is GCC on Linux (i386, x86-64) and mingw-w64 on Windows x64, with all FS98 math on the x87; those builds are bit-identical. Intel Mac (Apple Clang), ARM and MSVC builds are the tolerance tier. No soft 80-bit float in native hot paths; the guest may use it on non-x86 hosts.
- G3D's mesh compiler runs at 24-bit precision (`g3d_mesh_precision = 24`, or x87 precision control 24 where the x87 is used). Retail compiles meshes only on Direct3D, always at 24-bit (`decisions.md`).
- Machine layer is SDL3 (window, input, joysticks/haptics, audio, timers). Software HG2D first.
- UI is retail's dialogs over our own USER32 (≈140 functions) and GDI32 (≈51) subset drawn through SDL3, plus the used part of the CD's `MFC42.DLL` decompiled with the differential method. Native MFC must match retail class layouts, since decompiled code reads them by offset.
- Third-party code runs in one embedded x86-32 interpreter with a PE loader, the same on every platform. Imports thunk to native PANELS, the host and a bounded Win32 shim: the statically linked MSVC CRT's KERNEL32 set, `MSVCRT.DLL`, resources, WINMM `mmio`, `DirectSoundCreate`, `FindWindowA` (`decisions.md`). The PE loader also resolves imports between guest images and their resources.
- The guest shares the arena. The pointer PANELS `+0x98` writes into a guest MODULE_VAR points straight into `GLOBAL`, as retail does; one third-party gauge writes sim state through it.
- The native host keeps retail's unknown-import behaviour: null slot, module still loads.
- Kept retail bugs (`retail-bugs.md`) stay matched until CAP-1 is done, then every one is fixed in the i386 build, none kept as behaviour; CAP-2 and CAP-3 are checked against that fixed build.
- Mac builds are built, released and checked in CI only (GitHub Actions); there is no local Mac.
- Multiplayer speaks the published DirectPlay wire protocol ([MC-DPL4CS], TCP/IP service provider) and keeps MULTIP's message formats, so retail FS98 can join.
- Device support (CAP-10) is judged by hand-flying; there is no retail baseline for devices.

## Non-goals

- A new UI toolkit (Nuklear, ImGui) or a hand-written MFC lookalike.
- A 32-bit gauge-host process, or recompiling gauges ahead of time.
- Copying native structs at each guest crossing.
- Multiplayer in the first native releases; internet play (NAT traversal, lobbies); DirectPlay peers on DirectX 5-only runtimes.
- Android (a separate future project with its own UI).
- A GPU renderer (its own later work).
- FS9 support and the backlog in `future-backlog.md`.

## Success signal

- **Release 1:** on Linux x86-64 and Windows x64, a user with only the FS98 CD installs OpenFS98 through first-run setup, flies any of the 74 flights frame-identical to retail, and uses every retail dialog, with no wine.
- **Later releases:** the same user also loads a retail gauge binary, a third-party aircraft and a native add-on. Mac users download a release app for Intel or Apple Silicon (in every release once the Mac targets exist), which installs from the CD the same way and stays within its documented tolerances.

## Assumptions

- The corpus survey is static. A dynamic run (each aircraft flown with its panel showing) may still turn up reaches the disassembly missed; it is planned as epics Story 7.7.
- Retail FS98 on a DirectX 6+ runtime interoperates with a DX9VERSION-dialect peer; unverified until a capture (epics Story 10.1).
- The November 1997 pressing's game files and resources are compatible with the reference; unverified until it is catalogued (epics Story 6.1).
