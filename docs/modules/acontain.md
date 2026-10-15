# ACONTAIN.DLL — module id 23

**Reimplemented and verified.** ACONTAIN is the aircraft container. An
aircraft is a folder under `AIRCRAFT\` holding an `aircraft.cfg`, and each
`[fltsim.N]` section in it is a variant with a title, an `.AIR` file, and
model, panel, sound and texture folders. ACONTAIN finds an aircraft by title
and resolves those paths for whoever asks. It loads the flight model (a
`.SIM`) and the visual model (a `.MDL`), both of which are DLLs loaded through
the host, and lists every complete aircraft for the selection dialog. MAIN
drives it. PANELS, SOUND, WINDOW and CONTROLS ask it for paths.

`src/acontain/acontain.c` is the transliteration and the module shell
together. `make acontaindiff-run` drives retail and the OpenFS98 build side by
side over random scripts against generated aircraft trees --
**20,000 scenarios, 0 differ**. Each run compares ACONTAIN's whole `.data`
image as well as everything it calls. `make smoke` passes with ACONTAIN
installed beside ATC, CHAIN, GLOBAL, NUMERIC, SLEW, UTIL and WEATHERM: 27/27
modules, 95 gauges. That is a real in-game check: the sim loads its aircraft
through `+0x28` at startup, and the gauges load only once the flight model,
the visual model and the panel have all come up.

The reasons it came after WEATHERM are in [../preflight.md](../preflight.md).

## Binary and linkage

- Retail image base `0x20600000`; `.text` 5,504 bytes, all compiled C (MSVC
  prologues, `nop` padding, no assembler macro); `.data` at `0x20604000`,
  0x3b0 bytes; a 53-entry colour table in `.rdata` at `0x20603000`.
- Imports: host, GLOBAL, CHAIN, G3D, PANELS, UTIL.
- Win32: `DisableThreadLibraryCalls`, and `wsprintfA` -- the only direct
  USER32 call. Everything else goes through the host.

| slot | retail | who calls it | service |
| --- | --- | --- | --- |
| `+0x04` | `.text+0x020` | host | init: the two string pools |
| `+0x08` | `.text+0x040` | host | deinit: free them |
| `+0x18` | `.text+0x12b0` | WINDOW | `draw(a)`: G3D `+0x38(header+4, state)` if a model is loaded |
| `+0x1c` | `.text+0xfa0` | MAIN | `list(&count, &entries)`: every titled variant of every complete aircraft, sorted |
| `+0x20` | `.text+0x060` | MAIN | `select(a, title)`: find an aircraft by title into an empty record |
| `+0x24` | `.text+0x920` | MAIN | `release(a)`: let go of it, model and all |
| `+0x28` | `.text+0x970` | MAIN ×3 | `load(title)`: make it *the* aircraft, flight model and all |
| `+0x2c` | `.text+0xbf0` | MAIN | `unload()`: the reverse |
| `+0x30` | `.text+0xc50` | MAIN ×7, PANELS ×5, SOUND ×2, WINDOW, CONTROLS | `query(a, key, buf, len)`: a path or value of the aircraft |
| `+0x34` | `.text+0xcd0` | MAIN | `model_load(a, kind)`: 1 normal, 2 crashed |
| `+0x38` | `.text+0xf00` | MAIN | `model_unload(a)` |
| `+0x3c` | `.text+0xf80` | MAIN | `model_kind(a)`: which is loaded, 0 for none |

All are stdcall where they take arguments; `+0x2c` is a plain `ret`.

## The aircraft record

`a` is 12 bytes. GLOBAL keeps the user's at `+0x11e0`, and `+0x28`/`+0x2c`
always work on that one. MAIN keeps a second at `0x20807f90` and uses
`+0x38`, `+0x24`, `+0x20`, `+0x34` and `+0x3c` on it, in that order, to show
an aircraft without flying it.

| at | what |
| --- | --- |
| `a+0` | the visual model's header, which the `.MDL`'s own `+0x18` puts there |
| `a+4` | the model state, `header+0x18` bytes from the host |
| `a+8` | the record, 0x14 bytes, null until an aircraft is selected: variant, model kind, the `.MDL`'s Linkage, unused, and the folder (`AIRCRAFT\C182`) from a string pool |

GLOBAL `+0x11d4` holds the `.SIM` module's Linkage while there is one, and
`+0x11b6` is a word copied into each model state at `+0x60`.

## Keys: what `+0x30` resolves

`.text+0x320` turns `(folder, variant, key)` into a path. The key table is at
`.data+0x78`: twelve `{ string id, extension id, alias string id }`. The
strings are ENGLISH.FLL's. The host appends the extension's text later, when
the path is opened.

| key | resolves to | type |
| --- | --- | --- |
| 0 | `folder\aircraft`, if `aircraft.cfg` exists | `.CFG` |
| 1 | `[fltsim.N] title`, as text | -- |
| 2 | `folder\<sim>` | `.AIR` |
| 3 / 4 | the model folder / its `model.cfg` | dir / `.CFG` |
| 5 / 6 | panel | |
| 7 / 8 | sound | |
| 9 / 10 | texture | |
| 11 | `folder\<checklists>` | `.CFG` |
| 12 and up | nothing; `out` is cleared | |

For 3 to 10, the folder is `folder\model`, or `folder\model.<value>` when the
variant's `model=` names one. If that folder's `model.cfg` has an `[fltsim]
alias=`, the part comes from elsewhere. ACONTAIN tries `AIRCRAFT\<alias>\` if
its `model.cfg` exists there, and then `<alias>\` taken from the root. If
neither exists, the key resolves to nothing. So an alias names a part folder
(`C182\model`), not an aircraft. It is read from the first `.cfg` only and
does not chain.

An aircraft is **complete** (`.text+0x1f0`) when its `.AIR` file and its
model, panel, sound and texture folders all exist. The list shows only
aircraft whose variant 0 is complete. Selecting one requires the chosen
variant to be complete.

## Selecting, loading, listing

**`+0x20` select** walks `AIRCRAFT\` (`.text+0xd0`). A folder whose own name
matches the title, case-insensitively, is variant 0. Otherwise ACONTAIN reads
`[fltsim.0] title`, `[fltsim.1] title` and so on, stopping at the first
section with no title, so a gap ends the search. The first complete match in
listing order wins: the record is allocated, the folder interned and the walk
stops. An empty title clears `a` and fails. A record that is already set makes
it fail without looking.

**`+0x28` load** makes `title` the user's aircraft:

1. If there is one already, it remembers that aircraft's title and unloads it (`+0x2c`).
2. It selects `title`. Failing that, it looks the title up in `FLTSIM98.CFG`'s
   `[AIRtoCONTAINER]`, first as given and then with its path and extension
   stripped, so an old `C:\FS5\CESSNA.AIR` still finds its container.
3. Failing that too, it shows *Aircraft initialization failure.* (`0xf540`)
   and goes back to the previous aircraft. If even that fails, it alerts
   again with the previous title and gives up.
4. It reads chunk `0x100` of the `.AIR` file, the flight model's name, and
   loads `SIM\<name>.SIM` into GLOBAL `+0x11d4`. On failure: *Failed to
   startup the flight model.* (`0xf541`).
5. It loads model 1 (`+0x34`). On failure: *Visual model could not be
   displayed.* (`0xf542`).
6. Then PANELS `+0x18(a)`, which asks `+0x30` for key 6 and builds the panel,
   and CHAIN `+0x20(20, 0x60, 0)`.

**`+0x34` model_load** reads `[models] normal=` or `crash=` from the model
folder's `model.cfg` and loads `<model folder>\<name>.MDL` through host
`+0x26c`. Then it calls the `.MDL`'s own Linkage `+0x18(a)`, which leaves its
header in `a`. The header must start with `0x24` and carry `FSIM` or `AAF5`
at `+0x1c`, or ACONTAIN shows *This is not a FS98 aircraft model.* (`0xf544`)
and frees it. If `header+0x18` is non-zero, ACONTAIN allocates a zeroed model
state of that size and fills it:

| at | what |
| --- | --- |
| `+0x00` | its own size |
| `+0x40` | sixteen colour ids, from `model.cfg`'s `[colors] 00`..`15` by name through the `.rdata` table (`BLACK` is `0xf000`, ... ; 0 for an unknown name) |
| `+0x60` | GLOBAL `+0x11b6` |
| `+0x64` | the texture folder, from the texture pool |

The state goes to the `.MDL`'s `+0x1c`. `+0x38` reverses it: G3D `+0x58`,
then the `.MDL`'s `+0x20`, the module freed, the texture folder released and
the state freed.

**`+0x1c` list** builds one host block of `0xa08`-byte entries, one per
titled variant of each complete aircraft. Each entry holds the title at `+0`,
chunks 2 and 3 of its `.AIR` at `+0x100` and `+0x500`, and is cleared at
`+0x900` and `+0xa04`. The block is sorted by title with the host's
case-sensitive `strcmp`. MAIN shows it in dialog `0x1201`, calls `+0x28` with
the chosen entry's own title, and frees the block.

## The `.AIR` chunk reader

`.text+0x1180` treats the file as a tag, then `{ size, payload }` and the next
tag, and returns the first payload tagged `id`:

- Nothing bounds `size` against the output buffer.
- A short read zeroes `size` bytes of the output.
- A skip goes through host `+0x194`, which refuses any offset larger than the
  whole file. That ends the search there, so a corrupt size hides every chunk
  behind it.

## The string pools

`.data+0x39c` holds texture folders and `+0x3a0` aircraft folders. Each is a
sentinel node holding `""`, with a singly linked list of `{ next, refs, text }`
hanging off it. Interning a string already present adds a reference; releasing
drops one, **matched by text, not by address**, and frees the node at zero.

## The host services it brought in

Twelve, all read from the callee and now in
[include/fs98/host.h](../../include/fs98/host.h): profile strings on the
sim's own `FLTSIM98.CFG` (`+0x0c8`) and on a resolved path (`+0x0f0`), file
exists (`+0x158`), open/close/seek/read (`+0x17c`, `+0x13c`, `+0x194`,
`+0x19c`), `memset` (`+0x218`), `strncpy` (`+0x2ac`), `strcmp` (`+0x2bc`), and
load/free a module by path (`+0x26c`, `+0x270`).

Every one that takes a path and an extension id resolves it through
`0x40b350` into path + the extension's text from the host table at
`0x417f20`. The ids ACONTAIN uses are `0x0f` `.AIR`, `0x14` `.MDL`, `0x1b`
`.SIM`, `0x24` `.CFG` and -2, which adds nothing. Relative paths stay
relative, so an aircraft path is relative to the sim's current directory.
That includes `GetPrivateProfileStringA`'s, which looks in the current
directory rather than the Windows directory because the name contains a
backslash.

The `.SIM` and `.MDL` files are ordinary module DLLs with their own Linkage:
`SIM1.sim` is id 25 and the `.MDL`s are id 31. Those are two of the ids
[linkage.h](../../include/fs98/linkage.h) lists as unused by the retail
module set; they are used, just not from `Modules\`.

## Retail behaviour that is reproduced

- The colour lookup stops early on an alphabetical table, but only when the
  host's `_strcmpi` returns exactly -1. A name that sorts before an entry by
  more than that keeps searching. (The table is sorted, which was checked.)
- `+0x24` reads its argument without checking it, so a null record faults.
- `+0x28` never checks the chunk read or the `.AIR` query. A missing chunk
  `0x100` loads `SIM\.SIM`, which fails as a missing flight model.
- `+0x34` checks the module pointer, not host `+0x26c`'s return value.
- `+0x2c` unloads the visual model only if a flight model is loaded. Without
  one, `+0x24` unloads it, because the record still says it is loaded.
- The search for a title stops at the first `[fltsim.N]` with no title, even
  if later sections exist.
- A file in `AIRCRAFT\` with an extension the host does not know keeps the
  previous entry's type, so after a folder it is taken for a folder too.
  ACONTAIN then finds no `aircraft.cfg` and skips it.

## Where retail reads stack it never set

Three places, each started empty in the reimplementation, which is what
retail does when the stack is clean. The harness zeroes the stack before
each `+0x28` so retail sees the same.

- **`+0x28`'s previous title**, when there was no aircraft before, or when
  the previous one's variant has no title. If the new title then fails, that
  buffer is selected and shown in the second alert.
- **`+0x28`'s flight-model name**, when the `.AIR` file is missing, will not
  open, or has no chunk `0x100`.
- **The directory walk's entry type**, before the first find-next. The host
  leaves it alone for a file with an extension it does not know, so if that
  is the first entry, retail reads whatever the stack held. If that happens
  to be -2, the file is treated as a folder. The harness keeps such files
  off the front of the listing, because that read happens deep enough to
  pick up the harness's own stack.

## Limits the harness keeps inside

These would overrun retail's buffers. The overrun lands wherever each
compiler put the neighbouring variable, so the two builds cannot be compared
there:

- Paths over 0x100 characters. The host's path join writes up to 0x104
  bytes, and most of ACONTAIN's path buffers are 0x100.
- `.AIR` chunks larger than the buffer they are read into. That is 0x104 for
  chunk `0x100` in `+0x28`, and 0x400 for chunks 2 and 3 in a list entry. A
  truncated chunk is zeroed for its full stated size.
- A flight-model name with no terminator inside its chunk.
- A model state smaller than 0x68 bytes, because ACONTAIN writes `+0x60` and
  `+0x64` of it regardless.

## Verification

`make acontaindiff-run` (`tools/difftest/acontaindiff.c`) loads retail
ACONTAIN and the build into one process, with retail CHAIN behind a logging
wrapper. The stub host:

- allocates from a fixed arena with a size header for realloc;
- passes profile strings, file I/O and directory listing through to real
  files, behind transliterations of the host's join (`0x40a510`), split
  (`0x40a6c0`), find (`0x40a9a0`, `0x40ab60`, `0x40ac90`) and file services;
- reads its extension table out of `FLTSIM98.EXE` and its strings out of
  `ENGLISH.FLL`;
- hands out fake `.SIM` and `.MDL` modules. A fixture file's text (`MDL 7`)
  picks which fake, and the fake model's `+0x18` installs one of sixteen
  headers. Six of those are wrong in each way the check looks for, and three
  have no state.

Twelve aircraft trees are generated under `acfix\`. They have folders with
and without `aircraft.cfg`, variants with gaps and missing titles, duplicated
and case-flipped titles, part folders with suffixes, aliases of all three
outcomes, and colours in random case alongside near-miss names.
`[AIRtoCONTAINER]` entries are included. The `.AIR` files have chunks in
random order, junk chunks, truncated chunks, chunks too large to skip, and
empty files. The `.MDL` and `.SIM` files are good, bad or missing.

Each scenario picks a tree and runs a random script against each build from
its as-loaded image:

- loads, unloads and lists;
- MAIN's pick-from-the-list;
- queries of every key at several buffer lengths;
- selects, releases, model loads and unloads, and draws, on GLOBAL's record,
  MAIN's second one and null;
- MAIN's preview sequence;
- pokes of GLOBAL `+0x11b6`.

Compared: the transcript of every import call with its strings and results,
GLOBAL, MAIN's second record, the arena, and the `.data` image from `+0x78`.

The counts are from the retail run. `0xf543` is PANELS' alert and ACONTAIN
never raises it. Keys 12 and 13 are asked for and correctly refused.
Header 7 is never attached in these trees. It is a plain good header (`FSIM`,
with a state), the same kind as headers 1, 3 and 9, so no path goes
unexercised.

```text
ops: load 69058 unload 17206 list 21420 pick 25834 query 68631 draw 17088 select 34216 release 17015 model 47748 model_unload 25575 kind 17186 preview 51075 poke 17090
loads ok 15747, selects ok 21190, models ok 10386; listed 193650 entries, 7966 empty lists
alerts f540-f544: 56110 29550 23402 0 18514
.MDL loads missing/bad/ok 25582/689/42507, .SIM 18290/11260/39149
headers: 209 225 1414 207 6918 4975 3201 0 4462 2382 1643 1863 481 7205 6473 849
aliases found 139137: under AIRCRAFT 84516, as given 15335, neither 39286
colour compares: early out 22656, hit 209407, on 6414106; AIRtoCONTAINER hits 26912
chunks: short reads 14499, seeks refused 11825
query hits by key: 1675 1708 1623 1625 1674 1680 1709 1645 1707 1705 1657 750 0 0
20000 scenarios, 0 differ
```

Thirty-nine deliberate breaks were built one at a time, each into a DLL
checked to differ from the good one. Thirty-seven are caught. One of those
caught breaks makes chunks skip from the start of the file, which loops
forever; retail does not, so the hang counts as caught. The other two were
proven equivalent rather than assumed:

| break | result |
| --- | --- |
| `+0x20` with an empty title leaves `a+8` alone | equivalent: that clear runs only after `a+8 != null` has already returned, so it stores a null over a null |
| `+0x2c` unloads the visual model even without a flight model | equivalent: `+0x38` does nothing unless a `.MDL` is loaded, and a loaded one always has a non-zero kind in the record, so `+0x24` unloads it next anyway, with the same calls in the same order |

One break was missed at first and became a fix to the harness. **A list
entry's `+0x900` not being cleared** was invisible because the arena handed
out zeroes where the host's `malloc` and `realloc` hand out garbage. Blocks
the host does not zero are now filled with `0xa5`, and the break is caught in
the first scenario.

The other breaks, all caught:

- the key limit;
- each step of the alias search;
- the folder a directory key returns;
- each part of completeness, and the variant it is checked on;
- case in the folder-name match;
- the variant recorded;
- `+0x24` skipping the model;
- the `strncpy` length;
- the short-chunk zeroing;
- both colour early-out variants, the colour count, and the colour buffer reset;
- each header check;
- GLOBAL `+0x11b6`;
- which pool the texture folder goes in;
- reloading the same kind;
- the header cleared on unload;
- the `[AIRtoCONTAINER]` split fallback;
- the alert text;
- going back to the previous aircraft;
- the list-20 argument;
- the list's variant;
- sort order;
- reference counting;
- release by address;
- pool order at init and deinit;
- freeing an empty list;
- the draw arguments;
- the panel attach.
