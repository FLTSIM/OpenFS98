#!/usr/bin/env python3
"""Compare what each Linkage function leaves in the registers between two game runs.

    tools/reg_contracts.py retail/regs.txt ours/regs.txt [more pairs...] [--module NAME]

Written by FRAMELOCK_REGS (tools/drive/framelock.c).  Run with one of our modules alone in a
retail game the callers are retail's, so the same call must leave the same thing:

  BREAKS   retail kept the register on every call and ours did not.  Retail's assembly callers
           are entitled to rely on this, as MAIN relies on UTIL's +0x60 for cx.
  LEAVES   retail changes the register, and ours came back with different values.  For eax it
           may be a result; otherwise a leftover that matters only if a caller reads it.  A
           pointer into the module's own data differs without meaning anything.
  x87      the depth of the x87 stack changed differently.
  CARRY    the carry flag came back differently; some assembly services return it.

Both are only meaningful while the run has not diverged, which tools/reg_contracts.sh reports.
Registers holding unrelated leftovers reach a function without mattering, so differing input
registers are not held against it.
"""
import collections
import sys

NAMES = ("eax", "ecx", "edx", "ebx", "esi", "edi", "ebp", "x87", "carry")

# What follows these offsets is not services: DYNAMIC's interpreter dispatches through it.
INTERNAL_FROM = {"DYNAMIC": 0x20}


def read(path):
    table = {}
    for line in open(path):
        field = line.split()
        if len(field) != 4 + 2 * len(NAMES):
            continue
        table[(field[0], int(field[1], 16))] = {
            "calls": int(field[2], 16),
            "in": field[3],
            "out": field[4::2],
            "kept": [int(k, 16) for k in field[5::2]],
        }
    return table


def main():
    args = sys.argv[1:]
    module = None
    if "--module" in args:
        at = args.index("--module")
        module = args[at + 1].upper()
        del args[at:at + 2]
    found = collections.defaultdict(set)
    calls = collections.Counter()
    for retail_path, ours_path in zip(args[0::2], args[1::2]):
        retail, ours = read(retail_path), read(ours_path)
        for key, r in retail.items():
            if module and key[0] != module:
                continue
            if key[1] >= INTERNAL_FROM.get(key[0], 1 << 30):
                continue
            o = ours.get(key)
            if o is None:
                found[key].add("not called in ours")
                continue
            calls[key] += r["calls"]
            for i, name in enumerate(NAMES):
                if i < 7 and r["kept"][i] == r["calls"] and o["kept"][i] != o["calls"]:
                    found[key].add("BREAKS " + name)
                if r["out"][i] != o["out"][i]:
                    if name in ("x87", "carry"):
                        found[key].add(name.upper())
                    elif not (i < 7 and r["kept"][i] == r["calls"]):
                        found[key].add("LEAVES " + name)
    for key in sorted(found):
        order = sorted(found[key], key=lambda s: (not s.startswith("BREAKS"), s))
        print("%s +0x%02x (%d calls): %s" % (key[0], key[1], calls[key], ", ".join(order)))
    print("%d of %d slots called have a finding" % (len(found), len(calls) + len([k for k in found if k not in calls])))
    return 1 if any(s.startswith("BREAKS") for v in found.values() for s in v) else 0


if __name__ == "__main__":
    sys.exit(main())
