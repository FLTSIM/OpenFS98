# Building and running

How to get from a CD image to a runnable tree, build the modules, and test them
in the real game. Moved out of the README. Which tool does what is in
[tools.md](tools.md); how work is done is in [method.md](method.md).

## Setup

```sh
sudo apt install gcc-mingw-w64-i686-win32 p7zip-full   # 32-bit toolchain, 7z
7z x ~/Downloads/MSFlightSimulator98.iso -ocd     # the CD, for staging
./tools/extract.sh ~/Downloads/MSFlightSimulator98.iso   # orig/ reference binaries
cmake -S . -B build/win32-retail -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/win32-retail.cmake   # configure first: tools/ofs runs in it
tools/ofs sim1-reference   # once, before the first build: orig/Sim/SIM1.sim (the native step's resources and the harness runs read it)
tools/ofs stage                                   # game/ — a real install
ln -s "$PWD/game" ~/.wine/drive_c/FS98            # game/ as C:\FS98 (see below)
cmake --build build/win32-retail && tools/ofs ofs_install && tools/ofs smoke   # build, install, then boot the game
```

`cd/`, `orig/`, `game/` and `bin/` are gitignored — no game content goes in the
repository. The only derived artifact that is committed is
`docs/setup_map.tsv`, the CD-to-install file map read out of `Setup.exe`.

Module data images become C initialisers module by module (Story 1.4): the modules still to convert are the
ones in `tools/raw_dumps_allowed.txt`, and `tools/check_data_image.py <retail.dll> <ours.dll>` proves one equals retail's `.data` byte for byte.
Two layers guard those images. Every build checks our own output by hash, with no CD: after each of the twenty-five converted modules links, `tools/check_data_image.py --hash <MODULE> bin/<M>.DLL` compares our normalized `.data` image (with our relocation offsets) and our `.rdata` ranges (searched in our `.data` and `.rdata`, since MULTIP keeps its copy of retail's `.rdata` in `.data`) with the sha256s in `tools/data_images.txt`, and fails the build on a difference. The two test gauges are hashed by `tools/ofs test-gauges` (rows WARNING and SUCTION) instead of an `all` step. The 120 CD gauges (one row per `<NAME>`, with `ptrs=src/gauges/images/<NAME>/rebind.inc`) are hashed by one `--hash` run over all 120 `MODULE DLL` pairs at the end of the gauges step, which reruns when a file of `src/gauges/images/`, `src/gauges/*.c`, `*.h`, `gauges_manifest.json` or `tools/data_images.txt` changes. `tools/ofs data-images` re-proves every row against retail (needs `orig/`) and runs the script's self-test. After an intended change, edit the C, then run `tools/ofs data-images UPDATE=1`: the build warns instead of failing, and the hashes of rows that pass the retail check are rewritten. Do not leave `UPDATE=1` exported in a shell: it turns the build guard into warnings (and resets the DLL's mtime so the next build reruns the check). A row may carry `at=SYMBOL` (NUMERIC, UTIL, both `--kept`): our image is then found by that COFF symbol of our DLL (`_fs98_numeric_data`, `_fs98_util_data`) instead of by the `ImportTable` export, because each keeps a verbatim copy of retail's whole `.data` (retail's ImportTable and Linkage bytes included) in a buffer of its own, while the module's live ImportTable and Linkage are separate objects; the DLL must keep its COFF symbol table; with `at=` the image may lie in `.data` or `.rdata`. A row may carry `--mixed` instead of `--kept` (HG2D, G3D, MAIN): each retail relocated slot that is not ours must then be retail's raw value, or zero and listed in `ptrs=` (given the usual value check), or zero and listed in `cset=` (the zero slots the module sets in C: comma-separated offsets, and `A..B` for every 4th offset from A to B; each must be a slot retail relocates, not ours, not in `ptrs=`, and zero in ours; `cset=` is only valid with `--mixed`, and `--hash` hashes the parsed list). A deleted raw value or a deleted pointer entry therefore fails, and `UPDATE=1` cannot bless it. G3D's and MAIN's `ptrs=` files also list `G3D_CODE_TARGET` / `MAIN_CODE_TARGET(address)` lines, which must equal the distinct raw values of the image's kept slots with none repeated, and the self-test re-parses G3D's built-in bytecode from `src/g3d/g3d_image.inc` against retail's opcode table. MAIN's file also lists `MAIN_DATA_POINTER(at, target)` and 51 `MAIN_LINKAGE_RETAIL(at, value)` lines (retail's own Linkage words at +0xc0..+0x188, checked against retail, none counted as installed slots). A row may carry `tail=SYMBOL:VA:LEN[:SHA256]` (MAIN: `_main_text_tail:0x207f4093:0xbf6d`, retail's season table, code and `.rdata` kept as bytes for MAIN-6/MAIN-18's unbounded read): our symbol's bytes must equal retail's mapped image (`.text`, the zero gap, `.rdata`) from VA, no relocation of ours may overlap the window, and `--hash` hashes it; retail's own relocations over it are allowed, since it is retail's code kept raw. GLOBAL has `at=_Linkage` (retail exports only `Linkage`, at `.data`+0). A row may also carry `ptrs=PATH:SHA256` (ADVDRV, FACIL, FE, MULTIP, PANELS: the `.inc` list of pointers installed at attach; CONTROLS and SOUND install some in C, so they have none yet) and `switch=IDX:LEN:JT:T=A,...:SHA256` (MULTIP's `s_frame_actions`, rebuilt from retail's compressed switch). `--hash` hashes the parsed list entries (comments and whitespace don't count) and the switch window; `data-images` checks every slot and target against retail's relocated values, and that retail's relocations in the image are all ours, kept or listed. Editing a listed `.inc` reruns its module's `--hash` step (`ofs_step` globs `src/<m>/*.inc`). Owner-1 (`.rdata`-owned) MULTIP entries are rejected as unsupported, since retail has none. A step reruns when `check_data_image.py` changes (all twenty-five relink, harmlessly), but not when only the manifest changes; `data-images` re-checks everything.
`tools/check_raw_dumps.py` fails on any raw dump in git outside `tools/raw_dumps_allowed.txt`.

**Which CD.** FS98 shipped in more than one pressing, and the project is built
against one in particular. In the reference, `FLTSIM98.EXE` and every module
are dated 6 August 1997, except MAIN (11 August) and PANELS (7 August, and
the only one linked at `0x10000000`). One known-good image is a 581,996,544-byte
`MSFlightSimulator98.iso` with SHA-256
`be523a66bf368f8769f86aedcddad6f58fb0185908b6a398c456c0c5a4d07436`. Other
rips of the same CD may hash differently, so go by the dates.

A later pressing, whose ISO files are dated 10 November 1997, rebuilt
`FLTSIM98.EXE` (28 October 1997) and ten modules (17–20 October 1997, all
linked at `0x10000000`): ADVDRV, CONTROLS, FE, SOUND, WINMSG, MAIN, G3D,
HG2D, PANELS and MULTIP. Its other seventeen modules are byte-identical to
the reference's. The rebuilt ones are different programs (that pressing's
CONTROLS has 493 commands to the reference's 383, and the host's services
have moved), so the generators and harnesses do not fit them:
`tools/gen_main_bitmap.py`, for one, stops with `image base moved`. Check
after extracting:

```sh
objdump -p orig/Modules/CONTROLS.DLL | grep -E 'Time/Date|ImageBase'
# reference: Wed Aug  6 21:46:24 1997, ImageBase 20670000
```

**Resources from another pressing.** Since Story 1.4, the build scripts listed as
`resource:` in `tools/raw_dumps_allowed.txt` take resources (dialogs, strings, bitmaps,
by type and id) from whatever CD is in `orig/`, with no check of the image. The tables
our modules were decompiled with (`src/*/*_runtime_tables.inc` and `*_tables.inc`) are
committed from the reference. A pressing whose resources differ would therefore build
cleanly and could fail only at run time. Before such a pressing is supported, compare its
resources with `python3 -B tools/pressing_catalogue.py compare bin/pressings/reference.json
OTHER.json` and check every difference against those tables (Story 6.1). What is known
so far: the November pressing rebuilt MAIN, whose kneeboard bitmap `gen_main_bitmap.py`
reads, and `FLTSIM98.EXE`, whose resources `gen_host_resources.py` reads. The host build
no longer stops on a different EXE (the SHA-256 pin of the reference executable moved to `gen_host_rt_tables.py`, which `tools/ofs host-rt-tables-check` runs, and to the image headers); `tools/ofs data-images` compares the host's image with whichever `orig/FLTSIM98.EXE` is there.
The modules the other `resource:` scripts read (World, Alerts, Help, Views, Flights,
Options, Sim, MP_Dialog, SIMSUI) were not among the ten rebuilt, but no catalogue of
that pressing has been compared yet.

`cmake --build build/win32-retail` also builds all 120 gauges from generated C. `tools/ofs ofs_install` installs their
exact setup filenames and `tools/ofs ofs_revert` restores their retail versions. Use
`tools/ofs gauges`, `tools/ofs gauges-install` or `tools/ofs gauges-revert` for gauges alone.
See [modules/gauges.md](modules/gauges.md) for verification commands.

The `--image-base` of each module's step in `cmake/steps/all.cmake` is the reference's base for it.

`tools/ofs stage` exists because the CD layout is not the installed layout: setup
renames 263 files on the way in, and FS98 opens them by their installed names.
Skipping that is why the sim first dies with "Aircraft initialization failure".
It also installs the CD's regional scenery, as Setup's Custom install does; `tools/ofs
stage-scenery` adds just that to an existing `game/`, keeping its modules and
profile. See [architecture.md](architecture.md#the-cd-layout-is-not-the-installed-layout).

## Layout and naming

```
orig/       retail binaries, extracted from the CD (not committed)
game/       runnable FS98 tree for testing (not committed)
include/    the module ABI
src/        reimplemented modules, one directory each
tools/      analysis scripts
docs/       findings
bin/        build output
```

Identifiers in the code use the `fs98_` / `FS98_` prefix when they describe the
original game -- its ABI, its module ids, its data layouts -- and `openfs98`
only for this project's own identity. Same split OpenRCT2 draws between `rct2_`
and `openrct2`: most of what gets written here is a description of someone
else's 1997 build, and it should read that way.

## Testing against the real game

The game does not need installing — the CD's `fs98` tree runs as-is once
`mfc42.dll` and `MSVCRT.DLL` from the CD root sit next to `FLTSIM98.EXE`. That
tree and the regional scenery are staged in `game/` (gitignored, 288 MB) and symlinked into the wine
prefix as `C:\FS98`, so no bulk data lands in `~/.wine`. `tools/ofs ofs_install`,
`tools/ofs run` and `tools/ofs ofstest-run` default to that path (`FS98_DIR`), and the
game must run from drive C:. Two of `ofstest`'s checks are of UTIL's
drive-root quirk against a fixture at `C:\openfs98-enum`, and they fail when
the game runs from `Z:\` instead (`tools/ofs smoke` itself runs `game/` directly
and does not mind).

```sh
tools/ofs ofs_install     # install rebuilt modules, the English/ DLLs and SIMSUI included (tools/modules.txt)
tools/ofs run         # wine FLTSIM98.EXE
tools/ofs sehfix-install vd-on   # what a flyable sim with retail HG2D needs under wine -- docs/wine.md
tools/ofs ofs_revert      # restore retail modules, including the English/ DLLs
tools/ofs trace       # WINEDEBUG=+loaddll, to see which modules got loaded
```

Set `FS98_DIR` if the tree is elsewhere.

Two levels of test. `tools/ofs smoke` boots the real game; `tools/ofs difftest-run` drives
a module directly against the retail one, which is the only way to check
anything the game will not exercise on startup — SLEW's tick, for instance, runs
only in slew mode.

`tools/ofs smoke` asserts the game actually comes up: all 27 runtime modules and
the localized `English/` replacements (World, Alerts, Help, Sim, Views) load, followed by at least one
`.GAU`. The gauge check is the point — FS98 reports startup
failures in *modal dialogs*, so a game wedged behind "Aircraft initialization
failure" stays alive and looks identical to one that is flying. Gauges only load
once an aircraft has resolved, its visual model has been found and its panel
built, so a gauge load means startup went all the way through.

**Verified working**: under wine 9.0 the retail game reaches its main menu —
27/27 modules, 95 gauges — and so does the build with OpenFS98's `ACONTAIN`, `ADVDRV`, `ANALYSIS`, `ATC`, `BOOSTER`,
`CHAIN`, `DEMO`, `DYNAMIC`, `FACIL`, `FE`, `GLOBAL`, `LANDME`, `NUMERIC`,
`REALITY`, `SLEW`, `SOUND`, `UTIL`, `WEATHERM`, `WINDOW` and `WINMSG` modules substituted, with
identical counts.

The harnesses' own details are in [method.md](method.md) and the per-module
documents; `tools/ofs main-dialogs`, `tools/ofs flights-play`, `tools/hg2d_frames.sh` and
the like are listed in [tools.md](tools.md), with their baselines.

## Building

CMake, one toolchain file per target (only `win32-retail`, mingw i686, so far). Everything is written into
`bin/` of the source tree (the generators and windres write there); the build directory holds only CMake's
state.

```sh
cmake -S . -B build/win32-retail -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/win32-retail.cmake
cmake --build build/win32-retail                     # the 27 modules, 8 English DLLs, SIMSUI, gauges, MDLs, SIM1, OPENFS98.EXE
cmake --build build/win32-retail --target difftests  # the 120 difftest and driver executables
tools/ofs ofs_install                                # install into FS98_DIR (`install` is reserved by CMake)
tools/ofs ofs_revert                                 # restore the retail files
```

Every other target (the old Makefile's and `tools/*.mk`'s, same names; `clean` and `help` (Help.DLL's, from `tools/help.mk`) are `ofs_clean` and
`ofs_help`) is in `cmake/targets.cmake`. `tools/ofs TARGET VAR=value ...` runs one with make-style parameters,
which reach its commands through the environment with the old defaults; it is
`VAR=value cmake --build "$OFS_BUILD" --target TARGET`, and `OFS_BUILD` defaults to `build/win32-retail`:

```sh
tools/ofs maindiff-run SUITE=nav N=20000
tools/ofs gauges-install FS98_DIR=$HOME/.wine/drive_c/FS98
```

A target runs its own helper builds every time (the old `make -B` recipe), not only when out of date.
`tools/modules.sh` answers what `tools/modules.sh modules-of`, `installed-path-of M=X` and the rest did, from
`tools/modules.txt`, the module list `cmake/install.cmake` also reads.

One compile flag set, `OFS_MODULE_FLAGS` (`-fwrapv -fno-strict-aliasing -fno-delete-null-pointer-checks -mfpmath=387
-ffp-contract=off -fexcess-precision=standard`: the semantics retail's MSVC code relies on), is spelled only in
`cmake/toolchains/win32-retail.cmake`. CMake publishes it as the `OpenFS98::module` interface target and substitutes
it for `@MODULE_FLAGS@` in every build command; Python scripts take it from `tools/module_flags.py`, which parses the
toolchain file. A new gcc invocation takes it the same way and never spells those flags itself; touching the
toolchain file rebuilds every step. Optimisation, `-std`, freestanding and link flags stay in each rule, and the
four codegen overrides are listed with their reasons in the toolchain file.

`cmake/steps/*.cmake` hold the build steps, one per output, in the old Makefile's order; the per-module
`--image-base` there is the reference's base for each module. `python3 tools/cmake_compare.py <tree-a> <tree-b>`
compares two builds' `bin/` (the PE timestamp/checksum and the tree's own path are the only allowed
differences). The Makefile and `tools/*.mk` were retired on 2026-11-18 (Story 1.2); `cmake/targets.cmake` was
converted from their `make -n -B` recipes and checked against them (409 targets, defaults and parameters set; the checker went with the Makefile), and every
Baselines line of [tools.md](tools.md) was run through it (`tools/cmake_verify.py`).
