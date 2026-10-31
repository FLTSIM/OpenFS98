# MP_DIALOG.DLL — next-work handoff

**Partial isolated implementation: registries `0x110a`, `0x110b` and
`0x110c`.** The state, callback and lifetime comparisons for the three
factories pass, as do the targeted behavioral mutations, two unload cycles and
private-copy startup smoke. Complete native child-tree comparisons pass for
all three factories; all seven captured dialog states render identically to
retail, and 14/14 unchanged same-window repeat captures are stable. The
replacement stays in ignored `bin/mpdialog-diff/` and has not been copied to
the game install. Bounded actual HOST/MULTIP probes now pair retail and
replacement Connect → Cancel runs: both return the same five-HWND array,
observe `+0x1c=1` suppressing the queued Next, cancel Intro, destroy all five
pages through their MFC hooks, return to the owner, and clear provider
payloads. A separate actual host-mode attempt reaches validation and accept
callbacks. Matched retail and replacement Host attempts now trace the nested
provider callback through its return: provider/interface creation succeeds,
then Wine `dplayx.dll` returns `E_NOTIMPL` from `IDirectPlay3::Open` in both
binaries, leaving 0x110b Host Settings disabled. `0x110c` and confirmation
service `1016` remain unreachable through this external failure. Genuine MFC
partial-Create cleanup also remains unverified. MP_DIALOG is still partial
pending these owner-path limits and whole-module acceptance review.

Reference input: `orig/English/mpdialog.dll`, preferred base `0x22340000`,
module id `0x8000000a`. Installed name: `English/MP_DIALOG.DLL`. This is distinct
from the implemented `Modules/mpchat.DLL` chat window. Read
[the continuation rules](dialogs.md#starting-a-remaining-dialog-in-a-fresh-context)
and [the static inventory](dialogs.md#mpdialogdll).

| Registry id | Factory VA | Template(s) | Flags |
| --- | --- | --- | --- |
| `0x110a`, connection flow | `0x22341130` | `0x68,0x67,0x69,0x66,0x65` | `2` |
| `0x110b`, session settings | `0x223411e0` | `0x6a` | `0` |
| `0x110c`, session details | `0x22341270` | `0x6b` | `0` |

With three registrations, seven templates and 25,401 text bytes, this is the
smallest remaining whole English DLL by code size. It is a good independent
whole-module candidate, but the connection flow is not one ordinary dialog.
Flag 2 means the factory returns a zero-terminated HWND array. Its five-window
creation order and initial command are recorded in
[the array-factory section](dialogs.md#the-multiplayer-array-factory).

## Starting boundary

The 0x110a, 0x110b and 0x110c slices are built independently in the private
dialog workspace. State/callback/lifetime, complete native child-tree,
rendering, repeatability, mutation, repeated-unload and private-startup checks
pass for the isolated harness. Separate private-copy owner probes now observe
the actual 0x110a Connect → Cancel lifecycle through MULTIP cleanup and actual
host-mode validation/accept callbacks. Actual 0x110b entry/cancel is also
observed. Active-session 0x110c/1016 integration and genuine MFC construction
failure remain outside the observed coverage. The native differential
harness still models owner services for its repeatable comparisons.

## Connection flow (`0x110a`) contract and partial implementation

This is a static contract recovered from `orig/English/mpdialog.dll`
(preferred base `0x22340000`), the retail HOST in `orig/FLTSIM98.EXE`, and
MULTIP (`orig/Modules/multip.dll`, preferred base `0x20850000`). Resource and
message-map facts were checked with `tools/dialogmap.py`; control flow and
vtable targets were checked in disassembly. MULTIP callback prototypes below
are verified from the actual callee epilogues (`ret N`), not inferred from the
number of pushes at UI call sites.

### Factory, descriptor, and ownership

Registration `0x110a` points to factory `0x22341130` with flags 2. The factory
zeros the five HWND globals `0x2234a0b8..0x2234a0c8`, zeros current/previous
page indices at `0x2234a0ec/0x2234a0f0`, creates the five page objects, writes a
null terminator at `0x2234a0cc`, and returns the address of the global HWND
array (`ret 8` at `0x223411d0`). The HWND array is therefore static DLL state,
not an allocation returned to the caller. Its index order is the page-state
order:

| Array index | Page | Template | Helper factory | Object size / descriptor member | Page vtable |
| ---: | --- | ---: | ---: | --- | ---: |
| 0 | Introduction | `0x65` | `0x22341300` | `0x19f0` / `+0x19ec` | `0x22348f88` |
| 1 | Callsign and route choice | `0x66` | `0x223413a0` | `0x1cb0` / `+0x1cac` | `0x22348db0` |
| 2 | Host options | `0x67` | `0x22341440` | `0x1d5c` / `+0x1d58` | `0x22348348` |
| 3 | Connection type | `0x68` | `0x223414e0` | `0x18fc` / `+0x18f8` | `0x22348120` |
| 4 | Join-session browser | `0x69` | `0x22341580` | `0x1af0` / `+0x1ae8` | `0x223486d8` |

Creation order is the reverse of the user-facing path: connection type, host
options, browser, route choice, introduction. Each helper calls SIMSUI
`ui_new` (IAT `0x2234b4bc`) for the size shown, constructs the local class,
stores the same caller-owned descriptor pointer in the member shown, copies
the module instance to object `+0x80`, initializes its resource through the
MFC ordinal-2086 thunk `0x2234621a`, hides the window with `ShowWindow(0)` via
`0x22346214`, saves the C++ object in a module global, and returns its HWND at
object `+0x20`. The supplied parent HWND is passed to each class/base
constructor. The five object globals are `0x2234a0a0/a4/a8/ac/b0` in the
same page order as the table.

The returned array is exactly `[intro, choice, host-options, connection-type,
browser, NULL]` at `0x2234a0b8..0x2234a0cc`; factory stores are visible at
`0x22341172..0x223411a5`. The individual helpers do not allocate an array or
copy the descriptor. MULTIP's `multip_configuration_dialog` at
`0x20854d60` passes `D+0x3a8` to HOST `+0x118` (`0x20854d7b`); HOST forwards
to `run_dialog` at `0x00409720`, calls the factory with `(main_hwnd,
descriptor)`, and treats flags 2 as an HWND array.

The descriptor is owned by MULTIP and borrowed by all five pages. Useful
fields confirmed by the UI and owner code are `+0x00` result, `+0x04/+0x08`
provider/reset callbacks, `+0x0c` accept, `+0x10` validation, `+0x14` a
temporary HWND written by the connection-type handler, `+0x1c` introduction
checkbox state, `+0x24` route mode, `+0x2c` callsign buffer, `+0x44` host
options callback, `+0x4c/+0x60/+0x74` host session/capacity text buffers,
`+0x48/+0x5c/+0x70` related validation status words, `+0x84/+0x88/+0x90`
provider rows/count/selection, `+0x94..+0xa4` browser/options callbacks,
`+0xac/+0xb0` session rows/count, and `+0xb8` selected session index. MULTIP
initializes and retains the pointed-to buffers and row arrays until after the
modal call returns; the page objects do not free descriptor storage.

The five C++ objects are separately allocated. All five vtables put local
`PostNcDestroy` wrapper `0x22344820` at `+0xac`; their deleting-destructor
slots at `+0x04` are connection `0x22341910`, host options `0x223421f0`,
browser `0x22342de0`, choice `0x22345270`, and introduction `0x22345c90`.
Each destructor releases its embedded controls in reverse construction
order, calls the SIMSUI `CSimsDialog` base destructor, then calls SIMSUI
`ui_delete` (`0x2234b46c`) when invoked with deleting flag 1. The common
`PostNcDestroy` wrapper invokes the object's deleting-destructor slot with
flag 1. The class/vtable wiring is static evidence; the isolated retail probe
also observed this sequence for each of the five HWNDs when it called
`DestroyWindow` (see *Evidence and limits* below). The probe did not execute
HOST's own modal loop.

### Initial state and page transitions

The factory starts state 0 (introduction) and previous state 0. If descriptor
`+0x1c` is zero, at `0x223411c3` it calls the `PostMessageA` IAT entry
`0x2234b4ec` to queue `WM_COMMAND`, `wParam=0x3e8` (Next), `lParam=0` for the
last-created HWND (`array[0]`, introduction), then returns. This is not a
synchronous call to the window procedure. The introduction checkbox is ID
`0x42b`; `0x22346120/0x22346150` transfers its value to/from descriptor
`+0x1c`.
Nonzero suppresses this factory command; zero triggers it.

The page-state helpers are `0x22341620` (advance), `0x223416a0` (back), and
`0x223417b0` (display). For a real state change, display calls `ShowWindow(5)`
on `array[current]` and `ShowWindow(0)` on `array[previous]`. The transitions
are:

| Current page/index | Advance | Back | Condition |
| --- | --- | --- | --- |
| Intro `0` | Choice `1` | rejected (`-1`) | Always |
| Choice `1` | Host options `2` | Intro `0` | mode `+0x24 == 1` advances to host; all other modes advance to connection type `3` |
| Host options `2` | Connection type `3` | Choice `1` | Only reached in host mode |
| Connection type `3` | Browser `4` | Host options `2` when mode is 1, otherwise Choice `1` | Advance rejects mode 1; host mode accepts through its Host action |
| Browser `4` | rejected (`-1`) | Connection type `3` | Join uses the accept path instead |

The page-level transition handlers are Intro Next/Back `0x22345ea0/0x22345e90`,
Choice Next/Back `0x22345500/0x223454d0`, Host Next/Back
`0x22342520/0x223424f0`, Connection Next-or-Host/Back
`0x22341b30/0x22341b00`, and Browser Join/Back
`0x22343060/0x22343000`.

Mode values are tied to resource labels and MULTIP reads/writes: 0 is join
existing session, 1 is host a session, and 2 is the “find pilots online”
route. Mode 2 accepts directly from the choice page; the owner sets its
pending mode at MULTIP `0x2085505b`. These labels describe the visible controls
and owner code, not a claim about actual Internet service availability.

The host's flag-2 path at `0x00409720` shows the first array HWND with
`SW_SHOW` after factory return (`0x004097d3..0x004097dc`). The factory's
initial command is a queued post, so the host can show Intro before the normal
message loop dispatches that command. The isolated probe reproduced this
ordering by showing Intro and then dispatching the queued message: Intro's
Next handler advanced state 0→1 and left only Choice visible.

### Page commands, validation, and callbacks

All five templates have Back (`0x3e9`) and Cancel (`2`). Intro has Next
(`0x3e8`) and “Always show this screen” (`0x42b`). Choice has Next and IDOK
(`0x3e8`/`1`), callsign edit (`0x419`), and three route radios: Join (`0x41c`),
Host (`0x41e`), online (`0x41d`). The choice updater `0x223458a0` loads
descriptor `+0x2c` and `+0x24`; `0x22345920` captures the selected radio as
mode 0/1/2. Mode 2's Next/OK handler `0x22345500` calls owner accept and then
the shared accept helper; modes 0/1 advance instead.

Host options has session name edit `0x41a`, pilot capacity edit/spin
`0x40f/0x411`, observer capacity edit/spin `0x410/0x412`, host-as-observer
checkbox `0x3ed`, Settings `0x426`, Next, Back, and Cancel. Its Next handler
`0x22342520` captures the controls through virtual `+0xf4`, calls virtual
validation slot `+0xd8`, advances only on zero, and sends a nonzero field id
to virtual error-display slot `+0xdc`. Back `0x223424f0` captures values but
does not run the explicit Next validation. Settings `0x223425d0` calls the
owner function at descriptor `+0x44`.

Connection type has provider combo `0x41b`, Next `0x3e8`, Host `0x433`, Back,
and Cancel. Its initialization/update path `0x22341e60` resets the combo,
adds `+0x88` provider names from the `+0x84` array at 28-byte strides, selects
`+0x90`, and updates button visibility. Host mode shows Host and hides Next;
other modes show Next and hide Host (`0x22341f60`). Next and the provider
notification share Connection handler `0x22341b30`. The `WM_COMMAND`
`0x41b/code=2` message-map entry is thunk `0x22341c10`, which jumps through
vtable slot `+0xcc` to thunk `0x22341c00`, which tail-jumps to that handler. The handler captures the page and calls
descriptor `+0x04` at `0x22341b84`; a successful provider-selection
notification therefore advances state 3 to Browser immediately. A later Next
sent to that now hidden page is a separate command, not part of the normal
provider-selection transition. The isolated scripts do not send that stale
Next after successful selection.

* Mode 1 sets result 1 and calls descriptor `+0x0c` (accept). A zero callback
  return calls the shared accept helper; nonzero leaves the flow open.
* Mode 0 stores this HWND in descriptor `+0x14`, disables the page, and calls
  descriptor `+0x04` through adapter `0x223417f0`. Return zero reenables and
  advances to the browser. Nonzero reenables and calls descriptor `+0x08`
  through `0x22341800`, then leaves the page active.
* Mode 2 has no action on this page.

Browser template `0x69` has session list `0x3fd`, observer checkbox `0x430`,
Refresh `0x401`, Details `0x400`, Settings `0x3ec`, Back, Join `0x3ff`, and
Cancel. List selection is stored at `+0xb8`; selection handlers
`0x22343510/0x22343570` update Join availability and call `+0x98` with
`(descriptor, selected_index)` to refresh that row's player counts. Join
handler `0x22343060` calls `+0x0c` and then the shared accept helper regardless
of the callback result (see the later paired Browser audit).
Details `0x22343110` stores selection and calls `+0x9c`; Settings
`0x22343150` calls `+0xa4`; Refresh `0x22343850` calls `+0xa0` and refreshes
the controls when that callback reports success. The observer toggle updates
descriptor `+0x40`. The local `WM_USER+100` (`0x464`) handler at `0x22343780`
only clears descriptor `+0x14`; it does not enumerate sessions.

The Browser message map at `0x22342ff0` also contains three handlers involved
in activation and refresh:

| Message | Retail handler | Verified action |
| --- | ---: | --- |
| `WM_MOUSEACTIVATE` (`0x21`) | `0x223438b0` | Calls MFC ordinal 2379; when object `+0x1aec` is set, returns 4 (`MA_NOACTIVATEANDEAT`), otherwise returns the inherited result. `ret 0xc`. |
| `WM_SHOWWINDOW` (`0x18`) | `0x223438d0` | Calls MFC ordinal 2379, kills timer 1, and when shown starts timer 1 for 500 ms with a null `TIMERPROC`. `ret 8`. |
| `WM_TIMER` (`0x113`) | `0x22343910` | Kills the timer identified by `wParam`, then calls refresh `0x22343850`. `ret 4`. |

Refresh `0x22343850` sets object `+0x1aec` before calling descriptor
callback `+0xa0`, updates through vtable `+0xf0` on a nonzero result, and
clears the guard afterward. A nested refresh exits immediately; the
MouseActivate handler also returns 4 while this guard is set. These handler
bodies and the reentry guard are verified by retail disassembly.

**Paired runtime instrumentation:** the harness finds the Browser `WM_TIMER`
entry (`message=0x113`, `code=0`) through each module's `+0x30` message-map
getter, checks signature `0x0d` and that the target belongs to that binary,
then wraps and calls each original handler. Retail resolves to
`0x22343910`; the replacement resolves to its module-owned
`0x01553720` in this build. SetTimer/KillTimer calls are separately wrapped
through each image's USER32 IAT and compared by arguments and result. This
avoids relying on WH_CALLWND hooks for a `DispatchMessage`-delivered timer,
which those hooks did not count.

In Browser path case 3, the harness confirms the retail 500 ms timer request,
then posts the same `WM_TIMER` payload (`wParam=1,lParam=0`), removes that
queued message and dispatches it. The modeled `+0xa0` callback synchronously
sends a nested `WM_TIMER` while `+0x1aec=1`. Both original handlers are
entered twice; the outer call invokes `+0xa0` once, while the nested refresh
returns at the guard and does not invoke it again. The callback leaves the
outer guard set until the update finishes. This checks posted versus
synchronous dispatch and the reentry branch in both binaries; timer expiry is
modeled and the harness does not wait 500 ms. `WM_MOUSEACTIVATE` is verified
statically but physical mouse input is not exercised.

Focus-loss validation is inherited through `CSimsDialog::OnCommand`: vtable
slot `+0x80` is MP_DIALOG thunk `0x223461e4` to SIMSUI `0x204764c0`, which
dispatches `EN_KILLFOCUS` through the page's virtual `+0xd8`. Intro, connection
type, and browser inherit default `CSimsDialog::ValidateData` (thunk
`0x223461de` to `0x20471610`, returning zero). Choice overrides it at
`0x22345ab0`: while visible, it updates controls, calls descriptor `+0x10`,
and if callback failure coincides with nonzero descriptor `+0x28`, returns
field code `0x41a`. The template's callsign edit is `0x419`; the mismatch is
resolved by the page error handler `0x22345a40`, which maps nonzero `+0x28` to
the actual edit control `0x419` before calling virtual `+0xe0`.

SIMSUI's inherited handler at `0x20476706..0x20476759` turns a nonzero
focus-loss validation result into pending-field state at dialog `+0x7d0`,
posts message `0x5f4` with `wParam` equal to that field code and `lParam=0`
through `PostMessageA` IAT `0x204929e4` (`0x20476a00`), and suppresses ordinary
commands while `+0x7d0` is nonzero (`0x2047656b..0x2047657e`). IDOK and Cancel
are exceptions to that command suppression. On successful focus loss, it
clears the pending field (`0x20476742..0x20476754`). Dispatching the posted
`0x5f4` reaches each page's error handler. Choice displays the callsign edit
`0x419`; Host options displays the field ID selected by its validator.

Host options overrides `+0xd8` at `0x22342c40`: while visible it captures
controls and calls `+0x10`; callback failure maps the first nonzero status in
`+0x48`, `+0x5c`, `+0x70` to IDs `0x41a` (session name), `0x40f` (pilot
capacity), and `0x410` (observer capacity), in that order. Its error handler is
`0x22342b60`.

MULTIP's validation callback at `0x20854e80` writes exchange fields with
flag 1, validates a nonempty callsign except in mode 2, and sets `+0x28` to
`0x1003` for an empty callsign. In mode 1 it also derives observer state from
`+0x40`, requires a session name (`+0x48=0x1003` when empty), parses both
capacity strings at `+0x60/+0x74` with limit argument `0x1b`, and writes parse
statuses to `+0x5c/+0x70`. It processes both capacity fields even if earlier
name checks failed. Callee epilogue `0x20854f6c` is `ret 4`.

| Descriptor field | MP_DIALOG call sites/action | MULTIP callee | Callee cleanup / ABI |
| --- | --- | --- | --- |
| `+0x04` | Adapter `0x223417f0`, called from Connection handler `0x22341b30` at `0x22341b84` on provider notification (`0x22341c10` → vtable `+0xcc`) and on explicit Next in mode 0 | provider creation `0x20854f70` | `ret 4`: stdcall `int32_t callback(void *descriptor)` |
| `+0x08` | Adapter `0x22341800`, only after nonzero `+0x04` at `0x22341b9d` | provider/browser reset `0x20854fc0` | `ret 4`: one pointer |
| `+0x0c` | Connection Host `0x22341b59`; choice mode-2 accept `0x22345524`; browser Join `0x22343072` | accept `0x20855000` | `ret 4`: one pointer |
| `+0x10` | Choice validation `0x22345ad6`; host-options validation `0x22342c66` | validation `0x20854e80` | `ret 4`: one pointer |
| `+0x44` | Host-options Settings `0x223425d7` | options wrapper `0x208551b0` | `ret 4`: one pointer |
| `+0x94` | Stored browser-refresh function; not called directly by MP_DIALOG code | session discovery `0x208552f0` | `ret 4`: one pointer; called internally by refresh-all |
| `+0x98` | Browser selection `0x2234355c` | selected-session player refresh `0x20855660` | `ret 8`: stdcall `int32_t callback(void *descriptor, uint32_t index)` |
| `+0x9c` | Browser Details `0x22343145` | details-dialog owner `0x20855620` | `ret 4`: one pointer |
| `+0xa0` | Browser Refresh `0x22343890` | refresh-all `0x208555c0` | `ret 4`: one pointer |
| `+0xa4` | Browser Settings `0x22343157` | options wrapper `0x208551b0` | `ret 4`: one pointer |

The provider callback uses selected provider `+0x90` and 28-byte row
`+0x84 + index*28`, taking its GUID at row `+4`; it creates DirectPlay and
shows confirmation on failure. The reset callback clears browser records,
releases DirectPlay if present, then zeros `+0xb0`. Accept `0x20855000`
commits exchange values and selects the mode-specific owner effect: mode 2
sets pending mode 3; mode 1 sets host state and creates the selected provider;
mode 0 selects the current 84-byte browser record, copies its session data,
and allocates its name. These effects are owner behavior, not MP_DIALOG-owned
storage.

MULTIP initialization at `0x20854dc0` exchange-reads nine settings, enumerates
providers, publishes provider pointer/count in descriptor `+0x84/+0x88`,
clears the browser list, and formats host-limit strings. The browser Refresh
button calls `multip_browser_refresh_all` at `0x208555c0`; that calls session
enumeration `0x208552f0`, which marks old 84-byte rows stale, enumerates
DirectPlay sessions, matches their 16-byte GUIDs, updates/adds records, prunes
stale rows, and publishes descriptor `+0xac/+0xb0`. Refresh-all then calls
`0x20855660` once per row for player counts. The list and provider records
remain MULTIP-owned. Details callback `0x20855620` passes the selected session
to the separate `0x110c` dialog; Settings opens `0x110b`.

After HOST returns successfully, MULTIP `0x20854d88..0x20854dbc` branches on
descriptor result: a nonzero result calls launch `0x208551c0` (which calls
`ShellExecuteA` only when MULTIP mode `D+0x3cc == 2` and ignores its return);
zero clears owner mode words `+0xaa8/+0x1a4`. It then clears provider/browser
records at `0x208557a0` and returns the dialog result. Setup failure or a
nonzero HOST service return exits as cancellation before this cleanup. Thus
normal cancellation does not ask MP_DIALOG's `+0x08` callback to own the
provider/session allocations; the owner clears them after the HWND array has
been destroyed.

### Accept, cancel, HOST dispatch, and teardown

The shared accept helper `0x22341760` writes descriptor result 1 and closes
the five page objects in reverse creation order: Intro `0x22345ec0`, Choice
`0x22345550`, Browser `0x22343090`, Host `0x22342560`, Connection Type
`0x22341bc0`. Each
stores its local accepted state and calls virtual `+0xec` (`CSimsDialog::Close`).
The local Cancel virtuals are at `+0xd0`: Intro `0x22345f00`, Choice
`0x223455a0`, Browser `0x223430e0`, Host `0x223425b0`, Connection Type
`0x22341c20`. They write result 0 and call shared helper `0x22341710`, which
closes all five page objects in reverse creation order. Cancel does not call owner
callback `+0x08`; that callback is the retry cleanup after provider callback
`+0x04` fails. The template Cancel controls have no page-local `WM_COMMAND`
map entry; routing ID 2 through the listed class `OnCancel` virtual is
consistent with MFC's default dialog command dispatch. The retail probe sent
`IDCANCEL` to Intro and Choice in separate runs; both produced result 0 and
marked all five HWNDs done and hidden without calling the owner callbacks.

`CSimsDialog::Close` marks each HWND's `GWL_USERDATA` nonzero and hides it.
HOST's array modal loop at `0x00409928..0x00409a0a` checks the first HWND's
alive/done state, offers messages to the array, and while idle synchronously
sends `0x464` with zero `wParam/lParam` to each HWND through the terminating
null (`0x004099d3..0x004099e1`). On exit, HOST iterates the array and calls
`DestroyWindow` on each still-live HWND (`0x00409a17..0x00409a31`), then
releases the module at `0x00409b30..0x00409b39`. All five objects' `WM_NCDESTROY`
path is statically tied to their `+0xac` wrapper and deleting destructor
slots above. HOST's idle send and its array walk are actual retail static
paths. In the isolated probe, `DestroyWindow` was applied to each still-live
array entry in that same order; the retail page window procedure then showed
the `WM_NCDESTROY` → `PostNcDestroy` → deleting-destructor sequence for every
page. The HOST executable's own modal loop was not run in-process.

### Partial construction and evidence limits

There is no factory-level rollback loop. The factory calls the five helpers,
writes a terminator, and returns the static array; it does not test the helper
HWNDs. The helpers do not test MFC ordinal-2086's Create result before storing
the object and returning its `+0x20` HWND. HOST's array loops stop at the first
null entry, so later entries beyond a hole are not reached by those loops.

**Runtime observation — injected Create failure:** the probe intercepted MFC
ordinal 2086 and forced its return to false for template `0x68`, without
calling MFC. The connection object remained in global slot 3 with a null HWND;
the factory continued to create templates `0x67`, `0x69`, `0x66`, and `0x65`
and returned. The resulting array was `[intro, choice, host, NULL, browser,
NULL]`; the probe confirmed the later Browser HWND was still live, and a
modeled HOST null-terminated walk stopped at index 3. Probe-only cleanup then
destroyed valid HWNDs and explicitly invoked the deleting destructor on the
object without a HWND; retail factory cleanup did neither. This verifies the
MP_DIALOG caller/helper response to a
false Create result; it does not exercise resource lookup failure or MFC's
internal cleanup after a partially created child window.

**Runtime observation — injected allocation failure:** the HOST allocator
callback returned null for the final Intro object allocation (`size=0x19f0`,
fifth allocation, callback return site `0x20471b1a`). Retail faulted at
`0x2234134a` while the helper stored object fields through null. The exception
snapshot showed the previously created Choice, Host, Connection, and Browser
HWNDs still valid and Intro's array/object entries null. The probe's exception
filter recorded state and terminated the process; it did not observe C++/SEH
unwinding or post-fault cleanup. Thus the allocation-failure path is a fault,
not a clean null-window return, and cleanup after that fault remains unknown.

**Static evidence:** factory order, state transitions, callbacks, descriptor
reads/writes, vtables, `ret N` prototypes, HOST array dispatch, and MULTIP
provider/session ownership above.

**Runtime observation — probe setup and resource failure:** reusable probe
source is `tools/difftest/mpdialog110a_probe.c`; it was built standalone to
`/tmp`, without adding a shared build target or changing production sources.
The corrected
probe used a dedicated Wine prefix and loaded retail `MP_DIALOG.DLL`,
`SIMSUI.DLL`, `MFC42.DLL`, `GLOBAL.DLL`, and `DIALOG.DLL`. Its DLL search
directory was isolated to `orig`, matching the working dialog probes. A
fail-fast CBT hook captured the exact “Resource data not found” text and
terminated with status 4; it did not dismiss the dialog or count it as
successful initialization. Controlled
ablations identify the missing dependency: the bare bootstrap failed, as did
FLL-only and HOST string-service-only variants. With explicit DIALOG, FLL, and
the HOST string stub suppressed, setting the DLL directory to `orig` succeeded
and caused `DIALOG.DLL` to load dynamically
(`GetModuleHandleA("DIALOG.DLL") == 0x20000000`). Explicitly loading that
`DIALOG.DLL` also succeeded with no FLL, string-service stub, or DLL search
directory. Therefore the reproduced error is caused by missing DIALOG resource
DLL resolution, not a missing `ENGLISH.FLL` string or HOST string service. The
specific bitmap leaf/ID that prompted the lookup was not identified. This
matches the supporting-library role recorded by `dialogmap.py` and the
`SetDllDirectoryA(orig)` setup in working dialog probes such as
`tools/difftest/alertsuidiff.c` and `simuidiff.c`.

The reported protection failure was in the probe, not retail. For vtable slot
`0x22349034` (`+0xac`), `VirtualQuery` reported a committed `MEM_IMAGE` region
at `0x22349000`, size `0x1000`, `PAGE_READONLY` (`0x02`). `VirtualProtect` to
`PAGE_READWRITE` succeeded and returned the old protection. The probe then
wrote through its null `original` output pointer at executable PC
`0x00406e74`. `VirtualProtect` returned success, so no API error was reported;
the probe's error handler reads `GetLastError` only on a false return. The
corrected hook saves/restores the original slot. The `PostMessageA` IAT slot
`0x2234b4ec` is in committed image memory at
`0x2234b000`, size `0x1000`, `PAGE_WRITECOPY` (`0x08`); its patch/restore also
succeeded. These addresses and flags are from the isolated retail process.

**Runtime observation — construction and startup:** with DIALOG resolved, the
retail factory at `0x22341130` returned the global array at `0x2234a0b8`; all
five entries were valid HWNDs, in order Intro, Choice, Host options,
Connection Type, Browser, followed by NULL. MFC ordinal-2086 calls returned
success in creation order `0x68,0x67,0x69,0x66,0x65`, with each object already
holding its HWND before the next helper ran. Before HOST-style showing, all
five had `WS_VISIBLE` clear and `IsWindowVisible` false.

The factory's import at `0x2234b4ec` is **PostMessageA**. In the retail PE,
the USER32 first-thunk table begins at `0x2234b4dc`; the disk import name for
slot `0x2234b4ec` is RVA `0xb508` (`PostMessageA`). `SendMessageA` is a
different slot, `0x2234b4f8`, with import-name RVA `0xb526`.
In one captured call the descriptor was at `0x0065fc34`: result `+0x00=0`,
callbacks `+0x04/+0x08/+0x0c/+0x10` pointed to the probe stubs
`0x004080c8/0x004080fa/0x0040812c/0x0040815e`, `+0x1c=0`, mode `+0x24=0`,
and the callsign/session/pilot/observer buffers were at
`0x0065fc1c/0x0065fbfc/0x0065fbf4/0x0065fbec`. When descriptor `+0x1c` was
zero, the call at `0x223411c3` posted
`(intro HWND, WM_COMMAND=0x111, wParam=0x3e8, lParam=0)` after all five helper
returns. The hook recorded a valid same-thread Intro HWND, descriptor pointer
and contents, state/previous `0/0`, object pointer and object `+0x20` HWND
matching the target (captured object `0x003ed188`, HWND `0x0001010a`), callback
pointers at `+0x04/+0x08/+0x0c/+0x10`, and zero callback invocations.
`PostMessageA` returned 1, which is queue success; no
owner callback return exists at this point, and the state remained `0/0`.
`PeekMessage` after factory return found the same
`0x111/0x3e8/0` entry still queued. The probe then modeled HOST's initial
`ShowWindow(array[0],SW_SHOW)` and dispatched that queue entry. It returned 1,
advanced state `0→1` through Intro Next handler `0x22345ea0` and state helper
`0x22341620`, and left only Choice visible.

The first divergence from a post-return synchronous test is before the
MP_DIALOG handler: `PostMessageA` queues while `SendMessageA` synchronously
enters the window procedure. A direct same-parameter `SendMessageA` after
return used that same descriptor/object/HWND, returned 1 from the handler,
and advanced `0→1` immediately, with all four owner-callback counts still
zero at that checkpoint. In a deliberately artificial diagnostic,
that direct send followed by the already queued factory command advanced again
from state 1 to state 3; this demonstrates that the two events are distinct,
not two observations of one handler invocation. The normal queued transition
called Intro's virtual update at `0x22346120` and state helper `0x22341620`;
no owner callback ran. Thus callback stub behavior does not explain the
initial state mismatch. HOST's static first-window show at
`0x004097d3..0x004097dc` precedes its message-loop handling, but this probe
modeled that HOST path rather than running the HOST modal loop itself.

**Runtime observation — page paths:** all actions below ran against the retail
DLL with a visible STATIC parent. HOST and MULTIP were not loaded: the probe
modeled HOST's first-HWND show and final array destruction, while owner
callbacks were explicit stubs. The Browser's selected row was inserted by the
probe with `LB_ADDSTRING`, not supplied by a real MULTIP session record.

| Probe path | Observed transitions/effects | Callback evidence |
| --- | --- | --- |
| Intro Next; Choice Back; Intro Next | `0→1→0→1`; corresponding page visibility changed | No owner callback on these transitions |
| Mode 0: Choice Next; Connection Back; Connection Next; Browser Join | `1→3→1→3→4`, then close | `+0x04(descriptor)` returned 0; Browser selection called `+0x98(descriptor, 0)`; update called modeled `+0xa0(descriptor)` then `+0x98`; Details `+0x9c(descriptor)` and Settings `+0xa4(descriptor)` received descriptor when invoked; Join `+0x0c(descriptor)` saw result 0, then close set result 1 |
| Provider failure | `+0x04` configured to return 1; stayed on Connection; cancel closed with result 0 | `+0x08(descriptor)` reset callback followed the failure |
| Mode 1 host path | `0→1→2→3`; Host Settings and Connection Host actions exercised | `+0x44(descriptor)` on Settings; Connection Host called `+0x0c(descriptor)` and closed with result 1 |
| Cancel from Intro or Choice | each run closed all five and left result 0 | No owner callback from IDCANCEL; preceding Back/focus activity could separately call validation |

The `+0x10` callback was an explicit modeled MULTIP validator, not the real
MULTIP function. For blank callsign in mode 0 it returned `-1` and wrote
`+0x28=0x1003`, matching the actual callee's statically recovered branch; the
choice validator then returned `0x41a`. Runtime hooks observed the posted
`0x5f4` (`wParam=0x41a`), the error handler calling DisplayError for edit
`0x419`, and the next ordinary Next command leaving state at Choice because
SIMSUI's pending `+0x7d0` guard consumes it. For blank host session, the stub
returned `-1` with `+0x48=0x1003`, matching the MULTIP name-failure branch;
for its empty pilot/observer scenarios it supplied nonzero `+0x5c/+0x70` to
exercise MP_DIALOG's `0x40f/0x410` mapping. The validation runs confirmed
those error IDs reach the Host page's DisplayError slot and block the Next
transition. The probe intercepted DisplayError and returned 1 to suppress
modal UI, so it verified routing and field IDs, not rendered error text.

The modeled callback does not implement MULTIP's HOST `+0x20` parsing service
for capacity strings; capacity-field runs establish MP_DIALOG's status-word
mapping only. The actual `0x20854e80` callback prototype is independently
verified as stdcall one-pointer (`ret 4` at `0x20854f6c`), and its static body
does parse both capacities even after a session-name failure. Callback
arguments for `+0x04`, `+0x08`, `+0x0c`, `+0x10`, `+0x44`, `+0x9c`, `+0xa0`,
and `+0xa4` were observed with the descriptor; `+0x98` received descriptor
and index. These are runtime-verified argument shapes with modeled callback
targets, not proof of MULTIP side effects. `+0x94` was not reached directly by
MP_DIALOG in these scenarios. Real HOST modal/idle execution and real MULTIP
callback effects remain static-only.

**Runtime observation — teardown:** for each completed scenario, the isolated
probe called Win32 `DestroyWindow` on each still-live array HWND in array
order. The retail window procedure and vtable hooks recorded one sequence per
page: `WM_NCDESTROY` → local `PostNcDestroy` wrapper `0x22344820` → that page's
deleting-destructor slot at `+0x04` with flag 1. Each page's counters were
exactly one. This verifies the retail MFC destruction chain reached by
`DestroyWindow`; the caller was the probe, not HOST's actual loop. HOST's
`DestroyWindow` array walk and module release remain static evidence.

**Resource failure distinction:** when DIALOG was intentionally unavailable,
the fail-fast resource error occurred before a valid factory return; all five
HWND and object globals were zero at the captured failure. That reproduces a
lookup failure before page construction, not a later partial Create failure.
The injected Create and allocation probes above separately establish the
caller-visible failure behavior and the remaining cleanup limits.

**Inference and probe scope:** in the standalone retail probe, the first-HWND
show and final array walk were modeled, while the HOST modal loop was not run.
The later paired private-copy runs now verify the successful actual HOST show,
cancel, five-page teardown, and owner cleanup; see *Actual owner integration*
below. The injected-hole case still has only static HOST null-terminated walk
evidence plus the modeled probe walk, so retail behavior under partial array
construction remains inferred. The standalone probe's owner stubs establish
MP_DIALOG call shape, not real MULTIP side effects.

**Remaining uncertainty:** the exact DIALOG bitmap leaf that triggered the
standalone resource lookup is unidentified. The standalone retail probe
verified the factory-post path with `+0x1c=0`; actual HOST/MULTIP integration
now verifies the other branch, with `+0x1c=1` suppressing Next, plus actual
host-mode validation/accept entry. The mode-1 validation callback was
successful in that run; the actual error/recovery path and actual Browser
accept/Details/Refresh callbacks remain unexercised. The isolated native suite
covers those behaviors with modeled owner callbacks. Genuine MFC cleanup
after resource/child creation failure and unwinding after allocation failure
remain unverified. Passing isolated comparisons do not establish
whole-module acceptance.

## Session settings (`0x110b`) contract

The addresses below are preferred VAs in `orig/English/mpdialog.dll` (base
`0x22340000`) and `orig/Modules/multip.dll` (base `0x20850000`). The resource
facts come from `tools/dialogmap.py`; call behavior was checked in disassembly
against the MULTIP callback callees. Runtime evidence and remaining differences
are separated below.

### Construction and controls

Registry record `0x110b` points to factory `0x223411e0`, flags 0, template
`0x6a`. The factory allocates `0x2120` bytes with SIMSUI `ui_new` at
`0x223411fc`, calls the local class constructor `0x22343930` at `0x22341219`,
stores the supplied parameter block at object `+0x211c`, copies the module's
instance handle to object `+0x80`, initializes resource `0x6a` through the
MFC ordinal-2086 thunk `0x2234621a`, then calls `0x22346214` with `1`. It
returns the window at object `+0x20`; the factory ends with `ret 8` at
`0x22341268`. The factory does not use its first (parent-HWND) argument in
this local path; its call to the base `CSimsDialog` constructor passes a null
parent. The `ui_new` null branch clears the object register but continues into
field stores, so allocation failure is not a clean null-window return.

The class constructor initializes the `CSimsDialog` base and its owned control
objects in order: five `CSims3PieceButton`s at `+0x10dc..+0x14cc`, text at
`+0x15c8`, group boxes at `+0x1680/+0x1734`, five more buttons at
`+0x17e8..+0x1ac8`, six check buttons at `+0x1b80..+0x1f68`, and text art at
`+0x2030`. It then installs vtable `0x223488e8` at `0x22343a83`. The factory
stores the parameter block after construction and before creating the HWND.
`DoDataExchange` is at `0x22343c30`; the class message-map getter is
`0x22343d50`. The template is a standard dialog titled “Session Settings”
with checkbox controls for ADF lock (`1046`), DME lock (`1028`), autopilot lock
(`1045`), player names (`1049`), and sending/receiving detailed aircraft data
(`1047`, `1048`), plus Host Settings (`1027`), Get Flight Conditions (`1003`),
Set Flight Conditions (`1006`), OK (`1`) and Cancel (`2`).

`OnInitDialog` at `0x22343de0` calls the SIMSUI base initializer, configures
the controls and group boxes, and calls the local state updater at
`0x223440f0` through vtable slot `+0xf0`. The updater sets checkbox states and
enables or shows related controls from the parameter block. The local DDX map
binds the controls. This template has no editable text or numeric fields. No
class-specific parse or field-validation routine appears in the class code;
see the inherited validation path below. After the update, `OnInitDialog`
calls MFC ordinal `5981` through thunk `0x2234631c` with the OK control at
object `+0x11d8`; the replacement follows that order.
The updater reads option words at
block `+0x24`, `+0x2c`, `+0x30`, `+0x38`, `+0x40`, `+0x48`, and `+0x50`, and
uses `+0x18/+0x1c` for related control state. The save helper writes
`+0x24`, `+0x2c`, `+0x38`, `+0x40`, `+0x48`, and `+0x50`; `+0x30` is read by
the UI code but not written by this helper.

The flight-condition controls overlap in resource template `0x6a`. Both
control records, IDs `1003` and `1006`, have style `0x5003000b` (`WS_CHILD |
WS_VISIBLE` plus button flags). DDX binds ID `1003` to object `+0x13d0` and ID
`1006` to `+0x14cc`. Runtime `GetDlgCtrlID` checks on those embedded members
confirm that mapping in both binaries.

**Static trace:** `Update` at `0x223440f0` makes two visibility decisions. First it tests block
`+0x1c` at `0x223441d2`: zero shows `1006` and hides `1003`, and disables Host
Settings (`1027`); nonzero shows `1003`, hides `1006`, and enables `1027`. It
then tests block `+0x18` at `0x22344224`. Zero disables `1003`, `1006` and
`1027` while leaving the first visibility choice intact. Nonzero executes a
second pair of `ShowWindow` calls at `0x22344251..0x22344291`: if `+0x1c` is
zero it shows `1003` and hides `1006`; otherwise it shows `1006` and hides
`1003`. Thus with `+0x18` nonzero, the final visible control is the complement
of the first choice (`1003` is visible iff the zero/nonzero tests of `+0x18`
and `+0x1c` differ). The replacement originally omitted this second pair,
which left the first choice in place and reversed the retail result for every
nonzero `+0x18` case.

The call order distinguishes the update from inherited and parent behavior.
`OnInitDialog` calls inherited `CSimsDialog::OnInitDialog` at `0x22343de6`,
then performs local control initialization and calls `Update` through slot
`+0xf0` at `0x223440c7`; its final focus call is at `0x223440d3`. The factory
creates the resource window at `0x22341248`, then shows the parent at
`0x22341251`. The parent becoming visible changes effective visibility but
does not account for the child `WS_VISIBLE` bits cleared by `Update`.
The base initializer resolves to SIMSUI `CSimsDialog::OnInitDialog` at
`0x20475870`. The class message map at `0x22343d50` has its three button
`WM_COMMAND` entries, `WM_HELP` and `WM_CONTEXTMENU`; it has no local
`WM_SHOWWINDOW` entry. The inherited focus-loss `OnCommand`/`ValidateData`
path is separate; the harness reaches it with an explicit `EN_KILLFOCUS`
notification.

SIMSUI check-state access is through `CSimsCheckButton::IsChecked` at
`0x20471360` (reads member `+0xbc`, no stack arguments) and `SetChecked` at
`0x20474440` (writes member `+0xbc`, invalidates the control, `ret 4`). The
native harness uses these methods on the six embedded controls instead of
sampling standard Win32 checkbox messages.

### Inherited validation

The settings class inherits `CSimsDialog::OnCommand` through vtable slot
`+0x80` (MP_DIALOG thunk `0x223461e4`, SIMSUI callee `0x204764c0`). On a
`WM_COMMAND` whose notification code is `EN_KILLFOCUS` (`HIWORD(wParam) ==
0x0200`), that handler calls virtual slot `+0xd8` with the control ID. The
settings vtable leaves that slot on the imported
`CSimsDialog::ValidateData` (`0x223461de` -> `0x20471610`); the retail default
returns zero (`xor eax,eax; ret 4`). A nonzero override result would enter
SIMSUI's `0x5f4` validation notification and failure-ID bookkeeping at
`0x20476960`. MP_DIALOG does not override
`ValidateData`, and template `0x6a` has no edit or numeric controls, so its
ordinary controls do not produce this edit-focus validation event.

**Static finding:** the OK path does not call `Validate` or `ValidateData`: its
local handler runs the save helper and owner callback before virtual close.
Thus this dialog has no field validation that can veto OK in the recovered
path.

**Native probe:** the harness sends a `WM_COMMAND` with notification
`EN_KILLFOCUS` for control `0x419` after wrapping the settings object's `+0xd8`
slot with a counting trampoline. Retail and replacement each reach the
inherited slot once; the probe returns zero, and neither path shows an error or
blocks OK. This confirms the dispatch/slot path with real MFC/SIMSUI windows;
it does not exercise nonzero validation overrides, which this class does not
provide.

### Callback ABI and command transitions

The parameter block is the block supplied by MULTIP public `+0x1c`
(`.text+0x39a0`, host call at `0x208549cb`) from MULTIP data `0x2085d1e0`.
Its first dword is the dialog result;
the owner initializes it to zero. Callback pointers and the direct UI call
sites are:

| Block offset | UI call site / action | MULTIP callee | Verified ABI |
| --- | --- | --- | --- |
| `+0x04` | No call in the `0x110b` path; shared adapter `0x223417f0` is used by another message-map class | `.text+0x3b70`, `0x20854b70` (`multip_options_read`) | one `void *` argument, stdcall, `ret 4` |
| `+0x08` | `OnOK`, `0x22343d90` | `.text+0x3a40`, `0x20854a40` (`multip_options_accept`) | one `void *` argument, stdcall, `ret 4` |
| `+0x0c` | Host Settings handler, `0x22343d60` | `.text+0x3ae0`, `0x20854ae0` (`multip_options_details`) | one `void *` argument, stdcall, `ret 4` |
| `+0x10` | Get Flight Conditions handler, `0x22343d70` | `.text+0x3ac0`, `0x20854ac0` (`multip_options_weather`) | one `void *` argument, stdcall, `ret 4` |
| `+0x14` | Set Flight Conditions handler, `0x22343d80` | `.text+0x3ad0`, `0x20854ad0` (`multip_options_sync`) | one `void *` argument, stdcall, `ret 4` |

Each local button wrapper pushes exactly the parameter-block pointer before
calling its owner callback. The MULTIP callees' `ret 4` instructions confirm
that single-argument prototype. The `+0x14` x86 callback is a naked wrapper; it
pushes the zero argument required by message `0x1014` and preserves the retail
caller-stack tail. The `+0x10` callback invokes message `0x1013`. Host Settings
enters the separate `0x110c` details dialog through MULTIP; its descriptor is
freed only after that child succeeds, while earlier errors return without
freeing it.

`multip_options_read` is a one-pointer stdcall callback: the callee at
`0x20854b70` ends in `ret 4`. The only MP_DIALOG adapter that reads a callback
at block `+0x04` is `0x223417f0`: it pushes the same block pointer, calls
`[block+4]`, and converts zero/nonzero to zero/`-1`. Its only direct caller is
`0x22341b84`, inside handler `0x22341b30` and message-map getter `0x22341af0`
(commands `0x3e8`/`0x433`); that is not the settings class's map
(`0x22343d50`). In that other handler, the call is reached only when its record
field `+0x24` is zero; the handler first stores the current HWND in record
`+0x14`. The callback's booleanized result selects the next branch. The
`0x110b` factory/init, local controls and commands, the
owner's `multip_options_dialog` (`0x208549b0`, host call `0x208549cb`), and the
host modal loop do not call or inspect this slot. The owner initializes the
settings values before opening the dialog via UTIL `+0x90`. Therefore the
`+0x04` callback is present in the `0x110b` block but is not invoked on the
recovered `0x110b` path; the other class's adapter call does not establish an
invocation of this `multip_options_read` instance.

**Native callback probe:** the harness supplies five stdcall one-pointer
MULTIP-shaped callbacks. Each records the received block pointer and result
word. Retail and replacement pass the exact owner block at every observed
callback. Initialization and the settings command map do not invoke `+0x04`;
the three button handlers call `+0x0c/+0x10/+0x14` in order, OK calls `+0x08`
after setting result `1`, and Cancel calls no owner callback and leaves result
`0`. The harness also varies the three button callback returns; the dialog
continues to close in both binaries.

On OK, `0x22343d90` calls save helper `0x223442a0`, sets the result dword to
`1`, calls the owner callback at `+0x08`, then calls virtual slot `+0xec`. The
callback's return is ignored. The slot thunk `0x223461c0` resolves to
SIMSUI `CSimsDialog::Close` at `0x204751e0`, not MFC `CDialog::OnOK`. MULTIP's
accept callback commits the eight exchange records and, when connected,
publishes the local player name and updates player/observer membership; it can
return `-1`, which the UI handler ignores and still closes. Cancel at
`0x22343dc0` writes result `0` and calls the same virtual close slot without
calling accept.

`CSimsDialog::Close` writes `1` to `GWL_USERDATA` (`-21`) for the dialog HWND
through `SetWindowLongA` at `0x2049294c`, then hides the window through
`CWnd::ShowWindow(0)` (MFC42 ordinal `6215`). The flags-zero modal host loop at
`0x00409720` notices the nonzero `GWL_USERDATA`; if the HWND is still alive it
calls `DestroyWindow` at `0x00409b19`. The modal result is read from the
parameter block after the host returns, not from the factory or dialog-call
return value.

For the UI probe, HOST allocation/reallocation/free and module
registration/release are modeled at `+0x20c/+0x210/+0x214` and
`+0x298/+0x29c`. After OK or Cancel hides the window and sets
`GWL_USERDATA`, the harness applies a forwarding WndProc hook that counts
`WM_NCDESTROY`, and wraps class vtable slots `+0xac` and `+0x04` while calling
their original targets. On each observed `DestroyWindow`, the order is
`WM_NCDESTROY` → `PostNcDestroy` → deleting destructor, once each; the
destructor receives flag `1`. This verifies the MFC message-dispatch path in
retail and the replacement. The host loop's decision to call `DestroyWindow`
at `0x00409b19` is static retail evidence; the isolated harness models that
final host action by calling `DestroyWindow` after the dialog is hidden.

### Destruction

MP_DIALOG's class vtable is at `0x223488e8`. Its `+0xac` slot is local
`PostNcDestroy` wrapper `0x22344820`: for a nonnull object it calls vtable
`+0x04` with deleting flag `1`. That slot is the class deleting destructor
`0x22343aa0`. Its body at `0x22343ac0` destroys owned controls in reverse
construction order, calls the imported SIMSUI `CSimsDialog` destructor
(`0x2234b488` IAT call at `0x22343c0c`), then calls SIMSUI `ui_delete`
(`0x2234b46c`) when the deleting flag is set.

The slot identity is independently visible in SIMSUI: `CSimsTabPage` exports
`PostNcDestroy` at `0x2047f4f0`, its vtable at `0x20487060` places that method
at `+0xac`, and its body also calls deleting destructor slot `+0x04` with flag
`1`. The `CSimsDialog` base vtable at `0x20485ef0` has a thunk at `+0xac`
(`0x20482d64`) to MFC42 ordinal `5241`; the available retail MFC42 binary's
implementation at `0x5f415500` is a bare `ret`, the default no-op. MP_DIALOG
overrides that default with its deleting `PostNcDestroy` wrapper.

**Verified:** `WM_NCDESTROY` reaches the class `PostNcDestroy` override and then
the deleting destructor in the runtime probe. The class methods and addresses
are `PostNcDestroy` wrapper `0x22344820` at vtable `+0xac`, deleting destructor
`0x22343aa0` at `+0x04`, and destructor body `0x22343ac0`. The host loop's
decision to call `DestroyWindow` at `0x00409b19` remains static evidence; the
factory itself has no separate post-modal cleanup path.

### Native differential and current limit

Build and run the standalone slice with `make -f tools/mpdialog.mk mpdialog`
and `bash tools/run_mpdialogdiff.sh --ui`. The harness loads the retail
`orig/English/mpdialog.dll` and replacement side by side with retail MFC42,
SIMSUI and GLOBAL binaries. It models only HOST registration/memory services
and MULTIP owner callbacks; it does not load or alter MAIN, HOST or MULTIP.
Twelve synthetic owner-block patterns run through creation, initial control
capture, focus-loss, three button commands, checkbox changes, OK or Cancel,
hidden-window state, `WM_NCDESTROY` teardown and unload/reload. Two module
load/unload cycles produce 24 paired scripts.

**Verified matches:** callback order/arguments/block-result snapshots, the
unused `+0x04` callback, focus-loss validation count, checkbox snapshots, saved
descriptor, OK/Cancel result, close state,
`WM_NCDESTROY`/`PostNcDestroy`/deleting-destructor counts and order, HOST
registration/release, and repeated DLL unload match in all 24 paired scripts.

**Native checkpoints:** the harness installs a thread-local
`WH_CBT/HCBT_CREATEWND` probe and records each control's `CREATESTRUCT.style`
and parent visibility. The two styles match retail and replacement at
creation: `WS_VISIBLE` is set while the parent dialog is not yet visible.
After the factory returns, it records each child's full `GWL_STYLE`, the
`WS_VISIBLE` bit, `IsWindowVisible(child)`, and `IsWindowVisible(parent)`;
`IsWindowVisible` is therefore compared separately from the style bit. At
this checkpoint the parent is visible, so the effective child visibility
agrees with the child's `WS_VISIBLE` bit. It also records the descriptor's
`+0x18/+0x1c` values and each embedded control's HWND ID. Before the fix,
creation style, parent visibility and member-to-ID mapping matched, but the
post-init `WS_VISIBLE` bit and effective visibility were reversed for IDs
`1003`/`1006` in cases 2, 3, 6, 7, 10 and 11. The descriptor words were
unchanged through initialization. A direct call through vtable slot `+0xf0`
left each side at its post-init state, placing the cause in the local update
branch rather than DDX/control identity or inherited parent visibility. For
the two branch-driving cases, retail and the fixed replacement report:

| Probe case | Descriptor `+0x18/+0x1c` | Create `1003/1006` WS_VISIBLE, parent visible | After init `1003/1006` (`WS_VISIBLE`/`IsWindowVisible`), parent visible | After direct update |
| --- | --- | --- | --- | --- |
| 2 | `1/0` | `1/1`, `0` | `1003=1/1; 1006=0/0`, `1` | unchanged |
| 3 | `1/1` | `1/1`, `0` | `1003=0/0; 1006=1/1`, `1` | unchanged |

**Verified after the fix:** the replacement now performs the second pair of calls from retail
`0x22344251..0x22344291`. The harness reports resource, post-init and
post-direct-update visibility checkpoints separately; all 24 baseline pairs
match for creation styles, parent visibility, `WS_VISIBLE`, effective
visibility and member HWND IDs. The full native comparison covers twelve
synthetic owner-block patterns, three button commands, checkbox changes,
focus-loss, OK/Cancel, teardown and two module load/unload cycles: 24 paired
scripts, zero differences. This is evidence for those exercised states, not
proof of every possible owner descriptor. The current 0x110a baseline also
passes; the actual-owner boundary is summarized in the whole-module audit.

Run `bash tools/run_mpdialogdiff.sh --ui-breaks` for deliberate mutations. Its
case-0 baseline matches retail over two unload cycles. Mutations that expose
the wrong result word to the accept callback and swap the Get/Set Flight
Conditions handlers produce callback-group differences; inverting the DME
initial-state test produces a checkbox-group difference. A visibility mutant
that omits the final `+0x18 != 0` show/hide pair is run against case 2 and is
caught in the visibility comparison. A rendered-callsign mutant is caught by
the separate page-pixel hash comparison.

The owner-side code is already implemented and verified:

- `src/multip/multip_options.inc`: `0x110b`, public `+0x1c`.
- `src/multip/multip_details.inc`: `0x110c`, refresh/removal/capacity callbacks.
- `src/multip/multip_configuration.inc` and `multip_browser.inc`: `0x110a`
  setup, validation, providers, session discovery and related callbacks.
- `src/multip/multip_lobby.inc` and `multip_toggle.inc`: connection orchestration.
- `tools/difftest/multip_dialogs.inc`, especially `dialog_engine`, and
  `tools/difftest/multip_lobby.inc`: existing callback/COM/clock/file fixtures.

See [MULTIP's lobby/dialog checkpoint](multip.md#lobby-and-configuration-dialog-checkpoint)
and its later [acceptance checkpoint](multip.md#whole-module-acceptance-checkpoint).
Earlier MULTIP checkpoints retain historical pending states; use the latest
acceptance and source for current completion status.

The MULTIP tests establish owner callback behavior. The isolated UI harness
differentially covers retail and replacement `0x110a`, `0x110b` and `0x110c`
windows; it still models HOST array dispatch/idle messages and MULTIP callbacks.
The separate private-copy probe below exercised the actual HOST/MULTIP menu
entry for 0x110a and the actual settings entry for 0x110b. It did not replace
the differential harness or establish every owner-side callback/storage path.
The previous 0x110a differential failures, their fixes, and these boundaries
are listed below.

## Session details (`0x110c`) recovered contract

The contract was recovered from retail and implemented in the isolated slice;
the native differential and its coverage limits are recorded after the
contract. Preferred VAs are for
`orig/English/mpdialog.dll`
(`0x22340000`), `orig/Modules/multip.dll` (`0x20850000`), `orig/SIMSUI.DLL`
(`0x20470000`) and the CD `MFC42.DLL` (`0x5f400000`). Resource facts were
regenerated from the retail PE with `tools/dialogmap.py`; the function,
message-map, vtable and caller facts below come from disassembly and the
MULTIP callback-pointer table. `ret N` is used to verify callback stack
cleanup, not just to count pushes at an indirect call.

### Factory, resource and controls

The exported MP_DIALOG `Linkage` at `0x2234a060` points to the dialog table at
`0x2234a018`; its `0x110c` row starts at `0x2234a030` and has flags 0 at
`0x2234a038`. That row selects factory `0x22341270`, which allocates
`0x1d80` bytes with `ui_new` (`0x2234b4bc`), calls constructor `0x22344380`,
stores its descriptor at object `+0x1d78`, copies the MP_DIALOG instance
handle to `+0x80`, creates resource `0x6b` through MFC ordinal 2086 at
`0x2234621a`, shows the dialog through `0x22346214`, and returns HWND at
object `+0x20`. The factory returns with `ret 8` at `0x223412f8`. It passes a
null parent to the base constructor; its parent-HWND argument is not used by
this path.

The local constructor calls `CSimsDialog` with template `0x6b`, constructs the
dialog's owned controls, and installs class vtable `0x22348b60` at
`0x223444ea`. `DoDataExchange` is `0x223446a0` and calls the base exchange
before the local bindings. Resource `0x6b` is an extended 485-by-401 dialog
titled “Session Details”; dialog style is `0x84000440`, extended style
`0x10000`. Its child-control resource records initially carry `WS_VISIBLE`.
IDs `1002` (Close) and `2` (Cancel) share one rectangle; initialization
chooses which one to show. The local DDX map binds the key controls as follows:

| Resource control | Object member | Use |
| --- | --- | --- |
| `1` | `+0x12d4` | OK |
| `2` | `+0x10dc` | Cancel |
| `1002` | `+0x11d8` | Close, same command handler as Cancel |
| `1007` | `+0x13d0` | Remove selected player |
| `1029` | `+0x1844` | Session-name edit |
| `1030` | `+0x1964` | Player list |
| `1035`, `1036` | `+0x16bc`, `+0x14cc` | Pilot-count edit and spin control |
| `1037`, `1038` | `+0x1724`, `+0x150c` | Observer-count edit and spin control |
| `1068`, `1069` | `+0x19ac`, `+0x1a64` | Current pilot and observer count text |

The complete DDX call sequence, including the remaining labels and title art,
is at `0x223446a0..0x22344801`. The message map returned by `0x22344810`
shares the `CSimsDialog` base map and maps `WM_COMMAND` ID 1 to OK
(`0x22344850`), IDs 2 and 1002 to Cancel (`0x223448a0`), ID 1007 to Remove
(`0x22344830`), listbox ID 1030 / notification 1 to selection update
(`0x22344fd0`), and message `0x464` to refresh (`0x22344fa0`). It also maps
`WM_HELP` and `WM_CONTEXTMENU` to `0x22344f60` and `0x22344f80`.

The provider descriptor passed by MULTIP is distinct from the inner `0x110c`
dialog block. The inner block begins at MULTIP data `0x2085d588`; its recovered
layout is:

| Inner-block offset | Meaning established by use |
| --- | --- |
| `+0x00` | Result word: zero on cancel, set to one before accept callback |
| `+0x04` | Write/parse validation callback |
| `+0x08` | Accept callback |
| `+0x0c` | Remove-confirmation callback |
| `+0x10` | Refresh callback |
| `+0x14` | Display mode copied from provider descriptor `+0` |
| `+0x18` | Session-name text pointer copied from provider descriptor `+4` |
| `+0x1c`, `+0x30` | Pilot/observer parse status words written by validation callback |
| `+0x20`, `+0x34` | Pilot/observer edit text buffers |
| `+0x24`, `+0x38` | Corresponding edit-buffer lengths |
| `+0x28`, `+0x2c` | Strings converted to the pilot spin range's low/high words |
| `+0x3c`, `+0x40` | Strings converted to the observer spin range's low/high words |
| `+0x44`, `+0x48` | Current pilot/observer count display strings |
| `+0x4c`, `+0x50` | Player-row array pointer and row count |
| `+0x58` | Current list selection index |
| `+0xbc` | Opaque word copied from provider `+0x0c`; no reader found |
| `+0xd0` | Opaque word copied from provider `+0x10`; no reader found |

The MULTIP provider descriptor has a four-argument player-provider callback at
`+0x14`; its `+0x08` is the callback context, and `+0x0c/+0x10` are copied to
the opaque inner-block fields above. Retail MULTIP's options-details provider
at `0x20854ae0` supplies the session name and caller context in descriptor
`0x2085d330`; its static `+0x00` mode word is 1. The browser-details provider
at `0x20855620` fills the same fields in descriptor `0x2085d520`, whose static
mode word is 0. These values select editable and viewer behavior respectively.
Calling them “host” and “visitor” modes is an interpretation of the owner
contexts, not a proven flag name.

### Initialization, inherited validation and callback ABI

`OnInitDialog` at `0x223448c0` calls `CSimsDialog::OnInitDialog` through
`0x2234b448`, then `CSimsDialog::Init` through vtable slot `+0xe4` with
resource/module arguments at `0x223448d5..0x223448e5`. It initializes the
text, edit, spin, button and list controls, applies mode-specific visibility
and enable state, and calls vtable slot `+0xf8` (`0x22344d10`) once to build
the player list. It marks initialization complete at object `+0x1d7c` and
focuses Cancel in editable mode or Close in viewer mode (`0x22344cdc..0x22344cfc`).
When inner mode `+0x14` is zero, OK and Cancel are hidden, Close is shown, and
both capacity edits and spin controls are disabled. When it is nonzero, OK and
Cancel are shown, Close is hidden, and both capacity pairs are enabled. Remove
starts disabled and is enabled only by the list-state rules below.

The validation path is inherited dispatch with a local virtual override.
The class inherits `CSimsDialog::OnCommand` in vtable slot `+0x80`
(`0x223461e4` -> SIMSUI `0x204764c0`). For an `EN_KILLFOCUS` notification
(`HIWORD(wParam)==0x0200`), SIMSUI's base handler calls virtual `+0xd8` with
the control ID (`0x20476706..0x20476731`). `0x110c` overrides that slot with
`0x223450c0`. If the dialog HWND is visible, this method first calls local
slot `+0xfc` (`0x22344f00`) to capture the selected index and both edit
strings, then calls inner-block callback `+0x04` with the block pointer
(`0x223450d5..0x223450e6`). It returns focus target `0x40b` (pilot edit ID
1035) or `0x40d` (observer edit ID 1037) when callback `+0x04` returns nonzero
and the corresponding status word is set. It checks the pilot status first,
then the observer status. The inherited handler routes a nonzero result
through SIMSUI's error and focus bookkeeping. If the dialog is not visible,
this override exits before either local update or callback.

OK at `0x22344850` captures the edit/list state through `+0xfc`, calls
`+0xd8(0)` and then local `+0xdc` (`0x22345020`) to process the validation
result. An error result prevents acceptance and close. On success it stores
1 at block `+0`, invokes callback `+0x08`, ignores that callback's return, and
calls `CSimsDialog::Close` through `+0xec`. Cancel at `0x223448a0` stores 0
at block `+0` and closes without calling callback `+0x08`. Remove first calls
`+0xfc` to capture selection, then calls `+0x0c`; it does not close the
window. Focus movement can, of course, have already caused an inherited
focus-loss validation before the Cancel command arrives.

The callback slots and callee-cleanup evidence are:

| Inner slot | UI call site | MULTIP retail callee | Verified prototype |
| --- | --- | --- | --- |
| `+0x04` | `0x223450e6`; also direct owner initialization call `0x20858bfc` | `0x20858e60` (`.text+0x7e60`) | `int32_t stdcall (void *)`, `ret 4` |
| `+0x08` | `0x22344886` after result becomes 1 | `0x20858c10` (`.text+0x7c10`) | `int32_t stdcall (void *)`, `ret 4` |
| `+0x0c` | `0x22344842`, after list/edit capture | `0x20858c80` (`.text+0x7c80`) | `int32_t stdcall (void *)`, `ret 4` |
| `+0x10` | `0x22344fb8` for visible-window message `0x464` | `0x20858dc0` (`.text+0x7dc0`) | `int32_t stdcall (void *)`, `ret 4` |
| Provider `+0x14` | MULTIP `0x20858baf` at initial enumeration; `0x20858e3b` on refresh | options `0x20854b90`, browser `0x208556a0` | `int32_t stdcall (void **, uint32_t *, void *, void *)`, `ret 16` |

The four inner callback addresses are also fixed by the retail MULTIP pointer
table: data `0x2085d58c..0x2085d598` points to `.text+0x7e60`, `+0x7c10`,
`+0x7c80` and `+0x7dc0`. Both four-argument provider implementations end in
`ret 16`; they return zero for a successful player copy. These are retail
binary ABI facts, not prototypes inferred from the current C source.

Callback `+0x04` has three relevant invocation contexts. MULTIP's
`multip_details_init` at `0x20858b60` directly invokes the write/parse callback
at `0x20858bfc` after initial data read and numeric formatting, before it
calls HOST to create `0x110c`. After creation, focus loss invokes it through
the inherited `OnCommand` path above. OK invokes it once through `+0xd8` after
capturing the edit fields; Cancel and Remove do not call `+0x04` themselves.
The OK handler calls `+0xfc` twice (once directly and once from `+0xd8`), but
the owner callback runs only in the latter validation call. The callback
updates both status words and returns a nonzero result for its error paths;
the UI selects a focus target from the per-field status words, so a callback
failure with neither status word set does not produce a field target in the
local validator. Do not turn that into a general validation veto without
matching retail.

### Player lists, refresh and removal

Each player row is 16 bytes: player ID at row `+0`, a flag checked by the
removal gate at `+8`, and a name pointer at `+0x0c`. The descriptor stores the array
and count at `+0x4c/+0x50`; selected list index is `+0x58`. The refresh/update
routine `0x22344d10` saves the old listbox index, sends `LB_RESETCONTENT`
(`0x184`), adds each row name with `LB_ADDSTRING` (`0x180`), restores the old
index with `LB_SETCURSEL` (`0x186`) and stores the resulting index back at
`+0x58`. Selection is preserved by index, not by player ID. With no rows it
sets selection to `-1` and disables Remove. It also updates the session name,
current-count labels, capacity text/ranges and selected-player display.

After first initialization, the object flag at `+0x1d7c` prevents an editable
mode refresh from replacing capacity text the user may have changed. Viewer
mode continues to copy those text buffers on refresh. The two spin ranges are
built by converting descriptor strings `+0x28/+0x2c` and `+0x3c/+0x40` with
MSVCRT `atoi` (`0x2234b3f4`), packing the second string into the low word and
the first into the high word, then sending `UDM_SETRANGE` message `0x465` to
spin HWNDs at object `+0x14ec` and `+0x152c`
(`0x22344e4d..0x22344ec7`). Thus `+0x2c`/`+0x40` supply the packed low word
and `+0x28`/`+0x3c` the packed high word. That word order is direct binary
evidence; describing the words as limits follows the message's packed format.
The edit text capture in `+0xfc` calls MFC ordinal 3873 through thunk
`0x22346370`; the CD MFC callee at `0x5f407f8c` forwards its two arguments to
`GetWindowTextA` and ends in `ret 8`. This verifies the buffer/length roles of
`+0x20/+0x24` and `+0x34/+0x38`.

The `0x464` handler at `0x22344fa0` first calls `IsWindowVisible` on the
dialog HWND. If false it returns without asking MULTIP to refresh. Otherwise
it calls callback `+0x10`; a nonzero result runs `+0xf8` to rebuild the list
and text. The owner callback at `0x20858dc0` does not refresh if the provider's
`+0x14` is null or the unsigned elapsed tick count is at most
`M32(0x178)*1000`. When due, it updates the tick marker, frees the previous
player array, asks the four-argument provider for a replacement list and
count, then runs MULTIP's detail-data read. It returns 1 when the UI should
rebuild, 0 for no update/provider failure, and -1 on its explicit free failure
path. Since the UI tests for nonzero, that -1 result also takes the rebuild
branch with the callback's current block contents.

**Who sends `0x464`, and when:** MP_DIALOG's registry entry for `0x110c` has
flags 0 (`0x2234a030..0x2234a038`), so HOST `+0x10c` at `0x00409720` runs it
as a single modal window. In HOST's single-window pump, after the current
message has been offered to accelerator handling, the modeless-dialog filter
and `IsDialogMessageA`, the idle path checks the modal HWND is still alive and
not done. If the queue has no message, it calls `SendMessageA` synchronously
at `0x00409af4` with `(HWND, 0x464, 0, 0)`; the call is through HOST IAT slot
`0x41d980`. It then raises host event `0x20006` at `0x00409afa` and repeats
the pump. This is an idle-pump tick, not a timer or a queued `PostMessage`.
The same modal routine sends `0x464` to each HWND in a modal array in its array
idle path (arguments at `0x004099d5..0x004099d9`, call at `0x004099df`), which
does not apply to this single-HWND factory.

HOST `+0x114` at `0x00409bf0` is a separate modeless-list service. Its callback
`0x00409d80` sends the same `(HWND, 0x464, 0, 0)` synchronously at
`0x00409dc5..0x00409dcb` for each still-live modeless record. The per-message
modeless callback `0x00409ca0` has a distinct send at
`0x00409ce5..0x00409ceb`. `0x110c` is not registered as modeless, so neither
modeless-list route is its regular scheduler. During message dispatch HOST may
tick dialogs in the modeless list through `0x00409ca0`; this does not turn the
modal `0x110c` into a modeless record. All four direct `0x464` call sites in the
HOST image use `SendMessageA` through IAT slot `0x41d980`; no `PostMessageA`
producer appears in those paths.

The handler does not forward `wParam` or `lParam` to MULTIP. It first checks
effective visibility with `IsWindowVisible(dialog HWND)` at
`0x22344fa3..0x22344faf`; only then it calls the inner block's `+0x10`
function at `0x22344fb8`, passing the block pointer (`object+0x1d78`) as its
only argument. The callback ABI is `int32_t stdcall (void *)`, confirmed by
MULTIP callee `0x20858dc0` ending in `ret 4`. In that owner callback the
provider's `+0x14` function is called with `players_out=block+0x4c`,
`count_out=block+0x50`, third argument `0x2085de78`, and fourth argument from
provider descriptor `+0x08`. Its stdcall cleanup is `ret 16`; the UI then
consumes the updated list synchronously before the next message dispatch. The
dialog callback itself has `ret 4` and does not retain a second copy of the
block pointer.

The normal owner path makes this pointer lifetime concrete. MULTIP
`multip_details_dialog` at `0x20858b10` sets global description pointer
`0x2085de88`, initializes the module-static block at `0x2085d588`, and calls
HOST `+0x10c` at `0x20858b28..0x20858b38`. The host pumps and destroys the
modal HWND before returning; only then does MULTIP call
`multip_details_clear` at `0x20858b4d`. The UI object stores the block pointer
at `object+0x1d78` and borrows it while alive. The callback can replace the
player array; its new rows and name pointers are consumed by the UI's immediate
post-callback update before the synchronous message returns. After HOST has
destroyed the window and returned, the owner clears the current row names and
array. The UI destructor does not own this storage and must not free it.

### Opaque copied words at `+0xbc/+0xd0`

These names remain neutral because the consumer-side meaning is not
established. The retail MULTIP initialization at `0x20858b8a..0x20858b96`
copies provider descriptor `+0x0c` to the inner block's `+0xbc` and provider
`+0x10` to inner `+0xd0`. Because the inner block is the static object at
`0x2085d588`, the stores land at MULTIP data addresses `0x2085d644` and
`0x2085d658` respectively.

| Inner offset / data VA | Retail write | Source values in current callers | Reads and observable effect |
| --- | --- | --- | --- |
| `+0xbc` / `0x2085d644` | `0x20858b8a..0x20858b8d` | Options descriptor `0x2085d330+0x0c` is the address `0x2085d998`; browser descriptor `0x2085d520+0x0c` is zero | No load/reference found in MP_DIALOG or MULTIP after the initialization store; runtime sentinel was unchanged by create, update and destroy |
| `+0xd0` / `0x2085d658` | `0x20858b93..0x20858b96` | Options descriptor `0x2085d330+0x10` is the address `0x2085d99c`; browser descriptor `0x2085d520+0x10` is zero | No load/reference found in MP_DIALOG or MULTIP after the initialization store; runtime sentinel was unchanged by create, update and destroy |

The options owner at `0x20854ae0` sets descriptor `+4/+8` to the session-name
pointer and owner context before entering the dialog; its descriptor's two
opaque words point at MULTIP capacity dwords `0x2085d998/0x2085d99c`. The
browser owner at `0x20855620` sets the same name/context slots; its `+0x0c`
and `+0x10` descriptor dwords are zero in retail data. These source values
are binary facts; interpreting the options pointers as intended capacity
handles is plausible from their targets and the editable mode, but their
consumer-side role is unproven. Keep the two copied fields opaque and preserve
their values without dereferencing them.

An address cross-reference over the retail images found no MP_DIALOG operand
that accesses the session block at `+0xbc` or `+0xd0`. Its only `+0xd0`
operand is the unrelated virtual-dispatch thunk at `0x22345132`, which reads
`vtable+0xd0`; it is not a descriptor access. In retail MULTIP, the only code
references to `0x2085d644/0x2085d658` are the stores above. The block's
`+0xbc/+0xd0` contents therefore have no demonstrated effect on this dialog's
rendering, refresh, callbacks or lifetime.

The selection-change handler `0x22344fd0` queries `LB_GETCURSEL` (`0x188`),
reads selected row `+8`, and disables Remove if that word is nonzero. The
refresh routine only enables Remove when mode `+0x14` is nonzero and there are
more than one player; it then applies the same selected-row test. Thus a
viewer cannot remove players, the last player cannot be removed, and a row
with a nonzero eligibility word is not removable.

The Remove callback at `0x20858c80` obtains localized format string `0xf53b`,
formats it with the selected row's name, and calls UTIL `+0xa4` for a
confirmation. If the answer has bit `0x40`, it sends MULTIP message `1016`
with the selected row's player ID at `+0`. It then frees the formatted
messages and stores `ticks()+2000` in its refresh marker. Static code shows
that the next refresh test subtracts this marker as an unsigned value before
comparing it to the interval; near wraparound this subtraction can make the
refresh immediately due. This is a reproduced arithmetic consequence, not an
inference that the delay was intended to work that way. The remove callback's
callee ends in `ret 4`; on the send-failure early return, the already-created
confirmation strings are not freed.

### Accept, ownership and lifetime

The owner accept callback at `0x20858c10` commits its exchange records. Only
when the inner mode equals 1 does it also write the capacity exchange records
and call MULTIP session-capacity update (`multip_session_set_capacity`). The
UI sets result `+0` to 1 before invoking this callback. Cancel sets it to 0
and does not invoke accept. The `multip_details_dialog` owner at `0x20858b10`
initializes the block, calls HOST `+0x10c` with id `0x110c` and block
`0x2085d588`, then clears the player copy after a successful host dialog
return. If HOST reports failure, the owner returns before that cleanup.
`multip_details_clear` at `0x20859010` frees each row name in reverse count
order and then frees the row array. The MP_DIALOG destructor owns its C++
controls and class object; it does not own this MULTIP player allocation or
provider descriptor.

Class vtable `0x22348b60` points `+0xac` to `PostNcDestroy` wrapper
`0x22344820`; that wrapper calls vtable `+0x04` with deleting flag 1. The
deleting destructor wrapper is `0x22344510`, its body is `0x22344530`, and
the body destroys controls in reverse construction order, calls the SIMSUI
base destructor, then calls `ui_delete` when the deleting bit is set. The
SIMSUI `CSimsDialog` base vtable supplies the same `+0xac` framework slot;
MP_DIALOG overrides it with the deleting wrapper.

**Teardown path:** HOST's modal exit at `0x00409b18..0x00409b19` calls
`DestroyWindow` if the HWND remains alive. Windows delivers `WM_NCDESTROY` to
the MFC window procedure; SIMSUI's base destruction path then dispatches
virtual slot `+0xac`. The 0x110c vtable `0x22348b60` overrides that slot with
`PostNcDestroy` wrapper `0x22344820`; the wrapper calls vtable slot `+0x04`
with deleting flag 1. That resolves to deleting-destructor wrapper
`0x22344510`, body `0x22344530`, which destroys owned controls in reverse
construction order, calls the SIMSUI base destructor, and calls `ui_delete`
when the deleting bit is set.

**Isolated retail runtime probe:** a temporary native probe outside the
tracked tree loaded retail MP_DIALOG with the retail CD MFC42 and SIMSUI,
modeled the needed HOST allocation/registration services, and created factory
`0x110c` with a synthetic inner block and synchronous callback. It sent
`0x464` directly to the visible HWND and instrumented the window procedure,
refresh callback, `PostNcDestroy` vtable slot and deleting-destructor slot.
The observed message parameters were `wParam=0,lParam=0`; callback `+0x10`
ran once with the exact inner-block pointer. `DestroyWindow` then produced
one `WM_NCDESTROY`, one `PostNcDestroy`, and one deleting-destructor call, in
that order. The probe made the refresh callback return 1, so the dialog also
ran its list/text update routine. It set distinct sentinels at block
`+0xbc/+0xd0` and observed both unchanged after creation, message dispatch,
the update and teardown. This proves the retail class's dispatch and
destruction path under real MFC/SIMSUI;
the probe did not run the complete retail HOST modal pump or MULTIP owner, so
the scheduler and owner cleanup ordering above are static binary findings.

**Evidence and remaining uncertainty:** factory, class vtable, HOST send
sites, MULTIP owner calls, field stores, callback ABIs, and destructor calls
are statically verified in the retail PE images. The targeted native probe
verified actual 0x110c callback payload shape and `WM_NCDESTROY` teardown.
Provider-mode labels such as “editable” and “viewer” describe observed branch
effects, not proven retail names. The purpose of `+0xbc/+0xd0` is still
unknown; their copy behavior and lack of consumers are established, so this
does not block faithful implementation if they remain opaque. No unresolved
behavior in the requested refresh, field-copy or teardown contract blocks
the implemented 0x110c slice. Whole-module acceptance remains incomplete.

### Isolated implementation and native differential

`src/mpdialog/mpdialog_flow.c` implements the 0x110a five-page factory,
borrowed descriptor, posted initial Next, page transitions, page controls,
callback routing, focus validation, Browser refresh/selection and close
behavior. The five native resources plus their private support resources are
generated under ignored `bin/mpdialog-diff/`. The runtime maps retain separate
per-page vtables and message maps.

The current native differential reports **24 paired 0x110a scripts over two
DLL load/unload cycles, zero differing scripts**. It covers the five HWND
array, resource/control setup, initial posted command, forward/back/cancel
transitions, provider failure/retry, Browser refresh/selection/details/
settings/observer/join, host settings, validation failure and recovery,
accept/cancel, and teardown. The comparison retains its behavior groups; no
expected behavior or comparison was removed to reach this result.

#### Resolved differential groups and evidence

The earlier seven failures were two groups: one case-0 validation/callback
pairing artifact, plus the same Browser paths (cases 3, 4 and 10) repeated on
each of two load/unload cycles.

**Case 0 — visible and hidden Choice focus loss (runtime verified).** The
factory-posted Intro Next is dispatched only after the modeled HOST show. That
activation transfers focus to Choice's callsign edit `0x419`; the synchronous
`WM_COMMAND` is `wParam=0x02000419`, with the edit HWND in `lParam`. Choice is
visible (`WS_VISIBLE` and `IsWindowVisible` are both true), pending field
`+0x7d0` is zero, and inherited `OnCommand` reaches Choice `ValidateData`
(`0x22345ab0` retail; `0x01552480` in this replacement build). The owner
validation callback `+0x10` runs once on each side. The prior retail-only
count came from comparing sequential runs under a shared activated host: the
second run inherited focus/activation events. Each side now uses a fresh
independent STATIC owner window and resets focus before factory creation. With
that paired parent state, retail and replacement both produce the same
visible focus-loss callback.

Destroying array entry 0 later moves focus from the Choice edit back to the
owner and produces another `EN_KILLFOCUS` for ID `0x419`. By then Choice is
hidden (`WS_VISIBLE` cleared and `IsWindowVisible=0`). `+0x7d0` remains zero;
`+0x7d4` still contains the edit HWND but is not a pending-validation flag.
The validator is entered and returns zero, but Choice's visibility check
prevents another `+0x10` owner callback. No `0x5f4` validation message is
pending. Thus the apparent teardown-only owner callback was actually the
earlier visible focus handoff; teardown reaches the validator but does not
call the owner. These observations match on both binaries.

**Browser paths — one repeated handler family (runtime and static evidence).**
Cases 3, 4 and 10 select a provider and exercise Browser navigation or
cancellation; the repeated rows were not six separate transition bugs. The
`0x41b/code=2` provider notification is synchronous and reaches retail thunk
`0x22341c10`, then Connection vtable `+0xcc` target `0x22341c00`, which tail-jumps to
handler `0x22341b30`. That handler
calls mode-0 callback `+0x04` through adapter `0x223417f0` at call site
`0x22341b84`. A zero result advances state 3 to Browser state 4 immediately
through helpers `0x22341620/0x223417b0`; it is not deferred to HOST's later
idle dispatch. Case 4's provider-notification callback returns nonzero,
causing reset `+0x08` and leaving Connection active; the scripted explicit
Next retries `+0x04` and its zero result advances. Cases 3 and 10 proceed
directly from successful provider notification without sending another Next
to the hidden Connection window. Callback arguments, results, selected
provider, visibility masks and state checkpoints match.

The first divergence for this group was captured with the
`flow-browser-message-handlers` control mutant, which removes Browser
`WM_SHOWWINDOW`, `WM_TIMER` and `WM_MOUSEACTIVATE` behavior. For each of cases
3, 4 and 10 in both cycles, its first differing timer API event is retail
`KillTimer(browser, 1)` from `WM_SHOWWINDOW` (event 0, action 2, ID 1); the
mutant has no corresponding event. The legitimate replacement map retains
these handlers and its SetTimer/KillTimer arguments and results match retail.
In case 3, a posted timer is also dispatched; that mutant is caught even
though both sides enter the message-map slot, because the handler's refresh
callback differs. Cases 4 and 10 do not dispatch a timer in their script.
This controlled mutant identifies the shared missing-handler cause without
claiming that every historical failure began at that same API call.

**Browser dispatch and callback reentry (paired runtime).** The harness checks
the `WM_TIMER` message-map entry on both binaries: signature `0x0d`, handler
owned by the correct module, retail target `0x22343910`, and replacement
target `0x01553720` in this build. It posts `WM_TIMER(wParam=1,lParam=0)`,
retrieves the queued message and calls `DispatchMessage`; the modeled `+0xa0`
callback then synchronously sends a nested `WM_TIMER` with the same
parameters. Both handlers are entered for the outer and nested calls. The
outer refresh sets `+0x1aec=1` before `+0xa0`; the nested handler observes
that guard and returns without another timer-triggered callback. Both sides
preserve the outer guard until update completes. The later explicit Browser
Refresh command is counted separately. The no-op-handler mutant and a
separate reentry-guard mutant both produce a Browser difference.

**Lifetime hooks (runtime verified).** Before teardown the harness checks
each binary's Browser vtable slots and module ownership, then wraps the same
slots: retail `PostNcDestroy` `+0xac` is `0x22344820` and deleting destructor
`+0x04` is `0x22342de0`; replacement slots resolve to `0x01554320` and
`0x015519d0` in this build. `DestroyWindow` records one `WM_NCDESTROY`, one
`PostNcDestroy`, and one deleting-destructor call with flag 1 for every page,
in that order and before modeled owner cleanup. Browser teardown now matches
across both binaries and both cycles. HOST's actual modal array walk remains
static evidence; the harness explicitly destroys each still-live HWND in
array order.

The same run reports **24 paired 0x110b scripts over two cycles, zero
differences, and 24/24 visibility checkpoints matching**, plus **20 paired
0x110c scripts over two cycles, zero differences**. The 0x110c suite includes
refresh through `0x464`, capacity validation, removal confirmation,
opaque-field preservation and ordered destruction.

HOST and MULTIP remain explicit modeled boundaries. The harness models HOST
showing array entry 0 after factory return, dispatching the queued Intro
command and sending the recovered `0x464` idle message across all five HWNDs;
it does not execute HOST's modal loop. Callback fixtures implement the
recovered MULTIP ABI and model providers, session rows and owner cleanup. The
actual MULTIP DLL, UTIL confirmation UI and service `1016` are not loaded.
Timer expiry and the nested callback message are modeled; physical mouse
activation is not exercised.

`tools/mpdialog_ui_breaks.py` runs **15 behavioral mutations plus a rendering
mutation** against the combined replacement. All are caught: seven 0x110a mutations for
initial-command timing, page transition, validation-command suppression,
callback arguments, deleting-destructor lifetime, Browser message handling
and Browser refresh reentry guard; four 0x110b mutations for accept result
order, callback mapping, DME state and visibility; and four 0x110c mutations
for refresh, capacity validation, removal callback and lifetime. It also
changes the base-font selector and requires the per-control render gate to
report a rendering difference. The lifetime mutant is detected before
controlled cleanup and unload. The runner requires a clean behavior,
control-tree, rendering and repeatability baseline before it builds any
mutation.

The lifetime mutation originally left the Intro object undeleted and let the
combined harness continue into later dialog suites. That run faulted in
MFC42 at `0x5f40219e` with `0xc0000005`; disassembly shows the faulting
instruction reading `[edx+4]` while traversing an MFC handle-map bucket.
**Inference:** skipping the deleting destructor leaves one or more embedded
SIMSUI/CWnd associations stale, which a later handle-map lookup encounters.
The current mutation path records the difference, restores the wrapped vtable
slots, calls the saved original deleting destructor once with flag 1 for the
intentionally omitted object, then deinitializes and unloads both modules.
The runner requires the cleanup marker, a controlled comparison exit, and no
`MP_DIALOG UI FAULT`; the bad mutant no longer poisons later scripts.

### Resolved native control-tree and rendering differences

**0x110c capacity edit/spin geometry (retail evidence and differential
probe).** Resource `0x6b` gives edit IDs 1035/1037 rectangles
`(251,281,51,21)` / `(251,313,51,21)` with style `0x50830080`, and spin IDs
1036/1038 rectangles `(301,281,13,30)` / `(301,313,14,30)` with style
`0x500200b6`. Both resource styles carry `WS_VISIBLE`; the spin style also
includes `UDS_AUTOBUDDY`, so the buddy comes from the resource style. MFC
ordinal 2086 creates the template at factory call
`0x22341248`; CBT measurements are post-dialog-unit conversion and agree on
both binaries. CBT creation sees the edit at `(251,246,51,18)` and the spin at
`(301,246,13/14,26)`. Common-controls auto-buddy setup then shrinks each edit
to width 35 and positions a 16-pixel spin at x=284 on both binaries, before
the MP_DIALOG initializer runs. After the custom font is installed, both have
the same 35x20 edit and 16x20 spin at x=284. The first replacement divergence
was in the explicit initialization step: retail `OnInitDialog`
`0x223448c0` calls SIMSUI `RemapCtrlRect` through IAT `0x2234b428`
(`0x22344a37` and `0x22344a6a`), then MFC ordinal 4299 / `MoveWindow`
(`0x22344a59` and `0x22344a8c`) for members `+0x14cc` and `+0x150c`.
Replacement previously issued an extra `UDM_SETBUDDY` and omitted this remap
sequence. Removing the redundant buddy reset and matching the retail remap and
move calls fixed the cause; final coordinates are not hardcoded. The paired
creation/message trace is in `/tmp/mpdialog-visual-baseline-after-fixes.log`.

**Six prior dialog-level render mismatches (per-control diagnosis).** The renderer
captures each visible child with a separate `WM_PRINT` DIB and records child
order, class/text, dimensions, style/ex-style, enabled and style/effective
visibility, focus, check/state, list selection/content, and `LOGFONT`. Repeating
the same capture after a full redraw on the same HWND was stable on both
binaries: 14/14 captures (seven dialogs per side), zero unstable pairs. This
includes seven retail same-window repeats and establishes that retail pixels
are repeatable under the harness conditions.

| Capture | First meaningful state difference and correction |
| --- | --- |
| 0x110a Choice | Its callsign edit used the wrong base-font selector in replacement: retail pushes selector 2 at `0x22345674` and calls `GetSimsBaseFont` at `0x2234567a`, while replacement passed 0. Per-control LOGFONT was Arial weight 400 retail versus 600 replacement. `flow_init_edit` now passes 2. |
| 0x110a Host options | Edits had the same font-selector error: retail pushes 2 at `0x223427c8`, `0x2234280d`, and `0x22342838`, then calls the helper at `0x223427ce`, `0x2234280f`, and `0x2234283a`. The owner-drawn “Host session as an observer” control also went through standard `CheckDlgButton`; retail `UpdateHost` at `0x22342990` calls `CSimsCheckButton::SetChecked` via IAT `0x2234b418` (`0x22342a7a..0x22342a8d`). Replacement now uses selector 2 and that SIMSUI method. |
| 0x110a Connection type | Back control ID 1001 (`object+0x13d0`) received bitmap `0x137` in replacement. Retail initializer at `0x22341c40` passes bitmap `0x132` for all four buttons; its final button call is at `0x22341dbc`. The replacement table now uses `0x132`. |
| 0x110a Browser | The row fixture had an invalid shape (integer at row `+0`, name at `+0x28`) and did not model retail's formatter. Retail code `0x22343590..0x22343753` reads the name pointer at row `+0`, counts at `+0x20/+0x24/+0x28/+0x2c`, and inserts resource literal `" of "` at `0x2234a090`. Replacement now formats the same columns from those fields. The corrected owner fixture uses the recovered 84-byte row layout. |
| 0x110b Session settings | All six custom check controls were initialized with palette member `object+0xe8`; retail `CSimsCheckButton::Init` calls use palette member `object+0x7e8` (initializer range starts `0x22343fc4`). Replacement now passes `+0x7e8`, matching palette and check-state rendering. |
| 0x110c Session details | The name and two capacity edits used the same wrong base-font selector: retail pushes selector 2 at `0x223449af`, `0x223449ea`, and `0x22344a15`, then calls the helper at `0x223449b1`, `0x223449f2`, and `0x22344a1d`; replacement used 0. `details.c` now passes 2. Geometry is covered separately above. |

The mismatch triage compared fonts, styles, enabled/check/selection state,
focus, list contents, and child paint order before changing implementation.
The initial Browser difference was partly an invalid fixture and partly a
missing replacement formatter; it is documented as such rather than
attributed to raster instability. Full per-control diagnostics are in
`/tmp/mpdialog-control-render-first.log`; the passing captures are in
`/tmp/mpdialog-visual-baseline-after-fixes.log`.

### Actual owner integration observation (private Wine)

The bounded runs use separate private game copies and Wine prefixes:

| Run | Private tree | Private prefix | `English/MP_DIALOG.DLL` SHA-256 |
| --- | --- | --- | --- |
| Retail | `bin/mpdialog-diff/integration-retail-copy` | `bin/mpdialog-diff/wineprefix-integration-retail` | `dc980acf09df47f64081b7e4775a7dd83eba69a75ae83181eea0ecc724b2a666` |
| Replacement | `bin/mpdialog-diff/integration-replacement-copy` | `bin/mpdialog-diff/wineprefix-integration-replacement` | `252f1b8989a1645fafae685eea401a51361c7f4b48d8808b519f1549201da729` |

Before the paired run, I found that the earlier local-host attempt had changed
the replacement private copy's `FLTSIM98.CFG` (`HOST_SESSION=1`, host name and
callsign populated, `CONNECTION_TYPE=1`), while the retail copy had the
default join configuration. That earlier run was not a valid matched pair. I
reset only the replacement private tree from the retail private tree, then
installed the replacement DLL into that copy. All other copied files matched
between the pair at run start; their DLL hashes remained distinct as above.
The run-start `FLTSIM98.CFG` hash matched at
`9bbb8aa3590ab4160da41f18b72b045df0db0d345f502084254e4511b15a4fae`. The
repository game DLL remains the retail hash. No other Wine prefix or live
install was used or changed.

The final hook is `tools/difftest/mpdialoghosthook.c`; it filters to creation,
show/activation, command, destruction, selected callback, and owner-cleanup
events. It samples `0x464` four times per HWND and then once per thousand
messages rather than logging each idle tick. The controller is
`tools/difftest/mpdialoghostprobe.c`, with a 45-second workflow deadline and a
2-second cancel delay. In both runs it returned to the enabled game at about
4.2 seconds. Wine's `explorer` process remained open after the controller had
completed, so an outer 15-second timeout closed only that run's desktop and
reported an X `BadWindow`; the controller had already logged
`CONNECT_CANCEL END result=0 returned=1`. That outer shutdown is not a modal
workflow timeout or a replacement divergence.

**Paired Connect → Cancel.** In each private process the controller posted
`WM_COMMAND/0x1180` (Connect) to the real `FS98MAIN` HWND `0x000200be` on its
UI thread. HOST's actual `run_dialog` path (`0x00409720`) called the actual
MULTIP entry (`0x20854d60`) and returned through the owner boundary at
`0x2085a45c`. The runtime hook read the actual MP_DIALOG array at factory
return:

| Observation | Retail | Replacement |
| --- | --- | --- |
| Factory entry | `0x22341130` | `0x22341b50` |
| Returned array address | `0x2234a0b8` | `0x2234d01c` |
| Returned HWNDs | `[0x000101ea,0x000101c2,0x00010170,0x000d00dc,0x000101a2,NULL]` | `[0x000101ea,0x000101c2,0x00010170,0x000d00dc,0x000101a2,NULL]` |
| Descriptor `+0x1c` / mode `+0x24` | `1 / 0` | `1 / 0` |
| First shown page | Intro `0x101ea` | Intro `0x101ea` |
| Cancel dispatch | `WM_COMMAND`, `wParam=2`, Intro | `WM_COMMAND`, `wParam=2`, Intro |
| Modal result | main enabled, no visible pages | main enabled, no visible pages |

The factory-return array and all six slots, including the null terminator,
were read at runtime; the two address spaces agree on the exact HWND sequence.
At factory return every page had `WS_VISIBLE=0` and `IsWindowVisible=0`.
HOST then showed only array index 0 (`Intro`), so both `WS_VISIBLE` and
`IsWindowVisible` became true there while the other four remained hidden. The
post-message IAT hook saw no `WM_COMMAND/0x3e8` Next post, and the
`WH_GETMESSAGE` trace saw no Next dispatch. This is expected for the observed
descriptor `+0x1c=1`: retail tests that flag and skips the `PostMessageA` call
at `0x223411c3`; the runtime probe confirms the flag, no post, and no dispatch.
Thus this run verifies the actual suppressed case, not the factory-post case
for `+0x1c=0` (that path is covered by the isolated differential suite).

The event-filtered hook saw actual `0x464` deliveries with `wParam=0,lParam=0` to
the visible page and all four hidden page HWNDs. Both logs contain 49 sampled
`0x464` records, including the same five page HWNDs; this is bounded sampling,
not the total number of idle sends. Cancel was posted only after Intro was
visible; it was removed from the queue and dispatched to `0x101ea`. Both
controllers observed the modal return with `main_enabled=1` and
`visible_pages=0`.

**Actual cancellation teardown and borrowed data.** Both hooks observe all
five pages receive `WM_NCDESTROY`, followed by MP_DIALOG's `PostNcDestroy`
slot `+0xac` and deleting-destructor slot `+0x04` with flag 1. For retail the
common `PostNcDestroy` is `0x22344820`; the per-page deleting destructors are
Intro `0x22345c90`, Choice `0x22345270`, Host Options `0x223421f0`, Connection
Type `0x22341910`, and Browser `0x22342de0`. For the replacement,
`PostNcDestroy` is `0x223444b0` and the matching destructors are
`0x22341a10`, `0x22341a50`, `0x22341a90`, `0x22341ad0`, and `0x22341b10`.
The hook saves each HWND before `WM_NCDESTROY`, because MFC has cleared the
object's `HWND` member by the time `PostNcDestroy` runs. All five destructor
chains finish before MULTIP returns from `0x2085a45c`.

The owner descriptor was `0x2085b5a8` at factory entry, every page teardown,
and owner return. In the paired cancel run its `+0x84` provider list had four
rows at return. `multip_configuration_clear` enters at `0x208516f9`, after the
modal return, and reaches HOST's free service from table `0x00417ba0` slot
`+0x214` (`0x0040cef0`). It invokes the service for the four non-null provider
payloads and clears the count from 4 to 0 before nested
`multip_browser_clear` at `0x208514e0`. The provider storage region remained
committed after the row payloads were freed; the probe therefore establishes
the row cleanup and count change, not deallocation of that backing region.
Cancel invoked no MP_DIALOG-owned accept/validate callback. The owner and its
borrowed descriptor stayed valid through all `WM_NCDESTROY` calls and the
owner return; this is observed lifetime for the successful modal path.

**Matched retail/replacement Host attempt; DirectPlay session limit.** The
controller ran the same Intro → Choice → Host Options → Connection Type →
Host flow in the retail and replacement private copies and separate dedicated
Wine prefixes. Both started from `FLTSIM98.CFG` SHA-256
`9bbb8aa3590ab4160da41f18b72b045df0db0d345f502084254e4511b15a4fae`; both
used the actual game `MULTIPLAYER.DLL` SHA-256
`0125e45b91c5bd45c7c74d6fab57a76277437ccb2315c6a33f3ff766af8988fb`. The
controller entered callsign `OpenFS Local Pilot`, session name `OpenFS Local
Session`, pilot limit `8`, observer limit `0`, selected provider row 0, and
clicked the enabled Host button. In both hook traces row 0 is
`Internet TCP/IP Connection For DirectPlay`; the same GUID words are read from
its record (`36e95ee0`, `857711cf`, `80000c96`, `824e53c7`). The descriptor at
the callback boundary is `0x2085b5a8`, mode `+0x24=1`, with the same strings
and limits on both sides.

**Verified callback and provider creation path.** The actual `+0x10`
validation target `0x2085177f` runs three times and returns zero in both runs.
The actual `+0x0c` accept target `0x20852f6c` sees result `+0x00=1` and
returns zero. Its mode-1 path sets MULTIP host-intent word `0x2085bca8=1` and
tail-jumps at `0x20853006` to the provider routine at `0x20852f04`; this is a
direct code transfer, not an indirect reload of descriptor `+0x04`. The
runtime hook now observes the provider routine's return at `0x20852f69`:
`EAX=0` for retail and replacement. The selected provider GUID reaches
`multip_directplay_create` at `0x20852e60`. Wine's `DirectPlayCreate` target
is `0x1f57c900` in `C:\windows\system32\dplayx.dll`; its result at
`0x20852e85` is zero in both runs. The two queried interface calls return
zero at `0x20852eac` and `0x20852ed0`; MULTIP then has a non-null DirectPlay
interface at `0x2085bb88` and its queried flag at `0x2085bc44` is one. These
runtime values establish successful provider/interface creation, not a
created session.

**First and shared session failure.** Static code at
`multip_session_toggle` `0x20851e15` sends host mode to
`multip_session_host` `0x20853424` after the configuration modal returns.
Both runtime traces enter that function with host-intent `0x2085bca8=1`, a
non-null DirectPlay interface, and connected word `0x2085bcac=0`. Its first
session operation calls the interface vtable slot `+0x60` at `0x208534c7`;
the target is `0x1f572780` in the same Wine `dplayx.dll`. At the instruction
after that call, `0x208534ca`, both binaries receive HRESULT `0x80004001`
(`E_NOTIMPL`). `multip_session_host` returns `-1` at `0x208534f2`; connected
remains zero, and the later `multip_session_create_groups` routine at
`0x20853394` is not entered. The outer toggle returns zero at its caller
continuation `0x207eb1a3` in both traces. The first session-creation operation
after provider setup fails identically in retail and replacement: there is no
replacement divergence before the shared external DirectPlay method returns
this HRESULT. Attributing `E_NOTIMPL` to the Wine DirectPlay implementation is
supported by the observed target module/path; the traces do not establish how
a different DirectPlay runtime or a real network provider would behave.

**Why Host Settings and 0x110c are unavailable here.** After the failed
session attempt, both real 0x110b factories open. MULTIP options init is at
`0x20851d1d`, and the parameter block is `0x2085b3e0`. The runtime block has
`+0x18=0`, `+0x1c=1`, and `+0x30=1`; the latter comes from the separate
global test of GLOBAL `+0x764=1` and `+0x84c=0`. Static MP_DIALOG `Update`
logic at `0x223441d2` first enables Host Settings for nonzero `+0x1c`, then
the zero `+0x18` branch at `0x22344224` disables control 1027. Both
controllers observe 1027 disabled. This identifies the exact UI gate values;
it does not prove that block `+0x18` is synonymous with MULTIP's connected
word. Static MULTIP shows the eight option records loaded from UTIL exchange
storage at `0x20851d46`, while the runtime traces independently show
`M_CONNECTED=0` after the failed Open. Neither run has control 1030, opens
0x110c, creates a second player, or invokes confirmation service `1016`.
Therefore 0x110c refresh, capacity, removal, confirmation, and teardown are
still covered only by the isolated modeled-owner suite; actual owner
integration is blocked at the same DirectPlay dependency in both binaries.

The paired logs are
`bin/mpdialog-diff/integration-retail-hostsession-controller.log`,
`integration-retail-hostsession-hook.log`,
`integration-replacement-hostsession-controller.log`, and
`integration-replacement-hostsession-hook.log`. The older replacement-only
attempt remains in `integration-local-host-*`; it did not capture the nested
`+0x04` return. The new logs capture it at `0x20852f69`. Controller logs show
both dialogs returned to the game and were canceled after the disabled
Host Settings observation. The native 0x110c suites remain modeled coverage,
not actual HOST/MULTIP integration.

**Construction failure is separate from the successful modal run.** The
reachable page-construction failure points are `ui_new` at retail IAT
`0x2234b4bc` and MFC `CDialog::Create` ordinal 2086 through thunk
`0x2234621a`. Retail's five helpers allocate before construction, do not test
the allocation result, ignore the Create result, and do not roll back earlier
objects if a later page fails (`0x22341130`, helpers `0x22341300..0x22341580`).
The isolated replacement has the same caller-level shape in
`flow_create_page`: it dereferences the allocation without a null check,
ignores `fs98_mpdialog_create_window`'s result, and has no partial rollback.
The earlier forced-false Create probe confirms those callers continue after a
false return; the allocation hook observed retail fault at `0x2234134a` when
the fifth allocation returned null. Neither is a genuine MFC-internal failure
test.

Real reachable triggers include allocator exhaustion, a missing or malformed
dialog resource, or a child/window-class creation failure inside MFC's
ordinal-2086 path. A missing `DIALOG.DLL` probe failed before page construction
and is not evidence for MFC's partial-create cleanup. The retail and
replacement call the same MFC ordinal with the same templates, but no probe
forced a genuine `CreateDialog`/child-creation failure after MFC had started
construction. Therefore the replacement matches the statically recovered
caller decisions, while equivalence of MFC's internal partial-window cleanup
and cleanup after a real allocation fault remains unverified; no cleanup
guarantee is claimed.

## Isolation and acceptance

### Whole-module criteria audit

The acceptance gates in [the dialog continuation rules](dialogs.md#starting-a-remaining-dialog-in-a-fresh-context)
map to this evidence:

| Criterion | Evidence | Status |
| --- | --- | --- |
| All public registrations and factories | Retail/replacement registry tables are checked for ids `0x110a/0x110b/0x110c`, flags `2/0/0`, and terminator. Each factory pointer is checked for ownership by its own loaded module; the retail pointers are checked against `0x22341130`, `0x223411e0`, `0x22341270`. All three factories are invoked by their corresponding native suites. | **Pass** |
| Complete flag-2 factory output | The isolated 0x110a harness requires five live HWNDs and a null sixth slot; the paired runtime hook also reads the actual six slots from retail and replacement as `[intro, choice, host options, connection type, browser, NULL]`. Both owner runs observe `+0x1c=1`, no posted/dispatched Next, HOST showing Intro, and successful Cancel. The factory-post branch for `+0x1c=0` and the rest of transitions remain isolated-suite coverage. | **Pass for output and observed owner case; initial-post owner case partial** |
| Native binding and complete real-control trees | Vtables, message maps, callback targets and module ownership are checked before handlers are wrapped. All five 0x110a child trees match on `24/24` paired scripts (page counts `13/17/20/12/14`); all 0x110b trees match on `24/24`; all 21 0x110c child controls match on `20/20` pairs. `0x110c` resource creation, automatic spin buddy sizing, `RemapCtrlRect`, and `MoveWindow` now match in traced order. | **Pass in isolated suites** |
| Native rendering | The five 0x110a pages plus initial 0x110b and 0x110c dialogs are captured with per-child `WM_PRINT` into 32-bit DIBs. Font, styles, enabled/check/selection state, focus, list contents and paint order are recorded alongside pixels. All seven paired dialog captures and per-control records match. Same-HWND redraws are stable for `14/14` captures (seven per side, including seven retail repeats). Causes and fixes are detailed above. | **Pass in isolated suites** |
| Callbacks, validation, accept/cancel | The 0x110a scripts cover callback arguments/results, forward/back/cancel, validation failure and recovery, Browser refresh/selection/settings, and accept. The 0x110b and 0x110c suites compare callback order/arguments/results and their validation/removal/refresh paths. ABI prototypes are documented against MULTIP callee epilogues. Matched retail/replacement Host runs see three `+0x10` validations returning zero, `+0x0c` accept returning zero, and the nested `+0x04` provider callback returning zero at `0x20852f69`; DirectPlayCreate and both QueryInterface calls also return zero. Actual 0x110b OK and all actual 0x110c callbacks remain unexercised. | **Pass in isolated suites; actual owner callback path partial** |
| Window and module lifetime | Native hooks observe `WM_NCDESTROY → PostNcDestroy (+0xac) → deleting destructor (+0x04, flag 1)` for all five 0x110a pages and the 0x110b/0x110c windows. The suites execute two DLL load/init/factory/deinit/FreeLibrary cycles and verify registration/release counts and that neither module remains loaded. The paired actual 0x110a cancel runs also observe all five destructor chains before owner return, then MULTIP frees four provider payloads and clears the row count. Actual repeated owner unload is not covered. | **Pass in isolated suites; one owner teardown observed** |
| Behavioral and rendering mutation acceptance | State/callback/lifetime comparisons have zero differences in all three suites over two load/unload cycles. Fifteen targeted behavioral mutations and one wrong-font render mutation produce their expected differences. The runner requires a clean behavior, control-tree, rendering and repeatability baseline before mutation checks. The lifetime mutation is detected before the harness calls the saved deleting destructor to release the intentionally leaked object; each targeted mutant then deinitializes and unloads both modules. | **Pass for isolated behavior and render gates** |
| Isolated startup | `bash tools/run_mpdialogdiff.sh --smoke` starts a private copied game tree with the current replacement and a dedicated Wine prefix. It loaded `27/27` Modules DLLs and `95` gauges, observed MP_DIALOG load, reported no trace errors, and verified the private replacement and source game DLL remained unchanged. Log: `bin/mpdialog-diff/smoke-game-7h8f0e_x/mpdialog-smoke.log`; replacement SHA-256 `252f1b8989a1645fafae685eea401a51361c7f4b48d8808b519f1549201da729`. | **Pass for startup/load only** |
| Actual HOST/MULTIP dialog workflow | Separate private copies/prefixes complete matched Connect → Cancel runs and matched Host attempts from the same config hash and provider row. The Host traces capture the nested `+0x04` return, successful DirectPlayCreate/QueryInterface calls, then the same `dplayx.dll` `IDirectPlay3::Open` HRESULT `0x80004001` in retail and replacement; `M_CONNECTED` remains zero in both. Both real 0x110b dialogs show Host Settings disabled with block `+0x18=0,+0x1c=1`; no 0x110c list, second player, or UTIL confirmation `1016` is reached. The matched DirectPlay failure establishes an integration limit, not an MP_DIALOG difference. The factory-post path when `+0x1c=0` remains isolated coverage. | **Partial; actual 0x110a cancel and blocked Host path observed, whole-module gate blocked** |
| Construction-failure cleanup | Static retail and replacement caller paths both ignore MFC Create failure and have no partial-page rollback; both dereference a null allocation rather than returning a clean failure. The prior false-return and allocation probes were injected outside MFC, and missing-DIALOG failure preceded construction. No genuine MFC resource/child creation failure was exercised, so internal partial-window cleanup is unknown. | **Caller-level parity static; genuine cleanup unverified** |

**Why MP_DIALOG remains incomplete despite passing comparisons:** the
isolated suites cover all three factories and their modeled behaviors, and the
paired owner runs verify the actual 0x110a array, observed `+0x1c=1` branch,
cancellation teardown, and MULTIP provider-row cleanup. Matched retail and
replacement Host attempts now establish that provider/interface creation
succeeds but Wine `dplayx.dll` returns `E_NOTIMPL` from the first session
`Open` call in both binaries. The owner therefore leaves `M_CONNECTED=0`, and
0x110b receives a block whose `+0x18=0` gate disables Host Settings. Actual
0x110c refresh, capacity edits, removal, confirmation service `1016`, and
actual 0x110b OK remain unreachable through this external failure. The
`+0x1c=0` factory-post branch is represented by the isolated suite only.
Genuine MFC partial-Create cleanup also remains unverified; artificial
failures outside MFC do not establish framework cleanup. These limits keep
whole-module acceptance blocked, despite passing isolated comparisons.

Final evidence commands and logs:

- `make -f tools/mpdialog.mk mpdialog`
- `bash tools/run_mpdialogdiff.sh --ui` → `/tmp/mpdialog-visual-baseline-after-fixes.log`: 24 `0x110a`, 24 `0x110b` and 20 `0x110c` paired scripts match across two load/unload cycles; control trees match `24/24`, `24/24` and `20/20`; all seven render captures match; same-window redraws match `14/14` with zero unstable pairs.
- `bash tools/run_mpdialogdiff.sh --ui-breaks` → `/tmp/mpdialog-visual-mutations-after-fixes.log`: the runner first requires all behavior, complete child-tree, render and repeatability gates to pass, then runs `15/15` targeted behavioral mutations and one wrong-font rendering mutation. The lifetime mutant also requires controlled leaked-object cleanup and module unload.
- `bash tools/run_mpdialogdiff.sh --smoke` → `/tmp/mpdialog-visual-smoke-after-fixes.log`: `27/27` Modules DLLs, 95 gauges, replacement loaded, no trace errors, and source/private-copy DLL hashes unchanged. Full trace: `bin/mpdialog-diff/smoke-game-7h8f0e_x/mpdialog-smoke.log`.

The source, standalone build, generator, differential harness and probe tools
remain isolated under `src/mpdialog/`, `tools/` and ignored
`bin/mpdialog-diff/`. MAIN, HOST, MULTIP, shared build/install wiring and the
live game install are unchanged. Keep MP_DIALOG marked **partial** until the
remaining actual HOST/MULTIP owner paths are captured and reviewed; genuine
MFC construction-failure cleanup remains explicitly unverified.

### Independent review fixes

The Connection provider loop now follows descriptor `+0x88` without a 64-row
cap, matching retail `0x22341e99..0x22341ec3`. Browser observer availability
uses `+0xa8` (retail `0x22343435`), independently of selected mode `+0x40`.
The native fixtures now include 64/65 providers and all four combinations of
availability and selected mode, with explicit initial availability/count checks.
These fixtures also exposed capture differences: Host and Browser must read
the SIMSUI owner-drawn checked words at `+0x1a70` and `+0x1ad8`, respectively
(retail `0x22342aee` and `0x223434a5`), rather than IsDlgButtonChecked.

After these corrections the isolated native comparison passes all 24 flow,
24 settings and 20 details pairs, all control trees and seven render captures,
with 14/14 stable repeated captures across two load/unload cycles. Log:
`/tmp/mpdialog-review-ui.log`. Owner integration limits above are unchanged.
The mutation rerun also passed its clean baseline and caught all 16 existing
mutations; log: `/tmp/mpdialog-review-breaks.log`. Build and `git diff --check`
passed. No shared build files or live installation were changed.

### Browser row boundary audit (partial)

Retail's row loop at `0x223435d3..0x223436e3` reads the descriptor
`+0xb0` count without a 128-row cap. The replacement now does likewise. The
native flow fixture supplies 129 rows in case 0 and asserts the list count for
both modules. With the previous cap the retail side passed and the replacement
failed that assertion (`/tmp/mpdialog-audit-before.log`); after removing it,
`bash tools/run_mpdialogdiff.sh --ui` passed the isolated suites and rendering
checks (`/tmp/mpdialog-audit-after.log`). The previous implementation is the
targeted cap mutation for this fixture.

Retail's row rebuild at `0x22343590..0x22343752` resets the list and
selects row 0 when the count is nonzero, or -1 when empty. It enables Details
according to count and computes Join from the selected row. The selection
handler at `0x22343510..0x22343564` instead enables Join solely according to
whether a selection exists, reads selection again, and invokes `+0x98`; it
does not write `+0xb8` or recompute capacity afterwards. The replacement now
follows these decisions and orders observer availability before SetChecked.

The case-3 native probe records list selection/count, Join and Details enabled
state, descriptor selection and callback argument at each step and prints the
first divergent event. It covers a retained row-1 selection before rebuild,
shrink from two rows to one, an empty list and selection notification, growth
back to two, a callback changing capacity, and a callback that changes capacity,
rebuilds the list and recursively sends a selection notification. Retail
assertions check these branches, including a full selected row disabling Join
on rebuild and the nested selection enabling it again. Before the fix the first
difference was event 1: retail selected 0 and replacement selected 1. The
callback-order mutant first differs at event 6: Join is enabled at retail's
callback entry and disabled in the mutant
(`/tmp/mpdialog-browser-order-mutant.log`). The 129-row case remains in case 0.

`bash tools/run_mpdialogdiff.sh --ui` passed 24 flow, 24 settings and 20 details
pairs over two load/unload cycles, all control-tree and seven render captures,
and 14/14 stable repeated captures (`/tmp/mpdialog-selection-ui.log`). The
final `--ui-breaks` run repeated the complete baseline with the final fixture
and caught all 19 mutations, including row cap, rebuild selection and callback
ordering (`/tmp/mpdialog-selection-mutations-rerun.log`). One preceding
baseline run reported an isolated 0x110c case-5 focus difference (retail
control 2, replacement none; `/tmp/mpdialog-selection-mutations-final.log`);
the earlier comparison and subsequent full baseline passed. This focus
intermittency remains an observation, not a diagnosed Browser defect. Build and
`git diff --check` passed.

The remaining flow audit includes row formatting and long strings, descriptor
field substitutions and bounds outside these cases, owner-drawn state and
other callback/message ordering. The existing owner-integration and
whole-module acceptance limits above are unchanged.

### 0x110c case-5 focus investigation

The preserved failure is `bin/mpdialog-diff/focus-study/original-failure.log`
(SHA-256 `60a66dd8260ef0bfe7fd4e18f48f2b34665bde44cda0c57de8a34aca5e34c2b6`).
It reports cycle 0, case 5, `capacity=1`: the first and only recorded focus
checkpoint, `focus_after_validation`, was control 2 for retail and 0 for the
replacement. Callback results, values, other recorded state and controls
matched. That log did not record earlier checkpoints, so it cannot establish
the first actual divergence or attribute that particular event definitively.

The case sets observer capacity to `9` and sends the observer edit's
`EN_KILLFOCUS` notification. The harness now has opt-in `--details-focus-trace`
logging for activation/focus and command messages, pending queue head,
owner/foreground/active/focus windows, and validation callback entry, exit and
results. An uncontrolled targeted trace (`focus-study/uncontrolled-retail-replacement.log`)
showed the two sides already differed before setup: the first had the hidden
main window active and focused, and the second had neither an active nor a
focused window. Both happened to focus control 2 after creation in that run.
Thus the preexisting harness did not give the sides equivalent starting focus;
it is a demonstrated harness defect, although the original failure's exact
activation history remains unknown.

With the same visible, enabled, active and focused owner before each factory,
20 separate case-5 retail/replacement processes and 20 retail/retail processes
(two separately loaded DLL copies) all returned zero differences. Normalized
owner, activation, focus, pending-message and callback checkpoints matched in
all 40 pairs; detailed logs are `focus-study/ours-1.log` through `ours-20.log`
and `retail_copy-1.log` through `retail_copy-20.log`. The targeted runs use
`--details-mutation-case 5 --details-focus-trace --details-focus-reset`.
Three further runs of each pairing added `--details-focus-deactivate` after the
edit and before its notification. Both sides then had no focus rather than
control 2, returned the same failed validation (`observer=1`), and recorded a
pending `0x5f4` validation message; all six pairs matched. This demonstrates
that activation can change the measured focus on either binary. It does not
prove external deactivation occurred in the original uninstrumented run.

The normal 0x110c suite now establishes and asserts the same visible, enabled,
active, focused owner before **each** factory call. This changes the fixture
precondition only: focus after validation is still captured and compared, and
`--details-focus-uncontrolled` retains the old diagnostic setup. A targeted
mutation forcing focus onto invalid observer control `0x40d` was caught as a
capacity difference, first at the post-notification checkpoint (retail focus 2,
mutant 1037); trace `focus-study/focus-mutant.log`. It is now also part of the
mutation runner.

After the fixture fix, `bash tools/run_mpdialogdiff.sh --ui` passed 24/24 flow,
24/24 settings and 20/20 details pairs, all child trees and seven render
captures, with 14/14 stable repeats (`focus-study/fixed-ui.log`). The complete
existing 19-mutation suite passed (`focus-study/fixed-mutations.log`); the new
focus mutation was built and run separately and caught
(`focus-study/focus-mutant-check.log`). Build and `git diff --check` passed.
These runs use the isolated prefix and modeled owners; the integration and
whole-module limitations remain unchanged.

### Browser formatting, descriptor and ordering audit

Reviewed retail Browser Back/Join/Cancel and Details/Settings at
`0x22343000..0x2234315d`, Update/Capture at `0x22343420..0x223434cc`,
selection and row construction at `0x22343510..0x22343753`, and idle,
observer, Join availability, refresh, activation and timer handlers at
`0x22343780..0x2234392a`. The owner-drawn observer checked word is object
`+0x1ad8`, the availability flag is descriptor `+0xa8`, and the selected
mode is descriptor `+0x40`. The earlier independent-flag combinations and
refresh/selection reentry cases remain in the isolated suite.

- The row formatter uses MSVCRT `_itoa` on signed 32-bit row fields, with
  the order `+0x28/+0x20`, then `+0x2c/+0x24`. The old replacement printed
  unsigned values and used a 256-byte stack buffer. Case 3 compares an empty
  name and a long name with 0, -1, INT_MIN and INT_MAX; before the fix, with a
  200-character name, the first signed field differed. After the fix a
  700-character name and the boundary values match. The implementation now
  allocates the name plus a bounded numeric suffix. Allocation-failure parity
  and arbitrarily large or invalid pointers remain unverified.
- The Observer handler writes `+0x40` from the owner-drawn checked word but
  does not capture selection. Details and Refresh write `+0xb8` but do not
  capture `+0x40`; refresh does that before callback `+0xa0`, and updates
  only if the callback returns nonzero. Case 3 seeds independent fields and
  snapshots at observer handling, Details and refresh callback entry. Before
  the fix retail recorded `2,77,1,0,1,0` and replacement `2,0,2,0,2,0`.
- Browser Join calls `+0x0c` and finishes regardless of its return. Case 3 now
  supplies a nonzero accept result: the pre-fix replacement remained open and
  retail closed. Browser Back first clears the list, clears selection and
  disables Join, then invokes reset `+0x08` and navigates back; it does not
  capture the descriptor fields. Case 10 snapshots those values inside the
  callback. Before the fix retail saw list count/selection/mode/descriptor
  selection/Join `0,-1,1,77,0`, and replacement `2,1,2,1,1`.
- Retail rereads descriptor `+0xb0` after each list insertion and reloads
  `+0xac` before each row. A list-subclass probe changes count or row storage
  during the first `LB_ADDSTRING`: retail uses one row after a shrink and the
  new storage for the next row; the old replacement cached both. Case 3 now
  compares both and asserts retail reached the intended states. This is a
  controlled synchronous reentry fixture, not evidence that a specific real
  owner performs those mutations.

The before-fix transcripts for formatting/fields, nonzero accept, Back and
list reentry are preserved in `bin/mpdialog-diff/browser-audit/`. All four
focused cases pass after the fixes. The complete isolated UI run
`/tmp/mpdialog-browser-final-ui.log` passed 24 flow, 24 settings and 20 details
pairs over two load/unload cycles, all child-tree and seven rendering captures,
and 14/14 stable repeats. The 129-row, selection and 0x110c focus regressions
remain enabled.

Remaining gaps include null/invalid descriptor and row pointers, allocation
failure, list-insertion failures, abnormal or destructive callback reentry,
and full negative/zero refresh callback and UI message ordering coverage.
The DirectPlay integration blocker and whole-module acceptance limits above
remain unchanged; no actual owner integration was retried in this audit.

The final `bash tools/run_mpdialogdiff.sh --ui-breaks` rerun passed its full
baseline and caught **28/28** behavioral and rendering mutations, including
separate signed-format, three descriptor-field, nonzero-accept, Back-order,
and reentrant count and pointer mutations. Its log is
`bin/mpdialog-diff/browser-audit/mpdialog-browser-final-mutations-rerun.log`.
A preceding run timed out during retail-side teardown of the Details-field
mutant after ten mutations had been caught; the runner correctly rejected that
timeout as a detection. The subsequent full rerun passed; the original timeout
log is preserved alongside it. `git diff --check` and the standalone build
passed.

### Mutation timeout investigation and next audit family

The preserved timeout is
`bin/mpdialog-diff/browser-audit/mpdialog-browser-final-mutations.log`. It was
mutation `flow-browser-details-fields`, case 3, retail side 0. Intro page 0's
`DestroyWindow` was entered with all five page windows live and hidden. After
owner deactivation, the final completed logged dispatch was `WM_ACTIVATEAPP`
(`0x001c`) for Host Options page 2. There is no recorded Intro `WM_NCDESTROY`,
`DestroyWindow` return, or replacement-side execution. The record does not
include a blocking stack or the pending-queue contents at the stall. The
mutant changes the replacement Browser Details handler; that handler had not
been called when the timeout happened. Both DLLs had been loaded and
initialized, however, so this observation alone does not rule out all shared
MFC, SIMSUI, Wine or harness state effects.

With the same case, five clean and five mutant runs in separate private Wine
prefixes completed all five destruction chains for each side and unloaded both
modules; the expected mutant difference remained detected. At the retail
Intro teardown entry, all five windows were live, no refresh reentry was
pending, and the list subclass probe had been removed. Two additional runs of
each pairing used optional non-removing `PeekMessage` diagnostics. Their queue
head was `EM_SETSEL` (`0x00b1`), and no `0x5f4` validation message was pending
at that checkpoint; both pairings completed. `PeekMessage` can dispatch sent
messages, so those diagnostic runs are separate from the five uninstrumented
queue repetitions. The logs are under `bin/mpdialog-diff/timeout-study/`.
These observations neither reproduce the original stall nor establish whether
it blocked in retail code, shared framework code, Wine, or activation/message
dispatch. A successful rerun is not treated as its explanation.

The flow harness now records each `DestroyWindow`, `WM_NCDESTROY`,
`PostNcDestroy`, and deleting-destructor entry/return, live-window mask,
pending-input indication, callback guard and test-list-hook state. It retains
all lifetime assertions. `MPD_TRACE_LIFETIME_QUEUE=1` adds the opt-in queue
head and `0x5f4` observations. The mutation runner creates and owns a fresh
private Wine prefix for its run; timeout kills its process group and that
private wineserver only, reports the last lifetime checkpoint, and fails the
mutation. Baseline and render timeouts use the same handling. The
`tools/mpdialog_timeout_check.py` sentinel check deliberately stalls case 3
at Intro destruction in a private prefix, verifies the reported phase and
prefix removal, and checks that a separate Wine process remains alive. The
original forced-timeout evidence is `timeout-study/isolation-check.log` and
`timeout-study/forced-timeout.log`. The deliberate stall is opt-in via the
exact `MPD_TEST_STALL_DESTROY=case3-page0` test value.

The next bounded independent audit family was the 0x110a **Host Options**
update/capture and validation/error handlers (`0x22342990`, `0x22342aa0`,
`0x22342b60`, `0x22342c40`; replacement
`fs98_mpdialog_flow_update_host`, `capture_host`, `error_host`, and
`validate_host`). The handoff already identifies independent descriptor
fields, spin bounds, owner-drawn state, and validation ordering as audit
priorities. Initial disassembly shows retail sets both spin ranges before its
`SetChecked` call; the replacement currently calls `SetChecked` first and
also sends limit-text messages from update. These are hypotheses for a native
message-order fixture, not yet established observable defects. Test boundary
strings, independent status words and reentrant validation callbacks against
retail before changing the implementation. Owner-integration and genuine MFC
partial-creation limits continue to block whole-module acceptance under the
existing criteria.

Validation after the runner change: `bash tools/run_mpdialogdiff.sh
--ui-breaks` used the fresh private prefix, passed the full 24/24 flow, 24/24
settings and 20/20 details baseline and rendering gates, and caught all 28
mutations (`timeout-study/private-suite.log`). The checked-in isolation test
was run as `WINEARCH=win32 WINEDEBUG=-all python3 -B tools/mpdialog_timeout_check.py`; it reported the controlled Intro
`destroy-enter` stall, removed the private prefix, and verified the independent
Wine sentinel survived (`timeout-study/tracked-isolation-check.log`). These
checks do not retroactively identify the original timeout's blocking stack.

### Host Options audit: initial message probe (incomplete)

An isolated, private-prefix paired probe invoked virtual update `+0xf0` after
factory creation with normal capacities (64, 16, 16). A thread call-window
hook recorded `WM_SETTEXT`, `UDM_SETRANGE`, and `EM_SETLIMITTEXT`: both sides
sent three text updates followed by range messages to `0x411` and `0x412`.
Only replacement then sent `EM_SETLIMITTEXT` to `0x411` (15), `0x412` (15),
and `0x41a` (63). These messages establish a difference, but the probe did not
observe the `CSimsCheckButton::SetChecked` call, so they are not established
as the first divergent event. The temporary trace is
`/tmp/mpdialog-host-audit.log`; it is diagnostic evidence, not a regression
test. No implementation change was made on this evidence.

Retail disassembly reviewed: update `0x22342990..0x22342a97` (range calls at
`0x22342a20` and `0x22342a64`, `SetChecked` at `0x22342a7a/0x22342a8d`),
capture `0x22342aa0..0x22342b15`, error `0x22342b60..0x22342c37`, and
validation `0x22342c40..0x22342ca2`. A complete paired event fixture must
also intercept `SetChecked` and exercise zero/one/normal capacities, text
boundaries, independent and simultaneous status fields, visible/hidden
validation, and callback reentry. Error paths and capture capacity behavior
remain unverified dynamically. The full isolated baseline and mutation suite
were not run as part of this initial probe; the historical timeout limitation
above remains unresolved.

### Host Options paired update, capture and error audit

The paired Host update probe intercepts the shared SIMSUI `SetChecked` entry,
verifies retail's import resolves to that entry, and requires an invocation
from each binary. A thread call-window hook records the corresponding text,
spin-range and limit messages. Before the fix the first divergence was event 3:
retail sent `UDM_SETRANGE` to `0x411`, while replacement called `SetChecked`.
The complete retail sequence was `WM_SETTEXT` to `0x41a`, `0x40f`, `0x410`;
`UDM_SETRANGE` to `0x411` and `0x412`; and `SetChecked` for `0x3ed`.
Replacement then also sent three `EM_SETLIMITTEXT` messages. The failing
paired trace is `/tmp/mpdialog-host-sequence-before.log`.

Retail initialization sets edit limits at `0x22342909..0x2234295e`, using
capacity minus one for pilot, observer and name edit HWNDs respectively; this
happens before the update at `0x22342970`. The replacement now sets those
limits there and does not send limit messages in update. The fixture compares
initial `EM_GETLIMITTEXT` results separately, as well as the update event
sequence. It also captures into sentinel-filled buffers with capacities 0, 1
and 8 and strings of length 0, 1, 7, 8 and 9 for each of the three edits.
The tests compare the complete 12-byte buffers, including the guards.

The paired validation probe checks the three status fields independently and
in combinations, callback success with statuses present, hidden validation,
and a single nested validation call from the owner callback. The retail oracle
requires field priority name, pilot, observer and confirms the nested callback
is entered twice. The error probe intercepts the shared SIMSUI error-dialog
helper, checks retail's import target and tests status combinations, field zero,
and helper returns zero and one. It found two defects at the handler's final
selection: with no status retail forwards zero to `+0xe0`; on helper return
zero it forwards the original incoming field. The replacement now does both.
The paired failure evidence is `/tmp/mpdialog-host-validation-before.log` and
the corrected focused comparison is `/tmp/mpdialog-host-validation-after2.log`.
The helper is stubbed in this fixture: its actual modal UI and returned values
are not tested there. The capture capacity checks change descriptor capacities
after creation; initialization with zero or one capacity and its message order
remain untested. Null descriptor pointers, unusual huge capacities, other
reentrant mutations of descriptor pointers, and deeper callback nesting also
remain untested. These do not change the owner-integration and genuine MFC
creation-failure gaps or resolve the historical timeout described above.
The error fixture's `+0xe0` test hook returns 1; return propagation for other
values is untested. Retail leaves `+0xe0`'s return in EAX at
`0x22342bad..0x22342bb3` and `0x22342c2f..0x22342c37`, while the replacement
currently returns 1. Whether a caller can observe a different value remains a
specific follow-up rather than a verified behavioral equivalence.

Validation: `bash tools/run_mpdialogdiff.sh --ui-breaks` passed the full
24/24 flow, 24/24 settings and 20/20 details paired baseline, rendering and
repeatability gates, and caught all 33 mutations, including the five new Host
update, error and validation mutations (`/tmp/mpdialog-host-final-suite.log`).
The runner used and cleaned its own private Wine prefix.

### Host Options boundary and error propagation follow-up

The earlier Host audit's untested initialization and return-propagation notes
above are superseded by this follow-up. In paired factories with all three
capacities set to 0, retail sends `EM_SETLIMITTEXT` in pilot, observer, name
order with `wParam=0xffffffff` for each. At capacity 1 it sends `wParam=0` for
each. Under this Wine/USER32 fixture, all six edit controls report an effective
limit of `0x7ffffffe` and accept both characters delivered as `WM_CHAR`;
replacement matches. This is creation-time evidence, separate from the prior
capture-buffer tests that changed capacities after creation. It does not imply
that other Windows implementations expose the same effective edit limit.

The Host error handler returns the virtual `+0xe0` result unchanged (and
returns 0 immediately for a zero input field). With an injected `+0xe0`, paired
tests use 0, 2 and -7, both with and without a status; the prior replacement's
constant 1 differs. The fix returns that value. Separately injected helper
returns 2 and -7 are treated as nonzero, stored at object `+0x8c` and select
the status's field. These values are deliberately injected test vectors, not
claims about naturally occurring SIMSUI results.

The downstream SIMSUI `0x5f4` handler at `0x204767b0` calls `+0xdc`, passes its
return to `0x20476960`, which stores the value at `+0x7d0`, and uses nonzero to
decide whether to repost a deferred command; the message handler itself returns
0. Paired injected 0, 2 and -7 values verify the pending word and message
return. The Host Next handler at `0x22342520` also calls `+0xdc` on failure,
then returns from its void command handler; the inherited tab-dialog path at
`0x2048072b` tests the result for zero. These observations do not assume the
synthetic values occur in normal operation.

The separate real-helper scenario loads retail `Alerts.DLL`, initializes its
imports over the same fixture, creates its registered id-2 dialog (template
`0x8a`), and dismisses its actual first button, 2003. It invokes the real
SIMSUI error helper and `+0xe0`; neither return is injected. A bounded test
pump dispatches the alert's window messages and defers other windows' queued
messages, rather than implementing all HOST modal and idle processing. The
modeled owner still supplies validation and HOST services. Both sides observe
the pending `0x41a` before and during the alert, suppress Next while pending,
restore focus to edit `0x41a` after dismissal, retain the pending value until a
successful edit, and then clear it and advance. The alert writes result 0 into
its descriptor; SIMSUI's real error helper returns 1, and the real `+0xe0`
returns `0x41a`. The old constant-return implementation instead causes the
posted handler to store 1 as the pending field, so the real-modal mutation
also detects the defect. This is isolated real-dialog coverage, not a new
actual-HOST/MULTIP integration result.

#### Reviewed Host Options functions

| Retail function | Reviewed behavior and evidence | Remaining boundary |
| --- | --- | --- |
| `0x223425e0` initialization | Control setup and creation-time edit-limit arguments, including 0 and 1 capacities, paired against retail | Failure and null-descriptor paths remain outside this fixture |
| `0x22342990` update | Text, spin-range and `SetChecked` ordering; no update-time limit messages, paired event trace | Arbitrary reentrant descriptor changes remain open |
| `0x22342aa0` capture | All three fields, capacities 0/1/8, boundary strings and sentinel buffers, paired | Null pointers and huge capacities remain open |
| `0x22342c40` validation | Status priority, visibility, success/failure and one callback reentry, paired | Deeper callback nesting remains open |
| `0x22342b60` error | Status selection, injected helper and downstream return values, posted-message propagation and one real retail alert dismissal, paired | No claim that unusual injected results occur naturally; other alert choices and full HOST pump remain open |
| `0x22342520` Next | Failure calls `+0xdc`; its command handler is void; normal suppression and recovery observed | Real MULTIP/HOST owner path remains limited as above |

The earlier lifetime timeout's blocking stack and cause remain unknown; this
follow-up does not explain or reclassify it. The private-prefix isolation,
actual-owner limitations, and whole-module partial status remain unchanged.

Validation: the full private-prefix mutation run passed its baseline and
caught all 36 mutations (`/tmp/mpdialog-followup-full-suite.log`). After the
pending-propagation and modal observations were strengthened, the final paired
baseline passed 24 flow, 24 settings and 20 details scripts, all child trees,
seven render captures and 14/14 stable repeats
(`/tmp/mpdialog-followup-final-baseline.log`). The three affected return and
initialization mutations were rerun against that fixture and caught
(`/tmp/mpdialog-followup-final.log`); the constant-return mutant was also
caught through the actual alert (`/tmp/mpdialog-followup-final-modal-mutation.log`).
Build and `git diff --check` passed.
The controlled timeout isolation check again removed its private prefix while
an independent Wine sentinel remained alive (`/tmp/mpdialog-followup-isolation.log`).

### Reconciled local-target inventory and next differential family

This inventory supersedes the earlier recommendation to audit Host Options,
which was completed above. It reconciles the three registry factories, all
seven retail class vtables and all seven message maps against the evidence in
this handoff and the replacement bindings in `tools/gen_mpdialog_runtime.py`.
Addresses are retail preferred VAs. Imported SIMSUI/MFC thunks are framework
code, not additional MP_DIALOG-owned function bodies; the Connection provider
and Details command thunks are included with their ultimate targets. Binding,
child-tree and baseline-script checks show that a target is connected or that
particular scenarios match; they do not by themselves review every branch or
establish a full source-versus-retail comparison.

| Family | MP_DIALOG-owned targets and existing evidence | Remaining implementation review or differential coverage |
| --- | --- | --- |
| Factories and construction | Registry factories `0x22341130`, `0x223411e0`, `0x22341270`; 0x110a helpers `0x22341300`, `0x223413a0`, `0x22341440`, `0x223414e0`, `0x22341580`. Normal allocation, creation, output, native trees and lifetime are compared; caller failure behavior is statically examined. | Constructors, individual control initialization and DDX have normal-path/tree evidence, not exhaustive call-order or failure-path comparisons. The separate genuine MFC cleanup uncertainty below is not a proven implementation defect. |
| Common vtable and lifecycle | All seven tables contain local `0x22342040`, `0x22342cb0`, `0x22342050`, `0x22342060`, and `0x22344820`; the seven deleting destructors are `0x22345c90`, `0x22345270`, `0x223421f0`, `0x22341910`, `0x22342de0`, `0x22343aa0`, `0x22344510`. Normal window destruction and deleting flag 1 are observed. | `0x22342040` and `0x22342cb0` are no-op returns; `0x22342050/2060` call `EnableWindow` on object `+0x20` with 0/1. Their seven-class slot bindings are checked, but a focused paired enabled/already-disabled transition and no-op/ABI check is not documented. Destructor flag 0 and exceptional construction are not established by the normal teardown scripts. The flow `+0xec` replacement is a deliberate isolated model of inherited close, not a local retail vtable target. |
| Map getters and DDX | Getters `0x22345e80`, `0x223454c0`, `0x223424e0`, `0x22341af0`, `0x22342ff0`, `0x22343d50`, `0x22344810`; DDX `0x22345da0`, `0x223453b0`, `0x22342380`, `0x22341a10`, `0x22342f00`, `0x22343c30`, `0x223446a0`. Seven map bindings and complete created control trees are compared. | Full ordered DDX calls and unusual DDX/error paths have no documented focused paired oracle. Map presence does not prove each mapped handler's behavior; see the rows below. |
| Intro | Init `0x22345f20`, update/capture `0x22346120/0x22346150`, Back/Next `0x22345e90/0x22345ea0`, Cancel `0x22345f00`. Factory post, transitions, checkbox state and normal teardown are exercised. | No documented focused source-to-retail and paired boundary audit for independent checkbox values and capture/update ordering; ordinary flow coverage is not an exhaustive audit. |
| Choice | Init `0x223455c0`, update/capture `0x223458a0/0x22345920`, validation/error `0x22345ab0/0x22345a40`, Back/Next `0x223454d0/0x22345500`, Next/Cancel virtual wrappers `0x22345590/0x223455a0`, route handlers `0x22345b00/0x22345b50`. Normal modes and visible/hidden focus-loss are exercised. Error handling is covered by the follow-up below. | Route, text-capacity and reentrant validation/update cases still need a focused audit; do not infer their equivalence from the normal scripts. |
| Host Options | Init `0x223425e0`, update/capture `0x22342990/0x22342aa0`, validation/error `0x22342c40/0x22342b60`, Back/Next/Settings `0x223424f0/0x22342520/0x223425d0`, virtual wrappers `0x223425a0/0x223425b0`. The completed paired audits and their specific remaining boundaries are in the preceding sections. | Back, Settings and wrappers have normal flow evidence but no separate exhaustive branch/order audit. Preserve the already documented unusual-pointer, deeper-reentry and real-owner limits; do not repeat the completed investigation. |
| Connection | Init `0x22341c40`, update/capture `0x22341e60/0x22341ef0`, Back/Next `0x22341b00/0x22341b30`, virtual wrappers `0x22341c00/0x22341c20`, provider map thunk `0x22341c10`, activation `0x22342010`. Provider success/failure and navigation have paired coverage. | Provider list boundaries, capture/update order, activation and unusual callback/reentry behavior need focused source comparison and paired cases. The notification thunk's indirect `+0xcc` dispatch is documented, not a substitute for all handler branches. |
| Browser | Init `0x22343160`, update/capture `0x22343420/0x22343480`, Back/Join `0x22343000/0x22343060`, virtual wrappers `0x223430d0/0x223430e0`, Details/Settings `0x22343110/0x22343150`, selection/rows `0x22343510/0x22343570`, idle/observer/refresh `0x22343780/0x223437a0/0x22343850`, activation/show/timer `0x223438b0/0x223438d0/0x22343910`. The completed Browser audit and timer reentry cases are above. | Keep its stated allocation, insertion, invalid-pointer, abnormal reentry, refresh-result and physical-input limits; initialization and wrappers do not have an exhaustive focused audit. Do not repeat the completed investigation. |
| Session Settings | Init `0x22343de0`, update/save `0x223440f0/0x223442a0`, OK/Cancel `0x22343d90/0x22343dc0`, callback handlers `0x22343d60/0x22343d70/0x22343d80`. Twelve block patterns, callback order, six check states, visibility and mutation checks are paired. | There is no documented exhaustive source-to-retail audit of all independent checkbox words and noncanonical values, initialization order, or callback reentry; normal scenarios and the four settings mutations cover only their stated cases. |
| Session Details | Init `0x223448c0`, update/capture `0x22344d10/0x22344f00`, validate/error `0x223450c0/0x22345020`, OK/Cancel/Remove `0x22344850/0x223448a0/0x22344830`, their map thunks `0x22345120/0x22345130/0x22345140`, refresh/selection `0x22344fa0/0x22344fd0`. The 20 paired scripts cover selected focus, capacity, refresh, selection and teardown cases. | A focused source-to-retail audit remains for independent statuses and callback results, edit-buffer boundaries, list count/selection and reentrant mutation/order, including refresh negative returns. Existing cases are meaningful but do not establish every branch or actual owner integration. |
| Help and context messages | All seven maps contain `WM_HELP` and `WM_CONTEXTMENU`; their 14 local targets and a confirmed source discrepancy are listed below. | No paired `WinHelpA` argument, return or stack/dispatch coverage is documented. This is an implementation gap independent of DirectPlay and MFC cleanup. |

Shared 0x110a helpers `0x22341620`, `0x223416a0`, `0x223417b0`,
`0x22341710`, `0x22341760`, and callback adapters `0x223417f0/0x22341800`
are exercised through the documented transitions and callbacks, but unusual
state values, destructive callback reentry and all ordering permutations are
not thereby covered. Class constructors and their local initialization callees
also remain subject to the limits in the factory row. This inventory concerns
the local factory/vtable/map surface and directly reached helpers, not every
compiler support routine or imported framework implementation.

**Choice error source differences, resolved in the follow-up below:** retail
Choice error `0x22345a40` returns 0 for field 0. For nonzero field with no
`+0x28` status it sends field 0 to virtual `+0xe0`; with status set and error
helper returning 0 it sends the original field; with nonzero helper result it
sends `0x419`. It returns the `+0xe0` result. The current
prior `fs98_mpdialog_flow_error_choice` instead sent the original field when the
status was clear, sent 0 when the helper returned 0, and returned constant 1.

The seven retail `WM_HELP`/`WM_CONTEXTMENU` pairs and their per-dialog help
mapping tables are Intro `0x22346180/0x223461a0` (`0x22348ea8`), Choice
`0x22345a00/0x22345a20` (`0x22348c60`), Host Options
`0x22342b20/0x22342b40` (`0x22348218`), Connection
`0x22341f20/0x22341f40` (`0x22348000`), Browser
`0x223434d0/0x223434f0` (`0x22348500`), Settings
`0x22344340/0x22344360` (`0x223487d0`), and Details
`0x22344f60/0x22344f80` (`0x223489e0`). All retail pairs call `WinHelpA`
with `fs98.hlp` and their table: `WM_HELP` uses the HWND at help-info `+0x0c`
and command `0x0c`; context uses the supplied HWND and command `0x0a`, then
returns 1. Both retail entries end in `ret 8`. The replacement binds all
seven pairs to a no-op help function and a context function using
`HELP_CONTEXTPOPUP` with data 0 and returning the API result; its help
prototype also accepts only one stack argument. These differences are static
evidence, not a claim that an actual help viewer was run.

**Previous next-family proposal, now completed:** Choice error `0x22345a40` is one local target. It is
smaller than a page-wide audit or the 14 help targets. The isolated no-op
slots are also single bodies, but have no behavioral branches to compare;
Choice error is the smallest outstanding behavioral family with a concrete
source discrepancy. The proposed isolated-harness cases were:
resolve and verify each binary's Choice `+0xdc` and wrap the real error helper
and `+0xe0` independently, restoring all hooks afterward. Compare helper
arguments, object `+0x8c`, forwarded field, return value and callback order:

1. Field 0, with status clear and set: expect immediate 0 and no helper or
   `+0xe0` call.
2. Nonzero field, status clear: helper is not called and `+0xe0` receives 0.
3. Nonzero field, status set: inject helper return 0 and require the original
   field; inject 1 and a distinct nonzero value and require `0x419`, including
   the stored helper result at `+0x8c`.
4. For the nonzero-field paths inject `+0xe0` results 0, 2 and -7, and compare
   the handler return. Exercise a posted `0x5f4` path to observe pending-field
   propagation, keeping synthetic return values identified as test inputs.

First capture the expected paired differences, then fix and require a clean
paired baseline and targeted mutations for the no-status, helper-zero and
return-propagation differences. If a real-alert case is added,
state its pump and modeled-owner limits; it is not required to infer that the
injected values naturally occur. See the follow-up for the resulting evidence.

**Separate acceptance limits:** matched actual-owner attempts still stop at
Wine DirectPlay `Open` returning `E_NOTIMPL`, so actual active-session 0x110c,
service `1016`, and 0x110b OK remain unobserved; this is not evidence of an
MP_DIALOG mismatch. The `+0x1c=0` actual factory-post path and repeated actual
owner unload also remain unobserved. Genuine MFC partial-Create cleanup is
unknown because the artificial failures were outside MFC; caller-level static
parity does not settle framework cleanup. The historical teardown timeout is
unexplained. Keep private prefixes, modeled owners and ignored
`bin/mpdialog-diff/` outputs isolated; do not install the partial replacement or
change the existing whole-module acceptance criteria.

### Choice error handling follow-up

Retail `0x22345a40` tests its incoming field first: zero returns 0 without
calling the error helper or `+0xe0`. With a nonzero field and descriptor
`+0x28` clear, it calls `+0xe0(0)`. With status set, it passes the callsign
control object at `+0x17ac` and the address of `+0x28` to the SIMSUI error
helper, saves its result at object `+0x8c`, forwards the original field on
helper result zero or `0x419` on a nonzero result, and returns the result of
`+0xe0` unchanged. The replacement now follows these branches. Its existing
null-block guard is not a claim of retail behavior for an invalid descriptor.

The isolated harness verifies the retail `+0xdc` address and both targets'
module ownership, verifies the retail helper import, and exercises 12 paired
vectors. Incoming zero and `0x555` are independent of the status word; the
helper returns 0, 1, 2 and -7, and `+0xe0` returns 0, 2 and -7. It compares
call counts, control/status offsets, the saved helper result, forwarded field
and returned value. The focused fixture also posts three `0x5f4` messages and
compares message return and pending word: injected downstream returns 0, 2 and
-7 propagate to pending 0, 2 and -7 respectively, while the message returns
0. These numbers are injected test values, not claims about normal helper or
focus-handler results. The original paired run is preserved in
`bin/mpdialog-diff/choice-audit/before.log`: with status clear retail forwards
0 and the replacement forwarded `0x555`; with helper return 0 retail forwarded
`0x555` and replacement forwarded 0; injected downstream 0, 2 and -7 were
all replaced by the old constant 1. Each of the three changes is independently
covered by a mutation.

A separate focused `--flow-mutation-case 6 --choice-real-modal` scenario uses
the actual retail Alerts id-2 dialog, actual SIMSUI error helper and actual
`+0xe0`, without injecting their results. The bounded modeled HOST pump
services the alert and dismisses button 2003; it explicitly reactivates the
originating Choice page after the alert, before the error handler resumes.
It does not reproduce the full HOST modal pump. Both binaries observe pending
`0x41a` before and during the alert and suppress Next; the alert result is 0,
the real helper returns 1, `+0xe0` returns `0x419`, focus returns to edit
`0x419`, and pending becomes `0x419`. A valid callsign clears pending to 0,
after which Next advances to Connection. In the pre-fix focused run the
replacement stored pending 1 and remained on Choice after the edit, while
retail recovered. The fixture creates a fresh owner window per side. A separate
run without stable foreground activation produced an extra validation dispatch
and failed the fixture's retail preconditions; it is not counted as evidence of
a binary difference. The normal full baseline uses the modeled display hook,
while the real-alert variant is explicitly selected in focused runs.

The seven dialogs' `WM_HELP`/`WM_CONTEXTMENU` targets listed above remain the
next separate implementation and differential family. Their known source
mismatches are not addressed by the Choice change. DirectPlay integration,
genuine MFC partial-Create uncertainty, the unexplained historical timeout,
the private-prefix boundary and whole-module acceptance limits remain as
previously documented.

Validation: `make -f tools/mpdialog.mk mpdialog bin/mpdialog-diff/mpdialoguidiff.exe`
built the replacement and harness. The private-prefix `--ui-breaks` run passed
the complete 24/24 flow, 24/24 settings and 20/20 details paired baseline,
all child-tree and seven rendering gates, and 14/14 stable repeats. It caught
all three new Choice mutations (`flow-choice-error-no-status`,
`flow-choice-error-helper-zero`, and `flow-choice-error-return`) independently,
as well as the preceding Host and other mutations in the log. The separate
real-alert focused run also passed and unloaded both binaries; its paired
trace is `bin/mpdialog-diff/choice-audit/after-real-alert.log`.

The broader mutation run did **not** complete: the unrelated
`flow-browser-accept-return` mutation timed out before a lifetime checkpoint,
and the runner rejected that timeout rather than counting it as detection.
The complete run output is `bin/mpdialog-diff/choice-audit/full-suite.log`.
This does not invalidate the already-passed baseline or the three Choice
mutations, and it does not explain the prior historical teardown stall. No
actual HOST/MULTIP integration was retried. `git diff --check` passed.
