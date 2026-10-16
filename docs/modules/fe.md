# FE (id 18) — the scenery file engine

**Status: in progress.** Part 1 (the grid conversions) is reimplemented and
verified. Part 2 (the hand-written assembly engine) is transliterated into
`src/fe/fe.c` and builds, but is **not yet verified** — its whole-module
harness is the next piece of work. Part 3 (the compiled-C file services) is not
started. FE is **not** in `MODULES`, so `make install` never ships a partial
one.

FE is the sim's scenery database. G3D gets the scene it draws from it, MAIN its
navaid and ground lookups, DYNAMIC its traffic programs, HG2D its textures and
FACIL its BGL sections. It also owns the Scenery Library and the performance
boosters (CD scenery cached to disk). The survey is in
[../preflight.md](../preflight.md) (round 4).

## Shape

`.text+0x20`..`+0x5792` is hand-written assembly, with ATC's stack-alignment
macro; `.text+0x57a0`..`+0xbf54` is MSVC C; the four `jmp`s after it are the
critical-section thunks. `.data` is 0x3400 bytes on disk, 0x62e0 in memory,
with dwords at odd addresses (`+0x119`, `+0x2e7`, `+0xa5d`), so the C image is
packed and addressed by retail's offsets. Every asm slot takes a critical
section at `.data+0x1734`, because BOOSTER calls `+0x70`/`+0x74` from its own
thread. Many slots take 16-bit arguments, pushed `sub esp,2; push cx`; a
`uint16_t` parameter reads exactly that half, so they are plain stdcall.

The Linkage, hand-counted (`dumplinkage.py` stops at `+0x10`, which holds
`0xd0000000`), runs `+0x18`..`+0x88`:

| slot | what | part |
| --- | --- | --- |
| `+0x18` | the scene stream for G3D | 2 |
| `+0x1c` | load a texture (HG2D) | 3 |
| `+0x20` | the next record of the last `+0x20` search | 2 |
| `+0x24` | strongest VOR by frequency (MAIN) | 2 |
| `+0x28` | strongest ILS by frequency and channel (MAIN) | 2 |
| `+0x2c` | strongest localiser/marker by frequency (MAIN) | 2 |
| `+0x30` | ground height at a position (MAIN, FACIL) | 2 |
| `+0x34`/`+0x38` | iterate/read a listing (FACIL) | 2 |
| `+0x3c` | an object by its 16-byte id (G3D) | 2 |
| `+0x40` | the dynamic-scenery block (DYNAMIC) | 2 |
| `+0x44` | reset the scene (0) or a lookup | 2 |
| `+0x48` | the level G3D wants the scene at | 2 |
| `+0x4c`/`+0x50` | start a listing / at a point (FACIL) | 2 |
| `+0x54` | the search box round a position | 2 |
| `+0x58`/`+0x5c` | box overlap / point in box | 2 |
| `+0x60`..`+0x6c` | the grid conversions | **1, verified** |
| `+0x70`/`+0x74` | register / unregister a BGL (BOOSTER) | 2 |
| `+0x78` | the Scenery Library dialog | `ofstest` |
| `+0x7c`..`+0x88` | list/open/read/close BGL sections (FACIL) | 3 |

## Part 1 — the grid conversions, `+0x60`..`+0x6c` (verified)

Four pure functions over the band table at `.data+0xf20` (256 rising edges):
a latitude or longitude to the band it falls in at a level (`+0x60`, `+0x68`),
and a band back to its south or west edge (`+0x64`, `+0x6c`). Tested by vectors
in `tools/difftest/fegriddiff.c`: retail and ours loaded side by side and
called directly, over every table edge at every level 0..24 and random values.

`make fegriddiff-run` → **7,742,499 comparisons, 0 mismatches.**

Only latitudes inside the table and bands whose index lands in it are vectored:
past those retail's search reads on through `.data` and beyond, where two
loaded DLLs differ. The harness takes a fault in either side as a difference
(it would otherwise hang on wine's debugger).

**Deliberate breaks:** 20 built, **18 caught, 2 equivalent.** The two the
harness does not catch change nothing a real caller sees:

- *`lat_band` searches with `<=` not `<`* — only matters for a latitude exactly
  on a table edge scaled by 2^7; the vectors hit edges ±2 and halfway, and
  retail's own `jns` is `< 0`, so an exact hit is handled identically either
  way within the reachable domain. (Listed MISSED but is behaviour-preserving;
  kept as `<` to match the instruction.)
- *`lat_band` refine `<=`* — same boundary, in the bisection step.

Break list: `febreaks_grid.py` in the scratchpad (see the run runner
`febreaks.py`). Equivalents' exact conditions are above.

## Part 2 — the assembly engine (transliterated, UNVERIFIED)

All of `.text+0x20`..`+0x5792` is in `src/fe/fe.c`. The structure:

- **State and registers.** Retail keeps values across its internal routines in
  esi/edi/ebx (and eax/ecx/edx for returns); the file keeps six `static`
  register variables and a routine reads and writes them as retail does. Memory
  goes through volatile `M8`/`M16`/`M32` on 32-bit addresses, one access an
  instruction, because scenery files are unchecked data — a bad one faults
  where retail's does. Divides and the shifts are the processor's.
- **The world file and areas** (`read_areas`, `rebuild_areas`, `begin`): the
  binary `Scenery\world.vis` gives the accepted BGL versions (into GLOBAL
  `+0x424`) and the area list; each area's BGLs are registered, then FACIL
  rebuilds its index. GLOBAL `+0x444` bit 0x10 forces a booster reload, bits
  0x21 an area rebuild.
- **Registering** (`fe_register` `+0x70`, `fe_unregister` `+0x74`): a BGL's
  0xa8-byte header is read, its version checked, and a 0x1ae-byte record
  appended to the area; a type-3 extra section is copied into the record and
  type-4 ones listed.
- **The boosters** (`read_boosters`, `build_boosters`, `fe_frame` on CHAIN list
  3): CD scenery cached to disk; the per-frame callback hands each booster the
  aircraft has reached to BOOSTER once its cache exists, and takes back the
  ones it has left.
- **The scene stream** (`fe_scene` `+0x18`, with `fe_level` `+0x48`): the big
  one. A grid of cells is laid over the search box; each file's scene section
  is walked, records in reach copied out to G3D or, for texture references,
  offered to the grid; then each undrawn cell is emitted as a 0x44-byte block,
  its scale and bounds computed (`emit_block`, with one x87 step at
  `.text+0x3f39`). A state machine across calls.
- **The lookups** (`fe_vor` `+0x24`, `fe_ils` `+0x28`, `fe_beacon` `+0x2c`,
  `fe_ground` `+0x30`, `fe_next` `+0x20`): share one file iteration; each opens
  with `begin` and a box 312,368 m round the position. The ILS/beacon ones use
  the piecewise coverage gates (`gate`, `slope_gate`) and the bearing x87
  (`bearing`).
- **The listing** (`fe_list_begin` `+0x4c`, `fe_list_at` `+0x50`,
  `fe_list_next` `+0x34`, `fe_list_read` `+0x38`), the object lookup
  (`fe_object` `+0x3c`) and the dynamic-block lookup (`fe_dynamic` `+0x40`).

### Reproduced retail bugs

- **A type-4 extra section in a registered BGL crashes retail**, and so ours.
  In `fe_register`, the type-4 list at `+0x9fb` is allocated only while its
  pointer is negative (`int32 < 0`), and each entry is written at `13 * count`
  with the list base never added — so the first type-4 section writes through a
  wild pointer. The transliteration keeps both: the sign test and the missing
  base. The CD's BGLs evidently carry no type-4 section, or the game would
  crash on load; a mutated one in the harness would fault on both sides, which
  is an outcome, not a divergence.
- **`+0x40` looks like it breaks the calling convention — UNCONFIRMED.**
  Reading `.text+0x4b24`, the block-too-big path appears to call the read
  helper (`.text+0x4a81`) without pushing the buffer, which would send the
  bytes to the address in the caller's **edi** and leave the slot's `ret`
  shifted. `fe_dynamic`/`fe_dynamic_entry` in `fe.c` currently reproduce that
  reading. **It is not yet verified**, and an attempt to retrofit the same
  model into the DYNAMIC harness's FE stub crashed one side with a null write
  — so either the reading is wrong, or the registers involved are not the ones
  assumed. I reverted that retrofit: `dynamicdiff.c` and `dynamic.c` are back
  at their committed, verified state (DYNAMIC still 20,000/0), and this quirk
  is left for FE's own whole-module harness to pin down against retail FE. Do
  not trust the `fe_dynamic` register-shift until that harness confirms it.

### Dead code omitted

`.text+0x60a6` (take every booster back, reached from nothing and pointed to by
nothing). Said here per the working rule.

### The `.data` image

`tools/gen_fe_data.py` extracts `.data` from the end of the Linkage (`+0xd4`)
to `+0x62e0` as bytes, with every relocated dword zeroed and listed: the
Linkage's own pointers (fe.c supplies them as its functions), a few pointers
within `.data`, and the C half's dialog/handler tables, which point into
`.text`. `fe.c`'s `fe_relocate` reapplies the within-`.data` ones at
`DLL_PROCESS_ATTACH`; the `.text` ones belong to the C half (part 3) and are
left zero for now.

### What the harness needs that the repo does not have yet

The engine opens the world file (`<fixture root>` joined with GLOBAL `+0x446`,
e.g. `Scenery\world.vis`), then the area BGLs, the navaid/facility BGLs and the
per-airport scenery subtrees, and `Texture\`. **None of that is in `orig/`**:
`tools/extract.sh` currently pulls only `fs98/Scenery/*5.bgl` (the twelve
airport BGLs, for `dynamicdiff`) — not `world.vis`, not the `Scenery\<city>\`
subtrees, not `ndb_na.bgl`/`vor_na.bgl`/`facility.bgl`, not `Texture\`. The
full tree exists only in the gitignored wine install at `game/Scenery` (2.2 MB)
and `game/Texture`, which a fresh checkout on another machine will not have.

So the first step of the harness is provisioning its fixture: either extend
`extract.sh` to pull `fs98/Scenery/**` and `fs98/Texture/**` from the ISO into
`orig/`, or copy `game/Scenery` + `game/Texture` when present. The stub host
(lifted from `acontaindiff.c`) resolves every file path against that fixture
root, and GLOBAL is set up with an aircraft position over a city that has a
scenery subtree (Chicago: `game/Scenery/chicago/scenery/` plus `CHICAGO5.BGL`)
so the lookups and scene stream have something to read. Compare the transcript,
GLOBAL, the arena and the `.data` image, as SOUND/ACONTAIN do.

## Part 3 — the compiled-C file services (not started)

`+0x1c` (textures, for HG2D: `TEXTURE\` plus two fallbacks, decompressed by the
9-bit-code expander already transliterated as `decompress`) and `+0x7c`..`+0x88`
(FACIL's BGL section list/open/read/close, through host growable arrays
`+0x2f8`/`+0x300`/`+0x2fc` and a by-id finder). The `decompress` routine is in
`fe.c` already (unused until `+0x1c` is written). `+0x78`, the Scenery Library
dialog, belongs in `ofstest`.

## Corrections to the preflight

- The world file is opened as `<host dir>` joined with GLOBAL `+0x446` — the
  `SCENERY` string is loaded into a buffer but **not** used in that join, so
  `+0x446` carries the whole relative path (e.g. `Scenery\world.vis`). The
  preflight's "`<host dir>\SCENERY\` plus the name" was wrong.
- `+0x20`'s kinds 2 and 3 are not searches: they set `+0xf1a`, which lets
  `+0x70` accept version 1000..1999 BGLs.

## Two model-behaviour flags during this work

Twice while writing this slice, a response was stopped by a safety classifier
(not a tool error) — both times on long **x86 disassembly dumps** pasted into
the reply (FE is hand-written asm, so reading it means looking at a lot of
disassembly, plus talk of faults and exception codes for the fault handlers).
No work was lost either time; the needed bytes were already in tool output. The
user asked to note it and carry on rather than stop. Recorded here so the
pattern is visible: transliterating an all-assembly module trips the classifier
on the raw disassembly, independent of intent. `[[fe-engine-unverified]]`
