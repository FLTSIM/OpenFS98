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
   surprisingly small, count the table by hand before believing it. It
   happened again with WINDOW: `dumplinkage.py` stopped at the flag word
   `0xc0000000` at `+0x10` and reported 2 functions of 32. CHAIN and FE carry
   the same word and had been hand-counted around it; the tool now accepts
   it at `+0x10`.
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

- **A flag can be gone before it is read.** DYNAMIC's refuelling does
  `shr edx,1` / `xor eax,eax` / `rcr eax,1`. Read as an idiom it puts the
  bit shifted out at the top of eax, but the `xor` clears the carry first,
  so `rcr` rotates in a zero. Written the idiom's way, every tank came out
  a unit high now and then -- found only once steering reached the
  proportional refuelling at all.
- **What a callee leaves in a register can be behaviour.** DYNAMIC's frame
  ends its slicing loop with `loop`, and when there are no objects nothing
  sets ecx in between: the count is the caller's ecx, or, on the frame that
  registers with CHAIN, whatever CHAIN's add left there. The C entry is a
  stub that passes ecx in, and that one CHAIN call goes through a thunk
  that hands ecx back. Inside the module too: the ramped move keeps its
  offset in ecx across the timer, and the timer's `path_end` uses ecx as
  scratch, so the first ramped step after a path moves by the altitude's
  low word. Check every register a routine keeps across a call against
  what the callee does with it.

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

**A deliberate break must actually be built.** Two of CHAIN's breaks first
"passed" because the edit and the rebuild happened inside one make timestamp
granule and the old DLL was tested. Delete the output before rebuilding a
break, and check the harness copy changed.

**An uncaught break may be reachable only from one state -- find it.** A
CHAIN break went uncaught until the harness seeded new lists with a garbage
depth of `0xffffffff`, the one value that makes the broken path reachable.
Ask what state the broken line needs, then make the generator produce it.

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

**Fixture data has to be able to match.** ATC's translator compares its keys
against upper-cased text without upper-casing the keys, so the first set of
fixture keys -- mixed case, as anyone would write them -- almost never
matched. Thousands of translated messages went through and a break in the
matching loop went unnoticed. Read what the comparison actually requires of
its inputs, then count the hits, not just the calls.

**A synthetic fixture should prove its own branch.** FE part 3 generates a
compressed texture and a BGL with exact-match, fallback, repeated, absent and
legacy sections. The harness does not count those calls as coverage merely
because retail and ours agree: it also checks the decompressed bytes, selected
section ids, output sizes and null/non-null results against constants. This
caught a section fixture that ran successfully but would otherwise have said
nothing about whether the intended selector won.

**Random vectors are not coverage.** They are good at the bulk and blind to the
rare. Say which is which when reporting.

**Steer onto boundaries with the module's own published state.** A `>` that
should be `>=` only shows on the exact value, and random data almost never
lands there. WEATHERM's harness got six breaks of that kind past 20,000 random
scenarios. They were caught once ops were added to land exactly on those
values:

- the aircraft placed exactly on a boundary the last update published in
  GLOBAL;
- an area's edge placed from the offset the module itself left in `.data`;
- a scenery effect aimed square to the current wind.

Reading state back to aim the next input is fine: both sides read their own,
and if they disagree the transcripts already do.

**Fixtures generated once are a fixed sample.** SOUND's harness first wrote
nine random `SOUND.CFG` trees at startup and reused them for every scenario,
so 2,000 scenarios saw nine configurations. Three breaks went uncaught
because none of the nine had the section they needed. Rewriting the tree
for each scenario that uses it caught them. Count configurations, not
scenarios.

**Floating-point breaks need their boundary worked out by hand.** Taking
log10 two different ways differs only in the last bits, and only under one
precision control. SOUND truncates the result, which hides that difference
everywhere except a handful of exact inputs. The break was caught only
after the arithmetic was carried through on paper to find one: an eyepoint
400 m less 2/65536 above an aircraft on the ground, under 24-bit precision.
At exactly 400 m both ways give the same integer.

**A stub has to keep what the real callee keeps.** Hand-written assembly
relies on registers surviving calls that the C convention lets a callee
clobber: UTIL's routines save ecx and edx (some eax too), and retail
DYNAMIC counts loops in ecx across them. The harness first reached UTIL
through logging forwarders written in C, so in the harness retail's loop
counts were corrupted, and in the game they never are. It showed as a
difference that looked like a bug in the reimplementation. The forwarders are
now asm thunks that keep what the real routine keeps. Read the callee's
pushes and pops as well as its `ret N`.

**A hang is an outcome; where it was cut is not.** DYNAMIC runs programs
from scenery files that nothing checks, and some spin forever -- in
retail too. A watchdog thread moves a call that has spun for a second out
of the module, and the transcript says `HANG`. But the spin's state when it
was cut depends on timing, so the scenario stops there and only the
transcripts are compared. Comparing the state anyway gave false
differences that looked exactly like bugs.

**A retail fault is a finding, not a harness bug.** When retail crashes on a
generated state, work out why before steering away from it. Then put the
reason in the generator's header and the module's doc, and make the
reimplementation fault the same way. WEATHERM's first run crashed retail on its
own reset defaults. That turned out to be a real overflow, reachable in the
game above 500 m.

**A fault in the harness may not be one in the game.** WINDOW's first runs
"crashed" retail in more than half the scenarios, on `idiv` overflows in the
look direction. They were too common to have shipped, and they had not: the
host runs the sim under a filter that repairs integer divide faults by
saturating the result (`0x4119a0`, see [modules/host.md](modules/host.md)).
Ending each scenario at those faults would have hidden most of the eye code
behind them. Before treating a fault as an outcome, ask what the game does
with it; WINDOW's harness now models the filter, and the reimplementation
computes the saturation without trapping.

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

**Compare the module's state, not only its effects, when the layout allows
it.** ATC keeps ImportTable, Linkage and every byte of state in one `.data`
section. The reimplementation keeps one image laid out the same way, with the
exports as aliases into it. So `atcdiff` finds both images at Linkage-0x40,
restores each to its as-loaded bytes before every run, and compares them byte
for byte afterwards: state that has not shown up in an output yet is still
checked.

**Give each side its own instance of a mutable dependency.** LANDME needs real
FE behavior rather than a small stub, but FE keeps a search cursor, open files
and a heap in its module image. If retail and OpenFS98 LANDME shared one FE,
the second call would inherit the first call's state and the comparison would
be about execution order, not LANDME. `landmediff` therefore loads two
differently named copies of retail FE, one per side, and compares both FE
images too. A dependency may be shared only when the services being called are
stateless; the retail UTIL routines used by LANDME meet that condition.

## An unverified quirk does not go into a verified harness

FE's `+0x40` looked, on the page, as if it broke its calling convention on one
path (writing to the caller's edi, returning with the registers shifted). The
right place to prove that is FE's own differential harness, against retail FE.
It was tempting to also retrofit the same model into DYNAMIC's *already
verified* harness -- DYNAMIC is `+0x40`'s only caller -- so that the 20,000-
scenario run would exercise it. That retrofit crashed one side with a null
write, because the register model was wrong, and for a few minutes the verified
DYNAMIC baseline was broken by a change that had nothing to do with DYNAMIC.

The lesson: a reading you cannot yet test is a hypothesis. Keep it inside the
module it belongs to, behind that module's own pending verification. Do not
thread it through a harness that currently passes -- a wrong hypothesis there
does not surface as "FE is wrong", it surfaces as "the thing that was green is
now red", and you have spent the baseline's credibility to test a guess. Revert
to the committed state, confirm green again, and let the owning module's
harness be the thing that confirms or refutes it.

That owning harness later confirmed the reading. `fediff` gives `+0x40` a
one-byte buffer with the caller's edi aimed at committed output space, then
records the returned size, bytes, ESP/EBP and preserved registers across an
assembly boundary. Retail really does write through edi and rotate
ebx/esi/edi; ours now matches it. The same boundary found a second retail bug
in `+0x18`: one unmatched `push edi` in scene state 4 rotates the preserved
registers on that return. The important sequencing remains the lesson: prove
the hypothesis at its owner first, then treat the now-measured behavior as an
interface fact elsewhere.

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
