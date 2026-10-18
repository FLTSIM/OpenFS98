# ADVDRV.DLL — the adventure and lesson interpreter

ADVDRV (module id 21) runs FS98's compiled adventures (`.ADV`, in `ADV\`),
lessons (`.LES`, in `LESSONS\`) and challenges (`.CHL`, in `CHALLENG\`): small
bytecode programs produced by Microsoft's APL tools that narrate a flight,
drive the aircraft and weather, wait on the clock or a keypress, show text and
play sounds. It is reimplemented and verified: `make advdrvdiff-run` drives
retail and OpenFS98 through 20,000 whole-module scenarios over the CD's twenty
adventures and twenty-four lessons plus programs assembled by the harness, with
zero differences, after all its deliberate breaks were caught (one is a proven
equivalence). It runs in-game with `make smoke` passing.

## Shape and linkage

29,330 bytes of `.text`, MSVC C throughout with the x87 for every expression.
There is one 0x32a0-byte `.data` image (0x1400 on disk): `ImportTable` at `+0`,
`Linkage` at `+0x48`, then its tables and all of its state. Its 372
relocations are code pointers — the Linkage, the 0x2c-entry instruction table
at `+0xd8`, the operator `(id, handler)` pairs at `+0x198`, the 176 variable
descriptors at `+0x318`, the seven weather instructions at `+0x10f8` and the
0x2c-entry skip table at `+0x1230` — and six pointers into `.data`.
`tools/gen_advdrv_data.py` zeroes them all and lists each with the address it
held, and `advdrv.c` installs its own, pairing each retail address to the
reimplementation's routine. The generator also copies the `.rdata` constant
pool out as bytes, so every floating-point constant is retail's exact value
rather than a retyped decimal.

The imports are HOST, GLOBAL, CHAIN, DEMO (`+0x28`, to play a lesson's video),
MAIN, SOUND, UTIL and WINMSG. The debug console is KERNEL32 directly
(`AllocConsole`, `WriteFile`, …).

| slot | retail | what |
| --- | --- | --- |
| `+0x04` | `.text+0x020` | init: CHAIN lists 5, 3, 0xb; the CRC32 table; the WAV catalogue size |
| `+0x08` | `.text+0x100` | deinit |
| `+0x18` | `.text+0x160` | the type of program running, if one is (MAIN) |
| `+0x1c` | `.text+0x290` | load and start a program `(path, ext)` (MAIN) |
| `+0x20` | `.text+0x1f0` | restart the loaded program |
| `+0x24` | `.text+0x190` | stop |
| `+0x28` | `.text+0x56c0` | the adventure picker (dialog 0x1108) |
| `+0x2c` | `.text+0x56e0` | the lesson picker (dialog 0x1106), which remembers its place |
| `+0x30` | `.text+0x880` | the next CHAIN list 0x14 "stopped" (0x21) is ours, not a stop |

## The program format

A 0x200-byte header — `"FS6X Adventure\x1a"` (or the older `"FS6 Adventure\x1a"`),
a version at `+0x150`/`+0x154`, a CRC32 of the body at `+0x158`, a string count
at `+0x15c`, the code-and-variable size at `+0x160`, and a "scan size" at
`+0x164` — then the bytecode. `+0x1c` reads the header, checks the magic, the
version (6.1, or 1.0) and the CRC, copies the header into `.data`, then:

- **version 6.1** keeps its strings in a table after the code; they are moved
  to separate allocations and the code buffer is shrunk to `+0x160`.
- **version 1.0** is lightly encrypted: each body byte is its own offset's low
  byte minus the byte. Reproduced.

The frame callback, on CHAIN list 5 (or 1..3, as the program's `rate`
instruction asks), runs up to three instructions a frame. There are 0x2c
instructions (a jump table at `.data+0xd8`): jump, call, return, switch and
switch-call, `for`/`next` loops, assign, wait, messages, sounds, situations,
videos, the aircraft position, weather, key bindings, events, the rate, the
console, and the error/stop pair.

Expressions are a stack machine over doubles (0x32 operators at `.data+0x198`):
constants, the program's own variables (doubles in the code buffer) and 176
**system variables**, each a 0x14-byte descriptor at `.data+0x318` with a
getter and a setter over GLOBAL — a radio frequency in BCD, the aircraft's
latitude as degrees, a control axis, the DME, and so on. A getter returns an
x87 value; a setter validates and writes GLOBAL, alerting on a value out of
range. Events (`when`, the edge form, the change form) evaluate an expression
each frame and call a handler on true, on an edge or on a change.

## How it is transliterated

MSVC C over the x87, so it reads as C. The expressions are evaluated as retail
evaluates them: in `long double` where retail keeps a value on the x87 stack,
rounded to `double` where it stores one (the operand stack and the program
variables are `double`), comparisons through the x87's own condition codes (so
a NaN compares as retail's `fcom`/`fnstsw`/`test` make it — below and unequal),
and `idiv` for the integer divide and modulo, so a zero divisor or `INT_MIN /
-1` faults as retail's does. MSVCRT's `sprintf`, `_CIfmod`, `_CIasin` and
`modf` are bound by name and called for real, so number formatting and the
transcendental functions are the same code. `_ftol` is modelled as retail's:
truncation, 64 bits, the indefinite integer out of range, callers keeping the
low half.

## Retail behaviour worth knowing

All reproduced.

- **`op_error` is a use-after-free.** A bad instruction, a bad operator, an
  operand-stack over- or underflow, a call-stack overflow, a bad variable
  reference — each calls the internal error routine (`.text+0x1bc0`), which
  **stops the program** (freeing the code buffer) and shows alert 0xf439,
  then **returns to the interpreter**, which keeps running on a now-dangling
  cursor. What it does from there is undefined: sometimes it limps along a
  few instructions, sometimes it faults at once, and the two builds diverge.
  The harness treats the 0xf439 alert as a poison point (see below).
- **A dead sentinel check.** The feet getter (`.text+0x3d90`) loads a 16-bit
  value, zero-extends it, then compares the 32-bit result to `0xffffd8f1` —
  which the zero-extension has made unreachable. So the "−9999 means none"
  case never fires for this getter, though the near-identical `.text+0x4140`
  getter (which compares the 16-bit value directly to `0xd8f1`) does. Both
  are reproduced exactly; the dead one is the proven-equivalent break.
- **Latitude counts from a pole, longitude from the prime meridian.** The
  `north`/`south`/`east`/`west` instructions flip the sign convention, and
  the coordinate conversions negate accordingly. A latitude is clamped to
  ±90°, a longitude wrapped mod 360.
- **48-bit fixed point is sign-and-magnitude.** The position converter adds
  (or subtracts from zero) 2^−17, takes the integer part as the high dword
  and the magnitude of the fraction as the low word — so a negative position
  is not two's-complement.
- **Displaced weather layers keep the new layer's bounds.** When a cloud,
  wind or temperature layer is inserted and pushes an existing layer up a
  slot, the pushed layer is re-sorted but some of its fields are not
  reloaded, so it inherits the inserting layer's top altitude. The fourth
  temperature slot never records its layer id. Reproduced from the asm's
  register reuse.
- **`events forget` skips a node.** Dropping the events that call a handler
  unlinks a node and then advances past the node that followed it, so a run
  of two matching events in a row loses only the first. Reproduced.
- **The sound cache.** A program's sounds, when not already present, are
  copied from the CD's `fs98\adv\wav` into the cache directory and marked
  read-only; on stopping, the cached copies are deleted. The copy, not the
  source, gets the attribute; the delete is from string 0x900e's directory.
- **Faults.** The integer operators divide with `idiv`, so a program that
  divides by zero takes a #DE; ours takes the same. These are compared as
  outcomes.

### Dead code

The variable getter's dead `0xffffd8f1` test (above) is kept and marked. No
other dead code was found to omit.

## Differential harness

`tools/difftest/advdrvdiff.c`, built on demodiff's pattern. Both DLLs are
loaded into one process and driven through Linkage and the CHAIN callbacks
they register — a load, the per-frame dispatch, the menu pickers, bound-key
commands, the "stopped" message, and the stop/restart/running/skip entries —
one side after the other over trees rebuilt the same way for each.

- **host:** a stub. Memory is a bump arena reset before each side, so heap
  pointers come out the same; what the host does not zero is filled with
  0xa5. Files and listings pass through to real `ADV\`, `LESSONS\`,
  `CHALLENG\`, `SITUATIO\`, `PILOTS\`, `VIDEOS\` and a CD `fs98\adv\wav`
  tree, behind transliterations of the host's path, find and file services
  and its extension table (read out of `FLTSIM98.EXE`). String resources are
  `ENGLISH.FLL`'s. The clock, the config ints, the voice engine and the key
  maps are the scenario's. The dialog engine is a stand-in for the two
  pickers.
- **UTIL:** retail's — the catalogue (`+0x98`) real over the trees, the
  modeless alerts real over the stub host, the conversions and the random
  generator logged around retail's; the modal alert answered. Its `.data`
  (the generator's seed) is restored before each side.
- **CHAIN:** emulated, with duplicates allowed, removal during a dispatch
  deferred, and an add during one visited by it — the harness dispatches the
  lists itself.
- **MAIN:** `+0x3c` (load a situation), `+0x70` (range and bearing, a
  function of the bytes retail defines so no stack leftover leaks), `+0x74`,
  `+0x80` (the DME), `+0xa4` logged.
- **DEMO, SOUND, WINMSG** logged; WINMSG hands out message numbers.
- **GLOBAL** is random per scenario, with `+0x384` aimed at three navaid
  records and `+0x11d4` at a stub module, the −9999 sentinel sprinkled into
  the weather-readback words, and steered as the programs run.

**Programs** are the CD's forty-four, and synthetic ones written for every
scenario by an assembler in the harness: every instruction, operator, text
piece and system variable (read and written), loops, calls, switches, events
of all three kinds, key handlers, weather and the error paths. One program in
six is "wild" and may emit malformed code — a bad operator or instruction, an
operand-stack over- or underflow, an out-of-range variable reference — to
exercise `op_error`; the rest are always well-formed. The scan region always
ends on a real instruction boundary and never reaches a malformed opcode, as
the APL compiler's `scan_size` does.

**Faults** in either module are outcomes: the op is abandoned (the x87 reset)
and the next op runs. A fault that frees the code (`op_error`'s stop) leaves
the whole image as post-free garbage, so only the persistent heap and GLOBAL
are compared then; while a program is stopped the dead interpreter cursor is
masked. An op past a fixed count of host calls is cut at that count on both
sides (a program that sets its rate to the list it runs on re-enters the
dispatch and would hang). A watchdog reports a spin.

**Poisoning.** Once `op_error`'s 0xf439 alert fires, the program's code is
freed and anything it touches afterward is undefined and diverges between the
builds; retail sometimes survives the use-after-free where ours faults. Both
sides reach the alert identically, so from that alert the harness suppresses
the rest of the scenario's transcript (a `POISON` marker, emitted once on both
sides, then nothing). Error-triggering constructs are kept to the wild
minority so most scenarios keep full state comparison.

Compared after every operation: the transcript of every import call, the
whole `.data` image (code pointers through a pairing learnt in order of first
sight, pointers into the image as offsets), the arena, GLOBAL and UTIL's
`.data`, and at each scenario's end the sound cache's files.

Result: **20,000 scenarios, 0 differences**. `make advdrv-cover` builds a
counting ADVDRV and confirms every instruction, every operator and every
readable and writable system variable is exercised, with the error paths
firing.

Deliberate breaks, each built into `ADVDRV_ours.dll` (`make advdrv-break-N`)
and run over 200 scenarios: **10 tried, 9 caught, one proven an equivalence.**
The caught ones were the 256ths sentinel, the `wait` tick scale, the position
instruction's GLOBAL flag, the COM 25-kHz step, the key-binding encoding, the
48-bit fraction's sign, the sound-cache attribute, the latitude clamp, and the
visibility rounding. The equivalence is the feet getter's `0xffffd8f1` test,
dead behind retail's own zero-extension (above).

## Portability

Pure logic over host files, GLOBAL and the x87, with MSVCRT's `sprintf` and
three transcendental calls. The x87 path needs the same treatment as UTIL's
for a native build, and the precision control matters: the expression machine
keeps intermediates at 80 bits and rounds to 64 on the operand stack.
