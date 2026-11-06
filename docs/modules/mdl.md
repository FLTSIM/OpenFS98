# Aircraft MDLs

All eleven MDLs are built as drop-in DLLs, 2026-11-06. Normal callbacks and
crash lifecycle, simulation and drawing operations are implemented in C.
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

`make mdl-crash-modules` builds CESSNA182RRG_C, CESSNA182S_C and EXTRA300S_C
under `bin/mdl-crash/models/`. `tools/gen_mdl_crash.py` follows each model's
six exported/callback roots and direct helpers, emitting 32 C functions per
model. Generated control flow uses explicit operations and labels, including
backward jumps into shared tails. There is no runtime instruction decoder.
Objdump supplies boundaries; PE relocations identify address operands, and
unsupported operations, native signatures and flag dependencies fail generation.
Generated source and extracted retail data/resources stay in ignored `bin/`.

`src/mdl/mdl_crash_lift.h` supplies width-specific integer operations and x86
flags. `mdl_crash_native.S` bridges the models' nonstandard register contract
to native services while preserving input and output registers. A heap shadow
retains numeric caller-stack addresses, including partial-register pointer
operations. Native service argument masks distinguish pointers from packed
scalar angles; translating angles by their numeric address range was a defect
found during differential testing. The private data section retains its low
16 address bits through 64 KiB alignment. This remains an i386 implementation.

`make mdl-crash-diff-run` passes **12,000 complete lifecycle scenarios**:
2,000 per model in retail-first and forced-relocation modes. Every model
reaches all 32 generated functions. Scenarios include initialization, CHAIN
registration/dispatch, repeated update/draw, enabled/disabled drawing, signed
cleanup reasons, detach and reinitialization. Boundary pitch, bank, velocity
and delta cases precede random inputs. Comparisons include active model data,
state and canaries, GLOBAL, resources, stack balance, all general registers
and typed import events. UTIL calls use the real retail implementation behind
register-preserving logging proxies; other services use matched controlled
stubs. Function coverage establishes reachability, not exhaustive path coverage.

`make mdl-crash-breaks` rejects **eight isolated behavioral defects**: wrong
shared-tail entry, scalar-angle pointer translation, signed multiplication,
subtract carry, skipped initialization, unsigned cleanup reason, wrong drawing
phase, and a bank boundary. Each must produce its expected mismatch.
`make mdl-crash-render-break` also catches a valid quarter-turn crash-pose
mutation at pass 199 in real frames. Both retail baselines match and all
three games complete 1,200 passes cleanly; a fault is not accepted as proof.

`make mdl-crash-game` runs ANTILLES (C182RG), PARIS (C182S) and EXTRARIO
(Extra) in independent private games, using otherwise-retail modules and normal
MDLs. It requires the intended crash MDL to load on all three sides, matching
retail baselines, matching rebuilt frames, clean exits and the exact final pass.
Each run lasts 1,200 passes; `JOBS=1..3` and `MDL_GAME_DISPLAY=:86` control
concurrency and displays. Logs stay under `bin/mdl-crash-frames/`.
**All three flights passed on 2026-11-06**, with the intended crash MDL loaded
on every side, identical frames, clean exits and all 1,200 requested passes.

The private flight fixture enables crash detection without the modal analysis
graph. EXTRARIO additionally starts at 100 metres and removes its saved
non-cockpit view. REALITY only enables animated crashes when the currently
painted window is a 3D window with view kind zero; changing the active window
alone did not suffice. Earlier Extra runs that never loaded its crash MDL
and the PARIS run stopped by the graph are excluded from acceptance.
`tools/mdl_crash_fixture.py` validates the situation records before writing;
the source situations and primary game remain untouched.

**Limits:** longer flights, Direct3D model acceptance and native pointer-width
adaptation remain unverified. G3D program data remains retail bytecode for the
existing interpreter; its detailed contents have not been separately surveyed.
The primary `game/` tree retains all eleven retail MDLs. These workbench targets
build and test private copies and do not change default installation.
