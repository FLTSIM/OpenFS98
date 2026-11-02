# Open differences

Known places where OpenFS98 differs from retail or has not been shown to match it, so
they are not lost. Delete an entry when it is fixed, and record the fix in the module's
document. Results here are from 2026-11-01.

## What currently matches

All retail against all ours (`HG2D_FRAMES_ALL=1 tools/hg2d_frames.sh`), on `PARIS`,
`NEWYORK`, `STGEORGE`, `CVINSON`, `HONGKONG`, `KAUAI`, `FS98` and `fs6N`:

- software renderer, 900 passes: every frame checksum identical;
- Direct3D, 500 passes: scene and draw counts equal, four screenshots each 0 pixels
  apart, panel logs identical.

Nobody is flying in these runs, so everything below the surface of that is untested
until the first item under [Not yet tested](#not-yet-tested) is done.

## Differences only with retail and ours mixed

Neither shows in the all-ours game on the flights above. They matter when a single
module is swapped into a retail game to bisect a problem, and each could be hiding a
real difference. Each of our modules alone against all-retail, software, 60 passes, on
`PARIS` and `KAUAI`: all match except these.

### UTIL alone differs from the first frame

On `PARIS`, frame 1 (line 2 of the log); on `KAUAI`, line 3. GLOBAL differs at pass 0
(`+0x2ea`, `+0x5d2`, `+0xc92`, `+0xc96`, `+0xce8`, `+0xcf4` among others). On Direct3D,
before the `+0x60` and `+0x6c` fixes, it drew 3,994 times where retail drew 8,521, in the same 191
scenes.

**Not explained.** The likely cause: retail UTIL preserves registers across most
services and ours, plain C, does not, while retail's assembly modules keep values in
those registers across the call. `utildiff` compares results, not registers, so it
cannot see this. Only `+0x60`, `+0x6c` and `+0x70` have been fixed. Registers each
retail service pushes in its opening instructions, which is a first cut and not a proof
that it restores them:

| registers | slots |
| --- | --- |
| eax, ecx, edx | `+0x24`, `+0x2c`, `+0x30`, `+0x44`, `+0x48`, `+0x4c`, `+0x64`, `+0x68`, `+0x88`, `+0x94` |
| eax, edx | `+0x34`, `+0x40` |
| ecx, edx | `+0x38`, `+0x3c` |
| edx | `+0x18`, `+0x1c`, `+0x20` |
| eax | `+0x28` |

To continue: give these slots the same entry thunks as `+0x60`, extend `utildiff`'s
`keeps_registers` checks to them, and rerun UTIL alone. Our other modules might read a
register the same way in a path these flights do not reach, so this is not only about
mixed games.

### MAIN with retail ATC faults

MAIN alone on `KAUAI`: an access violation in retail ATC at `0x20641041`, pass 12,
which ends the frames. Retail ATC `+0x20` reads GLOBAL through edi on entry, and
retail MAIN's COM tuning (`.text+0x18a17`) has GLOBAL there; `radio_tune_com` in
`src/main/main_radios.inc` is C and does not. Our ATC does not depend on it. The
`radios` suite's stand-in for ATC does not read edi, so the harness cannot see it.

To fix: load edi on that call, and have the stand-in read it.

## Not yet tested

- **Flying.** No frame comparison has any input: the aircraft flies itself from each
  flight's start. Nothing has been flown by hand on the all-ours build. FRAMELOCK can
  post scripted messages for this (`FRAMELOCK_SCRIPT`).
- **Coverage of the frame comparison.** Eight of the 74 flights, a few hundred frames
  each. `KAUAI`, `FS98` and `fs6N` show few distinct frames on the software renderer.
  No views changed, no night or weather set by hand, no menus.
- **Direct3D's limits.** wined3d on llvmpipe, not a 1997 card, and four screenshots a
  flight between the panel logs. Hardware-specific paths in HG2D (real capability bits,
  texture memory running out) have run only in `hg2ddiff`.
- **Non-x86.** HG2D's floating precision and status; see
  [modules/hg2d.md](modules/hg2d.md).

## Unsettled questions

- **Does our HG2D still need `SEHFIX.DLL`?** Ours computes the divides that froze retail
  without trapping, but retail did not freeze in the nested X server without
  `SEHFIX.DLL` either, so there was no control. It stays installed. Both took 4 swallowed
  faults in 105 s. See [wine.md](wine.md).
- **Two deliberate HG2D breaks went unnoticed.** The depth-shade clamp (`value >= 14` in
  `hg2d_draw.inc`) and the haze tick's rate (`301u *` in `hg2d_init.inc`), over 300 passes
  of `STGEORGE`. Whether that flight reaches them is not established. Stronger breaks were
  caught at the first frame on `PARIS`.

## A class of bug to look for

Three bugs fixed on 2026-11-01 share a shape: a retail routine's register behaviour is
part of its contract, one side was reimplemented in C, and the harness's stub of the
other side hid it, so each module passed 20,000 scenarios alone.

| caller | callee | contract | the stub |
| --- | --- | --- | --- |
| DYNAMIC | FE `+0x40` | reads to the caller's edi, returns registers shifted | took the buffer argument |
| MAIN | UTIL `+0x60`, `+0x6c` | keeps ecx across the call | stamped a constant over ecx |
| FE `+0x1c` | (GLOBAL `+0xcd2`) | a different shape: `fediff` reached only one of two values | |

Worth auditing: the other `CLEAN` thunks in `tools/difftest/maindiff.c`, which fix ecx and
edx to constants; any harness stub of UTIL or FE; and `fe.md`'s other register-shifting
entries (`+0x18`). A stub that does something the real callee does not is the thing to
look for.
