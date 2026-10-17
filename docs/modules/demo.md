# DEMO.DLL — flight videos and instant replay

DEMO (module id 7) records and plays FS98's flight videos (`.VID`), runs
instant replay, and supplies the callbacks for the four dialogs around them.
It is reimplemented and verified: `make demodiff-run` drives retail and
OpenFS98 through 20,000 whole-module scenarios over the CD's six videos and
synthetic ones with zero differences, after all its deliberate breaks were caught (one is a proven
equivalence). It runs in-game with `make smoke` passing.

## Shape and linkage

16,864 bytes of `.text` in two programs. Hand-written assembly runs from
`.text+0x20` to `+0x353e`, with ATC's stack-alignment wrapper around every
call. MSVC C runs from `+0x35e0`: the dialogs' callbacks, the catalogue
hooks and the sound cue. There is one 0x1165-byte `.data` image (0x1000 on
disk): `ImportTable` at `+0`, `Linkage` at `+0x50`, then all of the state.
Its 98 relocations are code pointers and a few pointers into `.data`;
`tools/gen_demo_data.py` zeroes them and `demo.c` installs its own.

The assembly passes values in registers and returns status in the carry
flag. It is transliterated with six register variables and a carry, as FE's
engine was. The parts of the image the code reaches through are:

| `.data` | |
| --- | --- |
| `+0x610`..`+0x634` | the reader: open, close, rewind, byte, peek, word, dword, skip 1, skip n, sync. One set reads a file through a 0x200-byte buffer, the other memory (esi, to `+0x2bc`). The image starts with the memory set and the file open |
| `+0x63c` | 25 handlers for command tags 0..0x18f, by tag >> 4, the value in the low nibble |
| `+0x6b4` | 23 handlers for word commands 0x2001..0x2017 |
| `+0x710` | `+0x24`'s twenty events, which write tags |
| `+0x760`..`+0x7b0` | the command tables those index |
| `+0x7c0`, `+0x7c5` | the four sample rates: frame tag and ticks between frames |
| `+0x7d9` | the delta-key table |
| `+0x800`..`+0xe60` | the instant replay ring: 51 entries of 0x20 bytes |

| slot | retail | what |
| --- | --- | --- |
| `+0x04` | `.text+0x020` | init: CHAIN lists 1, 2, 3, 9, 0xd, 0x14; clear the ring |
| `+0x08` | `.text+0x122` | deinit: discard any recording, stop playing, unhook |
| `+0x18` | `.text+0x1537` | per frame while playing: GLOBAL `+0x55c` between the keys |
| `+0x1c` | `.text+0x1b78` | the interpolation alone, into any block |
| `+0x20` | `.text+0x1da8` | one frame of a recording in memory (ADVDRV) |
| `+0x24` | `.text+0x2927` | record a pilot event `(which, value)` (CONTROLS) |
| `+0x28` | `.text+0x138e` | play a video `(name, dialog at the end)` (ADVDRV, the list) |
| `+0x2c` | `.text+0x3860` | the video list dialog (0x1404) |
| `+0x30` | `.text+0x35e0` | the instant replay dialog (0x1403) |
| `+0x34` | `.text+0x1102` | stop recording at the next tick |
| `+0x38` | `.text+0x39b0` | the catalogue: VIDEOS (0x9007) then PILOTS (0x9002), through UTIL `+0x98` |
| `+0x3c` | `.text+0x353f` | a ring entry back, as a position above the ground |

New host prototypes: `+0x170` (path, ext, &length) is a full path's length,
and `+0x178` (name) is nonzero for a string that cannot be a file name.

## The format

A video is tagged records. A 16-bit tag is stored high byte first:

| tag | |
| --- | --- |
| `0x0000`..`0x018f` | a command, its value in the low nibble: through the tables to CONTROLS `+0x44`, or straight into GLOBAL (lights, smoke, the strobe) |
| `0x2001`..`0x2017` | a command with a word value |
| `0x4001`, `0x4002` | title (0x1f bytes at file offset 4) and description (0x100 at 0x27); the list reads them in place |
| `0x4003`, `0x4004` | the airspeed and vertical speed, which then are not derived |
| other `0x2xxx`..`0x5fff` | a word length and data, skipped |
| `0x6001` | a situation, a dword length: MAIN `+0x4c` loads it from the file (`+0x48` from memory), four GLOBAL flags held at 1 meanwhile |
| other `0x6xxx`.. | a dword length, skipped |

A single byte at or above 0x80 is something else:

| byte | |
| --- | --- |
| `0x80`, `0x81`, `0x88` | a frame tag: 1, 5 or 15 seconds |
| `0x90`..`0x9b` | a frame tag and the key that follows it (1 short, 2 delta, 3 long); 0x9a and 0x9b are half and quarter seconds |
| `0x82`, `0x83`, `0x86` | a short key (three dwords, three angle words), delta key (six bytes) or long key (three 48-bit positions, three dwords, a sequence byte) |
| `0x84` | a message, up to 0x37 bytes |
| `0x8a`..`0x8e` | odds and ends: 0x8b makes the next key replace the older key too, 0x8d and 0x8e take a key from the output or from GLOBAL |
| `0x89` | the end; past the end of the file every byte reads 0x89 |

Each frame tag's interval is scaled by GLOBAL `+0x634`, the replay speed in
percent, and added to the newer key's time. The clock is GLOBAL `+0x2aa`
ticks, times 1,000, shifted down 15. Pauses (lists 9 and 0xd) are taken out
of the keys. While applying (GLOBAL `+0x74c`) the airspeed is derived from
the key-to-key distance, averaged over five frames, along with the vertical
speed and pitch rate. The distance is three Newton steps of a square root.

A message `"%%s:name"` plays `name.WAV` once instead, through the host's voice
engine at SOUND's volume 4.

Recording samples GLOBAL on list 2. A frame record goes out at the rate
GLOBAL `+0x75c` picks, and every 18 ticks the controls that changed (masked
to 0xffe0 where a control jitters). The header goes into a temporary file in
PILOTS. On stopping, the finished dialog (0x14040001) reviews, discards,
saves (renaming over any file of that name) or goes on.

## Instant replay

List 3 copies GLOBAL's position block into the ring every frame unless the
sim is busy: recording, replaying, or `+0x268`, `+0x365`, `+0x260` under 2.
An end tag follows the newest entry. GLOBAL `+0x630` is how many entries
there are, up to 50. The dialog sets the length, speed and loop. Each tick
then parses ring entries as memory recordings (each a 0x98 frame) until the
interpolation catches up. Command 0x10001 ends it and puts the aircraft back
where it started.

## Retail behaviour worth knowing

All reproduced.

- **A flush loses edx.** The flush (`.text+0x117e`) keeps eax and ecx and
  nothing else. When a word event (`0x2006` and the rest) fills the buffer
  with its tag, the value is written from the dx the host's write left, not
  the event's. The harness's write leaves a known edx, and the recordings
  differ until ours takes it back through a thunk.
- **The 0x60 handler reads past its table.** It indexes the five values at
  `.data+0x6a0` by the record's nibble. Nibbles 5..15 read the next table,
  `+0x6b4`, which is code pointers, so retail sends CONTROLS command 0x1009b
  with one of its own addresses as the value. Ours sends one of its own. It
  cannot be byte-identical; the harness logs such values through its code
  pairing.
- **The 0x40 handler sends its second command with the ecx CONTROLS left.**
  It saves eax round the first call, not ecx. Ours takes ecx back through a
  thunk.
- **+0x24 reads `+0x11a0` of the caller's edi.** Events 10, 12 and 17 test it
  and never set edi, so it is the caller's (CONTROLS'). The entry stub takes
  edi in.
- **The reader table is shared.** Instant replay and `+0x20` switch it to
  memory, and nothing switches it back until the next `+0x28`. A video
  playing meanwhile then reads memory: a looping one rewinds to nothing and
  spins for ever, and deinit leaves the file open (its close is the memory
  one). The harness keeps the three apart, as the sim does.
- **Instant replay can start during a review, and then hang the sim.** The
  replay dialog refuses while GLOBAL `+0x758` is 1 (playing), not 2
  (reviewing a recording just made). Once started, the replay swaps the
  memory readers in. If the loop flag `+0x750` is still set from an earlier
  video (playing keeps it), `+0x18` then rewinds the memory reader to nothing
  and loops for ever. Found by the harness's watchdog; the harness now keeps
  the replay dialog to when nothing plays.
- **The message reader counts across the host's read.** The refill
  (`.text+0x197a`) keeps only eax, and the 0x84 reader counts its 0x37 bytes
  in ecx. A refill in the middle of a message hands the count the ecx the
  host's read left, and a large one writes past the 0x38-byte buffer into
  `.data+0x530`. Ours takes ecx (and edx) back from the read.
- **A review recurses on an unknown frame tag.** Stopping a review runs the
  review to its end, and an unknown frame tag stops playback, so a recording
  with a wild tag recurses until the stack runs out. The view event (10)
  writes tag 0x80 + its value, so a view number past 3 can make one.
- **A replay longer than the ring runs into the fill.** The dialog's check
  refuses a length over `+0x630`. A replay started past it would parse the
  ring's 0xffff fill and spin, alerting, for ever.
- **Faults.** A zero frame interval divides by zero deriving the speeds, and
  so does a vertical speed that overflows; ours takes the same #DE with a
  real `div`. A misparsed dword length makes the file reader skip gigabytes
  0x200 bytes at a time, and a record that will not parse makes it skip a
  second at a time to the clock. Both are slow, not wrong.

### Dead code omitted

The message routine's `ecx == 0x38` test, which a `repnz scasb` that always
counts one can never pass, is kept but marked. The `mov bx,1` before each
`mov bx,0` in the sampler is left out.

## Differential harness

`tools/difftest/demodiff.c`, built on boosterdiff's pattern. The sides run in
turn over VIDEOS\ and PILOTS\ trees rebuilt the same way for each. The host
stub passes files and listings through, with transliterated path, find and
file services and the extension table from `FLTSIM98.EXE`. Its dialog engine
is a stand-in for the four dialogs: it fills each block from the scenario's
script, calls DEMO's checks, and closes only as the real dialog would. UTIL's
catalogue is retail's; its alert is logged and answered. CONTROLS, MAIN,
NUMERIC, SOUND and WINMSG are logged, and CONTROLS and the host's write
leave registers of their own.

Fixtures are the CD's six videos (`Videos\FS98.VID` and the five lessons),
some with bits flipped, and synthetic videos with every record class. Faults
in either DEMO are outcomes: the op is abandoned and the next one runs. Ops
that run past 4,096 reads or 2,048 alerts are cut at that count on both
sides. A watchdog reports any spin with no host calls in it.

Compared after every operation: the transcript of every import call, DEMO's
whole image (code pointers through a pairing learnt in order of first sight,
pointers into the image as offsets), the arena and GLOBAL (its replay
pointers as offsets). At each scenario's end the trees are compared, with
their contents.

Result: **20,000 scenarios, 0 differences** (about 25 minutes). The run
made 74,471 plays, 265,715 frame ops (1,516,069 frames), 59,820 recordings
(4,487 saved), 97,181 sample ops with 46,803 `+0x24` events, 22,356 replay
dialogs, 36,646 ring positions, 18,429 memory-recording ops and 41,036
interpolations. Inside them: 368,394 CONTROLS commands, 20,464 situations
loaded, 27,369 messages shown and 1,835 sound cues. The dialog stand-in ran
89,457 list, 20,066 replay, 17,304 finished and 1,791 message dialogs. Both
sides took 1,143 faults alike. 50,461 ops were cut at the fixed count, and
retail left a file open 172 times.

Deliberate breaks, each built into `DEMO_ours.dll` and run over 300
scenarios (1,000 for the three register reproductions): **26 tried, 25
caught, and one proven an equivalence.**

The caught ones were: the interpolation's end test; the 48-bit multiply's
shift; the 0x4003 record's second store; the five-frame speed average; the
0x9a interval; the flaps mask; the 18-tick control sampler; the message cap;
the replay's loop count; the ring's cap; the ground height's shift; the rate
step's floor; the name-length limit; the title tag; the delete answer bit;
the lights handler's range; the flush's edx; the refill's ecx; CONTROLS'
ecx; the loop flag at the end; the past-the-end pad byte; the sequence byte;
the clock's scale; the fifth mode's tag; and the insert's chunk size.

The equivalence is the delta key's small-value threshold, `<= 0x10` against
`<= 0x0f`. At exactly 0x10 the table path gives 0x100, which is 16 << 4, and
at −0x10 the branch does not change, so no input can tell them apart.

Four of the 25 went uncaught at first, each a fixture gap: a path exactly
0x104 long, a word event whose tag fills the write buffer, the fifth mode's
value, and (before the steering) the edx one. The generator now aims at each:
path lengths 0x103..0x105, events while the buffer's fill (read from DEMO's
own `.data+0x4cc`) is past 0x1f0, and mode values 0..5.

## Portability

Pure logic over host files and GLOBAL, apart from the x87 multiply in the
48-bit interpolation (`.text+0x2fc2`). That one needs the same treatment as
UTIL's, and the precision control matters: the products reach 2^47, so under
24-bit precision they round.
