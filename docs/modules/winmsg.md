# WINMSG.DLL — module id 22

**Reimplemented, verified, and running in-game.** WINMSG is the message bar: the strip across
the top of the screen where ATC's tower, LANDME's guidance, DEMO's video
captions and ADVDRV's lesson text appear. Callers put text up in one of seven
slots through `+0x18`; messages queue, and a frame callback on CHAIN list 2
shows the first, either for a count of frames or scrolling across from the
right. When one is done WINMSG says so on CHAIN list 0x14 (message 0x50, with
the node `+0x18` returned), which is how ADVDRV knows to go on.

`src/winmsg/winmsg.c` is the transliteration and the module shell together.
`make winmsgdiff-run` drives retail and the OpenFS98 build over 20,000
whole-module scenarios, comparing everything WINMSG calls and its whole
`.data` image: **0 differ**. `make winmsg-breaks` applies 64 deliberate
breaks: 60 caught, 4 proven equivalent. `make smoke` passes with it installed
beside the other seventeen: 27/27 modules, 95 gauges. Messages only appear in
flight, which wine does not reach, so in-game that proves init and the
window, and the differential harness covers the rest.

The preflight ([../preflight.md](../preflight.md), round 1) set WINMSG aside
for `ofstest` because it owns a window. It turned out not to need one: what it
shows is entirely in the calls it makes to the host (the text it draws, where
and in what colour, the blanks, the show and hide), and the host's
side is four fields of the window record. A model of those is enough, and the
harness compares the calls rather than anything on screen.

## Binary and linkage

- Retail image base `0x20910000`; `.text` 4,416 bytes (MSVC C, `0x1000` to
  `0x2133`); `.data` at `0x20914000`, 0x200 bytes on disk, 0x4a8 mapped.
- Imports: host, GLOBAL, CHAIN, WINDOW (10), MAIN (3).
- Win32: `DisableThreadLibraryCalls`, and USER32 `CharNextA` for stepping
  through a scrolling message.
- One `.data` image: `ImportTable` at `+0`, `Linkage` at `+0x30`, state after.
  Its only relocations are the Linkage's six code pointers
  (`tools/gen_winmsg_data.py` zeroes them; `winmsg.c` installs its own).

| slot | retail | callers | service |
| --- | --- | --- | --- |
| `+0x04` | `.text+0x240` | | init: the queue, WINDOW record type 5 titled string `0x9500`, its window, CHAIN lists 2 and 0x14 |
| `+0x08` | `.text+0x380` | | deinit: the queue and the slots' texts freed; the window is left to message 0x37 |
| `+0x18` | `.text+0x450` | ATC, LANDME, DEMO, ADVDRV | show `(text, slot, time, append)`; returns the last node queued |
| `+0x1c` | `.text+0x5b0` | ATC | clear a slot's messages and its text; returns the queue |
| `+0x20` | `.text+0x620` | ADVDRV | clear every message; returns the head sentinel |
| `+0x24` | `.text+0x640` | ATC | 1 if anything is queued in the slot |

`impscan.py` reports a GLOBAL `+0x1254` *write* at `.text+0xb35`; it is a
`cmp dword [eax+0x1254], 1`.

## State

| `.data` | |
| --- | --- |
| `+0x058` | the slots' colours, 0..6: 0, 0x6a, 0x75, 0x75, 0x75, 0x6a, 0x6a |
| `+0x074` | the byte a blank fills the window's surface with, 0x0b |
| `+0x078` | per slot, the text shown in it (a host string) |
| `+0x098` | 0x400 bytes: the visible part of a scrolling message |
| `+0x498` | the window, WINDOW's record |
| `+0x4a4` | the queue: an 8-byte header of two sentinel nodes |

A node is 0xc bytes, `{data, next, prev}`, and its data a 0x2c-byte message:

| | |
| --- | --- |
| `+0x00`, `+0x04` | its own copy of the text, and the length |
| `+0x08` | slot |
| `+0x0c` | 1: a scroller that stays until replaced (GLOBAL `+0x1254` exactly 1) |
| `+0x10` | frames left (a plain message) |
| `+0x14` | 1 until first drawn |
| `+0x18` | speed, pixels a frame: GLOBAL `+0x362` (the frame rate) / 2 + 2 |
| `+0x1c` | pixels scrolled off the left |
| `+0x20` | pixels still to come in from the right; starts at the limit |
| `+0x24`, `+0x28` | extent: eight pixels a character, and 0xd7 |

Slots 1, 2, 3 and 6 scroll when GLOBAL `+0x1254` is non-zero (MAIN sets it);
the others, and all of them otherwise, are plain. The limit is the window's
pixel width (record `+0x38`) less 30, at most 0x2000.

## What it does

`+0x18` without `append` forgets the slot's text, empties the queue and
queues `text` at the end: a scroller whole, a plain message only if `time` is
non-zero, broken into several if it is wider than the limit. The breaker
(`.text+0xb90`) walks the text a word at a time and cuts a line at the last
space that fitted, each line's time `time * cut / len`.

With `append` the slot's text grows, and the last message queued in the slot
is extended:

- none: the slot's whole text is queued at the end;
- a plain one: it is replaced, where it was, by one with both texts, its time
  what it had left plus `time`;
- a scroller that is the first in the queue and whose tail is on screen
  (`len * 8 - scrolled + to_come <= limit`): a new message with just `text`
  goes after it;
- any other scroller: it takes the text onto its own, in place.

Either way the window is shown (record flag bit 31) and blanked if it was
hidden. The frame callback draws the first message: a plain one once, at
place 3 in its slot's colour, then counts it down; a scroller every frame,
from where it has got to, through the 0x400-byte line (`CharNextA` to skip
what has scrolled off). A plain message at 0, or a scroller all gone off the
left, is sent as 0x50 on list 0x14 and freed. With the queue empty the window
is hidden, unless it has no status line (flag bit 30, which only a restored
situation sets). CHAIN list 0x14 also carries the situation: record 0x3df is
the record's flags and its two rectangles (0x22 in, 0x23 out through MAIN
`+0x5c`..`+0x64`), and 0x37 destroys the window.

The window's rectangle is in 0x2000ths of the screen's width and 0x1800ths of
its height (the host's scales, `0x402a1e`). WINMSG's default, (0x40, 0x32,
0x1f80, 0x12c), is a full-width strip about a twentieth of the screen tall,
at the top.

## Retail behaviour worth knowing

All reproduced unless said otherwise.

- **A scrolling message can start before its buffer.** The start counts down
  by the speed while it is positive, so it ends anywhere down to `1 - speed`,
  and the line is drawn at `line + start / 8`. From about 14 frames a second
  (speed 9) the line can start a byte or more early: the first character
  overwrites the top byte of slot 6's text pointer, or more. A later
  `+0x18` or `+0x1c` in that slot then frees a pointer that is not one. A window
  narrower than 30 pixels starts every scroller early the same way. The
  reimplementation keeps retail's layout, so the same bytes are hit; the
  harness draws 227,748 such frames in its 20,000 scenarios, and 956
  scenarios end on a free of an overwritten pointer.
- **Further back it reaches code pointers.** At frame rates in the hundreds
  the line starts over the colours, the Linkage and the ImportTable, and the
  next call goes through text; past `.data` (retail's is preceded by
  read-only `.rdata`, ours by `.text`) it faults. Both are only reachable with
  frame-rate values no game produces, and the first cannot be compared, so the
  harness keeps GLOBAL `+0x362` below 500: speed at most 251, the line at most
  31 bytes early, within the slots' texts.
- **At 0x2000 the line has no end.** A window 0x201e pixels or wider makes the
  limit 0x2000 and the start 0x400 characters in. The room left, `0x3ff -
  start`, is then -1, which an unsigned `min` takes as unlimited: the message is
  copied over the window pointer, the queue and on past `.data` into
  retail's import table. A first harness run overwrote `CharNextA`'s IAT
  entry, and retail faulted in *later* scenarios. Not reproduced: what lies past
  the image cannot be. No screen is that wide; a situation file with an
  oversized rectangle could ask for it. The harness keeps widths below 0x201e,
  and fails if either module's memory outside its image changes.
- **The breaker reads past its copy.** A word ending at the end of the text
  leaves the position on the terminator; the next step starts one past it and
  scans whatever the heap has there. If it is not zero, the "text" goes on into
  the next block until a space or zero, and further messages are made from it.
  The harness pads each block with the scenario's byte (zero, printable or
  random) so this happens both ways.
- **The breaker's cut is not reset per line.** When a line's first word is
  wider than the limit, it is cut where the previous line was. With no
  previous line the cut is at 0: an empty message is queued, the word's first
  character is lost under the terminator, and the next line starts one on --
  again and again, until what is left of the word fits.
- **A line's time is `cut * time / len` in 32 bits**, so a large `time`
  wraps.
- **The breaker's queue header leaks**: its nodes are spliced into the queue
  and its 8-byte header and two sentinels are never freed. (An empty text in a
  window under 30 pixels gives it nothing to splice; the splice's writes then
  cancel out.)
- **The fill byte is pushed as `dl`** over whatever edx held. The host's
  memset keeps only the low byte (`and eax, 0xff` at `0x40cf28`).
- **Message 0x37 frees the record's title through the record**
  (`free(*rec)`), then WINDOW frees it again (already null). After it the
  record pointer is null and any frame or show faults; in the game 0x37 is
  the end.
- **`+0x1c` returns the queue and `+0x20` the head sentinel**, by what was
  last in eax. The list 0x14 callback returns `data` for a record other than
  0x3df and `message - 0x37` for messages it ignores; CHAIN discards both.

### Dead code omitted

None.

## Differential harness

`tools/difftest/winmsgdiff.c`. Both DLLs are loaded into one process and run
in turn over the same script, driven through Linkage and the two CHAIN
callbacks.

- **host:** a bump arena reset before each side, unzeroed blocks filled with
  0xa5, freed ones with 0xdd, the slack after each with the scenario's
  padding byte. A free or realloc of anything but a live block is a heap
  fault. The window model gives the record its width when it is made (the
  rectangle scaled to one of six screen widths), a surface (`+0x94` bytes at
  `+0x98`, sometimes none) and, now and then on `+0x7c`, a new width as a
  resize would: anything from nothing to just under 0x201e. Every slot not
  modelled traps with its number.
- **WINDOW** `+0x78`/`+0x7c`: a zeroed record titled from its string id, and
  its release.
- **MAIN** `+0x5c`..`+0x64`: logged with the bytes.
- **CHAIN:** keeps both callbacks; on 0x50 it calls WINMSG's own list 0x14
  callback, notes whether the node is the one `+0x18` last returned, and one
  time in six calls back into WINMSG from inside the dispatch (a show, an
  append, a clear), as a listener moving on would.
- **GLOBAL:** `+0x1254` 0, 1 or another non-zero (the code tests both
  `!= 0` and `== 1`) and `+0x362` 0..59, or one time in eight up to 499;
  both move between ops.

The operations: messages in every slot (words, long words, runs of spaces,
leading spaces, high-bit characters, empty, longer than the line), plain and
appended, with times from 0 through ATC's, LANDME's and DEMO's to ones whose
shares overflow; runs of frames, up to 450; slots cleared, all cleared,
queried; resizes; GLOBAL moved; situation records in (0x3df and not), out,
other messages, and the close. Two ops are aimed from each side's own state: a
text exactly as wide as the limit allows (±1 pixel), and an append on the
first scroller at the moment its tail arrives (±1).

Compared after every operation: the transcript of every import call with the
strings and bytes it carries and each service's return, WINMSG's whole
`.data` image (code pointers through a learnt pairing), the arena and GLOBAL.
A fault ends the scenario on both sides.

Result: **20,000 scenarios, 0 differences**, about two minutes. The run prints
its coverage: frames plain and scrolling, lines drawn from before the buffer,
done messages and those that were the last shown, re-entries, texts wider
than the window, every append branch (the exact edge included), narrow and
near-0x2000 windows, restores, closes, and where the faults were.

The 2,577 faults are the cases above: after a close (the null record, at
`.text+0xb1c`, `+0x82b`, `+0xe78`, `+0x505`, `+0x79a` and others), heap faults
(956, freeing a slot pointer a line overwrote), and string calls through such
a pointer outside the module.

Deliberate breaks: `tools/winmsg_breaks.py` applies each of 64 one-site
mutations to a copy of `winmsg.c`, builds it as the harness's "ours" and runs
2,000 scenarios. **60 are caught; 4 are proven equivalences**:

- `insert-guard`, dropping the insert's refusal when the queue is empty and
  the position is not the head. No caller can ask for it: every position is
  the tail's predecessor (the head, when empty), a node still queued (so not
  empty), or the predecessor of the node just erased (the head, if that was
  the only one).
- `splice-order`, swapping the splice's first two writes: they are `prev` of
  the node after the position and `next` of the sub-queue's last node,
  different fields of different nodes even when the sub-queue is empty.
- `charnext`, `p + 1` for `CharNextA`: the same in a single-byte code page,
  which is the harness's (see Portability).
- `blank-fill`, the constant 0x0b for the fill byte at `+0x74`. Only a line
  starting 36 bytes early reaches it: a start of -288, so a speed of 289, a
  frame rate of 574 -- past the harness's 499 and any game's. A narrow window
  starts the line at most 3 bytes early.

## Portability

`CharNextA` is the one platform call. In a single-byte code page it steps one
byte unless at the terminator, which is what a native build should do; in a
multibyte one it steps over a lead byte's pair, and a native build would need
the same table to scroll such text as retail does.
