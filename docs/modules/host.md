# FLTSIM98.EXE — the host, module id 1

82,016 bytes of `.text`. Every module imports it, and until the service table
was found it was the one thing in the dependency graph nothing could see into.

Regenerate everything below with `make hostmap`.

## It publishes a service table like any other module

The host has no `Linkage` export -- it is an executable -- but it hands modules
a pointer to a structure of the same shape, and the loader writes that pointer
into each module's `ImportTable` entry for id 1. It is found the way the module
tables were: scan `.data` for a dword 1 followed by a long run of pointers into
`.text`.

```
0x00417ba0   id 1, then 194 service slots to +0x31c, 191 distinct functions
```

0x320 bytes, more than twice UTIL's table, which fits a module everything else
leans on. Three slots alias: `+0x110` and `+0x118` share a function, and
`+0x108`, `+0x220` and `+0x224` share another.

## What the services are, by the libraries they call

This is the useful cut, because it is also the specification a native host has
to satisfy:

| calls | services | meaning |
| --- | --- | --- |
| nothing | 84 | pure logic and accessors onto host globals |
| `MSVCRT` only | 11 | C runtime only |
| `KERNEL32` | 40 | files, memory, processes |
| `KERNEL32` + `MSVCRT` | 14 | |
| `USER32` | 27 | windows, messages, input |
| `GDI32` (alone or with others) | 8 | drawing |
| `WINMM` | 4 | timing and sound |
| `DDRAW`, `MFC42`, `COMCTL32`, `SHELL32`, `ADVAPI32`, … | 6 | the big ones |

**95 of 194 services -- very nearly half -- call no platform API at all**, being
either pure logic or C runtime. The platform dependency is concentrated in the
27 windowing services, 8 drawing ones, and a handful of large services that
touch everything: `+0x030` is 27 KB and reaches for seven libraries, and
`+0x31c` is the one that touches `DDRAW`.

## The shape of the simple ones

Fifty services are 48 bytes or less, and the mapper names thirteen outright
from their code shape alone:

| kind | example |
| --- | --- |
| returns a constant | `+0x01c` returns 1 |
| reads a global | `+0x020` returns `*0x0041719c` |
| writes a global | `+0x02c` sets `*0x004187ec` to 1 |
| returns a global's address | `+0x058` returns `&0x00417ec0` |
| stub returning zero | `+0x108`, `+0x12c`, `+0x154`, `+0x220`, `+0x224` |
| import thunk | `+0x038` is `timeGetTime` |

`+0x058` is worth a second look: `0x00417ec0` is the address immediately after
the service table ends, so the host is handing modules a pointer to whatever
structure it keeps next door.

That the table is mostly accessors is itself the finding. The host's job, from
a module's point of view, is to expose state and a few primitives.

## The two services that mattered first

| slot | address | what |
| --- | --- | --- |
| `+0x38` | `0x4085c0` | `timeGetTime`, a jump straight through the IAT to WINMM |
| `+0x2b8` | `0x40ef40` | a null-safe `strlen`, `repnz scasb`, 0 for a null pointer |

Four more are identified but not yet implemented, because they are what UTIL's
remaining services are built on:

Every one of these was read from the **callee**, so the prototypes are the
binary's, not a guess from a call site. All `__stdcall`.

| slot | address | prototype |
| --- | --- | --- |
| `+0x20c` | `0x40cdc0` | `int alloc(int flags, size_t size, void **out, int zero)` |
| `+0x214` | `0x40cef0` | `int free_p(void **pp)` — frees `*pp`, then NULLs it |
| `+0x2a0` | `0x40ed70` | `int load_string(unsigned id, char *buf, int len)` |
| `+0x2a8` | `0x40ee70` | `char *strcpy(char *dst, const char *src)` |
| `+0x2b0` | `0x40eec0` | `char *strcat(char *dst, const char *src)` |
| `+0x2b8` | `0x40ef40` | `size_t strlen(const char *s)` — 0 for NULL |
| `+0x210` | `0x40ce60` | `int realloc_p(int flags, size_t size, void **pp)` |
| `+0x10c` | `0x409720` | `int run_dialog(unsigned id, void *param)` — see *The dialog engine* |
| `+0x110` | `0x409bd0` | the same, by forwarding to `+0x10c` |
| `+0x114` | `0x409bf0` | `void (void)` — service the modeless dialogs |
| `+0x024` | `0x408550` | `int (void)` — quit: `WM_CLOSE` to the main window |

`+0x20c` is the one worth knowing in detail. It calls `malloc(size)` and stores
the result through `out`, and zero-fills the block when the fourth argument is
non-zero -- that argument was once read as a tag. A caller passing 0 gets
whatever `malloc` returned, which is how WEATHERM's list code comes to walk
pointers it never set (see [weatherm.md](weatherm.md)). On failure its behaviour depends on `flags`: **`flags
== 4` returns -1** and lets the caller cope; anything else retries once after a
purge hook, and if that also fails it loads string id 9, puts it in a
`MB_ICONHAND` message box and exits the process. So most of the game treats
allocation failure as fatal by choice, not by omission.

So the host provides modules with memory management as a service rather than
letting them call the C runtime directly -- which is consistent with everything
else about this design, and is another thing a native host has to supply.

Both were reached for by UTIL, and naming them unblocked two services that had
been parked:

- UTIL `+0x9c` measures two strings through `+0x2b8` and returns the longer,
  ties going to the second argument.
- UTIL `+0x04` (init) seeds both of UTIL's generators from `+0x38`: the integer
  one takes the tick directly, the floating-point one takes `1.0 / tick` and
  retries while the clock reads zero.

Init had been written off as untestable, on the grounds that its result is not
a function of its arguments. That was too pessimistic. The *state it leaves
behind* is observable on both sides, so a host stub returning a controlled tick
sequence makes it testable -- and a deliberately broken version, with the retry
removed, is caught.

## Why this module matters more than its position suggests

1. **It gates every other module's unfinished services.** Anything that reaches
   outside itself reaches here first.
2. **A native build replaces it.** The host owns the window, the message loop,
   DirectDraw and the MFC user interface -- all the platform dependency the
   README's portability survey puts outside the 16 portable modules. Whatever
   drives a native OpenFS98 is a reimplementation of this service table on SDL,
   and the table above says how much of it is actually platform work: about
   half.

## Its services cannot be tested from outside the process

Worth recording before anyone tries again. The host has no `.reloc` section, so
every address inside it is absolute and correct only at `0x400000`: the service
pointers, the globals its accessors read, the calls between its functions.
`LoadLibraryEx` maps it wherever it likes -- 0x00a90000 here -- at which point
the service pointers are wrong and, worse, the code reads globals from whatever
occupies the old addresses.

Mapping it by hand at `0x400000` does not work either: that address is already
committed in any wine process, inside a private region the loader creates at
`0x330000`, so `VirtualAlloc` refuses it.

So the differential approach that works for UTIL -- load the retail binary
beside the reimplementation and call both -- cannot work here.

**The way to do it is to run the test inside the host rather than outside.**
The sim already loads every DLL in `Modules\` and hands each one the host's
linkage pointer, which is precisely the access a test needs. A test module with
its own module id, dropped in beside the others, would be handed a real host
pointer by the real loader, could call services for real, and could write its
results to a file. That is the same drop-in mechanism the project relies on
everywhere else, pointed at the host.

**That harness now exists**: `src/test/ofstest.c`, built and run by
`make ofstest-run`. It takes module id 5, which the retail set leaves unused,
imports the host, GLOBAL and UTIL, and writes its results to
`C:\openfs98-test.log` because a module has nowhere else to put output.

Its first run confirmed the static analysis from the outside in:

```
host pointer   0x00417ba0      <- the address derived by scanning .data
GLOBAL pointer 0x20722000      <- the OpenFS98 build, not retail's 0x20723000
UTIL pointer   0x208e5020      <- retail's Linkage RVA exactly
```

The host table address had been worked out by pattern-matching `.data`; being
handed the same pointer by the real loader settles it. The GLOBAL pointer is an
incidental bonus: `.data` sits at a different RVA in the reimplementation than
in retail, so the address alone proves the swapped-in module is the one running.

It also verified the two implemented host services against the real article --
`strlen` of an empty string, of "abcd", and of a null pointer -- and checked
UTIL `+0x9c` against retail across 25 string pairs while both measured through
the genuine host service rather than a stub.

Everything written here runs during the sim's startup, so a fault in it stops
the sim from starting. The module is defensive about null pointers for that
reason, and `make ofstest-remove` takes it back out when not in use.

## The enumeration cluster

Nine services that UTIL's file listings (`+0x94`, `+0x98`, `+0xa0`) are built
from. All nine prototypes are in [include/fs98/host.h](../../include/fs98/host.h),
read from the callee, and every behaviour below is checked in-process by
`ofstest` against a scratch tree it builds under `C:\`.

| slot | address | what |
| --- | --- | --- |
| `+0x164` | `0x40a9a0` | find-first: `(handle **out, dir, pattern, ext_id)` |
| `+0x160` | `0x40ab60` | find-next into a 0x110-byte entry |
| `+0x15c` | `0x40ac90` | find-close; frees the handle block and nulls it |
| `+0x184` | `0x40a510` | join drive, directory and name |
| `+0x188` | `0x40a6c0` | split a path; the extension comes back as an id |
| `+0x22c` | `0x40cfb0` | `qsort` with a stdcall comparator |
| `+0x2c0` | `0x40efa0` | `_strcmpi` |
| `+0x2f8` | `0x4101d0` | growable array: append a zeroed element |
| `+0x300` | `0x410210` | growable array: remove an element, closing the gap |

### File extensions are integers

The host keeps a table of 37 extensions at `0x417f20` -- id 0 is `""`, then
`.BGL`, `.STN`, `.MOD`, `.R8` and so on to `.CFG` at 36 -- and modules pass
**ids, not strings**. `+0x164` turns the id back into text to build its
pattern (`-1` appends `.*`, `-2` nothing), and `+0x160` turns each file's
extension into an id again, upper-casing it first, so `.stn` is 2.

The table grows. An extension that is not in it gets id `0x25 + n` from a
second, heap-allocated table -- but only `+0x188` adds to it, as a side effect
of being asked to split a path with a non-null id pointer. `+0x160` only looks
up. So a file with an unregistered extension comes back from find-next with
its `type` **untouched**: whatever the caller's entry buffer held before.

### The entry

| offset | field |
| --- | --- |
| `+0x000` | name, 0x104 bytes: a directory's whole name, a file's **without its extension** |
| `+0x104` | type: `-2` for a directory, else the extension id; unchanged for an unregistered extension |
| `+0x108` | `nFileSizeLow`; 0 for a directory |
| `+0x10c` | 1 if writable, 0 if read-only; files only, a directory leaves it alone |

`.` and `..` are skipped. Nothing else is: a directory whose name matches the
pattern is returned alongside the files.

### Quirks worth knowing before calling them

- **An empty directory string is the drive root.** `+0x164` appends a
  backslash unless the directory already ends in one, and it tests that by
  reading `buf[len-1]` -- which for `""` is the byte *before* the buffer, the
  top byte of a heap pointer. That is never a backslash, so `""` becomes `\`
  and the search runs from the root of the current drive. A null directory, by
  contrast, searches the current one. `ofstest` checks both.
- **`+0x184` strips one trailing backslash** after the drive and after the
  directory, not after the name, and turns a one-letter drive into `X:`. It
  builds in a 520-byte buffer and only checks the result against `MAX_PATH`,
  so it fails cleanly on a long result but not on a long input.
- **`+0x188` treats `\\server` as the drive**, walking the server name with
  `CharNextA`, so it is DBCS-aware; with a single-byte code page that is just
  `p + 1`. An empty path empties every output rather than calling `_splitpath`.
- **`+0x22c` is not reentrant.** It parks the comparator in a global and hands
  `qsort` a cdecl trampoline that calls it -- and does not clean up after it,
  so the comparator must be stdcall.
- **`+0x2f8` may move the array.** It grows by realloc through the allocator
  with the fatal flag, so it never returns null but earlier element pointers
  do not survive it. The header's `+0x10` is the host's for-each nesting
  depth: while a list is being walked, removals are queued onto a host-global
  list and applied when the outermost walk ends. A caller's own list is never
  walked by the host, so it stays 0.

## The dialog engine

`+0x10c` was first read, from UTIL's call sites, as `(mode, descriptor)` with
2 meaning modal and 3 modeless. The callee says otherwise, and so does the
running sim.

**The first argument is a dialog id.** The host keeps a registry at
`0x417ee0` -- a host list of 16-byte `{ id, module, index, flags }` records.
When the module scanner (`0x407b20`) loads a DLL it reads `Linkage+0x14`, and
if that is non-null registers every 12-byte `{ id, create, flags }` entry of
the table it points to. 65 dialogs are registered by the time the main menu
is up.

None of them come from `Modules\`. Apart from `mpchat`'s one, they belong to a
second family of modules in **`English\`** -- `Flights`, `Sim`, `Views`,
`World`, `Options`, `Help`, `Alerts` and `MP_DIALOG` -- with module ids
`0x80000002` to `0x8000000a`. `SIMSUI.DLL`, in the root, is `0x80000000`.
`make graph` does not see them, which is how a static search concluded that
nothing registers ids 2 and 3; a one-run probe of the live registry found
them in `Alerts.DLL` (`0x80000009`): 2 modal, 3 modeless, 4 modal.

`run_dialog(id, param)`:

1. Finds the record, or returns -1.
2. If the main window is enabled and the record lacks flag 8, calls
   `0x40e450(1)` -- by its pairing with `0x40e450(0)` on the way out, a
   pause or disable; not yet read.
3. Acquires the owning module through the host's module manager -- loaded on
   demand and reference-counted, with its `Linkage+4` init run on first load
   -- and calls `create(main_hwnd, param)` from its dialog table.
   With flag 2, `create` returns an array of windows rather than one.
4. **Flag 4, modeless:** shows the window, appends `{ record, hwnd }` to the
   modeless list at `0x417f00`, drains that window's queued messages, and
   returns 0.
5. **Otherwise, modal:** pushes `{ record, window }` onto a stack of nested
   modal dialogs at `0x41ba60`,
   loads accelerator table `0x69`, and pumps messages -- through
   `TranslateAccelerator`, the modeless dialogs' `IsDialogMessage` filter and
   its own -- until the window is destroyed or its `GWL_USERDATA` goes
   non-zero. When idle it sends the dialog `WM_USER+100` (`0x464`) and raises
   host event `0x20006` (and `0x20004` / `0x20005` around the whole loop).
   Afterwards it destroys what is left, releases the module, and calls
   `0x40e450(0)`. Returns 0.

The answer of a modal dialog does not come back as the return value: the
dialog writes it into `param`.

`+0x110` is a five-instruction forwarder to the same function; UTIL uses one
for the modal alert and the other for the modeless, which is what made them
look like different services. `+0x114` walks the modeless list: a dialog whose
window is gone is released and removed; the rest are sent `WM_USER+100`,
synchronously. That is the hook UTIL's alert-close uses -- it clears the
alert's live flag, then ticks, and the alert destroys itself while handling the
message.

None of this is reimplemented, and none of it can be swapped in: there is no
mechanism yet for replacing code inside the host. What is pinned is the
contract -- registry, ids, flags, the `param` convention and the tick -- which
is what a native host's dialog layer has to honour.

## Status

2 of 194 services implemented. 21 have prototypes confirmed from the callee
and recorded in `host.h`, and 11 are verified in-process: the clock, `strlen`,
and the nine-service enumeration cluster above. The dialog registry's contents
are checked in-process too. 13 more are named from their
shape alone, and all 194 are classified by subsystem. Sizes reported by the mapper are upper bounds:
they are the gap to the next table-referenced address, and internal helpers
that no slot points at fall inside those gaps.
