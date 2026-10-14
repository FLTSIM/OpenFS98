# CHAIN.DLL — module id 4

**Next implementation slice.** CHAIN is a 966-byte callback registry: 35
independent callback lists, selected by a numeric index. It imports the host
and GLOBAL, although the retail code never reads the GLOBAL linkage pointer.
The only external behavior it needs is allocation, reallocation, freeing and
`memmove`, all through confirmed host services.

This preflight is complete enough to begin implementation. The service bodies
still need a line-by-line transliteration and differential tests before any
names or inferred semantics below should be treated as final.

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
