# CONTROLS.DLL — module id 16

**Done, verified and running in-game.** `make controlsdiff-run` drives
retail and the OpenFS98 build side by side over 20,000 scenarios -- commands
through the dispatcher and through CHAIN list 0xb, the axis/engine/event
entries, the CHAIN frames (lists 1, 2, 3, 6 and 0x14), the joystick poll and
watcher, the key bindings (`+0x4c`/`+0x50`/`+0x54`), init/deinit, force
feedback (`+0x5c`) over a DirectInput 5 stand-in, and the key-assignment
dialog (`+0x58`) over a stand-in for Options.DLL's -- comparing GLOBAL, the
whole `.data` image, every import call and the profile files as each side
left them: **0 differences** (24,372 dialogs, 19,342 of them answered OK,
with 158,089 edits and 44,949 callbacks; 153,296 dispatches, 118,239
joystick polls, 25,448 key loads, 55,251 force-feedback calls with 269
crashes, 31 of them reaching their second part; 1,347 faults taken alike by
both sides; about 40 minutes). `make controls-breaks` breaks the module on
purpose: 81 breaks, 78 caught and 3 proven equivalent (`list-first`,
`current-mask` and `throttle-floor`, below), about 20 minutes. `make smoke`
boots the game with it installed: 27/27 modules, 95 gauges, PASS.

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

- This is the reference pressing's CONTROLS (README.md, "Which CD"). A later
  pressing's, dated 17 October 1997 and linked at `0x10000000`, is a
  different build: `.text` 55,654 bytes and 493 commands.
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
| `+0x04` | `.text+0x20` | init: register CHAIN lists 0xb (commands), 2, 3; read `PAN_RATE`; put the joystick poll and watcher on list 1 |
| `+0x08` | `.text+0xdb` | deinit: save `PAN_RATE`, `[PREFS] JOYSTICKS` and the joystick configs |
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
`0x17f` at `+0x15f5`: `{ handler, kind-mask, command, display-string id, name }`.
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
  use; `controlsdiff` compares the ecx `+0x44` leaves, the one DEMO reads.
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

## The compiled-C half

`.text+0x78d0`..`+0xbd16`, reached through Linkage `+0x4c`..`+0x5c`, the
assembly's init/deinit and CHAIN, is compiler-generated C in
`src/controls/controls_input.c`, transliterated from the listing with each
optimiser-shuffled `[esp+N]` resolved by hand.

### The key bindings (`+0x4c` load, `+0x50` save, `+0x54` list)

Each command of a kind (4 flight, 8 slew) is looked up by name in
`[KEYBOARD_MAIN]`/`[KEYBOARD_SLEW]` of FLTSIM98.CFG (`section_read`, host
`+0x100`/`+0xb0`) and, failing that, in `[<section> <layout>]` -- the keyboard
layout's handle, `"%s %08X"` -- of the devices file (`section_read_layout`,
`+0x104`/`+0xd8`). A key spec is "key,flags" (`+0xb0` parses with
`strtoul(p, &p, 0)`). Without either the default table, ENGLISH.FLL's
accelerator resource `0x67` or `0x68`, is loaded (`+0x1e4`). The host keeps
the tables (a list of `{ active, id, entries, count }`, 12-byte entries `{ key,
flags, command }`); `+0x50` writes them back and destroys them; `+0x54` packs a
command's key as `key + (flags << 8)`, the last of several winning. `+0x4c`
ends by tail-calling the force-feedback begin, and `+0x50` begins with its end.

### The joystick poll (CHAIN list 1, `.text+0x90c0`)

Reads each attached joystick (`+0x1c4`) and maps it to commands (`joy_map`,
the axis curve `axis_curve`), posted as WM_COMMAND (`+0x3c`) that come back as
CHAIN list-0xb commands. A button is a word, 0 or 1 (the host's read,
`0x40b8f0`, sets buttons 0..31 from `joyGetPosEx` and one of 32..39 from the
POV hat); a press posts the down command, a held button posts it again every
repeat interval (config `+0x1cc`, in polls), and the release posts the up
command. The curve's very-negative case returns `+0x4000` without the final
negate -- a retail quirk, kept.

### The joystick configs (`.text+0x91d0`, `+0x93b0`..`+0xa3ff`)

Each joystick has a flight and a slew config, 0x3cc bytes at `D+0xd48` /
`D+0x82c8` + joystick*0x3cc: LOCKED, TYPE, AXIS_FLAGS, then 16 axes' event,
scale and null, and 64 buttons' down event, repeat interval and up event.

- **The watcher** (`.text+0x93b0`, on CHAIN list 1 from init): asks the host
  how many joysticks answer (`+0x1c0`); each new one gets its configs loaded
  from `[JOYSTICK_nn]`/`[JOYSTICK_SLEW_nn]` (NUMERIC format 0xb, two digits)
  or, without a section, its model's defaults; when the first appear the
  joysticks are enabled from `[PREFS] JOYSTICKS`. Once the count holds,
  `D+0x43c` (32) frames later it moves itself to list 6. (The first
  transliteration called it `ff_frame`, a per-frame force-feedback update; it
  is not.)
- **An event** is read as text and matched by name against the command table
  (`+0x2c0`), else read as a number; written back by name, else as a number.
- **The defaults**: the host describes the stick (`+0x1c8`: its model's string
  id from ENGLISH.FLL `0xb000 + 4n`, a rudder flag, the OEM name); the devices
  file's `[<name> RUDDER]`, `[<name>]` (and their ` SLEW` forms) win, else the
  model's string `id+1` (flight) or `id+2` (slew), comma-separated integers.
- **Deinit** saves `[PREFS] JOYSTICKS` and every config.

### Force feedback (`+0x5c`, `.text+0xa400`..`+0xbd10`)

- **Settings**: seven ints in `[CONTROLS]` (`force_master_enabled`,
  `force_master_gain` and five per effect), read by the begin and saved by the
  end and by op 2.
- **The device**: CONTROLS loads `dinput` itself and calls
  `DirectInputCreateA` (version 0x500), enumerates attached force-feedback
  joysticks, creates the first, queries `IDirectInputDevice2A`, sets
  `c_dfDIJoystick`, cooperation level 5 with the main window (host `+0x54`)
  and the autocentre off. With force feedback off it only probes.
- **The effects**, all polar over X and Y: the stick force (constant), the
  stick shaker (square), three gear bumps (sine, a quarter-phase on, period
  four times the duration), two ground bumps (sine, period `0.5 + 1e6 /
  (slope * speed + intercept)` on the x87), and a crash in two parts. Their
  parameters default in `.text+0xaeb0` and come from the aircraft's own
  `[forcefeedback]` section, the file ACONTAIN `+0x30` names. An update
  re-acquires: `DI_OK` downloads and starts it again, `S_FALSE` sets its
  direction and magnitude.
- **`+0x5c(op, blk)`**: 1 reads the settings, 2 sets them and brings the
  device up or down; with the device up and neither a crash nor a pause
  holding it, 3 the stick force (two doubles; direction `atan2` in
  hundredths of a degree from north, magnitude clamped to ±10000), 4 the
  shaker, 5..7 the gear bumps, 8 the crash (its end holds every effect off
  until message 0x60), 9 the ground bumps at a speed.
- **Per frame** (list 1, while the device is up): the stick force made when
  wanted, everything off while slewing or with the joysticks off, the bumps
  reaped as they end, the crash's second part after its first. **Messages**
  (list 0x14): 0x20 stops everything, 0x38/0x40/0x42/0x44 the shaker and the
  ground bumps, 0x60 rereads the aircraft's settings.

### The key-assignment dialog (`+0x58`, `.text+0x7e40`..`+0x8e5f`, `+0x9130`)

Dialog `0x140a` of `English\Options.DLL`, run through the host's dialog
engine (`+0x10c`) on the block at `D+0x240`. Setup builds two lists, flight
(`D+0x270`) and slew (`D+0x280`): every command a kind can bind, in the order
of its display string id (the command table's fourth field, an ENGLISH.FLL
string, copied through host `+0x2a4`), with each device's bindings -- the
keyboard's from its accelerator table, each attached joystick's from its
config -- and the sensitivity records (GLOBAL `+0xc02`..`+0xc0c`, labelled
from ENGLISH.FLL), the force-feedback settings (op 1) and the joysticks'
enable (UTIL's exchange, one dword at `D+0xb00`). Options.DLL's pages edit
them and call back: `+0x1c` resets a device's sensitivities (0 the keyboard,
2 the mouse, 1 a joystick's axis scales and nulls to 100/36), `+0x20` counts
the joysticks again after the joystick control panel, `+0x4` does nothing.
`+0x2c` would reset a device to its defaults, but Options.DLL never calls it.
OK rebuilds both key tables, both joystick configs (from the bindings
alone: AXIS_FLAGS the bound axes, LOCKED, TYPE and every button-up event
dropped -- retail's), the sensitivities and force feedback (op 2).

### Bugs the verification found, all fixed

- `joy_map` cleared a held button's repeat counter every poll, so nothing
  repeated (retail clears it only while the button is up, `.text+0x9078`).
- `section_read_layout` returned 0 when its section existed, so `+0x4c`
  loaded the default table on top; called host `+0xd8` with `parsed` and the
  count swapped; and passed the devices file's bare name, not the joined path.
- `controls_keys_list` stopped at the first key of a command (retail keeps
  the last), and returned 0 where retail returns `+0x1fc`'s failure.
- `+0x50`, init and deinit returned the wrong eax.

### Retail quirks kept

- The integer parser (`.text+0xa200`) steps past the terminator after
  trailing blanks; the buffer is zeroed, so what follows reads as empty.
- `write_cmd` writes a nameless command's null name, which deletes the key.
- OK rebuilds a joystick's config from the dialog's bindings alone, so its
  button-up events, LOCKED and TYPE are lost.
- The defaults are skipped when the *current* table (flight or slew) has the
  stick locked, and a model whose string is missing installs whatever the
  stack held (no model the host names lacks its strings).
- With no force-feedback joystick enumerated, the device is created from an
  uninitialised stack GUID; the stand-in refuses any GUID then.
- A periodic effect of a kind outside 0..2 would start the device as if it
  were the effect; no caller passes one.

## Host services used (arg counts from the callees)

The profile on FLTSIM98.CFG (`+0xa8`/`+0xac` int, `+0xc8`/`+0xcc` string,
`+0xb0`/`+0xb4` key spec, `+0xf8` clear, `+0x100` section present) and on a
path (`+0xd0`/`+0xe0`/`+0xf0`/`+0xd8`, `+0x104`, `+0x158`), the sim's directory
and joining (`+0x130`/`+0x184`), the accelerator tables (`+0x1d8`..`+0x1fc`),
the joysticks (`+0x1c0`/`+0x1c4`/`+0x1c8`), strings and the heap (`+0x2a0`,
`+0x2a8`/`+0x2b0`/`+0x2b8`/`+0x2c0`, `+0x20c`/`+0x214`), the window and
instance (`+0x54`), the mouse capture (`+0x274`/`+0x278`), the menu (`+0x254`).
Each is modelled in `controlsdiff` from its listing.

## controlsdiff

`tools/difftest/controlsdiff.c` loads retail `CONTROLS.DLL` and the OpenFS98
build side by side (ImportTable at image+0, Linkage at +0x78), binds GLOBAL to
a shared array and UTIL/NUMERIC to retail, and drives:

- **the hand-written half**: the dispatcher `+0x44` over every command id,
  commands through CHAIN list 0xb (with list 0xc's "taken" answer scripted),
  the axes, engines, pilot event, radios, curves and yoke, list 2's frames
  (the repeat countdown and every sixth frame's brake step-back) and list 3's
  coordination, init and deinit;
- **the joysticks**: CHAIN list 1 run whole (the poll, then the watcher, then
  the force-feedback frame), list 6, a scripted joystick count and stick
  descriptions, and sticks read as host `0x40b8f0` reads them: the caller's
  axis mask narrowed to the device's axes (each of `wCaps`' Z/R/U/V takes
  one of `wNumAxes`, the rest shift in low bits, and a short count wraps to
  `0xffff`), only those axes written, and the hat, on a device that has one,
  as one of buttons 32..39 by its angle's octant; buttons held, repeated and
  released;
- **the profiles**: the host's profile services passed through to the Win32
  API over FLTSIM98.CFG, the devices file and an aircraft file written per
  scenario (joystick sections, keyboard sections, the layout's sections,
  force-feedback settings), ENGLISH.FLL's strings (sometimes mutated) and
  accelerator resources; the files as each side left them go into the
  transcript;
- **the key bindings**: `+0x4c`/`+0x50`/`+0x54` over a model of the host's
  accelerator tables;
- **the dialog**: `+0x58` over a stand-in for Options.DLL's dialog
  `0x140a` (create `0x22391930`, three tab pages), built from its listing:
  key and joystick assignments as its pages make them (a conflicting binding
  cleared, or the edit abandoned), extras, sensitivities, force-feedback
  settings, the joysticks' enable, the list the page shows, the callbacks as
  the pages call them, and OK or Cancel; what setup built is logged as a
  digest of its contents. It also calls `+0x2c`, which Options.DLL does not,
  so that code is verified too;
- **force feedback**: `+0x5c` weighted as the game calls it, and list 0x14's
  messages, over a DirectInput 5 stand-in (`LoadLibraryA`/`GetProcAddress`
  patched in both builds' import tables): no DirectInput, one that will not
  create, one with no force-feedback joystick, or one with such a joystick;
  fake COM objects with every method's arity log each call by content (a
  DIEFFECT whole) and answer from the scenario. A crash is usually followed
  by a few list-1 frames, as in the game, so its first part can end and its
  second start (the second part needs the device up, crashes enabled, the
  joysticks on and no slew: 31 in the full run's 20,000 scenarios).

It models the host's integer-divide repair (the keyboard axis curve divides
by a zero knee). For break runs it stops at the first difference
(`CONTROLS_FIRST`).

Finding the harness: a transliteration bug in `controls_xcall` (a phantom
first argument) surfaced on its first run; code pointers cannot be paired
across builds of different sizes, so the digest blanks the fixed pointer
fields. The dialog's two lists are heap blocks that `dlg_cleanup` frees and
nulls, but a fault in OK's force-feedback restart (a DirectInput without
`DirectInputCreateA`) skips the cleanup. Retail's leaked blocks then move
where ours allocates, so the digest compares the four list pointers as null
or not (nothing reads them before the next setup rebuilds them). A
300-scenario run never hit it; the first full run differed 6 times in its
first 1,000, and until it was fixed every break run "caught" its break there
at the latest. `current-mask` had shown as caught only that way. With the
artifact gone the break run missed it and `crash-second`, which was caught
before the dialog op was added but which no scenario of the new mix reached;
the frames after a crash now reach it.

Three breaks in `make controls-breaks` are proven equivalences:
- `list-first`: no table gives a command two keys (neither default table
  does, a profile section names each command once, the dialog rebuilds a
  table from one binding per command, and the one id with several names, 0,
  is never bound).
- `current-mask`: `joy_current` writes the device's axis mask into
  `build_list`'s stack copy of the config, which only `fill_joy` reads, and
  `fill_joy` reads the axis and button commands and their extras, never
  `+8`.
- `throttle-floor` (`<` to `<=`): at equality the floor is loaded over the
  same value. Retail is `cmp ax,[edi+0xb00]` / `jge` / `mov ax,[edi+0xb00]`
  (`.text+0x5daa`), and the flags are dead: the store, then `shr cx,1`. The
  A–E notes called it a reachability gap, which it is not.

Two findings beyond the handlers, both reproduced:
- **JOYSTICK_CALIBRATE's `+0x44` leaves `ecx` 0**, because `controls_joystick`
  ends in `axis_reset`'s `rep stosd`; DEMO reads the `ecx` `+0x44` leaves
  (docs/modules/demo.md), so `axis_reset` sets `ecx = 0`.
- **the pilot-event words to DEMO carry retail stack garbage in their top
  halves**; DEMO reads only the words, and OpenFS98 zero-extends (so the
  harness compares the words).

## What is left

Nothing in the module. What the verification does not reach:

- **`+0x1c4` failing.** The harness's sticks always answer. The host returns
  -1 when the joystick subsystem is absent, the index is out of range or
  `joyGetPosEx` fails (that last after narrowing the mask); CONTROLS then
  skips `joy_map` for that stick (`joy_poll`), or keeps the config's mask
  (`joy_current`, which nothing reads).
- **Options.DLL itself.** The dialog's callbacks are verified against a
  stand-in built from its listing, not the real MFC pages, and the dialog
  has not been opened in-game.
- **Hands-on input.** `make smoke` boots to the panel with CONTROLS
  installed, but no key, stick or force-feedback device was driven in-game.
