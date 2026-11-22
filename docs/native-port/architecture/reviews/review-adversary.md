# Adversarial review: ARCHITECTURE-SPINE.md (native port)

Reviewed 2026-11-15 against the spine (13 ADs), `.memlog.md`, `../SPEC.md`, `../decisions.md`, `AGENTS.md`, and the code in `src/` and `include/`.

Method: for each finding I build two units one level down. Each unit obeys every AD as written, and the two still don't fit together. Each finding gives the pair, why both comply, how they clash, the evidence in the tree, and the exact rule text that closes the hole. Proposed ADs are numbered from AD-14. Tightenings name the AD they amend.

**Verdict.** The spine fixes the right big choices: one arena, one loader, our own Win32, one thread. But it leaves the mechanics of those choices to each implementer, and those mechanics are where they will clash. Three things are missing:

- **Code handles:** what one is, its signature, its identity, and how long it lives.
- **The image lifecycle:** who copies, relocates and initialises a module's image, and when.
- **Process-wide machine state:** the x87 control word, the SEH chain, the divide repair, the clock, and stack memory for out-parameters.

On top of that, the ABI headers in the tree today contradict AD-1, and SPEC, which calls itself the canonical contract, still describes the rejected memory model.

---

## Critical

### C1. The ABI headers in the tree contradict AD-1
- **Pair:** a native add-on built against `include/fs98/linkage.h` + `host.h` (the "module ABI headers" of the Structural Seed) ↔ the native host loader that reads 4-byte slots per AD-1.
- **Both comply:** AD-1 says slots hold 32-bit code handles. Neither AD-1 nor the Seed says the existing headers must change before anyone builds against them.
- **Clash:** `struct fs98_linkage` has `void (*init)(void)` and `void *service[]`. `struct fs98_import` has `struct fs98_linkage *link`. On x86-64 those fields are 8 bytes, so an add-on's ImportTable entries become 16 bytes and its slot 2 sits at `+0x18` instead of `+0x0c`. `host.h` has the same problem: `void **out` (`+0x20c`), `void **pp` (`+0x210`, `+0x214`), `const char *path` (`+0x14c`-area module load), and `void *data` in structs. The loader walks a table of a different shape. GLOBAL's Linkage sits at offset 0 of GLOBAL, so every typed GLOBAL field after it would shift too.
- **Rule (new AD-14, "The module ABI is 32-bit on every build"):** Every type in `include/fs98/` that describes FS98 memory or a service signature uses `fs98_addr` (a `uint32_t` arena offset) for data addresses and `fs98_code` (a `uint32_t` code handle) for code, never a C pointer or function-pointer type. Each such struct has `_Static_assert(sizeof …)` and `_Static_assert(offsetof …)` checks against the retail size and offsets, and they compile on every AD-11 target. `win32-retail` maps `fs98_addr`/`fs98_code` to the real 32-bit values, so a CAP-9 add-on uses one header for both builds.

### C2. SPEC, the "canonical contract", still specifies the rejected memory model
- **Pair:** PANELS built from SPEC ("GLOBAL and all sim state stay native… a copy of that variable in the guest arena, synced… before and after each guest call") ↔ `fs98guest` built from AD-1 (the guest shares the arena, and `+0x98` points straight into GLOBAL).
- **Both comply:** SPEC's banner says it is "the complete… contract". The memlog records the conflicts only as an open `(question)`.
- **Clash:** PANELS keeps a mirror and copies it back after each gauge call. `generic.gps_dm.gau` writes GLOBAL `+0x7cc` directly through the AD-1 pointer, and the mirror's stale copy then overwrites it. The autopilot heading target is lost every frame, so CAP-7's named success case fails. The same split applies to SPEC's "pointer-width fields in native builds" (see C1) and to the Non-goal "Retail GLOBAL layout in the guest arena", which AD-1 adopts.
- **Rule (spine preamble, before AD-1):** "Where this spine and SPEC.md disagree, the spine governs until SPEC.md is updated. The SPEC Constraints *pointer-width fields*, *GLOBAL and all sim state stay native* and the *guest-arena mirror sync*, and the Non-goal *Retail GLOBAL layout in the guest arena*, are void. No unit may implement them. SPEC.md is updated before any CAP-2 work starts."

### C3. Two loaders own a module image, and a third copy lives in the `.so`
- **Pair (a):** the host loader (AD-3: "At load the host allocates each module's `.data` image in the arena…") ↔ `LoadLibraryA` (AD-7), which PANELS calls for gauges and MAIN/ACONTAIN call for MDLs and `SIM1.sim`. Both obey their ADs. Modules in `Modules/` get arena images. A native gauge or `SIM1.sim` loaded through `LoadLibraryA` gets none, because AD-3 binds only "the host loader".
- **Pair (b):** a decompiled module ↔ the host loader. Today every module relocates its own C-static image when it attaches (`FE_RELOCS`, `FE_REL_TEXT` in `src/fe/fe_data.inc`, `DllMain` in 45 files). If the host copies the "initial bytes" before that relocation, the code pointers are zero. If it copies them after, they are native 64-bit pointers truncated to 32 bits. The module also keeps using its C static (`panels_image`, `D = (uint32_t)(uintptr_t)panels_image.bytes`) while other modules use the arena copy. Two live copies of PANELS's state then exist.
- **Pair (c):** AD-7 "`GetProcAddress` returns… an arena address for exported data such as `Linkage`" ↔ `dlsym`. `dlsym("Linkage")` returns the native static, and `fs98win32` cannot tell a data export from a code export.
- **Pair (d):** `SIM1.sim` is reloaded for each aircraft "with its Linkage fresh" (`tools/drive/framelock.c`). `dlclose` need not unload the library (glibc with `RTLD_NODELETE` dependencies, or macOS), so C statics survive while retail gets fresh `.data`.
- **Rule (replaces AD-3):** "`LoadLibraryA` is the only code that places an image in the arena, for every caller and every format. A native module exports exactly one data symbol, `Fs98Image`: a const descriptor giving image size, initial bytes, relocation list (data→data, data→code as function ids), the offsets of `Linkage` and `ImportTable`, `DllMain` as a function id, and the function table. On every load, including a reload, the loader allocates a fresh arena image, copies the initial bytes, applies the relocations (data targets to arena offsets, code targets to code handles), and sets the module's `D`/`RDATA` base. Only then does it call `DllMain`, as a code handle on the sim thread. `GetProcAddress` on a native module resolves `Linkage`/`ImportTable` through the descriptor, never through `dlsym`. Native modules have no ELF or Mach-O constructors and no mutable state outside their arena image. The host's scan calls `LoadLibraryA` like any other caller."

### C4. Out-parameters and string literals have nowhere legal to live
- **Pair:** a native module calling `GetClientRect(hwnd, &rect)` or `MessageBoxA(0, "text", …)` on `fs98win32` ↔ `fs98win32` with "arena-typed signatures" (AD-6).
- **Both comply:** AD-3 says the native stack and C statics never cross. AD-6 doesn't say whether a pointer parameter is a `uint32_t` arena offset or a native `char *` that happens to point into the arena. One implementer types `LPRECT` as `uint32_t`. Another types it as `RECT *` and calls `fs98_handle()` on it inside.
- **Clash:** Decompiled code passes `&local` all the time (retail's out-structs sit on the stack). Each module will invent its own static arena scratch buffer, which breaks on reentrancy: a `SendMessage` in the middle of a call reuses the buffer. Each module will also invent its own string-interning scheme. The same source then compiles for one `fs98win32` header and not for the other.
- **Rule (new AD-15, "Arena stack and pointer parameters"):** "Every pointer parameter or result of an `fs98win32` export, and every pointer field of a retail struct, is an `fs98_addr`. `fs98arena` owns one LIFO arena stack per sim thread, and it is the same region the guest uses as `esp`. Native code takes temporary arena memory only through `FS98_FRAME_BEGIN`/`fs98_frame_alloc(n, align)`/`FS98_FRAME_END`. Allocations come from below the guest's current `esp`, so guest↔native nesting stays LIFO. Constant strings and tables that cross go into the module's `RDATA` image (C3) or come from `fs98_str(literal)`, which interns the literal into the arena once per process. A static scratch buffer in an image used as a call-out area is forbidden."

### C5. A code handle has no defined identity, signature or call path
- **Pair (a):** a native PANELS service called by a guest gauge (stdcall, callee pops) ↔ `fs98guest`'s thunk. AD-4 says thunks "follow each import's MSVC convention", but a code handle carries no arity or convention. The thunk can't know how many bytes to pop, or whether the result is in `eax`, in `edx:eax`, or in `st(0)` (PANELS's variable evaluation returns floating point). The reverse direction, PANELS calling a gauge callback in the guest, has the same gap.
- **Pair (b):** `CALL(m, off, fn)` (`(*(fn *)(uintptr_t)((m)+(off)))`, defined privately in about 25 modules) ↔ AD-1 handles. The macro reads the slot as a native function pointer. `keep.h` says outright "on anything else the slot holds the function itself". On i386, where handle == address works, this passes CAP-1, then breaks at CAP-2.
- **Pair (c):** module A compares a slot with the handle it registered for the same function earlier (retail compares function pointers, for example a default handler). If registration isn't idempotent, A gets a fresh handle and the comparison fails.
- **Pair (d):** a gauge is unloaded (PANELS changes panel) and its handles are reused for the next gauge. A stale PANELS slot then silently calls the wrong gauge where retail faulted.
- **Rule (new AD-16, "Code handles"):** "A code handle is the arena address of a 16-byte stub in a reserved, non-readable code-handle region of the arena. A guest jump there traps into the thunk path. Native code calls a handle only through `fs98_call*`, never by casting it. Each handle is registered with a signature: convention (`cdecl`, `stdcall`, `thiscall`), argument dwords, and result kind (none, `eax`, `edx:eax`, `st0`). Guest↔native thunks are generated from that signature. Registering the same function twice returns the same handle. Handles are never recycled within a process. Calling a revoked handle raises an access violation at the handle's address, as retail calling unmapped code would. Handles are never equal to native addresses on any native build, i386 included. `CALL()`'s per-module copies are replaced by one `fs98_call` macro in `include/fs98/`."

### C6. The integer-divide repair has four owners and no native definition
- **Pair (a):** BOOSTER (`src/booster/booster.c:755`, inline `idivl`/`divl`, relies on a trap) ↔ the native host. `host_rt_divide.c` decodes x86 `CONTEXT` bytes, checks `(opcode & 0xfe) == 0xf6` (so a REX-prefixed x86-64 `idiv` isn't matched), and is "inherently x86". On Linux there is no `__try`, and on AArch64 `sdiv` never traps: x/0 = 0 and `INT_MIN/-1 = INT_MIN`, not `0x7fffffff`/`0x80000001`.
- **Pair (b):** SIM1's private `sim1_idiv32` ↔ HG2D's private inline saturation (`hg2d_polygon.inc`, `hg2d_gouraud.inc`, `hg2d_projective.inc`). They are three implementations of one host filter, already slightly different ("excludes INT32_MIN"). A C `/` in any other module is UB on zero, and the compiler may delete the zero path.
- **Pair (c):** `fs98guest` ↔ the host filter. A guest `div` fault has to produce the host filter's saturation, but no AD says the guest owns that.
- **Rule (new AD-17, "Integer divide"):** "No native code relies on a divide trap, on any build. Every retail division that can fault calls `fs98_div32`/`fs98_idiv32`/`fs98_div16`/`fs98_idiv16`/`fs98_div8`/`fs98_idiv8` from `include/fs98/divide.h`. Those helpers return the quotient and remainder that filter `0x4119a0` writes (division by zero and overflow both saturate). Module-private copies and inline `div`/`idiv` asm are removed. `fs98guest` applies the same helpers to a faulting guest `div`/`idiv` and raises nothing. The native host installs a `SIGFPE`/`EXCEPTION_INT_DIVIDE` handler only to log the site and abort, so a missed site fails CAP-1 loudly."

### C7. Seven private SEH chains on `%fs:0`, MFC exceptions, and a guest TIB
- **Pair:** `alerts_unwind.h` (and `sim_`, `world_`, `options_`, `help_`, `flights_`, `mpdialog_unwind.h`) ↔ `fs98mfc` throwing (DDV failure `CUserException`, `AfxThrowMemoryException`) ↔ a guest gauge whose static CRT pushes frames on the guest's `fs:0`.
- **Both comply:** No AD mentions exceptions. Each module's header writes `movl %0, %%fs:0`. On x86-64 Linux, `%fs` is glibc's TLS, so the same source corrupts the thread pointer. On AArch64 it doesn't build at all. Seven modules will port the mechanism seven ways. A throw from `fs98mfc` then can't unwind a frame that ALERTS registered privately, and native→guest→native nesting has two chains that don't know about each other.
- **Rule (new AD-18, "One exception chain"):** "The sim thread has one SEH registration list, the `ExceptionList` of an arena-resident TIB owned by `fs98win32`. It is the same list the guest sees at `fs:0`. Native code pushes and pops records only through `fs98_seh_enter`/`fs98_seh_leave`. Each record is arena-resident and its handler is a code handle. `RaiseException`, MFC's throw, and guest faults all dispatch through that one list with Win32 two-phase semantics. No module or platform library reads or writes `%fs`, `%gs` or the TEB directly."

### C8. x87 and floating-point state has no owner
- **Pair (a):** a module built with the toolchain defaults ↔ a module built with `-mfpmath=387` on `linux-x86_64`. AD-11 fixes toolchain files but not FP flags, and the spine's "x87 on x86" doesn't say whether `double` arithmetic uses x87 or SSE. CAP-2 (bit-identical to i386, x87) passes for one module and not the other. FMA contraction is also on by default (`-ffp-contract=fast` in GNU C mode, `on` in Apple Clang), which breaks bit-identity again.
- **Pair (b):** the native host ↔ every module. The base control word is `0x037f` (64-bit precision) at Linux process start and `0x027f` under MSVC's CRT. Nobody is assigned to set retail's value, so all x87 code on Linux runs at the wrong precision.
- **Pair (c):** a guest gauge doing `fldcw` (as `_ftol` does, and as retail D3D paths set 24-bit) ↔ the native PANELS service it then calls. On x86 the interpreter's virtual control word and the real x87 control word are two states. On AArch64 there is no real one, so `g3d_mesh_precision` and the guest's control word are two variables.
- **Pair (d):** `fs98sdl` calling SDL or a system audio/video driver, which may change the x87 control word or MXCSR on the calling thread ↔ the module that runs next.
- **Rule (new AD-19, "Floating point"):** "(1) AD-11's toolchain files set one FP flag set for every unit, add-ons included through the exported SDK CMake package: x86 targets use `-mfpmath=387 -ffp-contract=off -fexcess-precision=standard`, and other targets use `-ffp-contract=off`. No unit overrides them. (2) There is one control word: on x86 the real x87 control word, elsewhere `fs98_fpu_cw` in `fs98arena`. The host sets it to retail's measured main-loop value at sim-thread start. (3) `fs98guest` loads the guest control word into that state at every guest→native transition and reads it back at every native→guest transition. (4) Every `fs98win32`/`fs98sdl` call into SDL or system code saves and restores the x87 control word and MXCSR."

### C9. Microsoft resources and data get built into the binaries we ship
- **Pair:** the host and the English DLL modules (resources from `tools/gen_host_resources.py`, which reads `orig/FLTSIM98.EXE`; `.data` images from `gen_*_data.py`, 26 committed `*_data.inc`) ↔ AD-13 setup ("doesn't install the CD's `FLTSIM98.EXE`… replaced language DLLs") plus the SPEC constraint "OpenFS98 ships no Microsoft files".
- **Both comply:** AD-13 keeps the CD's EXE and language DLLs out of the install, so the native host and English modules have to carry their dialogs, menus, bitmaps and strings themselves. They are generated from `orig/` at build time, and the release tarball (AD-12) contains them.
- **Rule (tighten AD-12/AD-13):** "Release artifacts contain no bytes generated from a Microsoft file: no resources and no retail `.data` or `.rdata` images (at least none beyond what a recorded decision explicitly classifies as decompiled program text). Setup copies the CD files whose resources or initial images our units need, `FLTSIM98.EXE` and the replaced language DLLs included, into `Data/Retail/` as resource-only inputs. The resource reader (AD-7) serves `FindResource`/`LoadString`/`DialogBox*` for the native host and modules from them, and their code is never executed or loaded as a module. Whether the committed `*_data.inc` files count as program text is recorded in `decisions.md` before the first release."

---

## High

### H1. Per-module copies of the address macros, mixed `D` types, and test builds where the arena base happens to be 0
- **Pair:** PANELS (`#define D ((uint32_t)(uintptr_t)panels_image.bytes)`, `M32(a) (*(pn_v32 *)(uintptr_t)(uint32_t)(a))`) ↔ ACONTAIN, ATC, SIM1, BOOSTER and ADVDRV, where `D` is a `uint8_t *` and `DA()` yields a native pointer.
- **Both comply:** the Conventions say "through the existing … macros", but there are about 27 private copies with no arena base. A module whose copy forgets the base works wherever the base is 0. That is i386 always (a 4 GB reservation can't exist there, so AD-1 can't be satisfied as written) and x86-64 Linux under the Deferred "arena placement optimisation". It fails on Windows and macOS. AD-3 violations, such as a native `malloc` pointer stored in FS98 memory, are likewise invisible on CAP-1.
- **Rule (tighten AD-1 and the Conventions):** "`M8`…`MD`, `R8`…`RDB`, `DA`, `D`, `RDATA` and `fs98_ptr`/`fs98_handle` are defined once in `include/fs98/arena.h`, and private copies are deleted. `D` and `RDATA` are `uint32_t` arena offsets in every module. On i386 the arena is the process's address space, with base 0, and `fs98arena` allocates only from a reserved sub-range. Every 64-bit test build maps the arena at a nonzero, per-run randomized base, so a base of 0 is a release-only optimisation."

### H2. FRAMELOCK can't live in the host, and can't log there
- **Pair:** the native host with FRAMELOCK "built in" (AD-8) ↔ `fs98win32`, which modules link directly (AD-6). Today's FRAMELOCK (`tools/drive/framelock.c`) works only because "every clock… and every blit… go through the host's import table". It patches IAT entries and detects a pass by the return address `0x402177`. Natively, modules call `fs98win32`'s `timeGetTime`/`BitBlt` with no table in between, and the host sits above `fs98win32`, so `fs98win32` can't call up into the host's log (layer rule, and the Conventions' "one host log through a host service").
- **Further clashes:**
  - `FRAMELOCK_LOG` and the other environment paths are host-native, while AD-10 would push them through DOS mapping.
  - Scripted messages are posted only "as that pass begins", so modal dialogs (their own `GetMessage` loop, with no passes) stall `main-dialogs` and `flights-play`.
  - Blit lines depend on how many `WM_PAINT`s the window system generates, which differs between wine and `fs98win32`, so CAP-1 against the wine references fails for reasons that have nothing to do with the modules.
  - `FRAMELOCK_REGS` and the snapshots hold addresses (see M8).
- **Rule (rewrite AD-8's mechanism):** "FRAMELOCK's clock, timers and blit logging live in `fs98win32`, the only owner of time (H7) and of blits. The host only marks a pass by calling `fs98_framelock_pass()` where retail's main loop reads the clock (`0x402177`). Log lines go to a sink in `fs98arena`, which every layer may call. The tool's environment paths are host-native and never DOS-mapped. The log grammar is frozen in `docs/tools.md` (`bitblt|stretchblt|stretchdibits x y w h size hash palette`, hash defined there), and CAP-1 compares the last presented frame per pass, not the count of intermediate blits. Modal message loops advance the virtual clock and dispatch script lines at each idle wait, with a frozen `idle` line kind."

### H3. Typed structs in the SDK and in `fs98win32` lay out differently per compiler
- **Pairs:**
  - An add-on built with `linux-i386` GCC, where `double` in a struct is 4-aligned, ↔ retail MSVC layout, where it is 8-aligned. Shared-struct offsets differ.
  - `LONG`/`DWORD` defined as `long` (8 bytes on LP64 Linux and macOS, 4 on Windows) ↔ the 32-bit `MSG`.
  - `WCHAR` as `wchar_t` (4 bytes on Linux and macOS) ↔ `DLGTEMPLATE` strings.
  - `mingw` (`-mms-bitfields` by default) ↔ Linux GCC bitfield packing.
  - An 80-bit field declared `long double` (10, 12 or 16 bytes depending on the target).
  - AD-1's "4,764-byte" GLOBAL ↔ `FS98_GLOBAL_SIZE 0x12a8` (4,776). An SDK struct sized 4,764 drops `+0x12a4`, the flag that gates G3D's mesh compile.
  - The mingw-w64 `windows.h` on `windows-x64` (64-bit `HWND`, `WPARAM`, `LPARAM`), already included by 58 files.
- **Rule (new AD-20, "Shared layouts"):** "Shared structs use only `<stdint.h>` fixed-width types, `float`, `double`, and `uint8_t[10]` for 80-bit fields, with no bitfields. Each is declared with explicit padding, `#pragma pack(push, 4)` where retail is 4-packed, and `_Static_assert` offset checks on every target. `WCHAR` is `uint16_t`, and wide literals are built with an `FS98_W` macro, never `L""`. Modules include only OpenFS98's own `include/fs98/win32/windows.h` and never a system or mingw `windows.h`. `fs98win32`'s exports carry an `fs98_` symbol prefix, mapped by that header, so no link line can resolve one to the real `KERNEL32`/`USER32`. GLOBAL is 4,776 bytes (`0x12a8`)." Also correct AD-1's 4,764.

### H4. The screen, top-level windows and the SDL thread
- **Pairs:**
  - WINDOW (undocked views as top-level windows, rectangles in 0x2000ths of "the screen") ↔ `fs98win32` mapping each top-level `HWND` to an SDL window on the real desktop. `SM_CXSCREEN` and window placement then depend on the user's monitor and DPI, and Wayland and macOS refuse absolute positioning.
  - The sim thread (AD-9 doesn't say which thread) ↔ macOS, where SDL video and events must run on the process main thread.
  - MFC's modal loop, which pumps through `PeekMessage`, ↔ any unit that calls `SDL_PollEvent` itself and steals events.
- **Rule (new AD-21, "One virtual desktop"):** "`fs98win32` presents one virtual desktop of a configured size (default 640×480; DPI 96), composited into one SDL window. Every top-level `HWND`, dialogs included, is a child of that desktop. `GetSystemMetrics` and the window rectangles refer to it. SDL events are consumed only inside `fs98win32`'s message retrieval (`GetMessage`, `PeekMessage`, `WaitMessage`, `MsgWaitForMultipleObjects`). The sim thread is the process's main thread on every platform."

### H5. Module scan order, id ownership, and code that runs before the id is known
- **Pairs:**
  - Two native add-ons that both claim the unused id 12 (FRAMELOCK already uses it) or the "OpenFS98 extension module" id (no number is allocated) ↔ AD-13, which defines a winner only between native and guest modules.
  - A native module whose `dlopen` runs constructors, or a PE whose `DllMain` runs, before the conflict is found ↔ AD-13's "skipped". The loser has already run.
  - The host enumerating `Modules/` in `readdir` order ↔ retail's `FindFirstFile` order, which sets load and init order and with it heap layout and behaviour.
- **Rule (tighten AD-13 and AD-2):** "The host lists `Modules/` and loads in ASCII-uppercase-sorted filename order on every platform. It reads each candidate's id before any of its code runs: from the PE's on-disk `Linkage`, or from a native module's `Fs98Image` descriptor (C3). If two candidates share an id, a native module wins over a PE. Between two natives, the alphabetically first wins, and the other is skipped with a log line. Ids 1–39 are retail's, ids 40–63 are reserved for OpenFS98 and listed in `linkage.h`, and id 12 is FRAMELOCK's."

### H6. DOS paths shown to retail code, and the file layout setup and the host share
- **Pairs:**
  - A module that copies `GetModuleFileNameA`/`GetCurrentDirectoryA` into a fixed retail-sized `.data` buffer and parses it with `strrchr('\\')` ↔ `fs98win32` returning `/home/user/.local/share/OpenFS98/...`. The buffer overflows into adjacent state. AD-10 only maps DOS paths *in*.
  - Setup, which writes the install path into the registry file and CFG in its own format, ↔ `fs98win32`'s registry reader ("a file in the install folder", format unspecified). The install folder is also read-only on macOS (inside a signed bundle) and on Windows (`Program Files`), so saved flights and the logbook have nowhere to go.
  - The flight and aircraft list dialogs (`FindFirstFile` order) ↔ the wine-recorded references.
  - Case: a new file created where `FLIGHTS\` and `flights\` both exist.
- **Rule (tighten AD-10):** "Retail code only ever sees DOS paths. `fs98win32` presents the install as `C:\FLTSIM98\` (retail's default), with every returned path in that form, fitting retail's buffers. `C:\FLTSIM98\` is layered: a read-only data root under a writable per-user root, and writes go to the user root. Case-insensitive lookup that matches more than one entry picks the first in ASCII-uppercase order and logs it. A new file keeps the caller's case. `FindFirstFileA` returns entries in ASCII-uppercase order. Names are CP1252, mapped to UTF-8 host names. Setup writes the registry only through `fs98win32`'s `Reg*` calls, into a REGEDIT4-text file that `fs98win32` owns."

### H7. Time, timers and cross-thread data have no single owner
- **Pairs:**
  - `fs98sdl` timestamps (`SDL_GetTicks` used for `MSG.time`, double-click and caret timing) ↔ `fs98win32`'s `timeGetTime`, `GetTickCount`, QPC and `GetLocalTime`. Under FRAMELOCK one of them is virtual and the other isn't.
  - The host's 20 ms `timeSetEvent` callback (`0x401950`, which runs on a worker thread in retail) ↔ AD-9's sim-thread-only rule. Nothing says when it runs natively. FRAMELOCK runs it at the main-loop read, and production has to do the same or the two diverge.
  - The `DirectSoundCreate` shim (SOUND and two guest gauges write their buffers in the arena) ↔ the SDL audio callback, which AD-9 restricts to a ring buffer. So who mixes, and when?
  - MULTIP's queues ↔ the frame. No drain point is defined, so arrival order varies between runs.
- **Rule (tighten AD-9):** "`fs98win32` is the only source of time FS98 code sees, and every Win32 time call and `MSG.time` derives from one clock that FRAMELOCK can make virtual. Multimedia timers run on the sim thread, at the main loop's clock read, in due order. DirectSound buffers are mixed into the ring buffer on the sim thread once per pass, and the audio callback reads nothing else. MULTIP's queues are drained only at the start of a pass."

### H8. Objects with vtables that FS98 code calls through
- **Pair:** SIMSUI and the English DLLs (they store `CWnd *` and call virtuals by vtable offset) and MULTIP (DirectPlay COM), HG2D (DirectDraw) and SOUND (DirectSound) ↔ `fs98mfc` and the `fs98win32` COM shims, which could build their objects on the native heap with native vtables of 8-byte function pointers.
- **Both comply:** AD-1 covers "FS98 memory", but nothing says an MFC or COM object is FS98 memory.
- **Rule (extend AD-1):** "Every object whose address FS98 or guest code can hold, including MFC objects, COM interfaces and `fs98win32`-created structures such as DIB sections and `LockResource` data, lives in the arena at retail's layout. Every vtable is an arena array of code handles (AD-16). Until a GPU renderer exists, `DirectDrawCreate`/`Direct3D*` fail, so HG2D takes its software path."

---

## Medium

### M1. Arena region map and allocator
- **Pairs:**
  - The guest PE loader placing images at `0x10000000` ↔ native image allocations made earlier. Whether a gauge gets relocated then depends on load history, so it's nondeterministic.
  - A null dereference reading `arena[0]` ↔ retail's access violation.
  - A 4-byte read at offset `0xfffffffe` ↔ the end of the reservation.
  - Kept retail bugs that read past an allocation ↔ an allocator with a different block layout.
- **Rule (new AD-22):** "`fs98arena` reserves 4 GB plus 64 KB and owns every allocation in it through one API. Fixed regions: `0x0`–`0xFFFF` is never mapped. The code-handle region is fixed. Guest preferred bases come from their own range, placed first-come but deterministically. The heap uses one deterministic algorithm, identical on every target, with 8-byte alignment and no guard bytes in release builds. Host `+0x20c`, CRT `malloc`/`new`, `HeapAlloc`, `GlobalAlloc` and `LocalAlloc` all allocate from it."

### M2. The layer rule forbids the calls the design depends on
- **Pair:** `fs98win32` dispatching to module `WNDPROC`s, and guest thunks calling into PANELS and the host (SPEC) ↔ "each layer may call only the layers below it". The diagram's `GUEST -.thunks.-> W32` already breaks it.
- **Rule (tighten the paradigm text):** "Link-time dependencies point only downward. Calls upward go only through code handles (AD-16) that the upper layer registered. The log sink and the code-handle registry live in `fs98arena`, so every layer can reach them."

### M3. Compiler flags drift per unit
- **Pair:** the current `Makefile` uses `-Os` for modules and `-O2` for SIMSUI and difftests. `-fno-delete-null-pointer-checks` is set only for DYNAMIC and FE, and `-fno-strict-aliasing` is set only in some rules ↔ a CMake port that keeps a different subset. UB-sensitive decompiled code behaves differently between builds. A CAP-9 add-on built standalone also gets none of the flags.
- **Rule (tighten AD-11):** "One flag set for every unit, published as the `OpenFS98::module` CMake interface target that add-ons must link: `-fwrapv -fno-strict-aliasing -fno-delete-null-pointer-checks` plus AD-19's FP flags. A per-module override is listed in the toolchain file with its reason."

### M4. `keep.h` and the asm entry stubs are keyed to the CPU, not to the ABI
- **Pair:** `win32-retail` (retail callers read registers, so `keep.h` stubs are needed) ↔ `linux-i386` (AD-4 says values are explicit "on every build, i386 included"). `keep.h` switches on `__i386__`, so the native i386 build also gets the stubs, which use mingw's `_` symbol prefix. The six `.S` files (`views_stack.S`, `help_stack.S`, `sim1_*.S`, `mdl_crash_entry.S`) use PE `.def` directives. `FS98_STDCALL` is defined in four places.
- **Rule (tighten AD-4):** "Register-keeping entries and `.S` stubs exist only under `FS98_RETAIL_ABI` (the `win32-retail` target). Every native target, `linux-i386` included, builds the C body with explicit parameters. `FS98_STDCALL` is defined once, in `include/fs98/abi.h`."

### M5. Kept retail bugs that read stale stack
- **Pair:** VIEWS (`views_stack.S`: "Retail's uninitialized Map metric flag and Zoom float both live at entry ESP-16") ↔ `fs98win32`'s message dispatch, which leaves different bytes on the native stack than `USER32` did. Guest gauges have the same problem: native thunks never write the guest stack that retail's PANELS used. The behaviour is then nondeterministic per build.
- **Rule (tighten the Kept-retail-bugs convention):** "A kept retail bug that reads uninitialized stack or heap takes the value retail observed from an explicit, documented source: the arena stack's prior contents under AD-15, or a recorded constant. It is listed in `retail-bugs.md` with that source. It is never left to whatever native stack contents happen to be there."

### M6. Guest PEs: unresolved imports and retail-format modules in `Modules/`
- **Pairs:**
  - A PE loader that fails the load on an import missing from the bounded shim ↔ CAP-7's "230 binaries load", and PANELS's missing-gauge path.
  - AD-2 ("every module is a shared library") ↔ AD-7 sending a third-party retail PE module in `Modules/` to the guest, with SPEC's shim sized for gauges.
- **Rule:** "An import the shim lacks binds to a code handle that logs `dll!name` and raises an access violation when called, and the load succeeds. A PE with code in `Modules/` loads in the guest under AD-13's rules. Its Win32 imports outside the bounded shim follow this rule."

### M7. Handle lifetimes and module-name matching
- **Pair:** a module that keeps an `HWND` or `HMODULE` and checks it later with `IsWindow` or `GetModuleHandle` ↔ an `fs98win32` that reuses handle values. Two aircraft folders that each ship `ATTITUDE.GAU` ↔ `LoadLibraryA` path versus base-name matching (Win9x matches by module name).
- **Rule (tighten AD-6/AD-7):** "`HWND`, GDI and kernel handle values are never reused within a session. An `HMODULE` is the arena address of the image base, for a PE, or of the descriptor's arena image, for a native module. `LoadLibraryA` matches already-loaded modules as Win98 does, and that matching is recorded in `docs/` with a test."

### M8. Snapshot and log comparability
- **Pair:** the `FRAMELOCK_SIM1_SNAPSHOT` and `FRAMELOCK_GLOBAL` dumps (whole `.data` images, `S1SN` records) and `FRAMELOCK_REGS` ↔ CAP-1 against retail and CAP-2 against CAP-1. Relocated dwords are retail VAs on one side and arena offsets or handles on the other, so every pointer field counts as a difference.
- **Rule (tighten AD-8):** "Dumps mask each image's relocated dwords to null versus non-null using its relocation list, and log no handle values. Alternatively, and preferred: native images are placed at their retail `.data` VAs in the arena, so their pointer fields equal retail's."

---

## Low

- **L1. GLOBAL size in AD-1.** 4,764 is wrong. The code says `0x12a8` = 4,776 bytes. Covered by H3's rule, but fix the text itself.
- **L2. Id 19 (GAUGES).** PANELS imports it and nothing provides it. State that it stays null on native builds, as CAP-8's unknown-import rule already allows, so no one builds a stand-in.
- **L3. Unaligned access through the `may_alias` macros on AArch64.** It works on normal memory today. Add "arena pages are never mapped device or uncached, and the macros carry `aligned(1)`" to AD-1 so a future strict-alignment target doesn't break silently.
- **L4. `OutputDebugStringA` from guest gauges.** Route it to the AD-wide log sink (M2), so no unit writes to stderr in violation of the Logging convention.
- **L5. AD-12 setup sources.** A physical drive or mounted folder gives Joliet or Rock Ridge names in mixed case, while ISO 9660 level 1 gives `NAME.EXT;1`. Setup strips `;1` and writes names in the case H6 chooses, so the two sources install identical trees.

---

## Summary of new and tightened rules

| # | Rule | Severity |
| --- | --- | --- |
| AD-14 | ABI headers 32-bit (`fs98_addr`/`fs98_code`), static offset asserts | critical |
| Preamble | spine overrides the stale SPEC constraints until SPEC is updated | critical |
| AD-3′ | `LoadLibraryA` sole image owner; `Fs98Image` descriptor; fresh image per load; no constructors | critical |
| AD-15 | arena stack (shared with guest `esp`) and `fs98_addr` pointer parameters; `fs98_str` | critical |
| AD-16 | code handles: stub region, signatures, idempotent, never recycled, never cast | critical |
| AD-17 | divide helpers everywhere; no trap reliance; guest uses the same helpers | critical |
| AD-18 | one arena-resident SEH chain; no `%fs` access | critical |
| AD-19 | FP flags, single control word, saved at thunks and around SDL | critical |
| AD-12/13′ | no Microsoft-derived bytes in releases; resources read from CD copies | critical |
| AD-1′ | shared macros, `uint32_t` `D`, randomized nonzero base in tests; objects with vtables in the arena | high |
| AD-8′ | FRAMELOCK in `fs98win32`, host marks passes, frozen grammar, modal idle ticks | high |
| AD-20 | shared struct layout, own `windows.h`, `fs98_` export prefix | high |
| AD-21 | one virtual desktop, SDL on the main thread | high |
| AD-13′ | sorted scan, id read before code runs, id ranges | high |
| AD-10′ | virtual `C:\FLTSIM98\`, user overlay, sorted enumeration, registry format | high |
| AD-9′ | one clock, timers and mixing on the sim thread, queue drain point | high |
| AD-22 | arena region map, deterministic allocator, null page | medium |
