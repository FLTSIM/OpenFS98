# FACIL

FACILITIES.DLL is module 37, the facility-directory compiler and browser. FE
loads facility directory blocks from BGL scenery files; FACIL turns them into
`INDEX_FILE`, then its lookup services and three dialogs browse that compiled
hierarchy. Setup installs the CD's `FACIL.DLL` as `FACILITIES.DLL`.

The module is in progress. Everything outside the dialogs is now
reimplemented and verified: init/deinit, `+0x2c` cache validation and rebuild,
`+0x20` copying a selected serialized record, and the mapped-cache query
services `+0x18`/`+0x1c`. The three dialogs remain. FACIL is therefore
deliberately absent from `MODULES`; `make install` cannot ship this partial
DLL.

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
| `+0x24` | `+0x2550` | remaining | airport dialog |
| `+0x28` | `+0x1e30` | remaining | facility-directory dialog |
| `+0x2c` | `+0x3be0` | verified | validate or rebuild `INDEX_FILE` |
| `+0x30` | `+0x1040` | remaining | scenery/facilities dialog |

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

Current baseline:

```text
INDEX_FILE 0x195 bytes, arena 0xbcd0 bytes, 0 differences
QUERY 20000 scenarios: 45615 searches (7005 with results, 232361 records,
most 704, 443/193 queue growths past 0x200/0x280, 201328 dedupe drops),
11658 describes (17683 records), 7140 record copies, 682 empty and 487
missing caches, 0 differ
IDENTICAL
```

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

The three dialog slots belong in `ofstest`, where the real host, MAIN and the
window lifecycle are available. `.text+0x67c0`, a NUMERIC distance test
called only from the airport dialog, goes with them.

FACIL has no hand-written assembly. Its tree, serializer, filters and heap
are ordinary C and suitable for a portable core. The file mapping and the
MSVCRT `operator new`/`delete` pair, which are bound by name at first use
because mingw's import library lacks the mangled names, should go behind the
eventual platform layer. The exported module shell, 32-bit pointers and
stdcall Linkage are compatibility-layer concerns, not requirements of the
facility algorithm.
