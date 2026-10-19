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
     "        } else if ((int16_t)AX <= (int16_t)GW(0xb00)) {",
     "equivalent: at equality the floor is loaded over the same value (retail "
     ".text+0x5daa: cmp ax,[edi+0xb00] / jge / mov ax,[edi+0xb00], and the "
     "flags are dead -- the store, then shr cx,1)"),
    ("engine-count", "    if (CL >= AL) {", "    if (CL > AL) {"),
    ("engine-or", "    GD(0x888) |= eax;", "    GD(0x888) = eax;"),
    # flaps
    ("flaps-step", "    V16(S_STEP) = AX;", "    V16(S_STEP) = AX + 1;"),
    # kohlsman
    ("kohlsman-step", "    GW(0x330) += 5;", "    GW(0x330) += 4;"),
    # the keyboard axis curve (reached through +0x28..+0x34)
    ("curve-clamp", "    imul16((uint16_t)w);", "    imul16((uint16_t)w + 1);"),
    # the CHAIN callbacks: list 0xb's command entry, list 2's frames, list 3
    ("cmd-range", "    if (eax >= 0x10000 && eax <= 0x1dfff) {",
     "    if (eax >= 0x10000 && eax < 0x1dfff) {"),
    ("taken", "    if (taken == 0) {", "    if (1) {"),
    ("frame-brakes", "        V8(S_FRAMES) = 6;", "        V8(S_FRAMES) = 5;"),
    ("brake-release", "            SET_AX(AX - 0x32);", "            SET_AX(AX - 0x31);"),
    ("coord-shift", "            SET_AX((int16_t)AX >> 6);", "            SET_AX((int16_t)AX >> 5);"),
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
    # the joystick configs: the watcher, the readers, the defaults, the saver
    ("watch-new", "    if (i < n) {", "    if (i + 1 < n) {"),
    ("watch-prefs", "controls_joystick(1, on == 0xffffffff ? 1 : on);",
     "controls_joystick(1, on == 0xffffffff ? 0 : on);"),
    ("watch-countdown", "            V32(0x43c)--;", "            V32(0x43c) -= 2;"),
    ("read-locked", "        HC(0xa8, f3)(section, D + 0x468, table + 0);        /* LOCKED */",
     "        HC(0xa8, f3)(section, D + 0x460, table + 0);        /* LOCKED */"),
    ("read-name", "            M32(out) = CMD(i).command;", "            M32(out) = CMD(i).command + 1;"),
    ("read-exists", "        if (HC(0x158, f2)(P(file), 0x24) == 0)",
     "        if (HC(0x158, f2)(P(file), 0x24) != 0)"),
    ("parse-neg", "    if (neg && negative)", "    if (neg)"),
    ("parse-bad", "    if (bad)\n        v = 0;", "    if (0)\n        v = 0;"),
    ("default-slew", 'h_strcat(buf, D + 0x3f8);                           /* " SLEW" */', ""),
    ("default-type", "        M32(blk + 4) = desc.id;", "        M32(blk + 4) = desc.id + 1;"),
    ("install-locked", "    if (M32(dst) == 0) {", "    if (1) {"),
    ("defaults-current", "    if (M32(V32(0x350) + joy * 0x3cc) != 0)",
     "    if (M32(D + 0xd48 + joy * 0x3cc) != 0)"),
    ("save-unset", "            if (M32(e + 0xc + k * 4) == 0)\n                continue;",
     "            if (0)\n                continue;"),
    ("save-repeat", "                HC(0xac, f3)(s, P(k78), M32(e + 0x1cc + k * 4));",
     "                HC(0xac, f3)(s, P(k78), M32(e + 0xcc + k * 4));"),
    ("write-nameless", "        if (CMD(i).command == value)",
     "        if (CMD(i).command == value && CMD(i).name)"),
    ("section-digits", "        NUMF(0x18, f3)(P(&joy), 0xb, s + h_strlen(s));",
     "        NUMF(0x18, f3)(P(&joy), 0xc, s + h_strlen(s));"),
    # the key bindings: the three faults the first transliteration had, put back
    ("layout-return", "        HC(0xd8, f6)(P(layout), P(CMD(i).name), P(parsed), 2, P(dir), 0x24);\n"
     "        ent[0] = parsed[0];\n        ent[1] = parsed[1];\n        ent[2] = CMD(i).command;\n"
     "        if (ent[0] == 0)\n            continue;\n        HC(0x1f4, f3)(*pAccel, P(ent), 1);\n"
     "    }\n    return got;",
     "        HC(0xd8, f6)(P(layout), P(CMD(i).name), P(parsed), 2, P(dir), 0x24);\n"
     "        ent[0] = parsed[0];\n        ent[1] = parsed[1];\n        ent[2] = CMD(i).command;\n"
     "        if (ent[0] == 0)\n            continue;\n        HC(0x1f4, f3)(*pAccel, P(ent), 1);\n"
     "    }\n    return 0;"),
    ("layout-args", "HC(0xd8, f6)(P(layout), P(CMD(i).name), P(parsed), 2, P(dir), 0x24);",
     "HC(0xd8, f6)(P(layout), P(CMD(i).name), 2, P(parsed), P(dir), 0x24);"),
    ("list-first", "                pair[1] = buf[k * 3 + 0] + (buf[k * 3 + 1] << 8);",
     "            { pair[1] = buf[k * 3 + 0] + (buf[k * 3 + 1] << 8); break; }",
     "equivalent while no table gives a command two keys: neither default table "
     "(ENGLISH.FLL 0x67/0x68) does, a profile section names each command once, "
     "and the one id with several names, 0, is never bound"),
    ("accel-default", "        HC(0x1e4, f2)(D + 0x186, 0x67);", "        HC(0x1e4, f2)(D + 0x186, 0x68);"),
    # force feedback's settings and DirectInput loading
    ("ff-setting", "    HC(0xa8, f3)(D + 0x192, D + 0x850, D + 0x470);          /* force_master_enabled */",
     "    HC(0xa8, f3)(D + 0x192, D + 0x850, D + 0x474);          /* force_master_enabled */"),
    ("ff-dinput", "    if (h == 0)\n        return (uint32_t)-1;", "    if (h == 0)\n        return 0;"),
    # force feedback: the device, the effects, the stick force's x87, the dispatch
    ("stick-clamp", "    if (!(m < RD(0x2a0) || m == RD(0x2a0) || m != m))\n        m = RD(0x2a0);",
     "    if (!(m < RD(0x2a0) || m == RD(0x2a0) || m != m))\n        m = RD(0x2a0) - 1;"),
    ("stick-north", "    a = a - RD(0x290);", "    a = a + RD(0x290);"),
    ("bump-period", "    per[3] = dur * 4;", "    per[3] = dur * 2;"),
    ("periodic-phase", "    per[2] = kind == 2 ? 0x2328 : 0;", "    per[2] = kind == 1 ? 0x2328 : 0;"),
    ("constant-sample", "    eff[3] = 0x2710;", "    eff[3] = 0x2711;"),
    ("periodic-set", "    eff[12] = D + 0xbfa0;", "    eff[12] = D + 0xbfa4;"),
    ("reap-playing", "    if ((uint8_t)status & 1)                            /* DIEGES_PLAYING */",
     "    if ((uint8_t)status & 2)                            /* DIEGES_PLAYING */"),
    ("frame-pause", "    if (M16(G + 0x5dc) == 0 && controls_joystick(0, 0) != 0)",
     "    if (controls_joystick(0, 0) != 0)"),
    ("crash-second", "        V32(0xbf98) = 2;", "        V32(0xbf98) = 3;"),
    ("message-60", "    case 0x60:", "    case 0x61:"),
    ("dispatch-gate", "    if ((r = V32(0xbfbc)) == 0 || (r = V32(0xbff0)) != 0 || (r = V32(0xbff4)) != 0)",
     "    if ((r = V32(0xbfbc)) == 0 || (r = V32(0xbff0)) != 0)"),
    ("bump-x87", "    t = (long double)RD(0x270) - t;", "    t = t - (long double)RD(0x270);"),
    ("create-level", "                r = di_create(wnd, inst, D + 0xbfc0, D + 0xbfc4, 5);",
     "                r = di_create(wnd, inst, D + 0xbfc0, D + 0xbfc4, 4);"),
    ("enum-type", "    if (M8(ddi + 0x24) != 4)                            /* dwDevType: DIDEVTYPE_JOYSTICK */",
     "    if (M8(ddi + 0x24) != 3)                            /* dwDevType: DIDEVTYPE_JOYSTICK */"),
    ("init-slope", "    HC(0xe0, f5)(P(sect), D + 0x944, D + 0x4c0, P(path), 0x24);    /* ground_bumps_slope1 */",
     "    HC(0xe0, f5)(P(sect), D + 0x944, D + 0x4d0, P(path), 0x24);    /* ground_bumps_slope1 */"),
    ("release-unacquire", "        COM(M32(pdev), 0x20, f1)(M32(pdev));            /* Unacquire */", ""),
    ("crash-args", "            r = eff_constant(V32(0xbfc4), D + 0xbfe4, V32(0x4d8), V32(0x4dc), V32(0x4e0));",
     "            r = eff_constant(V32(0xbfc4), D + 0xbfe4, V32(0x4d8), V32(0x4e0), V32(0x4dc));"),
    ("bump-speed", "        speed = (double)(int64_t)(M32(blk + 8) >> 16);",
     "        speed = (double)(int64_t)(M32(blk + 8) >> 15);"),
    # the keyboard-assignments dialog: setup, the lists, the callbacks, apply
    ("keys-joyflag", "            M32(bind + 4) = (CMD(idx).kind & 3) != 0;",
     "            M32(bind + 4) = (CMD(idx).kind & 1) != 0;"),
    ("joy-type", "        M32(bind + k * 8 + 4) = (CMD(idx).kind & 3) != 0 ? 2 : 3;",
     "        M32(bind + k * 8 + 4) = (CMD(idx).kind & 3) != 0 ? 3 : 2;"),
    ("joy-button-base", "        M32(bind + at * 8) = a + 0x11;", "        M32(bind + at * 8) = a + 0x10;"),
    ("apply-repeat", "                M32(P(cfg) + 0x188 + v * 4) = M32(ext + k * 0xc + 8);",
     "                M32(P(cfg) + 0x288 + v * 4) = M32(ext + k * 0xc + 8);"),
    ("apply-flags", "                M32(P(cfg) + 8) |= 1u << (v - 1);", "                M32(P(cfg) + 8) |= 1u << v;"),
    ("apply-keyflags", "        ent[1] = spec >> 8;", "        ent[1] = (spec >> 8) & 7;"),
    ("records-swap", "    M16(G + 0xc0a) = M16(rec + 0x50);", "    M16(G + 0xc0a) = M16(rec + 0x60);"),
    ("records-read", "    M16(rec + 0x88) = M16(G + 0xc04);", "    M16(rec + 0x88) = M16(G + 0xc06);"),
    ("adjust-mouse", "        M16(block + 0x74) = 0x19;", "        M16(block + 0x74) = 0x18;"),
    ("adjust-scale", "            M32(M32(e + 0x24) + k * 0xc) = 0x64;",
     "            M32(M32(e + 0x24) + k * 0xc) = 0x63;"),
    ("refresh-flag", "    V32(0x250) = HC(0x1c0, f0)() == 0;", "    V32(0x250) = HC(0x1c0, f0)() != 0;"),
    ("reset-default", "                HC(0x1e4, f2)(P(&accel), 0x67);", "                HC(0x1e4, f2)(P(&accel), 0x68);"),
    ("setup-noff", "    V32(0x300) = ff[0] == 0;", "    V32(0x300) = ff[0] != 0;"),
    ("dialog-answer", "        if (r == 0 && V32(0x240) == 1)", "        if (r == 0 && V32(0x240) == 0)"),
    ("keyboard-name", "    h_strcpy(M32(devs + 8), D + 0x1d0);", "    h_strcpy(M32(devs + 8), D + 0x208);"),
    ("current-mask", "        M32(out + 8) = M32(P(state) + 8);", "        M32(out + 8) = M32(P(state) + 8) | 1;",
     "equivalent: joy_current writes the mask into build_list's stack copy of the config, "
     "which only fill_joy reads, and fill_joy reads the axis and button commands and their "
     "extras, never +8"),
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
                         env=dict(os.environ, WINEDEBUG="-all", CONTROLS_FIRST="1"),
                         capture_output=True, text=True)
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
