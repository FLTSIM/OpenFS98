# CHAIN.DLL — module id 4

**Reimplemented, verified, and running in-game.** CHAIN is a 966-byte
callback registry -- the sim's event bus: 35 independent callback lists,
selected by a numeric index. MAIN dispatches 28 different lists, CONTROLS 16,
WINDOW 9; 19 modules register callbacks. It imports the host and GLOBAL,
although the retail code never reads the GLOBAL linkage pointer. The only
external behavior it needs is allocation, reallocation, freeing and `memmove`,
all through host services.

`src/chain/chain.c` is the transliteration and the module shell together.
`make chaindiff-run` drives retail and the OpenFS98 build side by side over
20,000 random scripts -- **0 differ** -- and `make smoke` passes with it
installed beside GLOBAL, NUMERIC, SLEW and UTIL: 27/27 modules, 95 gauges.

The preflight below held up. The sections after it record what the
line-by-line reading added.

## Binary and linkage

- Retail image base: `0x20660000`.
- `.text`: 966 bytes, `0x20661000` through `0x206613c5`.
- Imports: host (module 1), GLOBAL (module 2), terminator.
- Win32 imports: only `DisableThreadLibraryCalls`.
- Exports: `ImportTable` and `Linkage`, both as data.

| slot | retail address | preflight reading |
| --- | --- | --- |
| `+0x04` | `0x20661020` | init — zero all 35 list pointers |
| `+0x08` | `0x20661040` | deinit — free every allocated list |
| `+0x0c` | — | zero |
| `+0x10` | — | literal `0xe0000000`; meaning unknown, preserve it |
| `+0x14` | — | zero; no dialog table |
| `+0x18` | `0x20661070` | add a callback to the indexed list |
| `+0x1c` | `0x20661140` | remove a callback from the indexed list |
| `+0x20` | `0x20661200` | dispatch an indexed list with two opaque arguments |

The service-table mapper reports `+0x20` as 512 bytes because it rounds the
last entry to the next section. Its code and internal helpers actually end at
`0x206613bd`; the six-byte `DisableThreadLibraryCalls` forwarder begins at
`0x206613c0`.

## Data model

`.data+0x40` is an array of 35 list pointers. The service wrappers calculate
`&lists[index]` directly, with no bounds check. `.data+0xcc` is a module-wide
"some removal is pending" flag. Init clears the pointers but stops before that
flag, matching the retail `rep stosd` count exactly.

Each allocated list begins with this inferred layout:

```c
struct fs98_chain_list {
    uint32_t dispatch_depth;    /* +0x00 */
    uint32_t count;             /* +0x04 */
    uint32_t capacity;          /* +0x08 */
    uint32_t removal_pending;   /* +0x0c */
    struct {
        void *callback;         /* +0x10, then eight-byte stride */
        uint32_t removed;       /* +0x14 */
    } entries[];
};
```

The first add allocates `0x60` bytes and sets capacity to 10. A full list is
grown by another ten entries: the requested size is `capacity * 8 + 0x60`,
then capacity is incremented by ten. Duplicate callback pointers are allowed.

## Service behavior to confirm in the harness

`+0x18` appends one callback. It returns zero on success and `-1` for a null
list-pointer argument or an allocator failure. The public wrapper always
passes one of the module's 35 list-pointer addresses, so the null case belongs
to the internal helper rather than the public ABI.

`+0x1c` removes the first matching callback and returns `-1` if none matches.
Outside a dispatch it closes the gap immediately with host `+0x21c`. During a
dispatch it marks the entry and both pending flags instead, leaving compaction
until dispatch unwinds.

`+0x20` increments the selected list's dispatch depth, calls each unremoved
callback with the service's second and third arguments, then decrements the
depth. The loop rereads `count`, so a callback appended during dispatch may be
visited by that same dispatch. Once the outer dispatch finishes, deferred
removals are compacted across all 35 lists whose dispatch depth is zero. If
another list is still dispatching, the module-wide pending flag remains set.

The four host calls are:

| host slot | use |
| --- | --- |
| `+0x20c` | allocate the initial list block |
| `+0x210` | grow a list block |
| `+0x214` | free a list through its pointer address |
| `+0x21c` | `memmove(dst, src, size)`, returning zero |

The first three already have prototypes in `include/fs98/host.h`. Host
`+0x21c` was confirmed from its callee: it is a stdcall wrapper around MSVCRT
`memmove` and pops 12 bytes. Add that prototype as part of the implementation.

## Differential-test plan

Create a whole-module `chaindiff` harness rather than testing only the C
helpers. Load retail CHAIN, bind both implementations to the same deterministic
host stubs, and compare returns, callback order and allocator-visible state.
The external harness is sufficient because GLOBAL is unused and every host
dependency has a small deterministic stub.

Minimum cases:

- all 35 public indices;
- first allocation and growth across capacities 10 and 20;
- allocation and reallocation failures;
- duplicates, removal of the first duplicate, and a missing callback;
- immediate removal outside dispatch;
- self-removal and removal of another callback during dispatch;
- append during dispatch, nested dispatch and dispatch of an empty list;
- pending removals in two lists while one remains nested;
- deinit freeing every non-null list and init's exact reset range;
- an intentional broken build which the harness must reject.

After the differential suite is clean, add CHAIN to the module build at base
`0x20660000`, install it beside the current GLOBAL, NUMERIC, SLEW and UTIL
builds, and require the usual 27-module/95-gauge smoke-test result.

## What the transliteration added

Four behaviours the preflight did not record, all reproduced:

- **A list's dispatch depth is never initialised.** The first add sets count,
  capacity and the pending flag, but not `+0x00`, so a new list starts at
  whatever depth `malloc` left. If it is non-zero, every removal from that list
  is deferred and the list is never compacted. If it is `0xffffffff`, a
  dispatch increments it to zero, so removals inside a dispatch are immediate
  and those outside are deferred -- which is the only way a list can empty while
  removals are still pending.
- **Removing from a list that was never added to dereferences null.** The
  remove helper has no null check, unlike add's. Retail faults; the
  reimplementation returns -1, and the harness never asks retail to.
- **Removal matches entries already marked removed.** Removing the same
  callback twice during a dispatch marks the same entry twice and returns 0
  both times, rather than finding a later duplicate.
- **A list that empties keeps its pending flag.** Compaction skips a list with
  a count of zero before it clears the flag.

Both move sizes are computed as `((i+1)·0x1fffffff + count) << 3`, which wraps
to `(count − i − 1)·8`; transliterated as the latter. No caller in the retail
modules reads any of the three services' return values -- every one overwrites
`EAX` first -- but they are reproduced anyway, the empty-list dispatch
returning the list pointer as retail does.

## The harness

`tools/difftest/chaindiff.c` loads both DLLs and drives them only through
`Linkage`. The stub host numbers heap blocks in allocation order, fills new
memory with a chosen byte (the uninitialised depth), and can fail the Nth
allocation. Eight stdcall callbacks log every call and act back on the module
under test: removing themselves (once or twice), removing or adding another,
or dispatching a list from inside a dispatch. Compared after each script: the
transcript of host calls, callbacks and returns, every live block's bytes, and
deinit's frees.

Two things it needed:

- **An action budget.** A callback that adds to the list being dispatched is
  called by that same dispatch, so adds can chain without end -- in retail too.
  Callbacks act at most 40 times per run, alike on both sides.
- **A fill of `0xff`.** With only `0x00`, `0xab` and small fills, a deliberate
  break -- clearing the pending flag of an emptied list -- went uncaught,
  because that state needs a depth of `0xffffffff`. With it, the break is
  caught in 4 scenarios of 20,000. Rare, but it is now covered rather than
  assumed.

Coverage over the 20,000 scripts: 8,512 reallocs, 770 injected allocation
failures, 14,434 memmoves, 99,520 nested dispatches, 17,690 double
self-removals, 13,363 scenarios with a non-zero initial depth. Six
deliberate breaks are each caught: initialising the depth, skipping removed
entries, caching the count in dispatch, clearing the pending flag in init,
clearing it for an emptied list, and growing by 20.
