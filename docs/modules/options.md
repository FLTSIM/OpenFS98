# Options.DLL

`orig/English/Options.DLL`, module `0x80000007`, preferred base `0x22390000`, 84,381 bytes of
`.text`, 15 dialog registrations and 31 templates. **In progress, started 2026-11-03:**
slices 1 to 4 (the shared helpers, the four video dialogs and the three logbook dialogs)
are transliterated and verified in their harness. Nothing is built as a DLL or installed. Like Flights it is MFC dialogs over
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

1. The analysis dialogs (`0x1405`, `0x14050002`, `0x14050003`), Instant replay, Preferences with Display options and `0x14090002`, and Customise controls last.
2. The native DLL once enough classes exist to bind the module, as Flights' did; then
   main-dialogs, flights-play-style steps and installation.
