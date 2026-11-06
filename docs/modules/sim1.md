# SIM1.sim — module id 25

The flight model named by the stock aircraft's `.AIR` files. ACONTAIN reads
chunk `0x100`, loads `Sim\<name>.SIM` through host `+0x26c`, and publishes its
Linkage at GLOBAL `+0x11d4`. This is simulator code outside `Modules\`, not
`English/Sim.DLL` (the aircraft-settings UI).

**Partial, 2026-11-06:** six bounded services, both situation formats, startup
state restoration, saving, retained state and shutdown are implemented and
compared in isolation. The AIR file wrapper, descriptor publication and init
coordination also match at an explicit decoder-fixture boundary. The AIR record
decoder and frame callbacks remain unimplemented. There is no install target,
shared Makefile entry or game acceptance for the partial build.

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

## What follows

Initialization is not a small no-op slice. At `0x22010cc0`, SIM1 restores a
retained `0xc0`-byte record through GLOBAL `+0x11d8`, or asks MAIN `+0x6c`
for situation record `0x38e` and falls back to `0x5a`. Init registers callbacks
on CHAIN lists 1, 5 and 20. List 20 handles situation load/save events
`0x22/0x23`. Deinit removes the callbacks and serializes retained state.

Both readers, the velocity transform, startup state restoration, save callback
and retained-state/shutdown paths are now covered below. Init/file ownership
and publication have also been compared with the pending decoder replaced by
an explicit fixture. Next: implement the actual AIR record decoder, complete
aircraft setup and frame callbacks, then repeat initialization with the real
decoder. Full-module mutation acceptance, real aircraft fixtures, long state
trajectories and isolated game acceptance remain before SIM1 can join installation.

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

**The record decoder at `0x2200af90` is not implemented.** The ordinary
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
their renamed installed filenames. This inventory is input evidence for the
pending decoder, not a decoding or in-game acceptance result.
