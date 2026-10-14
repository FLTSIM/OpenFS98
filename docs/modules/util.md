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
| `+0x48` | `0x159a` | 271 | **longitude to metres** — the inverse of `+0x44` |
| `+0x4c` | `0x16a9` | 279 | **rotate3d** — vector through three Euler angles |
| `+0x50` | `0x17c0` | 19 | **stub** — ignores the first argument, returns the second |
| `+0x54` | `0x17d3` | 19 | **stub** — identical to `+0x50` |
| `+0x58` | `0x17e6` | 19 | **stub** — identical to `+0x50` |
| `+0x5c` | `0x17f9` | 7 | **stub** — saves three registers, restores them, returns |
| `+0x60` | `0x1800` | 60 | **unpack position** — 8-byte record to the 24-byte vector |
| `+0x64` | `0x183c` | 392 | **range and bearing, 48-bit** — with a shared scale |
| `+0x68` | `0x19c4` | 204 | **range and bearing** — distance and heading between two points |
| `+0x6c` | `0x1a90` | 39 | **rand** — Lehmer generator, seed in `.data+0x0f0` |
| `+0x70` | `0x1ab7` | 24 | **rand32** — two draws, sixteen bits of each |
| `+0x74` | `0x1acf` | 85 | **v48 add** — three-component 48-bit vector |
| `+0x78` | `0x1b24` | 85 | **v48 sub** |
| `+0x7c` | `0x1b79` | 26 | **copy24** — `rep movsd`, count six |
| `+0x80` | `0x1b93` | 93 | **v48 equal** — 1 if every component matches |
| `+0x84` | `0x1bf0` | 47 | **shr48** — arithmetic right shift of one 48-bit value |
| `+0xb0` | `0x1c1f` | 25 | **double to int** — `fld`/`fistp`, so it rounds |
| `+0xb4` | `0x1c38` | 40 | **double to int64 low** — stores a quadword, returns its low half |
| `+0x88` | `0x1c60` | 406 | **marshalling engine** — descriptor-driven type conversion |
| `+0x8c` | `0x1df6` | 1614 | **exchange, write** -- control records into GLOBAL variables |
| `+0x90` | `0x2444` | 1804 | **exchange, read** -- GLOBAL variables into control records |
| `+0x94` | `0x2b50` | 496 | **list a tree** -- every match under a directory, recursively |
| `+0xa0` | `0x2d40` | 400 | **is a directory** -- by listing its parent |
| `+0x98` | `0x2ed0` | 928 | **catalogue** -- the flight, lesson and video pickers' list |
| `+0x9c` | `0x3270` | 144 | **longer string** -- plus `+0x98`'s two comparators |
| `+0xa4` | `0x3300` | 144 | **modal alert** -- returns the answer |
| `+0xa8` | `0x3390` | 160 | **open a modeless alert** |
| `+0xac` | `0x3430` | 1312 | **close a modeless alert** -- 64 bytes, then the text builder |

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

The first two services found calling the host were these, and both are done --
see [host.md](host.md). The host's service table was found at `0x00417ba0`:
194 slots. The three file listings that call it far more heavily are under
*Status* below.

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

**All 47 services** reimplemented and verified against retail.

`make utildiff-run` covers 44 of them: **6,222,442 comparisons, zero
mismatches**, including 400,000 random descriptor tables through the exchange
engine, 100,000 alert sequences, and all 65,536 sincos angles and all 65,536 sqrt16
inputs exhaustively, 300,000 draws from the generator with both seeds
synchronised, shift counts past 31 where x86's masking bites, every x87 tie
case and both sides of the 32-bit boundary, both output pointers null, and the
zero-input short-circuit in `hypot`. They are also exactly the set SLEW
depends on, so `slewdiff` runs with **no stubs at all**.

`0x04`, `0x9c` and the three file listings call the host, so they run in-process
under `make ofstest-run`, against retail's UTIL and through the real host.

### Installed

UTIL is built as a module (`src/util/util.c`, `make`, `make install`) and runs
in the sim in place of retail's. `make smoke` passes with it -- 27/27 modules,
95 gauges, startup all the way to the panel.

The module is a shell around the four service files: an ImportTable in
retail's order (host, GLOBAL, NUMERIC) and a Linkage table with every service
at retail's offset and `+0x0c`, `+0x10` and `+0x14` null, `+0x14` being where
the host would look for a dialog table. Built with `FS98_UTIL_MODULE`, the
services read their imports straight out of ImportTable on every call, as
retail's `mov eax, ds:<slot>` does, rather than from a pointer an init would
have to set -- so they work for any module that calls them before UTIL's own
init has run. The harnesses build the same files without the flag and set the
pointers themselves (`util_imports.h`).

Two things were checked before installing. The image ends below WEATHERM's
base at `0x208f0000`, so it loads where retail's did. And no module reaches
into UTIL other than through the slot table: a scan of every access through
UTIL's link in all 27 modules found only slot calls (two apparent exceptions
in LANDME are a register reloaded by a `pop`).

**What the game actually calls.** `make util-trace` builds the module with
each slot replaced by `incl count[i] / jmp *target[i]` -- transparent to any
calling convention -- and boots the sim. Startup plus 90 seconds of an idle
flight call 15 of the 47:

| slot | calls | | slot | calls |
| --- | --- | --- | --- | --- |
| `+0x1c` sqrt32 | 2,204 | | `+0x44` metres to longitude | 9 |
| `+0x48` longitude to metres | 807 | | `+0xb4`, `+0xb8` | 5 each |
| `+0x18` sqrt64 | 394 | | `+0x24`, `+0x38` | 3 each |
| `+0x2c` sincos | 61 | | `+0x04` init, `+0x40`, `+0x4c`, `+0x6c`, `+0x70` | 1 each |
| `+0x3c` atan2 | 58 | | | |

A hands-on session -- a person flying, then quitting the sim normally --
raised that to **19 of 47**, with nothing going wrong: sincos 847,117 calls,
longitude to metres 461,867, atan2 8,730, sqrt32 2,693, frand 2,611, angle
from sine 873, and newly hypot, unpack position and deinit. **`+0xa4`, the modal
alert, ran once** -- with a clean deinit after it, consistent with the exit
confirmation going through our alert code into retail's `Alerts.DLL` and back.

Still unexercised in-game: the file listings (`+0x94`, `+0x98`, `+0xa0`), the
exchange engine (`+0x8c`, `+0x90`), the modeless alerts, `marshal`, the 48-bit
vector helpers and a few conversions -- 28 in all. Nothing built a flight list,
so the menus that use them were not reached under wine; they stand on the
differential and in-process tests until they can be.

### The file listings: `+0x94`, `+0x98`, `+0xa0`

All three are thin layers over the host's enumeration cluster -- find-first,
-next and -close, path join and split, a growable array, `qsort` and
`_strcmpi`, documented in [host.md](host.md). They live in
`src/util/util_files.c`.

**`+0x94`** `(void **data, int *count, dir, pattern, ext_id)` collects
`<dir>\<name>` for every match, then recurses into each subdirectory not
starting with a dot. Each 0x108-byte record is a path and **the file's size**
-- not its type. That was read wrong at first, as the extension id, because
the size is loaded *before* an interleaved `push` and so sits four bytes
further from the entry than it looks; the harness caught it on the first run.
The paths have no extension and nothing records what it was, so with ext `-1`
`a.BGL` and `a.TXT` come back as the same path.

**`+0xa0`** `(path)` splits the path, lists the parent, and answers 1 if some
directory there matches the last component case-insensitively. Two
consequences, both reproduced because the same host does the work:

- **A directory with a dot in its name is never found.** The splitter peels
  `.BGL` off `dir.BGL` as an extension, and the comparison is against `dir`.
- **A bare name is checked against the drive root**, not the current
  directory: `"Modules"` rejoins to an empty parent, and an empty directory
  string is the root (see host.md). `"Modules"` answers 0 from the game
  directory and `".\Modules"` answers 1; `"openfs98-enum"` answers 1 when
  `C:\openfs98-enum` exists.

**`+0x98`** is nine arguments and the most interesting of the three:

```
int catalogue(stock_id, user_id, ext_id, int *count, void **data,
              name_cb, cb100, cb500, sort)
```

MAIN calls it as `(0x9004, 0x9002, 2, ...)`. The ids are string resources --
not in the EXE but in `ENGLISH.FLL`, which the host's string loader tries
first -- and they resolve to **relative** directories: `0x9004` is `SITUATIO`,
`0x9002` `PILOTS`, `0x9007` `VIDEOS`, `0x900a` `CHALLENG`. So the stock flights
come first and the user's own second; DEMO lists `VIDEOS` and ADVDRV the
lessons the same way.

Each 0xa08-byte record holds a display name, two 0x400-byte fields the second
and third callbacks fill, the path, and a flag that is 0 for the first
directory and **the file's writable bit** for the second -- so a user's saved
flight can be deleted and a stock one cannot.

The two directories are not treated alike. In the first, any callback returning
0 drops the record. In the second, return values are ignored entirely, and a
record is dropped only if the name callback leaves the title empty. The test
tree has a file whose callback refuses but still writes a title: it is dropped
from `SITUATIO` and kept from `PILOTS`, on both sides.

The display name is the longer of the file name and the callback's title --
through `+0x9c`, so ties go to the title. Sort 1 orders by display name, 2 by
path, anything else not at all. When nothing survives it returns 0 **without
resetting its static list**, whose buffer then carries into the next call;
reproduced, though no output shows it.

`ofstest` checks all three against retail on a scratch tree built for the
purpose -- every callback combination, all three sort orders, read-only files,
dot-directories, directories named like files, a missing directory -- and runs
`+0x98` over the game's real `SITUATIO` and `PILOTS` (75 flights) and `VIDEOS`
as MAIN and DEMO call it. Each of the three, deliberately broken, is caught.

One thing cannot be matched: both loops of `+0x94` share one uninitialised
entry, and a file with an unregistered extension inherits the previous
entry's type -- in retail's first iteration, stack garbage. The test tree keeps
a `.BGL` in every directory so the first loop always sets it; the
reimplementation starts it at 0.

### The exchange engine: `+0x8c` and `+0x90`

Filed at first with the file browser because their slots are adjacent; they
are nothing of the kind. They move values between GLOBAL variables and an
array of per-control records, driven by a table of 20-byte descriptors. MAIN
calls them 85 times, FE, CONTROLS, ANALYSIS and REALITY a few more. Read as a
dialog's data exchange -- the reading here, from the shape: `+0x90` fills the
controls from the sim, `+0x8c` writes the edits back. The
same table drives both. They live in `src/util/util_exchange.c`, whose header
comment has the full descriptor and subtype layout.

```
+0x8c  int  put(desc *table, uint32 n, void *records, int mode)  ret 0x10
+0x90  void get(desc *table, uint32 n, void *records)            ret 0xc
```

A descriptor names a kind (0..8), a record offset, a subtype, a variable, and
a relocation flag. **While that flag is exactly 1 the variable is an offset
into GLOBAL**, and the first call through the table adds GLOBAL's address and
clears the flag -- editing the caller's table, once. A record is a status word
and a value; for text controls the value is a `char *`.

Subtypes say what the variable is: a dword, word or byte (`0x2002`..`0x200d`,
cycling); a single bit in a byte, word or dword (`0x41100000`, `0x42200000`,
`0x44400000` plus a one-bit mask); a string (`0x200e`); a callback (`0x2001`);
or, for the text kinds 4 and 8, a NUMERIC format id plus `0x1000`, formatted
by NUMERIC `+0x18` on the way out and parsed by `+0x1c` on the way in. Kinds
5..8 are output-only -- enable state and display text, written into the
*status* word -- and `+0x8c`'s handlers for them do nothing.

Things worth knowing:

- **It is hand-written assembly**, and its dispatch is one flat table in
  `.text` (`0x1d70` and `0x23f0`): nine kind handlers then twelve width
  handlers, indexed by kind and then by `subtype - 0x1ff9` -- the same formula
  for every kind. So kinds 9..20 work, as width handlers called directly; a
  subtype of `0x1ffd`..`0x2000` lands on a kind handler; one of
  `0x1ff9`..`0x1ffc` re-enters its own kind handler until the stack runs out;
  and anything off either end jumps into code. The first two are reproduced;
  the two that crash retail are inert.
- **`mode` means "validate" at 1, and only some handlers listen.** The width
  handlers skip their store when mode is exactly 1. The NUMERIC parse writes
  into a scratch buffer for any non-zero mode. The string and flag handlers
  always write. Callbacks are told 2 to validate and 1 to commit, the opposite
  numbering; on a read they are told 0.
- **`+0x8c` returns 1 if any handler objected** -- a parse error or a callback's
  non-zero return -- which is how a dialog refuses a bad entry.
- **`+0x90`'s return is garbage** on some paths -- a bit index, a kind number
  -- and every one of its 48 call sites in the retail modules ignores it, so the
  reimplementation returns nothing.
- Around its two NUMERIC calls and its `strcpy`, retail realigns the stack with
  a trick: it plants the bytes `07 06 05 04` below `esp`, rounds `esp` down,
  and reads back the right one as the adjustment to undo. Behaviourally just a
  call; noted because it looks alarming in a listing.

`utildiff` binds retail's three imports to stubs: the host for `strcpy`, a fake
GLOBAL block, and a NUMERIC whose `+0x18` and `+0x1c` log their arguments. Both
sides run 400,000 random tables against one arena, restored between them, and
the comparison covers the whole arena, the return value and the sequence of
callback and NUMERIC calls. Deliberately broken five ways -- the scratch-buffer
condition, the relocation test, kinds past 8, the byte-flag width limit, and
the flag handler's test against exactly 1 -- it goes red each time.

A sixth break was **not** caught, and that was correct. Letting an
out-of-range single bit fall through to the next flag width cannot change
anything: for a subtype to be a single bit above two bases at once, the
difference between those bases would have to be a difference of two powers of
two, and `0x01100000` and `0x02200000` are not. The search order is therefore
unobservable.

### The alerts: `+0xa4`, `+0xa8`, `+0xac`

First read as a general dialog framework, which is what the host service they
call looked like from here. They are narrower: the dialogs they open are ids 2
and 3 in the host's dialog registry, and both belong to `English\Alerts.DLL`.
So this is the game's message box -- see *The dialog engine* in
[host.md](host.md), which also corrects the "mode 2 / mode 3" reading.
`src/util/util_alert.c`.

```
+0xa4  int  alert(flags, tag, p1, p2, p3, p4, p5, lines)            dialog 2, modal
+0xa8  void alert_open(void **out, tag, p1, p2, p3, p4, p5, lines)  dialog 3, modeless
+0xac  void alert_close(void **pp)
```

**The text** is built by an internal function at `.text+0x2480` from up to five
pieces, each a string-resource id below 0x10000 or a `char *` at or above it,
joined by spaces -- or newlines when `lines` is set. Loaded strings pass through
a 256-byte buffer with a limit of 255, so 254 characters survive. The block is
grown as it goes, and in a specific order: the separator is appended *before*
the realloc, into the four bytes of slack the last allocation left, and the
new size is measured with it included. The first piece is mandatory: if it is
zero the block pointer is never initialised and the next append writes through
stack garbage.

**The modal descriptor** is 24 bytes on the stack -- zero, a do-nothing handler,
flags, the answer, the tag, the text. **The answer field is never initialised**:
the dialog writes it, and an alert that closes without answering returns
whatever was on the stack. With flag 8 set, an answer of exactly 8 calls host
`+0x24`, which sends the main window `WM_CLOSE`: the alert's "exit".

**The modeless object** is 0x28 bytes from the host allocator, stored through
`out` before it is filled. The layout is not the modal one: the tag moves to
`+0x08`, `+0x0c` is a live flag set to 1, and the text is at `+0x24`, with
sixteen bytes in between left as malloc returned them. Closing clears the live
flag and calls host `+0x114`, which sends every modeless dialog `WM_USER+100`
synchronously -- so the alert sees the flag down and destroys itself -- and only
then frees the text and the object.

`utildiff` checks them with a host stub that numbers heap blocks in allocation
order, so the transcript of allocations, reallocs, frees, string loads, dialog
calls and quits can be compared exactly, along with the descriptor each dialog
call received, the text, and the answer. 100,000 sequences of a modal alert
then an open/close pair, every piece shape, both separators, answers including
the quit case. Broken five ways -- the separator after the realloc, the
separators swapped, quitting without flag 8, the object opened dead, and a
256-byte load limit -- it goes red each time.

The first run did not pass, and the fault was the stub's: `malloc` reuses a
freed block's address, so looking blocks up by address found a dead one. Freed
blocks now leave the table.

