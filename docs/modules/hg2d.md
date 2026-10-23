# HG2D.DLL — module id 13

In progress, one slice at a time. Twelve Linkage services are implemented:
texture storage and access, loading through FE, palette remapping, hardware
texture release, colour conversion, and software palette realization. It uses snake_case and `<stdint.h>`; there is no `types.h` dependency.
HG2D is not in `MODULES` and must not be installed yet: initialization,
drawing, device-backed palette realization and the other services are incomplete.

The implementation and tests occupy HG2D-specific files. The shared Makefile,
README and MAIN files are untouched so MAIN can proceed independently.
For now build and verify from the repository root with:

```sh
make -f tools/hg2d.mk hg2ddiff-run N=20000              # all suites
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=palette
make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=textures
make -f tools/hg2d.mk hg2ddiff-run SUITE=colours       # finite sweeps
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

## Verification and limits

`make -f tools/hg2d.mk hg2ddiff-run N=20000 SUITE=textures`: 20,000 scenarios,
1,155,000 operations, zero differences. Per side: 113,970 allocations,
37,181 allocation failures, 94,046 FE loads, 124,850 COM releases,
1,250 full pools and 18,750 unaligned-handle cases.
`make -f tools/hg2d.mk hg2ddiff-run N=20000` also passes 20,000 palette
scenarios, 1,120,000 operations, 40,000 realizations and 53,297 CHAIN
notifications per side, with zero differences. The colour and layout sweeps
pass all 263,680 operations plus the seven directed fixtures.

`python3 tools/hg2d_breaks.py`: 64 mutations, all 64 caught, zero unresolved
(19 texture, 18 colour/layout, 27 palette).
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

Only texture-handle returns (+0xa8 and +0xac) and the shade-map pointer
(+0x7c) receive address normalization.
A larger run caught a harness bug here: scenario 16,552 returned numeric
hardware accounting that happened to fall inside the replacement DLL's address
range. Treating every such value as a pointer invented a mismatch. Scalar
returns now remain literal, and that scenario passes in the full baseline.

The harness compares eax and the palette cache carry, not every caller-saved register. The real host,
FE loader, DirectDraw objects, arbitrary malformed COM pointers, accounting
indices outside 0..8, and initialization are outside this slice. The existing
PANELS model and its verified harness remain unchanged.

Next: drawing primitives and the rasterizer behind the same owning harness.
Completion of these slices does not establish
rendering equivalence or readiness to run in-game.
