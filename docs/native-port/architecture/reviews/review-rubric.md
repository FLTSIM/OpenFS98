# Spine review (rubric): OpenFS98 native port

Reviewed: `architecture/ARCHITECTURE-SPINE.md` (draft, 2026-11-15), against `.memlog.md`, `../SPEC.md`, `../decisions.md`, `AGENTS.md`, and the code in `src/` and `include/fs98/`.
Out of scope, as asked: the known contradictions with SPEC constraints (pointer-width fields, GLOBAL native, var mirror sync), which already have a SPEC update pending.

## Verdict

The paradigm is sound and the big choices are made and recorded: one arena, one loader, our own Win32 on every platform, frame comparison built into the host. As a build contract, though, it leaves the most common porting act to each builder: carrying a native address across a module boundary. It says what must not cross, but not what to use instead or how a violation would be caught. Four other surfaces every module touches are also undecided: the CRT, the Win32 headers and symbol names, the DirectX/ACM/DirectPlay surfaces, and our own modules' resources. Two more are undecided too: the integer-divide repair and the floating-point code generation that CAP-2 bit-identity depends on. Fix the criticals and highs before epics are cut.

## Critical

### C1. AD-3 bans native stack and statics from crossing a boundary, but gives no replacement and no enforcement
- **Evidence:** `src/` has 372 `(uint32_t)(uintptr_t)` casts. Many of them hand a stack or C-static address to a service: `demo.c:367` `&wrote`, `demo.c:427` `H_LOADSTRING(id, (uint32_t)(uintptr_t)text, 0x100)` on a local buffer, `chain.c:224`. Module images are C statics today (`#define D ((uint32_t)(uintptr_t)demo_image.bytes)`, `analysis.c:66`).
- **Why it is critical:** Every module porter has to choose what replaces these: a per-call arena scratch allocator, an arena-resident "guest-style" stack, static arena buffers in the module image, or heap alloc/free. That is the exact divergence the spine exists to prevent. Worse, it can't be caught where porting starts. On linux-i386 (CAP-1) a 4 GB arena can only be the whole address space with base 0 (also unstated, see M1), so every violation works there and only shows up on CAP-2 as silent truncation.
- **Fix:** Add an AD such as "Arena scratch for cross-boundary temporaries". It needs three things:
  - (a) One mechanism, for example an arena-resident sim-thread stack in `fs98arena` with `fs98_scratch_push(size)` / `fs98_scratch_pop(mark)`, or a rule that every buffer passed to a service lives in the module's arena image.
  - (b) `fs98_handle(ptr)` is the only legal native-to-handle conversion, and it asserts that the pointer lies inside the arena on every build.
  - (c) Enforcement: a CMake lint check that fails on any `(uint32_t)(uintptr_t)` outside `src/platform/arena/`. On x86-64 debug builds the arena also gets a base above 4 GB, so a truncated pointer faults instead of aliasing.

### C2. The address macros are not "existing" shared macros; each module defines its own, assuming base 0
- **Evidence:** `M8`/`M32`/`CALL` are redefined privately in every module (`host.c:59`, `window.c:50,78`, `main.c:42,60`, `panels.h:29-78`, `controls.h:42-80`, and more) as `*(T *)(uintptr_t)(uint32_t)(a)`, with no arena base. `CALL(m, off, fn)` reads the slot as a native function pointer (8 bytes on x86-64, from a 4-byte slot), not as a code handle.
- **Why it matters:** The conventions row "through the existing macros" ratifies something that can't work under AD-1 off i386. Each builder will patch their own copy differently: add the base or not, resolve handles in `CALL` or not. The Deferred item "arena at base 0" claims it changes no rule, and that is only true if the macros are already base-relative. macOS arm64's 4 GB `__PAGEZERO` forbids base 0 anyway.
- **Fix:** Add a rule: "`include/fs98/arena.h` is the only definition of `M8`/`M16`/`M32`/`R32`/`DA` (base-relative) and `CALL` (resolves a code handle via `fs98arena`, typed by the callee prototype). Module-local definitions are deleted as each module is ported." Add a lint check: no `#define M32` outside that header. Reword the Deferred item to "base 0 is an optimisation inside `arena.h` only".

## High

### H1. No decision on which C runtime modules use
- **Evidence:** Modules link `-lmsvcrt` and deliberately use MSVCRT's own formatting (`host.c`: `__USE_MINGW_ANSI_STDIO 0 /* MSVCRT's own sprintf, as retail's */`). AGENTS.md requires the `memmove` ecx=7 quirk. `_ftol`, `rand`, `qsort` and `strtod` all behave differently in glibc and libSystem: exponent digits in `%e`, the `rand` LCG, and the order `qsort` leaves equal elements in.
- **Why it matters:** The layer table puts a "CRT subset" in `fs98win32`, but no Rule says modules must use it. A porter who calls libc `sprintf` or `rand` breaks frame identity (CAP-1/2), and the cause is hard to trace.
- **Fix:** Extend AD-6 or add a rule: "Modules, `fs98mfc` and the guest use only `fs98win32`'s MSVCRT-behaviour CRT (formatting, `rand`, `qsort`, string-to-number conversion, `_ftol`, `memmove`-ecx). Platform libc is allowed only inside `src/platform/`." Back it with a link-time check that module objects import no libc symbols except an allowlist (`memcpy`, `memset`).

### H2. The Win32 headers and exported symbol names are undecided
- **Evidence:** 58 files include `<windows.h>` and 161 lines use `HWND`/`WPARAM`/`LPARAM`/`LRESULT`. On x86-64 those types are pointer-width, which contradicts AD-6's "32-bit handles, retail struct layouts".
- **Why it matters:** If `fs98win32` exports the bare names `CreateFileA`, `sprintf` and `memmove`, it collides with libc through ELF symbol interposition (SDL3 and libc would bind to ours or the reverse). On Windows x64 it is ambiguous against `kernel32` and `msvcrt` import libraries.
- **Fix:** Add to AD-6: "Native builds include `include/fs98/win32.h` (arena-typed `HWND`/`WPARAM`/`LPARAM` = `uint32_t`, retail struct layouts), never the platform `<windows.h>`. `fs98win32` exports prefixed symbols (`fs98w_CreateFileA`), and the header maps the Win32 names onto them with macros. The guest thunk table binds to the prefixed symbols." State that the `win32-retail` target keeps the real `<windows.h>`.

### H3. The DirectX, ACM, DirectPlay, registry and shell surfaces have no home
- **Evidence:** The host and HG2D link `ddraw` (`host_rt_ddraw.c`). SOUND and the host use `msacm32`/`winmm`/`dsound`. MULTIP uses DirectPlay, `controls_input.c` uses `joyGetPos*`, and `advapi32`, `shell32`, `comctl32` and `imm32` are all linked.
- **Why it matters:** AD-5 covers USER32/GDI32 and AD-6 lists the guest's imports, but nothing says where the native modules' COM interfaces (vtables, `this`) and these other DLLs come from. The SOUND, HG2D, CONTROLS and MULTIP builders will each choose differently: COM shims, or a rewrite onto `fs98sdl`. CAP-10 and CAP-11 hang on this.
- **Fix:** Add an AD such as "Every Win32/DirectX DLL a module or the guest imports is served by `fs98win32` with retail-shaped interfaces". COM objects live in the arena with vtables of code handles. List the set: DDRAW (2D surfaces only; the Direct3D device is out until the GPU renderer, so say that the native mesh-compile path is unreachable), DSOUND, DPLAY, WINMM (`joy*`, `mmio`, timers), MSACM32, ADVAPI32 registry, SHELL32, COMCTL32 and IMM32, each backed by `fs98sdl` or files. Modules don't call `fs98sdl` directly, which keeps the layer graph honest.

### H4. The host's integer-divide repair, which retail modules rely on, isn't addressed
- **Evidence:**
  - AGENTS.md: "retail modules divide into overflow and rely on it".
  - `host_rt_divide.c` repairs faults by decoding the F6/F7 instruction at the fault address.
  - HG2D reproduces the repair inline (`hg2d_polygon.inc:7`, `hg2d_draw.inc:29`).
- **Why it matters:** On arm64 a division by zero gives 0 and never traps, and Linux and macOS raise SIGFPE rather than SEH. A builder who misses a divide will fly differently per platform. Guest gauges divide too, and the host repaired their faults in retail.
- **Fix:** Add a rule: "Native code never relies on a divide trap. Every retail `div`/`idiv` that can fault becomes a call to `fs98_div*` saturating helpers in `include/fs98/` (retail's repair values: signed 32-bit to ±INT32_MAX, unsigned 16-bit to AX=DX=0xffff, and so on). The guest interpreter applies the same repair to guest `div`/`idiv`. `host_rt_divide.c`'s fault filter exists only in `win32-retail`." Add a debug-build check that traps on any hardware divide fault, so a missed site fails loudly.

### H5. Floating-point code generation for CAP-2 bit-identity isn't fixed
- **Evidence:** The conventions row says only "x87 on x86". On x86-64, GCC and Clang compute `double` and `float` in SSE by default, while i386 uses the x87 under retail's control word (24-bit for the mesh compiler, per SPEC and `decisions.md`). 355 lines touch the x87 or the control word.
- **Why it matters:** Each module's flags decide whether CAP-2 can be bit-identical to CAP-1. One module built with SSE breaks identity in a way that's hard to trace.
- **Fix:** Add to AD-11: "All tier-1 x86 toolchain files (`linux-i386`, `linux-x86_64`, `windows-x64`, `macos-x86_64`) compile FS98 code with `-mfpmath=387` and the same `-O` level and `-ffp-contract=off`. FP flags are set only in the toolchain files, never per module." Name the arm64 flags (tolerance tier) in the same place.

### H6. fs98mfc's object model and the add-on dialog API (CAP-5, CAP-6, CAP-9) are undecided
- **Evidence:** Retail modules (SIMSUI, World, Options) call MFC42 by ordinal with `thiscall`, and MFC objects carry vtable pointers and retail layouts that decompiled code reads by offset (SPEC). CAP-6 and CAP-9 need one add-on source that shows a dialog through retail MFC42/SIMSUI and through `fs98mfc`.
- **Why it matters:** AD-4 covers ImportTable services, not MFC calls. Builders will diverge on whether objects live in the arena, how vtables hold code handles, how `this` is passed, and whether `fs98mfc` is C or C++.
- **Fix:** Add an AD: "`fs98mfc` is C. Objects live in the arena at retail layouts. A vtable is an arena array of code handles, `this` is an explicit first `uint32_t`, and entries are named by ordinal (`fs98mfc_<ordinal>_<name>`)." Ship an add-on header in `include/fs98/` that maps one dialog API onto MFC42 ordinals in the retail build and onto `fs98mfc` natively. Map CAP-6 and CAP-9 to it.

### H7. Our own modules' resources have no native source
- **Evidence:** Our modules embed PE resources (`alerts-res.o`, `help-res.o`, linked into the DLL), and code calls `LoadString`/`FindResource` on its own module. AD-7 only covers resource-only PEs from the CD.
- **Why it matters:** A `.so` or `.dylib` has no `.rsrc`, so each porter will choose something different (a C array, a side file, a companion PE).
- **Fix:** Add to AD-7: "Each native module carries its compiled `.res` as a blob in a named exported symbol, `fs98_resources`. `fs98win32`'s resource reader serves `FindResourceA`/`LoadStringA`/`DialogBoxParamA` from that blob for native modules and from `.rsrc` for PEs, with one parser for both."

### H8. AD-4's "every build, i386 included" conflicts with the `win32-retail` target
- **Evidence:** `win32-retail` builds our modules to run inside retail `FLTSIM98.EXE` beside retail modules (difftests, CAP-9 retail add-on builds). Retail callers there read the registers `keep.h` keeps (ATC, DYNAMIC, HG2D, MAIN, NUMERIC, WEATHERM).
- **Fix:** Reword the rule: "on every native build. `win32-retail` keeps the `FS98_ENTRY` wrappers from `keep.h` around the explicit-parameter implementations, so retail callers still see their registers." Keep `keep.h`, and say which targets compile it.

## Medium

- **M1. The arena on i386 is undefined.** A 4 GB arena can't be reserved inside a 32-bit process. **Fix:** in AD-1, say "on i386 the arena is the process address space (base 0); module images, heap and guest images are placed by `fs98arena`". Note that this hides C1 violations on CAP-1, which is why C1 needs its own check.
- **M2. Code-handle minting and typed calls aren't specified.** The code registers native functions at runtime: `demo.c:3273` stores `demo_event_entry` in Linkage, `panels.c:5705` returns `tree_unmark_entry`, and `simsui_native.c:77` relocates code sites. **Fix:** in AD-1, specify the `fs98arena` API: `fs98_code(fn, sig)` mints a handle, and calling a handle carries a signature (arity and callee-pops) so a guest target can be entered correctly. Say whether handles are stable across runs, since they get stored in saved state and frame logs.
- **M3. The load order for image relocation isn't fixed.** Modules fix up their own image (SIMSUI `bind_image`, relocation lists in FE, FACIL, ADVDRV and MDL). **Fix:** add to AD-3: "host allocates → copies → sets `D`/`RDATA` → calls the module's `fs98_bind(D, RDATA)` → `Linkage` init". Without this, each module chooses where its fixups run.
- **M4. The arena heap's coverage isn't stated.** "The FS98 heap" doesn't say that `malloc`/`new` in modules (`mpdialog_flow.c`, `host.c`), `fs98mfc` allocations, `GlobalAlloc`/`HeapAlloc` in `fs98win32` and the guest CRT's heap all come from one `fs98arena` allocator. **Fix:** add that sentence to AD-3, and add the libc-import check from H1 so that libc `malloc` in a module fails to link.
- **M5. AD-9 doesn't cover Win32 callback APIs.** WINMM `timeSetEvent`, waveOut and DirectSound notifications, and MULTIP's DirectPlay receive threads all call back on other threads in retail. **Fix:** "`fs98win32` delivers every Win32 callback and timer on the sim thread at a defined point in the frame, never from an SDL or worker thread."
- **M6. Native ids for extension modules are unassigned.** SPEC says native-only capabilities arrive as new module ids. **Fix:** reserve an id range for OpenFS98 extension modules, and a sub-range or registry file (`docs/module-ids.md`) for third-party add-ons, so two feature builders can't pick the same id. Also decide the SDK version stamp.
- **M7. AD-8's baseline once i386 is removed.** SPEC removes CAP-1 once CAP-2 matches, but AD-8 compares CAP-3 against "an i386 run of the same commit". **Fix:** record references from the bug-fixed i386 build once (like the retail references), with a manifest committed to git (flight, commit, CD pressing per `building.md`, config, checksums). After that, CAP-2 and CAP-3 compare against frozen logs.
- **M8. The testing strategy stops at whole flights.** It doesn't say how a module is verified natively before the 74 flights, how `main-dialogs` and `flights-play` drive dialogs without wine (today they post Win32 messages from outside the process), or how CAP-7's "guest gauge matches decompiled native" comparison runs. **Fix:** add a testing AD. Native per-module vector diffs run i386-vs-x86-64 in one process, or through FRAMELOCK input scripts. The dialog drivers move onto FRAMELOCK's input script. The guest-vs-native gauge comparison is a host test mode (an `allgaugesdiff` equivalent running in the native host).
- **M9. The writable user-data location is undecided.** "Registry served from a file in the install folder" fails in a macOS `.app` bundle and in Windows Program Files. Saved flights and `FLTSIM98.CFG` have the same problem. **Fix:** decide one per-platform user-data directory (`SDL_GetPrefPath`), mapped by AD-10's path layer to the DOS paths FS98 writes. Name that directory in AD-12.

## Low

- **L1.** The spine has no Open Questions section. The memlog's pending SPEC update (contradicted constraints, plus the new setup capability) should be listed in the spine, so builders know that SPEC lines like "pointer-width fields" are superseded.
- **L2.** Holes in the capability map: CAP-6's "in retail through MFC42/SIMSUI" and CAP-9's dual-build SDK have no governing AD (see H6). CAP-10 should cite the new DirectX/WINMM AD (H3), and CAP-11 should cite DirectPlay.
- **L3.** Add a conventions row: no `long`, `size_t` or pointer types in any struct that lives in FS98 memory, since LLP64 and LP64 differ (17 bare `long`/`size_t` uses today).
- **L4.** The deferral "CI decided at first release" is risky, because AD-8's cross-target identity needs six targets run on one commit. Defer the CI service, but decide now that a scripted `tools/` target runs the 74-flight check per target and writes a comparable report.
- **L5.** AD-13 says the native module wins on an id clash. Say whether a third-party native add-on may replace one of ours on purpose (and how), or that it always loses with a log line.
- **L6.** In the layer graph, `MOD --> HOST` means modules call the host by link-time symbols, which contradicts line 47 ("only through `ImportTable` slots"). Label the edge "via ImportTable" or drop it.

## Checklist summary

| Criterion | Result |
| --- | --- |
| Fixes the real divergence points | Partial: misses C1, C2, H1–H3, H5–H7 |
| Rules are enforceable and prevent the divergence | Partial: AD-1 and AD-3 have no check (C1); AD-4 is wrong for `win32-retail` (H8) |
| Nothing Deferred lets units diverge | One item: arena base 0 (C2) |
| Ratifies the existing codebase | Partial: the "existing macros" (C2), `windows.h` (H2), MSVCRT (H1) and resources (H7) don't match the code |
| Covers CAP-1..11 | CAP-6, CAP-9 and CAP-10/11 are thin (H3, H6) |
| Deployment, operations, testing decided or deferred | Deployment partial (M9); operations: logging only; testing: whole-flight only (M8) |
