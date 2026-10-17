# FACIL

FACILITIES.DLL is module 37, the facility-directory compiler and browser. FE
loads facility directory blocks from BGL scenery files; FACIL turns them into
`INDEX_FILE`, then its lookup services and three dialogs browse that compiled
hierarchy. Setup installs the CD's `FACIL.DLL` as `FACILITIES.DLL`.

The module is done: every Linkage slot is reimplemented and verified --
init/deinit, `+0x2c` cache validation and rebuild, `+0x20` record copy, the
mapped-cache queries `+0x18`/`+0x1c`, and the three dialogs `+0x24`, `+0x28`
and `+0x30`. FACIL is in `MODULES`; `make smoke` passes with it installed, and
the sim rebuilt `INDEX_FILE` through it.

## Shape and linkage

Retail is MSVC C++ with a 23,045-byte `.text` section. Its `.data` section is
0x2d1c bytes at `0x206b8000`: 0x600 initialized bytes followed by zero-filled
state. `tools/gen_facil_data.py` checks the imports, Linkage and relocation
layout and emits the complete image as `src/facil/facil_data.inc`. There are 74
relocations: 49 point back into `.data` and 25 into `.text`.

| Linkage slot | retail entry | status | purpose |
| --- | ---: | --- | --- |
| init | `+0x1020` | verified | read the `[FACILITIES]` profile values |
| deinit | `+0x1030` | verified | release a mapping and save the profile values |
| `+0x18` | `+0x5420` | verified | filtered, name-sorted search of one sibling run |
| `+0x1c` | `+0x55c0` | verified | resolve a filter list against a selected node's ancestry |
| `+0x20` | `+0x5540` | verified | copy one serialized record by file offset |
| `+0x24` | `+0x2550` | verified | airport dialog (World `0x1309`) |
| `+0x28` | `+0x1e30` | verified | exact-position dialog (World `0x1308`) |
| `+0x2c` | `+0x3be0` | verified | validate or rebuild `INDEX_FILE` |
| `+0x30` | `+0x1040` | verified | facilities dialog (World `0x1307`) |

The imports are host (1), GLOBAL (2), CHAIN (4), FE (18), MAIN (3), NUMERIC
(17), and UTIL (20). The verified file path uses FE `+0x7c` to list 16-byte
file ids, `+0x80` to open each facility block, and `+0x84` to obtain an
airport-name section when one exists.

Init reads `REGION`, `COUNTRY`, `STATE`, `CITY`, `GTL_BUTTON`, and
`ALLOW_GRAY_ERROR_DIALOGS` from `[FACILITIES]`. Deinit writes the four strings
and `GTL_BUTTON`; retail does not write the gray-dialog flag back.

## The in-memory hierarchy

The root at `.data+0x1af8` is a pair of host growable arrays:

- `order`, 4-byte offsets into the node array, kept sorted by name and kind;
- `nodes`, whose elements are 0x60-byte hierarchy nodes.

A hierarchy node contains its borrowed name pointer, kind, a source list,
an initialized flag and another two-list tree. Region, country, state and city
nodes use the 0x60-byte form. Airport nodes use a 0x24-byte leaf form with no
child tree. A source is 0x14 bytes: a 16-byte FE file id and a BGL value or
offset.

Names are compared with the host's case-sensitive `strcmp`, then by numeric
kind. A repeated node receives a source only when both its id and value are
new. All lists grow in steps of 0x20, exactly as retail's do. That detail is
observable in the differential allocator arena and is covered by the harness.

FACIL reads variable-length records from each FE block. The record count is at
block `+0x14`; records begin at `+0x18` and advance by their size at `+4` plus
eight. The recognized kinds are:

| kind | level |
| ---: | --- |
| `0x10001` | region |
| `0x20001` | country |
| `0x30001` | state |
| `0x40001` | city |
| `0x50001`..`0x50004` | airport/facility leaf |

If a record omits an ancestor, retail inserts an empty placeholder of kind
`0x10000`, `0x20000`, `0x30000`, or `0x40000`. The implementation preserves
that cascade rather than normalizing malformed input.

## INDEX_FILE

The 12-byte header is:

| offset | value |
| ---: | --- |
| `+0x00` | total file size |
| `+0x04` | root-node offset |
| `+0x08` | count of following 16-byte FE file ids |

After the ids, nodes are serialized depth first in sorted order. Each node is:

| offset | value |
| ---: | --- |
| `+0x00` | parent file offset |
| `+0x04` | next sibling file offset |
| `+0x08` | first child file offset |
| `+0x0c` | kind |
| `+0x10` | bytes after the first 0x14 bytes |
| `+0x14` | flags |
| `+0x18` | name size including NUL |
| `+0x1c` | source count |
| `+0x20` | name, then 0x14-byte sources |

The serializer remembers the last node offset at each of five levels and
backfills sibling and child links with seek/write operations. It performs the
child-field backfill even when the value is already zero; the harness checks
that observable call sequence.

A `0x50001` airport gets flag 2 and a synthesized `0x50002` sibling. FACIL asks
FE `+0x84` for section kind `0x10006`; the section's string supplies the
sibling name. If that section is absent, the name is empty. Sources are copied
from the original airport. This fallback is not dead behavior: the synthetic
harness fixture deliberately exercises it.

The file is first written to a host temporary name, fully backfilled and
closed, then moved over `INDEX_FILE` with the original attribute state. A
second `+0x2c` call reads the stored ids, obtains and deduplicates the current
FE ids, and leaves the cache untouched when they match.

## The query services

`+0x18(filters, filter, out, dedupe)` and `+0x1c(node, filters, out)` read
`INDEX_FILE` through a Win32 file mapping. A filter is 8 bytes, `{ kind, name
}`; a filter list is `{ count, entries }`. Both return `{ count, records }`,
where each 12-byte record is `{ name, kind, node offset }` and the caller owns
the names and the array. The dialogs pass a four-entry region..city list from
`.data`.

**Mapping.** `+0x18` maps the file on first use (`CreateFileA` read/write,
`FILE_FLAG_RANDOM_ACCESS`, a read/write mapping, a read view) and keeps three
copies of the view base at `.data+0x2b50`..`+0x2b58`. A header whose size
equals its root offset sets the empty flag at `+0x2b5c`. A successful search
leaves the file mapped; `+0x1c` and `+0x20` never map it themselves and read
whatever view is there. Unmapping (`.text+0x57e0`) is done by `+0x2c`, deinit
and every `+0x18` failure, and leaves the view copies stale. If
`MapViewOfFile` fails, retail closes both handles, still marks the cache
mapped, and reads the header through the null view.

**`+0x18`.** It descends first children from the root to the filter's level,
so it scans only that **first** sibling run -- for a state filter, the states
of the first country of the first region. A node is kept when it is at the
filter's level (and has its exact kind if the kind's low word is set), its
name starts with the filter's name **case-sensitively** (`memcmp`), and it
fits the filter list. The list check uses only entries that have a nonempty
name, in the caller's order: entries above the node's level must match its
ancestors, and the first entry at or below its level must be satisfied by
the node or a descendant chain, both by whole name **case-insensitively**.
Retail first copies and `qsort`s the list by kind, then discards that sorted
copy unused and rebuilds the list unsorted.

Matches go into a C++ binary heap ordered by `_strcmpi` on the name, created
with MSVCRT `operator new`. It starts at capacity 0x200 and grows by 0x80
through host `realloc`, and its storage pointer is kept one slot below the
allocation so it can be 1-based. Draining pops in name order; ties come out
in whatever order the heap leaves them, which the harness compares. With
`dedupe`, a record whose name equals the last one kept (case-insensitively)
is dropped and both its name and record are freed. The queue's "owning" bit
is always clear, and a check that would clear it again can never fire; that
dead branch is omitted, but its read of the queue (which faults on a failed
`new`) is kept.

Every failure after `out` is zeroed -- a missing or empty cache, or an
allocation failure -- unmaps the cache and returns **the unmap's result**,
so a failed search normally returns 0, not -1. Failures also leak the queue
and the filter copy.

**`+0x1c`.** It finds the start of the run containing the selected node:
the node's parent's first child, stepping over runs of flag-2 nodes (an
airport and the name record serialized after it). For each list entry, in
caller order, it climbs from there to the entry's level and takes the last
node in that flag-2 run that matches the entry and does not have flag 1. A
second match in the same run overwrites the first and leaks its name.
Entries that resolve to nothing are skipped. This service also copies and
sorts the list and never uses the copy.

Flag 1 is never written by the serializer, and flag-2 runs on non-airport
nodes do not arise from rebuilding. Both are reachable only from a cache
that FACIL did not write, and the harness covers them with synthetic caches.

## The dialogs

FACIL has no window code and imports nothing from USER32. Each dialog slot
fills a parameter block in `.data` and calls host `+0x10c` with a dialog id
that `English\World.DLL` registers; World draws the controls and calls back
through function pointers in the block. Everything in FACIL's
`.text+0x1040..+0x3be0` is those callbacks and what they call.

| id | block | entry | callbacks (block offset) |
| ---: | --- | --- | --- |
| `0x1307` | `.data+0x80` | `+0x30`, and a button in the other two | `+4`, `+8` do nothing; `+0xc` apply |
| `0x1308` | `.data+0x158` | `+0x28` | `+4` validate, `+8` apply, `+0xc` revert, `+0x10` reload, `+0x14` facilities |
| `0x1309` | `.data+0x3e8` | `+0x24` | `+4` nothing, `+8` OK, `+0xc` area, `+0x10` everything, `+0x14` facilities, `+0x18` places, `+0x1c` names, `+0x20` airports, `+0x24` runways |

**Facilities (`0x1307`).** World supplies parallel arrays of region,
country and state names and a flag word. Bit 0 lists airports: `+0x18` over
`.data+0xd0`'s place filter, then per airport a 0x64-byte row with its four
place names (`+0x1c`), latitude/longitude/altitude (FE `+0x84` section
`0x10001`, formatted by NUMERIC), long name (`0x10006`), first type-1
frequency (`0x20001`, three BCD digits, a `.`, three more) and runway ends
(FE `+0x88`). Each runway gives two 0x2c-byte ends: the far one is made by
turning FE's record round in place -- heading plus `0x80000000`, pattern
sides 1 and 2 swapped, the number less 18 wrapped into 1..36. An end's ILS is
the **last** type-4 `0x10004` record after the runway that is not behind it,
judged by the sign of the separation dotted with the heading's sine and
cosine (`.text+0x67c0`). Bit 1 lists VORs and NDBs (`.data+0x108`/`+0x10c`)
as 0x60-byte rows whose `+0x5c` bits record the frequency kinds seen. Rows
are sorted by host `qsort`: airports by city with the case-sensitive
`strcmp`, navaids with bit-1 rows last, then by country, state and name.

**Position (`0x1308`).** Reload copies GLOBAL's position, heading and the
indicated or true airspeed (`+0x612` chooses) into `.data+0x560..` and
through UTIL `+0x90` into World's text controls. Validate runs UTIL `+0x8c`
and four NUMERIC parses, but only the last parse decides the result. Apply
writes back each changed field, decides whether the aircraft is on the
ground (GLOBAL `+0x366`), then calls the `.SIM` module's `+0x34` through
GLOBAL `+0x11d4`, CHAIN `+0x20(0x14, 0x33, 0)` and MAIN `+0x1c`/`+0xb0`.

**Airports (`0x1309`).** Four place lists filter each other through
`.data+0x2f8`; a name list searches `0x50002` name records by prefix; the
airport list depends on the mode remembered as `GTL_BUTTON` -- `0x1000` by
place, `0x1001` by name record, `0x1002` FE's navaids around an FE area. The
runway list reads `0x10005` sections. OK starts from the selected runway's
threshold less FE `+0x30`'s ground height (or the aircraft's own position
without runways), puts the tower at the first `0x10003` section or else
2,000 units off the threshold, moves the aircraft, and saves the place
names to the profile. With an output pointer it also returns six names,
though only five are cleared first.

Retail quirks kept:

- OK's start position is an uninitialised stack buffer. In mode `0x1002` a
  navaid fills only part of it; in a mode other than the three with a stale
  airport list it fills none of it; either way the rest reaches GLOBAL
  `+0x560`. The reimplementation leaves its buffer uninitialised too.
- An airport with no sources makes the tower search use the block itself as
  the section, copy block words into GLOBAL `+0xd50..`, and free the block.
- Leaks everywhere: every row's `+0x1c` result and record copy, the found
  frequency section, the leading empty-name record, `0x1309`'s output
  array, and a second airport-name allocation in one run.
- Several `.data` copies -- the sorted filter list in `+0x1c`, a second
  free in mode `0x1002` -- are made and discarded.

**FE `+0x88` returns stack garbage.** On success it returns a local at
`[entry esp - 0x11c]` that it never writes (it only zeroes it if the
memmove fails). FACIL's runway code treats that word as success or failure.
In the game it is left over from earlier calls -- in this path, from FE
`+0x84`'s file lookup, whose helper initialises the overlapping local to -1
-- and is nonzero in practice, so runway lists are built. fe.md had described
the return as memmove's status, and the FE reimplementation returned 0
there, which would have emptied every runway list under an installed
OpenFS98 FE. FE now reproduces the leftover word exactly; see
[fe.md](fe.md). `make facildiff-fe-run` runs this harness with OpenFS98's FE
behind OpenFS98's FACIL against retail behind retail: 2,000 query and 2,000
dialog scenarios over the real CD index, zero differences.

## Differential harness

`make facildiff-run` loads retail FACIL and OpenFS98 FACIL side by side. Each
gets a separate retail FE instance so mutable FE state is not shared. The
harness creates a synthetic BGL directory with region, country, state, city
and airport records, plus a minimal type-1 airport header with no `0x10006`
section. That produces six serialized nodes, including the empty-name fallback.

The run compares:

- both rebuild results, including the unchanged-cache second call;
- the complete host/FE transcript;
- every byte of the 0x195-byte `INDEX_FILE`;
- the 0xbcd0-byte deterministic allocation arena;
- GLOBAL, FE state and FACIL's complete 0x2d1c-byte image;
- `+0x20`'s copied airport record, allocation size and contents.

A second phase covers the queries. Each of 20,000 seeded scenarios writes
a synthetic `INDEX_FILE` directly to the cache directory. The caches are
random five-level trees with placeholder kinds, airport/name-record pairs,
stray flag-1 and flag-2 nodes, and mixed-case, prefix and duplicate names.
Some caches are missing, some empty, and about one in 25 has a sibling run
of 0x1ff..0x2c0 nodes that crosses the queue's 0x200 and 0x280 steps. Each
side then runs the same sequence of `+0x18`, `+0x1c`, `+0x20` and deinit
calls. Filters are steered onto the cache's own names (case-flipped, and
truncated for the prefix match) and onto the first sibling runs that
`+0x18` actually scans. While this phase runs, `strlen`, `_strcmpi` and
`memcmp` are logged with their strings, so the order of comparisons is
compared, not only the results. Each scenario compares the transcript,
including every returned record, the arena, and the FACIL image. The view
copies, queue pointer and handles are compared only as present or absent.

Most generated caches thread each level's siblings into one chain across
parents, as the serializer does (`LAST_OFFSETS` is per level, not per
parent); a fifth chain siblings only within a parent. The first version
generated only the latter, so the subtree matcher's stop at a change of
parent was never reached; a break of it now fails.

A third phase covers the dialogs over real data. Each side first rebuilds
`INDEX_FILE` from the CD: the `world.vis` registry (`afd_na` -- regions,
countries, VORs, NDBs -- and `heloafd`) plus all sixteen boosters' AFD files,
which the game reads from the CD and BOOSTER registers through FE `+0x70`;
the harness registers copies the same way. That is 18 files, 3,791 airports,
and a 0x1139ba-byte index, and retail and ours build it identically. Then
10,000 scenarios each open one of the three dialogs, after randomising the
aircraft's position, units, speeds and the profile's remembered places and
mode. Host `+0x10c` is a stand-in for World.DLL that plays a seeded script:
selections and edits a user makes, kept inside their lists, with dependent
lists requeried as World's controls would, nested facilities dialogs, and
the answer. Every list is logged after every callback. MAIN, CHAIN and the
`.SIM` table are logging stubs; NUMERIC is retail's, bound behind both FACIL
and UTIL.

Three kinds of stack garbage are handled explicitly, because both sides
must see the same bytes:

- a callback is entered through an asm invoker that paints the 24 KB below
  its return address, so OK's uninitialised position reads the same paint;
- FE `+0x88`'s return word (see *The dialogs*) is written by a thunk in
  front of each FE's `+0x88`: -1 in seven scenarios in eight, 0 in the rest;
- FE `+0x94a` keeps the stack address of the position FACIL passed to
  `+0x30`, and is compared only as a stack address.

Half the scenarios mutate real sections through a wrapper on FE `+0x84`
(some kind-2 navaid frequencies become kind 8, some type-1 runway entries
type 3), and one airport or name record in 40 loses its name in the index,
as an airport without a `0x10006` section does. GLOBAL `+0x20` is steered
onto the aircraft's altitude in a quarter of the scenarios, the boundary of
the position dialog's ground test. Heap words that point into a side's own
FACIL or FE image are compared as offsets.

Current baseline (26 minutes):

```text
INDEX_FILE 0x195 bytes, arena 0xbcd0 bytes, 0 differences
QUERY 20000 scenarios: 45524 searches (10391 with results, 256060 records,
most 796, 448/194 queue growths past 0x200/0x280, 218549 dedupe drops),
11681 describes (17785 records), 7103 record copies, 674 empty and 474
missing caches, 0 differ
REAL INDEX_FILE 0x1139ba bytes, 18 ids, arena 0x10774e0 bytes, 0 differ
DIALOG 10000 scenarios (5955 airport, 2515 position, 1530 facilities):
12228 dialogs, 67766 callbacks; airport: 867/818/831/288 accepts by mode,
1513 moves, 586 outputs, 485 runway lists, 2114 area lists, 369 area navaid
lists; facilities: 648228 airport rows, 2021214 runway ends, 249516 ILS,
852982 navaid rows, 852982 with frequencies; position: 939 applies, 1434
refused validations; navaid frequency bits 303117/534418/291010/62934,
airport frequencies none/three-digit/other 560291/87937/0, FE +0x88 zero
in 1252; 0 differ
IDENTICAL
```

Forty-six deliberate breaks of the dialog code were each run against 400
scenarios, and forty were caught: BCD formatting, frequency kinds, runway
turning, ILS kind and selection, the ahead test's sine/cosine, every row
field and both comparators, the place selection and its first-call skip,
runway type, suffix and payload, the ground-height shift, the outputs,
the tower's angle, distance and altitude, the move's size, the position
dialog's loads, flags and ground test, the mode-`0x1001` index, the area
fields and FE argument order, and free order. Of the six that were not:

- two are equivalences: the ahead test rewritten into an identical
  expression, and reading the longitude's integer part as unsigned, which
  shifts the fraction by 2^32 and, after the 65536 multiply, only bits that
  the 16-bit truncation drops;
- reading the ahead test's metres through the other conversion for
  0xb0000000..0xc0000000 changes both points by the same 2^48 and cancels in
  the difference, unless a runway and its ILS straddle the 2^47 wrap --
  no real pair does;
- a failing earlier parse in validate is, it appears, always also refused
  by UTIL `+0x8c`, which already fails the call; not proven;
- airports with more than one source (the tower search's first-found
  break) and southern navaids in mode `0x1002` (the latitude's arithmetic
  shift) do not occur in the data. Both are real gaps.

A retail-vs-retail run (a second copy of retail as the comparison) is also
identical. Eighteen deliberate breaks were run against 2,000 scenarios: heap
sift-up ties, pop and sift-down child choice, dedupe against the wrong
record, the prefix length test, the exact-kind test, the ancestor and
descendant matchers, the root-sibling stop, unnamed list entries, the
flag-2 run step, the flag-1 skip, the resolve level test, the growth size,
the failure return, the first-run descent, and dropping unresolved entries.
Seventeen were caught. The one that was not, `start < node` changed to
`<=` in the run-start search, is an equivalence: the extra iteration calls
the step with `node == target`, which returns at once without reading
anything and leaves `start` unchanged.

The required deliberate break changed the synthesized kind from `0x50002` to
`0x50003`. The harness failed at `INDEX_FILE+0x16c` (`02` versus `03`), proving
that it detects a semantic serialization error rather than merely exercising
the entry point.

The fixture root still needs a provisioned `Scenery/world.vis` and
`Texture/fs5.pal`, because each retail FE instance performs its ordinary init
before the synthetic facility state is injected. `FE_FIXTURE` selects `orig/`
when those files are present and otherwise `game/`; the target stops with an
explicit message if neither tree is available.

## Remaining work and portability

Nothing in FACIL remains unverified externally. `ofstest` can still confirm
the World side of the protocol in-process -- the order World calls the
callbacks and which block fields it writes -- which the harness's stand-in
assumes.

FACIL has no hand-written assembly. Its tree, serializer, filters and heap
are ordinary C and suitable for a portable core. The file mapping and the
MSVCRT `operator new`/`delete` pair, which are bound by name at first use
because mingw's import library lacks the mangled names, should go behind the
eventual platform layer. The exported module shell, 32-bit pointers and
stdcall Linkage are compatibility-layer concerns, not requirements of the
facility algorithm.
