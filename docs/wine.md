# Running FS98 under wine

> **The 3D view freeze is solved** (2026-10-19): wine swallows the integer
> divide faults that the host's filter exists to repair, and `SEHFIX.DLL`
> hands them back. The first section is the cause, the evidence and the
> setup that works. Everything after it is the investigation that came
> before, kept so it is not retried; where its conclusions turned out wrong
> it now says so.

## The cause, and the fix

### Setup that works

Tested under wine 9.0 with the retail modules plus OpenFS98's, a flight
loaded and flown from the keyboard, the 3D view animating throughout:

```sh
make sehfix-install   # SEHFIX.DLL into Modules\ -- the fix itself
make wm-managed       # wine's default; undoes an earlier wm-unmanaged
make window-reset     # native 640x480 client at the top-left
make run              # inside a 1024x768 virtual desktop -- for focus, see below
```

with `[HARDWARE] Device=` (empty) and `Mode=640x480x8`, `FULL_SCREEN=2`, and
no `renderer` override under `HKCU\Software\Wine\Direct3D`.
**Untested with the fix**: `Mode=640x480x16`, a Direct3D `Device=`, running
without the virtual desktop, and resizing the window. Each was a suspect
before the cause was known and may well be fine now; try them one at a time.

`make sehfix-remove` takes the module out; `make sehfix-log` shows its log.

`make run` asks for the virtual desktop on the command line, `wine explorer
/desktop=FS98,1024x768 FLTSIM98.EXE`, so only the sim gets one (`make run
VD=` for none, `VD=800x600` for another size). A desktop set in the
registry (`HKCU\Software\Wine\Explorer`) is opened by every wine program,
the difference harnesses included; set per program under `AppDefaults` it
did not take effect here. `make vd-on`/`vd-off` now only clear those
registry settings.

### What happens

The host runs its main loop under a `__try` (`0x412140`) whose filter
`0x4119a0` repairs integer divide faults: it decodes the `div`/`idiv`, writes
a saturated quotient into the context, steps eip past it and returns
`EXCEPTION_CONTINUE_EXECUTION` (see [modules/host.md](modules/host.md)).
Retail code divides by zero and into overflow routinely and relies on it.

On 32-bit Windows a fault inside a window procedure unwinds out through
`DispatchMessage` and reaches that `__try`. Wine 9 calls window procedures
from `KiUserCallbackDispatcher`, which wraps the call in a catch-all of its
own -- that is the `err:seh:KiUserCallbackDispatcher ignoring exception` seen
in every log. So under wine the host's filter never sees a divide raised in a
window procedure. The procedure is abandoned at the faulting instruction and
everything it had not yet done stays undone.

Two such divides happen in every flight, both retail code:

| address | where | what |
| --- | --- | --- |
| `0x406525` (and `0x406538`, `0x40654a`, `0x40655d`) | host `WM_SIZE` (`0x4060b0`) | `idiv [record+0x60]` and its siblings, while a view is still 0x0; four records at startup |
| `0x207376e2` (and `0x207376fc`, `0x20737716`) | HG2D `.text+0x66e2` | a gradient `(c1 - c0) << 5 / edge` over a zero-length edge, `idiv ebp` |

The `WM_SIZE` ones abandon the tail of each startup layout: the view block's
scales at `+0x44`..`+0x50`, `0x406590`, and the `+0x70` resize callbacks. The
HG2D one is the freeze. Most frames are drawn straight from the main loop,
where the filter does see the fault. But when one is drawn from inside a
window procedure and hits a degenerate edge, wine unwinds out of the middle
of the frame, past G3D's clipper, which swaps the span routine at
`.data+0x4910` and restores it only on the way out
([preflight.md](preflight.md#g3d-id-14)), and past whatever else HG2D and G3D
had in flight. The 3D view never draws again; the panel, which is drawn
separately, carries on.

### The evidence

With `WINEDEBUG=+seh`, one session of each:

- **Without the fix**: five divide faults in the whole session, all
  `0xc0000094`, all `ignoring exception` -- the four `WM_SIZE` ones at
  startup and **exactly one** at HG2D `0x207376e2`, after which there were
  none: the rasteriser had stopped.
- **With the fix**: about 86,000 faults at each of HG2D's three divides,
  all repaired and resumed, 17 at G3D `0x206e18c6` (`.text+0x8c6`, inside
  service `+0x28`), and **zero** exceptions swallowed. The view animated.

A rasteriser that faults thousands of times a minute when working, and
faulted once and fell silent before, is the freeze seen from the inside.

### SEHFIX.DLL

`src/winecompat/sehfix.c`. A module with id 8 (unused by retail) that imports
nothing; its `DllMain` registers a vectored exception handler, which runs
before any frame-based handler, wine's included. On `0xc0000094` or
`0xc0000095` it walks the SEH chain from `fs:[0]` as x86 Windows would see it:
frames whose handlers live outside the game's directory -- wine's own -- are
skipped, and the first frame the game owns decides. If that is the host's
main-loop frame (handler `0x414cd6`, scope table `0x416110`, try level 0) it
calls `0x4119a0` and returns its verdict; anything else is left to wine as
before. It has no saturation logic of its own, so the result is what retail
computes on Windows.

The log confirmed the model before it was trusted: every chain was the host's
frame alone, or wine's `ntdll` catch-all and then the host's frame -- no MFC
or module frame in between. That matters because the game's own SEH users
(the MFC dialog DLLs in `English\`, `SIMSUI.DLL`, BOOSTER's C++ handler) would
see a fault before the host's filter on Windows, and a handler that simply
called the filter for every divide would have overridden them. The chain walk
keeps their order.

`C:\openfs98-sehfix.log` (`make sehfix-log`) holds the first 32 faults with
their chains and, at exit, a count per address and verdict. A killed sim
writes no counts.

### What it means beyond wine

- **It is not a decompilation defect and not a DirectDraw bug.** With
  `Device=` empty the sim never `Blt`s, `Lock`s or `Flip`s through DirectDraw
  at all (the ddraw trace has only surface creation, `GetSurfaceDesc` and one
  `Restore`); frames reach the screen through GDI. The "DirectDraw present
  failing" framing below was wrong.
- **Native builds must reproduce the repair** wherever retail divides into
  it: the host's `WM_SIZE`, HG2D's gradient setup, G3D `+0x28` and WINDOW's
  look direction so far. A saturated quotient is behaviour; WINDOW's
  reimplementation already computes it without trapping, and G3D and HG2D's
  will have to as well.

## History: the 3D view renders one frame and then stops

**Symptom.** A flight loads and the panel is live — instruments respond, the
aircraft accelerates, the attitude indicator follows the controls — but the 3D
viewport keeps showing its first frame. Present with the retail modules, so it
is the environment and not a decompilation defect.

**What the game is doing.** The config records the renderer:

```ini
[HARDWARE]
Device=DirectDraw HAL (Direct3D)
Mode=640x480x16
```

FS98 composes that name by appending a Direct3D suffix to the DirectDraw driver
description; the three suffixes it knows are `(Direct3D)`, `(Direct3D MMX)` and
`(Direct3D RGB)`, all of them strings in `HG2D.DLL`. So the 3D view goes
through Direct3D immediate mode -- wine's `ddraw` on top of `wined3d` -- while
the panel is drawn by a separate path. A live panel over a frozen 3D view is
what a failing D3D present looks like.

`[HARDWARE]` has only those two keys, both read by the main executable, and the
Display options dialog has an **Enable hardware acceleration** checkbox (the
string lives in `English/Options.DLL`). Clearing `Device` is that checkbox
turned off, which should drop the sim onto its own software rasteriser.

**The change now in place:**

```ini
[HARDWARE]
Device=
Mode=640x480x16
```

The game starts and runs with the key cleared, so the empty value upsets
nothing. Whether the 3D view animates is the open question.

**Result: no change**, with either renderer.

## What the symptom actually is

Forcing a repaint -- resizing the window, or switching view mode -- turns the
3D area **black**, while the panel redraws perfectly and even re-lays itself out
for the new window size.

That reframes the whole problem. The "frozen first frame" was never being
redrawn at all: it was stale pixels left in the window by a single successful
blit at startup. Clear the window and nothing replaces them. The 3D path has
been failing since the second frame; it only looked frozen because nothing
repainted over it.

So:

- the panel and the 3D view go through different paths, and only the 3D one
  fails;
- whatever fails does so from frame two onward, not from the start, which rules
  out "never initialised" and points at something that goes wrong *after* the
  first present -- a lost DirectDraw surface that never gets restored, or a blit
  that silently stops succeeding;
- the aircraft, panel, gauges, scenery and flight model are all fine. This is
  one blit, not a broken sim.

*In hindsight:* the first two points stand, the third does not. Nothing was
wrong with the blit: the frames stopped being drawn, because the rasteriser
had been unwound out of mid-frame (see the top of this file). A forced
repaint shows black because nothing draws into the cleared area.

## 8-bit changes the failure

With `Mode=640x480x8` the 3D view **animates** -- for a few seconds, then
freezes again. That is the first thing to actually move the needle, and it
confirms the depth conversion was a real obstacle: the desktop is 24-bit, and
16-to-24 on every windowed blit is the worst-travelled path in wine's ddraw,
where 8-to-24 is a palette expansion and very well travelled.

What remains is a different bug: something stops the view a few seconds in
rather than at frame two. Candidates, in no particular order -- a DirectDraw
surface going lost and never being restored, scenery or texture loading
completing and switching the renderer to a path that fails, or the swallowed
exception below actually killing the render loop.

`err:seh:KiUserCallbackDispatcher ignoring exception` appears four times in a
short run. Exceptions thrown inside window callbacks and dropped by wine are a
plausible shape for "works, then stops".

*In hindsight:* that last candidate was the cause. The four were the
`WM_SIZE` divides; the freeze is a fifth, in HG2D. The depth-conversion
explanation was never confirmed. When the freeze comes depends on when a
frame drawn from a window procedure first meets a degenerate edge, which
may be all the colour depth changed; 16-bit has not been retried with the
fix.

Exclusive fullscreen (alt+enter) still freezes the view, and earlier wedged the
session hard enough that nothing was clickable. Do not use it: wine reports

```
err:winediag:is_broken_driver Broken NVIDIA RandR detected, falling back to RandR 1.0
```

and mode-setting is exactly what exclusive fullscreen needs. `make kill`
recovers from a wedged sim.

## The window must stay at its native size

The 3D view renders at exactly 640x480 and does not survive being asked for
anything else. Resize the window and the view goes black -- and the new
geometry is written to `[MAIN] Location=`, so it stays black on the next
launch, and maximising does not help either. Everything else keeps working,
which is what makes it look like a fresh bug rather than the resize.

*Not yet retried with SEHFIX.* A resize is a `WM_SIZE`, the handler whose
divides wine swallowed (`0x406525`), so this may have been the same bug.
`make window-reset` still recovers if it is not.

```ini
Location=635,252,1283,785     ; 648 x 533 -- native: 640x480 client + chrome
Location=73,75,846,689        ; 773 x 614 -- resized: 3D view black
```

`make window-reset` puts it back to `0,0,648,533`: native size, parked at the
top-left so it fits inside a 1024x768 virtual desktop. The position the game
saves can leave most of the window off the right edge of that desktop, which is
exactly what tempts you into resizing it.

**Drive it from the keyboard instead of reaching for the panel:**

| | |
| --- | --- |
| `F4` / `F3` / `F2` / `F1` | throttle full / up / down / cut |
| `.` | brakes |
| `ctrl` + `.` | parking brakes |
| `Y` | slew mode |
| `S` | cycle view mode |

## What the virtual desktop fixed

The opening screen no longer locks up with the "Fly Now!" button flickering,
and it no longer takes two or three launches to get in. That is the same focus
fight documented further down as an environment quirk -- the host window
manager and the sim arguing over focus -- and running inside wine's own desktop
settles it, because the host WM never touches the sim's window.

Confirmed again on 2026-10-19, and the virtual desktop is still needed with
the fix in. Without it and with the WM managing the window, "Fly Now!"
would not take a click. With `make wm-unmanaged` (`Managed=N`), the WM never
gives the sim keyboard focus and keys go to whatever window had it before.
`wm-unmanaged` was a guess at the freeze; leave it off.

**Superseded** -- the list below was the plan before the cause was found.
None of it turned out to be needed, and `ddraw-gdi` is no longer applied.

1. `make ddraw-gdi` -- switches wine's DirectDraw from its OpenGL path to plain
   GDI blitting (`HKCU\Software\Wine\Direct3D` → `renderer=gdi`). This is the
   long-standing fix for mid-90s DirectDraw games whose screen never updates,
   which is the symptom exactly. `make ddraw-default` reverts it.
2. `Mode=640x480x8`. The software path is at home in 8-bit, and a palettised
   primary surface takes a different route through wine's ddraw.
3. Toggle `FULL_SCREEN` in `[PREFS]`, currently 2. Exclusive-mode page flipping
   and windowed blitting fail in different ways.
4. `make trace-render` and read what wine complains about while a flight is
   running -- fixme/err only, so the log stays readable.

## The sim has two top-level windows

Worth knowing before chasing this further:

```
0x08000008  400x300  class fltsim98.exe
0x08000012  640x480  class fltsim98.exe
```

640x480 matches `Mode=`, so that is the main frame. What the 400x300 one is for
is not yet established, and if the 3D view lives in its own window then "the
view does not update" may be a window-repaint problem rather than a DirectDraw
one -- a different and more tractable bug.

The baseline config, as the game wrote it with hardware acceleration on, is
kept in `docs/FLTSIM98.cfg.d3d-baseline` so the starting point can be restored.

## Driving the game without a mouse

`tools/drive/fsdrive.c` builds `bin/fsdrive.exe`, which finds the window by its
class (`FS98MAIN`) and sends real key events to it, so a test can dismiss the
opening screen, enter slew and move the aircraft unattended:

```sh
wine bin/fsdrive.exe 2000 ENTER 20000 y 1500 UP*30 F4*12
```

Tokens are key names or millisecond delays, and `NAME*N` repeats. It exists
because screenshots alone cannot answer the question: a parked aircraft renders
a static view perfectly correctly, so the view has to be made to move before
"static" means anything.

**It does not currently work.** The window is found and `SendInput` is called,
but the sim never leaves its opening screen -- every "flight" captured this way
turned out to be a screenshot of the menu. Synthetic input from another process
is evidently not reaching the game under wine; `SetForegroundWindow` across
processes is the usual suspect. Until that is solved, anything that needs the
sim actually flying has to be driven by a person.

## Screenshotting it is harder than it looks

Three traps, each of which produced a wrong answer here before being
understood:

- **Capture the window, not a screen rectangle.** The window lands in a
  different place on each launch, so a fixed crop compares position as much as
  content. That is what made an earlier investigation blame a rebuilt module
  for a flicker retail has too.
- **Re-find the window every time.** The game recreates it across a display
  mode change, and `import -window` against a stale id blocks until killed.
- **The window may be behind everything else**, in which case the capture
  succeeds and shows nothing of interest.

For anything about what is actually on screen, a person looking at it beats
this automation.

## A `pkill` footgun worth remembering

`pkill -f FLTSIM98` matches *its own shell*, because the pattern appears in the
command line being run -- which kills the whole invocation, exit code 144.
Bracketing the pattern (`'FLT[S]IM98'`) avoids that, but only until some other
part of the same command, a heredoc for instance, happens to contain the name
too. Put kills in their own command.

`make kill` used `pkill -f fltsim98`, which is case-sensitive and never
matched `FLTSIM98.EXE`; it now runs `wineserver -k`, which ends every process
in the prefix without matching anything.
