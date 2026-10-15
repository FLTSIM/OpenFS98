# SOUND.DLL — module id 39

**Reimplemented and verified.** SOUND decides what the sim sounds like:
the engine, as a chain of linked samples whose rate and volume follow rpm
along curves from the aircraft's `SOUND.CFG`; gear, flaps, stall, crash,
splash, wind and touchdowns; the warning horns; the three marker beacons;
and a Morse generator that builds a navaid's ident out of dit, dah and gap
samples. It plays nothing itself. The host's voice engine (`+0x2cc`..`+0x2f4`,
DirectSound underneath) does that, and SOUND tells it which sample, how loud,
how fast and where.

`src/sound/sound.c` is the transliteration and the module shell together.
`make sounddiff-run` drives retail and the OpenFS98 build side by side over
random scripts -- **20,000 scenarios, 0 differ** -- comparing SOUND's whole `.data` image, the
heap, GLOBAL and the sim's `FLTSIM98.CFG` as well as every call it makes.
`make smoke` passes with SOUND installed beside ACONTAIN, ATC, CHAIN, GLOBAL,
NUMERIC, SLEW, UTIL and WEATHERM: 27/27 modules, 95 gauges. The sim starts
SOUND, loads the aircraft's sounds and runs its frames on the way to the
panel.

The reasons it came next are in [../preflight.md](../preflight.md), round 3.

## Binary and linkage

- Retail image base `0x208d0000`; `.text` 15,174 bytes of MSVC-compiled C,
  475 of them x87 instructions, with `_ftol`; `.data` at `0x208d6000`,
  0x177c bytes in memory of which the file holds 0x1400; 45 doubles in
  `.rdata`.
- Imports: host, GLOBAL, ACONTAIN, CHAIN, UTIL -- all of them done.
- Win32: `DisableThreadLibraryCalls`, `wsprintfA` (the fader key names) and
  MSVCRT `strtok` (the crash and splash file lists).

| slot | retail | who calls it | service |
| --- | --- | --- | --- |
| `+0x04` | `.text+0x3b10` | host | init: the whine scale, 400 / log10(9/7) |
| `+0x08` | `.text+0x3b30` | host | deinit: nothing |
| `+0x18` | `.text+0xad0` | MAIN | start: ask the host whether there is sound; register on CHAIN lists 0x14, 0x12 and 3 |
| `+0x1c` | `.text+0xea0` | MAIN | stop: free every sound and unregister |
| `+0x20` | `.text+0x1190` | MAIN | the frame: listener, wind, flaps, engines, markers, overspeed, stall |
| `+0x24` | `.text+0x2810` | MAIN ×10, CONTROLS ×4, DEMO, REALITY, ADVDRV | `event(code, arg)`, 29 codes |
| `+0x28` | `.text+0x470` | MAIN | the wind, from the flight model's airspeed |
| `+0x2c` | `.text+0x400` | -- | set one fader, 1..5 |
| `+0x30` | `.text+0x450` | ADVDRV, DEMO | read one |
| `+0x34` | `.text+0x370` | -- | set several: `{ on[6], value[6] }` |
| `+0x38` | `.text+0x3d0` | -- | read several |
| `+0x3c` | `.text+0x2b0` | MAIN | write the five to `FLTSIM98.CFG` `[PREFS]`, then reread |
| `+0x40` | `.text+0x310` | MAIN | read them from there as doubles |
| `+0x44` | `.text+0x680` | PANELS ×5 | the marker-light bits |

`+0x18`, `+0x1c`, `+0x20` and `+0x44` are plain `ret`; the rest stdcall.
No caller reads what `+0x18`, `+0x1c`, `+0x20`, `+0x28` or `+0x2c` leave in
EAX. Those returns are reproduced where they are defined (`+0x18`, `+0x1c`)
and not compared by the harness.

## The sounds

Each sound is a 0x44-byte record in a host growable array at `0x208d7760`
(`struct snd_rec` in the source):

| at | what |
| --- | --- |
| `+0x00` | id, what `0x208d1040` looks up |
| `+0x04` | the id it was linked under (an engine's chain) |
| `+0x08` | its fader group, 1..5 |
| `+0x0c` | an engine's own array of linked sub-sounds |
| `+0x10` | a parameter block whose `+0x4` is the function that frees it |
| `+0x14` | the file, without `.WAV` |
| `+0x18` | the host's load type: 2, 4 or 5 |
| `+0x1c` | the voice handle |
| `+0x20` | playing (looped) |
| `+0x24` | a marker held while sound is off |
| `+0x28` / `+0x2c` / `+0x30` | `initial_volume`, `minimum_volume`, `maximum_volume`, 0..10000 |
| `+0x34` | the group's fader: 0 mutes, and most "is it enabled" tests are this |
| `+0x38` | `panning`, -10000..10000 |
| `+0x3c` | `viewpoint`: 1 heard only in cockpit views, 2 only outside |
| `+0x40` | `flags`: bit 0 disabled; 2, 4 and 8 a jet sub-sound's role |

The 29 templates at `0x208d6378` are `{ section name, loader, record }`,
stepping 0x4c. Message 0x60 on CHAIN list 0x14 -- a new aircraft -- frees
everything, asks ACONTAIN (`+0x30`, keys 8 and 7) where the aircraft's
`SOUND.CFG` and sound folder are, and builds the array from the templates,
each loader reading its `[section]`:

- **`0x208d2600`, most of them:** `flags`, `filename` (looked for in the
  aircraft's sound folder, then in the sim's -- string 0x9005, `SOUND` --
  and the sound disabled if neither has it), the three volumes, `panning`
  and `viewpoint`, clamped.
- **`0x208d2670`, crash and splash:** the same, plus `extra=`, up to eight
  alternative files, one picked at random each crash.
- **`0x208d2750`, wind:** plus `minimum_rate`/`maximum_rate` and
  `minimum_speed`/`maximum_speed`, and three derived values: volume per decade
  of speed, log10 of the minimum speed, and rate per unit of speed.
- **`0x208d2ab0`, the two engines:** only the one matching the view kind
  (GLOBAL `+0x609`, overridable by the section's `type=`) survives: kind 0
  keeps `SOUND_ENGINE`, 1 and 3 keep `JET_ENGINE`, 2 neither. It follows
  `link=` from its own section through at most 16 sections, each a sub-sound
  in the engine's own array. A prop sub-sound also gets `rparams` (two points
  of rate against rpm) and `vparams` (eight points of volume). The volume
  points must rise; from the first that does not, the last good point is
  repeated to the end.

The ids `0x21` and `0x22` have no template. `snd_find` hands back a shared
default record at `0x208d7230` for any id it cannot find, set up on first
use, with no file -- so asking it to play goes to the host with a null
file name.

The static engine sub-sounds in `.data` from `0x208d6078`, and the code at
`.text+0x980` that installs them when an engine has no chain, are
unreachable: the engine loader always either disables the engine or gives
it a (possibly empty) array. Kept, as retail has it.

## Every frame

`+0x20`, when GLOBAL `+0xb24` (sound on) is 1:

- **The listener.** The aircraft's position (GLOBAL `+0x560`, three 48-bit
  values) against the eyepoint -- the tower view's, from the view record at
  GLOBAL `+0x11a0`, when the view flags at `+0x119c` say so -- gives the
  squared range, the range in 16.16 at `0x208d7608`, its rate of change
  averaged over four frames (for Doppler) and a bearing. Longitude goes
  through UTIL `+0x48`.
- **Wind** is armed when the view is a cockpit and nothing is crashing.
- **Flaps**: GLOBAL `+0xbe0`/`+0xbe4` changing starts the start, run and
  stop samples, the run's volume shaped by an 18-entry table over the first
  frames. It stops when *either* word matches last frame's, not both.
- **Engines**: a prop's sub-sounds take their rate and volume from the
  curves at rpm × manifold (GLOBAL `+0x898`.., `+0x8c8`); a jet's from rpm
  alone, with a whine that builds while any engine's word at `+0x88c`.. is
  negative. Outside the cockpit, Doppler and range attenuation apply.
- **Markers** held while paused are replayed; overspeed and stall stop when
  their conditions lapse.

CHAIN list 3 repeats each ident every 15 frames (30 for the middle two) at
fader 2's volume.

## Events: `+0x24`

| code | what |
| --- | --- |
| 1 | crash over |
| 3 | overspeed horn on/off (`arg`) |
| 5 | gear-up warning on/off |
| 7 | glideslope |
| 8 | stall horn on/off |
| 9 | autopilot disconnect |
| 0xe | crash or splash: `arg` points at `{ flags, bits, -, callback, splash }` |
| 0xf / 0x10 / 0x11 | outer / middle / inner marker |
| 0x12 | markers off |
| 0x13 | touchdown: `arg` < 0 left, 0 centre, > 0 right |
| 0x14 / 0x15 | gear up / down |
| 0x17 | an ident: `arg` points at `{ slot 0..4, word, text }` |
| 0x18..0x1a | low, mid, high tone |
| 0x1b | the 0x21 horn on (1) or off |
| 0x1d | the 0x22 sound at a range |
| 0x16, 0x1c | nothing, 0 |
| others | 2 |

Code 0xe returns whatever was in EAX: usually the address of SOUND's own
copy of the caller's block, on its stack. REALITY tests it against zero.
The reimplementation returns its copy's address too.

## The voice engine and the arrays

The host services SOUND brought into `include/fs98/host.h`, each read from
the callee:

- `+0x050` the busy cursor; `+0x0b8`/`+0x0bc` a double from or to the sim's
  own `FLTSIM98.CFG` (`strtod`, `sprintf("%f")`); `+0x0d0`, `+0x0e0` and
  `+0x0e8` an int, a double and n doubles from a resolved file; `+0x2b4`
  strdup.
- The voice engine: `+0x2cc` is there sound, `+0x2d0` all on or off,
  `+0x2d4` load (types 2, 4, 5 from a file, 6 and 7 a copy), `+0x2d8` append
  one voice's samples to another and finish it, `+0x2dc` release
  (reference-counted), `+0x2e0` play with a completion callback, `+0x2e4`
  rate, `+0x2ec` volume, `+0x2f0` pan, `+0x2f4` stop. A handle is the
  address of a 0x16c-byte host block, checked against the host's list.
  0x90003 from `+0x2d4` means the buffer could not be made but, with no
  device, the voice was kept -- SOUND treats it as loaded.
- The growable arrays: `+0x2fc` free the data, `+0x308` walk until a
  callback returns non-zero, `+0x310` find by bytes at an offset. Removals
  during a walk are queued on a host-wide list and applied when the walk
  ends; SOUND's teardown relies on that, removing every record from inside
  its own walk.

## The floating point

Retail keeps chains on the x87 stack at 80 bits and rounds only where it
stores. The C does the same: `long double` for what stays on the stack,
`double` where retail stores, and the comparisons spelled as the status bits
retail tests (`FC0`, `FC3`) so that NaN goes the way it does. Three steps
are inline x87 because a C library would not round them the same:

- log10 as `fldlg2; fyl2x`;
- the other log10, at `0x208d2dc0`, as `fld1; fldl2t; fdivp; fyl2x`, which
  rounds 1/log2(10) to the precision control first;
- `fpatan` for the bearing, and `fsqrt`.

The constants are generated from `.rdata` as hex-float literals. `_ftol`
truncates to 64 bits and callers keep 32 or 16, as in UTIL. Everything runs
under whatever precision control the process has, the same for both
builds; the harness runs each scenario under 24-, 53- or 64-bit precision.
At 24 bits UTIL's frand comes back 0 every time -- `2531011 + x·214013`
has no fraction bits left -- and the ident timers start in step.

## Retail behaviour that is reproduced

- **Code 0x1b, horn off, stops a voice handle as if it were a record.**
  `snd_stop` reads the handle's `+0x1c` and stops that. With no voice loaded
  it reads address 0x1c and faults. The default record never has a voice, so
  retail faults every time this runs in the harness, and so does the
  reimplementation, at the same point.
- **The marker-light toggle (CHAIN list 0x12) checks its index only
  against 5, signed.** Index 5 writes past the five ident slots; a negative
  one writes below them, into the listener position. The bit shift takes
  its count mod 32.
- **Flaps stop when either flap word is unchanged.**
- **Ids 0x21 and 0x22 share the default record**, which has no file.
- `+0x34`'s clamp to 10000 is unsigned, so a negative fader becomes 10000.
- The vparams check reads one point past the block when all eight rise.
  Nothing comes of it; the reimplementation does not read it.

## Limits the harness keeps inside

- `+0x1c` only after `+0x18`. Retail CHAIN faults removing from a list
  nothing was ever added to, and MAIN always starts SOUND first.
- A null file name reaches the host from the default record. The real host
  would `strcpy` from null; the stub fails the load instead.
- The voice engine is a model: buffers that cannot be created and plays
  that fail are chosen by hash, and a sample "ends" only when the script
  says so. SOUND's view of it -- handles, return codes, when a callback
  runs -- is the host's.

## Verification

`make sounddiff-run` (`tools/difftest/sounddiff.c`): both builds loaded into
one process and driven through Linkage and the three CHAIN lists over
random scripts -- start, stop, frames, events, wind, the fader services,
CHAIN messages and marker lights, ident ticks, samples ending -- with
GLOBAL poked between calls. Retail CHAIN and UTIL are loaded for real;
ACONTAIN and the flight model are stubs; the host is a stub with the voice
engine modelled. Each scenario runs under a random x87 precision control,
24, 53 or 64 bits.

Compared after each scenario: the transcript of every import call with its
arguments and results (doubles bit for bit), GLOBAL, the `FLTSIM98.CFG`
`+0x3c` writes, every byte of the heap, and SOUND's whole `.data` image. The
image and the heap hold pointers into the module: a pointer into `.data`
is compared as an offset, and a pointer to code through a pairing of
retail's functions with ours, learnt as each is first seen and held to
after.

**20,000 scenarios, 0 differ** (about 40 minutes; `N=2000` for a quick
check).

### Coverage

Random scripts against nine fixed trees first left the engine curves, the
wind's computed volume and the ident timers nearly unvisited. The generator
now rewrites a generated tree for each scenario and steers onto the paths
the breaks below showed it missing (the list is in the harness's header):
the view kind matched to the tree's engine `type=`, engine words exactly on
curve points and about the 200 cut-off, the wind climbing through its curve
with its volumes reversed, eyepoints that make the range a single
coordinate difference, and many ident ticks at once.

From the 20,000: 151,499 events, every code from 0 to 0x1f; 387,904
sounds built from their templates; 65,947 voices loaded and 38,180 plays,
20,086 Morse appends finished into 1,627 idents; 54,176 frames that walked
an engine's sub-sounds, 2,798 of them in the steered 400 m case; the wind's
computed volume 2,332 times; 600 crash callbacks; and 2,999 retail faults
in the horn, each matched by ours.

### Deliberate breaks

56 breaks of `sound.c`, each built into its own DLL (checked to differ from
the good build) and run against the final harness at up to 5,000
scenarios: **54 caught, 2 proven equivalent.** They cover clamps and
boundaries (minimum volume, the fader and pan limits, the 0x10-link limit,
the 200 cut-off, the Doppler clamp, the whine's ceiling), the floating-point
order (the line through two points rewritten point-slope, log10 taken the
other way, a square root of the rounded sum), every reproduced retail bug
(the horn's handle-as-record, the unchecked marker-light index, flaps
stopping on either word), the error paths (the Morse loads, a disabled link
kept), the host arrays (growth step, deferred removal) and the decoding of
`+0x24`'s blocks. Most are caught within a hundred scenarios; the slowest
are the wind's volume clamp (1,193), the jet stack's carried rate (1,167)
and the ident reload (967).

Two are proven equivalent rather than caught:

- **Comparing the latitude difference after rounding it to double.** Both
  positions come from 48-bit fixed point (`snd_pos48`), so their difference
  is a multiple of 2^-16 under 2^33 and is exactly representable as a
  double: rounding it changes nothing.
- **The Morse volume minimum compared signed.** Every volume it compares has
  been clamped to 0..10000 when its section was read, and the running
  minimum starts at 10000, so signed and unsigned agree.

Eleven were missed by the first, unsteered harness and are caught now. Each
pointed at a gap, and each steering above is there because of one: the
wind clamp and the unsigned volume range (reversed volumes, repeated
retunes), the sixteen-link limit (fresh trees with link cycles), the square
root of the stored sum (a single-axis range, 64-bit precision), the Morse
error path (fresh trees missing samples), the inclusive curve bound and the
200 cut-off (exact engine words), the jet stack's carried rate (two flag-8
sub-sounds), the ident reload (many ticks), and the second log10 (the 400 m
eyepoint under 24-bit precision -- found by working the arithmetic through
by hand, since at exactly 400 m both ways truncate to the same integer).

`make install` and `make smoke`: PASS, 27/27 modules, 95 gauges, with all nine
OpenFS98 modules installed.
