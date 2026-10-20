# Preflight

Every module left imports at least one that is not done. A preflight is the
survey that picks the next one: for each candidate, what it calls in modules
that are not done, and how faithfully those calls can be stubbed in an
external harness.

Produced with `python3 tools/impscan.py orig/Modules/<M>.DLL`, then each call
site and each callee read by hand. The scanner under-reports rather than
over-reports (see its header); the call counts below are lower bounds.

Nine rounds so far, newest first.

# Round 9: what is left

## Verdict

**CONTROLS next** (now done), then MULTIP with MPCHAT behind it, then G3D.
PANELS and MAIN wait, and HG2D stays last. G3D's open question, who supplies
the span routine its clipper calls, is settled: G3D does (see its section).

CONTROLS is the only remaining module whose calls into unfinished modules
are all fire-and-forget. What it needs from outside is input, and input can
be scripted: joystick positions, profile sections and a DirectInput that
lists the devices the scenario chooses. It is also the input layer the
native goal needs behind a small host abstraction, so a verified CONTROLS
pins that abstraction down.

| | MPCHAT | MULTIP | CONTROLS | PANELS | G3D | MAIN |
| --- | --- | --- | --- | --- | --- | --- |
| id | `0xffffffff` | 35 | 16 | 15 | 14 | 3 |
| `.text` | 3,104 | 43,318 | 48,406 | 50,038 | 92,518 | 119,648 |
| `.data` | 0x200 | 0xa00 | 0xc00 | 0x1e00 | 0x18450 | 0x8000 |
| functions in Linkage | 2, and `+0x14` is a pointer into `.data` | 11 | 20 | 39 | 22 | 47 |
| written in | C | MSVC C (`_ftol`) | asm to `.text+0x78d0`, MSVC C from there | MSVC C | asm, ATC's wrapper throughout | asm and C interleaved |
| unfinished imports | MULTIP ×4 | G3D ×3, MAIN ×1 | MAIN ×7, HG2D ×1, PANELS ×1 | HG2D ×36, MAIN ×10, gauges (id 19) | HG2D ×20 | every one but MPCHAT: G3D ×21, HG2D ×21, CONTROLS ×12, MULTIP ×8, PANELS ×5 |
| return values read from them | 3 of 4 | G3D `+0x64` | **none** | MAIN `+0x80`, HG2D `+0x48`/`+0xac`/`+0xb0`, and four HG2D fields read directly | HG2D `+0x68`, `+0xa8` | 9 slots |
| host services | 13, 4 new | 44, 8 new | 46, 18 new | 38, 9 new | 11, **none new** | 75, 18 new |
| direct Win32 | a window of its own (USER32, `RICHED32.DLL` loaded) | threads, events, critical sections, `ShellExecuteA`; **DirectPlay through COM** | `GetKeyboardLayout`; **DirectInput 5 through COM** | GDI: DIB sections, fonts, `DrawTextA`, `FillRect`; resources; `fopen` | `CharLowerA`, `_CIacos` | `LoadImageA`, `GetObjectA`, string and locale calls |
| callers | none; nothing can import id -1 | MAIN, DYNAMIC, MPCHAT | MAIN, WINDOW, DEMO, REALITY | MAIN, WINDOW, ACONTAIN, CONTROLS | MAIN, MULTIP, HG2D, and five done modules (WINDOW, ACONTAIN, ANALYSIS, DYNAMIC, WEATHERM) | eleven modules, ten of them done |
| external harness | yes, after MULTIP: a USER32 model | probably: a fake DirectPlay and BOOSTER's drain | **yes**: scripted devices and profiles | after HG2D, or with an HG2D model | probably: retail FE over real scenery, HG2D logged | last before HG2D |

"New" means no done module calls the service, so no harness models it yet.
45 such services in all. "Return values read" comes from a scan of the
instructions after each call site that counts a read of eax before eax is
written. It is a heuristic. "None" is the reliable direction; check each
read by hand before building a stub on it.

How these were produced, so they can be rerun: `make graph`,
`dumplinkage.py` and `impscan.py` on each module, `objdump -p` for the
Win32 imports, and the service table at host `0x417ba0` for the new host
services. Each Linkage was read against its raw words, and
`dumplinkage.py` agrees with every one.

## CONTROLS (id 16)

The keyboard and joystick: key and button mapping to the named commands
(`THROTTLE_INCR`, `GEAR_TOGGLE` and the rest: 426 upper-case names in the
binary, as strings), the mouse as yoke, joystick calibration, and force feedback
(`force_ground_bumps_enabled`, `force_crash_enabled`,
`force_gear_bump_enabled`).

- **Layout.** Hand-written asm with ATC's stack-alignment wrapper (79
  sites) runs to `.text+0x78d0`. The Linkage entries there are unaligned:
  `+0x18` is 12,756 bytes and `+0x44` 10,492. MSVC C follows, with entries
  `+0x4c`..`+0x5c` on 16-byte boundaries. The C half holds the profile, the
  joystick enumeration and the DirectInput force-feedback code.
- **The unfinished imports are all fire-and-forget**: MAIN `+0x84`, `+0x8c`,
  `+0x90` ×2, `+0xa4`, `+0xac`, `+0xc8`, HG2D `+0x64` and PANELS `+0x30`.
  None has eax read after it, so logging stubs are faithful for the value
  path. The registers they leave still have to be checked against what
  CONTROLS reads (method.md, "What a callee leaves in a register").
- **Commands go out through host `+0x03c`** (8 sites), which posts
  `WM_COMMAND` to the main window. The posted command and parameter are
  output, so a harness can log them.
- **New host services, read from their openings** (not yet read in full):
  - the profile, all through `GetPrivateProfileStringA` and its writers:
    `+0x0b0`, `+0x0b4` (`sprintf` and `WritePrivateProfileStringA`),
    `+0x0d8`, `+0x0f8` (`WritePrivateProfileSectionA`), `+0x100`, `+0x104`;
  - the joystick: `+0x1c0` (`joyGetNumDevs`, `joyGetDevCapsA`) and `+0x1c4`
    (`joyGetPosEx`, `joyGetDevCapsA` and the registry); `+0x1c8` calls
    host helpers only;
  - the mouse as yoke: `+0x274` (`SetCapture`, `ClipCursor`,
    `ShowCursor`, `SetCursorPos`) and `+0x278` (releases them);
  - accelerators `+0x1e4`; menu items `+0x254` (`CheckMenuItem`,
    `EnableMenuItem`, `DrawMenuBar`);
  - `+0x048` (calls `0x403950`, which sets up something once, guarded by
    `0x418854`), `+0x054` (returns the main window and the instance through
    two pointers), and `+0x1ec`/`+0x1f0`/`+0x1fc`, which go through the
    host's lookup at `0x410240`.
- **DirectInput is CONTROLS's own.** `.text+0xb480` loads `dinput` and
  `DirectInputCreateA`. `.text+0xb4c0` creates version `0x500` and enumerates
  type 4 (joystick) devices with flags `0x101` (attached and force
  feedback), then drives the device through about twenty vtable calls. With
  no such device, enumeration finds nothing and that path stops. That is
  the case a first harness should model, before adding a device.
- **`GetKeyboardLayout`** makes key naming depend on the machine. A harness
  must hook it and fix the layout per scenario, or the two sides could agree
  by accident on one layout and never see another.

**Harness:** `sounddiff`'s profile passthrough over a generated
`FLTSIM98.CFG`, with mapping sections written per scenario (count
configurations, not scenarios). Add a scripted joystick for `+0x1c0`/`+0x1c4`
(device count, caps and positions as scenario inputs), a model of the mouse
capture state, and a DirectInput stand-in with zero devices first and one
force-feedback device later. Commands posted through `+0x03c` are logged.
Logging stubs cover MAIN, HG2D and PANELS. UTIL, NUMERIC, SOUND, ACONTAIN,
ADVDRV, ATC, DEMO and WINDOW are done and can be retail's.

**To read first:** where a key arrives. MAIN calls ten CONTROLS slots
(`+0x24`..`+0x58`); WINDOW calls `+0x24` and `+0x40`; DEMO `+0x44`;
REALITY `+0x5c`. Those harnesses already stub CONTROLS, and their stubs
record what retail CONTROLS must accept. Read them for the argument shapes
before reading `+0x18`.

**Status: done, verified and running in-game.** The hand-written half
(`src/controls/controls.c`: the dispatcher, all 342 handlers, the CHAIN
callbacks, init/deinit) and the compiled-C half
(`src/controls/controls_input.c`: the key bindings, the joystick poll and
watcher, the joystick configs and their defaults, force feedback over
DirectInput 5, and the key-assignment dialog `+0x58`) are transliterated.
`make controlsdiff-run` drives them against retail over 20,000 scenarios, 0
differences, over the host's profile services passed through to fixture
files, models of its accelerator tables and joysticks, a DirectInput
stand-in, and a stand-in for the dialog: `0x140a` of `English\Options.DLL`,
an MFC dialog that calls back into CONTROLS through a parameter block, built
from its listing. `make controls-breaks`: 78 of 81 deliberate breaks caught,
3 proven equivalent. `make smoke` passes with it installed. See
[modules/controls.md](modules/controls.md).

## MULTIP (id 35) and MPCHAT

**MULTIP** is multiplayer: sessions, players and the other aircraft. Its
options are named in `.data` (`CONNECTION_TYPE`, `MAX_SEGMENTS_PER_SEC`,
`ALLOW_TEXTURE_SEND`, `COLLISION_SECS`, `FS61_REQUEST_PARAMS`, …).

- MSVC C, 11 functions, with 177 x87 instructions. It reads the user's
  position and attitude (GLOBAL `+0x560`..`+0x57c`), so some of that is
  probably the other aircraft's extrapolation. Not yet read.
- **DirectPlay is driven directly through COM**, not through the
  host. Only four host services touch the host's DirectPlay globals
  (`0x41a518`/`0x41a51c`): `+0x120`, `+0x124` and `+0x128`, which load
  `dplayx`, call through its exports (`DirectPlayEnumerateA`, …) and free
  it, and the 7,312-byte `+0x094`. MULTIP itself makes COM-shaped calls
  (`mov ecx,[obj]` / `call [ecx+N]`) at about twenty distinct vtable
  offsets, and calls one function pointer held at `.data+0x980`. The main
  object is confirmed as `IDirectPlay3A`: MULTIP queries its literal IID and
  its calls line up with the inherited vtable through `SetSessionDesc` at
  `+0x7c`. A harness needs a fake DirectPlay object with sessions, players
  and a message queue.
  The other side of a session is then a script, which is better than a
  second machine.
- **A worker thread** (`CreateThread`, `WaitForMultipleObjects`, events,
  critical sections), so BOOSTER's pattern applies: run the sides in turn
  and drain the thread after every operation.
- Unfinished imports are minimal: MAIN `+0x74` (fire-and-forget) and G3D
  `+0x38` ×2 and `+0x64`. Only `+0x64`'s value is read.
- `ShellExecuteA` is called once, on a path MULTIP builds at run time in
  `.data+0xb00`. The harness must hook and log it, never run it.

DirectPlay does not exist on the native targets, so the fake object is more
than a fixture: it is a written-down account of exactly what MULTIP
expects from a transport, which a native build will need to provide.

**Status: in progress.** The public `+0x24`..`+0x38` player-list, lookup,
status and active-range slice is transliterated and differential-tested
against retail. It is kept out of the normal module list until session setup
and transport exist. See [modules/multip.md](modules/multip.md).

**MPCHAT** (3,104 bytes) is the chat window. Its id is `0xffffffff`, so no
module can import it. It hooks itself onto CHAIN lists `0x1d`, `0x14`, `0xb`
and `0xc`, registers a window class, loads `RICHED32.DLL` and builds the
window from `Button`, `Edit`, `ListBox` and `RichEdit` controls. It calls
MULTIP `+0x20`..`+0x2c` once each and reads three of the results, and
WINMSG `+0x18` three times. After MULTIP it is a small harness: the USER32
calls hooked through its IAT against a model of window text and list
contents, as `boosterdiff` hooks `MessageBoxA`. Linkage `+0x14`, unusually,
points into `.data+0x30`, at a record that starts with `0xff00`. That is
the dialog id MPCHAT passes to host `+0x110`. What reads that record is
not yet known.

## G3D (id 14)

The 3D pipeline: transforms, clipping and the scene from FE's stream, into
HG2D. 92,518 bytes of asm with ATC's wrapper at 119 sites, and 99 KB of
initialised `.data` (0x18600 raw bytes; tables, by the size).

- **Its host needs are trivial**: 11 services, all already modelled by done
  modules' harnesses. Its Win32 calls are `CharLowerA` and `_CIacos`.
- **FE ×20 is done**, so the harness can load retail FE over the real
  scenery tree, as `landmediff` does. `+0x34` (42,875 bytes) is by far
  the largest service, and the likeliest consumer of FE's scene stream.
- **HG2D ×20 sites, 17 slots**, and only `+0x68` and `+0xa8` have their
  results read. The drawing path does not change that. A clipper at
  `.text+0x7d73` saves the draw routine at `.data+0x4910`, replaces it from
  eax, calls it per span (nine sites) and restores it on the way out; those
  are the slot's only writes. **G3D supplies the routine itself** (traced,
  statically): its three calls (`.text+0x84ec`, `+0x8e29`, `+0xa137`) each
  load eax just before with an immediate address in G3D's own `.text`
  (`0x206e9508`, `0x206e9e2f`, `0x206eb17d`). What those reach calls HG2D only
  fire-and-forget: `+0x48` and `+0x74` with six register arguments,
  `+0x4c` and `+0x78` with the view record's `+0x80` behind GLOBAL
  `+0x2f0`'s countdown, and no result read. The third routine also
  dispatches through two tables at `.data+0x4bac` (11 entries) and
  `+0x4bd8` (8; the words after them are the clipper's bounds), and reaches
  a bytecode interpreter at `.text+0x15c0` (opcodes below `0x94`, the
  stream pointer at `.data+0x126`), whose 148-entry handler table at
  `.data+0x4220` is likewise G3D's own code. All three tables are static
  (relocated in the file, never written). So HG2D can be a logging stub on
  this path, with its arguments as the output.
- GLOBAL: 235 fields, 851 accesses. Most of the sim's view state goes
  through here, and every one of those names is worth recording.
- **It divides into the host's repair** ([modules/host.md](modules/host.md)):
  `0x206e18c6` (`.text+0x8c6`, in `+0x28`) faulted 17 times in one flight.
  The reimplementation has to produce the saturated quotient without
  trapping, as WINDOW's does, and the harness has to model the filter.
  HG2D does the same at `0x207376e2`/`0x207376fc`/`0x20737716`
  (`.text+0x66e2`..), tens of thousands of times a flight, so an HG2D
  logging stub on G3D's path never sees those -- but a real HG2D would.

## PANELS (id 15)

The instrument panel and the gauges. Gauges are module id 19: each of the
CD's 120 `.GAU` files has Linkage id 19 and imports only PANELS (id 15).
PANELS's own import of id 19 is never read: nothing in it references the
link. By the call shapes it reaches gauges through host `+0x298`, which
looks a record up through the host's table lookup `0x410240` and hands
back its `+0x08` (or -1 and a null), and `+0x29c`, which PANELS calls
after calling through `[esi+0xc]` or `[esi+0x10]`. Neither is read in full yet. Either
way a PANELS harness can load the real gauges, the way FE's runs over
real scenery.

What holds it back is HG2D. PANELS calls 10 HG2D slots 36 times and reads
the results of `+0x48`, `+0xac` and `+0xb0`. It also reads four HG2D Linkage
fields directly (`+0x8c`..`+0x98`, 17 sites), which are HG2D's own state
rather than a call a stub could answer. It draws with GDI itself (DIB
sections, fonts, `DrawTextA`) and reads panel files with `fopen`. MAIN
`+0x80`'s result is read at all four sites. Do it after HG2D, or after G3D
has shown how much of HG2D a model must carry.

## MAIN (id 3)

The sim's controller, by its strings: situations and flights ("Temporary
Flight"), the default aircraft, clouds and smoke, and 0x3664 bytes of
resources. It imports 22
modules, every unfinished one among them, and calls 75 host services, 18 of
them new: `+0x018`, `+0x01c`, `+0x028`, `+0x034`, `+0x044`
(`GetLocalTime`), `+0x048`, `+0x088`, `+0x098` (`GetCommandLineA`),
`+0x23c`, `+0x240`, `+0x254`, `+0x27c`, `+0x284`, `+0x288`, `+0x298`,
`+0x29c`, `+0x318` and `+0x31c`. It is the last module before HG2D for the
same reason HG2D is last: everything else has to be done for its harness to
be anything but stubs.

## HG2D (id 13)

Not surveyed further: 389,734 bytes, the rasteriser, still last. It calls
G3D `+0x44` four times and nothing else unfinished, and its host needs are
10 services, two of them new: `+0x290`, which copies the host's palette
tables (GDI32 and USER32), and `+0x31c`, a six-way dispatcher into the
host's DirectDraw, video and audio-compression code.

## Also open

- **The CHAIN ecx fix** from round 8: retail CHAIN calls each callback with
  ecx set to the dispatch's first argument, and OpenFS98's CHAIN does not.
  It is independent of this round and should be proven in `chaindiff`
  first.

# Round 8: ANALYSIS

## Verdict

**Externally testable**, and next. It has REALITY's shape: a frame callback
on CHAIN that records samples, and dialogs whose control procedures draw
only lines into a host record. A line is its arguments. The one thing it
draws in the 3D world, the course trail, is a display list that it builds in
its own heap and hands to G3D, so the list itself can be the logged output.
All nine host services it uses are already in `host.h`.

| | ANALYSIS |
| --- | --- |
| `.text` | 8,086 (`0x1f96`) |
| functions in Linkage | 12, `+0x04`..`+0x3c` with `+0x0c`..`+0x14` null (hand-counted; `dumplinkage.py` agrees) |
| written in | asm to `.text+0x915`, MSVC C from `+0x920` |
| unfinished imports | HG2D ×17 (`+0x2c`, `+0x30`, `+0x34`), G3D ×1 (`+0x28`) |
| done imports | CHAIN, NUMERIC, UTIL |
| host services | 9, none new: `+0x074`, `+0x078`, `+0x084`, `+0x10c`, `+0x20c`, `+0x214`, `+0x218`, `+0x21c`, `+0x2a8` |
| direct Win32 | none (MSVCRT `_ftol`) |
| callers | MAIN `+0x34` calls `+0x34` (the menu command); nothing else found |
| external harness | yes: REALITY's dialog stand-in and line stubs, a logging G3D, retail UTIL and NUMERIC |

## Shape

`.data` is one 0x32c-byte image (`ImportTable` at `+0x88`, `Linkage` at
`+0xc8`) with 23 relocations: the Linkage, three dialog parameter blocks,
their string pointers and the exchange table's callback. A harness compares
it as offsets, as `realitydiff` does.

GLOBAL `+0x814` is the mode: 0 off, 1 landing analysis, 2 course tracking,
3 manoeuvre analysis. Word `+0x1a` bit `0x200` is set while a mode is on.

- **Assembly, `.text+0x020`..`+0x912`.** It uses ATC's stack-alignment
  wrapper around every import call. `+0x04` allocates the sample buffer,
  1,000 records of 0x1c bytes (`.data+0x44`), clears it, and registers the
  frame callback `.text+0x2ae` on CHAIN list 5. `+0x08` unregisters lists 5,
  0xc and 0xe and frees both heap blocks.
  - **A sample** is a flag byte (0x81, |2 when GLOBAL `+0x366` is set, 0x83
    for the touchdown mark), then the 24 bytes of GLOBAL `+0x560`. It is
    recorded only when those 24 bytes differ from the last sample's
    (`.text+0x53b`).
  - **`+0x1c`..`+0x30`** are accessors over the buffer: a whole record, the
    24 bytes, the flag, a store, the count, and setting the count. Each is
    bounded by `< 1000`.
  - **The frame callback** keeps a countdown at `.data+0x70`. It reloads it
    with 2 in landing mode, with byte table `.data+0x73[+0x824]` (5, 14, 29,
    50) in course tracking, and with `.data+0x71` in manoeuvre mode.
  - **Landing mode arms and triggers on height.** It forms the height above
    the ground from `+0x572`/`+0x574` less `+0x20` with a half-width
    `sub ah,bl` / `sbb`. It arms above 0x22 and triggers at 0x1e:0x7ae5 or
    lower (about 100 ft), restarting the buffer. Once GLOBAL `+0x2b8` is at
    most 0xc7f and `+0x366` is set (both), it opens the landing dialog.
  - **Manoeuvre mode** registers `.text+0x277` on CHAIN list 0xc. That
    callback watches for command 0x1002b with a zero argument, which ends the
    run and opens the manoeuvre dialog. A crash state (GLOBAL `+0x838` 1..4)
    also ends a run.
  - **Course tracking with GLOBAL `+0x81c`** registers `.text+0x60a` on
    list 0xe. That callback builds a display list in a 0x59e4-byte block
    (`.data+0x50`), with opcodes 0x76, 0x51/0xf00f, 0x2f/0x2e with the
    sample's position, 0x0e, alternating 0x0f/0x10, and a 0 terminator. It
    passes the block to G3D `+0x28`. The number of segments comes from
    dword table `.data+0x76[+0x828]`, which holds 50, 250 and 500. That
    table overlaps the byte table's last entry.
- **Compiled C, `.text+0x920`..`+0x2f6e`**: three dialogs through host
  `+0x10c`.
  - **`0x14050002`, landing analysis.** The block is at `.data+0x148` and
    the control procedure at `.text+0x9a0` (0 attach, 1 paint, 2 release,
    REALITY's protocol). Attach clears the 0xa4-byte record at
    `.data+0x280`, attaches it with host `+0x074`, and allocates a 0xc0-byte
    view of half-extents. Paint fills the surface (`+0x94`/`+0x98`) with
    0xff through host `+0x218` and draws a grid with HG2D `+0x2c`/`+0x30` in
    colour 0xf000. It then draws the descent in 0xf00f through HG2D `+0x34`:
    UTIL `+0x64` ranges each sample from the first, and the result is scaled
    by integer arithmetic. The vertical speed at the 0x83 mark is formatted
    by NUMERIC `+0x18` (id `0x1000a`) into `.data+0x108`. Release goes
    through host `+0x078` and `+0x214`.
  - **`0x14050003`, manoeuvre analysis.** The block is at `.data+0x188` and
    the procedure at `.text+0x590`. The record and view are the same. It
    finds the run's extremes (`.text+0x2f0`) and converts them through UTIL
    `+0xc8`/`+0xc0`/`+0xbc`/`+0xc4`/`+0x48` (48-bit ↔ double, longitude to
    metres). It scales them on the x87 with `_ftol` and formats the scale
    with NUMERIC `+0x18`. The units come from GLOBAL `+0xc18`.
  - **`0x1405`, the options (`+0x34`).** The block is at `.data+0x1a0`, with
    a UTIL exchange table at `.data+0x1e0`: `+0x90` reads it, `+0x8c(…, 0)`
    commits it, and `.text+0x2d70` validates through `+0x8c(…, 1)`. The
    table's callback `.text+0x2d90` maps the mode to and from the dialog
    (jump table in `.text` at `+0x2e6c`) and writes GLOBAL `+0x814`..`+0x820`
    and `+0x1a`. GLOBAL `+0x824` and `+0x828` go through the table directly.

## Three things a harness must model

- **The compaction counts in ecx across host `memmove`.** When the buffer
  fills, `.text+0x5cd` keeps every second sample. Its slot index is ecx, kept
  across `.text+0x889`/`+0x8d1`. Those save only eax and edx, and they call
  host `+0x21c`, which loads ecx itself and calls MSVCRT `memmove`. So each
  kept sample goes to the slot that `memmove` leaves in ecx, and the count
  ends at that value plus one. Measured with a probe making ANALYSIS's own
  0x1c-byte copies:

  | runtime | ecx after | what compaction does |
  | --- | --- | --- |
  | wine 9 builtin `msvcrt` (what this prefix loads; no override) | 0 | every kept sample written to slot 0, then count 1 |
  | the CD's `MSVCRT.DLL` (Jan 1997) | 7 (the dword count) | slots 1..6 stale, slot 7 sample 998, then count 8 |

  Either way the trail collapses instead of halving. The interval byte
  `.data+0x71` is also `rol`'d, so after eight compactions it wraps from 0x80
  back to 1. The value is platform behaviour, not a property of ANALYSIS.
  **Decided: reproduce the CD runtime's 7**, because that is the DLL FS98
  shipped and ran with. The reimplementation leaves 7 in ecx after each of
  those copies on every platform, and the harness's `+0x21c` stub does the
  same, so both sides match the CD. Under wine, retail ANALYSIS still gets
  the builtin's 0 in-game. That is a difference in wine, not one to copy.
- **The change test counts with cx only.** `.text+0x54f` does `mov cx,0xc` /
  `repz cmpsw` in 32-bit code, so the count is the caller's top half of ecx
  joined to 0xc. With a nonzero top half and an unchanged position, the
  compare runs past the 24 bytes into GLOBAL `+0x578` against
  `.data+0x44`, and a stationary aircraft records samples anyway. ecx there
  is what the path left: host `+0x218`'s `rep stos` (0) after a reset,
  otherwise CHAIN's (`+0x18`, or the dispatch's). The entry needs DYNAMIC's
  ecx-passing stub, and the harness must choose ecx per scenario.
- **A trail length index past 2 has no limit.** `.data+0x76[3]` reads 0, and
  the segment loop's `dec`/`jne` then runs until the samples run out. 1,000
  samples need about 46,000 bytes of list, and the block is 0x59e4, so the
  list is written past the block's end. The options dialog's validator
  (`.text+0x2e80`) refuses list values above 2 (status 0xb; confirmed in
  the full read). Whether GLOBAL `+0x828` can reach 3 any other way (a saved flight)
  needs checking. The harness should guard-page that block as `dynamicdiff`
  does.

## Stubs

- **HG2D** `+0x2c`/`+0x30`/`+0x34`: logged with every argument and GLOBAL
  `+0x2ec`, plus the view they draw through, as `realitydiff`'s are.
- **G3D** `+0x28`: logged as the list's bytes up to its terminator. It reads
  ANALYSIS's own heap block and returns nothing ANALYSIS reads.
- **Host**: REALITY's dialog stand-in for `+0x10c`, which calls attach,
  paint, release and the exchange callbacks. Also WINMSG's record model for
  `+0x074`/`+0x078`/`+0x084`, with a render size and a real surface, because
  paint `memset`s it. The memory services are transliterated, with `+0x21c`
  leaving the chosen ecx.
- **CHAIN**: retail's or OpenFS98's (done). The frame, list 0xc and list 0xe
  callbacks are driven by the script with a chosen ecx.
- **UTIL, NUMERIC**: retail's, with state restored per side. `+0x64`,
  `+0x48`, `+0xbc`..`+0xc8`, `+0x8c` and `+0x90` are all done and verified.

x87: the manoeuvre graph ends in `_ftol`, so run each scenario under a random
precision control, as `sounddiff` does. UTIL `+0xbc` is the service that
cannot be bit-identical on ARM ([modules/util.md](modules/util.md)). ANALYSIS
inherits that through it and adds nothing new.

**Outcome:** done as planned, [modules/analysis.md](modules/analysis.md) --
20,000 scenarios, 0 differences, the CD runtime's 7 reproduced. The full
read confirmed the validator (an exchange callback's base is the block, so
`+0x34`/`+0x3c` are the lists' values and status 0xb a refusal). It added
the landing height's borrow (a `sar` between `sub ah,bl` and `sbb`), a zero
range's divide, the highest latitude's tie-break, "off" not staying off, and
both graphs reading an uninitialised frame with no samples. The change test's
ecx top half is latent in retail: retail CHAIN passes the dispatch's first
argument in ecx, and MAIN dispatches list 5 with 0.

# Round 6: WINDOW

## Verdict

**WINDOW, externally**, after all: what it does to a window it does through
the host's window services, and those turned out small enough to model
faithfully (the z-order, the sizes the host's `WM_SIZE` gives a record, the
menu items), so the harness tests WINDOW's logic rather than a fake host.

**Outcome:** done and verified -- [modules/window.md](modules/window.md).
`make windowdiff-run`, 20,000 scenarios, zero differences.

| | WINDOW |
| --- | --- |
| `.text` | 24,992 |
| functions in Linkage | 32 (`dumplinkage.py` said 2: it stopped at the `0xc0000000` at `+0x10`) |
| written in | MSVC C to `.text+0x3490`, asm from `+0x3504` |
| unfinished imports | MAIN ×8, G3D ×7, HG2D ×6, CONTROLS ×2, PANELS ×2, REALITY ×1 |
| host services | 32, 15 new: the window services, menus, strings |
| direct Win32 | none |
| external harness | yes: a model of the host's windows, logging stubs for the rest |

The unfinished imports are all fire-and-forget from WINDOW's side except
three values it reads back: MAIN `+0x9c` (whether the frame wants the
stars), G3D `+0x34` (what a cloud layer covered) and HG2D `+0xbc` (a hit
test). G3D `+0x3c` normalises a vector in WINDOW's own `.data`; its stub
does what G3D does (the top bit to bit 28, by G3D's own table).

Reading the harness's first faults found the host's divide filter: the sim
runs under a `__try` whose filter repairs integer divide overflows, and
WINDOW's eye overflows constantly. See [modules/host.md](modules/host.md).

# Round 5: DEMO, ADVDRV

## Verdict

**DEMO first**, because ADVDRV imports it (`+0x28`, to play a lesson's video).
It is now done: [modules/demo.md](modules/demo.md).

**Outcome:** ADVDRV is done and verified too --
[modules/advdrv.md](modules/advdrv.md). It is MSVC C, an x87 expression stack
machine over 176 GLOBAL-backed system variables driving a bytecode
interpreter. `make advdrvdiff-run` runs both sides over the CD's forty-four
programs and synthetic ones an assembler in the harness writes, 20,000
scenarios, zero differences. The one obstacle was `op_error`: it stops the
program (freeing the code) and returns to the interpreter, which runs on
undefined; the harness poisons comparison past that alert, both sides alike.

| | DEMO | ADVDRV |
| --- | --- | --- |
| `.text` | 16,864 | 29,330 |
| functions in Linkage | 12 | 9 |
| written in | asm to `.text+0x353e`, MSVC C after | C, by its aligned entries (not yet read) |
| unfinished imports | MAIN ×2, CONTROLS ×1, WINMSG ×1 | MAIN ×12, DEMO ×1, WINMSG ×2 |
| host services | 29, two new (`+0x170`, `+0x178`) | 38 |
| direct Win32 | none | none (MSVCRT) |
| external harness | yes: real CD videos, a dialog stand-in | yes, after DEMO |

DEMO's four unfinished calls are fire-and-forget: MAIN loads a situation,
CONTROLS takes a command, WINMSG shows a message. Logging stubs model them
faithfully, apart from the registers CONTROLS leaves, which the harness sets.
Its dialogs, like FACIL's and the Scenery Library's, are callbacks that the
host's dialog engine calls through parameter blocks, so they too were
verified externally.

The rest of the queue by `.text`: MPCHAT 3,104 (needs MULTIP), WINMSG 4,416
and REALITY 4,822 (windows and dialogs: `ofstest`), ANALYSIS 8,086 (draws
through HG2D), WINDOW 24,992 (round 6), ADVDRV, MULTIP 43,318, CONTROLS 48,406, PANELS
50,038, G3D 92,518, MAIN 119,648, HG2D 389,734.

# Round 4: FE, LANDME, FACIL, BOOSTER

## Verdict

**FE's three external-harness parts, LANDME and FACIL are done and verified;
LANDME and FACIL run in-game.** BOOSTER stays last.

**Outcome:** BOOSTER is done as well, verified externally after all:
[modules/booster.md](modules/booster.md). The thread was no obstacle once
the harness ran the two sides in turn and drained the thread after every
operation.

1. **The grid conversions, `+0x60`..`+0x6c`.** These are pure and table-driven, so
   they are tested by vectors like UTIL's. They are small, they pin down FE's
   word-argument entry stubs before anything depends on them, and the asm
   engine calls two of them itself.
2. **The asm engine.** A whole-module harness runs it over the CD's real
   scenery: a stub host passes file I/O through to a fixture tree, as
   `acontaindiff` does, and BOOSTER and FACIL are logging stubs.
3. **The C half's file services:** `+0x1c` (textures, for HG2D) and
   `+0x7c`..`+0x88` (BGL sections, for FACIL), in the same harness.
   `+0x78`, the Scenery Library and booster dialogs, was planned for
   `ofstest`; like FACIL's dialogs it turned out to be World.DLL callbacks
   and was verified externally instead (`make fesldiff-run`, 20,000
   scenarios, zero differences). FE is installed; see
   [modules/fe.md](modules/fe.md).

The completed `fediff` baseline runs all of parts 2 and 3 for 20,000 scenarios:
11 operation classes, 7,272 successful texture loads, 3,636 successful FACIL
section reads, and zero differences. Part 1 remains separately covered by
7,742,499 grid comparisons with zero mismatches.

The surprise is that **LANDME was never blocked on FE's record format.** Its
harness can load FE for real, retail or ours, once there is a stub host that
serves a scenery tree. That stub host is exactly what part 2 builds. So FE
first still holds, for a better reason: its harness is LANDME's fixture.

| | FE | LANDME | FACIL | BOOSTER |
| --- | --- | --- | --- | --- |
| `.text` | 49,016: asm 22,418, then C 26,548 | 4,094 | 23,045 | 8,024 |
| functions in Linkage | 29 (`dumplinkage.py` reports 2) | 3 | 9 | 4 |
| written in | asm (`+0x18`..`+0x74`), then MSVC C (`+0x1c`, `+0x78`..`+0x88`) | asm | C++ (`new`, `__CxxFrameHandler`, `_ftol`) | C |
| unfinished imports | BOOSTER ×2, FACIL ×1 | FE ×2, MAIN ×1, WINMSG ×1 | FE ×9 slots, MAIN ×3 | FE ×2 |
| host services | 46, 21 not yet read before this round | 4 | 38 | 19 |
| direct Win32 | critical sections | none | file mapping, `Sleep` | threads, mutexes, events, `CopyFileA` |
| external harness | yes, over real scenery files | yes, with FE loaded for real | its file half, after FE | no -- a worker thread |

## FE (id 18)

FE is the scenery file engine and the sim's whole scenery database. G3D gets
its scenery stream from it, MAIN its navaid and ground lookups, DYNAMIC its
traffic blobs, HG2D its textures and FACIL its BGL sections. It also owns the
Scenery Library and the "performance boosters" (CD scenery cached to disk).

**Layout.** `.text+0x020`..`+0x5792` is hand-written asm with ATC's alignment
macro. `.text+0x57a0`..`+0xbf54` is MSVC C, and the four `jmp`s after it are the
critical-section thunks. `.data` is 0x3400 bytes, with dwords at odd
addresses (`.data+0x119`, `+0x2e7`, `+0xa5d`), so an image struct has to be
packed. Every asm slot enters and leaves the critical section at `.data+0x1734`,
because BOOSTER calls `+0x70`/`+0x74` from its own thread. Many slots take
16-bit arguments, which callers push as `sub esp,2; push cx`, so each needs an
asm entry stub, as DYNAMIC's `+0x18` did.

**The Linkage, hand-counted.** `+0x10` holds `0xd0000000`, which is where
`dumplinkage.py` stops. The table actually runs `+0x18`..`+0x88`:

| slot | callers | `ret` | what it is |
| --- | --- | --- | --- |
| `+0x18` | G3D ×2 | `0x10` | fill G3D's display buffer `(buf, room, &a, &b)` with the scenery stream |
| `+0x1c` | HG2D ×3 | `0x10` | load a texture `(buf, name, dir, kind)` from `TEXTURE\` and two fallbacks. Kind 9 reads `0xc0` bytes at `0x240` (a palette); kind 10 reads `0x1000` and sets GLOBAL `+0xcd2`; otherwise a file under 64K is decompressed by `.text+0x54a0`, and a larger one is read raw |
| `+0x20` | MAIN ×2, G3D, LANDME | `0xc` | the next record of the last search `(word i, &a, &b)` |
| `+0x24` | MAIN | `0xc` | by its arguments, a navaid by frequency `(word GLOBAL+0x34e, pos, &out)` |
| `+0x28` | MAIN ×2 | `0x10` | the same with a channel byte `(word +0x34c, byte, pos, out)` |
| `+0x2c` | MAIN ×2 | `0xc` | the same shape without the channel byte `(word, pos, out)` |
| `+0x30` | MAIN, FACIL | `4` | from a position; MAIN stores the result in GLOBAL `+0x2a0` |
| `+0x34`/`+0x38` | FACIL ×2 each | `8` | iterate, after `+0x4c` (no arguments) or `+0x50` (`ret 0xc`) sets up |
| `+0x3c` | G3D ×2 | `0x14` | size, then contents `(rec, &b, 0, 1, &size)` -- the shape DYNAMIC's `+0x40` has |
| `+0x40` | DYNAMIC ×2 | `0x10` | the dynamic-scenery blob (see [modules/dynamic.md](modules/dynamic.md)) |
| `+0x44` | G3D | `4` | reset; also called from FE's own deinit |
| `+0x48` | G3D ×2 | `4` | a level word G3D derives from its range table -- 3,841 bytes, the engine's core |
| `+0x54` | MAIN ×2, G3D ×4, LANDME, DYNAMIC | `0xc` | search around a position `(pos, radius, GLOBAL+0x3e4)` |
| `+0x58`/`+0x5c` | none outside | | `+0x5c` is also FE's frame callback's area test |
| `+0x60`/`+0x64` | MAIN, G3D | `8`/`0xc` | latitude to grid band and back, through the band table at `.data+0xf20` |
| `+0x68`/`+0x6c` | MAIN, G3D | `8`/`0xc` | the same for longitude |
| `+0x70` | BOOSTER | `8` | register one BGL file: read its `0xa8`-byte header, check its version against GLOBAL `+0x424`, and append a `0x1ae`-byte record |
| `+0x74` | BOOSTER ×2 | `8` | BOOSTER's other callback |
| `+0x78` | MAIN | | the Scenery Library: `run_dialog` ×3, `qsort`, `.VIS` files, temp names, free space |
| `+0x7c`..`+0x88` | FACIL ×11 | `8`, `8`, `0x14`, `0x14` | list the scenery files, open one, read a section by kind (`0x10003`..`0x20001`), close |

**How it finds scenery.** Init loads `SCENERY` (string `0x9003`) and `TEXTURE`
(`0x9006`) from `ENGLISH.FLL`. The world file is `<host dir>\SCENERY\` plus the
name at GLOBAL `+0x446`, and is (re)read when GLOBAL `+0x444` bit `0x10` is
set; init sets `0x32`, and the reload clears `0x10`. On the CD that file is
`Scenery\world.vis`, a binary file with `MR` magic and a description. FE
reads 0x20 bytes of accepted BGL versions from it into GLOBAL `+0x424`, then
the area list, and then calls FACIL `+0x2c` (no arguments) to rebuild its
index.

**Unfinished imports:**

- **BOOSTER `+0x18`** comes from FE's CHAIN list 3 callback (`.text+0x4ce0`). For each area
  whose files live on the CD, it checks that the cache copy exists. If it
  doesn't, it raises UTIL's `+0xa4` alert ("Insert the Flight Simulator
  CD-ROM and press RETRY", string `0xf424`; `0x10` means retry), and then
  hands the area to BOOSTER. **BOOSTER `+0x1c`** releases one. The stub records
  both, and the harness calls `+0x70`/`+0x74` itself, as BOOSTER's thread
  would.
- **FACIL `+0x2c`**, after the world file is read. The stub records it.
- G3D is in the ImportTable but never called.

**Done imports:** CHAIN `+0x18`/`+0x1c`; NUMERIC `+0x18` ×10 (in the C half);
and UTIL `+0x18`..`+0x60`, `+0x8c`..`+0xac`, including the alert. All of these
can be retail's real modules, with the alert answered by the stub host's
dialog engine.

**Host services: 46.** Read this round from the callees:

- **Already in `host.h`:** `+0x13c`, `+0x17c`, `+0x194` and `+0x19c`.
- **Fatal file I/O:** `+0x180` open, `+0x198` seek, `+0x1a0` read and `+0x140`
  close. On failure each one shows string `0xf520`..`0xf523` plus the name in
  a `MessageBoxA` and calls `ExitProcess(1)`. A harness has to turn that into
  a scenario outcome, not a dead process.
- **File operations:**
  - `+0x148` (`ret 0xc`) creates a file for writing.
  - `+0x14c` (`ret 8`) deletes one, and `+0x16c` (`ret 0x10`) moves one.
  - `+0x134`/`+0x138` (`ret 0xc`) get and set attributes, through the
    host's own flag bits.
  - `+0x190` (`ret 8`) tells, `+0x1a4` (`ret 8`) gives the size, `+0x1a8`
    (`ret 8`) truncates and `+0x1b0` (`ret 0x10`) writes.
- **Paths and disk:**
  - `+0x130` (`ret 4`) copies the host's base directory.
  - `+0x150` (`ret 8`) gives free disk space as 64 bits.
  - `+0x174` (`ret 8`) makes a unique temp name with `GetTempFileNameA`,
    deletes the file, and strips `.TMP`.
  - `+0x18c` (`ret 4`) checks a name character by character.
- **Memory and lists:** `+0x228` (`ret 0xc`) is `memcmp`, `+0x230` (`ret 0xc`)
  swaps two blocks through a temporary, and `+0x304` (`ret 4`) zeroes a list
  header.
- `+0x2c4` (`ret 0xc`) fits a string to a length; not read further.

**GLOBAL:**

| field | what FE does with it |
| --- | --- |
| `+0x3e0`..`+0x3f4` | the search block. DYNAMIC's `+0x3e0` bit 0 lives here |
| `+0x414`..`+0x420` | written |
| `+0x424` | the accepted BGL versions |
| `+0x444`/`+0x446` | the flags and the world file name |
| `+0x365`, `+0x37e`, `+0x380`, `+0x390`, `+0xcb4`, `+0xcd2`, `+0x12a4` | written |

**The harness.**

- **Fixture tree.** A stub host passes file I/O through to it:
  - the installed `Scenery\` from the ISO (2.2 MB, including `world.vis`,
    the navaid and facility BGLs and the Chicago area);
  - `Texture\`;
  - two small boosters, `Antarcti` (1.5 MB) and `samerica` (1.1 MB), plus
    `Facility`. All the boosters together come to 186 MB, so the fixture
    takes these, not the lot.
- **Callers.** Scenarios drive the callers' slots (G3D's buffer fills, MAIN's
  lookups, searches and iterations, DYNAMIC's blobs, FACIL's section reads),
  the frame callback, GLOBAL `+0x444` reloads and BOOSTER's `+0x70`/`+0x74`.
- **Compared:** the transcript, every buffer handed back, GLOBAL, the heap,
  and `.data`. `.data` holds heap pointers, so it is compared with SOUND's
  pattern of pointers as offsets.
- **Mutation.** As with DYNAMIC, mutated BGLs will fault, and the fatal
  services end scenarios, so both are outcomes to compare.

**Risks:**

- the 16-bit arguments and unaligned `.data`;
- arithmetic that reads flags left by the previous instruction (`+0x64` does
  `sbb` straight after a `shl`);
- one x87 sequence (`fild`/`fmul`/`fistp` at `.text+0x3f39`);
- the texture decompressor, which builds an 8-byte-entry dictionary at
  `.data+0x1e10`;
- and sheer size: it is the largest slice yet.

## LANDME (id 26)

**Outcome: reimplemented, verified and running in-game.** Its two FE calls
(`+0x54` with radius 100000, then `+0x20` to read the records back) use
separately loaded copies of retail FE over FE's fixture tree, so the two sides
do not share FE's mutable search state. MAIN `+0x70` and WINMSG `+0x18` are
logging stubs, and host `+0x03c` records command `0x100d7`. The 20,000-scenario
baseline reaches all seven states and eleven nonzero phases, finds 18,881
runways, takes 1,119 no-runway paths and has zero differences. With LANDME
installed, `make smoke` reaches the panel with 27/27 modules and 95 gauges. See
[modules/landme.md](modules/landme.md).

## FACIL (id 37)

Facilities. It is C++, and maps files itself (`CreateFileMappingA`,
`MapViewOfFile`), outside the host. It reads BGL sections through FE
`+0x7c`..`+0x88`, iterates through `+0x34`/`+0x38`, and runs three dialogs.
Its GLOBAL writes are a position (`+0x56c`, `+0x572`..`+0x580`,
`+0x11f0`..`+0x1200`), which looks like the jump to an airport. MAIN calls
`+0x18`, `+0x24`, `+0x28` and `+0x30`, and FE calls `+0x2c`; it calls MAIN
`+0x1c`/`+0xb0`/`+0xc4` and CHAIN `+0x20`. The file half suits FE's harness,
and the dialogs go to `ofstest`. It is next.

**Outcome so far:** the complete `.data` image, profile init/deinit, cache-id
validation, hierarchy builder, serializer/replacement path and `+0x20` record
copy are reimplemented and verified by `make facildiff-run`. The synthetic BGL
fixture covers all five hierarchy levels and the absent-airport-name fallback;
the baseline produces a 0x195-byte cache and a 0xbcd0-byte deterministic arena
with zero differences. A deliberate wrong synthesized kind was detected at
cache offset `+0x16c`. The query services `+0x18`/`+0x1c` are now verified too,
over 20,000 seeded synthetic caches with zero differences. The dialogs did not
need `ofstest` after all: FACIL has no window code, only callbacks that
World.DLL's dialogs call through parameter blocks, so a stand-in for World
drove them over the real CD index, boosters included -- 10,000 scenarios, zero
differences. FACIL is installed. See [modules/facil.md](modules/facil.md).

## BOOSTER (id 34)

Now clearer: it is the CD scenery cache. Its strings are in `ENGLISH.FLL` from
`0xf420`: "Reading Scenery Files", the cache and disk-space errors, and
"Error copying file from this booster source directory". Each booster on the
CD is a directory with a `bstinfo` file and `scenery\files*\*.BGL`. Its thread
copies them into the cache and registers each with FE `+0x70`. The verdict
from round 3 stands: compare end states after the thread drains, once FE is
done.

## Things found on the way

- **`ENGLISH.FLL` holds the strings every module loads by id.** The directory
  names FE uses are at `0x9000`..`0x9016` (`SCENERY`, `TEXTURE`, `BSTINFO`,
  `BOOSTER`, `.BGL`), and the Scenery Library and booster messages start at
  `0xf420`.
- **The host has fatal variants of open, seek, read and close**
  (`+0x180`/`+0x198`/`+0x1a0`/`+0x140`). Only FE uses them so far.

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

# Round 7: REALITY

## Verdict

**Externally testable**, like WINMSG, and next. The "belongs in `ofstest`"
reading above (round 6) was made before WINMSG showed that a module which
draws can be verified by the calls it makes. REALITY draws only lines, and a
line is entirely its arguments.

| | REALITY |
| --- | --- |
| `.text` | 4,822 |
| functions in Linkage | 4 (`+0x04`, `+0x08`, `+0x18`, `+0x1c`) |
| unfinished imports | MAIN x3, HG2D x3, CONTROLS x1 |
| done imports | CHAIN, DEMO, WINDOW, NUMERIC, UTIL, WINMSG, SOUND |
| host services | 12; new: `+0x074`, `+0x078`, `+0x304` |
| direct Win32 | none (MSVCRT `_ftol`) |
| external harness | yes: logging stubs, WINMSG's window model, DEMO's dialog stand-in |

## Shape

Two programs, like WINDOW and DEMO. `.data` is one 0x510-byte image
(`ImportTable` at `+0x48`, `Linkage` at `+0xb0`) with 19 relocations: the
Linkage, an eight-entry jump table at `+0x00`, the dialog's callbacks and
some pointers into the image itself, which a harness compares as offsets.

- **Assembly, `.text+0x020`..`+0x8a4`**, with ATC's stack-alignment wrapper
  around every call. Init registers frame callbacks on CHAIN lists 1 and 3.
  They count down a crash state in GLOBAL `+0x838` and read the crash code at
  `+0x832` (0xe folded to 6; 0x10, 0x12 and 0x14 only with `+0x84a`; odd codes
  and codes above 0x14 become 4). On a crash it loads string `0x258 + code`
  into WINMSG slot 5 for 0x3c frames, pauses through CONTROLS `+0x5c(8, 1)`,
  plays SOUND event 0xe with a block (and keeps the result), switches a 3D
  view to the spot plane through WINDOW `+0x58(view, 4)`, greys the menu
  through MAIN `+0x30`, and later reloads through MAIN `+0x44`, or posts
  command `0x100c2`/`0x100c3` (host `+0x03c`). A stress check draws UTIL's
  random numbers (`+0x6c`, `+0x70`) against GLOBAL thresholds and sets the
  damage flags at `+0xb64`..`+0xb73` through the jump table, keyed on
  `+0x258 & 0xe`. `+0x18` clears those flags. `+0x1c`, called from WINDOW's
  frame, draws a shape from the point table at `.data+0xd0` into the current 3D
  view through HG2D `+0x34` (clamped to the view's half-extents).
- **Compiled C, `.text+0xa50`..`+0x22b4`**: the crash-analysis dialog, host
  `+0x10c` dialog `0x14050001` with the parameter block at `.data+0x228`
  (`.text+0xb10`, and the control procedure `.text+0xd20`: 0 attach, 1 paint,
  2 release) and a UTIL `+0x90` exchange table at `.data+0x280` (formatter
  `.text+0xcc0`, units from GLOBAL `+0xc18`). It reads 24 replay entries back
  through DEMO `+0x3c` into two host arrays (`+0x2f8`, emptied by `+0x304`),
  scales them with x87 arithmetic and `_ftol`, and draws a grid (HG2D `+0x2c`,
  `+0x30`) and the descent through HG2D `+0x34`. The colour is GLOBAL
  `+0x2ec`.

## Host services, from the callees

- `+0x074 (hwnd, rec)`, `ret 8`: attach a record to an existing dialog
  control. A 0x90-byte block at `+0xa0` with defaults, the hwnd at block
  `+0x24`, then `SendMessage(hwnd, 0x467, 0, rec)`, whose handler sizes it.
  REALITY reads the render size (`+0x84`, `+0x88`) and the surface (`+0x80`,
  `+0x94`, `+0x98`). -1 for a null record.
- `+0x078 (rec)`, `ret 4`: release the surface and the block.
- `+0x304 (list)`, `ret 4`: empty a host array (count 0).

## Stubs

- **HG2D** `+0x2c`/`+0x30`/`+0x34`: logged with every argument and GLOBAL
  `+0x2ec`, the colour. That is the whole drawing.
- **MAIN** `+0x30`/`+0x34` grey and restore the menu items `0x1001`..`0x1006`;
  `+0x44` (1,949 bytes) reloads the flight, and REALITY saves GLOBAL `+0x846`
  around it, so the stub should sometimes write GLOBAL.
- **CONTROLS** `+0x5c`: logged.
- **DEMO** `+0x3c`: a ring entry back, as a position above the ground
  ([modules/demo.md](modules/demo.md)), from the script.
- **SOUND** `+0x24`, **WINMSG** `+0x18`, **WINDOW** `+0x58`, **NUMERIC**
  `+0x18`: logged; NUMERIC writes the format and the value, as in windowdiff.
- **UTIL**: retail's, with its state restored per side, because `+0x6c` and
  `+0x70` advance a seed. `+0x90` runs the exchange table and `+0xc8` is called
  three times; both need reading for what they hand back.
- **The dialog**: a stand-in for host `+0x10c` that calls the block's
  callbacks the way DEMO's harness does: attach (with WINMSG's record model
  plus a render size), paint, release.

x87: the graph's scaling ends in `_ftol`, so run each scenario under a random
precision control, as `sounddiff` does.

**Outcome:** done as planned, [modules/reality.md](modules/reality.md) --
20,000 scenarios, 0 differences. UTIL `+0xc8` is 32.16 to double and `+0x90`
the exchange read, both retail's in the harness. The full read added the
metric scale's 0x30 stride and the countdown reading the dialog's result in
ah.

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

**Outcome (after round 6):** WINMSG was verified externally after all --
[modules/winmsg.md](modules/winmsg.md). What it does to the window is a short
list of host calls (blank, draw text at place 3 in a slot's colour, show,
hide, flush), and the host's side is four record fields: the pixel width at
`+0x38` and the surface at `+0x94`/`+0x98`, besides the flags. The harness
compares the calls, not the window, so the model only has to give the fields
plausible values. `+0x08c` was already prototyped by WINDOW; `+0x2b4` is the
strdup.

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
