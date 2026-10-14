# Preflight

Every module left imports at least one that is not done. A preflight is the
survey that picks the next one: for each candidate, what it calls in modules
that are not done, and how faithfully those calls can be stubbed in an
external harness.

Produced with `python3 tools/impscan.py orig/Modules/<M>.DLL`, then each call
site and each callee read by hand. The scanner under-reports rather than
over-reports (see its header); the call counts below are lower bounds.

Two rounds so far, newest first.

# Round 2: ACONTAIN, REALITY, WEATHERM

## Verdict

**WEATHERM next**, then ACONTAIN. REALITY last, through `ofstest`.

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
