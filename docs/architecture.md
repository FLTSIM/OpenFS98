# FS98 architecture, as reverse engineered from the retail CD

*Reference notes for OpenFS98. This file describes Microsoft's 1997 build, not
this project.*

Everything here comes from the retail `1997-08-07` build (`FLTSIM98.EXE`
timestamped 1997-08-07, `MAIN.DLL` 1997-08-12), extracted straight off the
install CD by `tools/extract.sh`. No symbols or debug information of any kind
ship with the game; see [Symbol situation](#symbol-situation).

## How much code there actually is

Summing `.text` across every code module on the disc — the exe, the 27 module
DLLs, the localised UI DLLs and `SIMSUI.DLL`:

**1.44 MiB total.**

That is the whole budget. The rest of the ~600 MB disc is scenery, textures,
sound and bitmaps. Two of the largest DLLs are not code at all: `DIALOG.DLL`
is 2.25 MB of resources behind 406 bytes of `.text`, and the `.GAU` gauges run
600–1000 bytes of code each behind up to 500 KB of bitmaps.

The code is dominated by one module. `HG2D.DLL` alone is 390 KB — 26% of the
project — and everything else is small:

| Module | `.text` | share |
| --- | --- | --- |
| `HG2D.DLL` | 389,734 | 26% |
| `MAIN.DLL` | 119,648 | 8% |
| `English/World.DLL` | 115,275 | 8% |
| `G3D.DLL` | 92,518 | 6% |
| `English/Options.DLL` | 84,381 | 6% |
| `FLTSIM98.EXE` | 82,016 | 5% |
| `SIMSUI.DLL` | 79,850 | 5% |
| everything else (21 modules) | ~544,000 | 36% |

Built with Visual C++ 5.0 (PE linker version 5.00) for every module. No Rich
header — expected for that toolchain. No PGO, no LTCG: this is a plain
`/O2`-era release build, which is what makes a matching decompilation
realistic rather than theoretical.

## The module system — the reason this project is tractable

FS98 is not a monolith with a plugin API bolted on. It is a set of 27 numbered
modules on top of a host, with **no static link-time coupling between them at
all**.

`FLTSIM98.EXE` imports only Windows libraries:

```
KERNEL32 USER32 GDI32 ADVAPI32 SHELL32 WINMM MSVFW32 IMM32 DDRAW MSACM32
MSVCRT MFC42 COMCTL32
```

It finds the simulator itself at runtime: it enumerates `*.DLL` in `Modules\`,
`LoadLibrary`s each one, and links them together by number. The config key
`AutoInitModules` and the string `*.DLL` are both in the exe.

Every module — and every `.GAU` gauge — exports exactly two symbols, as data:

```
Linkage      this module's identity and service table
ImportTable  the modules it needs, by numeric id
```

### ABI

```c
struct fs98_import {           /* array, terminated by an all-zero entry; 4-packed */
    uint32_t  id;              /* module id wanted */
    fs98_addr link;            /* zero on disc; written by the host at init: a struct fs98_linkage * */
};

struct fs98_linkage {
    uint32_t  id;              /* this module's id */
    fs98_code init;            /* slot 0 — inferred from position; void (*)(void) */
    fs98_code deinit;          /* slot 1 — inferred from position */
    fs98_code service[];       /* slot 2.. — module specific, called by index */
};
```

The shared headers describe FS98 memory with 32-bit contract types (AD-14), so one layout holds on every build.
`include/fs98/abi.h` defines `fs98_addr` (a data address, an arena offset), `fs98_code` (a code handle),
`FS98_WCHAR` and the only `FS98_STDCALL` (stdcall on `win32-retail`, the platform default elsewhere). `include/fs98/retail_abi.h` defines `FS98_RETAIL_ABI` once (an interim proxy for `win32-retail`: `_WIN32` and `__i386__`); `abi.h`, `keep.h`'s register-keeping wrappers and the `.S` entry stubs key on it (AD-4). `linkage.h`,
`host.h` and `global.h` use no C pointer, `long`, `size_t`, `wchar_t`, bitfield or `long double`; every host service is
a function type (`fs98_host_<name>_fn`) over `fs98_addr` and `fs98_code`, and each shared struct is 4-packed with
`_Static_assert`s on its size, alignment and offsets. `tools/check_abi_headers.py` checks the types and compiles the
headers with mingw i686, `gcc -m32` and `gcc`. On `win32-retail` only, `host.h` also keeps a transitional bridge: a
`fs98_host_<name>_call` type with the native pointer signature for each service, which the `FS98_HOST_*` macros use so
existing callers compile unchanged, plus a pointer view of `fs98_import.link` (`FS98_LINK`, an anonymous union member) and `FS98_RETAIL_PTR` for list `data` fields, which keep the code gcc generates for those fields byte-identical. A story of its own removes the bridge (`docs/implementation/deferred-work.md`).

`include/fs98/arena.h` is the one home of the address macros (AD-15): the `fs98_v8`..`fs98_vd` access types, `fs98_ptr`, `fs98_handle`, `fs98_call` (retail only), `M8`..`MD`, `R8`..`RDB`, and `D`, `RDATA` and `DA` for a module that defines `FS98_DATA_IMAGE`, `FS98_RDATA_IMAGE` or `FS98_DATA_VA`. The base is the constant 0 on i386 and `fs98_arena_base` elsewhere (Epic 3). A module includes it directly, after its system and ABI includes, and deletes its private copies; no shared header includes it yet. `tools/check_arena.py` runs on every `cmake --build` (the always-run `ofs_arena_lint` target, a dependency of every build step) and fails on a `(uint32_t)(uintptr_t)` cast or a `#define` of those names in `src/` outside `src/platform/arena/`, beyond the per-file counts in `tools/arena_lint_allowed.txt`, which may only shrink. REALITY, ANALYSIS, CONTROLS, PANELS and the gauges (`gauge.h`, `suction.c`, `warning.c`) are converted (PANELS keeps `PN_RD` and `PN_RF` for its `.rdata` readers); the other modules follow in slices, and the last deletes the allowlist.

The host reads each module's id out of `Linkage`, then walks every
`ImportTable` and patches in a pointer to the corresponding module's
`Linkage`. All cross-module traffic goes through those patched pointers.

Two things fall out of this, and they are what make the whole project
realistic:

1. **A reimplemented module is a drop-in.** Build a DLL that exports the same
   two symbols with the same id and service-slot layout, drop it in `Modules\`,
   and the retail exe loads and uses it. No binary patching, no IAT hooking, no
   trampolines, no injected loader — the thing OpenRCT2 had to build from
   scratch is already in the shipping product.
2. **Module granularity is the natural unit of work**, and the smallest
   modules are genuinely small (`CHAIN` is 966 bytes of code, `SLEW` is 1270).

### Compile flags

Every unit, harnesses and break builds included, compiles with the same six semantic flags,
`OFS_MODULE_FLAGS` in `cmake/toolchains/win32-retail.cmake` (signed overflow wraps, no strict aliasing, null
checks kept, x87 arithmetic with standard excess precision and no contraction). See [building.md](building.md).

### Worked example: SLEW.DLL

`SLEW.DLL` is 1270 bytes of `.text`, id 9, imports `{1, 2, 20}` =
host, `GLOBAL`, `UTIL`. Its `Linkage` is `{9, ret, ret, 0, 0, 0, 0x208c1020, …}`
— the first two slots are a bare `ret`, and the real work is in the slot at
`+0x18`. That function:

```asm
push ebp / mov ebp,esp / add esp,-0x20   ; frame
push ebx/esi/edi                         ; callee-saved
push eax/ebx/ecx/edx/esi/edi             ; then saves *everything* as well
mov  edi, ds:0x208c3034                  ; = ImportTable[1].link, i.e. GLOBAL
cmp  byte [edi+0x365], 0                 ; shared state flag
je   ...                                 ; early-out if set
```

Three things to read off that:

- Service callbacks save every register, not just the callee-saved set. That is
  a hard ABI constraint on reimplemented modules.
- `GLOBAL`'s `Linkage` pointer is used as the base of a **shared state block**,
  addressed by byte offset, not called through. `[global+0x365]` is a flag.
- Which means recovering `GLOBAL`'s field layout is the central shared task of
  the whole project, and every module worked through contributes offsets to it.

### Module id and dependency map

Reproduce with `tools/ofs graph`. Id 1 is the host; id 19 is imported by `PANELS`
but is not a file in `Modules\` — most likely the gauge host, unconfirmed.
`MPCHAT`'s id really is `0xffffffff`: nothing imports it, the host registers
its one dialog from Linkage `+0x14`, and MULTIP reaches it on CHAIN list 0x1d
(see [modules/mpchat.md](modules/mpchat.md)). Ids 5, 8, 11, 12, 25, 28, 30, 31, 33, 36
and 38 are unused by the retail set — probably modules that existed in FS95 or
during development.

| id | module | `.text` | imports |
| --- | --- | --- | --- |
| 2 | `GLOBAL` | 54 | — |
| 3 | `MAIN` | 119,648 | almost everything |
| 4 | `CHAIN` | 966 | host, `GLOBAL` |
| 6 | `DYNAMIC` | 12,300 | host, `GLOBAL`, `CHAIN`, `FE`, `G3D`, `UTIL`, `MULTIP` |
| 7 | `DEMO` | 16,832 | host, `GLOBAL`, `CHAIN`, `CONTROLS`, `MAIN`, `NUMERIC`, `SOUND`, `UTIL`, `WINMSG` |
| 9 | `SLEW` | 1,270 | host, `GLOBAL`, `UTIL` |
| 10 | `WINDOW` | 24,992 | +`ACONTAIN`, `HG2D`, `G3D`, `MAIN`, `NUMERIC`, `PANELS`, `REALITY` |
| 13 | `HG2D` | 389,734 | host, `GLOBAL`, `CHAIN`, `FE`, `UTIL`, `G3D` |
| 14 | `G3D` | 92,518 | +`HG2D`, `FE`, `WINDOW`, `WEATHERM`, `ATC`, `NUMERIC` |
| 15 | `PANELS` | 50,038 | +`ACONTAIN`, `HG2D`, id 19, `MAIN`, `SOUND`, `WINDOW` |
| 16 | `CONTROLS` | 48,406 | twelve modules |
| 17 | `NUMERIC` | 14,684 | host, `GLOBAL`, `UTIL` |
| 18 | `FE` | 49,016 | +`BOOSTER`, `FACIL`, `G3D`, `NUMERIC` |
| 20 | `UTIL` | 11,062 | host, `GLOBAL`, `NUMERIC` |
| 21 | `ADVDRV` | 29,330 | +`CHAIN`, `DEMO`, `MAIN`, `SOUND`, `WINMSG` |
| 22 | `WINMSG` | 4,416 | host, `GLOBAL`, `CHAIN`, `WINDOW`, `MAIN` |
| 23 | `ACONTAIN` | 5,504 | host, `GLOBAL`, `CHAIN`, `G3D`, `PANELS`, `UTIL` |
| 24 | `ATC` | 3,840 | +`NUMERIC`, `UTIL`, `WEATHERM`, `WINMSG` |
| 26 | `LANDME` | 4,094 | +`MAIN`, `FE`, `UTIL`, `WINMSG` |
| 27 | `WEATHERM` | 8,774 | host, `GLOBAL`, `CHAIN`, `G3D`, `MAIN`, `UTIL` |
| 29 | `ANALYSIS` | 8,086 | +`HG2D`, `G3D`, `NUMERIC`, `UTIL` |
| 32 | `REALITY` | 4,822 | ten modules |
| 34 | `BOOSTER` | 8,024 | host, `GLOBAL`, `CHAIN`, `NUMERIC`, `UTIL`, `FE` |
| 35 | `MULTIP` | 43,318 | +`ACONTAIN`, `HG2D`, `G3D`, `MAIN`, `NUMERIC`, `WEATHERM` |
| 37 | `FACIL` | 23,045 | host, `GLOBAL`, `CHAIN`, `FE`, `MAIN`, `NUMERIC`, `UTIL` |
| 39 | `SOUND` | 15,174 | host, `GLOBAL`, `ACONTAIN`, `CHAIN`, `UTIL` |

The utility layer — `GLOBAL`, `CHAIN`, `NUMERIC`, `UTIL` — is imported by
almost everything and imports almost nothing. That is the bottom of the
dependency order and the place to start.

## Symbol situation

No debug information ships with the game. Searched every code file for `.pdb`
references, `RSDS`/`NB10` debug directory entries, source paths and assert
strings: **nothing**. These are stripped release builds with no `/DEBUG`.

What exists instead:

- **`SIMSUI.DLL` exports 540 mangled C++ symbols.** MSVC name mangling encodes
  the class name, method name and complete parameter types, so this is a real
  symbol table for the UI layer: `CSimsDialog::CSimsDialog(int, CWnd *)`,
  `CDIBSection`, `CSimsCheckListBox`, `CSimsFont`, and so on. MFC-based. Run
  `strings orig/SIMSUI.DLL | grep '^??'` and demangle. This is the one genuine
  symbol windfall on the disc.
- **Config key names** are plaintext in the modules — `MAIN.DLL` carries
  `ADVANCED_WEATHER`, `CLOUD_THICKNESS`, `IMAGE_QUALITY`, `GROUND_SHADOWS`,
  `CACHE_WAV_FILES`, `AIRCRAFT_SHADOWS` and dozens more, naming settings
  fields directly. `FLTSIM98.EXE` has `AutoInitModules`,
  `CurrentJoystickSettings`, `AllowIdiotProofing`.
- **Simulation variable tokens**: `MAIN.DLL` has `INDASPD`, `GUGSPD`,
  `AIRtoCONTAINER`, `FS6BOUNDS` — note "FS6", the internal version number
  (FS95 was 6.0, FS98 is 6.1; `fltsim95.exe` ships alongside as a stub).
- **`G3D.DLL` is the BGL interpreter**, and it says so: the error string
  `"BGL VAR_SEG op misuse"` names a BGL opcode. BGL is the scenery bytecode
  language, and its opcode set is documented in the FS98 SDK and by twenty
  years of community scenery tooling. A large chunk of the second-biggest
  module is therefore a dispatch loop over an *already-documented* instruction
  set.

## Open questions

Settled since this was first written:

- `Linkage` slot 0 and slot 1 **are** init and deinit. They are a no-op `ret`
  in most modules, which is why the names started as positional guesses, but
  UTIL's are not: its `+0x04` seeds both of its generators from the host clock
  and its `+0x08` is the bare `ret`. See [modules/util.md](modules/util.md).
- `GLOBAL`'s state block is no longer unnamed: 24 offsets are identified in
  `include/fs98/global.h`, all of them recovered from SLEW. Each module
  reversed names a few more, which is the intended way to fill it in.

Still open:

- What is module id 19, imported by `PANELS` but absent from `Modules\`?
  Probably the gauge host, unconfirmed.
- How does the host hand a module its `ImportTable` fixups — by export lookup
  per module, or a single pass? The loader is in `FLTSIM98.EXE` and is now
  reachable, since its service table has been mapped.
- The remaining 4752 bytes of `GLOBAL`'s block.

## The CD's `fs98` tree is not the whole world

`fs98/` holds the default world only: the terrain base, Chicago, the metro areas
and navaids, and no airport but the 16 heliports of `heloafd.bgl`. The other 3,775
airports and all regional scenery are in `Boosters/`, 18 regions that Setup offers
either on the CD through a cache or on the hard drive, adding an area or a cache to
`Scenery\world.vis` as it goes. The CD's own `world.vis` lists neither, so a tree
without this step lists 16 places to go and is water away from Chicago.

`tools/stage_scenery.py` makes the hard drive choice for all 18: Setup's table
gives the 3,437 file destinations, and the world file's areas and their layers come
from Setup's code (see the script). Flying over the result reads no CD and no cache,
so BOOSTER stays idle in-game. Not installed, because the sim does not use them for
flying: `Media/` (help videos and pictures), `sampler/`, `directx/`, `Indeo4/`, and
`Boosters/Facility` and `ScnSetup/`, which no module reads and Setup's install
table does not copy.

## The CD layout is not the installed layout

Setup.exe renames 263 files and three directories as it copies, and FS98 opens
them by their *installed* names. Nothing in the game reads the CD names. The
table is embedded in `Setup.exe` as plain ASCII — each source path beginning
`fs98\` immediately followed by its destination — and
`tools/extract_setup_map.py` pulls all 2018 entries out into
`docs/setup_map.tsv`. That beats running the installer: no GUI, reproducible,
and the result is diffable.

Three naming rules matter to anyone poking at the data files:

- **Flight models.** `aircraft.cfg`'s `sim=X` is turned straight into a
  filename: `AIRCRAFT\<dir>\X.AIR`. The CD ships `cessna.air`; the install has
  `CESSNA182S.AIR`.
- **Visual models.** `model.cfg`'s `normal=Y` / `crash=Z` become `Y.MDL` /
  `Z.MDL`. The CD ships `CESSNA_N.MDL`; the install has `CESSNA182S_N.MDL`.
- **Gauges.** `panel.cfg`'s `gauge00=Cessna_182.Suction` wants
  `GAUGES\CESSNA_182.SUCTION.GAU`. The CD ships `CESSSUC.GAU`. Every gauge on
  the disc is renamed this way — the installed name is
  `<panel family>.<gauge function>.GAU`, which is far more informative than the
  8.3 name and worth using when reading `PANELS.DLL`.

**Two of the sim modules are renamed as well**: `FACIL.DLL` becomes
`FACILITIES.DLL` and `MULTIP.DLL` becomes `MULTIPLAYER.DLL`. A rebuilt module
copied in under its CD name would sit there unloaded, so `tools/ofs ofs_install` maps
through the installed names in `tools/modules.txt`.

Symptoms, if the renames are skipped: the sim starts, then stops at
"Aircraft initialization failure <title>"; fix the `.AIR` and it gets one step
further to "Visual model could not be displayed"; and so on through panels and
gauges. All of them are *modal dialogs*, which is why a smoke test cannot treat
"process still alive" as success — see `tools/smoketest.sh`.

## Integer divides

Retail relies on the host's divide filter (`0x4119a0`) to saturate a faulting `div` or `idiv`. Every retail divide that can fault is to call a helper from `include/fs98/divide.h` (`fs98_div32`, `fs98_idiv32`, `fs98_div16`, `fs98_idiv16`, `fs98_div8`, `fs98_idiv8`; ARCHITECTURE-SPINE AD-18), which returns what the filter writes. The helpers exist; the modules still use their own divides and move onto them in Stories 1.10b and 1.10c. `tools/ofs dividediff-run` checks them against retail's filter and ours on real faulting instructions. The filter's decode-time quirks (DIVIDE-BYTE-REGISTER, DIVIDE-DISP8-UNSIGNED, DIVIDE-SIB-REGISTER-STEP) are not modelled by the helpers.

## A wine quirk that is not ours

FS98's opening menu flickers under wine: the "Fly Now!" default button redraws
its focus highlight on and off a few times a second, and clicks land
unreliably. It is tempting to blame a rebuilt module for this. It is not one —
sampling the game window with the retail Microsoft modules installed shows the
same oscillation, in the same 113x28 region at +504+389 of the 640x480 window,
with the same 1510-pixel delta between the two states.

The decisive evidence is simpler than any of that, though. Three consecutive
launches -- retail, then the rebuilt modules, then the rebuilt modules again
with nothing changed in between -- flickered, did not flicker, and did not
flicker. Identical binaries on the second and third runs, different behaviour.
Whatever drives it is not in the module set; most likely it is whether the
window has focus when it maps, which varies with what else is on screen.

Worth recording because the first attempt to measure it was wrong in an
instructive way: sampling a fixed *screen* rectangle made window position
between launches look like a difference in content, which pointed the finger at
the rebuilt modules. Sampling the *window* instead showed retail behaving
identically. Capture the window, never the screen -- and prefer a repeated
launch over a clever measurement.
