---
status: final
stepsCompleted: [1, 2, 3, 4]
inputDocuments:
  - docs/native-port/SPEC.md
  - docs/native-port/architecture/ARCHITECTURE-SPINE.md
  - docs/native-port/decisions.md
  - docs/retail-bugs.md
---

# OpenFS98 native port - Epic Breakdown

## Overview

The epic and story breakdown for the OpenFS98 native port. Requirements come from `SPEC.md` (in place of a PRD) and the architecture spine. There is no UX document: the UI is retail's dialogs exactly, and new dialogs follow FS98's own style.

## Requirements Inventory

### Functional Requirements

FR1 (CAP-1): A native Linux i386 build runs FS98 without wine; all 74 flights are frame-identical to the retail references, kept retail bugs included.
FR2 (CAP-2): A Linux x86-64 build is bit-identical, 74 flights frame for frame, to the i386 build with the retail-bug fixes in.
FR3 (CAP-3): A Windows x64 (mingw-w64) build passes FR2's check; an Intel Mac build builds and passes the CD-free tolerance checks in CI.
FR4 (CAP-4): An Apple Silicon build builds and stays within documented per-routine tolerances in CI. Flight-path checks on the 74 flights are added only if Mac users report real issues.
FR5 (CAP-5): Retail's dialogs look and behave as in retail on every native build. `main-dialogs` and `flights-play` pass natively, and all 240 used MFC42 ordinals pass differential tests against the CD's `MFC42.DLL`.
FR6 (CAP-6): New options and dialogs look like FS98's. The EFB add-on's settings dialog, built on `fs98mfc`, opens and works natively and in retail FS98 (through retail MFC42/SIMSUI).
FR7 (CAP-7): Third-party FS98 aircraft and gauge binaries run on every native platform. Retail's 120 gauge and crash-MDL binaries in the guest match the decompiled natives. The 22 aircraft in `~/source/fs98stuff` load and fly with working panels, including `generic.gps_dm.gau` steering the autopilot.
FR8 (CAP-8): Native add-on shared libraries in `Modules/` load through `Linkage`/`ImportTable`. An unknown import id gets a null slot and the module still loads and inits, in the retail and the native host.
FR9 (CAP-9): The EFB data-output add-on lives in its own GitHub repo, is a git submodule of OpenFS98, and builds from one source both as a 32-bit retail DLL and as a native module; both stream GDL 90 that Garmin Pilot shows.
FR10 (CAP-10): Keyboard, joysticks with force feedback, sound and timing work on every native platform with the software HG2D renderer, shown by a hand-flown session on Linux and Windows; Mac users' reports are tracked as issues.
FR11 (CAP-11): Native builds join LAN multiplayer sessions with each other and with retail FS98, through a DirectPlay-compatible wire protocol behind MULTIP's calls (after the first native releases).
FR12 (CAP-12): First-run setup takes the user's FS98 CD (any catalogued pressing; an ISO 9660 or BIN/CUE image, a mounted folder or a drive) in an FS98-style dialog and installs the data, beside the binaries when writable, else in the user root. From a fresh release zip plus a CD image, setup completes and a flight starts.

### NonFunctional Requirements

NFR1: Neither releases nor the git repo contain Microsoft files, raw dumps or retail resources. Module data is decompiled into C initialisers like the code (AD-12); resources are read at runtime from the CD copies setup puts in `Data/Retail/`.
NFR2: One memory model: a 32-bit arena per process with GLOBAL in retail's layout, 32-bit offsets and code handles. Native add-ons use a typed SDK.
NFR3: One module contract (`Linkage`/`ImportTable`, 32-bit on every build). Native-only features arrive as new module ids, never as a second API.
NFR4: No maintained 32-bit port; i386 exists only as FR1 and is removed once FR2 matches.
NFR5: Fixed port order: i386, then x86-64, then Windows x64 and Intel Mac, then Apple Silicon.
NFR6: Tier 1 (GCC on Linux, mingw-w64 on Windows x64, all FS98 math on the x87) is bit-identical. Intel Mac, ARM and MSVC are the tolerance tier. No soft 80-bit float in hot paths.
NFR7: G3D's mesh compiler runs at 24-bit precision.
NFR8: SDL3 is the machine layer; the software HG2D renderer comes first.
NFR9: The UI is retail's dialogs over our USER32 (about 140 functions) and GDI32 (about 51) subset plus the decompiled used part of MFC42, matching retail class layouts.
NFR10: Third-party code runs in one embedded x86-32 interpreter with a PE loader, the same on every platform, behind a bounded Win32 shim (`decisions.md`).
NFR11: The native host keeps retail's unknown-import behaviour: null slot, module still loads.
NFR12: Kept retail bugs stay matched until FR1 passes, then are fixed in the i386 build; FR2 and FR3 are checked against that fixed build.
NFR13: Device support is judged by hand-flying.
NFR14: Non-goals: a new UI toolkit or a hand-written MFC lookalike; a 32-bit gauge-host process or recompiled gauges; copying structs at each guest crossing; multiplayer in the first releases; Android; a GPU renderer; FS9 support.

### Additional Requirements

From the architecture spine (`architecture/ARCHITECTURE-SPINE.md`). The AD numbers are binding.

- AR1 (AD-1, AD-14, AD-15, AD-22): `fs98arena`, its fixed region map and its single deterministic allocator. `include/fs98/arena.h` holds the only address macros, with a lint check against stray casts and private copies. 64-bit test builds use a randomised nonzero arena base. ABI headers are 32-bit with `_Static_assert` layout checks; GLOBAL is 4,776 bytes.
- AR2 (AD-17): code handles, each with a registered signature, through `fs98_call*`. The per-module `CALL()` macros are replaced.
- AR3 (AD-16): the arena stack (`FS98_FRAME_*`, shared with the guest's `esp`) and `fs98_str`.
- AR4 (AD-3, AD-2, AD-13): native modules export an `Fs98Image` descriptor. `LoadLibraryA` is the only owner of module images: fresh image per load, relocations, no constructors. The host scans in sorted order, reads ids before any code runs, and uses the 40–63 range for OpenFS98 modules.
- AR5 (AD-4): explicit parameters instead of register contracts on every native target, including ANALYSIS's `memmove` value of 7. `keep.h` and the `.S` stubs only under `FS98_RETAIL_ABI`. `FS98_STDCALL` defined once in `abi.h`.
- AR6 (AD-18): the `divide.h` saturating helpers replace every faulting divide, including the private copies in SIM1, HG2D and BOOSTER. The native host aborts on a divide trap.
- AR7 (AD-19): one arena SEH chain; the seven private `*_unwind.h` files are replaced; nothing touches `%fs`.
- AR8 (AD-20): FP flags in the toolchain files (`-mfpmath=387 -ffp-contract=off -fexcess-precision=standard` for tier 1); one control word, set by the host to retail's main-loop value (measure it first); the guest control word synced at transitions; control word and MXCSR saved around SDL and system calls.
- AR9 (AD-5, AD-6, AD-23, AD-24): `fs98win32` with our own `windows.h` and `fs98_`-prefixed exports. It provides the MSVCRT-behaviour CRT (with a libc import check) and DDRAW (2D only), DSOUND, DPLAY, WINMM, MSACM32, ADVAPI32, SHELL32, COMCTL32 and IMM32, with COM objects in the arena.
- AR10 (AD-21): one borderless SDL window per top-level `HWND`, Win98 chrome drawn by us, a virtual screen (640×480 under FRAMELOCK), SDL events consumed only in message retrieval, and the sim thread as the main thread.
- AR11 (AD-9): one clock; timers and callbacks run on the sim thread at the main loop's clock read; DirectSound is mixed into a ring buffer per pass; MULTIP's queues are drained at the start of a pass.
- AR12 (AD-10): DOS paths only (`C:\FLTSIM98\`), with a read-only data root under a writable user root, case-insensitive lookup, sorted listings, CP1252 names and a REGEDIT4 registry file.
- AR13 (AD-7): format-dispatching `LoadLibraryA` (native image, guest PE, or resource-only PE). Missing guest imports log the name and fault only if called. `HMODULE` values and module-name matching follow Win98.
- AR14 (AD-25, AD-26): `fs98mfc` in C with arena objects and handle vtables, named by ordinal. `dialog.h` gives add-ons one dialog API. The resource reader serves `Data/Retail` files, `fs98_resources` blobs and PE `.rsrc`.
- AR15 (AD-8, AD-27): retail reference logs recorded once under wine (outside git, manifest committed). FRAMELOCK lives in `fs98win32` with a frozen log grammar and per-window comparison. Dialog drivers run inside the process. The guest-versus-native gauge check and a per-target 74-flight report.
- AR16 (AD-11): CMake replaces the Makefile at parity. Toolchain files for `win32-retail`, `linux-i386`, `linux-x86_64`, `windows-x64`, `macos-x86_64` and `macos-arm64`. One `OpenFS98::module` flag set. SDL3 3.4.x and libcdio 2.3.0 from checksum-pinned tarballs.
- AR17 (AD-12): no Microsoft bytes in the repo. The 26 committed `*_data.inc` files and the generated resources become build outputs.
- AR18 (`retail-bugs.md`): mark all register entries in the code as `RETAIL BUG (kept): <id>`; today there are 57 markers for 289 entries. Then fix them after FR1.
- AR19 (`decisions.md`): the dynamic flight pass over the 22 third-party aircraft, which is still pending.

### UX Design Requirements

None: no UX document. The UI rule is retail's dialogs exactly (FR5). New dialogs, setup (FR12) and options (FR6), follow retail FS98 style through `fs98mfc`.

### FR Coverage Map

FR1: Epic 3 - native Linux i386 matches the retail references
FR2: Epic 5 - Linux x86-64 bit-identical to the fixed i386 build
FR3: Epic 5 (Windows x64), Epic 9 (Intel Mac, CD-free checks in CI)
FR4: Epic 9 - Apple Silicon, CD-free tolerance checks in CI
FR5: Epic 2 (MFC42 under wine), Epic 3 (native dialogs)
FR6: Epic 8 - new FS98-style dialog, native and retail
FR7: Epic 7 - third-party aircraft and gauges in the guest
FR8: Epic 8 - native add-on modules, unknown-import rule
FR9: Epic 8 - dual-built add-on repo and submodule
FR10: Epic 3 (Linux), Epic 5 (Windows), Epic 9 (Mac)
FR11: Epic 10 - DirectPlay-compatible LAN multiplayer
FR12: Epic 6 - first-run setup from a CD image

## Epic List

### Epic 1: The wine build follows the port rules
The retail-ABI build adopts the shared port rules while all 74 flights still match retail under wine: CMake at parity, 32-bit ABI headers and the single `arena.h`, explicit register parameters, divide helpers, FP flags, the CRT rule, every retail bug marked, no Microsoft bytes in git, and the retail reference logs recorded.
**FRs covered:** (enables FR1–FR4) · AR1, AR5, AR6, AR8 (flags), AR15 (references), AR16, AR17, AR18 (marking)

### Epic 2: Our own MFC42
The used part of the CD's MFC42 (240 ordinals) decompiled into `fs98mfc`, verified against the CD's DLL, and installed under wine, where every English dialog runs on it.
**FRs covered:** FR5 (retail half) · AR14

### Epic 3: FS98 flies on Linux with no wine (i386)
Arena, code handles, arena stack, the `Fs98Image` loader, `fs98win32`, `fs98sdl`, the native host and built-in FRAMELOCK. All 74 flights match the retail references, dialogs pass natively, and joystick and sound work on Linux.
**FRs covered:** FR1, FR5 (native), FR10 (Linux) · AR2–AR4, AR7, AR9–AR13, AR15

### Epic 4: Retail bugs fixed
The register's fixes go into the i386 build, and that build becomes the new reference.
**FRs covered:** (NFR12) · AR18 (fixes)

### Epic 5: Linux x86-64 and Windows x64, bit-identical
Both 64-bit builds match the fixed i386 build on all 74 flights, devices work on Windows, and i386 is retired.
**FRs covered:** FR2, FR3 (Windows), FR10 (Windows)

### Epic 6: Install from a CD image, and Release 1
Setup dialog, the native dialog API, libcdio, `Data/Retail/`, the user root, and the Release 1 archives: a fresh zip plus an ISO reaches a flight. Third-party binaries that need the guest come in a later release.
**FRs covered:** FR12 · NFR1 (runtime)

### Epic 7: Third-party aircraft and gauges
`fs98guest` runs 1998 binaries: retail gauges match their natives, the 22 aircraft fly, and the GPS gauge steers the autopilot.
**FRs covered:** FR7 · AR19

### Epic 8: Add-ons and new dialogs
The typed SDK, OpenFS98 module ids, `dialog.h`, UDP sockets, and the EFB data-output add-on (GDL 90) in its own repo, built both ways, with its FS98-style settings dialog.
**FRs covered:** FR6, FR8, FR9

### Epic 9: macOS, Intel and Apple Silicon
Both Mac targets build in GitHub Actions, pass the CD-free checks (unit tests, per-routine tolerances, the guest interpreter) and ship as release apps.
**FRs covered:** FR3 (Intel Mac), FR4, FR10 (Mac)

### Epic 10: Multiplayer, DirectPlay-compatible
`fs98win32`'s DPLAY speaks the published DirectPlay 4 wire protocol, so native builds share LAN sessions with each other and with retail FS98.
**FRs covered:** FR11

## Epic 1: The wine build follows the port rules

The retail-ABI build adopts the shared port rules while all 74 flights still match retail under wine. Covers AR1, AR5, AR6, AR8 (flags), AR15 (references), AR16, AR17 and AR18 (marking). Every story is checked with today's tools: the 74 frame-identical flights, the difftest baselines in `docs/tools.md`, and `main-dialogs`/`flights-play`.

### Story 1.1: CMake builds everything the Makefile builds

As the developer,
I want the `win32-retail` CMake target to build every module, English DLL, SIMSUI, SIM1, the gauges, the MDLs, `OPENFS98.EXE` and the difftests,
So that the port has one build system to grow.

**Acceptance Criteria:**

**Given** a clean tree and `orig/`
**When** I configure with `cmake/toolchains/win32-retail.cmake` and build
**Then** every binary the Makefile's `all` builds is produced
**And** each one matches the Makefile's output byte for byte, or its differences are listed with their reason (paths, timestamps)
**And** `install` and `revert` work as the Makefile's do, copying to a temporary name and then `mv`

### Story 1.2: CMake runs every verification target, and the Makefile is retired

As the developer,
I want every difftest, break suite, smoke, frames and dialog target runnable through CMake,
So that the Makefile can be deleted.

**Acceptance Criteria:**

**Given** Story 1.1
**When** I run each target listed in `docs/tools.md` through CMake
**Then** each meets its recorded baseline
**And** `docs/tools.md`, `docs/building.md` and `AGENTS.md` describe the CMake commands
**And** the `Makefile` and `tools/*.mk` are removed

### Story 1.3: One flag set for every unit

As the developer,
I want an `OpenFS98::module` interface target carrying `-fwrapv -fno-strict-aliasing -fno-delete-null-pointer-checks -mfpmath=387 -ffp-contract=off -fexcess-precision=standard`,
So that no unit behaves differently because of its flags.

**Acceptance Criteria:**

**Given** Story 1.2
**When** every module links the target and the per-rule flags are removed
**Then** all difftests meet their baselines and all 74 flights stay frame-identical
**And** any remaining per-module override is listed in the toolchain file with its reason

### Story 1.4: Module data decompiled, no raw dumps in git

As the developer,
I want every module's `.data`/`.rdata` initial image (the 26 committed `*_data.inc` files and the images generated from `orig/` at build time) decompiled into C initialisers: typed tables, constants and strings (dispatchable module by module),
So that the repo holds only decompiled work, and no module image depends on a CD pressing (AD-12).

**Acceptance Criteria:**

**Given** Story 1.1
**When** each module builds from its initialisers
**Then** its compiled initial image matches retail's byte for byte, and its difftests and all 74 flights still pass
**And** a check fails on any tracked raw dump: an untyped byte or dword array initialiser above a threshold set in the check, or a binary file copied from `orig/`
**And** only resources still come from `orig/` at build time, for `win32-retail`; a build without `orig/` stops with a message pointing to `docs/building.md`

### Story 1.5: Every kept retail bug is marked in the code

As the developer,
I want each of the 289 register entries marked `RETAIL BUG (kept): <id>` at its code, and each entry checked against that code,
So that the fixing in Epic 4 starts from a complete map.

**Acceptance Criteria:**

**Given** `docs/retail-bugs.md`
**When** I run the new consistency check
**Then** every id appears in `src/` at least once, and every marker in `src/` names an id in the register
**And** entries that no longer apply are marked as such in the register

### Story 1.6: Retail reference logs recorded

As the developer,
I want retail recorded once under wine (the 74 flights plus the scripted flights) with a committed manifest,
So that native builds have a fixed baseline.

**Acceptance Criteria:**

**Given** retail's game under wine
**When** I run the recording tool
**Then** the frame logs land under `bin/references/`
**And** a manifest in git lists each flight's commit, CD pressing, config and log checksum
**And** recording twice gives identical checksums; any flight where it doesn't is listed as nondeterministic
**And** one deterministic flight is also recorded with a GLOBAL dump each pass, for Story 3.6's headless smoke run

### Story 1.7: The module ABI headers are 32-bit

As the developer,
I want `include/fs98/` (`abi.h`, `linkage.h`, `host.h`, `global.h`) to use `fs98_addr` and `fs98_code`, with `_Static_assert` layout checks,
So that every build sees retail's layouts.

**Acceptance Criteria:**

**Given** the current headers
**When** I convert them
**Then** no type describing FS98 memory uses a C pointer, `long`, `size_t`, `wchar_t`, a bitfield or `long double`
**And** the size and offset asserts pass, including GLOBAL at 4,776 bytes
**And** `FS98_STDCALL` is defined only in `abi.h`
**And** all difftests and flights still pass

### Story 1.8: Every module uses the shared `arena.h`

As the developer,
I want `include/fs98/arena.h` to hold the only definitions of `M8`…`MD`, `R8`…`RDB`, `DA`, `D`, `RDATA`, `fs98_ptr`, `fs98_handle` and `fs98_call`, with base 0 on i386,
So that the 64-bit move later changes one header.

**Acceptance Criteria:**

**Given** Story 1.7
**When** each module drops its private macro copies (dispatchable module by module)
**Then** the lint check passes: no `(uint32_t)(uintptr_t)` cast or macro definition outside `src/platform/arena/`
**And** every module's difftest and all 74 flights still pass

### Story 1.9: Register contracts become explicit parameters

As the developer,
I want each value retail carries in a register across a call to be an explicit parameter or result, with the `keep.h` wrappers and `.S` stubs only under `FS98_RETAIL_ABI`,
So that no native build depends on registers.

**Acceptance Criteria:**

**Given** the 14 `keep.h` entries, the callers that read registers, and ANALYSIS's use of the `memmove` 7
**When** I convert them
**Then** `tools/reg_contracts.sh` passes in the retail build
**And** the C bodies take explicit values
**And** all difftests and flights pass

### Story 1.10: Divides go through the saturating helpers

As the developer,
I want every retail division that can fault to call the `include/fs98/divide.h` helpers, which return what host filter `0x4119a0` writes,
So that no code relies on a divide trap.

**Acceptance Criteria:**

**Given** the private copies (SIM1 `sim1_idiv32`, HG2D's inline saturation, BOOSTER's `idivl`/`divl`)
**When** I replace them
**Then** a helper test checks each helper against the host filter's results for zero and overflow divisors
**And** no inline `div` or `idiv` asm remains in `src/`
**And** all difftests and flights pass

## Epic 2: Our own MFC42

The used part of the CD's MFC42 (240 ordinals, about 20–40k instructions) decompiled into `fs98mfc` and installed under wine, where every English dialog runs on it. Covers FR5 (retail half) and AR14 (AD-25, AD-26 resources). Same method as the other modules: a forwarder first, then slices that each replace their part, checked by a difftest and a break suite.

### Story 2.1: An `MFC42.DLL` that forwards to the CD's

As the developer,
I want CMake to build an `fs98mfc` `MFC42.DLL` that exports all 240 used ordinals and forwards each to the CD's DLL,
So that decompiled slices can replace ordinals one at a time.

**Acceptance Criteria:**

**Given** Epic 1
**When** I install the forwarder under wine
**Then** `main-dialogs` and `flights-play` pass, and all 74 flights stay frame-identical
**And** `docs/modules/mfc42.md` lists each ordinal with its name, its callers (SIMSUI 189, World 105, Options 102, Flights 80, Views 78, Sim 76, mpdialog 72, Alerts 70, Help 68, the EXE 43) and the functions it reaches, including through vtables and message maps
**And** the CD DLL's data and resources are generated at build time from `cd/mfc42.dll`, never committed (NFR1)

### Story 2.2: The MFC difftest and the retail layouts FS98 depends on

As the developer,
I want a difftest harness that runs an ordinal in both the CD's DLL and ours on the same arena state, plus `_Static_assert` checks for every MFC layout FS98's own code depends on,
So that every later slice has ground truth.

**Acceptance Criteria:**

**Given** Story 2.1
**When** I run the harness on a few forwarded ordinals
**Then** it reports 0 differences, and a deliberate break is caught
**And** the size asserts cover the classes FS98 extends or embeds: `CWnd` and the `CButton`/`CStatic`/`CEdit`/`CComboBox` wrappers at 0x40 (SIMSUI's subclasses start at +0x40), `CDialog` at 0x60, `CGdiObject` and its subclasses at 8, `CString` at 4, `CException` at 0xc, and `CWinApp` (the host's application subclasses it)
**And** the field asserts cover the fields FS98 reads or writes directly: `CWnd::m_hWnd` +0x20, `CGdiObject::m_hObject` +4, `CDC::m_hDC` +4, `CPtrArray` data +4 and size +8, and `AFX_MODULE_STATE` +0x1c/+0x20/+0x28/+0x2c (moved by the Options and Flights `DllMain`)
**And** the vtable slot layouts match retail for `CWnd`, `CDialog`, the control wrappers, `CDC` (+0x70), `CFile` (+0x14, +0x30) and `CWinApp` (+0x70), which SIMSUI's classes and the host override or call by slot
**And** `CRuntimeClass`, `AFX_CLASSINIT` and the message-map structures match retail, since module data defines them
**And** MFC throws produce MSVC C++ exceptions (code `0xe06d7363`, ThrowInfo and CatchableType records naming `.PAVCException@@`), since World catches `CException*` by walking them
**And** `fs98mfc` objects use arena addresses, an explicit `this` and handle vtables (AD-25)

### Story 2.3: CString, memory and exceptions

As the developer,
I want the `CString`, `operator new`/`delete`, `CException`/`AfxThrow*` and collection ordinals decompiled,
So that the base everything else uses is ours.

**Acceptance Criteria:**

**Given** Story 2.2
**When** the slice stops forwarding
**Then** each ordinal in the slice passes its difftest, and the break suite catches its breaks
**And** World's exception catches still work, and MFC throws dispatch through the one SEH chain (AD-19)

### Story 2.4: Module, thread and app state

As the developer,
I want `AfxGetModuleState`, the thread state, `CWinApp` startup, `AfxWinInit` and the DLL-init path decompiled,
So that our DLL initialises itself.

**Acceptance Criteria:**

**Given** Story 2.3
**When** the slice stops forwarding
**Then** it passes its difftests and break suite
**And** the game starts under wine, and the Options and Flights extension-DLL init still works

### Story 2.5: CWnd, the handle maps and message dispatch

As the developer,
I want `CWnd` create/attach/subclass, the permanent and temporary handle maps, `AfxWndProc` and message-map dispatch decompiled,
So that windows route messages through our code.

**Acceptance Criteria:**

**Given** Story 2.4
**When** the slice stops forwarding
**Then** it passes its difftests and break suite
**And** `main-dialogs` passes

### Story 2.6: CDialog, DDX/DDV and the control classes

As the developer,
I want `CDialog` (modal and modeless), `DDX_*`/`DDV_*`, and the button, edit, combo, list, scroll-bar and spin wrappers decompiled,
So that FS98's dialogs run on our code.

**Acceptance Criteria:**

**Given** Story 2.5
**When** the slice stops forwarding
**Then** it passes its difftests and break suite
**And** `main-dialogs` and `flights-play` pass

### Story 2.7: CDC, the GDI wrappers and CFile

As the developer,
I want `CDC`/`CPaintDC`/`CClientDC`, the `CGdiObject` family (`CBitmap`, `CFont`, `CBrush`, `CPen`, `CPalette`) and `CFile` decompiled,
So that SIMSUI's owner-drawn controls and its DIB loading run on our code.

**Acceptance Criteria:**

**Given** Story 2.6
**When** the slice stops forwarding
**Then** it passes its difftests and break suite
**And** `main-dialogs` opens all 23 Options dialogs and pages identically

### Story 2.8: The rest, and the CD DLL is no longer loaded

As the developer,
I want the remaining ordinals decompiled and the forwarding removed,
So that FS98 under wine runs on `fs98mfc` alone.

**Acceptance Criteria:**

**Given** Story 2.7
**When** I install our DLL
**Then** all 240 ordinals pass their difftests (FR5, retail half), and the CD's `MFC42.DLL` isn't loaded (checked in the loader log)
**And** `main-dialogs`, `flights-play` and all 74 flights pass
**And** `revert` restores the CD's DLL

## Epic 3: FS98 flies on Linux with no wine (i386)

A native `linux-i386` build runs FS98 without wine: all 74 flights frame-identical to the retail references, dialogs passing natively, and devices working on Linux. Covers FR1, FR5 (native), FR10 (Linux) and AR2–AR4, AR7, AR9–AR13, AR15. Until Story 3.12, checks are unit tests and per-library difftests; the 74 flights arrive in 3.12.

### Story 3.1: The `linux-i386` target and `fs98arena`'s memory

As the developer,
I want the `linux-i386` toolchain and `fs98arena`'s region map, allocator and log sink,
So that native code has its memory.

**Acceptance Criteria:**

**Given** Epic 1
**When** I build `fs98arena` for `linux-i386` and run its tests
**Then** `0x0`–`0xFFFF` is never mapped, the code-handle and guest-base ranges are fixed, and the heap is 8-byte aligned and gives the same addresses for the same calls on every run and target (AD-22)
**And** host `+0x20c`, `malloc`/`new`, `HeapAlloc`, `GlobalAlloc` and `LocalAlloc` all reach that one allocator
**And** the log sink is the only output; nothing writes to stdout or stderr

### Story 3.2: Code handles and the arena stack

As the developer,
I want code handles with registered signatures, `fs98_call*`, the arena stack and `fs98_str`,
So that slots, vtables and out-parameters work the same on every target.

**Acceptance Criteria:**

**Given** Story 3.1
**When** I run the tests
**Then** registering a function twice returns the same handle, handles never equal native addresses and are never recycled, and a revoked handle raises an access violation at its address (AD-17)
**And** `FS98_FRAME_BEGIN`/`fs98_frame_alloc`/`FS98_FRAME_END` nest LIFO, and `fs98_str` returns one address per literal per process (AD-16)

### Story 3.3: Modules build as native shared libraries with an `Fs98Image`

As the developer,
I want every module, the decompiled gauges, the MDLs and `SIM1.sim` built as `.so` files exporting one `Fs98Image`, with `CALL()` replaced by `fs98_call*` (dispatchable module by module),
So that the native host can load them.

**Acceptance Criteria:**

**Given** Story 3.2
**When** a test loader builds each image twice from its compiled initialisers
**Then** both images are identical apart from the relocations it applied, and neither reads a retail file (AD-3)
**And** no module has ELF constructors, mutable state outside its image, or a libc import outside the allowlist (AD-23)

### Story 3.4: `fs98win32` kernel and CRT

As the developer,
I want `fs98win32`'s `windows.h`, kernel surface and MSVCRT-behaviour CRT,
So that modules have files, time and C library calls that behave as retail's.

**Acceptance Criteria:**

**Given** Story 3.3
**When** the same CRT built for `win32-retail` runs beside the CD's `MSVCRT.DLL` in a difftest under wine
**Then** formatting, `rand`, `qsort`, `strtod`/`atof` and `_ftol` match on vectors
**And** files follow AD-10: `C:\FLTSIM98\` over the read-only data root and the user root, case-insensitive lookup, sorted listings, CP1252 names, and a REGEDIT4 registry file in the user root
**And** every Win32 time call reads the one clock (AD-9)
**And** `CreateThread` from module code (MULTIP, BOOSTER, UTIL) makes a real OS thread with its own arena stack region and TIB, and the allocator and handle tables pass a multithreaded stress test (AD-9)
**And** the data root is found from its registry-file key, and the overlay rules hold: user root first, merged listings with the user root winning, copy-up on first write, whiteouts for deletes (AD-10)

### Story 3.5: One SEH chain and the FP control word

As the developer,
I want the arena TIB's SEH list, `fs98_seh_enter`/`leave`, and the host setting retail's measured main-loop FP control word,
So that unwinding and precision match retail natively.

**Acceptance Criteria:**

**Given** Story 3.4
**When** I measure retail's main-loop control word under wine and record it in `docs/`
**Then** the native host sets that value at sim-thread start (AD-20)
**And** the seven private `*_unwind.h` files are gone, those modules' exception tests pass natively, and nothing touches `%fs`/`%gs` (AD-19)
**And** every call into SDL or system code saves and restores the control word and MXCSR

### Story 3.6: The native host links the modules

As the developer,
I want the native host to scan `Modules/` and `<user root>/Modules/`, read ids before any code runs, link through `Linkage`/`ImportTable` and run the main loop with no display,
So that the module system works without wine.

**Acceptance Criteria:**

**Given** Story 3.5
**When** the host starts
**Then** it lists modules in ASCII-uppercase order, reads each id from the `Fs98Image` or a PE's on-disk `Linkage`, and resolves clashes per AD-13, logging each loser
**And** `LoadLibraryA` dispatches native images and resource-only PEs; a PE with code logs "guest not yet available"
**And** an unknown import id leaves a null slot and the module still loads (FR8 rule)
**And** a divide trap logs its site and aborts (AD-18)
**And** module id 19 (GAUGES) stays a null slot (AD-2)
**And** a headless smoke run flies Story 1.6's GLOBAL-dump flight with every module linked, and compares GLOBAL (relocated dwords masked) after each pass with the reference. The result is reported, not gating until Story 3.12; each difference is explained in the story before Story 3.7 starts
**And** an `HMODULE` is the arena address of the image base, and already-loaded modules match by module name as on Win98, documented in `docs/` with a test (AD-7)

### Story 3.7: Windows and messages through SDL3

As the developer,
I want `fs98sdl`, built on SDL3 3.4.x from its checksum-pinned tarball (AD-11), and `fs98win32`'s USER32 window, message and input surface,
So that FS98's windows exist and get input.

**Acceptance Criteria:**

**Given** Story 3.6
**When** I run the window tests
**Then** each top-level `HWND` is its own borderless SDL window with Win98 chrome drawn by us, `GetSystemMetrics` reads the virtual screen, and SDL events are consumed only in message retrieval (AD-21)
**And** the sim thread is the main thread
**And** each USER32 function FS98 uses passes a recorded vector set: against wine where wine is trusted, against retail frames where it isn't

### Story 3.8: GDI drawing

As the developer,
I want `fs98win32`'s GDI32 subset (DCs, bitmaps and DIB sections in the arena, fonts and text, blits, palettes),
So that FS98's windows and owner-drawn controls draw as in retail.

**Acceptance Criteria:**

**Given** Story 3.7
**When** I run the GDI tests
**Then** each of the 51 GDI32 functions FS98 uses passes a recorded vector set: against wine where wine is trusted, against retail frames where it isn't
**And** `HWND`, GDI and kernel handle values are never reused within a session (AD-6)

### Story 3.9: Resources and dialogs

As the developer,
I want `fs98win32`'s resource reader and dialog manager, and `fs98mfc` built for `linux-i386`,
So that retail's dialogs open natively.

**Acceptance Criteria:**

**Given** Story 3.8 and Epic 2
**When** a module asks for a resource
**Then** `FindResourceA`, `LoadStringA` and `DialogBoxParamA` serve it from `Data/Retail` files, `fs98_resources` blobs and PE `.rsrc` (AD-26)
**And** every English dialog opens

### Story 3.10: Graphics and sound

As the developer,
I want `fs98win32`'s DDRAW (2D), DSOUND, WINMM timers and wave, and MSACM32,
So that HG2D and SOUND run.

**Acceptance Criteria:**

**Given** Story 3.9
**When** the game runs
**Then** HG2D's software path presents frames and `Direct3D*` creation fails cleanly (AD-24)
**And** DirectSound is mixed once per pass into the ring buffer the SDL audio callback reads, and timers and callbacks run on the sim thread at the clock read (AD-9)
**And** COM objects live in the arena with handle vtables

### Story 3.11: Input and the remaining system DLLs

As the developer,
I want WINMM `joy*` and `mmio`, DPLAY, COMCTL32, IMM32 and SHELL32,
So that CONTROLS and MULTIP load and run.

**Acceptance Criteria:**

**Given** Story 3.10
**When** the game runs
**Then** joysticks and force feedback reach CONTROLS through SDL
**And** DPLAY is present, so MULTIP loads though sessions don't work yet
**And** every import FS98's modules make resolves to a real function, none to a missing-import handle

### Story 3.12: FRAMELOCK natively, and all 74 flights match

As the developer,
I want FRAMELOCK inside `fs98win32` and the one scripted 74-flight report,
So that FR1 is checked against the retail references.

**Acceptance Criteria:**

**Given** Story 3.11 and Story 1.6's references
**When** I run the report on `linux-i386`
**Then** all 74 flights are frame-identical to retail, kept bugs included (FR1)
**And** the log grammar matches `docs/tools.md`, comparison is per window, and dumps mask relocated dwords (AD-8)
**And** a bug that reads uninitialised memory takes retail's observed value from a documented source

### Story 3.13: Dialog drivers in-process

As the developer,
I want `main-dialogs` and `flights-play` driven by FRAMELOCK scripts inside the process,
So that FR5 holds natively.

**Acceptance Criteria:**

**Given** Story 3.12
**When** I run both on `linux-i386`
**Then** both pass with no wine (AD-27)

### Story 3.14: Devices on Linux, flown by hand

As a player,
I want keyboard, joystick with force feedback, sound and real-time timing to work on Linux,
So that I can actually fly it.

**Acceptance Criteria:**

**Given** Story 3.13
**When** you fly a session by hand
**Then** controls, force feedback, sound and frame pacing behave as in retail (FR10 Linux, NFR13)
**And** notes on the session go in `docs/open-differences.md`

## Epic 4: Retail bugs fixed

Every register entry fixed in the i386 build, which then becomes the reference Epic 5 checks against. Covers NFR12 and AR18 (fixes). All bugs are fixed; letting players switch one back is future work.

### Story 4.1: Triage the register

As the developer,
I want every entry in `docs/retail-bugs.md` given a class and a confirmed status,
So that the fixing works from a fixed list in a safe order.

**Acceptance Criteria:**

**Given** Epic 3 (FR1 passes)
**When** I go through each entry
**Then** it is classed **fix: undefined** (memory safety, uninitialised reads, overruns, stack garbage), **fix: behaviour**, or **not real** (a suspected entry a run disproves)
**And** the register records the class and its reason for every id
**And** every *(suspected)* entry is confirmed or dismissed by a run or a harness

### Story 4.2: Fix the undefined-behaviour bugs

As the developer,
I want every **fix: undefined** entry fixed (dispatchable module by module),
So that no result depends on memory layout, stack contents or the target.

**Acceptance Criteria:**

**Given** Story 4.1
**When** each fix lands
**Then** a test shows the triggering case gives the fixed result, and the module's other difftest vectors are unchanged
**And** its `RETAIL BUG (kept)` marker is removed and the register entry says fixed, with the commit
**And** the 74-flight report lists which flights changed and attributes each change to a fix

### Story 4.3: Fix the behaviour bugs

As the developer,
I want every **fix: behaviour** entry fixed (dispatchable module by module),
So that the port flies as FS98 should have.

**Acceptance Criteria:**

**Given** Story 4.2
**When** each fix lands
**Then** a test shows the triggering case gives the fixed result, and the module's other difftest vectors are unchanged
**And** its marker is removed and the register entry says fixed, with the commit
**And** every flight change in the 74-flight report is attributed to a fix

### Story 4.4: The fixed i386 build becomes the reference

As the developer,
I want the fixed i386 build's 74 flights recorded as the new references,
So that Epic 5 has its baseline (AD-8).

**Acceptance Criteria:**

**Given** Story 4.3
**When** I record twice
**Then** the checksums are identical and the manifest names the commit and marks itself "fixed i386"
**And** no `RETAIL BUG (kept)` marker remains in `src/`, and the consistency check from Story 1.5 confirms every register entry is fixed or not real
**And** you fly a session by hand and find nothing newly broken

## Epic 5: Linux x86-64 and Windows x64, bit-identical

Both 64-bit builds match the fixed i386 build on all 74 flights, devices work on Windows, and i386 is retired. Covers FR2, FR3 (Windows), FR10 (Windows), NFR4 and NFR6.

### Story 5.1: `linux-x86_64` builds, with the arena at a nonzero base

As the developer,
I want the `linux-x86_64` target with the arena as a reserved 4 GB + 64 KB region,
So that every layer runs where addresses aren't pointers (AD-1).

**Acceptance Criteria:**

**Given** Epic 4
**When** I build and run every unit test
**Then** they pass with the arena mapped at a nonzero base, randomised per run (AD-15)
**And** the layout asserts pass on LP64, and `fs98_handle` asserts on any pointer outside the arena
**And** a per-unit disassembly check finds no SSE arithmetic in FS98 units (AD-20)

### Story 5.2: x86-64 matches the fixed i386 build

As the developer,
I want the 74-flight report run on `linux-x86_64` against Story 4.4's references,
So that FR2 is shown.

**Acceptance Criteria:**

**Given** Story 5.1
**When** I run the report in a test build at a random base and in a release build
**Then** all 74 flights are frame-identical in both to an i386 run of the same commit (AD-8), and to Story 4.4's references
**And** `main-dialogs` and `flights-play` pass

### Story 5.3: `windows-x64` builds and matches

As the developer,
I want the `windows-x64` mingw-w64 target,
So that FR3's Windows half is shown.

**Acceptance Criteria:**

**Given** Story 5.2
**When** I run the report and the dialog drivers on Windows x64
**Then** all 74 flights are frame-identical to Story 4.4's references, and `main-dialogs` and `flights-play` pass
**And** an import check shows no unit calls real Win32 UI APIs (AD-5); only `fs98win32`'s file and time backends and SDL3 reach the OS
**And** the layout asserts from Story 1.7 pass on LLP64, and the SSE check from Story 5.1 passes
**And** the DOS paths map onto a Windows install and user root per AD-10

### Story 5.4: Devices on Windows, flown by hand

As a player,
I want keyboard, joystick with force feedback, sound and timing to work on Windows,
So that I can fly it there.

**Acceptance Criteria:**

**Given** Story 5.3
**When** you fly a session by hand
**Then** it behaves as on Linux (FR10 Windows, NFR13)
**And** notes on the session go in `docs/open-differences.md`

### Story 5.5: Retire i386

As the developer,
I want the `linux-i386` target and its only-on-i386 code removed,
So that there is no 32-bit port to maintain (NFR4).

**Acceptance Criteria:**

**Given** Stories 5.2, 5.3 and 4.4's references
**When** I remove the `linux-i386` toolchain and any base-0-only code paths
**Then** every remaining target builds and its checks pass
**And** `win32-retail` stays, for difftests, reference recording and retail add-on builds
**And** `docs/building.md` and the report tooling no longer list i386 as a target

## Epic 6: Install from a CD image, and Release 1

A fresh release archive plus the user's FS98 CD, any known pressing, reaches a flight, and Release 1 ships for Linux x86-64 and Windows x64. Third-party binaries that need the guest come in a later release (Epic 7). Covers FR12, NFR1 (runtime), AD-10, AD-12, AD-13, AD-25 (native half), AD-26.

### Story 6.1: Every FS98 pressing catalogued

As a player,
I want setup to accept whichever FS98 CD pressing I own,
So that I don't need one particular disc.

**Acceptance Criteria:**

**Given** Epic 5 and the ISOs collected so far (catalogued with the action-item script)
**When** each is catalogued in `docs/building.md` (checksums of its game files and of the resources our units read)
**Then** setup accepts every catalogued pressing, the November 1997 one included, since no module image is read from the CD (AD-12)
**And** where a pressing's resources differ from the reference, the differences are listed and our dialogs are checked against them, including the tables committed from the reference image (`src/*/*_runtime_tables.inc`, `*_tables.inc`: control ids, DDX bindings, message maps), since from Story 1.4 on the `resource:` build scripts take resources from any pressing without an image check
**And** an uncatalogued pressing is refused with a message naming it and asking the user to report it

### Story 6.2: Reading the CD from any source

As a player,
I want setup to read my CD from an ISO 9660 image, a BIN/CUE image, a mounted folder or a drive,
So that I can use whatever copy I have.

**Acceptance Criteria:**

**Given** Story 6.1 and libcdio 2.3.0 built from its checksum-pinned tarball (AD-11)
**When** I read the same CD from each of the four sources
**Then** each gives the identical file tree, with `;1` stripped and names cased per AD-10

### Story 6.3: Installing the data

As a player,
I want the CD's data installed and the retail files our modules read copied into `Data/Retail/`,
So that the sim has everything it needs.

**Acceptance Criteria:**

**Given** Story 6.2
**When** setup installs
**Then** the data root defaults to beside the binaries if that folder is writable, else to the user root (`SDL_GetPrefPath`), and the user can change it; setup writes its location as a key in the user root's registry file, and a missing or invalid key reruns setup with the last path filled in (AD-10)
**And** saves, `FLTSIM98.CFG`, the registry file and third-party `Aircraft/`/`Gauges/` go to the user root
**And** no CD code we replace is installed into `Modules/`, `Gauges/` or `Aircraft/` (AD-13)
**And** each file is copied under a temporary name then renamed; an interrupted install resumes or restarts cleanly, and a second install changes nothing

### Story 6.4: The dialog API, natively

As the developer,
I want `include/fs98/dialog.h` mapped onto `fs98mfc`, with dialog resources shipped as a `.res` blob in `fs98_resources`,
So that setup, and later the add-ons, show FS98-style dialogs from one API.

**Acceptance Criteria:**

**Given** Epic 5
**When** a test dialog is built from `dialog.h` with its resources in a blob
**Then** it opens and works natively on `linux-x86_64` and `windows-x64` through `fs98mfc` (AD-25), and the resource reader serves the blob (AD-26)
**And** it matches the retail dialogs beside it: SIMSUI controls, fonts and chrome
**And** a FRAMELOCK script drives it in a test

### Story 6.5: The setup dialog

As a player,
I want an FS98-style setup dialog on first launch,
So that installing feels like part of FS98.

**Acceptance Criteria:**

**Given** Story 6.3 and Story 6.4
**When** the host starts with no installed data
**Then** the setup dialog opens, built on `dialog.h` with its resources in `fs98_resources` (AD-26), offering image, folder or drive and showing the install path
**And** it shows progress, and a clear error for an unknown or refused pressing, an unreadable image or a full disk
**And** a FRAMELOCK script drives it end to end in a test

### Story 6.6: Release 1 archives

As a player,
I want a tarball for Linux x86-64 and a zip for Windows x64,
So that I can download and run OpenFS98.

**Acceptance Criteria:**

**Given** Story 6.5
**When** the archives are built
**Then** a check finds no CD file, retail resource or raw dump in either archive
**And** a fresh extract plus an ISO completes setup and starts a flight on each platform (FR12), with the data installed beside the binaries
**And** the release notes say plainly that third-party aircraft and gauges built as Windows binaries need a later release, and that until then they are skipped with a "guest not yet available" log line (Story 3.6)

## Epic 7: Third-party aircraft and gauges

`fs98guest` runs 1998 binaries: retail gauges match their natives, every corpus gauge matches itself run on a real CPU, the 22 aircraft fly, and the GPS gauge steers the autopilot. Covers FR7, AR19, AD-7, AD-17, AD-19, AD-27. Built on `linux-x86_64`, checked on Windows x64.

### Story 7.1: The x86-32 interpreter

As the developer,
I want `fs98guest`'s interpreter (integer, x87, and the instructions MSVC 4–6 code uses),
So that guest code computes what a real CPU computes.

**Acceptance Criteria:**

**Given** Epic 6 (Release 1 is out) and the recorded results from the `-m32` recorder action item
**When** I run an instruction test suite against results recorded once by a `-m32` test program on a real CPU
**Then** every result, flag and x87 state matches
**And** a faulting guest divide goes through the `divide.h` helpers (AD-18), the guest's `fs:0` is the arena TIB (AD-19), and the control word is copied at every transition (AD-20)

### Story 7.2: The PE loader and thunks

As the developer,
I want `LoadLibraryA` to load PEs with code into the arena, with thunks to native code,
So that guest images link to PANELS, the host and each other.

**Acceptance Criteria:**

**Given** Story 7.1
**When** a PE with code is loaded
**Then** it lands at a deterministic base in the guest range with relocations applied, and its imports bind to thunks that follow each handle's signature and preserve `ebx`/`esi`/`edi`/`ebp`; no native code runs on the guest stack (AD-7)
**And** a missing import logs `dll!name` and raises an access violation only when called
**And** guest images import from each other (`abrakedis_v2.gau` from `Autobrake_V2.GAU`)
**And** a retail PE with code in `Modules/` loads in the guest under AD-13

### Story 7.3: The Win32 shim for the corpus

As the developer,
I want the guest's imports served from `fs98win32`: the static MSVC CRT's ~60 KERNEL32 functions, `MSVCRT.DLL`, the resource APIs, WINMM `mmio*`, `DirectSoundCreate`, `FindWindowA`, `CreateFileA`/`ReadFile`,
So that the 230 corpus binaries find everything they call.

**Acceptance Criteria:**

**Given** Story 7.2
**When** I run a static check over all 230 binaries in `~/source/fs98stuff`
**Then** every import resolves to a real function
**And** any exception is listed in `decisions.md` with its reason

### Story 7.4: Retail gauges in the guest match the natives

As the developer,
I want PANELS to load PE gauges and crash MDLs in the guest, and the guest-versus-native check (AD-27) run on all 120 retail binaries,
So that the guest is proven on code we have ground truth for.

**Acceptance Criteria:**

**Given** Story 7.3
**When** each retail binary runs in the guest beside its decompiled native on the same element inputs
**Then** the outputs match (FR7, first half)
**And** the check runs on `linux-x86_64` and `windows-x64`

### Story 7.5: Third-party MDLs

As the developer,
I want the corpus's 22 MDLs to load and draw,
So that third-party aircraft show their models.

**Acceptance Criteria:**

**Given** Story 7.4
**When** each MDL is loaded
**Then** the 21 sharing the `MDL610.MOD` converter stub load their models from `.rsrc`, and `beavmp.mdl` loads with retail `CESSNA182S_N.MDL`'s layout
**And** each model appears in a frame capture

### Story 7.6: Every corpus gauge matches itself on a real CPU

As the developer,
I want each of the 208 corpus gauges run in a `win32-retail` harness under wine (the real CPU, stub PANELS) and in `fs98guest`, on the same element inputs and GLOBAL snapshot,
So that every third-party gauge is checked against ground truth, not just for crashes.

**Acceptance Criteria:**

**Given** Story 7.5
**When** the harness runs each gauge over its recorded input set
**Then** element outputs, GLOBAL writes and drawn bitmaps match, 0 differ, for every gauge
**And** the input sets cover each gauge's variables across their ranges, mouse areas included
**And** a deliberate break in the interpreter is caught

### Story 7.7: The 22 aircraft fly (the pinned dynamic pass)

As a player,
I want the 22 corpus aircraft to load and fly with working panels,
So that my 1998 add-ons still work.

**Acceptance Criteria:**

**Given** Story 7.6
**When** each aircraft is loaded and flown in a scripted FRAMELOCK flight on `linux-x86_64` and on `windows-x64`
**Then** every gauge draws, nothing hits a missing import, and the session ends cleanly
**And** with `generic.gps_dm.gau` running, the autopilot heading (GLOBAL `+0x7cc`) follows the GPS, written through PANELS `+0x98`'s pointer into the shared arena
**And** the results are written up in `decisions.md`, replacing the pinned item (AR19)

## Epic 8: Add-ons and new dialogs

One SDK, OpenFS98-only module ids, one dialog API, and the EFB data-output add-on in its own repo, built both ways. Covers FR6, FR8, FR9, NFR3, NFR11 and AD-2, AD-13, AD-14, AD-24 (WS2_32), AD-25, AD-26.

### Story 8.1: The add-on SDK and native modules in `Modules/`

As an add-on author,
I want the `include/fs98/` headers and the `OpenFS98::module` target installed as a CMake package,
So that I can build a native module outside the tree.

**Acceptance Criteria:**

**Given** Epic 7
**When** an out-of-tree test module built against the installed package is dropped into `Modules/`
**Then** it loads through `Linkage`/`ImportTable` and inits (FR8)
**And** a module importing an unknown id gets a null slot and still loads and inits, in the native host and in the retail host under wine, for an id in 40–63 as well as outside it (NFR11)

### Story 8.2: The OpenFS98 extension module

As an add-on author,
I want native-only services offered as a module with an id in 40–63, listed in `linkage.h`,
So that I can use them without a second API (NFR3).

**Acceptance Criteria:**

**Given** Story 8.1
**When** a test add-on imports the extension module
**Then** it calls at least one of its services natively
**And** in retail it finds a null slot and carries on, with no `#ifdef`

### Story 8.3: The EFB settings dialog, in both builds

As an add-on author,
I want `dialog.h` (Story 6.4) also mapped onto retail MFC42 ordinals in `win32-retail`,
So that one source shows an FS98-style dialog in both builds.

**Acceptance Criteria:**

**Given** Story 8.2 and Story 6.4
**When** I build the EFB settings dialog (on/off, target IP and port, output format) from it
**Then** `dialog.h` maps onto retail MFC42 ordinals in `win32-retail` (AD-25), and retail FS98 reads the same resource blob
**And** the dialog opens and works in a native build and in retail FS98 under wine (FR6)
**And** it matches the retail dialogs beside it: SIMSUI controls, fonts and chrome

### Story 8.4: UDP sockets in `fs98win32`

As an add-on author,
I want `fs98win32` to serve the WS2_32 subset a UDP sender uses (`WSAStartup`, `socket`, `setsockopt` for broadcast, `sendto`, `closesocket`, address helpers),
So that the EFB add-on's source is plain Winsock in both builds.

**Acceptance Criteria:**

**Given** Story 8.3
**When** a test module sends and receives UDP through it natively
**Then** datagrams arrive intact, broadcast included
**And** socket calls never block the sim thread, and their events reach it only at the start of a pass (AD-9)

### Story 8.5: The EFB data-output add-on, built both ways

As a player,
I want an add-on that streams my aircraft's position to an EFB app over the LAN as GDL 90,
So that Garmin Pilot shows me flying in FS98.

**Acceptance Criteria:**

**Given** Story 8.4
**When** the add-on runs, built once as a 32-bit retail DLL (`win32-retail`) in FS98 under wine and once as a native module
**Then** each sends GDL 90 over UDP (default port 4000): a heartbeat each second, and ownship report and ownship geometric altitude messages at a steady rate, with correct framing, byte stuffing and CRC
**And** a decoder test checks the messages against GDL 90 examples, and Garmin Pilot shows the aircraft moving in both builds (hand-checked by you)
**And** the source lives in its own repo, included as a git submodule; `docs/` explains starting an add-on from it (FR9)
**And** you create the GitHub repo and add the submodule; the story prepares the files

## Epic 9: macOS, Intel and Apple Silicon

Both Mac targets build in GitHub Actions and pass the checks that need no CD: unit tests, per-routine tolerances, and the guest interpreter. There is no local Mac. Flight-path checks are added only if Mac users report real issues, and user reports are tracked as issues. Covers FR3 (Intel Mac), FR4, FR10 (Mac) and NFR6 (tier 2).

### Story 9.1: Mac builds and tests in GitHub Actions

As the developer,
I want `macos-x86_64` and `macos-arm64` built with Apple Clang and their unit tests run on GitHub Actions macOS runners,
So that the Mac targets exist without a Mac.

**Acceptance Criteria:**

**Given** Epic 8
**When** the workflow builds both targets (`macos-15-intel` or `macos-26-intel`, and `macos-26`)
**Then** every unit test passes, including the 4 GB + 64 KB arena reservation and `fs98_fpu_cw` on ARM (AD-20)
**And** no CD file, reference log or third-party binary is used in CI

### Story 9.2: Per-routine tolerances

As the developer,
I want each FP routine's tier-2 output measured against the x86-64 build and its bound documented,
So that "within tolerance" is checkable.

**Acceptance Criteria:**

**Given** Story 9.1
**When** the routines' vectors (generated inputs, with outputs recorded from `linux-x86_64`) run in CI on both Mac targets
**Then** each routine's largest difference and its bound are recorded in `docs/`
**And** no native hot path uses soft 80-bit float (NFR6)
**And** any routine whose difference comes from a different branch rather than rounding is listed, then fixed or explained

### Story 9.3: The guest interpreter on Apple Silicon

As a player,
I want the guest to run 1998 code on Apple Silicon,
So that a Mac user has the same add-ons.

**Acceptance Criteria:**

**Given** Story 9.2
**When** Story 7.1's instruction suite runs on `macos-arm64` with the guest's x87 in soft 80-bit float (the NFR6 exception for the guest)
**Then** every integer result matches, and every x87 result matches or stays within its documented bound

### Story 9.4: Mac releases

As a player on a Mac,
I want a downloadable app that installs from my CD and flies,
So that I can use OpenFS98.

**Acceptance Criteria:**

**Given** Story 9.3
**When** CI builds a release
**Then** it produces a zipped `.app` per architecture, which starts and opens the setup dialog in a CI smoke test
**And** setup installs the data into the user root, since the app's own folder isn't writable
**And** the release notes say the builds are unsigned and explain how to open them
**And** an issue template asks testers for a hand-flown report (FR10 Mac). Reports are tracked as issues and don't block this story; a user-run 74-flight script is added if they show real issues

## Epic 10: Multiplayer, DirectPlay-compatible

`fs98win32`'s DPLAY speaks real DirectPlay on the wire (the published DirectPlay 4 protocol, TCP/IP service provider), so native builds play each other and retail FS98 on real DirectPlay, over a LAN. Covers FR11, AD-9, AD-24. Internet play is later work.

### Story 10.1: Map MULTIP's DirectPlay use and the wire protocol

As the developer,
I want every DirectPlay interface, method and message that MULTIP, mpdialog and MPCHAT use written down, and the parts of [MC-DPL4CS] they reach,
So that the transport implements exactly what FS98 needs, byte for byte.

**Acceptance Criteria:**

**Given** Epic 9
**When** I read MULTIP's calls (`IDirectPlay3A`/`2A`, `IDirectPlayLobbyA`, `DirectPlayCreate`, `DirectPlayEnumerateA`) and the protocol spec
**Then** `docs/modules/multip.md` lists each call with its arguments, results and the system messages FS98 handles, and the wire messages each one produces
**And** the doc confirms the spec's protocol versions interoperate with DirectX 5's `IDirectPlay3A` peers, or records what differs
**And** retail FS98 on Windows with the DirectPlay legacy component is recorded in a two-machine LAN session (host, join, chat, fly, leave) as a packet capture, the ground truth for Stories 10.2 and 10.4

### Story 10.2: The DirectPlay wire protocol in `fs98win32`

As the developer,
I want `fs98win32`'s DPLAY to implement the DirectPlay 4 TCP/IP service provider: UDP 47624 enumeration, TCP for guaranteed and UDP for unguaranteed messages on 2300–2400, and the system messages Story 10.1 lists,
So that MULTIP's calls work unchanged over a real DirectPlay wire.

**Acceptance Criteria:**

**Given** Story 10.1 and Story 8.4's WS2_32 subset, extended with TCP, receive and non-blocking calls
**When** the protocol tests replay Story 10.1's captures
**Then** every message we send parses and matches the capture's layout, and every captured message we receive is handled
**And** a network worker owns the sockets; the sim thread sees traffic only through queues drained at the start of a pass (AD-9)
**And** `DirectPlayEnumerateA` offers only the TCP/IP provider

### Story 10.3: Two native builds share a session

As a player,
I want to host or join a LAN session between two native builds,
So that I can fly with a friend.

**Acceptance Criteria:**

**Given** Story 10.2
**When** two native instances run a scripted host, join, chat and fly session
**Then** each sees the other's aircraft and chat (FR11)
**And** a Linux x86-64 build and a Windows x64 build share a session
**And** you fly a session by hand across two machines on a LAN

### Story 10.4: Native and retail FS98 share a session

As a player,
I want to fly with someone running retail FS98,
So that the old and new communities can meet.

**Acceptance Criteria:**

**Given** Story 10.3
**When** a native build and retail FS98 on Windows with real DirectPlay join one LAN session, each hosting in turn
**Then** both see each other's aircraft and chat, and join, leave and host-left behave as in a retail-only session
**And** differences caused by fixed retail bugs are listed in `docs/open-differences.md`, not treated as failures
