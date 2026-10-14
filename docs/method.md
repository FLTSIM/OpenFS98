# How work on this project is done

The state of the decompilation is in the README and the per-module documents.
This is the other half: how a service or a module actually gets from retail
binary to verified reimplementation, and the conventions that have earned their
place by catching mistakes.

## The loop

1. **Find the boundaries.** `make graph` for modules, `make hostmap` for the
   host, `python3 tools/dumplinkage.py orig/Modules/<M>.DLL` for one module's
   service table -- it prints every slot, the size of the function behind it,
   and a total, so the sizes in the module documents need no separate script.

   **Distrust the mappers until they have been checked against a hand count.**
   Both have been wrong in the same way: `hostmap.py` once reported zero slots
   because it stopped at the first null, and the table has interior gaps;
   `dumplinkage.py` capped every table at 32 entries, which silently truncated
   UTIL's Linkage at `+0x7c` and hid 19 slots -- including all eight services
   still to be done. A tool that under-reports looks exactly like a small
   module. Both are fixed, but the habit is the point: if a module looks
   surprisingly small, count the table by hand before believing it.
2. **Check what it reaches for** before reading any code. An imported service
   is externally testable when its behavior can be represented by a small,
   faithful stub; NUMERIC does this for the host, GLOBAL and UTIL. Code which
   needs the real host process or live game state belongs in the in-process
   harness. Decide which applies before disassembling the whole service.
3. **Disassemble it.** `objdump -d -M intel --start-address=... --stop-address=...`
   The sizes the mappers report are upper bounds: they are the gap to the next
   table-referenced address, so internal helpers fall inside them.
4. **Transliterate, do not rewrite.** See the next section.
5. **Add it to a harness** -- the existing examples cover individual UTIL
   services, whole-module SLEW and imported-service NUMERIC. Use edge cases
   chosen from the code, not at random.
6. **Run it.** The current targets are `make utildiff-run`,
   `make numericdiff-run`, `make difftest-run`, and `make utildiff-portable`
   for UTIL's portable floating-point path.
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
- **Argument counts come from the callee, never the call site.** The optimiser
  interleaves pushes across calls: UTIL's dialog builder pushes two arguments
  for `host +0x20c`, then calls `host +0x2b8` in the middle of doing it. Because
  `+0x2b8` is `ret 4` it pops only its own argument and the other two survive to
  become `+0x20c`'s. Counting pushes before the `call` said `+0x2b8` took three
  arguments and `+0x20c` took two; both were wrong. Disassemble the target and
  read its `ret N`.
- **An `[esp+N]` is relative to `esp` at that instruction.** Every push moves
  it by four, and the optimiser interleaves loads with pushes. UTIL `+0x94`
  does `lea edx,[entry]` / `mov ecx,[esp+0x120]` / `push edx`: the load comes
  *before* the push, so it reads the entry's size at `+0x108`, not its type at
  `+0x104`. Read the other way it compiles, runs, and returns the right number
  of records with the wrong field in every one.

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

**A break the harness misses is either a gap or an equivalence -- prove which.**
UTIL's flag decoder has a search order that no input can observe, and breaking
it changed nothing; that was shown arithmetically before it was accepted. "It
probably doesn't matter" is not the same finding.

**When the static picture says something is impossible, look at the running
sim.** Static search found nothing registering dialogs 2 and 3, which would
have meant UTIL's alerts always failed. One in-process probe of the live
registry showed 65 dialogs, from modules in `English\` that the tooling had
never scanned. A probe that reads host memory runs during startup, so bound
every count and pointer it follows: the first version trusted a word that was
not a list header and took the sim down.

**Know which build a comparison is against.** In-process tests compare the
reimplementation with "retail's" module by calling whatever the loader bound.
Once OpenFS98's UTIL was installed that was the reimplementation itself, and
every comparison would have passed for nothing. `make ofstest-run` now puts
retail's UTIL in for the run, and a check fails if the UTIL it is talking to
is not at retail's Linkage address.

**A crash must end the run and say where.** Under wine an unhandled fault
opens a dialog and the harness sits at 0% CPU until someone closes it.
`utildiff` installs an unhandled-exception filter that prints the faulting
address, which implementation was running, the vector number and its inputs,
and exits. The first exchange-engine fault was found that way in one run:
retail writing through a callback pointer the test generator should never have
paired with a width handler.

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

**Outside the process** -- `make utildiff-run`, `make numericdiff-run`,
`make difftest-run`. Loads the retail binary beside the reimplementation and
calls both. Fast, scriptable, millions of vectors, and the right tool for
self-contained code or code whose imports can be stubbed faithfully.

**Inside the sim** -- `make ofstest-run`. Builds `src/test/ofstest.c` as a
module with its own id, drops it into `Modules\`, and lets the real loader bind
and call it. Slower, needs the sim to start, and reports through a log file --
but it is the only way to reach anything that calls the host.

The split exists because the host cannot be loaded beside a test program: it
has no `.reloc`, so it only works at `0x400000`, and that address is already
committed in any wine process. UTIL's dialog trio and all 194 host services
need the in-process harness for that reason.

Anything running in the in-process harness runs during the sim's startup, so a
fault there stops the sim from starting. Be defensive, and take the module back
out with `make ofstest-remove` when not using it.

## Editing `util_math.c`

The file is append-structured: one `/* ---- slot 0xNN: ... */` banner per
service, in the order they were written rather than in slot order. Anything
that splices by marker must use *that section's own* end, which is the next
banner -- not a banner chosen by eye from further down. Getting that wrong
deletes every service in between, and the compiler reports it as a handful of
undeclared identifiers rather than as the twenty missing functions it is.
`grep -n '^/\* ---- '` first.

## Conventions

- Identifiers describing the original game take `fs98_` / `FS98_`; `openfs98`
  names only this project. Most of what gets written here is a description of
  someone else's 1997 build and should read that way.
- Generated files say they are generated and name the tool. `orig/`, `game/`,
  `cd/` and `bin/` are gitignored; no game content is committed.
- Offsets stay as raw numbers in comments so a number seen in a disassembly
  listing can be grepped for directly.
- Tables are extracted by a tool from the retail binary, never retyped.
