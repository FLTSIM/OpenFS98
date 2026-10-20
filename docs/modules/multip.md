# MULTIP

`MULTIP.DLL` is module 35, installed as `MULTIPLAYER.DLL`.  It owns the
DirectPlay session, the local and remote player records, network messages and
the extrapolated aircraft state handed to the rest of the simulator.

This module is in progress.  The first source slice implements and verifies
the public player-list, status and active-range queries (`+0x24` through
`+0x38`).  It builds
with `make bin/MULTIP.DLL` but is deliberately absent from `MODULES`, so
`make install` cannot replace the retail multiplayer module prematurely.

## Image and public interface

Retail is based at `0x20850000`.  It has 43,318 bytes of `.text`, 0xa00 bytes
of initialised `.data`, and uses the rest of the `.data` page for zero-filled
runtime state.  `ImportTable` is at `.data+0x000` and `Linkage` at `+0x058`.
The imports are host, GLOBAL, ACONTAIN, CHAIN, HG2D, G3D, MAIN, NUMERIC, UTIL
and WEATHERM.

| Slot | Retail address | Current meaning |
| --- | --- | --- |
| `+0x04` | `.text+0x0020` | initialise: register the module and a CHAIN list `0x14` callback |
| `+0x08` | `.text+0x0070` | unregister the module |
| `+0x18` | `.text+0x8e60` | session start/stop driver, called twice by MAIN |
| `+0x1c` | `.text+0x39a0` | multiplayer dialog/configuration entry, called by MAIN |
| `+0x20` | `.text+0x9840` | send a four-argument application message; MPCHAT is one caller |
| `+0x24` | `.text+0x9de0` | return `{count, records}` for the remote players |
| `+0x28` | `.text+0x8680` | find a player record by DirectPlay player ID |
| `+0x2c` | `.text+0x8690` | return the local-player record |
| `+0x30` | `.text+0xa560` | return multiplayer status bits |
| `+0x34` | `.text+0xa5b0` | conditionally replace MAIN's collision result with `-1` |
| `+0x38` | `.text+0xa5f0` | return the active remote aircraft's range in tenths of a nautical mile |

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
when a session is connected, remote-aircraft handling is active, the player
mode is 1, the active player ID resolves, and that record's `+0x28` is nonzero.

`+0x38` also always returns zero and leaves its output unchanged unless the
session is connected, remote-aircraft handling is active, the player mode is
nonzero, the active ID resolves, and the record has an aircraft-state pointer
at `+0x28`.  It asks UTIL `+0x64` for the distance between `GLOBAL+0x560` and
aircraft-state `+0x04`, using flags 3, then writes the truncated value of
`distance * 0.0005396115152459395 * 10`.  The two multiplications and MSVCRT
`_ftol` truncation are reproduced with x87 operations.

## DirectPlay interface

The main object at `.data+0x988` is conclusively `IDirectPlay3A`, not a custom
COM-shaped interface.  The evidence is both static and structural:

- `.rdata+0x070` is byte-for-byte `IID_IDirectPlay3A` and is passed to
  `QueryInterface` while filling `.data+0x988`.
- Calls through the object use the inherited `IDirectPlay2A` vtable at the
  documented offsets: `Close` `+0x10`, `CreateGroup` `+0x14`, `CreatePlayer`
  `+0x18`, `DeletePlayerFromGroup` `+0x1c`, `DestroyPlayer` `+0x24`,
  `EnumGroups` `+0x2c`, `EnumPlayers` `+0x30`, `EnumSessions` `+0x34`,
  `GetPlayerName` `+0x54`, `GetSessionDesc` `+0x58`, `Open` `+0x60`,
  `Receive` `+0x64`, `Send` `+0x68`, `SetPlayerName` `+0x78`, and
  `SetSessionDesc` `+0x7c`.
- The usual `QueryInterface` (`+0x00`) and `Release` (`+0x08`) calls bracket
  its lifetime.

This reduces the transport model to a well-defined fake `IDirectPlay3A`.
The receive loop treats `DPERR_BUFFERTOOSMALL` (`0x8877001e`) as a resize and
retry, while the send loop retries `DPERR_BUSY` (`0x8877010e`).  The first
fake can therefore implement only the methods
listed above, scripted sessions/players, and a deterministic receive queue.

## Verification

`make multipdiff-run` defaults to 20,000 deterministic randomized scenarios.
It loads retail and OpenFS98 together, seeds equivalent local and remote
records plus fake GLOBAL and UTIL imports, and compares normalized results for
the recovered `+0x24` through `+0x38` slice: the result block, local and remote
lookups, reverse duplicate-ID selection, missing IDs, all three status bits,
active/inactive collision overrides, every active-range guard, the UTIL call
shape, and the range conversion.  The current result is 20,000 scenarios with
zero differences.

This is not a whole-module 20k result.  Init/deinit and their CHAIN callback,
session configuration, DirectPlay transport, the worker thread, message
handling, and public slots `+0x18`, `+0x1c`, and `+0x20` remain unimplemented.

The remaining slots intentionally return failure in the in-progress build.
The next useful boundary is to model `IDirectPlay3A` and transliterate the
receive/send helpers before enabling init or adding MULTIP to the normal
module list.  Init itself is small, but registers the much larger CHAIN list
`0x14` message callback, so implementing it alone would create misleading
runtime coverage.
