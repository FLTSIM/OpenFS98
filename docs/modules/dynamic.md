# DYNAMIC.DLL — module id 6

**Reimplemented and verified.** DYNAMIC is the sim's dynamic scenery: the
airliners that taxi, take off, fly a circuit and land at the big airports,
the parked aircraft, and the fuel truck at Meigs that drives out to the
user's aircraft and fills its tanks. Each object is a program in a small
bytecode carried by the airport's scenery file. DYNAMIC is the interpreter:
it gets the block of programs nearest the aircraft from FE, makes an object
for each declaration whose scenery class is switched on, steps every
object's program each frame, and hands the objects to G3D to draw.

`src/dynamic/dynamic.c` is the transliteration and the module shell together.
`make dynamicdiff-run` drives retail and the OpenFS98 build side by side over
random scripts against the twelve real blocks on the CD, often mutated --
**20,000 scenarios, 0 differ** -- comparing DYNAMIC's whole `.data` image, GLOBAL and the
heap as well as every call it makes. `make smoke` passes with DYNAMIC
installed beside ACONTAIN, ATC, CHAIN, GLOBAL, NUMERIC, SLEW, SOUND, UTIL and
WEATHERM: 27/27 modules, 95 gauges. That shows the sim loads it and reaches
the panel with it; the smoke test does not check that dynamic scenery was
switched on in that boot.

The reasons it came next are in [../preflight.md](../preflight.md), round 3.

## Binary and linkage

- Retail image base `0x206a0000`; `.text` 12,300 bytes of hand-written
  assembly -- register-passing, results in the carry flag, the
  `mov [esp-7],0x4050607` / `and esp,-4` stack-alignment macro around every
  external call that ATC and WEATHERM use too; `.data` at `0x206a6000`,
  0x250 bytes.
- Imports: host, GLOBAL, CHAIN, FE, G3D, UTIL, MULTIP. CHAIN and UTIL are
  done; FE, G3D and MULTIP are stubbed by the harness (below).
- Win32: `DisableThreadLibraryCalls` only.
- One x87 sequence (`.text+0x3fd3`, the path interpolation: `fild`,
  `fimul`, `fistp`), so the result depends on the precision control.

| slot | retail | who calls it | service |
| --- | --- | --- | --- |
| `+0x04` | `.text+0x1020` | host | init: nothing |
| `+0x08` | `.text+0x1027` | host | deinit: off CHAIN, everything freed |
| `+0x18` | `.text+0x1188` | MAIN | the frame (below) |
| `+0x20`..`+0x12c` | | DYNAMIC itself | the dispatch table: opcode n's handler at `+0x20 + 4n` |

That is why `dumplinkage.py` reports 76 entries and 52 functions: 68 of
the slots are opcode handlers, 49 distinct, and the 2-byte one
(`stc; ret`) stands in 19 empty slots. The interpreter calls through the
table in `.data`, so the reimplementation does too, with retail's sharing
kept: one C function wherever retail repeats an address.

Three CHAIN callbacks, registered on the first frame:

| list | retail | what |
| --- | --- | --- |
| 16 | `.text+0x14f6` | draw: each live object whose class is on and whose density level is within GLOBAL `+0x1190` goes to G3D `+0x38` |
| 4 | `.text+0x14ca` | the block: kept while the aircraft is inside its bounds, else fetched from FE; then its declarations run |
| 3 | `.text+0x1309` | the table of aircraft: the user's from GLOBAL, MULTIP's players by id |

## The block

FE `+0x40` hands over a block it reads from the airport's BGL. FE's walk,
reproduced by the harness to extract them: BGL header `+0x52` is the
offset of a list of 9-byte areas (`0x14` continues the list elsewhere),
each pointing at 11-byte records; type 4 is dynamic scenery, its size in
the last word, the block after it. The CD has one block in each of the
twelve `*5.bgl` city files (Chicago's has 55 declarations; `dynlib.bgl`
holds the library objects they draw).

| at | what |
| --- | --- |
| `+0x00`..`+0x06` | bounds, four signed words: latitude `>>9` max and min, longitude `>>16` max and min |
| `+0x20` | declarations, until an opcode above `0x43` |
| after | the programs, addressed relative to their declarations |

A declaration is opcode `0x32` (8 bytes: library id, 16-bit program offset,
slot, flags) or `0x35` (0x18 bytes: 32-bit program offset, slot, flags,
then a 16-byte library object id); `0x34` among them sets the latitude the
metric conversions are about. The flags byte's low nibble is a density
level, compared with GLOBAL `+0x1190`; its high nibble a mask over the four
scenery switches at `+0x1180`..`+0x118c`. The slot word is DYNAMIC's: the
offset of the object it made, written back into the block.

FE is asked when the aircraft leaves the block's bounds, or, with no
block, a box of 5000 lat units and 0xe0000 lon units about where it last
asked. The first `+0x40` call has a 1-byte buffer and only learns the size.

## Objects

Each object is 0x120 bytes in an array (count first) that grows by realloc
to at most 0xe3, so object pointers are not stable: the offset is what is
kept, and every object's draw record is re-pointed after a grow.

| at | what |
| --- | --- |
| `+0x02`/`+0x04`, `+0x0a`/`+0x0c`, `+0x12`/`+0x14` | position: latitude, longitude, altitude, 48-bit |
| `+0x18`, `+0x1c`, `+0x20` | pitch, bank, heading |
| `+0x24` | flags: 1 hidden from `blocked`, 4, the GLOBAL `+0x1172` bits at 4-6, the level, the declaration's byte above |
| `+0x28`, `+0x2a` | timers counted down by the frame time |
| `+0x2c` | size, 6 |
| `+0x30`..`+0x3f` | eight parameter words (`0x31`), `0xf004` at first |
| `+0x42`/`+0x44` | base longitude |
| `+0x4a`, `+0x52`, `+0x5a` | bases the moves change: latitude, eastward metres, altitude |
| `+0x60` | the countdown |
| `+0x64` | the declaration (non-zero: live) |
| `+0x68` | program counter |
| `+0x6c`, `+0x74` | return addresses, from a call and from generated code |
| `+0x70` | generated code |
| `+0x78` | the aircraft linked by `0x3d` |
| `+0x7e`.. | move targets |
| `+0x94`, `+0x96` | turn and ramp state |
| `+0x98`..`+0xef` | path: the point before, the point after, and a pen the generators write with |
| `+0xf0` | the draw record G3D takes |

## Every frame: `+0x18`

Nothing while GLOBAL `+0x268` is set. With all four switches off it tears
down. On the first frame it registers on CHAIN and allocates the aircraft
table. Then the time, `(+0xc1a << 8) * +0x2aa >> 16`, is cut into slices of
about 0x6000 at most; for each slice the step time and clock advance and
every live object steps once. An object not on a path then takes its
position from its bases, the longitude through UTIL `+0x44`.

The slice loop ends when the time is used up -- or on the x86 `loop`
instruction's ecx reaching zero. With objects, the stepping leaves ecx zero,
`loop` wraps it and goes on. With no object array it leaves ecx untouched,
and the loop counts down whatever ecx was: the caller's, or, on the frame
that registers with CHAIN, whatever CHAIN's add left in it. The
reimplementation's Linkage slot is a stub that passes ecx in, and the CHAIN
registration goes through a thunk that hands back CHAIN's ecx.

## The interpreter

One step of an object (`.text+0x168a`): if something is in its way
(`blocked`: the user's aircraft or another object within their sizes and
two, within 5 of height, within 60 degrees of the heading) nothing happens.
Otherwise opcodes run from the program counter while each handler returns
carry clear and `S_RUN` is set, storing the counter after each. A handler
that sets carry ends the step -- unless it is below 0xa (the easing moves),
when `S_RUN` goes and the following opcodes run on without storing the
counter: that is how several eases proceed in one step. With `S_RUN` clear
nothing above 0x23 runs.

| op | bytes | what |
| --- | --- | --- |
| `00`,`08`-`11`,`21`-`23`,`28`,`2f`,`36`,`37`,`3c` | 1 | empty: carry set, counter unmoved |
| `01`-`03` | 5 | ease pitch, bank, heading toward a target at a rate |
| `04` | 2 | flag 4 |
| `05`-`07` | 3 | set pitch, bank, heading |
| `12`-`18` | 5-9 | timed moves of one, two or three bases |
| `19`-`1f` | 7-11 | the same, the rate ramped down from a start |
| `20` | 7 | turn to a heading about a circle |
| `24`, `25`, `26` | 3,3,1 | jump, call (one level), return |
| `27` | 3 | wait |
| `29` | 1 | hold here until released |
| `2a` | 3 | release the object another declaration made |
| `2b`-`2e` | 7 | jump if a setting (switch, density, two weather words, time of day) is `!=`, `==`, `>=`, `<=` a value |
| `30` | 1 | nothing |
| `31` | 4 | a parameter word |
| `32`, `35` | 8, 0x18 | a declaration |
| `33` | 0x13 | place |
| `34` | 7 | the metric conversions' latitude |
| `38` | 1 | start a path here |
| `39`, `3a`, `3b` | 0x1f, 0x1f, 0x20 | a path point, 1, 5 or n units on |
| `3d` | 0x11 | wait for an aircraft at a point, then call on |
| `3e` | 5 | drive to that aircraft, refuel it, come back (generated) |
| `3f` | 5 | refuel |
| `40` | 0x1a | when something is in a box: take off, fly a circuit, land (generated) |
| `41` | 0x17 | come round onto a heading at a point (generated) |
| `42` | 2 | hidden from `blocked`, or not |
| `43` | 0x19 | jump unless something is in a box |

The CD's programs use 36 of these, besides the declarations. `0x3d`..`0x3f` appear only at Chicago:
the fuel truck meets an aircraft within 16 of a point, waits, drives a
little, runs `0x3e`, which writes a program into a 0xe88-byte block --
the way out, round the aircraft at its speed radius to a stop, `0x3f` with
the rate, round and back -- and runs it. `0x40` writes 0x3c8c bytes,
`0x41` 0x1838. The three generators write their way back backwards from
the end of the block and join it to the way out with one `0x3b`, then
shrink the block.

## The aircraft table

Record 0 is the user's: GLOBAL's position (`+0x560`..`+0x583`), `+0x5dc`
(0x3d wants 0), `+0x2b4` (0x3d wants at most 0x7fff), the kind at `+0x609`
and a speed from the record at `+0x11e0` (10 without one), plus 3. The rest are MULTIP `+0x24`'s players with `+0x08 == 1` and
`+0x0c == 0`, matched by id; a player's id of 0 matches record 0, whose id
is never set, and overwrites the user. Records not seen this time are
cleared. While an object has the user (`S_LOCK`), list 3 zeroes GLOBAL
`+0x348`; refuelling the user sets it to 5 when done.

## Refuelling: `0x3f`

At a fixed rate, the user's five tanks (`+0xb74`..`+0xba0`, level and
capacity) fill until every tank with a capacity is full. With GLOBAL
`+0xc44` set, each tank fills in proportion to its room, at a rate from the
op and `+0xc44` -- and the total room leaves out the first tank: its room
is stored and then overwritten by the second's (`mov` where the others
`add`). A first tank with more than twice the others' room overflows the
divide and faults, in retail and here. A player just waits out a
countdown.

## Retail behaviour that is reproduced

- **The empty opcodes below 0xa spin.** `0`, `8` and `9` set carry without
  moving the counter, and below 0xa the step runs on from the same place,
  forever. A program counter that strays onto a zero does the same.
- **Nothing checks a program.** Jumps, calls and returns go where the
  bytes say; `0x31`'s index is a byte, so it writes up to 0x1fe past the
  parameters -- into the next object at 0x8a; a declaration run from a
  program leaves edi on the next object, or on GLOBAL, and the program
  carries on writing there. Divides fault on zero and overflow.
- **`0x20` steps with `add si,7`**: only the low 16 bits of the counter.
- **The first ramped step after a path moves by the altitude.** The ramped
  moves (`0x19`-`0x1f`, `.text+0x1abd`) keep their offset in ecx across the
  timer; when the object has just come off a path the timer's `path_end`
  (`.text+0x1df2`) uses ecx as scratch and leaves the altitude's whole part
  there, and the target becomes the base plus its low word. It happens
  whenever a ramped move is the first thing after a path or generated
  code; the harness found it on a mutated Kansas City program.
- **The frame's loop counts the caller's ecx** with no object array (above).
- **The refuel total leaves out the first tank** (above).
- **A player with id 0 overwrites the user's record.**
- Dead code left out: `mov eax,[ebp-8]; cdq; sub eax,edx` at
  `.text+0x3cbe`, whose result nothing reads.

## The harness

`make dynamicdiff-run` (`tools/difftest/dynamicdiff.c`): both builds in one
process, driven as the sim drives them -- `+0x18` every frame with a
chosen ecx, `+0x04`/`+0x08`, CHAIN lists 3, 4 and 16 dispatched by retail
CHAIN. UTIL is retail's behind forwarders that fold every argument and
result into a digest logged after each step of the script. FE is a stub
serving the CD's twelve blocks as FE reads them (the probe, then the read;
with GLOBAL `+0x3e0` bit 0 clear the probe moves past the record, as FE's
does). G3D logs each draw with a checksum of the object; MULTIP answers
with the scenario's players. Each scenario runs under a random x87
precision control.

Compared after each scenario: the transcript, GLOBAL (0x20000 bytes, room
for objects run past it), the heap, and DYNAMIC's whole `.data` image, code
pointers through a pairing learnt as first seen.

The programs are unchecked, so the harness has to survive them:

- **Guarded memory.** The heap and GLOBAL are VirtualAlloc regions with
  uncommitted guard space around them; a runaway program or generator
  faults at the edge instead of writing over the harness. The heap's
  first allocation is 64K in, so 16-bit offsets back from it stay inside.
  Unallocated heap reads 0xa5, past every opcode, so a stray counter stops.
- **Faults.** A vectored handler catches access violations and divide
  faults in DYNAMIC, CHAIN or UTIL, logs them and unwinds to the script.
- **Hangs.** A watchdog thread moves a call spinning past a second inside
  the module out of it. Where a spin is cut is timing, so the state after
  it is not comparable: the scenario ends there and only the transcripts
  are compared, which must both say `HANG` at the same point.

Steering, each added because a deliberate break showed the gap:

- the aircraft on the points the programs test (MULTIP's players too):
  `0x3d`'s meeting points
  (low, on the ground, a kind it takes, with the table refreshed), the
  boxes of `0x40` and `0x43` -- exactly on the long edge, found by
  rotating with retail UTIL itself -- and on or just past each of the
  block's four bounds;
- four synthetic blocks: 0xe6 declarations (past the 0xe3 limit; the
  stub host grows its top block in place, as a heap does); one object
  easing through distances its rate covers exactly, with the frame time
  held; a fuel truck at a known point, the aircraft put 99, 100 or 101
  from it, and tanks a whole number of refuel steps from full; and a
  circuit from a box the aircraft can be put in -- its speed about the
  clamps, its starting bank one of the 76 in 65,536 from which a turn's
  heading lands where its threshold decides -- beside a `0x41` whose two
  candidate points are mirror images;
- Seattle's paths, the only ones the CD's programs reach, ended early on
  an emptied second point;
- mutations that keep a block's structure: operands, opcodes of the same
  length, empty opcodes (rarely the spinning ones), `0x31` writing onto
  the next object's flags, a declaration inside a program, other opcodes
  and a wait in the declaration list, conditional jumps aimed at the time
  of day and the weather words with values on, beside or of the other sign
  from GLOBAL's, `0x3d`'s wanted kind past 3, `0x40`'s speed about its
  clamps;
- an object running generated code switched off;
- the block placed in the heap so one of its turns sits across a 64K
  boundary, where `add si,7` keeps only the low half;
- a MULTIP player, not the user, met by the truck, its countdown a whole
  number of frames.

UTIL is reached through asm thunks that keep ecx and edx (and eax where
UTIL does): retail DYNAMIC counts loops in ecx across UTIL calls.

## Verification

`make dynamicdiff-run`: **20,000 scenarios, 0 differ** (about 15 minutes;
`N=2000` for a quick check). The harness is described above and in the
header of `tools/difftest/dynamicdiff.c`.

### Coverage

From the 20,000: 2.48 million frames, 41,556 blocks loaded, 17.5 million
draws, 34.6 million live objects observed between frames, up to 227 at
once; the user's aircraft held by a fuel truck in 33,925 of those
observations, its tanks changed in 170 scenarios and GLOBAL `+0x348` set in
11,316; 2,733 circuits generated; 3,934 faults and 395 hangs matched on both
sides (a hang ends its scenario: transcripts compared, state not). Objects
were found stopped at 55 different opcodes; one that always clears carry
(the jumps, `0x3d`, the declarations) never stops a step, so cannot be
among them. 4,203 blocks were placed with a turn
across a 64K boundary and 1,787 paths ended early on an emptied point.

### Deliberate breaks

95 breaks of `dynamic.c`, each built into its own DLL (checked to differ
from the good build) and run against the final harness at 5,000 scenarios
(8,000 for the last three): **87 caught, 6 proven equivalent, 2 not caught.**
They cover the frame's slicing and its ecx, the stepper's rules, `blocked`,
the aircraft table, the block's bounds and box, declarations and the object
limit, every opcode group, the paths and their x87 interpolation, the three
generators and the refuelling, and the reproduced retail behaviour (the
16-bit `si` step, the refuel total, the ramp after a path).

Proven equivalent:

- **The aircraft table's countdown compared unsigned.** `+0x3c` is allocated
  zeroed, set only to 0x5a (refuelling) and decremented only while positive,
  so it is never negative.
- **`0x34`'s carry and the timer's floor tested `>=`.** The carry is from
  `add esi,7`, which needs a counter within 7 of 2^32 (user-mode addresses
  are below 2^31); at equality both forms of the floor store the same value.
- **`0x04` masking with 6.** The byte is forced to 0 or 4 first.
- **`emit_step`'s duplicate stores dropped.** Retail writes the same two
  values to the same two addresses twice, with nothing between.
- **`0x41`'s 16-bit `neg` done 32-bit.** Only the low half is ever used.

Not caught, with the only conditions under which they could show:

- **`0x18`'s three moves in another order.** They touch disjoint fields, so
  the order shows only if the first move's divide faults before the second
  runs -- a base more than 32,767 units from its target mid-move with a
  time slice of a few units, which only a stray write from `0x31`'s
  unchecked index can arrange.
- **Backward emission without its extra step for a repeat count.** Every
  generator leaves `S_REPEAT` at 1 after each point, so a backward point with
  a count above 1 needs a fault inside `emit_step` between a generator
  setting the count and the point being written.

The first pass caught 61 of 94. Each of the rest pointed at a gap, and each
steering listed under "The harness" is there because of one; the slowest
catches are the turn threshold (2,848, from 76 of 65,536 starting banks),
the selector-5 offset (1,750) and the player's refuel countdown (1,347).

Two real differences in the reimplementation were found on the way, both
reached only once the steering got there: refuelling kept a bit that
`xor eax,eax` clears before `rcr` (`method.md`, "a flag can be gone before
it is read"), and the ramp ignored what `path_end` leaves in ecx. And one
false difference, from the harness: its UTIL forwarders were C, which may
clobber ecx, and retail's loops count in ecx across UTIL. They are thunks
that keep what UTIL keeps now (`method.md`, "a stub has to keep what the
real callee keeps").
