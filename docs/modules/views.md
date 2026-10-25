# Views — View Options replacement DLL

`English/Views.DLL` is UI module `0x80000005`, with 13,090 bytes of `.text`,
one registered dialog (`0x1508`) and one recognised MFC message map with
19 entries. A standalone Win32 replacement is implemented in `src/views/`
and builds to `bin/views-diff/VIEWS_ours.dll`. It includes the complete event
surface, control initialization and binding, all embedded-object lifetimes,
factory and module lifecycle. The shared build and game install are left
untouched while other agents finish MAIN and the host.

It imports only HOST and GLOBAL through FS98's numeric ABI. PE imports are
USER32, SIMSUI, MFC42, MSVCRT and KERNEL32. Preview rendering and simulator
changes are supplied by descriptor callbacks rather than static imports of
WINDOW, MAIN, G3D or HG2D.

## Building and verification

All commands run from the repository root, independently of the shared Makefile:

```sh
make -f tools/views.mk viewsevents views
bash tools/run_viewsdiff.sh 20000
bash tools/run_viewsdiff.sh 20000 0x98a1e7d3
bash tools/run_viewsdiff.sh 2000 0x7e51a23b unwind
make -f tools/views.mk views-breaks N=2000
make -f tools/views.mk viewsuidiff-run views-ui-breaks
make -f tools/views.mk views-smoke
```

`run_viewsdiff.sh` serializes Wine jobs in the dedicated
`bin/views-diff/wineprefix`. Fixtures, mutated builds and logs stay in ignored
`bin/views-diff/`. The smoke boots a disposable game copy with only its
English Views DLL replaced, verifies the installed bytes against the launch
snapshot, and stops only this prefix. It retains the copy and trace.

`viewseventsdiff` maps the actual original DLL without its loader startup,
patches reached imports and compares the original routines against C.
There are **40 comparisons per scenario**: all 19 message handlers, control
initialization and 34 bindings, accept/cancel, validation and error display,
preview lifetime, five 3D view modes plus map, object construction/destruction,
deleting destructors, public factory and constructor/destructor sequences.
It compares every fixture byte (object, two replaceable descriptors, four
records, numeric buffers and guards), defined returns, dependency transcripts,
and the x87 control word. It varies 24/53/64-bit precision and all four
rounding modes. Callbacks and dependencies can change selection, replace the
descriptor and replace the object's vtable to expose stale-pointer mistakes.

Numeric and map string operations use the CD's real MFC42 CString methods,
with boundary logging rather than a reimplementation of CString. Synthetic
resource lists include extra entries after the fixed 12/15 choices, absent
separators, and empty labels. Unwind mode injects exceptions at successive
boundary calls through position 512; the original uses its own real MSVCRT
unwind handlers and cleanup funclets, compared with our cleanup-only SEH
scopes. The test covers late map-list temporaries as well as constructors,
destructors and allocation cleanup. Throws inside shared-library methods and
double exceptions are outside this boundary-injection fixture.

The native harness loads both DLLs normally with the CD's SIMSUI/MFC42,
a real MFC application/main window, and a small host model. It validates that
each dialog's own virtual methods belong to the expected module. It drives
creation, selection, every view mode, combos, checkbox fields, zoom, spin,
validation failure/retry, preview ticks and Return/Escape/OK/Cancel. It
compares descriptor/record/buffer bytes, child classes/text/geometry/states,
callback order, close signaling and **actual window pixels** across two
complete load/unload cycles. Native module-init probes also compare successful,
null and throwing allocation, throwing CDynLinkLibrary construction, and
repeated deinit. WinHelp's parameters are verified in the synthetic suite;
its external viewer is not launched.

The preview renderer is a **stateful caller fixture**, not G3D: it creates a
child on operation 1, changes its display from the selected record on ticks,
and destroys it on operation 2 before the dialog signals completion. The host
class `FS98DLG3D` must be registered with `CS_GLOBALCLASS` for dialog resources
owned by another DLL. Rendering the simulator's real preview and applying
edits remain MAIN/WINDOW responsibilities, not additional code in Views.
Native Windows, other MFC builds and a full host-driven modal-loop integration
are not established by the Wine comparison or startup smoke.

`views_events.c` implements the UI behavior, `views_objects.c` the derived
object and factory, `views_stack.S` the two entry-stack observations, and
`views_runtime.c` the native library adapter and module lifecycle. The DLL
exports exactly `Linkage` and `ImportTable`. `gen_views_tables.py` extracts
control-state recipes, bindings, subobject layout and floating constants.
`gen_views_runtime.py` extracts all 60 dialog vtable slots, the message map,
119 shared-library bindings and dialog registry. All 23 non-version resource
leaves are copied byte-for-byte into ignored build resources. No original
machine code or resource payload is committed. The transient private spin
vtable assignment is omitted: retail overwrites it immediately with SIMSUI's
vtable, with no intervening call or branch.

### Findings the comparisons established

- Spot and track use record modes 4 and 3, but their radio positions are 3
  and 4. Map sets record `+0x04` to 1 while retaining the previous 3D mode.
  Only an exact map flag of 1 chooses that radio; validation instead skips
  numeric checks for any nonzero map flag.
- Numeric record fields are exchange blocks, not floating values. Sync calls
  MFC `GetWindowTextA` with their buffer pointers and capacities, then the
  caller validates. Spot sync reloads the descriptor before distance and
  altitude. Validation reloads it after the caller callback as well.
- Error reporting returns the result of the inherited virtual at `+0xe0`,
  not the id passed to that virtual. Accept saves its original vtable across
  validation, error reporting and synchronization before calling Close.
- Retail's `-0.005`, `10.0` and `0.1` constants are binary64. Decimal literals
  evaluated directly in x87 extended precision can change the format-call
  argument, even when the eventual visible text agrees. Extracted hexadecimal
  long-double literals preserve the exact binary64 values before arithmetic.
- Both the map metric flag (invalid unit selector) and the zoom float (failed
  `sscanf`) read an uninitialized local at **entry ESP-16**. Entry shims capture
  those bytes before compiled-C frames overwrite them. The harness paints
  that exact slot for both implementations, including positive/negative,
  zero, NaN and infinity float patterns; valid input replaces the saved value.
  This establishes the individual entry contract, not a deterministic value
  for arbitrary prior stack histories in the simulator.
- WM_PRINT alone misses the skinned edit's background-color error. The native
  test now redraws the visible window and captures its real client pixels;
  the deliberately green edit changes pixels while control metadata agrees.

## Factory and caller boundary

Factory `0x223e1130` allocates a `0x2808`-byte object, calls constructor
`0x223e11d0`, stores the caller's descriptor at object `+0x2804`, creates
template `0x5e1` through MFC42 ordinal 2086, shows the window and returns
object `+0x20`. It returns with `ret 8`. The supplied parent argument is
replaced by a null parent for template creation. The registry flags are 0,
so the host runs the modal lifetime.

The reference MAIN call at `0x207f7e5d` supplies descriptor `0x20806e98` to
host `+0x10c`, with id `0x1508`. It first builds the descriptor through
`0x207f7f60`. On return it requires both a zero host-call result and
descriptor word 0 equal to 1 before calling its apply routine `0x207f8480`.
The caller then cleans up its temporary record array. These are observations
of retail input; no MAIN source, harness or documentation was changed.

The descriptor fields established by both sides include:

| Offset | Observed role |
| --- | --- |
| `+0x00` | Dialog answer; caller applies only when it is 1 |
| `+0x04` | One-argument callback, initially `0x207f8670` |
| `+0x08` | Three-argument preview callback, initially `0x207f8800` |
| `+0x0c` | Caller supplies a 0/1 value derived from a module service |
| `+0x10` | Caller copies GLOBAL word `+0xc18`; full interpretation pending |
| `+0x14` | Selected record index, initialized to zero |
| `+0x18` | Record count |
| `+0x1c` | Pointer to temporary records, each **0xb4 bytes** |
| `+0x20` | Axis-indicator selection written by the UI |
| `+0x24` | Window-title choice written by the UI |

The temporary `0xb4` records are **not WINDOW's `0xa4` service records**.
MAIN allocates `count * 0xb4` bytes and populates them from the window/view
state; handlers independently compute `(index * 5 * 9) * 4` to select a
record. Reusing the WINDOW structure here would put the second and later
records at the wrong addresses. The final pointer at temporary record
`+0xb0` is read by MAIN's preview code to reach the underlying view/window.

## Preview lifetime

Template `0x5e1` contains the custom `FS98DLG3D` control, id `0x188e`.
The following operations are implemented and exercised in both harnesses:

| Phase | Call site | Callback at descriptor `+0x08` |
| --- | --- | --- |
| Initialization | `0x223e1bfb` | `(1, descriptor, preview_child_hwnd)` |
| Close | `0x223e1f2e` | `(2, descriptor, 0)` |
| Idle tick `0x464` | `0x223e1f0b` | `(3, descriptor, 0)` |

The tick handler at `0x223e1f00` returns zero with `ret 8`. Close at
`0x223e1f20` calls operation 2 **before** forwarding to SIMSUI's
`CSimsDialog::Close`. Validation at `0x223e3600` first synchronizes the
current fields via `0x223e21a0`, then invokes descriptor `+0x04` with the
descriptor pointer before checking per-mode numeric validation.

The callback callees at `0x207f8670` and `0x207f8800` end in `ret 4` and
`ret 12`, respectively, and preserve the usual nonvolatile registers. Both
callback types are stdcall; Views ignores their return values.

MAIN's preview dispatcher at `0x207f8800` branches on operations 1, 2 and 3.
Its operation-3 path can create/destroy a preview view, tracks selection
changes and copies edited fields into preview state. Therefore a caller-only
stub that always returns success would hide observable preview behavior.

## MFC events

Message map `0x223e5120` is returned by getter `0x223e1910`. Commands below
use `WM_COMMAND` (`0x111`). Code 1 is the combo-box selection notification;
code 0 covers the buttons and checkboxes. Code `0x300` is the edit-change
notification. The handlers and their state changes are covered by the external differential
suite; the native suite exercises the inherited MFC dispatch.

| Control / event | Code | Handler | Narrow observation |
| --- | --- | --- | --- |
| Window selector `0x1876` | `1` | `0x223e22b0` | Changes selected-window state and controls |
| Direction `0x1891` | `1` | `0x223e34c0` | Store combo selection into selected record `+0x74` |
| Transition type `0x1896` | `1` | `0x223e3500` | Store combo selection into selected record `+0xa4` |
| Altitude reference `0x1894` | `1` | `0x223e3540` | Store combo selection into selected record `+0xac` |
| Axis indicator `0x1882` | `1` | `0x223e3580` | Store combo selection into descriptor `+0x20` |
| Cockpit `0x1879` | `0` | `0x223e24f0` | Mode-specific control changes |
| Virtual cockpit `0x187b` | `0` | `0x223e26f0` | Mode-specific control changes |
| Tower `0x187d` | `0` | `0x223e28f0` | Mode-specific control changes |
| Spot plane `0x187e` | `0` | `0x223e2ad0` | Mode-specific control changes |
| Track `0x187f` | `0` | `0x223e2d50` | Mode-specific control changes |
| Map `0x1880` | `0` | `0x223e2f30` | Mode-specific control changes |
| Automatic fullscreen `0x1887` | `0` | `0x223e35b0` | Copy object `+0x1d50` to selected record `+0x70` |
| Gradual transition `0x1890` | `0` | `0x223e35d0` | Copy object `+0x24e8` to selected record `+0xa8` |
| Window titles `0x187c` | `0` | `0x223e35f0` | Copy object `+0x27b0` to descriptor `+0x24` |
| Zoom edit `0x1886` | `0x300` | `0x223e3830` | Parse zoom and update spin control |
| `WM_VSCROLL` (`0x115`) | — | `0x223e38e0` | Spin/zoom text handling |
| `WM_HELP` (`0x53`) | — | `0x223e1ec0` | `WinHelpA`, command `0xc`, control id from help structure |
| `WM_CONTEXTMENU` (`0x7b`) | — | `0x223e1ee0` | `WinHelpA`, command `0xa`, returns 1 |
| Idle tick (`0x464`) | — | `0x223e1f00` | Preview callback operation 3 |

The combo handlers issue `SendMessageA(control_hwnd, 0x147, 0, 0)` and store
the result. The checkbox handlers shown are plain copies from MFC-bound
object fields; the inherited data-exchange ordering must be preserved before
assuming that those fields already reflect the newest Windows check state.

Numeric validation depends on selected record `+0x04` and `+0x08` (mode),
with different exchange descriptors inside the `0xb4` record. The mode
switch covers values 0..4. Distance and altitude have separate validation
paths for the spot-plane mode. A failure selects the zoom (`0x1886`),
distance (`0x1889`) or altitude (`0x188b`) control for error display. The synthetic fixture includes zero/out-of-range modes, independent error
flags, rejected error dialogs, negative record counts and callback reentry.
