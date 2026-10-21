# PANELS.DLL — the instrument panel and its gauges

PANELS (module id 15) draws the instrument panel and hosts the gauges: the
CD's 120 `.GAU` files are modules of id 19 that import only PANELS. It is
MSVC C, linked at `0x10000000`, `.text` 50,038 bytes, with 39 functions in a
Linkage of 43 entries (`+0x04`..`+0x08` init and deinit, `+0x18`..`+0xa8`).

**Status: reimplemented, verified, and running in-game** (`make smoke`: 27/27 modules, 95 gauges, with this PANELS).
`src/panels/panels.c` carries all 201 functions; `make panelsdiff-run` drives
it and retail side by side through the CD's own gauges (3,000 scenarios, 0 differ), `make
panelsunit-run` drives each of its 28 image routines alone (20,000 cases each, 0 differ), and `make
panels-breaks` breaks it on purpose (164 of 200 sampled mutants caught; the rest below). The HG2D model the harness
needs is checked against retail HG2D (`make hg2dmodeldiff-run`).

## Imports

From `impscan.py` and `dumplinkage.py`, every HG2D site read by hand:

| module | what PANELS uses |
| --- | --- |
| HOST (1) | 38 services, 9 of them no done module calls; see below |
| GLOBAL (2) | about 60 fields, read and written directly |
| ACONTAIN (23) | `+0x30` ×5 |
| CHAIN (4) | `+0x18` ×6, `+0x1c` ×6, `+0x20` ×2 |
| HG2D (13) | `+0x48` ×8, `+0x4c` ×2, `+0x5c`, `+0x64` ×2, `+0xac` ×2, `+0xb0` ×4; and the Linkage words `+0x8c` ×5, `+0x90` ×6, `+0x94` ×5, `+0x98` ×1, which are pointers into HG2D's `.data` |
| gauges (19) | in the ImportTable, never read |
| MAIN (3) | `+0x5c`, `+0x60`, `+0x64` ×2 each, `+0x80` ×4 (its result read) |
| SOUND (39) | `+0x44` ×5 |
| WINDOW (10) | `+0x78`, `+0x7c`, `+0x80` ×8, `+0x8c` |

Host services: `+0x03c`, `+0x050`, `+0x064`, `+0x068`, `+0x07c`, `+0x088`
(×15), `+0x0a8`, `+0x0ac`, `+0x0d0`, `+0x0e0`, `+0x0f0`, `+0x158`, `+0x184`,
`+0x1b4` (×16), `+0x1b8` (×15), `+0x1bc`, `+0x20c` (×23), `+0x214` (×25),
`+0x218`, `+0x21c`, `+0x26c`, `+0x270`, `+0x27c`, `+0x284`, `+0x28c`,
`+0x298`, `+0x29c`, `+0x2a0`, `+0x2a8`, `+0x2b4`, `+0x2b8`, `+0x2bc`,
`+0x2f8`, `+0x2fc`, `+0x300`, `+0x304`, `+0x308` (×10), `+0x310`. The
preflight read `+0x298` and `+0x29c` as the way to the gauges.

Win32 directly: GDI (DIB sections, fonts, `DrawTextA`, `FillRect`),
resources, and MSVCRT's `fopen` for the panel files.

## What PANELS needs of HG2D

PANELS draws a gauge as HG2D's textured polygon, as G3D does: it sets GLOBAL
`+0xc8c` (the texture), `+0xcb6`, `+0xca0`, `+0xd4c` and `+0x2f0` (-1), sends
four vertices through `+0x48` (the window record, x, y, w = `0x10000`, u, v,
all 16.16) and closes through `+0x4c` (the record + `0x80`). A gauge's
texture is refreshed by freeing the old one (`+0xb0(handle, 1)`, keeping
its pixels), redrawing the pixels into its own descriptor and making a new
one (`+0xac(descriptor, 0)`), whose handle goes to GLOBAL `+0xc8c`.

The texture pool (`0x2078fc50`, `0x2078fd60`): 200 records of `0x34` bytes
from `0x20797f60`. `+0xac` takes a descriptor of `[0]` 1 and `[1]` `0x100`
with flags 0, finds the first record whose pixels word is 0, and puts the
descriptor's pixels (`+0x18`) there, or `0x10000` bytes from host
`+0x20c(4, 0x10000, &record, 0)`. It returns the record, or 0 for any other
descriptor, a full pool or a failed allocation (which leaves the record
free). `+0xb0` does nothing for a handle outside the pool. Inside, it
releases the record's hardware texture (`+0xc`, a COM object), frees the
pixels through host `+0x214` unless flags bit 0 is set, and clears the first
four dwords. Its eax is the caller's for a handle outside the pool;
inside, it is the record's `+0xc` word (tested through eax), or the host
free's result when it frees.

The palette, `+0x8c` (`0x207950a0`, 256 RGB triplets): PANELS writes up to
32 colours into it from a panel file and then calls `+0x64`, `+0x5c`,
`+0x64`. `+0x5c` (`0x20731947`) realises it through host `+0x290`. It
rebuilds the colour words `+0x94` (`0x207954c0`, read by byte at
`index * 2`), the realised palette `+0x98` (`0x20795cc2`) and the remap
`+0x90` (`0x2079294e`, a byte a colour and their inverse after). The remap
is the colour words' low bytes, or a brightness ramp:
`sqrt(r*r + g*g + b*b) / 14` plus the byte at `0x207955c0`, with
`0x74`..`0x7f` kept (`0x207323f4`). PANELS tests the remap's first byte for
`0xa`. `+0x64` (`0x2073261f`) clears `0x20796004`, runs `+0x60`'s body
and sets `0x207956c0`.

## The HG2D model

`tools/difftest/hg2d_model.inc`: a fake HG2D Linkage whose ten used slots
are models, every other slot a thunk that logs its number. It is not a
transliteration: in the game PANELS runs against retail HG2D, and in a
harness both sides see the same model, so the model needs to be plausible
where it is not exact.

- **Exact: the texture pool.** `make hg2dmodeldiff-run` loads retail HG2D
  (its DllMain does nothing, and these slots need only the host's
  allocator) and runs it and the model over the same scripts:
  - descriptors right and wrong, and flags;
  - handles live, freed, null and outside the pool;
  - long scripts that fill the pool;
  - host allocations failing now and then.

  It compares every return and record after every operation: **20,000
  scenarios, 0 differences**. Three deliberate breaks of the model (a pool
  one record short, a failed allocation taken, the wrong flag bit kept)
  are each caught.
- **Plausible: the tables.** Each scenario picks one of three kinds: an
  identity colour map with a grey palette, the same with the brightness
  ramp, or random. `+0x5c` rebuilds the remap from the palette as
  `0x207323f4` does, so the colours PANELS writes come back to it.
- **Logged**: the vertices and closes (with GLOBAL `+0xc8c` and `+0xc90`),
  the realisations (with a hash of the palette), refreshes, and each
  texture made and freed. `+0x48` counts GLOBAL `+0x2f0`.
- **Registers**: each slot's wrapper leaves ecx and edx at constants of the
  slot, as the other harnesses' stubs do.

Two lessons from building the check, both found because a deliberate
break went uncaught:

- A transcript that stops growing when full compares nothing past that
  point. It now stops the run.
- `#include "..."` looks in the including file's directory before `-I`. A
  broken copy of the model has to sit beside a copy of the test that
  includes it.

## The module

`src/panels/panels.c` is the transliteration: every function is headed by its
retail address, in retail's order, and everything lives in one `.data`
image laid out as retail's (`0x16580` bytes from `0x1000f000`: the
ImportTable at `+0x0`, the Linkage at `+0x50`, the strings, from `+0x610` the
gauge variables' table, then the buffers) so that the harness can compare the
whole of it. `tools/gen_panels_data.py` extracts the image and its
constants; the code pointers in it (83) and the links to GLOBAL (311) are
installed at load by `panels_install()`. The floating point is retail's,
operation for operation, on the x87: `long double` where retail keeps a value
on the FPU stack, `double` or `float` where it stores one, `pn_ftol` for
MSVCRT's `_ftol`, `fsin`, `fcos` and `fpatan` inline. Integer divides that a
zero divisor can reach are inline `div`/`idiv`, because retail's fault into
the host's divide repair, and a compiler told it cannot happen turns the path
into `ud2` (it did).

What it is made of, by retail address:

| range | |
| --- | --- |
| `1020`..`12a0` | init, deinit, `+0x18` (a new aircraft: its panel unloaded and loaded, or only the windows refreshed) |
| `12e0`..`1950` | window show and hide, all, toggle, options; the element dispatch (`+0x40`..`+0x5c`) |
| `1970`..`1b60` | the same over an element's children (`+0x60`..`+0x7c`) |
| `1ba0`..`1d20` | a gauge's mouse rectangles, the windows by ident, `post` |
| `1d40`..`22f0` | a window's ident, the `.BMP` loader, a window's surfaces and gauges rebuilt, the paint callback |
| `2410`..`36b0` | `panels_load`: the colours into HG2D's palette, each `[WindowNN]` as a panel window (bitmap, size, layout, bounds), its gauges loaded; `bounds_build`; `gauge_load` |
| `3740`..`5660` | the eight bitmap scalers: nearest (`scale_24`, `scale_8`), filtering (`smooth_`), area averaging (`shrink_`), and the two 8-bit to 8-bit ones that match HG2D's palette (with a cache of 0x8000 colours) over the panel's background or not |
| `5660`..`6dc0` | `surface_from_bitmap`, the conversions to 8, 15, 16, 24 and 32 bits lit by a light surface, the mask, `clip`, the black-edge blending `edges_24` |
| `6dc0`..`73e0` | `blit`, `compose`, the DIB section (`dib_new`) and the surface over its bits |
| `7420`..`87e0` | the 38 functions of the gauge variables' table (heading, altitude, speeds, the engines, MAIN's and SOUND's state) |
| `8830`..`8d70` | the named variables a gauge publishes, the variables' initialisation and lookup |
| `8db0`..`9e60` | the failure flags, `trim` and `token`, the resource bitmap loader, `rotate`, the gauge walks, the CHAIN callbacks (frame, reset, situation, engines, command) |
| `9e70`..`d340` | the eight element types, seven operations each |

### The elements

A gauge is a tree of elements it built itself in its `.data`; PANELS scales
them to the window, loads their bitmaps from the gauge's resources, evaluates
their variables and draws them. An element is a record: `+0x00` its type,
`+0x04` its bitmap's resource, `+0x08` where it sits, `+0x20` its gauge's
header, `+0x24` its parent, `+0x28` its children, `+0x2c` its failures (the
flags GLOBAL's `+0xb02`.. keeps for what can fail on an aircraft), `+0x30` its
flags (bit 0 transparent, bit 1 keeps what it covers, `0x40000000` off,
`0x80000000` drawn) and `+0x34` what it raises. The type decides the rest:

| type | | operations (install, evaluate, convert, draw, take down, free, new) |
| --- | --- | --- |
| 0 | a still bitmap | `9e70`, -, `9f30`, `9f80`, `a070`, `a110`, `a140` |
| 1 | a needle: a bitmap turned about a pivot by an angle a table maps a variable to | `a170`, `a470`, `a6f0`, `a790`, `acf0`, `adb0`, `ae10` |
| 2 | a string of digits drawn by GDI into a DIB, up to three variables | `ae40`, `b1a0`, `b250`, `b280`, `b580`, `b630`, `b740` |
| 3 | a bitmap that slides on two variables | `b770`, `b860`, `b940`, `b9a0`, `bae0`, `bb90`, `bbc0` |
| 4 | an icon of several frames, one chosen by a variable | `bbf0`, `bd30`, `bd80`, `be10`, `bff0`, `c0a0`, `c100` |
| 5 | a strip seen through a window that slides on two variables | `c130`, `c300`, `c3e0`, `c460`, `c5f0`, `c6a0`, `c6f0` |
| 6 | a shape turned on its pivot with three variables | `c720`, `cbf0`, `ccd0`, `cda0`, `d1d0`, `d280`, `d300` |
| 7 | nothing, a node for children | `d330`, -, `d330`, -, -, -, `d340` |

The returns of the handlers are whatever retail leaves in eax and are not
reproduced (they return 0); the harness never compares them.

### Retail's bugs, kept

Each is noted in `panels.c` where it happens.

- The smoothing scalers (`smooth_8`, `smooth_24`) weight pixels by the loop
  counters, not by positions, and write their rows one late (row 0 never); so
  their blend is nil until the output is hundreds of pixels wide, and the
  centre case (`i == j == 0x8000`) cannot happen.
- `shrink_8_to_8` counts the first pixel's grey weight twice; `to_555` tests
  bit 15 where it means the colour; `edges_24` tests `i & j` where it means
  both nonzero.
- `window_ident` reads each table entry's window without checking it: a
  window that is not in the first of the table faults on the first null
  entry. `named_clear` hands `free` the address of a local, so nothing is
  freed; `named_remove` of an empty list faults on its head.
- `bitmap_load` passes `fclose` the `FILE *` of a file that did not open;
  `panel_free` reads the record `+0x10` it has just freed.
- `panels_load` takes a window the table does not name to be a panel of the
  last window (retail's `ebp`, stale; here 0); a window without a layout of its
  own takes its size from the first's `SIZE_MM`.
- The variables `v_7d30` and `v_8070` (the zoom) scale, when no case
  matches, the stack slot of an argument (the op, or the variable's own
  address).
- The needle's quad, the strip's and the shape's: the window clamps leave a
  corner unclamped (the needle's second corner's x has no lower bound; the
  shape's first corner's x).
- The digits: the DrawText measure leaves the rectangle's corner as the
  stack had it. A type-4 icon in mode 2 (or past 4) steps to its own
  pointer.
- Past 5 `dib_new` writes through an uninitialised pointer.

## The harness

`tools/difftest/panelsdiff.c` loads retail PANELS.DLL and the build, and
gives both the same world; `make panelsdiff-run [N=...] [START=...]`.

### Module mode

The world is stubs of HOST (the 38 services it calls: the allocator, the
lists with their deferred removal, surfaces, windows, profile files, module
loading), GLOBAL (a blob, typed from the variables' table), CHAIN (the
callbacks it registers, kept for the harness to dispatch), ACONTAIN (the
aircraft's panel file), MAIN, SOUND and WINDOW, the HG2D model, and **the
CD's own gauges**: the host's module loader is a stub that loads the `.GAU`
from the game's tree and links its PANELS import to the side under test, so
the gauges' code calls PANELS's Linkage as it does in the game and the
elements are their own data. The panel files and bitmaps are the CD's
(seven aircraft). A scenario is a random script of 3..40 operations -- an
aircraft's panel loaded, frames, resets, situation messages and commands,
windows shown and hidden, paints, mouse rectangles, named variables, GLOBAL
changed, elements made, deinit and init -- run once against each side.

Compared: the transcript of every call across the boundary (host, CHAIN,
GDI, HG2D) with its arguments and results, after each operation PANELS's
whole `.data` image, every live block of the arena (the surfaces' pixels
included), the DIB sections' bits, GLOBAL, and each loaded gauge's writable
data. Memory is a bump arena, so the two sides see the same addresses; a
pointer into a module image compares as an offset (and a constant that equals
an address of the other side's image is the same word on both). GDI is
virtualised: PANELS's imports are hooked, its handles are numbered, its
DIBs are regions. The host's integer-divide repair is modelled. Retail faults
(an unchecked `NULL`) are compared as outcomes: the two must agree up to
retail's fault, and what follows is not compared.

What the host's allocator does matters: `+0x20c` ends the process on a failure
unless the flag is 4 (HG2D's textures), when it returns -1 with `*out`
cleared; `+0x1b4` fails only for a type past 5. A harness that let an
allocation fail and go on made retail do what no game could, and ours do
something else.

### Unit mode

`panelsdiff unit [N [START]]` (`make panelsunit-run`) calls the 28 image
routines (the scalers, `surface_from_bitmap` over the CD's panel bitmaps and
the gauges' 538 resources, the conversions, the blits, `clip`,
`edges_24`, `dib_new`, `surface_over`, `bounds_build`, the bitmap loaders)
one at a time over bitmaps and surfaces made at random (with the colours
that matter in them: black, (1, 1, 1), and near them) in the same arena, and
the variables' lookup over the whole of the table; retail's by address, ours
through a table exported by a build with `-DPANELS_UNIT`. A routine wrong in
the fourth decimal of a blend shows in a window as a byte, and here as a case.
Sizes include images hundreds of pixels across, because the smoothing
scalers' weights are the coordinates.

### What it found

- Type 3's second multiplier is scaled by the gauge's **y** scale: the
  stack slot `[esp+0xc]` read after a `pop` is another's. Found by one gauge's
  doubles.
- `v_8070` (the view's zoom) scales its *first* argument's slot, not the op:
  found by a text buffer.
- A division by a zero that is constant on a path (`nbk / n` with `n` never
  incremented) was compiled to `ud2`; divides are inline `div`/`idiv` now.
- The variables table has 383 entries; the first sampling reached 171.
- Everything the harness itself got wrong: a DIB's address reused, a colour
  cache's `0xf5f5f5f5` that equals a module's address, `DrawText` leaving
  garbage, `FindResource` for the module the host's `enter` returns (its
  HMODULE), a `NULL` window block, a freed record kept readable, and an
  allocator that fails.

### Results

- `make panelsdiff-run N=3000 START=10000`: 3,000 scenarios, 0 differ (9,451
  aircraft loads, 134,184 gauges loaded, 1.17 million surfaces, 36,314 HG2D
  polygons, 419 scenarios cut short by a retail fault).
- `make panelsunit-run N=20000`: the 28 routines and the variables' lookup,
  0 differ.
- Earlier long runs showed two scenarios in about 4,000 differing in one
  row of GDI-rendered digits (one glyph's 9 bytes), which pass when run alone:
  wine's rasteriser, not PANELS.

### The breaks

`make panels-breaks [N=150 COUNT=200 SEED=1]` applies generated one-place
mutations (an offset four out, a comparison, `&&`/`||`, a sign, a shift, a
constant) to copies of `panels.c` and runs the unit mode for the routine, then
the whole-module mode. Of 200 sampled, 164 were caught, 2 did not build and
34 were missed. They are not all triaged; those looked at are:

- equivalent: an index clamp `< 0` on a value that is then 0 (`e_be10`), array
  sizes `0x300`/`0x100` of locals, zeroing a field the install already finds
  zero (`e_cbf0`, `e_a170`, `e_c300`), `var_sound`'s unused word;
- unreachable: `smooth_8`'s centre case (`i == j == 0x8000`);
- boundaries the data never hits exactly (`e_c720`'s `>=` clamps, the
  shrinkers' `>` on exact box edges);
- sampling: `e_b630` is caught at N=400 (2 scenarios in 400).

Still open as possible gaps: a flags word that reaches `surface_convert` with
bit 2 (the host palette) from the elements (`e_a6f0`, `e_ccd0`), `panels_refresh`'s
HG2D remap test, `dib_new`'s palette bytes, `e_bd80`/`e_c6a0` frame and surface
counts.
