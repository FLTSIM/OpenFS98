# Flights.DLL

`orig/English/Flights.DLL`, module `0x80000002`, preferred base `0x22310000`, 51,122 bytes of
`.text`, 13 dialog registrations. **Every dialog is transliterated and verified in its
harness, and the native DLL is installed (2026-11-02).** Slices 1 to 10 are the pickers
(Adventures and Challenges), Save Flight, Communications, Create a Flight, Select Aircraft,
the welcome dialog and its two sub-menus, the modeless dialog, Select Flight, Autopilot and
Navigation with its three tab pages: all 13 registrations. Like Help, Sim and Views it is MFC dialogs over the CD's SIMSUI and
MFC42, which stay as dependencies.

## Building and verifying

```sh
make -f tools/flights.mk flights-tables flightsevents    # tables from the retail image, then the harness
make -f tools/flights.mk flightseventsdiff-run                     # 20,000 scenarios, seed 0x5f1a6c2d
make -f tools/flights.mk flightseventsdiff-run SEED=0x98a1e7d3
make -f tools/flights.mk flightseventsdiff-run N=2000 SEED=0x7e51a23b MODE=unwind
make -f tools/flights.mk flights-breaks N=400
make -f tools/flights.mk flights                 # the native DLL, bin/flights-diff/FLIGHTS_ours.dll
python3 -B tools/flights_smoke.py 75            # boot private copies, retail's DLL then ours
make main-dialogs                               # the menus in the game, retail against ours
```

The DLL (`src/flights/flights_runtime.c`, tables from `tools/gen_flights_runtime.py`) is
retail's shape over the CD's SIMSUI and MFC42: the 13 registrations, 16 vtables (60 slots,
61 for a tab page) bound slot by slot to our functions or to the same imports, 16 message
maps, module init/deinit as at `0x22311030`/`0x223110b0`, and DllMain's extension-module
calls. In the shared Makefile it is in `ENGLISH_MODULES`: `make install` puts it in
`English\` (`make flights-install` / `flights-revert` alone), and `make smoke` requires it
to match and load.

In the game (2026-11-02): `tools/flights_smoke.py` boots private copies with retail's DLL
and ours, PASS (27/27 modules, 95 gauges, no error retail's boot does not log).
`make main-dialogs` with ours, which opens Create a Flight, Select Aircraft, Navigation,
Communications and Autopilot among its 19 commands: PASS, every control and its text the
same as retail in the Cancel and the OK pass, 14 of 20 pictures identical as before (the
rest are the clock, Select Aircraft's live preview and the crash's curve).

The drive was then extended to the rest of Flights (`tools/main_dialogs.sh`): Save (`0x1104`,
before the crash, since while a flight loads it opens nothing), Select Flight (`0x1102`), the
welcome dialog (`0x1109`) and its two menus (502 and 504), through them Select Flight and
Challenges again, and the two pickers (`0x1106`, `0x1107`), whose OK starts a flight. In the
OK pass Save's OK with an empty title raises Alerts' "invalid file name" alert, which
ignores `IDCANCEL`; `tools/drive/fsdialogs.c` now lists a dialog that cancel does not close
and presses its first visible button (here OK, 2005). Result, retail MAIN and retail English
DLLs against all ours: PASS, 28 commands, every control and its text the same in both
passes, 23 of 29 pictures identical; the six that move are the clock, live previews and the
crash's curve (`c0300`, `c1101`, `c1201`, `c1301`, `c1502`, `ccrash`). This run found one
bug, in MAIN rather than Flights: Select Flight's block lacked its ninth word (see
[main.md](main.md)). Installed after it, and `make smoke` PASS with every module and all six
English DLLs ours.

`tools/run_flightsdiff.sh` runs everything in its own prefix, `bin/flights-diff/wineprefix`,
behind a lock; nothing touches `game/` or the shared prefix.

## The callers: who opens each dialog, with what

Every caller is now our own code, MAIN's or ADVDRV's, so the descriptors are known from C,
not from pushes at a call site. All go through host `+0x10c`, run dialog (id, descriptor).

| Registry id | Template | Opened by | Descriptor |
| --- | --- | --- | --- |
| `0x1101` Create a Flight | `0x1fe` | MAIN command `0x1101` (`main_create.inc`) | MAIN's block `b` |
| `0x1102` Select Flight | `0x208` | MAIN (`main_flights.inc`) | `block` |
| `0x1104` Save Flight | `0x207` | MAIN (`main_flights.inc`) | `.data` `0x208035e8` |
| `0x1107` Challenges | `0x201` | MAIN command **`0x1106`** (`command_1106`) | picker, below |
| `0x1108` Adventures | `0x200` | ADVDRV `+0x28` = MAIN command **`0x1107`** (`picker`) | picker, below |
| `0x1109` opening menu | `0x206` | MAIN `+0x28` (`main_start.inc`) | `b` |
| `0x11090001` learning menu | `0x209` | MAIN (`main_start.inc`) | `b` |
| `0x11090002` flight-selection menu | `0x1ff` | MAIN (`main_start.inc`) | `b` |
| `0x1201` Select Aircraft | `0x1fb` | MAIN command `0x1201` (`main_aircraft.inc`) | `ACFT_DLG` |
| `0x1202` Navigation / radios | `0x205` | MAIN command `0x1202` (`main_radiodlg.inc`) | `RD_DLG` |
| `0x1203` Communications | `0x1fd` | MAIN command `0x1203` (`main_dialogs.inc`) | `.data` `0x208001a0` |
| `0x1204` Autopilot | `0x1fc` | MAIN command `0x1204` (`main_limits.inc`) | `LM_DLG` |
| `1` modeless, flags 4 | `0x20a` | **no caller** in any module we have | - |

The menu command numbers and the dialog ids are off by one for the pickers: command
`0x1106` opens Challenges (`0x1107`), command `0x1107` opens Adventures (`0x1108`), and
Help's Lessons (`0x1106`) is command `0x1105`.

**The picker descriptor**, shared by both: answer `+0`, a word `+4`, count `+8`, selection
`+0xc`, list `+0x10` of `0xa08`-byte records (title `+0`, description `+0x100`, path
`+0x900`). MAIN stores a callback at `+4` and ADVDRV leaves it uninitialised; **neither
dialog reads `+4`**, so ADVDRV's garbage is harmless. It is the same layout as Help's
Lessons.

## The classes

Each factory allocates through SIMSUI `ui_new`, constructs, stores the descriptor and
Flights' instance (`+0x80`), creates from the template and shows; the object's HWND
(`+0x20`) is the result. Constructors and destructors are listed with their RVAs; vtables
are in `.rdata`. In `.text` order:

| Id | Constructor | Code until the next | Message map |
| --- | --- | --- | --- |
| `0x1201` Select Aircraft | `0x18a0` | 2,736 | 6 entries |
| `0x1101` Create | `0x2350` | 2,240 | 8 |
| `0x1108` Adventures | `0x2c10` | 1,392 | 4 |
| `0x1204` Autopilot | `0x3180` | 7,808 | 11 (9 buttons, help) |
| `0x1107` Challenges | `0x5000` | 1,440 | 4 |
| `0x1203` Communications | `0x55a0` | 2,496 | 5 |
| `0x1104` Save | `0x5f60` | 1,776 | 2 |
| `0x1102` Select Flight | `0x6650` | 2,544 | 5 |
| (Navigation's EFIS page) | `0x7040` | 3,392 | 8 |
| `0x1202` Navigation | `0x7d80` | 2,016 | 3 |
| (Navigation's NAV 1 and NAV 2 page) | `0x8560` | 3,888 | 2 |
| (Navigation's transponder and ADF page) | `0x9490` | 2,176 | 2 |
| `1` modeless | `0x9d10` | 304 | 3 |
| `0x1109` opening menu | `0x9e40` | 2,960 | 10 |
| `0x11090002` flight menu | `0xa9d0` | 2,160 | 7 |
| `0x11090001` learning menu | `0xb240` | | 7 |

Module init (`0x1030`) registers the 13 factories through host `+0x298`, loads
`dialog.dll` (the skins and button art, kept at `.data 0x22322134`) and deinit
(`0x10b0`) frees it. `fs98.hlp`, the context-help file, is a string in `.data`
(`0x223220fc`). Shared by every class: `0x2310`/`0x2320` (empty), `0x2330`/`0x2340`
(disable/enable the window), `0x26a0` (delete through the deleting destructor),
`0x7fd0` (CSimsDialog's message map).

## Slice 1: the pickers, Adventures `0x1108` and Challenges `0x1107`

`src/flights/flights_events.c`, `flights_objects.c`; tables generated by
`tools/gen_flights_tables.py`. The two classes have the same members (two texts, a list,
an edit, two 3-piece buttons, text art, `0x15e4` bytes) and share their selection change
(`0x3040`), OnCancel (`0x5240`), selection sync (`0x54c0`), error check and descriptor
check. What differs, and what the transliteration keeps apart:

- **Populate.** Adventures selects the saved entry only when it is in range (unsigned) and
  then shows the description regardless; Challenges selects the saved entry whenever the
  list has any entries, in range or not, and shows it only then.
- **OK with nothing selected.** Adventures cancels through the virtual OnCancel (`+0xd0`),
  Challenges inline.
- The skin (`0x13b`, `0x13c`), the template, the DDX ids of the title and art, and the
  context-help tables.

Retail quirks reproduced: the error check `0x5580` always returns zero, so each OnOK's
second call to it is unreachable; the descriptor and vtable are read before the state
writes in OnCancel; the list is filled with the descriptor reloaded after every message.

**Verified** by `tools/difftest/flightseventsdiff.c`, Help's method: retail Flights.DLL
mapped without its imports, each import slot the slice uses pointed at a logging stub, and
36 operations (every event, init, help, context and DDX routine, the shared helpers, and
each class's construct, destroy, delete, factory and round trips) run on both sides from
one randomised fixture. Stubs swap the descriptor, shrink the list, replace the vtable and
change the list's window mid-call; every vtable slot logs, so a wrong slot differs rather
than faults. Factories see failed allocations (the store through null is compared as an
outcome).

Results, current for slices 1 to 10 together (286 operations):

| Run | Result |
| --- | --- |
| seed `0x5f1a6c2d` | 20,000 scenarios, 5,720,000 comparisons, 0 differ |
| seed `0x98a1e7d3` | 20,000 scenarios, 5,720,000 comparisons, 0 differ |
| unwind, seed `0x7e51a23b` | 2,000 scenarios, 6,662,582 comparisons with a throw at every call position (6,087,313 throws), 0 differ |
| `tools/flights_breaks.py 400` | 502 of 502 breaks caught; four more proven equivalent, listed in the runner |

The 502 were caught over three runs while Navigation's harness grew: 472 in one full run, then
the 28 OnInitDialog and page-allocator breaks, then the two that needed a harness or break
fix (a report given no failure, and a break that did not build).

The stubs disturb the fixture at two calls per operation, one among the first four and one
anywhere in the first 32; the replacement vtable's every slot, and each descriptor's
callbacks, log under their own names, so a call through the wrong table or a stale
descriptor shows.

What the harness cannot see is the shape of retail's vtables: its stand-in has a logging
stub in all 64 slots. The first in-game run of the DLL found the gap. Select Aircraft
calls its own Populate through `+0xf0`, a 61st slot its class adds, and the runtime had
given every dialog CSimsDialog's 60, so the game called through null. The runtime's slot
counts are now per class (`tools/gen_flights_runtime.py`), checked against the image.

A fault inside an operation (an access violation or a bad instruction) is logged as an
outcome and the run goes on, so a break that crashes one side only is a difference; only
fault addresses in the first page are logged, since the rest are where each side's code or
stack happens to be.

Not covered yet: a control-level UI harness (`tools/flightsuidiff`, as Help's and Sim's);
the modeless dialog, which no module we have calls; Save with a real title (which writes a
flight); and flying a flight started from the pickers by hand.

## Slice 2: Save Flight `0x1104`

`src/flights/flights_save.c`; class `0x5f60`, `0x17d0` bytes, descriptor at `+0x17cc`
(caller: MAIN's Linkage `+0xac`, `flight_save_dialog`). The descriptor: answer `+0`,
MAIN's check callback `+4` (`save_dialog_check`, stdcall), name status `+8`, name `+0xc`
with room `+0x10`, description status `+0x1c`, description `+0x20` with room `+0x24`,
"make it the default flight" `+0x30`.

- **OK** reads the vtable once and asks `+0xd8` (validate) then, on a failure, `+0xdc`
  (report it); only an unreported failure keeps the dialog open.
- **Validate** reads both edits and the box into the descriptor and calls MAIN's check. A
  failure names the edit at fault, the name first; with neither status set the check's
  own answer stands.
- **Report** asks through `CSimsDialog::DoErrorDialog`. A name that already exists
  (`0x1004`) passes silently if it is the name already agreed to, kept at `+0x10dc`;
  otherwise the question is asked, and agreeing remembers the name and posts `0x5fa` to
  move to the description, while declining clears it.
- Its DoDataExchange does not call CSimsDialog's; its buttons sit at 0,0 rather than 1,1;
  its init checks no descriptor; Close (`+0xec`) is CSimsDialog's own.

The first harness run for Save passed with whole paths unreached: the remembered name
differed only in scenarios where the status was never `0x1004`, and the stubs' swap point
never fell on the first call when a swap was due. Both selectors now draw on independent
seed bits, and the two descriptors differ in their buffer sizes; nine breaks went from
missed to caught.

## Slice 3: Communications `0x1203`

`src/flights/flights_comms.c`; class `0x55a0`, `0x1fe0` bytes, descriptor at `+0x1fdc`
(caller: MAIN command `0x1203`, `command_1203`). The descriptor: answer `+0`, MAIN's check
`+4` (stdcall; the dialog ignores its answer and reads the status), status `+0xc`, the
frequency text `+0x10` with room `+0x14`, the "slow" box `+0x2c`, the request `+0x34`
(0 none, 1 take off, 2 land), the rate slider `+0x3c` (range 0..14).

- **The request radios** each move the focus to themselves; only "none" leaves the box
  usable, the others clear and disable it. A radio is read as checked when its
  `GWL_USERDATA` equals its own window, as Help's Lessons does.
- **Populate** restores the focus it found (`GetFocus`, then a posted `0x5fa`); a request
  above 2 sets no radio at all.
- **OK** reads the controls before it reads the vtable, unlike Save.
- **Validate** (`+0xd8`) rewrites the frequency to three decimals (`atof`, then `sprintf`
  with `.data`'s `"%.3f"`) only when its *argument* is the edit's id `0xcb3`; OnOK passes 0.
- **Report** (`+0xdc`) gives `DoErrorDialog` the status field's *address* as its code, and
  answers whatever vtable `+0xe0` returns. Both reproduced.

Its first break run missed one break: the "none" request only ever met a cleared box,
because both came from the same low bits of the scenario number. Fixture values that
decide branches now come from independent seed bits.

## Slice 4: Create a Flight `0x1101`

`src/flights/flights_create.c`; class `0x2350`, `0x1c64` bytes, descriptor at `+0x10e0`
(caller: MAIN command `0x1101`, `command_1101`). The descriptor: answer `+0`, callbacks
(stdcall, given the descriptor) Save `+8`, Aircraft `+0xc`, Location `+0x10`, Weather
`+0x14`, Time `+0x18`; the four summaries `+0x1c`..`+0x28`; `+0x2c` set once there is
something to fly, which enables OK and Save.

- Every handler but Save tests for a null descriptor; any callback may be missing. Each
  choice calls its callback and redraws the summaries; Save does not redraw.
- **OnInitDialog answers FALSE**, having put the focus on Select Aircraft itself, and
  clears `+0x10dc` last.
- **Module-static state.** The summaries' font is a function-local static `CSimsFont` in
  `.data` (`0x22322150`, `0x70` bytes, guard bit in `0x223221c0`): built on the first
  OnInitDialog from `GetSimsBaseFont(8)`, with the guard set before it is built, and its
  destructor registered through `__dllonexit`. Every OnInitDialog then copies the dialog's
  colours and sizes into it. The harness keeps this state in its fixture, copies it in
  and out of retail's `.data` around each run, maps retail's address onto the fixture's
  in the logs, and runs OnInitDialog twice in a row to see the guard work.
- WM_PAINT goes straight to MFC's handler (`#2379`).

## Slice 5: Select Aircraft `0x1201`

`src/flights/flights_aircraft.c`; class `0x18a0`, `0x1868` bytes, descriptor at `+0x1864`
(caller: MAIN command `0x1201`, `command_1201`): answer `+0`, `+4` and `+0xc` unused here,
`+8` the details callback (zero means it changed the list), `+0x10` the preview
`(op, descriptor, window)` (1 open with the picture's window, 2 close, 3 draw a frame),
selection `+0x14`, count `+0x18` (signed), and ACONTAIN's 0xa08-byte records `+0x1c`, of
which it shows `+0x100` and `+0x500`, takes the title back from the list, and enables
Details by `+0xa04`.

- Populate, select and Details each hold a local `CString` under its own cleanup frame;
  Details makes one and never uses it.
- OK and Cancel close the preview (op 2) before closing; `WM_USER+100` draws a frame
  (op 3) of the aircraft now selected. The selection is stored even when it is -1, and
  the record at -1 is read: the harness keeps a guard record in front of the list.
- OnInitDialog reads the vtable once, sets `+0x84 = 0xb`, and with a descriptor fills
  the list through vtable `+0xf0`, fits the picture (item `0xc87`) to its remapped frame
  and opens the preview with the picture's window, the callback found through the
  descriptor read before `GetDlgItem`. It answers FALSE, the focus on the list.

## Slice 6: the welcome dialog `0x1109` and the modeless dialog `1`

`src/flights/flights_opening.c`. **The welcome dialog** (class `0x9e40`, `0x1830` bytes,
descriptor `+0x1828`; caller MAIN's `welcome`, at start and for command `0x1109`):
answer `+0`, callbacks More `+8`, Lessons `+0xc`, Create a Flight `+0x10`, Multiplayer
`+0x14`; multiplayer available `+0x18`; quit asked `+0x1c`; "show at start" `+0x20`.

- A menu button runs only if its callback exists and no other is running (`+0x10dc`).
  It disables the other buttons while the callback runs; an accepted choice saves
  "show at start", closes and **stays busy**. A declined one makes all five usable again,
  **Multiplayer included even when it is not available**. Fly (also `WM_CLOSE`) and Quit
  set the answer and the quit flag.
- `WM_USER+100` shows a hint (string resources `0xf6fd`..`0xf700`, through a `CString`
  under its own frame) for the focused shadow button, asking for the focus afresh before
  each comparison, and only when it changes: the button last shown is a module static
  (`0x22322320`), shared by every instance.
- Three function-local static `CSimsFont`s (shadow `0x22322240`, alternate `0x223221c8`,
  hint `0x223222b0`) under one guard byte, bits 1, 2 and 4, each from
  `GetSimsBaseFont(9)` with its own exit destructor. The constructor clears `+0x10dc`.

**The modeless dialog** (class `0x9d10`, `0x10e4` bytes, descriptor `+0x10dc`), which
nothing in FS98 opens: no members, an empty DDX, init answers whether the skin loaded,
`WM_USER+100` closes and destroys it (vtable `+0xec`, `+0x60`, read once) when descriptor
`+8` is clear, `WM_PALETTECHANGED` redraws it, and OK, Cancel and `WM_CLOSE` are the
shared empty function `0x22312b50`. Its help pair at `0x22319e00` belongs to the welcome
dialog's map.

The harness found a store missed on reading (the constructor's clearing of the busy
flag); the breaks found that a random fixture almost never left the busy flag clear, so
the buttons' bodies had not run: it now starts clear three times in four.

## Slice 7: the sub-menus `0x11090002` (flights) and `0x11090001` (learning)

Also `src/flights/flights_opening.c`, which now builds all three menus from shared
helpers: a menu button, the hover hint, and the three static fonts. The welcome dialog
was moved onto them and re-verified. Both sub-menus are `0x165c` bytes, descriptor
`+0x1658` (callers MAIN's `welcome_more_2` and `welcome_more_1`), with title art `0x10e0`,
Back `0x11c8`, shadow buttons `0x12c4`, `0x13b8`, `0x14ac` and the hint `0x15a0`.

- **Flights** (class `0xa9d0`): dual-pilot `+8`, review a challenge `+0x10`, a video
  `+0x14`. A button disables the other two and Back; a declined choice re-enables all four.
- **Learning** (class `0xb240`): lessons `+8`, quit `+0xc`, about `+0x10`. A button
  disables only the other two (Back stays usable), and a declined choice re-enables the
  three and **puts the focus back on its own button**, which the flights menu does not.
- OK, Cancel and `WM_CLOSE` are one routine (`0x2231b920`): answer 0 and close, unless a
  callback is running. OnInitDialog answers TRUE and sets no focus.
- Each has its own three static fonts and hint memory; together they run on in `.data`
  from `0x22322328` to `0x223225dc`, kept by the harness as one block.

The learning menu's focus call was missed on reading (my extraction looked only for the
enable calls) and caught by the first harness run.

## Slice 8: Select Flight `0x1102`

`src/flights/flights_select.c`; class `0x6650`, `0x1810` bytes, descriptor `+0x180c`
(caller MAIN's `flight_dialog`, Linkage `+0xc4` and others): answer `+0`, delete `+8`
(stdcall, given the descriptor), edit `+0xc` (stdcall, given the descriptor and a record),
"make it the default" `+0x10`, selection `+0x14`, count `+0x18`, records `+0x1c`, and
`+0x20`, the entry that may not be edited or deleted. **`+0x20` is past the end of
MAIN's eight-word block**, so in the game it is whatever MAIN's stack holds there; the
dialog reads it and Delete decrements it.

- Each list item carries its record's address as item data (`LB_SETITEMDATA`), and the
  selection is shown through `LB_GETITEMDATA`. The records' base is read once while
  filling the list; a selection out of range falls back to the first entry.
- The title edit and Delete are disabled, and the description made read-only, for a
  record flagged at `+0xa04` and for the protected entry.
- Validate (`+0xd8`) acts only while Delete is usable and only for OK or the title edit:
  it copies the record onto the stack, puts the edited title in, and if the title changed
  hands the copy to the edit callback; otherwise it hands over the description with the
  copy's title cleared. Its answer, `0xcbe`, is withheld for "already exists" when asked
  about the title edit itself. Report (`+0xdc`) asks about an existing title, and on
  agreement saves only the description; anything else is reported, and the answer is
  vtable `+0xe0`'s.
- OnInitDialog sets two edit limits directly in the members (`+0x1644 = 0xff`,
  `+0x15dc = 0x3ff`), fills the list only with a descriptor, and answers FALSE.

The second message map once counted with Select Flight belongs to a `CSimsTabPage`
(template `0x202`, class `0x7040`) built by the Navigation dialog.

## Slice 9: Autopilot `0x1204`

`src/flights/flights_autopilot.c`; class `0x3180`, `0x2178` bytes, 24 members, descriptor
`+0x2174` (caller MAIN command `0x1204`, which MAIN's source calls "limits"; the
descriptor is filled by UTIL's exchange). Its fields are listed in `flights_internal.h`:
active, the heading mode (LVL, ATT, HDG), altitude hold, the speed holds (ASH, MNH), AAT,
TO/GA, the yaw damper, the navigation mode, the five texts with their rooms and statuses,
two labels held in place in the descriptor (`+0x30`, `+0xa8`), and the aircraft's
capabilities at `+0x5c` (2 airspeed hold, 4 Mach hold, 8 vertical speed, `0x10` AAT,
`0x20` TO/GA, 1 ...).

- **Active** cascades: on, each control becomes usable as the other boxes and the
  capabilities allow; off, everything is cleared and disabled and the navigation combo
  set back to entry 0 (`CB_SETCURSEL`).
- Each box turns off the boxes it excludes (LVL, HDG and ATT; ASH, MNH and AAT), redrawing
  each, and opens or closes its edit, refilled from the descriptor. AAT off gives back the
  speed holds the descriptor remembers.
- **Populate**, with no airspeed hold (capability 2 clear), disables the *Mach* controls a
  second time instead of the airspeed ones: retail's, kept.
- **Validate** (`+0xd8`) reads every control back, runs MAIN's check (its answer
  ignored) and names the edit at fault, but **swapped**: an altitude failure answers the
  heading edit's id `0xcb1`, a heading failure the altitude edit's `0xcb0`; with nothing
  at fault it fills the controls again, and inactive nothing is at fault. **Report**
  (`+0xdc`) takes them the right way round, gives the error dialog the status field's
  address, and answers vtable `+0xe0`'s, told the edit's id.
- OnInitDialog limits five edits from the descriptor and answers TRUE.

## Slice 10: Navigation `0x1202` and its three tab pages

`src/flights/flights_navigation.c`. A tab dialog (class `0x7d80`, `0x2178` bytes, template
`0x205`, descriptor `+0x10dc`) over three `CSimsTabPage`s, which have no factories: its
OnInitDialog makes them. The descriptor's layout is in `flights_internal.h`: answer `+0`,
MAIN's check `+4`, and a slice for each page, `+0x10` (NAV 1 and NAV 2), `+0x130`
(transponder and ADF) and `+0x1bc` (EFIS). Each page is given its slice at `+0x1118`, the
whole descriptor at `+0x111c`, and the dialog's buttons.

| Page | Template | Class, vtable | Size | Members |
| --- | --- | --- | --- | --- |
| NAV 1 and NAV 2 | `0x203` | `0x8560`, `0xf518` | `0x244c` | 29: labels, two group boxes, four edits, the localizer combo |
| Transponder and ADF | `0x204` | `0x9490`, `0xf6d8` | `0x1a24` | 14: labels, two edits, a `CSimsStatic` |
| EFIS | `0x202` | `0x7040`, `0xf158` | `0x1b64` | 16: labels, three combos, two boxes, four radios, an edit, a `CString` |

- **OnInitDialog** reads the vtable once, after `CSimsDialog`'s, for both the skin and
  `InitTab` (`+0xe8`, six arguments). Each page is `ui_new`, constructed, stored and given
  `CSimsTabPage::Init(9)` even when `ui_new` failed; a throwing constructor frees its
  block through `CSimsDialog`'s operator delete. The tabs are captioned with
  `LoadStringA` and flagged with each slice's first word. Only then is a missing
  descriptor noticed. The first tab is 2 or 1 when the NAV slice's word is set (2 when
  the ADF slice's is too), and otherwise the descriptor's `+8`.
- **OK** asks each page's validate (`+0xd8`, given 0) and, on a failure, its report
  (`+0xdc`); a report that answers keeps the dialog open. Cancel answers 0 and sets
  `+0x2174` to 2. Close lets the tab control go first (its `+0xc4`). The dialog's own
  validate is a leftover `AfxMessageBox("CNavigation::ValidateData()")` that passes.
- **The pages' validates** do nothing until the page exists (`+0x1100`), read their
  controls back, run MAIN's check with its answer unused, and name the first edit whose
  status is set. NAV's asks `GetDlgItem` for the focus first: an edit (`IsKindOf`
  against `CEdit`'s runtime class, a data import) that is not modified passes at once,
  and one is marked unmodified when nothing failed. EFIS's answer is MAIN's, replaced by
  the altitude edit's id when the altitude's status is set. The reports give the error
  dialog the status's address, as Autopilot's does.
- **EFIS**: the master switch enables everything; the lock radios choose VOR and altitude
  (2, which opens the VOR 1/VOR 2 pair and the altitude edit) or ILS/LOC (4). The page
  reads its radios back by comparing each window's user data with its own handle. Its
  `CString` (the altitude, at most 6 characters) is exchanged with `DDX_Text`.
- **ADF**'s `CSimsStatic` is built inline: MFC's `CWnd()`, then `CStatic`'s vtable at
  once replaced by SIMSUI's (a data import). The first store is never seen and is left
  out. OnInitDialog moves it to the rectangle `RemapCtrlRect` gives.

## Next

1. Fly with it: start flights from the pickers and Select Flight, and save one.
2. Options, the last English dialog DLL.
