# Localised dialog DLLs

The retail English UI contains **64 registered dialogs in eight DLLs**, with
340,814 bytes of `.text`. Together with MPCHAT's chat window this accounts for
all **65** dialogs previously observed in the host's live registry. This is a
static survey of the reference binaries, not a reimplementation or a claim of
UI behavior verified against retail.

`tools/dialogmap.py` now covers the part of the module graph that `make graph`
misses. It reads the English DLLs, root `SIMSUI.DLL`, `DIALOG.DLL` and
`ENGLISH.FLL`, and `Modules/mpchat.DLL`, without loading or changing any DLL.
Only Python's standard library and, by default, `objdump` are needed. It does
not boot Wine, install modules, or share harness outputs with MAIN.

```sh
python3 -B tools/dialogmap.py
python3 -B tools/dialogmap.py --no-disassemble
python3 -B tools/dialogmap.py --callers --format json > /tmp/dialogmap.json
python3 -B tools/dialogmap.py --format registry > /tmp/dialog_registry.tsv
diff -u docs/dialog_registry.tsv /tmp/dialog_registry.tsv
python3 -B tools/test_dialogmap.py
```

`--orig PATH` selects a different reference tree. JSON includes source hashes,
module linkage, every named PE import and IAT address, complete dialog control
metadata, factory creation calls, recognised MFC message maps and optional
caller sites. The TSV is the small, committed registry inventory; it contains
addresses and identifiers, not copied templates, text strings or artwork.

## Boundaries and dependencies

Every English DLL has exactly two named exports, `ImportTable` and `Linkage`.
Each imports **only HOST (1) and GLOBAL (2)** through the FS98 service ABI.
This does not mean the UI is self-contained: every one also has static PE
imports from SIMSUI, MFC42, MSVCRT, KERNEL32 and USER32. Alerts and World also
use GDI32; Help uses ADVAPI32 and COMCTL32; World uses COMCTL32. The input
parameter blocks carry callbacks into the simulator modules, so those modules
need not appear in the UI DLL's `ImportTable`.

All addresses below are preferred virtual addresses in the retail reference,
not portable pointers or addresses to assume after ASLR/relocation. Resource
counts are leaves, including language variants, not an assertion that each
resource is opened during a flight.


| File | FS98 ID | .text bytes | Registered dialogs | Templates | MFC maps / entries |
| --- | --- | ---: | ---: | ---: | ---: |
| `Alerts.DLL` | `0x80000009` | 7,649 | 3 | 4 | 3 / 8 |
| `Flights.DLL` | `0x80000002` | 51,122 | 13 | 17 | 16 / 87 |
| `Help.DLL` | `0x80000008` | 10,657 | 5 | 6 | 5 / 16 |
| `Options.DLL` | `0x80000007` | 84,381 | 15 | 31 | 28 / 139 |
| `Sim.DLL` | `0x80000004` | 33,239 | 5 | 9 | 8 / 29 |
| `Views.DLL` | `0x80000005` | 13,090 | 1 | 2 | 1 / 19 |
| `World.DLL` | `0x80000006` | 115,275 | 19 | 25 | 24 / 161 |
| `mpdialog.dll` | `0x8000000a` | 25,401 | 3 | 7 | 7 / 51 |
| `SIMSUI.DLL` | `0x80000000` | 79,850 | 0 | 3 | 27 / 110 |
| `DIALOG.DLL` | — | 406 | 0 | 1 | 0 / 0 |
| `ENGLISH.FLL` | — | 438 | 0 | 0 | 0 / 0 |
| `mpchat.DLL` | `0xffffffff` | 3,104 | 1 | 0 | 0 / 0 |

The eight English DLLs contain 101 template leaves; the three supporting files
add four. MPCHAT builds a Win32 window directly and has no dialog-template
resource. Across the twelve surveyed files there are **105 template leaves,
1,218 controls, 119 recognised MFC message maps and 620 map entries**. Of the
105 templates, 64 are extended and 41 standard. The English modules alone
contain 92 of those maps and 510 entries.

`SIMSUI.DLL` is module `0x80000000`, with 540 named exports and no registered
dialogs. Its exported C++ classes implement the common dialog and control
widgets. `DIALOG.DLL` and `ENGLISH.FLL` have no Linkage or ImportTable exports:
they are supporting resource libraries, not missing numbered simulator
modules. `DIALOG.DLL` holds 24 bitmap leaves; `ENGLISH.FLL` has 123 string-table
leaves, a menu and three accelerator-table leaves. No module `0x80000003`
appears in this reference; no identity is inferred for the gap.

The installed multiplayer filename is `English/MP_DIALOG.DLL`; the extracted
CD reference is `English/mpdialog.dll`. Keep this rename separate from
`Modules/mpchat.DLL`, whose module id is `0xffffffff` and dialog id `0xff00`.

## Registry and factory ABI

The host scanner at `0x407b20` reads `Linkage+0x14`. Its registration routine
at `0x409c10` walks 12-byte records with fields at:

| Offset | Meaning |
| --- | --- |
| `+0x00` | Full 32-bit registry id |
| `+0x04` | Factory function pointer |
| `+0x08` | Host dialog flags |

**Only the id is tested for termination.** Do not require all three fields to
be zero, cap the table at 32 records, or truncate the id to 16 bits. For
example, `0x1308`, `0x13080001` and `0x13080008` are separate registrations.
The Windows template id used by their factories is yet another namespace.

The factories receive `(parent_hwnd, parameter_block)` and return a window
handle, or a zero-terminated array of handles for flag 2. All 65 factory
listings reach a first `ret 8`; this observation alone is not a proof of every
exception path. The MFC factories generally construct a dialog object, retain
the supplied parameter block, create a modeless Windows window from a
resource, show it and return the object's HWND at `+0x20`. The **host** applies
the modal loop according to the registry flags; the resource itself is not
sufficient to decide whether the simulator's call is modal.

The complete extracted records, including original table order, flags,
factory addresses and template ids, are in
[`../dialog_registry.tsv`](../dialog_registry.tsv). On the reference, the
nonzero flags are exactly:

| Owner / registry id | Flags | Meaning in the host |
| --- | --- | --- |
| Flights `1` | `4` | Modeless |
| Alerts `3` | `4` | Modeless |
| mpdialog `0x110a` | `2` | Factory returns an array of windows |
| MPCHAT `0xff00` | `0xc` | Modeless, with bit 8 bypassing the usual pause/disable gate |

Every other record has flags 0. The host contract, including its nested modal
stack, modeless list, module lifetime and idle tick, is described in
[host.md](host.md#the-dialog-engine).

Factory-to-template extraction follows imported creation calls rather than
matching arbitrary immediate constants. The common entry is **MFC42 ordinal
2086**: the CD's implementation at `0x5f406539` passes its first argument to
an `RT_DIALOG` (5) resource lookup. The scanner resolves direct IAT calls and
one local jump thunk to the IAT. It also recognises named
`CSimsDialog::Init` imports. Ordinal 2086 is a fact about this retail ABI,
not a portable MFC ordinal-name database.

This resolves all 63 ordinary English registry factories. The 64th is the
multiplayer array factory below. MPCHAT's direct Win32 factory deliberately
has no template result. JSON records each template call's address and import
so every association can be checked in disassembly.

## Dialog identities

These descriptions identify a screen or family from its controls, its
factory and, where already known, its simulator caller. They are explanatory
labels, not extracted titles or proof of the callbacks' behavior. Numeric
fields come from the registry/tool; regenerate their canonical form with
`--format registry`.


### Alerts.DLL

| Registry id | Template id(s) | Description |
| --- | --- | --- |
| `0x00000002` | `0x8a` | Modal alert |
| `0x00000003` | `0x8b` | Modeless alert / progress |
| `0x00000004` | `0x8c` | Advisory with suppression checkbox |

### Flights.DLL

| Registry id | Template id(s) | Description |
| --- | --- | --- |
| `0x00001101` | `0x1fe` | Create flight |
| `0x00001102` | `0x208` | Select flight |
| `0x00001104` | `0x207` | Save flight |
| `0x00001108` | `0x200` | Adventures |
| `0x00001107` | `0x201` | Challenges |
| `0x00001201` | `0x1fb` | Select aircraft |
| `0x00001202` | `0x205` | Navigation / radios |
| `0x00001203` | `0x1fd` | Radio communications |
| `0x00001204` | `0x1fc` | Autopilot |
| `0x00001109` | `0x206` | Opening flight menu |
| `0x00000001` | `0x20a` | Empty modeless flight window |
| `0x11090002` | `0x1ff` | Flight-selection menu |
| `0x11090001` | `0x209` | Learning menu |

### Help.DLL

| Registry id | Template id(s) | Description |
| --- | --- | --- |
| `0x00001607` | `0x2bd` | About / product information |
| `0x16070001` | `0x2be` | Credits |
| `0x00001106` | `0x2c1` | Flight lessons |
| `0x14050004` | `0x2bf` | Lesson analysis graph |
| `0x11060001` | `0x2c0` | Lesson completion |

### Options.DLL

| Registry id | Template id(s) | Description |
| --- | --- | --- |
| `0x00001404` | `0x3a2` | Flight video |
| `0x00001403` | `0x391` | Instant replay |
| `0x00001405` | `0x38e` | Flight analysis |
| `0x00001407` | `0x392` | Logbook |
| `0x00001409` | `0x39b` | Preferences |
| `0x14070001` | `0x394` | Add logbook |
| `0x14070002` | `0x393` | Logbook entry |
| `0x14040001` | `0x3a1` | Save video |
| `0x14090001` | `0x385` | Display options |
| `0x14050002` | `0x38a` | Landing analysis graph |
| `0x14050003` | `0x38b` | Manoeuvre analysis graph |
| `0x14090002` | `0x39c` | Select demo |
| `0x14040003` | `0x39f` | Video finished |
| `0x14040002` | `0x3a0` | Video message |
| `0x0000140a` | `0x3a5` | Customise controls |

### Sim.DLL

| Registry id | Template id(s) | Description |
| --- | --- | --- |
| `0x00001206` | `0x452` | Aircraft settings, initial page 0 |
| `0x00001207` | `0x452` | Aircraft settings, initial page 2 |
| `0x00001208` | `0x452` | Aircraft settings, initial page 3 |
| `0x00001209` | `0x452` | Aircraft settings, initial page 5 |
| `0x14050001` | `0x44d` | Crash analysis |

### Views.DLL

| Registry id | Template id(s) | Description |
| --- | --- | --- |
| `0x00001508` | `0x5e1` | View options |

### World.DLL

| Registry id | Template id(s) | Description |
| --- | --- | --- |
| `0x00001302` | `0x6a6` | Basic weather |
| `0x00001301` | `0x6aa` | Time / season |
| `0x00001303` | `0x6ba` | Weather areas |
| `0x00001304` | `0x6b1` | Scenery complexity |
| `0x00001305` | `0x6b2` | Dynamic scenery |
| `0x00001306` | `0x6b6` | Scenery library |
| `0x13040001` | `0x6a5` | Advanced weather |
| `0x13080001` | `0x6b5` | Scenery area list |
| `0x13080006` | `0x6ae` | Performance booster list |
| `0x13080007` | `0x6ab` | Edit booster |
| `0x13080008` | `0x6ab` | Add booster |
| `0x13080004` | `0x6b3` | Edit scenery area |
| `0x13080003` | `0x6b3` | Add scenery area |
| `0x13080002` | `0x6b0` | Find scenery areas on a drive |
| `0x13080005` | `0x6b4` | Scenery area priority |
| `0x13050001` | `0x6a8` | Copy weather |
| `0x00001309` | `0x70d` | Go to airport |
| `0x00001308` | `0x70c` | Exact location |
| `0x00001307` | `0x6bf` | Facility directory |

### mpdialog.dll

| Registry id | Template id(s) | Description |
| --- | --- | --- |
| `0x0000110a` | `0x68`, `0x67`, `0x69`, `0x66`, `0x65` | Five-window multiplayer connection flow |
| `0x0000110b` | `0x6a` | Session settings |
| `0x0000110c` | `0x6b` | Session details |

### mpchat.DLL

| Registry id | Template id(s) | Description |
| --- | --- | --- |
| `0x0000ff00` | — | Multiplayer chat |

### The multiplayer array factory

`mpdialog:0x110a` at `0x22341130` creates five ordinary dialogs, stores their
HWNDs in globals `0x2234a0b8` through `0x2234a0c8`, writes a null terminator
at `0x2234a0cc`, and returns the array. They are created in this order:

| Helper factory | Template | Role |
| --- | --- | --- |
| `0x223414e0` | `0x68` | Connection type |
| `0x22341440` | `0x67` | Host limits |
| `0x22341580` | `0x69` | Join a session |
| `0x223413a0` | `0x66` | Callsign and host/join choice |
| `0x22341300` | `0x65` | Connection introduction |

If parameter `+0x1c` is zero, the factory sends `WM_COMMAND` (`0x111`) with
command `0x3e8` to the last-created window. The full transition and validation
behavior is not yet recovered. This is why counting one template per registry
record would miss four screens in this module.

### Reused templates and child pages

Sim's `0x1206`..`0x1209` all create template `0x452`, store the descriptor at
object `+0x10ec`, and select an initial page at object `+0x1fa8`: respectively
0, 2, 3 and 5. They are not four copies of the template. Child pages cover
realism, engine controls, fuel, instruments, failures and reliability.

World's add/edit area dialogs share `0x6b3`; add/edit boosters share `0x6ab`.
Options' `0x140a` creates container `0x3a5`, while the controls, assignments
and force-feedback pages are resources `0x3a3`, `0x3a4` and `0x3a7`.
The factory-to-template list is not a complete graph of tab/page creation.
The JSON template inventory includes those child resources independently.

## Simulator callers and callback contracts

The optional `--callers` scan reports **63 call sites**, 59 with a locally
resolved id, representing **55 distinct registry ids**. It follows a host
ImportTable pointer within a basic block to `+0x10c` or `+0x110`, and records
the last explicit push. It also scans direct calls to host `0x409720` and
`0x409bd0`. It discards register/constant provenance after calls and branches.
This deliberately misses pointers held across calls, jump tables, and ids
passed to helper functions. It is a lower-bound survey, not code coverage;
an unresolved or absent id is not evidence that a dialog cannot run.


| Caller | Sites | Locally resolved |
| --- | ---: | ---: |
| `Options.DLL` | 2 | 2 |
| `FLTSIM98.EXE` | 2 | 0 |
| `ADVDRV.DLL` | 2 | 1 |
| `ANALYSIS.DLL` | 3 | 3 |
| `CONTROLS.DLL` | 1 | 1 |
| `DEMO.DLL` | 5 | 5 |
| `FE.DLL` | 9 | 9 |
| `MAIN.DLL` | 28 | 27 |
| `REALITY.DLL` | 1 | 1 |
| `UTIL.DLL` | 2 | 2 |
| `facil.dll` | 3 | 3 |
| `mpchat.DLL` | 1 | 1 |
| `multip.dll` | 2 | 2 |
| `SIMSUI.DLL` | 2 | 2 |

The MAIN records here are observations of the **retail input binary**. This
survey changes no MAIN code, harness or documentation.

Several parameter-block contracts are already documented and exercised from
the simulator side. These are the strongest starting points for UI harnesses:

- [UTIL alerts](util.md) and `src/util/util_alert.c`: dialogs 2 and 3; modal
  answer at `+0x0c`, modeless lifetime at `+0x0c`, text at different offsets.
- [CONTROLS](controls.md):
  `0x140a`; sensitivity reset at parameter `+0x1c`, device recount at `+0x20`,
  and shared lists/settings edited by the UI. See its key-assignment section.
- [FE](fe.md): `0x1306` and `0x13080001`..`0x13080008`; callbacks enumerate,
  validate, add/edit/delete, browse, reorder and flush scenery/cache records.
- [FACIL](facil.md): `0x1307`..`0x1309`; validation, placement, searching,
  lists of facilities and runways, and nested directory dialogs.
- [DEMO](demo.md), [ANALYSIS](analysis.md), [REALITY](reality.md) and
  [MULTIP](multip.md) own the remaining established video, graph, crash and
  session parameter contracts. Their existing stand-ins are input to a UI
  harness design, not verification of the real English DLLs.

## MFC event maps

JSON includes 119 recognised maps. The extractor finds literal map-address
getters ending in `ret`, then validates their base getter and terminated
24-byte entry arrays:

| Entry offset | Raw field |
| --- | --- |
| `+0x00` | Windows message |
| `+0x04` | Notification code |
| `+0x08` | First control/command id |
| `+0x0c` | Last control/command id |
| `+0x10` | MFC signature selector |
| `+0x14` | Handler address |

Each nonempty accepted entry must have a nonzero 16-bit message, an ordered
id range and an executable local handler. The base getter is a local thunk
that can return an imported SIMSUI map. This identifies events to drive, not
handler semantics, handler prototypes or complete reachability. Registered
messages and other compiler layouts may escape recognition. Empty maps and
inherited methods still matter. Do not treat 620 entries as 620 independent
functions or as all methods in the UI.

## Resource parsing and validation

The parser preserves resource language ids, string-versus-ordinal class/title
fields, signed dialog coordinates, font fields, extended 32-bit control ids,
and DWORD alignment between controls. It rejects cycles, out-of-bounds
resource-directory offsets, truncated templates and reads from unbacked
virtual section tails. It parses standard and extended control creation data
with their different size conventions, following Microsoft's
[DLGITEMTEMPLATE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-dlgitemtemplate)
and [DLGITEMTEMPLATEEX](https://learn.microsoft.com/en-us/windows/win32/dlgbox/dlgitemtemplateex)
format descriptions.

The 12 synthetic regressions need no game content. They exercise a 65-record
registry with 32-bit ids, a zero-id terminator with nonzero trailing words,
invalid factory pointers, truncated images, virtual tails, resource-tree
cycles and escaped offsets, a named Unicode resource with 40 language leaves,
both template formats with Unicode, ordinals,
alignment and creation data, a negative factory-call match, validated message
maps, and caller provenance/unknown ids. A coincidental `push` of a template id
must not be accepted as a creation call.

Acceptance on the local reference:

- All twelve files parse in static-only and disassembled modes.
- The generated TSV matches the committed registry inventory.
- Every one of the 68 English factory/template associations (63 ordinary plus
  five multiplayer child factories) names a resource present in its owning
  DLL. MPCHAT has no resource association.
- Controls `0x140a` resolves to the previously documented factory
  `0x22391930`; FACIL and FE ids agree with their existing module documents.
- All three Alerts message maps were independently checked in `.rdata`, and
  the host's record stride and id-only terminator were checked in disassembly.
- All 490 resource leaves displayed by GNU `objdump -x` agree with the new
  parser's RVAs and sizes. That display is incomplete; see below.

**Do not use `objdump -x`'s resource tree as the full resource count.** The
local GNU Binutils 2.42 display stops Alerts after 25 leaves, while the raw
directory declares 29. At `.rsrc+0x00` there are six root types. The string
directory at `+0xe0` declares 13 entries, including the final entries at
`+0x148/+0x150` that the display omits; it also omits the group-cursor and
version roots at `+0x30/+0x38`. Their data is intact. Across the twelve files,
the new parser walks 555 leaves while this objdump display shows 490. All
displayed pairs agree, but agreement on that subset is not a full independent
resource-directory validation. The synthetic traversal test and direct raw
directory checks guard against mistaking a short listing for a small module.

Those inventory checks are static analysis. The separate Alerts and Views
implementations now have differential suites and isolated startup tests; the
inventory tool itself does not build or install replacements. Per-file SHA-256 hashes in JSON identify the exact source
images for a later comparison. A different pressing must be surveyed again.

## Work to take next

**Alerts is implemented** as a standalone replacement DLL: factories,
object lifetime and exception cleanup, initialization, commands, progress,
MFC dispatch and module lifetime. [alerts.md](alerts.md) records the synthetic
and native differential suites, deliberate mutations and isolated startup
smoke. Build with `make -f tools/alerts.mk alerts`; shared build integration
is separate from MAIN's current work. It still uses the CD's SIMSUI/MFC42.

**Views is implemented** as a standalone replacement, including its preview
callback lifecycle, all 19 message handlers, numeric editing and validation,
control setup, object lifetimes and module ABI. [views.md](views.md) records
its synthetic and native differential suites, deliberate errors and isolated
startup test. It retains the CD's SIMSUI/MFC42, and the native preview renderer
is a stateful caller fixture; the actual renderer and application of edits
belong to MAIN/WINDOW. Build with `make -f tools/views.mk views`.

**Help is in progress** independently of MAIN/HOST. Its five dialogs' event,
initialization and object-lifetime core is implemented and differentially tested;
it is not yet a replacement DLL. [help.md](help.md) records the registry, RichEdit,
timer and callback contracts, synthetic results and remaining native acceptance.
Build the core with `make -f tools/help.mk helpevents`.

**Options/World in slices**: start with CONTROLS, FE or FACIL's already
specified descriptor contracts. Keep template creation, exchange-engine
validation, accept/cancel ordering, callbacks and nested-dialog lifetime in
scope. Replacing an MFC surface is much larger than copying its labels.

