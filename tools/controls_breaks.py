#!/usr/bin/env python3
"""Break OpenFS98's CONTROLS on purpose, one place at a time, and check that
controlsdiff notices.

Each break is a small edit to a copy of src/controls/controls.c (BREAKS, the
command dispatcher and its handlers) or src/controls/controls_input.c
(INPUT_BREAKS, the joystick poll) -- the kind of slip a transliteration
makes: a boundary, an offset, a clamp, a dropped case.  The copies, one
broken and the other unchanged, are built as the harness's CONTROLS_ours.dll
and run for N scenarios; controlsdiff exits non-zero when any differ.  The
real build goes back at the end.  A missed break is a gap in the harness or
an equivalence; either way it is reported.

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
INPUT = "src/controls/controls_input.c"
INPUT_TMP = "bin/difftest/controls_input_break.c"

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

# The same, in controls_input.c: the per-frame joystick poll.
INPUT_BREAKS = [
    # the bug the listing found: the repeat counter cleared on every poll,
    # not only while the button is up (.text+0x9078)
    ("joy-repeat-clear", "            M32(slot) = 0;\n        }\n    }",
     "        }\n        M32(slot) = 0;\n    }"),
    ("joy-repeat-held", "            M32(slot - 0x100) = 0;                   /* so it presses again */",
     ""),
    ("joy-countdown", "            M32(slot) = n - 1;", "            M32(slot) = n - 2;"),
    ("joy-release-guard", "                if (cmd != 0)\n                    HC(0x3c, f2)(cmd, 0);",
     "                HC(0x3c, f2)(cmd, 0);"),
    ("joy-pov-buttons", "    for (b = 0; b < 0x40; b++) {", "    for (b = 0; b < 0x20; b++) {"),
    ("joy-axis-change", "        if (cur != M32(D + 0x4b54 + (a + 0xe3 * joy) * 4)) {",
     "        if (1) {"),
    ("joy-lock", "    if (M32(G + 0x84c) != 0)", "    if (0)"),
    ("curve-quirk", "        return 0x4000;", "        return (uint32_t)-0x4000;"),
]

if not os.path.exists("bin/difftest/controlsdiff.exe"):
    print("run `make controlsdiff` first"); sys.exit(2)
src = open(SRC).read()
inp = open(INPUT).read()
caught = missed = 0
for b in [(SRC,) + t for t in BREAKS] + [(INPUT,) + t for t in INPUT_BREAKS]:
    path, name, old, new = b[0], b[1], b[2], b[3]
    note = b[4] if len(b) > 4 else None
    if ONLY and name not in ONLY:
        continue
    text = src if path == SRC else inp
    if text.count(old) != 1:
        print("%-18s SKIP (anchor appears %d times)" % (name, text.count(old))); continue
    open(TMP, "w").write(src.replace(old, new, 1) if path == SRC else src)
    open(INPUT_TMP, "w").write(inp.replace(old, new, 1) if path == INPUT else inp)
    os.remove("bin/difftest/CONTROLS_ours.dll") if os.path.exists("bin/difftest/CONTROLS_ours.dll") else None
    r = subprocess.run(["make", "controls-break-build"], capture_output=True, text=True)
    if not os.path.exists("bin/difftest/CONTROLS_ours.dll"):
        print("%-18s BUILD FAILED\n%s" % (name, r.stderr[-800:])); continue
    run = subprocess.run(["wine", "controlsdiff.exe", str(N)], cwd="bin/difftest",
                         env=dict(os.environ, WINEDEBUG="-all"), capture_output=True, text=True)
    differ = run.returncode != 0
    if note:
        print("%-18s %s (%s)" % (name, "caught" if differ else "not shown", note))
    elif differ:
        print("%-18s caught" % name); caught += 1
    else:
        print("%-18s MISSED" % name); missed += 1

# restore the real build
subprocess.run(["make", "controlsdiff"], capture_output=True)
print("\n%d caught, %d missed" % (caught, missed))
sys.exit(1 if missed else 0)
