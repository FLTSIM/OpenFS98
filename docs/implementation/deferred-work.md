- source_spec: spec-1-2b-verify-baselines-through-cmake.md
  summary: Open break coverage found by the CMake re-verification (PANELS' 12 mutants missed at N=3000; SIMSUI init-far and char-digit-end; MAIN open-id/open-answer and G3D plane-facet-skip crashing). Not caused by the port.
  evidence: docs/remaining-work.md "Open from the CMake re-verification"; logs in ~/openfs98-verify/p3-results (panels-breaks-3000.log, simsui-3-2000.log, simsui-probe2.log).

## Deferred from: code review of spec-1-2-cmake-targets-for-every-tools-md-target.md (2026-11-19)

- source_spec: spec-1-2-cmake-targets-for-every-tools-md-target.md
  summary: Break runners other than the nine already fixed (WINDOW, WINMSG, REALITY, ANALYSIS, MPCHAT, HOST, G3D, PANELS, CONTROLS) may still count a harness crash as a caught break.
  evidence: Unverified; read each remaining tools/*_breaks.py verdict test.
- source_spec: spec-1-2-cmake-targets-for-every-tools-md-target.md
  summary: No regression test for tools/modules.sh output or tools/ofs argument handling.
  evidence: modules.sh matched `make -s` for every query before the Makefile was deleted; a table test would guard later edits.

## Deferred from: code review of spec-1-3-one-flag-set-for-every-unit.md (2026-11-19)

- source_spec: `docs/implementation/spec-1-3-one-flag-set-for-every-unit.md`
  summary: Nothing fails if a codegen override (-fno-ipa-icf, -fno-tree-loop-distribute-patterns, -fno-tree-vectorize, -fno-math-errno) is later dropped from one of its sites; the toolchain table is documentation only.
  evidence: medium, unverified as a harm. Without -fno-ipa-icf, DEMO's code changes but demodiff still gives 0 differ, so a dropped override may go unnoticed. A section comparison against a reference build per site would settle it (Story 1.4's image checks are the natural home).

## Deferred from: Story 1.4, slice 1 (WINMSG and MPCHAT data decompiled, 2026-11-19)

- source_spec: `docs/implementation/spec-1-4-module-data-decompiled-no-raw-dumps-in-git.md`
  summary: Every other module's data image is still a raw dump or is generated from `orig/` by a `tools/gen_*.py`. Later slices convert them and shrink the allowlist to empty.
  evidence: `tools/raw_dumps_allowed.txt` is the list (`python3 tools/check_raw_dumps.py` fails when an entry stops being an offender).
- source_spec: `docs/implementation/spec-1-4-module-data-decompiled-no-raw-dumps-in-git.md`
  summary: tools/check_raw_dumps.py only scans src/ and include/; several harness fixtures in tools/difftest/ (panels_funcs.inc, hg2d_draw_suite.inc, multip_send.inc, g3ddiff.c) hold long literal runs that may be retail data.
  evidence: the review's wider rule found 64–270-literal runs there; whether each is retail data or a hand-made vector needs a look per file.
- source_spec: `docs/implementation/spec-1-4-module-data-decompiled-no-raw-dumps-in-git.md`
  summary: Nothing stops an entry from being added to tools/raw_dumps_allowed.txt, and check_data_image.py does not compare our image's own size (the structs' _Static_assert does for WINMSG and MPCHAT).
  evidence: once the allowlist is committed, the check can fail when an entry is not in `git show HEAD:tools/raw_dumps_allowed.txt`; later modules should keep the sizeof assert or the tool should check the export's section bounds.
- source_spec: `docs/implementation/spec-1-4b-generators-reading-orig-and-world-tables.md`
  summary: Scripts reached only through a `tools/*.sh` wrapper are not seen by check (c).
  evidence: the rule matches `tools/<name>.py` named in `cmake/steps/*.cmake` and `cmake/targets.cmake` only.
- source_spec: `docs/implementation/spec-1-4b-generators-reading-orig-and-world-tables.md`
  summary: The 14 remaining `gen:` entries after slice 13 (45 before slice 4, 34 before slice 5) in `tools/raw_dumps_allowed.txt` still need converting. Slice 4 moved the seven `*_runtime` scripts, `gen_host_resources` and `gen_main_bitmap` to `resource:`; `gen_options_tables` and `gen_sim_tables` are retired.
  evidence: `gen:` by module -- ACONTAIN, ADVDRV, ANALYSIS, ATC, BOOSTER, CONTROLS, DEMO, DYNAMIC, FACIL, FE, G3D, LANDME, PANELS, REALITY, SOUND, WEATHERM, WINDOW: `gen_<m>_data.py`; HOST: `gen_host_data` (also pins the reference `FLTSIM98.EXE` by SHA-256 for the host build), `gen_host_rt_tables`; MAIN: `gen_main_data`; MULTIP: `gen_multip_data`; SIM1: `gen_sim1_data/native`; SIMSUI: `gen_simsui_native` (embeds retail `.data` into `bin/`), `gen_simsui_tables`; MDLs and gauges: `build_mdls`, `gen_mdl_crash`, `gen_gauges`; test DLLs: `gen_warning_data`, `gen_suction_data`.
- source_spec: `docs/implementation/spec-1-4c-retail-table-generators.md`
  summary: One tables generator stays because another script reads it: `gen_simsui_tables` (`gen_host_rt_tables.py:40` imports helpers). `gen_options_tables` and `gen_sim_tables` were retired in slice 4, with the table halves of their readers.
  evidence: `gen_simsui_tables` goes later, together with `gen_host_rt_tables` and its `host_rt_rdata.inc` dump. `gen_sim1_native`, `gen_gauges`, `build_mdls` and `gen_mdl_crash` put retail data into `bin/` and stay `gen:` for the dump slices.
- source_spec: `docs/implementation/spec-1-4d-runtime-and-resource-generators.md`
  summary: With the image checks removed from the resource: scripts, a pressing whose dialogs or other resources differ from the reference builds without error against the vtable and message-map tables committed for the reference image, and would fail only at run time.
  evidence: maybe-false, medium if true. Owned by Story 6.1 (its resources AC now names the committed tables); method and known facts in docs/building.md, "Resources from another pressing". The November pressing rebuilt MAIN (gen_main_bitmap) and FLTSIM98.EXE (gen_host_resources, still guarded by gen_host_data's pin). Settle it with tools/pressing_catalogue.py compare. If resources differ, add a per-module resource-hash check that names the pressing.
- source_spec: `docs/implementation/spec-1-4f-window-acontain-dynamic-data.md`
  summary: WINDOW's masks and code tail (retail keeps them in `.text`; ours is a `.rdata` copy) have no retail oracle other than s14 identity and `windowdiff`. `gen_window_data.py` checks retail against its own literals, not against the typed C in `window.c`.
  evidence: `gen_window_data.py` still asserts retail's masks, zero block and pitch words against its own literals, but not against the typed C in `window.c`; the tail is copied from retail at build time.
- source_spec: `docs/implementation/spec-1-4f-window-acontain-dynamic-data.md`
  summary: Guard part RESOLVED by slice 8 (`tools/ofs data-images` and the per-build `--hash` check; a changed table now fails the build). STILL OPEN: the symbol-per-slot pointer check, because our DLLs carry no symbols. HG2D (`--mixed`) adds 40 unlisted zero code slots (+0x3c, +0x40, +0x50..+0xf4 apart from the listed +0xc4..+0xd0) that DllMain sets in C, and 112 raw retail code addresses kept as values; both pass the check unchecked. check_data_image.py zeroes relocated slots on both sides, so a swapped handler in DYNAMIC's 68-slot dispatch table, or a wrong Linkage, reader or handler pointer in any converted module, passes it. CONTROLS (44 of its 70 slots are listed in `controls_pointers.inc`, the rest are set in C) and SOUND (its pointers are static, all ours) have no `ptrs=` list and stay under this check too. Extend the planned `tools/ofs data-images`: resolve each of ours' relocated pointers to its symbol, and compare the symbol for each slot against a committed map of retail address to handler name. Alternatively, run the module difftests in the normal verification path.
  evidence: today's tables are proven by whole-DLL identity with the pre-slice build, whose generators asserted them; nothing guards a later edit except the difftests.

## Deferred from: Story 1.4, slice 7 (ANALYSIS, REALITY, LANDME and ATC data decompiled, 2026-11-20)

- source_spec: `docs/implementation/spec-1-4i-facil-multip-fe-data.md`
  summary: `src/fe/fe_bands.inc` (256 latitude-band edges and 257 cosine-like steps, typed u32) is allowlisted as data because no exact formula is known. Either derive a formula that reproduces both tables bit for bit, or confirm they stay data, then settle the allowlist entry.
  evidence: slice 9 decision (the user chose committed and allowlisted); `docs/implementation/spec-1-4i-facil-multip-fe-data.md`.

## Deferred from: Story 1.4, slice 11 (ADVDRV, SOUND, PANELS and CONTROLS data decompiled, 2026-11-20)

- source_spec: `docs/implementation/spec-1-4k-advdrv-sound-panels-controls-data.md`
  summary: Guard ADVDRV's 96 typed `.rdata` doubles (`advdrv_rdata` in `src/advdrv/advdrv.c`), SOUND's `sound_consts.inc` and NUMERIC's `fs98_numeric_rdata` (seven bit patterns, indices 0 and 3 read; slice 12) against later edits. gcc folds them into our literal pool, so no `rdata=` window exists. A retail-side check could parse the pool and compare it with retail's `.rdata`, with a per-build hash of the parsed values.
  evidence: slice 11 review; `--rdata 0x9000:0x300` finds 0 matches in ours; today the values are proven only by whole-DLL identity with the pre-slice build.

## Deferred from: Story 1.4, slice 12 (NUMERIC and UTIL data decompiled, 2026-11-20)

- source_spec: `docs/implementation/spec-1-4l-numeric-util-data.md`
  summary: The cos/asin/atan tables are still numbers (`src/util/util_trig.inc`) because C99 cannot evaluate them in a static initialiser. The latitude-circle table (78 entries) has no exact formula. Regions still undescribed, typed as words: UTIL +0x5ae (2 bytes), +0x6ec..+0x700 and +0x780..+0x7e8; NUMERIC +0x158..+0x198 (eight 64-bit words, unread by our code).
  evidence: slice 12; each is marked "meaning not yet known" in `src/util/util_math.c` and `src/numeric/numeric.c`.
- source_spec: `docs/implementation/spec-1-4m-global-hg2d-data.md`
  summary: Guard `src/hg2d/hg2d_projective_tables.inc`, the tail tables from retail's `.text` whose words are retail code addresses, with a check against retail's `.text` at the addresses its header lists, and a per-build hash.
  evidence: slice 13 Codex review; once `gen_hg2d_data.py` is gone, only DLL identity proves them.
- source_spec: `docs/implementation/spec-1-4n-g3d-data-strict-mixed.md`
  summary: Revisit HG2D's `U8` regions (`src/hg2d/hg2d_image.inc`: glyph bitmaps, byte ramps, tables marked "meaning not yet known") the way G3D's image was typed in slice 14: by structure (names, counts, dwords, words, instruction lines) rather than by width alone. G3D's remaining untyped regions (the scenery models +0x4f22..+0x64d2 and the tails after the two programs, written as words) wait for their meaning.
  evidence: slice 13 review finding 8; slice 14 left HG2D untouched by its frozen intent.
- source_spec: `docs/implementation/spec-1-4n-g3d-data-strict-mixed.md`
  summary: Prove that each `cset=` slot is installed at attach (the symbol-per-slot check deferred from slice 6 now covers G3D's 22 and HG2D's 40 `cset=` slots).
  evidence: slice 14 code review; `cset=` proves only that the slot is zero in the image, so deleting its assignment in C passes.
- source_spec: none
  summary: Parallel verification, after slice 15. First, give the break runners a worker pool, each worker with its own frozen tree copy, build directory and `WINEPREFIX` (starting with `host-breaks` and `main-breaks`). Then add `cmake_verify.py -j N`, running independent Baselines lines at once, each in its own tree, prefix and Xvfb display. Move the remaining wine steps off the shared `~/.wine`.
  evidence: Story 1.3's Baselines run (p13) took about 44 h serially on a 12-core, 78 GB machine with load around 1.8. Its longest steps are `host-breaks` (4.2 h), `options-breaks` (2.1 h), and three that hit the 2 h timeout (`main-breaks SUITE=situations`, `sim1r-breaks`, `g3d-breaks N=1000`). Docker is not needed: separate prefixes give the same isolation. Requested by the user on 2026-11-21.
- source_spec: `docs/implementation/spec-1-4o-main-data.md`
  summary: MAIN's `main_image.inc` types only the ImportTable, Linkage, month and season tables and the situation records by structure; the rest (menus, dialogs, strings and state, about 30 kB) is typed words marked "meaning not yet known". Describe them as the slices that read them settle their layout. The `tail=` window's retail-relocation allowance (the spec asked for none overlapping; retail's code there has 2,309) can tighten once the tail goes.
  evidence: slice 15 implementation.
- source_spec: `docs/implementation/spec-1-4o-main-data.md`
  summary: Prove that MAIN's 47 `cset=` Linkage slots are installed by `main_install()`, and that each module's compiled pointer arrays (`data_pointers`, `retail_linkage`) match the checked `.inc`. This extends slice 14's install-proof entry to MAIN.
  evidence: slice 15 code review; `cset=` checks only that the slots are zero in the image.
- source_spec: `docs/implementation/spec-1-4o-main-data.md`
  summary: Type more of `src/main/main_image.inc` by structure:
    - the G3D command blocks at +0x2386..+0x34fa, as `OP()` lines from the entry points in `main_natives.inc:770-822`, with a re-parse as G3D has;
    - the radio retuning records, using `radio_retune()`'s layout;
    - the clock and other dialog descriptors, using `main_clockdlg.inc` and `main_dialogs.inc`.

    Until then, relabel those regions "layout known, not yet typed".
  evidence: slice 15 code review. A linear parse from +0x2386 stops at once, so the block boundaries come from the entry table, not from a single walk.
- source_spec: `docs/implementation/spec-1-4p-host-data.md`
  summary: Check whether `gen_host_resources.py` (which reads `orig/FLTSIM98.EXE` in the `OPENFS98.EXE` build step) gives the same resources from another pressing. Add a pin or a pressing-independent check if it doesn't.
  evidence: slice 16 code review. `gen_host_data.py` used to stop the build on any EXE but the reference; now only `host-rt-tables-check` and the image headers name the reference sha256.
- source_spec: none
  summary: A break runner killed by `cmake_verify.py`'s timeout leaves its mutation in the tree. The timeout uses `killpg(…, SIGKILL)`, so no cleanup runs. Story 1.3's p13 kept `sim1r-breaks`' `>=` mutant in `src/sim1/sim1_small.c:843`, and every later SIM1 build in p13 used it (`sim1rdiff-run`, `sim1native-flights` and both smoke runs). Fix it with the parallel-verification work: each break runner mutates its own tree copy, never the tree it was started from. Until then, `cmake_verify.py` should send SIGTERM and wait before SIGKILL, the runners should restore on SIGTERM, and the verifier should check the tree is unchanged after each break step.
  evidence: found 2026-11-21 when re-running p13's `sim1rdiff-run`. It differed in `op_crash_count` (GLOBAL +0xb7c) because of the leftover mutant; the current tree gives 0 differ. Restoring the line in p13 was enough. `main`, `g3d`, `hg2d` and `controls` in p13 were clean.
- source_spec: none
  summary: Story 1.4 slice 18d: `tools/gen_window_data.py` (WINDOW's .text tail for WIN-2) stays a `gen:` build step, as the user chose in slice 6, until WIN-2 is fixed at porting.
  evidence: split from slice 18 on 2026-11-22; revisit only with the user.
- source_spec: `docs/implementation/spec-1-4u-mdl-data.md`
  summary: Story 1.4's "only resources from orig/ at build time" rests on self-declared `resource:` lines in `tools/raw_dumps_allowed.txt`; add an automated check that builds the converted modules against a copy of `orig/` with their `.data`/`.rdata` overwritten and asserts identical outputs (done by hand for the MDLs in slice 18c).
  evidence: 18c code review (Codex verification gap); `check_raw_dumps.py` checks only that a script is listed, not that it is right about what it reads.

- source_spec: `docs/implementation/spec-1-4-module-data-decompiled-no-raw-dumps-in-git.md`
  summary: `tools/ofs main-dialogs` is flaky: its control dump lists a dialog's child pages in z-order, so Display options' (0x1409) pages can come out in another order, and the Time of day combo (0x1301) differs when retail and ours start on either side of sunrise.
  evidence: 4 runs of the unchanged 1.4 tree on 2026-11-23/24 (~/openfs98-verify/p14-results, p14, md-bisect/): two failed on the page order in the cancel pass (same controls and text, one page block moved), one on Time of day (retail 06:25 Night, ours 06:35 Day), one passed. A swapped-in 1.3 SIMSUI.DLL differed only in its export timestamp. `tools/drive/fsdialogs.c:588` lists children with EnumChildWindows (z-order). Fix: list a dialog's top-level child pages in a fixed order (e.g. by their first control's id), and mask the Time of day combo as the hour and minute edits are masked (`tools/main_dialogs.sh`).

- source_spec: `docs/implementation/spec-1-5j-multip-english-dialogs-retail-bugs-marked-allowlist-gone.md`
  summary: `tools/check_retail_bugs.py` comment parsing and file listing have gaps that predate slice 10: if `git ls-files` fails for any reason (e.g. a corrupt index) the checker silently falls back to a filesystem walk, which also counts git-ignored files; in `.S` files a spaced directive (`# define X "RETAIL BUG (kept): ID"`) is taken as a comment; `;` after an instruction is accepted as a marker comment (the self-test relies on it); a backslash-newline inside `*/` is not followed; an empty register with no markers passes with `0 entries`; a malformed duplicate marker passes when a valid one exists elsewhere.
  evidence: Slice 10 code review (Codex gpt-6.2-sol, blind and edge-case layers, 2026-11-24). None affects the current tree: the check passes on 331 entries.

## Deferred from: Story 1.7 (the module ABI headers are 32-bit, 2026-11-24)

- source_spec: `docs/implementation/spec-1-7-the-module-abi-headers-are-32-bit.md`
  summary: `FS98_W` (the wide-literal macro) is not in `abi.h`: `u""` literals need C11 and the project builds `-std=c99`. Only `FS98_WCHAR` is added; the literal API belongs to the Win32 headers' story.
  evidence: spec Boundaries, `abi.h`.

- source_spec: `docs/implementation/spec-1-7-the-module-abi-headers-are-32-bit.md`
  summary: Not converted, and listed here for the stories that migrate them: `host_runtime.h`, `keep.h`, the rest of `numeric.h`, and modules' private struct copies (FE's harness `struct fs98_import` in `tools/difftest/fediff.c`, SLEW's private UTIL typedefs and export layouts, GLOBAL's private export layout, SIM1's `uintptr_t linkage[15]`). SIM1's embedded `fs98_import` array does change through the header. MinGW i686 accepts `(fs98_code)(uintptr_t)func` in a static initialiser and x86-64 GCC rejects it, so those tables need another form at migration.
  evidence: spec Boundaries, "The other users" and "Never".

- source_spec: `docs/implementation/spec-1-7-the-module-abi-headers-are-32-bit.md`
  summary: The retail-only bridge (`fs98_host_<name>_call`, `FS98_HOST_SLOT`) and the direct `(uint8_t *)(uintptr_t)x.link` / `(fs98_addr)(uintptr_t)p` casts at converted fields hold only for retail address words; native handle registration (AD-17) and `fs98_call` are Story 1.8's, and the bridge is removed module by module there.
  evidence: spec Boundaries, "The retail bridge".

- source_spec: `docs/implementation/spec-1-8-every-module-uses-the-shared-arena-h.md`
  summary: Remove Story 1.7's retail-only bridge in a story of its own, not in Story 1.8 (user's decision, 2026-11-24; this supersedes the 1.7 entry above that says 1.8 removes it). That means the `fs98_host_<name>_call` types, `FS98_HOST_SLOT`, the `fs98_import` union's pointer view with `FS98_LINK()`, `FS98_RETAIL_PTR`, and the modules' local `CALL` slot loads. The 365 `FS98_HOST_*` call lines in 14 files must pass `fs98_handle()` addresses and call through `fs98_call`, or through a native dispatch once AD-17's handles exist. It is needed before Epic 3's native i386 build.
  evidence: Story 1.8 spec review; the user chose to defer it because the rewrite is story-sized and would make more 1.8 slices change bytes.

- source_spec: `docs/implementation/spec-1-8-every-module-uses-the-shared-arena-h.md`
  summary: The arena lint (`tools/check_arena.py`) covers `src/` only, by the user's decision. The difftest harnesses in `tools/` keep their 1,293 `(uint32_t)(uintptr_t)` casts, since they run under wine on real host pointers. If Epic 3 runs harnesses natively against the arena, they need their own conversion.
  evidence: Story 1.8 scope question, 2026-11-24.

- source_spec: `docs/implementation/spec-1-8b-controls-panels-gauges-use-arena-h.md`
  summary: CONTROLS, PANELS and the gauges keep address-to-pointer and callback conversions (`(uint8_t *)(uintptr_t)`, `(const char *)(uintptr_t)(D + ...)`, `(fn *)(uintptr_t)` slot and callback loads, the gauges' `(panel_one)(uintptr_t)panel_service(...)` and `GAUGE_PANELS`), Win32 handle conversions and the `CALL`/`SVC`/`COM` slot loads. They hold only on i386 and are for the story that needs them off i386 (with the bridge removal above). The reverse-cast inventory is 43 after this slice (52 before).
  evidence: Story 1.8 slice 2 spec, "Reverse conversions".

- source_spec: `docs/implementation/spec-1-8b-controls-panels-gauges-use-arena-h.md`
  summary: The panels and gauges break runners' verdicts cannot supply the spec's acceptance fallback ("caught means the harness's mismatch exit with N differ non-zero, never a crash, timeout or guard exit") as written. `panels_breaks.py` counts a watchdog hang (exit 4) and a timeout as caught, and its unit mode runs 400 vectors whatever N is; `gauges_breaks.py` (warning and suction) runs a fixed 100. A later slice that needs a mutant's verdict compared at N=20000 must first give these runners the strict test and an N that reaches the harness. Slice 2 did not need the fallback: every output and mutant was byte-identical.
  evidence: Story 1.8 slice 2 review.

- source_spec: `docs/implementation/spec-1-8c-window-winmsg-mpchat-use-arena-h.md`
  summary: WINDOW, WINMSG and MPCHAT keep the same kind of retained reverse conversions as slice 2's entry: address-to-pointer and callback conversions (WINDOW's `(fn *)(uintptr_t)` slot loads in `CALL`, the `paint_fn` call and the call through `eax` in the eye code; MPCHAT's `WND(a)` Win32 handle conversion and its `HINSTANCE` casts; WINMSG's `CALL` and `CharNextA` operands), plus the `CALL` slot loads. Only the private access macros (8) and MPCHAT's `PTR`/`STR` bodies (2, now `fs98_ptr`) left the inventory: 43 to 33.
  evidence: Story 1.8 slice 3 spec, "Reverse conversions".

- source_spec: `docs/implementation/spec-1-8c-window-winmsg-mpchat-use-arena-h.md`
  summary: Pre-existing runner defects found and fixed in this slice: `winmsg_breaks.py` never ran `word-empty`, `split-fits` and `close-title` (their anchors sat across retail-bug comments, 61 of 64 ran), and `mpchat_ui_breaks.py` did not build since Story 1.7 (it copied `linkage.h` and `host.h` but not the `abi.h` they include). The WINMSG and MPCHAT UI break counts in `docs/tools.md` and the module docs predate this and were not re-run.
  evidence: Story 1.8 slice 3, runner `--build-only` work.

- source_spec: `docs/implementation/spec-1-8d-host-uses-arena-h.md`
  summary: HOST keeps its address-to-pointer conversions (`(T *)(uintptr_t)` in `host.c`, including `host.c:1242`, the Win32 handle and callback casts) and the cursor's outer `(uint8_t *)(uintptr_t)(...)` in `host_rt_cursor.c`. Only `host.c`'s private `M8`/`M16`/`M32` left the inventory: 33 to 30.
  evidence: Story 1.8 slice 4 spec, "Reverse conversions".

- source_spec: `docs/implementation/spec-1-8d-host-uses-arena-h.md`
  summary: Pre-existing runner defect fixed on both sides: `host_breaks.py` never ran `ab-ret`, `sd-stop`, `vw-rows-skip`, `vw-block-grow`, `tx-font0`, `tt-right` and `tt-dup` (retail-bug comments sit inside their anchors; 498 of 505 ran). The 505 count in `docs/tools.md` predates the repair.
  evidence: Story 1.8 slice 4, runner anchor work.

- source_spec: `docs/implementation/spec-1-4-module-data-decompiled-no-raw-dumps-in-git.md`
  summary: Two harnesses differ from retail, both already before Story 1.x: `g3ddiff-meshc-run` (G3D with the portable mesh compiler, `FS98_PORTABLE_FP`) 43 of 20000 scenarios, some polygons emitted as `line(...)` where retail emits `v(...)` (e.g. scenario 747, op 14); `gauges-panels-run N=1000` (PANELS_STRICT, retail/rebuilt warning and suction gauges) 143 of 1000, heap block hashes and full state (first scenarios 6, 20, 28). Not investigated.
  evidence: ~/openfs98-verify/p14-final/g3ddiff-meshc-run.log, p14-bisect/new-panels-1000.log; the same scenario lists from 27371fb in p14-bisect/old-meshc-b.log and old-panels-1000b.log (tree ~/openfs98-verify/p14-old).

- source_spec: `docs/implementation/spec-1-8f-main-uses-arena-h.md`
  summary: Story 1.8 slice 6b: FLIGHTS, MPDIALOG and WORLD convert to arena.h (12 casts; their DLL recipes, difftests and break runners need `-Iinclude`, the runners `--build-only`, and FLIGHTS' anchors that hold the literal cast are repaired).
  evidence: slice 6 (MAIN + English dialogs) split into four at its Checkpoint 1 (2026-11-25): about 15 recipes and 12 runners need work, too much for one spec.

- source_spec: `docs/implementation/spec-1-8f-main-uses-arena-h.md`
  summary: Story 1.8 slice 6c: OPTIONS converts to arena.h (27 casts across six files; `options_breaks.py` needs `-Iinclude`, `--build-only` and its 11 anchor lines that hold the literal cast).
  evidence: as above, split from slice 6 on 2026-11-25.

- source_spec: `docs/implementation/spec-1-8f-main-uses-arena-h.md`
  summary: Story 1.8 slice 6d: SIMSUI converts to arena.h (13 casts across five files; its per-unit, native and image compiles, `simsuidiff` and `simsui_breaks.py` need `-Iinclude`, the runner `--build-only`, and anchor `dib-valid-low`).
  evidence: as above, split from slice 6 on 2026-11-25.

- source_spec: `docs/implementation/spec-1-8f-main-uses-arena-h.md`
  summary: Pre-existing defect, repaired in slice 6a: MAIN break anchors `session/create-fail` (`main_kneeboard.inc`) and `spot/main-cmp1` (`main_spot.inc`) matched nothing at HEAD, because Story 1.5 put a RETAIL BUG and a `Not reproduced` comment inside them; their `main_breaks.py` counts since then never ran them. The anchors now take in the comment; the mutations are unchanged.
  evidence: `--build-only` on the HEAD tree without the repair reports both as not unique; slice 4 repaired its anchors the same way.

- source_spec: `docs/implementation/spec-1-10a-divide-helpers-match-the-host-filter.md`
  summary: Story 1.10b: the six modules whose inline `div`/`idiv` asm relies on the trap move onto `include/fs98/divide.h`: FE (`fe.c:254,262,277`), DYNAMIC (`dynamic.c:183,193,202`), PANELS (`panels.c:75,87,96,105`), BOOSTER (`booster.c:788,797`), ADVDRV (`advdrv.c:571`), plus DEMO's deliberate fault (`demo.c:2256` `fault_div`, DEMO-10, a design decision). The proof is their difftests under wine, after checking each harness's repair model against the filter.
  evidence: Story 1.10 split at its Checkpoint 1 (2026-11-25) into a foundation and two waves; asm becoming C changes code by construction, so these need behavioural proof.

- source_spec: `docs/implementation/spec-1-10a-divide-helpers-match-the-host-filter.md`
  summary: Story 1.10c: the six modules with private C saturating divides move onto `include/fs98/divide.h`: SIM1 (`sim1_internal.h:53-100`, about 25 uses), HG2D (7 `.inc` sites, keep `hg2d_affine`'s `*2`), G3D, WINDOW (WIN-1), CONTROLS and MAIN (MAIN-10 sites). The proof is byte identity where the code is unchanged and difftests elsewhere; break anchors inside replaced helpers are repaired.
  evidence: as above, split on 2026-11-25.

- source_spec: `docs/implementation/spec-1-10a-divide-helpers-match-the-host-filter.md`
  summary: The divide filter (`0x4119a0`, ours in `src/host/runtime/host_rt_divide.c`) reads a narrow (byte or word) memory divisor as a whole dword, so a divisor at the end of a readable page makes the filter itself fault; this is not in `docs/retail-bugs.md` and dividediff pads it away.
  evidence: Codex review of 1.10a (blind layer); the filter's read at about `host_rt_divide.c:111`; settle by a guard-page case against retail's filter, then log it as a retail bug if both fault.

- source_spec: `docs/implementation/spec-1-4z-two-harness-differences-fixed.md`
  summary: Fix the two pre-existing 1.4 harness differences, both probably harness artifacts. `g3ddiff-meshc-run` (43/20000): `g3ddiff.c` never matches `g3d_mesh_precision` (24 since `48756cd`) to retail's 53-bit default. `gauges-panels-run` (143/1000): `gauge_pointer()` does not normalise pointers into a rebuilt gauge's image outside `.data` (different `.rsrc` RVA and image base). The draft spec is ready for Checkpoint 1.
  evidence: read-only analysis 2026-11-25; `48756cd` is an ancestor of `27371fb` (same scenario lists); `all-gauges-panels` (which normalises those words) gave 0 differ in ~/openfs98-verify/p14-final. Each needs one confirming wine run (in the spec) when the wine lock is free.

- source_spec: `docs/implementation/spec-cleanup-1-deps-retail-ids-norm-pe.md`
  summary: Nothing re-checks the new rebuild edges automatically: a recurring incremental-build check is wanted (touch `tools/data_images.txt`, `advdrv_relocs.inc`, `fe_library.c`, a gauges input; delete a generated header; a wrong pinned hash; assert the exact recipe sets and a lint-only second build).
  evidence: the touch, delete and bad-hash proofs were one-off runs (the spec's Implementation Notes); removing the `data_images.txt` edge from `ofs_step` would pass every recurring check.
