# Gauges


> **Decompiled (2026-11-08).** All 120 gauges are hand-written C, one file per gauge
> (`src/gauges/<name>.c` over the generated `image.inc`, sharing `src/gauges/gauge.h`; see
> [Decompilation](#decompilation)); `tools/build_gauges.py` builds each from its hand file. They
> were first *lifted* (machine-translated C emulating the registers and flags, 2026-11-06); the
> lift, kept as an oracle until then, was deleted on 2026-11-14
> ([remaining-work.md](../remaining-work.md#lifted-code-all-removed-2026-11-14)).

All **120** reference-CD `.GAU` modules are rebuilt from C. `cmake --build build/win32-retail` builds them
and `tools/ofs ofs_install` installs them alongside the other replacements. The outputs
are `bin/gauges-all/Gauges/`, using the exact installed filenames from
`docs/setup_map.tsv`. Options work has separate sources and outputs.

## Implementation

`tools/gen_gauges.py` reads each CD gauge and writes, under ignored `bin/gauges-all/<name>/`,
the gauge's `image.inc` (its `.data` and `.rdata` at their RVAs, `ImportTable` and `Linkage`,
and a DllMain rebinding every relocated pointer, the code ones to the hand file's routines
through its `gauge_routine(rva)`), `gauge.def`, `gauge.res` and a manifest for the harness:
the 1,318 routines' argument bytes and x87 returns, the data words read as floats or integers,
the print and indirect slots, the pointers. It reads the disassembly for instruction boundaries
and operands only; it emits no code. Until 2026-11-14 it also emitted the lift.

Bitmap payloads are exact. Company version
resources are omitted, following the other extraction tools. Generated data,
resource files and binaries stay under ignored `bin/`; the reference files
stay under ignored `orig/`. No game assets are committed.

The collection includes the 737, Cessna, Extra, Bell, Learjet, Schweizer,
Sopwith and minicontrols modules, including radio stacks, autopilots and
PFD/EICAS. The original code totals 63,472 bytes. Six pairs have identical
complete code after image-base normalization; each still builds as its own
module with its own data and resources.

The initial hand-written warning and suction implementations remain in
`src/gauges/warning.c` and `suction.c`, with their independent extraction and
regression targets (`tools/ofs gauges-check`, `gauges-breaks`, ...).

## Verification

```sh
tools/ofs gauges
tools/ofs gauges-check N=20000
GAUGES_RELOCATE=1 tools/ofs gauges-check N=20000
tools/ofs all-gauges-hangs
tools/ofs gauges-breaks
tools/ofs gauges-panels N=1000
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

`tools/ofs gauges-breaks` runs `tools/gauge_breaks.py` (from a frozen copy: it rewrites
`src/gauges/`), the hand-written gauges' breaks. The lift's own mutation suite (348 service-slot,
numeric and bitmap changes to the generated C) went with it on 2026-11-14.
Build errors, faults and timeouts do not count as caught breaks.

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
tools/ofs gauges-install                 # same FS98_DIR as the shared install
tools/ofs gauges-revert                  # restore all 120 retail gauge files
```

Both commands use `tools/stage_gauges.py`, which validates the full collection
and installed filenames before copying. Retail restoration verifies reference
hashes. Copies use temporary files and replacement, then verify the bytes.
The same steps are included in the shared `tools/ofs ofs_install` and `tools/ofs ofs_revert`.
A disposable full-game `tools/ofs ofs_install` / `tools/ofs ofs_revert` roundtrip checks all
120 installed names and file contents alongside the eleven MDLs and modules.
`tools/ofs run` launches the same `FS98_DIR`, using those installed gauges.

## Decompilation

Started 2026-11-08, by the same method as the other modules: hand-written C checked against
retail by the harness, then deliberately broken.

**The layout.** `tools/gen_gauges.py` also writes each gauge's `image.inc`: retail's `.data` and
`.rdata` at their RVAs, the `ImportTable`/`Linkage` exports, and a DllMain that rebinds every
relocated pointer, the code ones through the gauge's `gauge_routine(rva)`. A gauge's file names
retail's addresses by RVA (`GD64(0x37a8)`, a constant `GR64(0x2000)`), lists its routines in
`GAUGE_ROUTINES`, and writes the gauge header's standard callbacks (install, initialise,
update, generate, draw, kill, query, the empty module init) as `gauge.h`'s macros: the SDK's
own header macros generated them, and they differ between gauges only in five addresses. Every
other routine is written out. Hand files build with `-fexcess-precision=standard -Werror`.

**x87 fidelity**, since the harness compares the 80-bit result and all six exception flags
under every precision and rounding mode: a comparison is `fcom`'s (`gauge_fcom`, `GAUGE_TEST`;
C's compile to `fucomi`, which raises invalid only for a signalling NaN); `_ftol` is `fistp` under
chop rounding (`gauge_ftol`); a store retail makes and then overwrites still raises its rounding
flags, so it is kept (`gauge_fst`), and so is the order retail stores before it tests (GCC sank
one such store into the branch that used it, which lost an underflow flag); a rounding store
whose unrounded value is used again goes through `gauge_fst`, since GCC otherwise rounds by
storing and reloading, and reloading a denormal raises DE where retail's `fst` raises nothing.
Two retail behaviours C cannot state go through small thunks: the turn coordinators' empty x87
return (`GAUGE_MAYBE_FP`, GAUGE-TURN-EMPTY-RETURN) and nothing else so far. The compasses'
non-terminating wrap (GAUGE-COMPASS-WRAP-HANG) is reproduced as written.

**The harness, strengthened** (`tools/difftest/allgaugesdiff.c`). Breaks of the first gauges went
unseen because every data double got the same small integer each case and no integer data
varied. Now: about half the cases give each data double its own value (an integer, a half, a
larger value, another slot's, the argument's own, or one of the gauge's finite normal `.rdata`
constants or one next to it); the integers the code reads (`int_slots`, recorded by the
generator, pointers excluded) take 0..3, a 16-bit word or a word above any user address; a
quarter of the cases vary the PANELS rectangles' heights; and 16383.0 is an edge value. The
lift passed all of it but EXGMETER and LRFLAPS, whose generated C reloaded a denormal.

**Result.** All 120 gauges against retail, `bash tools/run_gaugesdiff.sh --all 20000` (`tools/ofs
gauges-check N=20000`): 120 checked, 0 differ, on the strengthened harness. Breaks:
`tools/gauge_breaks.py` (run from a frozen copy; it rewrites `src/gauges/`): 67, all caught (re-run 2026-11-19: 66, `chrn-minutes` missed, also at N=20000; it differs only once the elapsed time passes 2^31, the clock behind the start, which the harness no longer reaches; open in remaining-work.md); one
proven equivalent and one not observable through the PANELS stub are in its docstring. In the
game, the 74 staged flights, retail gauges twice against the hand-written collection for 900
passes each (`HG2D_FRAMES_GAUGES=1 ... tools/frames_many.sh -`, as above): **74 match**, no faults. Installed
2026-11-08 (`tools/ofs gauges-install`).

More lessons: the shapes of two routines can be identical while their constants differ, so each
routine's addresses come from its own listing; constants are read through `volatile`
(`GR64`), since GCC folded `1000 - 1000` at compile time to +0 where retail's rounding-down mode
gives -0, and narrowed constants to floats; GCC hoisted an argument's load to a function's entry,
where retail loads it on one path only (a denormal's DE), so that load is volatile too; and a
store retail makes before possibly overwriting it is kept with `gauge_fst` (GCC dropped one, and
its precision flag with it).
