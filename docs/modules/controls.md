# CONTROLS.DLL — module id 16

**In progress.** The hand-written assembly half is transliterated and the
module builds and links; the compiled-C half and the `controlsdiff` harness
are not done, so **nothing here is verified yet**. This document records what
the module is and what the reading found, so the work can be finished against
it.

CONTROLS is the keyboard, the joysticks and the mouse. Every key, button and
axis becomes a *command* — an id from `0x10000` with a value — which it
dispatches through a table of 383 handlers. The handlers move GLOBAL: the
throttles, propellers, mixtures and magnetos of up to four engines, the
elevator, ailerons, rudder and their trim, the brakes and parking brake,
flaps, spoilers, the radios digit by digit (COM, NAV1/2, ADF, transponder),
the autopilot's holds and targets, the clock, the eyepoint in the active
view, slew, and the sim's toggles (gear, lights, smoke, sound, pause, …).
Most of what changes is also reported to DEMO as a pilot event while a flight
video records.

## Binary and layout

- Retail image base `0x20670000`. `.text` 48,406 bytes (`0x20671000`..`+0xbd16`),
  `.data` 0xc020 (0xc00 initialised), `.rdata` 0x30d, `.rsrc`, 20 Linkage
  functions hand-counted (`dumplinkage.py` agrees).
- Win32: `GetKeyboardLayout`, `wsprintfA`; `LoadLibraryA`/`GetProcAddress`
  for DirectInput, which CONTROLS loads itself.
- Two programs, like ANALYSIS and DEMO: hand-written assembly from
  `.text+0x20` to `+0x78c1` with ATC's stack-alignment wrapper around every
  import call (no observable effect, not reproduced), then MSVC C from
  `+0x78d0`.

| slot | retail | what it is |
| --- | --- | --- |
| `+0x04` | `.text+0x20` | init: register CHAIN lists 0xb (commands), 2, 3; load the key config; start joysticks |
| `+0x08` | `.text+0xdb` | deinit |
| `+0x18` | `.text+0x211` | set the three axes' response curves (six words) |
| `+0x1c`/`+0x20` | `.text+0x64a5`/`+0x64b9` | suspend / resume the mouse-as-yoke capture |
| `+0x24` | `.text+0x6df2` | report the active view's zoom (NUMERIC text into GLOBAL) |
| `+0x28`/`+0x2c`/`+0x30` | | ailerons / elevator / rudder by a step, through the airspeed curve |
| `+0x34` | `.text+0x5ff2` | elevator trim by a step |
| `+0x38`/`+0x3c` | `.text+0x5d64`/`+0x5dd3` | an engine field (throttle/prop/mixture) of every selected engine, by/to |
| `+0x40` | `.text+0x6f93` | a pilot event to DEMO (while recording) |
| `+0x44` | `.text+0x3468` | dispatch a command `(id, value)` directly |
| `+0x48` | `.text+0x33e5` | report the radios to DEMO |
| `+0x4c`..`+0x5c` | C | the key config load/save/list, the dialog, force feedback |

## The command table

383 records of 20 bytes at `.text+0x15f9`, its address at `+0x15f1` and count
`0x17f` at `+0x15f5`: `{ handler, kind-mask, command, default-key, name }`.
342 distinct handlers. `tools/gen_controls_data.py` extracts it to
`controls_table.inc`; `controls.c` turns each handler offset into its
reimplemented function with `#define H(x) h_##x`. The kind mask selects which
key table a command may be bound in (4 flight, 8 slew, and the joystick
variants). The hand-written assembly keeps this table among its own code, so
the generator reads it out of `.text`, not `.data`.

## What the reading settled (hand-written half)

- **It runs on a model of the registers.** The assembly passes values between
  its routines in registers and, across module boundaries, leaves values that
  other modules read and reads values callers leave — DEMO's `+0x24` reads the
  `edi` CONTROLS calls it with, and DEMO reads the `ecx` CONTROLS `+0x44`
  leaves (docs/modules/demo.md). So `controls.c` keeps `eax`..`edi` in
  `controls_regs`; every import call goes through `controls_xcall` (model in,
  callee's leftovers out) and every entry through a stub (`CT_ENTRY`) that
  captures the caller's registers into the model and returns the model's
  `eax`/`ecx`/`edx`. This is the same pattern WINDOW's and DEMO's asm halves
  use; it still needs `controlsdiff` to confirm the leftover registers match.
- **The MINUS/PLUS keys and their shifted forms** index one of four 21-entry
  jump tables (`.text+0x3637`, `+0x36ab`, `+0x3c11`, `+0x3c85`) by `+0x288`,
  which records which radio, view or setting the last key selected — so `+`
  and `-` act on "the last thing touched". Reproduced as `by_mode` tables.
- **Repeat acceleration.** Many steppers reload `D+0xe8` to a count and, while
  it is still nonzero from the last frame (CHAIN list 2 runs it down), take a
  bigger step — the held-key "fast" behaviour.
- **The altitude-hold target rounds by dividing into overflow below the sea**,
  which the host's integer-divide repair saturates (host `0x4119a0`,
  docs/modules/host.md). Reproduced with the saturating `div32`.
- **The eye** goes through GLOBAL `+0x11a0` (the active view): kinds 0/1 pan by
  their own offsets and are re-aimed through a view callback (`.text+0x6bda`),
  kind 4 (the spot plane) by a heading and a distance it recomputes with UTIL.
- **Zoom with no active view** leaves `ebx` = 0 and writes through it; retail
  faults there too, so it is kept.
- **Pilot events to DEMO** pass 16-bit values; retail leaves the top halves as
  whatever the stack held, and DEMO reads only the words, so OpenFS98 passes
  them zero-extended.

## The compiled-C half — NOT yet transliterated

`.text+0x78d0`..`+0xbd1x`, reached through Linkage `+0x4c`..`+0x5c`:

- **The key config** (`+0x4c`/`+0x50`): read and write `[KEYBOARD_MAIN]` and
  `[KEYBOARD_SLEW]` of `FLTSIM98.CFG` through the host's profile and
  accelerator services, matching each command by name and kind, into two
  accelerator tables (ids `0x67`, `0x68`). The alternate path names a key by
  the keyboard layout (`GetKeyboardLayout`, host `+0x130`/`+0x184`).
- **The dialog** (`+0x58`, dialog `0x140a`, block `D+0x240`): a key-assignment
  list built per command, with UTIL's exchange (`+0x90`) for the joystick
  choice and NUMERIC for the readouts.
- **The joysticks**: host `+0x1c0` counts them, `+0x1c4` reads a state,
  `+0x1c8` parses caps; a CHAIN list-1 frame poll (`.text+0x90c0`) turns axis
  and button motion into WM_COMMAND posts, which return as list-0xb commands.
- **Force feedback**: DirectInput 5, loaded by CONTROLS itself (`dinput`,
  `DirectInputCreateA`), driven through COM. With no force-feedback device —
  what wine gives, and the first harness state — every path stops early.

It is all compiler-generated C with reused, optimiser-shuffled stack slots, so
it must be transliterated instruction-faithfully from the listing and verified,
not paraphrased. `src/controls/controls_input.c` currently holds honest
placeholders (they keep the joystick-state word the assembly reads back) so the
assembly half links and can be driven on its own.

## Host services used (arg counts from the callees)

New to this module (no finished module calls them, so `controlsdiff` must model
them): the profile read/write on `FLTSIM98.CFG` (`+0xa8`/`+0xac`, 3 args) and on
a path (`+0xd0`/`+0xe0`/`+0xf0`), the section services (`+0xf8`/`+0x100`/`+0x104`),
the accelerator table (`+0x1d8`/`+0x1dc`/`+0x1e4`/`+0x1e8`/`+0x1ec`/`+0x1f0`/`+0x1f4`/`+0x1fc`),
the joysticks (`+0x1c0`/`+0x1c4`/`+0x1c8`, winmm + registry), the keyboard layout
(`+0x130`/`+0x184`), the mouse capture (`+0x274`/`+0x278`), the menu (`+0x254`),
and `+0x48`/`+0x54`. Their `ret N` are recorded in `controls_input.c`.

## What is left

1. Transliterate `controls_input.c` against the listing.
2. `controlsdiff`: `sounddiff`'s `FLTSIM98.CFG` passthrough with mapping
   sections written per scenario (count configurations, not scenarios); a
   scripted joystick for `+0x1c0`/`+0x1c4`/`+0x1c8`; a mouse-capture model; a
   DirectInput stand-in, no devices first then one force-feedback device;
   `GetKeyboardLayout` hooked and fixed per scenario; commands posted through
   `+0x3c` logged; logging stubs for MAIN, HG2D, PANELS; retail UTIL, NUMERIC,
   SOUND, ACONTAIN, ATC, DEMO, WINDOW. Drive commands and compare the whole
   `.data` image, GLOBAL, the arena and the transcript.
3. Confirm the register leftovers (`+0x44`'s `ecx` for DEMO, the joystick
   calibrate's) and reproduce them if they differ.
