# Views — View Options dialog boundary

`English/Views.DLL` is UI module `0x80000005`, with 13,090 bytes of `.text`,
one registered dialog (`0x1508`) and one recognised MFC message map with
19 entries. This is a static survey; no Views replacement or UI differential
test is implemented. The inventory and tool are described in
[dialogs.md](dialogs.md).

It imports only HOST and GLOBAL through FS98's numeric ABI. PE imports are
USER32, SIMSUI, MFC42, MSVCRT and KERNEL32. Preview rendering and simulator
changes are supplied by descriptor callbacks rather than static imports of
WINDOW, MAIN, G3D or HG2D.

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

Template `0x5e1` contains a custom `FS98DLG3D` control, id `0x188e`. A future
UI harness needs its lifecycle and the callbacks, even if its first graphics
model only logs them. The following calls are explicit in Views:

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

These are argument observations from call sites. Callee stack cleanup and
register preservation still need checking in MAIN's callback listings before
declaring C prototypes, as required by [method.md](../method.md).

MAIN's preview dispatcher at `0x207f8800` branches on operations 1, 2 and 3.
Its operation-3 path can create/destroy a preview view, tracks selection
changes and copies edited fields into preview state. Therefore a caller-only
stub that always returns success would hide observable preview behavior.

## MFC events

Message map `0x223e5120` is returned by getter `0x223e1910`. Commands below
use `WM_COMMAND` (`0x111`). Code 1 is the combo-box selection notification;
code 0 covers the buttons and checkboxes. Code `0x300` is the edit-change
notification. Labels here describe the associated controls, not verified
handler behavior for every mode.

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
distance (`0x1889`) or altitude (`0x188b`) control for error display. These
paths need boundary vectors, including zero and out-of-range mode values,
before any rewrite.

## Next verification slice

First pin the descriptor and `0xb4` record layouts, callback prototypes and
caller cleanup from retail. A harness should compare the descriptor and
records as well as the callbacks and HWND lifetime. Drive every initial view
type, switching between windows, all combo/check/radio changes, validation
failure and retry, accept/cancel, repeated ticks and nested help.

Use the real or faithfully modelled preview control and distinguish operation
1 creation, operation-3 updates and operation-2 cleanup. Include selection
changes while preview state already exists. Check numeric edits with empty
text, malformed text, locale/formatting variations, spin boundaries and x87
precision differences; `sscanf` and `_ftol` behavior are observable here.

The independent survey is complete enough to scope this work, but it has not
established the full field meanings, all inherited MFC behavior or a faithful
native preview. Those are verification tasks, not assumptions to put into an
already-passing WINDOW or MAIN harness.
