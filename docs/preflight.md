# Preflight: WINMSG, ATC, LANDME

With CHAIN done, every module left imports at least one that is not. This is
the survey that picks the next one: for each of the three smallest, what it
calls in modules that are not done, and how faithfully those calls can be
stubbed in an external harness.

Produced with `python3 tools/impscan.py orig/Modules/<M>.DLL`, then each call
site and each callee read by hand. The scanner under-reports rather than
over-reports (see its header); the call counts below are lower bounds.

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
