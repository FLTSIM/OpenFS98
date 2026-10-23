# HG2D.DLL — module id 13

In progress, one slice at a time. Twenty-five Linkage entries now have
implementations:
texture storage and access, loading through FE, palette remapping, hardware
texture release, colour conversion, software palette realization, points,
lines, frames, depth shading, text, plain polygon vertex submission and
untextured polygon filling, plus affine textured vertex submission. It uses
snake_case and `<stdint.h>`; there is no `types.h` dependency.
HG2D is not in `MODULES` and must not be installed yet: initialization,
textured drawing, device-backed palette realization and the other services
are incomplete.

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
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=textured
python3 tools/hg2d_breaks.py
```

The standalone makefile builds into `bin/hg2d-diff/`. The replacement is based
at `0x30000000` for testing beside retail; this is a test build, not the final
replacement DLL. The mutation runner uses its own `bin/hg2d-breaks/` directory
and leaves both the source and the baseline DLL alone. Nothing is installed
or committed by either command.

## Binary and table boundaries

Retail image base `0x20730000`, `.text` at `0x20731000`, 0x5f266 bytes.
`.data` at `0x20792000`: 0x28cd0 mapped bytes, 0x6000 initialized.
ImportTable is at data +0, with host (1), GLOBAL (2), CHAIN (4), FE (18),
UTIL (20), G3D (14), then the zero terminator. Linkage is at data +0x38,
0xc0 bytes, ending with the hit test at +0xbc (`0x2078b760`). It has forty
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
of this suite. DirectDraw paths and polygons remain unfinished.

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
unfinished textured-attribute edge builder and deliberately traps. Either
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

Textured polygons still trap when GLOBAL +0xc90's low word is nonzero. The
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

Textured close/fill +0x4c is not installed yet. The separate attribute builder
selected by +0x44 when GLOBAL +0xc90 and +0x2f4 are both nonzero remains
deferred, including through +0x48's plain fallback. Writes beyond the actual
60-edge storage, invalid view pointers and exception register contexts remain
outside this slice. No in-game installation is made.

## Verification and limits

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
similar expressions in the new fill code. All 214 mutation anchors match
exactly once in the expanded source.

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
`drawing`, `polygons`, `fills` or `textured`. Only a reported differential mismatch
counts as detection; a build
failure, fixture error or harness crash does not.
The generated files reproduce exactly when re-extracted from retail.

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

Next: affine textured close/fill +0x4c and its span selection, then the
remaining texture paths, clipping and rasterizer.
Completion of these slices does not establish
rendering equivalence or readiness to run in-game.
