# PANELS.DLL — the instrument panel and its gauges

PANELS (module id 15) draws the instrument panel and hosts the gauges: the
CD's 120 `.GAU` files are modules of id 19 that import only PANELS. It is
MSVC C, linked at `0x10000000`, `.text` 50,038 bytes, with 39 functions in a
Linkage of 43 entries (`+0x04`..`+0x08` init and deinit, `+0x18`..`+0xa8`).

**Status: started.** The survey below is done, and the HG2D model its
harness needs is built and checked against retail HG2D (`make
hg2dmodeldiff-run`). The harness itself and the transliteration are next.

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
