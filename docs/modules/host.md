# FLTSIM98.EXE — the host, module id 1

82,016 bytes of `.text`. Every module imports it, and until the service table
was found it was the one thing in the dependency graph nothing could see into.

Regenerate everything below with `make hostmap`.

## It publishes a service table like any other module

The host has no `Linkage` export -- it is an executable -- but it hands modules
a pointer to a structure of the same shape, and the loader writes that pointer
into each module's `ImportTable` entry for id 1. It is found the way the module
tables were: scan `.data` for a dword 1 followed by a long run of pointers into
`.text`.

```
0x00417ba0   id 1, then 194 service slots to +0x31c, 191 distinct functions
```

0x320 bytes, more than twice UTIL's table, which fits a module everything else
leans on. Three slots alias: `+0x110` and `+0x118` share a function, and
`+0x108`, `+0x220` and `+0x224` share another.

## What the services are, by the libraries they call

This is the useful cut, because it is also the specification a native host has
to satisfy:

| calls | services | meaning |
| --- | --- | --- |
| nothing | 84 | pure logic and accessors onto host globals |
| `MSVCRT` only | 11 | C runtime only |
| `KERNEL32` | 40 | files, memory, processes |
| `KERNEL32` + `MSVCRT` | 14 | |
| `USER32` | 27 | windows, messages, input |
| `GDI32` (alone or with others) | 8 | drawing |
| `WINMM` | 4 | timing and sound |
| `DDRAW`, `MFC42`, `COMCTL32`, `SHELL32`, `ADVAPI32`, … | 6 | the big ones |

**95 of 194 services -- very nearly half -- call no platform API at all**, being
either pure logic or C runtime. The platform dependency is concentrated in the
27 windowing services, 8 drawing ones, and a handful of large services that
touch everything: `+0x030` is 27 KB and reaches for seven libraries, and
`+0x31c` is the one that touches `DDRAW`.

## The shape of the simple ones

Fifty services are 48 bytes or less, and the mapper names thirteen outright
from their code shape alone:

| kind | example |
| --- | --- |
| returns a constant | `+0x01c` returns 1 |
| reads a global | `+0x020` returns `*0x0041719c` |
| writes a global | `+0x02c` sets `*0x004187ec` to 1 |
| returns a global's address | `+0x058` returns `&0x00417ec0` |
| stub returning zero | `+0x108`, `+0x12c`, `+0x154`, `+0x220`, `+0x224` |
| import thunk | `+0x038` is `timeGetTime` |

`+0x058` is worth a second look: `0x00417ec0` is the address immediately after
the service table ends, so the host is handing modules a pointer to whatever
structure it keeps next door.

That the table is mostly accessors is itself the finding. The host's job, from
a module's point of view, is to expose state and a few primitives.

## The two services that mattered first

| slot | address | what |
| --- | --- | --- |
| `+0x38` | `0x4085c0` | `timeGetTime`, a jump straight through the IAT to WINMM |
| `+0x2b8` | `0x40ef40` | a null-safe `strlen`, `repnz scasb`, 0 for a null pointer |

Four more are identified but not yet implemented, because they are what UTIL's
remaining services are built on:

| slot | address | what |
| --- | --- | --- |
| `+0x20c` | `0x40cdc0` | `malloc`, wrapped: stores the result through a pointer, and answers -1 for a failed four-byte request |
| `+0x214` | `0x40cef0` | `free`, wrapped: null-safe, and clears the pointer it frees |
| `+0x110` | `0x409bd0` | a two-argument tail call into `+0x10c` |
| `+0x10c` | `0x409720` | 1200 bytes, not yet read |

So the host provides modules with memory management as a service rather than
letting them call the C runtime directly -- which is consistent with everything
else about this design, and is another thing a native host has to supply.

Both were reached for by UTIL, and naming them unblocked two services that had
been parked:

- UTIL `+0x9c` measures two strings through `+0x2b8` and returns the longer,
  ties going to the second argument.
- UTIL `+0x04` (init) seeds both of UTIL's generators from `+0x38`: the integer
  one takes the tick directly, the floating-point one takes `1.0 / tick` and
  retries while the clock reads zero.

Init had been written off as untestable, on the grounds that its result is not
a function of its arguments. That was too pessimistic. The *state it leaves
behind* is observable on both sides, so a host stub returning a controlled tick
sequence makes it testable -- and a deliberately broken version, with the retry
removed, is caught.

## Why this module matters more than its position suggests

1. **It gates every other module's unfinished services.** Anything that reaches
   outside itself reaches here first.
2. **A native build replaces it.** The host owns the window, the message loop,
   DirectDraw and the MFC user interface -- all the platform dependency the
   README's portability survey puts outside the 16 portable modules. Whatever
   drives a native OpenFS98 is a reimplementation of this service table on SDL,
   and the table above says how much of it is actually platform work: about
   half.

## Its services cannot be tested from outside the process

Worth recording before anyone tries again. The host has no `.reloc` section, so
every address inside it is absolute and correct only at `0x400000`: the service
pointers, the globals its accessors read, the calls between its functions.
`LoadLibraryEx` maps it wherever it likes -- 0x00a90000 here -- at which point
the service pointers are wrong and, worse, the code reads globals from whatever
occupies the old addresses.

Mapping it by hand at `0x400000` does not work either: that address is already
committed in any wine process, inside a private region the loader creates at
`0x330000`, so `VirtualAlloc` refuses it.

So the differential approach that works for UTIL -- load the retail binary
beside the reimplementation and call both -- cannot work here.

**The way to do it is to run the test inside the host rather than outside.**
The sim already loads every DLL in `Modules\` and hands each one the host's
linkage pointer, which is precisely the access a test needs. A test module with
its own module id, dropped in beside the others, would be handed a real host
pointer by the real loader, could call services for real, and could write its
results to a file. That is the same drop-in mechanism the project relies on
everywhere else, pointed at the host.

**That harness now exists**: `src/test/ofstest.c`, built and run by
`make ofstest-run`. It takes module id 5, which the retail set leaves unused,
imports the host, GLOBAL and UTIL, and writes its results to
`C:\openfs98-test.log` because a module has nowhere else to put output.

Its first run confirmed the static analysis from the outside in:

```
host pointer   0x00417ba0      <- the address derived by scanning .data
GLOBAL pointer 0x20722000      <- the OpenFS98 build, not retail's 0x20723000
UTIL pointer   0x208e5020      <- retail's Linkage RVA exactly
```

The host table address had been worked out by pattern-matching `.data`; being
handed the same pointer by the real loader settles it. The GLOBAL pointer is an
incidental bonus: `.data` sits at a different RVA in the reimplementation than
in retail, so the address alone proves the swapped-in module is the one running.

It also verified the two implemented host services against the real article --
`strlen` of an empty string, of "abcd", and of a null pointer -- and checked
UTIL `+0x9c` against retail across 25 string pairs while both measured through
the genuine host service rather than a stub.

Everything written here runs during the sim's startup, so a fault in it stops
the sim from starting. The module is defensive about null pointers for that
reason, and `make ofstest-remove` takes it back out when not in use.

## Status

2 of 194 services implemented and now verified directly, in-process; 13 more
named from their shape; all 194 classified by subsystem. The remaining 192 are
unexplored but no longer unreachable. Sizes reported by the mapper are upper bounds --
they are the gap to the next table-referenced address, and internal helpers
that no slot points at fall inside those gaps.
