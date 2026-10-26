# World.DLL — native scenery-area, priority, booster and drive-search slices

**Partial replacement with native UI acceptance.** `bin/world-diff/WORLD_area.dll`
registers add/edit scenery area (`0x13080003` / `0x13080004`), scenery
priority (`0x13080005`), add/edit booster (`0x13080008` / `0x13080007`)
and drive search (`0x13080002`).
Their classes, initialization, controls, event/message maps, factories,
exception lifetimes and module binding are rebuilt in C, using
retail MFC42/SIMSUI as shared dependencies. The other 13 World registrations
remain pending native acceptance.

This is ready to review as a standalone six-dialog slice. It is **not a full
World replacement** and must not replace the active game's World.DLL. The
startup smoke changes only a private game copy; startup success does not supply
the other registrations or establish their behavior.

Reference: `orig/English/World.DLL`, preferred base `0x223f0000`, module id
`0x80000006`, 19 registrations. See [dialogs.md](dialogs.md#worlddll) and
[dialog_registry.tsv](../dialog_registry.tsv) for the complete inventory.
Read [the continuation rules](dialogs.md#starting-a-remaining-dialog-in-a-fresh-context)
before choosing a slice.

## Implemented slice: add/edit scenery area

Two factories share template `0x6b3`:

| Registry id | Factory VA | FE descriptor |
| --- | --- | --- |
| `0x13080003`, add area | `0x223f19f0` | FE `.data+0x3170` |
| `0x13080004`, edit area | `0x223f1920` | FE `.data+0x3138` |

Direct factory disassembly confirms that both allocate `0x1b20` bytes, call
shared constructor `0x223fbb30` with a null parent, and store the descriptor at
object `+0x10e4`. A failed template Create call displays a message before Show;
do not assume the completed Help/Sim factories' unchecked Create behavior
applies here. The constructor/destructor and factory exception paths now have the
independent lifetime acceptance below; native binding is exercised by the UI suite below.

The verified owner is `src/fe/fe_library.c`, not MAIN. Start with
[FE's Scenery Library contract](fe.md#part-4--the-scenery-library-0x78-verified)
and `tools/difftest/fesldiff.c`, especially `world_area`, `fesl_dialog` and
`fesl_call`. The latter is a stand-in for World; its passing results verify
FE's callbacks, not World's controls or event ordering.

The existing fixture identifies descriptor `+4` as the check callback, `+8`
as title, `+0xc` as the row, `+0x10` as position status, `+0x14` as position
and `+0x2c` as count. FE's 0x40-byte area row exposes path at `+4`, title at
`+0x18`, layer at `+0x2c`, active at `+0x34` and protected at `+0x3c`.
The first core comparison below establishes World's accesses and event ordering
at a synthetic UI boundary. FE's real filesystem validation remains separately
verified by its own harness.

This slice covers these two registrations and their shared class,
control exchange, validation, accept/cancel and lifetimes. Use independent
callback/file fixtures. Priority and drive search now have their own acceptance
below. Broader scenery lists and booster-list dialogs remain candidates for
later slices. The native
test DLL remains a partial World replacement.

## Other bounded slices

FACIL's exact-position dialog `0x1308` uses template `0x70c`, factory
`0x223f1c60`. Its owner contract is in [FACIL's dialogs](facil.md#the-dialogs),
`src/facil/facil.c` and `tools/difftest/facildiff.c` (`world_position`). FACIL's
validator/apply/revert/reload callbacks are established; World's numeric-edit,
error-display and nested facilities-dialog behavior still needs recovery.
Do not execute real placement callbacks against the active game while building
UI fixtures: FACIL's apply path reaches MAIN, CHAIN and the aircraft simulator.

The facility directory `0x1307` and go-to-airport `0x1309` have richer list and
nested-dialog contracts in the same owner/harness. Weather, time, scenery
complexity and other MAIN-driven descriptors need separate caller recovery;
their inventory entries are not behavior specifications.

## First milestone: seven area-class core routines

The implementation covers collect (`0x223fc360`), populate (`0x223fc440`),
Cancel (`0x223fc2c0`), OK (`0x223fc2e0`), validation (`0x223fc610`), error display
(`0x223fc660`) and path-change notification (`0x223fc720`). These are ordinary
C implementations; the reference side executes the retail routines through
`thiscall`, with only imported UI/CRT services and virtual callbacks replaced.
The caller-owned descriptor check is `stdcall`, one pointer argument, with its
return ignored. No FE code or verified owner harness was changed.

Observed behavior, now exercised against retail:

- Collect caches the original row, reads position with capacity `0x1f`, parses
  through `atoi`, then reloads the descriptor for its position store. Path and
  title use capacity `0x80`. Active state comes from object `+0x1b0c`.
  Combo selection 0..4 becomes layer 10..50; other values preserve the layer.
- Populate formats the position as `%04d`, changes combo selection only for
  valid layers, sets the check state and text, then applies protected state.
  Protected rows make path/title read-only and disable the active check,
  layer combo and associated label. Protection is reloaded between calls.
- OK collects, invokes the check callback and reloads the descriptor. A path
  status invokes DoErrorDialog with the scalar status and focuses the title
  control. Otherwise it invokes virtual validation at `+0xd8`, conditionally
  invokes virtual error at `+0xdc`, writes answer 1 and closes through `+0xec`.
  That virtual table is deliberately retained across validation/error calls.
  Cancel writes answer 0 before the virtual close.
- General validation collects/checks again and prioritizes title status at row
  `+0x14` over position status at descriptor `+0x10`; path status is handled by
  OK. The returned control ids are `0x1b07`, `0x1aef` or zero.
- Error display ignores its supplied argument. Title errors pass a scalar
  status; position errors construct a three-word record at descriptor `+0x18`
  containing status, temporary text pointer and count. Retail leaves that
  stack pointer at `+0x1c` after return. The routine returns the result of the
  virtual focus call at `+0xe0`, including when no error exists.
- Path changes collect only the path, invoke check, and enable/disable seven
  controls from path status. Title, position, active state and layer are not
  collected by that notification.

The class vtable is `0x2240fc38`, constructor `0x223fbb30`, destructor
`0x223fbc90`, deleting destructor `0x223fbc70`, DDX `0x223fbdc0` and initializer
`0x223fbef0`. The constructor makes 15 embedded controls, sets its own vtable
and initializes object `+0x10e0` to 1. DDX binds 14 controls; the extra embedded
text control at `+0x13a8` has no DDX binding. The message-map getter at
`0x223fbed0` returns `0x2240fbd0`: its local entries are path notification
`0x400` for control `0x1b05`, help and context menu. Accept/cancel and virtual
validation/error are inherited dispatch slots. No nested dialog registration
appears in the seven implemented routines. Initialization and inherited behavior now have the native comparisons below.

### Reproduce the core acceptance

```sh
make -f tools/world.mk worlddiff-run N=20000
make -f tools/world.mk worlddiff-run N=2000 MODE=interrupt
make -f tools/world.mk world-breaks
```

Reference SHA-256:
`35d67023ba2ff0113c519f6e2c4ab5f19c842c923401a255e7761eae423118f5`.
Seed `0x4a971b63`: the normal run made 140,000 comparisons and 140,000 fixture
self-checks with zero differences. The interrupted run made 97,256 comparisons,
including 83,256 interrupted reference calls, with zero differences. Its
14,000 normal fixture self-checks also passed. The seven operations run for
every scenario; scenario bits cover independent error statuses, both virtual
accept/reject outcomes, protected rows, mid-call descriptor swaps and virtual
table replacement. Edge cases cover every layer selection, invalid selections,
valid/invalid stored layers, negative and extreme position strings.

The harness checks ordered calls, integer returns and the complete fixture,
including object bytes, both descriptors/rows, string buffers and guard bytes.
Only descriptor `+0x1c`'s expired stack address is normalized; the text and
record fields are checked inside the error callback. Explicit fixture checks
assert validation priority, error return, Cancel, path-change call count and
valid/invalid layer behavior independently of the C implementation.

All eleven deliberate mutations produced actual `DIFFER` results: position
format, position/path capacities, layer conversion, Cancel/OK answers, title error priority, error
return, protected-title handling, missing enable and reloading the close
vtable after validation. Each mutant is freshly compiled to its own executable;
source, executable and difference log are under ignored
`bin/world-diff/breaks/`. Runtime files use `bin/world-diff/wineprefix` and
`wine.lock`. The runner never installs anything into the active game.

`interrupt` injects a private exception at each external-call boundary and
compares partial state/calls. These seven routines have no local EH cleanup;
the test escapes through a vectored handler. This is **not** acceptance of
constructor, destructor, factory or MSVC exception-unwind behavior. UI services
are synthetic fixtures, not real windows, SIMSUI's exchange engine or rendering.

World source, generators and test fixtures use snake_case identifiers and
standard `<stdint.h>` types such as `uint32_t`, matching the project style.
There is no World-local `types.h`. The project exception is recorded in
`CLAUDE.md` and the saved general agent instructions.

The style audit covered all World C headers/source, generated includes, Python
generators, mutation tools and fixtures. The three generated includes at that milestone reproduced
exactly in an isolated directory. The rebuilt DLL's `.text`, `.data`, `.rdata`
and `.rsrc` sections are byte-identical to the pre-cleanup startup-tested DLL.
Core/unwind comparisons, all 51 area mutation checks and the full 288-pair
native UI run (384 validation/error checks over two unload cycles) passed again.
These byte-identity results describe the style cleanup before adding Priority.

## Second milestone: factories, controls and exception lifetimes

`src/world/world_objects.c` implements the shared constructor, destructor,
scalar deleting destructor, DDX, help/context handlers and both factories.
`tools/gen_world_tables.py` extracts the 15-control lifetime graph, 14 DDX
bindings, terminated numeric help map and import-hook metadata directly from
retail. It validates reverse destruction, the MSVC EH state/action graph,
vtable/deleting-destructor association, base/module identity and DDX coverage.
The generated `.inc` files contain ABI metadata, not code or resource payloads;
regeneration produced identical output.

The constructor sets the class vtable and `+0x10e0` only after every embedded
constructor succeeds. Destruction does **not** reset the class vtable. When a
child destructor throws, the remaining earlier controls and base still receive
their destructors, without retrying the failed child. The deleting destructor
calls `ui_delete` only for flag bit 0 and only after successful destruction.
Both normal destruction and exception cleanup are checked against retail.

The two factories ignore their parent argument, allocate `0x1b20` bytes,
construct with a null parent, retain the supplied descriptor at `+0x10e4` and
load the current module into `+0x80`. An allocation returning null faults on
the descriptor store at address `0x10e4`; the harness explicitly requires that
write fault. Create failure calls MFC's message-box function with the same text,
flags and help id, then continues to Show(1). The returned handle is read after
Show. Exceptions before constructor completion release the allocation through
`CSimsDialog::operator delete`, a different import/convention from `ui_delete`.
Exceptions after ownership transfers to the created object do not release it
through the construction cleanup.

Retail has a typed `CException*` catch around the factory body. It unwinds first,
then calls `SIMSUI_ExitCleanlyE`, whose actual implementation displays an error
and calls MSVCRT `abort`. `world_exceptions.h` matches the MSVC x86 thrown-pointer
metadata and performs the catchable type's pointer conversion. Real
`_CxxThrowException` comparisons cover direct `CException*`, nonvirtual and
virtual derived-to-base conversions, null pointers, unrelated pointer types
and incompatible const-pointer types. Matching exceptions reach the fatal
service boundary; others propagate after cleanup. The fatal fixture records
its argument and ends that comparison side, without aborting the test process.
The real message-box/abort implementation is not executed by this core suite.

### Reproduce the lifetime acceptance

```sh
make -f tools/world.mk worldlifediff-run N=20000
make -f tools/world.mk worldlifediff-run N=2000 MODE=unwind
make -f tools/world.mk worldlifediff-run N=2000 MODE=typed
make -f tools/world.mk world-life-breaks
```

`tools/difftest/worldlifediff.c` executes nine operations per scenario: the eight
new entry points plus construction followed by deleting destruction. It loads
retail separately with `DONT_RESOLVE_DLL_REFERENCES`, verifies its Linkage export,
patches only external import boundaries, and compares it with the compiled C
implementation. The retail routines retain their own MSVC frame metadata and
use MSVCRT's actual `__CxxFrameHandler`; our routines register cleanup SEH frames
and a typed factory handler. In `unwind` and `typed`, a surrounding SEH catcher
calls real `RtlUnwind`. The earlier seven-operation harness remains separate.

Fixtures compare ordered calls, parameters, returns, the full object,
descriptor/exchange/help buffers, allocation-release counters, mutable module
state and guards. They vary object bytes, delete flags, creation results,
allocation failures and module changes during callbacks. Explicit self-checks
assert call counts, successful constructor state, factory handle returns and
context-help return 1. Only expected null-allocation faults are intercepted;
other faults end the run with side/scenario/operation information.

With seed `0x4a971b63`, 20,000 normal scenarios made 180,000 comparisons, with
3,638 expected allocation faults and 176,362 successful-path self-checks.
The 2,000-scenario unwind run made 283,292 comparisons, including 265,292 caught
exceptions, 364 allocation faults and 17,636 normal self-checks. All matched.
The 2,000-scenario typed run also made 283,292 comparisons: 217,742 exceptions
propagated to the outer catcher, 47,550 reached the factory fatal-service
boundary, and 364 produced the expected allocation fault. Its 17,636 normal
self-checks passed. Across these three runs, all 746,584 comparisons matched.
All runs use the reference hash given above.

All 20 lifetime mutations were detected. The suite builds distinct executables
and requires exit 1 with an actual `DIFFER` transcript. It covers construction state/order, base
cleanup, delete flags, partial-construction cleanup/ownership, wrong delete
operator, late ownership release, parent/Create/Show behavior, DDX omission,
help command/return, typed catch selection/cleanup/dispatch and both pointer
adjustments. Mutation sources, executables and logs live under ignored
`bin/world-diff/life-breaks/`; the runner shares only the World-specific Wine
prefix and lock with the earlier suite.

This is core acceptance, including real Windows/MSVC unwinding. Controls,
Create/Show, DDX, help and fatal-error presentation are still synthetic service
fixtures. It does not establish native UI, module lifetime or startup acceptance.

## Third milestone: initializer, native DLL and pixel comparisons

`world_init.c` implements `0x223fbef0`, including the virtual skin call, the
empty-path CString/resource branch, 15 embedded-control setup, text limits,
population and final enablement. `world_runtime.c` binds the retail imports,
rebuilds the 60-slot area vtable and MFC message map, and implements module
attachment, registration, dynamic-link-library cleanup and resource lifetime.
`gen_world_runtime.py` extracts bindings and registry records. Build-only
resource payloads stay in ignored `bin/world-diff/world.res`; no retail machine
code or resource payload is committed. The DLL exports only Linkage/ImportTable.

Initializer details established against retail:

- CString::GetBuffer takes one stack argument (`ret 4`). The other pushed
  `0x100` is retained for the later LoadString call; counting adjacent pushes
  as GetBuffer arguments corrupts the stack on the empty-path branch.
- Empty paths replace the heading with resource `0xf8d5` and clear object
  `+0x10e0`. That flag controls seven final enables, after population has
  applied protected-row read-only/enable settings. Preserve that ordering,
  including its override of some protected-state enables on initial display.
- The position limit is 4; path/title limits are their row capacities minus 1.
  The seven-control enable decision is read once, before the first enable call.
- A failed virtual skin step returns zero without proceeding. CString cleanup
  occurs only after successful construction, including on an exception during
  buffer acquisition, loading, release or text assignment.

```sh
make -f tools/world.mk world
make -f tools/world.mk worldinitdiff-run N=2000 MODE=unwind
make -f tools/world.mk world-init-breaks
make -f tools/world.mk worlduidiff-run
make -f tools/world.mk world-ui-breaks
make -f tools/world.mk world-smoke
```

The initializer's guarded fixture reuses the existing World event boundary,
executes retail's initializer directly, resolves the real MSVC frame handler
and compares call order, returns, complete fixture bytes and CString liveness.
It covers failed skinning, empty/nonempty paths, protection, descriptor changes
and a callback that changes the enable flag mid-sequence. At 2,000 scenarios,
**74,200 comparisons, including 72,200 real unwinds, matched**. All ten
initializer mutations produced actual `WORLD INIT DIFFER` results, including
missing CString cleanup, ownership/release mistakes, resource/control ids,
colors, limits, reloading the enable decision and incorrect skin/return behavior.

### Real controls, pixels and nested errors

`tools/difftest/worlduidiff.c` loads retail and our DLL normally with real
MFC42/SIMSUI, establishes a native MFC application/main window and supplies a
small independent HOST/GLOBAL fixture. It verifies that the dialog's own
constructor-driven vtable, DDX, initialization, validation and answer handlers
belong to the expected DLL. The callback is a controlled area-validator fixture;
FE's actual filesystem validator remains covered by FE's own harness.

Each of 24 cases runs six scripts over two complete load/unload cycles: initial
display, edits/accept/cancel, title-validation priority, invalid path, invalid
position and invalid title with repair. Cases cover both factory ids, empty and
existing paths, all five area types, active/protected flags, and Return/Escape
routing. Both ordinary WM_COMMAND routing and the native accept virtual are
exercised. The path validator can also change to rejection at OK time, after
controls were enabled; this explicitly witnesses the title-focus side effect
that is hidden when an ordinary path-change notification already disabled it. Tests compare descriptor/row/string state, ordered callbacks, child
classes/text/geometry/visibility/enabled state/edit limits and focus outcomes.

Pixels are copied from the actual window DC into 32-bit DIBs after activation,
redraw and message pumping. A paint sentinel rejects untouched buffers. The
error scripts call real SIMSUI error handling, which invokes the retail Alerts
factory through the host fixture. Its real controls/text and pixels are compared
before automatic dismissal. World must remain open, accept corrections and
subsequently close with the same answer/state. The host fixture restores the
area window's enablement/activation after its nested modal alert; focus must be
sampled immediately after error handling, before corrective edits can change it.
Only the expired position-error stack address at descriptor `+0x1c` is normalized;
its rendered error text is compared while live.

Baseline and failure BMPs are saved in `bin/world-diff/ui-images/`. The edit-color
mutation must produce a pixel difference while state, control metadata and
callbacks still agree. Native mutation inputs are snapshotted and hashed under
`bin/world-diff/ui-breaks/`; every mutation is freshly built and only exit 1 with
an actual UI/module comparison mismatch counts. Faults, timeouts and build
failures fail the suite. The final 24-case run passed **288 paired pixel/state/
callback comparisons**, with **384 validation/error self-checks**, eight paired
module-init comparisons and two complete unload cycles. **All ten native
mutations were detected**: edit color, position limit, empty-path enablement,
layer selection, path-notification dispatch, Cancel/OK answers, path-error
focus, module-constructor unwind deletion and repeated-deinit handle clearing.
The color mutant changed only pixels; state, controls and callbacks still agreed.
All ten initializer core mutations also passed their detection requirement.

Module initialization is also compared with real unwinding: successful/null/
throwing allocation and throwing CDynLinkLibrary construction, each followed by
two deinitializations. The four expected transcripts are `RACNL`, `RANL`, `RAXL`
and `RACDXL`. Registration/release counts and actual unloading are asserted.
This extends the earlier core-only lifetime tests to native module bindings.

### Isolated startup

`world_smoke.py` copies `game/` under `bin/world-diff/`, replaces only that copy's
`English/World.DLL`, boots it in the dedicated World Wine prefix for 75 seconds
and stops only that prefix. The run at `smoke-game-438tipvu` loaded **27/27
modules and 95 gauges**, loaded English World, verified installed replacement
bytes and reported no runtime errors: **PASS**. The replacement SHA-256 was
`db66067cbc5ea24b081630d5fa303c3b77cc83c073335c7bc75b8e6691d84190`.
The copy and `world-smoke.log` are retained for inspection. This proves startup
with the two-dialog slice, not that the other World menu commands work.

## Fourth milestone: scenery priority

Registry `0x13080005` uses template `0x6b4`, factory `0x223f1b90`, constructor
`0x223fc820`, destructor `0x223fc960` and vtable `0x2240fe50`. The object is
`0x1bb4` bytes with its descriptor at `+0x10dc`. Fourteen embedded controls are
constructed, destroyed and DDX-bound. `gen_world_priority.py` extracts their
lifetime/EH graph, DDX/help maps and layer-string recipe; the runtime generator
now binds both classes and all three registrations. Generated includes contain
numeric ABI metadata; build-only retail resources remain ignored.

FE owns this descriptor and the reorder callbacks; see `fe_library.c`'s
`fe_lib_priority` and the independent `fesldiff.c` owner fixture. No FE code was
changed. The descriptor fields established against World are:

| Offset | Meaning |
| --- | --- |
| `+0` | Answer |
| `+4`, `+8`, `+0xc`, `+0x10`, `+0x14` | Check, raise, lower, sort, move callbacks |
| `+0x18` | World title |
| `+0x1c`, `+0x20`, `+0x24` | Row count, zero-based selection, 0x40-byte row array |
| `+0x28`, `+0x2c` | Validation status, typed one-based position |
| `+0x30`, `+0x34`, `+0x38` | Error status, temporary text pointer, list count |

The callback ABI is stdcall with one descriptor pointer. Row title, layer and
active fields are at `+0x18`, `+0x2c` and `+0x34`. Populate formats tab-separated
name/type/three-digit layer rows, with `+`/`-` active markers, restores selection,
and updates edit text, arrow enablement and focus. Layers 10/20/30/40/50 select
the five retail resource strings. Invalid later layers reuse the previous
formatted position as type text. An invalid first layer reads uninitialized
scratch text in retail; the core fixture seeds that local at CString construction
to witness the same read rather than comparing arbitrary stack garbage.

Validation collects, checks and, when valid, invokes the move callback and
repopulates: it has visible side effects. OK retains its original virtual table
across validation/error handling, collects again, sets answer 1 and closes.
Errors use real SIMSUI's three-word position record and return the virtual focus
result. The expired stack pointer at descriptor `+0x34` is normalized after
return; its text and rendered alert are checked while live. Unlike the area
editor, Priority's PostNcDestroy calls the scalar deleting destructor with flag
1. The native harness witnesses that automatic deletion and does not delete the
object twice.

```sh
make -f tools/world.mk world-priority-core N=2000 MODE=unwind
make -f tools/world.mk world-priority-life N=2000 MODE=typed
make -f tools/world.mk world-priority-ui
make -f tools/world.mk world-priority-breaks
make -f tools/world.mk worlduidiff-run
make -f tools/world.mk world-smoke
```

`worldprioritydiff.c` executes twelve operations against direct retail entry
points: init, populate, selection, collect, raise/lower/sort, validation, error,
OK, Cancel and post-destruction. It compares full guarded objects, descriptors,
rows, returns and ordered calls. Fixtures cover negative/empty counts, invalid
selections/layers, active/protected rows, descriptor/vtable changes and all
reached exception boundaries, including CString cleanup. At 2,000 scenarios,
**452,520 comparisons, including 428,520 real unwinds, matched**.

`worldprioritylifediff.c` independently compares constructor/destructor,
deleting destruction, factory, DDX and help/context behavior. The 2,000-scenario
typed run passed **233,895 comparisons**, with 195,333 outer caught exceptions,
22,562 fatal-service exits, 182 expected null-allocation faults and 15,818 normal
self-checks. It uses the same actual MSVC exception and RtlUnwind boundaries as
the area lifetime harness, including derived/null/incompatible pointer cases.

`worldpriorityuidiff.c` reuses the native application, HOST and nested Alerts
helpers from `worlduidiff.c`, with separate Priority descriptors, callbacks,
row fixtures and scripts. Twenty-one cases cover zero through six rows, all
five valid layers and active/protected state. Five scripts exercise initial
rendering, selection/raise/lower, sort/OK, typed move/OK and invalid position,
real nested alert, repair/Cancel. Full list text, descriptor/row state, ordered
callbacks, controls, focus, automatic deletion and actual GDI pixels are compared.
The native suite passed **210 paired pixel/state/callback comparisons**, with
**648 witnesses**, eight module-init comparisons and two complete unload cycles.
Baseline images are under `bin/world-diff/ui-images/priority-baseline-*.bmp`.

All **22 deliberate Priority mutations were detected**: ten core, seven lifetime
and five native. Each requires exit 1 and an actual comparison mismatch; faults,
timeouts and build failures do not count. The color mutation changed pixels
while descriptor state, control metadata and callbacks still matched. Other
mutations cover position formatting/capacity, invalid-layer scratch text, active
markers, validation's move callback, error count/return, retained virtual table,
CString unwind, selection enablement, constructor/base/delete/ownership cleanup,
factory parent/message behavior, typed catch cleanup, list tabs, Up dispatch,
automatic deletion and edit limits. Inputs and logs are retained under ignored
`bin/world-diff/priority-breaks/`. The lifetime fixture covers the complete
`0x1bb4`-byte object and `0x40`-byte descriptor; all seven lifetime mutations
were rerun successfully after correcting its original smaller area-class sizes.

The existing area native suite also passed again: **288 paired comparisons**,
384 validation/error checks and two unload cycles. A fresh 75-second startup
with the three-registration DLL passed in private copy
`bin/world-diff/smoke-game-0av7gxsn`: **27/27 modules and 95 gauges**, matching
replacement bytes and no runtime errors. DLL SHA-256:
`67a86d22a07215bcce85c443840638e8289c602b847ddcd4d21d6106cf9a1493`.
The active game was not changed. Acceptance logs are
`bin/world-diff/priority-life-final.log`, `priority-breaks-acceptance.log`,
`priority-life-breaks-final.log`, `area-ui-regression.log` and
`priority-smoke-acceptance.log`.

All five World generated includes reproduce byte-for-byte in an isolated
directory. The World sources and fixtures retain snake_case identifiers and
standard `<stdint.h>` types, without `types.h`.

## Implemented family: add/edit performance booster

The shared booster editor now has core-event, class/factory, initializer and
native UI acceptance. `WORLD_area.dll` registers these two factories in addition
to the three scenery-area/priority registrations above.

| Registry id | Factory VA | FE descriptor |
| --- | --- | --- |
| `0x13080007`, edit booster | `0x223f1780` | FE `.data+0x30e8` |
| `0x13080008`, add booster | `0x223f1850` | FE `.data+0x3118` |

Both factories allocate `0x1f78` bytes, construct with template `0x6ab` and a
null parent, and store the descriptor at object `+0x10dc`. FE owns the
descriptor: `+4` checks, `+8` supplies the title, `+0xc` points to a `0x80`-byte
row and `+0x10` points to the parent list descriptor. See
[FE's Scenery Library contract](fe.md#part-4--the-scenery-library-0x78-verified),
`src/fe/fe_library.c` and `world_cache` in `tools/difftest/fesldiff.c`.
Those owner files and their harnesses were not changed. The new tests supply
independent descriptor, row, text and callback fixtures.

The native template clarifies the field labels: row `+4` is
**Cache path** (control `0x1ae8`), and row `+0x18` is **Source path** (control
`0x1aea`). Row `+0x5c` is **Cache size**, and the unsigned WORD at `+0x74` is
**Recommended size**, both in kilobytes. Earlier World/FE notes called
these values layers and used the opposite path names. World documentation now
follows the visible labels; callback offsets and measured owner behavior are
unchanged. FE and its verified harness remain unchanged.
The combo is **Cache cleanup strategy**, and `+0x54` is **Cache active**.

`world_booster.c` implements six routines: collect (`0x223f6dd0`), populate
(`0x223f6ea0`), OK (`0x223f6740`), path change (`0x223f6fd0`), virtual
validation (`0x223f7160`) and virtual error (`0x223f71a0`). Observed behavior:

- Collect reads the source path before the cache path, both with capacity
  `0x104`, reloading the descriptor/row between calls. Cache-size text uses
  capacity
  `0x1f` and `atoi`, stored at row `+0x5c`. Combo selection exactly 1 sets row
  `+0x7c` to 1; every other selection sets zero. Any nonzero check state at
  object `+0x1d64` becomes active 1 at row `+0x54`.
- Populate formats both sizes with `%5d`. The BSTINFO recommended size at row
  `+0x74` is an unsigned 16-bit value, even when the surrounding word contains
  nonzero upper bits. Name/description use SIMSUI text assignment, while the
  source path, size edit and cache path use MFC text assignment. Stored active
  and combo values are selected only when exactly 1, unlike collect's active test.
- OK collects/checks and chooses the first error at row `+0`, `+0x14`,
  `+0x78`, `+0x50`, then `+0x58`: cache path, source path, cleanup strategy,
  active, cache size.
  A nonzero error-service return focuses the corresponding control and keeps
  the dialog open. A zero return accepts immediately, without examining later
  errors. Acceptance writes answer 1 before virtual close. It does not call
  virtual validation/error as the scenery-area editor does.
- Cache-size errors build a three-word record at row `+0x60`: status, temporary
  edit text and the parsed maximum from the string at row `+0x70`. Both OK and
  virtual error leave the expired stack pointer at row `+0x64`. The latter
  returns virtual focus's result, including when no error exists. Virtual
  validation collects/checks again but examines only row `+0x58`, returning
  control id `0x1ae9` or zero.
- Path change reads only the cache path, with capacity `0x80`, then checks. It
  decides all seven enables once from the cache-path status, refreshes
  description, name and both size displays, and preserves other editable fields.
  Descriptor loads after those calls remain fresh.

The class vtable is `0x2240ee68`, constructor `0x223f6280`, destructor
`0x223f6420`, deleting destructor `0x223f6400` and DDX `0x223f65a0`.
`gen_world_booster.py` extracts and validates all 21 embedded controls, their
reverse destruction and MSVC cleanup graph, all 21 DDX bindings, the terminated
help map and the constructor/deleting-destructor vtable association. Its two
generated includes reproduce byte-for-byte in an isolated directory; they
contain ABI metadata, without retail machine code or resource payloads.

`world_booster_objects.c` implements those routines, both factories, help
(`0x223f6d90`), context menu (`0x223f6db0`), inherited Cancel (`0x223fe4e0`)
and PostNcDestroy (`0x223f3d30`). Construction sets its class vtable only after
all embedded constructors succeed; destruction does not reset it. Exceptions
destroy remaining controls/base once. The deleting destructor releases memory
only for flag bit 0 and after successful destruction. PostNcDestroy dispatches
deleting destruction with flag 1; Cancel writes answer 0 before virtual close.

Both factories ignore their parent argument. A null allocation faults on the
descriptor write at address `0x10dc`. A failed Create displays
`CEditBooster::Create() failed\n`, then continues to Show(1); the returned HWND
is read afterwards. Partial construction frees through the dialog delete
operator; exceptions after ownership transfers do not. The typed `CException*`
catch unwinds before fatal-service dispatch, including derived and virtual-base
pointer adjustments. These rules match the earlier World lifetime boundary.

### Reproduce booster core and lifetime acceptance

```sh
make -f tools/world.mk world-booster-core N=20000
make -f tools/world.mk world-booster-core N=2000 MODE=interrupt
make -f tools/world.mk world-booster-breaks
make -f tools/world.mk world-booster-life N=20000
make -f tools/world.mk world-booster-life N=2000 MODE=unwind
make -f tools/world.mk world-booster-life N=2000 MODE=typed
make -f tools/world.mk world-booster-life-breaks
```

All runs use seed `0x4a971b63` and the reference hash above. The six-operation
core harness made **120,000 normal comparisons** and 120,000 fixture self-checks;
the interrupt run made **105,520 comparisons**, including 93,520 interrupted
reference calls, and 12,000 normal self-checks. All matched. Fixtures compare
ordered calls, returns and every byte of the guarded object, both descriptors,
rows and string buffers. They cover all status combinations, error-service
outcomes, negative/extreme cache-size inputs, noncanonical active/combo values,
upper-half BSTINFO values and descriptor/vtable/status changes at external
boundaries. Only expired row `+0x64` addresses are normalized; text is checked
inside the error service. Core interruptions escape through a vectored handler;
they do not claim local exception-cleanup acceptance.

The eleven-operation lifetime harness made **220,000 normal comparisons**,
**386,064 unwind comparisons** and **386,064 typed-exception comparisons**, all
matching. The normal run includes 3,638 expected allocation write faults and
216,362 successful-path self-checks. Unwind covers 364,064 caught exceptions;
typed covers 301,958 propagated exceptions and 62,106 fatal-service dispatches.
Each exceptional run also includes 364 expected allocation faults and 21,636
normal self-checks. It compares the complete `0x1f78`-byte object, descriptor,
DDX/help buffers, module state, delete counters, ordered calls and returns.
Retail retains its own MSVC EH metadata and real CRT frame handler; the outer
catcher invokes real `RtlUnwind`. Null/non-null PostNcDestroy and both factories
are exercised independently. Create/Show and control services remain fixtures.

All **31 deliberate mutations** were detected: 18 core and 13 lifetime. Each
mutant is independently compiled and requires exit 1 with an actual `DIFFER`
transcript; faults, timeouts and build failures do not count. Mutation sources,
executables and logs are retained in `bin/world-diff/booster-breaks/` and
`booster-life-breaks/`. Acceptance logs in `bin/world-diff/` are
`booster-core-normal.log`, `booster-interrupt-acceptance.log`,
`booster-breaks-acceptance.log`, `booster-life-normal.log`,
`booster-life-unwind.log`, `booster-life-typed.log` and `booster-life-breaks.log`.

### Booster initialization and native acceptance

`world_booster_init.c` implements initializer `0x223f68b0`. It calls base init,
then virtual skinning; failed skinning returns zero immediately. Successful
skinning sets object `+0x84` to 12. An empty cache path loads heading resource
`0xf8d4` through a temporary CString and sets object `+0x10e8` to 1; existing
paths set that flag to zero. CString cleanup follows the same real-unwind
boundary as the earlier initializer.

A shared `CSimsFont` is constructed from base font 8 once per DLL load. Retail
sets bit 0 of its flag byte **before** asking for the base font/constructing,
then registers its destructor. Exceptions preserve that early flag, and a failed
constructor never registers cleanup. Each successful initialization copies the
current dialog's fields at `+0x228` / `+0x22c` / `+0x270` / `+0x274` into shared
font fields `+8` / `+0xc` / `+0x50` / `+0x54`, setting byte flag `+0x58` bit
`0x10` without disturbing adjacent bytes.
The initializer uses that font for the description, base font 2 for the three
edits, and the recovered groupbox, combo, checkbox, button and text setup.

Text limits are loaded afresh from the row's source/cache capacities minus one;
the cache-size edit limit is 7. For an empty cache path, population precedes
seven disables. For an existing path, the first five controls are enabled and
the last two disabled **before** population. Both branches decide their enable
sequence once, even if a callback changes the flag. Finally object `+0x10e0`
and `+0x10e4` receive the Cancel and OK embedded-control pointers.

The message-map getter `0x223f6730` returns `0x2240ede8`, based on getter
`0x223f5d40`. Local handlers are nonclient click `0x223f7240`, help, context menu
and cache-path change `0x400` for control `0x1ae8`. The nonclient handler consumes
12 stack bytes, but calls MFC ordinal 2379 (`Default`) with **no arguments**;
its callee ends in plain `ret`. Passing the handler's three arguments onward
corrupts the stack. The native suite explicitly sends `WM_NCLBUTTONDOWN` through
this map as well as exercising command dispatch.

`world_runtime.c` now rebuilds all three 60-slot vtables and all three message
maps, binds the new imports, propagates module handles to the booster factories,
and registers five dialogs. Shared native Cancel/PostNcDestroy wrappers serve
both priority and booster classes. The shared-font destructor runs after MFC
extension teardown on DLL detach, matching retail's DllMain/CRT exit order.
This `-nostdlib` DLL owns its callback locally; it does not put an unloadable
function pointer in the process-wide CRT exit table. Module deinit leaves the
font alive until actual DLL unload.

```sh
make -f tools/world.mk world-booster-init N=20000
make -f tools/world.mk world-booster-init N=2000 MODE=unwind
make -f tools/world.mk world-booster-ui
make -f tools/world.mk world-booster-native-breaks
make -f tools/world.mk worlduidiff-run
make -f tools/world.mk world-priority-ui
make -f tools/world.mk world-smoke
```

With seed `0x4a971b63`, the initializer made **20,000 normal comparisons** and
20,000 reference-side fixture checks. Its unwind run made **86,818 comparisons**,
including **84,818 real unwinds**, and 2,000 normal fixture checks. All matched.
The fixture compares complete object/descriptor/row/string state, ordered calls,
returns, shared-font bytes, constructor/destructor liveness and registered
cleanup. It covers empty/existing cache paths, failed skinning, initialized and
uninitialized font flags, descriptor changes, changing enable flags and seeded
neighbouring flag bytes. The font registration uses an independent `_onexit`
service fixture; registered destructors run separately after initializer unwind.

`worldboosteruidiff.c` uses the existing native application/HOST/GLOBAL, nested
Alerts, GDI capture and module-init fixtures, with an independent booster
validator and scripts. Sixteen cases cover both factory ids, empty/existing
paths, both active states and both cleanup strategies. Ten scripts cover initial
Cancel, edits with Return/OK and Escape/Cancel, all five error priorities with
repair, virtual validation/error, and invalid cache-path disable/repair.
The combo's focused child may have id `0x3e9`; the fixture checks that it belongs
to the intended combo. Error snapshots include real alert controls and pixels.
Full descriptor/row/text state, ordered callbacks, child metadata, focus, pixels
and automatic deleting-destruction are compared, with module ownership checked
for all class dispatch entries. Unload additionally probes the real SIMSUI font
destructor, requiring one module-owned shared-font destruction per DLL load.

The suite passed **320 paired pixel/state/callback comparisons**, **452
witnesses**, eight module-init comparisons and two full unload cycles. Baseline
images are `bin/world-diff/ui-images/booster-baseline-*.bmp`. All **18 new
mutations** were detected: 12 initializer and six native, bringing the booster
family to **49/49** across core, lifetimes, initialization and native UI. The
native colour mutant changed pixels while descriptor state, controls and
callbacks still matched. Other native mutations cover size limits, cache-path
dispatch, error focus, automatic deletion and shared-font cleanup. Faults,
timeouts and build errors do not count. Snapshotted inputs, mutant sources,
executables and difference logs are in `bin/world-diff/booster-native-breaks/`.

The existing area and priority suites also passed on the five-registration DLL:
**288** and **210** paired comparisons, respectively, with their full witnesses
and two unload cycles each. Runtime metadata regenerates identically in an
isolated directory, as do all seven generated World includes. All World code
retains snake_case and standard `<stdint.h>`
types, without `types.h`. Acceptance logs in `bin/world-diff/` are
`booster-init-normal.log`, `booster-init-unwind.log`, `booster-ui-acceptance.log`,
`booster-native-breaks-acceptance.log`, `booster-area-regression.log` and
`booster-priority-regression.log`.

A 75-second startup of the five-registration DLL passed in private copy
`bin/world-diff/smoke-game-koqll7ov`: **27/27 modules, 95 gauges**, matching
replacement bytes and no runtime errors. DLL SHA-256:
`67ac58c6b5d5f578ca2dc175e5d629df05a50e6da3a9b57df497f5bd19afa0f1`.
The final DLL's `.text`, `.data`, `.rdata` and `.rsrc` sections match the
native-tested DLL after the final comment-only rebuild. The startup log is
`bin/world-diff/booster-smoke-acceptance.log`; the active game was unchanged.
Selected area and priority colour mutations also passed again with the updated
mutation build inputs (`booster-area-mutation-regression.log` and
`booster-priority-mutation-regression.log`).

## Drive-search accepted slice

This slice implements FE's drive-search dialog `0x13080002`, factory
`0x223f1ac0`, template `0x6b0`. `world_search.c` implements its core handlers
behind an independent UI boundary. Its class, initialization and native UI now
have the acceptance below; it is the sixth registration in the partial DLL.

The owner is [FE's Scenery Library](fe.md#part-4--the-scenery-library-0x78-verified),
`src/fe/fe_library.c` and `tools/difftest/fesldiff.c`'s `world_browse` stand-in.
The descriptor embeds an area row at `+0x20`, with shown path `+0x24`, title
`+0x38`, layer `+0x4c` and active `+0x54`. Drive count, selection and packed
four-byte drive strings are at `+0x68`, `+0x6c` and `+0x70`; four button states
are at `+0x74..+0x80`. Owner callbacks take one descriptor pointer, `stdcall`;
their returns are ignored. No real FE filesystem or game callback executes in
this new harness, and the verified owner code/harness remains unchanged.

Recovered behavior:

- Add (`0x223fe270`), Add All (`0x223fe210`), active change (`0x223fe2d0`),
  Next (`0x223fe320`), Next Folder (`0x223fe370`) and Search (`0x223fe3c0`)
  first collect title and shown path, each with capacity `0x80`, reloading the
  descriptor between reads. Active change then stores the raw checkbox state
  from object `+0x1c70`, without boolean normalization. The other actions do
  not implicitly collect active, layer or drive selection.
- Add/Add All dispatch `+8`/`+0xc`, invoke the local validation helper and
  repopulate even after validation fails. Next/Next Folder dispatch `+0x14`/
  `+0x18` and repopulate. Search dispatches `+0x10`, repopulates, then posts
  message `0x5fa` to the current dialog HWND with the current title-edit HWND
  as wParam. This uses PostMessage, not SendMessage.
- The local helper `0x223fe760` invokes check (`+4`), reloads the descriptor,
  and reports the first nonzero status at `+0x20`, `+0x34`, `+0x48`, targeting
  object controls `+0x16c0`, `+0x1304`, `+0x1830`. It returns zero for any
  error, ignoring the error service's result; otherwise it returns one.
  This helper is not the class's inherited virtual validation slot.
- Populate (`0x223fe560`) uses SIMSUI title assignment for the shown path,
  MFC text assignment for title, maps layers 10..50 to combo indices 0..4
  and preserves selection for invalid layers. It passes active and the four
  button enables as raw 32-bit values. The drive list is reset and rebuilt
  from signed count `+0x68`, using four-byte entries and fresh descriptor
  loads at each iteration, before restoring drive selection. The final
  label/edit enables use one cached conjunction of `+0x74` and `+0x78`.
- Drive selection (`0x223fe430`) reloads the descriptor after CB_GETCURSEL;
  layer selection (`0x223fe470`, alias `0x223fe460`) caches its destination
  before that call and ignores indices outside 0..4.
- Cancel (`0x223fe4e0`) and the local OK command (`0x223fe500`) both write
  answer **zero**, then dispatch virtual close. The latter and validation
  call `0x223f99f0`, a bare `ret`; those no-effect calls are omitted in C.

`gen_world_search.py` extracts the complete class graph for continuation:
18 embedded controls and DDX bindings, reverse destruction and matching MSVC
cleanup actions, terminated help map, 60-slot vtable and 13 message entries.
Constructor/destructor/deleting destructor/DDX/initializer are
`0x223fda90` / `0x223fdc10` / `0x223fdbf0` / `0x223fdd60` / `0x223fdec0`;
object size is `0x1e78`, vtable `0x22410320`, map `0x224101c8`, base-map getter
`0x223f5d40`. Local messages include six buttons `0x1aae..0x1ab3`, drive/layer
selection and close-up notifications, OK, help and context menu. The metadata
contains no machine code or resource payloads. Both generated includes reproduce
byte-for-byte in an isolated directory. Extraction alone establishes a graph;
behavioral lifetime and native-dispatch acceptance follow below.

```sh
make -f tools/world.mk world-search-metadata
make -f tools/world.mk world-search-core N=20000
make -f tools/world.mk world-search-core N=2000 MODE=interrupt
make -f tools/world.mk world-search-breaks
```

With seed `0x4a971b63` and the reference hash above, `worldsearchdiff` made
**260,000 normal comparisons** and 260,000 independent reference-side fixture
checks. The interrupt run made **239,054 comparisons**, including **213,054
interrupted reference calls**, plus 26,000 normal fixture checks. All matched.
Thirteen operations include both layer entry points and both close handlers.
The fixture compares ordered calls, defined returns and every byte of the
`0x1e78`-byte object, two descriptors, strings, packed drives and guards.
It covers every error-status combination, ignored callback/error returns,
valid/invalid layers, negative/zero/positive drive counts, valid/invalid combo
indices, noncanonical checkbox/enables, descriptor swaps, changing enable
flags, changed HWNDs and vtable replacement. Its independent checks require
raw active collection, selection destinations, error priority/result, answer
zero before close and the search post. Core interruption escapes through a
vectored handler; these routines own no local exception cleanup.

All **22 deliberate mutations** produced actual `DIFFER` results with exit 1;
faults, build failures and timeouts do not count. They cover text capacities,
raw checkbox/enables, path-control assignment, layer mapping/range, drive
stride/count/selection, cached enable decisions, error priority/return,
action callbacks, missing validation, the posted message, descriptor caching
and answer zero. The existing area-core regression also passed **14,000
comparisons** and 14,000 fixture checks with zero differences.
Acceptance logs are `bin/world-diff/search-core-normal.log`,
`search-core-interrupt.log`, `search-core-breaks.log` and
`search-area-regression.log`. Mutant source, executables and actual difference
transcripts are in `bin/world-diff/search-breaks/`. The shared runner still
uses World's private Wine prefix and lock.

### Drive-search class and factory lifetime acceptance

`world_search_objects.c` implements constructor `0x223fda90`, destructor
`0x223fdc10`, scalar deleting destructor `0x223fdbf0`, factory `0x223f1ac0`,
DDX `0x223fdd60`, help/context `0x223fe520` / `0x223fe540` and shared
PostNcDestroy `0x223f3d30` as C routines. The 18 controls and bindings use the
extracted metadata above. The class vtable is installed only after all control
constructors succeed. Destruction leaves the incoming vtable in place; if a
control destructor throws, earlier controls and the base are still destroyed,
without retrying the failed control. The scalar deleting destructor frees
through UI delete only for flag bit 0 and after successful destruction.
PostNcDestroy invokes virtual deleting destruction with flag 1 for non-null
objects, and does nothing for null objects.

The single factory ignores its parent, allocates `0x1e78` bytes, constructs
with a null parent and stores the descriptor at `+0x10dc`. A null allocation
faults on that store at address `0x10dc`, matching retail. It reads the current
module after construction, stores it at `+0x80`, then transfers ownership before
Create. Failed Create displays `CSceneryAreaSearch::Create() failed\n` and
continues to Show(1). The returned HWND is read after Show. Failed partial
construction releases the allocation through `CSimsDialog::operator delete`,
whose calling convention and import differ from UI delete; Create/Show
exceptions do not release it through that construction cleanup.

The factory's typed `CException*` handler unwinds before calling the fatal
`SIMSUI_ExitCleanlyE` service boundary. The new fixture independently executes
retail's own catch metadata and real CRT frame handler, using
`_CxxThrowException` for direct, derived and virtual-base pointers, null
pointers, unrelated pointer types and incompatible const pointers. Matching
exceptions reach fatal dispatch with the adjusted pointer; other exceptions
propagate after cleanup. The fatal fixture terminates the comparison side;
it does not run SIMSUI's message box or CRT abort. Normal services, Create/Show,
DDX and WinHelp remain synthetic boundaries.

```sh
make -f tools/world.mk world-search-life N=20000
make -f tools/world.mk world-search-life N=2000 MODE=unwind
make -f tools/world.mk world-search-life N=2000 MODE=typed
make -f tools/world.mk world-search-life-breaks
make -f tools/world.mk world-search-core N=2000
```

With seed `0x4a971b63` and the reference hash above, nine lifetime operations
made **180,000 normal comparisons**, including 1,819 expected allocation
write faults and 178,181 successful-path fixture checks. Each exception suite
made **292,103 comparisons**: unwind includes **274,103 real unwinds**; typed
includes **246,689 propagated exceptions** and **27,414 fatal dispatches**.
Each also includes 182 expected allocation faults and 17,818 normal fixture
checks. All matched. The fixture compares the complete guarded `0x1e78`-byte
object, `0x84`-byte descriptor, exchange/help buffers, module state, deletion
counters, ordered calls and defined returns. Independent witnesses assert
control markers, call counts, final constructor vtable, base destruction,
flag-controlled deletion, factory result, context result and PostNcDestroy's
flag. Injection occurs at every reached external boundary with real RtlUnwind;
this is separate from the earlier core suite's vectored-handler interruption.

All **17 lifetime mutations** produced exit 1 with an actual `DIFFER`
transcript, bringing drive search to **39/39** across core and lifetimes.
They cover constructor vtable timing, destructor vtable preservation, missing
controls/base destruction, delete flags, allocator size, factory template and
parent, the post-Show handle, allocation cleanup service, failed-Create message,
typed catch cleanup, ownership transfer, DDX omission, help/context behavior
and PostNcDestroy's flag. Faults, timeouts and build failures do not count.
The final core regression also passed **26,000 comparisons** and 26,000
reference-side fixture checks with zero differences.

Acceptance logs are `bin/world-diff/search-life-normal.log`,
`search-life-unwind.log`, `search-life-typed.log`, `search-life-breaks.log` and
`search-life-core-regression.log`; mutant source, executables and actual
`DIFFER` transcripts are under `bin/world-diff/search-life-breaks/`.

### Drive-search initializer acceptance

`world_search_init.c` implements initializer `0x223fdec0` using the shared
control-setup boundary and a search-specific groupbox service. It ignores
base initialization's return, reloads the current vtable, then calls virtual
skinning at `+0xe4` with bitmap `0x14f`, the current resource module and flags
15. Failed skinning returns zero immediately. Successful initialization:

- Sets up the heading artwork and six buttons. OK uses font pair `+0x5a0` /
  `+0x610`, bitmap `0x132` and coordinates 0/0; Add All, Add, Next, Next
  Folder and Search use `+0x680` / `+0x6f0`, bitmap `0x137` and coordinates
  1/1, in that order. Resource-module loads remain fresh between calls.
- Initializes drive and layer labels/combos and selects index zero in each
  before population. The drive label uses font `+0x220`, the layer label
  `+0x370`; both combos use `+0x3e0`.
- Sets up static text, the checkbox, path display, groupbox and title edit.
  Checkbox setup dispatches its current vtable slot `+0xc4`, passing font
  `+0x4c0`, bitmap `0x139`, DIB `+0xff4`, palette `+0x7e8` and the current
  resource module. This is `CSimsCheckButton::Init` at SIMSUI `0x20473c20`;
  its exported signature and `ret 0x14` at `0x2047419a` confirm five arguments
  after `this`. The fixture supplies two distinguishable checkbox dispatches.
- Uses base font 2 and colors `0xffffff` / 0 / `0xff0000` for the title
  edit. EM_LIMITTEXT receives descriptor `+0x3c` minus one, preserving unsigned
  wraparound for zero capacity. The descriptor is loaded after edit setup.
- Disables the title label/edit, assigns heading text from the current
  descriptor `+0x1c`, then calls population, which may enable those controls
  again from the owner states. Success returns one. The class makes no
  explicit stores at `+0x84`, `+0x10e0` or `+0x10e4`.

There are no owned CString/font temporaries, shared font initialization or
local EH cleanup in this initializer. This milestone uses external-boundary
interruptions, separately from the preceding class/factory real-unwind suite.

```sh
make -f tools/world.mk world-search-init N=20000
make -f tools/world.mk world-search-init N=2000 MODE=interrupt
make -f tools/world.mk world-search-init-breaks
```

With seed `0x4a971b63` and the reference hash above, the normal suite made
**20,000 comparisons** and 20,000 independent reference-side fixture checks.
The interrupt suite made **67,523 comparisons**, including **65,523 interrupted
reference calls**, plus 2,000 normal fixture checks. All matched. The fixture
compares ordered calls, returns, every guarded object/descriptor/string/drive
byte, and resource-module state. It seeds failed/successful skinning, both
checkbox dispatches, extreme capacities, descriptor/vtable swaps, changing
resource modules and changing enable flags. Independent witnesses require six
buttons, two combos, six text setups, one checkbox/groupbox/edit setup, the
failed-skinning boundary, defined returns and preservation of the fields above.

All **24 initialization mutations** produced exit 1 with an actual `DIFFER`
transcript, bringing drive search to **63/63** across core, lifetimes and
initialization. Mutations cover failed skinning, cached skin vtables, artwork,
button count/order/coordinates/module reloads, fonts, combo labels/selections,
checkbox bitmap, groupbox binding, edit colors, title-limit arithmetic and
window, initial disabling, cached title descriptors, missing population,
extra object stores and return value. Faults, timeouts and build failures do
not count. The core and typed-lifetime regressions also passed **26,000** and
**292,103 comparisons**, respectively, with their full normal fixture checks;
the latter includes 246,689 propagated exceptions and 27,414 fatal dispatches.

Acceptance logs are `bin/world-diff/search-init-normal.log`,
`search-init-interrupt.log`, `search-init-breaks.log`,
`search-init-core-regression.log` and `search-init-life-regression.log`.
Mutant sources, independently compiled executables and actual difference
transcripts are under `bin/world-diff/search-init-breaks/`.

### Drive-search native acceptance

`world_runtime.c` binds the search class as the fourth 60-slot vtable and fourth
message map, registers factory `0x13080002` and propagates module handles to
its factory across attach/init/deinit. Shared inherited Cancel/PostNcDestroy
wrappers retain the earlier priority/booster dispatch. Build-only resources
continue to come from the reference image; no resource payload is tracked.

Native binding splits the path display's text getter from the title edit's
getter. Retail uses SIMSUI `CSimsText::GetWindowTextA` (IAT `0x22415700`) for
path and MFC ordinal 3873 (`0x22415598`) for title. Both had shared one
synthetic fixture previously. The search service now exposes `get_path`
separately so the native adapter binds each callee correctly. The SIMSUI getter
at `0x204819a0` can copy its cached full text after display fitting; its cached
branch ignores the requested capacity. The native fixture includes long paths
with a larger backing buffer to distinguish this from MFC's bounded getter.

`worldsearchuidiff.c` uses the existing real MFC/SIMSUI application setup,
HOST/GLOBAL services, nested Alerts and GDI capture. Independent FE-shaped
callbacks use a descriptor with its embedded area row and packed drive strings;
they log ordered callback inputs and change row text, position and enable flags
without using real FE files or game callbacks. Twenty cases cover all stored
layers, active states, independent button states, differing title limits,
drive selections and long paths. Seventeen scripts cover baseline, Return/OK,
Escape/Cancel, a real checkbox mouse click, both selection/close-up notification
types on both combos, Add/Add All/Next/Next Folder/Search and path/title/layer
errors with a combined-priority case. Add/Add All must repopulate after failed
validation. Both close commands must leave answer zero.

Retail exposes **three** layer choices in this template. Selecting index 4
therefore leaves the actual combo at its prior selection; the native scripts
use indices 0..2 and independently require count three. This does not change
the recovered core's 0..4 conversion, whose broader domain remains covered by
synthetic fixtures. The three-choice finding corrected an initial native
fixture assumption rather than changing World's behavior.

Search's posted `0x5fa` message is inspected before pumping: it must be queued
for the dialog, with the title HWND as wParam and zero lParam. Pumping must
focus the title edit. The fixture compares this queued-message witness as
well as normalized focus, full descriptors/text, callback order, child metadata,
real alert controls/pixels, window pixels and automatic deleting destruction.
It verifies ownership of the factory, local class slots and all 13 local
message handlers. The inherited import slots are shared MFC/SIMSUI code;
search's inherited validation/error slots must not be mistaken for local ones.

```sh
make -f tools/world.mk world-search-ui
make -f tools/world.mk world-search-native-breaks
make -f tools/world.mk worlduidiff-run
make -f tools/world.mk world-priority-ui
make -f tools/world.mk world-booster-ui
make -f tools/world.mk world-smoke
```

The search suite passed **680 paired pixel/state/callback comparisons**,
**1,160 witnesses**, eight module-init comparisons and two full unload cycles.
Reference images are `bin/world-diff/ui-images/search-baseline-*.bmp`.
All **13 native mutations** produced actual `SEARCH UI DIFFER` results with
exit 1, bringing drive search to **76/76** across core, lifetimes,
initialization and native UI. They cover edit color, title limit, active/next/
drive/layer-close-up dispatch, missing and synchronous posts, error priority,
automatic deletion, the path getter, answer zero and title enables. The color
mutation changed pixels while descriptor state, controls and callbacks still
matched. The getter mutation requires the long-path case: the tool runs twenty
cases for it and ten for the others. Its difference occurs at case 16, step 3,
with descriptor state differing while controls and callbacks still match.
Faults, timeouts and build failures do not count. Snapshotted input hashes,
sources, DLLs, harnesses and difference logs are in
`bin/world-diff/search-native-breaks/`. `search-native-breaks.log` aggregates
the initial run and the expanded long-path rerun; the original ten-case result
and rerun remain in `search-native-breaks-initial.log` and
`search-native-path-getter.log`.

The six-registration DLL also passed the existing **288 area**, **210
priority** and **320 booster** paired native comparisons, with their full
witnesses, module-init checks and two unload cycles each. Booster's shared-font
cleanup remains accepted. Selected area, priority and booster color mutations
were detected again with the expanded native build inputs.

After splitting the path getter service, search core again passed **260,000
normal comparisons**, **239,054 interrupt-suite comparisons** and all **22
core mutations**. Initializer and typed-lifetime regressions passed **67,523**
and **292,103 comparisons**, respectively, including their normal fixture
checks. The initializer includes 65,523 interrupted calls; the typed lifetime
run includes 246,689 propagated exceptions and 27,414 fatal dispatches.

Runtime/search metadata and build-only resource/anchor files reproduce
byte-for-byte in an isolated directory. Logs are
`bin/world-diff/search-ui-acceptance.log`, `search-native-breaks.log`,
`search-area-regression.log`, `search-priority-regression.log`,
`search-booster-regression.log`, `search-native-core-normal.log`,
`search-native-core-interrupt.log`, `search-native-core-breaks.log`,
`search-native-init-regression.log` and `search-native-life-regression.log`.
Selected existing native mutation build inputs also include search source;
logs are `search-area-mutation-regression.log`,
`search-priority-mutation-regression.log` and
`search-booster-mutation-regression.log`.

A 75-second startup passed in private game copy
`bin/world-diff/smoke-game-8oraaxgf`: **27/27 modules, 95 gauges**, matching
replacement bytes and no runtime errors. The log is
`bin/world-diff/search-smoke-acceptance.log`; the active game was unchanged.
Final DLL SHA-256:
`b9ed65eff63cedc4c03ebe37f2b42b0cf8b258f0786e074907fde1a60b22eca2`.
The final comment-only rebuild's `.text`, `.data`, `.rdata` and `.rsrc`
sections are byte-identical to the native-tested DLL retained at
`bin/world-diff/search-native-tested.dll`.

The drive-search family is complete through this native milestone. The DLL is
still a partial replacement. Continue with a separately bounded remaining
family and its own owner contract and acceptance.

## Remaining boundary

Thirteen World registrations are still pending native acceptance: scenery lists/library,
booster lists, weather/time, scenery settings and FACIL-backed
position/facility screens. Continue as bounded slices with their own native
acceptance before building or installing a complete World replacement. Shared
build/install integration and complete host-driven interaction remain separate.

FE, FACIL, MAIN, HOST, their verified harnesses and the shared Makefile remain
unchanged by this work.
