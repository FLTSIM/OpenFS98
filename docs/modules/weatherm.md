# WEATHERM (module id 27)

The weather. Each frame MAIN asks for it at the aircraft, and ATC asks for it at
an airport. WEATHERM blends GLOBAL's global weather with whichever of two
weather areas the position is deepest inside. The result goes into a working
copy that the renderer, ATC and the flight model read: cloud, temperature and
wind layers, visibility, and the temperature, pressure, wind and cloud at the
position itself. Between updates, a frame callback moves the areas with their
own wind and lets three pressures wander.

Retail is 8,774 bytes. Up to `.text+0x2a3f` it is hand-written assembly: the
stack-alignment macro around every import call, values passed between routines
in registers, 16-bit arithmetic with carries. After that it is compiled C, a
list library that nothing in the retail set calls (see *The C half*).

`src/weatherm/weatherm.c` transliterates all of it, and `make weathermdiff-run`
checks it against retail.

## Linkage

| slot | retail | who calls it | what it is |
| --- | --- | --- | --- |
| `+0x04` init | `.text+0x1020` | host | registers the frame callback on CHAIN list 3; points `.data+0xe6..0xf1` at the three pressure blocks; switches both areas off |
| `+0x08` deinit | `+0x1089` | host | unregisters it |
| `+0x18` | `+0x10b9` | ATC | `(position)`: the update at another position, then back to the aircraft |
| `+0x1c` | `+0x10e5` | MAIN | the update at the aircraft, then MAIN `+0x94` |
| `+0x20` | `+0x1d70` | the update itself | `(base, *temp, *swing)`: the time-of-day temperature swing |
| `+0x24` | `+0x1fb3` | MAIN ×2, multip | resets the blocks whose flags ask for it |
| `+0x28` | `+0x2406` | G3D | one 8-byte weather effect record from scenery |
| `+0x2c`..`+0x5c` | `+0x2a40`.. | nobody found | the C half |

All are stdcall where they take arguments. `+0x1c` and `+0x24` are plain `ret`.

## GLOBAL's weather

Inputs, which the weather dialog and scenery write:

| at | what |
| --- | --- |
| `+0xf1c` | **global block**: three cloud layers `(top, base, cover, spare)` |
| `+0xf34` | four temperature layers `(altitude, temperature)` from the top down; `+0xf44` daily swing on, `+0xf46` its amplitude, `+0xf48` surface pressure, `+0xf4a` pressure wander on |
| `+0xf4c` | three wind layers `(top, base, speed, direction, extra, spare)`; `+0xf70` surface wind `(height, speed, direction, extra)` |
| `+0xf7a`, `+0xf8c` | more surface wind, and visibility |
| `+0xf12`, `+0xf13` | reset flags for `+0x24` |
| `+0xf90`, `+0x1078` | **two areas**, 0xe8 bytes each: on/off at `+0`; two corners as 48-bit lat/lon at `+0x06`..`+0x23`; half-length `+0x24`; fade width `+0x28`; their own wind `+0x2c`/`+0x2e`; reset flags `+0x30`/`+0x32`; then centre, axis angle, half-width and weight, which the update computes; and their own weather from `+0x74`, laid out like the global block |
| `+0x1162` | cloud-thickness multiplier |
| `+0x1164` | an object handed to G3D `+0x20` and cleared each frame |

Outputs: the working copy at `+0xe8a`..`+0xf09` (visibility, clouds,
temperatures, winds, surface wind), plus:

- `+0xe8c`/`+0xe8e`: the temperature and pressure at the position.
- `+0xe90`..`+0xe98`: the wind there, as speed, direction and two components.
- `+0xf0a`..`+0xf0e`: the cloud layer the position is in.
- `+0xe6a`..`+0xe80`: the layers as the renderer wants them.
- `+0x1168`..`+0x116a`: turbulence and lift.

## The update

`.text+0x1128`, at `.data+0xf6`, the position pointer:

1. **No area on.** Both weights go to 0x4000 and the global block is copied
   straight into the working copy (`+0x2382`).
2. **Otherwise**, each area's shape is recomputed from its corners (`+0x150b`):
   - the centre;
   - the axis angle, from UTIL atan2, after scaling both components up
     together until one would overflow;
   - the half-width, by rotating a corner into the area's frame.

   Then the position is measured in each area's frame (`+0x1596`) and weighted
   from 0 (deep inside) to 0x4000 (outside) by how far past each edge it is
   over the fade width (`+0x160b`). The area with the smaller weight is blended
   with the global block by that weight (`+0x11ec`). Ties go to the first area.
3. The wind at the position (`+0x1e18`), the clouds (`+0x171d`), the
   temperature and pressure (`+0x188e`).

The blend (`+0x1348`) is word by word. It walks three cursors (area, global,
destination) that every routine hands on to the next in `esi`, `ebx` and `edi`.
`weatherm.c` makes those a `struct wx_cursor`.

The temperature is the involved part. It has the most retail behaviour worth
knowing:

- Below the lowest layer, a fixed lapse from it. Between two layers, a linear
  interpolation (`+0x2567`). Above the top layer, a fixed lapse up to
  11,000 m, then constant.
- The pressure is carried up band by band (`+0x25a8`). With a lapse it is
  `p · (T_top/T_base)^k`: the ratio goes through a log table, gets multiplied,
  then goes through an exp table. Without a lapse it is the isothermal
  `p · e^(-Δh/kT)`. The tables are at `.data+0x103`, `+0x181` and `+0x145`.
- The divisions behind those ratios are a hand-written 64/32 restoring
  division (`+0x2803`/`+0x2845`) with its step counter in `.data+0xf2`. It is
  transliterated bit by bit.

## Retail bugs, reproduced

- **An even band faults.** `+0x2567` divides
  `((altitude − base) >> 1) << 32` by `(top − base)` with `idiv`. At
  `altitude == top` with an even band, the quotient is exactly 2³¹, which
  overflows. On the way up through the layers it is called with
  `altitude == top` every time, so a position above any layer whose band is
  even faults. `+0x24`'s own defaults (layers at 180 m and 500 m) are 320 m
  apart, so after a reset, anywhere above 500 m crashes retail. A band of zero
  (two layers at one altitude) is a divide by zero. `weatherm.c` raises the
  same exceptions (`wx_fault`).
- **A fade width of 1 divides by zero** (`+0x160b` halves it first).
- **The surface wind's blend writes the wrong word.** `+0x1329` offsets both
  sources but not the destination, so its three blends all land on
  `GLOBAL+0xef8`, and the copy after them overwrites that word. The words they
  were meant for (`+0xefc`, `+0xf02`, `+0xf08`) keep whatever they held.
- **The pressure wander indexes word arrays by byte.** `+0x1c51` uses the block
  number 0-2 unscaled into five three-word arrays at `.data+0xc8`. Blocks 1
  and 2 read and write their neighbours' bytes.
- **The second cloud layer can raise the ground.** In `+0x171d`, if that layer's
  middle is above ground + 200, the ground line stays 200 higher for the
  comparisons after it.
- **Both areas are always measured.** The position is converted against each
  area's centre whether or not the area is on. A wild centre in an area that
  is off walks UTIL off its latitude table. In the game GLOBAL starts zeroed,
  so this does not happen.
- **`+0x28` type 3 turns round.** It retries a wind more than 90° off by
  adding 180°. After a sincos, the type register holds the strength's low
  byte, so a second retry asks that byte instead of the type.

## The C half

Slots `+0x2c`..`+0x5c` are a list of 0xc0-byte areas. Each area has four lists
of effect records hanging off `+0xb0`..`+0xbc`, keyed by record type. Neither
static analysis nor any module's ImportTable finds a caller. The `English\`
dialog modules import only the host and GLOBAL. Every static host `+0x298`
acquire passes the caller's own Linkage.

It almost certainly is not used. It allocates through host `+0x20c` without
the zero-fill flag (the fourth argument: host `+0x20c` zeroes the block when it
is non-zero), and then walks `next` pointers and list heads it never set. It
only works if `malloc` happens to hand back zeroes. Transliterated anyway, with
its own quirks:

- Record types are words at `+4` but compared as the dword at `+4`. Types 4, 5
  and 6 are created with a non-zero word at `+6`, so no type-filtered walk ever
  finds them, and freeing or copying by type never touches them.
- Freeing a record or area that is not in its list frees it anyway.
- Freeing by type loses the previous pointer after the first free that is not
  at the head. A second such free writes through null.

## Testing

`make weathermdiff-run` (`tools/difftest/weathermdiff.c`) drives retail and
OpenFS98 through Linkage over random scripts of:

- updates at the aircraft and elsewhere;
- frames;
- `+0x20`, `+0x24` and `+0x28` calls;
- areas switched on and off;
- GLOBAL pokes;
- C-half list operations.

Retail CHAIN and UTIL are loaded for real, UTIL behind logging forwarders, with
its rand seed reset before each side runs. G3D and MAIN are logging stubs.
Compared after each scenario:

- the transcript of every import call;
- GLOBAL;
- the arena the C half allocates from;
- WEATHERM's `.data` from `+0x98`, which both builds keep at Linkage−0x38 in
  retail's layout.

**20,000 scenarios, 0 differ, in about 3 seconds.**

**In the game**, `make smoke` passes with it installed: 27/27 modules, 95
gauges. A throwaway build that reported each entry point through
`OutputDebugStringA` showed init, one frame callback and one `+0x1c` update
running in the sim under wine. Then the sim stalls on the parked 3D-viewport
problem ([docs/wine.md](../wine.md)). The per-frame path has therefore run
once in the game, but not over time. `+0x18` (ATC), `+0x24` and `+0x28` have
not run in the game, and rest on the differential test.

The generator keeps clear of the retail faults above:

- temperature bands are odd;
- after a `+0x24`, the default layers are logged and then replaced;
- fade widths are never 1;
- latitudes stay on UTIL's table;
- the C half gets zeroed memory, valid types and no list cycles.

Several ops exist to land exactly on the boundaries that random data misses:

- a layer at exactly 11,000 m;
- an area's edge tuned so that the weight is exactly 0x2000 or the end of the
  fade;
- the aircraft snapped onto a published wind, cloud or temperature boundary;
- scenery effects exactly square to the wind;
- long frame runs, so the pressure clocks land on 0xffff.

Each of these was added because a deliberate break went uncaught without it.

**Deliberate breaks**: 71 single mutations, each run against the full 20,000.
66 were caught. Two of them, the type-3 retry with a cosine of exactly 0 and
a record copy that drops its `next`, make the module spin; the run's timeout
catches them. The other 5 cannot change any output:

- The long division's `ecx` high word: only `cx` is read.
- The `dx` that `imul`-then-shuffle leaves: it only matters when the product's
  middle word is zero with a non-zero `dh`, which no 16-bit band can produce.
- The blend's zero-takes-the-other case: no caller passes those flags.
- `+0x28` type 4 at exactly hour 12: both branches give 6.
- The free-by-type previous pointer: it only differs on the path where retail
  writes through null, which the generator keeps clear of.
