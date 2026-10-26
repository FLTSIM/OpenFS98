# Help.DLL

The dialog core and standalone native replacement DLL are implemented. The
Win32/MFC runtime adapter binds all five factories, native vtables and message
maps, including the credits placeholder and the cached About font's unload
lifetime. MAIN, HOST and the shared Makefile are outside this work's scope.

Build and run independently:

```sh
make -f tools/help.mk help
make -f tools/help.mk helpuidiff-run
make -f tools/help.mk help-ui-breaks
make -f tools/help.mk help-smoke
make -f tools/help.mk helpeventsdiff-run
make -f tools/help.mk helpeventsdiff-run N=2000 SEED=0x7e51a23b MODE=unwind
make -f tools/help.mk help-breaks N=100
```

The runner uses `bin/help-diff/wineprefix` and its own lock. It neither installs
anything into `game/` nor uses another module's Wine prefix. The implementation
uses snake_case identifiers and standard `<stdint.h>` integer types.

## Reference and implemented boundary

Reference image base `0x22330000`, module id `0x80000008`, Linkage RVA `0x5078`.
Only HOST and GLOBAL appear in the numeric import table. Win32 dependencies
include ADVAPI32 registry access, USER32 timers/windows/help, KERNEL32 file and
library calls, and COMCTL32; the dialogs retain the CD's MFC42/SIMSUI contracts.

Registry order matters:

| Dialog | Registry id | Template | Object bytes | Descriptor offset | Factory RVA | Constructor / destructor RVA |
| --- | --- | --- | --- | --- | --- | --- |
| About | `0x1607` | `0x2bd` | `0x197c` | `0x10dc` | `0x11c0` | `0x1410` / `0x1530` |
| Credits | `0x16070001` | `0x2be` | `0x149c` | `0x1174` | `0x1250` | `0x1b70` / `0x1c50` |
| Lessons | `0x1106` | `0x2c1` | `0x1880` | `0x10dc` | `0x1130` | `0x2280` / `0x2390` |
| Graph | `0x14050004` | `0x2bf` | `0x12d0` | `0x10e0` | `0x12e0` | `0x2ae0` / `0x2b70` |
| Lesson completion | `0x11060001` | `0x2c0` | `0x1390` | `0x10dc` | `0x1370` | `0x2e30` / `0x2ed0` |

`src/help/help_events.c` implements all five initializers, help/context commands,
control exchange, accept/cancel/close paths, lessons list handling and path
checks, graph callbacks, and credits streaming/timers. `help_objects.c` implements
all five factories and object lifetimes, the credits placeholder's deleting
destructor, and class delete. `help_unwind.h` provides cleanup-only PE32 SEH
scopes. The generated includes contain control/help/lifetime ABI metadata;
`tools/gen_help_tables.py` extracts the control mappings and lifetime call lists
from retail and rejects unexpected layouts. No code or resource payload is
copied into these includes.

Factories ignore their parent argument, allocate the precise class size,
construct with a null parent, install the descriptor and module, call Create
and ShowWindow, then return object `+0x20`. Create/Show failures do not alter
that return. Null allocation still writes the descriptor through a null object;
the comparison harness records the resulting access violation on both sides.
Constructor exceptions destroy only completed subobjects in reverse order,
then the base. Factory cleanup uses CSimsDialog's class delete; deleting
destructors use `ui_delete`, while the placeholder uses MFC delete.

## About and its stack pointer

Registry reads use HKLM and
`Software\Microsoft\Microsoft Games\Flight Simulator\6.1`, access `0x20019`.
The first query is `Name`, capacity 99; the second is `ProductID`, capacity 49.
Open failure displays `Registry Error` in both controls. Individual query failure
displays `Failed Registry`. There is no RegCloseKey call: retain the retail leak.
The Name control is at `+0x1528`, ProductID at `+0x15e0`.

The Name buffer is also published through descriptor `+8`. It is a **local stack
buffer**, not persistent storage. `help_stack.S` reserves the original local
area and keeps that pointer at population entry ESP minus `0x270`. About's
initializer calls population with its own frame live, placing the pointer at
initializer entry ESP minus `0x294`. Both addresses are compared directly with
retail; the harness never dereferences that expired pointer after return.
Synchronous text setters compare the strings while their lifetimes are valid.

Population still constructs and destroys its unused CString. Exceptions after
construction run its destructor; a throwing destructor is not invoked twice.
It loads resource ids `0xf70b`, `0xf6fd`, `0xf70c`, formats major 6, minor 10,
build `0x1a2`, and appends the version before setting the controls.

The cached font occupies retail `0x51c8..0x5237`, with initialized flag `0x51c0`.
Initialization sets bit 0 **before** requesting base font 11 and constructing
CSimsFont. A throw leaves that bit set. Exit registration follows successful
construction; later initializers reuse the font. Foreground, background and two
font state fields are then reset before the art initializer. The harness restores
and compares the complete flag, padding and 0x70-byte font, tests repeated
initialization, and independently calls the registered destructor entry.
The native adapter retains the registered destructor and calls it once on DLL
unload, after terminating the MFC extension module, matching retail CRT order.

About's Credits callback takes its descriptor as one stdcall argument. MAIN's
callee `0x207e10f0` ends in `ret 4` and forwards it to dialog `0x16070001`.

## Credits

Credits' constructor initializes only pause `+0x1108 = 1`, timer `+0x10e8 = 0`
and scroll count `+0x10f4 = 0` beyond its controls. In particular, it does not
zero every handle field.

Initialization remaps the placeholder rectangle at `+0x10f8`, destroys the
placeholder, initializes common controls and loads `RICHED32.DLL`. It creates a
`RICHEDIT` window with extended style `0x200`, style `0x503000c4` and id `0xede`.
Library/window failures are not checked before loading the stream.

The loader replaces the external descriptor at `+0x1174` with embedded storage
`+0x1110` **before** copying `fltcred.rtf` to `+0x1124`. The embedded descriptor's
`+0xc` points to that filename. Its EDITSTREAM at `+0x10dc` contains file handle,
error and callback. The synchronous EM_STREAMIN call is followed by read-only
setup, CloseHandle, line count minus six, background color and timer creation.
The callback clears the returned byte count after ReadFile failure but always
returns zero, presenting EOF to RichEdit rather than a callback error.

Timer creation tries ids 3 through 14 inclusive, each at 500 ms. Every failed
attempt stores zero in the object. Matching timer ticks increment `+0x10f4`,
scroll down and compare the returned first visible line with `+0x10f0` using a
**signed strict greater-than**. Passing the limit scrolls to the top and resets
the counter. Pause toggles only bit 0 using XOR, including for non-boolean input.

Close kills the timer, clears it, destroys the RichEdit window, clears that
handle, frees the library when nonzero, clears the library handle and calls the
SIMSUI base close. Handle reloads and writes follow retail's dependency order.

## Lessons and graph descriptors

Lessons' descriptor contains answer `+0`, count `+8`, selected index `+0xc`,
records pointer `+0x10` and radio mode `+0x14`. Entries have stride `0xa08`, with
list text at `+0`, description at `+0x100` and path at `+0x900`. A selection saves
the chosen record before updating the description, loading HOST string `0x9007`,
splitting/joining paths and querying extension `0x1d`. Host exists returns zero
for availability. Unavailable files disable the second radio and select the
first. Synchronization compares the second radio's HWND with its
GetWindowLong(GWL_USERDATA) result; it does not merely test for nonzero.

Population reloads the descriptor after each external list call, including the
record count inside the loop. Accept checks selection before committing,
synchronizes again and closes. The helper at `0x2930` always returns zero, so
the second call to it in retail's accept path is unreachable and omitted.
Double-click performs selection handling before virtual accept.

The graph descriptor is object `+0x10e0`; its callback is descriptor `+8`.
Callback operations are 0 after initialization, 1 for message `0x464` and 2 on
accept. Arguments are operation, descriptor, object `+0x10dc` (a HWND); object
`+0x10e4` records the operation before each callback. Accept writes answer 1,
then calls the callback, then reloads the virtual close target. Null and debug
pointer `0xcdcdcdcd` fail validation with retail's message. The callback's
12-byte stack consumption is evident at these call sites; no caller supplying
this dialog id was found in the reference module set, so an independent callee
has not been identified. Native validation uses an explicit graph owner fixture, described below.

## Core differential evidence

The harness maps retail Help with DONT_RESOLVE_DLL_REFERENCES, verifies its
Linkage RVA, and patches the dependency IAT slots. It runs both sides over the
same restored object, two descriptors, four lesson records, buffers, font state
and guards. It compares every fixture byte, defined returns and complete
ordered dependency transcripts. Dependencies can replace descriptors/vtables,
change selection/count and alter handles during calls. Original vtable addresses
are used only as expected constructor outputs; handlers are called explicitly
from their respective implementations.

The only normalized persistent function pointer is EDITSTREAM's read callback.
Its ownership is checked separately, including on the synchronous stream path.
Expected null-allocation faults compare access type/address and land through a
private VEH continuation. Other faults fail the run. Exception mode injects a
private Windows exception at **every** dependency position reached by a normal
operation. The outer catcher performs real RtlUnwind, allowing retail's actual
MSVCRT C++ handler and our C cleanup records to run.

Verified on the current implementation:

- 20,000 scenarios, 85 operations: **1,700,000 comparisons, zero differences**,
  seed `0x19a847e3`.
- 2,000 scenarios with exception injection: **1,304,753 comparisons, zero
  differences**, seed `0x7e51a23b`; 1,133,843 injected exceptions caught and
  1,820 expected allocation faults.

The mutation script requires exit 1 and an actual `DIFFER scenario=` report;
build failures, crashes and timeouts do not count as detection. The stable-source
run caught **71/71 deliberate errors**, including three cleanup errors under
exception injection. The Name-capacity mutation targets the assignment before
RegQueryValueExA: its earlier initial assignment is overwritten before any
registry query and changing that dead store alone is unobservable here.

## Native integration and acceptance

`help_runtime.c` supplies typed Win32, HOST and MFC/SIMSUI adapters, DLL entry
points and both ABI exports. `tools/gen_help_runtime.py` validates the reference
image and generates six vtable bindings, five message maps and all five
registry factories. The sixth vtable belongs to Credits' embedded CWnd
placeholder; it must keep its own deleting destructor. Resource payloads and
an ordinal SIMSUI import anchor are generated only into ignored build output.
The replacement still requires the CD's shared MFC42 and SIMSUI implementations.

The module retains Help's MFC module-state initialization, extension-library
allocation and constructor exception cleanup. Deinit releases the host
registration and shared Dialog.DLL resource module and clears their handles.
About's one registered cached-font destructor runs on unload; the font survives
module deinit while the DLL remains loaded, just as in retail.

`tools/difftest/helpuidiff.c` loads retail and rebuilt Help against the CD's
actual MFC42 and SIMSUI. A native MFC application supplies the main window.
HOST memory/string/path services and descriptor owners are fixtures. Both
Help images' registry imports use a private fixture: no actual key is opened,
including on About's intentional handle-leak path. The fixture exercises open
success/failure and independent Name/ProductID query failures. Help's timer
imports are deterministic fixtures, including initial allocation failures;
WM_TIMER is dispatched through the real native message maps.

Credits reads and streams a synthetic RTF file from `bin/help-diff/fltcred.rtf`
through the real RichEdit window. The harness checks the populated line count,
drives scroll/wrap and pause/resume, then verifies timer/window/library handles
are cleared on close. The wrap fixture lowers the threshold to a reachable
line; a small viewport can prevent the first visible line from ever passing
retail's default line_count-minus-six threshold. About also opens and closes
a nested native Credits window through its descriptor callback.

Lessons varies empty/nonempty lists, selected records, descriptions, file
availability, radio choices, accept/cancel and double-click. The graph owner
creates and updates an actual child window and tears it down from the callback.
Retail's graph constructor/initializer do not initialize the saved HWND at
object `+0x10dc`; the fixture locates control `0xee2` independently and ignores
that indeterminate callback argument. Exact argument forwarding is verified by
the guarded core harness. No real simulator graph renderer is claimed here.

Every native dialog has two checkpoints comparing child controls, text,
geometry, visibility/enabled state, callback transcripts and all client pixels.
Dispatch ownership is checked against the expected module. Descriptor results,
registry/timer counts and close state are compared too. About's expired stack
pointer is normalized only in the final native descriptor comparison; live
control text and the core's exact stack-pointer checks cover its valid uses.
The harness checks two DLL load/unload cycles and observes exactly one cached
font destruction per module unload through the actual shared font destructor.
Module-init probes also compare allocation failure and allocation/constructor
throws with real exception unwinding and repeated deinit.

Native acceptance on the reference:

- Core regression after integration: **20,000 scenarios, 85 operations,
  1,700,000 comparisons, zero differences**, seed `0x19a847e3`. Log:
  `bin/help-diff/native-core-normal.log`.
- Exception regression: **2,000 scenarios, 1,304,753 comparisons, zero
  differences**, seed `0x7e51a23b`; 1,133,843 injected exceptions and 1,820
  expected allocation faults. Log: `bin/help-diff/native-core-unwind.log`.
- **60 public-factory/message cases across two DLL load/unload cycles, zero
  differences**, including all five dialogs and nested Credits. Full transcript:
  `bin/help-diff/native-ui.log`.
- **9/9 native mutations detected**: About font color, registry-failure text,
  Lessons edit color, Credits timer counter, graph message operation,
  completion answer, module exception cleanup, deinit handle clearing and
  cached-font unload. Summary: `bin/help-diff/native-breaks.log`.
- Isolated game startup: **27/27 modules, 95 gauges, PASS**, with English/Help.DLL
  loaded and replacement bytes confirmed. Trace:
  `bin/help-diff/smoke-game-k86rquyf/help-smoke.log`.

`tools/help_ui_breaks.py` snapshots sources, assembly, generated metadata,
resources/import library and harness before compiling deliberate errors. Its
baseline must pass first; only exit 1 with an actual native comparison mismatch
counts. Build errors, faults and timeouts fail the run. Logs and source hashes
remain under `bin/help-diff/ui-breaks/`. Retail text uses its Windows code page;
mutation diagnostics decode invalid UTF-8 with replacement so a copyright
character cannot discard an otherwise valid comparison result.

`tools/help_smoke.py` substitutes the rebuilt DLL only in an ignored private
game copy, uses the dedicated Help Wine prefix and stops only that prefix. It
does not alter `game/` or shared build/install integration. The native suite
complements the high-volume core comparisons; it does not claim every possible
Windows notification or native exception path.
