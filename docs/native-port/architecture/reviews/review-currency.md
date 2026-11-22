# Review: currency and reality checks, ARCHITECTURE-SPINE.md

- **Reviewed:** `ARCHITECTURE-SPINE.md` and `.memlog.md` as of 2026-11-15T14:00
- **Lens:** was each committed decision web-researched or checked against reality, or asserted from memory? Are the versions current, do the named technologies still exist, and do they fit?
- **Date:** 2026-11-15

## Verdict

The named technologies exist and are current, and they fit. SDL3 3.4.x, CMake 4.4.x, libcdio (GPL-3.0-or-later) and mingw-w64 are all live, and each covers the job the spine gives it. The memlog shows real version research for CMake and SDL3, though the versions it found are now a few point releases old. Two load-bearing claims did not survive a reality check:

1. A 4 GB reserved arena cannot exist in an i386 process.
2. An 80-bit `long double` does not make x86-64 C code compute the way x87 does. Apple Clang on x86-64 cannot generate x87 code for `float` or `double` at all.

The libcdio "vendored copy" is really a third-party fork built for another project.

## What was verified

| Claim in spine/memlog | Result | Evidence |
| --- | --- | --- |
| SDL3 3.4.x is current | **Yes.** 3.4.0 shipped 2026-01-01; point releases 3.4.2 through 3.4.18 followed. 3.5 is the development branch, so 3.4 is still the stable line. The memlog's "3.4.14 newest, Aug 2026" is stale. | [Phoronix via mirror](https://backiee.wasmer.app/http_www_phoronix_com/news/SDL-3_4-Released), [SDL 3.4.0 announcement](https://discourse.libsdl.org/t/announcing-sdl-3-4-0/65438), [Fedora SDL3-3.4.14 bug](https://bugzilla.redhat.com/show_bug.cgi?id=2510846), [GameDev.net 3.4.16](https://gamedev.net/news/sdl-release-3416-released-r5499/), [GameDev.net 3.4.18](https://gamedev.net/news/sdl-release-3418-released-r6179/), [Fedora mingw64-SDL3 (3.4.18)](https://packages.fedoraproject.org/pkgs/mingw-SDL3/mingw64-SDL3/index.html) |
| SDL3 covers window, input, joystick/haptics, audio, timers | **Yes.** These are core SDL3 subsystems. The haptic subsystem is not thread-safe, which AD-9's single sim thread already handles. | [SDL3 CategoryHaptic](https://wiki.libsdl.org/SDL3/CategoryHaptic) |
| SDL3 builds with mingw-w64 cross-compilation | **Yes.** SDL ships `build-scripts/cmake-toolchain-mingw64-x86_64.cmake` (and an i686 one). Fedora cross-builds `mingw64-SDL3` and `mingw32-SDL3` 3.4.x on Linux. | [SDL3 README-windows](https://wiki.libsdl.org/SDL3/README-windows), [SDL3 INTRO-mingw](https://wiki.libsdl.org/SDL3/INTRO-mingw), [Debian copy of i686 toolchain file](https://sources.debian.org/src/libsdl3/3.2.10%2Bds-1/build-scripts/cmake-toolchain-mingw64-i686.cmake), [Fedora mingw-SDL3-3.4.14](https://bugzilla.redhat.com/show_bug.cgi?id=2510847) |
| Linux i386 is a supportable SDL3/GCC target | **Yes, as a userland target on a 64-bit kernel.** Debian trixie and sid build libsdl3 for i386. Fedora withdrew its proposal to drop i686 multilib (June 2025), but it has shipped no i686 kernel or installer since F31. GCC still supports `-m32`. See finding M-2 for what this means for the toolchain. | [Debian libsdl3-0 (trixie, i386 row)](https://packages-pkgmirror-csail.debian.org/trixie/libsdl3-0), [Fedora F44 drop-i686 proposal](https://discussion.fedoraproject.org/t/f44-change-proposal-drop-i686-support-system-wide/156324), [Fedora holds off (Linuxiac)](https://linuxiac.com/fedora-holds-off-on-dropping-i686-citing-community-needs/), [The Register](https://www.theregister.com/2025/07/01/fedora_43_i686_32bit/) |
| CMake 4.4.x is current | **Yes.** 4.4.0 shipped 2026-07-14, 4.4.1 on 2026-07-30 and 4.4.3 on 2026-09-03. No 4.5 release candidate was found. The memlog's "4.4.1 newest" is stale. | [CMake 4.4 release notes](https://cmake.org/cmake/help/latest/release/4.4.html), [4.4.0 announce](https://mail.kde.org/pipermail/kde-devel/2026-July/004349.html), [4.4.1](https://www.kitware.com/cmake-4-4-1-available-for-download/), [4.4.3 (2026-09-03)](https://discourse.cmake.org/t/cmake-4-4-3-available-for-download/15821) |
| libcdio reads ISO 9660 and BIN/CUE | **Yes.** Checked in the local source: `lib/driver/image/bincue.c`, `cdio_open_bincue()`/`cdio_open_cue()` (`include/cdio/device.h:520,590`) and `iso9660_fs_readdir(CdIo_t*, ...)` (`include/cdio/iso9660.h:1087`). Together they read the ISO 9660 file system inside a BIN/CUE image. `iso9660_ifs_readdir`/`iso9660_open_fuzzy` handle a plain ISO. | local source; [libcdio-devel thread](https://lists.gnu.org/archive/html/libcdio-devel/2005-03/msg00001.html), [iso9660_fs.c](https://git.stg.centos.org/source-git/libcdio/blob/c8/f/lib/iso9660/iso9660_fs.c) |
| libcdio's GPL fits GPL-3.0 | **Yes.** `COPYING` is GPLv3, and the source headers say "version 3 ... or (at your option) any later version". OpenFS98's `LICENSE` is GPLv3. Compatible. | local `COPYING`, `lib/iso9660/iso9660_fs.c`, `OpenFS98/LICENSE` |
| libcdio's latest release | **2.3.0 (2025-12-23)**, the latest upstream release found. It is packaged by Arch, FreeBSD, ALT and NetBSD, and its tarball is hosted on GitHub. | [FreshPorts](https://www.freshports.org/sysutils/libcdio/), [Arch](https://archlinux.org/packages/extra/x86_64/libcdio/flag/) |
| mingw-w64 x86_64 `long double` is 80-bit x87 | **Yes** (with GCC). MSVC's and UCRT's `long double` is 64-bit, so `printf("%Lf")` needs `__USE_MINGW_ANSI_STDIO=1`. | [mingw-w64 bug 520](https://sourceforge.net/p/mingw-w64/bugs/520/), [Wine dev note 2024](https://list.winehq.org/mailman3/hyperkitty/list/wine-gitlab@winehq.org/message/MQDCP4RJXKUJ4FINFE27LGKDZL232KJW) |
| GCC/Clang x86-64 Linux `long double` is 80-bit | **Yes** (SysV ABI). | [Wikipedia: long double](https://en.wikipedia.org/wiki/Long_double) |
| Apple Clang macOS x86-64 `long double` is 80-bit | **Yes.** It is 80-bit precision in 16 bytes of storage. On arm64 macOS it is plain `double`. | [Swift forums](https://forums.swift.org/t/whats-the-difference-between-clongdouble-and-double/57076), [Apple dev forums](https://developer.apple.com/forums/thread/673482), [GCC PR 107315](https://gcc.gnu.org/pipermail/gcc-bugs/2022-October/800993.html) |
| 4 GB reserved arena works on macOS arm64 | **Plausible, not demonstrated.** WebAssembly runtimes (Wasmtime and others) routinely reserve 4 GiB or more of PROT_NONE address space on 64-bit hosts, Apple Silicon included. Apple Silicon has 16 KB pages, and macOS enforces W^X. No source tested exactly this layout. | [Wasmtime config](https://docs.wasmtime.dev/api/src/wasmtime/config.rs.html), [fast Wasm memory write-up](https://iangrunert.pages.dev/2023/08/25/fast-webassembly-memory.html), [Wine 16K-page loader](https://mail.winehq.org/mailman3/hyperkitty/list/wine-commits@winehq.org/thread/4YCVLWTVNYLDXRYN4NLJYEN2UW2EH4FN), [QEMU PROT_NONE-first fix](https://git.cs.tu-dortmund.de/SYS-OSS/FRET-qemu/commit/b7da02dad0f25928e688a5dce74a0f10fa120594) |

## Findings

### C-1 (critical): a 4 GB arena can't be reserved in an i386 process (AD-1)

AD-1 says all FS98 memory lives in "one reserved 4 GB arena" per process, and it binds every target. That includes `linux-i386` (CAP-1, the first port step and the baseline for CAP-2/3) and `win32-retail`. A 32-bit process has at most 4 GB of address space, and the executable, libc, SDL, the stack and the kernel split (3 GB user space on a 32-bit kernel) all take part of it. A contiguous 4 GB reservation will fail. This is a hard fact, not something that changes with versions. The memlog never treats i386 separately, so the rule was written for 64-bit hosts only.

- **Sources:** basic i386 address-space limits; the spine's own Deferred item ("mapping at a low address so the base is 0 on x86-64 Linux") shows the base-0 case was considered only for x86-64.
- **Fix:** amend AD-1 so that on i386 the arena *is* the process address space: base 0, offsets equal native pointers, nothing reserved, and FS98 allocations come from a normal heap. On 64-bit hosts the arena stays a reserved 4 GB region. Code handles and `fs98_ptr`/`fs98_handle` stay the same API on both. Say this explicitly so CAP-1 doesn't ship with an arena allocator that fails at startup.

### H-1 (high): an 80-bit `long double` doesn't make x86-64 builds compute like x87; Apple Clang x86-64 can't emit x87 code for `float`/`double` (Floating point convention, SPEC CAP-2/3)

The convention says "x87 on x86 (`long double` is 80-bit there)". The memlog concludes that "tier-1 bit-identity can keep x87 arithmetic" from the size of `long double`. That inference doesn't hold:

- On x86-64, every GCC and Clang target (Linux, mingw-w64, macOS) compiles `float`/`double` arithmetic to SSE2. It doesn't use x87 and ignores the x87 precision-control word. Retail's MSVC code, and our i386 build, evaluate in x87 registers under precision control (`0x007f`, 24-bit, for mesh compiles, per `decisions.md`). SSE rounds every float operation to float and every double operation to double, while x87 rounds to the precision-control width with a wider exponent. Float-heavy code will differ in the last bits.
- GCC accepts `-mfpmath=387` on x86-64, but it may still pick SSE where it judges the results equivalent, and the ABI still passes floats in SSE registers.
- Clang rejects `-mfpmath=387` while SSE is enabled ("the '387' unit is not supported with this instruction set"). SSE can't be disabled on x86-64 without breaking the ABI. So the `macos-x86_64` (Apple Clang) target, which the SPEC places in the bit-identical CAP-2/3 tier ("x86-64 GCC/Clang builds are bit-identical"), has no compiler switch that gives x87 semantics. The only routes are explicit `long double` everywhere, which still isn't precision-controlled unless the control word is also set, or the inline asm.
- `long double` being 80-bit is true on all three x86-64 targets (see the table above). It only helps code that is explicitly written with `long double` or x87 inline asm.

- **Sources:** [GCC x86 options (`-mfpmath`)](https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html), [GCC bug: -mfpmath=387 still uses SSE](https://gcc.gnu.org/pipermail/gcc-bugs/2003-May/076205.html), [clang -mfpmath test](https://llvm.googlesource.com/clang/+/master/test/Frontend/mfpmath.c), [FreeBSD: clang and -mfpmath=387 on amd64](https://lists.freebsd.org/pipermail/freebsd-toolchain/2010-December/000036.html), [LLVM list: x86-64 calling convention needs SSE](https://lists.llvm.org/pipermail/llvm-dev/2011-February/037990.html), [Wine MR 8487 discussion on x87 vs SSE behaviour](https://list.winehq.org/archives/list/wine-gitlab@list.winehq.org/message/VDKZ3VRKGBM3L2DWWLQ45DRHKD6TSGLW)
- **Fix:** before CAP-2 is planned, run a reality check: build one float-heavy module (G3D or SIM1) for x86-64 with GCC `-mfpmath=387`, and with Clang as is, then FRAMELOCK-compare it against the i386 build. Then restate the convention as one of three options:
  - (a) Bit-identity on x86-64 holds only for GCC with `-mfpmath=387` plus the existing control-word asm, and the Apple Clang `macos-x86_64` target moves to the tolerance tier.
  - (b) The hot paths go through an explicit x87 layer (inline asm or `long double` plus the control word) on every x86 build.
  - (c) Accept SSE results and move CAP-2/3 to tolerance.

  Update SPEC line 61 to match, because it currently promises Clang bit-identity.

### H-2 (high): "libcdio as vendored in `~/source/libcdio`" isn't vendored; it's a third-party fork built for another project (Stack, AD-12)

The local tree is `https://github.com/Zizin13/libcdio` at `bb5a5682` (2026-04-14). It is upstream master from after the 2.3.0 release (up to 2026-02-24) plus 7 fork commits that add a Zig build (`build.zig`, `mise.toml`, zig 0.15.2) for [FatalDecomp/ROLLER](https://github.com/FatalDecomp/ROLLER), per its `ZIG_BUILD_PLAN.md`. It has no tag, uncommitted files (`.zig-cache/`, `ZIG_BUILD_PLAN.md`) and a commit called "adding all platform driver stubs", which in that build may stub out physical-drive access. Nothing in OpenFS98 references it. Upstream libcdio has no CMake build (autotools only), so AD-11's "CMake builds every target" has no path to libcdio as it stands.

- **Sources:** local `git log`/`git remote`/`configure.ac` (`RELEASE_NUM 2.3.0`); [upstream 2.3.0 packaging](https://www.freshports.org/sysutils/libcdio/) (tarball `libcdio-2.3.0.tar.bz2`, SHA256 `53e83d28…a7ed`)
- **Fix:** pin the upstream **libcdio 2.3.0** release tarball, by URL and SHA256, the same way SDL3 is pinned, rather than this fork. Decide how CMake builds it: either `ExternalProject_Add` with autotools, which is awkward for mingw cross builds, or a small `CMakeLists.txt` covering only `libiso9660` and the `bincue` image driver, which is all setup needs. Read physical drives and mounted folders through `fs98win32` file access (AD-10), not through libcdio device drivers.

### M-1 (medium): the `macos-x86_64` target is on a platform Apple is winding down (AD-11, CAP-3)

Apple has said macOS Tahoe 26 is the last release for Intel Macs; macOS 27 supports Apple Silicon only. Rosetta 2 is reduced from macOS 28. Xcode 26 already comes in an Apple-Silicon-only build. Nothing found says Xcode drops x86_64 *targeting* yet. The spine adopts `macos-x86_64` with no note on its lifespan, and H-1 already puts its bit-identity in doubt.

- **Sources:** [Apple macOS 26.4 release notes](https://developer.apple.com/tutorials/data/documentation/macos-release-notes/macos-26_4-release-notes.md), [Tom's Hardware](https://www.tomshardware.com/software/macos/macos-tahoe-is-the-last-release-for-intel-macs-apple-silicon-exclusivity-will-mark-end-of-hackintoshes), [9to5Mac: Apple-Silicon-only Xcode 26](https://9to5mac.com/2025/08/05/apple-now-offers-a-separate-xcode-26-beta-build-for-apple-silicon-macs/), [Wccftech on Rosetta 2 / macOS 28](https://wccftech.com/macos-tahoe-26-will-be-the-final-intel-compatible-update-as-apple-phases-out-rosetta-2-with-macos-28-ending-support-for-x86-apps/)
- **Fix:** keep the target, but record a minimum deployment target (macOS 26 or lower) and treat it as a best-effort target that is dropped when Xcode drops x86_64. Build it as a slice of a universal binary (`CMAKE_OSX_ARCHITECTURES`) rather than a separate toolchain.

### M-2 (medium): "GCC (Linux): system GCC" understates what `linux-i386` needs (Stack, AD-11)

No mainstream distro ships an i386 kernel or installer today: Fedora stopped at F31, and Debian trixie dropped the i386 installer while keeping the i386 userland. In practice `linux-i386` means `gcc -m32` with multilib on an x86-64 host. Because SDL3 is built from source (AD-11), the build also needs the i386 `-dev` packages of SDL's backends: X11/Wayland, ALSA/PulseAudio/PipeWire, udev/evdev for joysticks and haptics. On Fedora those i686 packages survive only because the drop proposal was withdrawn, and leaf i686 packages are still being trimmed.

- **Sources:** [Fedora Drop i686 change](https://www.fedoraproject.org/wiki/Changes/Drop_i686_support), [Fedora leaf i686 removal](https://www.fedoraproject.org/wiki/Changes/EncourageI686LeafRemoval), [Debian libsdl3 i386 builds](https://packages-pkgmirror-csail.debian.org/trixie/libsdl3-0)
- **Fix:** name the i386 host in the stack: Debian or Ubuntu x86-64 with `gcc-multilib` and the i386 multiarch `-dev` packages, or an i386 container image. This matters most because i386 is the CAP-2/3 comparison baseline (AD-8), so it has to keep building after CAP-1.

### M-3 (medium): neither the target CMake nor x86_64 mingw-w64 is installed on the build machine (AD-11)

This machine has `cmake 3.28.3` and `i686-w64-mingw32-gcc` only; `x86_64-w64-mingw32-gcc` is missing. "CMake 4.4.x" is current upstream, but it isn't what Ubuntu 24.04 ships. CMake 4.x also removed compatibility with `cmake_minimum_required(VERSION < 3.5)` projects, which matters for any vendored dependency that has a CMake build.

- **Sources:** local `cmake --version`, `which`; [CMake 4.0 release notes (removed <3.5 compatibility)](https://cmake.org/cmake/help/latest/release/4.0.html)
- **Fix:** state how CMake 4.4 is obtained (Kitware tarball or `pip install cmake==4.4.*`, pinned) and list `gcc-mingw-w64-x86-64` among the prerequisites. Alternatively, lower the floor to the CMake version actually needed, since nothing in the spine requires a 4.4 feature, and pin only the tested version.

### L-1 (low): the versions in the memlog are already stale

The memlog records SDL3 3.4.14 and CMake 4.4.1 as the newest found. Current releases are 3.4.18 (or later) and 4.4.3, as shown in the table. This doesn't change any decision.

- **Fix:** write "pinned at adoption" with a version and checksum when the CMake work starts, as the spine already does for libcdio. Keep "3.4.x"/"4.4.x" as the series.

### L-2 (low): 16 KB pages on Apple Silicon and macOS W^X (AD-1, AD-3, AD-7)

A 4 GB `mmap(PROT_NONE)` reservation followed by committing pages with `mprotect` is the standard technique (Wasmtime, V8) and is very likely fine. However, page granularity on Apple Silicon is 16 KB, while PE sections are aligned to 4 KB. The arena can't use page protection to separate a module's `.rdata` from its `.data`, or a guest image's sections from one another. Because `fs98guest` is an interpreter, no page ever needs to be executable, so W^X/`MAP_JIT` doesn't apply. The spine should say so, so nobody adds a JIT later without seeing the constraint. Separately, a base-0 arena (the Deferred item) is impossible on macOS (`__PAGEZERO` covers the low 4 GB) and on Windows x64. The spine scopes that item to Linux, which is correct.

- **Fix:** add a 20-line arena probe (reserve 4 GB, commit and write at 0, 2 GB and 4 GB−16 KB, release) to the first `fs98arena` story and run it on all four 64-bit targets. Note "never executable; protections at host-page granularity only" under AD-1.

### L-3 (low): `long double` I/O on mingw-w64 (Floating point convention)

If any logging or difftest code prints 80-bit values, mingw-w64 needs `-D__USE_MINGW_ANSI_STDIO=1`, because UCRT treats `long double` as 64-bit.

- **Source:** [mingw-w64 bug 520](https://sourceforge.net/p/mingw-w64/bugs/520/)
- **Fix:** add the define to the `windows-x64` and `win32-retail` toolchain files.

### L-4 (low, outside this review's lens): license reach of native add-ons

libcdio and OpenFS98 are both GPLv3, so they are compatible. But CAP-8/CAP-9 native add-on modules `dlopen`ed into a GPLv3 host, which links GPLv3 libcdio, raise the usual question of whether add-ons must be GPL-compatible. SDL3 (zlib) raises no issue.

- **Fix:** have SPEC state the add-on licensing position, or add a plugin exception. Libcdio doesn't change the answer, because the host is already GPLv3.

## Decisions with no visible research trail

The memlog records "(version) ... newest found" for CMake and SDL3, so those were researched. These were asserted without a source:

- the x87 / `long double` reasoning (H-1)
- the i386 arena feasibility (C-1)
- the libcdio source and build path (H-2)
- the lifespan of the macOS Intel target (M-1)
- the i386 host toolchain (M-2)
