# mpchat.DLL — module id 0xffffffff

**Reimplemented, verified, and running in-game.** MPCHAT is the multiplayer chat window: a
tool window of its own, class `FS98MPCHAT`, with the transcript in a
RichEdit on the left three quarters, the players in a ListBox on the right,
and a line to type in and a Send button under them. It appears when a
multiplayer session begins and goes when it ends; the "Views" menu item
`0x1183` and two keys show and hide it in between.

`src/mpchat/mpchat.c` is the transliteration and the module shell together.
`make mpchatdiff-run` drives retail and the OpenFS98 build over 20,000
whole-module scenarios against a model of the window system, comparing
every call MPCHAT makes and its whole `.data` image: **0 differ**.
`make mpchat-breaks` applies 117 deliberate breaks: 113 caught, 4 proven
equivalent. `make smoke` passes with it installed: 27/27 modules, 95 gauges.
A session needs a multiplayer connection, which the smoke test does not
make, so in-game that proves init (RICHED32 loaded, the class registered,
list 0x1d joined); the differential harness covers the rest.

The preflight ([../preflight.md](../preflight.md), round 9) expected this:
"the USER32 calls hooked through its IAT against a model of window text and
list contents". Like WINMSG and REALITY, what MPCHAT does is entirely in the
calls it makes, so a model of the other side of those calls is enough.

## Binary and linkage

- Retail image base `0x20840000`; `.text` 3,104 bytes (MSVC C, `0x1000` to
  `0x1c12`); `.data` at `0x20843000`, 0x200 bytes on disk, 0x50c mapped.
  The CD and setup both name it `mpchat.DLL`, in lower case; `make install`
  keeps the name.
- Imports: host, GLOBAL (unused), CHAIN, MULTIP (35), WINMSG (22).
- Win32: USER32 (16 functions: windows, messages, the class, the scroll
  bar), `LoadLibraryA("RICHED32.DLL")` so that the `RichEdit` class exists,
  `FreeLibrary`, and GDI32 `GetStockObject`.
- One `.data` image: `ImportTable` at `+0`, the dialog table at `+0x30`,
  `Linkage` at `+0x48`, strings and state after. Its relocations are the
  dialog table's `create`, the Linkage's two code pointers and Linkage
  `+0x14`, which points back at the dialog table (`tools/gen_mpchat_data.py`
  zeroes all four; `mpchat.c` installs its own).

| Linkage | retail | |
| --- | --- | --- |
| `+0x00` | `0xffffffff` | the id: nothing can import MPCHAT |
| `+0x04` | `.text+0x020` | init |
| `+0x08` | `.text+0x120` | deinit |
| `+0x14` | `.data+0x30` | the dialog table: one entry, `{ 0xff00, .text+0x430, 0xc }`, then zeros |

The id is not a parser bug in `modgraph.py` (as architecture.md once
allowed): MPCHAT really is `-1`. It needs no id because nothing calls it;
the host's module scanner registers its dialog from Linkage `+0x14`
([host.md](host.md)), and MULTIP talks to it on CHAIN list 0x1d. Dialog
`0xff00` has flags 4 (modeless) and 8 (leave the sim's pause alone).

## State

| `.data` | |
| --- | --- |
| `+0x060` | `"FS98MPCHAT"`, the class |
| `+0x06c` | `"RICHED32.DLL"` |
| `+0x07c`, `+0x080` | `0xc000` twice: the transcript's share of the width, 16.16; only `+0x80` is read |
| `+0x084`, `+0x088` | `" "` and `":"` |
| `+0x08c`..`+0x0a4` | `"Button"`, `"Edit"`, `"ListBox"`, `"RichEdit"` |
| `+0x0b0` | `"\n"` |
| `+0x0b4` | MPCHAT's own instance (host `+0x298`), the class's |
| `+0x0b8` | the players: MULTIP's `{count, records}` from `+0x24`, or list 0x1d's message 3 |
| `+0x0bc` | a second list of the same shape, list 0x1d's message 4 (MULTIP never sends it) |
| `+0x0c0` | the local record, as `+0x2c` gave it on the last send |
| `+0x0c4`, `+0x0c8` | the sim's main window and instance (host `+0x54`) |
| `+0x0cc`, `+0x0d0`, `+0x0d4` | host copies of strings `0x9400` (the title), `0x9401` (the button) and `0x9402` (the name of a sender MULTIP does not know) |
| `+0x0d8` | the window |
| `+0x0dc`, `+0x0e0`, `+0x0e4`, `+0x0ec` | the RichEdit (id `0x100`), Edit (`0x101`), ListBox (`0x102`) and Button (`0x103`); `+0x0e8` is unused |
| `+0x0f0` | `""` |
| `+0x100` | the chat message sent: `{0x1020, size, text}`, up to 0x3fe characters |
| `+0x508` | `RICHED32.DLL`'s module |

## What it does

**Init** loads `RICHED32.DLL`, asks the host for its own instance, the three
strings and the main window, registers the class (`CS_GLOBALCLASS |
CS_HREDRAW | CS_VREDRAW`, `DLGWINDOWEXTRA` bytes, the arrow, the light grey
brush; the procedure falls through to `DefDlgProcA`) and joins CHAIN list
0x1d. **Deinit** undoes it.

List **0x1d** is MULTIP's ([multip.md](multip.md)):

| message | | |
| --- | --- | --- |
| 1 | a session began | dialog `0xff00` run, the list filled, the menu item `0x1183` added under `0x1108` (its "text" is `0x1183` too, a number, not a pointer), lists 0x14, 0xb and 0xc joined |
| 2 | it ended | the lists left, the menu item removed, the window destroyed, `+0xb8`, `+0xbc`, `+0xc0` zeroed |
| 3, 4 | a player list | stored at `+0xb8` or `+0xbc`, the ListBox refilled: `+0xbc`'s names, then `+0xb8`'s |
| 5 | `{sender, payload, size}` | a payload of type `0x1020` is a line of chat, shown under the sender's name (MULTIP `+0x28`) or string `0x9402` |

The dialog's `create` makes the window (`WS_EX_TOOLWINDOW`, popup with a
caption, system menu and sizing frame, 400 by 200, owned by the main window)
and shows it without activating it, then takes MULTIP's player list. The
host then shows it with `SW_SHOW` itself. On `WM_CREATE` the procedure lays
out the controls from the client rectangle (right `r`, bottom `b`):

| | x | y | width | height |
| --- | --- | --- | --- | --- |
| RichEdit | 2 | 2 | `r·0xc000 >> 16` − 3 | `b` − 0x23 |
| ListBox | `r·0xc000 >> 16` + 1 | 2 | `r` − that − 1 | `b` − 0x23 |
| Edit | 2 | `b` − 0x1e | `r` − 0x50 | 0x19 |
| Button | `r` − 0x4b | `b` − 0x1e | 0x46 | 0x19 |

and again on `WM_SIZE`, through `SetWindowPos`. `WM_SETFOCUS` puts the caret
in the Edit; `WM_CLOSE` posts command `0x1183` to the main window, as the menu item
would (it comes back on list 0xb, the sim's commands, and hides the window),
rather than closing; the RichEdit taking the focus has its
selection dropped (`EM_SETSEL -1`).

**Send** (`BN_CLICKED` from `0x103`, the default button)
reads up to 0x3fe characters of the line. If there are any it sends
`{0x1020, length + 1, text}` to everyone (MULTIP `+0x20(0, msg, length + 9,
0)`), shows the line under the local player's name (MULTIP `+0x2c`, or `""`
with no local record), and clears the Edit. Either way the keyboard then goes
back to the sim's main window.

A line is shown (`.text+0x15b0`) as `name:` in blue (`0xff0000`, a
COLORREF), indented 40 twips, after two line breaks, then the text, indented
400, after one; no breaks into an empty transcript. The append
(`.text+0x1a50`) moves the selection to the end if it was not there, and puts
it back afterwards, and scrolls to the bottom (`WM_VSCROLL`,
`SB_THUMBPOSITION`) when the new text runs past the bottom (`nMax > nPage +
nPos`) and the view was at the bottom before (`nPos > nMax − nPage`) or its
scroll state could not be read. When the window is hidden a received line
also goes up on WINMSG's bar, slot 6, for 0x5a frames, as `name:`, `" "` and
the text appended.

Lists **0xb** and **0xc** and **0x14**: command `0x1183` toggles the window;
`0x10170` toggles it and `0x10171` shows it and puts the caret in the line,
either answered with 1 through the second argument; message `0x40` sets the
menu item's check mark from the window's visibility (host `+0x254`).

## Retail behaviour worth knowing

All reproduced unless said otherwise.

- **Send hands the keyboard back to the sim.** After a click on Send the
  focus goes to the main window (`+0xc4`), not back to the line; a second
  line needs a click in the Edit, or key `0x10171`.
- **The default button is set through `DefDlgProcA`**, called directly with
  `DM_SETDEFID`, not sent to the window.
- **The scroll position is packed from a 16-bit maximum.** `WM_VSCROLL`'s
  wParam is `((nPage << 16) − nPage + nMax) << 16 | 4`; the first shift is
  lost to the second, so the position is `(nMax − nPage) & 0xffff`.
- **A name of 0x3fe characters or more overruns the stack**: the line is
  built with `strcat` in a 0x400-byte buffer. DirectPlay names are not that
  long in practice; the harness keeps them under 0x300.
- **A missing string `0x9402` crashes** the first line from an unknown
  sender (`strcat` of a null pointer, inside the host). Seen in the harness
  as a fault on both sides; the game always has the string.
- **The RichEdit append reads its selection uninitialised** when the
  RichEdit is gone (a line arriving after the session's window has been
  destroyed, when `+0xdc` still names it): `EM_EXGETSEL` to a dead window
  writes nothing. The harness's model writes zeros there so the two sides
  compare.
- **Message 4's list is shown but never sent.** MULTIP dispatches 1, 2, 3
  and 5 on list 0x1d; `+0xbc` stays null in the game.
- **Callbacks return what eax held.** CHAIN discards a callback's eax, so
  the entry eax that retail's list 0x14 and 0xb callbacks return for a
  message they ignore is not reproduced; nor are their leftover ecx and
  edx, which no dispatcher's caller reads after a C module's callback.
- **The unmasked fields of the RichEdit formats are stack garbage** in
  retail (`PARAFORMAT2` past the indent, `CHARFORMATA` past the colour);
  ours are zero. RichEdit reads only the masked fields.

## mpchatdiff

`tools/difftest/mpchatdiff.c` loads retail `mpchat.DLL` and the OpenFS98
build side by side (ImportTable at image+0, Linkage at +0x48), replaces
every one of their twenty Win32 imports with a model, and runs each
scenario on both:

- **the window system**: windows numbered alike on both sides; the class's
  procedure called as Windows calls it (`WM_GETMINMAXINFO`, `WM_NCCREATE`,
  `WM_NCCALCSIZE`, `WM_CREATE` from `CreateWindowExA`; `WM_SIZE` at the
  creation or the first show, by scenario; `WM_SHOWWINDOW`, `WM_ACTIVATE`
  and `WM_SETFOCUS` from `ShowWindow`; `WM_DESTROY` from `DestroyWindow`),
  a ListBox that keeps its rows, an Edit that keeps the user's line, a
  RichEdit with its text and selection, and a `GetScrollInfo` whose answers
  sit on each of the append's two comparisons, one either side, or anywhere;
  client rectangles from the window's size, and now and then narrower than
  the margins, negative, or at the width where `r·0xc000` wraps;
- **the host**: the dialog runner (the table read from the side's own
  Linkage `+0x14`, then `ShowWindow(SW_SHOW)` and `UpdateWindow`, as
  `0x409720` does), strings that are sometimes missing, the menu, the module
  stack;
- **MULTIP**: player lists in memory both sides share, renamed, shortened
  and replaced between ops, the local record changed;
- **CHAIN** add and remove per side (an add sometimes fails), and WINMSG
  `+0x18` logged.

The ops are MULTIP's session news (begin, end, both lists, chat lines of
type `0x1020` and others, from known and unknown senders), the menu
command, the keys, message `0x40`, window messages (resizes, focus, close,
the RichEdit's `EN_SETFOCUS`, Send clicked and its other notifications,
other messages), a line typed (empty, short, exactly 0x3fe and longer than
the read) and sent, the user's selection in the transcript, the window
hidden or shown from outside, and deinit and init again. Compared after
every op: the transcript of every call (strings, the formats' masked
fields, the geometry) and its return, the whole `.data` image with code
pointers paired, the heap and the model's windows.

The first runs found two transliteration slips, both now fixed: the default
button sent through `SendMessageA` rather than told to `DefDlgProcA`, and
the focus after Send given back to the line instead of the main window.

`make mpchatdiff-run`: 20,000 scenarios (about 20 s), 0 differences, with
29,711 sessions begun, 38,030 lines sent and 142,838 received, 28,762
scrolls, and 1,183 faults, all the null-name `strcat` above, alike on both
sides. `make mpchat-breaks` (1,000 scenarios a break): 113 of 117 caught;
the four equivalences are the ListBox fill's count guard (the loop tests the
same word), the bold flag (both callers pass 0), the split recomputed before
the ListBox (the same right and the never-written `+0x80`), and create's
return of `+0xd8` against its local (nothing in between writes it). Two
breaks that swapped the sim's instance and MPCHAT's were missed at first,
because the log printed both as "other"; the harness now names them, and
both are caught.

Two harness lessons: a message's parameter is a number, not a pointer --
one `WM_SIZE` lParam (`0x017a0217`) fell inside ours' load range and was
logged as code, a false difference at scenario 3,084 -- and a local's
address differs between the sides, so stack pointers are logged as
`stack`.
