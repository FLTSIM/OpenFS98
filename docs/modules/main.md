# MAIN.DLL — module id 3

**In progress.** MAIN is the sim's controller: situations and flights, the
default aircraft, clouds and smoke, the frame's order of business, and most
of the menus' commands. It imports every other module, so it is last but
HG2D, and it is being done the way MULTIP was: a slice at a time behind one
harness, `tools/difftest/maindiff.c`, with a suite per slice. Until every
slot is done MAIN is not in `MODULES` and is never installed.

| slice | services | suite | status |
| --- | --- | --- | --- |
| 1, situation records | `+0x54`..`+0x6c`, and `.text+0x15c80`/`+0x15d50` | `records` | verified: 20,000 scenarios, 0 differ; 38 of 40 breaks caught, 2 proven equivalent |
| 2, the situation reader and writer | `+0x38`..`+0x4c` (assembly, `.text+0x11cd2`..`+0x13104`) | `situations` | verified: 20,000 scenarios, 0 differ; 63 of 69 breaks caught, 6 proven equivalent |
| 3, range and bearing | `+0x70` (assembly, `.text+0x19507`..`+0x1972f`) | `nav` | verified: 20,000 scenarios, 0 differ; 13 of 14 breaks caught, 1 proven equivalent |
| 4, the nav read-outs | `+0x78`, `+0x80` (assembly, `.text+0x19c94`, `+0x19f10`) | `navio` | verified: 20,000 scenarios, 0 differ; 12 of 12 breaks caught |
| 5, the station descriptor | `+0x7c` (assembly, `.text+0x19cd5`..`+0x19db9`) | `navdesc` | verified: 20,000 scenarios, 0 differ; 9 of 9 breaks caught |
| 6, the season, zone and clock | `+0x74` (assembly, `.text+0x18781`..`+0x189c3`) | `season` | verified: 20,000 scenarios, 0 differ; 23 of 24 breaks caught, 1 proven equivalent |
| 7, the clock as digits | `+0xa4` (assembly, `.text+0x16ddf`..`+0x16e30`) | `clock` | verified: 20,000 scenarios, 0 differ; 6 of 6 breaks caught |
| 8, the view cycler | `+0xc8` (assembly, `.text+0x18f0`..`+0x193a`) | `view` | verified: 20,000 scenarios, 0 differ; 10 of 10 breaks caught |
| 9, the analysis menu greying | `+0x30`, `+0x34` (assembly, `.text+0x16920`, `+0x169a0`) | `menu` | verified: 20,000 scenarios, 0 differ; 6 of 6 breaks caught |
| 10, an object slot release | `+0x8c` (assembly, `.text+0x16570`..`+0x165a2`) | `obj` | verified: 20,000 scenarios, 0 differ; 6 of 6 breaks caught |
| 11, the latitude clamp | `+0x1c` (assembly, `.text+0xc520`..`+0xc552`) | `clamp` | verified: 20,000 scenarios, 0 differ; 5 of 6 breaks caught, 1 proven equivalent |
| 12, the sun direction and intensity | `+0xa0` (assembly, `.text+0x830d`..`+0x95e9`, helper `+0x9097`) | `sun` | verified: 20,000 scenarios, 0 differ; 12 of 14 breaks caught, 2 proven equivalent |
| 13, allocate an object slot | `+0x84` (assembly, `.text+0x16100`..`+0x162e5`) | `objalloc` | verified: 20,000 scenarios, 0 differ; 14 of 15 breaks caught, 1 proven equivalent |
| 14, the eye's reticle | `+0xc0` (assembly, `.text+0x16a0`..`+0x18c2`) | `eye` | verified: 20,000 scenarios, 0 differ; 12 of 13 breaks caught, 1 proven equivalent |
| 15, the altitude band and its 3D submission | `+0x9c` (assembly, `.text+0x6de9`..`+0x6f58`, helper `+0x7200`..`+0x73d6`) | `band` | verified: 20,000 scenarios, 0 differ; 33 of 33 breaks caught |

## Binary and linkage

- Retail image base `0x207e0000`. `.text` 0x1d360 bytes: by entry-point
  alignment about 92 KB of MSVC C (entries on 16 bytes) and 28 KB of
  hand-written assembly (entries anywhere), interleaved in runs; about 367
  routines. The assembly runs are `.text+0x67fc`..`+0x6c88`,
  `+0x6de9`..`+0x7095`, `+0x741f`..`+0x77c7`, `+0x7ec7`..`+0x8609`,
  `+0x8868`..`+0x918b`, `+0x9772`..`+0xa04b`, `+0x11695`..`+0x11b81`,
  `+0x11e3d`..`+0x12f4f` (the situation reader and writer),
  `+0x16c6c`..`+0x16e31` and `+0x17e68`..`+0x19eb2`.
- `.data` at `0x20800000`: 0x2ee00 bytes mapped, 0x8000 initialised.
  ImportTable at `+0` (23 entries: the host, GLOBAL and every module but
  BOOSTER, WINMSG, REALITY and MPCHAT), Linkage at `+0xc0` (51 slots, 47
  routines). 635 relocations land in it: 455 pointers into `.data` itself
  and 180 code pointers (92 distinct targets) outside the Linkage.
  `tools/gen_main_data.py` writes the image with the self-pointers and the
  Linkage zeroed, and a table for `main.c` to install the self-pointers;
  the other code pointers keep retail's values, as G3D's do. Which of those
  tables only MAIN calls through, and which it hands to the host (menus,
  dialogs, commands), is for the slices that reach them to settle.
- `.rsrc` 0x3664 bytes (bitmaps, by `LoadImageA`); `.rdata` 0x1f9 bytes.
  Neither is reproduced yet.
- Win32: KERNEL32 (interlocked counts, `lstrcpyA`, `lstrcmpiA`,
  `GetUserDefaultLCID`), USER32 (`CharNextA`, `CharPrevA`, `wsprintfA`,
  `LoadImageA`), GDI32 `GetObjectA`, MSVCRT (`atoi`, `_itoa`, `_ftol`,
  `memmove`, `strchr`, `sscanf`, `sprintf`).

## Linkage

Callers among the retail modules, by `impscan.py`. The services no module
calls are reached from the host, the `English\` dialog modules, or CHAIN.

| slot | retail | callers | |
| --- | --- | --- | --- |
| `+0x04` | `.text+0x0020` | host | init: Linkage `+0x18`'s registration, the instance (host `+0x298`), `.text+0x6cf0`, CHAIN list 0x14 |
| `+0x08` | `.text+0x0070` | host | deinit |
| `+0x18` | `.text+0xc000` | | |
| `+0x1c` | `.text+0xc520` | FACIL | **slice 11**: clamp the latitude |
| `+0x20` | `.text+0xa090` | | |
| `+0x24`..`+0x34` | `.text+0x16a60`, `+0x165b0`, `+0x16880`, `+0x16920`, `+0x169a0` | REALITY (`+0x30`, `+0x34`) | `+0x30` and `+0x34` (**slice 9**) grey and enable menu items `0x1001`..`0x1006` |
| `+0x38`..`+0x4c` | `.text+0x11eeb`, `+0x11e3d`, `+0x120aa`, `+0x127b3`, `+0x12f50`, `+0x12f6a` | ADVDRV (`+0x3c`), REALITY (`+0x44`), DEMO (`+0x48`, `+0x4c`) | the situation reader and writer, assembly: `+0x48` loads a situation from memory, `+0x4c` from a file |
| `+0x50` | `.text+0x13105` | | assembly, the largest gap (11 KB) |
| `+0x54`..`+0x6c` | `.text+0x15cc0`..`+0x15f50` | WINMSG, WINDOW, PANELS (`+0x5c`..`+0x64`) | **slice 1**, below |
| `+0x70` | `.text+0x19507` | ADVDRV, LANDME | **slice 3**: range and bearing between two points |
| `+0x74` | `.text+0x18781` | ADVDRV, MULTIP | **slice 6**: the season, the zone and the clock |
| `+0x78`, `+0x7c` | `.text+0x19c94`, `+0x19cd5` | | `+0x78` **slice 4**, `+0x7c` **slice 5** |
| `+0x80` | `.text+0x19f10` | ADVDRV, PANELS | **slice 4**: `(which, &tenths)` |
| `+0x84`..`+0x90` | `.text+0x16100`, `+0x162f0`, `+0x16570`, `+0x10560` | CONTROLS, WINDOW (`+0x88`) | object table at `.data+0x9794`, 0x2d4-byte entries: `+0x84` **slice 13** allocates, `+0x88` iterates, `+0x8c` **slice 10** releases |
| `+0x94`..`+0xa0` | `.text+0x7140`, `+0x6f59`, `+0x6de9`, `+0x830d` | WEATHERM (`+0x94`), WINDOW (`+0x98`, `+0x9c`) | `+0x9c` is **slice 15** (the altitude band), `+0xa0` is **slice 12**: the sun direction |
| `+0xa4`, `+0xa8` | `.text+0x16ddf`, `+0x6450` | ADVDRV, CONTROLS, WINDOW | `+0xa4` is **slice 7** |
| `+0xac`..`+0xbc` | `.text+0xc870`, `+0xcdb0`, `+0xdd70`, `+0xe5a0`, `+0xed10` | CONTROLS (`+0xac`), FACIL (`+0xb0`) | |
| `+0xc0`..`+0xc8` | `.text+0x06a0`, `+0xc560`, `+0x18f0` | WINDOW, FACIL, CONTROLS | `+0xc0` is **slice 14** (the eye's reticle), `+0xc8` is **slice 8** |

## Slice 1: situation records

`src/main/main_records.inc`, `.text+0x15c80`..`+0x15fff`, MSVC C. The layer
under the situation reader and writer.

A situation file is a run of records: a 16-bit tag, then a length that
counts the header too, 16 bits for a tag below 1000 and 32 bits otherwise.
Tag `0xffff` ends the file. The assembly reader takes the records it knows
into GLOBAL itself, through a table of 0x12-byte entries at `.data+0x4456`;
any other record goes out to its owner through `.text+0x15c80`, which makes
it current (`+0x9754`, its body `+0x9758`, what is left `+0xd144`) and sends
CHAIN list 0x14 message 0x22 with it. The owner takes the body with `+0x58`
and can ask which record it is with `+0x54`. Writing, `.text+0x15d50` makes
the file current (`+0x975c`) and sends message 0x23. Each module then writes
its own records with `+0x5c` (begin: the header), `+0x60` (body bytes, never
more than the record declared) and `+0x64` (end: whatever is still owed,
`+0x9760`, written as zeros). A record begun before the last one was ended
ends it first.

`+0x68(path, tag, &out, &size)` reads the file `path` (extension 2) whole and
returns a heap copy of record `tag`'s body; `+0x6c` does the same in the file
named at `.data+0x40dc`.

Retail behaviour worth knowing, all reproduced:

- **Both header writes report into the size argument's own slot**, so
  `+0x5c` reads the size first.
- **A record's 16-bit length wraps** for a tag below 1000 and a body of
  0xfffc bytes or more.
- **The finder trusts each record's length.** A length of 0 never moves on:
  retail spins. A length below the header's own size steps back over the
  header. A huge one wraps the index. A file cut short inside a header has
  it read on past the block into whatever follows on the heap, which is how
  the harness meets the spin.
- **The reader keeps its leftovers.** Outside a dispatch `+0x54` says there
  is no record, but `+0x58` still reads from where the last record's body
  had got to.
- **`+0x60` with no record open answers -1 and 0**, but `+0x64` with nothing
  owed answers 0.

## maindiff

`tools/difftest/maindiff.c` loads retail `MAIN.DLL` and the OpenFS98 build
side by side (ImportTable at image+0, Linkage at +0xc0) and runs a suite on
both. Each suite compares the transcript of every call and its return, and
MAIN's whole `.data` image after every op. Words the loader relocated are
compared as what they point at, the Linkage is left out, and so are the
import links. Internal routines whose callers are not done are called
directly: retail's at their addresses, ours through exports named
`main_<routine>`, which go when their callers are done. A fault is an
outcome; so is a hang, which a watchdog cuts after 300 ms, ending the
scenario with its state not compared.

`records`: the host's file services are a model over files in memory (its
writes sometimes fall short, as on a full disk); list 0x14's listeners are
scripted modules writing records (whole, short, too long, begun and never
ended, none) and reading them in chunks; the finder runs over generated
situation files with both header sizes, the terminator or none, lengths
below the header's, past the end of the file, and huge.

`make maindiff-run SUITE=records`: 20,000 scenarios (about 15 minutes, most
of it the hangs' 300 ms), 0 differences, with 88,508 saves writing 177,262
records, 70,872 records read in 285,113 chunks, and 134,010 finds (56,807
found). 3,996 scenarios end on an allocation the host would refuse (a huge
record length) and 1,358 on the spin, alike on both sides.

`make main-breaks SUITE=records`: 40 breaks, 38 caught. The two
equivalences: `+0x60`'s file check (what is owed, `+0x9760`, is set non-zero
only by `+0x5c`, which needs a file, and the save zeroes it before it clears
the file), and its clamp taken as `<=` (at `n` equal to what is owed both
branches leave nothing owed).

## Slice 2: the situation reader and writer

`src/main/main_situation.inc`, `.text+0x11cd2`..`+0x13104`, assembly. The
entry points:

| slot | retail | |
| --- | --- | --- |
| `+0x38` | `.text+0x11eeb` | the default flight: GLOBAL `+0x24`'s file, or the one strings `0x9004` (a directory) and `0x9016` name |
| `+0x3c` | `.text+0x11e3d` | the flight `{ ext, path }`, or the default if it is not there; WEATHERM `+0x24` afterwards |
| `+0x40` | `.text+0x120aa` | save as `(title, description)` into string `0x9002`'s directory |
| `+0x44` | `.text+0x127b3` | the current flight again; if it will not open, an alert (string `0xf462`) and the default |
| `+0x48` | `.text+0x12f50` | a situation in memory (DEMO's videos) |
| `+0x4c` | `.text+0x12f6a` | `length` bytes of an open file |

Every call it makes goes through a stack-alignment shim:
`mov [esp-7], 0x04050607`, `and esp, -4`, `sub esp, 4`, `and dword [esp], 7`,
the call, then `add esp, [esp]`. With esp already aligned, as it always is,
that is a plain call.

**The table at `.data+0x4456`** lists the 77 records MAIN takes itself, in
0x12-byte entries:

| | |
| --- | --- |
| `+0x0` | u16 tag; `0xffff` ends the table |
| `+0x2` | u8 mode: 0, `0xa` and `0xb` taken in; `0xc`, `0xd` and `0xe` only while GLOBAL `+0x1248`, `+0x124c` or `+0x1250` (in that order) is set, `0xe` then clearing `.data+0x4213`; 1 and anything else skipped. A save leaves mode `0xa` records out altogether |
| `+0x3` | u8 where: 0 GLOBAL plus the offset; 1 the absolute address (MAIN's own `.data`); 5 through a pointer (unused); 2, 3 and 4 the address, at the record's own length |
| `+0x4` | u16 length, or `0xffff` for a list of five-byte fields `{ u32 offset, u8 size code }` after the entry (codes `0x80` 1, `0x81` 2, `0x83` 3, `0x82` 4, otherwise the low seven bits) |
| `+0x6` | u32 offset or address |
| `+0xa` | u32 the next entry, after a list of fields |
| `+0xe` | u32 a routine, called `(tag, 1)` before a save, when a non-zero answer stops the save, and `(tag, 0)` after a load |

Six entries have routines, all just before the reader (`.text+0x11cd2`..
`+0x11e3a`): records `0x59`, `0x92`, `0x93` (which of a pair of flags they
set), `0x35`, `0x5e` and `0x5f` (clouds: 0 or `0x3fff`, or halved). Our table
keeps retail's code addresses, as G3D's does, and `table_call()` maps them to
ours.

**Loading** (`.text+0x1295e`): list 0x14 hears `0x20`, SOUND event 1, a
dozen GLOBAL fields are reset; the records are walked; then what they left
is made consistent. A season with no day of the year takes one from the
table at `.text+0x13093`. Clouds of `0x7f0000` become `0x7fffff`. CONTROLS
are told, and the aircraft is loaded through ACONTAIN. Host command
`0x100c2` or `0x100c3` is posted, and list 0x14 hears `0x70` and then `0x21`.
**Saving** writes every entry of the table, then every module's records
(slice 1), then the end tag.

Retail behaviour worth knowing, all reproduced:

- **A load copies the table entry's length, not the record's.** A record of
  another length leaves the walk out of step with the file from there on.
  With a damaged situation the walk reads its next tag from wherever that
  leads, and usually faults there (`.text+0x1251b`).
- **When neither default flight file is there, the sim stops.** An alert
  (UTIL `+0xa4`, string `0xf461`), then a spin at `.text+0x1207c` that never
  ends.
- **The day of the year comes from a table in `.text`**, the days each
  season starts on (355, 81, 172, 264 for seasons 0 to 3, winter first), indexed by GLOBAL
  `+0x248` without a bound. Past 3 it reads on through MAIN's code, the gap
  and `.rdata` (`src/main/main_text_tail.inc`, generated), and far enough,
  its live `.data` (self-pointers as retail's), as G3D's season letter does.
- **A save leaves a stack pointer in the table.** Record 2 (the description)
  is written from a buffer on `+0x40`'s stack, whose address goes into its
  entry at `.data+0x4480` and stays there. Record 2 is mode 1, so a load
  never writes through it.
- **ACONTAIN `+0x30` is asked only when GLOBAL `+0x1244` is clear**, but the
  aircraft at `.data+0x4217` is loaded (`+0x28`) whenever it is not empty.
  The branch lands on the alignment shim before the `strlen`. (The first
  transliteration put both under the test; the harness caught it.)
- **The load leaves `.data+0x41e1` naming a file it has closed.**

MAIN `+0x74` (the season and date, **slice 6** below) and `+0xa4`, which
loading calls, are still seams *in the situations suite*: retail's two call
sites (`.text+0x12b5f`, `+0x130fe`) are patched in the harness to stubs, and
ours calls them through its Linkage, which the harness points at the same
stubs. Both are now transliterated and verified on their own -- the `season`
suite drives `+0x74` directly and the `clock` suite drives `+0xa4` (slice 7),
each past the seam. Un-seaming them in the load path waits on FE being
modelled there (`+0x74` needs it; `+0xa4` has no dependencies and could be
un-seamed on its own). `+0x74` is pushed the day as a word over two stale
bytes, which the slice confirms it reads only the low word of.

### Verifying the reader and writer

`make maindiff-run SUITE=situations` runs 20,000 scenarios and reports 0
differences; `make main-breaks SUITE=situations` applies 69 deliberate breaks,
63 caught and 6 proven equivalent (none missed). The equivalences are all
"retail's table never reaches this": no size code outside `0x80`..`0x83`/`0x20`
(`field-low`), no tag `1000` (`write-hdr`), no entry of kind 5 (`write-kind5`)
or mode `0xe` (`take-e`), GLOBAL `+0x5f4` never 4 at the one byte that is read
(`after-5f4`), and the one count that is stored as itself either way
(`after-100`).

The `situations` suite fills free and unread memory with `0xff` rather than
`0x00`, so a mutated situation's record lengths come back as large values
instead of zero — which is what drives the parser off the end of the file and
exercises the walk the way a damaged `.stn` on disk would.

**Walking off into unshared memory is not a difference.** When a corrupt
situation drives the record walk past the end of its buffer, the next tag,
length and `NEXT` link are read from whatever follows — the arena's `0xff`
fill, or a base-relative pointer the walk itself clobbered in. From there the
walk follows pointers into memory the two sides do not share: retail is loaded
at its preferred `0x20800000`, OpenFS98's MAIN is relocated to a different
base in the same process, so identical code following its own base-relative
pointers reaches different bytes and diverges. This is the same class the
harness already handles for a cursor left outside the image (`cursor_wild`)
and for a load that overwrote the situation table (`table_overwritten`): the
op ran, but its result cannot be compared.

The harness recognises the walk-off at the moment it happens. `cap_lengths`
caps every generated record body to `0x400` (max dispatched length `0x406`),
so a dispatched record whose length exceeds `0x800` can only be one read out
of the fill or off a clobbered pointer. Both sides read that same impossible
length at the same first such record — the buffer is still shared up to there —
so when the dispatch hook (`list 0x14`, selector `0x22`) sees a length over
`0x800` it drops that record's text on both sides and ends the op, which is
then counted among the walk-offs rather than compared. The guard is scoped to
the `situations` suite only: the `records` suite fills with `0x00` and
deliberately dispatches huge-length records to test the length path, so there
the guard stays out of the way (`situations_suite` in `maindiff.c`). Across
20,000 situations, 7,077 scenarios end this way (most after a fault both sides
take alike); none is a missed difference.

## Slice 3: range and bearing

`src/main/main_nav.inc`, `.text+0x19507` with its two helpers `.text+0x1962c`
(the delta vector and its magnitude) and `.text+0x195ab` (the angle of the
delta), assembly. ADVDRV and LANDME call `+0x70(from, to, &bearing, &range)`.
It has an ordinary stack frame -- no inbound registers -- so it transliterates
as plain stdcall C, not through the register-threaded entry of slice 2.

The two positions are records whose longitude is a 48-bit value at `+2`/`+4`
and whose latitude is at `+0xa`/`+0xc`. The service builds a three-component
vector between them: the longitude delta's high dword (the low word is
dropped), a term from each record's field `+0x14` (or, when it holds the
adventure sentinel `0xffffd8f1`, GLOBAL `+0x20` shifted down a byte), and the
latitude delta turned into a planar x by UTIL `+0x48`. The magnitude of that
vector (UTIL `+0x18`) is the range, stored through the pointer and left in
`.data+0x72e0`; the angle of its x and z (UTIL `+0x3c`, after scaling both up
until one fills its high word) is the bearing.

Retail behaviour worth knowing, reproduced:

- **`+0x70` restores every register, `eax` included, before it returns.** Its
  result is the caller's own `eax`, not an output; only the two pointers and
  `.data+0x72e0` carry anything out. The harness does not compare the return.
- **The delta's y-term is only ever squared.** Its sign reaches nothing
  observable -- the bearing is taken from x and z alone -- which is why the
  `y-neg` break is a proven equivalence, not a miss.

### Verifying the range and bearing

`make maindiff-run SUITE=nav`: 20,000 scenarios, 0 differences, over random
position pairs (the same point now and then, the adventure sentinel in the
field now and then) with the three UTIL geometry services as **logging
models**. UTIL's own geometry is verified in UTIL; the models return a value
that varies with their inputs and log every argument, so what this suite
compares is MAIN's glue -- the 48-bit subtractions, the squares, the magnitude
and the normalising shift -- not UTIL. The models preserve the registers the
retail assembly keeps live across their calls: `nav_delta` holds the `out`
pointer in `ecx` across UTIL `+0x48`, so the `util_geo48` model (alone among
the three) is entered through an assembly trampoline that saves and restores
`ecx` and `edx`, the harness rule that a stub must leave the registers the
real callee leaves (`maindiff.c`).

`make main-breaks SUITE=nav`: 14 breaks, 13 caught, 1 proven equivalent
(`y-neg`, above).

## Slice 4: the nav read-outs

`src/main/main_nav.inc`, `.text+0x19c94` (`+0x78`) and `.text+0x19f10`
(`+0x80`), assembly. Two `(which, &out)` demultiplexers that hand back values
the nav machine left in `.data`. `+0x78` (reached from the host) copies the
word at `.data+0x7482` for `which` 1 or `+0x7484` for 2, and answers -1 for
anything else. `+0x80` (ADVDRV, PANELS) has four cases: 1 and 2 first refresh
`.data+0x72e8` and `+0x72ec` through MULTIP `+0x38` and `+0x34` (and answer -1
if MULTIP fails), 3 and 4 just read `+0x72f0` and `+0x72f4`; `which` 0 or above
4 writes nothing and answers 0 (`dec`/`cmp`/`ja` to the no-op tail, then a jump
table).

### Verifying the read-outs

`make maindiff-run SUITE=navio`: 20,000 scenarios, 0 differences. Each op seeds
the six `.data` words afresh and calls one service with a `which` spanning the
real cases and the edges on either side, so a wrong offset or case shows at
once. MULTIP's two accessors are **logging models** (MULTIP is verified
elsewhere): each fills the word it is handed and returns a status that varies
with what was there, so both the taken and the failed path of `+0x80` are
exercised. `make main-breaks SUITE=navio`: 12 breaks, all caught.

## Slice 5: the station descriptor

`src/main/main_nav.inc`, `.text+0x19cd5` (`+0x7c`), assembly.
`+0x7c(which, arg2, &out)` looks a station up through FE `+0x2c` into a local
buffer, formats it with UTIL `+0x88` through a `.text` descriptor
`{3, 3, 6, &.data+0x7248}` into `.data+0x7248`, and hands the 0x58-byte result
back with a HOST memmove; a failed lookup zeroes `out` and answers -1.

Two retail details, reproduced:

- **FE `+0x2c`'s first argument is pushed as a word over two stale bytes**, so
  only its low word reaches FE (as MAIN `+0x74`'s does, slice 2).
- **The descriptor's fourth word is a relocation into `.data`.** Retail reads
  it from `.text` already fixed to its own base; ours builds the descriptor
  with its own `.data` address (`V(0x20807248)`) rather than reading retail's
  unrelocated pointer out of the generated `.text` tail.

### Verifying the station descriptor

`make maindiff-run SUITE=navdesc`: 20,000 scenarios, 0 differences. FE `+0x2c`
and UTIL `+0x88` are **logging models** (both verified in their own modules):
FE `+0x2c` fills the buffer deterministically and fails one lookup in five;
UTIL `+0x88` writes exactly the bytes `+0x7c` copies out and logs a hash of
them, not the per-side `.data` address it was handed. The output lands in the
harness's fixture memory so the memmove is over memory both sides share.
`make main-breaks SUITE=navdesc`: 9 breaks, all caught.

## Slice 6: the season, the zone and the clock

`src/main/main_season.inc`, `.text+0x18781` (`+0x74`) with its helpers
`.text+0x18394` (the position snapshot) and `.text+0x18994` (minutes into
hours and minutes), assembly. Loading calls it; ADVDRV and MULTIP too.

It fixes the time-zone offset from the longitude (`(lon >> 16) / 0x555`, past
the date line wrapped, halved and scaled to minutes), then lets FE's nearest
`d' records override it by priority, then trims it by a daylight rule whose
season window is a five-entry `.text` table at `+0x18eb0`. Finally it converts
between local and zulu time -- with its first argument zero and a `d' record
ever seen, local drives zulu; otherwise zulu drives local.

Retail details, reproduced:

- **The minutes-to-hours divide runs into overflow and relies on the host's
  repair.** `div bl` on a count that will not fit after one wrap raises a
  divide fault that retail's host (`0x4119a0`) catches and saturates to all
  ones; ours models the saturated value in C, and maindiff stands in for the
  host's repair (see below).
- **The daylight rule indexes a five-entry table**; retail bounds the index at
  5. A rule of 6 would read a sixth entry that is an empty window (start 3, end
  0), so it changes nothing -- which is why the `dst-bound` break is a proven
  equivalence, not a miss.
- **FE `+0x20`'s state argument is a word pushed over two stale bytes** (as
  `+0x74`'s own day argument and FE `+0x2c`'s index are), so only its low word
  counts.

### Verifying the season, zone and clock

`make maindiff-run SUITE=season`: 20,000 scenarios, 0 differences. The suite
drives `+0x74` directly through the export `main_season`, past the seam the
situations suite still uses, over random position, clock and day inputs. FE
`+0x54` (which sets the enumeration going) and FE `+0x20` (which hands back a
deterministic run of records, some of them `d' records) are **logging models**,
their records in shared fixture memory so a record pointer is the same on both
sides. The harness models the **host's integer-divide repair** (`repair_divide`
in `maindiff.c`): retail's `div` into overflow is caught, saturated to all ones
and stepped over, exactly as the host's `__try` filter does for the modules it
calls (docs/modules/host.md, window.md). `make main-breaks SUITE=season`: 24
breaks, 23 caught, 1 proven equivalent (`dst-bound`, above).

## Slice 7: the clock as digits

`src/main/main_season.inc`, `.text+0x16ddf` (`+0xa4`), assembly. A leaf with
no arguments and no module calls: the three bytes of the zulu clock at GLOBAL
+0x238 (hours), +0x239 (minutes) and +0x23a (seconds) each become a
two-character decimal at +0xc20, +0xc23 and +0xc26. Retail uses `aam 0xa` to
split a byte into tens and units and `xchg`/`add 0x3030` to lay them down as
ASCII, low character first; a byte past 99 keeps retail's result, which the
transliteration reproduces with plain `/` and `%`.

### Verifying the clock

`make maindiff-run SUITE=clock`: 20,000 scenarios, 0 differences, driving
`+0xa4` directly through the export `main_clock` over clock bytes that are
usually in range and now and then past it. `make main-breaks SUITE=clock`:
6 breaks, all caught.

## Slice 8: the view cycler

`src/main/main_nav.inc`, `.text+0x18f0` (`+0xc8`) with its one-line helper
`.text+0x18c0`, assembly. `+0xc8()` steps the view index: from the current
index at `.data+0x7fc8` when the current view (the record `.data+0x7fc4`
points at) is active -- the sign bit of its `+8` field -- or from -1
otherwise; one on, and once it reaches the count at `.data+0xfda4` it wraps
back to -1. It posts the result to the host: a real index as itself plus
0x300, a -1 as host command 0x1625.

### Verifying the view cycler

`make maindiff-run SUITE=view`: 20,000 scenarios, 0 differences. Each op points
`.data+0x7fc4` at a view record in shared fixture memory (its `+8` field, and
so the active bit, set at random), seeds the index and a small count so the
wrap is reached often, and calls `+0xc8` through its Linkage slot; the posted
command is the host stub's log. `make main-breaks SUITE=view`: 10 breaks, all
caught.

## Slice 9: the analysis menu greying

`src/main/main_nav.inc`, `.text+0x16920` (`+0x30`) and `.text+0x169a0`
(`+0x34`), assembly. Two leaves REALITY calls: `+0x30` greys the six analysis
menu items `0x1001` through `0x1006` with six HOST `+0x254(id, 2)` calls, and
`+0x34` enables them (state 0). Nothing else (retail unrolls the six; the
transliteration loops, the same six calls).

### Verifying the menu greying

`make maindiff-run SUITE=menu`: 20,000 scenarios, 0 differences -- the six host
calls of whichever of the two is picked, logged by the host stub, match.
`make main-breaks SUITE=menu`: 6 breaks, all caught.

## Slice 10: an object slot release

`src/main/main_nav.inc`, `.text+0x16570` (`+0x8c`), assembly. `+0x8c(&handle)`
releases one entry of the object table at `.data+0x9794`, whose entries are
0x2d4 bytes (181 dwords) apart, indexed by the handle's low byte: if the
entry's first dword is set, both it and the caller's handle are zeroed. Like
the other assembly leaves it leaves the index (`handle & 0xff` times 181) in
`eax`, which the transliteration returns. (`+0x84` allocates into the same
table and `+0x88` iterates it; both remain to do.)

### Verifying the object slot release

`make maindiff-run SUITE=obj`: 20,000 scenarios, 0 differences. Each op seeds a
small handle (so the entry stays inside the compared image) and the entry's
first dword, set or clear, and checks the entry and the handle after. `make
main-breaks SUITE=obj`: 6 breaks, all caught.

## Slice 11: the latitude clamp

`src/main/main_nav.inc`, `.text+0xc520` (`+0x1c`), assembly. FACIL calls it:
it clamps the latitude dword at GLOBAL +0x564 into `[0xff683bb7, 0x97c449]`
(signed), and leaves GLOBAL's address in `eax` as retail does.

### Verifying the latitude clamp

`make maindiff-run SUITE=clamp`: 20,000 scenarios, 0 differences, over latitudes
clustered on the two bounds and their neighbours. `make main-breaks SUITE=clamp`:
6 breaks, 5 caught, 1 proven equivalent (`max-op`: `>` versus `>=` at exactly
the maximum both leave the maximum).

## Slice 12: the sun direction and intensity

`src/main/main_sun.inc`, `.text+0x830d` (`+0xa0`) with its helper `.text+0x9097`,
assembly. It builds a unit sun vector into GLOBAL (`+0xb2a` x, `+0xb2e` y,
`+0xb32` z) from the time of day (GLOBAL `+0x23b`/`+0x23c`), the day of the year
(`+0x23e`) and the latitude (`+0x564`), through four of UTIL `+0x2c`'s
sine/cosine pairs, and then the helper turns the vertical component into a light
intensity (`+0xcba`) and a sky-colour band (`+0x116e`, `+0x12a0`). The flags
`+0xb34` and `+0x116e` are primed to 1 and cleared or raised as the geometry
decides; a sun at or above the horizon zeroes the vector, and a gate word under
`0x1000` forces it straight down. The arithmetic is 16-bit signed Q15 fixed
point (`q15`: `imul`, double, keep the high word).

- **Several divides run into overflow and lean on the host's divide filter.**
  The minutes-of-day and day-of-year `div`s saturate to all ones; the latitude
  `idiv bx` (by 305) and the helper's `idiv bx` (by 6000 or 14000) overflow for
  a mid-range sun and saturate to `0x7fff`, or `0x8001` when the divisor and the
  dividend's top half differ in sign. This is the first MAIN slice to divide
  *signed* into overflow, so it pins down the `idiv` half of `repair_divide`
  (maindiff.c), which had stood unverified since WINDOW; ours computes each
  repaired quotient in C and never traps.
- **UTIL `+0x2c` is modelled, not reproduced.** The real trig is verified in
  UTIL; the harness drives this slice against a deterministic sine/cosine model
  (`util_2c`), identical on both sides, so only MAIN's fixed-point arithmetic
  and glue are under test. The angle is pushed over two stale bytes, so the
  model reads its low word only.
- **`eax` at exit is leftover, not a result.** The routine does not restore it
  and the one caller ignores it, so the suite does not compare a return value.

### Verifying the sun direction

`make maindiff-run SUITE=sun`: 20,000 scenarios, 0 differences, over a random
time, day, latitude (now and then large enough that the latitude `idiv`
overflows) and gate (straddling `0x1000`). `make main-breaks SUITE=sun`: 14
breaks, 12 caught, 2 proven equivalent -- `light-zero-op` (`>` versus `>=`
where the subtraction already left zero) and `idiv-sat` (both `idiv` sites
discard the quotient's low bits, so the low-order saturation value cannot be
told apart, while the sign still shows and `idiv-neg` catches it).

## Slice 13: allocate an object slot

`src/main/main_nav.inc`, `.text+0x16100` (`+0x84`), assembly. `(tmpl, kind,
&handle)`: a sibling of the slice-10 release, over the same 0x2d4-byte table at
`.data+0x9794`. If `*handle` already carries the `0xf0000` flag and its entry
is still active, nothing is done and 0 is returned. Otherwise the first entry
whose active word is zero is taken: `kind` (0 or 1, anything else `-1`) picks
four field words; the host allocates a 0xaa-byte buffer filled from a `.data`
template at `+0x6240`, a 0xc-byte sub-template chosen by `kind` through the
pointer table at `+0x62f0` is copied in, and fourteen more buffers are each a
copy of the first; nine dwords of `tmpl` go into the entry, it is marked
active, and `*handle` becomes `index | 0xf0000`. Returns 0, or `-1` when all
twenty entries are taken.

The host's allocator and memmove are verified elsewhere; the harness's models
hand back pointers into its shared arena, the same address on both sides, so
the pointers the table ends up holding compare equal. The slot's active word
is only ever 0 or 1, so the fast path's `== 1` and a bare `!= 0` are
indistinguishable (the `fast-eq` break).

### Verifying the allocation

`make maindiff-run SUITE=objalloc`: 20,000 scenarios, 0 differences, over a
random template, kind (including invalid), and handle (fresh, already
allocated with any nonzero subset of the flag bits over an in-range index, or
random), with entries now and then pre-marked to move the free slot. `make
main-breaks SUITE=objalloc`: 15 breaks, 14 caught, 1 proven equivalent
(`fast-eq`).

## Slice 14: the eye's reticle

`src/main/main_eye.inc`, `.text+0x16a0` (`+0xc0`), assembly. `(view)`: the view
record's `+0x1c` points to a box `{extent, lo, _, hi, done, guard, guard}`.
Unless the box is already done (`+0x10 == 1`) or either guard (`+0x14`,
`+0x18`) is set, GLOBAL `+0x2ec` is stamped `0xf00f` and, by the reticle style
in GLOBAL `+0x600`, HG2D draws the segments of a frame (style 1, four `+0x1c`
segments) or a cross (styles 2 and 3, four `+0x34` segments), each coordinate
scaled from the extent (`>>6`, `>>5`, `>>4`). Styles 0 and above 3 draw
nothing.

HG2D (module id 13) is verified elsewhere; the eye only ever hands it
coordinates and never reads anything back, so the harness logs the segments it
is handed (the view context by what it points at) and compares those along
with the stamp. `eax` at exit is leftover and the callers ignore it. The box's
done flag is only ever 0 or 1, so its `== 1` and a bare `!= 0` gate on the same
boxes (the `done-eq` break).

### Verifying the reticle

`make maindiff-run SUITE=eye`: 20,000 scenarios, 0 differences, over a random
box and style (now and then done or guarded, so the early return shows). `make
main-breaks SUITE=eye`: 13 breaks, 12 caught, 1 proven equivalent (`done-eq`).

## Slice 15: the altitude band and its 3D submission

`src/main/main_band.inc`, `.text+0x6de9` (`+0x9c`) with its tail helper
`.text+0x7200`, assembly. The service classifies the aircraft's reference
altitude (GLOBAL `+0x5c4`, a signed word) into one of five bands and leaves the
index 0..4 in GLOBAL `+0xe86`, with a derived word in the scratch `.data+0x1a76`.
The band edges are two centre-half-width pairs (`+0xe6a`/`+0xe6c` and
`+0xe72`/`+0xe74`) laid out as centre-minus-half-width in `.data+0x1a94` and
`+0xaac`; which edges apply, and whether a middle band interpolates (an unsigned
16x16 multiply whose high word is taken), is chosen by the `+0xe78` flag. A gate
word `+0xe70` of zero drops straight to band 4. The helper then stages a run of
draw commands: it patches a handful of static command blocks in `.data` from
GLOBAL fields and hands each, in turn, to G3D `+0x1c` once and `+0x24` one to
three times.

- **This is the first MAIN slice to drive G3D (module id 14).** The draws are
  fire-and-forget -- their results are never read -- so the harness logs the
  command block each is handed (by image offset, identical on both sides) and
  only MAIN's own classification arithmetic, its `.data` patches and the order
  of the draws are under test. G3D is verified in its own harness.
- **One scratch word holds a run-time image pointer.** `.data+0x1a72` stages the
  block pointer for G3D; it is not a relocation, so put_state normalises it like
  one (keeping the offset, so a wrong block still shows), alongside the
  situation reader's run-time pointers.
- **The stack-alignment dance and the `+0x3a8` save/restore are left out.** The
  hand-rolled alignment around each call and the push/pop of GLOBAL `+0x3a8`
  across the first call (which leaves it unchanged) have no observable effect.

### Verifying the band

`make maindiff-run SUITE=band`: 20,000 scenarios, 0 differences, over random
centre/width pairs, flags and gate, with the altitude now and then snapped onto
a computed band edge (and its neighbours) so the comparison boundaries are
exercised exactly. `make main-breaks SUITE=band`: 33 breaks, 33 caught.
