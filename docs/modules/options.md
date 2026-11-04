# Options.DLL

`orig/English/Options.DLL`, module `0x80000007`, preferred base `0x22390000`, 84,381 bytes of
`.text`, 15 dialog registrations and 31 templates. **In progress, started 2026-11-03:**
slices 1 to 6 (the shared helpers, the four video dialogs, the three logbook dialogs and
the three analysis dialogs: 10 of 15) are transliterated and verified in their harness. Nothing is built as a DLL or installed. Like Flights it is MFC dialogs over
the CD's SIMSUI and MFC42, which stay as dependencies, and it follows Flights' method
([flights.md](flights.md)): core C over a service table, generated tables, a differential
harness with deliberate breaks, then the native DLL, then the game.

## Building and verifying

```sh
make -f tools/options.mk options-tables optionsevents   # tables from the retail image, then the harness
make -f tools/options.mk optionseventsdiff-run                     # 20,000 scenarios, seed 0x5f1a6c2d
make -f tools/options.mk optionseventsdiff-run SEED=0x98a1e7d3
make -f tools/options.mk optionseventsdiff-run N=2000 SEED=0x7e51a23b MODE=unwind
make -f tools/options.mk options-breaks N=400
```

`tools/run_optionsdiff.sh` runs everything in its own prefix, `bin/options-diff/wineprefix`,
behind a lock. The pieces, each Flights' own adapted:
- `tools/gen_options_tables.py`: from the retail image, each class's context-help table,
  DDX bindings and member controls, checked against its constructor and destructor.
- `src/options/`: `options_internal.h` (the service table), `options_events.c` (shared
  helpers, help and data exchange), `options_objects.c` (construction, destruction and
  the factories, with cleanup-only SEH frames), and one file per family of dialogs.
- `tools/difftest/optionseventsdiff.c`: retail's routines against ours over the same
  logged stubs, as `flightseventsdiff.c` does.
- `tools/options_breaks.py`: src/options/ broken on purpose; only a reported difference
  counts.

## The registrations

From `tools/dialogmap.py` and the factories (`operator new` size, constructor, vtable). The
labels are the static inventory's ([dialogs.md](dialogs.md#optionsdll)); where a caller's
code says otherwise, the code decides.

| Id | Dialog | Template | Factory | Class | Constructor | Vtable | Caller |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `0x1404` | Flight video | `0x3a2` | `0x223911c0` | `0x1a10` | `0x2239c800` | `0x223a8860` | DEMO (recording) |
| `0x1403` | Instant replay | `0x391` | `0x22391130` | `0x1a38` | `0x22396990` | `0x223a73f0` | DEMO `+0x30` |
| `0x1405` | Flight analysis | `0x38e` | `0x223912e0` | `0x1b24` | `0x22394ab0` | `0x223a6e80` | ANALYSIS |
| `0x1407` | Logbook | `0x392` | `0x22391370` | `0x1c00` | `0x223980f0` | `0x223a7ac0` | MAIN command `0x1407` |
| `0x1409` | Preferences | `0x39b` | `0x22391520` | `0x2358` | `0x2239b480` | `0x223a8390` | MAIN (`main_prefdlg.inc`) |
| `0x14070001` | Add logbook | `0x394` | `0x22391400` | `0x1408` | `0x2239a650` | `0x223a7ed8` | MAIN (`main_logbook.inc`) |
| `0x14070002` | Logbook entry | `0x393` | `0x22391490` | `0x1ca4` | `0x22399720` | `0x223a7d70` | MAIN (`main_logbook.inc`) |
| `0x14040001` | Save video | `0x3a1` | `0x22391250` | `0x1900` | `0x2239d510` | `0x223a8950` | DEMO, recording finished |
| `0x14090001` | Display options | `0x385` | `0x223915c0` | `0x21c4` | `0x22393b20` | `0x223a69c0` | MAIN (`main_prefdlg.inc`) |
| `0x14050002` | Landing graph | `0x38a` | `0x22391660` | `0x1724` | `0x22397900` | `0x223a77e8` | ANALYSIS |
| `0x14050003` | Manoeuvre graph | `0x38b` | `0x223916f0` | `0x1444` | `0x2239ab90` | `0x223a8060` | ANALYSIS |
| `0x14090002` | Select demo (MAIN: Flight's aircraft) | `0x39c` | `0x22391780` | `0x1650` | `0x2239c140` | `0x223a8530` | MAIN (`main_prefdlg.inc`) |
| `0x14040003` | Video finished | `0x39f` | `0x22391810` | `0x1290` | `0x2239de00` | `0x223a8ae0` | DEMO `dialog_stopped` |
| `0x14040002` | Video message | `0x3a0` | `0x223918a0` | `0x13fc` | `0x2239e030` | `0x223a8c48` | DEMO `dialog_ask` |
| `0x140a` | Customise controls | `0x3a5` | `0x22391930` | `0x2178` | `0x2239e470` | `0x223a8dc8` | CONTROLS `+0x58` |

Besides these 15 there are 13 more MFC classes, each with a message map no dialog's vtable
reaches: tab pages and controls. Customise controls has the three largest
(`0x2239f050`, `0x223a0ee0`, `0x223a29e0`, about 15 KB, its pages `0x3a3`, `0x3a4`,
`0x3a7`); the others sit between `0x22391d00` and `0x2239b2a0`, near Display options,
Flight analysis, Instant replay, the landing graph, the logbook and Preferences. Laid out by
constructor, each dialog's own code runs from about 0.5 KB (Video finished) to about 8 KB
(Flight analysis), with Customise controls and its pages about 18 KB.

Every dialog's vtable shares five functions: `0x22392400` (empty), `0x22392410` and
`0x22392420` (the window disabled and enabled), `0x223943d0` (empty, one argument) and
`0x22397c30` (delete through the deleting destructor); `0x22392f20` (an error check that
finds nothing) and `0x223943c0` (the descriptor's own check) are called from the classes.

## Slice 1: the shared helpers, Video finished `0x14040003` and Video message `0x14040002`

- **Video finished** (DEMO's `dialog_stopped`, no descriptor): a message and OK. Its
  factory stores no descriptor, unlike every Flights one. OnInitDialog: CSimsDialog's, the
  skin (vtable `+0xe4`, resource `0x14f`, flags `0xf`), OK as a three-piece button, the
  message's text. OnOK closes through vtable `+0xec`.
- **Video message** (DEMO's `dialog_ask` while recording): descriptor answer `+0`, check
  `+4` (stdcall), the message `+8` (0x38 bytes). OnInitDialog sets the edit's limit to 0x37,
  empties it and gives it the focus when there is a descriptor, and answers 0 either way.
  OnOK reads at most 0x36 characters, cuts them to 0x35, drops a lead byte left at the end,
  copies them into the descriptor and is always accepted: a failed check is followed by the
  error check `0x22392f20`, which finds nothing. OnCancel answers 0. The answer state is
  `+0x13f8` (1 accepted, 2 cancelled).
- **Retail's bug, kept** (OPT-1 in [retail-bugs.md](../retail-bugs.md)): with an empty message, OnOK's lead-byte test looks at the byte
  before its buffer, the top byte of the ebp it saved. A stack below 16 MB (wine's and
  Windows' main threads) makes it 0, no lead byte, so nothing is written there; ours passes
  0, and the harness, whose stack is low too, agrees.

Results (2026-11-03): 31 operations; 20,000 scenarios, 620,000 comparisons, 0 differ on each
of seeds `0x5f1a6c2d` and `0x98a1e7d3`; unwind mode 2,000 scenarios, 290,410 comparisons,
228,046 throws, 0 differ. Breaks: 82, all caught; two proven equivalent and left out (in
`tools/options_breaks.py`'s docstring).

## Slice 2: Save video `0x14040001` and Flight video `0x1404`

`src/options/options_videos.c`. Both make a CNotify in OnInitDialog (`ui_new(0x29c)`,
`CNotify(0x30, 1)`, Options' vtable `0x223a6ab0`, whose only own slot is the deleting
destructor `0x22394290`), kept at `+0x10dc` and deleted by their close (`+0xec`): Save
video's is the shared `0x22396d70`, Flight video's `0x2239cb70` also forgets it. If the
constructor throws, the block goes back through `CNotify::operator delete`.

- **Save video** (DEMO when a recording stops, `D+0xed0`): the choice words are `+8` save,
  `+0xc` go on, `+0x10` review, `+0x14` discard, as DEMO's `dialog_finished` reads them.
  Review (`0x1165`) and go on (`0x113b`) set theirs and close. Save (`0x116c`, and OnOK)
  validates and reports through the vtable read once, as Flights' Save Flight does.
  OnCancel asks `0x1011` (style `0xc0`/1) and discards on yes. Validate reads both edits
  into the descriptor and runs DEMO's check; a failure is `0x11a3` (the name) when its
  status is set, else `0x11a0`. The report does nothing while review or go on has the focus
  (GetFocus, FromHandle, each asked again); a name already there passes if it is the one
  agreed to at `+0x10e4`, else asks, and yes remembers it and posts `0x5fa` to move on.
- **Flight video** (DEMO's `video_dialog`): a list of DEMO's 0xa08-byte video records,
  refilled with the count reloaded after every entry. Record, play and delete map onto
  `+0x14`, `+0x10` and DEMO's delete; the rate combo and the loop box go to `+0x20` and
  `+0x24`. Selecting shows the pick's name and title and makes the edits, their label and
  delete usable as the record is writable (`+0xa04`). Validate copies the pick's record,
  puts in the typed name (or, for the title edit `0x11a1`, no name and the typed title)
  and gives it to DEMO's edit. For the name edit alone it skips that when the name is the
  one last seen, which it keeps in .data `0x223ac450` (0x108 bytes) through HOST's
  `_strcmpi` (`+0x2c0`) and `strcpy` (`+0x2a8`), both stdcall.

Results (2026-11-03): 75 operations; 20,000 scenarios, 1,500,000 comparisons, 0 differ on
each seed; unwind mode 2,000 scenarios, 1,130,878 comparisons, 979,704 throws, 0 differ.
Breaks: 187, all caught; three proven equivalent and left out.

Harness lessons: with no selection the pick is -1, so the fixture has a real record before
the list, and its mutation never moves the pick before it (HOST's `strcpy` would copy the
fixture's random bytes past the static's end, and retail's lands in its .data, ours in the
fixture). Each FromHandle call takes its own seed bits, or the second focus test is never
reached alone.

## Slice 3: Add logbook `0x14070001` and Logbook entry `0x14070002`

`src/options/options_logbook.c`. Both are given MAIN's one logbook block
(`main_logbook.inc`) and take their part of it in their "has a descriptor" test: Add
logbook's from `+0x28` into `+0x10e0`, Logbook entry's from `+0x54` into `+0x10dc`. The
part is never null, so the test always passes (a null block reads through `0x28`/`0x54`).
The fields are in `options_internal.h`.

- **Add logbook**: a name edit bound to a CString by DDX_Text (at most 0xf0), a CNotify as
  in slice 2. Its skin's flags are 9, not 0xf. OK reads the name (0x100 whatever its room),
  runs MAIN's check, and on a failure reports it: a name already there passes when `+0x1c`
  is zero; any other failure is reported, and once dismissed the name is selected for
  retyping (`EM_SETSEL 0, -1`).
- **Logbook entry**: a row's date, remarks, day, night and instrument times and its
  aircraft (a combo filled from MAIN's aircraft records, the count reloaded after each). The
  totals row (`+0xa4`) hides the aircraft, shows the row's label, loads title `0xf744`
  instead of `0xf74f`, and leaves only the remarks editable. Validate reads every field and
  the aircraft and names the first field whose status is set, else the check's own answer.
  An instrument time over the total (`0x1013`) is shown in its label while it is reported.
- **Retail's bugs, kept** (OPT-2 to OPT-5 in [retail-bugs.md](../retail-bugs.md)): the day and night times, and an instrument time failing other
  than `0x1013`, are reported with the address of their status rather than its value.
  Validate copies the aircraft's name from a stack buffer that CB_GETLBTEXT does not fill
  when nothing is picked. OnInitDialog loads the title, up to 0x27 characters, into a
  0x18-byte slot of its frame.

Results (2026-11-03): 109 operations; 20,000 scenarios, 2,180,000 comparisons, 0 differ on
each seed; unwind mode 2,000 scenarios, 1,987,881 comparisons, 1,768,345 throws, 0 differ.
Breaks: 261, all caught; four proven equivalent and left out. Harness lessons: CString
stubs keep their text in fixture slots handed out in call order, so the counter is reset
for each side; a branch that needs every status clear is too rare from independent bits,
so one scenario in four clears them all.

## Slice 4: the Logbook `0x1407`

`src/options/options_book.c`. Class at `0x223980f0`, `0x1c00` bytes, template `0x392`,
MAIN's block at `+0x10fc` (its fields are in `options_internal.h`), a CNotify at
`+0x10f8`, the answer state `+0x1af4`.

- **Two lists**: the logbooks (`+0x15bc`), each item tagged with its 0x124-byte record, and
  the chosen logbook's rows (`+0x150c`), each a CString of tab-separated `sprintf` fields
  (`%-16s` the aircraft, `%-24s` the remarks with a second tab when they are shorter than
  three, `%6s` each time) tagged with its 0x4f-byte row; the totals line is string
  `0xf759` and the block's four totals.
- **Every button goes through MAIN's `lb_button`** (block `+0x150`: an operation, an index,
  the block) and then refreshes both lists: new logbook 1, delete it 2, choose it 3, a new
  row 4 (opening with the first row's fields), edit one 5 (with its own), delete one 6,
  choose one 7, rename the logbook 8 (when the title edit loses the focus with a changed
  title). OK and Cancel answer as Video message's do.
- **The rows list is Options' own class**: CSimsListBox with its vtable `0x223a7bb0`
  stored over it in the constructor, a WM_HSCROLL that does nothing and CSimsListBox's
  destructor.
- **Two function-local static CSimsFonts** (.data `0x223ac368` the rows', `0x223ac3d8` the
  totals line's, guard byte `0x223ac448`), built once from `GetSimsBaseFont(3)` and `(1)`
  and destroyed through `atexit`; each OnInitDialog gives them fields of the dialog's own
  fonts and their tab stops (six at `+0x10e0`, four of them from `+0x10e8`).
- **Retail's bug, kept** (OPT-6 in [retail-bugs.md](../retail-bugs.md)): the rows list's
  width and a quarter more is stored at `+0x1af8` and at once overwritten with `0x299`.
  MAIN's `lb_button` has one of its own (MAIN-1).

Results (2026-11-03): 140 operations; 20,000 scenarios, 2,800,000 comparisons, 0 differ on
each seed; unwind mode 2,000 scenarios, 3,292,972 comparisons, 3,011,260 throws, 0 differ.
Breaks: 334 over slices 1 to 4, all caught; seven proven equivalent and left out. Harness
lessons: a list with no selection has to be possible one call at a time, not only
for a whole scenario, or a second list's own test is never reached alone; the static fonts
are copied in and out of retail's .data around each retail run.

## Slice 5: the landing `0x14050002` and manoeuvre `0x14050003` graphs

`src/options/options_graphs.c`. Each finds its graph control (`0x11a8`, `0x11af`), sizes
it to its remapped frame, keeps its window at `+0x10dc`, and lets ANALYSIS draw through
the block's control (`+8`: 0 set up, 1 draw, 2 closing) on WM_USER+100, on OK and Cancel,
and at the end of OnInitDialog. A block that is missing or `0xcdcdcdcd` (MSVC's debug
fill) gets an AfxMessageBox ("Bad m_pliveInfo passed to dialog by core code").
- The **landing** graph also paints (WM_PAINT with a CPaintDC): its scale's two ends
  (`atoi`, `%4d`) and a zero, right-aligned in white beside the graph at a seventh and
  five sixteenths of its height, through the DC's own `SetTextColor` and `TextOut`
  (vtable `+0x38`, `+0x64`) and MFC `#5875`/`#6170`; and the first time, the caption
  (the block's text, two spaces and string `0xf750`) and the touchdown label.
- The **manoeuvre** graph makes its scale's label the first time it draws with one: the
  block's text and string `0xf751` or `0xf750`.
- Their **factories** differ from every other one: no module stored at `+0x80`, and the
  block stored after the frame's end (OPT-7, suspected, in
  [retail-bugs.md](../retail-bugs.md)).

## Slice 6: Flight analysis `0x1405`

`src/options/options_analysis.c`. ANALYSIS's options, filled by UTIL's exchange. An "off"
radio and three mode radios (read as SIMSUI's are: on when the window's user data is its
own handle), two boxes and two combos. Off or the first two modes disable the six
controls that depend on the third; the third enables them. OK reads everything into the
block and runs the check; the first failing field is reported on its control and, once
dismissed, takes the focus (the mode's is reported against no control). Its descriptor
test `0x223957e0` is shared with Display options, Preferences and Customise controls.

Results for slices 1 to 6 (2026-11-03): 193 operations; 20,000 scenarios, 3,860,000
comparisons, 0 differ on each seed; unwind mode 2,000 scenarios, 4,220,157 comparisons,
3,831,901 throws, 0 differ. Breaks: 431, all caught; nine proven equivalent and left out.
Harness lessons: a window rectangle that is never upside down hides a signed division;
`sprintf`'s hook tells `%d` from `%s` by the format.

## Slice 7: Instant replay `0x1403`

`src/options/options_replay.c`. DEMO's replay dialog: the length and the speed in two edits,
each with a spin button, and a loop box. While a replay is already running everything is
disabled and the caption is string `0xf757` (else `0xf752`). OK runs the descriptor's
check, then reports the length's failure (against its maximum) or the speed's (against
100) on its edit, with the value as text in an error block in DEMO's block.
- Its two **spin buttons** are members built inline: MFC `CWnd()` (`#567`), Options' own
  `CSpinButtonCtrl` vtable stored and at once replaced by SIMSUI's `CSimsSpinButtonCtrl`
  vtable (a data import, `0x223ad6c4`). The destructor stores that vtable back and calls
  `~CWnd` (`#793`); an unwind destroys them through `~CSimsSpinButtonCtrl` instead, so the
  object code keeps a separate unwind entry that tells the two apart.
- The speed is also bound by **DDX as an int** (`0x1a34`, DDV 0..100), which the
  constructor zeroes.
- The spin buttons are given their positions masked to 16 bits; the fixture's lengths and
  speeds pass 16 bits now and then, or the masks go untested.
- Its error report leaves the address of its own stack buffer in DEMO's block (OPT-8 in
  [retail-bugs.md](../retail-bugs.md)); the harness compares those words as "a stack
  address" rather than by value.

Results for slices 1 to 7 (2026-11-03): 210 operations; 20,000 scenarios, 4,200,000
comparisons, 0 differ on each seed; unwind mode 2,000 scenarios, 4,659,862 comparisons,
4,237,437 throws, 0 differ. Breaks: 476, all caught; nine proven equivalent and left out.

## Slice 8: Select demo `0x14090002`

`src/options/options_demo.c`. Class `CSelectDemo`; MAIN's `pf_aircraft`
([main_prefdlg.inc](../../src/main/main_prefdlg.inc)) opens it for General Preferences'
default aircraft, with a block of seven words: the answer, three callbacks that do
nothing, the pick (always 0), the count and the 0xa08-byte records (the name `+0`, the
path `+0x900`). The list holds the records' names, each item tagged with its record; a
pick shows the list's text in one edit and the tagged record's text in the other. OK puts
the pick back in the block.
- Its descriptor test is its own: a block that is null or `0xcdcdcdcd` is reported through
  the CNotify (`CNotify::Send`, a cdecl variadic: "In CSelectDemo::InitializeData", "Bad
  m_pliveInfo = %lx\npassed to dialog by core code") instead of a message box.
- The buttons are placed at 0,0 rather than 1,1 as everywhere else, and the edits have
  no label.
- With nothing picked, the pick's handler shows `LB_ERR` as a text pointer (OPT-9).

RESULTS_SLICE_8

## Slice 9: Display options `0x14090001` and its four pages

`src/options/options_display.c`. MAIN's `pf_display` (General Preferences' Display button)
opens it with its preferences block. It is the first dialog with tab pages: a
`CSimsTabCtrl` (`0x12dc`), four `CSimsTabCtrlHelper`s, and four `CSimsTabPage` classes
that OnInitDialog makes itself (`ui_new`, the page's constructor given the dialog,
`CSimsTabPage::Init(9)`), gives each its part of the block (`+0x1118`), and adds to the
tab control with its title (strings `0xf75a`, `0xf760`, `0xf762`, `0xf765`) and a word of
the block. The pages are classes like the dialogs (generated members, help, DDX), built on
`CSimsTabPage` rather than `CSimsDialog`, with no factory:

| Page | Template | Class | Part | What it has |
| --- | --- | --- | --- | --- |
| Aircraft | `0x386` | `0x22392aa0`, `0x168c` | `+0xc` | four check boxes (of six members) |
| Map | `0x387` | `0x22392f30`, `0x13a4` | `+0x38` | a combo and a check box |
| Scenery | `0x388` | `0x223943e0`, `0x19ac` | `+0x4c` | ten check boxes; one enables two others |
| Hardware Acceleration | `0x3a6` | `0x22393240`, `0x18ac` | `+0xa0` | MAIN's driver block: drivers and modes combos, Refresh, acceleration and three boxes that need it |

- A page reads its controls into its part only once it has been shown (`+0x1100`), so OK
  keeps the block's values for a page never opened.
- The Hardware page calls MAIN's three stdcall callbacks in its part (list the drivers,
  list the chosen driver's modes, refresh), fills its combos from MAIN's null-ended name
  lists up to their room, and counts every change at part `+0x14` (MAIN applies the
  driver and mode only if it is nonzero).
- OK with Return held (`GetAsyncKeyState`'s low bit) moves the focus to OK first; the
  pages' error checks are all Options' empty one, so OK fails only on the block's check.
- Unlike the dialogs before it, Display options declares its destructor (it stores its
  vtable first), its factory shows "CDisplayOptions::Create() failed" in a message box when
  `Create` fails, and the vtable it reads for the skin (`+0xe4`) is used again for the frame
  (`+0xe8`, six arguments).
- Page `0x386`'s message map is filled in at run time by a static initialiser (it holds
  `CSimsTabPage::OnActivate`, an import); nothing to transliterate there.
- A failed page allocation is stored and `Init` called on it all the same (FLT-3).

RESULTS_SLICE_9

## Slice 10: Preferences `0x1409` and its six pages

`src/options/options_prefs.c`. MAIN's General Preferences (`main_prefdlg.inc`), built as
Display options is, with a Defaults button and six pages:

| Page | Template | Class | Part | What it has |
| --- | --- | --- | --- | --- |
| General | `0x398` | `0x223957f0`, `0x1c4c` | `+0x10` | three combos, seven boxes, two buttons for MAIN's default aircraft and flight; a real error check |
| Instrument | `0x399` | `0x22397500`, `0x150c` | `+0x6c` | five boxes |
| International | `0x39a` | `0x223963f0`, `0x1714` | `+0x98` | three combos (two stand for codes 1/0 and 2/3), a box hidden unless the part asks for it |
| Display | `0x396` | `0x22392430`, `0x16ec` | `+0xc0` | MAIN's presets (0x80-byte names), two combos, a box and the Display options button; any change drops the preset |
| Sound | `0x395` | `0x223919d0`, `0x2dbc` | `+0xf8` | five boxes, each with a `CSimsSlider` (1 to 100) |
| Flight Options | `0x397` | `0x2239b0e0`, `0x14f8` | `+0x124` | a group and four boxes |

- Pages are given more than their part (`+0x1118`): the block, OK and Cancel at `+0x111c`
  on, in an order of each page's own; Sound's part is at `+0x111c`.
- **Defaults** (`0x1131`) records the tab shown (when `GetActiveTab` gives 0..5), calls
  MAIN's defaults (block `+0xc`; a null or `0xcdcdcdcd` one gets a message box instead), then
  fills again every page that has been shown.
- **General's error check** (the only real one among the pages): the first field whose
  status is set is reported on its box; once dismissed, its tab is shown, the box takes
  the focus and its text is selected.
- **Sound's volumes** are doubles, a slider's position times 0.01, negative while its box is
  off (x87: `fild`, `fmul` by the double 0.01, `fimul` by ±1, `fstp`). Filling a slider takes
  the volume's magnitude times 100 through MSVCRT `_ftol`, which truncates to 64 bits and
  keeps the low 32, so a huge volume wraps rather than saturating. Two things a C build has
  to get right: under `-std=c99` on x87 GCC evaluates a literal `0.01` in long double, so the
  constants are `static const double`s as in retail's `.rdata`; and the conversion goes
  through `int64_t` as `_ftol` does.
- Preferences declares its destructor (vtable stored first) and its factory shows
  "CPreferences::Create() failed", as Display options' do.
- The General page gives its third combo the first combo's label (OPT-10, suspected).

RESULTS_SLICE_10

## Customise controls, for when it comes

Read [CONTROLS' key-assignment contract](controls.md#the-key-assignment-dialog-0x58-text0x7e400x8e5f-0x9130),
`src/controls/controls_input.c` and `tools/difftest/controlsdiff.c`, especially `h_run_dialog`
and its adjacent dialog stand-in. The descriptor is built at CONTROLS `D+0x240`; callback
`+0x1c` resets sensitivity and `+0x20` recounts joysticks. Callback `+0x2c` is tested by
CONTROLS' harness but is not called by retail Options: preserve the real call boundary
rather than adding it. That stand-in verifies CONTROLS, not Options' page construction,
focus and key capture, widgets, validation or nested lifetimes; those come from Options,
with their own fixtures.

## Next

1. Instant replay (`0x1403`), Preferences with Display options and `0x14090002`, and Customise controls last.
2. The native DLL once enough classes exist to bind the module, as Flights' did; then
   main-dialogs, flights-play-style steps and installation.
