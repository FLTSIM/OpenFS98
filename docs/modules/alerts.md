# Alerts — replacement DLL and differential acceptance

`English/Alerts.DLL`, module `0x80000009`, is implemented as a standalone
Win32 replacement in `bin/alerts-diff/ALERTS_ours.dll`. All three factories,
object lifecycles, event handlers, message maps, derived vtables, module
initialization and DLL attachment/detachment are rebuilt from C. The retail
module has 7,649 bytes of `.text`, three registered dialogs and three message
maps. The replacement retains HOST/GLOBAL's numeric ABI and uses the CD's
native SIMSUI/MFC42 libraries. This is the Windows compatibility stage; those
shared dependencies still need replacement for a native Linux/Mac port.

## Current acceptance

Run from the repository root, independently of MAIN's shared Makefile:

```sh
make -f tools/alerts.mk alertsevents alertsobjects alerts
bash tools/run_alertsdiff.sh 20000
bash tools/run_alertsdiff.sh 20000 0x98a1e7d3
bash tools/run_alertsdiff.sh --objects 20000
bash tools/run_alertsdiff.sh --events-unwind 2000
bash tools/run_alertsdiff.sh --objects 2000 0x587a41e9 unwind
make -f tools/alerts.mk alerts-events-breaks alerts-object-breaks N=2000
make -f tools/alerts.mk alertsuidiff-run alerts-ui-breaks
make -f tools/alerts.mk alerts-smoke
```

The runner serializes Wine jobs in its dedicated prefix,
`bin/alerts-diff/wineprefix`; outputs and mutation copies stay under
`bin/alerts-diff/`. The startup smoke boots a disposable copy of `game/`,
replacing only that copy's `English/Alerts.DLL`. It retains the copy and load
trace for inspection and stops only its own Wine prefix. The source game,
normal Wine prefix and shared Makefile are untouched.

The three complementary comparisons are:

| Suite | Acceptance | Compared behavior |
| --- | --- | --- |
| Events, seed `0x41e2753b` | 20,000 scenarios, 340,000 comparisons, 0 differences | Fourteen routines and three stateful init/answer/tick scripts |
| Events, seed `0x98a1e7d3` | 20,000 scenarios, 340,000 comparisons, 0 differences | Independent generated descriptors/objects |
| Objects, seed `0x587a41e9` | 20,000 scenarios, 460,000 comparisons, 0 differences | Constructors, destructors, deleting destructors, virtual methods and public factories |
| Event unwind | 2,000 scenarios, 34,000 comparisons, 8,597 caught throws, 0 differences | Partial CString/DIB/DC lifetime and cleanup ordering |
| Object/factory unwind | 2,000 scenarios, 46,000 comparisons, 23,651 caught throws, 0 differences | Partial subobjects, array destruction and class-specific allocation cleanup |
| Native MFC/SIMSUI | 96 GUI cases over two complete load/unload cycles | Public factories, inherited dispatch, controls, keyboard, close and pixels |
| Native module init | Eight comparisons over two load cycles | Successful/null/throwing allocation, throwing constructor, repeated deinit |
| Isolated startup smoke | 75 seconds, 27/27 modules and 95 gauges loaded, PASS | Rebuilt Alerts loaded, installed bytes match the launch snapshot, no reported runtime errors |

The synthetic acceptance totals **1,220,000 comparisons with zero differences**.

The synthetic harnesses map the actual reference with
`DONT_RESOLVE_DLL_REFERENCES`, validate its layout and replace reached
imports with controlled dependencies. They compare every object/descriptor
byte, guards, defined returns and call transcripts. Unwind runs resolve the
reference's real MSVCRT exception handlers and inject Windows exceptions at
successive dependency calls: retail's own unwind metadata executes alongside
our cleanup-only SEH frames. Double exceptions/termination are not covered.

The object suite reproduces retail's null-allocation access violation at
`0x10dc` (5,457 fault outcomes in the ordinary acceptance run), ignores
Create/Show failure returns as retail does, and distinguishes SIMSUI delete,
MFC global delete and CSimsDialog's class-specific delete. It also compares
constructor/destructor sequences and low-bit deleting-destructor flags.

The native harness loads both DLLs normally against the CD libraries and a
small host model, with a real MFC application/main-window context. It checks
that each dialog's own virtual methods belong to the expected DLL, preventing
a retail-versus-retail false pass. It compares all 40 descriptor bytes,
child classes/text/geometry/visibility/enabled state, Return/Escape outcomes,
advisory checkbox/buttons, modeless close suppression and tick destruction.
Modal/advisory Close must hide the HWND and set GWL_USERDATA to 1 before the
host stand-in destroys it; the modeless tick must destroy its HWND itself.
Modal/advisory pixels come from WM_PRINT; progress pixels come from the actual
window DC after the tick. A paint sentinel rejects untouched buffers.

Deliberate errors must produce reported differences, never merely compilation
failures, faults or timeouts: all **63 mutations are caught** (39 event,
22 object/cleanup and two native). The native color
mutation changes `0x7f0000` to `0x7e0000`: descriptor/control results still
agree while a pixel differs (127 versus 126). The other native mutation
removes the MFC allocation cleanup on a throwing module constructor.

The primary event run reaches all 256 low-byte flag combinations, 1,176
self-checked product-wrap witnesses, 17,777 successful and 2,223 failing
base initializations, 16,667 resource-string and 3,333 direct-text inputs.
The progress script also changes the vtable during Close to verify the saved
original vtable used by the following call. Synthetic coverage is broader
than the native GUI sample; neither random scenarios nor startup alone prove
every UI interaction.

`alerts_events.c` implements events, `alerts_objects.c` implements lifetime
and factories, and `alerts_runtime.c` adapts MFC/SIMSUI/Win32. The DLL exports
only `Linkage` and `ImportTable`. `gen_alerts_runtime.py` extracts 100 import
bindings, all 228 vtable slots, three message maps and the dialog registry as
ABI metadata. Dialogs, strings and bitmaps are extracted into ignored build
resources; no retail machine code or resource payload is committed. The
retail version resource is excluded. `gen_alerts_tables.py` extracts the
button-label jump table separately.

The modal init listing contains a dead default-focus branch: its stack local
starts at -1 and can only change behind a check that it is already not -1.
That branch is omitted. Live answer indices at `+0x10f4/+0x10f8` are retained
and compared.

### Findings from differential testing

- The icon selector is 1-based: tags 1, 2, 3 and 4 select `0x86`, `0x87`,
  `0x88` and `0x89`. Tag 0 skips both load and sprite copy. Other tags load
  `0x88`. The raw jump table's first target is after the load, not its first
  icon case; the first comparison caught that misreading.
- Broad random range inputs usually clamp start to `limit - 1`, leaving a
  multiplier of one. A deliberate widened-product error initially escaped.
  A self-checking fixture now uses current=total=`0x7fffffff`, start=0,
  limit=100: the low product is `0xffffff9c`, and the percentage must be 1,
  whereas a widened product would give 100. All 39 mutations now produce an
  actual reported difference; compile errors, faults and timeouts are not
  counted as caught mutations.
- SIMSUI's `SetChecked` at `0x20474440` stores its argument at checkbox
  `+0xbc` and returns the prior value. The advisory embeds that checkbox at
  `+0x1494`, so this directly explains its field at `+0x1550`; it is not an
  unrelated dialog data-exchange variable.

## Scope and integration

The standalone DLL can replace the matching retail English module. The
shared build/install targets are intentionally left to the MAIN integration
work. The real GUI acceptance runs under Wine with the CD's native MFC42 and
SIMSUI; native Windows and other MFC versions have not been exercised.
WinHelp's call is transcript-verified, but its external viewer is not driven.
Nested dialogs through the simulator's complete modal loop, arbitrary host
callbacks, and double exceptions remain outside these acceptance fixtures.
The isolated startup smoke establishes loading and initialization, not an
interactive flight or every alert reachable during one.

The sections below retain the retail boundary notes. Regenerate the broader
inventory with `python3 -B tools/dialogmap.py --format json`; tool limitations
are in [dialogs.md](dialogs.md), and the host modal/modeless contract is in
[host.md](host.md#the-dialog-engine). UTIL's callers are already implemented
in `src/util/util_alert.c`.

## Entry points and lifetime

| Entry | Address | Observation |
| --- | --- | --- |
| ImportTable | `0x22304048` | HOST (1), GLOBAL (2), zero pair |
| Linkage | `0x22304060` | Module id `0x80000009` |
| Init | `0x22301030` | Host module registration, MFC module state, `dialog.dll` load |
| Deinit | `0x223010b0` | Host deregistration, resource-library release, saved handles cleared |
| Dialog table | `0x22304018` | Three 12-byte records, then zero id |

Init calls host `+0x298` with Linkage and storage at `0x223040bc`. It then
creates MFC module state and loads `dialog.dll`, retaining its HMODULE at
`0x223040a4`. Deinit calls host `+0x29c` if `0x223040bc` is nonzero, calls
`FreeLibrary` for the saved resource module, then zeroes both handles. DLL
process-attach/detach at `0x223010f0` is a separate MFC lifecycle entry; a
harness must not conflate it with Linkage init/deinit.

| Registry id | Factory | Object bytes | Constructor | Template | Flags |
| --- | --- | --- | --- | --- | --- |
| `2` | `0x22301130` | `0x14a8` | `0x223012f0` | `0x8a` | `0` |
| `3` | `0x223011c0` | `0x11dc` | `0x22301a50` | `0x8b` | `4` |
| `4` | `0x22301250` | `0x155c` | `0x223020d0` | `0x8c` | `0` |

Each factory receives `(parent_hwnd, parameter_block)` by stdcall (`ret 8`),
allocates through SIMSUI's `ui_new`, constructs the object, stores the second
argument at object `+0x10dc`, and stores the module handle from
`0x223040bc` at object `+0x10e0`. It calls MFC42 ordinal 2086 with the template
id and a null parent, calls ordinal 6215 with 1, and returns object `+0x20`
(HWND). The supplied parent is not forwarded by these three factories.

The allocation-failure branch sets the object pointer to zero and then
continues to writes through it. Creation/show failures are not turned into a
clean error result in these listings. The object harness compares those
failure paths, including the fault address and continuation after Create/Show failure.

## Modal alert, id 2

The parameter layout from UTIL and the dialog agree:

| Offset | Meaning |
| --- | --- |
| `+0x00` | Initial zero in UTIL's descriptor |
| `+0x04` | Caller-supplied handler field (UTIL supplies a no-op) |
| `+0x08` | Button flags |
| `+0x0c` | Answer, written by button handlers |
| `+0x10` | Icon/tag selector |
| `+0x14` | Message string or string-resource id |

The text dispatch at `0x2230154c` uses a **signed** comparison with `0xffff`.
Values greater than `0xffff` are text pointers; other values go through host
`+0x2a0` into a `0x200`-byte buffer. That includes high-bit-set values, which
a naive unsigned MAKEINTRESOURCE check would classify differently. Pointer
values in a normal reference-process mapping do not usually expose this.

Initialization at `0x223014c0` scans flag bits from `1` through `0x80` in
ascending order and keeps at most three enabled bits, at object
`+0x10e4/+0x10e8/+0x10ec`. It shifts one or two choices toward the last
button positions and hides positions without a choice. Button label lookup
uses an internal jump table; do not infer return values from the translated
caption or the Windows control id.

The message map at `0x22303000`, returned by getter `0x223014b0`, contains:

| Message | Command/control id | Handler | Effect before virtual Close |
| --- | --- | --- | --- |
| `WM_COMMAND` (`0x111`), code 0 | `0x7d3` | `0x223019c0` | Copy object `+0x10e4` to parameter `+0x0c` |
| `WM_COMMAND`, code 0 | `0x7d4` | `0x223019e0` | Copy object `+0x10e8` to parameter `+0x0c` |
| `WM_COMMAND`, code 0 | `0x7d5` | `0x22301a00` | Copy object `+0x10ec` to parameter `+0x0c` |

The object vtable is `0x22303068`. Its message-map getter is at slot `+0x30`,
Init at `+0xe4`, and Close at `+0xec`. Init and Close resolve to SIMSUI's
named imports. A common empty routine at `0x223025a0` and the inherited MFC
handlers participate in Escape/Return and ordinary accept/cancel behavior;
the native harness drives the inherited keyboard routing as well as local
commands. Reading only this local map cannot establish the keyboard contract.

UTIL frees the message after the host's modal call returns, and interprets
answer 8 with flag 8 as a request to close the simulator. Therefore the
parameter write and the dialog's destruction must finish before the host
releases the module and UTIL frees the descriptor's text.

## Modeless alert and progress, id 3

UTIL allocates a `0x28`-byte descriptor and initializes the live flag, but
deliberately leaves the progress fields uninitialized:

| Offset | Observed use |
| --- | --- |
| `+0x00` | Initial zero |
| `+0x04` | Handler field (UTIL supplies a no-op) |
| `+0x08` | Icon/tag selector |
| `+0x0c` | Live flag: zero closes on the next tick |
| `+0x10` | Progress enabled; zero hides/skips progress |
| `+0x14` | Current progress, read as a signed dword |
| `+0x18` | Total progress, read as a signed dword |
| `+0x1c` | Percentage range start, also written by the tick |
| `+0x20` | Percentage range limit, also written by the tick |
| `+0x24` | Message string or resource id |

The UI's initialization at `0x22301bc0` reads the message from `+0x24`, uses
the icon selector from `+0x08`, and hides the progress control if `+0x10` is
zero. The custom tick **does not refresh the descriptor's message text** in
its listing; its live and progress fields are what it rereads.

Message map `0x22303158`, returned by `0x22301bb0`:

| Message | Handler | Observed action |
| --- | --- | --- |
| `WM_CLOSE` (`0x10`) | `0x223025a0` | No-op |
| `WM_USER+100` (`0x464`) | `0x22301db0` | Close if not live; otherwise draw progress if enabled |

The modeless object vtable is `0x223031a8`. On a zero live flag, the tick
calls virtual Close (`+0xec`, SIMSUI) and then virtual `+0x60` (a thunk to
MFC42 ordinal 2446). The runtime preserves this inherited call; the native
harness verifies that the window is destroyed before the tick returns. The tick
returns zero with `ret 8`.

On the enabled-progress path, the tick loads current and total into local
registers. It substitutes 1 for a total <= 0 and caps current to total using
a signed comparison. Those adjustments are local; they do not rewrite
parameter `+0x14/+0x18`. It then mutates the range fields:

1. If the unsigned range limit is greater than 100, write 100 to `+0x20`.
2. If the resulting limit is zero, write 1 to `+0x20`.
3. If the unsigned range start is at least the limit, write `limit - 1`
   to `+0x1c`.

The displayed percentage is calculated from range start plus
`(limit - start) * current / total`. The multiplication is **low 32 bits**,
followed by **unsigned** `div` with edx cleared. There is no lower clamp on
negative current, so an arithmetic rewrite using signed division, a widened
product or a neat 0..100 clamp would change retail. The resulting percentage
also feeds signed width calculations for drawing.

Drawing measures the progress rectangle, formats `"%3d%%"`, and renders it in
two clipped passes with foreground/background colors exchanged (white and
`0x007f0000`). The split is derived from the percentage and control width.
This needs a GDI transcript or real-pixel comparison; a parameter-only check
cannot verify clipping, positioning, colors or text metrics.

UTIL's close service first clears `+0x0c`, then calls the host's modeless
tick synchronously, then frees the text and descriptor. A faithful
replacement must stop retaining or dereferencing the descriptor before that
tick returns. Clicking the title-bar close is not a substitute: this map
explicitly ignores `WM_CLOSE`.

## Advisory, id 4

This dialog uses template `0x8c`, with a suppression checkbox and three
commands. Its initialization at `0x223022f0` takes message text from host
string id `0xf6f0`, initializes the checkbox from parameter `+0x10`, and
builds the common SIMSUI controls. The dialog object vtable is `0x223033c0`.
Map `0x22303358` is returned by `0x223022e0`:

| Command id | Handler | Observed action |
| --- | --- | --- |
| `0x7d8` | `0x22302560` | Copy object `+0x1550` to parameter `+0x10`; write 1 then 0 to parameter `+0x00`; Close |
| `0x7d9` | `0x223025b0` | `WinHelpA(hwnd, "fs98.hlp", 8, 0xbc2)` |
| `0x7da` | `0x223025d0` | Copy object `+0x1550` to parameter `+0x10`; write 0 to `+0x00`, 1 to `+0x0c`; Close |

The first command's consecutive 1/0 writes are present in retail. Do not
turn them into a presumed successful answer of 1. SIMSUI's `SetChecked` writes
checkbox `+0xbc`, which is object `+0x1550`; initialization and the command
handlers now agree on that field in the differential harness. The ordering
of descriptor writes and Close is transcript-verified, and the native harness
drives checkbox edits and both closing commands. The static caller scan does not resolve a literal call to id 4; that is not
evidence that the advisory is unused.
