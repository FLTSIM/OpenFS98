# Running FS98 under wine

> **Parked, deliberately.** Everything below reproduces with the *retail*
> modules, so none of it is a decompilation defect, and the project is heading
> for native renderers that make it moot. Kept as a record of what was tried so
> it is not retried. Do not spend time here unless asked.

## The 3D view renders one frame and then stops

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

**Next, in order of promise:**

1. `make ddraw-gdi` -- switches wine's DirectDraw from its OpenGL path to plain
   GDI blitting (`HKCU\Software\Wine\Direct3D` → `renderer=gdi`). This is the
   long-standing fix for mid-90s DirectDraw games whose screen never updates,
   which is the symptom exactly. `make ddraw-default` reverts it. **Applied.**
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
