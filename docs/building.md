# Building and running

How to get from a CD image to a runnable tree, build the modules, and test them
in the real game. Moved out of the README. Which tool does what is in
[tools.md](tools.md); how work is done is in [method.md](method.md).

## Setup

```sh
sudo apt install gcc-mingw-w64-i686-win32 p7zip-full   # 32-bit toolchain, 7z
7z x ~/Downloads/MSFlightSimulator98.iso -ocd     # the CD, for staging
./tools/extract.sh ~/Downloads/MSFlightSimulator98.iso   # orig/ reference binaries
make stage                                        # game/ — a real install
ln -s "$PWD/game" ~/.wine/drive_c/FS98            # game/ as C:\FS98 (see below)
make && make install && make smoke                # build, install, then boot the game
```

`cd/`, `orig/`, `game/` and `bin/` are gitignored — no game content goes in the
repository. The only derived artifact that is committed is
`docs/setup_map.tsv`, the CD-to-install file map read out of `Setup.exe`.

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
`tools/gen_controls_data.py`, for one, stops with `image base moved`. Check
after extracting:

```sh
objdump -p orig/Modules/CONTROLS.DLL | grep -E 'Time/Date|ImageBase'
# reference: Wed Aug  6 21:46:24 1997, ImageBase 20670000
```

`make` also builds all 120 gauges from generated C. `make install` installs their
exact setup filenames and `make revert` restores their retail versions. Use
`make gauges`, `make gauges-install` or `make gauges-revert` for gauges alone.
See [modules/gauges.md](modules/gauges.md) for verification commands.

The Makefile's `BASE_*` give the reference's base for each module OpenFS98
builds.

`make stage` exists because the CD layout is not the installed layout: setup
renames 263 files on the way in, and FS98 opens them by their installed names.
Skipping that is why the sim first dies with "Aircraft initialization failure".
It also installs the CD's regional scenery, as Setup's Custom install does; `make
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
prefix as `C:\FS98`, so no bulk data lands in `~/.wine`. `make install`,
`make run` and `make ofstest-run` default to that path (`FS98_DIR`), and the
game must run from drive C:. Two of `ofstest`'s checks are of UTIL's
drive-root quirk against a fixture at `C:\openfs98-enum`, and they fail when
the game runs from `Z:\` instead (`make smoke` itself runs `game/` directly
and does not mind).

```sh
make install     # install rebuilt modules, including World, Alerts, Help, Sim and Views under English/
make run         # wine FLTSIM98.EXE
make sehfix-install vd-on   # what a flyable sim with retail HG2D needs under wine -- docs/wine.md
make revert      # restore retail modules, including the English/ DLLs
make trace       # WINEDEBUG=+loaddll, to see which modules got loaded
```

Set `FS98_DIR` if the tree is elsewhere.

Two levels of test. `make smoke` boots the real game; `make difftest-run` drives
a module directly against the retail one, which is the only way to check
anything the game will not exercise on startup — SLEW's tick, for instance, runs
only in slew mode.

`make smoke` asserts the game actually comes up: all 27 runtime modules and
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
documents; `make main-dialogs`, `make flights-play`, `tools/hg2d_frames.sh` and
the like are listed in [tools.md](tools.md) and in [CLAUDE.md](../CLAUDE.md).
