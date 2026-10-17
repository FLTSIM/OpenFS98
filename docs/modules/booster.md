# BOOSTER.DLL — the performance boosters

BOOSTER (module id 34) is the CD scenery cache, and it is reimplemented and
verified. `make boosterdiff-run` drives retail and OpenFS98 through 20,000
whole-module scenarios, worker thread included: RESULT_SUMMARY.

A "performance booster" is a directory on the CD (`Boosters\Antarcti`,
`Boosters\europe`, ...) with a `bstinfo` file and `scenery\files*`,
`texture\files*` trees. The Scenery Library gives each one a cache directory
on disk. When the aircraft flies into a booster's box, FE hands it to
BOOSTER. BOOSTER copies the files in reach from the CD to the cache, nearest
first, on its own thread, and registers each new BGL with FE. It deletes the
files that fall out of reach again and keeps the cache under the size the
world file gives it.

## Shape and linkage

8,024 bytes of MSVC C++ (`operator new`/`delete`, a priority-queue template,
one `__CxxFrameHandler` frame) and one 0x904-byte `.data` section, of which
0x200 bytes are on disk. `ImportTable` is at `+0x0`, `Linkage` at `+0x38`,
then the strings, the level factors and all of the state.
`tools/gen_booster_data.py` extracts the image; its only relocations are the
four Linkage code pointers, which `booster.c` installs.

Imports: host, GLOBAL, CHAIN, NUMERIC (never called), UTIL and FE.
Direct Win32 calls: thread, mutex, event and critical section;
`CopyFileA`, `CreateFileA`/`GetFileSize`, `DeleteFileA`,
`Get`/`SetFileAttributesA`; the message loop; `MessageBoxA` and `wsprintfA`.

| slot | retail | what |
| --- | --- | --- |
| `+0x04` | `.text+0x020` | init: CHAIN lists 3 and 0x14, the queue, `\SCENERY\` and `\TEXTURE\` from strings `0x9003`/`0x9006`, the thread, strings `0xf429` and 3 |
| `+0x08` | `.text+0x220` | deinit: `WM_QUIT` to the thread and wait, free the queue and every booster (emptying the caches whose `+0x21a` is 0) |
| `+0x18` | `.text+0x360` | add `(rec, area)`: scan the booster and **wait** while the thread copies |
| `+0x1c` | `.text+0x540` | drop `(rec)`: empty its cache if `+0x21a` is 0 or 1, unlink its queued jobs, wake the thread without waiting |

The CHAIN callbacks are list 3, every frame (`.text+0x670`), and list 0x14
(`.text+0x7d0`). The list 3 callback rescans one booster a frame, round
robin, and skips the frame if the thread holds the mutex. The list 0x14
callback rescans every booster and waits for the copying, for messages 0x21
and 0x33..0x35. Both callbacks, and `+0x18`, show UTIL's modeless "Reading
Scenery Files" (`0xf420`) and the host's busy state (`+0x2c`/`+0x30`) while
they work.

New host prototypes, read from the callees: `+0x2c` and `+0x30` take
nothing (`+0x2c` sets `*0x4187ec`, `+0x30` calls a hook with `0x20013` and
clears it). `+0x14c` is delete `(path, ext)`. `+0x150` is free space
`(path, u64 *out)`, `ret 8`. `+0x1a4` is file size `(file, &size)`.

### FE's booster record

FE passes its own 0x220-byte record. BOOSTER reads:

| offset | |
| --- | --- |
| `+0x012` | the **cache** directory: bstinfo is read here, files are copied here |
| `+0x116` | the **source**, the booster's directory on the CD |
| `+0x21a` | byte: 0 empties the cache on drop (and at deinit); 0 or 1 on drop |
| `+0x21c` | word: the cache's size in KB |
| `+0x21f` | byte: set to 1 when the user cancels a copy; scanning stops |

`src/fe/fe.c` had `+0x12` and `+0x116` the other way round in three
comments. BOOSTER's use settles which is which, and the comments are fixed.

## bstinfo

The cache's copy is read whole into a host block. `+0x14` is the offset of
the records, and `+0x16` a width. The number of levels is
`quota_kb * 2 / width - 1` (signed), or 1 for width 0. Each record is a type
byte and a word length. Type 0 ends the list, and so does any type above 5:

| type | |
| --- | --- |
| 1 | a list of files around one box, one file each |
| 5 | the same, a BGL and a TXR each |
| 2 | a grid of cells, one file a cell |
| 4 | the same, a BGL and a TXR a cell |
| 3 | `+3`/`+7`: the altitude band (GLOBAL `+0x574`) for the records after it |

Boxes are 24-bit fields: latitude north at `+3` and south at `+6`, doubled;
longitude east at `+9` and west at `+0xc`, shifted left 8. `+0xf` is the
reach, shifted left 8. `+0x11` is the kind: the type nibble (0 `.TXR`, 1
`.BGL` in `SCENERY`, 2 `.PAL`, else `.HAZ`) and a three-digit directory
number. A list's file numbers follow from `+0x13`, bit 0x2000 set once the
file is cached. A grid has columns at `+0x13` and rows at `+0x15`, then a
nibble a cell from `+0x17`: bit 1 present, bit 2 cached.

**The CD's sixteen bstinfo files use only type 1.** Types 2..5, the altitude
bands and the grids are reachable only from bstinfo files the CD never
shipped, so the harness writes its own.

Paths: the cache file is `cache\SCENERY\F012034.BGL` (`G` for grid cells).
The CD file is `source\SCENERY\FILES012\F012034.BGL` (`GRID012` for grids),
built with `"%s%s%s%.3u%\%c%.3u%.3u%s"`. The backslash there comes from
`wsprintf` printing an unknown `%\` literally, which wine does too. A native
port has to write it as `\\`.

## Scanning

For each record or present grid cell, the reach is scaled by the level
factor at `.data+0x60`: 0.5, 1, 1.18, 1.36, 1.54, indexed by
`levels - level` clamped to 4. That goes through `_ftol` on the x87. The
range test at `.text+0x1bd0` then puts the box in one of five places
relative to the aircraft:

- **level with it in longitude:** in reach if the south edge is within the
  reach and the north edge within minus the reach. The distance kept for the
  queue is the *north* edge's, squared, even when the south edge is further.
- **level with it in latitude:** the longitude gap against the reach
  converted to longitude at the aircraft's latitude (UTIL `+0x44`). The
  distance is that gap converted back to metres (`+0x48`), squared.
- **a corner:** latitude difference squared (in GLOBAL's raw units) plus the
  longitude gap in metres squared, against the reach squared, all on the
  x87, so the precision control matters.

In reach, the record or cell is queued. Not in reach even with a quarter of
the unscaled reach added, its cached files are deleted, and BGLs
unregistered with FE `+0x74`.

A scan of a booster whose `+0x21a` is 0 first reads its drive's free space
again. Once a copy has found the cache full (`used >= quota`) or its drive
under 256K, a counter starts. It counts scans, and every 60 the reach shrinks
a level, to 5. It resets only when the cache is under quota again and more
than 512K is free.

## The queue and the thread

Jobs are 0x14-byte `operator new` blocks: the booster, the record, the
distance (a double) and the cell. They sit in a binary min-heap on the
distance, in a host block that grows by 0x80 entries. Equal distances favour
the child on the way down. The pop inlined in the thread at `.text+0x13d1`
has its own three-way first step.

The thread waits in `GetMessageA`. Message 0x401 empties the queue, holding
the mutex for each job, then sets the event the main thread is waiting on. A
`WM_QUIT` waiting behind it stops the emptying. Jobs whose booster has been
dropped are discarded. A job copies what its record or cell needs:

- `CopyFileA` with fail-if-exists;
- the read-only bit cleared;
- the size added to the cache and taken from the free space (as 64 bits);
- a new BGL registered with FE `+0x70`, after which GLOBAL `+0x444` gets bit
  4 when the waiting call returns, or bit 8 from the frame callback at most
  every 0x96 ticks of GLOBAL `+0x2ac`.

A failed copy puts up a retry/cancel `MessageBoxA` naming the source.
Cancel sets `+0x21f`, marking the booster done for good, and empties the
queue by setting its count to 0. The jobs leak, because the queue's "owns
its elements" bit is never set. For a pair, a failed TXR deletes the BGL just
copied.

Emptying a cache (`.text+0xaa0`) lists `SCENERY\` and `TEXTURE\` through the
host and deletes everything in them. BGLs are unregistered first if named
`F` or `G` and six digits; a BGL with any other name is left alone. It is
skipped while GLOBAL `+0x444` has 0x10 without 0x20, and sets bit 8
otherwise.

## Retail behaviour worth knowing

All reproduced.

- **Init does not wait for its thread.** A thread has no message queue until
  it first asks for a message, and a 0x401 posted before then is lost. `+0x18`
  would then wait for ever on an event nobody sets. The sim starts long before
  a booster is handed over, so in practice it cannot happen. The harness waits
  for the queue after init.
- **The host's find-close dereferences the handle unconditionally.** A cache
  without a `SCENERY\` or `TEXTURE\` directory leaves a null handle after the
  failed find, and BOOSTER closes it anyway, so the sim faults.
- **A file with an extension the host does not know** keeps the previous
  entry's extension id in BOOSTER's find buffer. So `NOEXT` after `G001.HAZ`
  is opened as `NOEXT.HAZ` when the cache is measured, and deleted as
  `NOEXT.HAZ` when it is emptied.
- **A short grid or list record runs away.** Columns or rows of 0 divide by
  zero; rows divide signed and columns unsigned. A record shorter than 0x13
  bytes makes the file loop run 2^31 times.
- **Retail UTIL `+0x44` faults when the reach is a whole latitude circle.**
  It divides once, 64 by 32 bits, so the quotient overflows (#DE) once the
  distance reaches the circumference at that latitude. That is about 4,200 km
  at 84°S, and BOOSTER's largest reach is under 32,000 km. The CD's boosters
  reach 78.8 km at most, so this only matters within about a kilometre of the
  poles. **OpenFS98's UTIL does not reproduce this fault.** It handles only a
  zero divisor and returns a quotient where retail traps. `utildiff` does not
  reach the case; it is noted here for UTIL to pick up.

### Dead code omitted

The exception frame around the queue's construction in init; nothing in it
can throw. The default arm of the range test's corner switch, which the
quadrant byte (always 1..4) never reaches.

## Differential harness

`tools/difftest/boosterdiff.c`. Both DLLs are loaded into one process and
run **in turn**, never together, each over cache trees rebuilt from the same
description. Arenas are reset between sides, so heap pointers come out
identical. What BOOSTER imports:

- **host:** a stub. Memory comes from a bump arena filled with 0xa5. Files
  and listings pass through to the real trees, behind transliterations of the
  host's join, split, find and file services and its extension table (read
  out of `FLTSIM98.EXE`). Free space is the scenario's: fixed, failing, or a
  base less what the caches hold.
- **CHAIN, UTIL alerts, FE `+0x70`/`+0x74`:** logged. UTIL `+0x44`/`+0x48`
  are retail's, called directly.
- **operator new/delete:** redirected to a second arena, through retail's
  import table and ours' `GetProcAddress`.
- **Win32 file calls, `MessageBoxA`, `PostThreadMessageA`, `SetEvent`:**
  import-table hooks that log and pass through. The message box answers from
  the scenario's script: retry, cancel, or retry after putting the missing
  source or cache file in place.

**The thread is made deterministic by draining it after every operation.**
Every 0x401 posted is counted, and so is every `SetEvent` that answers it.
The harness waits until the two agree. So the thread is idle whenever the main
thread calls in, and the frame callback's zero-timeout mutex wait fails only
when a helper thread holds the mutex on purpose ("busy" frames).

Fixtures: four of the CD's boosters (Antarcti, samerica, oceania, canada,
sometimes with their width changed), and synthetic boosters written for every
scenario. The synthetic ones carry all five record types, inverted grids,
unknown types, missing and read-only sources, pre-cached files (some
read-only), and every file one size for the 32-bit carry edge. Their caches
also hold odd files: unregistered extensions or none, BGLs not named as
BOOSTER names them (`NOTMINE`, `F12345`, `F1234567`, `G654321A`), and a
subdirectory.

Steering, per [method.md](../method.md):

- the aircraft is moved inside boxes, onto their edges, and onto both of
  BOOSTER's reaches (scaled, and scaled plus a quarter of the unscaled one),
  in latitude, in longitude (converted through UTIL), and at a corner exactly
  `R` south;
- the altitude onto band edges;
- the free space onto exactly 0x3ffff..0x80001, and one file short of
  wrapping the low dword;
- the tick onto the one BOOSTER set.

One scenario in sixteen keeps one booster for 150..300 operations, so its
full cache shrinks the reach. One in 64 runs 500+ operations with no free
space, so the reach shrinks to the last level. Each scenario also runs under
a random x87 precision control.

Compared after every operation: the transcript, BOOSTER's whole image
(handles, the critical section and Linkage's code pointers masked), both
arenas, GLOBAL and FE's records. At the end of each scenario the cache trees
are compared too: names, sizes, read-only bits and contents.

Kept clear of: `+0x1c` with nothing added (null ring), caches without their
two subdirectories, empty grids, records under 0x13 bytes, and aircraft
latitudes beyond ±9,400,000 m with reaches big enough to fault retail UTIL.

Result: RESULT_DETAIL

**Deliberate breaks, each built into `BOOSTER_ours.dll` and run over 300
scenarios: all 25 caught.** They were: each of the range test's five
boundary comparisons and its box edge; all three heap tie rules; the level
cap; both free-space thresholds; the 32-bit carry on deletion; signed row
division; drop's not-found cursor; the PAL/HAZ extension; cancel's queue
clear; the seventh character of BOOSTER's own names; the flush condition;
the tick comparison; the 0x32 message boundary; the factor clamp; the
quarter-reach step; a pair's BGL rollback; and the cancelled check in the
pair loop.

Four went uncaught at first, and each was a fixture gap: a corner exactly on
the reach, free space exactly 0x80000, the carry edge, and names past six
digits. The generator now produces each of them.

## Portability

Win32 throughout: the thread, its message queue, the mutex and event,
`CopyFileA` and `MessageBoxA`. A native host would replace the thread's
message loop with any queue and condition variable. It would also have to
honour the order the harness relies on: one 0x401, one drain, one event. The
`%\` in the source path format must become an explicit backslash.
