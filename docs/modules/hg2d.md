# HG2D.DLL — module id 13

In progress. Thirty-seven of forty Linkage services have implementations.
The software renderer now includes all 24 ordinary projective targets,
plain polygon plane spans, software triangle fans, affine and perspective textures, Gouraud shading,
primitives, text, blits and discs. Palette/resource loading and initialization
are implemented too. Some service branches remain unfinished below.
HG2D uses snake_case and `<stdint.h>`; there is no `types.h` dependency.

HG2D is not in `MODULES` and must not be installed yet. Device-backed palette
realization, Direct3D services and their rendering dispatch remain incomplete.
The negative-length shadow corrupt-stack return is also deferred.

The implementation and tests occupy HG2D-specific files. The shared Makefile,
README and MAIN files are untouched so MAIN can proceed independently.
For now build and verify from the repository root with:

```sh
make -f tools/hg2d.mk hg2ddiff-run N=20000              # all suites
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=palette
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=textures
make -f tools/hg2d.mk hg2ddiff-run SUITE=colours       # finite sweeps
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=drawing
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=polygons
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=fills
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=matrix_vertices
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=matrix_fills
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=projective
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=projective_keyed
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=projective_fog
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=projective_key_fog
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=projective_dither
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=projective_dither_fog
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=projective_dither_key
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=projective_dither_key_fog
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=projective_plane
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=projective_mip
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=projective_mip_fog
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=projective_mip_dither
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=projective_mip_dither_fog
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=projective_mip_build
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=resources
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=initialization
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=fans
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=textured
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=affine
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=optimized
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=gouraud
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=blits
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=perspective
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=perspective_fill
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=discs
python3 tools/hg2d_breaks.py
```

The standalone makefile builds into `bin/hg2d-diff/`. `HG2D_DIR` selects an
isolated output directory, and `HG2D_TIMEOUT` sets the run timeout in seconds
(default 7200 for the expanded combined suite). The replacement is based
at `0x30000000` for testing beside retail; this is a test build, not the final
replacement DLL. The mutation runner uses its own `bin/hg2d-breaks/` directory
and leaves both the source and the baseline DLL alone. Nothing is installed
or committed by either command.

## Binary and table boundaries

Retail image base `0x20730000`, `.text` at `0x20731000`, 0x5f266 bytes.
`.data` at `0x20792000`: 0x28cd0 mapped bytes, 0x6000 initialized.
ImportTable is at data +0, with host (1), GLOBAL (2), CHAIN (4), FE (18),
UTIL (20), G3D (14), then the zero terminator. Linkage is at data +0x38,
0xc0 bytes, ending with device-format selection at +0xbc (`0x2078b760`). It has forty
function entries and four data pointers:

| Linkage | retail address | data |
| --- | --- | --- |
| +0x8c | `0x207950a0` | 256 RGB palette triplets |
| +0x90 | `0x2079294e` | colour remap and inverse remap |
| +0x94 | `0x207954c0` | colour words; texture remapping uses their low bytes |
| +0x98 | `0x20795cc2` | realized palette triplets |

`dumplinkage.py` currently walks past the Linkage's trailing zero padding to
+0x84c and reports 532 entries. That is not the service-table boundary.
This slice checks the last service explicitly instead of trusting that extent.
The shared mapper has not been changed during concurrent MAIN work.

`tools/gen_hg2d_data.py` extracts the complete data image and its 55 relocated
pointers into itself. Linkage code pointers become zero; the loader installs
only implemented services. Other retail code pointers retain their retail
values pending the slices that own them. No executable code is copied.

## Slice 1: textures

| Linkage | retail `.text` offset | behavior |
| --- | --- | --- |
| +0x6c | +0x5ebf0 | remap 65,536 pixels in place through the palette words |
| +0xa8 | +0x5ecb0 | allocate a texture, load through FE +0x1c, remap, set flags |
| +0xac | +0x5ec50 | create from a descriptor or allocate pixels |
| +0xb0 | +0x5ed60 | free texture pixels and release hardware texture |
| +0xb4 | +0x5edc0 | mark accessed and return pixel storage |
| +0xb8 | +0x5edf0 | clear accessed, optionally remap, set update flags |

The pool is 200 records, 0x34 bytes apiece, at `0x20797f60..0x2079a800`.
The first dword alone marks a slot occupied. Allocation reuses the first free
slot without resetting its other fields. `+0xac` accepts only descriptor
kind 1, width 0x100, flags zero; descriptor +0x18 supplies pixels, otherwise
host +0x20c allocates 0x10000 bytes. The allocator's return is ignored: the
stored pointer determines success.

`+0xa8(name, selector, flags)` calls FE +0x1c with `(pixels, name, selector,
0)`. A zero FE result frees storage and returns zero. Success remaps pixels,
sets or clears record flag 0x100000 according to the first four name bytes
(`"side"`), and writes the second byte of its flags to 0xff or zero according
to the third argument. Other bits survive.

Retail details preserved:

- The handle tests check the range, not record alignment. An in-range
  unaligned handle accesses the words at that address.
- Free clears only the first four dwords. Hardware release additionally
  clears record +0x1c, zeroes eight scratch dwords at `0x2079a820`, decrements
  the count indexed by record +0x1c at `0x2079a800`, and subtracts
  `1 << (((8 - index) * 2) & 31)` from `0x207ba9ec`. The COM Release result
  is discarded. A host free, if performed, supplies the eventual return.
- Free and update return incoming eax for an out-of-range handle.
- Update first clears flag 0x10000. For an in-range handle it replaces only
  al with the flags' low byte; flag bit 1 then replaces eax with the pixels
  pointer while remapping. Either low flag bit sets 0x20000 and clears
  0x40000. The upper bytes of incoming eax survive when no remap occurs.
- Pixel access sets 0x10000 even for an empty record, then returns zero.

## Slice 2: software palettes and colour conversion

`src/hg2d/hg2d_palette.inc` adds six services and the internal palette-layout
routine, `0x20731c42`. The layout routine is exported to the harness as
`hg2d_palette_layout` until its initialization caller is implemented.

| Linkage | retail `.text` offset | behavior |
| --- | --- | --- |
| +0x5c | +0x947 | pack and realize the software palettes through host +0x290 |
| +0x60 | +0xfd3 | refresh brightness, ramps, colour remaps and shade tables |
| +0x64 | +0x161f | force refresh, marking the palette dirty even on a cache hit |
| +0x70 | +0x1320 | convert an encoded colour and pair to two palette words |
| +0x7c | +0x156d | select a colour table and return the shade-map pointer |
| +0x88 | +0x163b | remap only the high byte of a colour word |

Refresh takes its inputs from GLOBAL: brightness in the high byte of +0xcba,
mode and night overrides at +0xc46, +0x374, +0x280 and +0xb6a, view/count
selection at +0x390 and +0xcbc, and the red adjustment at +0x12a0. A cached
brightness can return early. `+0x60`, `+0x64` and `+0x7c` return that cache's
changed bit in carry; assembly entry shims preserve it. Their eax returns
are compared independently, including the upper bits on an early return.

The palette's duplicate-colour layout uses signed 16-bit comparisons and
wrapping word arithmetic. Realization packs adjacent colours by averaging
with the addition's carry preserved, constructs a grey ramp, and preserves
the output bytes it does not write. If GLOBAL +0xcbe is 2 it first restores
colours from `192 + GLOBAL[+0xcbc]` through 255. That count must be at most
64: retail's loop stops on equality and runs beyond the palette otherwise.
After packing the current and full palettes, host +0x290 receives pointers
to both. A zero device pointer at `0x207ba940` ends realization with eax zero.
The nonzero-device branch is deliberately unfinished and traps in this build.

Retail details preserved:

- Colour modes 0x69 and 0xe0 take their low byte directly; 0x67 selects the
  alternate 16-colour bank; other modes use the default bank. The no-op
  `cmp`/`test`/zero-distance branch in the default path is omitted.
- Mode 0xf0 selects a three-byte gradient record. If its third byte is zero
  and the pair is at most 0x1800, record zero replaces it. Subtraction wraps
  at a byte, multiplication uses the pair's high byte, and the low-byte
  doubling adds just its carry to the high byte. It is not a full doubling
  of the product. Conversion updates the current colour at `0x2079290c`.
- `+0x70` writes two words, in order, after completing all palette reads.
  `+0x88` writes one word and keeps incoming eax's upper half.
- A normal remap writes only the forward table. Brightness mode also writes
  the inverse table, indexed by the full palette word. Colours 0x74..0x7f
  retain their own mapping. Changing ramp mode sends CHAIN list 0x14 message
  0x70 with argument zero, after publishing the new mode.
- Shade rebuilding uses a divisor of 16 instead of 14 and toggles the
  generation byte at `0x20796034` with 0xa5. The inverse shade table likewise
  uses full-word indexing. The sky ramp's final byte wraps then becomes 255.
- Refresh's saved EBX includes bits left by the remap loop: `0x200` in
  brightness mode. Replacing that saved word with only the mode byte loses
  observable state.

The palette suite uses host/CHAIN logging stubs and retail UTIL's stateless
integer square root, without changing the PANELS model or harness. It
compares GLOBAL, the complete HG2D image except Linkage, modified output
words and guards, eax, carry for the three refresh entries, and call digests.
Scenarios cover all safe packing layouts 0..41, brightnesses 0, 1, 127, 128,
254 and 255, repeated and forced refreshes, night overrides, changed and
unchanged selectors, palette counts on both sides of 64, restoration counts
0..64, saturation, arbitrary colour words, and full-word inverse indexing.

The finite sweeps check every 16-bit input of both colour APIs, every gradient
at every interpolation fraction, both third-byte states at 0x17ff, 0x1800 and
0x1801, and all 65,536 signed/wrapping layout inputs. These are 263,680
operations, plus seven independently checked conversion and aliasing fixtures.
Realization is tested only on the safe packing layouts, not on the layout
values that fault or run out of bounds. The real host and DirectDraw device
path are outside this slice.

## Slice 3: drawing primitives and text

`src/hg2d/hg2d_draw.inc` adds all ten entries from +0x18 through +0x3c.

| Linkage | retail `.text` offset | behavior |
| --- | --- | --- |
| +0x18 | +0x57 | realize a dirty palette; otherwise preserve incoming eax |
| +0x1c | +0xde | integer point, no clipping |
| +0x20 | +0x3a9 | retail no-op, three arguments |
| +0x24 | +0x3c3 | depth-shaded point, with optional right/upper neighbours |
| +0x28 | +0x480 | packed bitmap text, optional centering |
| +0x2c | +0x10d | horizontal inclusive span, or pixel remapping |
| +0x30 | +0x240 | vertical inclusive span |
| +0x34 | +0x28b | integer line, excludes the final endpoint |
| +0x38 | +0x1f28 | 16.16 line, distance shading and fractional stepping |
| +0x3c | +0x6e | rectangular frame as four inclusive edges |

The host-owned surface is a 32-bit record: width at +4, height at +8,
signed pitch at +0xc, and a pointer to row pointers at +0x10. A primitive
looks up its initial row; vertical and general lines then advance by pitch,
not by looking up each subsequent row. The harness tests negative pitch and
nonlinear row tables to make that distinction observable. Points do not
check surface bounds; a vertical span orders its y endpoints and includes
both. A zero frame dimension skips drawing and colour selection entirely.

Horizontal spans reject y with an unsigned comparison against height. They
clamp negative **low words** of x to zero; only that clamping path can swap
the low words. There is no right-edge clipping. Reversed nonnegative x
endpoints can produce a zero or wrapped count; the short loops execute once
before decrementing it, so these inputs write past the row. Guarded canvas
fixtures compare the resulting bytes and the access violation at the guard.
These are retail behaviors, not bounds checks added by the port.

Integer lines reorder by x, select the major axis and use the original
half-length error accumulator. Nondegenerate lines omit the final endpoint;
a coincident pair draws one point. The 16.16 entry instead draws nothing for
an exactly coincident pair, but a nonzero subpixel length draws once. Its
loop decrements a signed **word** count. Negative slopes divide the bitwise
complement of their numerator before restoring the sign, not its absolute
value. The y-major path advances its initial x by the complement of the
starting y fraction. Directed fixtures land on the one-bit differences in
both computations.

Depth and distance shading use the 16-by-256 byte table at `0x20792d70`.
Depth comparisons and rescaling tests are signed words; distance rescaling
uses zero-extended words. Dithering examines only the low byte of GLOBAL
+0x127e, then conditionally writes the right pixel and the pixel above it.
The host's `0x4119a0` repairs unsigned division faults with an all-ones
quotient and remainder. The implementation computes that saturation without
trapping, and the harness repairs the four retail division sites before
resuming them. Zero thresholds, overflowing word quotients, negative scales,
and the distance cutoff 0x7d00 are checked through the resulting pixels.

Text uses an 11-byte glyph record: a width/flags byte and ten bitmap rows.
Header bit 7 shifts the bitmap down two rows; bits are drawn high bit first.
Centering measures the original byte codes and subtracts half the total
advance and six rows. Rendering then substitutes 0xe1 with 0xa6 and 0xfd with
0xbd, so the measured widths can differ from the widths actually advanced.
Codes below 0x20 wrap the glyph index as a byte, reaching beyond the usual
font records. Glyphs draw only if x and y are nonnegative, x+8 is strictly
less than width, and y+12 is strictly less than height. The depth argument
is saved but not applied to bitmap pixels. Glyph metadata and the advancing
text pointer are compared along with the pixels.

When GLOBAL +0x1298 is nonzero, a horizontal span remaps existing pixels
through the pointer at `0x20793d88`. Retail's helper at `0x207311b9` replaces
EBP with its count and leaves it zero. After a positive span completes, the
outer routine's `leave` faults reading address zero. This is verified against
retail, including an aliased map and destination: the eight-pixel path reads
both words before writing the mapped bytes. The replacement deliberately
raises the same read fault after remapping. Fault-time register contexts
are not reproduced; the harness compares fault code, read/write access,
address, modified pixels, and module/GLOBAL state. For zero counts the map
loop reaches the canvas guard before that frame fault.

`tools/difftest/hg2d_draw_suite.inc` adds a drawing suite to the owning harness.
A 64 KB shared canvas has inaccessible pages before and after it. Every
operation compares the complete canvas, including padding outside the logical
surface, plus HG2D's image and GLOBAL. A vectored handler repairs the known
retail divides and returns access faults to an assembly recovery point.
Normal returns compare eax and the callee-preserved registers, including ebp.
The point/span/frame wrappers also match ecx and the low-byte edx change from
colour selection; the two line entries match the preserved ecx/edx pair.
Caller-saved ecx/edx of +0x18, +0x24 and +0x28 are outside the comparison.

Scenarios include point writes outside logical bounds, the no-op entry,
negative word coordinates, span lengths around 4, 8, 16 and 32 at every byte
alignment, reversed vertical endpoints, all line octants and both endpoint
orders, coincident and subpixel lines, zero-sized frames, signed depth and
dither boundaries, saturated divides, all text clipping gates, wrapped glyph
codes, centering and substitutions, dirty/clean palette flushes, aliased
remapping, and both guard faults and the retail null read. Other arbitrary
32-bit coordinate overflows and malformed surface pointers are not a claim
of this suite. DirectDraw paths remain unfinished; polygon support is described below.

## Slice 4: plain polygon vertices

`src/hg2d/hg2d_polygon.inc` implements the plain path of +0x44
(`0x20732aa5`) and its 16-byte edge builder (`0x207329dc`). The four arguments
are a host record, signed x and y, and signed depth. The record's +0x1c points
to four dwords: x scale, x offset, y scale, y offset. Projection shifts each
scale by 16 with dword wrapping, multiplies signed dwords into a signed
64-bit product, divides by signed depth, and adds the shifted offset.
The entry preserves eax, ebx, ecx, edx, esi, edi and ebp.

The host repairs signed division by zero and quotient overflow to positive
or negative `INT32_MAX`, determining the sign from the dividend's high dword
and the divisor. A valid `INT32_MIN` quotient remains valid. The harness
repairs only the known projection IDIV sites and checks their instruction
bytes. The implementation calculates the same results without raising.

GLOBAL +0x2f0 starts at -1: its first increment initializes the first and
previous positions, y bounds and signed depth bounds. Subsequent vertices
increment the count and update depth bounds even when the edge buffer is
full. Retail rejects an edge when its pointer plus **0x60** reaches the limit
at data +0x884, despite a plain edge occupying only 16 bytes. Starting at
+0x104, this allows 114 edges; rejected vertices leave previous coordinates
and y bounds unchanged. The minimum-y comparison is unsigned, maximum-y is
signed, and endpoint ordering is signed.

Edges contain start row, end row, fixed-point x and slope. Equal integer
rows take the flat path, adding 0x8000 to x and retaining the full horizontal
delta. Other edges use the existing signed-complement division, clamp the
integer slope to 0x7fff, and advance x by the complement of the starting y
fraction. The original read/write order of projection scratch is retained,
including when the supplied view aliases that scratch. The dead `lea` of
record +0x80 is omitted; its destination is overwritten before use.

The conjunction of nonzero low words at GLOBAL +0xc90 and +0x2f4 selects the
matrix-derived texture edge builder, now verified in slice 13. Either
flag alone, including nonzero upper words, still selects plain edges. Polygon
close/fill +0x40 and affine textured submission +0x48 are implemented in the
following slices.

The `polygons` suite compares the complete image, GLOBAL, canvas and the seven
registers after every submission. It normalizes the known edge pointer
at GLOBAL +0xc14 in addition to the original data pointer list. The fill
suite also normalizes the dynamic intersection-sort and selected-map pointers. Directed
fixtures independently check first-vertex projection, flat and vertical
edges, valid minimum quotient, scratch aliasing and buffer capacity. Mixed
scripts cover fractional coordinates, negative slopes, wraparound, signed
depths, saturation, steep slopes and both flag widths. Deliberate mutation
checks cover projection, bounds, capacity, edge records and pointer advance.

## Slice 5: untextured polygon close and fill

`src/hg2d/hg2d_fill.inc` adds +0x40 (`0x20732697`), taking one surface
argument and preserving the same seven registers as vertex submission. It
closes the last edge to the first vertex with **unsigned** y ordering, shifts
the y bounds to scanline rows in place, selects the polygon colour from
GLOBAL +0x3c0 using the pair at +0xcc8, and applies distance shading.

There are two rasterizers. A nonzero low word at GLOBAL +0x2f4, or a vertex
count below three (the count is submitted vertices minus one), chooses the
edge-pair walker at `0x20732834`. It searches for edges beginning exactly on
the current row, replaces them at their end rows, and draws flat edges only
when encountered on the final row. Other polygons use the scanner at
`0x20732be8`: it reads count+2 records, including one beyond the closing edge,
compares row endpoints as unsigned words, updates fixed-point x with a
negative-result clamp, and collects intersections as words at data +0x888.
An odd final intersection is ignored. One or two pairs are sorted in
registers; larger sets are sorted as signed words in place. The exact final
scratch contents and the extra record's x advance are compared.

The span selector at data +0x402c retains the retail code address as state;
C dispatches on it. Supported spans are:

- Solid: inclusive horizontal drawing with the primitive's height check.
- Stippled (colour mode 0x68): checkerboard parity comes from x XOR y, without
  a height check. A one-pixel span with the opposite parity still writes the
  right neighbour.
- Shadow (mode 0x68 and a negative word at GLOBAL +0xb34): either the inverse
  remap or a cached shadow map transforms existing pixels. The cached map
  uses the negated word's high byte as a **signed** multiplier and doubles
  the word product before indexing the ramp. Its cache ignores changes to
  the source table when the generation byte is unchanged. The span's tail
  leaves pixels unmapped; an odd-address singleton instead maps the same
  pixel twice. Both behaviours are retained.
- Palette remap: GLOBAL +0x1298 chooses a primary or alternate 256-byte map,
  with signed comparisons and the original fallback bank. This forces the
  polygon colour to 0x6900. The pixel mapper is shared with primitives, but
  polygon callers save and restore EBP, so their remapping completes without
  the primitive entry's null-read fault.

The `fills` suite submits vertices and closes through the public entries,
then compares the full image, GLOBAL, canvas and all seven registers. It
covers both winding orders, triangles, convex and concave shapes, crossings,
six simultaneous intersection pairs, repeated points, flat polygons,
singletons, subpixel projections, negative x and y, height rejection, both
shadow maps, cache hits with changed source bytes, signed factor 0x80, and
remap selectors -1, 1, 15, 16, 17, 19, 20 and 31. A rectangle fixture checks
every canvas byte independently, including untouched padding and rows. Row
tables retain negative pitch and nonlinear addressing from the drawing suite.

The joint nonzero +0xc90/+0x2f4 path now supports matrix edges and far-depth
fills (slice 13) and basic projective spans (slice 14). Other projective spans
and the separate +0xc90-only path remain deferred and trap when entered. The
shadow span's corrupt-stack return for a negative low-word length also
remains deferred and traps explicitly; these ordered, bounded fixtures do
not reach it. Closing an exhausted vertex buffer, arbitrary invalid row
pointers, and polygons that produce unbounded scanline loops are outside
this verification. This slice does not install HG2D in-game.

## Slice 6: affine textured vertex submission

`src/hg2d/hg2d_textured.inc` adds +0x48 (`0x20787a2c`): six arguments,
record, x, y, signed depth and two texture attributes. A zero **low word** at
GLOBAL +0xcb6 delegates to the existing plain +0x44 path and preserves the
incoming registers. Otherwise it projects with the same signed division and
scratch read/write ordering, increments the vertex count, and keeps unsigned
minimum and maximum y bounds. It does not update the depth bounds. The first
vertex stores both first/previous positions and attributes; later vertices
order endpoints with an unsigned y comparison and exchange the attributes
alongside their coordinates. The fifth argument becomes the edge's second
attribute at +0x14; the sixth becomes its first at +0x10.

The edge builder at `0x20785780` appends 32-byte records: integer start/end
rows, fixed x, x slope, two attributes and their slopes. Flat edges retain
the exact x and raw attribute deltas, omitting the plain edge's half-pixel
rounding. Nonflat edges independently divide the three deltas with the same
signed-complement arithmetic and 0x7fff integer clamp, then advance all three
values by the complement of the starting y fraction. No additional clamp is
applied to the attributes. Retail's apparent capacity check branches to the
next instruction; that dead cmp/jb pair is omitted. The textured path keeps
appending after the plain path's reservation threshold.

This entry exposes EAX, ECX and EDX as outputs while preserving EBX, ESI, EDI
and EBP. The entry shim passes incoming volatile registers through local
storage to the C body, then restores the calculated outputs:

| path | EAX | ECX | EDX |
| --- | --- | --- | --- |
| plain fallback | incoming | incoming | incoming |
| first affine vertex | projected y | y offset shifted by 16 | signed y-division remainder |
| flat edge | GLOBAL pointer | low x shifted arithmetically by 16 | low y shifted arithmetically by 16 |
| nonflat edge | GLOBAL pointer | second attribute slope | second attribute's fractional advance |

The shared projection helper now optionally returns the remainder: a valid
signed divide keeps the dividend's remainder sign; the host's overflow/zero
repair sets EDX to 0x7fffffff independently of the saturated quotient's sign.
The harness repairs the two extra retail IDIV sites and checks instruction
bytes. Returns remain literal scalars, including projected coordinates whose
numeric values could fall in a module's address range.

The `textured` suite compares the entire image, GLOBAL, unchanged guarded
canvas, and all seven registers after each public call. Directed fixtures
check fallback word masking, attribute order, exact flat and vertical edge
records, quotient/remainder boundaries, aliased projection scratch and the
unchanged depth bounds. Random scripts mix wrapped coordinates and attributes,
negative depths, signed quotient saturation and steep slopes. One in sixteen
scenarios submits 61 vertices, writing 60 edges up to data +0x884, to prove
the capacity branch has no effect without overwriting the adjacent scratch.
Thirty-five new mutations cover these paths and the returned registers.

Textured close/fill +0x4c is implemented in the next slice. The matrix attribute
builder selected by +0x44 when GLOBAL +0xc90 and +0x2f4 are both nonzero is
implemented in slice 13; +0x48's plain fallback reaches that same builder. Writes beyond the actual
60-edge storage, invalid view pointers and exception register contexts remain
outside this slice. No in-game installation is made.

## Slice 7: affine textured close and spans

`src/hg2d/hg2d_affine.inc` installs +0x4c (`0x20787bbe`). It closes a
32-byte edge with unsigned y ordering, logically shifts the y bounds, and
walks pairs of active edges. Flat edges draw only on the final row, using
synthetic records at data +0x4638/+0x4658. Edges become inactive by replacing
their start row with -1. The texture record comes from GLOBAL +0xc8c.

The software selector combines distance shading, a raw-texel key threshold,
GLOBAL +0xcce dithering, and the quality word at +0xc92. Keyed spans and
low-quality spans sample once per two pixels; other spans sample every pixel.
The key comparison precedes shade mapping. The two coordinates occupy the
high bytes of separate accumulators, and wrapping arithmetic is retained.
Dither phases depend on row parity and alignment. Widths above 1,500 return
without drawing in this backend. Its divide is unusual: CDQ runs **before**
SHL EAX,7, so the original sign remains in the high dividend dword. The
signed quotient is doubled after dividing. Host saturation, partial EAX
writes on skipped keys, batched texture reads, tail counters, and scratch
words +0x4618..+0x4634 are reproduced.

Backend bit 16 at data +0x4678 selects the optimized spans. All sixteen upper
selectors are implemented as C, including both dithered versions. Retail's
setup routines patch texture and shade addresses into instructions; C reads
the same bases directly. Code-pointer selector words retain their retail
numeric values for image comparison. The harness makes retail `.text`
writable for this suite, modeling the initialization step required by its
self-modifying code. The replacement does not copy executable retail code.

The optimized backend divides true signed deltas by inclusive pixel count,
keeps the second coordinate unshifted, and has no software width cap. Plain
spans add packed coordinate fields; keyed spans OR them. Four samples are
read before their four destination bytes are written, including when texture
storage aliases the canvas. The shaded-key path increments the destination
**before** writing and therefore writes one pixel to the right. That bug is
preserved. The original saved-stack value at data +0x5e04 is reproduced from
the entry shim's register storage and the regular/flat call depth; it is not
masked out of comparison. The block-loop residue at +0x5e08 is also compared.

Distance bucket 15 bypasses texturing and fills with data +0x1c7a. Its remap
path uses the shared raw mapper and restores EBP, avoiding the primitive
entry's null read. A zero low word at GLOBAL +0xcb6 delegates to plain fill,
but retail's fallback epilogue rotates the saved registers: EBX gets incoming
ESI, ESI gets incoming EDI, and EDI gets the entry frame pointer. The assembly
shim reproduces that rotation. All seven returned registers are compared.

The `affine` suite checks all sixteen software selectors, winding orders,
alignment and width boundaries, triangles, crossings, flat/singleton shapes,
large attributes and repaired divides, distance fallback and its remap path.
`optimized` adds all sixteen upper selectors, random fractional coordinates,
negative deltas, packed-coordinate overlap, destination aliasing, setup-only
returns and the saved-stack state. Both include independently checked constant
texture rectangles over the entire canvas. Edge-stride fixtures deliberately
put plausible records halfway through real records so an incorrect stride
cannot pass merely because it finds no edge.

## Slice 8: Gouraud vertices and fill

`src/hg2d/hg2d_gouraud.inc` implements +0x74 (`0x20785375`, five arguments)
and +0x78 (`0x20785472`, one surface argument). Vertex shades use only the low
word, stored at unaligned data words +0x90d/+0x90f. Edges occupy 24 bytes:
row endpoints, x and slope, then shade shifted by eight and its slope. Flat
edges add half a pixel to x. Like textured edges, the apparent capacity
comparison has no effect; bounded fixtures reach the actual end of storage.

The closer walks edge pairs, including synthetic flat endpoints and signed
x ordering on the final row. It exposes the bucket-shaded colour in EAX
without replacing the current-colour byte. Span interpolation subtracts word
shades, shifts the word delta by sixteen, divides signed, then doubles the
quotient. Two row-dependent phases use the original tables at +0x45e4/+0x45f4.
An odd-address prefix uses the second phase; four-pixel blocks store the
sample pairs in their original reversed order. Word-count wrap, divide repair,
partial EAX writes and the final ECX/EDX state are compared.

`gouraud` covers independent edge records, shade-word masking, projection,
capacity, winding and flat shapes, random shade words, stride traps and an
independently checked constant-shade rectangle. It compares all seven
registers, the canvas, GLOBAL and the complete image.

## Slice 9: scanline blits and teardown

`src/hg2d/hg2d_blit.inc` installs +0x80 (`0x2078aff0`), nine arguments:
record, packed row/mode, x endpoints, two starting texture coordinates,
two ending coordinates, and texture pixels. The host record differs from
primitive surfaces: height is at +0x88 and its row table at +0x90. Only the
row/mode low word survives; bits 0..11 select the row and bits 12..15 the mode.
Height rejection uses a signed dword comparison. Coordinates use their low
words; both signed texture steps are calculated before dispatch, including
no-op modes and zero/reversed spans. Zero denominators resume with the host's
signed saturation.

Modes 0, 1, 2, 4 and 8 are implemented: mapped texture, constant fill, two
coordinate-threshold bands, and raw-zero transparency followed by remapping.
Other modes return without drawing after computing the steps. Mode 1 indexes
the byte remap with the **full word** at data +0x34ca. Mode 0 preserves the
four-sample read-before-write ordering. The packed coordinate update uses
addition, retaining carries between its two halves. ECX returns the original
row/mode low word because retail overwrites its saved stack slot. Partial EAX
and mode-specific EDX leftovers are compared on every branch.

The `blits` suite exercises all sixteen modes, four destination alignments,
block/tail widths, row-mask boundaries, signed heights, reversed/zero spans,
negative steps, threshold equality, full-word mapping and canvas/texture
aliasing. A constant-fill fixture independently checks every canvas byte.

Linkage +0x08 (`0x2073104a`) is a register-preserving no-op. Its assembly shim
and lifecycle fixture preserve and compare all seven registers and all state.
The fixture runs before every selected suite, including `SUITE=lifecycle`.

## Slice 10: perspective textured vertices

`src/hg2d/hg2d_perspective.inc` adds +0x50 (`0x20788680`), seven arguments.
Projection, fallback, unsigned bounds/order, and the first two attributes
follow +0x48. The seventh argument supplies a third interpolated attribute,
kept at data +0x934/+0x938 for first/previous vertices. Edges are 40 bytes;
the third base and slope occupy +0x20/+0x24. All four slopes use the same
signed-complement arithmetic and fractional advance. The capacity branch is
again a zero-distance branch and is omitted.

First submission returns the third attribute in ECX and projected y in EAX;
nonflat edges return their third slope in ECX and third fractional advance
in EDX. Flat edges retain the +0x48 register behavior. The `perspective` suite
checks independent records, all attributes, projection/saturation/remainders,
scratch aliasing, signed/unsigned boundaries and 48 edges up to data +0x884.
The corresponding +0x54 closer and the third attribute's shading behavior are
verified in slice 12. The existing perspective entry names were provisional;
this path interpolates UVs affinely and applies the third attribute as shading.

## Slice 11: table-shaped discs

`src/hg2d/hg2d_disc.inc` installs +0x84 (`0x20787ce0`), four arguments:
record, profile selector, center x, center y. Its surface is embedded at
record +0x80 and its view pointer at +0x1c. The 48 profile pointers and profile
bytes are extracted from retail with the existing data generator, not retyped.
Selectors use only the low word. Values at least 48 halve the selector, clamp
it to 47, and double profile widths and rows. The repeated-row pointer steps
retain retail's mixture of word SI and full ESI arithmetic.

Clipping bounds wrap as words. The two vertical bounds are twice offset plus
or minus y scale, so a conventional on-screen interval uses a negative y
scale. Rows are compared signed before the span helper zero-extends and
halves the row word. The forward half adds one to each width; the reverse
half does not. Solid spans check surface height; checkerboard spans do not,
and retain the singleton right-neighbour write. The colour helper converts
mode 0x68 to direct colour plus a separate stipple flag. Pixel remapping saves
and restores the frame pointer and exposes the mapper's EAX/ECX/EDX leftovers.
The shared mapper now optionally records those values while preserving its
original load/store order; primitive callers still reproduce their null read.

`discs` covers all profiles, doubled and clamped selectors, upper argument
words, signed clipping, reversed vertical intervals, zero-width clips,
solid/stipple/gradient colours, height equality and remapping, plus a wide
viewport that distinguishes selectors 46/47 and a fully negative x interval. An independent
profile-zero fixture checks each affected and unaffected canvas byte. Inputs
that allow negative rows through a stippled clip interval can read an invalid
row pointer; those malformed intervals are outside these bounded fixtures.

## Slice 12: three-attribute textured close and fill

`src/hg2d/hg2d_perspective_fill.inc` implements +0x54 (`0x20788851`), one
surface argument. It closes a 40-byte edge through the existing +0x50 edge
builder, exchanges all three attributes on unsigned y reversal, logically
shifts the y bounds, and computes distance shading. Its pair walker and two
edge searches follow `0x20788b5b`/`0x20788c09`/`0x20788c99`. They advance x,
both UVs and the third attribute. Flat edges draw on the final row through
synthetic records at data +0x4d74/+0x4d9c. The span selector at +0x4d70 keeps
its retail numeric address (`0x20788d29`); C calls the implementation directly.

The third attribute is a **shade coordinate**, not a perspective denominator.
UV steps use the software affine path's CDQ-before-SHL signed dividend and
host saturation. The shade step divides a true signed dword delta, doubles
it, then clears its low word. Clearing after division/doubling retains the
original negative rounding behavior. Two shade accumulators subtract
0x06000000 and use the extracted row tables at +0x4de0/+0x4df0. Each phase
advances before its lookup. AL/AH are replaced by the sampled and shaded
bytes inside EAX, preserving the other phase bits exactly.

The raw texel and phase bits select a byte at +0x4e00; the resulting byte
then selects the distance row at +0xd70. Normal initialization builds sixteen
256-byte rows, but the unchecked five-bit phase can address thirty-two rows.
The implementation retains those reads into adjacent module state. Dithering
uses the **low word** of GLOBAL +0xcce, unlike +0x4c's byte selector. Even rows
use UV biases 0x400000/0xc00000 and odd rows 0/0x800000. An odd-address prefix
uses the second shade phase and a single-pixel tail uses the first. Each pair
reads both texels and both shade lookups before writing the two bytes. The
8-pixel blocks retain their pointer update order and signed count residue.
There is no 1,500-pixel width rejection, raw-texel key, low-quality doubling,
or existing-pixel remap in this path.

The closer's zero-low-word +0xcb6 fallback has the same saved-register
rotation as +0x4c. Active returns expose EAX's partial phase/pixel value,
ECX's first UV accumulator and EDX's first UV step; the regular walker then
replaces EAX with the right edge's third slope. If no edge starts on the
chosen row, setup returns the texture pointer in EAX and the edge-buffer end
pointer in ECX. The fixture explicitly marks that latter ECX as a pointer
for module-relative comparison and checks it against GLOBAL +0xc14. Numeric
UV results remain literal scalars.

The `perspective_fill` suite exercises public submission and closing, both
windings, all row phases and destination alignments, widths around block/tail
boundaries, crossings, concave polygons, flat/vertical/single-point shapes,
fractional projection, random wrapped attributes, distance rows including 15,
zero threshold repair, and UV quotient overflow. It includes an independently
checked constant lookup rectangle, 1,501-pixel spans, and texture/canvas aliasing
one pixel apart to expose read-before-write order. A deliberately plausible
half-record catches an incorrect 20-byte edge stride. An independent sweep
over 1,024 scenarios checks every combination of 32 phase rows and 256 texels.

For the wrapped-attribute fixtures the entire 8 KB lookup window is filled
with deterministic bytes. This intentionally replaces two normally relocated
words at +0x5ee0/+0x5ee4. The harness compares those words literally as lookup
bytes during this suite, rather than subtracting a module base from them.
They remain part of the full image comparison. The ordinary pointer
normalization resumes for the other suites. These vectors verify the
unchecked lookup arithmetic without depending on different DLL pointer bytes.
They do not establish behavior for arbitrary invalid surface or view pointers,
negative rows, or unbounded/malformed scanline walks.

## Slice 13: matrix-derived texture edges, setup and far fill

`src/hg2d/hg2d_matrix.inc` implements the alternate +0x44 edge builder at
`0x20733386`, setup at `0x20733130`, and the far-depth +0x40 fill. This adds
branches to the existing services; the Linkage implementation count stays 33.
Both GLOBAL +0xc90 and +0x2f4 must have nonzero low words for matrix edges.
Projection, first-vertex state, depth bounds and the existing 0x60-byte
reservation check remain in the owning +0x44 body. Forty-byte edges allow
46 submissions before that reservation check rejects further edges. Rejected
vertices still update count/depth bounds, as for plain vertices.

Edges contain row endpoints, x and x slope, three matrix attributes, then
three attribute slopes. Both endpoints are normalized from screen coordinates
by subtracting the shifted view offset and dividing by twice the unshifted
view scale, with dword wrapping, signed division repair and a signed-word
clamp. Signed-word x/y and signed-word coefficients at GLOBAL +0xcfa/+0xd00
form three dot products. Their sum wraps to a dword **before** arithmetic
shifting by +0xd42 masked to five bits; dword offsets at +0xd0c are added last.
The low/high normalized positions and high attributes remain in their retail
scratch fields at +0x4168..+0x418c.

Nonflat x slopes keep the existing signed-complement division. Attribute
slopes omit the final sign subtraction after XOR, which changes negative
rounding by one bit. An independent case yields a slope of -1 where adding
that subtraction would produce zero. Fractional advance uses the complement
of the initial y fraction. Flat x omits half-pixel rounding; flat attributes
retain raw deltas. Retail also calculates three clamped slopes for a flat
edge with denominator 1 before overwriting them; those stores and final raw
deltas are reproduced. No observable scratch survives from the overwritten
slope calculations.

Setup computes three fixed-point scale ratios and copies three GLOBAL matrix
rows into data +0x41c8..+0x41ec. Products are reduced by shifting both dividend
and divisor, retaining the original truncation and unsigned divide repair.
Fog scaling uses the absolute signed word at +0x30c and unsigned +0xcd4, with
paired reduction of two 64-bit quantities. The helper at `0x20787fb9` returns
only zero/INT32_MAX or carry plus INT32_MAX on this entry path; its multiply
block has no incoming control-flow path from this call. Consequently the
far shortcut is selected when the signed minimum depth is INT32_MAX and the
normal fog gates admit the depth range. The implementation omits that dead
multiply block and reproduces the actual gates.

The 64 setup indices combine signed mode, matrix-row ordering, fog scaling,
raw-texel keying, hardware eligibility, and word-sized dithering. Their table
at +0x4204 contains 24 distinct span targets. C retains the selected retail
numeric code address at +0x4200. All 64 selections are checked through +0x40
with an empty starting row, so setup returns before invoking the unfinished
span bodies. Hardware eligibility is checked using already-uploaded textures
or the keyed bypass; actual upload/failure fallback remains deferred and
traps explicitly. The normal texture pointer is published at +0x948. The far
shortcut returns earlier, retaining the previous texture/key scratch; its
fixtures supply a null texture handle to verify that it is never dereferenced.

The far target (`0x20784d41`) fills with data +0x1d06. Its walker advances
40-byte edges and all three attributes, using synthetic final-row records at
+0x4304/+0x432c. It sorts fixed x as unsigned, shifts x arithmetically, and
uses the primitive's word clipping and height rejection. Existing-pixel
remapping uses the map selected by +0x40's original preprocessor and restores
the frame pointer. The public +0x44/+0x40 shims still preserve all seven
registers; no new return normalization is required.

`matrix_vertices` checks independent flat records, the negative-slope bit,
wrapping dot-product addition, integer slope saturation, random full-width
projection/transform inputs, shift counts 0/1/13/31/32/33/63/255, normalization
repairs, and capacity/rejected submissions. `matrix_fills` checks all selector
indices, signed/word flag boundaries, zero/overflow divides, the maximum-depth
boundary, retained far scratch, an independent whole-canvas rectangle, both
windings, flat/interior records, remapping, negative-x clipping and height
rejection. A plausible half-record makes an incorrect 20-byte search visible.
A midrange scale exercises reduction with a nonzero quotient; extreme scales
alone hid that mutation by making both calculations round to zero.

The ordinary 24 projective span bodies are **not** implemented in this slice.
They trap if actual drawing reaches them. GLOBAL +0xc90 with a zero +0x2f4
still uses the deferred plane-span path. This slice verifies matrix-edge
production, setup selection and far fill; it does not claim near-texture or
device rendering equivalence. Arbitrary malformed views/surfaces and negative
scanline intervals remain outside these bounded raster fixtures.

## Slice 14: basic software projective spans

`src/hg2d/hg2d_projective.inc` implements targets `0x20733bbb` and
`0x2073751e` through the existing public +0x44/+0x40 matrix path. Selectors
0/3 draw right to left; 1/2 draw left to right. These are raw texture spans
with two destination pixels per texture sample. The service count remains 33.
The remaining 22 ordinary span targets still trap explicitly.

Endpoints clear bit 16 while retaining the fractional x bits, sort as unsigned
fixed-point values, and select attributes according to drawing direction.
Projection divides the signed matrix-row difference by the middle attribute
shifted arithmetically by eight and ORed with one. Signed 64-bit products
produce U/V, then add/subtract the copied matrix bases and clamp with ordered
signed lower/upper comparisons. A reversed pair of bounds therefore retains
retail's lower-bound-first behavior. Valid INT32_MIN quotients and the host's
overflow saturation are distinct cases.

The ratio of the absolute endpoint factors selects 16-sample correction blocks
strictly below 1/16 and 32-sample blocks otherwise. Each full block advances all
three homogeneous attributes, projects again, and uses arithmetic shifts for
its U/V increments. A partial tail instead uses signed division. Integer U/V
bytes advance independently using their fractional carries; they do not carry
between texture-index bytes. Scratch fields, including shifted step words,
block counters, and the last tail jump address, retain their retail values.
The four 32-entry tail tables are extracted from data embedded in retail's
`.text` section by `gen_hg2d_data.py`; no executable bytes are copied.

Full blocks align their four-byte stores with a direction-dependent prefix.
Each pair of samples is read before either doubled pixel is stored. Tails
store one doubled sample at a time. This ordering matters when the texture
aliases the canvas. These spans ignore logical surface height and the
existing-pixel remap flag, and may cover one extra pixel at an even right
endpoint. The +0x40 shim captures its entry stack pointer to reproduce the
visible saved ESP at +0x4164, including the extra call frame for flat edges;
the harness compares that value literally and still checks all seven registers.

The `projective` suite exercises all four selectors, both windings, all tail
lengths, both correction sizes and the exact threshold, fractional endpoints,
flat/point/crossing geometry, signed factors, random matrix coefficients,
clamp order, division repairs, aliasing and independent whole-canvas fills.
Fog, raw keying, dithering and hardware rendering remain outside this slice.

## Slice 15: raw keyed projective spans

The same projective implementation now also dispatches `0x2073ae7b` and
`0x2073e1ec`. Selectors 8/11 draw right to left and 9/10 draw left to right;
24..27 select the same software targets even when hardware eligibility is set.
The existing keyed setup bypasses upload, including for a texture without the
hardware-resident flag. Four of the 24 ordinary projective targets are now
implemented. The Linkage service count remains 33.

A raw unsigned texel below BYTE +0x435c leaves both destination pixels alone.
Equality draws. Coordinates and destination position advance for skipped
texels too. Keyed full blocks and tails read one texel, conditionally store its
doubled pair, then read the next texel. They have no alignment prefix or paired
sample prefetch. Canvas/texture aliasing therefore exposes a different order
from the basic full blocks. Both paths retain the same projection, clamp,
16/32-sample correction, saved stack and scratch semantics. Four additional
tail-address tables are extracted from retail data embedded in `.text`.

`projective_keyed` reuses the projection, geometry, signed division, clamp,
threshold and alias fixtures from `projective`, then independently checks
whole-canvas fills for raw texels immediately below, equal to and above keys
1/128/255 and a scenario-dependent key in 1..255. Key 255 also checks wrapped
zero texels. Both directions cover full blocks and every tail length. Seeded
nonzero bytes above the key verify byte-sized reads/stores, and alternate
scenarios exercise the hardware-eligible selector aliases without uploading.
Per-scenario snapshots grow from 256 to 512 operations for the expanded
337-operation script; all image, GLOBAL, canvas and register comparisons remain.
Fog, dithering and actual device rendering are still deferred.

## Slice 16: unkeyed fogged projective spans

Targets `0x207352a4` and `0x20738c03` now render through the existing matrix
+0x44/+0x40 path. Selectors 4/7 draw right to left and 5/6 draw left to right.
Six of the 24 ordinary projective targets are implemented; 18 remain deferred.
The Linkage service count stays 33.

Fog level uses the signed factor's complement magnitude, omitting the +1 of
true absolute value for a negative factor. An unsigned product with +0x41f8
is shifted right 16 and truncated to a dword before clamping to 0xfff. Start,
old-block and final levels remain at +0x41bc/+0x41c0/+0x41c4. Matching endpoint
shade rows shortcut before homogeneous stepping: row zero uses the basic
texture span, retaining the fog endpoint scratch; row 15 uses the existing
solid far fill with original endpoint coverage, surface-height rejection and
optional existing-pixel remapping. The selected numeric fog target stays in
+0x4200. The far shortcut retains the previous saved ESP and UV steps.

Intermediate spans project again per correction block and interpolate shade
alongside U/V. Full blocks use arithmetic shifts, then replace steps -1 and
-2 with zero using an unsigned comparison. Tails use signed division and keep
those small negative steps. Each sample indexes the 16-row map at +0xd70,
advances shade, masks only the upper nibble of its row byte, and stores a
doubled pair before fetching the next sample. This differs from the basic
renderer's pair prefetch and is visible when canvas and texture alias. The
four fog tail-address tables are extracted from retail `.text` data.

The `projective_fog` suite reuses geometry, projection, clipping, signed repair
and alias cases with an identity map, then varies shade maps, fog strengths,
factor signs and middle attributes. Independent whole-canvas checks cover
levels just below/at row 1 and row 15, both factor signs, zero level, clamp,
height rejection and remapping. Two exact gradient cases independently check
the -1/-2 suppression. A destination row aliases the visible shade step at
+0x4358 so pixel stores change subsequent increments and expose the row-byte
mask. Both directions, both correction sizes and all tail residues are
compared, including literal scratch and tail addresses.
Keyed fog, dithering and actual device rendering remain deferred.

## Slice 17: keyed fogged projective spans

Targets `0x2073c402` and `0x2073f771` now render through +0x44/+0x40.
Selectors 12/15 draw right to left and 13/14 draw left to right; 28..31 use
the same keyed software paths while bypassing hardware upload. Eight of the
24 ordinary projective targets are implemented, with 16 deferred. The
Linkage service count remains 33.

The raw unsigned texel is tested against BYTE +0x435c before shade lookup.
Equality draws, including when the mapped result is zero. A rejected raw
texel cannot become opaque because the map would produce a bright value.
U/V, shade and destination position advance even for rejected texels. Accepted
samples store their doubled pair before reading the fractional steps and fog
increment for the next sample. This ordering is visible when a destination
aliases the U step, key or shade increment. The unkeyed fog loop advances
before its store, so the two paths retain distinct order.

The fog projection, levels, correction sizes and -1/-2 block suppression are
shared with the preceding slice. Matching zero shade rows jump into the raw
keyed renderer; matching maximum rows jump into the solid far fill and bypass
the key entirely, retaining ordinary far coverage, height rejection and
existing-pixel remapping. Four additional numeric tail tables are extracted
from data embedded in retail `.text`.

A new alias into the high half of the shifted U step exposed a bug in the
previous raw keyed implementation: its helper advanced before storing. The
raw keyed loop now stores first, matching retail. The old canvas/texture alias
had verified inter-sample fetch/store order but could not expose the timing
of reads from the separate step scratch. This slice adds a directed raw-keyed
regression fixture and a mutation that reverses that corrected order.

`projective_key_fog` compares two phases per scenario within the existing
512-operation snapshot capacity. Phase one reuses the complete 506-operation
fog suite with keying enabled. Phase two independently checks keys 1/128/255
and a scenario-dependent key, values below/equal/above each key, zero/middle/
maximum fog, mapped zero versus bright rejected texels, height/remap behavior,
mixed skipped/accepted gradients, and destination aliases into the U step and
key for both the fogged and zero-row fallback paths. It totals 679 operations
per scenario. The expanded raw `projective_keyed` script now has 340 operations.
Dithering, actual device rendering and the remaining plane/resource/init paths
are still deferred.

## Slice 18: unkeyed dithered projective spans

`src/hg2d/hg2d_dither.inc` implements pixel sampling for `0x20749d95` and
`0x207541ee`, selected through the shared matrix +0x44/+0x40 path. Selectors
32/35 draw right to left and 33/34 draw left to right. Ten of the 24 ordinary
projective targets are implemented; 14 remain deferred. The Linkage service
count stays 33.

Dithered spans retain unmasked fixed-point endpoints and sample once per
pixel. Both U and V receive a fractional bias based on row parity and the
physical destination address: odd rows use 0/0.5 texel at even/odd addresses;
even rows use 0.25/0.75. Each coordinate retains its own wrapped integer byte
and fractional carry. Full blocks align paired stores with a direction-specific
single-pixel prefix, then fetch and advance two samples before storing the
pair. Prefixes, final single pixels and tails store before advancing. These
orders are compared with texture/canvas and destination/step aliases.

The initial endpoint complement magnitudes are exposed at +0x4365/+0x4361.
Unlike the correction ratio's true absolute values, negative factors omit the
final +1. If **both** magnitudes are at least +0x436d, retail enters the old
doubled-pixel span after clearing endpoint bit 16, retaining the already
projected attributes and UVs. It does not sort again after masking. Fractional
endpoints can therefore yield a zero sample count, repaired homogeneous divides
and no drawing. Exact threshold and negative-factor cases check that behavior.

Dither tails expose the starting address parity at +0x45d1, a final-pixel skip
flag at +0x45d5 and the padded jump count at +0x45d9. The jump count can be 32,
so all eight direction/block-size/row tables contain 33 numeric entries,
extracted from retail `.text` data. Tails update +0x4160 even without a full
block. The final skip flag is read after earlier stores; an alias into that
flag makes caching it incorrect. The odd-row parity comparison branches to
the next instruction and has no effect; that dead branch is omitted. Even-row
tails apply their initial quarter bias, preserving the same four phases.

The two-phase `projective_dither` script has 664 operations per scenario. It
reuses the projection/geometry cases, then checks whole canvases independently
for four selectors, both row parities, four alignments and four U/V fractional
positions. Counts cover both correction sizes, full blocks and all tails.
Additional cases check exact fallback bounds, fractional mask reversal,
destination aliases into a fractional step and the live tail flag, and a
canvas texture alias that distinguishes pair prefetch from immediate stores.
Dithered fog/keyed variants, hardware, plane, resource and init paths remain
deferred.

## Slice 19: unkeyed dithered fog spans

Targets `0x2074e49d` and `0x207588f0` now render through +0x44/+0x40.
Selectors 36/39 draw right to left and 37/38 draw left to right. Twelve of the
24 ordinary projective targets are implemented. The Linkage service count
remains 33 of 40, with unfinished branches still listed below.

The new path combines the dither renderer's one-pixel coverage and four U/V
biases with the fog renderer's complement magnitude, 12-bit level, correction
blocks and -1/-2 step suppression. It uses individual byte stores throughout:
each texel is shaded and stored before reading the fractional U/V steps and
fog increment for the next sample. Plain dither instead prefetches paired
samples before its word store. Destination/step and canvas/texture aliases
make that distinction observable. Mapped zero remains opaque.

Shortcut order matters. If both endpoint complement magnitudes meet the
distance threshold, retail first masks the endpoints and enters the existing
doubled-pixel fog path. Otherwise matching zero shade rows use plain dither,
and matching maximum rows use the solid far primitive. Fog level and endpoint
magnitude scratch remain literal across these jumps. The far shortcut keeps
its height rejection, original integer endpoint coverage and optional remap.
It can therefore draw where the masked doubled-pixel count would be zero.

Dither tails retain the padded count, starting parity, live final-skip flag
and saved count from the preceding slice, with an additional signed shade
step. Eight new 33-entry numeric tail tables are extracted from retail `.text`
data. Shade advances and masks its row byte after the store, including when
the destination aliases its increment. There is no separate dither bias on
the shade index: the four phases apply to U/V sampling.

The 761-operation `projective_dither_fog` script has two phases within the
existing 512-operation snapshot capacity. It reuses the fog geometry, repair,
clamp, gradient and alias fixtures, then independently checks shaded whole
canvases for four selectors, both row parities, two physical alignments and
four U/V fractions. Additional cases cover exact signed distance thresholds,
all three shortcuts, fractional endpoint masking, and aliases into the U step,
shade increment, tail flag and canvas texture. Expected coverage distinguishes
single-pixel sampling, doubled fog and solid far fill.
Keyed dither variants, hardware, plane, resource and initialization paths remain
deferred. These service/target counts are not a whole-module completion
percentage, and HG2D remains outside `MODULES`.

## Slice 20: keyed dithered projective spans

Targets `0x2075e63f` and `0x20766058` now render through +0x44/+0x40.
Selectors 40/43 draw right to left and 41/42 draw left to right; 56..59 use
the same keyed software targets even with hardware eligibility. Fixtures clear
the texture's resident flag to check that the keyed upload bypass still holds.
Fourteen of the 24 ordinary projective targets are implemented; the service
count remains 33 of 40. The last software projective pair is keyed dithered fog.

The raw unsigned texel is compared with BYTE +0x435c. Equality draws one byte;
rejected texels retain the destination but still advance U/V and destination
position. Every accepted store precedes the fractional-step reads for the next
sample. There is no pair prefetch or alignment prefix. Tests cover accepted/
rejected mixtures and live key changes caused by destination stores.

Retail's full 16/32-sample blocks use **no dither bias**, for either row parity.
Only the tails apply the usual 0/0.5 and 0.25/0.75 U/V phases. Independent
whole-canvas fixtures distinguish full blocks from their tails and preserve
that discontinuity. The existing complement-magnitude distance gate still
selects the doubled-pixel raw keyed renderer when both endpoints meet the
threshold. Keying remains enabled across that jump; masked fractional endpoints
retain the preceding slice's zero-count behavior.

Tails retain starting parity, padded count and the live final-skip flag, with
eight new 33-entry numeric tables extracted from retail `.text` data. A pixel
store can change the key or final-skip flag before a later sample. Caching
either changes the output. Captured integer U/V step bytes stay separate from
the fractional words that stores may overwrite.

The two-phase `projective_dither_key` script has 830 operations per scenario.
It reuses the raw-keyed geometry and key-boundary cases, then checks four
selectors, both row parities, four alignments, four fractional U/V positions,
full blocks, every tail residue, exact signed distance thresholds and masked
endpoint reversal. Additional mixed-key gradients exercise both correction
sizes. Destination aliases target the U step, tail flag and key byte; an
inverse texture pattern makes stale key reads visible. A canvas texture alias
distinguishes immediate stores from pair prefetch.
Keyed dithered fog, hardware, plane, resource and initialization paths remain
deferred; HG2D is still outside `MODULES`.

## Slice 21: keyed dithered fog

Targets `0x20761af9` / `0x20769510` complete the sixteen software-only
projective targets. Selectors 44/47 reverse and 45/46 forward have the same
60..63 keying aliases that bypass mip creation. Raw key comparison precedes
shade lookup; mapped zero remains opaque. Skipped texels still advance UV and
shade. Full blocks omit the dither bias, while tails use the four phases.
All eight 33-entry tail tables are extracted from retail.

The 64-scenario baseline passes 62,528 operations, zero differences/faults
and 512 repaired divides. All 23 new mutations are caught. All preceding 229
projective mutations were re-executed and caught. The large run passes **20,000 scenarios / 19,540,000 operations**, zero
differences/faults and 160,000 repaired divides. Its combined 2,000-scenario
regression was launched separately; final results are recorded below when complete.

## Slice 22: ordinary polygon plane spans

GLOBAL +0xc90 with a zero +0x2f4 now uses `0x20784d69`. Ordinary 16-byte
polygon edges produce matrix endpoints per span, using the same coordinate
and transform arithmetic as matrix edges. Three high-word stores retain the
fractional scratch words left by edge construction. Plane spans preserve
retail's saved-stack values for the walker, its flat edges, scanned flat
edges, sorted intersections and the four-intersection register path.

The baseline passes 64 scenarios / 92,928 operations, zero differences and
faults, with 9,216 repaired divides. All 20 new mutations are caught. One
initially survived because the fixtures had two and six intersections but
no four-intersection polygon; the new five-vertex fixture catches it. A
20,000-scenario sweep uses the expanded fixtures.

## Slices 23–26: mipmapped projective targets

The other eight projective targets are software mip renderers. A hardware
eligibility bit selects them, but the span loops sample an indexed packed
pyramid directly. They are independent of actual Direct3D draw submission.
All 24 ordinary targets selected through data +0x4200 now have native paths:

| Family | Reverse | Forward | New mutations caught |
| --- | --- | --- | --- |
| Mip | `0x20741559` | `0x2074597a` | 27 |
| Mip fog | `0x207431a7` | `0x207475c4` | 15 |
| Mip dither | `0x2076da6d` | `0x207793d8` | 21 |
| Mip dither fog | `0x20772c04` | `0x2077e569` | 16 |

Distance uses a 64-bit multiply followed by SHRD 21, truncation, a clamp to
127, and the retail level table. Coordinate shifts, signed increments,
packed level offsets and live coordinate masks retain the assembly order.
Near endpoints fall back to the ordinary software kernels; distant dither
spans become doubled mip spans. Fog retains its row-zero and row-fifteen
shortcuts. Tail tables are extracted as data, not copied executable code.

Mip dither masks the biased coordinate register before each fetch, then
removes the bias. A destination aliasing +0x4375 changes that mask during
sampling; the fixture catches discarding this history. Plain full blocks
prefetch pairs, while fog variants store individual pixels. Direct fixtures
cover all nine levels, exact near thresholds, signed coordinates, high product
bits, palette-zero opacity and aliased texture reads.

All four 64-scenario baselines pass with zero differences or faults. The
final directed raw-mip baseline has 29,312 operations; mip fog has 47,808,
mip dither 58,112, and mip dither fog 64,320. Raw mip mutations initially
missed the high-product and V-mask cases; corrected directed fixtures catch
both. An initial complement mutation was equivalent because its overwritten
scratch word was replaced before use; its replacement changes the saved
initial magnitude, and is caught. The raw-mip large run passes **20,000 scenarios / 8,360,000 operations**,
zero differences/faults and 500,000 repaired divides. Its directed fixtures
were strengthened after that run began and pass separately at 64 scenarios.
The other large runs were launched independently.

## Slice 27: palette and haze resources

Linkage +0x58 and +0x68 load the default palette/haze resources and custom
palettes. The FE boundary uses the verified FE loader's actual sizes: 192
bytes for kind 9 and 4 KB for kind 10. The default path ignores file-service
return values, while the custom path exits on the initial FE failure. It
copies through the first period and appends `haz`, then refreshes the palette.

The native haze builder remaps sixteen rows through full palette-word output
indices, copies the eight special fog colours in rows ten through fourteen,
and builds twelve neutral-colour maps. RGB tolerance, darkness and ramp
search equality retain the original signed comparisons and first match.

The resource baseline passes **20,000 scenarios / 340,000 operations**, zero
differences, with full image/GLOBAL comparison and host/FE event digests.
It covers partial reads, FE failures, repeated cache hits, palette words over
255 and independently checked neutral-map boundaries. All **27 mutations**
are caught at 64 scenarios.

## Slice 28: initialization and haze transition

Linkage +0x04 now builds the palette layout, loads defaults, installs edge
pointers, remaps the glyph/shadow/colour tables, registers the CHAIN haze
callback and performs the processor/protection capability probe. The C
renderer has no patched instructions; the probe retains its result and
optimized-selector state. The harness replaces GetSystemInfo and
VirtualProtect for both DLLs, covering architecture/type boundaries,
protection failure and repeated initialization with a cached result.

The haze callback retains its 16-bit target history, fractional movement,
change cap, immediate-update flag and seven breakpoint records, including
the extra record preceding the nominal table. A differential failure at
current value 0x8000 exposed a signed-overflow error in the breakpoint test:
retail's JLE compares signed operands even when subtraction overflows. The
corrected signed comparison passes the full callback script.

The baseline passes 64 scenarios / 3,008 operations with zero differences.
All **30 mutations** are caught. The large run passes **20,000 scenarios /
940,000 operations**, zero differences.

## Slice 29: software mip creation

The missing preparation path is a host allocation and software filter at
`0x207900e0`, not a device upload. It grows a pooled texture to 96 KB, clears
the new 32 KB, and creates all eight packed levels using fixed 256-byte
row strides. Each 2×2 block averages the realized RGB triplets and selects
the nearest of its four original indices, retaining the earlier candidate
on ties. Resident textures skip growth; failed growth or out-of-pool handles
clear the mip selector bit and use the ordinary renderer.

The new harness compares the entire 96 KB heap after every operation, with
reallocation failures, cached levels, out-of-pool and unaligned handles,
and all fog/dither combinations. The baseline passes 64 scenarios / 10,304
operations, zero differences/faults. All 20 mutations are caught. One cache
mutation initially survived because fixtures omitted the unrelated 0x80000
metadata bit; a new uncached texture with that bit catches it. The final
baseline also explicitly reaches the last pool slot, aligned and unaligned.
The large sweep is in progress.

## Slice 30: optional software triangle fans

Linkage +0xa4 is a software fallback gated by kind 6, format 100, a nonzero
GLOBAL +0xcb6 word and a zero +0xca0 word. It sorts each fan triangle by
signed fixed-point Y, walks both halves with the original floating slopes,
and draws inclusive horizontal spans. The initial row comes from the row
table; later rows use the saved pitch. It does not clip to logical width or
height. Signed pixel pointers below 255 mean constant colours; 255 itself
is a texture pointer and faults on the directed null-page fixture.

U/V edge deltas are zero-extended before floating conversion, while X and
horizontal span deltas are signed. The slope conversion retains x87 caller
precision and reproduces __ftol truncation/indefinite results. Two apparently
unused horizontal-gradient calculations are retained because degenerate
triangles set floating exception status. The harness checks all three x87
precision modes and four rounding modes, control words and exception status,
plus the full canvas/image/GLOBAL and callee-saved registers. ECX/EDX are
unspecified caller-saved leftovers for this compiled-C service and are not
compared. Non-x86 floating status/precision still needs native validation.

The baseline passes 64 scenarios / 43,264 operations with zero differences.
There are 256 matching read faults at texture pointer 255. Independent
constant-fill pixels, all vertex permutations, flat/vertical/fractional
triangles, negative coordinates, fans of 3..8 vertices, unsigned UV wraps,
texture/destination aliases and early returns are covered. Mutation and
20,000-scenario sweeps are in progress.

## Verification and limits

The keyed-dither suite passes **20,000 scenarios / 16,600,000 operations**,
with zero differences and zero faults. All 800,000 retail host divide repairs
resume and complete. The full combined regression passes 2,000 scenarios per
suite. The preceding **204** projective mutations were rerun and all caught:
**229 caught** including the 25 new keyed-dither mutations. The other 489
mutants were not all rerun.

The keyed-dither family catches **25 new mutations** at 64 scenarios with zero
unresolved mutations. All **718** mutation anchors pass the separate audit.


The dithered-fog suite passes **20,000 scenarios / 15,220,000 operations**,
with zero differences and zero faults. All 160,000 retail host divide repairs
resume and complete. Both phases compare the full image, GLOBAL, guarded
canvas, literal saved ESP/tail addresses and all seven drawing registers.

The complete combined regression passes 2,000 scenarios per suite, including
**1,522,000 dithered-fog operations** with zero differences and zero faults
and 16,000 resumed retail host divide repairs. All earlier projective,
affine, three-attribute, palette and primitive suites pass in the same run.

The dithered-fog family catches **27 new mutations** at 64 scenarios with zero
unresolved mutations. The preceding **177** projective mutations were rerun
and all caught: **204 caught** in this slice. All **693** mutation anchors
pass the separate audit; the other 489 mutants were not all rerun.


The dither suite passes **20,000 scenarios / 13,280,000 operations**, with
zero differences and zero faults. All 800,000 retail host divide repairs
resume and complete. Both phases compare the full image, GLOBAL, guarded
canvas, literal saved ESP/tail addresses and all seven drawing registers.

The complete combined regression passes 2,000 scenarios per suite, including
**1,328,000 dither operations** with zero differences and zero faults and
80,000 resumed retail divide repairs. All preceding projective, affine,
three-attribute, palette and primitive suites pass in the same run.

The dither family catches **40 new mutations** at 64 scenarios, with zero
unresolved mutations. The preceding **137** projective mutations were rerun
and all caught, giving **177 caught** in this slice. All **666** mutation
anchors pass the separate audit; the other 489 mutants were not all rerun.
The mutation runner now captures output in files: Wine helpers can retain
inherited pipe handles after a test exits. The first pipe-based attempt timed
out; the full 40-mutant file-based rerun passes. Detection still requires exit
status 1 and a differential mismatch on stdout; fixture errors and crashes do
not count.


The keyed-fog suite passes **20,000 scenarios / 13,580,000 operations**, with
zero differences and zero faults. All 160,000 retail host divide repairs
resume and complete. Both phases compare the full image, GLOBAL, guarded
canvas, literal saved ESP/tail addresses and all seven drawing registers.

The corrected raw-keyed suite passes **20,000 scenarios / 6,800,000
operations**, with zero differences and zero faults; its 320,000 retail host
divide repairs resume and complete. The complete combined regression passes
2,000 scenarios per suite, including 1,358,000 keyed-fog operations, 680,000
raw-keyed operations, the existing unkeyed fog path, and every previous suite.

This slice catches **29 new keyed-fog mutations** plus **one new raw-keyed
ordering regression mutation**, with zero unresolved mutations. All **107**
preceding basic/keyed/fog projective mutations were rerun too: **137 caught**
in this slice. They include raw-versus-mapped key comparison, opaque mapped
zero, skipped U/V and shade advances, live versus cached key bytes, stores
before U/V/shade advancement, both shortcuts, and tail metadata. All **626**
mutation anchors pass the separate audit; the other 489 mutants were not all
rerun for this slice.

The expanded fog suite passes **20,000 scenarios / 10,120,000 operations**,
with zero differences and zero faults. All 160,000 retail host divide repairs
resume and complete. The final 506-operation script includes the independent
suppression and destination-to-step alias checks, with full image, GLOBAL,
guarded canvas, literal saved ESP/tail addresses and seven-register comparisons.

The fog slice catches **38 new mutations** at 64 scenarios, with zero
unresolved mutations. The initial family missed six mutations: identity
shade maps and shortcut-only random attributes hid changes to shade evolution;
exact -1/-2 gradients and a destination-to-step alias were needed to expose
suppression and row masking. Nonidentity gradient cases and those directed
fixtures close those gaps, and the full 38-mutant rerun passes. All **596**
anchors pass the separate audit. The preceding **69** basic/keyed projective
mutations were also rerun and all caught; the other 489 were not all rerun.

The complete combined regression passes 2,000 scenarios per suite with fog
dispatch enabled, including the existing basic/keyed projective paths and all
matrix, affine, palette, primitive and three-attribute suites. The standalone
fog fixtures were then strengthened for shade gradients, suppression and
scratch aliasing; those additions pass the 64-scenario baseline too.

The keyed suite passes **20,000 scenarios / 6,740,000 operations**, with zero
differences and zero faults. All 320,000 retail host divide repairs resume and
complete. Comparisons include literal saved ESP and tail addresses, the full
scratch image, GLOBAL, guarded canvas and all seven drawing registers.

The complete combined regression passes 2,000 scenarios per suite with the
keyed renderer enabled: 674,000 keyed operations and 434,000 basic projective
operations have zero differences and zero faults, alongside all existing
matrix, affine, three-attribute, palette and primitive suites.

The keyed slice catches **19 new mutations** at 64 scenarios, with zero
unresolved mutations. They cover target/direction selection, unsigned byte key
comparison and equality, both destination bytes, skipped coordinate advance,
immediate sample/store order, block/tail gates, and the keyed tail-table
family/direction/size/count. All **558** mutation anchors pass the separate
audit. The preceding 50 basic-projective mutations were also rerun and all
caught; the other 489 mutants were not all rerun for this slice.

The projective suite passes **20,000 scenarios / 4,340,000 operations**, with
zero differences and zero faults. Its 320,000 resumed retail divide repairs
complete successfully. These comparisons include the literal saved stack
pointer, full scratch image, GLOBAL, guarded canvas and all seven registers.

The projective slice catches **50 new mutations** at 64 scenarios with zero
unresolved mutations. Three initial misses exposed fixture gaps: equal matrix
base rows hid an incorrect axis stride, while repeated flat-edge drawing and
already-doubled source pixels hid changes to alignment and sample/store order.
Distinct nonzero base rows and a seeded canvas alias rendered once per row
close those gaps. The complete 50-mutation family then passes. All **539**
anchors pass the separate audit; the preceding 489 mutants were not all rerun
for this slice. The complete combined regression passes 2,000 scenarios per
suite, including the new projective path and all existing suites.

Both matrix suites pass 20,000 scenarios: 1,256,250 vertex operations with
1,560,888 resumed retail divide repairs, and 4,600,000 setup/fill operations
with 280,000 repairs. Both have zero differences and zero faults. The complete
combined regression passes 2,000 scenarios per suite, including the existing
software/optimized affine, three-attribute and primitive suites, plus all
263,680 finite colour/layout comparisons.

The matrix slice catches **61 new mutations** at 64 scenarios: 26 matrix-edge
and 35 setup/fill mutations. The setup family initially missed numerator
reduction because its large input scales all produced zero quotients. A
midrange scale closed that fixture gap and the focused recheck catches it.
The combined evidence has zero unresolved mutations. All 489 mutation
anchors then defined pass the separate audit; the preceding 428 mutations were not all
rerun for this slice.


The +0x54 suite passes 20,000 scenarios and 3,900,000 operations, with zero
differences and zero faults. Its 100,000 retail divide repairs resume and
finish their operations. The complete combined regression also passes 2,000
scenarios per suite, including +0x54 and the existing optimized affine backend.

The +0x54 slice adds **53 mutations**, all caught at 64 scenarios by
`python3 tools/hg2d_breaks.py 64 perspective_fill`. They cover shading rows,
UV/phase stepping, partial EAX writes, word-only dithering, both lookup stages,
pair ordering and aliasing, block/tail counters, the uncapped width, edge
stride/advancement, flat synthetic records, setup returns and all three
closing attributes. The flags used only by other affine paths are deliberately
mutated into this path to prove the fixtures observe their absence. Together
with the preceding 375, that slice brought the count to 428 defined and
detected mutations; the old 375 were not all rerun for that slice. Their
anchors were audited again.


The expanded combined regression passes 2,000 scenarios for every selected
suite, plus 263,680 exhaustive colour/layout comparisons, with zero differences.
The drawing suite intentionally matches 14,000 retail access faults in this
run; the new rendering suites have zero faults. The two final disc fixtures
are also checked in the final standalone 20,000-scenario run below.

Final large runs for the added rendering services:

| Suite | Scenarios | Operations | Differences / faults |
| --- | ---: | ---: | --- |
| affine | 20,000 | 3,120,000 | 0 / 0 |
| optimized | 20,000 | 4,120,000 | 0 / 0 |
| gouraud | 20,000 | 1,821,250 | 0 / 0 |
| blits | 20,000 | 2,460,000 | 0 / 0 |
| perspective | 20,000 | 1,321,250 | 0 / 0 |
| perspective_fill | 20,000 | 3,900,000 | 0 / 0 |
| matrix_vertices | 20,000 | 1,256,250 | 0 / 0 |
| matrix_fills | 20,000 | 4,600,000 | 0 / 0 |
| discs | 20,000 | 3,100,000 | 0 / 0 |

The affine run includes 80,000 resumed retail divide repairs, Gouraud 359,645,
blits 360,000, perspective vertices 808,843, and the +0x54 suite 100,000. Those repairs finish their
operations and are distinct from unrepaired faults. Teardown's independent
register/state fixture passes before every suite.

The preceding slice's sweep and focused fixture rechecks caught **375 distinct
mutations**: 19 texture, 18 colour/layout, 27 palette, 50 drawing, 22 plain
vertex, 43 fill, 35 affine vertex, 30 affine rasterizer, 19 optimized spans,
25 Gouraud, 18 blit, 43 perspective vertex, 24 disc and 2 teardown mutations.
An interrupted full sweep completed 285 checks; its resumed 90 checks exposed
two disc coverage gaps. Focused reruns catch both after adding a wide clip
that distinguishes the final profiles and a negative right endpoint. The
full union has zero unresolved mutations. All 489 anchors then defined also pass the
separate audit; that audit does not count as executing the mutants.

Other gaps found and closed during these slices: horizontal UVs needed an
x component to expose an incorrect second-coordinate advance; stride checks
needed plausible records between real records; packed keyed coordinates
needed distinct sampled texels rather than a constant texture. These are
fixture corrections, not accepted equivalences.


`make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=textured` passes 20,000
scenarios and 1,336,250 operations: zero differences and zero faults, including
808,843 repaired retail divides. All 35 textured-vertex mutations are caught
(`python3 tools/hg2d_breaks.py 64 textured`), and all 22 plain-vertex mutations
pass their recheck after the projection helper gained its optional remainder.
A combined regression passes 2,000 scenarios for every suite, including
133,625 affine vertex operations, plus the 263,680 finite colour/layout
comparisons. The three plain-vertex mutation anchors affected by the helper
signature and duplicated edge expressions now target their original code.


`make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=fills` passes 20,000
scenarios and 2,720,000 operations: zero differences and zero faults. All
43 new fill mutations are caught (`python3 tools/hg2d_breaks.py 64 fills`),
and all 50 existing drawing mutations pass their recheck after the mapper
refactor.
The combined regression run passes 2,000 scenarios each for textures,
palettes, drawing, vertices (157,625 operations) and fills (272,000), plus
the 263,680 finite colour/layout comparisons. Three existing drawing mutation
anchors now include their primitive's surrounding code to distinguish the
similar expressions in the new fill code. The earlier 214 mutation anchors remain checked in their owning source sections.

The final `polygons` run passes 20,000 scenarios and 1,576,250 operations,
with zero differences and zero unrepaired faults. Retail resumes after 673,201
signed divide repairs.

The vertex slice's full mutation run caught all 136 deliberate breaks: the
original 114 plus 22 for polygon projection and edges, with zero unresolved.

`make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=drawing`: 20,000 scenarios,
4,200,000 operations, zero differences. This includes 140,000 matching access
faults (five completed-remap null reads and two guarded zero-count spans per
scenario), plus 206,670 repaired retail divides that resume and finish drawing.
The final drawing mutation family catches all 50 breaks:
`python3 tools/hg2d_breaks.py 64 drawing`.

The texture, palette and colour baselines below still pass with the drawing
entries installed. Drawing validation is recorded separately so matching
retail faults are not mistaken for scenarios skipped before drawing.

`make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=textures`: 20,000 scenarios,
1,155,000 operations, zero differences. Per side: 113,970 allocations,
37,181 allocation failures, 94,046 FE loads, 124,850 COM releases,
1,250 full pools and 18,750 unaligned-handle cases.
`make -f tools/hg2d.mk hg2ddiff-run N=20000` also passes 20,000 palette
scenarios, 1,120,000 operations, 40,000 realizations and 53,297 CHAIN
notifications per side, with zero differences. The colour and layout sweeps
pass all 263,680 operations plus the seven directed fixtures.

The first three slices' mutation checks caught all 114 distinct breaks, zero unresolved:
19 texture, 18 colour/layout, 27 palette, and 50 drawing. The earlier families
were rechecked in the full mutation run; the final drawing family includes
the two extra zero-count loop breaks. `python3 tools/hg2d_breaks.py` runs all
families, or its second argument selects `textures`, `colours`, `palette` or
`drawing`, `polygons`, `fills`, `textured`, `affine`, `optimized`, `gouraud`,
`blits`, `perspective`, `perspective_fill`, `matrix_vertices`, `matrix_fills`,
`discs` or `lifecycle`. Only a reported differential mismatch
counts as detection; a build
failure, fixture error or harness crash does not.
The generated files reproduce exactly when re-extracted from retail.
The mutation runner can audit anchors with `HG2D_AUDIT_ONLY=1`; it reports
anchor checks separately from executed mutations. `HG2D_BREAKS_DIR` and
`HG2D_BASELINE_DIR` select isolated test directories. An interrupted sweep
can continue after an already verified mutation using `HG2D_RESUME_AFTER`
with its exact name; its report counts only the resumed portion.
`HG2D_MUTATION_NAME` selects one exact mutation for a focused recheck.

`tools/difftest/hg2ddiff.c` loads two distinct DLLs and checks retail's base,
both exported tables, and module ids. It restores each side's complete
initial image before each scenario. A shared deterministic pixel arena and
host/FE/COM stubs give both sides identical pointer values and inputs.

After each operation it compares eax (texture-handle returns normalized), a digest of
all host/FE/COM arguments, and the entire 0x28cd0-byte data image except
Linkage. The 55 self-pointers are normalized. Each modified 64 KB pixel block
is compared byte for byte; all allocated blocks and supplied pixels are also
compared at the end of each mixed script.

One in sixteen scenarios fills all 200 slots, checks refusal at capacity,
then frees and reuses the last slot. Other scenarios explicitly exercise the
lower and upper handle boundaries, in-range unaligned handles, and an
independently checked complete palette remap, followed by forty scripted
operations. Reused records have nonzero metadata. Allocation failures, FE
success and failure, name flags, access and update flags, ownership flags,
and hardware releases with accounting indices 0..8 are exercised. Mutation
checks reject fixture errors and crashes: only a reported differential
mismatch counts as a caught break.

Among return values, only texture handles (+0xa8 and +0xac) and the shade-map pointer
(+0x7c) receive address normalization.
A larger run caught a harness bug here: scenario 16,552 returned numeric
hardware accounting that happened to fall inside the replacement DLL's address
range. Treating every such value as a pointer invented a mismatch. Scalar
returns now remain literal, and that scenario passes in the full baseline.

The harness compares eax, palette cache carry, and the drawing registers
described above. It does not compare every caller-saved register. The real host,
FE loader, DirectDraw objects, arbitrary malformed COM pointers, accounting
indices outside 0..8, and initialization are outside this slice. The existing
PANELS model and its verified harness remain unchanged.

Remaining Linkage services: device-backed +0x9c/+0xa0/+0xbc.
Device palette realization, live Direct3D dispatch and the negative-length
shadow corrupt-stack return remain unfinished. The verified subsets do not
establish whole-module equivalence or readiness to run in-game.
