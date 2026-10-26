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

All of it is now reimplemented (slice 28) and verified against the real host
in process, over a seeded registry and module records, factories that make
scripted windows, and a scripted message queue. Read from the callee, the
details are: the pause is 0x40e450 -- on a palette display the grey palette
(0x41a250), a 216-colour cube DIB header and a 16K table dithering each
palette entry onto it over an 8x8 ordered matrix (0x40e6f0, the matrix in
`.rdata` at 0x4160c8), the palette realised again on the way out; page
flipping's part (0x413e80) goes back to the GDI surface; and host events
0x100f8/0x100f9. A factory that makes nothing returns -1 *without releasing
the module*. Each message a modal loop takes is first offered to every
modeless dialog, each of which is ticked (`0x464`, sent) on the way -- so the
modeless dialogs tick once per message, not once per idle. A modal dialog
pushed over an array of windows reuses the register that held its own array:
if both are arrays, its loop runs on the one below (reproduced). The loops'
order of `IsWindow`/`GWLP_USERDATA` checks is retail's exactly, since the
harness logs both.

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
| 7, the command line, file extensions and the C-runtime wrappers | `+0x98`/`+0x9c`/`+0xa0`/`+0xa4` (a command-line option as text, int, double, or whether given), `+0x1ac` (an extension's id, registering a new one), `+0x188` (split a path, UNC-aware, and identify its extension), `+0x22c` qsort, `+0x230` swap two blocks, `+0x234` bsearch, `+0x2ac` strncpy, `+0x2b4` strdup, `+0x2c0` _strcmpi, `+0x2c8` (delete a character throughout a string) | verified: 0 differ over 20,000 iterations; 30 breaks, 27 caught, 3 proven equivalent |
| 8, string resources | `+0x2a0` (LoadStringA from the string modules, newest first, then the host), `+0x2a4` (the same into a new block), `+0x298`/`+0x29c` (a module's HMODULE onto / off the string list), `+0x1d0`/`+0x1d4` (an input's name -- "Button 07", "Axis 03" -- and back) | verified: 0 differ over 20,000 iterations; 22 breaks caught |
| 9, names, paths and image buffers | `+0x178`/`+0x18c` (a valid file name / path), `+0x184` (join drive, directory and name), `+0x1b4`/`+0x1b8`/`+0x1bc` (create, free, resize an image buffer) | verified: 0 differ over 20,000 iterations; 18 breaks caught |
| 10, accelerator tables | `+0x1d8` new, `+0x1dc` destroy, `+0x1e0` id by index, `+0x1e4` load from a resource, `+0x1e8`/`+0x1ec` enable/disable, `+0x1f0` clear, `+0x1f4` add entries, `+0x1f8` remove entries, `+0x1fc` read entries | verified: 0 differ over 20,000 iterations; 23 breaks, 19 caught, 4 proven equivalent |
| 11, key names | `+0x200` (a key code's name: "CTRL + SHIFT + F1"), `+0x204` (a name's key code) | verified: 0 differ over 20,000 iterations; 15 breaks caught |
| 12, profiles | `+0x0a8`..`+0x0cc` (ints, int lists, doubles, double lists, strings in FLTSIM98.CFG), `+0x0f8` (empty a section), `+0x100` (does a section have keys), and `+0x0d0`..`+0x0f4`, `+0x0fc`, `+0x104` (the same over path + extension) | verified: 0 differ over 20,000 iterations |
| 13, files | `+0x17c` open, `+0x148` create, `+0x13c` close, `+0x19c` read, `+0x1b0` write, `+0x194` seek, `+0x190` tell, `+0x1a4` size, `+0x1a8` truncate, `+0x134`/`+0x138` attributes, `+0x158` exists, `+0x14c` delete, `+0x144` copy, `+0x16c` move, `+0x11c` make a directory | verified: 0 differ over 20,000 iterations |
| 14, directory enumeration | `+0x164` find first, `+0x160` find next, `+0x15c` close | verified: 0 differ |
| 15, messages, the clock, and the fatal file wrappers | `+0x024`/`+0x028` (quit by WM_CLOSE / command 0x110a), `+0x038` timeGetTime, `+0x03c`/`+0x040` (post / send a WM_COMMAND to the main window), `+0x044` (local date and time), `+0x07c`/`+0x080`/`+0x084` (a view window's titles and notifications), `+0x180`/`+0x1a0`/`+0x198`/`+0x140` (open, read, seek, close -- fatal on failure) | verified: 0 differ |
| 16, dirty rectangles | `+0x088` (a rectangle into a view window's dirty list, merged with one within 16 pixels) | verified: 0 differ |
| 17, joysticks | `+0x1c0` (enumerate), `+0x1c4` (read one: axes, buttons, POV), `+0x1c8` (describe one: axes, buttons, rudder, and the language DLL's name for it by its registry OEM name), `+0x1cc` (wait up to 5 s for an axis or button to move) | verified: 0 differ; 24 breaks caught |
| 18, odds and ends | `+0x2c4` (shorten a string with "..."), `+0x274`/`+0x278` (capture / release the mouse), `+0x238` (validate the heaps), `+0x150` (free disk space), `+0x174` (a temporary file name), `+0x208` (fold the keypad), `+0x170` (the length of a path made absolute) | verified: 0 differ |
| 19, the main menu | `+0x23c` (set the menu bar from a definition), `+0x240` (drop it), `+0x244`/`+0x248` (append an item / items to an item's submenu), `+0x24c`/`+0x250` (insert before an item), `+0x254` (gray / check), `+0x258` (an item's flags), `+0x25c` (remove), `+0x260`/`+0x264` (change id / text), `+0x268` (exists) | verified: 0 differ; 20 breaks caught |
| 20, modules | `+0x26c` (load a module by path and extension: its Linkage), `+0x270` (release it), `+0x294` (load a language module and put it first for strings) | verified: 0 differ |
| 21, the busy cursor | `+0x030` (the counterpart of `+0x2c`), `+0x050` (the hourglass on / off) | verified: 0 differ |
| 22, sound | `+0x2d4` new voice, `+0x2dc` drop, `+0x2d8` append, `+0x2e0` play, `+0x2e4` frequency, `+0x2ec` volume, `+0x2f0` pan, `+0x2f4` stop, `+0x2e8` state, `+0x2d0` sound on/off | verified: 0 differ over 20,000 iterations |
| 23, view-window surfaces and order | `+0x074` (a record's host block), `+0x078` (free it), `+0x06c`/`+0x070` (lock / unlock the drawing surface: DIB or DirectDraw), `+0x068` (destroy a view window), `+0x090` (bring to the front of its group) | verified: 0 differ; 18 breaks, 17 caught, 1 equivalent |
| 24, tooltip regions | `+0x27c` (register a tree of tooltip regions), `+0x280` (re-place it), `+0x284` (unregister it) | verified: 0 differ; 15 breaks caught |
| 25, view windows and the palette | `+0x064` (make a view window: frame, client, z-order group), `+0x04c` (size the main window for 640x480), `+0x094` (the nth visible record of a group key), `+0x128` (clear three words, free the DirectPlay library), `+0x290` (set the palette: GDI, DirectDraw, tinted tables), `+0x314` (a no-op) | verified: 0 differ; 52 breaks, 49 caught, 3 equivalent |
| 26, text and DirectPlay | `+0x318` (draw text in a view window, four fonts, or measure it), `+0x08c` (a shadowed label at one of fifteen places in a view), `+0x124` (load dplayx and hand out its three entry points), `+0x120` (the lobby's connection settings, raw or flattened) | verified: 0 differ; 66 breaks, 63 caught, 3 equivalent |
| 27, display settings | `+0x31c` (read the HARDWARE section or its defaults, list the DirectDraw/Direct3D devices, list a device's modes, write or remove the section) | verified: 0 differ |
| 28, display mode and dialogs | `+0x048` (full screen on/off: window styles, DirectDraw torn down and rebuilt, the display mode changed on a thread), `+0x10c`/`+0x110`/`+0x118` (run a dialog, modal or modeless), `+0x114` (tick the modeless dialogs) | verified: 0 differ |

The harness now seeds `.data` with distinct random values on both sides before
each iteration (leaving the service table and bounding the string constant at
0x41bca0), so a service that reads the wrong offset of an otherwise-zero global
is caught -- `make host-breaks` runs 82 deliberate breaks: 79 caught, 3 proven
equivalent (a hang after two minutes counts as caught). (An
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

Slice 7 is the first that calls the window system and the command line.
**The command line is the harness's**: `GetCommandLineA` is hooked on both
sides (like `realloc`) to return a command line made up each iteration, with
a byte before it -- the option finder tests each '/' or '-' against the byte
before it, the first included, so retail reads one byte before
`GetCommandLineA`'s buffer. An option is "/name:value" or "/name value" (or
'-'); `+0x98` (and so `+0x9c`/`+0xa0`) accept only the colon form and return
the value up to the next option, trimmed of spaces -- and an empty value
trims from the byte *before* the caller's buffer, clearing it if it is a
space. `+0xa4` returns 2 for the colon form, 1 for the space form, 0 if absent.

**File extensions have ids.** 37 are built in, a table at 0x417f20 of pointers
to ".BGL", ".STN", ".MOD" ... ".CFG" (id 0 is the host's empty string at
0x4187f8, so a missing extension is id 0); more are registered at run time
from 0x25 on, `_strdup`'d into a heap array (`*0x4188b0`, count at
0x4188ac). Lookups upper-case the name in place. The table holds retail
addresses, which our image keeps as they are (so the `.data` comparison stays
byte-for-byte) and rebases when it reads them (`host_rebase`). The harness
keeps that table, its strings and the empty string out of the random seeding,
and gives each side an identical registry whose names it compares through each
side's own array.

`+0x22c`/`+0x234` take a stdcall comparator, parked at 0x41a24c for a cdecl
trampoline -- not reentrant. Both sides call one MSVCRT, so ties sort alike.
`+0x22c`'s return is whatever MSVCRT's `qsort` left in eax; ours returns 0,
and no caller reads it.

Slice 8 reaches string resources, which the harness can serve because
**LoadStringA works on the mapped host**: wine finds resources from a module's
own headers, so `LoadStringA(0x400000, 9)` reads the real host's string table
although the loader never loaded it. The strings the game asks for mostly live
in the language DLL, ENGLISH.FLL ("Button ", "Axis ", "Generic_%d" ...),
which the harness loads as a data file, with SIMSUI.DLL and DIALOG.DLL; it
seeds the host's string-module list (0x4182e0, HMODULEs) and its module
records (0x417b30, 0x14 bytes: HMODULE at +8, id at +0xc, loaded at +0x10)
on both sides with those handles, and the host instance (0x41882c) with
0x400000. `+0x1d0`/`+0x1d4` load the two input-name prefixes on first use
(flag 0x41a240); the harness seeds that flag both ways, and, when set, the
prefixes and their lengths with short strings of its own. Quirks reproduced:
both return whatever eax holds (wsprintfA's count, the loader's last length,
or the last compare); a number past 64 buttons or 16 axes gives code 0; and
the axis prefix is compared from just past a matched button prefix.

The **image buffers** of slice 9 are a 0x20-byte header -- format (0..5),
width, height, stride, row table, size, pixels -- with the bytes per pixel of
each format (1 1 2 2 3 4) in a table at 0x4180e8 and the stride rounded up
to 4. Pixels and row table are zeroed; a resize discards the contents. The
name checks refuse `" & * / < > ? |` (a file name also `:` and `\`), let
everything outside 0x22..0x7c through, and return 0x1003 for an empty name.

**Accelerator tables** (slice 10) are the host's own, not Windows': a list
at 0x418278 of 0x10-byte tables { enabled, id, entries, count }, ids from a
counter at 0x418294 that skips 0 on wrapping (but hands out 0 if it starts
there), entries of 12 bytes { command, flags (1 shift, 2 ctrl, 4 alt, 8
virtual key), key + 0x10000 } converted from Windows' 6-byte ACCEL. `+0x1e4`
loads an accelerator resource where strings are found (the string modules,
then the host instance) -- ENGLISH.FLL has three, 103 to 105, the first with
148 entries; the harness sometimes makes ENGLISH.FLL the host instance so the
fallback is exercised. `+0x1dc` takes a *pointer* to the id and zeroes it;
`+0x1f8` removes every entry equal to one given, zeroing the vacated slot,
and only then shrinks the array (freeing it if empty); `+0x1fc`'s second
argument is unused and it does anything only in mode 2.

**Key names** (slice 11) use ten words the host's init loads from
ENGLISH.FLL (strings 0x9520..0x9529: "CTRL + ", "SHIFT + ", "ALT + ",
"NumLock", "Num Enter", "Num /", "Break", "Menu", "Right Windows", "Left
Windows") into pointers at 0x41bec0 and nearby, which the harness points at
its own copies on both sides. `+0x200` names a key code (virtual key in the
low byte, 2 ctrl / 1 shift / 4 alt above it): the modifier words, then the
host's word for keys 3/0x13, 0x5b..0x5d, 0x6f, 0x87, 0x90, else
GetKeyNameTextA of the scan code with the don't-care bit. It passes `len -
used - 1` on unchecked, so a buffer shorter than the modifier words makes a
negative count -- strncpy's zero fill then runs off the end of memory; the
harness keeps buffers long enough. `+0x204` parses a name back, trying the
host's words as prefixes and then every virtual key 0..0xff by
case-insensitive name (0x100 when none matches).

**Files and profiles** (slices 12 to 14) are tested against real files, each
side in its own directory -- `hostfs\r\` for the real host, `hostfs\o\`
for ours, under the harness's directory -- with paths of one length that
differ only in that letter (the CFG path at 0x41c120 is compared everywhere
but that byte). Each iteration writes the same made-up profile and the same
few files (some read-only) to both, runs random operations on each side, and
compares the results, the host file objects (mode, extension, path), and the
two directories file by file. A profile gets a new name every iteration so
wine's profile cache can never serve a stale copy. Two constraints of the
harness, not the host: the format strings the profile code uses ("%d" at
0x417a24, ", " / "," / "%f" at 0x417ed0) must not be randomised with the
rest of `.data`, and `+0x150` (free disk space) is left out because the
free space changes between the two calls.

Quirks reproduced: profile lists are read into a block of 15 bytes an int or
30 a double and written into one the same size, unchecked; an absent double
leaves the output alone, an absent int returns the default; `+0x104` takes
its output *last*, after path and extension (the first harness run caught
our version taking it second); `+0x17c` opens unshared, and its mode 3 seeks
to the end; `+0x194` refuses any position past the size whatever the method;
`+0x11c` leaks its path copy when CreateDirectoryA fails; `+0x13c`, `+0x17c`
and `+0x194` record GetLastError at 0x41ba50 (or 0), the others do not;
`+0x164` with an empty directory string tests the byte *before* its path
buffer -- the top byte of the search block's address -- for '\'; `+0x160`
looks a file's extension up without registering it, leaving the type alone
for an unknown one.

Slice 15 hooks the window system and the clock on both sides --
`SendMessageA`, `PostMessageA`, `SetWindowTextA`, `timeGetTime`,
`GetLocalTime` -- into one call log the harness clears before each side's
call and compares after, with made-up results the same for both; and
`MessageBoxA`, so that a fatal path, should one be reached, exits at once
rather than leaving a dialog open. The fatal file wrappers are driven only
on their non-fatal paths, over a file both sides always have.

**Joysticks** (slice 17) are a model in the harness, hooked on both sides
like the window system: `joyGetNumDevs`, `joyGetPosEx` and `joyGetDevCapsA`
over up to six made-up devices whose positions change after a scripted
number of polls, `Sleep` (the slow probe sleeps 260 ms), and
`RegOpenKeyExA`/`RegQueryValueExA`/`RegCloseKey` over the two keys the host
reads a joystick's OEM name through. The script restarts before each side's
call, and `timeGetTime` advances 700 ms a call so `+0x1cc`'s 5-second wait
ends. The host keeps its joystick table at 0x4188b8 (16 entries: the WINMM id
+ 1, then the JOYCAPSA); `+0x1c4` cuts the caller's axis mask to the axes the
caps report -- one bit for each of R, U, V and Z the device has, then one more
for every axis left over, counted as a 16-bit number that wraps when the caps
claim fewer axes than flags -- and maps the POV to eight directions with
0x8ca-wide bands either side of north.

Slice 18's quirks: `+0x2c4` tests strcpy's result the wrong way round, so
whenever it actually shortens a string it returns -1 and leaks its working
copy; `+0x150` multiplies sectors-per-cluster by bytes-per-sector in 32 bits
before widening; `+0x208` turns an extended Enter into 0x87 without setting
the folded flag. The mouse, heap, disk and temporary-name calls are hooked
into the call log (`GetProcessHeaps`/`HeapValidate` over a made-up set of
heaps, so a failed validation can be shown).

**The main menu** (slice 19) is the host's own tree of 0x14-byte items --
parent id, id (0xffff ends an array, 1 is a separator), text (a string id
below 0x10000, else a pointer), flags (1 checked, 2 grayed), submenu items,
HMENU -- rooted in a one-item array at 0x418298 (the bar, id 0). Every change
rebuilds all the HMENUs from the tree (`CreateMenu`/`AppendMenuA`, hooked,
handles numbered from a counter each side restarts). Retail trusts the tree:
it finds an item's array through the item's own parent-id field, so an item
whose field is wrong, a renumbered parent, the bar itself (whose "parent" is
the bar), or an append to an item with no submenu all send it running off
an array -- the harness keeps ids unique and fresh and never asks for those.
Quirks: a removed item's submenu is never freed; `+0x254` rebuilds the bar
before redrawing when the parent's own parent id is 0 (while 0x4182c0 is
set).

**Modules** (slice 20): the host's module records (0x417b30: name, id,
HMODULE, Linkage, references) are filled by loading a module once to read its
Linkage (`GetProcAddress(h, "Linkage")`, its dialog table at +0x14 registered
on the dialog registry at 0x417ee0), and a module is loaded while referenced:
the first reference resolves its ImportTable (`{ id, slot }` pairs, each slot
given the Linkage of the module with that id, acquired in turn) and calls its
init (Linkage +4, cleared after), the last calls its deinit (+8), releases its
imports and frees it. The harness fakes four modules (with imports and dialog
tables) and two language modules behind hooked `LoadLibraryA`,
`GetProcAddress` and `FreeLibrary`, restoring their Linkage blocks before each
side's call. **Correction:** `+0x298` takes a module's *Linkage* (records are
found by it at +0xc), not a module id.

**Sound** (slice 22) runs over a fake IDirectSound and a pool of fake
buffers (each call logged; failures injected by a script both sides replay;
Unlock logs a checksum of what was written), fake MSACM32 streams, and real
.wav fixtures (8- and 16-bit PCM, a non-PCM one converted through ACM, a
truncated one, a non-RIFF one). Both sides' `malloc` now fills new blocks with
one pattern, because a voice leaves fields uninitialised that the state
service reads. Retail's sound has several holes the harness has to step
round, all reproduced in ours: a duplicate (type 6/7) that fails to get a
buffer frees the sound it shares with its source, which keeps a dangling
pointer; appending to a duplicate does the same; a type 7 duplicate of a type
5 voice (which frees its sound once buffered) copies from null, as does
appending to or from such a voice; `+0x2e4`/`+0x2f0`/`+0x2f4`/`+0x2e8` use the
buffer (and `+0x2e8` its voice) unchecked; and a start that fails at the
primary buffer leaves DirectSound set with no primary. The frequency is the
rate times a double multiplier, computed in the x87 register and truncated
without being rounded to a double first (`long double` in our C) -- a
non-x87 build has to reproduce that.

**View-window surfaces** (slice 23) run over records the harness builds per
side (with host blocks and row tables), a fake DirectDraw surface (Lock,
Restore, Unlock, failing by script), and hooked `GetObjectA`,
`DeleteObject`, `DeleteDC`, `DestroyWindow`, `GetParent` and the
`DeferWindowPos` trio; the z-order groups at 0x41a3dc are compared
structurally. `+0x074` overwrites a record's host block without freeing the
old one; `+0x06c` refills the row table only when its first row moved; the
locks, `+0x068` and the restack read the host block unchecked, so the harness
keeps blocks in place for them.

**Tooltip regions** (slice 24): a window's hot regions are a tree of
0x2c-byte nodes in arrays (type 1 opens a group whose nodes follow, 3 closes
it, 0 ends the list), each given a heap rectangle at the offsets it is
placed at and, if it has a tip string, a tool in the window's tooltip control
(`tooltips_class32`, made on first use) by TTM_ADDTOOL with the rectangle
scaled by the window's 16.16 factors. Retail registers the tools *before*
checking the tree fits, nested nodes grow the outermost bounds rather than
their group's, and the check reads every top-level node as a group, so a
tree whose top level is not all groups sends it scanning past the end for a
closing 3 -- the harness's trees have groups at the top. `SendMessageA`'s log
records a TOOLINFO's fields and its rectangle, not the stack pointer.

**View windows and the palette** (slice 25). `+0x064` makes a record's host
block as `+0x074` does, a hidden "FS98CHILD" frame at the record's rectangle
and a "FS98FLOAT" client, puts the record at the front of its z-order group
(0x411710: groups in descending key order, a new one before the first with a
smaller key; a record already in its group is left where it is), and sends
the frame 0x467; if either window fails it frees the block and returns -1.
`+0x094` walks *every* group of a key, its count of visible (top-bit) records
restarting in each. `+0x290` copies the ten system colours at each end of the
master palette (0x4171a4) to its copy (0x4175ac) and, as BGR, to 0x41adc0;
given RGB triples it sets entries 10..245 of master and copy (PC_RESERVED),
builds the BGR table at 0x41b620 (0x40e540: grey at half brightness when
0x41a250 is set, else the animated ranges as a dim red of their luminance),
hands the master to DirectDraw in a full-screen 8-bit mode (0x412b50, through
0x412bf0, which reads the screen format from the primary surface or the
screen DC), and animates the GDI palette (realising it and redrawing by day)
or, on a non-palette display, sets its entries. It returns `ReleaseDC`'s
result. The harness hooks `ShowWindow`, `UpdateWindow`, `GetWindowLongA`,
`GetMenu`, `AdjustWindowRectEx`, `SetWindowPos`, `GetDC`/`ReleaseDC`,
`GetDeviceCaps` and the GDI palette calls, makes `CreateWindowExA` fail by
script, and gives fake DirectDraw, primary-surface and palette objects.
Three breaks are proven equivalent through `+0x290`: `(x/1000)/2` is `x/2000`
for non-negative x, and 0x412b50 reads only whether the depth is 8, not the
width, height or the 555-means-15 rewrite (they matter to `+0x048`/`+0x31c`).

**Text and DirectPlay** (slice 26). `+0x318` and `+0x08c` draw through a
view record's DC (0x4108f0: the DirectDraw surface's, unlocking a locked
record first and keeping its lock flag so the release, 0x4109f0, relocks it --
which reads the row table unchecked, as `+0x06c` does -- else the host
block's memory DC with its bitmap selected). Colours go through the master
palette for a record with 0x8000 in the 8-bit mode, else as palette
indices. `+0x318`'s fonts: kind 0 is a 7-pixel font made afresh on every
call (a GDI leak in retail, reproduced), 1 the stock fixed font, 2 and 3
plain and bold fonts from string resources 0x10/0x11 (`Verdana,-9,1` and
`Verdana,-11,1` in ENGLISH.FLL), made once; another kind returns -1 holding
the DC. `+0x08c` capitalises the label and uses the small font for a record
with 0x10000 outside code page 932, and shadows it in colour 0 otherwise.
`+0x124` loads dplayx once and, if DirectPlayCreate and a QueryInterface for
IDirectPlay2A work, hands out DirectPlayCreate, DirectPlayEnumerateA and
DirectPlayLobbyCreateA; else it frees it again through `+0x128`. `+0x120`
asks the lobby for its connection settings and returns them raw or
flattened into one block with offsets for pointers (the session password's
pointer is copied as it is, into the freed copy); a lobby that has none
returns 0 without being released, and a dplayx it loaded is freed with
0x41a51c left set. The harness fakes dplayx, DirectPlay, the lobby and the
GDI text calls, and now counts both sides' `free` calls (a missing free of
the lobby's copy showed only there). Three text breaks are proven
equivalent: the DC handed back is always the one 0x4108f0 gave, `atoi`
stops at the comma the size cut replaces, and the face cut differs only for
a string without a comma, which no shipped resource has.

**Display settings** (slice 27). `+0x31c(op, block)` works on a 0xe8-byte
settings block: device string (+0x00), its Direct3D suffix (+0x80, one of
"(Direct3D)", "(Direct3D MMX)", "(Direct3D RGB)", "(Direct3D RAMP)"), mode
"WxHxB" (+0xa8), option flags (+0xd0, named by the table at 0x418458 --
DirectDrawDisable, Direct3D, DirectDraw, MMX, Filter, Dither, MipMap,
PageFlip, and four texture/fog keys -- in FLTSIM98.CFG's `[HARDWARE]`
section), the parsed mode and a list cursor (+0xe4). Ops 1/2 read it (or
the defaults; an empty device becomes the default driver's label, a bad
mode 640x480 at 16 bits with a suffix, 8 without); 3 lists the devices
(DirectDrawEnumerateA, each driver's label -- "desc (name)" for a
"\\." name -- and its HAL/MMX Direct3D devices); 4 lists the device's
modes (8-bit without a suffix or with RAMP, else 16-bit ones the video
memory holds three times over plus 1 MB); 5/6 write or delete the section
and post the main window 0x46f. The profile section name and the GUIDs it
compares live in `.rdata` (0x416120..0x41616f), outside the `.data` image, so
host.c carries its own copies. The harness fakes DirectDrawCreate and
DirectDrawEnumerateA with scripted drivers, Direct3D devices and modes, over
real CFG files. Four breaks are proven equivalent: the label's extra text is
always empty at its one caller (cleared on the line before), no suffix ends
with another, and the flags are read into a block just zeroed.

**Display mode and dialogs** (slice 28). `+0x048(on)` is the full-screen
toggle. Going full screen keeps the window's zoom, rectangle and styles,
makes it a popup without a menu covering the screen, tears DirectDraw down
(0x412b10: the views told 0x46d by a broadcast to the main window's children
and this thread's "FS98FLOAT" windows, page flipping off, the surfaces,
Direct3D and the object released) and rebuilds it (0x412690: the settings
re-read, the chosen driver opened -- or the default -- with exclusive
cooperation, the mode set, 640x480 if that fails, Direct3D kept if a device
with hardware rasterisation and enough video memory exists, the primary
surface made, flipping with a back buffer when page flipping is on), then
changes the display mode with ChangeDisplaySettingsA on a thread of its own
(0x4038d0; retried without the depth if it asks for a restart) unless a
secondary driver owns the screen. Going back restores the mode, the style
(the extended style is not restored), the menu and the rectangle. The
harness models the window calls, the thread's ChangeDisplaySettingsA, and
DirectDraw's palettes, surfaces, cooperative level and display modes, each
failing by script. The dialog services are described under "The dialog
engine" above.

Finishing the table turned up harness faults worth knowing before writing the
next one. A fake COM vtable must hold the highest slot any caller uses
(IDirectDraw's SetDisplayMode is slot 21; a 16-entry table was overrun into
its neighbours). Per-side state must be drawn from the random stream once and
copied, never drawn twice. A block the host frees must be the heap's: the
sound test's in-memory sounds were static arrays, and freeing one damaged
wine's heap a thousand iterations later -- they now come from a pool the
harness's `free` leaves alone, since retail goes on using a freed sound. A
moving `realloc` must not free the old block at once, or alternating growth
lands the next block on it and a stale pointer reads live data again (the
removal queue's reload went unnoticed that way); old blocks are now
quarantined. And `.data` strings a service compares against (the window
classes at 0x417168) must be kept out of the random seeding, or the paths
behind them are never taken. `HOSTDIFF_HEAPCHK=1` validates the heap after
every check.

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
services (`sprintf`-family and friends) -- with the imports bound, anything
that calls only MSVCRT, or a window-system call the harness can hook as it
hooks `GetCommandLineA`, is as testable as the pure services; then the ones
that call a host-internal helper that is itself pure. `+0x204` (a key-name
parser) waits on the strings the host loads at run time into 0x41bec0 and
its neighbours; the file services (`+0x1a4`/`+0x1a8` and the rest) want a
sandboxed directory. Nothing is left: every slot is reimplemented. What remains is the
harness's reach -- the paths noted above that only a scripted model covers --
and the move from verifying in process to running OpenFS98's host in the
game.

## Status

All 194 services reimplemented in `src/host/host.c` and verified against the
real host in process (slices 1 to 28 above). 71 have prototypes confirmed from the
callee and recorded in `host.h` (15 of them the window services, from WINDOW),
and 11 were already exercised in-process through other modules' harnesses: the
clock, `strlen`, and the nine-service enumeration cluster above. The dialog
registry's contents are checked in-process too. 13 more are named from their
shape alone, and all 194 are classified by subsystem. Sizes reported by the
mapper are upper bounds: they are the gap to the next table-referenced address,
and internal helpers that no slot points at fall inside those gaps.
