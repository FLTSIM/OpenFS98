# MULTIP

`MULTIP.DLL` is module 35, installed as `MULTIPLAYER.DLL`.  It owns the
DirectPlay session, the local and remote player records, network messages and
the extrapolated aircraft state handed to the rest of the simulator.

This module is in progress. The source verifies public send enqueue (`+0x20`),
internal send/drain/flush, receive/dequeue, both worker lifecycles, DirectPlay
loading/interface lifetime, and player-list/status/range queries (`+0x24`
through `+0x38`). It builds explicitly with
`make bin/MULTIP.DLL` and remains absent from `MODULES`, so `make install`
cannot replace the retail multiplayer module prematurely.

## Continuation contract

This section is the handoff for a fresh context. The current project decision
for new MULTIP code is **snake_case**, the standard `<stdint.h>` types
(`uint32_t`, `int16_t`, and so on), and **no `types.h`**. This supersedes stale
instructions that request Hungarian names or the old typedefs. Preserve the
retail offsets and ABI, but use descriptive snake-case names where semantics
are known and offset-based names where they are not.

The working files are:

- `src/multip/multip.c`, `.def`, and `.rc`: the in-progress DLL;
- `tools/difftest/multipdiff.c`, `multip_send.inc`, `multip_receive.inc`, and
  `multip_workers.inc` and `multip_directplay.inc`: query, transport, controlled
  worker, and loader/interface-lifetime harness;
- `tools/multip_breaks.py`: isolated transport, worker, and DirectPlay mutations;
- the `multipdiff`, `multipdiff-run`, and explicit `bin/MULTIP.DLL` targets in
  the top-level `Makefile`.

Do not add MULTIP to `MODULES` or install it into the game until all public
slots, both worker lifecycles, and the CHAIN callbacks are implemented and a
smoke test passes. The present failure stubs return `0xffffffff`; they are a
development guard, not plausible retail behavior.

## Session, aircraft and message continuation

Additional implementation lives in `multip_profile.inc`, `multip_session.inc`,
`multip_aircraft.inc`, `multip_records.inc`, `multip_tracking.inc`,
`multip_messages.inc` and `multip_players.inc` under `src/multip/`.
The differential fixtures with the same names live under `tools/difftest/` (record enumeration shares the aircraft
copy fixture). The generated profile field order comes from
`tools/gen_multip_profile.py`; initialized image data and pointer fixups come
from `tools/gen_multip_data.py`. Neither generator embeds executable code.

The following internal paths are now implemented and compared against retail:

- Profile read/write at `.text+0x8f30/+0x93e0`: 33 reads and 25 writes, with
  unsigned clamps and the derived packet interval. Retail spells the key
  `ALLOW_EXTURE_RECEIVE`; its player-list update key writes `+0x174` while
  validation changes `+0x178`. Both behaviors are retained.
- Host/join and named groups at `+0x2340/+0x2380/+0x23c0/+0x2400`,
  `+0x3890/+0x3940`, membership swaps at `+0x97a0/+0x97f0`. Enumeration treats
  only three specific HRESULTs as failure; membership changes ignore errors.
- Session-description allocation at `+0xa7a0`, capacity packing/unpacking
  at `+0x9f40/+0xa020`, name type/host encoding at `+0xa050/+0xa100`, local
  name publication at `+0xa1a0`, collision timer at `+0xa590`, and type counts
  at `+0xa8d0`. Name publication leaks its allocation on success as retail does.
- Aircraft selection/release/copy at `+0x8a20/+0x8aa0/+0x8b30`. Copy preserves
  partial reallocations, a zero-filled replacement header, and separately
  allocated model title. ACONTAIN uses nonzero success, host memory services
  use zero success. The fixtures include both ownership modes and failures.
- Browser record construction/copy/free, name-list cleanup and player
  enumeration at `+0xa2a0` through `+0xa540`. An invalid final name leaves
  its appended record in the list; EnumPlayers errors still publish partial
  results. Cleanup ignores a walker error and frees through a temporary.
- Track-name display and CHAIN `0x19` callback at `+0x96b0/+0x9710/+0x9740`,
  selection reset/get/apply at `+0x8cb0/+0xa830/+0xa6b0`, view guard at
  `+0xa680`, and local model references at `+0x9ea0`. The callback preserves
  the argument as its return when it skips drawing. View application preserves
  the different state destinations, word writes, and copy-error boundaries.
- Fourteen message constructors at `+0x5a80` through `+0x5f30`:
  tags `1001,1008,1002,1009,1003,1004,1005,1006,100f,1010,1011,100a,100c,100e`.
  Tag `100e` carries a variable title, its terminator, and a visibility byte.
  Four set request-pending/time only after successful enqueue; three allocate
  an extra packet and leak it on enqueue failure. The unused tails of retail's
  1000-byte stack buffers are omitted because they are never sent.

The private test table now has entries 0–78; `send_internal` in
`tools/difftest/multip_send.inc` maps each to its retail RVA. Entries 25–33
are profile and group/session helpers, 34–41 description/name/counter helpers,
42–44 aircraft ownership, 45–50 browser records/enumeration, 51–58 tracking
and selection, 59–72 message constructors, and 73–78 player ownership
helpers. Production exports still contain only ImportTable and Linkage.

New comparison labels are `profile load/save`, `session/group primitives`,
`session descriptors/name metadata`, `aircraft select/release`,
`aircraft copy/session browser records`, `track-name display`,
`application message constructors`, and `player lifecycle helpers`. They compare
ordered callbacks/imports, outputs,
partial state, heap bytes, failure leaks and queue contents as applicable.
Real critical sections/events are used for message enqueueing; the worker
suite runs last because it replaces the Win32 import cells.

Mutation checking accepts `--slice profile`, `groups`, `metadata`, `aircraft`,
`records`, `tracking`, `messages`, and `players` in addition to the original
slices. Each variant runs 1,200 scenarios so independently generated invalid/valid name
bytes reach all type characters. Constructor snapshots must be saved before
later enumeration resets the embedded list; a stride mutation exposed this
fixture gap and now fails the comparison.

The expanded fourteen-suite differential run passes 20,000 scenarios per suite
with zero differences. `make multipdiff` builds both production and test DLLs
without warnings; the production export table contains only ImportTable and
Linkage. These remain slice results, not whole-module acceptance. The
106-mutation full sweep passed before adding the player helpers; all nine
additional player mutations then passed their isolated checks (115 caught
mutations across those runs). No crash or fixture error counts as a catch.

Remote creation/removal and list ownership at `+0x82c0/+0x84b0/+0x8930`,
unknown-type initialization at `+0x8980`, local entry removal at `+0xa8a0`,
and remove-all at `+0x8f00` now pass the fourteenth suite. The small name-label
model at RVA `0x2cf4` is extracted by the data generator into
`multip_nametag.inc`. The fixture retains moving-array banks so retail's stale
local pointer is observable, and checks per-action live-record hashes, heap
bytes/ownership, partial setup, duplicate IDs, model failures, host capacity
updates, selected-player reset, and CHAIN list publications. Local creation
at `+0x8820` remains pending.

Public init/deinit, session toggle, and dialog entries remain disabled. The
next dependencies are local record creation (`+0x8820`), variable-size
application messages (starting `+0x59d0`; `+0x5d20` is implemented), receive
dispatch and multipart traffic,
lobby/provider/dialog callbacks, and the main CHAIN callbacks. Local creation
leaves DPNAME's reserved flags word uninitialized; inspect that boundary when
building the CreatePlayer fixture. Do not claim whole-module coverage or
install this DLL until those paths are recovered and the acceptance gates
below pass.

## Image and public interface

Retail is based at `0x20850000`.  It has 43,318 bytes of `.text`, 0xa00 bytes
of initialised `.data`, and uses the rest of the `.data` page for zero-filled
runtime state.  `ImportTable` is at `.data+0x000` and `Linkage` at `+0x058`.
The imports and their pointer cells are:

| Import-table offset | Pointer offset | Module |
| --- | --- | --- |
| `+0x00` | `+0x04` | host |
| `+0x08` | `+0x0c` | GLOBAL |
| `+0x10` | `+0x14` | ACONTAIN |
| `+0x18` | `+0x1c` | CHAIN |
| `+0x20` | `+0x24` | HG2D |
| `+0x28` | `+0x2c` | G3D |
| `+0x30` | `+0x34` | MAIN |
| `+0x38` | `+0x3c` | NUMERIC |
| `+0x40` | `+0x44` | UTIL |
| `+0x48` | `+0x4c` | WEATHERM |

| Slot | Retail address | ABI/meaning | Port |
| --- | --- | --- | --- |
| `+0x04` | `.text+0x0020` | no arguments; register module and first CHAIN `0x14` callback | stub |
| `+0x08` | `.text+0x0070` | no arguments; unregister the host module handle | stub |
| `+0x18` | `.text+0x8e60` | no arguments; load transport and start/join/stop a session | stub |
| `+0x1c` | `.text+0x39a0` | no arguments; options/session dialog entry | stub |
| `+0x20` | `.text+0x9840` | stdcall `(to_id, payload, size, flags)`; enqueue an application message | exact enqueue |
| `+0x24` | `.text+0x9de0` | no arguments; return `{count, records}` | exact |
| `+0x28` | `.text+0x8680` | stdcall `(player_id)`; return a player record | exact |
| `+0x2c` | `.text+0x8690` | no arguments; return local-player record | exact |
| `+0x30` | `.text+0xa560` | no arguments; return multiplayer status bits | exact |
| `+0x34` | `.text+0xa5b0` | stdcall `(int32_t *result)`; conditional MAIN result override | exact |
| `+0x38` | `.text+0xa5f0` | stdcall `(int32_t *result)`; selected aircraft range | exact |

The functions are widely separated because Linkage points to public entries
among many internal MSVC functions; the byte counts printed by
`dumplinkage.py` are gaps between public entries, not function sizes.

## Player records

Remote records are 0x100 bytes.  The live count is at `.data+0x660` and the
allocated array pointer at `+0x678`.  The local record pointer is at `+0xa3c`.
The first fields established so far are:

| Offset | Meaning |
| --- | --- |
| `+0x00` | name pointer; MPCHAT displays this directly |
| `+0x04` | DirectPlay player ID |
| `+0x08` | record type (`1` and `2` are counted separately internally) |
| `+0x0c` | state flag; zero on a type-1 record sets status bit 2 |
| `+0x28` | associated remote-aircraft state; tested by `+0x34` and `+0x38` |

The local-record refresh at `.text+0x9ea0` also establishes the following
fields. Their deeper meaning is not yet required by the implemented slice, so
the source data is more reliable than speculative names:

| Offset | Source written by retail |
| --- | --- |
| `+0x20` | `GLOBAL[GLOBAL+0x11e0]+0x04` |
| `+0x24` | `GLOBAL[GLOBAL+0x11e0]+0x08` |
| `+0x28` | pointer at `GLOBAL+0x11e4` |
| `+0x2c` | `GLOBAL[GLOBAL+0x11e0]+0x18` |
| `+0x30` | `GLOBAL[GLOBAL+0x11e0]+0x0c` |
| `+0x34` | literal `1` |
| `+0x38` | zero-extended byte at `GLOBAL+0x609` |

`+0x28` tests the local record first, then searches the remote array backwards.
That reverse order is observable if duplicate IDs exist and is covered by the
differential test.  `+0x24` writes the count and pointer to the stable two-dword
result at `.data+0xef8` and returns that result's address.  DYNAMIC consumes
exactly this shape and filters records whose `+0x08` is `1` and `+0x0c` is
zero.

The `+0x30` result is a mask:

| Bit | State |
| --- | --- |
| `0` | the DirectPlay loader succeeded (`.data+0xa48`) |
| `1` | a session is connected (`.data+0xaac`) |
| `2` | while connected, at least one type-1 remote record has `+0x0c == 0` |

MAIN uses these bits to enable and check its multiplayer menu items.

`+0x34` always returns zero.  It writes `-1` through its sole argument only
when a session is connected, `DISTANCE_DISPLAY` is enabled, the player mode is
1, the active player ID resolves, and that record's `+0x28` is nonzero. MAIN's
caller makes this a result override; “collision result” is the current
interpretation, not a confirmed field name.

`+0x38` also always returns zero and leaves its output unchanged unless the
session is connected, `DISTANCE_DISPLAY` is enabled, the player mode is
nonzero, the active ID resolves, and the record has an aircraft-state pointer
at `+0x28`. It asks UTIL `+0x64` for the distance between `GLOBAL+0x560` and
aircraft-state `+0x04`, using flags 3, then writes the truncated value of
`distance * 0.0005396115152459395 * 10`. The two multiplications and MSVCRT
`_ftol` truncation are reproduced with x87 operations. The current source name
`M_AIRCRAFT_ACTIVE` for `.data+0xab4` is provisional and should eventually be
renamed to `M_DISTANCE_DISPLAY`; the profile-key mapping proves the latter.

## Confirmed runtime state

The initialized `.data` ends at `+0x9ff`, but the loader maps the complete
page. Retail uses both the initialized tail and the zero-filled remainder.
Keep the offsets stable because the differential harness seeds them directly.

| Data offset | Confirmed or bounded meaning |
| --- | --- |
| `+0x174` | session-list update time and destination passed for the player-list read |
| `+0x178` | player-list update time value subsequently range-checked by retail |
| `+0x660` | remote-record count |
| `+0x678` | pointer to the contiguous 0x100-byte remote-record array |
| `+0x980` | loaded `DirectPlayLobbyCreateA` export |
| `+0x984` | session/open transition flag; exact public meaning unresolved |
| `+0x988` | live `IDirectPlay3A *` |
| `+0x98c` | loaded `DirectPlayCreate` export |
| `+0x990` | loaded `DirectPlayEnumerateA` export |
| `+0x998` | configured maximum players, clamped to 1..255 (zero becomes 8) |
| `+0x99c` | configured maximum observers, clamped to at most 255 |
| `+0x9a0` | live count of type-1 records |
| `+0x9a4` | live count of type-2 records |
| `+0x9a8` | player-name buffer, profile read with capacity 0x13 |
| `+0x9c0` | 0x50-byte session/open descriptor scratch block |
| `+0xa10` | configured maximum track-name display time, clamped to 5..30 |
| `+0xa34` | host module-registration handle/output |
| `+0xa38` | value returned by host `+0x58`, used in session descriptors |
| `+0xa3c` | local 0x100-byte player-record pointer |
| `+0xa40` | local DirectPlay player-created flag |
| `+0xa44` | successful DirectPlay interface-query flag |
| `+0xa48` | DirectPlay loader succeeded; status bit 0 |
| `+0xa5c` | player mode; zero disables selected-player lookup |
| `+0xa60` | selected/active DirectPlay player ID |
| `+0xa6c` | `CONNECTION_TYPE` option |
| `+0xa70` | `PROTOCOL_SELECTION` option |
| `+0xaa0`, `+0xaa4` | DirectPlay group IDs used by add/remove helpers |
| `+0xaa8` | `HOST_SESSION` option/current host-session flag |
| `+0xaac` | connected/open-session flag; status bit 1 |
| `+0xab0` | `ADF_TRACK` option |
| `+0xab4` | `DISTANCE_DISPLAY` option |
| `+0xab8` | `AUTOPILOT_LOCK` option |
| `+0xabc`, `+0xac0` | descriptive-message send/receive options |
| `+0xac4`, `+0xac8` | texture send/receive options |
| `+0xacc`, `+0xad0` | aircraft-model send/receive options |
| `+0xae0` | session-name buffer, profile read with capacity 0x1d |
| `+0xb00` | 0x104-byte internet-address/path buffer |
| `+0xf00` | `GUARANTEED_MESSAGES` option |
| `+0xf1c` | derived value `18 * maximum_track_name_display_time` |
| `+0xf24` | both worker subsystems started flag |
| `+0xf28` | one-byte checksum/cache used by an aircraft update path |
| `+0xef8` | stable `{remote_count, remote_records}` result block |

Offsets not listed here are not free. Large initialized descriptor tables run
through `+0x93f`, and transport, interpolation, queue, and dialog state occupy
much of `+0x940..+0xeff`. Name an unknown by offset until its reads and writes
establish its role.

## Initialization and CHAIN lifecycle

The public init/deinit entries are small, but init exposes two larger callback
paths and therefore must not be implemented with a dummy callback.

- `.text+0x0020` calls host `+0x298(Linkage, &data[0xa34])`, saves host `+0x58`'s
  result at `+0xa38`, and adds `.text+0x0090` to CHAIN list `0x14`.
- `.text+0x0070` calls host `+0x29c(data[0xa34])`. It does no other teardown.
- The first callback at `.text+0x0090` recognizes messages `0x36` and `0x3a`.
  Message `0x36` loads the profile at `.text+0x8f30`, fills the 0x104-byte
  address/path buffer through host `+0x2a0`, registers the second callback,
  starts the lower-level subsystems through `.text+0x9e00`, then enters the
  lobby/connection path at `.text+0x9990`. Message `0x3a` deep-copies and
  relocates a supplied compound block, conditionally passing it to
  `.text+0x9a20` when DirectPlay is loaded.
- The second callback at `.text+0x0220` recognizes message `0x37`. It runs the
  disconnect path at `.text+0x8cf0`, saves the profile at `.text+0x93e0`,
  tears down supporting state, and calls `.text+0x9e20`.

Those message numbers and call boundaries are confirmed; descriptive event
names are intentionally deferred until the CHAIN producer is traced. A first
init test should log registration, dispatch `0x36`, `0x37`, and `0x3a`, and
compare every host/CHAIN call and modified state block with retail.

The profile section name is `MULTIPLAYER`. The loader at `.text+0x8f30` and
saver at `.text+0x93e0` cover these keys:

| Key | Primary data destination |
| --- | --- |
| `ALWAYS_SHOW_INITIAL_DIALOG` | `+0x180` |
| `SESSION_NAME` | `+0xae0` |
| `HOST_SESSION` | `+0xaa8` |
| `PLAYER_NAME` | `+0x9a8` |
| `GUARANTEED_MESSAGES` | `+0xf00` |
| `EXTRAPOLATION` | `+0x184` |
| `MAX_PLAYERS` / `MAX_OBSERVERS` | `+0x998` / `+0x99c` |
| `TICKS_PER_SYNC` / `PACKETS_PER_SEC` | `+0x164` / `+0x1a8` |
| `COLLISION_SECS` / `FOLLOWING_MULTIPLIER` | `+0x160` / `+0x168` |
| `MAX_TRACK_NAME_DISPLAY_TIME` | `+0xa10` |
| `MAX_SEGMENT_SIZE` / `MAX_SEGMENTS_PER_SEC` | `+0x16c` / `+0x170` |
| `SESSION_LIST_UPDATE_TIME` | read into and validated at `+0x174` |
| `PLAYER_LIST_UPDATE_TIME` | retail passes `+0x174` to the reader, then validates `+0x178` |
| `CONNECTION_TYPE` / `PROTOCOL_SELECTION` | `+0xa6c` / `+0xa70` |
| `INTERNET_ADDRESS` | `+0xb00` |
| `ALLOW_DESCRIPTIVE_SEND` / `RECEIVE` | `+0xabc` / `+0xac0` |
| `ALLOW_TEXTURE_SEND` / `RECEIVE` | `+0xac4` / `+0xac8` |
| `ALLOW_PLANE_MODEL_SEND` / `RECEIVE` | `+0xacc` / `+0xad0` |
| `ADF_TRACK` / `DISTANCE_DISPLAY` / `AUTOPILOT_LOCK` | `+0xab0` / `+0xab4` / `+0xab8` |
| `ALLOW_NAME_TAG_DISPLAY` / `ALLOW_KEEPALIVE` | `+0x188` / `+0x18c` |
| `TIME_BETWEEN_PLAYER_TYPE_QUERIES` | `+0x17c` |
| `TIME_BETWEEN_CONNECTION_ATTEMPTS` | `+0x194` |

The mismatched destination and validation for `PLAYER_LIST_UPDATE_TIME` are
visible in the retail instructions and may be a retail bug; reproduce them
unless the host service's contract proves it writes a larger overlapping type.

## Transport and worker boundaries

The public send entry at `.text+0x9840` is only a four-argument stdcall wrapper
around `.text+0x87e0`. The latter applies the `GUARANTEED_MESSAGES` option and
passes the request to `.text+0x7890`, which allocates a 0x20-byte queue record,
copies the payload, chooses one of two send queues, and signals an event.
`.text+0x79d0` removes one queued record and calls `IDirectPlay3A::Send`; it
retries `DPERR_BUSY` (`0x8877010e`) without discarding the record.

The enqueue path is now ported. Its record layout is `{to_id, payload, size,
flags, reserved[4]}` (0x20 bytes). Each queue is `{count, head, tail,
CRITICAL_SECTION}` (0x24 bytes); its 8-byte nodes hold `{record, next}`.
Appending holds the queue lock while allocating and linking the node.

Retail only ORs flag bit 0 when `GUARANTEED_MESSAGES == 1`, and selects queue
`+0xe60` for **any nonzero flags**, otherwise `+0xe64`. It does not inspect
`CONNECTED`. Payload allocation and copying happen even for zero length.
Record, payload, copy, or queue failures return `0xffffffff` without cleaning
up earlier allocations. Node allocation failure additionally leaves the queue
lock held. A failed `SetEvent` also returns failure, but the record is already
queued. The port preserves these observed orderings. Host allocation flag 1
normally makes allocation exhaustion fatal; the harness injects nonzero host
returns to check the branches that remain in the retail code.


Send-one (`.text+0x79d0`), normal draining (`+0x7970`), and full flushing
(`+0x7aa0`) are now ported and tested against retail. Send-one removes the
queue node before calling `IDirectPlay3A::Send`, using the local record's
`+0x04` as from-ID. Only `DPERR_BUSY` retries; all other HRESULTs, including
generic errors, proceed to freeing the payload and record and return success
if cleanup succeeds. Node-free failure returns before sending and leaves the
record/payload allocated; payload-free failure leaves both allocated;
record-free failure leaves the record with its payload pointer cleared.
The queue has already advanced in each case, and its lock is released even
when node freeing fails.

Normal draining empties plain queue `+0xe64` before sending **at most one**
flagged message from `+0xe60`. Full flush empties both queues in that order.
A missing queue counts as empty, but send-one on a missing queue returns
failure, so a normal drain with a missing flagged queue fails whereas a full
flush can succeed. Empty, non-null queues succeed without sending.

The receive helper at `.text+0x5820` calls `IDirectPlay3A::Receive` at
`.text+0x5875`. Its work record contains from-ID, to-ID, a buffer pointer and
buffer size. On
`DPERR_BUFFERTOOSMALL` (`0x8877001e`) it allocates or resizes through host
`+0x20c`/`+0x210` and retries. A successful record is queued with the value
returned by host `+0x38`, after which higher-level message dispatch owns it.

Receive-one (`.text+0x5820`) and receive-dequeue (`+0x5980`) are ported.
A receive record is 0x14 bytes: `{from_id, to_id, payload, size, timestamp}`.
Each receive-one starts with a fresh zero-filled record and calls `Receive`
with flags 1 and a null buffer/zero capacity. `DPERR_BUFFERTOOSMALL` allocates
the first buffer and reallocates on subsequent growth, retrying until another
HRESULT arrives. A successful result sets the caller's received flag to 1,
reads host `+0x38` into `timestamp`, and appends to queue `+0xdf8`.

Other receive HRESULTs, including no-message and generic errors, free the
buffer (if present) and record and return zero with received=0. Allocation,
reallocation, free and queue failures return -1; earlier allocations remain
where retail leaves them. In particular, successful receive followed by queue
append failure leaves received=1, and node allocation failure leaves the queue
lock held. Receive-dequeue sets its received flag only after a successful queue
take; node-free failure can return a non-null record while leaving that flag
zero. Dequeued records and their payloads belong to the higher-level consumer;
these helpers do not free them.

There are two independent event/thread subsystems:

| Role | Setup | Teardown | Start | Stop | Thread entry | Main unit of work | State |
| --- | --- | --- | --- | --- | --- | --- | --- |
| receive | `.text+0x56a0` | `+0x56e0` | `+0x5730` | `+0x5770` | `+0x57a0` | `+0x5820` | events `+0xdf0/+0xdf4`, queue `+0xdf8`, thread `+0xdfc`, id `+0xe00`, running `+0xe04` |
| send | `.text+0x7700` | `+0x7750` | `+0x77b0` | `+0x77f0` | `+0x7840` | enqueue `+0x7890`, drain `+0x7970`, send `+0x79d0` | events `+0xe58/+0xe5c`, queues `+0xe60/+0xe64`, thread `+0xe68`, id `+0xe6c`, running `+0xe70` |

Both worker lifecycles and queue construction/clear/destruction are ported
and verified. Setup creates the work event before the stop event, both
unnamed, auto-reset and initially unsignalled. It ignores event failures and
returns only queue construction failures, without rolling back partial setup.
Receive builds one queue; send builds flagged then plain queues. Teardown
reverses queue construction order, then closes work and stop events, returning
on the first failure. Repeated setup overwrites prior pointers and handles;
teardown leaves event cells unchanged.

Start sets its running flag before `CreateThread` and returns zero even if
creation fails, leaving running=1. Repeated starts do nothing while that flag
is set. The security attributes at data `+0x538` and `+0x548` are `{12, 0, 0}`.
Both entries wait on stop/work handles with `WaitForMultipleObjects`,
wait-all=false and timeout=INFINITE. Stop clears running; work drains receive
until no message remains, or invokes normal send-drain once. Other wait
results loop again. A receive error exits with -1 without clearing running;
the send worker ignores drain failures.

Receive stop signals its stop event, ignores the result of the 100 ms wait,
and retains its thread handle. Send stop first clears flagged then plain
queues, signals stop, ignores the 100 ms wait result, and returns the thread
handle close result. Queue clear frees the record and node for each entry,
leaving record-owned payloads allocated. It advances head before freeing,
retains its lock on a free failure, and clears count/tail only after complete
success. Queue destruction then deletes its critical section and frees the
queue, returning immediately if either clearing or freeing fails.

The combined wrappers are also ported: `.text+0x9e00` sets up receive then
send, and `+0x9e20` tears down send then receive. `+0x9e40` starts send then
receive only while data `+0xf24` is zero, and sets that shared flag after both
calls. `+0x9e70` stops receive then send while the flag is nonzero, and clears
it only after both calls succeed. They do not roll back failures. Public
init/session entries remain stubs, so these workers are not yet started by
normal game calls.

The observed Win32 imports are `InitializeCriticalSection`,
`DeleteCriticalSection`, `EnterCriticalSection`, `LeaveCriticalSection`,
`CreateEventA`, `SetEvent`, `CloseHandle`, `CreateThread`,
`WaitForSingleObject`, `WaitForMultipleObjects`, `Sleep`, `wsprintfA`,
`ShellExecuteA`, and MSVCRT `_ftol`. `ShellExecuteA` is called at
`.text+0x41d8` with the runtime buffer at `+0xb00`; a harness must log and
suppress it.

## DirectPlay interface

The main object at `.data+0x988` is conclusively `IDirectPlay3A`, not a custom
COM-shaped interface.  The evidence is both static and structural:

- `.rdata+0x070` is byte-for-byte `IID_IDirectPlay3A` and is passed to
  `QueryInterface` while filling `.data+0x988`.
- Calls through the object use the inherited `IDirectPlay2A` vtable at the
  documented offsets: `AddPlayerToGroup` `+0x0c`, `Close` `+0x10`,
  `CreateGroup` `+0x14`, `CreatePlayer` `+0x18`, `DeletePlayerFromGroup`
  `+0x1c`, `DestroyGroup` `+0x20`, `DestroyPlayer` `+0x24`,
  `EnumGroups` `+0x2c`, `EnumPlayers` `+0x30`, `EnumSessions` `+0x34`,
  `GetPlayerName` `+0x54`, `GetSessionDesc` `+0x58`, `Open` `+0x60`,
  `Receive` `+0x64`, `Send` `+0x68`, `SetPlayerName` `+0x78`, and
  `SetSessionDesc` `+0x7c`.
- The usual `QueryInterface` (`+0x00`) and `Release` (`+0x08`) calls bracket
  its lifetime.

The loader boundary is also known. A descriptor at `.data+0x148` names the
three output cells `+0x98c`, `+0x990`, and `+0x980`; host `+0x124` fills them
at `.text+0x9cf0`, and host `+0x128` unloads them at `.text+0x9d20`.
`.text+0x9d50` calls `DirectPlayCreate` at `+0x98c`, queries the temporary
object first with `IID_IDirectPlay2A` at `.rdata+0x050` and then with
`IID_IDirectPlay3A` at `.rdata+0x070`, stores the latter at `+0x988`, and
releases the temporary interfaces. `DirectPlayLobbyCreateA` at `+0x980` is
used only by the lobby/connection path after host `+0x120`; the observed lobby
object calls `Release` at vtable `+0x08` and `SetConnectionSettings` at
`+0x30`.

The loader/interface-lifetime helpers above are now ported and verified. The
host boundaries are stdcall with one argument: host `+0x124` at `0x413570`
and host `+0x128` at `0x413620` both end in `ret 4`. The module's descriptor
contains three addresses of output cells, with no names or terminator; host
code owns the DLL and export names. The port initializes those addresses in
`multip_install` to preserve relocation and the data `+0x148` ABI.

Load normalizes any nonzero host result to -1. On host success, it sets loaded
flag `+0xa48` to 1 only when `DirectPlayEnumerateA` is non-null. An absent
enumerator, or load failure, leaves a previous flag unchanged. Other exports
are not checked here. Unload releases a non-null `+0x988` interface, clears its
cell, and then calls the host unloader; it leaves both loaded `+0xa48` and
query-success `+0xa44` flags unchanged, regardless of the host return.

Create (`.text+0x9d50`) takes a provider GUID pointer (nullable), clears query
flag `+0xa44`, and calls `DirectPlayCreate(provider, &created, NULL)`. Factory
failure returns -1 without releasing any partially returned object. The first
query, for `IID_IDirectPlay2A`, also returns -1 on nonzero HRESULT without
releasing the factory reference. Both IID queries use the **factory object**;
the first queried interface is only retained for later release. The second
query writes directly to global `+0x988`. Zero sets query flag=1, but a nonzero
second-query result is not propagated: both temporary interfaces are released
(first queried, then factory), and the function returns zero. Query output
writes on failure are not repaired, and a pre-existing global interface is not
released before replacement. These are retail behavior, not cleanup fixes.

This reduces the transport model to a well-defined fake `IDirectPlay3A` plus
a very small fake `IDirectPlayLobbyA`.
The receive loop treats `DPERR_BUFFERTOOSMALL` (`0x8877001e`) as a resize and
retry, while the send loop retries `DPERR_BUSY` (`0x8877010e`).  The first
fake can therefore implement only the methods
listed above, scripted sessions/players, and a deterministic receive queue.

The fake should expose call logs as data, not assertions buried inside COM
methods. At minimum each scripted call records the method, object identity,
player/group IDs, flags, buffer size, and relevant input bytes, then returns a
queued HRESULT and outputs. It needs:

- export shims for `DirectPlayCreate`, `DirectPlayEnumerateA`, and
  `DirectPlayLobbyCreateA`;
- reference counting and both observed `QueryInterface` IIDs;
- lobby `Release` and `SetConnectionSettings` behavior;
- deterministic ID allocation for `CreateGroup` and `CreatePlayer`;
- scripted `EnumSessions`, `EnumGroups`, and `EnumPlayers` callbacks;
- mutable player names and session descriptors for the get/set pairs;
- group membership for `AddPlayerToGroup`, `DeletePlayerFromGroup`,
  `DestroyGroup`, and `DestroyPlayer`;
- a receive queue capable of the buffer-too-small first pass;
- a send script capable of one or more busy results before success;
- `Open` and `Close` state with injectable failures at each stage.

Use the real retail DLL on one side and the same fake object/script on the
OpenFS98 side. Normalize allocated pointers, COM object addresses, handles,
thread IDs, and callback addresses before comparing results.

## Verification

`make multipdiff-run` defaults to 20,000 deterministic randomized scenarios.
It loads retail and OpenFS98 together, seeds equivalent local and remote
records plus fake GLOBAL and UTIL imports, and compares normalized results for
the recovered `+0x24` through `+0x38` slice: the result block, local and remote
lookups, reverse duplicate-ID selection, missing IDs, all three status bits,
active/inactive collision overrides, every active-range guard, the UTIL call
shape, and the range conversion.  The current result is 20,000 scenarios with
zero differences.

A separate 20,000-scenario run covers `+0x20` enqueue. Each scenario sends six
messages through real Win32 critical sections and an auto-reset event backed
by a deterministic fake host heap. It compares return values, per-send event
signals, ordered allocation/copy calls, both queue chains, payload bytes,
reserved fields, leaked partial records, and lock recursion counts. Inputs
cover zero-length payloads, flags 0/1/2/random, guaranteed option values 0..3,
connected/disconnected state, missing queues, invalid event handles, and
injected failures at each host-call position after two initial sends. Caller
buffers are overwritten after each send to check that queued data is copied.

A third 20,000-scenario suite invokes send-one/drain/flush directly, after
enqueueing zero to six messages. A fake `IDirectPlay3A::Send` at vtable `+0x68`
logs object identity, from/to IDs, flags, sizes, payload bytes and scripted
HRESULTs, interleaved with host allocation/copy/free calls. The suite compares
live allocations, remaining queue chains, return values and queue counts
after each drain step, including repeated flushes, multiple messages in each
queue, zero to three busy retries, success and generic errors, missing queues,
and failures at node, payload and record frees. Full-flush cases begin with
both queues populated to check ordering independently of normal draining.

Retail's private helpers are called at loaded-image RVAs `0x89d0`, `0x8970`
and `0x8aa0`; receive-one and dequeue use `0x6820` and `0x6980`.
`make multipdiff` builds a separate `MULTIP_test.dll` with
`MULTIP_DIFFTEST`, exposing the `multip_test_entries` address table. The normal
`bin/MULTIP.DLL` exports only `ImportTable` and `Linkage`; no public slots are
added to reach internal helpers. Staging copies use temporary files and atomic
renames so rebuilding a fixture cannot truncate a DLL already mapped by Wine.

`make multip-send-breaks` builds temporary source mutations and DLLs in `/tmp`.
The unmodified baseline must pass, then nine mutations must produce completed
delivery comparisons with differences: busy retry, ignored HRESULT, send
arguments, payload cleanup, the plain drain loop, the flagged drain limit,
dequeue tail, full-flush ordering, and node-free failure. Crashes, timeouts and
fixture errors do not count as catches.

A fourth 20,000-scenario suite covers receive-one/dequeue with three receive
attempts and four dequeue attempts per scenario. The fake `Receive` logs the
input IDs, capacity, flags and buffer identity as well as output IDs, required
sizes, HRESULTs and payload bytes. Cases include immediate zero-byte success,
no-message/generic errors, one to four buffer-growth passes, moving realloc
with preserved contents, timestamp values, missing queues, and injected
allocation/reallocation/free failures. Snapshots compare queue counts after
each receive, dequeue pointers and flags, the complete call transcript, live
allocations, leaked partial records, and held locks. The fake heap records
freed storage for comparison but does not reuse it within a scenario.

`make multip-breaks` checks all forty-two transport/worker/DirectPlay mutations; the send-only
alias is `make multip-send-breaks` and the receive-only alias is
`make multip-receive-breaks`. The six receive mutations cover retry, flags,
timestamp, successful-receive flag, error cleanup, and dequeue flag behavior.
`make multip-worker-breaks` checks sixteen lifecycle mutations: event order,
repeated start, thread creation failure result, security attributes, receive
handle retention, shutdown wait duration, send handle close, send queue clear,
ignored send errors, receive-until-empty, running flag after receive failure,
record freeing, held locks on clear failure, critical-section deletion, and
the shared start/stop flag.

A fifth 20,000-scenario suite covers worker lifecycles. It patches the two
DLLs' Win32 import cells with deterministic event, thread, wait, close and
critical-section services. Thread creation records the callback; callbacks
run synchronously under scripted waits, sometimes when the stop call waits
for completion. Completed callbacks are not rerun. Twenty-four action slots
mix individual and combined setup/start/stop/teardown, repeated setup/start,
restart after completion, queued work, repeated stops and partial teardown.

Each side receives independent but equivalent fake handles and heap records.
The suite compares a unified ordered Win32/host/DirectPlay call transcript,
worker state after every action (including shared `+0xf24`), queue headers,
locks, live and leaked allocations, event signalling, handle lifetime and
callback results. Inputs inject event/thread/SetEvent/CloseHandle and host
allocation/free failures, failed/timeout waits, transport errors and busy
retries. Failure schedules are independent of deferred execution so worker
error paths are reached. This proves controlled callback behavior and ABI;
it does not exercise OS thread scheduling races.

A sixth 20,000-scenario suite covers DirectPlay loader/interface lifetime at
private RVAs `0xacf0`, `0xad20` and `0xad50`, exposed as test-table indexes
22–24. Eight actions mix repeated load, create/query, unload, and create after
unload. Four fake COM identities distinguish factory, first query, second
query and pre-existing global interfaces. The suite compares ordered host and
COM calls, exact IID/provider bytes, output-cell addresses, state visible
inside callbacks, per-action module/status snapshots, and reference counts
(including leaked or overwritten references).

Scripts cover nullable provider GUIDs, each export-presence combination,
zero/positive/error HRESULTs, partial outputs on failed calls, query failure
that clears or preserves its destination, and arbitrary Release returns.
Factory availability is seeded independently before calling create, so a
missing factory export is compared as loader state rather than executed as
null code. The lifetime fake is separate from the send/receive fixtures; the
next integrated session fake must combine those methods and add the session,
player, group and lobby contracts. No real DirectPlay DLL or network is used.

`make multip-directplay-breaks` checks eleven mutations: loader export
selection, loaded-flag retention, export descriptor order, unload flag and
release-state ordering, first-query leak, second-query target and error return,
temporary release order, factory outer argument, and query-flag reset. As with
the other slices, a mutation counts only when its comparison completes with
differences; a crash or fixture error is not a catch.

This is not a whole-module 20k result. Init/deinit and their CHAIN callbacks,
session configuration, lobby handling, message dispatch, and public slots
`+0x18` and `+0x1c` remain unimplemented. Worker lifecycles are verified through
controlled internal entry calls; no public init/session entry is enabled yet.

Run the current checkpoint with:

```sh
make multipdiff-run
make multipdiff-run N=20000
make multip-breaks
```

The first command already defaults to 20,000; the explicit form is useful in
logs. The expected differential checkpoint lines are:

```text
MULTIP +0x24..+0x38: 20000 scenarios, 0 differ
MULTIP +0x20 enqueue: 20000 scenarios, 0 differ
MULTIP send-one/drain/flush: 20000 scenarios, 0 differ
MULTIP receive-one/dequeue: 20000 scenarios, 0 differ
MULTIP DirectPlay loader/lifetime: 20000 scenarios, 0 differ
MULTIP worker lifecycles: 20000 scenarios, 0 differ
```

## Recommended continuation order

The transport units and worker lifecycles in stages 1–4 are complete, including
queue construction/clear/destruction and the combined subsystem wrappers.
DirectPlay load/unload/create/query/release helpers are also verified. Next
merge the lifetime and transport fakes into a session-capable `IDirectPlay3A`,
then recover host/join/open/close, session descriptions, player/group creation
and cleanup edges. Begin with the internal helpers called by public `+0x18`;
keep that slot disabled until its full path is ready. The loader flag alone
does not prove the required factory is available, and create's first-query
failure leaks a reference: preserve those boundaries in integrated scenarios.
Public init/deinit remain disabled until their CHAIN callbacks have the
required lobby, profile, session, and message-dispatch paths.

The next public-entry boundaries are confirmed in `.text+0x8e60`: load first;
when connected, UTIL `+0xa4` supplies confirmation before disconnect
`+0x8cf0`. The disconnected path first calls `+0x3d60`; data `+0x1a4 == 0`
unloads and `== 3` returns 1. Otherwise data `+0xaa8` chooses host helper
`+0x3890` or join helper `+0x2340`, followed by common completion `+0x9c70`.
Disconnect includes CHAIN messages, send flush, receive queue clear, worker
stop, player destruction, `IDirectPlay3A::Close`, and unloader calls. These
are boundaries for recovery, not implemented session coverage.

Each stage should extend the harness before the corresponding Linkage slot
is enabled. Keep the test output labeled with the exact covered slice until
the last stage.

1. **Build the fixtures.** Add the host heap/memory services, deterministic
   events, CHAIN registration/dispatch, call logging, the DirectPlay export
   loader cells, and the fake `IDirectPlay3A`. Hook `ShellExecuteA` so retail
   can never launch anything.
2. **Port the queue primitives and transport units.** Start with receive
   `+0x5820`, send enqueue `+0x7890`, send drain `+0x7970`, and send-one
   `+0x79d0`. Test allocation/reallocation failures, empty queues,
   `DPERR_BUFFERTOOSMALL`, repeated `DPERR_BUSY`, success, and generic errors.
3. **Port public send `+0x20`.** Cover both guaranteed-message settings, both
   queues, zero and nonzero payload lengths, disconnected state, and failure
   propagation. MPCHAT is the easiest real caller to inspect for argument
   meaning.
4. **Port worker setup/start/stop.** Test idempotent starts and stops, event
   creation failure at each index, thread creation failure, ordered draining,
   and the retail 100 ms shutdown wait.
5. **Port DirectPlay object/session helpers and public `+0x18`.** Cover loader
   failure, interface-query failure, host and join paths, session enumeration,
   group/player creation, open/close, every cleanup edge, and repeated calls
   while already connected or disconnected.
6. **Port player/message dispatch.** Recover every on-wire message tag and
   length check before naming structures. Cover joins, leaves, type changes,
   names, aircraft updates, multipart model/texture traffic, malformed input,
   duplicate IDs, and reverse lookup behavior.
7. **Port public dialog `+0x1c` and profile round-trip.** Model host dialog
   `0x110b`, UTIL `+0x8c/+0x90`, enumeration callbacks, and the suppressed
   `ShellExecuteA` path.
8. **Port init/deinit and both CHAIN callbacks.** Only now enable `+0x04` and
   `+0x08`, because their callbacks can reach every subsystem above.
9. **Promote the harness to whole-module coverage.** Then, and only then,
   change the output from a slot range to `MULTIP whole module`.

## Whole-module acceptance

A final 20,000-scenario claim means all eleven public Linkage entries are
non-stubs and invoked, plus asynchronous work is drained after every action.
The scenario generator must mix:

- fresh init, repeated init-like messages, orderly shutdown, and injected
  failures at every host allocation, event, thread, and DirectPlay call;
- disconnected, hosting, joining, connected, and closing session states;
- zero, one, maximum, duplicate-ID, observer, and player record sets;
- all DirectPlay enumeration and name/session descriptor callbacks;
- empty, single, and multiple receive/send queues, including retry HRESULTs;
- every recovered application-message type, malformed sizes, and multipart
  boundaries;
- all profile booleans and boundary values, including retail clamps;
- pointer-normalized public returns, state snapshots, ordered host/CHAIN/COM
  call logs, sent payload bytes, and queued/received payload bytes.

Before installation, also require a clean `make all`, `git diff --check`, the
whole-module differential run with zero differences, and a game smoke test.
Only after those pass should MULTIP be added to `MODULES`; its installed name
must remain `MULTIPLAYER.DLL` through the existing Makefile mapping.
