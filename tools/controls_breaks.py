#!/usr/bin/env python3
"""Break OpenFS98's CONTROLS on purpose, one place at a time, and check that
controlsdiff notices.

Each break is a small edit to a copy of src/controls/controls.c -- the kind
of slip a transliteration of the command dispatcher and its handlers makes:
a boundary, an offset, a clamp, a dropped case.  The copy is built as the
harness's CONTROLS_ours.dll (with the unchanged controls_input.c) and run for
N scenarios; controlsdiff exits non-zero when any differ.  The real build goes
back at the end.  A missed break is a gap in the harness or an equivalence;
either way it is reported.

Usage: tools/controls_breaks.py [N] [name...]   (from the repository root,
       after make controlsdiff)
"""
import os
import subprocess
import sys

N = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 2000
ONLY = [a for a in sys.argv[1:] if not a.isdigit()]
SRC = "src/controls/controls.c"
TMP = "bin/difftest/controls_break.c"

# (name, old, new[, why it cannot show]).  A fourth element marks a break
# proven equivalent (or out of the dispatcher harness's reach), reported so.
BREAKS = [
    # the dispatcher
    ("select-decr", "        if ((eax & 3) == 0)\n            V8(S_SELECT) >>= 1;",
     "        if ((eax & 3) != 0)\n            V8(S_SELECT) >>= 1;"),
    ("crash-filter", "            eax != 0x100c4 && eax != 0x100c5 && eax != 0x1008a &&",
     "            eax != 0x100c4 && eax != 0x1008a &&"),
    ("index-range", "    if (eax >= controls_count)", "    if (eax > controls_count)"),
    # brakes
    ("brakes-clamp", "    SET_CL(0xc8);", "    SET_CL(0xc7);"),
    # the transponder / sim rate
    ("sim-rate-floor", "    if (AX > 0x40) {", "    if (AX >= 0x40) {"),
    # the OBS wrap
    ("obs-wrap", "    if ((int16_t)GW(at) >= 0x168)", "    if ((int16_t)GW(at) > 0x168)"),
    # the engines
    ("throttle-floor", "        } else if ((int16_t)AX < (int16_t)GW(0xb00)) {",
     "        } else if ((int16_t)AX <= (int16_t)GW(0xb00)) {"),
    ("engine-count", "    if (CL >= AL) {", "    if (CL > AL) {"),
    ("engine-or", "    GD(0x888) |= eax;", "    GD(0x888) = eax;"),
    # flaps
    ("flaps-step", "    V16(S_STEP) = AX;", "    V16(S_STEP) = AX + 1;"),
    # kohlsman
    ("kohlsman-step", "    GW(0x330) += 5;", "    GW(0x330) += 4;"),
    # the keyboard axis curve (reached through +0x28..+0x34)
    ("curve-clamp", "    imul16((uint16_t)w);", "    imul16((uint16_t)w + 1);"),
]

if not os.path.exists("bin/difftest/controlsdiff.exe"):
    print("run `make controlsdiff` first"); sys.exit(2)
src = open(SRC).read()
caught = missed = 0
for b in BREAKS:
    name, old, new = b[0], b[1], b[2]
    note = b[3] if len(b) > 3 else None
    if ONLY and name not in ONLY:
        continue
    if src.count(old) != 1:
        print("%-16s SKIP (anchor appears %d times)" % (name, src.count(old))); continue
    open(TMP, "w").write(src.replace(old, new, 1))
    os.remove("bin/difftest/CONTROLS_ours.dll") if os.path.exists("bin/difftest/CONTROLS_ours.dll") else None
    r = subprocess.run(["make", "controls-break-build"], capture_output=True, text=True)
    if not os.path.exists("bin/difftest/CONTROLS_ours.dll"):
        print("%-16s BUILD FAILED\n%s" % (name, r.stderr[-800:])); continue
    run = subprocess.run(["wine", "controlsdiff.exe", str(N)], cwd="bin/difftest",
                         env=dict(os.environ, WINEDEBUG="-all"), capture_output=True, text=True)
    differ = run.returncode != 0
    if note:
        print("%-16s %s (%s)" % (name, "caught" if differ else "not shown", note))
    elif differ:
        print("%-16s caught" % name); caught += 1
    else:
        print("%-16s MISSED" % name); missed += 1

# restore the real build
subprocess.run(["make", "controlsdiff"], capture_output=True)
print("\n%d caught, %d missed" % (caught, missed))
sys.exit(1 if missed else 0)
