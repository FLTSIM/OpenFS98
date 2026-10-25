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

## Windows

WINDOW's harness needed a model of the window services, so they were read
from the callees; prototypes are in `host.h`. A window is the caller's
0xa4-byte record ([window.md](window.md)). `+0x64` allocates a 0x90-byte
block of the host's own at record `+0xa0`, creates two native windows, and
puts the record at the front of its group (record `+0x78`) in a z-order kept
as a list of groups at `0x41a3dc`, each a singly linked list front first.
`+0x68` undoes it. `+0x90` raises a record within its group, `+0x94` returns
the nth shown one. `+0x7c` sends `0x467` and sets the title, `+0x80` sets the
title on both windows, `+0x84` sends `0x466` to a shown one, `+0x6c`/`+0x70`
bracket painting with a DC, and `+0x8c` draws text at one of fifteen places.
The window procedure's `WM_SIZE` (`0x4060ee`) writes the record's render size
(`+0x84`/`+0x88`) and surface size (`+0x60`/`+0x64`): halved with flag
`0x10000`, clamped by `+0x68`..`+0x74`, and made odd when the record has a
view block and is not a map. `+0x244`/`+0x25c`/`+0x264` add, remove and
rename "Views" menu items, keyed by the whole 32 bits of an id whose low word
is the command. `+0x2a4` is a string resource as a host copy.

WINMSG reads three more record fields ([winmsg.md](winmsg.md)). `+0x30`..`+0x3c`
are the window in pixels (`0x405d24`): the rectangle `+0x20`..`+0x2c` times
`0x41bda8` (the screen's width `<< 16 / 0x2000`) or `0x41c350` (its height `<< 16
/ 0x1800`), `sar 16`, the screen's origin added to x and y. So the rectangle
is in 0x2000ths of the width and 0x1800ths of the height, not 0x1000ths.
`+0x8c`..`+0x98` describe the surface (`0x4069d0`), `+0x94` its size in bytes
and `+0x98` its bits.

## Integer divides are repaired, not fatal

The sim's main loop (`0x401a70`) runs inside a `__try` (`0x412140`) whose
filter is `0x4119a0`. On `0xc0000094` or `0xc0000095` it decodes the faulting
`F6`/`F7` (a `0x66` prefix, ModRM, SIB and displacement for the length), and
for `div` sets the quotient and remainder to all ones; for `idiv` it reads the
divisor (register or memory) and sets the quotient to the largest value, or
the most negative but one (`0x8001`, `0x80000001`; `0x7f81` for a byte), when
the divisor and the dividend's top half differ in sign, and the remainder to
the largest. Then it steps EIP past the instruction and continues. Retail
modules divide into overflow routinely — WINDOW's look direction does every
time the eye looks within a degree or so of north — and rely on it; a harness
that loads a retail module has to model it (`windowdiff.c` does).

The filter is `LONG __stdcall (EXCEPTION_POINTERS *)` and returns -1
(`EXCEPTION_CONTINUE_EXECUTION`) when it has repaired a divide and 0
(`EXCEPTION_CONTINUE_SEARCH`) otherwise, the same values a vectored handler
returns. Other sites that divide into it, from the running sim: the host's
own `WM_SIZE` (`0x406525`, `0x406538`, `0x40654a`, `0x40655d`, by a zero
surface size while a view is 0x0), HG2D's gradient setup (`0x207376e2`,
`0x207376fc`, `0x20737716`, a zero-length edge, tens of thousands per flight)
and G3D `+0x28` (`0x206e18c6`). Wine never hands the filter a fault raised
inside a window procedure, and that is what froze the 3D view; see
[../wine.md](../wine.md) and `SEHFIX.DLL`.

## Reimplementation

`src/host/host.c` is the reimplementation, built as `HOST.DLL` and verified by
`tools/difftest/hostdiff.c` a slice at a time, the way the modules are. It is
not installed -- the host is the executable and cannot be swapped out -- so it
exists for the native build and as a tested specification; a native OpenFS98 is
this service table on a portable runtime.

The harness runs the **real host in process beside ours**, which is the trick
that makes the host testable at all. The real host is a reloc-stripped image
that must sit at 0x400000, an address wine keeps committed and will neither
hand out (`VirtualAlloc` there fails 487) nor free (`VirtualFree` crashes). So
the harness does not load it: it `VirtualProtect`s the region wine already owns
to `PAGE_EXECUTE_READWRITE` and copies the file's headers and sections into
place. The service table then stands at 0x417ba0 with its absolute 0x4xxxxx
code pointers valid, and any pure service can be called. The harness is linked
at 0x13000000 so it does not itself occupy 0x400000; our `HOST.DLL` is a normal
relocatable DLL exporting its table (`HostTable`) and its `.data` image
(`host_image`). For each done slot the harness calls both sides and compares
the result, normalising a pointer into either side's `.data` to an offset.
`make hostdiff-run`.

| slice | services | status |
| --- | --- | --- |
| 1, the plain accessors | `+0x1c` (returns 1), `+0x20`/`+0x34` (host globals), `+0x58`/`+0x5c`/`+0x60` (addresses of host structures) | verified: 0 differ; a deliberate wrong return is caught |
| 2, the simple state services | `+0x18` (copies six fields of its argument into the globals at 0x418830), `+0x2c` (sets the flag at 0x4187ec), `+0x54` (reads the globals 0x41880c and 0x418828 back through two out-pointers) | verified: 0 differ over many iterations; a deliberate wrong write is caught |
| 3, the C-runtime strings | `+0x2a8` strcpy, `+0x2b0` strcat, `+0x2b8` strlen (0 for a null pointer) | verified: 0 differ over thousands of random strings; a deliberate off-by-one is caught |
| 4, the block and string primitives | `+0x130` (strcpy from the host constant at 0x41bca0), `+0x168` (the "\\" UNC-prefix test), `+0x218` memset, `+0x228` memcmp, `+0x28c` (compacts the 4-byte palette at 0x4171a4 to 3 bytes an entry), `+0x2bc` strcmp | verified: 0 differ |
| 5, the small accessors and stubs | `+0x154`/`+0x2cc` (a constant and the 0x41a3cc flag), `+0x288` (stores into 0x417194), `+0x304` (clears a dword), `+0x108`/`+0x12c`/`+0x220`/`+0x224` (the zero stubs) | verified: 0 differ |
| 6, the allocator and the growable array | `+0x20c` alloc, `+0x210` realloc, `+0x214` free, `+0x21c` memmove; `+0x2f8` append, `+0x2fc` free a list, `+0x300` remove, `+0x308`/`+0x30c` walk forward/back, `+0x310` find | verified: 0 differ over 20,000 iterations; its 24 breaks caught (`free-null` by a hang: the double free wedges wine's heap) |

The harness now seeds `.data` with distinct random values on both sides before
each iteration (leaving the service table and bounding the string constant at
0x41bca0), so a service that reads the wrong offset of an otherwise-zero global
is caught -- `make host-breaks` runs 52 deliberate breaks, all caught (a hang
after two minutes counts as caught). (An
early run with a plain LCG missed seven, because the generator's low bits had a
period that aligned with the per-iteration call count and froze the
`.data`-seeding decisions; switching to xorshift32 fixed the coverage.)

Slice 2 grew the harness two ways that every later slice reuses: it passes
arguments (a struct pointer, out-pointers), and after each call it digests the
whole `.data` image on both sides and reports the first word that differs.
Because our image is a byte-for-byte copy of the host's `.data`, the two start
identical and the digest needs no relocation masking -- a difference is a write
we got wrong.

Slice 6 needed two more things of the harness. **The real host's imports are
bound**: the copy at 0x400000 arrives with its import table unresolved, so
`hostdiff` walks it and fills each slot by LoadLibrary/GetProcAddress (a trap
for anything wine lacks, MFC42's ordinals among them). Both sides then call one
MSVCRT and share its heap. And **realloc always moves**: both sides' `realloc`
is hooked (the real host's when its imports are bound, ours by patching
`HOST_ours.dll`'s import table) to copy to a new block and poison the old one,
because wine's realloc grows a small block in place and a pointer kept across
a move otherwise reads intact old contents and goes unseen.

The growable array (`struct fs98_host_list` in `host.h`) is the host's general
container: count, capacity, grow step, element size, a for-each depth, a
removal-queued flag, data. While a list is being walked, removals are not
applied but queued on the host's own list at 0x418380 (8-byte `{ list, index
}` entries, grow 0x20) and applied newest first when the walk ends. Retail's
quirks, reproduced:

- a removal **always drops the count**, even for an index at or past the end
  (which then moves nothing), and a queued removal does the same when it is
  applied -- so more queued removals than elements drive the count negative,
  and the next append writes below the data;
- the walk's end applies the queue **without consulting the depth**: a nested
  walk's end applies the outer walk's removals while it is still running, where
  each is queued again -- and the list's queued flag is then cleared over it,
  so they stay on the queue until some later walk sets the flag again;
- `+0x2fc` frees the data but leaves the capacity, so a list freed and not
  re-initialised never reallocates, and its next append writes through null;
- `+0x300` returns what eax held: the element size after a move, the queue
  entry when queued, else 0; `+0x2fc` returns the address of the data field
  when there was nothing to free;
- `+0x310` with a zero length matches the first element (`repz cmpsb` with
  ecx 0 leaves the flags of the `xor` before it);
- `+0x210`'s second failure is not fatal the way `+0x20c`'s is: it shows the
  out-of-memory box and posts the quit (`+0x024`'s body), then returns 0 with
  the pointer null.

The harness drives a pair of lists through random appends, removals (inside
and outside walks), nested walks, finds and clears, with a script both sides'
walk callbacks follow; a bystander list kept permanently mid-walk puts a
second list's entries on the queue, and sometimes the queue starts empty with
a grow step of 1 so that it moves under a flush. Removals are only issued while
the list keeps more elements than are queued against it (a negative count is
a caller's error, not behaviour to test). Allocation failure is exercised only
with flags 4; the fatal paths are transliterated but not run.

### Continuing the reimplementation

To add the next service: find it in the table with `python3 tools/hostmap.py`
(it sizes every slot and names the imports each one uses, so a pure or
self-contained service is easy to spot), read its body with `python3
tools/hostdis.py <addr>` (the service table's addresses are retail's), add a
`host_###` function and table entry in `src/host/host.c`, a check in
`tools/difftest/hostdiff.c`, and a break in `tools/host_breaks.py`. Then `make
hostdiff-run N=20000` (0 differ) and `make host-breaks`. A service that reads a
global proves nothing unless the global varies, which the harness's per-call
`.data` seeding takes care of; one that calls `malloc` or a window/GDI/DDRAW
API is not out-of-process testable this way and waits for a model or the
`ofstest` harness.

Easiest remaining, roughly in order: the other pure accessors and getters (many
read one global and return it, with an early-out); the remaining C-runtime
services (`sprintf`-family and friends, `+0x22c` qsort) -- with the imports
bound, the eleven services that call only MSVCRT (`hostmap.py` lists them) are
now as testable as the pure ones; then the ones that call a host-internal
helper that is itself pure. `+0x094` (a pure walk of a
run-time linked list at 0x41a3dc) and `+0x314` (a no-op) are the two pure
leaves left unimplemented.

## Status

36 of 194 services reimplemented in `src/host/host.c` and verified against the
real host in process (slices 1 to 6 above). 71 have prototypes confirmed from the
callee and recorded in `host.h` (15 of them the window services, from WINDOW),
and 11 were already exercised in-process through other modules' harnesses: the
clock, `strlen`, and the nine-service enumeration cluster above. The dialog
registry's contents are checked in-process too. 13 more are named from their
shape alone, and all 194 are classified by subsystem. Sizes reported by the
mapper are upper bounds: they are the gap to the next table-referenced address,
and internal helpers that no slot points at fall inside those gaps.
