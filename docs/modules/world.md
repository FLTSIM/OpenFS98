# World.DLL — native scenery, position and facilities dialogs

**Partial replacement with native UI acceptance.** `bin/world-diff/WORLD_area.dll`
registers the scenery-area list (`0x13080001`), add/edit scenery area
(`0x13080003` / `0x13080004`), scenery
priority (`0x13080005`), booster/cache list (`0x13080006`), add/edit booster
(`0x13080008` / `0x13080007`), drive search (`0x13080002`) and the world-files
container (`0x1306`), exact position (`0x1308`), facilities directory (`0x1307`)
and go-to-airport (`0x1309`).
Their classes, initialization, controls, event/message maps, factories,
exception lifetimes and module binding are rebuilt in C, using
retail MFC42/SIMSUI as shared dependencies. The other 7 World registrations
remain pending native acceptance.

This is ready to review as a standalone twelve-dialog slice. It is **not a full
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
callback/file fixtures. Priority, drive search and the scenery-area list now have their own acceptance
below, as do the booster/cache list and world-files container. The native
test DLL remains a partial World replacement.

## Booster/cache list scope and owner

Start with registry **`0x13080006`**, factory **`0x223f16b0`**, template
**`0x6ae`**. This is now the eighth accepted registration, through the native
acceptance recorded below. It uses new World fixtures and the standalone World
build, independently of MAIN/HOST. Its accepted add/edit children
(`0x13080008` / `0x13080007`) execute as actual nested windows.

Read [FE's Scenery Library contract](fe.md#part-4--the-scenery-library-0x78-verified),
`src/fe/fe_library.c` (the `dialog 0x13080006` section), and
`tools/difftest/fesldiff.c` (`world_caches`, `world_cache`, `fesl_call`).
The established owner block is FE `.data+0x3000`: answer `+0`, callbacks
`+4` nothing, `+8` Add, `+0xc` Edit, `+0x10` Delete and `+0x14` Flush;
title `+0x18`, count `+0x1c`, selection `+0x20`, rows `+0x24`.
Rows are `0x80` bytes. These are caller-side contracts, not proof of World's
callback ordering, control behavior or validation. Recover those from retail.

The class/control graph, lifetimes, initializer, message/help maps, selection,
formatting, accept/cancel and owner refresh behavior have been recovered from
retail and compared independently. Core, lifetime and native harnesses use the
area-list harness structure with separate cache-row and callback fixtures;
booster behavior is not inferred from scenery-area behavior. That milestone
left eleven other registrations pending; world-files and exact-position
acceptance below reduce the remaining boundary to nine.

Keep edits within `src/world/`, World generators/harnesses, `tools/world.mk`,
`tools/run_worlddiff.sh` and World handoff documentation. Preserve FE/FACIL,
MAIN/HOST and their harnesses/docs, and the shared Makefile. The World runner
uses `bin/world-diff/wineprefix` and `bin/world-diff/wine.lock`; keep its output
and prefix separate from MAIN/HOST runs. Never install the partial DLL into
the active game. Check the worktree first and preserve any uncommitted slices
and handoff work.

The FE-owned world-files container `0x1306` (factory `0x223f1440`, template
`0x6b6`, owner fixture `world_files`) now has native acceptance below, including
its area/cache list children. FACIL's exact-position dialog `0x1308` now has
its own acceptance below. FACIL's facilities directory `0x1307` now also has native acceptance below;
go-to-airport `0x1309` now also has native acceptance below over independent
owner callbacks; keep real placement callbacks out of tests
because they reach MAIN and the aircraft simulator.

## Other bounded slices

FACIL's exact-position dialog `0x1308` uses template `0x70c`, factory
`0x223f1c60`. Its owner contract is in [FACIL's dialogs](facil.md#the-dialogs),
`src/facil/facil.c` and `tools/difftest/facildiff.c` (`world_position`). FACIL's
validator/apply/revert/reload callbacks are established; its World class now
has the native control/error/lifetime acceptance below. Facilities directory now has native acceptance below; the separate
flight-selection owner flow remains pending.
Do not execute real placement callbacks against the active game while building
UI fixtures: FACIL's apply path reaches MAIN, CHAIN and the aircraft simulator.

Facilities directory `0x1307` and go-to-airport `0x1309` have richer list and
nested-dialog contracts in the same owner/harness, with native World acceptance
recorded below. Weather, time, scenery
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

## Scenery-area list accepted slice

This slice implements the scenery-area list `0x13080001`, template `0x6b5`,
factory `0x223f12e0`. `world_list.c` implements its core routines and initializer
behind independent services. Its class, factory and native UI have the
acceptance below; it is the seventh registration in the partial DLL.

Its owner is [FE's Scenery Library](fe.md#part-4--the-scenery-library-0x78-verified),
`src/fe/fe_library.c` and `tools/difftest/fesldiff.c`'s `world_areas` stand-in.
The descriptor at object `+0x10dc` contains heading `+0x1c`, signed row count
`+0x20`, the 0x40-byte row array `+0x24` and selection `+0x28`. The callbacks
are `stdcall(descriptor)`: `+8` Add, `+0xc` Edit, `+0x10` Delete, `+0x14` Browse
and `+0x18` Priority. Their returns are ignored. The `+4` check/no-op callback
is not used by these World routines. The row's title, layer, active and
protected fields are `+0x18`, `+0x2c`, `+0x34` and `+0x3c`. FE and its verified
harness are unchanged; no real files or nested owner dialogs execute here.

Recovered core behavior:

- Collect (`0x223fa870`) gets LB_GETCURSEL from the control HWND at object
  `+0x1778` and reloads the descriptor before storing selection.
- Selection change (`0x223fac70`) collects, reads protection through that
  selection and caches one boolean for the Delete (`+0x1998`) and Edit
  (`+0x189c`) enables. A callback changing protection between enables does
  not change the second decision.
- Add (`0x223fabb0`), Priority (`0x223fabd0`), Edit (`0x223fac30`) and Browse
  (`0x223fac50`) invoke the owner then repopulate, without collecting first.
  Delete (`0x223fabf0`) collects before invoking the owner, then repopulates.
- Double-click (`0x223fad10`) calls LB_GETCURSEL but discards its return. It
  tests protection using the descriptor's old selection and invokes Edit
  only if that row is unprotected. It does not implicitly collect the new
  selection before that check or owner dispatch.
- OK (`0x223fab80`) collects, writes answer **one**, reloads the current
  vtable and closes. Cancel (`0x223faba0`) closes without changing the answer.
  There is no validation/check callback on either path.

Population `0x223fa8a0` constructs its CString first, assigns the heading,
saves the current UI selection, then resets the list. Each row becomes
`title<TAB>layer<TAB>001+` (or `-`), with one-based `%03d` position and raw
nonzero active state. The layer strings are extracted from retail's switch;
all five valid values 10..50 and every default entry are validated by the
generator. Descriptor/row accesses stay fresh across assignment and append
calls. Row stride is `0x40`; the row-count test is signed.

The scratch buffer survives iterations: an invalid later layer reuses the
preceding formatted index, and an invalid first layer reads indeterminate
stack text. The C local layout retains CString at `+0`, saved selection at
`+4` and 100-byte scratch at `+8`; this differs from the priority class's
adjacent locals. The fixture paints only the owned scratch through the CString
constructor and independently requires the first-invalid-layer witness.
No retail stack bytes are copied into source.

After population, a positive LB_GETCOUNT enables Add/Edit/Delete, stores the
saved selection (mapping exactly `-1` to zero), calls LB_SETCURSEL, then runs
selection change to refresh Edit/Delete protection. An empty list disables
those three buttons without writing descriptor selection. The CString is
destroyed on success and during real exception unwinding after construction;
it is marked dead before ordinary destruction and is not retried if that
service throws.

Selection and double-click do not range-check their row read. The differential
fixture supplies committed rows before/after the logical array so negative
and out-of-range selections can be observed safely, including 32-bit shift
wraparound for `-1`. This establishes those accesses within the fixture;
it does not claim acceptance of arbitrary invalid-address faults against real
owner allocations. Native empty-list/selection behavior remains to be tested.

Initializer `0x223fa5d0` calls base init, reloads the vtable and skins with
`0x14f`, the current resource module and flags 15. Failed skinning returns zero.
Success sets up the artwork; OK/Cancel buttons with `0x132` and coordinates
0/0; five action buttons with `0x137` and coordinates 1/1; list/heading/static
text; and tab stops `0x118` / `0x154`. The list uses font `+0x450` and its
label `+0x16a0` uses `+0x370`; other text uses `+0x220`. Resource modules are
loaded afresh for each button. It populates and returns one, without extra
base color-field or close-pointer stores.

`gen_world_list.py` recovers the complete class graph as metadata: 14 embedded
controls and DDX bindings, their reverse destruction and MSVC cleanup actions,
terminated help map, 60-slot vtable and nine local message entries. Preferred
VAs are constructor `0x223fa260`, destructor `0x223fa3a0`, scalar deleting
destructor `0x223fa380`, DDX `0x223fa4b0`, help/context `0x223facd0` /
`0x223facf0`, vtable `0x2240f750`, message map `0x2240f658` and base-map getter
`0x223f5d40`. Object size is `0x1c8c`. Message entries cover the five action
buttons `0x1aa3..0x1aa7`, list `0x1b16` selection/double-click, help and context
menu. PostNcDestroy is shared `0x223f3d30`; virtual OK/Cancel are the routines
above, while validation/error remain inherited imports. Metadata extraction alone establishes the graph; behavioral lifetime and
native acceptance follow below. Both includes reproduce
byte-for-byte in an isolated directory and contain no code/resource payloads.

```sh
make -f tools/world.mk world-list-metadata
make -f tools/world.mk world-list-core N=20000
make -f tools/world.mk world-list-core N=2000 MODE=unwind
make -f tools/world.mk world-list-breaks
```

With seed `0x4a971b63` and the reference hash above, the normal run made
**240,000 comparisons** and 240,000 independent reference-side fixture checks.
The unwind run made **521,948 comparisons**, including **497,948 real
unwinds**, plus 24,000 normal fixture checks. All matched. Twelve operations
cover initialization, population, collection, protection, all five owner
actions, double-click, OK and Cancel. The suite compares ordered calls,
defined returns, the complete guarded `0x1c8c`-byte object, two descriptors,
padded rows, title buffers, simulated list state, callback counters,
resource-module state and CString liveness. Scenarios include negative/empty/
positive counts, divergent UI/descriptor selections, protected rows, all valid
and invalid layer paths, raw noncanonical active state, descriptor/vtable
swaps and changing protection/resource modules at external boundaries. Retail
retains its MSVC frame handler; the harness unwinds with real RtlUnwind rather
than escaping through a vectored handler. This accepts CString cleanup in the
core call chains; the separate embedded-control/factory lifetime suite follows
below.

All **31 deliberate core/initializer mutations** produced exit 1 with an
actual `LIST DIFFER` transcript. They cover CString unwind cleanup, heading
assignment, reset/selection handling, row count/stride, formatting/capacity,
descriptor reloads, invalid-layer scratch reuse, raw active flags, button
enables, cached protection, OK/Cancel answers, close-vtable reload, owner
callback choice, Delete collection, double-click's stale selection/protection,
skin vtable/result, module reloads, artwork, button setup, list font and tabs.
Each mutant is independently compiled and run with 1,024 scenarios in unwind
mode; build failures, faults and timeouts do not count. The existing search
core regression also passed **26,000 comparisons** and 26,000 fixture checks
with zero differences.

Acceptance logs are `bin/world-diff/list-core-normal.log`,
`list-core-unwind.log`, `list-core-breaks.log` and `list-search-regression.log`.
Mutant source, executables and actual difference transcripts are under
`bin/world-diff/list-breaks/`. The World runner retains its private Wine prefix
and lock; existing native registration/build inputs are unchanged.

### Scenery-area list class/factory lifetime acceptance

`world_list_objects.c` implements the constructor, destructor, scalar deleting
destructor, factory, DDX, help/context and PostNcDestroy using the extracted
14-control graph. Scalar deleting destruction is named
`fs98_world_list_scalar_delete`, keeping it separate from the existing core
Delete action. Construction installs the class vtable only after every
embedded constructor succeeds. Destruction retains the incoming vtable;
remaining controls and the base still receive cleanup if a destructor throws,
without retrying the failed control. UI delete runs only for flag bit 0 and
after successful destruction. PostNcDestroy ignores null objects and otherwise
dispatches virtual deleting destruction with flag 1.

The factory ignores its parent, allocates `0x1c8c`, constructs with null parent,
stores descriptor `+0x10dc`, then reads/stores the current module at `+0x80`.
A null allocation faults on the descriptor store at address `0x10dc`.
Ownership transfers before Create. Failed Create displays
`CSceneryAreas::Create() failed\n` and continues to Show(1); the returned HWND
is read afterwards. Partial construction releases through the dialog delete
operator, whose import/convention differs from UI delete. Exceptions after
ownership transfer do not invoke that construction cleanup.

Retail's typed `CException*` handler unwinds first, frees a partial construction
if needed, then calls the fatal `SIMSUI_ExitCleanlyE` boundary. The test uses
real `_CxxThrowException` with direct, derived and virtual-base pointers, null
pointers, unrelated pointer types and incompatible const pointers. Matching
pointers reach fatal dispatch after conversion/cleanup; other exceptions
propagate. The fixture ends that comparison side instead of invoking SIMSUI's
message box or CRT abort. Embedded controls, Create/Show, DDX and WinHelp remain
independent service fixtures in this suite; native control acceptance is below.

```sh
make -f tools/world.mk world-list-life N=20000
make -f tools/world.mk world-list-life N=2000 MODE=unwind
make -f tools/world.mk world-list-life N=2000 MODE=typed
make -f tools/world.mk world-list-life-breaks
```

With seed `0x4a971b63` and the reference hash above, nine operations made
**180,000 normal comparisons**, including 1,819 expected allocation write
faults and 178,181 normal fixture checks. Each exceptional suite made
**236,859 comparisons**: unwind includes **218,859 real unwinds**, while typed
includes **196,297 propagated exceptions** and **22,562 fatal dispatches**.
Each also includes 182 expected allocation faults and 17,818 normal fixture
checks. All matched. The fixture compares the complete guarded `0x1c8c`-byte
object, 0x30-byte descriptor, exchange/help buffers, module state, deletion
counters, ordered calls and defined returns. Independent witnesses check
control markers, call counts, constructor vtable, base destruction, deletion
flags, factory result, context result and null/non-null PostNcDestroy.

All **20 lifetime mutations** produced actual differences with exit 1, bringing
core/initialization/lifetimes to **51/51**. Besides the earlier lifetime
boundaries, these specifically test destruction order, retrying a failed child
and freeing before typed-catch unwind. Faults, timeouts and build failures do
not count. Logs are `bin/world-diff/list-life-normal.log`,
`list-life-unwind.log`, `list-life-typed.log`, `list-life-breaks.log` and
`list-life-core-regression.log`; mutant sources, executables and differences
are in `bin/world-diff/list-life-breaks/`. The core/init regression again passed
**521,948 comparisons**, including 497,948 real unwinds, with 24,000 normal
fixture checks.

### Scenery-area list native acceptance

`world_runtime.c` now rebuilds five 60-slot vtables and five message maps,
binds the list services and registers factory `0x13080001`, bringing the partial
DLL to seven registrations. Module handles are propagated to the new factory
on attach/init/deinit. `world_list.c` uses real CString assignment/append,
CSimsListBox initialization and tab-stop services; existing shared bindings
continue to serve the previously accepted classes.

`worldlistuidiff.c` uses actual MFC/SIMSUI controls and an independent FE-shaped
owner. Twenty-one cases cover empty through six-row lists, all five valid
layers, active states, protected-first, mixed-protection and all-protected
rows, and initial descriptor selections that differ from the initial UI.
Twelve scripts cover baseline, Return/OK, Escape/Cancel, protected selection,
Add/Edit/Delete/Browse/Priority, double-click, stale double-click selection and
stale Delete selection. Native fixtures use committed rows before/after the
logical array to observe retail's unchecked protection read when removing the
last selected item leaves LB_GETCURSEL at `-1`. This preserves the access;
it is not a claim about faults from arbitrary owner addresses.

Owner callbacks open actual nested World dialogs: Add/Edit use the accepted
area factories, Priority uses its dialog and move/check callbacks, and Browse
uses drive search with an independent Add callback. No real FE files or game
placement callbacks execute. Each child is inspected for import/local-dispatch
ownership and compared through child metadata, pixels and callback logs, then
closed. Area objects receive caller-owned scalar deletion; priority/search
use their automatic deletion. Parent list population, disabled controls,
ordered rows/active markers, callback selections, full descriptor/row/text
state, nested descriptor state and automatic parent destruction are compared.
The suite verifies ownership of the factory, local class slots and all nine
message handlers, plus module init, double deinit and repeated unloads.

Independent witnesses require initial UI selection zero for nonempty lists,
Edit/Delete protection, disabled Add on empty lists, callback choices, list
counts, double-click's stale descriptor protection, Delete's fresh selection,
OK answer one and Cancel preserving a nonzero seeded answer. Pixel comparisons
include parent and child windows. Invalid first-layer scratch remains a
separately painted core fixture; native inputs use defined layer text.

```sh
make -f tools/world.mk world-list-ui
make -f tools/world.mk world-list-native-breaks
make -f tools/world.mk worlduidiff-run
make -f tools/world.mk world-priority-ui
make -f tools/world.mk world-booster-ui
make -f tools/world.mk world-search-ui
make -f tools/world.mk world-smoke
```

The suite passed **504 paired parent/child pixel/state/callback comparisons**,
**2,180 witnesses**, eight module-init comparisons and two full unload cycles.
Baselines are `bin/world-diff/ui-images/list-baseline-*.bmp`. The embedded
control is `CSimsListBox` (constructor/import `0x22415720`, initializer
`0x22415718`); its final text column uses `+`/`-` status markers rather than
an interactive checkbox. The native cases compare both row text and pixels.

All **13 native mutations** produced exit 1 with actual `LIST UI DIFFER`
transcripts, bringing this family to **64/64** across core/init, lifetimes
and native UI. They cover column tabs, row numbering, active markers,
protection, OK/Cancel answers, Delete collection, Add/Edit dispatch,
double-click dispatch/stale selection, automatic deletion and empty-list Add
enabling. The tab mutation changed pixels while descriptor state, controls
and callbacks still matched. Faults, timeouts and build failures do not count.
Inputs, hashes, independently compiled DLLs/harnesses and actual difference
logs are retained under `bin/world-diff/list-native-breaks/`.

The seven-registration DLL passed the existing **288 area**, **210 priority**,
**320 booster** and **680 drive-search** paired comparisons, totaling **1,498**,
with their full witnesses/module-init checks and two unload cycles each.
Booster's shared-font cleanup on unload still passes. Selected existing area,
priority, booster and search color mutations were detected with the expanded
native build inputs.

Generated runtime/list metadata and build-only resource/anchor files reproduce
byte-for-byte in an isolated directory. Acceptance logs are
`bin/world-diff/list-ui-acceptance.log`, `list-native-breaks.log`,
`list-area-regression.log`, `list-priority-regression.log`,
`list-booster-regression.log` and `list-search-ui-regression.log`.
Selected existing color-mutation logs are `list-area-mutation-regression.log`,
`list-priority-mutation-regression.log`, `list-booster-mutation-regression.log`
and `list-search-mutation-regression.log`. Mutation build inputs include both
new list source files.

A 75-second startup passed in private copy
`bin/world-diff/smoke-game-vxa1d47d`: **27/27 modules, 95 gauges**, matching
replacement bytes and no runtime errors. The log is
`bin/world-diff/list-smoke-acceptance.log`; the active game was unchanged.
Final DLL SHA-256:
`101a33d8d925385001be48f45a3d6af711ccd0c67d60133389bcdb310674bec0`.
Its `.text`, `.data`, `.rdata` and `.rsrc` sections match the native-tested DLL
retained as `bin/world-diff/list-native-tested.dll` after the final comment-only
rebuild.

The scenery-area list is complete through native acceptance. The DLL remains
a partial replacement; continue with a separately bounded remaining family.

## Booster/cache list accepted slice

`world_cache.c` and `world_cache_objects.c` implement registry `0x13080006`,
factory `0x223f16b0`, template `0x6ae` and class `CPerformanceBoosters`.
`gen_world_cache.py` extracts 15 embedded controls and DDX bindings, reverse
destruction, the MSVC cleanup graph, terminated help map, 60-slot vtable and
seven message entries. The constructor is `0x223f91c0`, destructor
`0x223f9310`, scalar deleting destructor `0x223f92f0`, DDX `0x223f9430`,
initializer `0x223f9560`, vtable `0x2240f338` and message map `0x2240f270`.
The base-map getter is `0x223f5d40`. The object is `0x1d00` bytes; descriptor
is `+0x10dc`, local accept/cancel result `+0x1cfc`, list control `+0x18c4`
and its HWND `+0x18e4`. The title field at descriptor `+0x18` is not read by
these routines. Owner callbacks are `stdcall(descriptor)`.

Recovered behavior:

- Collect (`0x223f99f0`) is empty. Check (`0x223f9c20`) directly calls owner
  `+4` and returns its result. Validate (`0x223f9c00`) gets the UI selection,
  discards it and returns zero. These helpers are direct calls, separate from
  inherited virtual validation/error slots.
- OK (`0x223f97d0`) writes local result 1, calls Collect and Check, and calls
  Validate only when Check returns nonzero. It then writes descriptor answer
  1 and closes through the freshly loaded vtable. Even nonzero Check cannot
  prevent closing because Validate always returns zero. OK does not collect
  selection or active flags. Cancel (`0x223f9810`) loads the close vtable,
  writes local result 2 and closes, preserving the descriptor answer.
- Selection (`0x223f9930`) saves LB_GETCURSEL, then updates source (`+0x18`),
  cache path (`+4`) and description (`+0x40`) detail text. Each title call
  reloads descriptor/rows while retaining the saved index. Only afterwards
  does it store selection at descriptor `+0x20`. There is no range check.
- Add (`0x223f9830`) calls owner `+8` then populates. Flush (`0x223f9910`)
  calls owner `+0x14` then populates; World performs no filesystem work.
- Edit (`0x223f98c0`) and Delete (`0x223f9850`) save UI selection, call owner
  `+0xc` / `+0x10` with the **old descriptor selection**, and populate. Delete
  decrements the saved index exactly when new LB_GETCOUNT equals that index.
  Both set UI selection and then descriptor selection to the saved/adjusted
  index. They do not refresh details again. Delete can therefore leave the
  deleted row's detail text visible after selection moves to its predecessor.

Populate (`0x223f9a00`) constructs CString, resets the list and adds each
`name<TAB>marker` row. The name is row `+0x2c`; **only active `+0x54 == 1`**
produces `+`, with all other values producing `-`. Descriptor/row reads stay
fresh across external calls and the row-count loop is signed. After the loop,
exactly zero count selects the descriptor index, clears three detail texts
with null pointers, focuses the list and enables Add/Flush while disabling
Edit/Delete. Any nonzero count, including negative, selects the descriptor
index, reads its details, focuses the list and enables all four buttons.
It does not normalize selection or store a new one. CString is destroyed on
success and real exception unwind; destruction is marked dead before the
ordinary call so a throwing destructor is not retried.

The embedded list is `CSimsListBox`, not a checkbox control: status is text,
with no checkbox/toggle handler. Double-click also has no local handler and
does not open Edit. The seven entries cover four buttons `0x1a9d..0x1aa0`,
list `0x1b10` selection, help and context menu. Initializer uses skin `0x14f`,
art `0xf831`, two `0x132` OK/Cancel buttons and four `0x137` action buttons.
It initializes the list with font `+0x450` and heading `+0x15b4`, its heading
with font `+0x370`, the group/text with `+0x220`, and a single tab stop 250.
It reloads the resource module for every button, then populates and returns 1;
skin failure returns zero.

The constructor installs the class vtable only after all embedded constructors
succeed. Destruction retains the incoming vtable and unwinds remaining earlier
controls/base if a destructor throws, without retrying the failed control.
Deleting destruction uses UI delete only for flag bit 0 after success.
PostNcDestroy dispatches virtual deleting destruction with flag 1 for non-null
objects. Factory ignores parent, allocates `0x1d00`, constructs with null
parent, stores descriptor/module and transfers ownership before Create.
Failed Create displays `CPerformanceBoosters::Create() failed\n` then still
shows the window and returns its post-Show HWND. Null allocation faults on a
write at `0x10dc`. Partial construction cleanup uses dialog delete, distinct
from UI delete. Typed `CException*` catches perform real unwind before fatal
`SIMSUI_ExitCleanlyE` dispatch; unrelated/incompatible exceptions propagate.
The fatal fixture records that boundary instead of executing its abort.

```sh
make -f tools/world.mk world-cache-metadata
make -f tools/world.mk world-cache-core N=20000
make -f tools/world.mk world-cache-core N=2000 MODE=unwind
make -f tools/world.mk world-cache-breaks
make -f tools/world.mk world-cache-life N=20000
make -f tools/world.mk world-cache-life N=2000 MODE=unwind
make -f tools/world.mk world-cache-life N=2000 MODE=typed
make -f tools/world.mk world-cache-life-breaks
make -f tools/world.mk world-cache-ui
make -f tools/world.mk world-cache-native-breaks
```

Using seed `0x4a971b63` and the reference SHA-256 recorded above, the 12-operation
core/init suite passed **240,000 normal comparisons** and independent fixture
checks. Its unwind run passed **337,392 comparisons**, including **313,392 real
unwinds**, plus 24,000 normal fixture checks. It compares ordered calls, defined
returns, full guarded object, two descriptors, padded independent cache rows,
four string fields, simulated list state, callback counters, resource modules
and CString liveness. Cases cover negative/empty/positive counts, divergent UI
and descriptor selections, active 0/1/2/high-bit values, descriptor/vtable/module
swaps at service boundaries, both check outcomes and interrupted call chains.
Padded committed rows cover the observed unchecked reads, including `-1`;
this is not acceptance of arbitrary invalid owner-address faults.

The nine-operation lifetime suite passed **180,000 normal comparisons**, with
1,819 expected allocation write faults and 178,181 independent fixture checks.
Both exceptional suites passed **250,537 comparisons** and 182 expected
allocation faults, with 17,818 normal fixture checks each. Unwind mode includes
**232,537 real unwinds**. Typed mode includes **208,762 propagated exceptions**
and **23,775 fatal dispatches**, covering direct, derived, virtual-base and null
exception pointers, unrelated types and incompatible const pointers. Full
objects, descriptor/exchange/help buffers, module state, deletion counters,
ordered calls and returns match. Construction markers, deletion flags, call
counts, vtable timing, factory results and PostNcDestroy have independent
witnesses. All **29 core/init** and **20 lifetime mutations** produced exit 1
with actual difference transcripts; faults, timeouts and build failures do not
count. Sources, executables and logs are retained in `bin/world-diff/cache-breaks/`
and `cache-life-breaks/`.

Native runtime builds six class vtables/maps and registers eight factories.
`worldcacheuidiff.c` uses real MFC/SIMSUI controls and its own FE-shaped owner.
Twenty-eight cases cover zero through six rows, four active-state patterns,
initial descriptor selection at the last row and padded detail rows. Twelve
scripts cover baseline, keyboard Return/Escape, selection, nested Add/Edit,
Delete, Flush, double-click with no Edit, stale descriptor selection for
Edit/Delete, and OK after changing UI selection without a notification.
Add/Edit open actual accepted booster editors, edit controls, invoke an
independent validator and close with automatic child deletion. Parent/child
control metadata, pixels, callbacks and complete descriptor/row/text state
are compared. Factory, local vtable and all seven message-handler ownership
are checked so native calls cannot fall back to retail implementation code.
Parent local result and automatic deletion are also compared.

The native suite passed **672 paired pixel/state/callback comparisons**,
**2,840 witnesses**, eight module-init comparisons and two full unload cycles.
Baseline images are `bin/world-diff/ui-images/cache-baseline-*.bmp`.
All **14 native mutations** produced exit 1 with actual `CACHE UI DIFFER`
transcripts, bringing this slice to **63/63 deliberate breaks** across the
three suites. Native mutations cover tabs, markers, exact active equality,
accept selection/result/answer, Cancel answer, Delete adjustment, Add/Edit/Flush
and selection dispatch, automatic deletion and empty-list Edit enabling.
Inputs/hashes, independently compiled DLLs/harnesses and difference logs are
under `bin/world-diff/cache-native-breaks/`.

Acceptance logs are `bin/world-diff/cache-core-normal.log`,
`cache-core-unwind.log`, `cache-core-breaks.log`, `cache-life-normal.log`,
`cache-life-unwind.log`, `cache-life-typed.log`, `cache-life-breaks.log`,
`cache-ui-acceptance.log` and `cache-native-breaks.log`. The runner retains
WORLD's private Wine prefix and lock. No real FE file callbacks execute.

The eight-registration DLL also passed **288 area**, **210 priority**,
**320 booster**, **680 drive-search** and **504 scenery-area-list** native
comparisons, totaling **2,002**, with their witnesses/module-init checks and
two unload cycles per suite. Generated cache/runtime
metadata and build-only resource/anchor files reproduce byte-for-byte in an
isolated directory; the retail dialog registry inventory is unchanged.
All existing World native mutation build lists include the cache sources.
Selected existing area, priority, booster and search color mutations and the
area-list tab mutation also produced actual differences with the expanded
build. Regression logs are `bin/world-diff/cache-area-regression.log`,
`cache-priority-regression.log`, `cache-booster-regression.log`,
`cache-search-regression.log` and `cache-list-regression.log`; corresponding
mutation logs use `cache-<family>-mutation-regression.log`.

A 75-second startup passed in private copy
`bin/world-diff/smoke-game-lwba0okj`: **27/27 modules, 95 gauges**, matching
replacement bytes and no runtime errors. The log is
`bin/world-diff/cache-smoke-acceptance.log`; the active game was unchanged.
Final DLL SHA-256:
`0066518f21c00770af813ac09af24f382528881095ed15e1dc165572b5f1759d`.
The native-tested DLL is retained as `bin/world-diff/cache-native-tested.dll`;
it is byte-identical to the final and startup-tested artifact.

The booster/cache list is complete through native acceptance. That milestone
produced a partial World replacement with eight accepted registrations.

## World-files container accepted slice

The ninth accepted registration is **`0x1306`**, factory **`0x223f1440`**,
template **`0x6b6`**. `world_files.c` implements core/initialization and
`world_files_objects.c` implements class/factory lifetimes. Its independent
owner fixtures follow FE's established `.data+0x2e28` block: answer `+0`,
`stdcall(descriptor)` callbacks `+4` Check/no-op, `+8` Edit Areas and `+0xc`
Edit Caches, signed count `+0x10`, rows `+0x14` and selection `+0x18`.
Rows are `0x40` bytes: raw enable flag `+0`, path `+8`, title `+0x1c` and
second title/description `+0x30`. No real FE file/placement callbacks execute.

`gen_world_files.py` extracts the nine-control constructor/destructor graph,
MSVC cleanup actions, nine DDX bindings, terminated help map, 60-slot vtable
and six message entries. Constructor `0x223fd420`, destructor `0x223fd520`,
scalar deleting destructor `0x223fd500`, DDX `0x223fd600`, initializer
`0x223fd6c0`, vtable `0x22410040` and message map `0x2240ff90` are now rebuilt.
The object is `0x1828` bytes, descriptor `+0x10dc`, local result `+0x1824`,
list control `+0x152c` and its HWND `+0x154c`. The base-map getter remains
`0x223f5d40`; PostNcDestroy uses the shared virtual deleting-destruction path.

Recovered behavior:

- Populate (`0x223fd930`) captures count and descriptor selection before any
  external call, appends that many titles without resetting the list, sets
  UI selection to the saved index, reads its description and updates the
  two buttons. Count is signed and stays fixed across descriptor changes;
  descriptor/row pointers stay fresh between appends and the detail call.
  Calling Populate twice therefore appends duplicates. Negative/zero counts
  append nothing but still select and read the detail row.
- Selection (`0x223fda10`) reads UI selection, updates the description,
  reloads the descriptor and stores the saved index, then updates enables.
  Enable (`0x223fd9c0`) independently reloads descriptor, selection and rows
  before each button call. It passes the row flag unchanged, including 2 and
  high-bit values. This differs from the scenery-area list's cached boolean.
- Edit Areas (`0x223fd870`) and Edit Caches (`0x223fd880`) invoke owner `+8`
  and `+0xc`, respectively, with no implicit selection collection or refresh.
  The callbacks see the descriptor's old selection even when UI selection
  differs. Owner changes to row strings do not update displayed text.
- OK (`0x223fd8b0`) writes local result 1, executes the empty shared Collect,
  invokes owner Check and conditionally calls the empty zero-return validator
  `0x223f3720`. It writes answer 1 and closes through a freshly loaded vtable,
  with no validation error or selection collection. Cancel (`0x223fd890`)
  retains the close vtable, writes local result 2 and preserves the answer.
- Background erase (`0x223fda60`, WM_ERASEBKGND) posts WM_SETFOCUS to the
  list before calling imported `CSimsDialog::OnEraseBkgnd`, returning that
  handler's result. Its native fixture supplies a real device context.
  Double-click has no local action handler.

The initializer calls base init, reloads the skin vtable and uses `0x14f`, the
current resource module and flags 15. Failed skin returns zero. Success sets
up art `0xf831`, two `0x132` OK/Cancel buttons, two `0x137` action buttons,
heading font `+0x370`, list font `+0x450` and group/detail font `+0x220`, then
populates and returns 1. Button resource modules are reloaded individually.
There are no CString lifetimes, numeric controls, tab stops or active toggles
in this class. Its six messages cover the two action buttons `0x1aab/0x1aac`,
list `0x1b11` selection, erase, help and context menu.

Constructor installs its vtable only after all nine controls succeed.
Destruction preserves the incoming vtable and cleans earlier controls/base
when a child destructor throws, without retrying the failed child. Scalar
delete releases only for flag bit 0 after successful destruction.
Factory ignores parent, allocates `0x1828`, constructs with null parent,
stores descriptor/module and transfers ownership before Create. It ignores
Create's result: **no failure message**, then Show(1) and post-Show HWND return.
Null allocation faults on a write at `0x10dc`. Partial construction uses
dialog delete, distinct from UI delete; typed `CException*` conversion/catch
runs real unwind before the fatal service boundary. Unrelated/incompatible
exceptions propagate. The fixture terminates that comparison side at fatal
dispatch instead of displaying the real SIMSUI error and aborting the process.

```sh
make -f tools/world.mk world-files-metadata
make -f tools/world.mk world-files-core N=20000
make -f tools/world.mk world-files-core N=2000 MODE=unwind
make -f tools/world.mk world-files-breaks
make -f tools/world.mk world-files-life N=20000
make -f tools/world.mk world-files-life N=2000 MODE=unwind
make -f tools/world.mk world-files-life N=2000 MODE=typed
make -f tools/world.mk world-files-life-breaks
make -f tools/world.mk world-files-ui
make -f tools/world.mk world-files-native-breaks
```

The reference SHA-256 is recorded above; seed is `0x4a971b63`.
The twelve-operation core/init suite passed **240,000 normal comparisons**
and independent fixture checks. Unwind mode passed **94,826 comparisons**,
including **70,826 real unwinds**, and 24,000 normal fixture checks. It compares
ordered calls, defined returns, the guarded whole object, two descriptors,
padded independent rows/text buffers, list state, callback counters and
resource-module state. Cases include negative/empty/positive counts,
UI/descriptor selection divergence, raw enable flags, descriptor/vtable/module
swaps and changing flags between enables. Explicit witnesses check appending
without reset, unchanged population selection, direct callback counts,
collection/validation no-ops, erase result, selection and accept/cancel.

Lifetime acceptance passed **180,000 normal comparisons**, with 1,819 expected
allocation write faults and 178,181 independent fixture checks. Both exceptional
modes passed **166,818 comparisons**, 182 expected allocation faults and 17,818
normal fixture checks each. Unwind mode includes **148,818 real unwinds**;
typed mode includes **132,928 propagated exceptions** and **15,890 fatal
dispatches**. It covers direct/derived/virtual-base/null exception pointers,
unrelated types and incompatible const pointers, all controls/DDX/help data,
module mutation, ownership transfer, failure paths and automatic deletion.

`worldfilesuidiff.c` uses actual MFC/SIMSUI controls and independent FE-shaped
owners. Twenty-eight cases cover zero through six logical rows, four enable
flag patterns and last-row initial selection. Twelve scripts cover baseline,
Return/Escape, first/last selection, both nested editors, erase, double-click
with no action, stale descriptor selection for both owner callbacks, and OK
with UI selection changed without notification. Edit Areas opens the accepted
scenery-area list and exercises selection/Add; Edit Caches opens the accepted
cache list and exercises Flush. Child callbacks remain independent fixtures.
Parent/child pixels, metadata, ordered callbacks and complete descriptor/row/
text state match. Both child and parent automatic deletion are counted.
Factory, local class slots and all six message handlers must belong to the
module under test, preventing fallback into retail implementation code.

The native suite passed **672 pixel/state/callback pairs**, **2,868 witnesses**,
eight module-init comparisons and two complete unload cycles. Baselines are
`bin/world-diff/ui-images/files-baseline-*.bmp`. Padded committed rows permit
the unchecked `-1`/out-of-range reads to be observed safely, including empty
lists; this does not accept arbitrary invalid owner allocations or establish
that retail tolerates a null row array on an empty list.

All **26 core/init**, **19 lifetime** and **11 native mutations** produced
exit 1 with actual difference transcripts: **56/56 detected**. Core breaks
include missing append/reset behavior, cached count, row/index/detail fields,
raw enable flags and reloading, selection ordering, callback refresh/choice,
accept/cancel/check state, erase message/return and initializer bindings.
Lifetime breaks cover control/vtable timing, cleanup order, typed catches,
allocation/template/parent/handle/ownership, deletion flags, DDX, help and
PostNcDestroy. Native breaks cover list font, accept collection/result/answer,
Cancel answer, area/cache/selection dispatch, automatic deletion, callback
refresh and button polarity. Build failures, faults and timeouts do not count.
Sources, executables, input hashes and actual difference transcripts are in
`bin/world-diff/files-breaks/`, `files-life-breaks/` and `files-native-breaks/`.

Acceptance logs are `bin/world-diff/files-core-normal.log`,
`files-core-unwind.log`, `files-core-breaks.log`, `files-life-normal.log`,
`files-life-unwind.log`, `files-life-typed.log`, `files-life-breaks.log`,
`files-ui-acceptance.log` and `files-native-breaks.log`. Generated includes
contain ABI metadata only. Metadata for the files class and runtime, plus
build-only resource/anchor outputs, reproduce byte-for-byte in an isolated directory;
the retail registry inventory is unchanged. All existing World native
mutation build inputs include the new files source units.

The nine-registration DLL passed **288 area**, **210 priority**, **320 booster**,
**680 drive-search**, **504 scenery-area-list** and **672 booster/cache-list**
paired comparisons, totaling **2,674**, with their witnesses/module-init checks
and two unload cycles per suite. Selected area/priority/booster/search color
mutations and area/cache list tab mutations produced actual differences with
the expanded native builds. Logs are `bin/world-diff/files-<family>-regression.log`
and `files-<family>-mutation-regression.log`, with family `area`, `priority`,
`booster`, `search`, `list` or `cache`.

A 75-second startup passed in private copy
`bin/world-diff/smoke-game-iapym8g4`: **27/27 modules, 95 gauges**, matching
replacement bytes and no runtime errors. Its log is
`bin/world-diff/files-smoke-acceptance.log`; the active game was unchanged.
Final DLL SHA-256:
`aab7b831d973dbe40ae8c93925378eee85b4ff079cee2b7604795a82e2768cae`.
The final DLL, `bin/world-diff/files-native-tested.dll` and the startup-tested
copy are byte-identical.

The world-files container is complete through native acceptance. That milestone
produced a partial World replacement with nine accepted registrations.

## Exact-position accepted slice

The tenth registration is **`0x1308`**, template **`0x70c`**, factory
**`0x223f1c60`**, retail class `CGoToCoordinates` ("Go To Exact Location").
`world_position.c` implements text exchanges, initialization, validation/error
ordering, notifications and owner dispatch. `world_position_objects.c` rebuilds
its class and factory lifetimes. The owner is FACIL, block `.data+0x158`;
`src/facil/facil.c` and `facildiff.c`'s `world_position` remain unchanged.
Callbacks are `stdcall(descriptor)`: `+4` validate, `+8` apply, `+0xc` copy
Aircraft to Tower (revert), `+0x10` Select Flight/reload and `+0x14` Facilities.
The fixture never runs real aircraft-placement, MAIN or filesystem callbacks.

The eight text records have status at the offset below, buffer pointer `+4`,
capacity `+8`, minimum/maximum pointers `+0xc/+0x10`, and (where used) change
flag `+0x14`. Answer is `+0`; descriptor `+0x18` gates Select Flight.

| Field | Status offset | Control id | Snapshot offset |
| --- | --- | --- | --- |
| Aircraft latitude | `0x64` | `0x119c` | `0x1145` |
| Aircraft longitude | `0x7c` | `0x11a3` | `0x11aa` |
| Aircraft altitude | `0x94` | `0x11a4` | `0x10e0` |
| Heading | `0xac` | `0x119f` | `0x120f` |
| Airspeed | `0xc0` | `0x119e` | `0x1274` |
| Tower latitude | `0x20` | `0x11a1` | `0x133e` |
| Tower longitude | `0x38` | `0x11a2` | `0x13a3` |
| Tower altitude | `0x50` | `0x119d` | `0x12d9` |

WORLD performs no numeric parsing in this class. FACIL owns NUMERIC/UTIL
conversion and simulator placement; its already verified behavior remains
separate from World's UI acceptance. New core/native callbacks supply statuses
and effects independently rather than invoking FACIL or copying its code.

Recovered behavior:

- Populate (`0x224043f0`) sets the eight texts in table order, reloading the
  descriptor each time, then enables Select Flight exactly when descriptor
  `+0x18` is zero. Collect (`0x22404950`) reads aircraft altitude, aircraft
  latitude/longitude, heading, airspeed, tower altitude, tower latitude/longitude,
  reloading each pointer and caller-supplied capacity. It does not parse text.
- Snapshot (`0x224047c0`) does nothing for a zero argument. Any nonzero value
  copies all eight strings, including NUL, into the packed 101-byte buffers
  above. Retail copies without a length check; the implementation preserves
  that behavior. Fixtures cover empty and 100-character strings in committed
  allocations, without claiming acceptance of arbitrary invalid/overflowing
  owner buffers.
- Validate (`0x224044e0`) calls virtual Collect at `+0xf4`, invokes owner
  validate and ignores its return. It reloads the descriptor and prioritizes
  tower altitude, aircraft altitude, heading, tower latitude/longitude,
  aircraft latitude/longitude, then airspeed, returning the first control id.
- Error (`0x224045a0`) ignores its supplied argument and uses a **different**
  priority: aircraft altitude, tower altitude, heading, tower latitude/longitude,
  airspeed, then aircraft latitude/longitude. It passes the complete status
  record to DoErrorDialog. A true result selects that field; false selects
  zero. It always reloads the vtable and calls inherited Validate (`+0xe0`),
  returning its result even when no error exists. The real SIMSUI method
  returns the chosen id and posts message `0x5f5` to arrange focus; native
  checks pump that asynchronous message before asserting focus.
- OK (`0x22404a40`) retains one vtable across virtual Validate, Error and
  Close. It always calls Error, including when Validate returns zero. Only
  Error's return decides whether to call owner Apply, write answer 1 and close.
  Shared Cancel (`0x223fe4e0`) writes answer 0 and closes without applying.
- Select Flight (`0x22404770`) calls owner `+0x10`, reloads the vtable and
  calls virtual Populate at `+0xf0`. Set Tower from Aircraft (`0x22404790`)
  calls owner `+0xc`, then suppresses notifications by setting object `+0x26e8`
  to zero during virtual Populate and setting it to one afterwards. Facilities
  (`0x22404a80`) directly invokes owner `+0x14` without collecting/refreshing.
- The five EN_CHANGE handlers (`0x22404a90..0x22404b10`) write flags `+0x78`,
  `+0x90`, `+0xa8`, `+0x34`, `+0x4c` only when object `+0x26e8` equals
  **exactly 1**. Other nonzero values do not count as enabled.

Initializer `0x22403e10` captures the current vtable after base init and retains
it for virtual Populate, including across skin/control callbacks. Skin uses
`0x14f`, current resource module and flags 15. It sets up artwork, five buttons,
eight edits/labels, groups and two spin controls. All edits obtain base font 2,
with white text, black background and red errors; aircraft/tower altitude use
flags 3, others zero. RemapCtrlRect feeds MoveWindow for the spin controls;
ranges are `[1,360]` for heading (`0x11b7`) and `[0,500]` for airspeed (`0x11b8`).
It sends eight EM_LIMITTEXT values of capacity minus one, suppresses changes
while calling the retained virtual Populate, enables changes, snapshots all
strings and focuses the first edit. **Successful initialization returns zero.**
Skin failure also returns zero. Null/`0xcdcdcdcd` descriptors are checked after
control setup and cause the recovered bad-descriptor message before returning
zero; the core fixture exercises both diagnostic branches.

`gen_world_position.py` extracts 29 embedded controls/DDX bindings, terminated
help map, ten messages, 62-slot vtable and MSVC cleanup graph. Constructor is
`0x224037a0`, destructor `0x224039e0`, scalar deleting destructor `0x224039c0`,
DDX `0x22403be0`, vtable `0x22410f30`, message map `0x22410e20` and base-map
getter `0x223f5d40`. Object size is `0x26ec`; descriptor is `+0x10dc` and
notification gate `+0x26e8`. The two additional virtuals are Populate and Collect.
DDX calls imported base exchange before binding all 29 controls.

Two spins at `+0x1d44/+0x1ea4` have distinct normal/exceptional destruction.
Construction calls the MFC spin base constructor and installs the imported
SIMSUI spin vtable. Retail's intermediate local vtable store has no intervening
call before being overwritten, so the final store is sufficient at accepted
service boundaries. Normal destruction installs that imported vtable and calls
MFC's base destructor; exceptional cleanup directly calls SIMSUI's spin
destructor. The generated normal and unwind tables preserve that distinction.
Other controls unwind in reverse order; failed children are not retried.
Class vtable installs only after complete construction; normal destruction does
not reset it. Scalar delete releases only for flag bit 0 after success.
PostNcDestroy dispatches virtual deleting destruction with flag 1.

Factory ignores parent, allocates `0x26ec`, constructs with null parent, stores
descriptor/module and transfers ownership before Create. It ignores Create's
return, shows the window and returns the post-Show HWND, without a failure
message. Null allocation faults on a write at `0x10dc`; partial construction
uses dialog delete, distinct from UI delete. **This factory has no typed
`CException*` catch**: matching and unrelated C++ exceptions all propagate after
cleanup. It never dispatches the fatal SIMSUI service tested by earlier classes.

```sh
make -f tools/world.mk world-position-metadata
make -f tools/world.mk world-position-core N=20000
make -f tools/world.mk world-position-core N=2000 MODE=unwind
make -f tools/world.mk world-position-breaks
make -f tools/world.mk world-position-life N=20000
make -f tools/world.mk world-position-life N=2000 MODE=unwind
make -f tools/world.mk world-position-life N=2000 MODE=typed
make -f tools/world.mk world-position-life-breaks
make -f tools/world.mk world-position-ui
make -f tools/world.mk world-position-native-breaks
```

With seed `0x4a971b63` and the reference SHA-256 above, the 16-operation core/
initializer suite passed **320,000 normal comparisons** and independent fixture
checks. Unwind mode passed **177,140 comparisons**, including **145,140 real
unwinds**, with 32,000 normal fixture checks. Ordered calls, defined returns,
the complete guarded object, two descriptors, all buffers, flags, callbacks
and resource-module state match. Cases cover capacities 1/32/101, empty and
100-character snapshots, competing error statuses, both error-service results,
notification gates 0/1/2/3, descriptor swaps, vtable changes during initialization
and other external calls, module changes and invalid descriptor diagnostics.
Snapshot/notification operations have no external calls; their full byte effects
are still compared. Independent witnesses check counts, validation priority,
error return, cancellation and change-free operations.

The nine-operation lifetime suite passed **180,000 normal comparisons**,
including 1,819 expected allocation write faults and 178,181 fixture checks.
Both exceptional modes passed **445,130 comparisons**, including **427,130 real
unwinds/propagated exceptions**, 182 expected allocation faults and 17,818 normal
fixture checks each. Typed mode exercises direct/derived/virtual-base/null
`CException*` metadata, unrelated and incompatible const-pointer types, with
**zero fatal dispatches**. Full guarded objects, control markers, descriptor/
exchange/help data, module state, release counts, ordered calls and returns
match; spin vtables, base DDX, cleanup order and ownership are independently
observable at service boundaries.

`worldpositionuidiff.c` runs real MFC/SIMSUI edits, groups, buttons, spins and
Alerts dialogs over an independent FACIL-shaped owner. Sixteen cases vary
values, capacities and Select Flight enable state; sixteen scripts cover
baseline, Return/Escape, edits/apply, Select Flight, Set Tower from Aircraft,
Facilities callback, virtual validation, and each of eight error statuses.
Parent/error pixels, control metadata, edit limits, queried spin ranges,
callbacks, full descriptor/text state, defined snapshot strings, gate state,
error focus and automatic deletion match. Factory, local virtuals (including
Populate/Collect), and all ten message handlers must belong to the module under
test. Numeric parsing, actual flight selection/placement, and the separate
facilities window remain **owner boundaries**; no native acceptance of those
unimplemented dialogs is inferred from these callback fixtures.

The native suite passed **512 paired pixel/state/callback comparisons**,
**512 script witnesses**, including **256 paired error-dialog checks**, eight
module-init comparisons and two full unload cycles. Baselines are
`bin/world-diff/ui-images/position-baseline-*.bmp`. The first range mutation
revealed that pixels alone did not establish spin bounds; the final fixture
also reads both ranges, independently checks the retail constants and compares
them between modules. Build failures, faults and timeouts never count as
mutation detections.

All **27 core/init**, **21 lifetime** and **12 native mutations** produced
actual difference transcripts with exit 1: **60/60 detected**. They cover
collection order/capacity, snapshots, validation/error priorities and dispatch,
notification gating, owner callbacks, retained vtables, accept/cancel answers,
initializer diagnostics/limits/colors/flags/geometry/ranges, spin constructor/
normal/unwind vtables and destructors, base DDX, factory ownership and automatic
deletion. Core/unwind mutants are independently compiled; native mutants retain
input hashes and full DLL/harness snapshots. Artifacts are under
`bin/world-diff/position-breaks/`, `position-life-breaks/` and
`position-native-breaks/`. Compiler failures, faults and timeouts are excluded.

Acceptance logs are `bin/world-diff/position-core-normal.log`,
`position-core-unwind.log`, `position-core-breaks.log`,
`position-life-normal.log`, `position-life-unwind.log`, `position-life-typed.log`,
`position-life-breaks.log`, `position-ui-acceptance.log` and
`position-native-breaks.log`.

Generated position/runtime metadata and build-only resource/anchor outputs
reproduce byte-for-byte in an isolated directory. The retail registry inventory
is unchanged. All existing World native mutation builds include position source.
Logs/artifacts use `bin/world-diff/position-` and the existing World prefix/lock.

The ten-registration DLL passed **288 area**, **210 priority**, **320 booster**,
**680 drive-search**, **504 scenery-area-list**, **672 booster/cache-list** and
**672 world-files** paired comparisons: **3,346 total**, with their witnesses,
module-init checks and two unload cycles each. Existing area/priority/booster/
search color, area/cache tab and world-files font mutations also produced
actual differences with the expanded native build inputs. Regression logs use
`bin/world-diff/position-<family>-regression.log` and
`position-<family>-mutation-regression.log`, for `area`, `priority`, `booster`,
`search`, `list`, `cache` or `files`.

A 75-second startup passed in private copy
`bin/world-diff/smoke-game-gg_6y46j`: **27/27 modules, 95 gauges**, matching
replacement bytes and no runtime errors. The log is
`bin/world-diff/position-smoke-acceptance.log`; the active game was unchanged.
Final DLL SHA-256:
`0098da6ef14364bd34d3064f84c33eda5ff1b00724b9b7a3c53183e4d080c761`.
The final DLL, `bin/world-diff/position-native-tested.dll` and the startup-tested
copy are byte-identical.

Exact position is complete through this native UI milestone over independent
owner callbacks. WORLD remains a partial replacement with ten accepted
registrations; actual flight selection, simulator placement and the facilities
window remain separate owner flows.

## Facilities directory: class and factory milestone

The facilities slice starts **`0x1307`**, factory **`0x223f1d80`**, template
**`0x6bf`**, owned by FACIL `.data+0x80`. Its first milestone established independent class/factory acceptance without
registering it in the native DLL. The subsequent browser/native milestone below
adds that registration and raises the accepted count to eleven.

`world_facilities_objects.c` implements the constructor `0x22406f00`,
destructor `0x22407010`, scalar deleting destructor `0x22406ff0`, factory,
DDX `0x224070f0`, help `0x224095e0`, context help `0x22409600` and shared
PostNcDestroy `0x223f3d30`. The independent harness runs these eight entry
points plus construction followed by deleting destruction. It calls retail
code directly, patching imported services only, and leaves retail's local
vtable, DDX/help maps and MSVC exception metadata intact. FACIL's implementation
and verified harness are unchanged; no real placement or apply callbacks run.

Recovered class details:

- The object is `0x1db8` bytes with owner descriptor at `+0x10dc`, class vtable
  `0x224116b8` and nine embedded controls. Their offsets in construction order
  are `0x1628`, `0x1724`, `0x1764`, `0x1860`, `0x195c`, `0x1a50`, `0x1b44`,
  `0x1c14`, `0x1cfc`. All nine have DDX bindings, preceded by base DDX.
- Unlike exact position, the spin at `+0x1724` retains a **local** vtable,
  `0x224117a8`. Normal destruction calls imported destructor `+0x25564`
  directly without resetting that vtable. EH cleanup reaches the same import
  through local jump thunk `0x22409aa2`.
- The constructor installs its class vtable and clears `+0x1428` only after
  every embedded constructor succeeds. Destruction retains the incoming class
  vtable and destroys controls in reverse order, then the base. Exceptions
  continue cleanup without retrying the failed destructor.
- The factory ignores its parent, constructs with null parent, writes the
  descriptor and current module, then calls Create and Show(1). Create failure
  still proceeds to Show without a message. The handle is read after Show.
  A null allocation faults on the descriptor write at address `0x10dc`.
  Construction failures release through the dialog delete import; ownership
  transfers before Create. There is no typed `CException*` catch here.
- Help forwards WinHelp's result with command 12. Context help obtains the
  **current cursor position**, queries the artwork window at `+0x1c34` and
  calls PtInRect. Any nonzero result suppresses help and returns zero;
  otherwise command 10 is issued for the supplied window and the return is 1.
  The geometry-query return values and supplied message coordinates are ignored.
- PostNcDestroy calls virtual deleting destruction with flag 1 for nonnull
  objects, and does nothing for null objects.

`gen_world_facilities.py` validates both EH state chains, reverse control
lifetimes, all DDX bindings, class/deleting-destructor identity, local spin
vtable and the terminated help/message maps. Its outputs are ABI metadata,
without retail code or resource payloads. Both generated includes reproduce
byte-for-byte in an isolated directory. The inventory also regenerates
unchanged; all 12 dialogmap tests pass.

```sh
make -f tools/world.mk world-facilities-metadata
make -f tools/world.mk world-facilities-life N=20000
make -f tools/world.mk world-facilities-life N=2000 MODE=unwind
make -f tools/world.mk world-facilities-life N=2000 MODE=typed
make -f tools/world.mk world-facilities-life-breaks
```

Reference SHA-256 is the same `35d67023...3118f5` recorded above. Seed
`0x4a971b63`: the normal run passed **180,000 comparisons**, with **1,819
expected allocation write faults** and **178,181 successful-path witnesses**.
Each exceptional mode passed **173,834 comparisons**, including **155,834 real
unwinds**, **182 expected allocation faults** and **17,818 normal witnesses**.
The total is **527,668 comparisons, zero differences**. Typed mode covers
direct/derived/virtual-base/null `CException*` metadata, unrelated and const
pointer types; every thrown exception propagates, with zero fatal dispatches.

Comparisons include ordered calls and parameters, returns, complete guarded
object and descriptor/exchange/help buffers, mutable module state, control
markers and both deletion counters. Explicit witnesses check successful
constructor state, control markers, call counts, factory handle, help and
context returns, delete flags and PostNcDestroy. Every external service boundary
is separately interrupted in the exceptional modes. Geometry fixtures exercise
both suppression outcomes with a noncanonical nonzero PtInRect result and
zero-returning cursor/rectangle queries that still fill their outputs.

All **24/24 deliberate mutations** produced exit 1 and an actual `DIFFER`
transcript. They cover constructor state/vtable timing and control omission,
reverse destruction and base cleanup, delete flags and operators, exception
ownership transfer, allocation size, Create template/parent, post-Show handle,
base/local DDX, help command/return, context artwork window, coordinate order,
help suppression and PostNcDestroy deletion. Build errors, faults and timeouts
do not count as detections.

Acceptance logs are `bin/world-diff/facilities-life-normal.log`,
`facilities-life-unwind.log`, `facilities-life-typed.log` and
`facilities-life-breaks.log`. Mutation sources, independent executables and
actual difference logs live in ignored `bin/world-diff/facilities-life-breaks/`.
All runs use the existing World-specific Wine prefix and lock. The existing
exact-position typed lifetime regression also passed **7,124 comparisons**
(`N=32`), logged in `facilities-position-life-regression.log`.

## Facilities directory: browser core and native acceptance

Facilities directory **`0x1307`** is accepted through native UI over independent
FACIL-shaped owner callbacks. `WORLD_area.dll` now registers eleven dialogs;
eight World registrations remain. This milestone adds the map/browser,
rich-text results and their lifetimes to the earlier class/factory milestone.

`world_facilities.c` implements row release (`0x22407210`), airport/navaid and
runway output (`0x224073b0`), map/output transitions (`0x224086e0`), bitmap
hit testing (`0x22408a40`), map highlighting (`0x22408aa0`), initialization
(`0x22408c80`), command dispatch (`0x22409230`), mouse handlers, accept,
close and home/back/refresh. Local CWnd-derived spin and rich-edit vtables
are rebuilt alongside the dialog vtable and all ten message-map entries.
Only imported MFC42/SIMSUI and Win32/CRT services remain shared. Native fixtures
check module ownership of every local virtual/message handler and both local
control deleting destructors; they cannot call retail World as the replacement.

The native adapter resolves imported services with their actual calling
conventions. The rich-edit control is separately allocated as `0x40` bytes,
constructed through MFC, assigned its local vtable and created with style
`0x403108c4`, id `0xede`, in the remapped placeholder rectangle. Failure during
construction releases through MFC operator delete; ownership transfers before
Create. Close deletes the current map mask, deletes the rich-edit control
through its virtual scalar destructor, writes result 2 / answer 0 and invokes
the imported dialog Close. Automatic dialog deletion follows on destruction.
The facilities shared font is constructed once per module; the adapter owns
its destructor on unload instead of registering unloadable code with the
process-wide CRT exit table. Its lifecycle is independent of the booster font; both callbacks run in
reverse registration order when the module unloads.

Observed behavior preserved and tested:

- The first screen has nine region entries; USA opens the 50-state screen.
  Other selections build results using the previous screen's filter context
  before changing mode to 40. Map, back and refresh use the stored history.
  The apparent history limit at index 100 aliases object `+0x1420`, the history
  index itself: storing that entry's mode changes the index used for the next
  resource store. The C port retains this reload and alias. Transitions load
  replacement mask bitmaps without deleting the earlier handles; Close deletes
  the current mask, preserving retail's earlier-handle leak.
- The actual radio state, stored in window user data, selects airports or
  navaids. Output clears the rich edit, applies paragraph/character formats,
  selects a wait cursor, fills borrowed stack-based region/country/state arrays,
  calls the independent owner at descriptor `+0xc`, formats its returned rows,
  and restores the arrow cursor. Expired filter-array addresses remain in the
  descriptor after return. They are normalized only after callback contents
  have been compared. Zero-count output retains the owner's returned storage;
  nonzero and negative counts follow retail's distinct release paths.
- Airport output uses city, country or state, and long name, followed by
  frequency, altitude, latitude, longitude, ID and runway-end details. Navaid
  output varies its place heading by mode and preserves frequency-bit priority,
  including the unusual test `(flags & 8) && (flags & 0xf982)`. Runway output
  preserves the `H` prefix, L/R/C suffixes, special runway names and surface
  resources. Retail writes `_itoa` text through a newly constructed CString's
  data pointer; the port retains that operation and its ordering.
- Release retains each list's signed count but reloads the descriptor and row
  base before every pointer-to-pointer free. It releases six fields per airport
  row, five per navaid row, both row arrays, then zeros both counts. Descriptor
  swaps, zero/negative/extreme counts and the complete guarded fixture are
  compared at the external service boundaries.
- Native hover/click tests use actual palette-index mask pixels and screen
  coordinates. Hover, bitmap highlighting, keyboard cycling, Return, mouse
  activation and the USA exception use the recovered command/mouse paths.
  Help uses current cursor geometry to suppress context help over the artwork.
- Initialization preserves the early base-init/skin return branches, CString
  and bitmap temporaries, virtual radio setup, shared-font flag order, device
  DPI conversions, character formats, paragraph tabs, initial transition and
  focus. Failed temporary construction and rich-edit allocation cleanup are
  independently compared with real Windows/MSVC unwinding.

```sh
make -f tools/world.mk world-facilities-core N=20000
make -f tools/world.mk world-facilities-core N=256 MODE=unwind
make -f tools/world.mk world-facilities-breaks
make -f tools/world.mk world-facilities-life N=20000
make -f tools/world.mk world-facilities-life N=2000 MODE=unwind
make -f tools/world.mk world-facilities-life N=2000 MODE=typed
make -f tools/world.mk world-facilities-life-breaks
make -f tools/world.mk world-facilities-ui
make -f tools/world.mk world-facilities-native-breaks
make -f tools/world.mk world-smoke
```

The core fixture runs eighteen operations per scenario. It compares ordered
calls, parameters and returns, all object bytes, both descriptors, row/runway
buffers, CString storage and live-state counters, frees, bitmap bytes and guard
regions. Imported service fixtures supply independent text and geometry;
retail retains its internal calls and MSVC EH/vector-construction helpers.
Both `__CxxFrameHandler` and `_except_handler3` are real MSVCRT handlers, and
interruption calls real `RtlUnwind`. Callback contents are checked before
normalizing expired stack addresses; generated node tables are compared by
contents before normalizing their module-specific addresses. Initialization
includes null output allocations, failed base/skin calls, 96/120 DPI and
exceptions at each external call boundary. Native painting and mouse paths
supply the real GDI/window acceptance beyond these synthetic services.

The native fixture runs eighteen scripts per case across two unload cycles,
plus shared-font construction/unload checks in both dialog-opening orders.
It compares pixels, full rich-edit text, control metadata/geometry, defined
character/paragraph formats, callbacks and filter contents, owner row releases,
mode/history/selection and close results. The fixture physically clicks the
radio button and asserts navaid flags; sending WM_COMMAND alone did not
exercise that state change and was rejected after a missed mutation exposed
it. Map scripts use actual mask pixels and reapply hover highlighting after the
full-window redraw, so clearing/repainting focus cannot hide a highlight
regression. WM_SETCURSOR is dispatched explicitly with its three stack
arguments; its native result is compared. Independent hooks check one dialog
and one output deletion with flag 1, plus exactly one construction and destruction of each shared font per module,
in both facilities/booster construction orders. The shared-font cleanup mutation must fail during
unload, not through a rendering difference.

All mutation suites require exit 1 with actual difference transcripts; build
failures, faults, fixture failures and timeouts never count as detections.
Native mutants retain hashes and complete source/DLL/harness snapshots.

With seed `0x4a971b63`, **20,000 normal scenarios passed 360,000 core
comparisons and 360,000 witnesses**. The **256-scenario unwind run passed
434,734 comparisons**, including **430,126 real unwinds** and **4,608 normal
witnesses**. Together with the class/factory milestone's **527,668 lifetime
comparisons**, that is **1,322,402 guarded core/lifetime comparisons, zero
differences**. Core logs are `facilities-core-normal.log` and
`facilities-core-unwind.log`; the final fixture also retains those runs as
`facilities-core-final-normal.log` / `facilities-core-final-unwind.log`.

The native suite passed **576 paired pixel/state/callback comparisons**, **576
script witnesses**, eight module-initialization comparisons and two full unload
cycles. It also verified both shared fonts' construction/destruction counts and
reverse registration order in **four module unload checks covering both construction orders**.
Baselines are `bin/world-diff/ui-images/facilities-baseline-*.bmp`.

All **37 core/init**, **24 lifetime** and **16 native mutations** produced actual
differences: **77/77 detected**. They include list release and descriptor reloads,
filter/heading/placeholder construction, navaid frequency priority, runway
prefix/suffix/surfaces, the history-index alias, back/keyboard wrap, bitmap
stride/orientation/highlight dimensions and raster operation, mouse selection,
close results/deletion flags, initialization formats/styles/cleanup, dispatch
ownership and shared-font cleanup on unload. The initially missed radio/type
mutation and highlight dimension error led to stronger fixtures, followed by
passing baseline and mutation runs. Artifacts are under `facilities-breaks/`,
`facilities-life-breaks/` and `facilities-native-breaks/`. Logs are
`facilities-core-breaks.log`, `facilities-life-breaks.log`,
`facilities-native-breaks.log` and `facilities-ui-acceptance.log`; the final
keyboard/highlight run is also retained in `facilities-ui-keyboard.log`.

The eleven-registration DLL also passed all earlier native suites: **288
area**, **210 priority**, **320 booster**, **680 drive-search**, **504 area-list**,
**672 cache-list**, **672 world-files** and **512 exact-position** pairs:
**3,858 regression pairs**, zero differences, with their witnesses,
module-init checks and two unload cycles each. Including facilities, the final
build has **4,434 paired native UI comparisons**. One deliberate native mutation
from each earlier family also produced an actual difference with the expanded
build inputs. Their scripts now include the facilities sources. Regression logs
are `facilities-<family>-regression.log` and
`facilities-<family>-mutation-regression.log`, for `area`, `priority`, `booster`,
`search`, `list`, `cache`, `files` or `position`.

Facilities/runtime metadata and build-only resources/anchor outputs reproduce
byte-for-byte in an isolated directory. The retail registry is unchanged and
all twelve dialogmap tests pass. `git diff --check` passes.

A **75-second startup** passed in private copy
`bin/world-diff/smoke-game-doc_qdnw`: **27/27 modules, 95 gauges**, matching
replacement bytes and no runtime errors. The active game's World.DLL hash is
unchanged; the smoke runner only replaces the DLL in its private copy. Startup
log: `bin/world-diff/facilities-smoke-acceptance.log`. Final DLL SHA-256:
`82d24d4d5b5572e0a67ce07f7bc4f9cc7fdabab322953071cf53102845cdccb5`.
`WORLD_area.dll`, `facilities-native-tested.dll` and the startup-tested copy are
byte-identical. `facilities-final.log` records the successful acceptance and
regression stages.

Logs/artifacts use `bin/world-diff/facilities-`, the dedicated
World Wine prefix and its lock. No FE/FACIL/MAIN/HOST implementation or verified
owner harness, shared Makefile or active-game DLL is changed.

## Go-to-airport: class and factory milestone

Go-to-airport **`0x1309`**, factory **`0x223f1cf0`**, template **`0x70d`**,
is now implemented through independent class/factory lifetime acceptance.
Its FACIL owner is `.data+0x3e8`; the established owner fixture is
`world_airport` in `tools/difftest/facildiff.c`. This first milestone did not register
the airport dialog in `WORLD_area.dll`. The core/native milestone below adds
that registration, raising the accepted count to twelve.

`src/world/world_airport_objects.c` implements constructor `0x22404b50`,
destructor `0x22404d70`, scalar deleting destructor `0x22404d50`, factory,
DDX `0x22404f60`, help `0x224055a0`, context help `0x224055c0` and shared
PostNcDestroy `0x223f3d30`. The new harness calls these eight retail entry
points plus construction followed by deleting destruction. It patches imported
services only; retail retains its local code, maps, vtable and MSVC exception
metadata. No FACIL callback or real aircraft placement runs.

Recovered and independently compared behavior:

- The object is `0x23e0` bytes, with descriptor at `+0x10dc` and class vtable
  `0x22411468`. Its 29 embedded controls are 11 text controls, one text-art
  control, three radio buttons, five combo boxes, two edit controls, three list
  boxes and four three-piece buttons. All 29 have DDX bindings, preceded by
  base DDX. Exact offsets, construction order and ids are in the generated
  `world_airport_tables.inc`; DDX order differs from construction order.
- The constructor installs the class vtable only after all controls succeed.
  It leaves the tail state untouched. The destructor retains its incoming
  vtable, destroys controls in reverse order, then destroys the base. A failed
  destructor is not retried during exception cleanup; earlier controls and
  the base still receive destruction.
- The factory ignores its parent, allocates `0x23e0` bytes, constructs with a
  null parent and stores the descriptor and current module. A null allocation
  faults writing address `0x10dc`. Create failure still proceeds to Show(1),
  without a message; the handle is read after Show. Exceptions during
  construction release the allocation through dialog operator delete, while
  successful ownership transfer occurs before Create. This factory has no
  typed `CException*` catch: all tested exception types propagate.
- Scalar deletion calls UI operator delete only for flag bit 0 after successful
  destruction. PostNcDestroy dispatches virtual scalar deletion with flag 1
  for nonnull objects and does nothing for null objects.
- Help returns WinHelp's result with command 12 and the full numeric map.
  Context help uses command 10 and returns 1, ignoring its message coordinates.
  Unlike the facilities directory, it has no artwork geometry suppression.

`tools/gen_world_airport.py` validates the reference image base/module id,
both 29-state EH cleanup graphs, reverse control lifetimes, complete DDX
coverage, class/deleting-destructor identity and the terminated help/message
maps. It extracts a 60-slot vtable and 26 local message entries plus terminator.
Generated outputs contain ABI metadata, with no copied code or resources;
both reproduce byte-for-byte in an isolated directory. The registry regenerates
unchanged and all twelve dialogmap tests pass.

```sh
make -f tools/world.mk world-airport-metadata
make -f tools/world.mk world-airport-life N=20000
make -f tools/world.mk world-airport-life N=2000 MODE=unwind
make -f tools/world.mk world-airport-life N=2000 MODE=typed
make -f tools/world.mk world-airport-life-breaks
```

Against reference SHA-256 `35d67023...3118f5`, seed `0x4a971b63`, the normal
run passed **180,000 comparisons**, **178,181 successful-path witnesses** and
**1,819 expected allocation write faults**. Each exceptional mode passed
**445,206 comparisons**, including **427,206 real unwinds**, **17,818 normal
witnesses** and **182 expected allocation faults**. Total: **1,070,412 guarded
comparisons, zero differences**, including **854,412 real unwinds**. Typed mode
uses direct/derived/virtual-base/null `CException*` metadata, unrelated and const
pointer types; no fatal dispatch occurred.

The harness compares ordered service calls, parameters, returns, every object
and descriptor byte, exchange/help buffers, mutable module state, control
markers, deletion counts and guards. Successful-path witnesses independently
check call counts, every control's marker, constructor vtable, post-Show handle,
help/context returns, deletion flags and PostNcDestroy behavior. Every external
service boundary is separately interrupted through real `RtlUnwind` and the
retail MSVCRT exception handler. The fixture supplies synthetic controls;
these results do not establish native window behavior or owner callback order.

All **24/24 lifetime mutations** produce exit 1 with actual `DIFFER`
transcripts. They cover constructor template/parent, vtable timing and control
omission, destructor vtable reset, reverse destruction and retry behavior,
base cleanup, delete flags/operators, allocation size, factory template/parent,
module, Show flag and post-Show handle, ownership transfer, base/local DDX,
help command/return, context command/return and PostNcDestroy deletion.
Build errors, faults and timeouts do not count as detections.

Logs are `bin/world-diff/airport-life-normal.log`, `airport-life-unwind.log`,
`airport-life-typed.log` and `airport-life-breaks.log`. Mutated sources,
independent executables and actual difference logs live in ignored
`bin/world-diff/airport-life-breaks/`. Runs use World's existing Wine prefix
and lock. The facilities typed lifetime regression (`N=32`) also passes
**2,778 comparisons**, including **2,490 real unwinds**; its log is `airport-facilities-life-regression.log`.

At this first milestone the native DLL was unchanged, with SHA-256
`82d24d4d5b5572e0a67ce07f7bc4f9cc7fdabab322953071cf53102845cdccb5`.
No native UI or startup acceptance is claimed for the airport dialog.

The subsequent milestone below completes those core and native boundaries.

## Go-to-airport: core and native UI acceptance

Go-to-airport **`0x1309`** now has core, initialization and native acceptance
over independent FACIL-shaped callbacks. `WORLD_area.dll` registers twelve
World dialogs; seven registrations remain pending. `world_airport.c` implements
the complete place/name/airport/runway flow, mode and edit/dropdown handlers,
refresh, owner facilities dispatch, accept/cancel, HOST resource lifetimes and
initialization. Runtime bindings rebuild the 60-slot vtable and all 26 local
message entries; the existing shared facilities callback and inert name-list
double-click handler are also module-owned C implementations. The replacement
never calls retail World code.

Observed behavior preserved and independently compared:

- Descriptor modes `0x1000`, `0x1001` and `0x1002` select place, name and
  FE-area searches. Transitions clear lists using the **old** mode before
  storing the new mode and changing control visibility/enabling. Area mode
  invokes the owner area callback before population. Its visible controls use
  Show(5), while the other mode controls use Show(1). The radio-handler cache
  at `+0x23dc` is separate from descriptor mode; initialization leaves it at 3.
- Population calls owner `+0x20` first. It inserts a formatted placeholder
  before each place list, appends borrowed 12-byte records, restores saved
  selections and calls the runway owner except in area mode. Counts are
  signed; row bases and descriptors reload at service boundaries. Refresh
  calls owner `+0x10`, restores the seven saved selections, checks the mode's
  radio and runs its transition. Place/area selection handlers have distinct
  owner callbacks, then clear the selected-airport text and repopulate.
- Place dropdown opening replaces the first row with the generic all-items
  string, deletes the old placeholder and restores selection. Close-up
  reformats the placeholder and reverses the replacement. The first place
  combo only formats on close-up when selection is zero; the other three
  format regardless of selection. Both the intermediate and final rows are
  checked by the native fixture.
- Airport and name edits use case-insensitive recursive insertion search,
  with empty, before-first, exact, between-row and after-last branches. Airport
  edit skips under the `+0x23d8` notification gate and permits a null row base.
  Name edit returns immediately for a zero count, performs an extra exact
  comparison and either queries/rebuilds airports or clears airport/runway
  controls. It writes its edit selection to **`+0x23a0`**, rather than the
  name-selection snapshot `+0x239c`; that retail quirk is preserved. Selecting
  an airport sets the gate while changing its edit text and updating runways.
- Accept retains the validation/error vtable for final Close, samples runway
  selection only when that combo is enabled, calls owner `+8`, stores answer 1
  and releases temporary HOST storage. Cancel stores answer 0, releases storage
  and reloads the close vtable. Airport double-click invokes virtual accept;
  name double-click is inert. Native fixtures exercise Return and Escape too.
- Initialization retains retail's base-init/skin behavior, artwork, fonts,
  radio setup, all list/edit/button bindings, both edit limits of 100 and the
  unusual shared edit label at `+0x1a88`. It invokes owner places then names,
  loads nine HOST strings, allocates a work buffer at twice their maximum
  returned length, restores the initial mode and focuses the airport edit.
  Null and `0xcdcdcdcd` descriptors follow the diagnostic-message branch.
  There is no local cleanup for failures during these HOST allocations;
  accept/cancel free all nine strings and the work buffer in ascending order.

```sh
make -f tools/world.mk world-airport-core N=20000
make -f tools/world.mk world-airport-core N=256 MODE=unwind
make -f tools/world.mk world-airport-breaks
make -f tools/world.mk world-airport-life N=20000
make -f tools/world.mk world-airport-life N=2000 MODE=unwind
make -f tools/world.mk world-airport-life N=2000 MODE=typed
make -f tools/world.mk world-airport-life-breaks
make -f tools/world.mk world-airport-ui
make -f tools/world.mk world-airport-native-breaks
make -f tools/world.mk world-smoke
```

The core fixture exercises 39 operations per scenario. It compares ordered
calls, arguments and returns, complete guarded object state, both descriptors,
all row and text buffers, saved selections, control visibility/enabling,
resource allocations/releases and callback counters. Independent witnesses
check construction of HOST storage, release counts, mode writes, notification
suppression and cancel results. Descriptor swaps and validation-time vtable
replacement exercise reload/caching behavior. Fixtures keep payload fields
valid under deliberate row-stride mutations, so a fault cannot masquerade as
a detected difference. Each list has independent selections, making swapped
selection mappings observable. Interruption injects a Windows exception at
every external boundary and uses real `RtlUnwind`; retail internal calls remain
intact. No FACIL implementation or simulator placement callback runs.

The native fixture uses 24 scripts per case across two module unload cycles.
It compares GDI pixels, control text/geometry/visibility/enabling and edit
limits, all eight lists' counts, full contents and selections, radio state,
owner callback order/parameters, descriptor/text/saved state, allocation/free
counts and automatic deletion. It physically clicks the radio buttons and
opens an actual nested facilities directory from the independent owner
callback. Factory, local virtual and all message-handler addresses must belong
to the tested module. A scalar-delete probe verifies flag 1 and exactly one
deletion after window destruction. Module-init allocation/exception and repeated
deinit checks run on both sides before each UI cycle.

With seed `0x4a971b63`, the final normal core run passed **780,000 comparisons
and 780,000 witnesses** over 20,000 scenarios. The final 256-scenario unwind run
passed **218,420 comparisons**, including **208,436 real unwinds** and **9,984
normal witnesses**. Including the class/factory milestone's **1,070,412 lifetime
comparisons**, this is **2,068,832 guarded core/lifetime comparisons, zero
differences**. The final core fixture includes direct one-row search boundaries;
its logs are `airport-core-final-normal.log` and `airport-core-final-unwind.log`.
The typed lifetime regression (`N=32`) also passed **7,126 comparisons**,
including **6,838 real unwinds** (`airport-life-regression.log`).

The final native suite passed **576 paired pixel/state/callback comparisons**,
**576 script witnesses**, eight module-init comparisons and two full unload
cycles. It opens a nested facilities directory in each case in each cycle.
Baselines are `bin/world-diff/ui-images/airport-baseline-*.bmp`; the final
baseline log is `airport-ui-final.log`.

All **38 core/init**, **24 lifetime** and **14 native mutations** produced
actual differences: **76/76 detected**. Every accepted detection requires
exit 1 and an actual difference transcript. Compile errors, faults, fixture
failures and timeouts do not count. One native dropdown-placeholder mutation
initially passed because close-up restored the row before capture; the fixture
now captures and witnesses the open row too. A core search mutation exposed
missing one-row search coverage; direct single-row cases were added. Strengthened
normal/unwind/native baselines and all affected mutation suites then passed.
Final mutation logs are `airport-core-final-breaks.log`,
`airport-life-breaks.log` and `airport-native-final-breaks.log`; artifacts are
under `airport-breaks/`, `airport-life-breaks/` and `airport-native-breaks/`.
Native mutation builds retain input hashes and complete source/DLL/harness
snapshots. Earlier World mutation builders now include the airport sources.

Airport/runtime metadata and build-only resource/anchor outputs reproduce
byte-for-byte in an isolated directory. The retail registry is unchanged;
all twelve dialogmap tests pass. Runs use World's dedicated Wine prefix/lock
and ignored `bin/world-diff/airport-` artifacts. No FE/FACIL/MAIN/HOST source,
verified owner harness, shared Makefile or active-game DLL is changed.

## Remaining boundary

Seven registrations remain pending native acceptance: basic weather `0x1302`,
time/season `0x1301`, weather areas `0x1303`, scenery complexity `0x1304`, dynamic
scenery `0x1305`, advanced weather `0x13040001` and copy weather `0x13050001`.
These need independent caller/descriptor recovery before choosing another
bounded family. Their inventory entries are not behavior specifications.

FACIL's three World dialogs now have native acceptance over independent owner
fixtures. Real filesystem lookup, flight-selection and simulator placement
remain owner responsibilities; no real placement callback runs in World tests.
WORLD remains a partial replacement. Shared build/install integration and full
host-driven interactions are separate. FE, FACIL, MAIN, HOST, their verified
harnesses/docs, the shared Makefile and the active game remain unchanged.
