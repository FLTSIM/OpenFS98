# SIM1.sim — module id 25


> **Lifted, not decompiled (2026-11-07).** Everything but the six services and the state lifecycle (the AIR decoder, aircraft setup and the simulation callbacks) is machine-translated from retail's
> instructions into C that emulates the x86 registers and flags, generated into ignored `bin/` at
> build time. It matches retail but is not readable source and cannot leave x86. Decompiling it is
> listed in [remaining-work.md](../remaining-work.md#lifted-not-decompiled), and under way: see
> [Decompilation](#decompilation-started-2026-11-07) at the end of this document.

The flight model named by the stock aircraft's `.AIR` files. ACONTAIN reads
chunk `0x100`, loads `Sim\<name>.SIM` through host `+0x26c`, and publishes its
Linkage at GLOBAL `+0x11d4`. This is simulator code outside `Modules\`, not
`English/Sim.DLL` (the aircraft-settings UI).

**Installed, 2026-11-06:** AIR decoding, aircraft setup and the tick/frame
callbacks now join the six services and state lifecycle. The production build
has the two ordinary module exports and the original version resource.
`make sim1` builds `bin/SIM1.sim`; shared build/install/revert include it.
The earlier partial harnesses below remain useful isolated tests. Complete
build verification and installation are described at the end of this document.

## Reference and shape

`tools/gen_sim1_data.py` checks the extracted CD image before copying it to
ignored `orig/Sim/SIM1.sim`. It refuses to overwrite a different original.
The CD and installed game copies both have SHA-256
`66ebc70ff63fd6beb51a146b9fa2d1ce5d92a5cdff99af92960eed9e2ab68c1b`.

Preferred image base is `0x22000000`. `.text` has 78,216 bytes, `.rdata`
1,881, and `.data` **17,400 virtual bytes but only 4,608 file-backed bytes**.
The rest of `.data` is zero-filled by the loader. ImportTable is at RVA
`0x16000`, Linkage at `0x16058`; the table has fifteen DWORDs including its
id. The two entries at `+0x20/+0x24` point to data, not code.

Imports, in order: HOST (1), GLOBAL (2), ACONTAIN (23), CHAIN (4), CONTROLS
(16), MAIN (3), REALITY (32), SOUND (39), UTIL (20), WINDOW (10). The static
PE imports are only KERNEL32 and MSVCRT. Options is not a dependency.

All addresses below are preferred retail VAs. Function-size gaps printed by
`dumplinkage.py` include intervening internal helpers, so they are not service
body sizes.

| Linkage | Address | What is known | State |
| --- | --- | --- | --- |
| `+0x00` | `25` | module identity | reproduced |
| `+0x04` | `0x22001060` | init: restore state, register CHAIN callbacks, load aircraft data, publish descriptor | caller verified; decoder modeled |
| `+0x08` | `0x220010a0` | deinit: remove callbacks, retain state and restore engine flags | implemented and compared |
| `+0x0c/+0x10/+0x14` | zero | reserved; no dialogs | reproduced |
| `+0x18` | `0x2200ac0d` | copy 44-byte descriptor to one output pointer; `ret 4` | implemented and compared |
| `+0x1c` | `0x2200fc40` | aircraft/simulation setup; full contract unrecovered | pending |
| `+0x20` | `0x22017150` | data pointer | layout only |
| `+0x24` | `0x22017158` | data pointer | layout only |
| `+0x28` | `0x2200ac2f` | copy one word to an output pointer; `ret 4` | implemented and compared |
| `+0x2c` | `0x220117c0` | signed integer scaled by `1/32768` to an output double, return zero; `ret 4` | implemented and compared |
| `+0x30` | `0x220117e0` | three ordered double outputs, return zero; `ret 12` | implemented and compared |
| `+0x34` | `0x22011750` | scale unsigned DWORD and clear five doubles; `ret 4` | implemented and compared |
| `+0x38` | `0x220096b8` | return a double through ST0; `ret` | implemented and compared |

## The first six services

Offsets here are from retail `.data`, `0x22016000`. `src/sim1/sim1.c`
keeps a matching `0x43f8`-byte image; the two published tables are aliases
into it. Data after the tables is extracted into ignored build output, not
copied by hand into tracked source. Pointers in unused extracted state remain
reference VAs. This is deliberately not a complete relocated SIM1 image;
unimplemented executable slots raise an exception instead of succeeding.

- **`+0x18`:** forward byte copy from `+0x1730`, length `0x2c`. Source and
  destination may overlap, so replacing `rep movsb` with `memcpy` is not a
  faithful description. All seven general registers survive unchanged.
- **`+0x28`:** load the word at `+0x1518` into AX, then store that word.
  EAX's high half survives; the other general registers survive. It is not
  a zero-extended integer return.
- **`+0x2c`:** signed DWORD at `+0x17a4`, x87 multiply by the readonly
  `1/32768` constant, then an output-double store. EAX is zero and ECX is
  the output pointer; EDX and the nonvolatile registers survive.
- **`+0x30`:** load/store `+0x13c8`, then `+0x13b8`, then `+0x13c0`.
  Each store happens before the following load; outputs can alias each other
  or the later source fields. The x87 also quiets signalling NaNs. EAX is
  zero, ECX is output two, EDX is output three. MULTIP calls these last two
  services in its control-update callback.
- **`+0x34`:** zero-extend the argument into a signed qword for `fild`,
  multiply by the mutable double at `+0x740`, and store at `+0x13b0`.
  Clear doubles at `+0x13a0`, `+0x13a8`, `+0x13e8`, `+0x13f0`, `+0x13f8`.
  EAX contains the input argument; ECX/EDX survive. The original sequence of
  x87 loads, exchanges and stores is retained, including its stack depth.
- **`+0x38`:** load the double at `+0x640` into ST0, leaving general
  registers unchanged. SOUND uses it for the airspeed-dependent wind sound.

`sim1_stack.S` implements the register boundaries around C helpers. This is
32-bit x86 work; no non-x86 equivalence is claimed.

## Differential verification

```sh
make -f tools/sim1.mk sim1-partial
make -f tools/sim1.mk sim1diff-run N=20000
make -f tools/sim1.mk sim1-breaks
```

`tools/difftest/sim1diff.c` loads independently owned retail and rebuilt
modules. Both get the same seeded backing state on every call, restored from
their own snapshots. Merely copying the descriptor's startup zeros would
prove very little: its source lies in the virtual tail, and initialization
has not run here.

Compared after each call: all 17,252 bytes after the tables, the separately
preserved ImportTable/Linkage prefix, the entire output buffer including
guards, all seven integer registers and ESP. No module import is bound;
these six services must not call or dereference dependencies. x87 comparison
covers control/status/tag words and every live 80-bit stack value. Saved
instruction/data addresses and payloads in empty stack slots are excluded
because they identify different code or historical values, not results.

The run covers all twelve combinations of 24/53/64-bit precision and four
rounding modes, with masked exceptions. Input x87 stacks contain either zero
or two live values. Cases include unsigned conversion boundaries, arbitrary
source bytes, unaligned and overlapping outputs, signed zeros, subnormals,
infinities, quiet/signalling NaNs and random double encodings. Every 16-bit
source word is additionally swept in one FP mode.

At `N=20000`, each load arrangement runs 240,000 cases per service, plus
65,536 extra `+0x28` cases. Retail-first, rebuilt-first and forced relocation
all pass: **4,516,608 service comparisons, zero differences**. The harness
asserts which module occupies the preferred base and that all six callbacks
belong to their respective executable sections. Wine uses a private prefix
and lock under `bin/sim1-diff/`.

`tools/sim1_breaks.py` rebuilds defects in ignored directories and requires
actual comparison failures with the expected service and diagnostic. Source
anchors must match; build failures, loader failures and missing failures do
not count as caught defects. **All sixteen rebuilt mutations are caught**:
descriptor length/source, word width/EAX, signed scaling/scale/registers,
vector order/bit-copy/registers/stack cleanup, unsigned reset conversion,
reset clears/arithmetic, and wind source. Logs and the machine-readable summary
live under `bin/sim1-diff/breaks/`. The final positive run is recorded in
`bin/sim1-diff/acceptance.log`, the negative run in `mutations.log` beside it.

## Boundaries recovered after the initial services

Initialization is not a small no-op slice. At `0x22010cc0`, SIM1 restores a
retained `0xc0`-byte record through GLOBAL `+0x11d8`, or asks MAIN `+0x6c`
for situation record `0x38e` and falls back to `0x5a`. Init registers callbacks
on CHAIN lists 1, 5 and 20. List 20 handles situation load/save events
`0x22/0x23`. Deinit removes the callbacks and serializes retained state.

Both readers, the velocity transform, startup state restoration, save callback
and retained-state/shutdown paths are covered below. Init/file ownership and
publication were first compared with an explicit decoder fixture. The complete
build subsequently adds the real decoder and simulation; its acceptance is
separate from that earlier modeled boundary.

## Saved state and shutdown

`src/sim1/sim1_state.inc` adds the internal state paths without changing the
retail offsets. `sim1_state_stack.S` preserves explicit register contexts
around the C logic and seeds/captures imported EAX/ECX/EDX values. These
functions include observable compiled-C register leftovers; the import bridge
does not replace them with constants.

| Retail VA | Operation | Accepted boundary |
| --- | --- | --- |
| `0x220110a0` | serialize the `0xc0`-byte state record | complete writer |
| `0x22011290` | restore a modern record and recompute orientation/heading | extended `0xc0` record and non-`0xc0` fallback |
| `0x22010df0` | restore legacy record `0x5a` | fixed 64-byte body; advertised callback size ignored |
| `0x22002ac0` | transform body velocities | both coordinate conventions, intermediate stores and output aliasing |
| `0x22010cc0` | startup state restore | retained state, modern/legacy lookup, missing records and buffer ownership |
| `0x22011050` | begin MAIN record `0x38e`, then write its body | success and nonzero begin-error returns |
| `0x22010d50` | replace GLOBAL `+0x11d8` with retained state | null/existing allocation; repeated saves |
| `0x22010da0` | CHAIN list 20 callback | save, both load formats, unknown messages/tags |
| `0x220010a0` | deinit | all three removals, retained state and both flag gates |

The serialized layout, with offsets relative to `.data`:

| Record offset | Source |
| --- | --- |
| `+0x00/+0x08/+0x10` | `+0x1538/+0x1540/+0x1548` |
| `+0x18/+0x20/+0x28` | `+0x13e8/+0x13f0/+0x13f8` |
| `+0x30/+0x38/+0x40` | `+0x13a0/+0x13a8/+0x13b0` |
| `+0x48` | double at `+0x15a8` |
| `+0x50` | DWORD at `+0x1d18` |
| `+0x54` | GLOBAL `+0x366`, zero-extended to a DWORD |
| `+0x58/+0x60/+0x68` | `+0x1358/+0x1360/+0x1368` |
| `+0x70/+0x78/+0x80` | `+0x1328/+0x1330/+0x1338` |
| `+0x88/+0x90/+0x98` | `+0x1418/+0x1420/+0x1428` |
| `+0xa0/+0xa8/+0xb0` | `+0x13b8/+0x13c0/+0x13c8` |
| `+0xb8` | double at `+0x1760` |

The writer performs an x87 load/store for the first double only. All later
double-shaped fields are raw pairs of integer loads/stores: signalling NaNs
are quieted in the first field but preserved in those later fields. Both
DWORD halves of a pair are loaded before its stores, including when the
destination overlaps state. The reader likewise retains the original order
and uses MSVCRT `_ftol` directly for its three angle conversions. It restores
the cached sine/cosine values and six fixed-point words through the original
x87 sequence, then uses **real retail UTIL `+0x3c`** for its heading calculation.
That service is stateless and is shared by the two sides. The padding after
each cached fixed-point word is preserved, not cleared by a wider store.

The callback sets `.data+0x1778` on every load message, even for a record tag
it does not recognize. It writes only the low byte of the saved engine flag
back to GLOBAL. Saving instead writes that byte as a DWORD in the record.
MAIN `+0x5c(0x38e, 0xc0)` returning nonzero suppresses the body write;
otherwise `+0x60(body, 0xc0, NULL)` is called and its result survives.

Retaining state frees an existing GLOBAL `+0x11d8` allocation through host
`+0x214`, then calls `+0x20c(1, 0xc0, &slot, 1)` and fills all 192 bytes.
Deinit first removes the list 1, 5 and 20 callbacks, ignoring removal return
values. It then retains state, restores GLOBAL `+0x278` from `.data+0x1c80`
only when GLOBAL `+0x604 == 1`, and resets four WORD engine selectors to 3
only when GLOBAL `+0x609 == 1` and `.data+0x1778 == 0`.

### Legacy loading, fallback and startup restoration

Legacy record `0x5a` is 64 bytes. Its first three orientation values are
unsigned split DWORDs: high words at `+0/+2/+4`, low words at **`+0x32/+0x30/+0x34`**.
They are zero-extended to qwords for `fild`. The product of `.data+0x690` and
`+0x680` stays on the x87 stack across all three conversions; rounding that
scale to a double first would change behavior. The raw DWORDs are also stored
at `+0x1550/+0x1554/+0x1558` for later signed conversions to cached trigonometry.

The three rates instead use **signed** words at `+6/+0xa/+8`. Body velocities
combine high words at `+0xc/+0xe/+0x10` with low words at `+0x12/+0x14/+0x16`
and use signed DWORD conversion, multiplying first by `.data+0x720`, then
`+0x690`. The DWORD at `+0x3c` is unsigned and supplies `+0x15a8`; `+0x18`
supplies `+0x1d18`. The reader clears the extended state and GLOBAL's engine
flag, computes cached orientation, transforms velocity and recomputes the
orientation/heading helper. All of those stores and both trigonometry passes
are retained.

The constants are extracted as their original doubles. For example, retail
uses `9.5873e-05`, `1.52590219e-05` and `3.28083` in these conversions; replacing
them with more exact angular or unit-conversion constants is a behavior change.

For modern records, only a size **equal to `0xc0`** restores the extended
fields directly. Every other advertised size clears those fields and
reconstructs world velocities from the base fields. The reader still reads
through body `+0x54`; smaller sizes are tested with sufficient mapped backing,
not by assuming they bound its memory access. Values above `0xc0` also take
the fallback. A legacy callback does not consult its advertised body size.

`src/sim1/sim1_velocity.S` keeps the transform's x87 sequence. Selector
`.data+0x7c0 == 1` takes one convention; every other value takes the other.
With identity orientation, the results are `(x,y,z)` and `(y,-x,z)` respectively.
The function rewrites stack argument copies as intermediate doubles, so
retaining only higher-precision intermediate expressions is not equivalent.
Outputs are stored in order and may overlap each other or cached orientation.
Arguments are marshalled as raw DWORDs to preserve NaN encodings until the
actual x87 loads. This remains an x86 verification boundary for native FP work.

Startup restoration borrows a `0xc0`-byte buffer from GLOBAL `+0x11d8` when
present, without looking up records or freeing that buffer. Otherwise MAIN
`+0x6c` looks for `0x38e`, then `0x5a` only if the modern lookup failed.
Both present means modern wins; both missing means neither reader runs.
A buffer returned by MAIN is freed through host `+0x214` using the local
pointer slot. This helper is implemented independently; it is also exercised by
the init caller comparison described below.

### State verification

```sh
make -f tools/sim1.mk sim1state-run N=20000
make -f tools/sim1.mk sim1state-breaks
```

The state test DLL has additional helper exports only in
`tools/sim1_state_test.def`. The production partial DLL retains the two
ordinary module exports. The harness calls retail's known helper RVAs beside
the corresponding owned functions in the test DLL; deinit uses the actual
Linkage entry. All imports are checked through a reset host model, except
the real stateless UTIL angle service. Before each side, GLOBAL, the model
heap, import transcript and output buffers are reset to the same input.

Fifteen operations cover direct writing, saving through MAIN, retained state,
unknown/save callbacks, deinit, direct modern loading, modern-load callbacks,
two successive retained-state or deinit calls, legacy readers/callbacks,
modern fallback readers/callbacks, the velocity transform and startup restore.
Compares include the full
seeded state, import/Linkage prefix, GLOBAL, heap and input/output guards,
all integer register results/ESP, import order/arguments, written record and
the same meaningful x87 state as the first suite. Only the direct writer's
known image-relative output pointer and the transform's three proven output
pointer results are normalized; arbitrary numeric register values are not.

Legacy inputs include directed zero, carry-boundary, signed-limit and unsigned
limit values, plus unaligned and state-overlapping bodies. The transform runs
selector values `0/1/2`, distinct/overlapping/state-overlapping outputs, special
FP encodings and cancellation cases, with independent identity-orientation
oracles for both conventions. Modern fallback sizes include `0`, `0x54`,
`0x55`, `0x58`, `0xbf`, `0xc1`, `0xffffffff` and `0x80000000`. The harness
requires every convention, size and startup restore path to be exercised.

Teardown control inputs cover the **54-way Cartesian product** of saved-mode
flag `0/1/2`, engine flag `0/1/2`, loaded flag `0/1/2` and null/existing retained
allocation. A non-one engine-gate mutation initially escaped because a correlated
generator never placed engine flag 2 beside loaded flag 0. Independent gate
generation now catches it; the harness rejects runs missing any of the 54
combinations. The mutation suite includes width/order/NaN behavior, register
results, MAIN errors, host ownership, teardown gates, angle conversions and
heading calculations. Results are recorded in `bin/sim1-diff/state-acceptance.log`
and `state-mutations.log`; individual rebuilt defects and their diagnostics
are under `state-breaks/`.

At `N=20000`, all fifteen operations run 240,000 times per load arrangement.
Retail-first, rebuilt-first and forced relocation pass: **10,800,000 state
comparisons, zero differences**. The two repeated-lifetime operations each
invoke their helper twice per comparison. **All 41 rebuilt state mutations
are caught.** The original six-service suite was rerun on the extended
partial DLL: its 4,516,608 comparisons still match, and all sixteen original
mutations remain detected.

This remains isolated acceptance: malformed/null record bodies, allocation
failure, unmasked x87 exceptions and live-game owner flows
are not covered by this slice. Tick/frame callbacks remain explicit traps.
The init caller now registers those identities, but the partial build is
not suitable for running them in the game.

## AIR file wrapper, publication and init coordination

`src/sim1/sim1_air.inc` implements three caller-side boundaries:

| Retail VA | Operation | Acceptance |
| --- | --- | --- |
| `0x2200ae90` | query aircraft AIR path, open/size/allocate/read, invoke decoder, free/close | file ownership and decoder call boundary |
| `0x22010c40` | publish the 44-byte descriptor | real copy, x87 conversions and register results |
| `0x22001060` | restore state, register three callbacks, load AIR, publish | real restore/publication, modeled decoder |

**This caller-only suite substitutes the decoder at `0x2200af90`.** The
partial DLL traps if a successful full read reaches that boundary. A separate
`SIM1_AIR_TEST.DLL`, built with `SIM1_AIR_TEST`, exposes one decoder-fixture
pointer. The harness replaces only retail's direct decoder call at
`0x2200af48` in its own process memory, after checking the expected instruction
and target. Both callers then invoke the same explicit fixture, which checks
all input bytes and publishes controlled state changes. The original call is
restored afterward. No binary on disk or live game is patched. This proves
the callers' ownership, ordering and call arguments, not AIR decoding or a
complete initializer. The normal DLL has no fixture export or bypass.

The file wrapper asks ACONTAIN `+0x30` for key 2 with a `0x100`-byte path
buffer, opens extension `0x0f` in mode 0, obtains the size, allocates with
host `+0x20c(1, size, &buffer, 0)`, and reads the whole file. It invokes the
decoder only when both read status and byte count match; a successful
zero-byte read also invokes it. Read errors and short reads still free the
buffer and close the file. The decoder result is returned unchanged after
cleanup. Open errors return zero.

**RETAIL BUG (kept): SIM1-AIR-SIZE-HANDLE.** When open succeeds and size fails,
the wrapper returns zero without closing the file or freeing its host block.
The harness retains that open ownership in its model and catches an attempted
cleanup "fix" as a difference. See [retail-bugs.md](../retail-bugs.md#sim1sim).

Publication copies exactly `0x24` bytes from GLOBAL `+0x560` to `.data+0x1730`,
then converts doubles at `+0x13b8/+0x13c0/+0x13c8/+0x13b0` using the mutable
scale at `+0x728` and MSVCRT `_ftol`. Their low words go to
`+0x1754/+0x1756/+0x1758/+0x175a`. Each next product is loaded before the
previous AX is stored. The last conversion's full EAX/EDX result and the
actual imported ECX survive; the return is not normalized to a boolean.

Initialization restores state, registers list 1, 5 and 20 callbacks in that
order, calls the AIR wrapper, then publishes the descriptor **even if loading
failed or the decoder returned zero**. Init/deinit comparisons also check
callback identities, paired removal and retained-state ownership. The frame
and tick callback bodies are still traps, so registration is not evidence
that the partial module can simulate a frame.

### Caller verification

```sh
make -f tools/sim1.mk sim1air-run N=20000
make -f tools/sim1.mk sim1air-breaks
python3 -B tools/sim1_air_survey.py
```

The new caller harness reuses the reset host/MAIN state model and real
stateless UTIL service, then adds ACONTAIN path queries, file ownership,
uninitialized-buffer allocation, read outcomes, decoder fixture, descriptor
copy and CHAIN registration. It compares direct loading, direct publication,
init, and init followed by deinit across all twelve x87 modes and the three
load arrangements. All integer register results/ESP, meaningful x87 state,
the complete seeded image, GLOBAL, retained heap, file-buffer guards, import
transcript and model ownership/registration state are compared.

Inputs cross open failure, size failure, read failure, short read and full
read against eleven sizes from zero to 512 bytes; paths include lengths
1, 15, 254 and 255. The decoder fixture uses distinct pre/post-load values
so early publication cannot pass by coincidence. Parsed output values are
modeled; actual disk I/O, failed aircraft-path queries and allocation failure
are outside this caller test. The source manifests, rather than these small
I/O fixtures, supply real AIR contents for future decoder acceptance.

The caller suite's logs are `bin/sim1-diff/air-acceptance.log` and
`air-mutations.log`; rebuilt defects and their results are under `air-breaks/`.
These results are explicitly labeled **decoder modeled**, separately from
the real state/service comparisons.

At `N=20000`, all four caller operations run 240,000 times per load layout:
**2,880,000 comparisons, zero differences** over retail-first, rebuilt-first
and forced relocation. All **20 rebuilt caller mutations** are detected,
covering query/open arguments, allocation, read status/count, cleanup,
decoder result forwarding, descriptor copy/conversion/registers and init
ordering. The state and six-service suites are also rerun against the
extended build; their acceptance remains separate from this modeled boundary.

### AIR reference inventory

`tools/sim1_air_survey.py` walks the reference CD's eight aircraft AIR files,
checking each chunk's header and length without assuming alignment. It emits
ordered offsets, tags, sizes, per-file hashes and matching installed paths
to ignored `bin/sim1-diff/aircraft-survey.json`, without copying payloads into
tracked source. The current reference has **732 chunks and 105 distinct tags**;
all eight installed AIR files have byte-identical CD counterparts despite
their renamed installed filenames. This inventory supplies the real fixtures
for the decoder acceptance below.

## Complete simulation

`SIM1_COMPLETE` connects the AIR decoder (`0x2200af90`), aircraft setup
(`0x2200fc40`) and the tick/frame callbacks (`0x22001020/0x22001040`). The
handwritten services and state lifecycle retain their existing entry shims.
`tools/gen_sim1_lift.py` follows the remaining control flow and emits explicit
C operations and branches, following the crash-model transliteration approach.
There are **190 reachable routines, 17,836 distinct instructions and ten switch
dispatches**. These are static generated functions; no instruction decoder or
retail executable payload runs inside the replacement.

The generator verifies the original SHA-256, rejects overlapping instructions
and switch data, and copies only the proven switch tables, their 52-byte byte
selector, read-only data and version resource. It does not copy executable
text. Generated files stay under ignored `bin/sim1-diff/lift/`; the generator
and ABI support are tracked source. Unreachable instruction bytes and padding
are omitted. The replacement has exactly `ImportTable` and `Linkage`, and
preserves the reference 760-byte version resource.

Integer operations retain register widths and explicit condition flags using
the existing crash-model arithmetic primitives. x87 operations retain their
instruction order through inline assembly in a separately compiled Intel-syntax
translation unit. Calls to `_ftol`, `_CIacos`, `_CIasin` and `_CIpow` use the
actual MSVCRT functions. The module uses its own DLL entry rather than CRT
startup, avoiding an extra floating-point reset when aircraft load it.

`src/sim1/sim1_lift_native.S` transfers every GPR and flags across native
imports. The native callee performs its actual stack cleanup, including
cdecl and nonstandard register conventions. The modeled stack is probed within
the real Windows thread stack and placed below suspended C frames, so a native
callback can reenter SIM1 without overwriting them. TLS saves and restores the
active context across nested calls. Floating-point values remain on the x87
across these boundaries.

This remains a **32-bit x86 implementation**. The explicit integer C helps
future porting, but x87, pointer widths, register contracts and stack bridges
still need a native-platform implementation. No ARM or non-x86 equivalence is
claimed.

### Real decoder and ABI verification

```sh
make -f tools/sim1.mk sim1decode-run
make -f tools/sim1.mk sim1-regression-run N=20000
make -f tools/sim1.mk sim1-lift-breaks
```

`sim1decodediff` calls the untouched retail decoder beside the real rebuilt
decoder. It covers all eight AIR files, reversed and repeated records, 31
shuffled arrangements per file, each individual record omitted, and empty
files: **1,012 arrangements in twelve FP modes**, repeated retail-first,
rebuilt-first and with the preferred base reserved. There are **36,432 decoder
comparisons**. Input guards, the full 17,252-byte state body, each side's
import/Linkage prefix, GLOBAL, returned GPRs/ESP, meaningful x87 state and import
counts/message digests are compared without pointer-range normalization.

UTIL's random service has a deterministic fixture shared as an input, so each
side receives the same glider pitch choice. A separate oracle checks both pitch
signs. Sharing retail UTIL's mutable random generator originally gave the two
sides different choices and produced a false orientation mismatch; the other
UTIL services remain real.

Omitting Bell's engine-type record `0x105` reaches an unchecked interpolation
table and causes an access violation in both implementations, in all twelve
FP modes. These **36 paired fault cases** compare exception class/access type,
state written before the fault and imports. Interrupted GPRs and x87 condition
codes are not function-return results and are excluded on those cases. This
preserves **SIM1-AIR-UNCHECKED-TABLE**, marked in the generator and recorded in
[retail-bugs.md](../retail-bugs.md#sim1sim).

The native bridge also has independent machine-code callees with known answers
for all seven GPRs, arithmetic flags and cleanup sizes `0/4/12/36/256`. Tests
include cdecl arguments, an actual nested callback, and live x87 inputs/results.
This closes a coverage gap where corrupting returned EDX was unobservable in
the decoder alone because the caller overwrote it.

Aircraft setup also runs directly after decoding each complete AIR file, with
nonzero seeded position/velocity state and both branches of its optional
control-message dispatch. **288 comparisons** cover eight aircraft, twelve FP
modes and three load layouts. HOST `+0x40` is modeled as its verified two-argument
control-message boundary. Removing setup was invisible on the initial scripted
flight, so its mutation is required to fail this direct comparison instead.

The complete DLL retains the original six-service and fifteen-state-operation
acceptance: **4,516,608 service comparisons and 10,800,000 state comparisons**
across the three load arrangements. Their existing mutation suites remain
separate from the simulation mutations. Logs live in
`bin/sim1-diff/complete-acceptance.log`, `decode-acceptance.log` and
`lift-mutations.log`.

The simulation mutation suite checks decoder return/copy behavior, angle
arithmetic and unsigned inputs, native EAX/EDX/flags, direct aircraft setup,
and missing tick/frame callbacks. Value comparisons must fail for decoder/ABI/
setup defects. Callback defects must change state or rendered frames while
both baseline games and the defective game complete cleanly without faults.
**All ten rebuilt defects are caught.**

### Game verification and installation

```sh
make -f tools/sim1.mk sim1flights-run
make sim1-install FS98_DIR=game
make sim1-revert FS98_DIR=game
```

All **74 stock flights match over 1,200 passes each** with the `controls`
script, comparing two baseline runs and the replacement. Every run must load
SIM1, observe its numeric state and complete its requested passes cleanly.
FRAMELOCK records the numeric fields of the 192-byte situation record on every
pass, so matching rendered frames cannot hide different physics state. Both
the state samples and rendered-frame records match. These comparisons change
only SIM1 between sides; module DLL dependencies are restored to retail and
the other installed assets are shared unchanged.

The disposable flight copies suppress the modal crash-analysis graph while
preserving crash detection settings. DirectSound is disabled for repeatable
crash timing. The runner freezes its script, observer DLL and SIM1 candidate
before starting, so source edits or rebuilds cannot change a test midway.
Its candidate hash is recorded with the results. Logs and summaries are under
`bin/sim1-diff/flights/` and `flights-acceptance.log`.

A separate 1,200-pass St George control flight also matches with all rebuilt
module DLLs installed together (`integration-acceptance.log`).

`tools/stage_sim1.py` validates the two exports, module id, import table and
version resource, then replaces `Sim/SIM1.sim` atomically and verifies the
installed bytes. Revert uses the hash-verified original. Shared `all`,
`install` and `revert` include SIM1; the standalone targets support installing
it alone. Install/check/revert have also been exercised in a disposable tree.
The rebuilt module is installed in `game/Sim/SIM1.sim`; its code, state,
read-only data, imports, relocations and resources match the frozen candidate
used by the 74-flight acceptance. Only build metadata differs after relinking.
Post-install startup on a private display and Wine prefix passes against the
actual game tree: **27/27 modules, 95 gauge trace mentions, exact rebuilt SIM1
loaded**. The log is `bin/sim1-diff/install-smoke/result.log`.

These are short scripted trajectories, not long flights or landing acceptance.
Direct3D-specific rendering and unmasked FP exceptions are not covered by this
SIM1 run. The module's renderer-independent state is observed directly.

## Decompilation (started 2026-11-07)

The lifted simulation is being replaced routine by routine with hand-written C, by the method
the other modules used. The lifted build stays installed, and serves as a second oracle, until
the hand-written one has every routine and passes the same game acceptance.

**The routines and their contracts.** `tools/gen_sim1_tables.py` writes
`src/sim1/sim1_tables.inc` from the CD's SIM1 and the retail modules it calls: 186 routines
(the Linkage entries, every call target, code addresses held as data) with the bytes each pops,
and 35 module services with what each pops. Much of SIM1 is old hand-written assembly that
passes values in registers (edi holding GLOBAL, esi a table, ax a 16-bit position) and leaves
results in them, so a routine's contract is what its **callers** read: the generator walks
every path from each call site (through jumps and switch tables) until each register is read or
written, and lists the registers read, whether the flags are tested and whether the x87
condition codes are read (`fnstsw` before a compare of the caller's own). What reaches a `ret`
counts for the caller's own callers, to a fixed point. Partial writes (`mov dx, ...`) count as
writes of the low part only. No routine's callers read the condition codes. Linkage entries leave
eax and the callee-saved registers; CHAIN callbacks only the callee-saved (CHAIN sets eax before
each and ignores what it returns), which is what took 0x22008a77's eax, the x87 status word, off
its contract.

**The fixtures are real flights.** FRAMELOCK can now write SIM1's whole `.data` and GLOBAL every
n passes (`FRAMELOCK_SIM1_SNAPSHOT`, `HG2D_FRAMES_SNAPSHOT=n` in `tools/hg2d_frames.sh`). Eight
retail flights, one per stock aircraft type (STGEORGE, BELLSFR, LEARLON, EXTRARIO, CESSROME, SAIL1,
KENNEDY, PARIS), 1,200 passes of the `controls` script, a snapshot every 40: 240 states in
`bin/sim1-diff/snapshots.bin` (the two retail runs of each flight are identical; `make -f tools/sim1.mk sim1-snapshots` remakes it, `tools/sim1_snapshots.sh`). The floating
point then runs on values the flight model meets, which random doubles would not give.

**The harness**, `tools/difftest/sim1rdiff.c` (`make -f tools/sim1.mk sim1rdiff-run`): retail
SIM1 mapped at its base, its MSVCRT imports real, every module service a logging stub that pops
what the retail service pops (HOST's memmove modelled). Each scenario takes a snapshot and
perturbs it (a few doubles nudged, negated, zeroed or scaled, a few bytes changed; one scenario
in four scribbles over many words too, for fields a flight leaves zero). Each operation runs
retail then ours from the same fixture and entry registers, x87 precision control 53, 64 or 24
bits. Compared: `.data`, GLOBAL, the heap and blocks, the service log, the registers the callers
read, and the x87 stack (depth and every value; the condition codes only where callers read
them). The sticky exception flags are not compared: all are masked, SIM1 tests only C0/C3 of the
status word, and the compiler may load an operand early (a denormal raises DE) or compare with
`fcomi`. UTIL's pure arithmetic (sincos, angle from sine, atan2,
longitude to metres) runs OpenFS98's own verified UTIL (`bin/UTIL.DLL`) inside the stubs, so the
values SIM1 gets back are real; MAIN's nav read-out (+0x78) writes a scripted word; HOST's allocator and free work on the
fixture's heap; CONTROLS' force feedback (+0x5c) answers its three axes and logs the forces it
is given; MAIN's record write (+0x60) logs a hash of the record. Retail's
integer-divide faults are repaired as the host's filter repairs them in the
game; ours does the same arithmetic in C (`sim1_div32`, `sim1_idiv32`, `sim1_div16`,
`sim1_idiv16` in `src/sim1/sim1_internal.h`, without the fault). Faults are compared coarsely
(code, and whether the address was low, in the fixture or wild).

Each operation's adapter in `tools/difftest/sim1r_operations.inc` calls the C as a C caller will
and sets what it returns into the registers retail leaves it in. Rules the first slices taught:
copies through the x87 quiet signalling NaNs, so a C copy of a double goes through `sim1_x87`;
`fsin`, `fcos`, `fsqrt` and `fistp` are the instructions themselves (`sim1_fsin` and the others),
not library functions; comparisons use the `X87_C0`/`X87_C3`/`X87_C0C3` macros, which are what
retail's `fnstsw`/`test ah` sequences mean, NaN included; an adapter passes a double argument
by its bits, since loading it on the x87 could raise flags or quiet a NaN retail never touches.

**Progress (2026-11-08).** `src/sim1/sim1_small.c` has all 186 routines (state resets, the
16-bit and two-dimensional table interpolation of the hand-written flight model, integer and
48-bit fixed-point helpers, the parts' forces and moments, the control surfaces and their
weighted inputs, the brakes' hold, the heading hold's filter, the stall warning, the autopilot's
pitch schedule, the gear's model state, a contact's spring rise, the autopilot's holds, coupler and
autothrottle, the air data, the attitude matrix, the parts' winds, slips and side forces, the
structural limits, the inertias, the quaternion, the pitching, rolling and yawing moments, the
propeller and its governor, the aircraft setup (Linkage +0x1c), init and deinit (Linkage +0x04,
+0x08), the six other Linkage services (+0x18, +0x28..+0x38; `sim1.c` has the other agent's asm
versions), the situation's save, restore and startup, the three CHAIN callbacks (the tick, list
1; the engines' frame, list 5; the situation messages, list 0x14), the AIR file's loading and
decoding, the autopilot's course, altitude and flare, the carburettor, the rotor's mixing and
forces, the body accelerations, the contact points' ground test and stiffness, the ground's
roughness, a rate limiter, an angle follower, the integration step, the flaps, spoilers and
landing gear, the air-relative motion, the engines', ground and autopilot passes and the
autopilot's entry, the helicopter's rotor terms, blades, tail rotor, power and frame, the jet
and piston engines and the jets' atmosphere, the controls' pass, the attitude's quaternion
step, the simulation step and the tick's substeps). `make -f tools/sim1.mk sim1rdiff-run`: 186
operations, one per routine (the two CHAIN wrappers 0x22001020 and 0x22001040 run their
routines' C), 2,000 scenarios each, 372,000 comparisons, 0 differ. Deliberate breaks: `tools/sim1r_breaks.py` (run
it from a frozen copy; `ONLY=` picks breaks): 882, each caught when added; the ones left out
as proven equivalent (a few only equivalent in practice, one unreachable through its caller) are
listed with the reasons in its docstring. Operation shapers in `sim1r_operations.inc` steer
fixtures onto the edges a flight rarely reaches (equalities in compares, unsigned high bits,
special doubles and signed zeros, list terminators, the tick's cuts at 0x420, 0x800 and 0x1000
ticks). Retail bugs SIM1-SCALE-DISCARDED, SIM1-GOVERNOR-RPM, SIM1-AUTOTHROTTLE-ENGINES,
SIM1-GEAR-HORN-ENGINES (the last three suspected) and SIM1-SLEW-HEIGHT-EAX are registered;
SIM1-AIR-FRAME-OVERRUN is registered as fixed, not kept. The routine table had stopped at
0x22013340, taking the CRT's start for SIM1's end; `gen_sim1_tables.py`'s `CRT_START` is now
0x22014160, which added 0x22013340, 0x220135f0 and 0x22013b30 (the helicopter's frame, power and
tail rotor).

Lessons so far: GCC merges or moves stores past loads, which changes where a wild pointer
faults, so where retail stores before it reads the C says so (a compiler barrier in
`sim1_height`); GCC folds `-x + y` into `y - x`, which keeps a NaN x's sign where retail's
`fchs` flips it, so a retail negation is `sim1_fchs`, placed where retail places it; and GCC drops identities such as a multiply by a constant 1.0, which are exact only at full
precision: the game runs the x87 at 24 bits (Direct3D sets it; snapshots carry the control word), where that
multiply rounds, so such a constant goes in opaque (`sim1_x87(1.0)`); `sim1_ftol` pins its value into st0
before switching to chop rounding, since GCC may otherwise compute the value after the `fldcw` and
round it toward zero (caught in the AIR decoder's contact ratios at 24-bit precision); where a signalling NaN in memory
meets a NaN on the stack, retail's memory-operand `fmul` returns the stack's while GCC may have
loaded and quieted the operand first, so the larger significand wins: the game never holds a
signalling NaN in memory, so sim1rdiff counts such a difference (`nan_collisions`, only in a
scenario whose perturbation planted one, only between quiet NaNs) instead of failing it; a
register's wild value is kept in the upper half of the address space, since a write through one
into the harness's own memory (which neither side resets) made retail's run change ours; a
break for an edge one scenario in a thousand reaches names its scenario count and routine; a
shaper that points a register into .data gives the image's address (BASE + DATA_RVA + offset),
not the fixture buffer's, since the fixture is copied into the image and back around each run; `xor r, r`, `fnstsw` and 16-bit partial writes needed care in the contract
analysis, and each over-approximation there showed up as a register a routine "had" to leave.

### The native build

`make sim1` (`make -f tools/sim1.mk sim1-native`) builds `bin/sim1-native/SIM1_ours.sim` from
`src/sim1/sim1_small.c` and `src/sim1/sim1_native.c`; `make sim1-install` installs it
(`tools/stage_sim1.py`; `--lifted` installs the lifted build instead, `--retail` the CD's). The
image is retail's: `sim1_native.c` lays out the ImportTable, the Linkage and retail's `.data`
bytes as one 0x43f8-byte block, as retail's `.data` is, and points `sim1_base` at a copy of
retail's `.rdata`, so every `SF64`/`RF64` offset is retail's. `tools/gen_sim1_native.py` takes
those bytes and the resources from the CD's SIM1 into `bin/sim1-native`, and checks retail's
relocations: the only pointers in `.data` are the Linkage's, and `.rdata` has none, so nothing
else needs relocating. MSVCRT's `_CIpow`, `_CIacos` and `_CIasin` are found at attach;
`_ftol` is `sim1_ftol`. Three Linkage entries go through small thunks that keep the caller's eax,
which C cannot express: +0x18 (eax untouched), +0x28 (only ax set) and +0x38 (eax untouched, the
result in st0). CHAIN registers the C functions themselves.

Its acceptance is the lifted build's: `make -f tools/sim1.mk sim1native-flights`, the 74
scripted flights of `tools/fly_scripts.py`, retail SIM1 twice against ours, 1,200 passes each,
compared frame for frame and on SIM1's numeric state every pass: **74 match, 0 differ** (the first
build, 2026-11-08; STGEORGE alone matched first). With every other module ours too (`frames_many.sh` without
`HG2D_FRAMES_ONLY`), the whole game matches retail on the same 74 flights, no faults on either side. The register check (`tools/reg_contracts.sh SIM1` with
`HG2D_FRAMES_SIM1` set) needed FRAMELOCK to wrap SIM1's Linkage when the host loads it (it is
loaded per aircraft, after FRAMELOCK's first wrap); it reports one leftover, no breaks: init
(+0x04) leaves edx differently. Retail's init ends in the record words' `_ftol`, which leaves the
conversion's high dword in edx; no caller of a Linkage entry reads edx (`sim1_tables.inc`), and
the games run identical.

**Open.** Nothing is lifted in the installed SIM1 now. The lifted build stays buildable
(`make -f tools/sim1.mk sim1-complete`) as a second oracle. The remaining portability work is
SIM1's x87 dependence (`sim1_fsin` and the other instruction helpers, the `X87_C*` comparisons,
precision control) and the 32-bit layout of its image, which every module shares.
0x2200a7fe is the tail of deinit (0x220010a0, reached by a jmp) and goes with it. 0x2200a40e
(a hypotenuse), 0x2200a642 (ten to a power) and 0x22005293 (a clamp) have no references at all:
dead code, left out; so are 0x220022a0 and 0x22002468 (16-bit attitude code after the rate
limiter). 0x22002260 is init's tail (a jmp from 0x22001352) and 0x22004d80 the tail of
0x22004260 (a jmp from 0x2200437a); each goes with its routine. CHAIN passes a callback's edx
on to the next callback and returns its ecx/edx (chain.md); the contract treats SIM1's
callbacks as leaving only the callee-saved registers, which the game-level register check
(`tools/reg_contracts.sh` with FRAMELOCK_REGS) confirms (above). Harness notes from the last routines: the
snapshots hold the game's own pointers (+0x1fd0's four engine records), so an operation that
reaches code using them points them into the fixture first; a service called with words pushed
over stack garbage (`sub esp,2; push dx`) is logged by its low words (UTIL +0x2c/+0x38, WINDOW
+0x2c, SOUND +0x28); and `SIM1R_EIP=1` prints where each fault is.
