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
| 16, the smoke trails | `+0x88` (MSVC C, `.text+0x162f0`..`+0x16567`) | `objstep` | verified: 20,000 scenarios, 0 differ; 32 of 34 breaks caught, 2 proven equivalent |
| 17, the quick save | `+0xb0` (MSVC C, `.text+0xcdb0`) | `quicksave` | verified: 20,000 scenarios, 0 differ; 6 of 7 breaks caught, 1 proven equivalent |
| 18, the preferences | `+0xb4`, `+0xb8`, `+0xbc` (MSVC C, `.text+0xdd70`..`+0xf2e0`, preset `+0x14d20`) | `prefs` | verified: 20,000 scenarios, 0 differ; 26 of 28 breaks caught, 2 proven equivalent |
| 19, the cloud layers' draw | `+0x98` (assembly, `.text+0x6f59`, `+0x741f`..`+0x77c6`, `+0x8275`, `+0x9146`..`+0x922c`, `+0x9d57`..`+0xa089`) | `clouds` | verified: @CL@ |
| 20, the cloud layers' textures | `+0x94` (assembly, `.text+0x7140`, `+0x77c7`..`+0x7bc1`) | `cloudtex` | verified: 20,000 scenarios, 0 differ; 33 of 34 breaks caught, 1 proven equivalent |
| 21, the spot plane's eye | `+0xa8` (assembly, `.text+0x6450`..`+0x6ce2`) | `spot` | verified: 20,000 scenarios, 0 differ; 47 of 50 breaks caught, 3 proven equivalent |

**What is left** (as of slice 21): `+0x04`/`+0x08` (init and deinit: they
register and remove three CHAIN callbacks, `.text+0x2e10` on list `0x14` -- an
empty `ret 8` -- and the frame callbacks `.text+0x7095` on list 1, the cloud
puff field through UTIL `+0x44`/`+0x6c`, and `.text+0x85ea` on list 3, through
FE's grid conversions `+0x60`..`+0x6c`; so the callbacks come first), `+0x18`,
`+0x20` (the frame's order of business), `+0x24`..`+0x2c`, `+0x50`, `+0x90`,
`+0xac` and `+0xc4` (the Flights dialogs: UTIL `+0x98` lists the flights
through MAIN callbacks and host `+0x10c` runs a dialog that calls back into
MAIN, as FACIL's were done), and the menu command handlers the host reaches
through the command table `+0x04` registers (most open dialogs through host
`+0x10c`). UTIL's `+0x6c` keeps its seed in UTIL's `.data`, so a suite over
retail UTIL that reaches it must reset the seed for each side.

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
| `+0x84`..`+0x90` | `.text+0x16100`, `+0x162f0`, `+0x16570`, `+0x10560` | CONTROLS, WINDOW (`+0x88`) | object table at `.data+0x9794`, 0x2d4-byte entries: `+0x84` **slice 13** allocates, `+0x88` **slice 16** steps them all (the smoke trails), `+0x8c` **slice 10** releases |
| `+0x94`..`+0xa0` | `.text+0x7140`, `+0x6f59`, `+0x6de9`, `+0x830d` | WEATHERM (`+0x94`), WINDOW (`+0x98`, `+0x9c`) | `+0x94` is **slice 20** (the cloud layers' textures), `+0x98` is **slice 19** (the cloud layers' draw), `+0x9c` is **slice 15** (the altitude band), `+0xa0` is **slice 12**: the sun direction |
| `+0xa4`, `+0xa8` | `.text+0x16ddf`, `+0x6450` | ADVDRV, CONTROLS, WINDOW | `+0xa4` is **slice 7**, `+0xa8` is **slice 21** (the spot plane's eye) |
| `+0xac`..`+0xbc` | `.text+0xc870`, `+0xcdb0`, `+0xdd70`, `+0xe5a0`, `+0xed10` | CONTROLS (`+0xac`), FACIL (`+0xb0`) | `+0xb0` is **slice 17** (the quick save); `+0xb4`, `+0xb8`, `+0xbc` are **slice 18** (load, save and default the preferences) |
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
`eax`, which the transliteration returns. (`+0x84`, slice 13, allocates into
the same table and `+0x88`, slice 16, steps it.)

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

## Slice 16: the smoke trails

`src/main/main_nav.inc`, `.text+0x162f0` (`+0x88`), MSVC C. WINDOW calls it
once a frame with no arguments. It walks all twenty entries of the slice-10/13
object table at `.data+0x9794`; each active one (any non-zero first dword)
keeps a run of 40-byte records at `+0x14` and as many buffer pointers at
`+0x26c`, `n` of them (`+0xc`, which `+0x84` makes 15), `used` of them live
(`+0x10`). It is the smoke system: CONTROLS' `SMOKE_TOGGLE` allocates an
entry (`+0x84`, kind 1) whose template is GLOBAL `+0x560`, the aircraft's
position, and releases it with `+0x8c`. So each entry is a trail of puffs that
grows by one each frame to `n` and then scrolls:

- both runs shift up by one (MSVCRT `memmove`, through MAIN's own import as
  retail calls it, not the host's `+0x21c`), the last buffer coming round to
  the front, and `used` grows unless it is already `n`;
- the entry's nine-dword template copy (`-0x28`) becomes the newest record's
  `+4`..`+0x24`;
- the longitude difference from the aircraft (GLOBAL `+0x5d0` less that
  record's `+0x24`) picks a wrap: within the quarter turns either side of the
  half turn (`0x15540000`..`0x6aa40000` and `0x954c0000`..`0xea9c0000`,
  exclusive) the side word is the record's `+0x1c` moved by `0x3ffc0000` one
  way or the other; otherwise it is 0;
- each live record is restamped from the aircraft (GLOBAL `+0x5d0` into
  `+0x24`, `+0x5c8` less `0x7ff80000` into `+0x1c`, the side word into
  `+0x20`), aged (`+0x18` grows by `i/13`, a multiply by `0x4ec4ec4f`), and
  every one after the first xor-toggled by the entry's words `+4`/`+6` when
  GLOBAL `+0x250` bit 0 is set; eight words at its buffer's `+0x42` (held in
  `.data+0xd140`) are laid out from a width that steps by the entry's `+8`
  word / 4 per record, and the buffer and record go to G3D `+0x38`;
- between records the template copy is refreshed from the template itself
  (the pointer at `-0x2c`).

Retail details, reproduced:

- **A side word that wraps to exactly 0 counts as no wrap.** The width is
  taken from the running width whenever the side word is 0, whichever branch
  made it.
- **One stack local is written as a word on two paths and as a dword on the
  third**, and read both ways; only its low word ever reaches memory.
- **`eax` at exit is what the last entry left there**: its active word, its
  live count, or the index the loop ended on. WINDOW ignores it; ours
  returns it anyway, and the suite compares it.
- **The step trusts `n`.** At 0 the record shift is 4 GB; past 15 it runs
  into the buffer pointers. `+0x84` only ever makes 15, so the suite keeps
  `n` in 1..15.

### Verifying the step

`make maindiff-run SUITE=objstep`: 20,000 scenarios, 0 differences (about 4
minutes), with 2,296,860 active entries stepped and 18,540,797 submissions, 574,555
of them on a wrap edge and 190,796 with a side word wrapped to 0. Each op
re-seeds the twenty entries now and then (inactive, active with 1 or a
stray even word, `n` 1..15 and mostly 15, `used` 0..`n`, the records,
template and template copy random, the buffers in the shared arena), and
otherwise lets the trails run on from where the last step left them. Before
every step it aims entries' longitude differences onto the four wrap edges
(and one either side) and their side words onto the values that wrap to 0.
G3D `+0x38` is a **logging model** that records both pointers, the eight
words laid out at the buffer's `+0x42` and a digest of the record (G3D is
verified in its own harness); the image, arena and GLOBAL are compared after
every op. `make main-breaks SUITE=objstep`: 34 breaks, 32
caught, 2 proven equivalent: `s-high` (only the running width's low word is
ever stored, and the low word of a sum depends only on the low words) and
`width-else` (the branch that sets the width from `+8` also makes the side word
0, so every record takes the width from the running one before reading it).


## Slice 17: the quick save

`src/main/main_flights.inc`, `.text+0xcdb0` (`+0xb0`), MSVC C. FACIL calls
it. It saves the flight through slice 2's `+0x40`, called directly as retail
calls it, with a title from string `0x2201` and the description at
`.data+0x7fd0`; the save's result is the answer. The title buffer starts as
the 17 bytes at `.data+0x3634`, `"Temporary Flight"` and its terminator, and
zeros to 0x104 bytes, so a string the host cannot load leaves that title.

### Verifying the quick save

The `quicksave` suite is a `situations` scenario (slice 2's model of the
host's files, its loads and its walk-off guard) in which a third of the ops
are `+0xb0`, with string `0x2201` absent, `"Temporary Flight"`, short, or
long enough to fill the buffer (the path join then refuses it). What it saves,
`FLIGHTS\` and the title as the save copied it to `.data+0x441f`, joins the
files the loads pick from. `make maindiff-run SUITE=quicksave`: 20,000 scenarios, 0 differences (about 20 minutes, most of it slice 2's hangs), with 80,599 quick
saves (19,853 with the string absent; 62,659 saved, 51,916 of them found and
loaded back). `make main-breaks SUITE=quicksave`: 7 breaks, 6 caught, 1 proven
equivalent (`zero-rest`: the default carries its own terminator and a loaded
string ends in one, and nothing reads past it).

## Slice 18: the preferences

`src/main/main_prefs.inc`, `.text+0xdd70` (`+0xb4`, load), `+0xe5a0` (`+0xb8`,
save) and `+0xed10` (`+0xbc(mode)`, the defaults), MSVC C, with four helpers
(`+0xe520`/`+0xe560` read a word or byte field, `+0xf290`/`+0xf2c0` write
one) and `.text+0x14d20`, the performance-mode preset. All three are straight
runs of host profile calls on `[PREFS]` in FLTSIM98.CFG (`.data+0x364c`), a
key at a time (the keys are `.data` strings), each into or out of a GLOBAL
field: 62 calls to load, 60 to save, 58 to write the defaults. The runs were
transcribed from the disassembly by script, call for call.

- **Load** reads each dword field through host `+0xa8` (the field is the
  default) and each word or byte field through a helper that reads the field
  zero-extended as the default and stores the low part back only when `+0xa8`
  answers 0 -- which it always does. Three string fields (GLOBAL `+0x12c`,
  `+0x24`, the default flight, and `+0x638`) come through `+0xc8` at 0x104
  bytes. Then the performance mode's preset is applied and host `+0x288` is
  told whether GLOBAL `+0x1220` is clear.
- **The preset** (`.text+0x14d20(mode)`) copies sixteen fields into GLOBAL
  from a table of 0x80-byte entries at `.data+0x5740` (a 0x40-byte name, string
  `0x2300 + n`, then the settings). Mode 0 applies nothing. **The mode is not
  bounded**: `PERFORMANCE_MODE` past the twelve entries, or negative, reads on
  through `.data` (reproduced).
- **Save** writes the same fields back, word and byte fields masked to their
  width (retail pushes a register whose other half is stale).
- **The defaults** write fixed values, `PERFORMANCE_MODE` and `DEF_PERF_MODE`
  as `mode`, five keys whose values depend on `mode` above 4 (signed), six
  `[AIRtoCONTAINER]` strings (the default aircraft, the Cessna Skylane 182RG)
  and `[APL] CACHE_WAV_FILES=1`.

### Verifying the preferences

maindiff models the host's profile services over an in-memory CFG as the
verified host performs them through `Get`/`WritePrivateProfile*A`: `+0xa8`
reads an int as `GetPrivateProfileIntA` does (the old value is the default; a
present value is blanks, a sign and digits) and always answers 0, `+0xac`
writes `"%d"`, `+0xc8` reads a string with the buffer as its default and at
most `size - 1` characters, and `+0xcc` writes one. Every call is logged with
its section, key and value, and the CFG's digest after every op. The `prefs`
suite loads, saves and writes the defaults over a random GLOBAL, between ops
changing CFG values (huge, negative, empty, digits with a tail, text) or
GLOBAL fields. Before every load it steers the performance mode onto one whose
preset block lies in the image and holds no relocated pointer (one would reach
GLOBAL as each side's own address): mostly 0 to 11, sometimes past the table
or negative. `make maindiff-run SUITE=prefs`: 20,000 scenarios, 0 differences (about 5 minutes): 99,089 loads, 65,813 saves, 66,118 defaults (33,092 with a mode
above 4), 93,024 presets applied (27,655 past the table, 4,198 negative).
`make main-breaks SUITE=prefs`: 28 breaks, 26 caught, 2 proven equivalent
(`word-mask` and `byte-mask`: every caller passes a field already read at its
width, so the helpers' masks, which clear a register's stale half in retail,
have nothing to clear in C).

## Slice 19: the cloud layers' draw

`src/main/main_clouds.inc`, `.text+0x6f59` (`+0x98(which)`), assembly, with
the routines it reaches: `+0x741f` (the layers), `+0x7744`/`+0x7777` (the
layer block's submit), `+0x8275` (the puffs), `+0x9146`/`+0x918b` (GLOBAL
`+0xf10`), `+0x9d57` (the layer's texel under the aircraft) and
`+0x9f39`/`+0xa04b` (the layer's vertical bands). WINDOW calls it.

- **`which` 1 and above 3** copy the aircraft's position into the first
  layer's command block (`.data+0x1aca` on, with slice 15's band edge at
  `+0x1a92`/`+0x1a94`) and submit block `+0x2368` (1) or `+0x238c` to G3D
  `+0x24`; **2 and 3** do the same for the second layer (`+0x1ae2` on,
  `+0x1aaa`/`+0x1aac`; blocks `+0x23b0`, `+0x23d4`).
- **`which` 0**, unless GLOBAL `+0xdce` is set, submits the puffs (GLOBAL
  `+0xe7c`/0x1000 rounded of them, at most 16, each a pair of blocks from the
  table at `.data+0x208e` to G3D `+0x38`), then works out whether the aircraft
  is inside a layer, near one or clear: from slice 15's band edges (within 1500
  either way sets `.data+0x1ff5`/`+0x1ff7`), and from each layer's texel under
  the aircraft, looked up in an HG2D texel map through UTIL `+0x48`, against a
  threshold HG2D `+0x88` hands back. That picks the layer block's colour
  (`.data+0x22cc`, from a 32-entry table at `.data+0x1ff9` indexed by GLOBAL
  `+0x2a8`), submits it, and sets GLOBAL `+0xf10`. It always clears GLOBAL
  `+0xc90`.

Retail details, reproduced:

- **`+0x9f39` takes `shl ax,1` / `rol dx,1` after an `imul`**: dx's own top
  bit comes round, not ax's.
- **`+0x9d57`'s two 32-bit divides run into overflow** for an aircraft far
  from the layer's origin and lean on the host's divide repair. This is the
  first MAIN slice with a repaired 32-bit `idiv` (`idiv32` in
  `main_clouds.inc`: `0x7fffffff`, or `0x80000001` when the divisor and the
  dividend's high half differ in sign, as maindiff's `repair_divide` does).
  Only the quotients' bits 8 to 31 are used.
- **The texel index is two quotient bytes less the layer's origin bytes**, as
  bh:bl, each byte wrapping on its own.
- **`+0x918b`'s second-layer test reads the first layer's `+0xe6e`**, not
  `+0xe76`, and compares the position the first test stored.
- **Two run-time image pointers**, `.data+0x3470` (the layer record) and
  `+0x35da` (unaligned, the layer block), join put_state's list as slice 15's
  `+0x1a72` did.

The stack-alignment shims around each call are left out, as in slice 15.

### Verifying the cloud layers

G3D, HG2D and UTIL are verified in their own harnesses. maindiff's stubs log
what each is handed (a block of MAIN's own by its image offset alone, since
it holds relocated pointers the image comparison already covers) and hand back
what the routines read: HG2D `+0x88`'s threshold byte, `+0xb4`'s 64 KB texel
map in fixtures, and slice 3's UTIL `+0x48` model. The `clouds` suite seeds
GLOBAL's layer fields (slice 15's centre/width pairs and flags, the altitude
now and then on or beside a band edge, or 1500, `0x34` or 6 from one), the band
edges as slice 15 leaves them or not, the layer records with the aircraft
usually near their origin (now and then far, so the divides saturate), the
view kind behind GLOBAL `+0x1198`, and calls `+0x98` with `which` 0 to 5.
`make maindiff-run SUITE=clouds`: @CLLONG@

## Slice 20: the cloud layers' textures

`src/main/main_clouds.inc`, `.text+0x7140` (`+0x94`) and its texture maker
`.text+0x77c7`, assembly. WEATHERM calls it. When GLOBAL `+0x398` is set,
each layer whose coverage's high byte (GLOBAL `+0xe6e`, `+0xe76`) differs from
the last made (`.data+0x1eda`, `+0x1edc`) gets its texture remade, and the new
one is kept only if it was written. Then the haze's cloud base, GLOBAL
`+0xcd4`: the first layer's `+0xe6a`, at most `0x2bc` once GLOBAL `+0xe8a`
reaches `0x7d00`, or 0 with no first layer.

The maker submits the layer's block to G3D `+0x24`, then, when GLOBAL `+0xcb6`
is set, reads `<string 0x9006>\clouds` (extension 4, 0x10000 bytes) into the
texel map HG2D `+0xb4` hands out. It remaps every texel by a curve the
coverage's high byte picks (`0xed` times it, the product's high byte, a
shoulder below `0x80` or a knee above, the steepest past `0xe0`), shifted and
offset by the coverage band (`.data+0x1ff3`/`+0x1ff4`, from `0xe001` up the
second set). It then clears `cloudN`'s read-only flag (host `+0x134`/`+0x138`)
and writes the map there. It answers retail's carry.

Retail behaviour, reproduced:

- **The read's open is never checked**: with no `clouds` file the host's read
  and close of a null file fail quietly, and the map is remapped as it was.
- **The shoulder's `div bl` runs into overflow** at a texel just inside the
  curve, or divides by 0 at the lowest coverages, and leans on the host's
  divide repair (`0xff`).
- **A failed write leaves edi on the texel map.** The maker puts GLOBAL back in
  edi only on the way to the end or before its early return; when the write's
  open (or the write) fails it returns with edi still on the map, and `+0x94`
  goes on reading GLOBAL's fields through edi: the second layer's flags and
  coverage, the haze inputs, and the store of `+0xcd4` all go to the map
  instead. `clouds_texture` hands its edi back for that.
- **A failed write leaves the file open.**

### Verifying the textures

maindiff's file model grows: files hold up to 64 KB (the cap where a write
runs out of disk stays per suite, so the situation suites' streams are
unchanged), a file has the host's attribute flags (`+0x134`/`+0x138`), and a
read-only file will not open for writing, as `CreateFileA` would refuse it.
The `cloudtex` suite has `TEXTURE\clouds` there (whole, short or empty) or
not, `cloud1`/`cloud2` there or not and now and then read-only. Each op seeds
the coverages across the curve's turns, the flags and the last-made ids. The
texel map is in fixtures, and its digest goes in the transcript after every op.
`make maindiff-run SUITE=cloudtex`: 20,000 scenarios, 0 differences (about 5 minutes): 149,622 calls over all three curves, 27,486 ops whose write's open was
refused (edi left on the map). `make main-breaks SUITE=cloudtex`: 34 breaks,
33 caught, 1 proven equivalent (`edi-early`: the early return needs GLOBAL
`+0xcb6` clear, read from GLOBAL each call, so every earlier call in the op
returned early too and the caller's edi is still GLOBAL). The breaks found one
gap, now closed: the suite seeded `+0xcb6` only as all-or-odd, so a read one
byte lower went unseen.

## Slice 21: the spot plane's eye

`src/main/main_spot.inc`, `.text+0x6450` (`+0xa8`) with `+0x669a`..`+0x6ce2`,
assembly. WINDOW, CONTROLS and ADVDRV call it. In the spot view (GLOBAL
`+0x1198`'s view record of kind 0 or 1, with `+0x10` and `+0x18` clear) it
places the eye, a 48-bit position at `.data+0x1868`, behind the aircraft it
follows. The aircraft is chosen by GLOBAL `+0xe64` (mode 2, the first or, with
`+0xe66`, the second of GLOBAL `+0x384`'s pair; mode 4, the first, through
UTIL `+0x60`'s unpack). The eye is stepped by UTIL's 48-bit vector services
and measured by G3D `+0x1c`, which leaves its distance in GLOBAL `+0xd4c`
(GLOBAL `+0xe88` held at `0x7fff` across each call):

- when the followed aircraft has moved, or GLOBAL `+0xe62` bit 0 asks, the
  step is worked out afresh from the view's direction (UTIL `+0x2c`, the
  metres turned to longitude by `+0x44`) and the eye walked along it, forward
  and then (if the first step forward did not help) back, while the distance
  falls; with `+0xe62` bit 1, a slewed transition is armed;
- each call steps the eye out along the step 3, 7 or 15 times (GLOBAL `+0xe60`
  0, 1 or 2) and back, and walks it again if the distance said so; an armed
  transition slews in fifteen steps while the distance is past `0x40000`.

Retail details:

- **The step count for GLOBAL `+0xe60` past 2 is G3D's leftover ecx**:
  retail's `loop` counts with whatever G3D `+0x1c` left there. The game sets
  only 0 to 2. *Not reproduced*: the transliteration takes no steps there, and
  the suite keeps `+0xe60` in range.
- **The height term in mode 2 is an unsigned 32x32 product** whose high dword
  becomes the count kept in `.data+0x1924`; mode 4's is a signed product's low
  dword shifted down a byte.
- **The slewed transition raises `.data+0x1888` by a quarter turn and puts it
  back**; nothing reads it in between.
- **The angle handed to UTIL `+0x2c` is a word over two stale bytes**, as in
  slice 12.

### Verifying the spot plane

The walk's flow turns on what UTIL computes, so **this suite runs retail UTIL**
(`use_retail_util` in maindiff.c), shared by both sides as g3ddiff and
landmediff share it. UTIL's vector services are stateless arithmetic verified
in UTIL's own harness, and the real routines keep the registers the real ones
keep. G3D `+0x1c` is a model that leaves a distance in GLOBAL `+0xd4c`: falling
for a per-op number of calls and then rising, so every walk ends, above a
per-op base (often near the slew's `0x40000`), plus the low bits of the eye's
position, so where the eye was stepped shows in what comes back. It also logs
GLOBAL `+0xe88` during the call. The suite keeps the aircraft's latitude
(GLOBAL `+0x564`) inside slice 11's clamp, as the game does, because UTIL
`+0x44` indexes its latitude table by it. `make maindiff-run SUITE=spot`:
20,000 scenarios, 0 differences (about 4 minutes), with 3,532,883 G3D measures, 122,615 eyes placed afresh and 41,839
transitions slewed. `make main-breaks SUITE=spot`: 50 breaks, 47 caught, 3
proven equivalent: `target-sar` (the count is used only as its low word,
product bits 8 to 23, and `sar` and `shr` differ only in the top byte),
`target-1e` (the term is shifted up 16 at once, dropping the bits where sign-
and zero-extension differ) and `follow-copy` (the copy's other source was just
filled from the same place). The breaks found three gaps, now closed: the
distance never repeated (the walk's "unchanged" exits), and never landed on
the slew's `0x40000`. The first full run also found a harness artifact: the
suite's seeding wrote random words over the eye blocks' relocated pointers, and
a value that happened to point into retail's image compared as a pointer on
that side only; the seeding now skips relocated words.
