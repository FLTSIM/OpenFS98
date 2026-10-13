# How work on this project is done

The state of the decompilation is in the README and the per-module documents.
This is the other half: how a service or a module actually gets from retail
binary to verified reimplementation, and the conventions that have earned their
place by catching mistakes.

## The loop

1. **Find the boundaries.** `make graph` for modules, `make hostmap` for the
   host. Both read the retail binaries live, so neither can drift.
2. **Check what it reaches for** before reading any code. A service that calls
   the host or another module cannot be checked by the current harness -- see
   *What cannot be tested yet* below -- so finding that out first saves
   disassembling something unverifiable.
3. **Disassemble it.** `objdump -d -M intel --start-address=... --stop-address=...`
   The sizes the mappers report are upper bounds: they are the gap to the next
   table-referenced address, so internal helpers fall inside them.
4. **Transliterate, do not rewrite.** See the next section.
5. **Add it to the harness** -- `tools/difftest/utildiff.c` for UTIL services,
   `tools/difftest/slewdiff.c` for whole modules -- with edge cases chosen
   from the code, not at random.
6. **Run it.** `make utildiff-run`, `make difftest-run`, and
   `make utildiff-portable` for anything touching floating point.
7. **Write down what it is** in the module's document, including anything that
   surprised you. The surprises are the valuable part.

## Transliterate, do not rewrite

This is the rule that has paid for itself most often. Write what the
instructions do, not what they appear to mean. Every one of these was a real
mismatch found by the harness after the "obvious" version looked right:

- **Half-width operations.** `sub ax` and `add ax` leave the top half of the
  register alone. UTIL's arctangent interpolation depends on it.
- **Partial doubling.** `add eax,eax / adc edx,0` doubles only the low half and
  folds the carry upward -- it is not a 64-bit shift. Writing `<<= 1` moved
  UTIL's longitude scaling by a unit.
- **Shift counts are masked to five bits.** A 48-bit shift by 32 shifts by
  nothing. Do not "fix" it.
- **Return widths.** `_ftol` returns 64 bits and callers often keep 32 or 16.
  Modelling it as 32-bit saturates where retail wraps.
- **Precision.** Retail keeps chains on the x87 at 80 bits. Two of UTIL's
  conversions give different answers in 64-bit double; use `long double`.

And reproduce bugs. Retail's sum of squares carries the wrong way, one of its
clamps is off by a factor of sixteen, and a conversion treats a signed value as
unsigned. All reproduced, all documented. The goal is a sim that behaves as
FS98 behaves, not one that is arithmetically tidier.

The exception is dead code with no observable effect -- a `cmp` whose flags
nothing reads, a negation that can never fire. Leave it out and say so, so the
omission reads as a decision.

## Testing

**A test that cannot fail is worse than no test.** Twice here a test passed
while checking nothing: once because it overwrote the result it was about to
compare, once because the game never left its menu and every screenshot was of
the main screen. The habit that catches this is to break the implementation on
purpose and confirm the harness goes red.

**Sweep exhaustively when the domain allows it.** 16-bit inputs are only 65536
cases and run in under a second: sine, arcsine and the 16-bit square root are
all covered completely rather than sampled.

**Choose edge cases from the code.** Range boundaries in a piecewise function,
both signs, zero, the equal-magnitude branch that avoids a divide, the x87 tie
cases, both sides of a 32-bit boundary. Random vectors will not find a branch
taken once in 2^32 attempts.

**Random vectors are not coverage.** They are good at the bulk and blind to the
rare. Say which is which when reporting.

## Two harnesses, and when to use which

**Outside the process** -- `make utildiff-run`, `make difftest-run`. Loads the
retail binary beside the reimplementation and calls both. Fast, scriptable,
millions of vectors, and the right tool for anything self-contained.

**Inside the sim** -- `make ofstest-run`. Builds `src/test/ofstest.c` as a
module with its own id, drops it into `Modules\`, and lets the real loader bind
and call it. Slower, needs the sim to start, and reports through a log file --
but it is the only way to reach anything that calls the host.

The split exists because the host cannot be loaded beside a test program: it
has no `.reloc`, so it only works at `0x400000`, and that address is already
committed in any wine process. Ten of UTIL's remaining services and all 194
host services need the in-process harness for that reason.

Anything running in the in-process harness runs during the sim's startup, so a
fault there stops the sim from starting. Be defensive, and take the module back
out with `make ofstest-remove` when not using it.

## Conventions

- Identifiers describing the original game take `fs98_` / `FS98_`; `openfs98`
  names only this project. Most of what gets written here is a description of
  someone else's 1997 build and should read that way.
- Generated files say they are generated and name the tool. `orig/`, `game/`,
  `cd/` and `bin/` are gitignored; no game content is committed.
- Offsets stay as raw numbers in comments so a number seen in a disassembly
  listing can be grepped for directly.
- Tables are extracted by a tool from the retail binary, never retyped.
