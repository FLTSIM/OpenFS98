# WINDOW.DLL — the view windows and the eyepoint

WINDOW (module id 10) keeps the sim's windows: it creates and destroys the 3D
views, numbers them and their "Views" menu entries, writes their titles and
the status and message lines, covers the panel with them, and saves and
restores their layout with a situation. Its other half places the eye for
each 3D view's mode — cockpit, virtual cockpit, tower, track, spot plane,
the map — with the transitions between them, builds GLOBAL's view matrix,
and drives MAIN, G3D and HG2D through each window's frame.

It is reimplemented and verified: `make windowdiff-run` drives retail and
OpenFS98 through 20,000 whole-module scenarios with zero differences, and
`make window-breaks` (`tools/window_breaks.py`) applies 109 deliberate
breaks, of which 106 are caught and 3 are proven equivalences. It runs in-game
with `make smoke` passing.

## Shape and linkage

24,992 bytes of `.text` in two programs, like DEMO's. MSVC C runs from
`.text+0x0` to `+0x3490`; 0x74 bytes of constants follow; hand-written
assembly runs from `+0x3504` to the end, with ATC's stack-alignment wrapper
around every call. There is one 0x8ac-byte `.data` image (0x400 on disk):
`ImportTable` at `+0`, `Linkage` at `+0x68`, then all of the state. Its only
relocations are the Linkage's 32 code pointers; `tools/gen_window_data.py`
zeroes them, and also extracts the `.text` bytes the assembly reads as data
(below).

`dumplinkage.py` reported a 3-entry Linkage: it stopped at `+0x10`, which
holds the word `0xc0000000`. CHAIN (`0xe0000000`) and FE (`0xd0000000`) have
the same header and were hand-counted for the same reason. The tool now
accepts a non-pointer at `+0x10` only; elsewhere one still ends the table
(SOUND's `.data` goes on with `0xac` and a code pointer past its `+0x44`).

| slot | retail | callers | what |
| --- | --- | --- | --- |
| `+0x04` | `.text+0x020` | | init: `scenery\stars.fs` read whole (a length word, then the rest), thirty strings, CHAIN list 0x14 |
| `+0x08` | `.text+0x0e0` | | deinit |
| `+0x18` | `.text+0x110` | MAIN | a jump to `+0x44` |
| `+0x1c` | `.text+0x120` | MAIN | paint every shown window through its callback, between CHAIN messages 0x1b, 0x17..0x1a for each, 0x1c |
| `+0x20` | `.text+0x53c0` | | the view matrix's cofactors (GLOBAL `+0xcfa`..`+0xd14`) |
| `+0x24` | `.text+0x5381` | | a view block's turn `(view, code)` into its matrix at `+0x3e` |
| `+0x28` | `.text+0x563f` | | pitch, bank, heading from a matrix `(m, out)` |
| `+0x2c` | `.text+0x385a` | | a matrix from `(pitch, bank, heading, out)` |
| `+0x30` | `.text+0x3ab8` | | stretch GLOBAL's matrix to the window's shape |
| `+0x34`, `+0x38` | `.text+0x55af`, `+0x559e` | CONTROLS | the active view's mode back and forward, round 0..4, past the track (3) when GLOBAL `+0x11b0` has none |
| `+0x3c` | `.text+0x14e0` | MAIN, CONTROLS | a new 3D view `(kind, mode)`; past 32, alert 0xf580 and null |
| `+0x40` | `.text+0x1670` | MAIN, CONTROLS | close one; the front view becomes active |
| `+0x44` | `.text+0x1700` | | close every 3D view |
| `+0x48` | `.text+0x1750` | MAIN | show it and make it active (GLOBAL `+0x119c`, `+0x11a0`) |
| `+0x4c`..`+0x54` | `.text+0x17c0`.. | MAIN, CONTROLS | maximise over the panel, un-, toggle; the panel goes with the first and comes back with the last |
| `+0x58` | `.text+0x1890` | MAIN, CONTROLS | a view's mode |
| `+0x5c` | `.text+0x17a0` | MAIN, CONTROLS | a view's kind (0 3D, 1 map) |
| `+0x60` | `.text+0x560d` | CONTROLS | the active view's mode |
| `+0x64`..`+0x6c` | `.text+0x290`.. | MAIN, CONTROLS | toggle, show, hide |
| `+0x70`, `+0x74` | `.text+0x1910`, `+0x1930` | MAIN | flag `0x10000` on and off (half-size rendering) |
| `+0x78` | `.text+0x360` | MAIN, PANELS, WINMSG | a record of any type `(&out, title, type)`: a string id below 0x10000, else a string |
| `+0x7c` | `.text+0x500` | WINMSG | free one `(&rec)` |
| `+0x80` | `.text+0x5e0` | MAIN, PANELS | the records of a type `(&list or null, &count, type)` |
| `+0x84`, `+0x88` | `.text+0x6f0`, `+0x780` | CONTROLS | the next and previous of a type, wrapping |
| `+0x8c` | `.text+0x810` | MAIN, PANELS, CONTROLS | find by number `(id, type)` |

## The window record

0xa4 bytes on the host heap, listed by pointer in a host growable array at
`.data+0x110`. WINDOW owns the record; the host fills its own fields when it
makes the native windows (`+0x64`, see [host.md](host.md)).

| | |
| --- | --- |
| `+0x00` | title, a host string |
| `+0x04` | type: 3 a 3D view, 2 the map, others for other modules' windows |
| `+0x08` | flags: bit 31 shown, 30 no status line, 29 maximised over the panel, 16 half size |
| `+0x0c` | number: 0x100 + n for 3D views, 0x200 + n for maps — and their menu ids |
| `+0x18` | paint callback `(mode, rec, arg, out)`: mode 0 paints, 5 hit-tests |
| `+0x1c` | the view block, 0xc0 bytes (3D views and maps) |
| `+0x20`..`+0x2c` | rectangle, in 0x1000ths of the screen |
| `+0x50`..`+0x5c` | a second rectangle, saved and restored |
| `+0x60`, `+0x64` | surface size (host) |
| `+0x68`..`+0x74` | minimum and maximum size (3D views: 0x32 by 0x1e) |
| `+0x78` | z-order group (3D views 0x2000) |
| `+0x80` | the HG2D surface (host) |
| `+0x84`, `+0x88` | render size (host) |
| `+0xa0` | the host's 0x90-byte block |

The view block:

| | |
| --- | --- |
| `+0x00`..`+0x0c` | half-extents left, right, top, bottom, from the render size |
| `+0x10` | kind, 0 3D or 1 map |
| `+0x14` | mode: 0 cockpit, 1 virtual cockpit, 2 tower, 3 track, 4 spot |
| `+0x18` | turn: the low three bits eighths of a circle round, bit 3 straight down |
| `+0x1c`..`+0x24` | zoom for each mode |
| `+0x26` | the map's zoom |
| `+0x28`, `+0x2c`, `+0x30`, `+0x31`, `+0x34` | spot: heading, distance, whose heading (aircraft's, 1 the aircraft's turned with its pitch, or its own), pinned to the target, height |
| `+0x3e` | the turn's matrix |
| `+0x50`..`+0x54` | the turn's own angles |
| `+0x56` | last frame's mode, or a transition in progress |
| `+0x58`, `+0x70` | the eye's position and angles as last placed |
| `+0x5a`/`+0x7e`/`+0x96` | per axis: a glide's position, the distance left, the rate (the spot plane's spring uses them too) |
| `+0xac` | per angle: how far a glide's turn has to go |
| `+0xbc` | the view asks to cover the panel when active |

## The layout in a situation

CHAIN list 0x14 carries a situation's messages. 0x22 is one record: the
current kind, 0x3de, is a whole view packed by the field list at
`.data+0x150` (entry kind 1 the record, 2 the view block, 3 the 0x18-byte
layout entry — its place from the front and whether it was active — and 4
GLOBAL). The older kinds describe up to three views field by field:
4/7/10 rectangles (words into the low halves), 5/8 modes and zooms (a mode
from before the virtual cockpit moves up one), 6/9 turns, 11 which was
active, 12..14 shown, 0x1e their order, 0x4d the map's zoom, 0x66/0x67 the
eye, 0x73 three GLOBAL words. The three entries for those are made on the
first record of any kind. 0x21 (the situation is in) replaces the 3D views
with one per shown entry if GLOBAL `+0x1250` says so, stacks them and
activates the one that was. 0x23 saves every 3D view as a 0x3de record
through MAIN `+0x5c`..`+0x64`. 0x60 is a new aircraft: its `"Default View"`
X, Y, SIZE_X and SIZE_Y move a 3D view that sits within 25 of the old
aircraft's on every side.

## The eye

`frame_3d` (`.text+0x5840`) sets GLOBAL's current window and view
(`+0x1194`, `+0x1198`), follows the aircraft's attitude with a lag of four
units of 1/65536 (`.data+0x2b0`), and asks CHAIN list 0x13 whether anyone
places the eye; if not, the view's mode does. Cockpit and virtual cockpit
put the eye at the aircraft plus its offset (GLOBAL `+0xdb0`) turned by UTIL
`+0x4c`; tower looks from GLOBAL `+0xd50`, track from `+0xd74` (or from the
aircraft, `+0xdc2`), both kept two units above the ground; the spot plane
chases on a spring; the map looks straight down from a height its zoom interpolates
between powers of two. When the mode changes and GLOBAL `+0xdc0` allows,
the eye glides from where it was to where it is going over several frames,
view `+0x56` naming the transition (0x8, 0x14, 0x1e, 0x28, 0x32, 0x3c,
0x46, 0x50, 0x5a). Then the view's own turn, the zoom, the window's shape,
the stars (when the sky is dark and G3D has the file), the aircraft's model,
MAIN's scene between the cloud layers, and the frame presented.

## Retail behaviour worth knowing

All reproduced.

- **Divides overflow, and the host repairs them.** The look direction
  (`.text+0x50fa`) divides each horizontal component, shifted up 15, by the
  horizontal length. When that length rounds to the larger component (the
  vector normalised so its largest part's top bit is bit 28, the other
  part's high word under about 64), the quotient is +0x8000 and `idiv`
  overflows: looking within about a degree of north or east, and likewise
  for the pitch near level. So do the matrix-to-angles divides and the aspect's
  `div` for a wide window. In the game none of this crashes: the sim's main
  loop runs under a `__try` (`0x412140`) whose filter (`0x4119a0`) decodes
  the F6/F7, saturates the result (div: all ones; idiv: 0x7fff, or 0x8001
  when the operands' signs differ, with a 0x7fff remainder) and carries on.
  OpenFS98's WINDOW computes the same saturation in C and never traps; the
  harness models the filter for retail.
- **The map reads its own code as heights.** With GLOBAL `+0x390` 2 the
  map's zoom (view `+0x26`, copied to GLOBAL `+0x2b2`) indexes the interpolation masks unclamped, up to
  0x803 bytes past them: into the assembly's instructions. The generator
  carries `.text+0x3490..+0x3c94` so ours reads the same bytes.
- **A 3D view's menu id carries the record's top half.** The id is pushed
  from `dx` over the record pointer, so the host's menu gets
  `(rec & 0xffff0000) | number`; a map's carries its counter instead.
- **The title is built on an uninitialised stack buffer.** With a view kind
  other than 0 or 1, or a mode past 4, nothing is copied into it first and
  the title is whatever was there. Ours starts empty; the harness keeps clear.
- **Restoring with nothing loaded faults**: message 0x21 reads the first
  layout entry's view through a null array (`.text+0x2ce4`).
- **The track-to-tower transition aims twice** and keeps the second, so the
  turn starts and ends at the same angles.
- **The old-format entries are kept by pointer into a host array** that
  moves when it grows past 32, after which the pointers are stale.
- **UTIL `+0x5c` is a bare `ret`**, and UTIL's sincos, rotation and longitude
  services keep every register: retail calls them through `eax` = UTIL's
  Linkage, so that is what `eax` holds after.

### Dead code omitted

Only the `mov ax,ax`s, which change nothing. The first, overwritten write of
GLOBAL `+0x2e4` and the second `cbw` are kept.

## Differential harness

`tools/difftest/windowdiff.c`. Both DLLs are loaded into one process and run
in turn over the same script, driven through Linkage and the CHAIN list 0x14
callback, the way MAIN, CONTROLS, PANELS and WINMSG drive them.

- **host:** a stub over a bump arena reset before each side, so heap
  pointers agree; allocations not zeroed are filled with 0xa5, freed blocks
  with 0xdd (a list that grows has its old data marked freed, as a realloc
  that moves it would leave it). The window services are a model of the host's ([host.md](host.md)):
  the record's block at `+0xa0`, the z-order by group, `+0x90` and `+0x94`,
  and the sizes its `WM_SIZE` gives (halved, clamped, odd for a 3D view),
  applied at creation, on resizes and now and then on an update. The
  growable arrays, strings (made up from their ids, the titles' from
  `ENGLISH.FLL`), the profile reads and the stars file (the CD's, a
  synthetic one of any length its word claims, or none) are the scenario's.
  Every slot not modelled traps with its number.
- **The divide filter.** Retail's divides overflow tens of thousands of times
  a run. A vectored handler does what the host's filter does to retail's
  instruction; ours saturates in C. The run counts them (322,932 in the
  baseline).
- **UTIL:** retail's, its alert logged.
- **CHAIN, MAIN, G3D, HG2D, PANELS, CONTROLS, REALITY, ACONTAIN, NUMERIC:**
  logging stubs. MAIN `+0x9c` logs the eye as WINDOW left it in GLOBAL, so
  each window's eye is compared where it is used, and sometimes asks for a
  redraw (GLOBAL `+0x832`); G3D `+0x34` and HG2D `+0xbc` answer from the
  script; G3D `+0x3c` normalises as G3D does; list 0x13 sometimes claims the
  eye; GLOBAL `+0x11e0` is an aircraft object whose callback logs.
- **GLOBAL** is random per scenario and moves between frames, and is steered
  onto the boundaries the breaks found: the attitude at the lag's edge, the
  ground at a cloud band's edge or just under the eye, the dark test's
  threshold, layer 4's `0x5db`, `+0xcba` 0x8000, g loads about the 158
  clamp, the map's zoom where G3D's own frame begins, a spot view's spring
  at its 0x200000 reach, matrices on the 45-degree switch, eyes level with
  or on the meridian of what they look at.

The operations: 3D views created, past 32 too; other modules' records of
every type with their own paint callbacks and flag bits; closed one at a
time, by record and all at once; shown, hidden, maximised, made half-size,
activated; their kind and mode set and stepped; listed, walked, found;
resized by the user; raised; painted frame after frame; situation records
of every kind loaded, restored, saved, and a new aircraft's geometry; the
matrix services. A scenario ends at its first access violation, as the game
would; the only ones are the deliberate restores with nothing loaded.

Compared after every operation (and every frame): the transcript of every
import call with the strings and bytes it carries, WINDOW's whole `.data`
image (code pointers through a pairing learnt as they are first seen, its
pointer into itself as an offset), the arena (the records' paint callbacks
through the pairing) and GLOBAL.

Result: **20,000 scenarios, 0 differences**, about 1.5 minutes. The run
prints its coverage: every operation, each eye mode painted 180,000+ times,
every transition state, every record kind, 15,683 alerts past 32 views.

Deliberate breaks: `tools/window_breaks.py` applies each of 109 one-site
mutations to a copy of `window.c`, builds it as the harness's "ours" and
runs 2,000 scenarios. **106 are caught; 3 are proven equivalences**:
setting the view's kind after `set_mode` (which never reads it), `w >= 2`
for `w > 2` in the half-extents (the host makes a 3D view's width odd, so
the even branch never runs), and `<= 0` for `< 0` in the turn's limit (at
equality it adds 0). Twenty of the breaks first went uncaught. Each was a
generator that never reached the state, and the steering above is what
fixed them. Two first breaks were themselves equivalent, a clamp of 158.4
moved to 157.4 and a map clamp of 0x780 at 0x700, and were replaced.

## Portability

No platform calls: everything goes through the host, G3D, HG2D and UTIL.
The C half's one x87 compare (the vertical speed against ±158.4) is exact.
The divides are C with the host filter's saturation, so a native build
needs nothing from the filter.
