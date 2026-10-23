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
| 2, the situation reader and writer | `+0x38`..`+0x4c` (assembly, `.text+0x11cd2`..`+0x13104`) | `situations` | verified: SITUATIONS_RESULT |

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
| `+0x1c` | `.text+0xc520` | FACIL | |
| `+0x20` | `.text+0xa090` | | |
| `+0x24`..`+0x34` | `.text+0x16a60`, `+0x165b0`, `+0x16880`, `+0x16920`, `+0x169a0` | REALITY (`+0x30`, `+0x34`) | `+0x30` greys menu items `0x1001`..`0x1006`; `+0x34` the options dialog |
| `+0x38`..`+0x4c` | `.text+0x11eeb`, `+0x11e3d`, `+0x120aa`, `+0x127b3`, `+0x12f50`, `+0x12f6a` | ADVDRV (`+0x3c`), REALITY (`+0x44`), DEMO (`+0x48`, `+0x4c`) | the situation reader and writer, assembly: `+0x48` loads a situation from memory, `+0x4c` from a file |
| `+0x50` | `.text+0x13105` | | assembly, the largest gap (11 KB) |
| `+0x54`..`+0x6c` | `.text+0x15cc0`..`+0x15f50` | WINMSG, WINDOW, PANELS (`+0x5c`..`+0x64`) | **slice 1**, below |
| `+0x70` | `.text+0x19507` | ADVDRV, LANDME | range and bearing between two points |
| `+0x74` | `.text+0x18781` | ADVDRV, MULTIP | |
| `+0x78`, `+0x7c` | `.text+0x19c94`, `+0x19cd5` | | |
| `+0x80` | `.text+0x19f10` | ADVDRV, PANELS | `(which, &tenths)` |
| `+0x84`..`+0x90` | `.text+0x16100`, `+0x162f0`, `+0x16570`, `+0x10560` | CONTROLS, WINDOW (`+0x88`) | |
| `+0x94`..`+0xa0` | `.text+0x7140`, `+0x6f59`, `+0x6de9`, `+0x830d` | WEATHERM (`+0x94`), WINDOW (`+0x98`, `+0x9c`) | |
| `+0xa4`, `+0xa8` | `.text+0x16ddf`, `+0x6450` | ADVDRV, CONTROLS, WINDOW | |
| `+0xac`..`+0xbc` | `.text+0xc870`, `+0xcdb0`, `+0xdd70`, `+0xe5a0`, `+0xed10` | CONTROLS (`+0xac`), FACIL (`+0xb0`) | |
| `+0xc0`..`+0xc8` | `.text+0x06a0`, `+0xc560`, `+0x18f0` | WINDOW, FACIL, CONTROLS | |

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

MAIN `+0x74` (the season and date) and `+0xa4`, which loading calls, are
not done yet. They are seams for now: retail's two call sites
(`.text+0x12b5f`, `+0x130fe`) are patched in the harness to stubs, and ours
calls them through its Linkage, which the harness points at the same stubs.
`+0x74` is pushed the day as a word over two stale bytes; the stub logs the
word, and `+0x74`'s slice must show that it reads only that.

SITUATIONS_DETAIL
