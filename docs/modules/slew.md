# SLEW.DLL — module id 9

Slew is FS98's free-movement mode: the aircraft is dragged through the world
instead of being flown. 1270 bytes of `.text`, and it is that small because it
holds no state of its own worth speaking of — it reads rate inputs out of
GLOBAL's shared block and integrates them back into the position and attitude
that live there too.

Imports: host (1), GLOBAL (2), UTIL (20). The only Win32 import in the whole
module is `DisableThreadLibraryCalls`.

## Shape

| address | what |
| --- | --- |
| `.text+0x000` | `DllMain`, the same 30-byte boilerplate every module has |
| `.text+0x020` | the tick — `Linkage` slot `0x18` |
| `.text+0x496` | rate-step helper |
| `.text+0x4e0` | bare `ret`, used for both `init` and `deinit` |
| `.text+0x4f0` | import thunk |

`Linkage` is 28 bytes: `{ id=9, init, deinit, 0, 0, 0, update }`. `ImportTable`
is `{1},{2},{20},{0}`.

Private data is 40 bytes at `.data+0`, all scratch except `+0x3004`, which is
the constant **30480** — metres, and 100,000 feet exactly. That one constant
pins the unit of the high half of the altitude field.

## What the tick does

1. Bail out if `global+0x365` is set.
2. Consume two one-shot requests: `global+0x5e0` (zero the attitude) and
   `global+0x5de` (recentre the rates, which also restores the two
   `0x4000`-centred rates).
3. Decide whether fast slew applies, gated on `global+0x390 >= 2` and on two
   fields of the aircraft object at `global+0x11a0`.
4. Rotate the two horizontal rate inputs from body axes into world axes using
   a sine/cosine pair from **UTIL slot `0x2c`**, keyed on the 16-bit heading at
   `global+0x582`.
5. Scale by the per-tick scalar at `global+0x2aa` and accumulate into position.
   Latitude takes its step directly; longitude goes through **UTIL slot `0x44`**
   first, which is what makes a degree of longitude narrow as you move away
   from the equator.
6. Integrate altitude, then clamp it: a floor from terrain height
   (`global+0x20` plus an offset at `global+0x120c`) and the 100,000 ft ceiling.
   Hitting either also cancels the rate that drove you into it.
7. Integrate attitude. Pitch wraps at ±`0x40000000` and when it does, bank and
   heading both get half a turn added — ordinary gimbal handling.
8. Square the three velocity components, pass the 64-bit sum to **UTIL slot
   `0x18`** (an integer square root), and publish the result as speed.

Positions are 48-bit: a 16-bit low half and a 32-bit high half, updated with
`add`/`adc` and `sub`/`sbb`, so the carry between halves is part of the format.

## UTIL's calling contract

Call sites look like this:

```asm
mov   DWORD PTR [esp-0x7], 0x4050607
and   esp, 0xfffffffc
sub   esp, 4
and   DWORD PTR [esp], 7
push  ...args...
mov   eax, ds:<UTIL linkage>
call  DWORD PTR [eax+0x2c]
add   esp, DWORD PTR [esp]
```

The magic constant is a four-byte table indexed by however much the `and`
consumed, so `[esp]` ends up holding the number of bytes to give back. The
important part for reimplementation is the last line: after the call, `[esp]`
is still the padding slot, which means **the callee had already popped the
arguments** — UTIL's services are `__stdcall`.

This module calls them without knowing what they compute. That is the whole
point of a hybrid decompilation: UTIL stays the retail binary, and only the
contract has to be right.

## The speed ramp

The helper at `.text+0x496` turns a speed-step count into a rate. Starting from
zero it applies `value += value/4 + 1` once per step, so the notches get
progressively further apart, and it saturates at 32767 once the steps keep
coming — step 26 gives 887, and from roughly 47 steps up it is flat out.

The loop looks at first glance as though it reloads its counter: it branches
back to a `movzx ecx,cx`. It does not. `CX` *is* `ECX`'s own low half and
`loop` has already decremented it, so that instruction is a no-op and the loop
runs exactly the step count. An earlier draft of this file claimed the ramp
never ran because of that instruction; the differential harness disproved it
within minutes by showing retail returning 887 where the rebuilt module
returned 32767.

## Two retail bugs, reproduced

**Three negations that can never fire.** Before squaring the velocity
components the code tests each against `0x0000ffff` and negates on sign. The
operand is 32-bit, so the result of that `and` always has bit 31 clear, the
sign flag is always clear, and the `jns` is always taken. Confirmed by reading
the module's scratch area after a tick: the velocity components are still
negative where the negations would have flipped them. Harmless either way, as
the values are squared on the next instruction, which discards the sign.
Omitted from the C as dead.

**A clamp off by a factor of sixteen.** In fast slew the speed is tested
against `0x3fffff` and, if it exceeds it, assigned `0x3ffff` — one digit short.
A speed over the threshold is therefore replaced by a value sixteen times
*smaller* instead of being limited to it. Reproduced as written.

## The sum of squares carries the wrong way

UTIL slot `0x18` reads its argument as an ordinary little-endian 64-bit value,
so `0x3018` is the low half and `0x301c` the high half, and each product's two
halves land where they belong.

What is wrong is the carry. Retail adds the high halves first and carries into
the **low** half:

```asm
add  DWORD PTR ds:0x208e301c, edx   ; high += product high
adc  DWORD PTR ds:0x208e3018, eax   ; low  += product low + carry from above
```

A correct 64-bit addition does the opposite: low first, carry upward. As
written, the low half's own overflow is discarded and a carry that belongs
nowhere is injected into it. Reproduced exactly rather than corrected.

An earlier draft of this file read the layout as big-endian with swapped
halves. That was wrong, and only became clear once UTIL's square root was
disassembled and shown to take a normal little-endian quadword.


## Fields this module names in GLOBAL's block

See `include/fs98/global.h`. Twenty-four offsets, covering the position and
attitude triples, the six rate inputs, the terrain floor, the per-tick scalar
and the published speed outputs — the first real dent in the 4776-byte block.

## Status

**Behaviourally verified.** `tools/difftest/slewdiff.c` loads the retail module
and the rebuilt one into one process, hands each its own copy of an identically
seeded GLOBAL block plus a shared stub UTIL, calls both ticks and compares the
blocks byte for byte. 200,000 randomised vectors, zero differences.

The stubs are deterministic but are not FS98's real UTIL functions, and they do
not need to be: both modules are driven through the same stubs, so a difference
in the output is a difference between the modules. What the stubs must get
right is the contract — stdcall, and the +2/+4 layout that slot `0x44` reads
and writes.

Caveat worth keeping in view: random vectors are not path coverage. The seeding
reaches fast slew in roughly 3% of vectors, but nothing here proves every branch
was taken, and the `0x3fffff` clamp in particular needs a large speed to reach.

The harness found four real defects in the rebuilt module that review had not:
latitude and longitude crossed, both altitude clamps inverted, an `att_reset`
test that should mask the low byte only, and the swapped-halves layout of the
sum of squares. It also disproved a claimed retail bug that was really a
misreading of the loop. It is reusable for any module whose behaviour is a
function of the GLOBAL block and its imports, which is most of them.
