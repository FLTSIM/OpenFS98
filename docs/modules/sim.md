# Sim.DLL

The crash-analysis and aircraft-settings dialog cores are implemented in C.
The comparison harness covers all eight object lifetimes, five factories,
control exchange, help maps, six settings pages, and the outer settings dialog.
This is not yet a replacement DLL: the native MFC/SIMSUI adapter, module entry
points, native vtables/message maps, resources, and native dialog/startup checks
remain pending. The implementation does not require changes to MAIN, HOST, or
the shared Makefile.

Build and run independently:

```sh
make -f tools/sim.mk simdiff-run
make -f tools/sim.mk simdiff-run N=2000 SEED=0x9e37b184 MODE=unwind
make -f tools/sim.mk sim-breaks N=100
make -f tools/sim.mk sim-runtime-tables
```

The runner uses `bin/sim-diff/wineprefix` and a module-specific lock. It does not
install files into `game/`. The implementation and tools use snake_case and
standard `<stdint.h>` types; there is no module-local `types.h`.
Individual mutation checks can be selected by name:

```sh
bash tools/run_simdiff.sh --breaks 100 fuel-validation-edit-modify
```

## Reference and layout

Reference: `orig/English/Sim.DLL`, preferred base `0x223c0000`, numeric module id
`0x80000004`, Linkage RVA `0xc070`, ImportTable RVA `0xc058`. The numeric imports
are HOST and GLOBAL. Shared dialog/control classes come from SIMSUI and MFC42.

| Class | Template | Bytes | Constructor / destructor RVA | Page record offset |
| --- | --- | --- | --- | --- |
| Crash analysis | `0x44d` | `0x16b4` | `0x1420` / `0x1510` | `0x112c` |
| Damage | `0x44e` | `0x1758` | `0x1d60` / `0x1e60` | `0x1118` |
| Engines | `0x44f` | `0x1e84` | `0x24d0` / `0x2680` | `0x111c` |
| Fuel | `0x450` | `0x292c` | `0x3630` / `0x39e0` | `0x1120` |
| Systems | `0x451` | `0x207c` | `0x5430` / `0x5600` | `0x1118` |
| Realism | `0x453` | `0x2240` | `0x6170` / `0x62f0` | `0x111c` |
| Reliability | `0x454` | `0x1958` | `0x6bc0` / `0x6c90` | `0x1118` |
| Settings container | `0x452` | `0x226c` | `0x6fd0` / `0x70f0` | `0x10ec` |

Factories for ids `0x1206` through `0x1209` create the same settings container
with initial tabs 0, 2, 3, and 5. Id `0x14050001` creates crash analysis. The
factories ignore their parent argument, write the descriptor before calling
Create, ignore Create/Show results, and return the object's stored HWND.

`tools/gen_sim_tables.py` derives ordered control lifetimes, constructor stores,
vtable reset requirements, constructor/destructor exception cleanup imports,
DDX bindings, help maps, engine recipes, systems feature groups, and the fuel
validator's sparse edit IDs. Generated includes contain ABI metadata, not
retail instructions or resources.

`tools/gen_sim_runtime.py` prepares bindings for all eight class vtables, the
notification and private CWnd vtables, eight message maps, and five factories.
It extracts resources and an ordinal import anchor only into ignored build
output. The generated wrappers pass a standalone C syntax check; they still
need the native runtime adapter and native behavioral comparisons.

## Implemented behavior

`sim_objects.c` handles the eight constructors, destructors, deleting
destructors and five factories. Each base/member lifetime uses the original
import boundary. Cleanup-only x86 SEH registrations reproduce partial
construction and destructor unwinding. Damage's static member has different
normal and exceptional cleanup imports; these are preserved independently.

`sim_crash.c` and `sim_events.c` implement crash initialization, plot painting,
notification allocation, accept/cancel/close, descriptor validation, timer
notifications and text synchronization. Graph callbacks take three stdcall
arguments; the REALITY callee confirms that contract. Painting retains the
original alternating tick placement and integer arithmetic, including the
division exception for an invalid axis count. The synthetic harness compares
that specific exception as an outcome.

The six settings pages implement initialization, population, validation and
their control handlers:

- Damage retains the three crash modes and feature-dependent controls.
- Engines retain per-engine/all-engine checkbox behavior, unsigned engine
  counts, separate piston/turbine combo recipes and descriptor reloads.
- Systems retain eight checkboxes, seven combos, and fifteen feature groups.
  Paired controls share one feature test. Combo values other than exactly zero
  or one map to two, including `CB_ERR`.
- Realism retains slider/checkbox commits, both numeric edit buffers, MAIN's
  validator and conversion callbacks, error priority and retry IDs.
- Reliability retains its slider, checkbox state and feature gate.
- Fuel retains all fifteen numeric edits, five tank rows, capacity labels,
  unit-dependent headings, selector availability, manual control enablement,
  conversion callbacks and error handling.

Fuel's validation and selector-change paths intentionally differ: validation
stores the raw combo result, while selector changes map an available item back
to a tank index. Only availability equal to one counts, and missing selections
map to eight. Two edits update the cached row after GetWindowText; the others
update it before. Capacity zero still produces a wrapped edit limit. The last
edit in the first initialization group shares the preceding edit's label.

MAIN's settings validator at `0x207e5010` takes one stdcall argument. Realism's
converters at `0x207e5a80` / `0x207e5ac0` also take one; fuel's converters at
`0x207e5c40`, `0x207e5ca0` and `0x207e5df0` take the master descriptor and row,
and return with `ret 8`. Their return values are ignored by the fuel page.

`sim_settings.c` implements the outer initializer and descriptor wiring. It
constructs the pages in realism, reliability, damage, engines, fuel, systems
order, adds the labels with each descriptor's availability flag, initializes
six tab helpers, converts the cancel-button rectangle, wires page records and
owner controls, and activates the requested tab. It retains both descriptor
guards and the saved virtual table used for the extra artwork. Each page
constructor has its own allocation cleanup scope; completed pages are not
deleted by that scope when a later operation throws.

The container's otherwise-unused ValidateData virtual retains its diagnostic
message box and zero return; it is distinct from the descriptor guard used
inside initialization.

The outer accept/cancel/close handlers preserve validation/error dispatch
through a saved page vtable, freshly loaded close dispatch, answer/state
writes, and notification deletion from nested engine/fuel pages.

## Verification boundary

`tools/difftest/simdiff.c` maps the reference PE without resolving its imports,
patches typed dependency hooks, and compares reference routines with C over
identical guarded fixtures. It checks complete fixture bytes, defined return
values, and ordered calls. Dependencies can change descriptors, handles,
resource modules, vtables, selected rows, and numeric conversion state during a
call. Nested settings-page allocations use distinct fixture objects.

Numeric test inputs vary edit identity, validation success, error presence and
popup acceptance independently. This matters: correlating validation success
with a non-edit control hides fuel's `EM_SETMODIFY` path, and correlating a
first-field error with a dismissed popup hides realism's first-field retry.
Deliberate mutations exposed both gaps and the fixture now covers those paths.

The 146-operation suite passes 20,000 normal scenarios (2,920,000 comparisons,
seed `0x73a915e2`) and 200 exception-injection scenarios (468,614 comparisons,
same seed) after the snake_case cleanup. The preceding 145-operation suite also
passed 2,000 exception-injection scenarios (4,683,633 comparisons, seed
`0x9e37b184`). Logs are `style-normal.log`, `style-unwind.log`, and
`core-unwind.log` under `bin/sim-diff/`.

All 90 deliberate mutations have produced real differences after the cleanup.
The full run caught 89; the fuel-availability mutation needed a more specific
anchor after two index variables acquired the same snake_case name. Its
targeted rerun then differed at scenario 2. See `style-breaks.log` and
`style-breaks-retry.log` in the same directory.

Exception mode injects an exception at every observed dependency position and
uses the original MSVCRT C++ frame handler to execute the retail funclets.
Tests exercise notification allocation failure and the existing factory fault
on a null allocation. Native MFC rendering, message dispatch, and DLL lifecycle
behavior are outside this synthetic boundary.

`tools/sim_breaks.py` snapshots all sources, generated metadata and the harness
before compiling deliberately broken implementations. A mutation counts only
when it exits with an actual `DIFFER scenario=` report. Build errors, startup
timeouts, and faults do not count. Snapshots and logs stay under ignored
`bin/sim-diff/`.

## Remaining integration

Implement module init/deinit and DllMain, exports, runtime import adapters,
and binding of the generated virtual tables/message maps. Then
compare native dialogs and mutation cases under the private Sim Wine prefix,
and perform an isolated startup smoke before installing or advertising a
replacement DLL.
