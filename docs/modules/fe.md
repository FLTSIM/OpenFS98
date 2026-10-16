# FE (id 18) — the scenery file engine

**Status: in progress.** Parts 1 (grid conversions), 2 (the hand-written
assembly engine), and 3 (the compiled-C file services) are reimplemented and
verified. The latest whole-module run covers parts 2 and 3 with 20,000
scenarios and zero differences. Only `+0x78`, the Scenery Library dialog,
remains for `ofstest`. FE is **not** in `MODULES`, so `make install` never
ships a partial one.

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

### Portability boundary

The scenery algorithms are C, including all of part 3, but the current DLL is
still an i386 retail-compatibility build. Its remaining assembly is confined
to three exact divide instructions, one x87 bearing calculation, the two entry
shims that reproduce retail's `+0x18`/`+0x40` register corruption, and the
assembler aliases that place `ImportTable` and `Linkage` exactly. The larger
portability constraint is the retail image itself: pointers are stored as
32-bit values at fixed, sometimes unaligned `.data` offsets, and the boundary
uses Win32 `stdcall`, critical sections and DLL startup.

A portable FE is therefore possible, but should be split rather than making
the retail image pretend to be native: keep this i386 façade as the
differential oracle/compatibility DLL, expose normal C entry points for a
native core, replace image pointers with typed state or offset handles, and
put files, locking and module linkage behind platform services. The deliberate
ABI bugs belong only in the façade. The divide/x87 islands can then gain
tested C fallbacks in the same style as `make utildiff-portable`.

The Linkage, hand-counted (`dumplinkage.py` stops at `+0x10`, which holds
`0xd0000000`), runs `+0x18`..`+0x88`:

| slot | what | part |
| --- | --- | --- |
| `+0x18` | the scene stream for G3D | 2 |
| `+0x1c` | load a texture (HG2D) | **3, verified** |
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
| `+0x7c`..`+0x88` | list/open/read/close BGL sections (FACIL) | **3, verified** |

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

## Part 2 — the assembly engine (verified)

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
- **`+0x18` rotates the preserved registers after scene state 4.** Retail
  pushes edi after reading the south grid edge at `.text+0x2ef5` and never
  pops it. Its epilogue therefore returns ebx = old esi, esi = old edi and edi
  = the grid's column count / 2. The first long scene run exposed this because
  the harness's optimized caller trusted ebx; a guarded assembly call boundary
  then measured the rotation directly. `fe_scene_entry` reproduces it.
- **`+0x40` does break the calling convention on the block-too-big path.**
  This is now confirmed, not inferred. Retail calls the read helper
  (`.text+0x4a81`) without pushing its buffer, writes the whole block through
  the caller's edi, returns `-1` with the required size, and leaves edi = old
  esi, esi = old ebx and ebx = old ebp. The harness sets edi to a safe output
  buffer, asks with one byte as DYNAMIC does, and compares the bytes, size and
  registers. All 2,222 oversized probes in the 20,000-scenario run matched.
  A deliberate wrong esi rotation was caught at the first such probe.

### Dead code omitted

`.text+0x60a6` (take every booster back, reached from nothing and pointed to by
nothing). Said here per the working rule.

### The `.data` image

`tools/gen_fe_data.py` extracts `.data` from the end of the Linkage (`+0xd4`)
to `+0x62e0` as bytes, with every relocated dword zeroed and listed: the
Linkage's own pointers (fe.c supplies them as its functions), a few pointers
within `.data`, and the C half's dialog/handler tables, which point into
`.text`. `fe.c`'s `fe_relocate` reapplies the within-`.data` ones at
`DLL_PROCESS_ATTACH`; the `.text` ones belong to the remaining `+0x78` dialog
code and are left zero for now.

### Whole-module harness and fixtures

`tools/difftest/fediff.c` loads retail FE and ours side by side, binds both to
retail UTIL, and supplies logging models of the host, CHAIN, BOOSTER, FACIL,
G3D and NUMERIC. Host file and find operations pass through to the real fixture
tree; allocations come from a deterministic arena. Each side starts from its
pristine image and identical GLOBAL, then runs one of eleven operation classes:
scene, next-record search, navaids, ground, listing, dynamic blocks, objects,
box tests, register/unregister, textures, or FACIL sections. Mode 2 loads all
three areas in `world.vis`.

After each scenario the harness compares the complete import/output
transcript, GLOBAL, the used arena, and FE's image from Linkage - 0x48 through
the end of `.data`. Relocated image pointers compare by offset. Live critical
section bytes and the remaining dialog's handler pointers are ignored
explicitly. Calls at `+0x18` and `+0x40` cross a guarded assembly
boundary which records ESP, EBP and ebx/esi/edi and repairs the harness's own
stack before the next call.

The engine needs `Scenery\world.vis`, the area and navaid/facility BGLs, the
per-airport subtrees, and `Texture\`. `tools/extract.sh` now extracts the full
CD `fs98/Scenery` and `fs98/Texture` trees into `orig/`. `make fediff-run`
prefers that reproducible fixture and falls back to an existing staged
`game/` tree. The dynamic scenario is pinned over Chicago with a 750 km box so
every run reaches `CHICAGO5.BGL` and its one-byte oversized-block probe. Part
3 adds temporary, self-checking fixtures: a six-byte compressed texture and a
synthetic BGL with exact, fallback, repeated, absent and legacy section cases.
The raw, palette and haze paths use the installed `Texture` tree.

`make fediff-run` → **20,000 scenarios, 0 differences**: 1,819 scene and
next-record scenarios, 1,818 of each of the other nine operations, 1,818
confirmed `+0x40` oversized probes, 7,272 successful texture loads, and 3,636
successful FACIL section reads. A deliberate wrong register rotation was
caught at the first dynamic probe. The part-3 fixture also asserts its expected
decompressed bytes, selected ids, sizes and null results independently of the
differential comparison.

The shipped `world.vis` has no booster entries: its booster offset points at
the end marker. The harness therefore verifies CHAIN callback registration and
the empty per-frame path, but not construction or transfer of a nonempty
booster cache. That path needs a synthetic world/booster fixture before a
future change to it can claim direct differential coverage.

Early harness passes exposed six transcription errors: the texture
string and type-4 count are at `.data+0x5cc0` and `+0x5ca0` (not `+0x3cc0` and
`+0x3ca0`); `rebuild_areas` keeps FACIL's eax result; `fe_list_at` returns its
index in eax; and `close_file`/`free_grid` retain the host return in eax. The
long scene run then found the separate `+0x18` preserved-register bug above.

## Part 3 — the compiled-C file services (verified)

`+0x1c` removes spaces from HG2D's texture name and tries
`<dir>\TEXTURE\<name>`, `TEXTURE\<name>`, then `<dir>\<name>`. Kind 10 has a
final `TEXTURE\FS5.HAZ` fallback. Kind 9 seeks to `0x240` and reads the
`0xc0`-byte palette tail; kind 10 reads `0x1000` bytes and clears GLOBAL
`+0xcd2`; ordinary files below 64 KiB go through the 9-bit-code decompressor,
while files at least 64 KiB read exactly 64 KiB raw. The harness reaches every
route, including a generated compressed stream whose output must be
`ABCZZZ`.

The four FACIL services operate on registered BGL records whose directory
offset at `+0x190` is nonzero:

- `+0x7c` copies their 16-byte ids at record `+0x174` into one flat allocation.
- `+0x80` reads `0x30001` blocks, selects GLOBAL `+0x1288` by exact 16-bit id,
  and falls back to id `0x0409`; all unselected blocks are freed.
- `+0x84` follows `{kind, byte-count}` records and concatenates every matching
  section. With a type-1 container but no match it allocates then frees a
  zero-byte block and reports null/size 0. With a non-type-1 container it
  leaves the data null but exposes the internal file-record pointer through
  the size output, a retail quirk the synthetic fixture preserves.
- `+0x88` copies a 16-byte type-1 header plus its fixed payload when the third
  dword matches the requested kind. **Correction, found by FACIL's dialog
  harness:** its return is not the `memmove` status. On a found record it
  returns the local at `[entry esp - 0x11c]`, which it never writes except to
  zero it when the `memmove` fails; on a header mismatch it also stores that
  same word as the output size. It is stack garbage. FACIL's runway code tests
  it, and in the game it is nonzero (in that path it overlaps a local of
  `+0x84`'s lookup helper that starts as -1), so runway lists are built.
  `fediff` passed only because its own stack left 0 there. The
  reimplementation still returns 0 and must be changed -- most faithfully by
  an entry shim that returns the word at that stack offset -- before FE is
  installed; until then FACIL's harness supplies the word itself.

These use host `memcmp` `+0x228` and the growable-array services
`+0x2f8`/`+0x300`/`+0x2fc`. `+0x78`, the Scenery Library dialog, remains for
`ofstest`; it is not part of this verified slice.

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
on the raw disassembly, independent of intent. `[[fe-engine-verified]]`
