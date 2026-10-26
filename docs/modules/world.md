# World.DLL — native add/edit scenery-area slice

**Partial replacement with native UI acceptance.** `bin/world-diff/WORLD_area.dll`
registers only add/edit scenery area (`0x13080003` / `0x13080004`). Their shared
class, initialization, controls, event/message maps, factories, exception
lifetimes and module binding are rebuilt in C, using retail MFC42/SIMSUI as
shared dependencies. The other 17 World registrations remain pending.

This is ready to review as a standalone two-dialog slice. It is **not a full
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
callback/file fixtures. Broader scenery lists, priority, drive search and
booster dialogs can follow in the same standalone harness. A two-registration
test DLL is a partial slice, not a complete World replacement.

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

The new code follows Hungarian notation and local `types.h` aliases. The
repository currently has no shared `types.h`; these aliases stay inside the
World slice, without changing established module conventions.

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
`gen_world_runtime.py` extracts bindings and the two registry records. Build-only
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

## Remaining boundary

Seventeen World registrations are still pending: scenery lists/library,
priority/search/boosters, weather/time, scenery settings and FACIL-backed
position/facility screens. Continue as bounded slices with their own native
acceptance before building or installing a complete World replacement. Shared
build/install integration and complete host-driven interaction remain separate.

FE, FACIL, MAIN, HOST, their verified harnesses and the shared Makefile remain
unchanged by this work.
