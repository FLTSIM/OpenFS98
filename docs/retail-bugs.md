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

Only those met while doing the dialogs; the rest are still to be collected (below).

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| MAIN-1 | `lb_button`, `.text+0xf800`; `src/main/main_logbook.inc` | After deleting a logbook row (operation 6) or renaming a logbook (8), a failed write answers success and a successful one answers -1: the test is the wrong way round. | Every row delete and rename; the Logbook ignores the answer to 6, and to 8 shows MAIN's error only when it says failure. | Answer -1 when the write fails. |

## Flights.DLL

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| FLT-1 | Select Flight, `+0x20` of its descriptor; `src/flights/flights_select.c` | Reads, and on Delete decrements, a word past the end of MAIN's eight-word block: whatever MAIN's stack holds there. (Our MAIN now provides a ninth word, as retail's frame happens to: see [main.md](modules/main.md).) | Every Select Flight. | Make the word part of the block MAIN defines. |
| FLT-2 | Select Aircraft, `src/flights/flights_aircraft.c` | Stores a selection of -1 and reads the record at -1, before the list. | No aircraft selected. | Ignore -1 rather than index with it. |
| FLT-3 | Factories, `src/flights/flights_objects.c` (and Options' `src/options/options_objects.c`, and Display options' pages, `src/options/options_display.c`) | A failed allocation leaves the object null and the next store faults; Display options calls `CSimsTabPage::Init` on a page it failed to allocate. | Out of memory when opening a dialog. | Fail the dialog instead of storing through null. |

Behaviour that is odd but harmless (Navigation's leftover "ValidateData" message box that
always passes, OnOKs whose second check is never reached) is documented in
[flights.md](modules/flights.md) and is not listed as a bug.

## SIM1.sim

| Id | Where | What retail does | When | Fix at porting |
| --- | --- | --- | --- | --- |
| SIM1-AIR-SIZE-HANDLE | AIR file loader `0x2200aef3 -> 0x2200af7e`; `src/sim1/sim1_air.inc` | Returns zero without calling HOST `+0x13c`, leaking the already opened file handle and its host block. | The AIR file opens successfully but HOST `+0x1a4` fails to obtain its size. The isolated caller harness confirms that ownership remains open on this exit. | Close the opened file on the size-error path before returning. |
| SIM1-AIR-UNCHECKED-TABLE | AIR interpolation `0x22011660`, unchecked access at `0x22011692`; `tools/gen_sim1_lift.py` | Uses `(count-1)*16` to read a table entry without validating the count or table format. | Omitting engine-type record `0x105` from the Bell AIR file selects a table interpretation that faults during postprocessing. Decoder comparisons reproduce the access fault and preceding state in all twelve FP modes. | Validate engine type and interpolation table dimensions before looking up entries. |
| SIM1-SCALE-DISCARDED | Rudder input `0x220051dc`; `src/sim1/sim1_small.c` (`sim1_rudder_input`) | With GLOBAL `+0x278` bit 0 set and `+0x366` clear it scales the rudder value by `0x1999 / 65536` (a tenth), then pops the unscaled word back over the result, so the scaling never takes effect. Our C leaves the dead arithmetic out. | Always, under those two flags. | Decide whether the tenth was meant (a ground-steering reduction?) and apply it, or drop the test. |
| SIM1-GOVERNOR-RPM (suspected) | Propeller governor `0x2200ea69` and its pass `0x2200e744` (the same loop); `src/sim1/sim1_small.c` (`sim1_governor`, `sim1_governor_pass`) | For an engine whose rpm word (scaled by `+0x439a`) is below `+0xac7`, the throttle should keep its value; instead the reused register still holds the scaled rpm, which is clamped to 0..0x3fff and written as the throttle. | An engine under the governor's minimum rpm while the governor is on. | Leave the throttle alone below the threshold. |
| SIM1-AUTOTHROTTLE-ENGINES (suspected) | Autothrottle modes `0x2200e4fa`; `src/sim1/sim1_small.c` (`sim1_autothrottle_modes`) | The governor's engagement test on the ground should take the highest rpm over the engines fitted (`+0xaec`); the count tests are inverted (`jge` for `jl`), so with two or more engines only the first engine's rpm counts, and a single looks at all four rpm words. | The autothrottle armed on the ground outside takeoff mode, multi-engine with an engine other than the first fastest, or a single with stale words for engines 2 to 4. | Test `index < count` for each further engine. |
| SIM1-GEAR-HORN-ENGINES (suspected) | Landing gear `0x220017f0`; `src/sim1/sim1_small.c` (`sim1_landing_gear`) | In the air with a leg not down and the flaps past the approach setting, the horn should sound for any engine's throttle below the closed threshold; a throttle at or above it on engine 2 ends the test (`jge` out of the chain), and with one engine the count tests skip the rest, so engines 3 and 4 are never looked at. | Three or four engines, gear up, flaps partly out, engines 1 and 2 above idle and 3 or 4 closed. | Test each fitted engine's throttle. |
| SIM1-AIR-FRAME-OVERRUN (fixed, not kept) | AIR decoder `0x2200af90`, record 0x302; `src/sim1/sim1_small.c` (`sim1_air_decode`) | The contact stiffness record is copied with its own length into a 0x1c-byte frame buffer: a longer record overruns retail's stack (its saved registers and return address), a shorter one leaves stale frame bytes in the seven words. The C copies at most 0x1c bytes into a zeroed buffer; a stack overrun cannot be reproduced. | A malformed or modified .AIR file with a 0x302 record not 0x1c bytes long. | Check the length. |
| SIM1-SLEW-HEIGHT-EAX | CHAIN tick `0x2200a84b` (`0x2200a8b4`); `src/sim1/sim1_small.c` (`sim1_chain_tick`) | Slewing in the height mode (GLOBAL +0x366), the height's 48-bit position (+0x572, +0x574) is GLOBAL +0x1c:+0x20 shifted right 24 plus +0x120c; retail loads only the high dword into edx, so the `shrd eax,edx,0x18` takes the fraction's low byte from whatever is in eax: the callback's second argument, or after a slew frame the record words' `_ftol` result. | Every slew frame in the height mode: the low 8 bits of the height's fraction word are stale. | Load the position's low dword (presumably GLOBAL +0x1c) into eax first. |

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
