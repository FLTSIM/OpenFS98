# Gauges

All **120** reference-CD `.GAU` modules are rebuilt from C. `make` builds them
and `make install` installs them alongside the other replacements. The outputs
are `bin/gauges-all/Gauges/`, using the exact installed filenames from
`docs/setup_map.tsv`. Options work has separate sources and outputs.

## Implementation

`tools/gen_gauges.py` translates the reference disassembly into explicit C
expressions, calls and branches. It emits 1,318 routines, including 1,317
callbacks referenced by the original data images. The remaining helper is
called internally. DllMain and the `_ftol` import thunks are handled separately.
No retail executable code is copied into the replacement, and there is no
runtime instruction decoder. Unsupported instructions, imports, callback
signatures and live flag dependencies fail generation.

`src/gauges/gauge_lift.h` supplies register-width arithmetic, flags and x87
semantics. Each routine has a local frame and direct C control flow. Callback
wrappers preserve stdcall argument cleanup and the original floating-point
return boundary. Hardware division preserves the integer fault boundary used
by PANELS' exception repair. This is the project's current 32-bit Windows
hybrid ABI; removing its pointer widths and x87 return shim remains part of
the eventual native-platform work.

The generator extracts the writable data, constants and bitmap resources,
rebinds relocations to C routines and relocated data, and emits normal DLL
exports `ImportTable` and `Linkage`. Bitmap payloads are exact. Company version
resources are omitted, following the other extraction tools. Generated C,
resource files and binaries stay under ignored `bin/`; the reference files
stay under ignored `orig/`. No game assets are committed.

The collection includes the 737, Cessna, Extra, Bell, Learjet, Schweizer,
Sopwith and minicontrols modules, including radio stacks, autopilots and
PFD/EICAS. The original code totals 63,472 bytes. Six pairs have identical
complete code after image-base normalization; each still builds as its own
module with its own data and resources.

The initial hand-written warning and suction implementations remain in
`src/gauges/warning.c` and `suction.c`, with their independent extraction and
regression targets in `tools/gauges.mk`.

## Verification

```sh
make gauges
make gauges-check N=20000
GAUGES_RELOCATE=1 make gauges-check N=20000
make -f tools/gauges.mk all-gauges-hangs
make gauges-breaks
make gauges-panels N=1000
```

The standalone harness loads separate retail and rebuilt DLLs, exercises all
1,317 exposed entries, and compares full writable images, argument writeback,
PANELS calls, text and surface buffers, bitmap bytes, stack cleanup and
specified return values. Floating results are compared as raw 80-bit x87
values with all six exception flags. Tests cover all three x87 precision
settings and four rounding directions, arbitrary double bit patterns, and
264 directed edge cases per entry alongside 20,000 random vectors.
Void lifecycle functions have no specified EAX result.

A complete run compares **26,687,688 vectors** across all 120 modules. Both
preferred-address and forced-relocation runs pass. The separate nontermination
suite passes all 16 original/replacement compass cases with watchdogs.

The mutation harness requires explicit comparison failures for **348**
independent service-slot, numeric and bitmap changes across the collection.
Build errors, faults and timeouts do not count as caught mutations.

The PANELS integration harness runs identical rebuilt PANELS on both sides,
substituting only the gauges on the second side. It compares the transcript,
PANELS state, live heap, gauge data and final surfaces through aircraft
changes, painting, mouse input and scripted allocation failures. Stock panel
files reference 118 gauges; `737XFEED` and `CESSVAC` are unused by those files
and are covered by the standalone suite. The first scenario explicitly loads
C182/PANEL.RG and runs four frames; the remaining scenarios use the existing
seven-aircraft random scripts. Successful module loads are counted and required
for all 118 stock-referenced gauges.

The final strict run passes **1,000 scenarios, zero differences**, including
**129 matching scripted fault outcomes**. All 118 stock-referenced gauges load
successfully on both sides, with equal per-gauge counts. The zoom adapter
verifies and canonicalizes 44 address-derived results per side.

Pointer comparisons pair each relocated callback and each logical data
address, including retained pointers after unload. Numeric fields and pixels
are compared literally: their bits can coincide with another module's address.
Declared data pointer fields have explicit masks derived from relocations and
runtime element layouts. The panel-window list also compares its four float
scale fields literally. Packed palette bytes and text buffers allocated into
type-2 elements are compared literally too; the allocation's destination
field identifies a text buffer before any text is written. Regression seeds
34, 63, 118, 275, 351 and 379 exercise the address coincidences found while
expanding the comparison to the full collection.

PANELS' existing address-derived zoom bug needs a fixture adapter: in its
invalid-view branch, the retail variable service divides the variable's own
address by 64. The adapter first verifies that exact raw result, then expresses
it at the corresponding retail gauge address. Neither runtime DLL is changed
by this adapter. See [retail-bugs.md](../retail-bugs.md).

Real-game acceptance uses private game copies, Wine prefixes and Xvfb. All
74 staged flights are run for 900 FRAMELOCK passes, comparing retail twice
against the complete rebuilt gauge collection. Other modules are held equal,
and successful gauge loads are required. The normal game installation is
untouched by these checks.

The final collection run passes **74/74 flights**, with zero differing,
nondeterministic or missing runs. Antilles and St Kitts use the runner's
sound-disabled retry on both sides; all other final flights use sound.

```sh
HG2D_FRAMES_GAUGES=1 HG2D_FRAMES_OURS=orig/Modules/HG2D.DLL \
HG2D_FRAMES_ONLY=GAUGES FRAMES_MANY_JOBS=4 FRAMES_MANY_PASSES=900 \
bash tools/frames_many.sh -
```

## Preserved retail quirks

Schweizer and Sopwith compasses wrap headings with repeated subtraction.
NaNs, infinities and sufficiently large finite inputs can loop indefinitely;
the returning-vector suite bounds only these two callbacks, while the separate
watchdog suite verifies their hangs. Bell, Cessna and Extra turn callbacks
leave the x87 return stack empty for invalid selectors; conditional return
shims preserve that behavior. These are documented in the retail-bug ledger.

## Install and restore

```sh
make gauges-install                 # same FS98_DIR as the shared install
make gauges-revert                  # restore all 120 retail gauge files
```

Both commands use `tools/stage_gauges.py`, which validates the full collection
and installed filenames before copying. Retail restoration verifies reference
hashes. Copies use temporary files and replacement, then verify the bytes.
The same steps are included in the shared `make install` and `make revert`.
A disposable full-game `make install` / `make revert` roundtrip checks all
120 installed names and file contents alongside the eleven MDLs and modules.
`make run` launches the same `FS98_DIR`, using those installed gauges.
