# Open differences

Known places where OpenFS98 differs from retail or has not been shown to match it, so
they are not lost. Delete an entry when it is fixed, and record the fix in the module's
document. Results here are from 2026-11-01.

## What currently matches

All retail against all ours (`tools/frames_many.sh`, one `tools/hg2d_frames.sh` a flight).
Retail runs with `SEHFIX.DLL`, because without it retail is the side that freezes
([wine.md](wine.md)), and ours without.

- **Every flight, nobody flying:** all 74, 900 passes, every frame checksum identical;
  rerun 2026-11-02 with every current build and all five English DLLs swapped.
- **Flown by script, software renderer:** seven scenarios from `tools/fly_scripts.py`
  (`controls`, `systems`, `views`, `avionics`, `slew`, `toggles`, `keys`) on `PARIS`,
  `STGEORGE`, `KAUAI`, `NEWYORK`, `LEARLON`, `HONGKONG`, `BELLWTC` and `CVINSON`, 900 passes:
  all identical. `keys` goes through real key messages, the others post commands.
- **Flown by script, Direct3D:** `controls`, `views` and `systems` on `PARIS` and `LEARLON`,
  900 passes: scene and draw counts equal, eight screenshots each 0 pixels apart. Also the
  earlier eight flights with nobody flying, 500 passes.
- **By hand, on a real display, briefly:** no freeze without `SEHFIX.DLL`, and resizing the
  window works.

**Crashes need care.** A crash pauses the sim until its sound ends, in real time, so with
sound neither retail nor ours repeats: the pass it resumes on varies, and two runs can agree
by chance. `ANTILLES` and `STKITTS` do this with nobody flying. Without DirectSound retail
and ours are byte-identical, six runs out of six on `ANTILLES` and on `STGEORGE` flown with
`avionics`. So `frames_many.sh` retries any failure without sound and says so. A real
difference still differs. The cost is that those runs do not compare the crash's timing with
sound, and no comparison hears sound at all.

**Do the scripts reach anything?** `tools/fly_coverage.sh` removes each step in turn. On
`LEARLON`, 98 of 113 steps change the frames. These 15 do not, so frames say nothing about
them there: `SMOKE_TOGGLE` (twice), `ANTI_ICE_TOGGLE`, `ZOOM_1X`, `PAN_LEFT`, `PAN_UP`,
`PAN_RESET`, `BAROMETRIC`, `AP_NAV1_HOLD`, `AUTO_THROTTLE_ARM`, `SIM_RATE`, `SOUND_TOGGLE`
(twice), `LANDME_TOGGLE` (twice). On `PARIS`, gear and spoilers do nothing either.

**Do they catch bugs?** Deliberate breaks in CONTROLS, each caught: throttle cut to the wrong
value, `VIEW_RIGHT` looking the wrong way, gear toggle removed, heading hold removed, slew
step doubled. Two were not: see [Unsettled questions](#unsettled-questions).

## Our modules alone in a retail game

Each of our modules alone, everything else retail (`HG2D_FRAMES_ONLY`), matches all-retail frame
for frame on `PARIS` flown with `controls`, `LEARLON` with `avionics`, `NEWYORK` with `views`
and `KAUAI` with nobody flying, 300 passes without sound. This is how a problem is bisected to
one module, and it is where one module's expectations of another's registers show.

`tools/reg_contracts.sh` runs that and compares, slot by slot, what every Linkage function
received and left in all seven registers (FRAMELOCK wraps them; `tools/reg_contracts.py` says
how to read it). With retail callers, a slot that retail leaves a register alone in and ours
does not is a broken contract. On 2026-11-01 it found 18, now fixed with entries from
`include/fs98/keep.h`:

| module | slots | what retail keeps | seen in the game |
| --- | --- | --- | --- |
| NUMERIC | `+0x18`, `+0x1c`, `+0x20` | ecx | retail ATC printed the ATIS ceiling as `Scattered -` |
| ATC | `+0x20`, `+0x24` | eax, ecx, edx | |
| WEATHERM | `+0x24` | edx | |
| DYNAMIC | `+0x18` | edx | |
| MAIN | `+0x94`; `+0xc0` | edx; ecx, edx | |
| HG2D | `+0x70`, `+0x88`, `+0xac`, `+0xb4`; `+0x7c`; `+0x24` | edx; ecx, edx; ecx holds the third argument | |

and one the other way: retail ATC `+0x20` reads GLOBAL through the caller's edi, which our MAIN's
COM tuning now loads (`main_call_edi1`); without it MAIN alone faulted in retail ATC on `KAUAI`.
The `radios` suite's stand-in now logs edi, and catches the old call.

**What it does not settle:**
- **Slots not called** on those four flights are not checked, and a register is only seen to be
  kept on the calls that happened.
- **77 slots leave different values** in a register retail changes too (`LEAVES`): results that
  are pointers into each module's own data, and compiled-C leftovers. None is known to be read,
  and no frame differs, but nothing proves no caller reads one.
- **The carry flag** comes back differently from 18 slots. Retail's import calls go through a
  stack-alignment wrapper whose `add` clobbers it, so it is probably never read across modules.
- **DYNAMIC from `+0x20` on** is its interpreter's dispatch table, not services, and is left out.

## Not yet tested

- **Flying for long.** The scripts run 900 passes, about 30 seconds of sim time, and fly no
  route or landing. By hand it has been flown only briefly. Long flights, scenery loading far
  from the start and a real approach are untested.
- **The mouse and menus.** No script clicks anything or opens a dialog. Dialogs need a way to
  close them in the script, since a modal one hangs a run until its timeout.
- **Night, weather and time set by hand.** Only as the flights set them.
- **The English dialogs in play.** `make main-dialogs` reaches Help's About, Credits and
  Lessons, View Options, Sim's Aircraft Settings and crash analysis, and Alerts' modal alert
  (the Exit confirmation), mostly on their first page. Not yet opened in play: Alerts'
  dialogs 3 and 4, Help's lesson graph and lesson completion. Their register contracts are
  not checked in the game: FRAMELOCK wraps Linkage tables, and theirs hold only init and
  deinit, which run outside its window; the rest is entered through the host's dialog
  registry and MFC.
- **Sound.** Nothing compares what is heard: `sounddiff` only compares calls.
- **Direct3D's limits.** wined3d on llvmpipe, not a 1997 card, and four screenshots a
  flight between the panel logs. Hardware-specific paths in HG2D (real capability bits,
  texture memory running out) have run only in `hg2ddiff`. Direct3D has also not been
  run on a real display without `SEHFIX.DLL`; see [wine.md](wine.md).
- **Non-x86.** HG2D's floating precision and status; see
  [modules/hg2d.md](modules/hg2d.md).

## Unsettled questions

- **Two deliberate HG2D breaks are nearly or wholly unreached.** The depth-shade clamp
  (`value >= 14` in `hg2d_draw.inc`) is caught on one of 74 flights, `KENNEDY`, at pass 522.
  The haze tick's rate (`301u *` in `hg2d_init.inc`) is caught on none, with nobody flying, nor
  on `PARIS`, `STGEORGE`, `fs6N` or `NEWYORK` flown with `controls`. Neither was tried with
  every scenario. Haze set by hand may reach them, which needs the weather dialog in a script.
- **A weak gear break went uncaught.** The gear toggle's `0x3fff` changed to `0x3000` on
  `LEARLON` with `systems`, though removing the toggle is caught. The target may only be tested
  for nonzero; that has not been checked.

## A class of bug to look for

Three bugs fixed on 2026-11-01 share a shape: a retail routine's register behaviour is
part of its contract, one side was reimplemented in C, and the harness's stub of the
other side hid it, so each module passed 20,000 scenarios alone.

| caller | callee | contract | the stub |
| --- | --- | --- | --- |
| DYNAMIC | FE `+0x40` | reads to the caller's edi, returns registers shifted | took the buffer argument |
| MAIN | UTIL `+0x60`, `+0x6c` | keeps ecx across the call | stamped a constant over ecx |
| retail modules | UTIL `+0x2c` and others | keep their registers | `utildiff` looked only at results |
| retail ATC | NUMERIC `+0x18` | keeps ecx | `numericdiff` looked only at results |
| ATC `+0x20` | (MAIN's COM tuning) | GLOBAL in edi | the stand-in ignored edi |
| FE `+0x1c` | (GLOBAL `+0xcd2`) | a different shape: `fediff` reached only one of two values | |

Worth auditing: the other `CLEAN` thunks in `tools/difftest/maindiff.c`, which fix ecx and
edx to constants; any harness stub of UTIL or FE; and `fe.md`'s other register-shifting
entries (`+0x18`). A stub that does something the real callee does not is the thing to
look for. `tools/reg_contracts.sh` checks every module at once with the game's own inputs;
run it after changing any module's entry points, and with more flights and scenarios to reach
slots it has not seen called.
