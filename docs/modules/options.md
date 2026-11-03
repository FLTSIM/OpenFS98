# Options.DLL

`orig/English/Options.DLL`, module `0x80000007`, preferred base `0x22390000`, 84,381 bytes of
`.text`, 15 dialog registrations and 31 templates. **In progress, started 2026-11-03:**
slice 1 (the shared helpers, Video finished and Video message) is transliterated and verified
in its harness. Nothing is built as a DLL or installed. Like Flights it is MFC dialogs over
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
- **Retail's bug, kept:** with an empty message, OnOK's lead-byte test looks at the byte
  before its buffer, the top byte of the ebp it saved. A stack below 16 MB (wine's and
  Windows' main threads) makes it 0, no lead byte, so nothing is written there; ours passes
  0, and the harness, whose stack is low too, agrees.

Results (2026-11-03): 31 operations; 20,000 scenarios, 620,000 comparisons, 0 differ on each
of seeds `0x5f1a6c2d` and `0x98a1e7d3`; unwind mode 2,000 scenarios, 290,410 comparisons,
228,046 throws, 0 differ. Breaks: 82, all caught; two proven equivalent and left out (in
`tools/options_breaks.py`'s docstring).

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

1. The rest of the video family: Save video (`0x14040001`) and Flight video (`0x1404`).
2. Then the logbook (`0x1407`, `0x14070001`, `0x14070002`), the analysis dialogs, Instant
   replay, Preferences with Display options and `0x14090002`, and Customise controls last.
3. The native DLL once enough classes exist to bind the module, as Flights' did; then
   main-dialogs, flights-play-style steps and installation.
