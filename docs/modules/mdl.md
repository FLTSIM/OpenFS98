# Aircraft MDLs


> **Decompiled (2026-11-08).** All eleven models are hand-written C: the eight normal ones in
> `src/mdl/mdl_normal.c`, the three crash ones in `src/mdl/mdl_crash.c` (see
> [Crash models](#crash-models)). The crash models were first lifted (C generated from retail's
> instructions, 2026-11-06); that build is kept only as a second oracle.

All eleven MDLs are built as drop-in DLLs, 2026-11-06; all eleven are decompiled C
since 2026-11-08.
`python3 tools/mdl_survey.py` emits the measured boundaries and
SHA-256 fingerprints as JSON, using the existing `impscan.py` PE reader.

## Reference set

All eleven installed files were compared byte for byte with their counterparts
under `cd/fs98`, selected through `docs/setup_map.tsv`. All matched. Verified
copies now live under `orig/Aircraft/`, preserving the installed paths. These
are ignored retail artifacts, not repository content.

The earlier nine-file count missed both `C182/MODEL.RG` models. There are eight
normal models and three crash models, with **35,074 bytes of `.text`** total.
Section sizes below are PE virtual sizes, not file-aligned sizes or measured
reachable instruction counts; CRT startup code is included.

| Installed basename | Image base | `.text` | `.data` | State bytes | Update RVA |
| --- | --- | ---: | ---: | ---: | --- |
| BOEING737-400_N.MDL | 0x22100000 | 982 | 47,800 | 192 | 0x1037 |
| BELL_206B_JETRANGER_N.MDL | 0x22100000 | 774 | 91,008 | 132 | 0x1037 |
| CESSNA182RRG_C.MDL | 0x22100000 | 9,110 | 29,904 | 600 | 0x1164 |
| CESSNA182RRG_N.MDL | 0x22110000 | 950 | 27,648 | 140 | 0x1037 |
| CESSNA182S_C.MDL | 0x22100000 | 9,350 | 80,424 | 600 | 0x1167 |
| CESSNA182S_N.MDL | 0x22120000 | 934 | 96,488 | 140 | 0x1037 |
| EXTRA300S_C.MDL | 0x22100000 | 9,894 | 69,496 | 744 | 0x1164 |
| EXTRA300S_N.MDL | 0x22120000 | 758 | 41,424 | 124 | 0x1037 |
| LEAR45_N.MDL | 0x22100000 | 918 | 87,952 | 140 | 0x1037 |
| SCHWEIZER2_32_N.MDL | 0x22100000 | 678 | 5,800 | 112 | 0x1037 |
| SOPWITHCAMEL_N.MDL | 0x22100000 | 726 | 18,816 | 120 | 0x1045 |

## Module boundary

All eleven export `ImportTable` and `Linkage`. Their module id is 31; their
import id sequence is 1 (HOST), 2 (GLOBAL), 4 (CHAIN), 14 (G3D), 20 (UTIL).
Their Windows imports are KERNEL32 and MSVCRT. Declaring a module import does
not establish that every model calls it.

| Linkage offset | Meaning |
| --- | --- |
| +0x00 | module id 31 |
| +0x04, +0x08 | same one-instruction `ret` routine |
| +0x0c, +0x10, +0x14 | zero |
| +0x18 | attach: writes the model header pointer to the caller's aircraft; `ret 4` |
| +0x1c | initialize model state; `ret 4` |
| +0x20 | detach; plain `ret` |

In every normal model, +0x1c is the same no-op at RVA 0x1020 and +0x20 is
the same register-saving no-op at RVA 0x1030. The attach thunk is also the
same instruction sequence apart from the header address. This is shared
scaffolding, not proof that their per-frame callbacks are interchangeable.

The model header lives at RVA 0x3010 for normal models and 0x5010 for crash
models. Its observed nine dwords are:

| Offset | Observed contents |
| --- | --- |
| +0x00 | 0x24, header size |
| +0x04 | model program pointer, used by ACONTAIN's drawing service |
| +0x08 | varying value; interpretation not established |
| +0x0c | varying value; interpretation not established |
| +0x10 | code pointer, update callback (RVA in table above) |
| +0x14 | zero |
| +0x18 | model state allocation size |
| +0x1c | `FSIM` |
| +0x20 | 0x1a2 |

`src/acontain/acontain.c:ac_model_load` checks size and signature, allocates
state, initializes colours, texture folder and GLOBAL +0x11b6, then calls
Linkage +0x1c. `ac_draw` passes header +0x04 and state to G3D. The
retail caller of header +0x10 is WINDOW at `0x20906a20..0x20906a49` (also
transliterated in `src/window/window.c`). Unless GLOBAL word +0xdce has bit 0
set, it checks the header, callback, GLOBAL +0x11dc and state pointers, pushes
the state pointer, calls the update, then calls ACONTAIN's drawing service.
Normal updates use `ret 4` and preserve ebx/esi/edi/ebp; WINDOW overwrites eax
before the next call. No caller-saved return value is consumed on that path.

## C182 code traced so far

Normal C182: Linkage +0x18 at RVA 0x1200 attaches header 0x3010. The header's
update callback at RVA 0x1037 copies GLOBAL +0x560 (0x24 bytes) to state +4,
GLOBAL +0x5b0 (0x18 bytes) to state +0x28 through HOST +0x21c, then updates
individual fields from GLOBAL, including control-surface values. It uses
16-bit arithmetic and a helper at RVA 0x11c1 that returns values in bx/cx.
Those widths and register results need literal treatment in a transliteration.

Crash C182: +0x1c at RVA 0x1020 has one-time initialization guarded by a
module flag at VA 0x22105034. It registers callback 0x2210120c with CHAIN
list 15, calls the flight model through GLOBAL +0x11d4, and initializes
state and GLOBAL fields. Detach at RVA 0x110d unregisters that callback,
clamps GLOBAL byte +0x838 to 5 when its signed value is at least 6, clears
GLOBAL +0x26c, and resets the flag. The header update at RVA 0x1167 runs
only when GLOBAL byte +0x838 is 6. The CHAIN callback chooses between two
G3D +0x18 programs according to state word +0x82. Import scanning also
finds UTIL +0x2c and +0x44 calls in the crash code.

The normal and crash behavior is verified by the separate harnesses below.

## Normal callback verification

`src/mdl/mdl_normal.c` implements all eight normal updates in C with explicit
little-endian accesses and arithmetic widths. The shared operations retain
the two HOST +0x21c calls. Model-specific writes include altitude-dependent
words inside the G3D programs: Boeing RVAs 0xea12/0xe682; C182RG
0x647e/0x6a08/0x6d68; Extra 0xac94/0xa5a2; Schweizer 0x3ec6; Sopwith
0x5daa/0x5dec. Fixed-gear C182 and Lear compute the altitude temporarily but
overwrite it without a store; that dead computation is omitted.

Boeing's update also retains a partial-register dependency: when it shifts
eax at RVA 0x10cf, the high half is still from GLOBAL +0xd48, and bit 16
enters the word written to state +0x6e. Treating it as a 16-bit shift changes
the output. No intent or defect classification is assigned to this behavior.

```
make mdl-normal-diff-run             # N=70000 per model by default
make mdl-normal-breaks
```

The standalone harness loads each retail MDL, binds HOST and GLOBAL, and
compares its header update with the C update. On 2026-11-06: **70,000 inputs
per model at preferred bases and under forced relocation, 1,120,000 updates
total, zero differences**. It compares the complete `.data` section, state
including surrounding canaries, unchanged GLOBAL, both copy calls, preserved
ebx/esi/edi/ebp, and stack balance. The first 65,536 inputs sweep the complete
16-bit range of each gear helper/control input; the remaining 4,464 use
full-width random values. Three of every four vectors carry forward state,
exercising the cumulative rotor/propeller updates. This is exhaustive for
each swept operand, not for combinations of all inputs.

Six isolated, freshly compiled mutations were caught with the expected
behavioral mismatch: wrong copy source, wrong G3D program word, losing
Boeing's eax high half, wrong jet split threshold, wrong C182 split
threshold, and wrong Sopwith string pointer. Mutation logs and executables
are under `bin/mdl-diff/breaks/`; tracked source is never modified by them.

`tools/gen_mdl_fixtures.py` extracts the eight `.data` payloads, fingerprints
and measured harness metadata into ignored `bin/mdl-diff/`. Payload pointers
are still retail preferred-base addresses; these are extraction artifacts,
not standalone relocated model images. For this callback slice the harness
clones loaded retail data, runs C against a non-executable image addressed by
retail RVA, and normalizes Sopwith's string pointer for comparison. It does
not execute copied retail instructions on the C side.

This first harness proves the normal update slice. The module-level checks
below additionally cover independently generated DLLs and their boundaries.

## Generated normal modules

`make mdl-normal-modules` builds all eight replacements under
`bin/mdl-diff/models/`, with the installed basenames. `src/mdl/mdl_module.c`
provides the shared boundary: bare-return init/deinit/detach, a state-init
`ret 4`, the attach thunk, and the C update dispatched by model kind. The
no-op register pushes/pops are omitted; return registers and stack balance
are retained and tested. DllMain retains DisableThreadLibraryCalls; retail
CRT allocation/environment bookkeeping is omitted because no model service
uses it. There is no CRT dependency in the rebuilt DLLs.

`tools/build_mdls.py` extracts model data, the Sopwith literal and every
resource payload. It emits a writable image addressed by retail RVA, with
the `.data` payload at its original offset and no extracted instructions.
All seven `.data` HIGHLOW relocations in each normal model are emitted as
linker relocations: one program pointer, one update callback and five Linkage
function pointers (init and deinit share a target). Unknown targets, pointer
fixups into the CRT tail and unexpected model sets stop generation. The
original version resource payloads and language ids are preserved.

```
make mdl-normal-modules-run          # N=70000 per model in each mode
make mdl-normal-module-breaks
```

The loaded-module harness runs **1,680,000 lifecycle/update scenarios with
zero differences**: 70,000 per model with retail loaded first, rebuilt loaded
first, and both forced away from preferred addresses. It asserts that these
address cases actually happened. Both sides bind HOST/GLOBAL independently;
the rebuilt side executes its own DLL callback. The attach thunk must write
only the aircraft header pointer. Every init/deinit/state-init/detach call
must preserve the observed registers, including eax/ecx/edx; updates retain
the callee-saved registers and stack. State and canaries, GLOBAL and copy
calls are compared as in the callback harness.

All model data, headers, imports and Linkage are compared after normalizing
the seven pointers. Data pointers must reference the exact expected rebuilt
image offset; code pointers must be in the rebuilt DLL's executable section,
with shared retail targets remaining shared. Lifecycle and update tests
exercise those code pointers, rather than assuming pointer normalization
proves equivalence. Version resources are compared by type/id/language,
size and every payload byte. Retail's private CRT tail after Linkage +0x24
differs from the CRT-free replacement; each side's tail is checked against
its own snapshot after every scenario and is not compared across sides.

Six fresh broken DLL builds were rejected for the expected reason: wrong
program relocation, changed program byte, update pointing at state-init,
state-init writing a byte, detach clobbering ecx, and changed version data.
Artifacts and logs are under `bin/mdl-diff/module-breaks/`.

`make mdl-normal-game` runs private game copies with otherwise-retail modules,
using `tools/mdl_frames.py` and `HG2D_FRAMES_MDLS=1`. It compares two retail
runs before comparing rebuilt MDLs, disables sound for deterministic crash
timing, and requires the intended normal model to load on all three sides.
The `models` script steps the active view backward and exercises throttle,
flaps, gear, roll, lights and rudder over 600 main-loop passes. The eight
flights are HONGKONG, BELLWTC, STGEORGE, PARIS, EXTRARIO, LEARLON, CHELAN and
WAIKIKI. Logs and summaries stay under `bin/mdl-frames/`.
The resulting view depends on each flight's starting mode; the exterior
model's picture sensitivity is directly demonstrated on STGEORGE below.
**All eight scripted flights passed on 2026-11-06**, 600 main-loop passes each:
the two retail baselines and rebuilt MDLs match frame for frame, and each
flight's intended normal model was confirmed loaded on all three sides.
`JOBS=1..4` selects concurrent independent games (default four), each with a
separate prefix and display; `MDL_GAME_DISPLAY=:76` selects the display base.

`make mdl-normal-render-break` rotates C182RG's rendered heading by 90 degrees
in an isolated DLL build. It is **caught at pass 40**, exactly when the script
switches to the exterior view; the two retail runs match, and all three games
complete 600 passes with clean exits. This is a picture difference, not a
fault counted as a successful break.

The rendering check exposed a frame-runner bug: relative script paths were
resolved after changing into the private game copy, silently skipping input.
`tools/hg2d_frames.sh` now resolves and checks scripts before changing directory.
The original eight un-scripted frame matches do not establish control/exterior
coverage; the accepted eight-flight run was repeated with this correction.
An earlier END
mutation removed required model setup and faulted, and is not counted as a
successful rendering check (`bin/mdl-render-break/invalid-end.result.txt`).

## Crash models

`src/mdl/mdl_crash.c` is the three crash models (CESSNA182RRG_C, CESSNA182S_C, EXTRA300S_C)
decompiled, one source built three times (`-DCRASH_MODEL=0/1/2`) by `tools/gen_mdl_crash.py`
into `bin/mdl-crash/models/`, over each model's generated data image (retail's `.data` at its
RVAs, the relocated code pointers bound to the C entries; 64-KiB aligned as before).
`make mdl-crash-modules` builds them.

**One program, three tables.** Retail's three are the same hand-written assembly (32 routines,
3,394 to 3,952 instructions) assembled with different data: the module variables (`0x5034` to
`0x50e4`) are shared; C182S keeps its flag and phase words at state `+0x80`/`+0x82` instead of
`+0x7e`/`+0x80`, copies GLOBAL `+0x11b8` into state `+0x7e` each frame, and seeds every debris
piece's attitude from the aircraft; the Extra throws 17 pieces to the Cessnas' 13, so its
variables after the per-piece sink rates sit 0x10 later (written `T(k)` in the C), and its piece
list differs. The code names retail's addresses by C182RG RVA, as the gauges do.

**What it does.** Linkage `+0x1c` (once until `+0x20`) registers the CHAIN list 15 draw, has the
flight model fill a snapshot (SIM `+0x18`), and clamps three speeds from it. Each frame while
GLOBAL byte `+0x838` is 6 the update copies the aircraft into the state and steps one of six
animations, picked by GLOBAL `+0x834` (6, 12: impacts) or the snapshot's pitch and bank: a level
slide that bounces to a stop, a wing torn off and a yaw to a stop (|bank| 10 to 30 degrees), a
cartwheel (over 30), a nose-in that goes over on its back (pitch 15 to 40), or a break-up (over
40, and both impacts) that throws the pieces, 0x24-byte position records from state `+0x84`.
Retail unrolls the debris pass piece by piece; the C has one routine and a table per model, in
retail's order. The crash ends with `+0x838` set to 4. The CHAIN callback draws the wreck program,
or the debris one once the phase word is 2.

**Registers.** The C's entries are stdcall; `src/mdl/mdl_crash_entry.S` adds what the callers
read beyond that, established from the callers: the update returns with edi = GLOBAL (retail
loads it; WINDOW reads edi right after, `0x20906a49`), and the CHAIN callback ends with the G3D
`+0x18` call entered with G3D's Linkage in eax and its own ecx/edx, so it returns with G3D's eax,
ecx and edx, which CHAIN hands on. Retail's update also leaves ebx and esi changed; WINDOW pops
both unread, and ours keeps them. ACONTAIN (compiled C) reloads eax/ecx/edx after attach, init
and detach. Omitted as without effect: retail's stack-alignment wrapper round every imported
call (a store below esp and a pushed 4 the callee never reads) and dead loads.

**Retail bugs kept** (all in [retail-bugs.md](../retail-bugs.md#crash-mdls)): the nose-in
pitch and the wing-loss heading turn by the high half of a leftover eax (in the nose-in's first
frame, UTIL's Linkage address, since UTIL `+0x44` preserves eax), piece `0x114` stops piece
`0xf0`'s tumbling instead of its own, the Extra skips two pieces while piece `0x210` flies and
sinks piece `0x234` for `0x2a0`, the slide's pitch comes from a byte (`cbw`), and the settle
compares altitudes in halves, unsigned, and drops a carry.

### Verification

`make mdl-crash-diff-run` (`tools/difftest/mdlcrashdiff.c`): retail and the decompiled DLLs
side by side over the same lifecycle scripts, 2,000 scenarios per model, retail first and both
forced away from their preferred bases: **12,000 scenarios, 0 differences** (2026-11-08).
Scenarios include initialization, CHAIN registration and dispatch, repeated update/draw, enabled
and disabled drawing, signed cleanup reasons, detach and reinitialization; 198 boundary pitch, bank,
speed and time-step cases precede random ones, whose attitudes are spread over the animations'
ranges (pitch for the slides, the nose-in and the dive; bank within 10 or 30 degrees) or are
anything, and half of whose speeds are within the clamps; the ground is none, near the aircraft, small and
signed, or anything, the aircraft starts at 100 m or near or below the ground, a fifth of them
turn the aircraft every frame (three turns in four onto or beside one of retail's pitch
thresholds), and some start with the wing-loss words after the piece rates as earlier crashes
leave them (init never resets them, so in the game they carry over from crash to crash). Compared: model data, state and canaries, GLOBAL, resources,
stack balance, each entry's live registers (above) and every import call (CHAIN, the snapshot,
HOST moves, G3D draws with a hash of the state, and UTIL `+0x2c`/`+0x44` with their arguments;
UTIL is retail's own, behind logging proxies).

`make mdl-crash-lifted-run` runs the lifted oracle (`bin/mdl-crash/lifted/`, all registers,
`tools/mdl_crash_lift_breaks.py` its own eight breaks) over the same scenarios: 0 differences,
**32 of 32 retail routines reached**, which is the scenarios' coverage of retail.

`python3 tools/mdl_crash_breaks.py` (`make mdl-crash-breaks`; run from a frozen copy) breaks the
decompiled C on purpose, 82 single edits (every helper, each animation's thresholds and signs,
the debris tables, the retail bugs fixed one at a time, the register outputs in the entry
stubs), each built for the models it applies to and run over 2,000 relocated scenarios: **78
caught, 4 proven equivalent** (listed in the script; 2026-11-08). Three of them were caught only
after the scenarios were widened: the ground had always been 0, the aircraft never turned between
frames, and init does not reset two words that carry over from crash to crash.

In the game, `make mdl-crash-game` (ANTILLES, PARIS and EXTRARIO, 1,200 passes each, otherwise
retail modules, the intended crash model loaded on all three sides): **identical frames on all
three with the decompiled models**, 2026-11-08. `make mdl-crash-render-break` turns the
decompiled C182RG's crash pose a quarter turn and requires the frames to differ.

The private flight fixture enables crash detection without the modal analysis
graph. EXTRARIO additionally starts at 100 metres and removes its saved
non-cockpit view. REALITY only enables animated crashes when the currently
painted window is a 3D window with view kind zero; changing the active window
alone did not suffice. `tools/mdl_crash_fixture.py` validates the situation records before
writing; the source situations and primary game remain untouched.

**Limits:** longer flights and Direct3D model acceptance remain unverified. The G3D programs
stay retail bytecode in the data image, for the existing interpreter. The 48-bit positions are
explicit 16/32-bit arithmetic and the imports are called through their stdcall pointers, so the
C itself is portable; the entry stubs' register outputs are x86 conventions only retail asm
callers need.

## Install and restore

`make` builds all eleven MDLs alongside modules and gauges. `make install`
installs them into `FS98_DIR`, and `make revert` restores all eleven retail
files from the verified `orig/Aircraft/` references. `make run` launches that
same game tree, so aircraft selection exercises the installed replacements.

```sh
make install
make run
# Restore modules, gauges and aircraft MDLs:
make revert
```

`make mdls-install` and `make mdls-revert` operate on just the eleven MDLs;
`make mdls` builds them without the differential test executables. Set
`FS98_DIR=/path/to/FS98` consistently for installation, running and restoration
when using a tree other than the default `~/.wine/drive_c/FS98`.

`tools/stage_mdls.py --all` validates both complete families and every input
before copying, then replaces each file through a temporary sibling and verifies
the resulting bytes. Its default single-family mode still restores the other
family to retail for isolated frame tests. A disposable full-game install/revert
roundtrip checks all eleven MDLs, all 120 gauges and the shared module set.
