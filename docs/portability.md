# Portability and the native goal

Moved out of the README. The end state is native builds; this is how far away
that is and the decisions already made on the way.

The end state is native builds -- modern Windows, Linux, Mac, maybe Android --
at which point wine stops mattering. Running the retail binary under wine is
scaffolding for the middle of the project, not the destination. Its one real
problem, the 3D view freezing, was wine swallowing divide faults the host
repairs; `make sehfix-install` and `make vd-on` give a flyable sim, and our HG2D
makes the first unnecessary (see [wine.md](wine.md)).

How far away that is, measured by what each retail module actually calls:

| | modules | `.text` | share |
| --- | --- | --- | --- |
| Portable as-is -- pure logic, or C runtime only | 16 | 213 KB | 21% |
| Needs a host for input and messages (USER32) | 7 | 217 KB | 22% |
| Needs a host for drawing (GDI32) | 4 | 563 KB | 57% |

The platform dependency is concentrated, not spread: `HG2D` (390 KB, the 2D
rasteriser), `MAIN` and `PANELS` account for essentially all of the drawing.
Sixteen modules -- including every one reimplemented so far and every one next
in the queue -- call nothing but the C runtime and can be built natively today.

So the order of work already serves the native goal: finish the portable layer,
then the input layer behind a small host abstraction, and leave the rasteriser
until last, where a native replacement is a renderer rather than a port.

**Floating point is kept portable deliberately.** Retail rounds on the x87, and
the reimplementation uses the same instructions on x86 so that "the same" is
not a matter of opinion. There is no x87 on ARM, so a parallel path goes
through `sqrt` and `nearbyint` instead -- and `make utildiff-portable` holds it
to the same standard, checking it against the retail DLL with the assembly
compiled out. It currently passes all 6,452,880 comparisons, so an ARM build
will compute the same numbers as an x86 one.

One service is known not to survive the move: UTIL's double-to-fixed-point
conversion depends on the x87's 80-bit intermediate precision, and ARM's
`long double` is 128-bit, which computes a *different* -- in fact more exact --
answer. The difference is a unit or two in 1/65536ths of a position -- around a
hundredth of a millimetre -- and the decision is to accept it rather than
emulate 80-bit arithmetic to preserve a rounding artefact. See
[modules/util.md](modules/util.md).

SIM1's current simulation path is verified on 32-bit x86: explicit integer C
retains register widths and flags, while x87 instructions and native stack
bridges retain retail's ABI and arithmetic. Its completed x86 replacement is
installed, but those bridges, pointer widths and FP operations still need a
native-platform implementation. See [modules/sim1.md](modules/sim1.md#complete-simulation).

## What is left that differs from the retail binary

`GLOBAL.DLL`'s 4764-byte shared state block is byte-identical to retail, and the
module id matches. The remaining differences from the retail binary are
cosmetic and belong in a future `OpenFS98-matching`, not here: the entry point
sits one byte later because gcc orders the no-op first, mingw emits unused
`__CTOR_LIST__`/`__DTOR_LIST__` tables into `.text`, the PE linker-version field
reads 2.41 rather than 5.00, and the version resource deliberately says
OpenFS98 instead of copying Microsoft's company name and copyright.

`GLOBAL`'s state block is the shared workspace: every module worked through
names more of its bytes, and those names are the project's real asset.
