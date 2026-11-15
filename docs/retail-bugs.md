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

**Collected 2026-11-14.** Every module doc was read and its reproduced bugs added, below
the sections that were already here (Gauges, Options, SIMSUI, MAIN's first entry, Flights,
SIM1, the crash MDLs, the runtime). The entries added then (every id in the sections from
"FLTSIM98.EXE services" on, MAIN-2 on and FLT-4 on) carry the module doc's account and
the offsets it gives, and **are not yet marked in the code**: the code has its own
comments (often `QUIRK` or "retail") where it reproduces them. Marking each with its id,
and checking each entry against the code, is the next step. Entries marked *(suspected)*
are a reading of the disassembly no game run has shown; *(fixed, not kept)* were fixed on
purpose because their only outcome is a crash. Faults that depend on stack garbage,
unchecked lengths or a wild program are listed even where the harness keeps clear of them:
the port should not inherit them.

## Gauges

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| GAUGE-COMPASS-WRAP-HANG | `SCHCMP.GAU` and `SOPCOMP.GAU`, RVA `0x1020`; `src/gauges/schcmp.c`, `src/gauges/sopcomp.c` | Wraps headings by repeatedly subtracting 360 or -360. Infinity and NaN do not progress; sufficiently large finite values also stop changing at the current x87 precision. The callback never returns. | Non-finite or extremely large heading values. Ordinary bounded headings return normally. | Reject non-finite input and use a bounded remainder operation, retaining the historical endpoint convention. |
| GAUGE-TURN-EMPTY-RETURN | `BELLTURN`, `CESSTURN`, `EXTURN`, RVA `0x1020`; `src/gauges/bellturn.c` and the others, through `GAUGE_MAYBE_FP` (`src/gauges/gauge.h`) | The floating-point selector returns without putting a value on the x87 stack for selectors other than 0 and 1. With an empty incoming stack, the caller's store produces the x87 indefinite NaN and stack-fault status. | An invalid turn-indicator selector. The normal two selector values return their constants. | Define a fallback value and return it explicitly. |
| PANEL-ZOOM-ADDRESS | PANELS `0x10008070`, variable `0x151`; `src/panels/panels.c` | For a non-map view mode past 4, scales the MODULE_VAR's own address by 1/64 instead of a zoom field. Its numeric/text result consequently changes when a gauge is relocated. | An unsupported view mode. The whole-panel test verifies the raw address-derived result, then canonicalizes the address for the relocated-gauge comparison; runtime behavior remains unchanged. | Supply a defined zoom fallback from the view rather than the argument slot. |

## Options.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| OPT-1 | Video message OnOK, `0x2239e300`; `src/options/options_video.c` | With an empty message, the lead-byte test reads the byte before its buffer (the top byte of the saved ebp), and would clear it if that byte were a lead byte. | Empty message. Harmless while the stack is below 16 MB (that byte is 0). | Skip the lead-byte test when the message is empty. |
| OPT-2 | Logbook entry error report, `0x2239a300`; `src/options/options_logbook.c` | The day and night times, and an instrument time failing other than `0x1013`, are reported with the **address** of their status rather than its value, so the error dialog gets a pointer as its message code. | A bad day, night or instrument time in a logbook row. | Pass the status's value, as the date and remarks already do. |
| OPT-3 | Logbook entry validate, `0x2239a150`; `src/options/options_logbook.c` | The aircraft's name is read with `CB_GETLBTEXT` into an uninitialised stack buffer; with no aircraft picked (`CB_ERR`) nothing is written there and up to 16 bytes of stack garbage are copied into MAIN's block. | Validating a row with no aircraft picked. | Clear the buffer first, or skip the copy on `CB_ERR`. |
| OPT-4 | Logbook entry OnInitDialog, `0x22399d80`; `src/options/options_logbook.c` | The title is loaded with room 0x27 into a 0x18-byte slot of the frame, so a title longer than 0x17 characters overwrites the rest of the frame. | Only if strings `0xf744`/`0xf74f` are translated to something long; the English ones fit. | Give the title a buffer of its declared room. |
| OPT-6 | Logbook OnInitDialog, `0x22398550`; `src/options/options_book.c` | Measures the rows list (its width and a quarter more) into `+0x1af8`, then overwrites it with the constant `0x299` a few instructions later, so the measurement is lost. | Every Logbook. Harmless as far as is known; nothing seen reads `+0x1af8` yet. | Keep one of the two: the measurement if something uses it, else neither. |
| OPT-7 (suspected) | The landing and manoeuvre graphs' factories, `0x22391660`, `0x223916f0`; `src/options/options_objects.c` | Unlike every other factory, they never store Options' instance at `+0x80`, yet both graphs load their captions (strings `0xf750`, `0xf751`) with `LoadStringA(+0x80)`, so the strings come from whatever instance CSimsDialog's constructor left there. If that is not Options', the captions come out empty. | Every landing and manoeuvre analysis. Not yet seen in the game. | Store the instance as the other factories do. Confirm in the game first. |
| OPT-8 | Instant replay's error report, `0x223973d0`; `src/options/options_replay.c` | Puts the address of its own stack buffer (the failing value as text) into DEMO's block (`+0x1c` or `+0x38`) for the error dialog, and leaves it there when it returns: a dangling pointer in DEMO's block. | A length or speed out of range. Harmless as far as is known: nothing reads it afterwards. | Clear the pointer after the dialog, or keep the text in the block. |
| OPT-9 | Select demo's LBN_SELCHANGE, `0x2239c5a0`; `src/options/options_demo.c` | Shows the picked item's data as a text pointer without checking for a pick: with nothing picked `LB_GETITEMDATA(-1)` answers `LB_ERR`, and `0xffffffff` goes to `SetWindowTextA` as the text (and `CListBox::GetText(-1)` sizes a CString by `LB_ERR`). OnInitDialog runs it after `LB_SETCURSEL` with MAIN's pick. | Nothing picked: MAIN's pick (always 0, `pf_aircraft`) past the list, which takes an empty aircraft list. Not seen. | Skip both texts when the pick is `LB_ERR`. |
| OPT-10 (suspected) | Preferences' General page OnInitDialog, `0x22395d1c`; `src/options/options_prefs.c` | The third combo (`0x155c`) is initialised with the first combo's label (`0x11f0`); its own label (`0x1360`) is initialised but tied to nothing. | Every Preferences. What SIMSUI does with a combo's label (greying it with the combo, its accelerator) decides whether anything shows; not yet seen. | Give the combo its own label. Confirm in the game first. |
| OPT-11 | Customise controls' Sensitivities page, choosing the axes combo's row, `0x223a0490`; `src/options/options_controls.c` | After counting the bound axes up to the one chosen, it checks the chosen one is still bound before keeping its row. For the mouse it tests the keyboard's chosen axis (`+0x2578`) instead of the mouse's (`+0x257c`); for a joystick it tests the binding after the chosen axis, or the one past the end of the list when the chosen axis is not the last. A wrong answer moves the combo to the first axis, or keeps a row whose axis was unbound; past the end it reads whatever follows CONTROLS' binding array. | After the Keys page changes a binding (registered message `0x5f9`) with the mouse or a joystick chosen. | Test the chosen axis's own flag and binding. |
| OPT-5 | Add logbook and Logbook entry, "has a descriptor" `0x2239ab20`, `0x2239a630`; `src/options/options_logbook.c` | Each tests its part of MAIN's block (block + 0x28, + 0x54) for null, which it never is; a null block is read through `0x28`/`0x54`. | Only if MAIN passed no block, which it never does. | Test the block, not the part. |

## SIMSUI.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| SUI-1 | AddFont, `0x20471770`; `src/simsui/simsui_fonts.c` | Makes the font from three locals (height, charset, weight) that the spec parser sets only on success: when the string resource will not load, or holds no `,` after the face, the font is created from whatever the stack held (within DllMain, what the previous AddFont left). | A missing or malformed font string (`0xf82f`-`0xf84f`). The CD's are all well formed. | Skip the font, or start the locals at defaults. |
| SUI-2 | CDIBPal::InitColorTables, `0x20477810`; `src/simsui/simsui_dib.c` | Ranks each colour by its lightness, (max + min) / 2, to pick one of five of the palette's colours for a palette device; the minimum is taken of red and green and then compared with green again instead of blue, so a dark blue component never lowers the lightness. | Every palette made or given static colours, on a palette (256-colour) display. | Take the minimum with blue. |
| SUI-3 | CDIBSection::LoadDib, `0x20477c40`; `src/simsui/simsui_dibsection.c` | Sizes a 1-bit bitmap's rows as width / 128 bytes (a shift by 7, not 3), so the image is taken as about a sixteenth of its size: too few bytes are copied, and the rest of the section stays as it was. | A 1-bit bitmap resource. None of the CD's dialogs is known to use one. | Shift by 3 ((width + 7) / 8). |
| SUI-4 | CDIBSection::FillColor(CRect, BYTE), `0x20478200`; `src/simsui/simsui_dibsection.c` | Unites the rectangle with the section's own (UnionRect) where it should intersect them, so it fills the whole section, and past it when the rectangle reaches outside (writing out of the bits). | Any call with a rectangle; FillColor(BYTE) passes the whole section, which is harmless. | IntersectRect. |
| SUI-5 | CDIBSection::GetColorIdxNotUsed, `0x20478d40`; `src/simsui/simsui_dibsection.c` | Searches the bits up to and including the byte just past them, so that byte (whatever follows the section's memory) can make a colour count as used. | Every call. | Stop before the end. |
| SUI-6 | CSimsFont::DrawSimsText, `0x20479770`; `src/simsui/simsui_font.c` | Splits the text at tabs and places each piece at the next of the font's tab stops, without checking the count: a text with more tabs than stops reads the word past the last stop as a position. | A tabbed text in a control whose font has fewer stops than the text has tabs. | Stop at the last stop (or draw the rest unsplit). |
| SUI-7 | CSimsFont::DrawSimsText, `0x20479770`; `src/simsui/simsui_font.c` | Chooses the text, shadow and background colours by state 0, 1 or 2 and sets none for any other state, so the drawing uses whatever its locals held (with MSVC's frame, the state itself as the background). | A control drawn with a state outside 0..2; none of the CD's callers is known to pass one. | Treat other states as 0. |
| SUI-8 | CSimsEdit::Init, `0x204790c0`; CSimsComboBox::Init, `0x20474750`; `src/simsui/simsui_edit.c`, `src/simsui/simsui_combo.c` | Places the control from its text's spec "name#l,t,r,b,drop" and sets its height to `drop` without checking that the spec had a fifth number: with four, `drop` is never set. The edit's lives in the stack slot of Init's fourth argument, so it takes that argument's value (ours does the same); the combo box's is a plain local (ours starts it at 0, as a cleared stack would give). | A control whose spec has four numbers. | Default `drop` (or reject the spec). |
| SUI-9 | CSimsSlider::OnMouseMove, `0x2047e480`; `src/simsui/simsui_slider.c` | Clamps the dragged thumb's offset from the track's start to the margin plus the travel, where the travel alone is the end: past the end the thumb can go a margin too far. | Dragging past a slider's right end. | Clamp to the travel. |
| SUI-10 | CSimsSlider::OnSliderClicked, `0x2047eef0`; `src/simsui/simsui_slider.c` | Stepping right past the last position sets the position to the travel less 6 (a pixel count, not a position) and does not move the thumb. | A click on the track after the thumb at the last position. | Clamp to the last position and move the thumb. |
| SUI-11 | CSimsText::GetWindowTextA(char *, int), `0x204819a0`; `src/simsui/simsui_text.c` | Once the text has a font, copies its own text whole into the buffer without looking at the buffer's size. | A caller's buffer shorter than the text. | Copy at most the room less one, and end it. |
| SUI-12 | CDIB::Load(resource id, instance), `0x20476fc0`; `src/simsui/simsui_cdib.c` | Measures a 1-bit row as the width shifted right by 7 (plus one if the width is not a multiple of 8) instead of by 3, so it copies an eighth of a 1-bit bitmap's bits. | Loading a 1-bit bitmap resource into the unexported CDIB class, which nothing in FS98 does (it is creatable only by its runtime class's name). | Shift by 3. |

## MAIN.DLL

MAIN-1 was met while doing the dialogs; MAIN-2 on were collected from [main.md](modules/main.md) on 2026-11-14.

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| MAIN-1 | `lb_button`, `.text+0xf800`; `src/main/main_logbook.inc` | After deleting a logbook row (operation 6) or renaming a logbook (8), a failed write answers success and a successful one answers -1: the test is the wrong way round. | Every row delete and rename; the Logbook ignores the answer to 6, and to 8 shows MAIN's error only when it says failure. | Answer -1 when the write fails. |
| MAIN-2 | Situation record writers, `+0x5c` | Both header writes report into the size argument's own slot; a record's 16-bit length wraps for a tag below 1000 and a body of `0xfffc` bytes or more. | A large record. | Use a 32-bit length. |
| MAIN-3 | The record finder | Trusts each record's length: a length of 0 never moves on (retail spins), a length below the header's own size steps back over the header, a huge one wraps the index, and a file cut short inside a header is read on past the block. The reader also keeps its leftovers: outside a dispatch `+0x54` says there is no record but `+0x58` still reads from where the last record's body got to. | A damaged situation. | Validate every length. |
| MAIN-4 | A situation load | Copies the table entry's length, not the record's: a record of another length leaves the walk out of step with the file (usually faulting at `.text+0x1251b`); `.data+0x41e1` is left naming a file it has closed; ACONTAIN `+0x30` is asked only when GLOBAL `+0x1244` is clear while the aircraft at `.data+0x4217` is loaded whenever it is not empty. | A damaged situation. | Use the record's length; clear the name. |
| MAIN-5 | Neither default flight file present | An alert (UTIL `+0xa4`, string `0xf461`) then a spin at `.text+0x1207c` that never ends. | No default flight. | Exit with the alert. |
| MAIN-6 | The day of the year, `+0x74` | Comes from a table in `.text` (355, 81, 172, 264) indexed by GLOBAL `+0x248` without a bound: past 3 it reads on through MAIN's code and `.rdata`. | A season past 3. | Bound the index. |
| MAIN-7 | A save | Writes record 2 from a buffer on `+0x40`'s stack, whose address goes into its entry at `.data+0x4480` and stays there (a stack pointer in the table). | Every save. | Copy the description. |
| MAIN-8 | `+0x70`, range and bearing | Restores every register, `eax` included, before it returns (the result is the caller's own `eax`); the delta's y-term is only ever squared. | Always. | Return a defined value. |
| MAIN-9 | Calls into FE and UTIL | FE `+0x2c`'s first argument, FE `+0x20`'s state, `+0x74`'s day and UTIL `+0x2c`'s angle are pushed as a word over two stale bytes: only the low word counts; the item ids `+0xae70` pushes to host `+0x254` are pushed over stale upper halves. | Always. | Push the full value. |
| MAIN-10 | The minutes-to-hours divide, the clouds' two `idiv`s, the puff re-centring's `idiv`s, the grid's `idiv`, the shoulder's `div bl`, the word and dword divides of the station dialogs | Run into overflow (or divide by zero) and rely on the host's divide repair (`0x4119a0`, see DIVIDE-DISP8-UNSIGNED). | The cases listed (a wild position, the lowest cloud coverages). | Saturate explicitly. |
| MAIN-11 | The daylight rule, the performance presets | The daylight rule indexes a five-entry table; `PERFORMANCE_MODE` is not bounded: past the twelve entries (or negative) the preset (`.text+0x14d20`) reads on through `.data`. | A bad setting. | Bound the indices. |
| MAIN-12 | The object step, `+0x1a72`'s family | Trusts `n`: at 0 the record shift is 4 GB, past 15 it runs into the buffer pointers; a side word that wraps to exactly 0 counts as no wrap; one local is written as a word on two paths and a dword on the third; `eax` at exit is what the last entry left. | `n` outside 1..15 (`+0x84` only makes 15). | Check `n`. |
| MAIN-13 | The cloud layers' draw, `+0x9d57`..`+0x9f39`, `+0x918b` | `+0x9f39` takes `shl ax,1` / `rol dx,1` after an `imul` (dx's top bit comes round); the texel index is two quotient bytes less the layer's origin bytes, each wrapping on its own; the second-layer test reads the first layer's `+0xe6e`, not `+0xe76`, and compares the position the first test stored. | Every cloud draw. | Compute the 32-bit values. |
| MAIN-14 | The cloud textures' maker | The read's open is never checked (with no `clouds` file the host's read and close of a null file fail quietly); a failed write (open or write) returns with edi still on the texel map so `+0x94` goes on reading GLOBAL's fields through it; a failed write leaves the file open. | No `clouds` file; a failed write. | Check and restore. |
| MAIN-15 | The frame callbacks (list 1 and 3) | The step count for GLOBAL `+0xe60` past 2 is G3D's leftover ecx (not reproduced); the height term in mode 2 is an unsigned 32x32 product whose high dword becomes `.data+0x1924`; the slewed transition raises `.data+0x1888` by a quarter turn and puts it back; the puff re-centring's 48-bit value has its high word twice (`shld`/`shr`); the grid step's distance test is signed (a distance of `0x80000000` counts as falling); the wind's speed keeps bits 8 to 23 and each component bits 15 to 30. | Always. | Use defined widths. |
| MAIN-16 | `+0x50`, replacing a situation record | Is handed host `+0x174` the directory without the drive: for a file on another drive the temporary file goes on the current drive. | A situation on another drive. | Pass the drive. |
| MAIN-17 | Select Flight (`+0xc4`, command `0x1106`) | With no flights `.text+0xc6e0` alerts and returns its block's first word, uninitialised stack; a too-long name gets status `0x100d` but answer `0x1001`; the Save check (`.text+0xd6b0`) goes on testing the path length after host `+0x170` has failed, reading an uninitialised local. | No flights; a long name; a failed path. | Initialise; return the matching answer. |
| MAIN-18 | A wild season index | Can read through `.data+0x35ec` (the module handle and the close button at `.data+0x7fc0`, `+0xdc24`). | A bad season. | Bound it. |
| MAIN-19 | The natives (`+0x88e2`..`+0x9230`) | `+0x88e2` loads `di` before reading GLOBAL `+0xe76` through `edi`, so the read lands at G's upper half, that `di`, plus `0xe76`; `+0x8d47` writes through the `edi` it was called with; `+0x9230` goes on using ecx and edx across G3D `+0x40`. | Always. | Use explicit pointers. |
| MAIN-20 | `+0x197b1` | Compares a record's address with `0x12160` and `0xb4dc` where it meant a distance: an address always passes. NAV2's retune word steps with `inc` where NAV1's is set to 1. | Every radio update. | Compare the distance. |
| MAIN-21 | The Create a Flight dialogs `0x1301`, `0x1204`, `0x1205`, `0x1306`, `0x1307`, `0x1409` | `0x1301`/`0x1204` test GLOBAL `+0xc18` differently (`== 2` for the units shown, `!= 0` for the clamp), so with `+0xc18` at 1 a field is checked against feet and clamped to metres; `0x1205`'s argument slot serves as scratch (a value NUMERIC does not write leaves retail's block address in it); `0x1306`'s cloud sort keeps its locals across passes and compares stale rows; `0x1307` loads the second area from the same GLOBAL fields as the first (`+0x1004`) and only the first two ever go back by name; `0x1409`'s preset comparison inverts the first word (a preset is forgotten only when GLOBAL `+0x11b2` differs and every other word agrees) and a twelfth preset's string overwrites the descriptor beside the table. | The cases listed. | Test the same flag; use separate slots. |
| MAIN-22 | The logbook, `0x1407` (the wrong-way-up write test is MAIN-1) | `+0x3b40` shrinks each time it loads; a failed read leaves the file open; `+0x90` sets the attribute on the directory rather than the path. | The cases listed. | Test correctly; close. |
| MAIN-23 | Dialog fields | A field NUMERIC turned down keeps whatever the slot held (stack contents), in several dialogs (the stand-in keeps fields parseable). | A refused field. | Initialise every slot. |
| MAIN-24 | `+0x1620` and `+0x11b0` (the kneeboard pages) | `+0x1620` hands the host's `+0x318` a RECT whose bottom it never set the first time; `+0x11b0` reads a byte past a row ending in transparent pixels. | The first draw; a row ending in transparent pixels. | Initialise the rectangle; bound the row. |
| MAIN-25 | `+0x1320` (loading a bitmap) | Retail imports no `DeleteObject`: every call leaves its DIB section behind (the process runs out of address space after about 30,000). Ours keeps the leak. | Every call. | Delete the section. |

## Flights.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| FLT-1 | Select Flight, `+0x20` of its descriptor; `src/flights/flights_select.c` | Reads, and on Delete decrements, a word past the end of MAIN's eight-word block: whatever MAIN's stack holds there. (Our MAIN now provides a ninth word, as retail's frame happens to: see [main.md](modules/main.md).) | Every Select Flight. | Make the word part of the block MAIN defines. |
| FLT-2 | Select Aircraft, `src/flights/flights_aircraft.c` | Stores a selection of -1 and reads the record at -1, before the list. | No aircraft selected. | Ignore -1 rather than index with it. |
| FLT-3 | Factories, `src/flights/flights_objects.c` (and Options' `src/options/options_objects.c`, and Display options' pages, `src/options/options_display.c`) | A failed allocation leaves the object null and the next store faults; Display options calls `CSimsTabPage::Init` on a page it failed to allocate. | Out of memory when opening a dialog. | Fail the dialog instead of storing through null. |
| FLT-4 | Each OnOK's error check `0x5580` | Always returns zero, so each OnOK's second call to it is unreachable. | Always. | Return a real status. |
| FLT-5 | Navigation's own validate | Is a leftover `AfxMessageBox("CNavigation::ValidateData()")` that passes. | Every OK on Navigation (documented as harmless in flights.md). | Remove the box. |
| FLT-6 | The pickers' descriptor `+4` | MAIN stores a callback there and ADVDRV leaves it uninitialised; neither dialog reads it. | Always. | Drop the word. |

Behaviour that is odd but harmless (Navigation's leftover "ValidateData" message box that
always passes, OnOKs whose second check is never reached) is listed too (FLT-4 to FLT-6): it
is dead weight to remove at porting, not a defect.

## SIM1.sim

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| SIM1-AIR-SIZE-HANDLE | AIR file loader `0x2200aef3 -> 0x2200af7e`; `src/sim1/sim1_air.inc` | Returns zero without calling HOST `+0x13c`, leaking the already opened file handle and its host block. | The AIR file opens successfully but HOST `+0x1a4` fails to obtain its size. The isolated caller harness confirms that ownership remains open on this exit. | Close the opened file on the size-error path before returning. |
| SIM1-AIR-UNCHECKED-TABLE | AIR interpolation `0x22011660`, unchecked access at `0x22011692`; `src/sim1/sim1_small.c` | Uses `(count-1)*16` to read a table entry without validating the count or table format. | Omitting engine-type record `0x105` from the Bell AIR file selects a table interpretation that faults during postprocessing. Decoder comparisons reproduce the access fault and preceding state in all twelve FP modes. | Validate engine type and interpolation table dimensions before looking up entries. |
| SIM1-SCALE-DISCARDED | Rudder input `0x220051dc`; `src/sim1/sim1_small.c` (`sim1_rudder_input`) | With GLOBAL `+0x278` bit 0 set and `+0x366` clear it scales the rudder value by `0x1999 / 65536` (a tenth), then pops the unscaled word back over the result, so the scaling never takes effect. Our C leaves the dead arithmetic out. | Always, under those two flags. | Decide whether the tenth was meant (a ground-steering reduction?) and apply it, or drop the test. |
| SIM1-GOVERNOR-RPM (suspected) | Propeller governor `0x2200ea69` and its pass `0x2200e744` (the same loop); `src/sim1/sim1_small.c` (`sim1_governor`, `sim1_governor_pass`) | For an engine whose rpm word (scaled by `+0x439a`) is below `+0xac7`, the throttle should keep its value; instead the reused register still holds the scaled rpm, which is clamped to 0..0x3fff and written as the throttle. | An engine under the governor's minimum rpm while the governor is on. | Leave the throttle alone below the threshold. |
| SIM1-AUTOTHROTTLE-ENGINES (suspected) | Autothrottle modes `0x2200e4fa`; `src/sim1/sim1_small.c` (`sim1_autothrottle_modes`) | The governor's engagement test on the ground should take the highest rpm over the engines fitted (`+0xaec`); the count tests are inverted (`jge` for `jl`), so with two or more engines only the first engine's rpm counts, and a single looks at all four rpm words. | The autothrottle armed on the ground outside takeoff mode, multi-engine with an engine other than the first fastest, or a single with stale words for engines 2 to 4. | Test `index < count` for each further engine. |
| SIM1-GEAR-HORN-ENGINES (suspected) | Landing gear `0x220017f0`; `src/sim1/sim1_small.c` (`sim1_landing_gear`) | In the air with a leg not down and the flaps past the approach setting, the horn should sound for any engine's throttle below the closed threshold; a throttle at or above it on engine 2 ends the test (`jge` out of the chain), and with one engine the count tests skip the rest, so engines 3 and 4 are never looked at. | Three or four engines, gear up, flaps partly out, engines 1 and 2 above idle and 3 or 4 closed. | Test each fitted engine's throttle. |
| SIM1-AIR-FRAME-OVERRUN (fixed, not kept) | AIR decoder `0x2200af90`, record 0x302; `src/sim1/sim1_small.c` (`sim1_air_decode`) | The contact stiffness record is copied with its own length into a 0x1c-byte frame buffer: a longer record overruns retail's stack (its saved registers and return address), a shorter one leaves stale frame bytes in the seven words. The C copies at most 0x1c bytes into a zeroed buffer; a stack overrun cannot be reproduced. | A malformed or modified .AIR file with a 0x302 record not 0x1c bytes long. | Check the length. |
| SIM1-SLEW-HEIGHT-EAX | CHAIN tick `0x2200a84b` (`0x2200a8b4`); `src/sim1/sim1_small.c` (`sim1_chain_tick`) | Slewing in the height mode (GLOBAL +0x366), the height's 48-bit position (+0x572, +0x574) is GLOBAL +0x1c:+0x20 shifted right 24 plus +0x120c; retail loads only the high dword into edx, so the `shrd eax,edx,0x18` takes the fraction's low byte from whatever is in eax: the callback's second argument, or after a slew frame the record words' `_ftol` result. | Every slew frame in the height mode: the low 8 bits of the height's fraction word are stale. | Load the position's low dword (presumably GLOBAL +0x1c) into eax first. |

## Crash MDLs

The three crash models (`CESSNA182RRG_C`, `CESSNA182S_C`, `EXTRA300S_C`), all in `src/mdl/mdl_crash.c`; RVAs are C182RG's.

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| CRASH-PITCH-LEFTOVER | Nose-in `0x1b37`; `crash_nose` | Adds the high half of whatever eax holds to the pitch: after the first frame the high word of GLOBAL `+0xd48` (left by the update), in the first frame the high half of UTIL's Linkage address (UTIL `+0x44` preserves eax, which held it for the call), so the first frame depends on where UTIL is loaded. The helper `0x2e19` was evidently meant to return the turn. | Every nose-in crash (pitch 15 to 40 degrees) while its speed word is at least 0x320. | Turn the pitch by a defined amount (probably the decayed speed word). |
| CRASH-HEADING-LEFTOVER | Wing loss `0x1637`; `crash_wing` | Adds the high half of eax to the heading, which there is the high word of the longitude step just made (`0x29b2`'s eax). | A banked crash (10 to 30 degrees) to the left, while it slides. | As above, for the heading. |
| CRASH-PIECE-STOP-FIELD | Debris pass, piece `0x114`; `ZERO_F0` | A grounded piece `0x114` zeroes `0x108`/`0x10c`, the pitch and bank of piece `0xf0`, instead of its own `0x12c`/`0x130`, so it keeps tumbling on the ground and `0xf0` stops. | Every debris crash once piece `0x114` lands. | Zero `0x12c`/`0x130`. |
| CRASH-EXTRA-SKIP | Extra debris pass `0x27a6`; `DEBRIS_SKIP_FROM` | While piece `0x210` flies, the pass jumps on to piece `0x27c`, so pieces `0x234` and `0x258` move only once `0x210` has landed. | Every Extra debris crash. | Go on to the next piece. |
| CRASH-EXTRA-PIECE-2A0 | Extra debris pass `0x296e`, `0x298f`; the `0x2a0` row | Piece `0x2a0` sinks piece `0x234` instead of itself and tumbles `0x234`'s pitch (`0x24c`), so `0x2a0` never falls and `0x234` falls twice. | Every Extra debris crash. | Sink and tumble `0x2a0` (`0x2b8`). |
| CRASH-SLIDE-CBW | Level slide `0x140c`; `crash_slide` | The pitch is computed from the climb word with `cbw`, its low byte sign-extended, not the word. | Every level slide frame. | `cwde` the word (check the scale against the result). |
| CRASH-GROUND-COMPARE | `0x30ef`, `0x316d`; `settle_altitude` | Clamping a settling altitude to the ground compares the whole and the fraction separately and unsigned: a piece below the ground with a larger fraction, or a negative ground, is left where it is. | Pieces and the aircraft settling after a cartwheel. | One signed 48-bit comparison. |
| CRASH-GROUND-OFFSET-CARRY | `0x316d`; `settle_aircraft` | The ground offset GLOBAL `+0x120c` is added to the 48-bit ground without the fraction's carry. | Settling after a cartwheel, by up to one unit. | Carry into the whole part. |

## FLTSIM98.EXE runtime

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| DIVIDE-DISP8-UNSIGNED | Divide repair filter `0x4119a0`; `src/host/runtime/host_rt_divide.c` | Decoding an `idiv` with an 8-bit displacement (`[reg+d8]`, mod 1), it adds the byte zero-extended, so a negative displacement reads the divisor 256 bytes too far on. Only the divisor's sign is used, to choose the saturated quotient. | An `idiv` by zero or into overflow through a memory operand with a negative 8-bit displacement. | Sign-extend the displacement. |
| MODULE-NO-LINKAGE | Adding a module `0x407b20`; `src/host/runtime/host_rt_modules.c` | A DLL in the modules directory (or named in `AutoInitModules`) without a `Linkage` export is loaded, its missing Linkage skipped for the id, then read through null for its registrations: the game faults at startup. | Any such DLL in `MODULES\`. | Skip a DLL without a Linkage (and free it). |
| DIVIDE-BYTE-REGISTER | Divide repair filter `0x4119a0`; `src/host/runtime/host_rt_divide.c` | For a byte `idiv` by a register, registers 4-7 (AH, CH, DH, BH) are read as ESP, EBP, ESI and EDI, whose low byte then decides the quotient's sign. | A byte `idiv` by AH, CH, DH or BH that faults. | Read the high byte of EAX, ECX, EDX or EBX. |
| DIALOG-UNSET-BITMAP | FS98DLG3D's window procedure `0x4058a0`; `src/host/runtime/host_rt_floating.c` | Before a dialog window has its data (GWL_USERDATA, given with message 0x467), the procedure's bitmap pointer is a local never set, and WM_SETFOCUS, WM_PAINT and the button-down messages read through it. | Any of those messages to a 3D dialog window before its 0x467. | Skip them without data. |

## FLTSIM98.EXE services (HOST)

All reproduced in `src/host/` (the offsets are service slots, [host.md](modules/host.md)). The runtime's own bugs are in the section above.

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| HOST-1 | `+0x164` (directory search), an empty directory string | Appends a backslash unless the directory already ends in one, tested by reading `buf[len-1]`: for `""` that is the byte *before* the buffer (the top byte of a heap pointer), so `""` becomes `\` and the search runs from the drive root. | An empty directory. | Test for the empty string. |
| HOST-2 | `+0x184` (join) | Builds in a 520-byte buffer and checks only the result against `MAX_PATH`: fails cleanly on a long result but not on a long input. | A very long input path. | Bound the copy. |
| HOST-3 | `+0x22c` (sort) | Is not reentrant (the comparator is parked in a global) and its cdecl trampoline does not clean up after the comparator, which must therefore be stdcall. | A nested sort. | Pass the comparator through. |
| HOST-4 | `+0x2f8` (list remove) | Always drops the count, even for an index at or past the end (which moves nothing), and a queued removal does the same when applied: more queued removals than elements drive the count negative and the next append writes below the data. | A removal past the end. | Test the index. |
| HOST-5 | The list walk's end | Applies the queue without consulting the depth: a nested walk's end applies the outer walk's removals while it is still running; each is queued again and the queued flag is then cleared over it, so they stay on the queue until a later walk. | Nested walks. | Consult the depth. |
| HOST-6 | `+0x2fc` / `+0x300` / `+0x310` | `+0x2fc` frees the data but leaves the capacity, so a list freed and not re-initialised writes through null on its next append; `+0x300` and `+0x2fc` return what eax held; `+0x310` with a zero length matches the first element. | Reusing a freed list; a zero length. | Reset the capacity; return defined values. |
| HOST-7 | `+0x210` | Its second failure is not fatal: it shows the out-of-memory box and posts the quit, then returns 0 with the pointer null. | An allocation failure. | Make it fatal as `+0x20c`'s is. |
| HOST-8 | `+0x98`/`+0x9c`/`+0xa0` (the option finder) | Tests each `/` or `-` against the byte before it, the first included: reads one byte before `GetCommandLineA`'s buffer. | Every command line. | Test the first character separately. |
| HOST-9 | `+0x1d0`/`+0x1d4` (input names) | Return whatever eax holds (wsprintfA's count, the loader's last length or the last compare); a number past 64 buttons or 16 axes gives 0; the axis prefix is compared from just past a matched button prefix. | Always. | Return a defined value. |
| HOST-10 | `+0x200` (key names) | Passes `len - used - 1` on unchecked: a buffer shorter than the modifier words makes a negative count and strncpy's zero fill runs off the end of memory. | A short buffer. | Check the room. |
| HOST-11 | The profile list readers | Read lists into a block of 15 bytes an int or 30 a double and write into one the same size, unchecked. | A longer list. | Bound the count. |
| HOST-12 | `+0x11c` | Leaks its path copy when `CreateDirectoryA` fails. | A failed create. | Free it. |
| HOST-14 | `+0x2c4` (shorten with "...") | Tests strcpy's result the wrong way round: whenever it actually shortens a string it returns -1 and leaks its working copy. | Every shortening. | Test it correctly. |
| HOST-15 | `+0x150` (free disk space) | Multiplies sectors-per-cluster by bytes-per-sector in 32 bits before widening. | A disk over 4 GB per product. | Widen first. |
| HOST-16 | `+0x208` (fold the keypad) | Turns an extended Enter into `0x87` without setting the folded flag. | Every extended Enter. | Set it. |
| HOST-17 | The main menu tree (`+0x254` etc.) | Finds an item's array through the item's own parent-id field, so an item whose field is wrong, a renumbered parent, the bar itself or an append to an item with no submenu send it running off an array; a removed item's submenu is never freed; `+0x254` rebuilds the bar before redrawing when the parent's own parent id is 0. | A malformed tree. | Check the parent and free the submenu. |
| HOST-18 | The sound voices (`+0x2e4`, `+0x2e8`, `+0x2f0`, `+0x2f4` and the duplicate service) | A duplicate (type 6/7) that fails to get a buffer frees the sound it shares with its source (a dangling pointer); appending to a duplicate does the same; a type 7 duplicate of a type 5 voice (which frees its sound once buffered) copies from null, as does appending to or from such a voice; the services use the buffer (and `+0x2e8` its voice) unchecked; a start that fails at the primary buffer leaves DirectSound set with no primary. | A failed buffer; a duplicate of a released voice. | Reference-count the sound; check the buffer. |
| HOST-19 | `+0x074` and the view-window services | `+0x074` overwrites a record's host block without freeing the old one; `+0x06c` refills the row table only when its first row moved; the locks, `+0x068` and the restack read the host block unchecked. | Every `+0x074` on a record that has a block. | Free the old block; check it. |
| HOST-20 | The tooltip regions | Registers the tools before checking the tree fits; nested nodes grow the outermost bounds rather than their group's; the check reads every top-level node as a group, so a tree whose top level is not all groups scans past the end for a closing 3. | A malformed tree. | Check first. |
| HOST-21 | `+0x318` (text), font kind 0 | Makes a 7-pixel font afresh on every call: a GDI leak. | Every call with kind 0. | Make it once. |
| HOST-22 | The modal dialog loop | A modal dialog pushed over an array of windows reuses the register that held its own array: if both are arrays its loop runs on the one below; a factory that makes nothing returns -1 without releasing the module. | Nested modal arrays; a failed factory. | Keep the arrays apart; release the module. |
| HOST-23 | Joystick read (`+0x1c4`) | The caller's axis mask is narrowed to the device's axes counted as a 16-bit number that wraps when the caps claim fewer axes than flags (a short count wraps to `0xffff`). | A device claiming fewer axes than flags. | Count in 32 bits. |

## UTIL.DLL

All in `src/util/`; the offsets are service slots ([util.md](modules/util.md)).

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| UTIL-1 | `+0x44`, metres to longitude | The first `div` takes a 64-bit numerator and a divisor of about 2^25 and raises #DE if the quotient will not fit in 32 bits: retail crashes outright rather than saturating. (OpenFS98's UTIL handles only a zero divisor and returns a quotient: a known difference, see BST-4.) | A step reaching the circumference of the latitude circle. | Saturate or reject. |
| UTIL-2 | `+0x44`, the latitude index | Walks off the table past roughly +-0x4e of latitude index into whatever sits next in `.data` (the generator emits the whole section so the same bytes are read). | Latitudes near the poles. | Clamp the index. |
| UTIL-3 | `+0x44`, the interpolation | `add eax,eax / adc edx,0` doubles only the low half of the interpolation product and folds its carry upward (`adc edx,edx` was meant): the divisor is slightly different from the interpolation intended, by a unit or two in the low word. | Every call. | Double the 64-bit product. |
| UTIL-4 | `+0x44`, the negative branch | The 64-bit negate is `not ecx / neg eax / sbb edx,-1`: the borrow correction lands on `edx`, the leftover remainder, instead of `ecx`: the high half comes back as `~q` without its `+1`, right only when the low half is non-zero. | A negative step whose low half is zero. | Correct `ecx`. |
| UTIL-6 | `+0x84` | Its shift count reaches `shrd`/`sar` in CL, which x86 masks to five bits: asking for 32 shifts by nothing. | A shift of 32 or more. | Shift by the whole count. |
| UTIL-7 | `+0xbc` / `+0xc0` (a 48-bit position to and from a double) | Convert the integer half back to a double as **unsigned** (the low dword of a quadword whose high dword it has just zeroed): harmless in `+0xbc` (the error, 2^48, cannot reach the stored 16 bits), but in `+0xc0` a negative position becomes a value near 2^32 (`-1.5` becomes `4294967295.5`), so the pair does not round-trip across zero. | A negative position. | Convert signed. |
| UTIL-8 | `+0x80` | Sets its zero result and then runs `cmp eax,1` whose flags nothing reads (dead, not reproduced). | Always. | Delete it. |
| UTIL-9 | `+0x94` (collect files), a file with an unregistered extension | Both loops share one uninitialised entry: the file inherits the previous entry's type (stack garbage in the first iteration; ours starts at 0). With ext `-1`, `a.BGL` and `a.TXT` come back as the same path. | An unknown extension. | Reset per entry. |
| UTIL-10 | `+0xa0` (directory exists) | A directory with a dot in its name is never found (the splitter peels `.BGL` off `dir.BGL` and the comparison is against `dir`); a bare name is checked against the drive root, not the current directory. | Those names. | Compare the whole name; use the current directory. |
| UTIL-11 | The file lister, nothing surviving | Returns 0 without resetting its static list, whose buffer then carries into the next call. | An empty result. | Reset the list. |
| UTIL-12 | `+0x8c` / `+0x90` (the exchange engine) | Its dispatch is one flat table indexed by kind and `subtype - 0x1ff9` without a bound: a subtype of `0x1ffd`..`0x2000` lands on a kind handler; one of `0x1ff9`..`0x1ffc` re-enters its own kind handler until the stack runs out; anything off either end jumps into code (the last two crash retail and are inert in ours). | A bad subtype. | Bound the subtype. |
| UTIL-13 | `+0x8c`'s `mode` | Means "validate" at 1 and only some handlers listen: the width handlers skip their store when mode is exactly 1; the NUMERIC parse writes into a scratch buffer for any non-zero mode; the string and flag handlers always write. Callbacks are told 2 to validate and 1 to commit (the opposite numbering). | Always. | Document and unify. |
| UTIL-14 | `+0x90`'s return | Is garbage on some paths (a bit index, a kind number); all 48 call sites ignore it. | Always. | Return nothing. |
| UTIL-15 | The alert's text builder, `.text+0x2480` | The first piece is mandatory: if it is zero the block pointer is never initialised and the next append writes through stack garbage; the separator is appended *before* the realloc, into the four bytes of slack the last allocation left. | A zero first piece. | Initialise the block. |
| UTIL-16 | The modal alert, `+0xa4` | The 24-byte descriptor's answer field is never initialised: an alert that closes without answering returns whatever was on the stack. | An alert closed without an answer. | Initialise it. |
| UTIL-17 | The modeless alert, `+0xa8` | The 0x28-byte object is stored through `out` before it is filled; the layout differs from the modal one and sixteen bytes in between are left as malloc returned them. | Every modeless alert. | Initialise the object. |

## CHAIN.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| CHAIN-1 | A callback's registers | Callbacks are entered with eax = the list's block, ecx = the dispatch's first argument, edx = the list's depth after the increment (then what the callback before left in it, not reloaded), ebx = the list's slot, esi = `0x10 + 8 * i`, edi = `i`, ebp = the second argument; the dispatch returns ecx and edx as left by the last callback or the compaction. Other modules rely on these (ANALYSIS's change test, WINMSG's callbacks). The add and remove services' leftover registers are not reproduced. | Every dispatch. | Make the contract explicit and remove the dependence. |
| CHAIN-2 | Removing from a list nothing was added to | Faults (retail CHAIN reads a null list). | A remove before any add. | Test the list. |

## NUMERIC.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| NUM-1 | The validator, a repeated minus | Is accepted because retail checks a flag it never sets (the unsigned wrappers still reject the negative value). | Input such as `--5`. | Set the flag. |
| NUM-2 | Id `0x19` | Accepts only octal characters but converts the resulting decimal digit string to packed BCD. | Every octal field. | Convert as octal. |
| NUM-3 | The upper end of a signed 48-bit range | Is exclusive, while the ordinary integer and fixed-point ranges are inclusive. | The top value. | Make them agree. |

## FACIL.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| FACIL-1 | Deinit | Writes the four strings and `GTL_BUTTON` back but not the `ALLOW_GRAY_ERROR_DIALOGS` flag it read at init. | Every exit. | Write it back. |
| FACIL-2 | The cache view | A successful search leaves the file mapped; `+0x1c`/`+0x20` never map it and read whatever view is there; unmapping (`.text+0x57e0`) by `+0x2c`, deinit and every `+0x18` failure leaves the view copies stale; if `MapViewOfFile` fails, both handles are closed, the cache is still marked mapped and the header is read through the null view. | A failed map. | Clear the copies; test the view. |
| FACIL-3 | `+0x18`, a failure after `out` is zeroed | Unmaps the cache and returns **the unmap's result**, so a failed search normally returns 0, not -1; the queue and the filter copy leak. | Every failed search. | Return -1; free. |
| FACIL-4 | `+0x1c` | A second match in the same run overwrites the first and leaks its name; it copies and sorts the list and never uses the copy. | A second match in a run. | Keep both; drop the copy. |
| FACIL-5 | OK's start position | Is an uninitialised stack buffer: in mode `0x1002` a navaid fills only part of it, in a mode other than the three with a stale airport list none of it, and the rest reaches GLOBAL `+0x560`. | Those modes. | Initialise the buffer. |
| FACIL-6 | An airport with no sources | Makes the tower search use the block itself as the section, copy block words into GLOBAL `+0xd50..`, and free the block. | An airport with no sources. | Skip the search. |
| FACIL-7 | Leaks | Every row's `+0x1c` result and record copy, the found frequency section, the leading empty-name record, `0x1309`'s output array, and a second airport-name allocation; several `.data` copies (the sorted filter list in `+0x1c`, a second free in mode `0x1002`) are made and discarded. | Always. | Free them. |
| FACIL-8 | FE `+0x88`'s return (see FE) | FE returns a local it never writes (left over from FE `+0x84`'s file lookup, in practice nonzero); FACIL's runway code treats it as success. | Every runway list. | Return a real status. |

## FE.DLL

All in `src/fe/` ([fe.md](modules/fe.md)).

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| FE-TYPE4 | `fe_register`, the type-4 list at `+0x9fb` | Allocates the list only while its pointer is negative (a signed test), and writes each entry at `13 * count` without adding the list base: the first type-4 section writes through a wild pointer and crashes. | A registered BGL with a type-4 extra section. The CD's BGLs carry none. | Test for null; add the base. |
| FE-18-PUSH | `+0x18`, after scene state 4 | Pushes edi after reading the south grid edge at `.text+0x2ef5` and never pops it, so it returns ebx = old esi, esi = old edi and edi = the grid's column count / 2. | Every scene run that reaches state 4. | Pop it. (Callers have to stop depending on the rotation, `fe_scene_entry` reproduces it.) |
| FE-40-CONVENTION | `+0x40`, the block-too-big path | Calls the read helper (`.text+0x4a81`) without pushing its buffer, writes the whole block through the caller's edi, returns -1 with the required size and leaves edi = old esi, esi = old ebx, ebx = old ebp. DYNAMIC's probe relies on it. | A caller probing with a buffer too small. | Return the size without reading. |
| FE-GRID-TABLE | `+0x60`..`+0x6c`, the grid conversions | Search a latitude table without checking the index: a latitude outside the table or a band whose index lands outside it reads on through `.data` and beyond. | A latitude past the table. | Clamp the latitude. |
| FE-84-LEFTOVER | `+0x84`, a BGL section's data | With a non-type-1 container leaves the data null but exposes the internal file-record pointer through the size output, and returns its never-written local at `[entry esp - 0x120]` (type-1 records return 1). | A non-type-1 container. | Return a status and a size of 0. |
| FE-88-LEFTOVER | `+0x88`, a BGL header copy | Returns a local at `[entry esp - 0x11c]` it never writes (it zeroes it only if the `memmove` fails, which cannot happen); on a header that does not match it also stores that word as the output size. In the game the word is left over from `+0x84`'s lookup helper and is nonzero, so FACIL builds its runway lists. | Every call. | Return a real status. |
| FE-LIB-EMPTY-DIR | The Scenery Library's area checks, `.text+0xb2f0` and `+0xb170` | With an empty area directory compare from the byte below the directory buffer (the top byte of the saved esi): a match would store zero into that register (only for a module at `0x5c000000`). | An empty directory. | Test the length. |
| FE-LIB-PRIORITY | The priority dialog's move | Does not check the selection: from one past the list it swaps a row in from beyond the working copy and writes over the heap after it. | The move at the list's end. | Check the selection. |
| FE-LIB-NEXT | Next after a cancelled drive search | Walks the listing the search freed: host `+0x188` is handed a path at `0x108`, and the game faults. | A cancelled search followed by Next. | Clear the listing. |
| FE-LIB-LEAKS | Deleting a cache row; storing the area list | Deleting a row leaks its four strings; storing the area list takes the fields the exchange table does not cover from whatever `.data+0x4080` last held; `.text+0xa9f0` always returns -1; the temporary file's joined name at `.data+0x3b60` is built and never used. | The cases listed. | Free; initialise; return a status. |

## WINDOW.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| WIN-1 | The look direction, `.text+0x50fa`; the matrix-to-angles divides; the aspect's `div` for a wide window | Divide each horizontal component (shifted up 15) by the horizontal length; when that rounds to the larger component the quotient is +0x8000 and `idiv` overflows. In the game the host's divide filter repairs it (see DIVIDE-DISP8-UNSIGNED), so it never crashes; OpenFS98's WINDOW computes the same saturation in C. | Looking within about a degree of north or east, pitch near level, a wide window. | Saturate explicitly. |
| WIN-2 | The map's interpolation masks | With GLOBAL `+0x390` 2 the map's zoom (view `+0x26`, copied to GLOBAL `+0x2b2`) indexes the masks unclamped, up to 0x803 bytes past them: into the assembly's instructions, read as heights. The generator carries `.text+0x3490..+0x3c94` so ours reads the same bytes. | A map at a large zoom. | Clamp the zoom. |
| WIN-3 | A 3D view's menu id | Is pushed from `dx` over the record pointer, so the host's menu gets `(rec & 0xffff0000) \| number`. | Every 3D view. | Push the number alone. |
| WIN-4 | The title | Is built on an uninitialised stack buffer: with a view kind other than 0 or 1, or a mode past 4, nothing is copied into it. | Such a view or mode. | Initialise it. |
| WIN-5 | Restoring (message `0x21`) | With nothing loaded, reads the first layout entry's view through a null array (`.text+0x2ce4`) and faults. | A restore with no layout. | Test the array. |
| WIN-6 | The track-to-tower transition | Aims twice and keeps the second, so the turn starts and ends at the same angles. | Every transition. | Keep the first aim. |
| WIN-7 | The old-format entries | Are kept by pointer into a host array that moves when it grows past 32, after which the pointers are stale. | More than 32 entries. | Keep indices. |
| WIN-8 | UTIL `+0x5c` and the sincos, rotation and longitude services | `+0x5c` is a bare `ret`; the others keep every register, and retail calls them through `eax` = UTIL's Linkage, so that is what `eax` holds after. | Always. | Return defined values. |

## WINMSG.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| WMS-1 | A scrolling message | The start counts down by the speed while positive and ends anywhere down to `1 - speed`; the line is drawn at `line + start / 8`: from about 14 frames a second (speed 9) it starts a byte or more early, and the first character overwrites the top byte of slot 6's text pointer, so a later `+0x18`/`+0x1c` frees a pointer that is not one. A window under 30 pixels does it for every scroller. At frame rates in the hundreds the line reaches the colours, the Linkage and the ImportTable. | Every game at 14 frames a second or more (the harness found 956 scenarios in 20,000 ending on such a free). | Keep the start at or after the buffer. |
| WMS-2 | A window 0x201e pixels or wider | The limit becomes 0x2000 and the start 0x400 characters in; the room left, `0x3ff - start`, is then -1, which an unsigned `min` takes as unlimited: the message is copied over the window pointer, the queue and on past `.data` into the import table. Not reproduced (past the image). | A situation file with an oversized rectangle. | Compute the room signed. |
| WMS-3 | The breaker | A word ending at the end of the text leaves the position on the terminator; the next step starts one past it and scans whatever the heap has there. | A word ending the text. | Stop at the terminator. |
| WMS-4 | The breaker's cut | Is not reset per line: a first word wider than the limit is cut where the previous line was; with no previous line the cut is 0, an empty message is queued and the word's first character is lost under the terminator, repeatedly. | A first word wider than the limit. | Reset it. |
| WMS-5 | A line's time | Is `cut * time / len` in 32 bits, so a large `time` wraps. | A large time. | 64-bit. |
| WMS-6 | The breaker's queue header | Leaks: its 8-byte header and two sentinels are never freed. | Every break. | Free them. |
| WMS-7 | The fill byte | Is pushed as `dl` over whatever edx held (the host's memset keeps only the low byte). | Harmless. | Push the byte. |
| WMS-8 | Message `0x37` | Frees the record's title through the record (`free(*rec)`), then WINDOW frees it again (already null); afterwards the record pointer is null and any frame or show faults. | In the game 0x37 is the end. | Free once. |
| WMS-9 | `+0x1c` / `+0x20` | Return the queue and the head sentinel by what was last in eax; the list `0x14` callback returns `data` for a record other than `0x3df` and `message - 0x37` for messages it ignores (CHAIN discards both). | Always. | Return a defined value. |

## MPCHAT.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| MPC-1 | Send | After a click on Send the focus goes to the main window (`+0xc4`), not back to the line: a second line needs a click in the Edit or key `0x10171`. | Every Send. | Return the focus to the line. |
| MPC-2 | The scroll position | `WM_VSCROLL`'s wParam is `((nPage << 16) - nPage + nMax) << 16 \| 4`: the first shift is lost to the second, so the position is `(nMax - nPage) & 0xffff`, from a 16-bit maximum. | A chat longer than 0xffff units. | Pack the position properly. |
| MPC-3 | A line built with `strcat` in a 0x400-byte buffer | A name of 0x3fe characters or more overruns the stack. | A long DirectPlay name; none is. | Bound the copy. |
| MPC-4 | String `0x9402` | A missing string crashes the first line from an unknown sender (`strcat` of a null pointer, inside the host). | A missing string; the game has it. | Test the pointer. |
| MPC-5 | The RichEdit append | Reads its selection uninitialised when the RichEdit is gone (`+0xdc` still names a destroyed window; `EM_EXGETSEL` writes nothing). | A line arriving after the session's window was destroyed. | Test the window. |
| MPC-6 | Message 4's list | Is shown but never sent: MULTIP dispatches 1, 2, 3 and 5 on list `0x1d` and `+0xbc` stays null. | Always. | Send it or drop it. |
| MPC-7 | The RichEdit formats | `PARAFORMAT2` past the indent and `CHARFORMATA` past the colour are stack garbage in retail (ours zero); RichEdit reads only the masked fields. | Always. | Zero the structures. |
| MPC-8 | List `0x14`/`0xb` callbacks | Return the eax they were entered with for a message they ignore (CHAIN discards it; ours does not reproduce it). | Always. | Return a defined value. |

## REALITY.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| REAL-1 | The countdown after the analysis | Tests for 1 on the step's count in `ah` while the dialog returns in `eax`: a result whose second byte is 1 (`0x100`..`0x1ff`) also runs the step at 1 (the resume, in the same frame). | A dialog result of `0x100`..`0x1ff`. | Compare the whole result. |
| REAL-2 | The metric scale search | Steps 8 bytes through the feet table but `0x30` through the metric one (same layout), so it sees every sixth entry: only 10, 1000 and `.rdata+0xc0` (the low word of a double, 2,545,942,528) as limits. A metric range over 1,000 m takes its step from that double's high word and the graph is flat. | Every metric analysis graph over 1,000 m. | Step by the table's own entry size. |
| REAL-3 | The scale search | Has no bound: past its table's last limit it reads on, through `.rdata`'s slack into `.data` and the code pointers there (not reproduced past 2,545,942,528: a range of 2.5 million km). | An absurd range. | Bound the search. |
| REAL-4 | Units past 2 | Leave the scale as it was: the axis is drawn with the last scale (0 the first time) and the curve divides by it. | A unit past 2. | Reject the unit. |
| REAL-5 | The points' x | Is the index read as 64 bits with a zero top half (-22 is 4,294,967,274). | Nothing reads it but the host's array. | Sign-extend. |
| REAL-6 | The heights' labels | Start at 0 from the high dword of an 8-byte local whose low dword the seconds' labels used; every other one is blank. | Every height graph. | Use their own local. |
| REAL-7 | The upper descent clamp | Replaces only `ax`: a rate over `0xa674` keeps its high word. | A rate over `0xa674`. | Replace the whole value. |
| REAL-8 | `.text+0x144` | Compares GLOBAL `+0xc44` with `0x32` and goes on the same way either side: the module's only dead code (omitted). | Always. | Delete the test. |

## ANALYSIS.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| ANA-1 | The sample compaction | Keeps its slot in ecx across host `+0x21c` (a `memmove`): every kept sample lands in one slot and the count ends one past it, so the trail collapses rather than halving (7 with the CD's 1997 `MSVCRT.DLL`, 0 with wine's builtin). Reproduced as 7 everywhere (`AN_MEMMOVE_ECX`). | Every compaction. | Keep the slot in memory. |
| ANA-2 | The manoeuvre interval | Doubles `.data+0x71` with a `rol`, so after 0x80 it is 1 again. | After 8 doublings. | Saturate. |
| ANA-3 | The change test | `mov cx,0xc` / `repz cmpsw` in 32-bit code counts (ecx's top half : 0xc) words; with a top half an unchanged position compares on into GLOBAL and takes the sample anyway. | Latent: in the game the top half is always 0. | `movzx ecx, 12` / use `ecx`. |
| ANA-4 | The landing height | Takes its borrow from bit 7 of the ground's low byte (`sub ah,bl` / `sar ebx,8` / `sbb edx,ebx`: the `sar` replaces the subtraction's carry) and never subtracts the fraction's low byte. | Every landing. | Do the 48-bit subtraction. |
| ANA-5 | The landing scale | Divides by zero when the run has not moved (`7692 * (range - distance) / range`); the host's divide filter yields all ones. | A landing graph with a zero range. | Test the range. |
| ANA-6 | The landing graph with fewer than two samples; the manoeuvre extents | A run of none reads the "last sample" from index -1, which `+0x20` refuses, so a frame local is used uninitialised; the manoeuvre extents' loop starts at -1, so the first sample is taken twice. Not reproducible: ours is zeros. | A landing or manoeuvre with no movement since the last recorded sample. | Handle an empty run. |
| ANA-7 | The highest latitude's tie-break | With equal whole parts, compares the whole part with the stored fraction word instead of the fraction with it. | Equal whole parts. | Compare the fractions. |
| ANA-8 | Committing the options | "Off" does not stay off: committing with "on" cleared zeroes the mode, and the selection then sets it again. | Turning analysis off. | Do not set the mode when off. |
| ANA-9 | The trail's segment count | `.data+0x76[3]` can read 0, which the loop's `dec`/`jne` takes as no limit; past 500 samples it writes past the 0x59e4-byte block. The tables are indexed by a GLOBAL dword with no bound. | A saved flight or an OK past the dialog's refusal. | Bound the index and the count. |
| ANA-10 | The manoeuvre scale | The value is a mixed 8-byte local: the eighth of the latitude span as a double, then `_ftol`'s result over its low dword, and all eight bytes are formatted. | Every manoeuvre scale. | Format one value. |

## DEMO.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| DEMO-1 | The flush, `.text+0x117e` | Keeps eax and ecx and nothing else: when a word event (`0x2006` and the rest) fills the buffer with its tag, the value is written from the dx the host's write left, not the event's. | A word event filling the buffer. | Save edx. |
| DEMO-2 | The 0x60 handler | Indexes the five values at `.data+0x6a0` by the record's nibble: nibbles 5..15 read the next table (`+0x6b4`, code pointers), so retail sends CONTROLS command `0x1009b` with one of its own addresses as the value. | A record with a nibble over 4. | Bound the nibble. |
| DEMO-3 | The 0x40 handler | Sends its second command with the ecx CONTROLS left (it saves eax round the first call, not ecx). | Every such event. | Save ecx. |
| DEMO-4 | `+0x24` | Reads `+0x11a0` of the caller's edi: events 10, 12 and 17 test it and never set edi, so it is the caller's (CONTROLS'). | Those events. | Pass it. |
| DEMO-5 | The reader table | Is shared: instant replay and `+0x20` switch it to memory and nothing switches it back until the next `+0x28`; a video playing meanwhile reads memory (a looping one rewinds to nothing and spins for ever) and deinit leaves the file open. | A video playing during a replay. | Restore it. |
| DEMO-6 | Instant replay | The dialog refuses while GLOBAL `+0x758` is 1 (playing), not 2 (reviewing a recording just made); started then with the loop flag `+0x750` still set from an earlier video, `+0x18` rewinds the memory reader to nothing and loops for ever. | A replay started during a review. | Refuse on 2 as well; clear the loop flag. |
| DEMO-7 | The message reader's refill, `.text+0x197a` | Keeps only eax, and the 0x84 reader counts its 0x37 bytes in ecx: a refill in the middle of a message hands the count the ecx the host's read left, and a large one writes past the 0x38-byte buffer into `.data+0x530`. | A refill in the middle of a message. | Keep ecx. |
| DEMO-8 | Stopping a review | Runs the review to its end, and an unknown frame tag stops playback, so a recording with a wild tag recurses until the stack runs out (the view event, 10, writes tag 0x80 + its value). | A view number past 3. | Bound the tag. |
| DEMO-9 | A replay longer than the ring | The dialog refuses a length over `+0x630`; a replay started past it parses the ring's `0xffff` fill and spins, alerting, for ever. | A replay past the ring. | Check at the start. |
| DEMO-10 | Zero frame interval; misparsed lengths | A zero frame interval divides by zero deriving the speeds (so does an overflowing vertical speed); a misparsed dword length skips gigabytes 0x200 bytes at a time, and an unparsable record skips a second at a time. | Corrupt recordings. | Validate. |
| DEMO-11 | The message routine's `ecx == 0x38` test | Never passes (a `repnz scasb` that always counts one). Kept, marked. | Every message. | Delete the test. |

## ADVDRV.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| ADV-1 | `op_error`, `.text+0x1bc0` | **Stops the program** (frees the code buffer) and shows alert `0xf439`, then **returns to the interpreter**, which keeps running on a dangling cursor: a use-after-free. Covers a bad instruction, a bad operator, an operand-stack over- or underflow, a call-stack overflow and a bad variable reference. | A faulty adventure. | Stop the interpreter after an error. |
| ADV-2 | The feet getter, `.text+0x3d90` | Zero-extends a 16-bit value and compares the 32-bit result to `0xffffd8f1`, which is then unreachable: "-9999 means none" never fires (the near-identical getter at `.text+0x4140` compares the 16-bit value and does). | A feet value of -9999. | Compare 16 bits. |
| ADV-3 | The position converter | Is sign-and-magnitude (adds 2^-17, integer part as the high dword and the magnitude of the fraction as the low word), so a negative position is not two's complement. | Every negative position. | Use two's complement. |
| ADV-4 | Inserting a cloud, wind or temperature layer | The pushed layer is re-sorted but some fields are not reloaded, so it inherits the inserting layer's top altitude; the fourth temperature slot never records its layer id. | An insertion that pushes a layer up. | Reload every field. |
| ADV-5 | `events forget` | Unlinks a node and then advances past the node that followed it: a run of two matching events loses only the first. | Two matching events in a row. | Advance from the unlinked node's successor. |
| ADV-6 | The sound cache | Marks the copy, not the source, read-only; the delete is from string `0x900e`'s directory. | Every program with sounds. | Delete from the directory copied to. |
| ADV-7 | The integer operators | Divide with `idiv`: a program that divides by zero takes a #DE. | A division by zero in a program. | Test the divisor. |

## DYNAMIC.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| DYN-1 | The opcodes below `0xa` (`0`, `8`, `9`) | Set carry without moving the counter, and the step runs on from the same place for ever; a program counter that strays onto a zero does the same. | A program with such an opcode. | Advance the counter. |
| DYN-2 | The interpreter | Checks nothing: jumps, calls and returns go where the bytes say; `0x31`'s index is a byte, so it writes up to 0x1fe past the parameters (into the next object at `0x8a`); a declaration run from a program leaves edi on the next object, or on GLOBAL, and the program writes there; divides fault on zero and overflow. | A wild program. | Validate every operand. |
| DYN-3 | `0x20` | Steps with `add si,7`: only the low 16 bits of the counter. | A counter past 0xffff. | Step 32 bits. |
| DYN-4 | The ramped moves (`0x19`-`0x1f`, `.text+0x1abd`) | Keep their offset in ecx across the timer; right after a path the timer's `path_end` (`.text+0x1df2`) leaves the altitude's whole part in ecx and the target becomes the base plus its low word. | A ramped move first after a path or generated code. | Save ecx. |
| DYN-5 | The frame's loop | Counts the caller's ecx when there is no object array. | No object array. | Count the array. |
| DYN-6 | Refuelling, `0x3f` | The total room leaves out the first tank (`+0xc44`'s add); a first tank with more than twice the others' room overflows. | Every refuel. | Include the first tank. |
| DYN-7 | A player with id 0 | Overwrites the user's record. | A player with id 0. | Skip id 0. |

## CONTROLS.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| CTL-1 | The integer parser, `.text+0xa200` | Steps past the terminator after trailing blanks; the buffer is zeroed, so what follows reads as empty. | Trailing blanks. | Stop at the terminator. |
| CTL-2 | `write_cmd` | Writes a nameless command's null name, which deletes the key. | A nameless command. | Skip it. |
| CTL-3 | OK on a joystick | Rebuilds the joystick's config from the dialog's bindings alone, so its button-up events, LOCKED and TYPE are lost. | Every OK. | Keep the fields the dialog does not edit. |
| CTL-4 | The defaults | Are skipped when the *current* table (flight or slew) has the stick locked; a model whose string is missing installs whatever the stack held. | A locked stick. No model the host names lacks its strings. | Skip by the table being filled; initialise. |
| CTL-5 | Force feedback | With no force-feedback joystick enumerated, the device is created from an uninitialised stack GUID. | No such joystick. | Skip creation. |
| CTL-6 | A periodic effect | A kind outside 0..2 starts the device as if it were the effect. | No caller passes one. | Reject it. |
| CTL-7 | The axis response curve, `.text+0x8e90` | Takes the live reading, subtracts a dead zone (`+0x8c`, `<< 7`), scales (`+0x4c`, `>> 7`), clamps and negates: the very-negative case returns `+0x4000` without the final negate. | A reading far below the dead zone. | Negate it. |
| CTL-8 | The throttle floor, `.text+0x5daa` | `cmp ax,[edi+0xb00]` / `jge` / `mov ax,[edi+0xb00]`: at equality the floor is loaded over the same value (the flags are dead). | Always. | Compare with `<`. |
| CTL-9 | JOYSTICK_CALIBRATE's `+0x44`, and the pilot-event words to DEMO | `+0x44` leaves `ecx` 0 (the `rep stosd` of `axis_reset`), which DEMO reads; the pilot-event words carry retail stack garbage in their top halves (DEMO reads only the words). | Always. | Pass values explicitly. |

## SOUND.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| SND-1 | Code `0x1b` (horn off) | `snd_stop` stops a voice handle as if it were a record: it reads the handle's `+0x1c`, and with no voice loaded it reads address `0x1c` and faults. | Every horn-off when the default record has no voice. | Test the handle. |
| SND-2 | The marker-light toggle (CHAIN list `0x12`) | Checks its index only against 5, signed: index 5 writes past the five ident slots, a negative one below them, into the listener position; the bit shift takes its count mod 32. | An index of 5 or negative. | Check the range both ways. |
| SND-3 | Flaps | Stop when either flap word is unchanged. | A flap move with one word unchanged. | Compare both. |
| SND-4 | Ids `0x21` and `0x22` | Share the default record, which has no file. | Always. | Give each its own record. |
| SND-5 | `+0x34`'s clamp | Clamps to 10000 unsigned, so a negative fader becomes 10000. | A negative fader. | Clamp signed. |
| SND-6 | The vparams check | Reads one point past the block when all eight rise (nothing comes of it; ours does not read it). | Every such check. | Stop at the block. |

## ATC.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| ATC-1 | The translator's word copy; `src/atc/` | A position with no match copies one word and the space after it, up to the next space even past the end of the message: a message that does not end in a space drags following buffer bytes into the output (the same image bytes on both builds). | A message not ending in a space. | Stop at the terminator. |
| ATC-11 | The translator's keys | The message is upper-cased for the comparison; the keys are not, so a key with a lower-case letter can never match; keys match as prefixes (`MI` matches `MICROSOFT`) and the first match in file order wins. | Any lower-case key. | Fold both. |
| ATC-12 | `+0x20` (the ATIS) | Reads GLOBAL `+0xb04` and `+0xb68` through the caller's EDI, which it never loads: MAIN's only call site has GLOBAL there. | Any other caller. | Take GLOBAL from the import table. |
| ATC-2 | `+0x18` / `+0x1c`, starting | Register the step callback on every call, so two starts before a finish step the exchange twice a frame. | Two starts before a finish. | Register once. |
| ATC-3 | `+0x4a2` | Is never cleared, so every finish unregisters, whether or not the callback is still registered. | Every finish. | Clear it when unregistering. |
| ATC-4 | The cloud-layer match | Matches the first layer only below its top (`<`) and the second also at it (`<=`). | A layer boundary at a layer's top. | Make the two tests the same. |
| ATC-5 | Posting the squawk reminder and the taxi instructions | Does not wait for the message to go out, as the other posts do. | Those two posts. | Wait for them as well. |
| ATC-6 | An empty digit field with blanking | Would write its `'0'` one byte before the field. | No id ATC formats can produce one. | Start the write at the field. |
| ATC-7 | Translation after deinit, or after a re-init with GLOBAL `+0x128c` clear | Both zero the section table without clearing "loaded", so the next translated message reads a null section. | Never in the game (init once, deinit at exit). | Clear "loaded". |
| ATC-8 | The composer, a cloud base of `0xffff` that is not overcast | The next search starts at base+1, which wraps to 0, and the composer loops for ever. | A cloud base of `0xffff`. | Stop at the top. |
| ATC-9 | LocaleID | One `sscanf("%x")` cannot parse leaves the locale uninitialised on retail's stack. | An unparsable locale. | Default it. |
| ATC-10 | The formatter | Copies NUMERIC's output from six bytes at `[ebp-6]`, so a seven-byte result would overwrite the saved EBP. | Never: every id ATC uses is fixed width and five characters or fewer. | Size the buffer. |

## ACONTAIN.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| ACN-1 | The colour lookup | Stops early on an alphabetical table only when the host's `_strcmpi` returns exactly -1; a name that sorts before an entry by more keeps searching. | A name sorting before an entry by more than 1. | Test `< 0`. |
| ACN-2 | `+0x24` | Reads its argument without checking it: a null record faults. | A null record. | Check it. |
| ACN-3 | `+0x28` | Never checks the chunk read or the `.AIR` query: a missing chunk `0x100` loads `SIM\.SIM` and fails as a missing flight model. | A missing `.AIR` chunk. | Check both. |
| ACN-4 | `+0x34` | Checks the module pointer, not host `+0x26c`'s return value. | A failed load. | Check the return. |
| ACN-5 | `+0x2c` | Unloads the visual model only if a flight model is loaded. Without one, `+0x24` unloads it, because the record still says it is loaded. | No flight model loaded. | Unload the visual model on its own flag. |
| ACN-6 | The title search | Stops at the first `[fltsim.N]` with no title, even if later sections exist. | A gap in the section numbers. | Skip the gap. |
| ACN-7 | The directory walk | A file in `AIRCRAFT\` with an extension the host does not know keeps the previous entry's type: after a folder it is taken for a folder too (no `aircraft.cfg`, skipped). | An unknown extension. | Reset the type per entry. |
| ACN-8 | Reads of stack it never set: `+0x28`'s previous title, its flight-model name, the directory walk's first entry type | Read whatever the stack held (the title shown in the second alert; a file as a folder if the leftover is -2). | No previous aircraft or title; a missing `.AIR` chunk; an unknown extension first in the listing. | Initialise all three. |
| ACN-9 | Path and chunk buffers | Paths over 0x100 characters (the host's join writes up to 0x104), `.AIR` chunks larger than their buffers (0x104; 0x400), a flight-model name with no terminator, a model state under 0x68 bytes (`+0x60`/`+0x64` written regardless) overrun retail's buffers; the harness keeps inside them. | Long paths; oversized chunks. | Check every length. |

## BOOSTER.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| BST-1 | Init | Does not wait for its thread, which has no message queue until it first asks for a message: a 0x401 posted before then is lost and `+0x18` waits for ever on an event nobody sets. | In practice never (the sim starts long before a booster is handed over). | Wait for the queue. |
| BST-2 | The directory scan | The host's find-close dereferences the handle unconditionally; a cache without `SCENERY\` or `TEXTURE\` leaves a null handle that BOOSTER closes anyway. | A cache without one of those directories. | Test the handle. |
| BST-3 | The find buffer | A file with an extension the host does not know keeps the previous entry's extension id: `NOEXT` after `G001.HAZ` is opened as `NOEXT.HAZ` when the cache is measured and deleted as `NOEXT.HAZ` when emptied. | An unknown extension. | Reset per entry. |
| BST-4 | Grid and list records | Columns or rows of 0 divide by zero (rows signed, columns unsigned); a record shorter than 0x13 bytes makes the file loop run 2^31 times. | A short or zero record. | Validate the record. |
| BST-5 | UTIL `+0x44` (retail's, not BOOSTER's) | Divides once, 64 by 32 bits, so the quotient overflows (#DE) once the distance reaches the circumference at that latitude (about 4,200 km at 84 degrees south). **OpenFS98's UTIL does not reproduce this fault** (a known difference). | A reach of a whole latitude circle: within about a kilometre of the poles. | Divide in two steps. |

## WEATHERM.DLL

All in `src/weatherm/weatherm.c`; the offsets are the module's.

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| WXM-1 | `+0x2567`, a position inside a layer's band | Divides `((altitude - base) >> 1) << 32` by `(top - base)` with `idiv`. At `altitude == top` with an even band the quotient is exactly 2^31 and overflows (#DE); it is called with `altitude == top` on the way up through every layer. | Above any layer whose band is even (the defaults, layers at 180 m and 500 m, are 320 m apart, so anywhere above 500 m after a reset). A band of zero is a divide by zero. | Clamp the quotient; reject a zero band. |
| WXM-2 | `+0x160b`, the fade | Halves the fade width first and divides by it: a width of 1 divides by zero. | A fade width of 1. | Test the halved width. |
| WXM-3 | `+0x1329`, the surface wind's blend | Offsets both sources but not the destination, so its three blends all land on `GLOBAL+0xef8` and the copy after them overwrites that word; `+0xefc`, `+0xf02` and `+0xf08` keep what they held. | Every surface wind blend. | Offset the destination too. |
| WXM-4 | `+0x1c51`, the pressure wander | Uses the block number (0 to 2) unscaled as an index into five three-word arrays at `.data+0xc8`: blocks 1 and 2 read and write their neighbours' bytes. | Every pressure wander for block 1 or 2. | Scale the index by the element size. |
| WXM-5 | `+0x171d`, the second cloud layer | If that layer's middle is above ground + 200, the ground line stays 200 higher for the comparisons after it. | A second layer whose middle is more than 200 above the ground. | Keep the raised ground local to the layer. |
| WXM-6 | Area conversion | Converts the position against both areas' centres whether or not the area is on; a wild centre in an area that is off walks UTIL off its latitude table. | A wild centre in an area that is off. GLOBAL starts zeroed in the game, so it does not happen. | Skip areas that are off. |
| WXM-7 | `+0x28`, wind type 3 | Retries a wind more than 90 degrees off by adding 180 degrees; after a sincos the type register holds the strength's low byte, so a second retry asks that byte instead of the type. | A wind more than 90 degrees off, retried twice. | Keep the type in its own variable. |
| WXM-8 (suspected, not used) | The C half, areas' effect lists (`+0x2c`..`+0x5c`) | Allocates through host `+0x20c` without the zero-fill flag, then walks `next` pointers and list heads it never set. Nothing is known to call it. | Never seen. | Zero-fill the allocation. |

## SLEW.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| SLEW-1 | The three velocity-component negations before the squares; `src/slew/` (QUIRK 2, omitted as dead) | Tests each component against `0x0000ffff` and negates on sign; the 32-bit `and` always leaves bit 31 clear, so the `jns` is always taken and nothing is ever negated. | Every tick. Harmless: the values are squared next, which drops the sign. | Delete the dead tests. |
| SLEW-2 | Fast slew's speed clamp, `src/slew/` (QUIRK 3) | Tests the speed against `0x3fffff` and then assigns `0x3ffff`, one digit short: a speed over the limit becomes a sixteenth of it instead of the limit. | Fast slew with a speed of `0x3fffff` or more. | Assign `0x3fffff`. |
| SLEW-3 | The sum of squares of the velocity components (UTIL slot `0x18`'s quadword at `0x3018`/`0x301c`); `src/slew/` | Adds the high halves first and carries into the **low** half (`add [hi]` / `adc [lo]`), the wrong way round: the low half's own overflow is lost and a carry that belongs nowhere is injected into it. | Whenever the low half of the running sum overflows. | Add low first and carry upward. |

## PANELS.DLL

All in `src/panels/panels.c`, each noted where it happens.

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| PANEL-SMOOTH | `smooth_8`, `smooth_24` | Weight pixels by the loop counters, not by positions, and write their rows one late (row 0 never): the blend is nil until the output is hundreds of pixels wide, and the centre case (`i == j == 0x8000`) cannot happen. | Every smoothed scaling. | Weight by position and write each row in place. |
| PANEL-SHRINK-GREY | `shrink_8_to_8` | Counts the first pixel's grey weight twice. | Every 8-bit shrink. | Count it once. |
| PANEL-555-TEST | `to_555` | Tests bit 15 where it means the colour. | Every conversion to 555. | Test the colour. |
| PANEL-EDGES-AND | `edges_24` | Tests `i & j` where it means both nonzero. | Every 24-bit edge pass. | Test both for nonzero. |
| PANEL-WINDOW-IDENT | `window_ident` | Reads each table entry's window without checking it: a window that is not in the first of the table faults on the first null entry. | A window not in the first entry. | Stop at the first null. |
| PANEL-NAMED-CLEAR | `named_clear`, `named_remove` | `named_clear` hands `free` the address of a local, so nothing is freed; `named_remove` of an empty list faults on its head. | Every clear; removing from an empty list. | Free the list; test the head. |
| PANEL-FCLOSE | `bitmap_load` | Passes `fclose` the `FILE *` of a file that did not open. | A bitmap that does not open. | Close only an open file. |
| PANEL-FREE-USE | `panel_free` | Reads the record `+0x10` it has just freed. | Every panel free. | Read it before freeing. |
| PANEL-LOAD-WINDOW | `panels_load` | Takes a window the table does not name to be a panel of the last window (retail's stale `ebp`; here 0), and a window without a layout of its own takes its size from the first's `SIZE_MM`. | A window not named in the table. | Reject the window. |
| PANEL-ZOOM-ADDRESS-2 | Variable `v_7d30` (`v_8070` is PANEL-ZOOM-ADDRESS above) | When no case matches, scales the stack slot of an argument (the op, or the variable's own address). | An unsupported case. | Supply a defined fallback. |
| PANEL-QUAD-CLAMP | The needle's, strip's and shape's quads | The window clamps leave a corner unclamped (the needle's second corner's x has no lower bound; the shape's first corner's x). | A corner outside the window. | Clamp every corner. |
| PANEL-DIGITS | The digits' DrawText measure; a type-4 icon | The measure leaves the rectangle's corner as the stack had it; a type-4 icon in mode 2 (or past 4) steps to its own pointer. | Digits drawn; a type-4 icon in mode 2 or past 4. | Initialise the corner; step to the next icon. |
| PANEL-DIB-NEW | `dib_new` | Past 5 it writes through an uninitialised pointer. | Past case 5. | Initialise the pointer. |

## G3D.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| G3D-1 | Opcodes `0x01`, `0x4b`, `0x59` | Are a bare `ret` that does not move the counter: the interpreter runs them for ever. | A program holding one. | Advance the counter. |
| G3D-2 | Opcode `0x03` (case) | A value at or past the count sets `bx` to `0xffff` (index -1 in 16-bit code) but doubles it in a zero-extended `ebx`, so the default is read 128 KB past itself (`+0x20006`). | An out-of-range case value. | Sign-extend. |
| G3D-3 | Opcode `0x8c` | Calls an absolute code address from the program, so the built-in bytecode embeds G3D's own addresses (and the sky program names code in MAIN, `0x207e9d47`). Ours maps them through `routine()` and `native_thunk`. | Every sky. | Call by id. |
| G3D-4 | Opcodes `0x3a`/`0x3c`, `0x18` | Write into the program they run: `0x3a`/`0x3c` exchange a one-shot word at `+8`; `0x18` rewrites a texture name. | Always. | Keep state out of the program. |
| G3D-5 | Opcode `0x47`'s scale | Stores the eye unshifted for operands above 32 (it jumps back into the store) and shifts by `32 - operand` below. | An operand above 32. | Shift for every operand. |
| G3D-6 | Opcode `0x07` outside a surface | In the map (GLOBAL `+0xdca`) with a line colour other than `0x6900` returns before reading its point, having moved the counter only past the first byte: the program goes on from inside the instruction. | The map with another line colour. | Skip the whole instruction. |
| G3D-7 | The two-word point form | Is unreachable: x and z with y from `.data+0x157` when bit 0 of `.data+0x156` is set; nothing in G3D writes `.data+0x156` (0 on disk). | Never. | Remove the dead form. |
| G3D-8 | Opcode `0x36` (a row of points by a mask) | Its mask walk moves on with `rol al,1; adc esi,1` (a byte a point, and one more each time the bit comes round, not a byte each eight); its counter goes to `(ebx + 15) >> 4` past `+0x1e` with ebx as the last point left it (screen x, eye-space x, the step's y, or with one point and its bit clear the address of `.data+0x104`). It can match retail only where G3D sits at its retail base. | Every `0x36`. | Count in bytes properly. |
| G3D-9 | The clippers' unclipped path | Every clipper sends its vertices straight to HG2D when `.data+0xf6` is 2, which G3D never writes (1; 0 on disk). | Never reached. | Remove it. |
| G3D-10 | Opcode `0x27`'s vertex loop | Counts in all of ecx but restores only `cx` (`push cx`/`pop cx` around each call): the high half is what the polygon clipper left (the last vertex's y, or its clamp, or 0); with y of `0x10000` or more or below 0 the facet walks on past its count through the words after it as indices to far-away points. | A vertex y of `0x10000` or more or below 0. | Save and restore all of ecx. |
| G3D-11 (fixed, not kept) | Opcode `0x60`'s fast triangle, `0x206f0cec` | With `.data+0xf4` set (opcode `0x7d`), outside the map and GLOBAL `+0x12a4` bit 3 clear, a three-vertex `0x60` pushes its vertices with a `push edi` that is never popped: it draws from a skewed frame, frees only `0x3e` bytes and returns to a vertex's y as a code address. | A three-vertex `0x60` in that state. | (Fixed on purpose: pushes and pops as `0x20`'s triangle does.) |
| G3D-12 (fixed, not kept) | Opcode `0x7b`'s fast quad, `0x206ec75e` | On the same conditions pushes the quad's depth with `push 0` (a dword) where its siblings push a word, so every vertex is read two bytes off and the handler returns into the middle of an address. | A four-vertex `0x7b` in that state. | (Fixed on purpose: pushes a word.) |
| G3D-13 (fixed, not kept) | Queueing a model whose first instruction is not an object, `0x206e5eae` | For anything but `0x3a`, `0x4c`, `0x3c`, `0x2f`, `0x77` after a `0x89` context, sets the carry with the instruction's address still pushed, pops it into `.data+0x1b7` and returns to the old context as a code address. | A model starting with another instruction. | (Fixed on purpose: pops both and refuses the model.) |
| G3D-14 (fixed, not kept) | Opcode `0x49`'s second kind of building, `0x206f5a30` | With the eye inside the building and off the map, marks GLOBAL `+0x832` after a `push edi` that is never popped, so its `ret` returns to edi's value. | The eye inside such a building. | (Fixed on purpose: marks it and returns.) |

## HG2D.DLL

All in `src/hg2d/`; the offsets are `.text` offsets ([hg2d.md](modules/hg2d.md)).

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| HG2D-1 | The handle tests | Check the range, not record alignment: an in-range unaligned handle accesses the words at that address. | An unaligned handle. | Check alignment. |
| HG2D-2 | Free and update | Free clears only the first four dwords (hardware release also clears record `+0x1c`, zeroes eight scratch dwords at `0x2079a820`, decrements the count at `0x2079a800` and subtracts from `0x207ba9ec`), discarding the COM Release result; free and update return the incoming eax for an out-of-range handle; pixel access sets `0x10000` even for an empty record. | Every free; an out-of-range handle. | Clear the whole record; return a status. |
| HG2D-3 | The shaded-key span path | Increments the destination **before** writing, so it writes one pixel to the right (preserved on purpose). | Every shaded keyed span. | Write, then increment. |
| HG2D-4 | The optimised backend | Divides true signed deltas by the inclusive pixel count, keeps the second coordinate unshifted and has no software width cap; four samples are read before their four destination bytes are written (including when texture storage aliases the canvas). | Always. | Document the order; cap the width. |
| HG2D-5 | The distance-row lookup | The raw texel and phase bits select a byte at `+0x4e00` that selects the distance row at `+0xd70`; initialisation builds sixteen 256-byte rows but the unchecked five-bit phase can address thirty-two, reading adjacent module state. | A phase of 16 or more. | Build or mask 32 rows. |
| HG2D-6 | The line, point and span primitives | Points do not check surface bounds; horizontal spans have no right-edge clipping and only clamp negative low words of x, and reversed non-negative endpoints produce a zero or wrapped count that, with the short loops executing once before decrementing, writes past the row; a zero frame dimension skips drawing and colour selection. | A line or span outside the surface. | Clip every primitive. |
| HG2D-7 | The palette | Shade rebuilding uses a divisor of 16 instead of 14 and toggles the generation byte at `0x20796034` with `0xa5`; the inverse shade table is indexed by the full word; the gradient mode `0xf0` doubles only the low byte of the product's low half and adds just its carry to the high byte; the sky ramp's final byte wraps then becomes 255; refresh's saved EBX includes bits left by the remap loop. | Every palette change. | Use the intended divisor and doubling. |
| HG2D-8 | Device texture preparation, `0x2078f600` | A failed staging allocation leaks the already queried texture interface; a failed upload can still proceed to handle lookup; the stored HRESULT is the final lookup's. | A failed staging allocation or upload. | Release and stop. |
| HG2D-9 | The depth helper, `0x207880ac` | Leaves its lookup index in EBX and the compiled device point does not save EBX: an ABI bug that changes the caller's EBX whenever depth shading takes the lookup path. | Depth shading with the lookup. | Save EBX. |
| HG2D-10 | The device vertex set-up | Leaves specular and UV stack bytes uninitialised. | Always. | Zero them. |

## MULTIP.DLL

All in `src/multip/`; the offsets are `.text` offsets ([multip.md](modules/multip.md)).

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| MULTI-1 | Local name publication, `+0xa1a0` | Leaks its allocation on success. | Every publication. | Free it. |
| MULTI-2 | The message constructors, `+0x5a80`..`+0x6060` | Four allocate an extra packet and leak it on enqueue failure; four set request-pending and the time only after a successful enqueue; the initial packet leaks on enqueue failure in the transfer messages (`101a`..`101e`) too. | An enqueue failure. | Free the packets. |
| MULTI-3 | System notifications | Player destruction returns -1 even after a successful removal; host migration publishes its flag before querying the session and keeps its partial changes and leaks on failure. | Every destruction; a failed migration. | Return the real result; publish after the query. |
| MULTI-4 | The owner list, removing a non-head owner | Replaces the global head with that owner's successor, detaching the earlier owners without freeing them. | Removing a non-head owner. | Unlink the removed node only. |
| MULTI-5 | The transfer messages (`101a`..`101e`) | Ignore their title and `strcpy` results; the model header ignores its second argument and reads the current GLOBAL aircraft total after the callbacks; names are raw fifteen-byte. | Every transfer. | Use the argument; check the results. |
| MULTI-6 | The state packets | The compact packets' two padding words and the full-state packet's `0x6c`-byte tail are uninitialised stack (the harness paints it); the DPNAME reserved word is read uninitialised at entry ESP-0x400 in `+0x8820` and ESP-0x404 through `+0x8700`. | Every send. | Zero the packets and the reserved word. |
| MULTI-7 (suspected) | `PLAYER_LIST_UPDATE_TIME` (`+0x174`, `+0x178`) | The destination passed to the reader and the one validated differ. | Every read of the setting. | Validate the field read. |
| MULTI-8 | Receive pumping | A NULL payload leaks its record; a failed dispatch, node free, payload free or record free leaves partial state; successful receive followed by a queue append failure leaves `received=1`; node allocation failure leaves the queue lock held. | The failures listed. | Free and unlock on every path. |
| MULTI-9 | Message `1000` | Subtracts its lengths unchecked: wrapped copy lengths (`0xffffffff`, `0x80000000`, a length under `0x6c`) overrun the copy. | A wild length. | Bound the copy. |
| MULTI-10 | The stale local-player pointer | The host's `+0x300` skips the copy when the signed index is at or beyond the new end and does not reject a stale out-of-range record pointer, and MULTIP keeps its stale-local pointer (the fixture retains moving-array banks). | After the array moves. | Hold an index. |
| MULTI-11 | The browser's enumeration cleanup | Never advances its record pointer: it frees the first name and then rechecks that now-null field for every remaining iteration; other names leak; a failed free returns -1 before clearing the list count; enumeration consumes that failure as success without calling `DirectPlayEnumerateA`. | Every cleanup of two or more records. | Advance the pointer. |
| MULTI-12 | Session refresh | Reinitialises matching records without freeing their previous name/list; a failed record setup returns -1, which DirectPlay treats as a true callback result; `TIMEOUT` returns false without inspecting a descriptor; marker/prune walker errors are consumed. | A refresh. | Free; return false. |
| MULTI-13 | Connection and lobby | The lobby creation and `SetConnectionSettings` HRESULTs are ignored; Connect's second `QueryInterface` failure is consumed; a later failure leaks the lobby interface; explicit-connection startup returns -1 even after success (and a copy failure leaks the block); create's first query failure leaks a reference; `multip_session_host` reads the interface's vtable before writing (a null interface faults first). | Failures; every explicit start. | Check and release. |
| MULTI-14 | Player details | Refreshes with an unsigned elapsed time and a strict greater-than against `period * 1000` (wrap makes a refresh immediately due); removing a player leaks the allocated strings on an early send/free error. | A tick wrap; an early error. | Use signed arithmetic; free. |
| MULTI-15 | The weather-button entry | Transmits four caller-stack bytes at the end of message `1014`. | Every send. | Zero them. |

## Views.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| VIEW-1 | Validation of the Map and Spot pages | Both the map metric flag (invalid unit selector) and the zoom float (failed `sscanf`) read an uninitialised local at **entry ESP-16**; the harness paints that slot. | An invalid unit selector or a failed `sscanf`. | Initialise both. |
| VIEW-2 | Error reporting | Returns the result of the inherited virtual at `+0xe0`, not the id passed to it. | Every report. | Return the id. |
| VIEW-3 | The map flag | Only an exact map flag of 1 chooses the Map radio, while validation skips numeric checks for any nonzero map flag; spot and track use record modes 4 and 3 but their radio positions are 3 and 4. | A map flag other than 0 and 1. | Use one test. |

## Help.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| HELP-1 | About's registry reads | There is no `RegCloseKey`: the key handle leaks. | Every About. | Close the key. |
| HELP-2 | About's Name buffer | Is a **local stack buffer** published through descriptor `+8`: a dangling pointer once population returns. | Every About. | Copy it into the descriptor. |
| HELP-3 | The cached font | Initialisation sets bit 0 of its flag **before** requesting base font 11 and constructing the CSimsFont: a throw leaves the bit set and the font unbuilt. | A throwing construction. | Set it after. |
| HELP-4 | Credits | The library and window creation are not checked before the stream is loaded; the stream callback clears the returned byte count after a `ReadFile` failure but always returns zero, presenting EOF rather than an error. | A missing `RICHED32.DLL` or `fltcred.rtf`. | Check; return the error. |
| HELP-5 | Credits' scroll | Compares the first visible line with `+0x10f0` with a **signed** strict greater-than; Pause toggles only bit 0 using XOR, including for non-boolean input. | Always. | Compare as unsigned; normalise the flag. |

## Alerts.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| ALERT-1 | The modeless alert's descriptor (id 3, via UTIL `+0xa8`) | UTIL allocates the 0x28-byte descriptor and initialises only the live flag; the progress fields are left as malloc returned them. | Every modeless alert. | Initialise them. |

## World.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| WLD-1 | The scenery layers' populate | Invalid later layers reuse the previous formatted position as type text; an invalid first layer reads uninitialised scratch text. | An invalid layer. | Initialise the scratch. |
| WLD-2 | The error display | Ignores its supplied argument; builds a three-word record at descriptor `+0x18` and leaves that stack pointer at `+0x1c` after return; returns the result of the virtual focus call at `+0xe0`, including when no error exists. | Every error. | Copy the record; return a status. |
| WLD-3 | The files list's protection read | When removing the last selected item leaves `LB_GETCURSEL` at -1, the protection read is unchecked (`-1`/out-of-range rows); double-click keeps a stale selection and descriptor protection. | Delete of the last item; double-click after a change. | Check for `LB_ERR`. |
| WLD-4 | The search history | The apparent history limit at index 100 aliases object `+0x1420`, the history index itself: storing that entry's mode changes the index used for the next resource store. | A history of 100 entries. | Bound the history below the index. |
| WLD-5 | The mask bitmaps | Transitions load replacement masks without deleting the earlier handles (Close deletes only the current one): a leak. | Every transition. | Delete the old mask. |
| WLD-6 | Output formatting | Writes `_itoa` text through a newly constructed CString's data pointer, and tests `(flags & 8) && (flags & 0xf982)` for priority. | Every runway line. | Format into a buffer. |
| WLD-7 | Release | Reloads the descriptor and row base before every pointer-to-pointer free and keeps each list's signed count: negative or extreme counts free wild pointers. | A corrupt descriptor. | Bound the counts. |

## MP_DIALOG.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| MPD-1 | The factory, after a failed template `Create` | Does not clean up: the connection object stays in slot 3 with a null HWND, the factory goes on to create the other templates and returns `[intro, choice, host, NULL, browser, NULL]`. | A failed `Create` of one dialog. | Clean up and fail. |
| MPD-2 | The helpers after `Create` | Call `ShowWindow` without checking the result and then read `this+0x20`; with a failed `Create` the object has been freed (its destructor returns the same pointer) and MFC reads `this+0x3c` of a freed object (a post-free access). | A failed `Create` of the Intro or Settings first child. | Check the result. |
| MPD-3 | The Choice error handler, `0x22345a40` | Tests its incoming field first (zero returns 0 without calling the error helper); with a nonzero field and descriptor `+0x28` clear calls `+0xe0(0)`; saves the helper's result at `+0x8c`; leaves `+0xe0`'s return in EAX at `0x22342bad` and `0x22342c2f`. | Every Choice error. | Return a defined result. |
| MPD-4 | The remove callback and the refresh | On a send failure returns early without freeing the already-created confirmation strings; the refresh test subtracts the marker (`ticks() + 2000`) as an unsigned value before comparing it to the interval, so near wraparound a refresh is immediately due. | A failed send; a tick wrap. | Free the strings; compare signed. |

## Modules with nothing to list

LANDME and the eight normal aircraft models (`.MDL`) record no retail bug. Sim.DLL and the
installed dialogs not named above (Options, Flights, SIMSUI) are in their own sections. The
MDLs' crash models, the gauges and the runtime are the first sections of this file.

When a module doc gains a "retail behaviour" bullet that is a defect (an unchecked length, a
leftover register, a wrong operand, a leak), add it here with an id and mark the code.
