# MP_DIALOG.DLL — next-work handoff

**Not reimplemented.** This guide joins the static UI inventory to the verified
MULTIP callback owners. The UI's own validation, transitions and lifetimes still
need decompilation. No `src/mpdialog/`, `tools/mpdialog.mk`, native UI harness or
replacement DLL exists yet.

Reference input: `orig/English/mpdialog.dll`, preferred base `0x22340000`,
module id `0x8000000a`. Installed name: `English/MP_DIALOG.DLL`. This is distinct
from the implemented `Modules/mpchat.DLL` chat window. Read
[the continuation rules](dialogs.md#starting-a-remaining-dialog-in-a-fresh-context)
and [the static inventory](dialogs.md#mpdialogdll).

| Registry id | Factory VA | Template(s) | Flags |
| --- | --- | --- | --- |
| `0x110a`, connection flow | `0x22341130` | `0x68,0x67,0x69,0x66,0x65` | `2` |
| `0x110b`, session settings | `0x223411e0` | `0x6a` | `0` |
| `0x110c`, session details | `0x22341270` | `0x6b` | `0` |

With three registrations, seven templates and 25,401 text bytes, this is the
smallest remaining whole English DLL by code size. It is a good independent
whole-module candidate, but the connection flow is not one ordinary dialog.
Flag 2 means the factory returns a zero-terminated HWND array. Its five-window
creation order and initial command are recorded in
[the array-factory section](dialogs.md#the-multiplayer-array-factory).

## Starting boundary

Begin with the ordinary session-settings dialog `0x110b`; then `0x110c` for
refresh, player lists, capacity edits and removal confirmation; finally the
five-window `0x110a` flow. This order is a scope recommendation, not a claim
that the classes have no shared helpers.

The owner-side code is already implemented and verified:

- `src/multip/multip_options.inc`: `0x110b`, public `+0x1c`.
- `src/multip/multip_details.inc`: `0x110c`, refresh/removal/capacity callbacks.
- `src/multip/multip_configuration.inc` and `multip_browser.inc`: `0x110a`
  setup, validation, providers, session discovery and related callbacks.
- `src/multip/multip_lobby.inc` and `multip_toggle.inc`: connection orchestration.
- `tools/difftest/multip_dialogs.inc`, especially `dialog_engine`, and
  `tools/difftest/multip_lobby.inc`: existing callback/COM/clock/file fixtures.

See [MULTIP's lobby/dialog checkpoint](multip.md#lobby-and-configuration-dialog-checkpoint)
and its later [acceptance checkpoint](multip.md#whole-module-acceptance-checkpoint).
Earlier MULTIP checkpoints retain historical pending states; use the latest
acceptance and source for current completion status.

These tests stand in for MP_DIALOG. They establish owner callbacks and many
boundary cases, not real UI event order, control behavior or array-window
transitions. Confirm each callback's prototype at its callee before forwarding
it from C; the harness's numeric callback indices are not service ordinals.

## Isolation and acceptance

Create `src/mpdialog/`, `tools/mpdialog.mk`, its own harness, private Wine prefix
and ignored `bin/mpdialog-diff/`. Use scripted provider/session/player data,
clock and launch callbacks; no real DirectPlay session or external launch is
needed for native UI comparisons. Model the host's array-factory contract in
the new harness, rather than editing HOST to accommodate it. Keep MULTIP's
already-verified source/harness and MAIN/HOST/shared build files unchanged.

Recover and test class lifetimes, DDX, maps, callbacks and transitions before
claiming a whole replacement. Native acceptance must include all three public
factories, the complete five-window array and teardown, real controls,
deliberate mutations, repeated DLL unload and isolated startup. A settings-only
slice must be labeled partial and kept out of the live game install.
