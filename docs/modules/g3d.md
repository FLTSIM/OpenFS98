# G3D.DLL — the 3D pipeline, a bytecode machine

G3D (module id 14) draws the scene. It is a virtual machine for FS5/FS98
scenery bytecode -- opcodes `0x00`..`0x93`, the language of BGL files and of
FE's scene stream -- with the 3D transform, clipping and span set-up the
opcodes share, a depth-sorted queue of objects, and its own built-in
scenery (the world's land classes, coasts, runways, the sky). It draws
through HG2D.

**Status: in progress.** Stages 1 to 3 of the plan (at the end of this
file) are done and verified: the image, the harness, the transform core,
init and deinit, the interpreter and the 50 opcodes that draw nothing, the
drawing core (the line and polygon clippers, the projection, points) with
the dozen opcodes that use it directly, and the services that need nothing
more (see *Progress*). See [../preflight.md](../preflight.md#g3d-id-14)
for the survey that chose it.

## Shape

- `.text` at `0x206e1000`, 0x16966 bytes: DllMain at `+0x0`, then
  hand-written assembly from `+0x20` to the end, ATC's stack-alignment
  wrapper around every import call (no observable effect; not reproduced,
  as in the other asm modules). About 23,700 instructions in some 577
  routines, 202 `lods`, 234 x87 instructions.
- **`.text` carries data**: jump tables indexed in place (`0x206e1322`, 32
  entries for `bsr`; 64 `.text`-to-`.text` relocations in all), so a linear
  disassembly shows `(bad)`/`retf` runs there. Read those as words.
- `.rdata` 0x98 bytes, then the export directory.
- `.data` at `0x206f9000`, 0x18450 bytes (0x18600 on disk): `ImportTable` at
  `+0x0` (ten imports), `Linkage` at `+0x58`, then all of the state, and
  the built-in scenery. 378 relocations: 224 code pointers, 154 pointers
  into `.data`. 6,633 relocations in `.text` point into `.data`: the asm
  addresses its state absolutely.
- Imports: HOST, GLOBAL, CHAIN, HG2D, UTIL, FE, WINDOW, WEATHERM (called),
  ATC and NUMERIC (in the table, never called). Win32: `CharLowerA`,
  `_CIacos`, `DisableThreadLibraryCalls`.

## The machine

- The program counter is the dword at `.data+0x126`; the interpreter is
  `0x206e25c0` (`.text+0x15c0`).
- **The opcode table** at `.data+0x4220`: 148 `{handler, length}` pairs,
  141 distinct handlers. The length word is `base` (low 16, signed), and
  when its top byte `mul` is nonzero, `+ ((word[at] * mul + 1) & ~1)`
  with `at` the signed byte at bits 16..23. Opcode `0x15` adds
  `(word[2]+1) * (word[4]+1) * 4` as well. Service `+0x64` is exactly this
  rule.
- Two more dispatch tables, `.data+0x4b9c` (23 code pointers, two
  families) and `.data+0x4839` (6, unaligned); 20 Linkage-adjacent code
  pointers at `+0x70`.
- **The built-in scenery** (`.data+0x4790` on): the palette `fs5     .pal`,
  land-class textures (`crops`, `city`, `swamp`, ... `.r8`), coasts
  (`coast0`..`coastf`, `coasti0`..`3`), `fs5world.r81`/`.r82`,
  `runway00.r8`, and bytecode programs from `+0x16180` whose operands
  **embed absolute code addresses, unaligned** (`0x206f28b7` at
  `+0x16186`), plus an unaligned 10-entry table of code addresses at
  `+0x16e32`. A reimplementation's image must carry its own addresses in
  those places, or the opcode that reads them must map them.

## Linkage

| slot | retail | callers | what |
| --- | --- | --- | --- |
| `+0x04` | `0x206e36c0` | | init (`0x206e3df0`, `0x206f431a`) |
| `+0x08` | `0x206e36d1` | | deinit (`0x206f431b`, `0x206e3e27`, `0x206e615e`) |
| `+0x18` | `0x206e2580` | MAIN | run a program `(program, context)`; context to `.data+0x1b7` |
| `+0x1c` | `0x206e2550` | WINDOW, MAIN ×7 | run a program `(program)` -- the sky and stars |
| `+0x20` | `0x206e1590` | WINDOW ×2, WEATHERM | draw a `'v'` model `(model)`, keeping the countdown `.data+0x15b` |
| `+0x24` | `0x206e15c3` | MAIN ×6 | the same with `.data+0x15d`/`+0x15e` cleared |
| `+0x28` | `0x206e15ee` | ANALYSIS | draw a `'v'` model `(model)`: header byte `0x76`, then the program from the next even address |
| `+0x2c` | `0x206f2140` | WINDOW | G3D's own frame from high up |
| `+0x30` | `0x206e5ff7` | WINDOW | `+0x34(0x7fffffff, 0x80000001, 1)`: the whole queue |
| `+0x34` | `0x206e600f` | WINDOW ×2 | draw the queue between `(far, near, flag)`; returns what it drew |
| `+0x38` | `0x206e5d60` | MAIN ×4, MULTIP ×2, ACONTAIN, DYNAMIC | queue a `'v'` model `(model, key)`, sorted, 0x2c-byte entries |
| `+0x3c` | `0x206e1536` | WINDOW | normalise a vector `(v)` in place (three dwords) |
| `+0x40` | `0x206e36e7` | MAIN ×2 | the dword at `.data+0x12a` |
| `+0x44` | `0x206e36f3` | HG2D ×4 | `(x * 2 << (GLOBAL[0xd42] + 8)) / GLOBAL[0x320]`, a 64-bit `idiv` |
| `+0x48` | `0x206f38b0` | none | |
| `+0x4c` | `0x206f38db` | none | |
| `+0x50` | `0x206f2220` | none | |
| `+0x54` | `0x206f078a` | none | |
| `+0x58` | `0x206f07ef` | ACONTAIN | |
| `+0x5c` | `0x206e1577` | WINDOW | once a frame: count `.data+0x15b` down, reloading from `+0x15c` |
| `+0x60` | `0x206f431c` | MAIN | |
| `+0x64` | `0x206e2850` | MULTIP | the address after the instruction at `(instr)`, or 0 |

Callers are from `impscan.py` over every retail module and the host. No
module calls `+0x48`..`+0x54`, and gauges import only PANELS, so those four
are reached, if at all, some other way; find out before transliterating
them. `dumplinkage.py`'s sizes for this module are gaps between Linkage
entries, not function sizes: most of the code is reached through the
opcode table.

## Variables

A variable operand is a signed word. With a context set (`.data+0x1b7`:
opcode `0x89`, or `+0x18`'s second argument) its address is context +
number. Otherwise the table at `.data+0x6950` gives it: 9,124 entries, to
`.data+0xf7e0`. Init (`0x206f1a90`) adds GLOBAL's address to every entry,
so an entry that was an offset into GLOBAL is that field's address and an
entry that was 0 is GLOBAL itself, which the lookup sends to a scratch word
at `.data+0xf7e0` (`0x206f1c5c`). So BGL variables are GLOBAL offsets, by a
whitelist; several numbers can name one field (nine name `+0x558`). Init
run twice adds GLOBAL twice. `0x4d`..`0x4f` take their target through the
table without the context, and `0x4f` reads both numbers unsigned.

A context is not cleared by `+0x1c`: it stays from one program to the next
until `+0x18` sets it again, a model (`+0x20`..`+0x28`) clears it, or `0x48`
with a zero operand ends it.

## Retail behaviour worth knowing

- **Three opcodes hang**: `0x01`, `0x4b` and `0x59` are a bare `ret` that
  does not move the counter, so the interpreter runs them forever.
- **The case opcode `0x03` reads its default 128 KB past itself.** A value
  at or past the count sets `bx` to `0xffff` -- index -1 in 16-bit code, the
  default just before the table at `+6` -- but doubles it in a zero-extended
  `ebx`, so retail reads the offset at `+0x20006`. Reproduced.
- **`0x8c` calls an absolute code address from the program**, which is why
  the built-in bytecode embeds G3D's own addresses. Ours maps them through
  `routine()`.
- **Some opcodes write into the program they run**: `0x3a`/`0x3c` exchange a
  one-shot word at `+8`, `0x18` rewrites a texture name.
- Opcode `0x47`'s scale stores the eye unshifted for operands above 32 (it
  jumps back into the store), and shifts by `32 - operand` below.
- **`0x07` outside a surface can leave the counter one byte on.** In the map
  (GLOBAL `+0xdca`) with a line colour other than `0x6900` it returns before
  reading its point, having moved the counter only past the opcode's first
  byte, and the program goes on from inside the instruction.
- **The two-word point form is unreachable.** A start or continue point is
  three words, or x and z with y from `.data+0x157` when bit 0 of
  `.data+0x156` is set (the byte is shifted down at every point). Nothing
  in G3D writes `.data+0x156`, and it is 0 on disk.
- **`0x36`, a row of points by a mask, goes on to where its last point
  left it.** Its mask walk moves on with `rol al,1; adc esi,1`: a byte a
  point, and one more each time the bit comes round, not a byte each eight.
  And its counter goes to `(ebx + 15) >> 4` past `+0x1e`, ebx as the last
  point left it: the screen x when it is drawn, the eye-space x (16.16)
  when it is out of view, the step's y when its bit is clear, and with one
  point and its bit clear the address of `.data+0x104`. Reproduced; the
  last can only match retail where G3D sits at its retail base, and
  `g3ddiff` emits `0x36` only where its last point is drawn at a known
  screen x, followed by nops it lands in.
- **So is the clippers' unclipped path.** Every clipper sends its vertices
  straight to HG2D when `.data+0xf6` is 2, but G3D only ever writes 1
  there, and it is 0 on disk. Ours keeps the path.
- **`0x27`'s vertex loop counts in all of ecx but restores only cx.** It
  saves its count with `push cx`/`pop cx` around each `0x206e3aef`, then
  `loop`s; the high half is whatever the polygon clipper left in ecx, the
  last vertex's y (HG2D `+0x44` keeps ecx), or its clamp, or 0. With a y
  of `0x10000` or more, or below 0, the facet walks on past its count
  through the words after it, as indices to points far outside the table,
  until ecx happens to come to 0. Ours reproduces it register for register
  (it needed `import_call` to enter the callee with ecx and edx as they
  are); `g3ddiff` gives `0x27` table points (`0x38`..`0x3f`) with y within
  `0`..z and z below `0x10000`, as the walk reads memory neither side can
  compare. The record is the opcode, a count, the plane (which
  `0x206e7c4c` reads without moving the counter, and which is also copied
  to `.data+0x1ab`..`0x1b3`), and the indices; facing away, it skips
  `0xe + 2 * count`.

## Retail crashes fixed on purpose

The rule is to reproduce retail's bugs. A bug whose only outcome is a crash
is the exception, decided with the project's owner: fix it, document it
here, and keep the harness clear of it.

- **Opcode `0x60`'s fast triangle** (`0x206f0cec`). With `.data+0xf4` set
  (opcode `0x7d`), outside the map and with GLOBAL `+0x12a4` bit 3 clear, a
  three-vertex `0x60` pushes its vertices for the recursive triangle
  (`0x206f0d6e`) with a `push edi` (the table point's address) that is never
  popped: six dwords a vertex where the triangle reads five, so it draws
  from a skewed frame with addresses among its coordinates; its caller then
  frees only `0x3e` bytes, takes the first vertex's z as its count for the
  counter, and returns to the first vertex's y as a code address. Ours does
  what `0x20`'s triangle and both quads do (push and pop around the loop).
  `g3ddiff` never generates a three-vertex `0x60`.
- **Opcode `0x7b`'s fast quad** (`0x206ec75e`). On the same conditions, a
  four-vertex `0x7b` pushes the quad's depth with `push 0` (`6a 00`, a
  dword) where `0x7a` and both triangles push a word (`66 68 00 00`), and
  frees `0x62` bytes after as they do: the quad (`0x206ecf0d`) reads every
  vertex two bytes off, and the two bytes left over make its handler pop
  the count from the wrong place and return into the middle of an address.
  Ours pushes a word. `g3ddiff` never generates a four-vertex `0x7b`.
- **Queueing a model whose first instruction is not an object**
  (`0x206e5eae`). `+0x38` and the queueing opcodes take a `'v'` model
  whose first instruction (after a `0x89` context) is `0x3a`, `0x4c`,
  `0x3c`, `0x2f` or `0x77`; for anything else retail sets the carry with
  the instruction's address still pushed, pops it into `.data+0x1b7`, and
  returns to the old context as a code address. Ours pops both and refuses
  the model. `g3ddiff`'s models always start with one of the five.

## Drawing

Lines and polygons are clipped in eye space against the four planes of the
view's cone, |x| <= z and |y| <= z, and projected through the view block at
GLOBAL `+0x1198` (`+0`/`+4` x's scale and centre, `+8`/`+0xc` y's) onto the
window at GLOBAL `+0x1194`:

- **Lines** (`0x206e168a`): trivially dropped by outcodes, else clipped end
  by end, five or ten tries; projected and sent to HG2D `+0x38`, the second
  end cached (`.data+0x2e6`) for the next to start from.
- **Polygons** (`0x206e1a1f` first vertex, `0x206e1b30` next, `0x206e1fd0`
  close): a four-stage Sutherland-Hodgman pipeline, each stage keeping its
  first and previous vertex and an outside flag in `.data`; the stages jump
  and call into one another's middles (each entry point is a routine in
  `g3d.c`). Survivors go to HG2D `+0x44` in clip space, x and y clamped to w
  (a w of 0 becomes `0x10000` at 0, 0); the close draws through `+0x40`
  unless GLOBAL `+0x2f0`, which HG2D's `+0x44` counts up per vertex, is
  negative after its decrement. The crossing (`0x206e18c0`) divides by the
  edge's extent, and degenerate edges are where the host's repair fires
  (`0x206e18c6`, the 17 faults seen in a flight).
- **Points** (`0x206e2439`): drawn when |x| and |y| (one's complement) are
  within z, through HG2D `+0x24` with a depth of z shifted by GLOBAL
  `+0xd42` over GLOBAL `+0x320`, an unsigned divide.

HG2D's line, polygon and vertex calls preserve eax, ecx and edx; its colour
lookup `+0x88` writes only a word through its pointer.

**Faces** test their side first: `0x206e7ce5` (a point and normal) and
`0x206e7c4c` (a plane) answer in the sign flag, which their callers branch
on straight after the call. Two more clippers follow the flat one's shape:
the **shaded** one (`0x206eb900`.., a shade per vertex, to HG2D `+0x74` and
`+0x78`) and the **textured** one (`0x206ed710`.., u and v, to `+0x48` and
`+0x4c`). Textured triangles and quads may take a **fast path**
(`0x206f0d6e`, `0x206f1277`) that splits them recursively, to depth 5 or
until the nearest z over the farthest passes 0.75 (`.data+0x178`), with
the shape passed on the stack; HG2D's shaded and textured calls do not
preserve eax, ecx and edx. A fourth clipper, **shaded and textured**
(`0x206ee2b0`.., `0x7a` and `0x7b`), carries a shade per vertex as well,
the high half of the table point's `+0xc`; it adds GLOBAL `+0xc94` and
`+0xc9c` to u and v (as dwords) when it emits, where `0x20`'s vertices add
the words at `+0xc96`/`+0xc9e` up front, and draws through HG2D `+0x50`
and `+0x54` (which hand over to `+0x44`/`+0x40` while GLOBAL `+0xcb6` is
clear). Its own fast triangle and quad (`0x206ec959`, `0x206ecf0d`) are
`0x20`'s with six dwords a vertex and their own depth, outcode and
midpoints (`0x206ff858`, `0x206ff85a`, `0x206ff7e0`..). In the map all
four families draw through the flat clipper. Face loops are `dec` then `loop` (or a word
`dec`/`jne` for `0x2a`): a face of one vertex walks four billion (65,536).

## Divides into the host's repair

G3D divides by zero and into overflow and relies on the host's filter
(`0x4119a0`, [host.md](host.md)): `0x206e18c6` faulted 17 times in one
flight, and `+0x44` divides by GLOBAL `+0x320`. The reimplementation
computes the saturated quotient without trapping, as WINDOW's does, and the
harness models the filter.

## Plan

1. **The image.** `tools/gen_g3d_data.py`: the whole `.data` image, with
   its pointers as tables -- data-to-data as image offsets, installed in a
   loop; code pointers (aligned and unaligned) as retail addresses, which
   `g3d.c` maps to its own routines and checks for completeness at load.
   The `.text` data the asm reads (the jump tables) extracted the same way.
2. **The harness first, small.** `tools/difftest/g3ddiff.c` after
   `landmediff`: retail G3D and ours in one process, each with its own
   retail FE over the real scenery, retail UTIL, logging stubs for HG2D
   (every argument; `+0x68`/`+0xa8` answer from the script), CHAIN (the
   callbacks recorded), WINDOW, WEATHERM and the host's 11 services, and
   the host's divide repair modelled. Compared: every import call, GLOBAL,
   the heap, and the whole image, with pointers into it as offsets and code
   pointers through a learnt pairing. Start with the self-contained
   services (`+0x3c`, `+0x40`, `+0x44`, `+0x5c`, `+0x64`) to prove the
   plumbing.
3. **The machine, in address order**, region by region as `window.c` is
   laid out: the transform and clipping core, the interpreter, then the
   opcode handlers, each family added to the harness as it lands. Programs
   come from three places: FE's stream over the real scenery, the
   built-in programs, and a generator that emits every opcode with chosen
   operands (count opcodes covered, not programs run).
4. **The queue and frames** (`+0x30`..`+0x38`, `+0x2c`), then `+0x48`..
   `+0x60`, then breaks (`tools/g3d_breaks.py`), then in-game.

## Progress

**Stage 1, verified.** `tools/gen_g3d_data.py` writes the image and its
pointer tables; `src/g3d/g3d.c` has the transform core (`.text+0x20` to
`+0x535`: the stream's points, relative to the eye with the overflow
recovery, the normaliser, the two rotations, the outcodes) and the services
`+0x3c`, `+0x40`, `+0x44`, `+0x5c`, `+0x64`. `make g3ddiff-run`: 20,000
scenarios, 0 differences, every register compared after every call. Every
normaliser branch is reached (magnitudes of every bit length, zero, the
clamp below bit 4, the right shifts at 29..31), `+0x44`'s shift count
masked to five bits 37,421 times, 37,738 divides repaired, and all 148
opcodes' length rules through `+0x64`.

Breaks: the saturation's sign, `+0x64`'s rounding, `+0x5c`'s reload test,
a leftover `ecx`, the clamp moved to bit 1 and its shift to 24 are all
caught. Moving the clamp's edge from 3 to 2 is **equivalent**, not a gap:
at bit 3 both branches shift by 25 (`0x1c - 3 = 0x19`), so retail's clamp
changes anything only for bits 0..2.

Two harness lessons, both from false differences in the first runs:

- **Compare pointer-valued registers as a pair, not per side.** Ours is
  relocated (retail holds the base), and a computed number that happens to
  fall in either side's 100 KB image range looked like a pointer on that
  side only. Registers go into the transcript raw and tagged; the
  comparison accepts two different values only when each points at the
  same offset in its own side's image.
- **Normalise the pristine copy the way the live image is normalised**
  before listing what changed: `.data+0x12a` is a pointer into the image.

**Stage 2, verified.** The interpreter (`0x206e25c0`), `+0x18`..`+0x28`,
init and deinit with all their parts (the variable table, the CHAIN
callbacks, the depth queue's allocation, the texture table, `+0x54`,
`+0x58`, `+0x60`, the library list's ageing), and the opcodes that draw
nothing: end, return, the jumps, calls relative, 32-bit, absolute and
through a variable, case, the ifs in range and masked, setting and moving
variables, the context, and the setters of GLOBAL's drawing state. `make
g3ddiff-run`: 20,000 scenarios, 0 differences, over 140,000 generated
programs with every one of those opcodes emitted over 100,000 times, and
1.5 million variable values steered onto the bounds and indices they are
compared with.

The generator keeps to programs that end: branches forward, subroutines in
slots of their own, none of the hanging opcodes, and `0x5b`'s addresses in
GLOBAL fields nothing else names, one field per call (two numbers can name
one field, and a subroutine that called itself through a shared one blew
the stack on both sides). Generation order is execution order, so once a
context may be set it may be set for the rest.

Harness lessons from stage 2:

- **Compare memory pairwise too, not only registers.** G3D makes pointers
  into its own image at run time (`draw_model` stores `.data+0x166` at
  `+0x161`), so the image, GLOBAL and the arena are snapshotted after every
  operation and compared side against side, a dword passing only when both
  point at the same offset in their own images.
- **A stub's leftover registers are the harness's, not G3D's.** G3D hands on
  what an import leaves in ecx and edx (`import_call()` keeps them), and a C
  stub leaves pointers into per-side harness state, so every stub returns
  through an asm wrapper that sets them to constants of its slot.
- **Grep a build for `Error` case-blind**: gas reports with a capital, and a
  failed link left a stale harness running for one round.

**Stage 3, verified.** The drawing core above and the opcodes that use it
directly: surface (`0x05`), start and continue points (`0x06`, `0x07`),
closure (`0x08`), `0x09`, points into the table (`0x0e`, `0x11` coloured),
starting and continuing at table points (`0x0f`, `0x10`), points (`0x37`,
`0x80`) and the bounding-sphere reject (`0x2c`). `make g3ddiff-run`: 20,000
scenarios, 0 differences, with 271,411 polygons, 118,434 lines, 16,536
points and 1.77 million clipped vertices compared call by call, and
661,309 divides repaired.

The harness gives each program a scene: the window record and view block in
GLOBAL (kept from the generator's variables), an eye near the origin and
usually the identity matrix, sane depth divisors with an occasional zero.
Lessons:

- **Model the callee's registers where it keeps them.** HG2D's drawing
  calls push and pop eax, ecx and edx, and `+0x44` counts the vertices in
  GLOBAL `+0x2f0`; until the stubs did both, no polygon was ever drawn,
  and 20,000 scenarios passed while checking none.
- **A retail tracer finds where the sides part.** `G3D_BREAK` plants `int3`
  at retail addresses and logs the registers there; a build with
  `-DG3D_TRACE` logs the same points from ours. It found the first stage-3
  difference in one run: a harness op that wrote a random value into the
  point table's pointer (`.data+0x12a`) for an earlier test, so both sides
  read memory at a random address at different times.
- **Find a minimal case first** (`G3D_SMALL`: one drawing per program).

**Stage 4 verified**: 20,000 scenarios, 0 differences (about 9.5 minutes),
with 207,263 flat, 103,992 shaded, 257,023 textured and 248,063 shaded
textured polygons drawn. Covered: faces (`0x1d`), facets
(`0x3e`), separation planes (`0x25`), shaded polygons (`0x0a`..`0x0c`,
`0x12`, `0x13`), shaded facets (`0x2a`) and the colour ramps (`0x50`,
`0x53`, `0x66`), and textured facets and faces (`0x20`, `0x60`) on both
paths -- 77,720 textured polygons in one run, with 3,524 fast triangles and
11,975 fast quads drawn. Every backface outcome, map and scale-word variant
is reached (counted with the retail tracer). Facets that keep their plane
(`0x27`) are verified too, all three paths reached (937 drawn, 906 facing
away, 12 shaded in 600 scenarios), and so are shaded textured facets and
faces (`0x7a`, `0x7b`) with the fourth clipper, both fast routines and the
map's flat path: 41,185 shaded textured polygons in 3,000 scenarios, the
fast routines' draws and splits reached for triangles and quads alike.

Two more harness lessons: a break reintroduced deliberately (the shaded
clipper's edx adjustment) is the quickest check that new code is covered;
and fixtures have to be able to draw -- until the point table was seeded,
every fast triangle indexed zero points and was culled, and the counts
showed it. A third: **a callee that keeps a register hands back the
caller's**, so the call has to be made with it. `0x27` loops on the ecx
HG2D's vertex call returns, which is the ecx G3D called it with;
`import_call` entered every callee with ecx 0 until then, and the loop ended
early.

**Stage 5, in progress**: the remaining opcodes. Verified so far, 1,000
scenarios at a time, every outcome reached: the colours `0x1e`, `0x2d`,
`0x2e` (which stores ah where it read al: the handler's address byte
`0x8a`, three times), `0x84`, `0x68`; the level of detail `0x56`; the size
jumps `0x5f`/`0x6d` (`0x206f1ee8`, an idiv by GLOBAL `+0xd4c` the host
repairs); `0x55`, whose x bound is half the low word of the view block's
address (ebx is loaded over the operand); and `0x70`, the eye inside an
area, through retail UTIL's atan2 (`+0x3c`), which `g3ddiff` now loads.
Then a weather effect to WEATHERM `+0x28` (`0x57`), the sun's shade
(`0x30`), a subroutine run as in the map (`0x3f`), a reject by table
points (`0x62`), the light record (`0x69`, `0x6b`, `0x6e`), programs kept
for later in the 64 lists at `0x20710360` (`0x74`, `0x8b`; not yet run),
a value at a point through HG2D `+0x28` (`0x91`), and the rows of points
(`0x31` into the table, `0x35`, `0x45`, and `0x36` by a mask -- see
below). And the positions: the object relative to the eye
(`0x206e686c`) and the eye relative to the reference and the object
(`0x206e69cd`), turned by a heading (`0x206e6aa8`), all through retail
UTIL (`+0x2c`, `+0x40`, `+0x48`, `+0x4c`); the box tests `0x73` and `0x1b`;
an object's shadow down the sun (`0x40`, which can stop the program) and
`0x82`, which runs `0x40` on a fake instruction at `0x206fd8e7` with the
bytes after its own opcode as the context -- reading its record six bytes
past its own end. The objects: an object's distance on the x87
(`0x206e694a`, the same instructions on x86, `FS98_PORTABLE_FP` for the
long double path), its sphere against the view in eye space (`0x206e1774`),
the eye from its offsets at a scale (`0x206e63ee`, counting overflows), and
the opcodes that place and test one -- `0x34` (which also sets the scale
word), `0x2f`, `0x77` (which writes its altitude into the program first),
`0x4c`, `0x83`, `0x3a`, `0x3c`. A range rejection counts down GLOBAL
`+0xe04`; an out-of-view one returns past the count. Their one-shot word at
`+8`, taken when `.data+0x17c` is set, names an entry in the depth queue,
which `g3ddiff` leaves 0 in the programs it runs and the queue fills in
when it draws.

**The depth queue** is in too: `+0x38` (`0x206e5d60`) queues a model,
prepared from its first instruction and sorted by distance (a binary
search, then a `std; rep movsd` to open the slot); full, it reuses the
farthest entry's place, and a refused model gives back the newest place
even then. `+0x34` (`0x206e600f`) draws the entries whose altitude offsets
fall in its range, farthest first -- all but the farthest when the queue is
at its limit -- each model run with its one-shot word set to its entry's
offset; `+0x30` draws them all and empties the queue. The queue's size
comes from the table at `0x206fd864` (250..700 entries) by GLOBAL
`+0xb60`, which `+0x38` and `+0x34` read unclamped (init clamps it to 4):
at 5 the "limit" is `0x206fd878` itself, 0 before anything set it, so a
queued model takes entry -1, the count goes below zero and the insert
writes before the queue. `g3ddiff` keeps the level in 0..4, as the game's
detail levels are. The opcodes `0x32`, `0x2b` queue at an offset,
`0x75` into the second queue (`0x206fd880`, which the scene builder draws,
still to come), and `0x5a` runs a subroutine nested and goes on past
itself (it queues only when the subroutine set `0x206fdc37`, which only
opcodes still to come do). `g3ddiff` keeps sixteen models at
`programs+0x10000` for the whole scenario, as the queue holds pointers to
them, and two more operations: `+0x38` and `+0x34`/`+0x30`.

**Shadows**: points into the table (`0x1a`, and `0x29` with a shade each),
which in the map are carried down the sun onto the ground (`0x206e7047`:
through the sun's matrix with `.data+0x1bd` set, scaled by `0x206fd8e7`
with `0x206fd8dd` set, each axis an idiv the host repairs, clamped to
+-0x7fff); and `0x4a`, which builds the sun's matrix from an object's
angles through WINDOW `+0x2c` -- `g3ddiff` now loads retail WINDOW for it
-- and runs a subroutine as a shadow while the sun is high enough.

**Meshes and lights**: `0x90` (compiled C) draws a mesh of table points by
byte index from a base, a fan of triangles with GLOBAL `+0x12a4` bit 4 and
polygons otherwise, behind its own plane test; `0x28` draws a light, a
point at size 1 and a blob through HG2D `+0x84` larger, sized by a rough
16-bit distance (`0x206f514f`) and a 16-bit idiv the host repairs (0x7fff or
0x8001, remainder 0x7fff).

**Transforms**: `0x33` (three angles), `0x3b` (a variable's record of
them; in the map only the heading) and `0x46` (a move of the eye and angles
plus variables' words) run a subroutine turned, the view state pushed and
popped round it (`0x206ef162`/`0x206ef1e0`: matrix, eye, sun, angles,
`.data+0x198`). The turn (`0x206ef25e`) takes UTIL's sine and cosine of
each angle and applies heading, pitch and bank to the eye and the sun (as
64-bit products `<< 17`, their high dwords) and to the view matrix (16-bit
products summed in 32 bits and doubled, the high word saturating on
overflow -- `0x7fff` when the doubling came out negative, `0x8000`
otherwise); each is skipped when its angle tests 0 **as a dword**, so the
word after it counts too. **Object shadows**: `0x41` builds the sun's
matrix from an object's angles (WINDOW `+0x2c`), turns the shadow's sun
vector (`0x206fd8ff`.., which the map's facing tests read) by them in
sign-and-magnitude 32x16 arithmetic, and runs a subroutine as in the map
with `.data+0x1bd` up; `0x46` with `.data+0x1bd` set (`0x206e7dce`) moves
by its offset carried onto the ground and turns the sun's matrix by
columns (`0x206e8030`). `0x206e7dce`'s branch for `.data+0x1bd` clear,
which would pass whatever cx, bx and dx hold to WINDOW as angles, is never
reached and is left out.

**Strips** (taxiways, roads): `0x69`/`0x6b` start one, `0x6a`/`0x6c`
(the same width), `0x78`/`0x79` (a width of their own) and `0x6f` (edge
lights) draw a segment to the next point (`0x206f48f0`): nothing when both
ends are out of view on one side; a quad, with a joint to the last
segment's far corners, when the eye is beside the segment, near an end, or
the width over the nearer end's distance squared (an unsigned 32x32
product, a 64-bit divide the host repairs) is 6 or more; a line at 2 to
6; nothing below. Then the centre-line dashes (the built-in program at
`0x20710ab8`, a `0x51` and a `0x35` over the segment's own fields) or, for
`0x6f`, rows of lights along both edges -- but `0x6f` reaches them only
from a quad, as its line path returns first.

**Textures by name**: `0x18` lower-cases its 8.3 name in the program
(USER32 `CharLowerA`, G3D's other import; OpenFS98 calls the same), reads
"red " and "side" as "side2" with GLOBAL `+0x12a4` bit 0, skips
`"water   .r8"` (`0x206ff8c6`) unless GLOBAL `+0x11bc` is set -- comparing
13 bytes with `repz cmpsb` and taking the count left, so a name that
differs only in its last byte counts as the same -- and finds its slot in
the texture table: the one cached in its operand, one with the name and
context, a free one, or the one aged most (failing when even that one is
younger than this one). A slot taken afresh is named but not loaded; one
taken from another texture is reloaded through HG2D `+0xa8`. With a
texture, GLOBAL `+0xc8c`, the offsets and WINDOW `+0x20` (the cofactors,
once a frame); then it opens a surface. `0x5d` opens one with the texture
there is.

**Grids of ground** (`0x15`): columns, rows, cell sizes, origin and height
range, then a texture word and a height a vertex. The box's eight corners
go first (as table points 0..7): all out of view on one side and the
vertices are skipped; the eye inside below the top draws the floor. The
vertices then fill the point table from 0 (a grid of more than the table
holds writes past it), and `0x206e8d73` visits the cells within 5000 of the
eye's, far rows and far cells first, the eye's row and column last,
through a routine whose address it keeps in `.data` (`0x206e9508`). Each
cell is split along alternate diagonals; a triangle is drawn (through the
textured fast triangle, its texture word's bytes as u and v) when a 64-bit
sum of the eye's offsets against its normal's terms has the right sign;
the cell under the eye with neither drawn sets GLOBAL `+0x832` to 2.

**The mesh compiler** (`0x38`, `0x206e35b0` and the compiled C below it to
`0x206e361a`): with GLOBAL `+0x12a4` bit 4 it turns itself into a `0x02`
and rewrites the next instruction it can take -- a surface of table points
(`0x05`, `0x0f`, `0x10`.., `0x08`), a facet `0x3e` or a face `0x1d`, of 4
to 150 points, past any `0x1e`, `0x7d` or `0x81` -- as a `0x90` mesh and
nops. It triangulates in single-precision floats: the points scaled by the
float at `0x206f8000`, ears found by an angle-weighted sum of normals
(`0x206e3090`, through MSVCRT's `_CIacos`) and a containment test
(`0x206e3260`), then packs the triangles as fan bytes from the lowest index
(starting the minimum at 0xff); when they do not fit it triangulates again
by fans (`0x206e2db0`) but packs with the first triangulation's count. The
face's plane distance is a dot product of zero-extended words. Because the
decisions turn on which float intermediates MSVC rounded and which it kept
at extended precision, **this block is carried over as its own
instructions** (`src/g3d/g3d_mesh.S`, generated by
`tools/gen_g3d_mesh.py`: jumps relabelled, `.data` references made
image-relative, the `.rdata` constants and dispatch tables copied), with
the CD's `MSVCRT.DLL` `_CIacos` transcribed beside it (the caller's
precision kept, rounding to nearest, exceptions masked while it works). A
portable C version, for builds without an x87, is still to come and will be
checked against it.

Harness lessons from the breaks of stages 1 to 4 (15 missed at first):
**a fixture has to reach the boundary a comparison sits on**, and random
values almost never do. `g3ddiff` now steers: counters seeded near their
turns (`0x206fd6d0`, `0x206fd1fe`, GLOBAL `+0xcbe`); table points on the
view's cone and at 3:4 depths (the fast routines' 0.75 exactly); rejects
whose point and radius the generator works out as `point_from_words()`
would, onto each of the sphere's boundaries, with a radius where sqrt 2's
last bit shows; plane distances at the eye's dot product, give or take one;
map faces at right angles to the sun, where the borrow shows. And **an
outcome has to stay visible**: a reject's skipped `0x09` only counts in a
GLOBAL word later instructions reset, so half of them skip a `0x87`
instead; the polygon closes log GLOBAL `+0xc90`, which HG2D reads. The
generator's own state (scene flags) must be set before it makes anything
-- twice the sides' programs parted because the models were made from what
the other side's last scene left. One break is a proven equivalence:
`sphere-part` (a partly-in sphere taken as wholly in), as both callers
test only for out.

Left out as dead, with no reference in code or `.data`: `0x206e11c8` (a
string copy), `0x206e11d8` (a per-axis step divided by `cx`), the
`call`/`retf` stubs at `0x206e155f`..`0x206e1576`, and `0x206e1404`, which
only one of those stubs calls; the `call`/`ret` wrappers at
`0x206e1742`..`0x206e1773`; and `0x206e3cc8`/`0x206e3ce6` (reached only
from themselves) and `0x206e3db5`.
