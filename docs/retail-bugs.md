# Retail bugs kept on purpose

The reimplementation matches retail, bugs included, because differential testing
against retail is how it is verified ([method.md](method.md)). These are the bugs we know
we are reproducing. **They are to be fixed at porting time**, once comparison with retail
is no longer the test; until then each stays matched, and a fix before then breaks the
comparison that would catch everything else.

Each one is marked in the code with `RETAIL BUG (kept):` and its id here, so
`grep -rn "RETAIL BUG" src/` lists them all. An entry says what goes wrong, when it can
happen, what the fix should be, and where the code is.

New bugs found while reimplementing go here as well as in the module's own doc.

## Options.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| OPT-1 | Video message OnOK, `0x2239e300`; `src/options/options_video.c` | With an empty message, the lead-byte test reads the byte before its buffer (the top byte of the saved ebp), and would clear it if that byte were a lead byte. | Empty message. Harmless while the stack is below 16 MB (that byte is 0). | Skip the lead-byte test when the message is empty. |
| OPT-2 | Logbook entry error report, `0x2239a300`; `src/options/options_logbook.c` | The day and night times, and an instrument time failing other than `0x1013`, are reported with the **address** of their status rather than its value, so the error dialog gets a pointer as its message code. | A bad day, night or instrument time in a logbook row. | Pass the status's value, as the date and remarks already do. |
| OPT-3 | Logbook entry validate, `0x2239a150`; `src/options/options_logbook.c` | The aircraft's name is read with `CB_GETLBTEXT` into an uninitialised stack buffer; with no aircraft picked (`CB_ERR`) nothing is written there and up to 16 bytes of stack garbage are copied into MAIN's block. | Validating a row with no aircraft picked. | Clear the buffer first, or skip the copy on `CB_ERR`. |
| OPT-4 | Logbook entry OnInitDialog, `0x22399d80`; `src/options/options_logbook.c` | The title is loaded with room 0x27 into a 0x18-byte slot of the frame, so a title longer than 0x17 characters overwrites the rest of the frame. | Only if strings `0xf744`/`0xf74f` are translated to something long; the English ones fit. | Give the title a buffer of its declared room. |
| OPT-6 | Logbook OnInitDialog, `0x22398550`; `src/options/options_book.c` | Measures the rows list (its width and a quarter more) into `+0x1af8`, then overwrites it with the constant `0x299` a few instructions later, so the measurement is lost. | Every Logbook. Harmless as far as is known; nothing seen reads `+0x1af8` yet. | Keep one of the two: the measurement if something uses it, else neither. |
| OPT-5 | Add logbook and Logbook entry, "has a descriptor" `0x2239ab20`, `0x2239a630`; `src/options/options_logbook.c` | Each tests its part of MAIN's block (block + 0x28, + 0x54) for null, which it never is; a null block is read through `0x28`/`0x54`. | Only if MAIN passed no block, which it never does. | Test the block, not the part. |

## MAIN.DLL

Only those met while doing the dialogs; the rest are still to be collected (below).

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| MAIN-1 | `lb_button`, `.text+0xf800`; `src/main/main_logbook.inc` | After deleting a logbook row (operation 6) or renaming a logbook (8), a failed write answers success and a successful one answers -1: the test is the wrong way round. | Every row delete and rename; the Logbook ignores the answer to 6, and to 8 shows MAIN's error only when it says failure. | Answer -1 when the write fails. |

## Flights.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| FLT-1 | Select Flight, `+0x20` of its descriptor; `src/flights/flights_select.c` | Reads, and on Delete decrements, a word past the end of MAIN's eight-word block: whatever MAIN's stack holds there. (Our MAIN now provides a ninth word, as retail's frame happens to: see [main.md](modules/main.md).) | Every Select Flight. | Make the word part of the block MAIN defines. |
| FLT-2 | Select Aircraft, `src/flights/flights_aircraft.c` | Stores a selection of -1 and reads the record at -1, before the list. | No aircraft selected. | Ignore -1 rather than index with it. |
| FLT-3 | Factories, `src/flights/flights_objects.c` (and Options' `src/options/options_objects.c`) | A failed allocation leaves the object null and the next store faults. | Out of memory when opening a dialog. | Fail the dialog instead of storing through null. |

Behaviour that is odd but harmless (Navigation's leftover "ValidateData" message box that
always passes, OnOKs whose second check is never reached) is documented in
[flights.md](modules/flights.md) and is not listed as a bug.

## Other modules

Each module's doc records the retail bugs it reproduces. These have a section for them:

- [slew.md](modules/slew.md#two-retail-bugs-reproduced)
- [weatherm.md](modules/weatherm.md#retail-bugs-reproduced)
- [panels.md](modules/panels.md#retails-bugs-kept)
- [fe.md](modules/fe.md#reproduced-retail-bugs)
- [g3d.md](modules/g3d.md), whose rule is to reproduce them; [sound.md](modules/sound.md)
  and [multip.md](modules/multip.md) mention theirs in passing.
- [preflight.md](preflight.md) notes a probable one (two words written over each other).

The rest (MAIN, HOST, UTIL, CONTROLS, DEMO, ANALYSIS and the others) describe their
reproduced quirks in their own docs without a dedicated section. **They have not been
collected here yet**: before porting, go through each module doc and add its entries,
with ids and markers in the code, as above.
