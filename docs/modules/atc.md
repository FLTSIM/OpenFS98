# ATC.DLL — module id 24

**Reimplemented, verified, and running in-game.** ATC is the tower. MAIN gives
it an airport's ATIS record and it composes the broadcast; MAIN starts a
takeoff or landing clearance and it runs the exchange with the pilot as a
state machine on CHAIN list 3, one step a frame. Every message goes out
through WINMSG, optionally through a phrase translator driven by `ATIS.XLT`.

`src/atc/atc.c` is the transliteration and the module shell together.
`make atcdiff-run` drives retail and the OpenFS98 build side by side over
20,000 random scripts -- **0 differ**, comparing ATC's whole `.data` image as
well as everything it calls -- and `make smoke` passes with it installed beside
CHAIN, GLOBAL, NUMERIC, SLEW and UTIL: 27/27 modules, 95 gauges.

What `make smoke` proves is narrower than it sounds: ATC's init (the ATIS.XLT
load) runs at startup, but ATIS and the clearances only run in flight, which
is not reachable under wine. Those paths are verified by the differential
harness alone.

The reasons it was picked over WINMSG and LANDME are in
[../preflight.md](../preflight.md).

## Binary and linkage

- Retail image base `0x20640000`; `.text` 3,840 bytes; `.data` at `0x20643000`,
  0xa00 bytes on disk, 0x1a00 mapped.
- Imports: host, GLOBAL, CHAIN, NUMERIC, UTIL, WEATHERM (27), WINMSG (22).
- Win32: `GetPrivateProfileSectionA`, `GetPrivateProfileStringA`,
  `IsValidLocale`, `DisableThreadLibraryCalls`; MSVCRT `sscanf`.
- **Mostly hand-written assembly.** `.text+0x000` to `+0xb97` has an assembler
  macro around every cross-module call, `pusha`/`popa`, and register calling
  conventions. The translator from `.text+0xba0` on is compiled C (MSVC
  prologues, `int3` padding before it).

| slot | retail | service |
| --- | --- | --- |
| `+0x04` | `0x20641020` | init -- load ATIS.XLT (`.text+0xd20`) |
| `+0x08` | `0x2064102c` | deinit -- free the translation sections (`.text+0xec0`) |
| `+0x18` | `0x2064176f` | start a **takeoff** clearance exchange |
| `+0x1c` | `0x2064173f` | start a **landing** clearance exchange |
| `+0x20` | `0x20641038` | `atis(record)`, stdcall -- compose and show an ATIS broadcast |
| `+0x24` | `0x20641107` | clear WINMSG slots 2, 1 and 3 |
| `+0x28` | `0x20641b41` | is slot 2 or slot 1 still up? |

Callers: MAIN uses `+0x18`, `+0x1c` and `+0x20`; CONTROLS uses `+0x24`.
`make graph` reports 256 slots; the table is 11 entries and the rest is
`.data` (see the preflight).

## The data image

ATC keeps everything in one `.data` section, at fixed offsets: its ImportTable
(`+0x00`) and Linkage (`+0x40`), the message templates it patches in place, the
tables, and its state. The reimplementation keeps **one image laid out
exactly the same way**. `ImportTable` and `Linkage` are exported as aliases
for the image's `+0x00` and `+0x40`, and the code addresses everything else
by retail's offsets, so a number in a disassembly listing can be grepped for
in the C. `tools/gen_atc_data.py` extracts the initialised bytes and checks
the tables the C relies on.

Pointers *inside* the image (the macro table, the message tables) stay as
retail's absolute addresses and are translated on use (`ATC_PTR`). Heap
pointers stored in it -- the translation sections -- are real. That is a
32-bit layout; a 64-bit native build will need those two slots moved out.

| offset | what |
| --- | --- |
| `+0x070` | expanded message buffer (to `+0x470`) |
| `+0x471` | the ATIS record's four runway bytes |
| `+0x479` | the airport position, 24 bytes, unpacked by UTIL `+0x60`; ATC overwrites the last six with the field elevation from GLOBAL+0x20 |
| `+0x491` | delay counter, byte; `0xff` = idle |
| `+0x492` | state-machine step; `+0x496` mode (1 takeoff, 2 landing) |
| `+0x49a`, `+0x49e` | retry and wait counters |
| `+0x4a2` | "registered on CHAIN", set on first start, **never cleared** |
| `+0x4a6` | macro table: 21 pointers for template bytes `0x80`-`0x94` |
| `+0x4fa`-`+0x735` | ATIS and tower templates, patched in place |
| `+0x735` | `" LRC"` runway suffixes |
| `+0x739`, `+0x771` | takeoff (14) and landing (6) step handlers -- code addresses |
| `+0x789`, `+0x7c4` | request and tower-message string tables |
| `+0x8b3` | statute-mile visibility table, 6-byte entries, ends at `0xffff` |
| `+0x92b`, `+0x92f` | 160934 and 10000: miles to metres |
| `+0x934`-`+0x969` | `"%x"`, `"TRANSLATOR"`, `"LocaleID"`, `"00000000"`, `"ATIS.XLT"`, `"A"` |
| `+0x96c`, `+0x970` | translator: matched key length, "loaded" flag |
| `+0x980` | 26 translation sections, A to Z (heap) |
| `+0x9e8` | translator cursor (heap) |
| `+0xa00` | translated message buffer, in the zero-filled part of the section |

## Messages and macros

A template is text with macro bytes: `0x80 | n` expands to the string the
`n`th macro-table entry points at, copied raw (no nesting). `0x8f` is the
callsign, "Microsoft Flight Simulator"; `0x8a` is whichever wind phrase the
ATIS composer chose, by writing the table entry itself; `0x8b` is the cloud
text built at `+0x5e9`; `0x94` is the squawk.

Every message, ATIS or tower, goes through `.text+0x0c6`: unless GLOBAL+0xb6a
or +0xb68 is exactly 1, it clears the WINMSG slots, **recomposes the whole
ATIS** (calling WEATHERM to compute the weather at the airport and putting it
back on the aircraft afterwards), expands the template into `+0x70`,
translates it if GLOBAL+0x1290 and +0x128c are both set, and shows it for
`600 / (GLOBAL+0x362 + 1)`. Slots: 1 for the pilot's request, 2 for the
tower, 3 for ATIS.

The ATIS fields: temperature (`F` or `C` by GLOBAL+0xc18), wind direction and
speed (calm below 1, "light and variable" to 5), the hour, the cloud layers
from GLOBAL's two (`+0xe9a`.. and `+0xea2`..), thunderstorms, the runway --
the record byte chosen by the wind direction's top two bits, number in its low
six bits and `L`/`R`/`C` in its top two -- altimeter, and visibility, in
statute miles from the table or in metres or kilometres.

`+0x20` reads GLOBAL+0xb04 and +0xb68 **through the caller's EDI**, which it
never loads. MAIN's only call site has GLOBAL there, so the C reads GLOBAL
from ImportTable; the harness calls retail with EDI set the same way.

## The clearance exchange

`+0x18`/`+0x1c` set GLOBAL+0x27c, the mode and step 0, show the request
("Microsoft Flight Simulator requesting takeoff clearance... ") in slot 1,
set step 1 and register `.text+0x7ff` on CHAIN list 3. GLOBAL+0x27c going to
zero abandons the exchange on the next frame.

| step | takeoff | landing |
| --- | --- | --- |
| 1 | wait for slots 1 and 2 to clear | same |
| 2 | pause: 3 frames from idle | same |
| 3 | airborne already (GLOBAL+0x366 = 0)? "has already taken-off", finish | down already? "has already landed", finish |
| 4 | pick a squawk, "taxi to runway ... Squawk 02xy" | "cleared to land", when quiet |
| 5 | wait for GLOBAL+0x2b8, at most 30 quiet frames, else skip to 7 | finish |
| 6 | hold while +0x366 and +0x2b8 are both set | |
| 7 | transponder (GLOBAL+0x354, BCD) on the squawk? skip to 11; else "set transponder code" | |
| 8 | wait for quiet | |
| 9 | pause: 11 frames | |
| 10 | back to 7 twice; on the third time, on | |
| 11 | pause: 3 frames | |
| 12 | "cleared for takeoff", when quiet | |
| 13 | finish: step 0, unregister | |

The squawk is `02` and two octal digits from GLOBAL+0x2aa and +0x2ac. A pause
counts down a byte that idles at `0xff`: reload, count to zero, step on.

## The translator

`ATIS.XLT` lives in the directory named by string `0x9052`. Its
`[TRANSLATOR] LocaleID` decides GLOBAL+0x1290 (through `IsValidLocale`), and
when GLOBAL+0x128c is set at init, one section per initial letter is read
whole, growing the buffer by 0x400 until `GetPrivateProfileSectionA` stops
returning size-2, and each `key=value` is split at the `=`.

To translate, each position in the message is looked up in the section for
its first letter:

- The message is upper-cased for the comparison; **the keys are not**. A key
  with a lower-case letter can never match.
- Keys match as prefixes, not words: `MI` matches `MICROSOFT`.
- The first full match in file order wins, so a short key before a longer one
  shadows it.
- An empty key (`=value`) ends the search as if the section had ended. A line
  with no `=` is a key whose "value" is the next line.
- A position with no match copies one word and the space after it -- **up to
  the next space even past the end of the message**, so a message that does
  not end in a space drags following buffer bytes into the output.

## Retail behaviour that is reproduced

- The word copy runs past the terminator (above). The bytes it reads are the
  same image bytes on both builds.
- `+0x18`/`+0x1c` register the step callback on every call, so two starts
  before a finish step the exchange twice a frame.
- `+0x4a2` is never cleared, so every finish unregisters, whether or not the
  callback is still registered.
- The two cloud layers are matched asymmetrically: the first only below its
  top (`<`), the second also at it (`<=`).
- Posting the squawk reminder and the taxi instructions does not wait for the
  message to go out; the other posts do.
- An empty digit field with blanking would write its `'0'` one byte before the
  field. No id ATC formats can produce one; kept anyway.

## Retail faults the harness keeps away from

- **Translation after deinit, or after a re-init with GLOBAL+0x128c clear.**
  Both zero the section table without clearing "loaded", so the next
  translated message reads a null section. The host inits once and deinits
  at exit, so the game never does this. The harness does the same.
- **A cloud base of 0xffff** that is not overcast: the next search starts at
  base+1, which wraps to 0, and the composer loops for ever.
- **A LocaleID that `sscanf("%x")` cannot parse** leaves the locale
  uninitialised on retail's stack. The C starts it at 0; the fixtures always
  parse.
- The formatter copies NUMERIC output from six bytes at `[ebp-6]`, so a seven
  byte result would overwrite the saved EBP. Every id ATC uses is fixed-width
  and five characters or fewer, so it cannot happen.

## Verification

`make atcdiff-run` (`tools/difftest/atcdiff.c`) loads retail ATC and the build
into one process. Retail CHAIN, NUMERIC and UTIL are loaded for real; the
host, WINMSG and WEATHERM are stubs. The host stub allocates from a fixed
arena, so heap pointers match between the sides. The WEATHERM stub re-rolls
the weather in GLOBAL each time it is asked, and the WINMSG stub answers
"still up?" from a scripted stream. Eight real ATIS.XLT files are written
under `xlt/`. Each scenario is a random script of ATIS records, takeoffs,
landings, frames, clears, transponder changes and GLOBAL pokes, run once
against each build from its as-loaded image. Compared: the transcript of
every import call, GLOBAL, and the whole 0x1a00-byte `.data` image.

```text
covered: shows by slot 1/2/3 = 39921/14374/36323, 24764 translated with 89392 replacements, 2070 section regrows, 603 " <10" ceilings
takeoff steps seen after a tick: 0:44521 1:31363 2:31513 3:11120 4:8761 5:238413 6:29152 7:2151 8:2769 9:14535 10:1128 11:2031 12:1090 13:593
landing steps seen after a tick: 0:241249 1:22849 2:21771 3:7627 4:1690 5:537
20000 scenarios, 0 differ
```

Nineteen deliberate breaks were built one at a time, each into a DLL checked
to differ from the good one. Sixteen are caught. The other three were proven
equivalent rather than assumed:

| break | result |
| --- | --- |
| field copy limit six characters -> five | equivalent: every id ATC formats is at most five characters wide, so the copy always stops on the terminator first |
| metric visibility `>= 10000` -> `> 10000` | equivalent: `v * 160934 / 10000` goes from 9994 (v = 621) to 10010 (v = 622) and never equals 10000 |
| empty blanked field writes at `dst[0]` not `dst[-1]` | equivalent: ids 0x1f, 0x21 and 0x22 never format to an empty string |

Two breaks were missed on the first harness and became fixes to it:

- **The translator's restart position after a mismatch.** The fixture keys
  were mixed case, and keys are compared against upper-cased text, so almost
  nothing matched and a match after a mismatch never happened. The fixtures
  are now mostly upper-case template words, several to a section, with
  prefixes and filler ahead of them.
- **The ceiling's `<= 30` boundary.** Random 16-bit cloud bases never landed
  exactly 30 above the field. The weather stub now puts a base 26 to 34 above
  it a third of the time.
