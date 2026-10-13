# UTIL.DLL — module id 20

The utility layer. 11,062 bytes of `.text` and **47 services**, in slots from
`Linkage+0x18` to `+0xc4`, every one of them a distinct function. Imported by
almost everything and importing almost nothing itself: the host, GLOBAL and
NUMERIC. Its only Win32/CRT imports are `DisableThreadLibraryCalls`, `modf`
and `_ftol`.

## Why this module cannot be swapped in piecemeal

SLEW was a whole module in one go — 1,270 bytes, one service. UTIL is not:
the host binds all 47 slots at load, so a part-finished UTIL cannot be dropped
into `Modules\` and tested by running the sim. Progress is therefore measured
a service at a time against retail, with `make utildiff-run`, which loads the
retail DLL and calls its slots directly.

When enough of it exists to be worth installing, the remaining slots can be
forwarded to the retail module rather than waiting for all 47 — the same
incremental trick one level down.

## Service map

47 slots, 47 distinct functions. Sizes are the gap to the next function in
address order, so they include any alignment padding.

| slot | address | bytes | what |
| --- | --- | --- | --- |
| `+0x04` | `0x1020` | 45 | **init** — seeds both generators from the host's clock |
| `+0x08` | `0x104d` | 1 | **deinit** — a bare `ret` |
| `+0x24` | `0x104e` | 98 | **mul48** — 48-bit value times a 16-bit multiplier, doubled |
| `+0x28` | `0x10b0` | 113 | **hypot** — length of a 2D vector, 16-bit components |
| `+0x2c` | `0x1121` | 152 | **sincos** — sine and cosine of a 16-bit angle |
| `+0x30` | `0x11b9` | 93 | **div48** — 48-bit value by an unsigned 16-bit divisor |
| `+0x18` | `0x1216` | 70 | **sqrt64** — root of a 64-bit value at a pointer |
| `+0x1c` | `0x125c` | 45 | **sqrt32** — root of an unsigned 32-bit value |
| `+0x20` | `0x1289` | 39 | **sqrt16** — root of a 16-bit value, clamped to a byte |
| `+0x34` | `0x12b0` | 89 | **rotate2d** — Q15 rotation of a 2D vector |
| `+0x38` | `0x1309` | 287 | **angle from sine** — piecewise arcsine, quadrant resolved |
| `+0x3c` | `0x1428` | 124 | **atan2** — angle of a 2D vector, via a table and a quadrant fixup |
| `+0x44` | `0x14a4` | 198 | **metres to longitude** — latitude table, 64-bit divide |
| `+0x40` | `0x156a` | 48 | **metres32 to longitude** — sign-extends to 48 bits, calls `+0x44` |
| `+0x48` | `0x159a` | 271 | |
| `+0x4c` | `0x16a9` | 279 | |
| `+0x50` | `0x17c0` | 19 | **stub** — ignores the first argument, returns the second |
| `+0x54` | `0x17d3` | 19 | **stub** — identical to `+0x50` |
| `+0x58` | `0x17e6` | 19 | **stub** — identical to `+0x50` |
| `+0x5c` | `0x17f9` | 7 | **stub** — saves three registers, restores them, returns |
| `+0x60` | `0x1800` | 60 | **unpack position** — 8-byte record to the 24-byte vector |
| `+0x64` | `0x183c` | 392 | |
| `+0x68` | `0x19c4` | 204 | |
| `+0x6c` | `0x1a90` | 39 | **rand** — Lehmer generator, seed in `.data+0x0f0` |
| `+0x70` | `0x1ab7` | 24 | **rand32** — two draws, sixteen bits of each |
| `+0x74` | `0x1acf` | 85 | **v48 add** — three-component 48-bit vector |
| `+0x78` | `0x1b24` | 85 | **v48 sub** |
| `+0x7c` | `0x1b79` | 26 | **copy24** — `rep movsd`, count six |
| `+0x80` | `0x1b93` | 93 | **v48 equal** — 1 if every component matches |
| `+0x84` | `0x1bf0` | 47 | **shr48** — arithmetic right shift of one 48-bit value |
| `+0xb0` | `0x1c1f` | 25 | **double to int** — `fld`/`fistp`, so it rounds |
| `+0xb4` | `0x1c38` | 40 | **double to int64 low** — stores a quadword, returns its low half |
| `+0x88` | `0x1c60` | 406 | |
| `+0x8c` | `0x1df6` | 1614 | largest in the module |
| `+0x90` | `0x2444` | 1804 | |
| `+0x94`…`+0xc4` | `0x2b50`… | 64–1312 | |

Reproduce with `make graph` plus the slot dump in `tools/dumplinkage.py`.

## The trigonometry table

A quarter-wave cosine at `.data+0xf6`: 129 entries spanning 0 to `0x4000`,
four-byte stride, unsigned Q16. Entry 0 is `0xffff`, entry 64 is `0xb504`
(0.7071) and entry 128 is 0 — it matches `cos` to within a unit across the
quarter. `tools/gen_util_tables.py` extracts it into `src/util/cos_table.inc`.

The lookup interpolates linearly: the index is bits 7..14 of the angle and the
fraction bits 0..6. The table is read with a *sign-extending* load even though
the values are unsigned, so entry 0 arrives as -1 and only becomes `0x7fff`
after the `and 0xffff` and the halving at the end. That sequence is reproduced
rather than simplified, because the masking is what makes it come out right.

Angles are 16-bit: `0x4000` is ninety degrees, `0x10000` a full turn. The
service at `+0x2c` folds any angle into the quarter wave and fixes the signs,
recursing once for negative inputs (and clamping `-32768`, which has no
positive counterpart, to `-32767`). Either output pointer may be null. **The
second argument receives the sine and the third the cosine.**

## The latitude table, and slot 0x44

A second table lives at `.data+0x5b4`: 78 int32 entries starting at **40075000**
— the Earth's circumference at the equator, in metres — falling away to nothing
at the pole. It is the circumference of the latitude circle.

Slot `0x44` turns a metric step into a longitude step by dividing by it, which
is why a metre of easting buys more longitude the further from the equator you
go. The index is the top bits of the latitude and the leftover bits
interpolate between neighbouring entries; the quotient comes from a two-step
64/32 division that yields 32 fractional bits, and the result is stored as the
usual 48-bit value.

Three things about it are worth knowing before calling it:

- **It faults on a large step.** The first `div` takes a 64-bit numerator and a
  divisor of about 2^25, and raises #DE if the quotient will not fit in 32
  bits. Retail crashes outright rather than saturating.
- **It walks off the table** past roughly ±0x4e of latitude index, into
  whatever sits next in `.data`. `tools/gen_util_tables.py` therefore emits the
  whole section, so the same bytes are read rather than guessed at.
- **Two of its steps are wrong**, and both are reproduced:

  `add eax,eax / adc edx,0` doubles only the *low* half of the interpolation
  product and folds its carry upward; doubling a 64-bit value wants
  `adc edx,edx`. The divisor therefore comes out slightly different from the
  interpolation the code reads as though it were doing. This is not academic —
  it moves the answer by a unit or two in the low word, and it was the last
  mismatch to fall out of the differential test.

  On the negative branch the 64-bit negate is written
  `not ecx / neg eax / sbb edx,-1`: the borrow correction lands on `EDX`, the
  leftover remainder, instead of on `ECX` where the high half of the quotient
  lives. The high half comes back as `~q` and never gets its `+1`, which is
  right only when the low half is non-zero.

## Rounding

Everything that leaves the integer domain goes through the x87 in its default
mode — nearest, ties to even. The three square roots use `fild`/`fsqrt`/
`fistp`, and the double-to-int at `+0xb0` is a bare `fld`/`fistp`, so it rounds
rather than truncating the way a C cast would. The reimplementations use the
same instructions rather than the C library, which would otherwise disagree on
every halfway case — and with a million comparisons, halfway cases turn up.

`sqrt16` has a tail worth noting: `r - (r >> 8)`. It looks redundant until the
top of the range, where sqrt(65535) rounds up to 256 and the subtraction pulls
it back to 255 so the answer still fits in a byte.

## The 48-bit vector

Four services share a 24-byte type: three 48-bit values at a stride of eight,
each laid out the usual way with a 16-bit low half at +2 and a 32-bit high half
at +4. `+0x74` adds, `+0x78` subtracts, `+0x80` compares, and `+0x7c` is
exactly the 24-byte copy for it. Three world axes in the same fixed-point
format the position fields use, almost certainly.

`+0x84` shifts a single 48-bit value right, arithmetically. Its count reaches
`shrd` and `sar` in CL, and x86 masks shift counts to five bits — so asking for
32 shifts by nothing rather than by a whole word. Reproduced masked, instead of
quietly doing the arithmetically sensible thing.

`+0x80` sets its zero result and then runs `cmp eax,1`, whose flags nothing
reads. Dead, and not reproduced: it has no effect a caller can observe.

## Four unfinished services

`+0x50`, `+0x54` and `+0x58` are byte-for-byte identical to each other:

```asm
mov eax, [ebp+0x8]      ; load the first argument
mov eax, [ebp+0xc]      ; ...throw it away and load the second
ret 8
```

which is what a compiler emits for a body of `a; return b;`. `+0x5c` saves
EBX, ESI and EDI, restores them and returns, leaving EAX at whatever the caller
had in it. No names, no callers inside UTIL, no behaviour beyond the above.
They are reimplemented as what they are rather than guessed at, and `+0x5c`
returns void because its result is never a value retail defines.

## The other two tables

`.data` holds three lookup tables, and they tile the section exactly, which is
how their sizes confirm themselves:

| offset | size | what |
| --- | --- | --- |
| `+0x0f6` | 129 words, stride 4 | quarter-wave cosine, `+0x2c` |
| `+0x2f8` | 60 words | arcsine, first range, `+0x38` |
| `+0x370` | 12 words | arcsine, second range, `+0x38` |
| `+0x388` | 16 words | arcsine, third range, `+0x38` |
| `+0x3ac` | 257 words | arctangent over an eighth-turn, `+0x3c` |
| `+0x5b0` | 4 bytes | quadrant adjustments, `+0x3c` |
| `+0x5b4` | 78 dwords | latitude circle circumference, `+0x44` |

Every one abuts the next, which is what makes the sizes self-confirming. It
also corrects an earlier reading: the 130th cosine entry is not a guard the
table carries, it is the first word of the arcsine table. Retail reads it too,
at index 128, where the interpolation weight is zero -- so it never affects an
answer, but the explanation was wrong.

The arctangent table runs from `+0x3ac` to `+0x5ae`, finishing immediately
before the quadrant bytes. Its 257th entry exists because the interpolation
reads index+1, the same reason the cosine table carries a 130th.

`+0x3c` takes magnitudes, divides the smaller by the larger to land inside the
first eighth-turn, looks the ratio up, then puts the answer back in the right
octant and quadrant. Equal magnitudes short-circuit to `0x2000`, which is also
what keeps a zero vector away from the divide.

Its interpolation needs transliterating rather than rewriting: the `sub ax` and
`add ax` are 16-bit and leave the top half of the register alone, while the
multiply between them is unsigned and reads the whole 32 bits. Expressing it as
arithmetic on a signed difference gives different answers.

## The 48-bit type is 32.16 fixed point

Slots `+0xbc` and `+0xc0` convert between the 48-bit form and a double, and
their two constants say what the format is: `65536.0` one way and `-1/65536`
the other. The 32-bit high half is an integer and the 16-bit low half is a
fraction in 1/65536ths. Every position field in the sim is 32.16 fixed point.

Both go through MSVCRT's `_ftol`, which truncates towards zero, and both
convert the integer half back to a double as **unsigned** -- retail stores it
as the low dword of a quadword whose high dword it has just zeroed.

In `+0xbc` that is harmless, and interestingly so. The error is exactly 2^32,
which after multiplying by 65536 is 2^48, and 2^48 cannot reach the sixteen
bits that actually get stored. Model `_ftol` as returning 32 bits rather than
64 and it saturates to the indefinite value instead, and the fraction comes out
zero -- which is the first way this was got wrong here.

In `+0xc0` it is not harmless: a negative position converts to a value near
2^32 rather than to itself, so the pair does not round-trip across zero.
`-1.5` becomes `4294967295.5`. Reproduced, not corrected.

### These two need 80-bit arithmetic

`+0xbc` is the one service so far that **cannot be made bit-identical on ARM**
without emulation.

Retail keeps the whole chain on the x87 at 80-bit extended precision, and with
that 2^32 offset in play the precision is load-bearing: the same arithmetic in
64-bit double gives a fraction one or two units different. `long double` is
80-bit on x86, so the reimplementation matches there -- but on AArch64 `long
double` is 128-bit, which computes the *exact* answer, and the exact answer is
not what retail produces.

**Decided: accept the difference.** A native ARM build will compute the exact
answer where x86 reproduces retail's, and the two will disagree by one or two
units in the 1/65536ths of a position -- about a hundredth of a millimetre of
latitude. Emulating 80-bit arithmetic to preserve a rounding artefact is not
worth the cost. Recorded here so it is a decision rather than a surprise.

`+0xc4` falls under the same decision, for a reason that was not obvious: it
wraps a negative into the positive range before converting, so its intermediate
sits just below 2^32 and only about twenty bits of fraction survive in a 64-bit
double. It was written here as portable first, and the harness caught it.

Note that `make utildiff-portable` does **not** catch this: it compiles out the
x87 assembly but still runs on x86, where `long double` is still 80-bit.

## Reaching outside the module

Two services call the host, and both are now done -- see
[host.md](host.md). The host's service table was found at `0x00417ba0`:
194 slots, of which these two matter here.

`+0x9c` measures two strings through the host's `strlen` at `+0x2b8` and
returns the longer, ties going to the second argument.

`+0x04` (init) seeds both generators from the host's `timeGetTime` at `+0x38`:

```asm
mov  eax, ds:<ImportTable slot for module 1>
call DWORD PTR [eax+0x38]       ; the tick, straight into the integer seed
mov  ds:<seed>, eax
call <internal routine at .text+0x2ab0>   ; 1.0 / tick into the float seed,
                                          ; retrying while the clock reads zero
```

Init was written off here as untestable, because its result is not a function
of its arguments. That was too pessimistic: the state it leaves behind is
observable on both sides, so a host stub returning a controlled tick sequence
-- zeros included -- tests it properly. A version with the retry removed is
caught, which is the check that makes the passing result mean something.

## Status

**34 of 47 services** reimplemented and verified against retail: `0x04`, `0x08`,
`0x18`, `0x1c`, `0x20`, `0x24`, `0x28`, `0x2c`, `0x30`, `0x34`, `0x38`, `0x3c`,
`0x40`, `0x44`, `0x50`, `0x54`, `0x58`, `0x5c`, `0x60`, `0x6c`, `0x70`, `0x74`,
`0x78`, `0x7c`, `0x80`, `0x84`, `0x9c`, `0xb0`, `0xb4`, `0xb8`, `0xbc`, `0xc0`,
`0xc4`, `0xc8`. **4,822,345 comparisons, zero mismatches** — including all 65,536 sincos angles
and all 65,536 sqrt16 inputs exhaustively, 300,000 draws from the generator
with both seeds synchronised, shift counts past 31 where x86's masking bites,
every x87 tie case and both sides of the 32-bit boundary, both output pointers
null, and the zero-input short-circuit in `hypot`.

None of the five touches GLOBAL or NUMERIC, which is why they can be checked
without binding retail's imports. A service that reaches for an import will
need the binding `slewdiff.c` does.

They are also exactly the set SLEW depends on, so `slewdiff` now runs with **no
stubs at all** — SLEW is driven over the same arithmetic FS98 drives it over,
and stays byte-identical.

13 services remain, and they fall into two groups.

**Five are self-contained** and testable with the external harness today:
`+0x48` (271 bytes), `+0x4c` (279), `+0x64` (392), `+0x68` (204) and `+0x88`
(406, which touches GLOBAL but no host service).

**Eight form a dynamic data structure subsystem** built on the host's
allocator: `+0x8c`, `+0x90`, `+0x94`, `+0x98`, `+0xa0`, `+0xa4`, `+0xa8` and
`+0xac`. They allocate through host `+0x20c` (`malloc`), release through
`+0x214` (`free`), and share a cluster of host services at `+0x15c`, `+0x160`,
`+0x164` and `+0x184`. `+0xa8` is the clearest entry point: it asks for a
0x28-byte block and initialises its fields, so it is a constructor for whatever
object the rest manipulate.

These eight are worth reversing as a group rather than one at a time. The
object layout only becomes clear from seeing what all of them do with it, and
they are now reachable through the in-process harness.

A note on the scan that produced this split: service sizes are the gap to the
next table-referenced address, so an internal helper sitting inside that gap
gets attributed to the service before it. That is what made `+0xc8` look as
though it called the host -- it does not; the init helper next door does.
