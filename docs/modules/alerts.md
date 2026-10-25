# Alerts — verified event slice and remaining DLL boundary

`English/Alerts.DLL`, module `0x80000009`, has 7,649 bytes of `.text`, three
registered dialog factories and three MFC message maps. It imports HOST and
GLOBAL through FS98's numeric ABI, and USER32, GDI32, SIMSUI, MFC42, MSVCRT
and KERNEL32 through PE imports. **The event slice is now implemented and
differential-tested**, including all three initialization handlers, all three
control-binding routines, the command handlers and the modeless progress
tick. Construction, module lifetime and MFC dispatch remain to be implemented
and verified before Alerts is a drop-in replacement.

## Current acceptance

Run from the repository root, independently of the shared Makefile:

```sh
make -f tools/alerts.mk alertsevents
bash tools/run_alertsdiff.sh 20000
bash tools/run_alertsdiff.sh 20000 0x98a1e7d3
bash tools/run_alertsdiff.sh --breaks 2000
```

`tools/run_alertsdiff.sh` uses a dedicated Wine prefix at
`bin/alerts-diff/wineprefix`. It does not install Alerts, boot the simulator,
touch the normal Wine prefix, or share another module's harness outputs.
Outputs and mutation copies live under `bin/alerts-diff/`.

`src/alerts/alerts_events.c` contains the C translation. It uses the local
`types.h` integer types and Hungarian identifiers. `alerts_internal.h`
declares a dependency adapter for MFC/SIMSUI/Win32 calls; the harness provides
that adapter. No public Linkage table or factory is exported yet, so this
slice cannot accidentally be installed as a complete Alerts DLL.

The harness maps the actual reference `orig/English/Alerts.DLL` with
`DONT_RESOLVE_DLL_REFERENCES`, validates its layout, patches the dependency
IAT entries reached by the selected routines, and calls the original x86
routines beside the C translation. It does not copy original instructions
into the implementation. DLL startup and unresolved CRT paths are never
invoked. The module's exception-unwind paths are outside this slice.

Acceptance with seed `0x41e2753b`:

```text
Alerts events: 20000 scenarios, 340000 event comparisons, 0 differ
progress enabled=14084 disabled=3226 closed=2690 negative-current=6892
self-checked product-wrap witnesses=1176
init success=17777 failure=2223; text resource=16667 direct=3333
modal low-byte flag combinations reached=256/256
Alerts events mutations: 39/39 caught
```

Seed `0x98a1e7d3` also passes 20,000 scenarios and 340,000 comparisons with
zero differences: **680,000 comparisons across the two acceptance runs**.
The final mutation suite was rerun with the stateful scripts enabled and
still catches all 39 changes.

There are fourteen individually driven routines and three stateful scripts:
modal init then a button, advisory init then a user checkbox edit and an
answer, and modeless init then tick/close. The scripts preserve the object
and descriptor between operations. Each comparison checks **every byte of
the object and descriptor**, the return where defined, and the dependency
transcript: text/resources, icon loads, button setup, control bindings,
virtual Close/destroy order, coordinate conversions, text metrics, drawing
rectangles, positions, options and colors. Initialization-failure cleanup is
exercised too. The progress test can change the object's vtable during Close
to check retail's use of the saved original vtable for the subsequent call.

This verifies Alerts' logic under the stated dependency scripts. The models
do not establish real MFC dispatch, pixel equivalence, keyboard navigation,
allocation/creation failure behavior, nested host dialogs or the lifetime of
fully constructed C++ objects. Those remain whole-module acceptance work.
The control-binding model assigns child HWNDs when driven independently;
the base-init model does not simulate the full inherited exchange/dispatch
engine.

Button string ids are generated from retail's two-level jump table by
`tools/gen_alerts_tables.py` into `alerts_tables.inc`; no UI resource assets
are copied into the repository. The modal init listing contains a dead
default-focus branch: its stack local starts at -1 and can only change behind
a check that it is already not -1. That branch is omitted. The live answer
indices at object `+0x10f4/+0x10f8` are retained and compared.

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

## Remaining implementation

1. Implement and verify the three constructors/destructors, deleting
   destructors, local virtual methods, message-map getters and the progress
   child class. Preserve C++ subobject offsets and reverse cleanup order.
2. Supply real MFC/SIMSUI adapters and derived vtables/message maps using the
   recovered retail ABI. Pin stack cleanup and preserved registers from the
   dependency callees, including the varargs and by-value rectangle calls.
3. Implement Linkage init/deinit, DLL attachment/detachment and all three
   factories, with original ids/flags. Extract needed resources only into
   ignored build output, and keep the shared Makefile/game installation out
   of the work until the standalone DLL passes.
4. Exercise real creation, inherited validation/data exchange, Return,
   Escape, system close, modeless ticks and destruction through MFC. Compare
   actual UI/graphics behavior and allocation/creation failures, then run a
   separate installation smoke test with restoration.

The sections below retain the detailed retail boundary notes for that work.

Regenerate the tables and control metadata with
`python3 -B tools/dialogmap.py --format json`. The broader inventory and
tool limitations are in [dialogs.md](dialogs.md); the host's modal/modeless
contract is in [host.md](host.md#the-dialog-engine). UTIL's side of dialogs
2 and 3 is already implemented in `src/util/util_alert.c`.

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
clean error result in these listings. A future harness must compare those
failure paths instead of silently improving them.

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
handlers still need to be driven when checking Escape, Return, system close
and ordinary accept/cancel behavior. Reading only this local map cannot
establish the complete keyboard contract.

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
MFC42 ordinal 2446). Confirm the latter's exact cleanup contract in the real
runtime before replacing it with a guessed destructor or free. The tick
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
of inherited validation/Close callbacks still needs real MFC verification. The
static caller scan does not resolve a literal call to id 4; that is not
evidence that the advisory is unused.

## A useful differential harness

Keep this work independent from MAIN by giving Alerts its own build fragment,
test binaries and fixtures. The hard part is the real MFC/SIMSUI lifecycle,
not the three small command routines. A valid sequence is:

1. Run retail Alerts with real SIMSUI/MFC and an isolated host-service model,
   or observe it in an isolated simulator install. Match the module manager,
   resource-module lookup, string loading, object creation and destruction.
2. Drive initialization, every button, keyboard accept/cancel, title-bar close,
   modeless ticks and nested alert opens. Log callback order, descriptor
   writes, HWND lifetimes, host calls and graphics calls.
3. Cover all 256 low button-flag combinations, including zero and more than
   three bits; higher flag bits; direct text and resource ids; absent text;
   tags outside 0..4; initialization and allocation failures.
4. For progress, hit live/progress gates separately; total -1, 0, 1 and large
   positive values; current -1, 0, total and total+1; range limits 0, 1, 100,
   101 and `0xffffffff`; starts just below, at and above each limit; wrapped
   products and narrow/zero-sized rectangles.
5. Compare the replacement with the same script and dependency instances.
   Break answer selection, title-bar close suppression, range clamps,
   descriptor-write ordering and drawing splits on purpose, and show that the
   comparison catches each change.

The event harness above follows this plan for the explicitly scoped handlers,
including descriptor writes, dependency transcripts and stateful sequences.
It supplies controlled C++ objects and dependency models, so the factory,
real MFC lifecycle and actual rendering parts of the plan remain pending.
UTIL's already-passing harness tests the caller's protocol, not Alerts' UI.
