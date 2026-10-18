# REALITY.DLL — module id 32

**Reimplemented, verified, and running in-game.** REALITY handles crashes and damage. Each
frame it watches the crash code MAIN puts in GLOBAL `+0x832`. On a crash it
says why (WINMSG slot 5), pauses, plays the crash, turns a cockpit view to the
spot plane and greys the menu. A countdown in GLOBAL `+0x838` then reloads the
flight, may show the crash analysis, and lets the sim go on. It also rolls
UTIL's dice for stress damage, clears the damage flags on request (`+0x18`),
and draws the cracked windscreen into a cockpit view for WINDOW (`+0x1c`).

The crash analysis is a dialog with the descent rate at impact and a graph of
the last 22 seconds' height above the ground, read back from DEMO's replay
ring and drawn into a dialog control with HG2D's lines.

`src/reality/reality.c` is the transliteration. `make realitydiff-run` drives
retail and the OpenFS98 build over 20,000 whole-module scenarios: **0
differ**. `make reality-breaks` applies 67 deliberate breaks: 61 caught, 6
proven equivalent. `make smoke` passes with it installed: 27/27 modules, 95
gauges. Crashes only happen in flight, which wine does not reach, so in the
game that proves init; the harness covers the rest.

The survey is round 7 of [../preflight.md](../preflight.md). It had been set
aside for `ofstest` because it draws and runs a dialog. But it only draws
lines, a line is entirely its arguments, and the dialog is a parameter block
of callbacks of the kind DEMO's harness already stands in for.

## Binary and linkage

- Retail image base `0x208b0000`; `.text` 4,822 bytes. Hand-written assembly
  runs from `.text+0x20` to `+0x8a4`, with ATC's stack-alignment wrapper around
  every call. MSVC C runs from `+0xa50` to `+0x12b4`.
- `.rdata` at `0x208b3000`, 0x19c bytes: the graph's scale tables and
  constants, then the export directory.
- `.data` at `0x208b4000`, 0x400 bytes on disk and 0x510 mapped: one image
  with `ImportTable` at `+0x48` and `Linkage` at `+0xb0`. Its 19 relocations
  are 15 code pointers (an eight-entry jump table at `+0x00`, the Linkage, the
  dialog's three callbacks) and four pointers into the image.
  `tools/gen_reality_data.py` zeroes them, and `reality.c` installs its own.
- Imports: host, GLOBAL, CHAIN, CONTROLS (16), DEMO (7), HG2D (13), MAIN (3),
  NUMERIC, SOUND (39), UTIL, WINDOW, WINMSG. Win32: `DisableThreadLibraryCalls`;
  MSVCRT `_ftol`, which the reimplementation does inline.

| slot | retail | callers | service |
| --- | --- | --- | --- |
| `+0x04` | `.text+0x020` | | init: CHAIN lists 1 and 3 |
| `+0x08` | `.text+0x079` | | deinit |
| `+0x18` | `.text+0x78a` | | clear the damage: GLOBAL `+0x374`, `+0x364`, and each of `+0xb64`..`+0xb73` that is exactly 1 |
| `+0x1c` | `.text+0x8b0` | WINDOW | the cracked windscreen, into the current 3D view if it is a cockpit |

Neither `+0x18` nor `+0x1c` returns anything meant. `+0x18` never sets eax,
and `+0x1c` leaves whatever its last multiply or call did. WINDOW ignores both.

## What it does

**List 1, the crash** (`.text+0x2f7`). The code is GLOBAL `+0x832`'s low
byte. Nothing happens in slew (`+0x760` 0 with `+0x18` 5), with
`+0x365`, `+0x270` or a countdown already running (`+0x838`), with no `+0x830`,
or while a hold (`+0x83c`) lasts. Code 0xe becomes 6. Codes 0x10, 0x12 and 0x14
need `+0x84a`. An odd code, or one past 0x14, becomes 4. Then:

- string `0x258 + code` into WINMSG slot 5 for 0x3c frames;
- CONTROLS `+0x5c(8, 1)`, pause;
- SOUND event 0xe with the block at `.data+0x30`. With GLOBAL `+0xb24` set
  the block carries a callback (`.text+0x2c6`, which unpauses) and the crash
  counts as sounding until it is called. A zero result from SOUND clears that;
- the countdown at 5; and if `+0x846`, `+0x82c` and the current window is a 3D
  view in its first kind, WINDOW `+0x58(view, 4)` and the countdown at 7;
- MAIN `+0x30` greys the menu's `0x1001`..`0x1006`.

With no crash it keeps the descent rate for the analysis: GLOBAL `+0x2c8`
clamped to ±0xa674, times `0xc4d9b366`, the high word, into `+0x842`; and
`+0x2b8 >> 7` into `+0x844`.

**List 3, the countdown** (`.text+0x0ea`). The hold counts down. The countdown
steps by one each frame, but waits at 6 for `+0x846` and at 3 for the sound.
At 4 it unpauses. At 3 it keeps `+0x830`, reloads through MAIN `+0x44`, keeps
what the reload left in `+0x846` and puts the old value back. At 2, if the kept
`+0x830` was 2, comes the crash analysis. At 1 the sim goes on: the kept
`+0x846`, SOUND 0xe again (flags 8), MAIN `+0x34`, and a posted `0x100c2`
(`0x100c3` in slew). Then the stress roll and `+0x348`, which counts frames
while `+0x344` is set and `+0x2b4` at most 0x7fff.

**Stress** (`.text+0x144`). Every 0x79 frames UTIL's generator is rolled. Under
100 and not under GLOBAL `+0x372`, string 0x276 goes up for 0x6c frames, and
one of eight damages follows through the jump table, chosen by `+0x258 & 0xe`.
Separately, with `+0xc46` bit 3 and `+0x280`, a rand32 at most 0xd1b71 sets
`+0x374`.

**The windscreen** (`.text+0x8b0`). The point table at `.data+0xd0` is words
in 256ths of the view, y bit 15 a move. Each stroke is drawn four times a
pixel off in colour 0 (GLOBAL `+0x2ec`), then once in 0xf, clamped to the
view through HG2D `+0x34`.

**The analysis** (`.text+0xa50`). The descent in metres a second is kept for
the metric display. 25 replay entries come back from DEMO `+0x3c` (indices
-22..2; DEMO refuses the last three) into host array `.data+0x2c0`, with the
highest and lowest heights and a point each into `.data+0x2e0`. Then UTIL's
exchange `+0x90` fills the dialog's records from the table at `.data+0x280`:
units from GLOBAL `+0xc18`, the descent through the callback `.text+0xcc0`,
and `+0x844`. Then host `+0x10c` runs dialog 0x14050001 with the block at
`.data+0x228`. Its control procedure (`.text+0xd20`) has three modes. 0 attaches
the graph control: the record at `.data+0x440` and a view block, through host
`+0x74`. 1 paints: cleared to 0xff, the scale and labels, a grid in 0xf000,
the curve in 0xf00f, host `+0x84`. 2 lets the control go (host `+0x78`).

## Retail behaviour worth knowing

All reproduced unless said otherwise.

- **After the analysis, the countdown's test for 1 reads the dialog's
  result.** The step's count is in ah, and the dialog returns in eax, so a
  result whose second byte is 1 (0x100..0x1ff) also runs the step at 1. That is
  the resume, in the same frame.
- **The metric scale sees every sixth entry.** The height-scale search steps 8
  bytes through the feet table but 0x30 through the metric one, which has the
  same layout. So it only sees 10, 1000, then `.rdata+0xc0` (the low word of a
  double, 2,545,942,528) as limits. A metric range over 1,000 m takes its step
  from that double's high word, 1,070,825,944, and the graph is flat.
- **The scale search has no bound.** Past its table's last limit it reads on.
  It stops inside `.rdata` for any range up to 2,545,942,528, which is why
  `.rdata` is carried whole. Past that it runs through `.rdata`'s zero slack into
  `.data` and the code pointers there, which a rebuild cannot match. It is not
  reproduced. A range that large is 2.5 million km; the harness keeps heights
  within 10^9 of each other.
- **Units past 2 leave the scale as it was.** The axis is drawn with the last
  scale, or 0 the first time, and the curve divides by it.
- **The points' x is the index read as 64 bits with a zero top half**: -22 is
  4,294,967,274. Nothing reads it but the host's array.
- **The heights' labels start at 0**, from the high dword of an 8-byte local
  whose low dword the seconds' labels used; every other one is blank.
- **The upper descent clamp replaces only ax**: a rate over 0xa674 keeps its
  high word.
- **The rand32 test at 0xd1b71 is never on its edge.** No state of UTIL's
  generator draws 0xd1b71 (an exhaustive search of its 1,771,875 states).
- **`.text+0x144` compares GLOBAL `+0xc44` with 0x32 and goes on the same way
  either side.** Left out: it is the module's only dead code.

## Differential harness

`tools/difftest/realitydiff.c`. Both DLLs are loaded into one process and run
in turn over the same script, driven through Linkage and the two CHAIN
callbacks.

- **host:** a bump arena as in the other harnesses, and the host arrays. The
  dialog engine stand-in calls the block's check and control: attach, up to
  two paints, sometimes an unknown mode, and release. It logs what the dialog
  shows (units, both texts, every label, the graph's ranges) and answers from
  the script, including results whose second byte is 1. Host `+0x74` gives the
  record a size, from nothing through the plausible to any 32 bits, a surface
  and bits, and now and then fails.
- **UTIL and NUMERIC:** retail's, loaded for real, with their writable
  sections restored before each side. UTIL's generator has a seed, and NUMERIC
  has a buffer.
- **DEMO** `+0x3c`: heights from the script, refusing indices from 0 up and
  now and then others. A third of the scenarios use whole-unit heights, so the
  range lands exactly on flat, on 1, or on a scale table's limit ±1.
- **HG2D:** every line with its arguments, the colour, and the view extents of
  the record its surface belongs to, which is what the real HG2D draws
  through.
- **CHAIN, CONTROLS, MAIN, SOUND, WINDOW, WINMSG:** logged. MAIN `+0x44` moves
  GLOBAL as a reload might. SOUND keeps the crash's callback, and the script
  calls it later.
- **GLOBAL:** random, with every crash gate opened and shut, codes 0..0x18
  and beyond, the countdown driven onto the analysis, the descent around its
  clamps, the stress odds and thresholds, and the current view in every kind
  and mode. Each scenario runs under a random x87 precision control.

Compared after every operation: the transcript, REALITY's whole image (code
pointers paired, pointers into itself as offsets), the arena, GLOBAL, and
UTIL's and NUMERIC's state.

Result: **20,000 scenarios, 0 differences**, about 45 seconds, with no
faults. Per run: 128,776 reloads, 95,875 analyses (24,105 of them with a
result whose second byte is 1), 21,333 spot views, 53,784 windscreens and 17
million lines.

Deliberate breaks: `tools/reality_breaks.py` applies each of 67 one-site
mutations to a copy of `reality.c`, builds it as the harness's "ours" and
runs 2,000 scenarios. **61 are caught; 6 are proven equivalences**:

- `descent-low`, the lower clamp at -0xa673: -0xa674 and -0xa673 give the
  same high word, 9845 (a second break, clamping to -0xa600, is caught);
- `stress-32`, `<` for `<=` against 0xd1b71: no state of UTIL's generator
  draws it;
- `crack-clamp`, `>` for `>=`: at equality both store the limit;
- `collect-high`, `>=` for `>` on the fractions: a tie copies what is there;
- `analysis-one`, dropping the call to `.text+0xb00`: it returns the 0 the
  result already is;
- `scale-double`, the range rounded to a double: two 32.16 values differ by
  at most 49 bits, exact in a double, and under 24-bit precision the
  subtraction rounds the same either way.

Four breaks first went uncaught, and each was a harness gap. Ranges exactly
on a table limit, and a flat graph, needed whole-unit heights. A change to the
graph control's view block was released before the comparison could see it,
until the line stubs logged the view extents of the record they draw
through.

## Portability

No platform calls. The x87 arithmetic is the graph's: a handful of operations
in a fixed order, then truncation (`_ftol`). It is portable if a native
build keeps the order and, on x86, the precision control.
