# ANALYSIS.DLL — module id 29

**Reimplemented, verified, and running in-game.** ANALYSIS is the Flight menu's analysis: landing
analysis, manoeuvre analysis and course tracking. Each frame it records the
aircraft's position (GLOBAL `+0x560`, 24 bytes) into 1,000 samples, every few
frames, for the mode in GLOBAL `+0x814`: 1 landing, 2 course tracking, 3
manoeuvre. Landing arms above about 34 m, starts recording at about 100 ft,
and opens its dialog on the ground: the descent's profile and the vertical
speed at touchdown. Manoeuvre runs until command 0x1002b and opens its
dialog: the ground track. Course tracking draws the trail in the 3D world,
as a display list handed to G3D. The options dialog (`+0x34`, from MAIN's
menu) sets the mode.

`src/analysis/analysis.c` is the transliteration. `make analysisdiff-run`
drives retail and the OpenFS98 build over 20,000 whole-module scenarios: **0
differ**. `make analysis-breaks` applies 103 deliberate breaks:
93 caught, 10 proven equivalent. `make smoke` passes with it installed: 27/27 modules, 95
gauges. The analysis only runs in flight, which wine does not reach, so in
the game that proves init; the harness covers the rest.

The survey is round 8 of [../preflight.md](../preflight.md). The module has
REALITY's shape (window record, dialog blocks, line stubs), plus one 3D
output, which is a list in its own heap block and so is logged as its bytes.

## Binary and linkage

- Retail image base `0x20630000`; `.text` 8,086 bytes. Hand-written assembly
  from `.text+0x20` to `+0x912`, with ATC's stack-alignment wrapper around
  every import call. MSVC C from `+0x920` to `+0x2f6e`.
- `.rdata` at `0x20633000`: nine doubles at `+0x00` (0.5, 0.1, 0.125, 512,
  1/710, 2^-13, 1/6144, -4096, 3072), then the export directory.
- `.data` at `0x20634000`, 0x32c bytes: one image with `ImportTable` at
  `+0x88` and `Linkage` at `+0xc8`. Its 23 relocations are 18 code pointers
  (the Linkage, the three dialog blocks' callbacks, the exchange table's
  callback at `+0x1ec`) and five pointers into the image (the landing block's
  four texts and the manoeuvre block's, which points at `+0x179`: one byte
  into the text NUMERIC writes at `+0x178`). `tools/gen_analysis_data.py`
  zeroes them, and `analysis.c` installs its own.
- Imports: host, GLOBAL, CHAIN, HG2D (13), G3D (14), NUMERIC, UTIL. Win32:
  `DisableThreadLibraryCalls`; MSVCRT `_ftol`, done inline.

| slot | retail | callers | service |
| --- | --- | --- | --- |
| `+0x04` | `.text+0x020` | | init: the sample buffer (1,000 × 0x1c, unchecked), cleared; CHAIN list 5 |
| `+0x08` | `.text+0x08e` | | deinit: lists 5, 0xc, 0xe; both heap blocks; the mode off |
| `+0x18` | `.text+0x1b2` | | nothing |
| `+0x1c` | `.text+0x1b9` | | `(index, out)`: a whole record, if `index < 1000` |
| `+0x20` | `.text+0x1e2` | | `(index, out)`: its 24 bytes of position |
| `+0x24` | `.text+0x207` | | `(index, out)`: its flag byte |
| `+0x28` | `.text+0x22c` | | `(index, record)`: a record stored |
| `+0x2c` | `.text+0x24d` | | the count |
| `+0x30` | `.text+0x259` | | `(n)`: the count set, if `n < 1000` |
| `+0x34` | `.text+0x2ce0` | MAIN `+0x34` | the options dialog, `0x1405` |
| `+0x38` | `.text+0x920` | | the landing analysis, `0x14050002` |
| `+0x3c` | `.text+0x21e0` | | the manoeuvre analysis, `0x14050003` |

`+0x0c`..`+0x14` are null. Only MAIN calls ANALYSIS, and only `+0x34`. The
accessors keep eax and edx, so they return the caller's eax; nothing reads
it, and the reimplementation returns nothing.

A sample record is a flag byte (0x81 in the air, 0x83 on the ground: GLOBAL
`+0x366`), three bytes, then the position: latitude, longitude and altitude,
each 48-bit with its fraction word at `+2` and its whole dword at `+4`.

## What it does

**The frame** (CHAIN list 5, `.text+0x2ae`). A reset (image `+0x04`, which
nothing sets, or GLOBAL `+0x820`) clears the buffer. With a crash in progress
(GLOBAL `+0x838` 1..4) and samples, manoeuvre and landing end and show their
dialogs. Nothing runs while GLOBAL `+0x268` is set. Then, by mode:

- **landing** (not with `+0x365`): the height over the ground is GLOBAL
  `+0x574:+0x572` less `+0x20` (24.8). State `.data+0x72` arms above 0x22 m.
  At 0x1e:0x7ae5 (100 ft) or less, and no more than 0xff, it starts
  recording, from an empty buffer;
- **course tracking**: with `+0x81c` it registers the trail on list 0xe; it
  records only with `+0x818`;
- **manoeuvre**: it registers `.text+0x277` on list 0xc, which watches for
  command 0x1002b with a zero argument.

A countdown (`.data+0x70`) then takes a sample every few frames. The reload
is 2 for landing, `.data+0x73[+0x824]` (5, 14, 29, 50) for course tracking,
and `.data+0x71` for manoeuvre. A sample is taken only if the position
changed (`.text+0x53b`). Landing ends on the ground (`+0x2b8` at most 0xc7f
and `+0x366`). Manoeuvre ends on the command or a crash. Either way the
dialog runs, then the mode goes off and the buffer is cleared.

**The compaction** (`.text+0x5cd`): at 1,000 samples, every second one is
meant to be kept and the manoeuvre's interval doubled. See below for what it
actually does.

**The trail** (list 0xe, `.text+0x60a`), in course tracking with `+0x81c`.
The display list is 0x59e4 bytes, allocated once and cleared. It starts
0x76, then from the newest sample back: 0x51 0xf00f, a 0x2f/0x2e point
record with the sample's three coordinates (0x20 bytes), a 0x0e record
numbered by a toggle (0xa), and 0x0f with the toggle. Further segments are
closed by 0x10, the toggle flipping each time. There are as many segments as
`.data+0x76[+0x828]` says (50, 250, 500), and the list ends with 0. It goes
to G3D `+0x28` (stdcall, one argument).

**The landing dialog** (`.text+0x920`). The block at `.data+0x148` holds the
check (`.text+0x2250`, 0), the control procedure (`.text+0x9a0`), a
touchdown flag, and four texts: the vertical speed (`.data+0x108`, "+0"
until known), "100", "200" and the horizontal scale (`.data+0x138`). The
control has REALITY's protocol: 0 attach, 1 paint, 2 release, anything else
0. Painting clears the surface to 0xff, then:

- **the grid**, in 0xf000: the frame and the glide-path marks, in 8192ths of
  the width and 6144ths of the height;
- **the descent**, in 0xf00f. Each sample's x is how far along the track it
  is: UTIL `+0x64` gives its distance back from the last sample, out of the
  first-to-last range. Its y is its height over the last sample's. A run of
  one sample gets a second, a touchdown mark two units north on the ground.
  The first on-ground sample gives the vertical speed: the height lost
  between the two samples before it, times 120, made negative, formatted by
  NUMERIC `0x1000a`. The scale is range × 256 / 5 / 256 (`0x10000`). The
  curve stops at a sample whose flag is neither 0x81 nor 0x83.

**The manoeuvre dialog** (`.text+0x21e0`). Its block at `.data+0x188`
holds the units (GLOBAL `+0xc18`), a flag, and the scale's text. Before the
dialog it finds the run's extents: the highest and lowest latitudes, and the
longitudes' two ends, compared unsigned while the longitude's whole part is
between 0x4000 and 0xc000 (`.data+0x220` remembers which). These become
doubles through UTIL `+0xc8` (signed) or `+0xc0` (unsigned): the spans, the
midpoints, and the longitude span in metres at the middle latitude
(`+0xbc`, `+0xc4`, `+0x48`). Painting draws the scale and a cross with ticks
in 0xf000, and the track in 0xf00f. The scale is the larger of a tenth of
the east-west span and an eighth of the north-south one; it is shown as 512
times that (NUMERIC `0x1003a`) and kept as 1/710 of it. Each point is the
sample's offset from the middle, divided by the scale, about (4096, 3072) in
8192ths and 6144ths.

**The options** (`.text+0x2ce0`). UTIL's exchange (`+0x90`, the table at
`.data+0x1e0`) fills the records in the block at `.data+0x1a0`. Its callback
(`.text+0x2d90`, `(kind, base, mode)`) reads the mode into "on" (`+0xc`) and
a selection (`+0x14`: 0 landing, 1 manoeuvre, 2 course). It also reads the
reset (`+0x1c`) and GLOBAL `+0x818`/`+0x81c`. The table moves GLOBAL
`+0x828` and `+0x824` directly (records `+0x30`, `+0x38`). The block's check
(`.text+0x2d70`) validates through `+0x8c(…, 1)`, which refuses either list
value above 2 (status 0xb). On OK, `+0x8c(…, 0)` commits: the mode from the
selection, `+0x1a` bit 9 on, and for course tracking `+0x818` and `+0x81c`.

## Retail behaviour worth knowing

All reproduced unless said otherwise.

- **The compaction keeps its slot in ecx across host `memmove`.** Host
  `+0x21c` loads ecx and calls MSVCRT `memmove`, and what that leaves in ecx
  is the C runtime's. So every kept sample lands in one slot, and the count
  ends one past it: the trail collapses rather than halving. With the CD's
  1997 `MSVCRT.DLL` the slot is 7, the dword count (slots 1..6 stale, slot 7
  sample 998, count 8). With wine's builtin it is 0 (count 1). **Decided:
  reproduce the CD's 7 on every platform** (`AN_MEMMOVE_ECX`). Under wine,
  with no DLL override, retail ANALYSIS in-game still gets 0; that is the
  runtime's difference, not ANALYSIS's. Measured with a probe making
  ANALYSIS's own 0x1c-byte copies through each runtime.
- **The manoeuvre interval wraps.** The compaction doubles `.data+0x71` with
  a `rol`, so after 0x80 it is 1 again.
- **The change test counts with cx only.** `mov cx,0xc` / `repz cmpsw` in
  32-bit code counts (ecx's top half : 0xc) words. With a top half, an
  unchanged position compares on past the 24 bytes, into GLOBAL `+0x578`
  against the image's `+0x44`, finds a difference and takes the sample
  anyway. ecx there is what the path left: the frame's entry ecx, 0 after a
  reset (the host's `rep stos`), or a CHAIN add's (the list's new count).
  Retail CHAIN calls a callback with ecx set to the dispatch's first
  argument, and MAIN dispatches list 5 with (0, 0), so in the retail game
  the top half is always 0. The defect is latent. The reimplementation takes
  ecx through an entry stub, and those calls go through thunks that hand ecx
  back.
- **The landing height's borrow is bit 7 of the ground's low byte.** `sub
  ah,bl` / `sar ebx,8` / `sbb edx,ebx`: the `sar` replaces the subtraction's
  carry. The fraction's low byte (`al`) is never subtracted at all.
- **The landing scale divides by zero when the run has not moved.** The x of
  a point is `7692 × (range − distance) / range`. With a zero range the
  host's divide filter (see [host.md](host.md)) gives all ones. The
  reimplementation computes that without trapping.
- **The landing graph needs two samples.** A run of one gets a touchdown
  mark. A run of none reads the "last sample" from an index of −1, which
  `+0x20` refuses, so the frame's local is used uninitialised. The manoeuvre
  extents do the same (their loop starts at −1, so the first sample is
  always taken twice). Not reproducible. The reimplementation's local is
  zeros, and the harness ends a scenario that reaches a graph with no
  samples. In the game it takes a landing or manoeuvre with no movement
  since the last recorded sample.
- **The highest latitude's tie-break is wrong.** When the whole parts are
  equal, it compares the whole part with the stored fraction word, not the
  fraction with it.
- **"Off" does not stay off.** Committing the options with "on" cleared
  zeroes the mode, then the selection sets it again.
- **The trail's segment count can be unlimited.** `.data+0x76[3]` reads 0,
  which the loop's `dec`/`jne` takes as no limit. Past 500 samples that
  writes past the 0x59e4-byte block. The options' validator keeps the
  dialog's values to 0..2, so it takes a saved flight or an OK past the
  refusal to get there. Both tables are indexed by a GLOBAL dword with no
  bound; the harness keeps the index inside the image.
- **The manoeuvre scale's value is a mixed 8-byte local.** It stores the
  eighth of the latitude span as a double, then `_ftol`'s result over its low
  dword, and formats all eight bytes.
- **fst keeps the difference unrounded.** The midpoints are computed from the
  span's 80-bit value, not from the double just stored.

## Differential harness

`tools/difftest/analysisdiff.c`. Both DLLs are loaded into one process and
run in turn over the same script, driven through Linkage and the three CHAIN
callbacks.

- **Registers:** the host's memset stub leaves ecx 0, its memmove stub 7 (the
  CD's runtime), and CHAIN's add the list's new count, all as asm wrappers.
  Callbacks are called with a chosen ecx: 0 mostly, as retail CHAIN passes
  it, otherwise random, with and without a top half.
- **host:** the bump arena and window-record model of `realitydiff`. The
  dialog stand-in attaches, paints (0..2 times), asks an unknown mode now and
  then, and releases the graphs, logging every text and flag they leave. It
  shows the options' records, edits them as a user might, and calls the
  check. A refusal mostly keeps the dialog open; now and then OK goes
  through anyway. The divide filter is modelled for retail's `div`.
- **UTIL, NUMERIC:** retail's, state restored per side.
- **HG2D, G3D:** logged; the display list by its whole 0x59e4 bytes.
- **GLOBAL:** the options at `+0x814..+0x828`. The position moves a little
  or stands still, and latitudes stay where UTIL `+0x48` can take them (one
  in 64 does not, and retail UTIL faults on both sides alike). The height is
  steered onto 0x22, 0x1e:0x7ae5 and their neighbours, with the ground's bit
  7 both ways. The ground and crashes are set, and resets requested. The
  count is set to just under 1,000 so compactions happen. Each scenario runs
  under a random x87 precision control.
- **Compared** after every op: the transcript, the whole `.data` image (code
  pointers paired, self pointers as offsets), the arena, GLOBAL, UTIL and
  NUMERIC. Each side's module memory outside the image is digested before
  and after and must not change.

Coverage in the 20,000-scenario baseline: 4.8 million frames, 454,393
samples, 14,838 compactions, 12,290 landing dialogs (4,656 with a touchdown
speed), 51,326 manoeuvre dialogs, 25,596 options dialogs (10,129
refusals), 362,069 trails (47,704 reaching the block's last 0x40 bytes),
106,366 samples of an unmoved position taken because of ecx's top half,
5,815 divide fixes. 2,740 scenarios end at a graph with no samples and 230
at a retail UTIL fault on an out-of-range latitude, both sides alike.
Random vectors are not coverage: the boundaries above are steered onto, and
the breaks below say which edits the run can see.

## Deliberate breaks

`tools/analysis_breaks.py`: 103 breaks, 93 caught, 10 proven equivalent,
none missed. The first run missed fifteen. Five were gaps, now steered onto:
- the reset's ecx on the frame recording starts on (the last sample's
  position written back, the countdown due);
- the interval at 0x80 at a compaction;
- an altitude whose whole part is 0x100;
- units over 255;
- a negative list value in the validator.

The other ten are equivalences, each shown:

- `landing-ff`: anything over 0xff is over 0x1e, the next test.
- `lpoint-negative`: 1/256 truncates to 0, as 0 does.
- `lpoint-20`: the value is clamped to 0..0x1000000 first, so `>>8` and `/256`
  agree.
- `extents-min-frac`: on a full tie the store writes what is there.
- `compute-fst`: two 32.16 values differ by at most 49 bits, so the
  register's unrounded difference is already a double under any precision.
- `scale-compare`: at equality both branches use the same double.
- `mcurve-drawn`: `.data+0x170` is 1 in the image and only ever set to 1.
- `oread-status`: UTIL's read of the following record clears its status.
- `owrite-mode`: UTIL treats write modes 0 and 2 alike (dword stores, and a
  callback told to commit).
- `lcurve-one`: it differs only with no samples, retail's uninitialised
  frame, where the harness stops.

## Not yet

- **In-game.** `make smoke` proves only that it loads and the sim starts;
  the analysis needs flight, which wine does not reach.
- **OpenFS98's CHAIN does not set ecx for a callback.** Retail's passes the
  dispatch's first argument; ours leaves whatever the loop had. With both
  ANALYSIS builds that only matters through the change test's top half,
  but a module that reads it more directly would see it. A CHAIN fix, to
  prove in `chaindiff`.
