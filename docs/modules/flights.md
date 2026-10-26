# Flights.DLL — next-work handoff

**Not reimplemented; caller/descriptor recovery is the first task.** The static
inventory identifies 13 registrations and 17 templates, but it is not a UI
behavior specification. No `src/flights/`, `tools/flights.mk`, differential
harness or replacement DLL exists yet.

Reference: `orig/English/Flights.DLL`, preferred base `0x22310000`, module id
`0x80000002`. Read [the inventory](dialogs.md#flightsdll),
[the registry TSV](../dialog_registry.tsv) and
[the continuation rules](dialogs.md#starting-a-remaining-dialog-in-a-fresh-context).

This DLL covers create/select/save flight, adventures/challenges, aircraft
selection, navigation/radios, communications, autopilot and opening/learning
menus. The 13 factories span `0x22311130` through `0x22311800`. Registry id `1`
is the exceptional modeless registration (flags 4); its template `0x20a` and
factory `0x223116e0` are separate from the flight-menu factories.

## Why defer it while MAIN is active

Many natural starting points are flight/aircraft/menu orchestration, where
MAIN owns simulator-side behavior. It is technically possible to recover the
retail descriptors and use independent fixtures without editing MAIN, but the
repository does not yet have the same bounded UI handoff as FE's scenery
contracts, CONTROLS' settings pages or MULTIP's dialog callbacks. World and
MP_DIALOG therefore have clearer independent starting slices.

## First concrete work when selected

Run `tools/dialogmap.py --callers --format json` and trace the selected
registration's retail callers, including MAIN's helper-mediated calls that the
static caller scan misses. Recover descriptor allocation, callback prototypes,
shared GLOBAL accesses and accept/cancel ownership. Check prototypes at the
callee; do not infer them from pushes near a call or from resource labels.
Record a per-family contract here before translating the UI.

Then recover the chosen class/page lifetimes, DDX, vtables/maps and handlers.
Use `src/flights/`, standalone `tools/flights.mk`, a private output/prefix and
new fixtures. Keep MAIN/HOST and their verified harnesses unchanged. The
modeless id-1 lifetime and nested opening menus need explicit acceptance;
a successful startup or one translated selection form is not whole-DLL
completion.
