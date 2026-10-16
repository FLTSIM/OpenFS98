# FACIL

FACILITIES.DLL is module 37, the facility-directory compiler and browser. FE
loads facility directory blocks from BGL scenery files; FACIL turns them into
`INDEX_FILE`, then its lookup services and three dialogs browse that compiled
hierarchy. Setup installs the CD's `FACIL.DLL` as `FACILITIES.DLL`.

The module is in progress. The externally testable file half is now
reimplemented and verified: init/deinit, `+0x2c` cache validation and rebuild,
and `+0x20` copying a selected serialized record. The search services
`+0x18`/`+0x1c` and the three dialogs remain. FACIL is therefore deliberately
absent from `MODULES`; `make install` cannot ship this partial DLL.

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
| `+0x18` | `+0x5420` | remaining | filtered facility search |
| `+0x1c` | `+0x55c0` | remaining | list facilities below a selected node |
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

Current baseline:

```text
INDEX_FILE 0x195 bytes, arena 0xbcd0 bytes, 0 differences
IDENTICAL
```

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

`+0x18` and `+0x1c` lazily map `INDEX_FILE` with Win32 file mapping, build a
temporary C++ priority queue and return filtered 12-byte result records. They
must be transliterated and added to this harness before the query half is
called verified. The three dialog slots then belong in `ofstest`, where the
real host, MAIN and window lifecycle are available.

There is no hand-written assembly in the new FACIL file half. Its tree and
serializer logic is ordinary C and is suitable for a portable core. The
remaining file mapping should be put behind the eventual host/platform file
abstraction; the exported module shell, 32-bit pointers and stdcall Linkage are
compatibility-layer concerns, not requirements of the facility algorithm.
