# SIMSUI.DLL

`orig/SIMSUI.DLL`, preferred base `0x20470000`, 0x137ea bytes of `.text` (about 80 KB). **Started
2026-11-07**, after Options. FS98's widget library: an MFC extension DLL of skinned dialog and
control classes that all eight `English\` dialog DLLs build on (retail's and ours alike). It is
also an FS98 module, with a `Linkage` and an `ImportTable` like the others.

## Exports and constraints

540 exports: 448 functions and 92 data.

- **Data:** each class's vtable (`??_7...@@6B@`), message map and its entries, three
  `CRuntimeClass`es (`CSims3PieceButton`, `CSimsDialog`, `CSimsTabPage`), and four statics:
  `CDIBSection::ms_nShadowFactor`, `CDIBSection::ms_cbShadow`, `CSimsButton::ms_ObjectCount`,
  `CSimsDialog::m_hInstMainDLL`.
- **The dialog DLLs depend on the layouts.** They derive their classes from these, embed these
  classes as members (their constructors call SIMSUI's), and copy SIMSUI's vtables as data imports
  (Options' inline spin buttons and scroll bars store `??_7CSimsSpinButtonCtrl@@6B@` and
  `??_7CSimsScrollBar@@6B@`). A replacement must keep every object layout, every vtable's slots
  and every message map exactly, and export the same mangled names, data included (a native DLL can
  fill its vtables in `DllMain`, before any importer runs).
- The English DLLs import 128 distinct exports directly; the vtables and message maps reach many
  more.

Imports: MFC42 (189), USER32 (51), GDI32 (26), MSVCRT (14), KERNEL32 (10), IMM32 (5).

## The classes

Approximate code per class, from the exports' addresses:

| Class | Bytes | Exports | Class | Bytes | Exports |
| --- | --- | --- | --- | --- | --- |
| CSimsDialog | 10,176 | 42 | CSimsGroupbox | 1,760 | 6 |
| CSimsTabCtrl | 8,336 | 24 | CSimsComboBox | 1,664 | 10 |
| CSimsHotJoy | 5,616 | 11 | CSimsEdit | 1,504 | 11 |
| CDIBSection | 5,424 | 33 | CSimsTabPage | 1,456 | 22 |
| CSimsSlider | 5,312 | 20 | CSimsText | 1,456 | 12 |
| CSims3PieceButton | 4,080 | 17 | CSimsCheckListBox | 1,376 | 9 |
| CSimsRadioButton | 3,360 | 21 | CSimsListBox | 1,344 | 9 |
| CSimsCheckButton | 3,056 | 19 | CSimsButton | 1,248 | 11 |
| CSimsShadowButton | 2,960 | 8 | CSimsHotKey | 1,152 | 16 |
| CSimsPicture | 2,592 | 15 | CSimsTabCtrlHelper | 464 | 10 |
| CSimsTextArt | 2,512 | 6 | CNotify | 352 | 5 |
| CSimsFont | 2,176 | 6 | CSimsHelpButton, CSimsExitButton | 320, 304 | 8, 8 |
| CDIBPal | 2,160 | 9 | CSimsStatic, CSimsSpinButtonCtrl, CSimsScrollBar | ~200 each | 5 each |
| CSimsShadow | 1,808 | 5 | CSimsPictureView | 144 | 4 |

and free functions: `ui_new`/`ui_new_db`/`ui_delete` (SIMSUI's heap), `GetSimsBaseFont`,
`AddFont`, `ClearSimsFonts`, `RemapCtrlRect`/`RemapCtrlPt`/`RemapCtrlPtWth` (dialog-unit
remapping), `SIMSUI_ExitCleanly`/`SIMSUI_ExitCleanlyE`.

## Method

As Options' ([options.md](options.md)): the classes transliterated a slice at a time into C over a
service table (`src/simsui/`), and a differential harness that maps retail SIMSUI without its
imports, points every import slot at a logging stub, and runs retail's routines and ours over the
same randomised objects (`tools/difftest/simsuidiff.c`), with deliberate breaks; then a native DLL
that exports the same names. Slices from the leaves up: the free functions, CNotify, CDIBPal,
CDIBSection and CSimsFont first; the controls next; CSimsDialog and the tab classes last.

## Progress

2026-11-07 (overnight): the harness (`tools/gen_simsui_tables.py`, `tools/difftest/simsuidiff.c` with
`simsui_semantics.inc`, `simsui_fixture.inc`, `simsui_operations.inc`, `tools/simsui.mk`,
`tools/run_simsuidiff.sh`, `tools/simsui_breaks.py`) and 125 routines: the CRT helpers, the spec
parsers and heap, fonts, remapping and DllMain, CDIBPal, CDIBSection, CSimsFont, CNotify, and the
small accessors and plain controls.

2026-11-07 (day): **every routine is transliterated** (`src/simsui/`, one file per class family),
including the unexported CDIB class (`simsui_cdib.c`, the MFC samples' DIB, DYNCREATE but used by
nothing) and the CRT entry. `make -f tools/simsui.mk simsuidiff-run`: 444 operations, every exported
routine and every routine a vtable, message map or runtime class reaches, 2000 scenarios, 0 differ.
Retail bugs SUI-1 to SUI-12 are registered in [retail-bugs.md](../retail-bugs.md). Deliberate
breaks (`tools/simsui_breaks.py`, one suite per source file; run from a frozen copy): the last
suites, for the tab classes, Text, the shadow, HotKey/HotJoy, CDIB and CSimsDialog's OnActivate and
OnCommand, are 119 breaks, all caught (one only at N=2000) but one proven equivalent, after the
fixture was widened for the ten first missed (child windows enumerated through the callback, command
ids, the tab strip's frame count, CString lengths, static styles, header sizes, a file read from
part-way). The proven equivalents are listed in the script's docstring.

### The native DLL

`make -f tools/simsui.mk simsui` builds `bin/simsui-native/SIMSUI_ours.dll` (the shared build copies
it to `bin/SIMSUI.DLL`). Retail's data (vtables, message maps, runtime classes, strings, statics,
`Linkage` and `ImportTable`) is not rewritten: `tools/gen_simsui_native.py` takes retail's `.rdata`
and `.data` from RVA 0x15000 as an image (under `bin/`, never tracked) and lists in
`src/simsui/simsui_native_tables.inc` what each of the 2,129 pointers retail's relocations mark there
becomes at load (`src/simsui/simsui_native.c`):

- 130 into the image itself: the same RVA in our copy;
- 486 to routines: our transliteration of each, taken from the harness's operation table, so a
  routine is bound only if `simsuidiff` verifies it (the generator stops on any other);
- 1,234 to import thunks (MFC base classes' virtuals): the import itself;
- 279 into C++ exception tables (unwind funclets, handlers): 0, since only retail's code used them.

All 540 exports keep retail's names and ordinals: routines bound to ours, the 92 data exports to
their place in the image. Imports are found by name (MFC42's by ordinal) before anything runs; one
static import from each library keeps retail's load order. Retail's resources (but its version
block) are copied in. The entry transliterates the CRT's (`0x20483490`, `0x204833c0`: MFC's raw
DllMain, the onexit table, `_initterm` over retail's initialiser table, DllMain).

SIMSUI is a shared module (`SHARED_MODULES` in the Makefile since 2026-11-07, installed as
`SIMSUI.DLL` beside `FLTSIM98.EXE` by `make install`; alone by `make simsui-install`/`simsui-revert`).
`make main-dialogs` and `make flights-play` run ours on the "ours" side. With it, `main-dialogs`
passes as before: 47 commands, the same controls and text, 41 of 47 pictures identical (the six
others the clock, live previews and the crash's curve, as without it).
