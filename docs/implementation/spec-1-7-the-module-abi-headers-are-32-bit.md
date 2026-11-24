---
title: 'Story 1.7: The module ABI headers are 32-bit'
type: 'refactor'
created: '2026-11-24'
status: 'done'
route: 'dispatch'
baseline_commit: '7c354a84c061559cd1341bcf19f2d8d74fbbc558' # worktree also holds uncommitted Story 1.5 slices 8-10, the 1.4 close-out and Story 1.6
review_loop_iteration: 0
context: ['{project-root}/OpenFS98/AGENTS.md', '{project-root}/OpenFS98/docs/native-port/architecture/ARCHITECTURE-SPINE.md']
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** The shared ABI headers describe FS98 memory with native C types (`void *` slots and links, pointer parameters, function-pointer typedefs), so every table and struct would shift on a 64-bit target. AD-14 requires one 32-bit layout on every build, and `FS98_STDCALL` is defined in six places.

**Approach:**
- Add `include/fs98/abi.h` with `fs98_addr`, `fs98_code`, `FS98_WCHAR` and the only `FS98_STDCALL`.
- Convert `linkage.h`, `host.h` and `global.h` to 32-bit contract types with layout asserts.
- Keep existing callers compiling through a retail-only bridge (the user's decision).
- Prove nothing changed: the rebuilt PEs and objects should be byte-identical. Wherever they still differ after trying, that module's difftests and the 74 flights must pass.

## Boundaries & Constraints

**Always:**
- **`abi.h`**
  - It holds `typedef uint32_t fs98_addr;` (arena offset), `typedef uint32_t fs98_code;` (code handle) and `typedef uint16_t FS98_WCHAR;`, each with a size assert. It includes only `<stdint.h>`.
  - `FS98_STDCALL` is `__attribute__((stdcall))` when `defined(_WIN32) && defined(__i386__)` (`win32-retail`, the only Windows i386 build). It is empty elsewhere, so native `linux-i386` uses the platform default (AD-4).
    - It has one textual definition, `#define FS98_STDCALL FS98_ABI_STDCALL_`, where the conditional sets the helper.
  - **`FS98_W` is not added here.** `u""` literals need C11, and the project builds `-std=c99`. The literal API is left to the Win32 headers' story, and that is logged in `deferred-work.md`.
  - Every other `FS98_STDCALL` definition is replaced by `#include "fs98/abi.h"`:
    - `host.h:28`, `numeric.h:7`;
    - `src/util/util_math.c:152`, `src/g3d/g3d_mesh.c:29`, `src/multip/multip.c:20`;
    - `tools/difftest/utildiff.c:20`.
    - Both G3D mesh object recipes (`cmake/steps/difftests.cmake:124` and beside it) gain `-Iinclude`.
- **The contract types (AD-14).** In the four headers, no shared layout and no contract signature uses a C pointer, `long`, `size_t`, `wchar_t`, a bitfield or `long double`. The one exception is the guarded retail bridge below (the user's decision). The rest of `numeric.h` is not checked.
  - Data pointers become `fs98_addr`, code becomes `fs98_code`, and 80-bit fields become `uint8_t[10]`.
  - Each service signature is a function type (`typedef int32_t FS98_STDCALL fs98_host_alloc_fn(int32_t, uint32_t, fs98_addr /* void ** */, int32_t);`), with each parameter's pointer meaning kept in a comment.
  - Callback parameters are `fs98_code`, with their convention stated in a comment. They are: qsort's, bsearch's and list walk's comparators and visitors, and voice play's stdcall completion callback (`host.h:403`, which SOUND passes as `snd_crash_done`, `src/sound/sound.c:1792`).
  - `struct fs98_import` has `fs98_addr link`. `struct fs98_linkage` has `fs98_code` slots and a `fs98_code service[]` tail.
  - `fs98_host_find` stays opaque, as an `fs98_addr` handle. `fs98_host_file`'s Win32 handle and path fields become `uint32_t` and `fs98_addr`.
- **The retail bridge (user's decision).**
  - Under `#if defined(_WIN32) && defined(__i386__)`, `host.h` also keeps, for each service, a call type with today's native pointer signature, named `fs98_host_<name>_call`. `FS98_HOST_SLOT` and the `FS98_HOST_*` macros use it, as today: they load the slot as an `fs98_code` and convert it to that call type.
  - Under the bridge, `FS98_HOST_SLOT` keeps exactly today's expansion: it loads the slot directly as the call type. Codex measured that an `fs98_code` load followed by a conversion changes SOUND's code.
  - So the 365 existing `FS98_HOST_*` call lines in 14 files compile unchanged, and pointer-signature callbacks still match.
  - `src/mdl/mdl_normal.h:14` and `.c`, whose API takes `fs98_host_memmove_fn`, move explicitly to `fs98_host_memmove_call`, because `tools/difftest/mdldiff.c:108` passes `copy_stub` to it.
  - Outside that build, the bridge doesn't exist and calls fail to compile until Story 1.8's `fs98_call`.
  - The bridge is commented as retail-only and transitional, and Story 1.8 removes it module by module.
  - These direct casts hold only for retail address words. Native handle registration (AD-17) is not part of this story.
- **The other users.** Direct readers and writers of the converted fields are adapted with the smallest retail cast, for example `(uint8_t *)(uintptr_t)x.link`, or `link = (fs98_addr)(uintptr_t)blob`. That includes:
  - `NULL` initialisers and comparisons on converted fields (they become `0`);
  - link reads (`src/test/ofstest.c:537`, `src/slew/slew.c:160`); `NULL` link initialisers (`src/facil/facil.c:3254`); pointer writes into links (`tools/difftest/numericdiff.c:276-278`, `fesldiff.c:1677`, `facildiff.c:481`, `landmediff.c:232`);
  - and the direct typedef users (`dynamic.c`, `ofstest.c`, `mdl_normal.c`/`.h`, `slew.c`).

  The inventory comes from the compiler, not a grep: build, fix each error or warning, and repeat. Private layouts that only look similar are not converted:
  - FE's harness `struct fs98_import` (`tools/difftest/fediff.c:48`);
  - SLEW's private UTIL typedefs and its and GLOBAL's private export layouts;
  - SIM1's `uintptr_t linkage[15]` (`src/sim1/sim1.c:27`). SIM1's embedded `fs98_import` array does change, through the header. Implementation Notes record that MinGW i686 accepts `(fs98_code)(uintptr_t)func` in a static initialiser and x86-64 GCC rejects it, for whoever migrates those tables.
- **Layout asserts.**
  - Every complete struct in the four headers gets explicit padding and `#pragma pack(push, 4)` where retail is 4-packed. That is `fs98_host_list`, `fs98_host_find_entry`, `fs98_host_image`, `fs98_host_accel` (`host.h:557`), `fs98_host_file` (`:596`), `fs98_import` and `fs98_linkage`.
  - Each gets `_Static_assert`s on its size, alignment (`_Alignof`) and every field offset. The values are measured by a probe compile on `win32-retail` before any edit, and recorded in Implementation Notes.
  - `fs98_linkage` asserts its 12-byte fixed header, and the tail's offset and 4-byte stride.
  - `global.h` gains `FS98_GLOBAL_SIZE` (4776, `0x12a8`). It asserts that every named `FS98_G_*` offset plus its accessed width fits inside it, with `FS98_G_POS_BASE` spanning 8 bytes (UTIL reads through `+7`, `src/util/util_math.c:298`). `src/global/global.c:22`'s local definition is replaced by the header's, and its assert at `:169` uses it.
  - `FS98_G_PTR` reads an `fs98_addr`.
  - A header-only probe compiles the four headers and their asserts with three compilers, each with `-std=c99 -Wall -Wextra -Werror`:
    - `i686-w64-mingw32-gcc`, with `module_flags.FLAGS`, which `tools/check_module_flags.sh` requires;
    - host `gcc -m32`;
    - host `gcc` (x86-64).
- **The identity gate.** It is the evidence that "all difftests and flights still pass" without running them (story AC).
  - Both sides build from frozen source snapshots, one after the other, at the same absolute path (`~/openfs98-verify/p17`), because the MDL generators embed resolved `orig/` paths in what they compile. Each side's outputs are saved separately before the next build.
    - The snapshot is a `cp -a` of the tree without `bin/` and `build/`, with `orig/` (the build reads it); before is the current tree, after is the edited one.
    - Each is configured freshly.
    - `cd/` and `game/` are added only for fallback runs.
  - Each runs `cmake --build`, `tools/ofs difftests`, and an explicit relink of every on-demand PE whose recipe compiles an affected unit (for example `FE_sl_ours.dll` `cmake/targets.cmake:630`, `OFSTEST.DLL` `:673`, `FRAMELOCK.DLL` `:1750`, the SIM1 links `:102`).
  - **The unit manifest** comes from the actual compiler invocations.
    - A wrapper earlier on `PATH` for `i686-w64-mingw32-gcc` logs each one's working directory and argv, subprocesses included (the MDL builders run GCC from Python, `tools/build_mdls.py:84`).
    - Then each translation unit of each logged invocation is replayed with `-c -MD` into a uniquely named object and depfile.
    - The depfiles decide which units reach the six headers. The affected set is the union of before and after, plus every edited unit.
    - The before snapshot and its generated inputs are kept for replaying units only the after side reveals.
    - Of run targets such as `ofstest-run`, only the build lines are replayed, never the install (`cmake/targets.cmake:676`).
    - Mutation builds (`controls_break.c`, `*-breaks` variants) are excluded and named as excluded.
    - `.S` units are recorded with the link order they feed.
  - Compare the artifact sets first: the same paths on both sides. A missing artifact or an `ERR` fails the gate outright.
    - Then compare each PE by full SHA-256 after normalising only: the COFF timestamp, the optional-header checksum, the export directory's timestamp, and `CSWTCH.<digits>` numbering in the COFF symbol string table only. `peh3.py`'s whole-file `_CSWTCH` rewrite is narrowed to that.
    - Objects are compared with `cmp`.
  - Result: normalised identity of every PE in scope and byte identity of every object.
- **Identity is the goal, not a guarantee.** Changing `fs98_import.link`'s type may change code (SOUND's object changed in Codex's probe).
  - For each differing object, first try a formulation that gives the same bytes.
  - Any difference left is listed in Implementation Notes with its cause. It then needs:
    - each affected module's and harness's difftest at its Baselines count, with 0 differ;
    - the 74 flights under pinned settings: `FRAMES_MANY_JOBS=1 FRAMES_MANY_PASSES=900 tools/frames_many.sh -`, with the rebuilt `SIM1.sim` selected (`HG2D_FRAMES_SIM1`) if it changed.
      - The unedited snapshot runs first under the same settings.
      - Every flight's verdict must be `match` or `match, no sound`, and equal to that baseline run's verdict for the same flight (ANTILLES and STKITTS are `match, no sound` there).

  Those runs are serial, from a frozen copy, and never during Story 1.6's recording.
- Builds run under `nice -n 19`, with `-j 2` and `CMAKE_BUILD_PARALLEL_LEVEL=2`. No staging or commits, no reverting sources to take a baseline, and no changing this spec's status.

**Never:**
- No `arena.h`, `fs98_ptr` or `fs98_call` (Story 1.8), and no register-contract change (Story 1.9).
- Don't convert `host_runtime.h`, `keep.h`, the rest of `numeric.h`, or modules' private struct copies. They are listed in `deferred-work.md`.
- No behaviour change, and no new warnings (`-Werror`).

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|----------|--------------|---------------------------|----------------|
| Layout drift | a header struct field moved, or a change to a struct's size or alignment (a resize that padding absorbs is not caught) | `_Static_assert` fails on every probe compiler | build stops |
| Outside GLOBAL | an `FS98_G_*` offset plus width past 4776 | `_Static_assert` fails | build stops |
| Second STDCALL | a `FS98_STDCALL` definition outside `abi.h`, any spacing | the directive check fails | fixture proves it |
| Forbidden type | a pointer, `long`, `size_t`, `wchar_t`, bitfield or `long double` in a contract type, typedef-hidden included | the type check fails | fixture proves it |
| Native build | the four headers on host gcc (x86-64) | the asserts pass, and the bridge is absent | — |

</frozen-after-approval>

## Code Map

- `include/fs98/linkage.h:56-83`: `fs98_import` and `fs98_linkage`. No static `struct fs98_linkage` initialiser exists in `src/`.
- `include/fs98/host.h`:
  - `FS98_STDCALL` at `:28`; the typedefs from `:65`;
  - the callback-taking typedefs, qsort at `:146` and list walk at `:427`;
  - the structs at `:158`, `:188`, `:537`, `:557` and `:596`; `FS98_HOST_SLOT` and the call macros at `:741-808`.
- `include/fs98/global.h:17-21`: the accessors. SLEW uses them (`src/slew/slew.c:94-96`, `:199`).
- **Users:**
  - `FS98_HOST_*` calls: 365 lines in 14 files (`grep -rlE 'FS98_HOST_[A-Z_]+\(' src tools`), unchanged through the bridge;
  - direct typedef users: `dynamic.c`, `ofstest.c`, `mdl_normal.c` and `.h`, `slew.c`, and `tools/difftest/mdldiff.c:82`.
- **Gate tools:** `/tmp/claude-1000/-home-steve-source/b2e3078b-1d77-426e-816f-2de2772a67dc/scratchpad/peh3.py`. A copy extended to full hashes and `ERR` failures goes in `pre17/`. The Story 1.5 slice 10 manifest (`pre15j/obj/manifest.txt`) shows the form.
- `docs/architecture.md`: the module ABI section.

## Tasks & Acceptance

**Execution:**
- [x] Before any edit:
  - the layout probe;
  - the frozen before snapshot, built in full with its relinks;
  - the unit manifest from its expanded recipes and `-MD` output;
  - hashes and objects into `pre17/`.
- [x] `include/fs98/abi.h` (new). Then `linkage.h`, `host.h`, `global.h` and `numeric.h`: the contract types, the bridge and the asserts.
- [x] The four other `FS98_STDCALL` sites, the G3D mesh `-Iinclude`, and every user the compiler reports.
- [x] `tools/check_abi_headers.py` (new):
  - the directive check: exactly one `FS98_STDCALL` definition, in `abi.h`, and none elsewhere in `src`, `tools` or `include`, any spacing;
  - the type check: it preprocesses `abi.h`, `linkage.h`, `host.h` and `global.h` and inspects their contract declarations, typedefs resolved. It excludes the accessor macros and the guarded bridge block by name;
  - the probe compiles;
  - and `--self-test` with failing fixtures for each.

  Add it to `docs/tools.md` Baselines.
- [x] `docs/architecture.md`: a paragraph on `abi.h`, the contract types and the bridge. `docs/implementation/deferred-work.md`: the deferred items named above.
- [x] The after snapshot, built in full; the gate.
- [x] This spec: `## Implementation Notes` with the layouts, the manifest counts, the exclusions and the gate result.

**Acceptance Criteria:**
- Given the converted headers, when both snapshots build, then they succeed with no new warnings, and every assert passes on all three probe compilers.
- Given the gate, then the artifact sets match (no missing file, no `ERR`), and every in-scope PE and manifest object is identical. Any explained content difference instead has its difftests and the 74 flights passing as stated.
- Given `python3 -B tools/check_abi_headers.py` and `--self-test`, then both pass.

## Verification

**Commands:**
- `python3 -B tools/check_abi_headers.py --self-test && python3 -B tools/check_abi_headers.py` -- expected: both pass.
- the before and after snapshot builds, then the gate's diffs -- expected: same artifact set, empty diffs.

## Review Triage Log

Spec review 1 (Codex gpt-6.2-sol), 11 findings, all applied:
1. high: `FS98_STDCALL` is stdcall only on `_WIN32 && __i386__`, so native `linux-i386` uses the default convention (AD-4).
2. high: function types of the same width are still incompatible in C. The retail bridge keeps pointer-signature call types, and callbacks become `fs98_code`.
3. high: the casts are limited to retail address words. AD-17 registration is out of scope, and the private tables (SLEW, GLOBAL, SIM1, FE harness) are left alone, with the static-initialiser note.
4. high: there are 365 `FS98_HOST_*` calls in 14 files. The user chose the retail bridge; the other users are found by the compiler, and the named pointer and `NULL` sites are listed.
5. high: the G3D mesh recipes need `-Iinclude`, and the unit set is the union of before and after dependencies.
6. high: the gate builds from frozen snapshots, relinks the on-demand PEs, compares artifact sets and full hashes, and fails on `ERR`.
7. high: the manifest comes from expanded recipes and `-MD` output, with mutation builds excluded by name and `.S` link order recorded.
8. high: identity is the evidence; any exception needs named difftests at Baselines counts and 74 exact `match` verdicts with `FRAMES_MANY_JOBS=1`.
9. medium: the layout map is corrected (`accel`, `file`), with alignment, linkage-tail and GLOBAL width asserts, and probes on three compilers.
10. medium: `FS98_W` is deferred (C11 literals versus `-std=c99`), and only `FS98_WCHAR` is added.
11. medium: the grep checks are replaced by `tools/check_abi_headers.py` with failing fixtures.

Spec review 2 (Codex gpt-6.2-sol), 10 findings, all applied:
1. high: identity isn't guaranteed (a probe changed SOUND's object). `FS98_HOST_SLOT` keeps today's expansion, and the difftest-and-flights fallback is the stated path for any difference left.
2. high: the MDL generators embed absolute `orig/` paths, so both snapshots build at one path, one after the other. `orig/` is kept, and `cd/` and `game/` are for fallback runs only.
3. high: the units come from a compiler wrapper (Python-launched GCC included), replayed per unit with `-c -MD`; run targets replay their build lines only.
4. high: normalisation is named field by field (`CSWTCH` only in the symbol strings), and missing files and `ERR` fail outright.
5. high: the fallback flights compare per-flight verdicts against an unedited-snapshot run under the same settings (`match, no sound` allowed where the baseline has it), with SIM1 selected if it changed.
6. medium: the bridge is stated as the one exception to the contract rule, and the type check names its four headers.
7. medium: `FS98_STDCALL` has one textual definition through a helper, and the MinGW probe uses `module_flags.FLAGS`.
8. medium: voice play's callback is `fs98_code`, and `mdl_normal.h`/`.c` move to the call type.
9. medium: the matrix states what the asserts detect, `POS_BASE` spans 8 bytes, and `global.c`'s local constant is replaced.
10. low: the inventory labels are corrected, and the FESL, FACIL and LANDME link writers are added.

## Implementation Notes

**Layouts** (probe compile on `win32-retail` before any edit, values unchanged by the conversion; all 4-aligned, `#pragma pack(push, 4)`):
- `fs98_host_list` 28: count 0, capacity 4, grow 8, elem_size 12, iterating 16, pending 20, data 24.
- `fs98_host_find_entry` 272: name 0, type 260, size 264, writable 268.
- `fs98_host_image` 28: format 0, width 4, height 8, stride 12, rows 16, size 20, pixels 24.
- `fs98_host_accel` 12: command 0, flags 4, key 8.
- `fs98_host_file` 16: handle 0, path 4, mode 8, ext 12.
- `fs98_import` 8: id 0, link 4. `fs98_linkage` 12 fixed (id 0, init 4, deinit 8), `service[]` at 12, stride 4.
- `FS98_GLOBAL_SIZE` 4776; `FS98_G_POS_BASE` asserted over 8 bytes. `global.c`'s local constant is replaced by the header's.

**Bridge and identity formulations.** Plain casts at the converted fields changed gcc's code (register and stack-slot choices) in 9 modules, so identity needed pointer-typed access:
- `fs98_import` gets, on `_WIN32 && __i386__` only, an anonymous union `{ fs98_addr link; struct fs98_linkage *link_ptr; }`; `FS98_LINK(import)` reads `link_ptr`. A punned `*(T *)&x.link` did not restore FE or mdl_crash; the union (a component reference) did.
- The union makes `{ id, 0 }` initialisers warn (`-Wmissing-braces`), so the initialisers in acontain, atc, chain, dynamic, facil, fe, numeric, slew, util, weatherm, sim1, ofstest, sehfix and `tools/drive/framelock.c` became `{ id, { 0 } }`.
- List `data` reads in FACIL and SOUND use `FS98_RETAIL_PTR(void *, x.data)` (abi.h, retail only, same transitional status). `FS98_RETAIL_PTR` and `FS98_LINK` are additions beyond the spec's abi.h list.
- Direct typedef users (`dynamic.c`, `ofstest.c`, `mdl_normal.c/.h`) moved to `fs98_host_<name>_call`; leaving `_fn` there would have silently called the slot's address instead of the pointer stored in it.
- Other fixes: NULL to 0 on converted fields, `(void **)&list.data` for host free, `(fs98_addr)(uintptr_t)blob` in `numericdiff.c`, `(void *)(uintptr_t)` in `util_files.c`, the four `FS98_STDCALL` sites, `-Iinclude` on both G3D mesh recipes.

**Manifest.** 522 translation units from 489 logged gcc invocations (423 from `all` and `difftests`, 62 from the on-demand gcc/windres lines of `cmake/targets.cmake`, 66 lines run); 15 are `.S`. 80 reach the headers before, 86 after (abi.h). Excluded and named: the mutation and break builds (`advdrv-break`, `analysis-break-build`, `controls-break-build`, `g3d-break-build`, `host-break(s)`, `main-break-build`, `mpchat-break-build`, `mpchat-ui-breaks`, `multip-*-breaks`, `panels-break(s)`, `reality-break-build`, `sim1-breaks`, `window-break-build`, `winmsg-break-build`), and every run/install line. On-demand lines that Python scripts launch (MDL builders) are in the gcc log through the PATH wrapper.

**Gate result** (both sides built fresh at `~/openfs98-verify/p17`, one after the other; the before tree is kept at `p17-before-built`, its sources at `p17-before-src`):
- Artifact sets: 1129 files, same paths; 0 `ERR`.
- 320 PEs: all identical after normalising the COFF timestamp, checksum, export timestamp and `CSWTCH.<n>` in the symbol string table. 206 `.o`/`.a` identical by hash.
- All 522 replayed unit objects identical by `cmp`, bar the two `g3d_mesh.c` units whose recipes gained `-Iinclude` (their `bin/difftest/g3d_mesh_*.o` outputs are identical).
- Warnings in the `all` and `difftests` logs: the same set as before; no errors.
- So no difftest or flight fallback run was needed; none was run.

**Checks.** `python3 -B tools/check_abi_headers.py` and `--self-test` pass (three probe compilers, directive, type and layout fixtures). `tools/check_module_flags.sh` passes. The pre-existing `maindiff.c` sign-compare warnings (2) are in both builds.

**Not done.** SIM1's `uintptr_t linkage[15]`, FE's harness `fs98_import`, SLEW's and GLOBAL's private layouts and the other deferred headers are untouched, as the spec says. `(fs98_code)(uintptr_t)func` static initialisers: MinGW i686 accepts them, x86-64 GCC rejects them.

Code review 1 (Codex gpt-6.2-sol: blind, edge-case and verification-gap layers; deduplicated). No finding touched the headers' or the users' code; all concern `tools/check_abi_headers.py`.
- medium, patch: no calling-convention check, so stdcall on native `linux-i386` would pass (blind, gap).
- medium, patch: the type check reads only host-gcc output and misses Win32/i386-only branches (blind, edge, gap).
- low, patch: the native bridge macros (`FS98_RETAIL_PTR`, `FS98_LINK`) are not checked absent with `-dM` (blind, edge).
- low, patch: array and function parameters, which C adjusts to pointers, pass (blind, edge, gap).
- low, patch: the directive scan misses `#define/**/` and line splices and counts commented-out definitions (blind, edge, gap).
- low, patch: the headers are probed only together, not each alone (edge).
- low, patch: layout fixtures don't require the static-assertion message (edge).
- low, rejected: typedef multi-declarator and embedded-aggregate resolution, `extern` skips, braces in assert strings, trailing inline definitions, array-bound arithmetic, a missing-compiler exception, and fixture tidiness (blind, edge). These are full-C-parser cases with no current occurrence in the four headers, and the fixes need a real parser.
- After the patches: `tools/check_abi_headers.py` and `--self-test` pass. Only the checker changed, so the identity gate's result stands.
