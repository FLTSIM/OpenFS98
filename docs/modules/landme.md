# LANDME.DLL — landing assistance

LANDME (module id 26) is reimplemented, verified and running in-game. `make
landmediff-run` drives retail and OpenFS98 through 20,000 whole-module scenarios
over the real scenery fixture: zero differences. The run reached all seven
controller states and all eleven nonzero guidance phases, found 18,881 runways,
exercised the no-runway path 1,119 times, showed 182,185 messages and posted
command `0x100d7` 3,061 times. With the module installed, `make smoke` reaches
the panel with 27/27 modules and 95 gauges.

## Shape and linkage

Retail has 4,094 bytes of hand-written assembly and one 0x854-byte `.data`
image. State and constant tables occupy `+0x000`..`+0x7f7`, followed by
`ImportTable` at `+0x7f8` and `Linkage` at `+0x838`. The generated
`landme_data.inc` retains the complete image and zeroes only relocated code
pointers, which `landme.c` installs when the DLL attaches.

Imports are host, GLOBAL, MAIN, CHAIN, FE, UTIL and WINMSG. Linkage contains
only three functions:

| slot | retail | purpose |
| --- | --- | --- |
| `+0x04` | `.text+0x020` | register the frame callback on CHAIN list 3 |
| `+0x08` | `.text+0x050` | remove that callback |
| `+0x18` | `.text+0x080` | start landing assistance when GLOBAL `+0x84c` is active |

Starting sets bit `0x100` in GLOBAL `+0x1a` and moves the controller from idle
to setup. Repeated starts while it is running do nothing.

## The seven-state controller

The callback at retail `.text+0x0af` runs every frame:

| state | behavior |
| --- | --- |
| 0 | idle; only update the requested speed cap |
| 1 | search FE for the nearest runway and initialize the approach |
| 2 | choose a guidance phase from the runway-local script |
| 3 | follow that phase until its signed boundary comparison fails |
| 4 | finish, clear the active flags and restore GLOBAL `+0x278` |
| 5 | delay the runway announcement and the transition to state 2 |
| 6 | show the abort message, then move to state 4 |

GLOBAL `+0x84c` becoming zero forces state 4. GLOBAL `+0x874` forces state 6
and is cleared. State 5 has a visible timing detail: its counter is tested
*before* incrementing, so the runway message appears at old value 1 and the
pending state is entered at old value 4. Moving the latter test to 3 was the
deliberate harness break; it changed the message sequence, `.data` and GLOBAL
on the first active scenario.

## Runway acquisition

State 1 calls FE `+0x54` with the aircraft position and radius 100,000, then
iterates FE `+0x20`. It accepts type-7 records and chooses the smallest squared
horizontal distance as an unsigned 48-bit value. Several record fields are
packed and unaligned:

- the latitude is a signed 24-bit value doubled into GLOBAL's scale;
- longitude is converted to metres through UTIL `+0x48`;
- each endpoint is unpacked through UTIL `+0x60`, after which LANDME replaces
  the altitude component from an overlapping signed 24-bit field;
- the runway name is copied from 31 record bytes into a 35-byte buffer.

The endpoint midpoint is calculated by add/`rcr` pairs, including the carry
above bit 47. UTIL `+0x3c` and `+0x2c` produce the runway angle and sine/cosine.
With at least five units of wind, an angle test may swap the endpoints and the
two runway designators. The selected designator becomes decimal runway digits
plus the `L`, `R`, `C` or blank suffix inserted after the last non-space in the
name. No runway leaves the 48-bit best-distance high word at `0xffff`, which
selects resource `0x214` and finishes instead of entering guidance.

LANDME calls the midpoint routine twice: once at the end of the search and
again in state 1. That is redundant when the endpoints are not swapped, but is
observable through its UTIL calls and is retained.

## Guidance scripts and phases

The main script is forty unaligned 0x24-byte boxes at `.data+0x0d7`, followed
by a type-0 terminator. Runway kinds 1 and 2 use short scripts containing only
that terminator. Script types 1 and 2 test altitude, cross-runway position and
forward position relative to opposite runway ends. Type 3 uses the runway's
half-length as a symmetric forward bound. The aircraft heading selects one of
eight phase bytes in a matching box; zero means continue searching.

The phase table at `+0x683` has twelve 0x10-byte entries: setup function,
boundary function, signed threshold and message resource id. Phases publish a
requested position at GLOBAL `+0x85a`..`+0x86f`, heading at `+0x870`, speed at
`+0x878`/`+0x87a`/`+0x87c`, and set `+0x854`. Phase 5 calls MAIN `+0x70`;
phase 11 retains the horizontal target and changes only heading and altitude.
Phase 1 posts host command `0x100d7` when GLOBAL `+0x116e` and `+0x28c` allow
it.

The phase boundary helpers are intentionally instruction-shaped. They call
UTIL sine/cosine, perform signed 16-by-16 multiplication, multiply that result
by `+6` or `-6`, take the value above bit 15, add the phase's signed threshold
with 32-bit wrap, then make the phase-specific signed comparison.

## Differential harness

`tools/difftest/landmediff.c` loads four modules for the comparison:

- retail and OpenFS98 LANDME;
- two separately named copies of retail FE, one per side;
- retail UTIL, shared because the used services are stateless.

Separate FE copies matter: FE's search cursor, open files and heap are mutable,
so sharing one instance would make the second LANDME run inherit the first
one's state. Both FEs use the proven `fediff` host model and the same installed
scenery tree. MAIN `+0x70`, WINMSG `+0x18`, CHAIN and host `+0x03c` are logging
stubs.

Each scenario restores both module images, initializes FE, varies the aircraft,
wind, speed and cancellation gates, starts LANDME and runs 24 frames. Once
LANDME publishes a target, the harness steers the aircraft onto and around that
target; this is what reaches every phase rather than hoping random positions
cross narrow guidance boundaries. It compares:

- every imported call and message;
- GLOBAL and the deterministic host arena;
- FE's whole `.data` image;
- LANDME's whole `.data` image, normalizing module-local pointers and ignoring
  only the relocated code-pointer fields.

The full baseline is:

```
states: 79536 18461 146516 139166 2999 92305 1017
phases: 25941 37643 19912 47100 39809 80 82441 18486 6914 8338 25492
runways found 18881, missing 1119, messages 182185, commands 3061
20000 scenarios, 0 differ
IDENTICAL
```

## Portability

The controller itself is now ordinary C; none of its arithmetic needs x87 or
instruction-specific assembly. The remaining portability boundary is the same
compatibility ABI used by the surrounding modules: 32-bit pointers embedded in
the retail-shaped image, unaligned fields and Win32 stdcall imports. A native
port can lift the controller state and phase tables out of that façade without
having to replace an assembly algorithm.
