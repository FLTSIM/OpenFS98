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
parsers and heap, fonts, remapping and DllMain, CDIBPal, CDIBSection, CSimsFont (but DrawSimsText's
operation), CNotify (but Send's), and the small accessors and plain controls. All agree over 1000+
scenarios. Retail bugs SUI-1 to SUI-5 registered; SUI-6 and SUI-7 are marked in the code but not yet
registered. Left: the controls from CSims3PieceButton (0x2390) on, CSimsDialog, the tab classes, the
native DLL, in-game checks.
