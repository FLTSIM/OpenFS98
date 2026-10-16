# Preflight

Every module left imports at least one that is not done. A preflight is the
survey that picks the next one: for each candidate, what it calls in modules
that are not done, and how faithfully those calls can be stubbed in an
external harness.

Produced with `python3 tools/impscan.py orig/Modules/<M>.DLL`, then each call
site and each callee read by hand. The scanner under-reports rather than
over-reports (see its header); the call counts below are lower bounds.

Three rounds so far, newest first.

# Round 3: SOUND, DYNAMIC, BOOSTER

## Verdict

**SOUND next**, then DYNAMIC. BOOSTER last, and probably not externally.

**Outcome:** SOUND is done -- [modules/sound.md](modules/sound.md). The
voice engine was the right boundary: a model of it in the stub host carries
everything SOUND does. Corrections from the full read: the host's profile
services are `+0x0b8`/`+0x0bc` (`+0x0b4` is UTIL's, not the host's); `+0x18`
starts SOUND, `+0x1c` stops it, `+0x20` is the frame, and the aircraft's
sounds are loaded on CHAIN list 0x14 message 0x60, not by a Linkage slot;
`+0x2d8` appends one voice to another (the Morse idents), and `+0x2e0` takes
a completion callback. SOUND also calls the loaded flight model's `+0x38`
(through GLOBAL `+0x11d4`), which the import scan cannot see, and UTIL's
frand and rand32 -- state a harness has to seed. ACONTAIN was stubbed after
all: two path queries.

**Outcome:** DYNAMIC is done too -- [modules/dynamic.md](modules/dynamic.md).
The plan held: real blocks behind an FE stub, mutated. The blocks are in the
twelve city `*5.bgl` files, found through BGL header `+0x52` (FE's cache entry
`+0x150` is the header from `+2`), and `dynlib.bgl` holds the library
objects they draw. What the survey did not see: three opcodes (`0x3e`,
`0x40`, `0x41`) write programs of their own at run time -- a fuel truck's
drive, a take-off and circuit, a turn onto a heading; `0x3f` refuels the
user's aircraft, so GLOBAL's tanks (`+0xb74`..`+0xba0`) and `+0x348` are
written too; and `+0x18` depends on ecx at entry. Unchecked programs spin as
well as fault, so the harness needed a watchdog.

| | SOUND | DYNAMIC | BOOSTER |
| --- | --- | --- | --- |
| `.text` | 15,174 | 12,300 | 8,024 |
| functions in Linkage | 14 | 52, most of them bytecode handlers | 4 |
| written in | C, with x87 (475 instructions) and `_ftol` | asm, register-passing, carry-flag returns | C |
| unfinished imports | **none** -- GLOBAL, CHAIN, UTIL, ACONTAIN | FE ×2, G3D ×1, MULTIP ×1 | FE ×2 |
| host services | 33: profile I/O, the voice engine, growable arrays | 3: alloc, realloc, free | 19, file and path |
| direct Win32 | `wsprintfA`, `strtok` | none | threads, mutexes, events, `CopyFileA`, `MessageBoxA` |
| external harness | yes, voice engine logged at the host boundary | yes, with real scenery blobs behind an FE stub | no -- a worker thread |

SOUND is the first module left whose module imports are all done, and its one
large boundary -- the host's voice engine -- is a set of narrow services a stub
host can log. The risk is the floating point, which UTIL has already shown how
to match.

Also surveyed and set aside: ANALYSIS (draws through HG2D, runs dialogs --
`ofstest`), MPCHAT (windowing on top of MULTIP), FACIL (23 KB, file-heavy,
imports FE and MAIN) and DEMO (lessons; MAIN, CONTROLS, WINMSG and SOUND).

## SOUND (id 39)

Engine, wind, touchdown, gear, flap, stall, marker and warning sounds, and a
Morse generator for navaid idents (the dot-dash table is in `.data` from
`0x208d713c`). Configured per aircraft from `sound\SOUND.CFG` -- sections
named `JET_ENGINE`, `GEAR_DOWN` and so on in `.data` from `0x208d6c6c`, keys
`type`, `link`, `flags`, `filename`, `viewpoint`, `panning`,
`minimum_`/`maximum_`/`initial_volume`, `minimum_`/`maximum_rate`,
`minimum_`/`maximum_speed`, `rparams`, `vparams`, `extra` -- with
`SOUND_FADER%d` under `PREFS`.

Callers: MAIN uses `+0x18`, `+0x1c`, `+0x20`, `+0x3c`, `+0x40` once each and
`+0x24` ten times; PANELS `+0x44` ×5; CONTROLS, ADVDRV, DEMO and REALITY
`+0x24`; ADVDRV and DEMO `+0x30`. By the services each slot reaches, `+0x20`
loads the aircraft's sound set, `+0x24` starts or updates a sound, `+0x18` and
`+0x1c` start and stop the engine group, and `+0x44` is PANELS' gauge sounds.
All of that is to be confirmed by the full read.

**Module imports, all done:** CHAIN `+0x18`/`+0x1c`/`+0x20`; UTIL `+0x48`,
`+0x70`, `+0xb4`, `+0xb8`; ACONTAIN `+0x30` (the aircraft query, ×2). These
can be retail's real modules in the harness. ACONTAIN would need the fixture
trees its own harness built, so a small stub answering `+0x30` from the
fixture is the cheaper choice.

**Host services, 33**, each prototype to be read from the callee:

- **The voice engine, `+0x2cc`..`+0x2f4`.** Host state at `0x41a3cc`, set up by
  `+0x2d0` (1,824 bytes: `waveOutGetNumDevs`, `GetPrivateProfileIntA`, then
  `LoadLibraryA` and `GetProcAddress` -- DirectSound). The rest call through a
  DirectSound buffer's vtable, which names them: `+0x2e0` (`ret 0x14`) plays,
  `+0x2e4` (`ret 0xc`) sets the frequency (`+0x44`), `+0x2ec` (`ret 8`) the
  volume (`+0x3c`), `+0x2f0` (`ret 8`) the pan (`+0x40`), `+0x2f4` stops
  (`+0x48`), `+0x2e8` reads the status (`+0x24`). `+0x2d4` (`ret 0xc`) and
  `+0x2d8` (`ret 0x10`) load a sound and `+0x2dc` (`ret 4`) frees one.
  `+0x2cc` returns whether the engine is off. A stub host records every call
  with its arguments and hands back handles and statuses from the scenario.
  That boundary is where SOUND's whole observable behaviour lands, so logging
  it is a full comparison, not a fake.
- **Growable arrays:** `+0x2f8`/`+0x300` (documented in
  [modules/host.md](modules/host.md)), `+0x2fc` (`ret 4`, frees one) and three
  not yet read: `+0x308`/`+0x30c` (`ret 0xc`, walk calling a function) and
  `+0x310` (`ret 0x14`). Portable host logic -- transliterate into the stub.
- **Profile I/O:** `+0x0b4`/`+0x0b8`/`+0x0bc` get and *write*
  `FLTSIM98.CFG` values (`+0x0b8` parses with `strtod`, `+0x0bc` writes with
  `sprintf` and `WritePrivateProfileStringA`); `+0x0d0` int, `+0x0e0` float and
  `+0x0e8` (`ret 0x18`) read from a resolved file; `+0x0f0` and `+0x158` as
  ACONTAIN. Passed through to fixture files, as `acontaindiff` does, and the
  written `FLTSIM98.CFG` compared after each scenario.
- `+0x050` is the busy cursor (`SetCursor`/`ShowCursor`) -- record it.
  The rest (`+0x184`/`+0x188` path, `+0x20c`/`+0x214`/`+0x218`/`+0x21c` memory,
  `+0x2a0`/`+0x2a8`/`+0x2b4`/`+0x2bc` strings) are in `host.h` or stubbed by an
  earlier harness already.

GLOBAL reads are the flight state (`+0x2a8`, `+0x365`, `+0x609`, `+0xbe0`,
`+0x11a0` and so on); writes are few (`+0xb20`, `+0xb24`, `+0xb6a`).
`+0x11d4` is the `.SIM` Linkage pointer ACONTAIN fills.

**Fixtures:** the seven aircraft's real `SOUND.CFG` files are in
`cd/fs98/Aircraft/*/sound/`, plus generated variants for the keys they do not
use.

**Risk:** the x87. Retail is MSVC with `_ftol`; the rebuild has to keep the
same operation order and the same spills to 64-bit memory, or volumes and
frequencies come out one unit apart. UTIL faced the same and `make
utildiff-run` holds it; follow its pattern.

## DYNAMIC (id 6)

Dynamic scenery: a bytecode interpreter. Each object is a `0x120`-byte record
in the array at `.data+0x1f0` (count first), its program pointer at `+0x68`;
`.text+0x168a` steps it, fetching one opcode byte and dispatching through the
table at `.data+0x60` -- which is **the Linkage itself from `+0x20`**.
Opcodes run `0`..`0x43`; those above `0x23` run only while the flag at
`.data+0x244` is set, which the stepper sets on entry and a handler clears. That is why
`dumplinkage.py` reports 76 entries and 52 functions: most of the "slots" are
opcode handlers, and the many 2-byte ones are `stc; ret`. Only `+0x18` has an
outside caller (MAIN, once).

`+0x18` is the per-frame entry. When GLOBAL `+0x1180`..`+0x118c` (the dynamic
scenery switches) are all clear it tears everything down; otherwise, on its
first run, it registers callbacks on CHAIN lists 16, 4 and 3, allocates the
`0x48`-per-entry player list at `.data+0x1e8`, and then steps every object a
number of times scaled from GLOBAL `+0xc1a` and `+0x2aa`.

Unfinished imports:

- **FE `+0x54`** -- `(GLOBAL+0x560, 5000, GLOBAL+0x3e4)`, the same search
  around the aircraft that LANDME calls with 100000.
  Called when the aircraft leaves a box it keeps at `.data+0x214`..`+0x220`.
  Stub records it.
- **FE `+0x40`** -- `(0, buf, size, &size)`, called twice: once with a 1-byte
  buffer to learn the size, then into a buffer of that size, which becomes the
  program block at `.data+0x1ec`. FE builds it from scenery files near the
  aircraft (a state machine reading a header's `+0x150`). **The format is
  DYNAMIC's own** -- it is read nowhere else -- so unlike LANDME's records, the
  harness does not need FE understood: extract the dynamic-scenery blobs from
  the CD's BGL files and hand them back. Finding which BGLs carry them is the
  first job of the full read.
- **G3D `+0x38`** -- `(object+0xf0, 0)`, the draw ACONTAIN also calls, from the
  CHAIN list 16 callback for each visible object. Stub records it.
- **MULTIP `+0x24`** -- no arguments; returns `{count, records}` with
  `0x100`-byte records, of which DYNAMIC reads `+0x04` (id), `+0x08`
  (`== 1`), `+0x0c` (`== 0`), `+0x28` (a position), `+0x30`..`+0x38` and
  `+0xcc`. Called from the CHAIN list 3 callback. Stub generates players.

Done imports: CHAIN `+0x18`/`+0x1c`; UTIL `+0x18`..`+0x48` (the position and
trig helpers), all real in the harness. Host services are only `+0x20c`,
`+0x210` and `+0x214`. State is GLOBAL plus `.data` (0x250 bytes) plus the
three heap blocks, all comparable.

Second because the blobs have to be found and the interpreter's 68 opcodes
need scenarios that reach them. Random programs would mostly fault on their
first jump; real blobs, mutated a little, are the better start.

## BOOSTER (id 34)

By its imports, the scenery cache: a worker thread (`CreateThread`, a message
loop on `GetMessageA`/`PeekMessageA`, mutex, events, critical section) that
copies files with `CopyFileA`, checks them with `GetFileSize` and
`GetFileAttributesA`, clears read-only bits and deletes them, and can put up
its own `MessageBoxA`. Only FE calls it (`+0x18`, `+0x1c`), and it calls FE
`+0x70`/`+0x74`.

The work happens on its own thread, so a transcript depends on scheduling. An
external harness would have to compare end states after the thread drains,
not call sequences. Leave it until FE is under way, and look at it again
then.

## Things found on the way

- **`dumplinkage.py` under-reports FE.** It stops at FE's Linkage `+0x10`,
  which holds `0xd0000000`, and reports 3 entries; the table runs on past
  `+0x7c`. Hand-count FE's table before relying on the tool for it.
- **The host's `+0x2cc`..`+0x2f4` are the sound engine.** SOUND makes most
  of the calls; ADVDRV (×7) and DEMO (×4) call it directly too, so they are the
  other clients a native host's audio has to serve.

# Round 2: ACONTAIN, REALITY, WEATHERM

## Verdict

**WEATHERM next**, then ACONTAIN. REALITY last, through `ofstest`.

**Outcome:** ACONTAIN is done too -- [modules/acontain.md](modules/acontain.md).
The fake loaded modules worked as planned: the `.SIM` and `.MDL` files are
ordinary module DLLs (ids 25 and 31), and a fake `.MDL` only has to supply a
header through its `+0x18`. Corrections from the full read: `+0x26c` takes
`(out, path, ext)` and ACONTAIN checks `*out`, not the return value; the
loaded module's `+0x18` takes the aircraft record and `+0x1c` the model state,
and the `0x24` check is on the header that `+0x18` leaves behind. G3D `+0x38`
is the draw and `+0x58` a flush before a model is unloaded; PANELS `+0x18` builds the panel
and calls back into ACONTAIN `+0x30`.

**Outcome:** WEATHERM is done -- [modules/weatherm.md](modules/weatherm.md).
The two stubs held up as described. Corrections from the full read:
`+0x24` resets the global and area blocks whose flags ask for it; `+0x20`
is the time-of-day temperature swing, called from the update itself; and
retail faults on an even temperature band, including its own reset defaults.

| | WEATHERM | ACONTAIN | REALITY |
| --- | --- | --- | --- |
| `.text` | 8,774 | 5,504 | 4,822 |
| functions in Linkage | 20 | 12 | 4 |
| written in | asm to `.text+0x2a3f`, C after | C | asm, with x87 and `_ftol` |
| unfinished imports | MAIN ×1, G3D ×1 | G3D ×2, PANELS ×2, and modules it loads itself | MAIN ×3, HG2D ×3, DEMO, WINDOW, CONTROLS, WINMSG, SOUND |
| host services | 2: alloc, free | 26, 12 new -- mostly file I/O | 12, 6 new -- windowing, GDI, dialogs |
| direct Win32 | none | `wsprintfA` | none |
| external harness | yes, two logging stubs | yes, with fake loaded modules | no -- it draws and runs dialogs |

WEATHERM is the largest of the three but the cheapest to test: apart from the
two calls below, everything it touches is GLOBAL, CHAIN, UTIL and the host
heap, all of which a harness already owns or loads for real.

## WEATHERM (id 27)

Weather: winds, temperatures, cloud and visibility state in GLOBAL
`+0xe6a`..`+0x116a`, recomputed each frame. Two halves:

**Hand-written asm, `.text+0x020`..`+0x2a3f`.** The same alignment macro and
register-passing internal calls as ATC. No floating point: the two "x87"
instructions objdump shows (`+0x2e48`, `+0x3110`) are switch tables in
`.text`.

| slot | who calls it | what it does |
| --- | --- | --- |
| `+0x04` init | host | registers `.text+0x28c9` on CHAIN list 3; points `.data+0xe6`.. at three GLOBAL weather records (`+0xf34`, `+0x101c`, `+0x1104`) |
| `+0x08` deinit | host | unregisters it |
| `+0x18` | ATC | weather at a given position: the update run with `.data+0xf6` pointed at the caller's record, then back at GLOBAL `+0x560` |
| `+0x1c` | MAIN | the update at the aircraft, then MAIN `+0x94` |
| `+0x20` | nobody outside; `.text+0x1c3c` | per-layer helper. Pushes a 16-bit `bx`, so the stack is 2 bytes off until `leave` |
| `+0x24` | MAIN ×2, multip | when GLOBAL `+0xf12` is set, sets `+0xf20`/`+0xf24`/`+0xf26`, then goes on; MAIN calls it after a weather change. Not read further |
| `+0x28` | G3D | reads a stream of 8-byte records -- type byte at `+1`, types 1-4 -- by its caller and shape, weather records out of scenery |

The CHAIN callback at `.text+0x28c9` hands GLOBAL `+0x1164` (a pointer) to
G3D `+0x20` if set, clears it, and steps the three layers through
`.text+0x1c51`. That helper indexes its three-word arrays at
`.data+0xc8`/`+0xce`/`+0xd4`/`+0xda` by the layer number unscaled, so layers
1 and 2 read and write overlapping words -- a probable retail bug to
reproduce, not fix.

**Compiled C, `.text+0x2a40` on, slots `+0x2c`..`+0x5c`.** A container of
`0xc0`-byte records, in linked lists. It calls only host `+0x20c`/`+0x214` and
touches neither GLOBAL nor WEATHERM's `.data`, so a harness can call it
directly with an arena allocator. **No caller was found** for these 13 slots
or for `+0x20`. No module's ImportTable reaches them, and the `English\`
dialog modules import only the host and GLOBAL. Every static call to host
`+0x298` (acquire a module's Linkage) passes the caller's own Linkage --
MAIN, multip, mpchat and all the `English\` modules -- except one in PANELS,
which passes a runtime value. A short in-game trace would settle whether the
C half is live; it does not block the harness.

Unfinished imports, and the stubs they need:

- **MAIN `+0x94`** -- no arguments. Reads the cloud fields and writes GLOBAL
  `+0xcd4`, which WEATHERM never reads. Stub records the call.
- **G3D `+0x20`** -- one pointer, from GLOBAL `+0x1164`; 51 bytes that save
  it and run a G3D-internal routine. Nothing comes back. Stub records the
  pointer.

The done imports are CHAIN `+0x18`/`+0x1c` and UTIL `+0x24` mul48, `+0x2c`
sincos, `+0x34` rotate2d, `+0x3c` atan2, `+0x44`/`+0x48` the
metres/longitude pair and `+0x6c` rand -- all of which can be retail's real
modules in the harness. UTIL's rand seed (`.data+0x0f0`) is shared state, so
it has to be reset to the same value before each side runs.

State is GLOBAL plus WEATHERM's `.data` (0x1c5 bytes at `0x208f5000`), so the
ATC pattern applies: compare the transcript, GLOBAL and the whole `.data`
image.

## ACONTAIN (id 23)

By its calls, the aircraft container. It reads profile strings and files and
loads modules by path through the host. MAIN calls nine of its slots, and
CONTROLS, PANELS, SOUND, WINDOW and multip call `+0x30`, so most of the sim
depends on it.

- **Host services:** 26 of them. The 12 not yet in `host.h` are mostly thin
  file wrappers:
  - `+0x0f0`: profile string, after resolving the path.
  - `+0x0c8`: raw `GetPrivateProfileStringA`.
  - `+0x158`: file exists, via `GetFileAttributesA`.
  - `+0x17c` open, `+0x13c` close, `+0x194` seek, `+0x19c` read.
  - `+0x218` memset, `+0x2ac` strncpy, `+0x2bc` strcmp.
  - `+0x26c`/`+0x270`: load and free a module through the host's loader.
- **Modules it loads itself:** `+0x26c` takes `(out, path, ext_id)` with ext
  ids `0x1b` and `0x14`. ACONTAIN then calls the loaded module's Linkage
  `+0x18`, `+0x1c` and `+0x20`, and checks the first dword of what comes back
  against `0x24`. A harness needs fake modules with those entry points.
- **UTIL `+0xa4`:** the modal alert, with strings `0xf540`-`0xf544`, which
  look like load errors.
- **G3D `+0x38`/`+0x58` and PANELS `+0x18`/`+0x1c`:** not yet read.

Testable externally, with the file services passed through to real fixture
files, but it needs more stubs than WEATHERM and fakes for the modules it
loads.

## REALITY (id 32)

Asm with x87 throughout (56 instructions, plus MSVCRT `_ftol`). It writes a
spread of GLOBAL flags and reaches nine unfinished modules. Its host calls
are where it does its work:

- `+0x074`/`+0x078`: GDI, including `DeleteDC`/`DeleteObject`.
- `+0x084`: `SendMessageA`.
- `+0x03c`: `PostMessageA`.
- `+0x10c`: `run_dialog`.
- HG2D `+0x2c`/`+0x30`/`+0x34`.

Only WINDOW `+0x1c` was found calling it. Its observable behaviour is drawing
and dialogs, like WINMSG's, so it belongs in `ofstest`.

# Round 1: WINMSG, ATC, LANDME

## Verdict

**ATC next**, then LANDME. WINMSG is the smallest by `.text` but the hardest to
test outside the sim, and is better reached through `ofstest`.

**Outcome:** ATC is done -- [modules/atc.md](modules/atc.md). The stubs held up
as described; the WEATHERM and WINMSG readings below were confirmed by the
harness. Two corrections from the full read: ATC's translator (`.text+0xba0`
on) is compiled C rather than assembly, and `+0x20` reads GLOBAL through the
caller's EDI.

| | ATC | LANDME | WINMSG |
| --- | --- | --- | --- |
| `.text` | 3,840 | 4,094 | 4,416 |
| functions in Linkage | 7 | 3 | 6 |
| unfinished imports | WINMSG ×3, WEATHERM ×2 | MAIN ×1, FE ×2, WINMSG ×1 | MAIN ×3, WINDOW ×2 |
| host services | 5, all already prototyped | 4, 2 new but trivial | 14, 5 of them windowing |
| direct Win32 | `GetPrivateProfile*`, `sscanf` | none | `CharNextA` |
| external harness | yes, small stubs | yes, needs FE's record format | no -- it owns a window |

## ATC (id 24)

Air traffic control: reads its phrase/frequency configuration with
`GetPrivateProfileSectionA`/`StringA` and `sscanf`, works out weather for a
position, and puts text up through WINMSG.

Unfinished imports, and the stubs they need:

- **WINMSG `+0x18`** -- show a message (from reading WINMSG's side). ATC passes
  `(text, arg, 600 / (GLOBAL+0x362 + 1), 1)`. Stub records the call.
- **WINMSG `+0x1c`** -- clear message slot `n`; ATC clears 2, 1 and 3 in turn.
  Stub records the call.
- **WINMSG `+0x24`** -- query slot `n`; ATC ORs the results for slots 2 and 1.
  Stub returns a scripted value per vector.
- **WEATHERM `+0x18`** (44 bytes) -- by its shape, weather at a given position:
  stores the position pointer, runs the weather update, then points itself back
  at the aircraft (`GLOBAL+0x560`). ATC passes its own position record.
  The results land in GLOBAL, which the harness already owns, so the stub can
  write chosen weather fields and record the position.
- **WEATHERM `+0x1c`** -- the weather update at the aircraft's position, ending
  in G3D `+0x94`. Stub records the call.

Host services are `+0x184` path join, `+0x20c` alloc, `+0x214` free, `+0x2a0`
LoadString and `+0x2b8` strlen -- all in `include/fs98/host.h` already, and
`numericdiff` stubs two of them. The done imports are CHAIN `+0x18`/`+0x1c`
(register/unregister), NUMERIC `+0x18` (format) and UTIL `+0x60` (unpack
position), which can be retail's real modules in the harness.

The INI reads go to the real filesystem. Retail and rebuild will read the same
file, so the harness can point both at generated test INIs.

Who calls it: MAIN uses `+0x18`, `+0x1c`, `+0x20`; CONTROLS uses `+0x24`.
Nothing found calls `+0x28`, which is probably a scanner miss (G3D imports
ATC but shows no uses).

## LANDME (id 26)

"Land Me": registers a callback (`0x207d10af`, inside the one 3,968-byte
function behind `+0x18`) on CHAIN list 3 in `+0x04` and removes it in `+0x08`.
MAIN calls `+0x18`.

- **FE `+0x54`** -- `(GLOBAL+0x560, 100000, GLOBAL+0x3e4)`: by its arguments, a
  search around the aircraft's position within a radius, writing a result.
- **FE `+0x20`** -- returns a status and a pointer through `0x207d30cf`, after
  which LANDME reads the record (`cmp byte [esi], 7`). The stub has to hand back
  records in FE's format, which nobody has worked out yet.
- **MAIN `+0x70`** (1,933 bytes) -- `(0x207d303c, GLOBAL+0x560, GLOBAL+0x870,
  &local)`; purpose not yet read.
- **WINMSG `+0x18`** -- the same show-message call as ATC's.
- **Host `+0x03c`** -- new: `PostMessageA(main_hwnd, WM_COMMAND, a, b)`,
  `ret 8`, returns 0. LANDME posts command `0x100d7`. Stub records the call.
- **Host `+0x218`** -- new: `memset(dst, byte, n)`, stdcall, `ret 0xc`,
  returns 0. Trivial.

Testable externally, but only after FE's records are understood well enough to
generate them. That is what puts it second.

## WINMSG (id 22)

The on-screen message window -- the service the other two use to put text up.
It keeps message slots, creates and destroys a host window object, sends it
custom messages (`0x466`, `0x467`) and has the host paint it:

- Host `+0x064` creates the window object (allocates `0x90`, fills it in,
  `CreateWindowExA`), `+0x068` destroys it (`DestroyWindow`), `+0x07c` sends
  `0x467` then `SetWindowTextA`, `+0x084` sends `0x466` if the object's top
  flag bit is set, and `+0x08c` is a 1,168-byte service calling GDI32, KERNEL32
  and USER32.
- Host `+0x2b4` -- new: strdup, via `+0x2b8`, `+0x20c` and `lstrcpyA`.
- MAIN `+0x05c`/`+0x060`/`+0x064`, WINDOW `+0x078`/`+0x07c`.

Its observable behaviour is mostly what it does to a window, which needs a real
host. Faking that would be testing the fake. It belongs in `ofstest`, once ATC
and LANDME have shown what its callers expect of it.

## Things found on the way

- **ATC, LANDME and WEATHERM are hand-written assembly**, not compiled C
  (ATC's translator excepted, as it turned out). Each
  cross-module call is wrapped in the same stack-alignment macro:

  ```
  mov  dword [esp-7], 0x04050607   ; bytes 07 06 05 04 just below esp
  and  esp, ~3                     ; align down
  sub  esp, 4
  and  dword [esp], 7              ; picks up the byte that says how far it moved
  push ...args
  call [link+slot]                 ; callee is stdcall and pops its args
  add  esp, [esp]                  ; undo the alignment
  ```

  So the stack is 4-aligned at every call no matter how it arrived, and the
  callee must pop exactly its own arguments. There are `pusha`/`popa` pairs and
  hand-chosen registers throughout. Transliteration will read more like the
  assembly than CHAIN's C did; expect to keep register-level structure.
- **`make graph`'s `slots` column is an upper bound.** It walks zeros past the
  end of the Linkage into `.data`, so ATC shows 256 (its cap) and LANDME 113.
  The hand counts are 11 and 7 entries; `fns` (7 and 3) is right.
  `dumplinkage.py` stops at the right place.
