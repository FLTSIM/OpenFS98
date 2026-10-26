# World.DLL — next-work handoff

**Not reimplemented.** This is a starting guide, not a recovered UI behavior
specification. The static registry is complete; some simulator-side descriptor
contracts are verified. No `src/world/`, `tools/world.mk`, World UI harness or
replacement DLL exists yet.

Reference: `orig/English/World.DLL`, preferred base `0x223f0000`, module id
`0x80000006`, 19 registrations. See [dialogs.md](dialogs.md#worlddll) and
[dialog_registry.tsv](../dialog_registry.tsv) for the complete inventory.
Read [the continuation rules](dialogs.md#starting-a-remaining-dialog-in-a-fresh-context)
before choosing a slice.

## Recommended first slice: add/edit scenery area

Two factories share template `0x6b3`:

| Registry id | Factory VA | FE descriptor |
| --- | --- | --- |
| `0x13080003`, add area | `0x223f19f0` | FE `.data+0x3170` |
| `0x13080004`, edit area | `0x223f1920` | FE `.data+0x3138` |

The verified owner is `src/fe/fe_library.c`, not MAIN. Start with
[FE's Scenery Library contract](fe.md#part-4--the-scenery-library-0x78-verified)
and `tools/difftest/fesldiff.c`, especially `world_area`, `fesl_dialog` and
`fesl_call`. The latter is a stand-in for World; its passing results verify
FE's callbacks, not World's controls or event ordering.

The existing fixture identifies descriptor `+4` as the check callback, `+8`
as title, `+0xc` as the row, `+0x10` as position status, `+0x14` as position
and `+0x2c` as count. FE's 0x40-byte area row exposes path at `+4`, title at
`+0x18`, layer at `+0x2c`, active at `+0x34` and protected at `+0x3c`.
These are owner-side starting facts; recover actual World accesses, capacities,
validation order and callback prototypes from both binaries before coding.

Keep the first slice to these two registrations and their shared class,
control exchange, validation, accept/cancel and lifetimes. Use independent
callback/file fixtures. Broader scenery lists, priority, drive search and
booster dialogs can follow in the same standalone harness. A two-registration
test DLL is a partial slice, not a complete World replacement.

## Other bounded slices

FACIL's exact-position dialog `0x1308` uses template `0x70c`, factory
`0x223f1c60`. Its owner contract is in [FACIL's dialogs](facil.md#the-dialogs),
`src/facil/facil.c` and `tools/difftest/facildiff.c` (`world_position`). FACIL's
validator/apply/revert/reload callbacks are established; World's numeric-edit,
error-display and nested facilities-dialog behavior still needs recovery.
Do not execute real placement callbacks against the active game while building
UI fixtures: FACIL's apply path reaches MAIN, CHAIN and the aircraft simulator.

The facility directory `0x1307` and go-to-airport `0x1309` have richer list and
nested-dialog contracts in the same owner/harness. Weather, time, scenery
complexity and other MAIN-driven descriptors need separate caller recovery;
their inventory entries are not behavior specifications.

## First concrete work

Generate the dialog-map JSON, then disassemble the chosen factories, class
vtable/message-map getters, constructors/destructors, DDX and handlers. Record
exact object sizes, offsets, callback argument counts and nested registrations.
Create `src/world/`, `tools/world.mk` and module-specific test tools/output only.
Use the completed Help/Sim work as the C core/native adapter pattern. Keep FE,
FACIL, MAIN, HOST, their verified harnesses and the shared Makefile unchanged.
