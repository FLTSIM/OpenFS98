# Options.DLL — next-work handoff

**Not reimplemented.** The registry and template/message-map inventory exist;
CONTROLS, DEMO and ANALYSIS have verified simulator-side descriptor contracts.
The Options UI itself has no implementation or acceptance harness yet. There
is no `src/options/` or `tools/options.mk`.

Reference: `orig/English/Options.DLL`, preferred base `0x22390000`, module id
`0x80000007`, 15 registrations and 31 templates. Start with
[the inventory](dialogs.md#optionsdll), [the registry TSV](../dialog_registry.tsv)
and [the continuation rules](dialogs.md#starting-a-remaining-dialog-in-a-fresh-context).

## Preferred independent family: customise controls

Registry `0x140a`, factory `0x22391930`, container template `0x3a5`; child
resources `0x3a3`, `0x3a4`, `0x3a7` cover controls, assignments and force feedback.
This is a multi-page family, not a single-form shortcut.

Read [CONTROLS' key-assignment contract](controls.md#the-key-assignment-dialog-0x58-text0x7e400x8e5f-0x9130),
`src/controls/controls_input.c` and `tools/difftest/controlsdiff.c`, especially
`h_run_dialog` and its adjacent dialog stand-in. The descriptor is built at
CONTROLS `D+0x240`; callback `+0x1c` resets sensitivity and `+0x20` recounts
joysticks. Callback `+0x2c` is tested by the owner harness but is not called by
retail Options. Preserve the actual UI call boundary rather than adding it.

The existing stand-in covers shared lists, conflicting bindings, edit abandon,
keyboard/joystick assignments, sensitivity, device enablement and force feedback.
It verifies CONTROLS, not Options' page construction, focus/key capture, widget
behavior, validation or nested lifetimes. Recover those from Options and build
independent device fixtures; no changes to CONTROLS or its harness are needed.

## Smaller alternative: replay/video or analysis slices

Instant replay `0x1403` has factory `0x22391130`, template `0x391`, and owner
DEMO `+0x30`. Other video dialogs and ANALYSIS' settings/graphs also have owner
contracts. Read [demo.md](demo.md), [analysis.md](analysis.md),
`tools/difftest/demodiff.c` and `tools/difftest/analysisdiff.c` before selecting
one. Such a slice can use descriptor/graph fixtures independently of MAIN.

Resolve a terminology inconsistency before starting the video family:
`demo.md` calls `0x14040001` the finished dialog, while the static UI inventory
labels its template `0x3a1` Save video and labels `0x14040003` Video finished.
The registry IDs, factory/template associations and actual caller/callee
behavior must decide the implementation; neither prose label is a contract.
Logbook, preferences and display options require their own caller/descriptor
recovery and are less convenient while MAIN is active.

## First concrete work

Choose one family explicitly, generate the dialog-map JSON and recover its
factories, page graph, class sizes, vtables, DDX, maps, help tables and handlers.
Create only `src/options/`, standalone `tools/options.mk` and Options-specific
harnesses/output/prefix. Follow Help/Sim's core then native acceptance pattern,
including deliberate mutations and exception cleanup. A partial family is not
a complete 15-registration Options DLL and must not be installed as one.
