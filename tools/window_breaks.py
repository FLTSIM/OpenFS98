#!/usr/bin/env python3
"""Break OpenFS98's WINDOW on purpose, one place at a time, and check that
windowdiff notices.

Each break is a small edit to a copy of src/window/window.c -- the kind of
slip a transliteration makes: a boundary, an offset, a flag, a dropped call.
The copy is built as the harness's WINDOW_ours.dll and run for N scenarios
(windowdiff exits non-zero when any differ).  At the end the real build goes
back.  A missed break is a gap in the harness or an equivalence; either way
it is reported, and docs/modules/window.md says which.

Usage: tools/window_breaks.py [N] [name...]       (from the repository root,
       after make windowdiff)
"""
import os
import subprocess
import sys

N = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 2000
ONLY = [a for a in sys.argv[1:] if not a.isdigit()]
SRC = "src/window/window.c"
TMP = "bin/difftest/window_break.c"

# (name, old, new[, why it cannot show]).  A fourth element marks a break
# proven equivalent (or unreachable through the host), reported as such.
BREAKS = [
    # the window list
    ("menu-id-top", "H_MENU_ADD(0x1005, (rec & 0xffff0000u) | M16(rec + R_ID),",
     "H_MENU_ADD(0x1005, M16(rec + R_ID),"),
    ("title-kept", "    H_FREE(M32(prec) + R_TITLE);\n", ""),
    ("gather-any", "if (g->type == 0) {", "if (g->type == 1) {"),
    ("next-wrap", "    if (i >= count)\n        i = 0;\n", "    if (i >= count)\n        i = count - 1;\n"),
    ("previous-wrap", "    if (i == 0)\n        i = count;\n", ""),
    ("find-first", "for (i = 0; i < (int32_t)count; i++)", "for (i = 1; i < (int32_t)count; i++)"),
    ("hide-mask", "        M32(rec + R_FLAGS) &= 0x7fffffffu;\n        H_WIN_UPDATE(rec);",
     "        M32(rec + R_FLAGS) &= 0x7ffffffeu;\n        H_WIN_UPDATE(rec);"),
    # text
    ("position-format", "NUM_FORMAT(G + 0x568, 0x14,", "NUM_FORMAT(G + 0x568, 0x15,"),
    ("rate-tick", "if (tick >= 8) {", "if (tick >= 7) {"),
    ("rate-clamp", "> 158.4L) {", "> 159.4L) {"),
    ("rate-magic", "q = (int32_t)(p >> 32) + n;", "q = (int32_t)(p >> 32);"),
    ("status-lower", "    if (GLB16(0x552) != 0 && !(M32(rec + R_FLAGS) & F_NO_STATUS)) {\n        first = 4;",
     "    if (GLB16(0x552) != 0) {\n        first = 4;"),
    ("crash-or", "((M8(g + 0x258) & 2) || M32(g + 0x760) == 0)", "((M8(g + 0x258) & 2) && M32(g + 0x760) == 0)"),
    ("slew-text", "case 4: text = STR(0x43c); break;", "case 4: text = STR(0x870); break;"),
    ("replay-text", "H_TEXT(rec, 6, 0x6a, STR(0x8a0), 1);", "H_TEXT(rec, 6, 0x6a, STR(0x87c), 1);"),
    ("title-on-view", "    if (GLB16(0x552) != 0 && !(M32(rec + R_FLAGS) & F_NO_STATUS))\n        H_TEXT(rec, 3,",
     "    if (GLB16(0x5dc) != 0 && !(M32(rec + R_FLAGS) & F_NO_STATUS))\n        H_TEXT(rec, 3,"),
    ("title-kind", "case 3: kind = STR(0x874); break;", "case 3: kind = STR(0x868); break;"),
    # covering the panel, modes
    ("max-guard", "(F_NO_STATUS | F_MAXIMISED)", "(F_MAXIMISED)"),
    ("panel-back", "        H_WIN_UPDATE(rec);\n        if (M32(S_MAXIMISED) == 0)\n            PANELS_SHOW();",
     "        H_WIN_UPDATE(rec);\n        if (M32(S_MAXIMISED) == 1)\n            PANELS_SHOW();"),
    ("mode-fullscreen", "if (GLB32(0x54c) != 1) {", "if (GLB32(0x54c) != 0) {"),
    ("mode-view-asks", "if (M32(view + 0xbc) == 0 || rec != GLB32(0x119c))", "if (rec != GLB32(0x119c))"),
    ("defaults-26", "M16(view + 0x26) = 0x2c0;", "M16(view + 0x26) = 0x2c1;"),
    ("defaults-order", "    M32(view + 0x10) = kind;\n    set_mode(rec, mode);",
     "    set_mode(rec, mode);\n    M32(view + 0x10) = kind;",
     "set_mode never reads the kind"),
    ("limit", "if (count >= 0x20) {", "if (count > 0x20) {"),
    ("first-geometry", "if (count == 0 && M32(S_GEOMETRY + 8)", "if (count <= 1 && M32(S_GEOMETRY + 8)"),
    ("cascade", "(M32(rec + R_ID) - 0x101) * 3 << 5", "(M32(rec + R_ID) - 0x101) << 6"),
    ("plain-first", "    if (GLB32(0x1238) != 0)\n        set_plain(rec);\n    else\n        clear_plain(rec);\n    activate(rec);",
     "    activate(rec);\n    if (GLB32(0x1238) != 0)\n        set_plain(rec);\n    else\n        clear_plain(rec);"),
    ("front-group", "H_NTH(G + 0x119c, 0x2000, 0);", "H_NTH(G + 0x119c, 0x1000, 0);"),
    ("extent-odd", "M32(view + 0) = (uint32_t)(w / 2);", "M32(view + 0) = (uint32_t)((w + 1) / 2);"),
    ("extent-small", "} else if (w > 2) {", "} else if (w >= 2) {",
     "the host makes a 3D view's width odd"),
    ("extent-height", "M32(view + 8) = (uint32_t)(1 - h / 2);", "M32(view + 8) = (uint32_t)(-h / 2);"),
    ("redraw-20", "case 2: case 6: case 8: case 10: case 12: case 20:", "case 2: case 6: case 8: case 10: case 12:"),
    ("map-zoom", "GLB16(0x2b2) = M16(view + 0x26);", "GLB16(0x2b2) = M16(view + 0x24);"),
    ("paint-19", "        CHAIN_SEND(0x19, 0, M32(list + 4 * i));\n", ""),
    ("paint-hidden", "        if (!(M32(rec + R_FLAGS) & F_SHOWN))\n            continue;\n", ""),
    # the layout
    ("old-73", "TAKE(G + 0xb60, 8, 2);", "TAKE(G + 0xb60, 6, 2);"),
    ("old-mode", "case 3: M32(p) = 4; break;", "case 3: M32(p) = 3; break;"),
    ("old-eye", "{ 0x9e, 2 }, { 0xa0, 4 },", "{ 0x9e, 4 }, { 0xa0, 4 },"),
    ("old-z", "            if (M32(e + L_Z) == 4)\n                M32(e + L_Z) = 2;",
     "            if (M32(e + L_Z) == 4)\n                M32(e + L_Z) = 3;"),
    ("old-active", "case 2: M32(M32(S_OLD_LAYOUT + 8) + L_ACTIVE) = 1; break;", "case 2: break;"),
    ("old-shown", "    if (M8(src) != 0)\n        M32(rec + R_FLAGS) |= F_SHOWN;",
     "    if (M8(src) > 1)\n        M32(rec + R_FLAGS) |= F_SHOWN;"),
    ("restore-flags", "& 0xdffeffffu;", "& 0xfffeffffu;"),
    ("restore-56", "            M16(nv + 0x56) = M16(ov + 0x56);\n", ""),
    ("restore-bc", "            M32(nv + 0xbc) = M32(ov + 0xbc);\n", ""),
    ("restore-550", "M32(M32(data + L_VIEW) + 0xbc) = GLB16(0x550);", "M32(M32(data + L_VIEW) + 0xbc) = GLB16(0x552);"),
    ("order-ff", "if (z < 0xff && z > deepest)", "if (z <= 0xff && z > deepest)"),
    ("order-depth", "if (M32(entries + 0x18 * i + L_Z) == deepest - k)", "if (M32(entries + 0x18 * i + L_Z) == k - 1)"),
    ("z-missing", "            i = 0xff;", "            i = 0xfe;"),
    ("save-active", "entry[1] = entry[2] == GLB32(0x119c);", "entry[1] = entry[3] == GLB32(0x11a0) && 0;"),
    ("geometry-25", "(int32_t)M32(rec + 0x24) > gy + 0x19 ||", "(int32_t)M32(rec + 0x24) > gy + 0x18 ||"),
    ("geometry-panel", "    if (M32(S_MAXIMISED) != 0) {\n        PANELS_HIDE();\n    } else",
     "    if (M32(S_MAXIMISED) != 0) {\n    } else"),
    ("stars-length", "        length -= 2;\n", "        length -= 1;\n"),
    # the eye
    ("deadband", "        if ((int32_t)eax < 0x40000)\n            return;",
     "        if ((int32_t)eax < 0x3ffff)\n            return;"),
    ("eye-dwords", "eax = (uint32_t)M16(edi + 0x2e8) << 0x10;", "eax = (uint32_t)M16(edi + 0x2ea) << 0x10;"),
    ("offset-axis", "    offset48();\n    add48(edi + 0x572);", "    offset48();\n    add48(edi + 0x570);"),
    ("offset-shift", "static void offset48(void)\n{\n    eax = (uint32_t)((int32_t)eax >> 8);",
     "static void offset48(void)\n{\n    eax = (uint32_t)((int32_t)eax >> 7);"),
    ("build-e", "    SET_AX(AX + M16(D + 0x2ce));", "    SET_AX(AX - M16(D + 0x2ce));"),
    ("sincos-order", "    ecx = ebx;\n    U_SINCOS(CX, D + 0x2d2, D + 0x2d8);\n    ecx = keep;\n    U_SINCOS(CX, D + 0x2d6, D + 0x2dc);",
     "    U_SINCOS(CX, D + 0x2d2, D + 0x2d8);\n    ecx = ebx;\n    U_SINCOS(CX, D + 0x2d6, D + 0x2dc);\n    ecx = keep;"),
    ("aspect-signed", "if ((int32_t)eax > (int32_t)ebx) {", "if (eax > ebx) {"),
    ("aspect-shift", "    eax >>= 0xd;\n", "    eax >>= 0xc;\n"),
    ("panel-matrix", "if (M32(edi + 0xd2c) != 1)", "if (M32(edi + 0xd2c) == 0)"),
    ("ground-clear", "    eax -= 0x200;\n    if ((int32_t)eax < 0) {", "    eax -= 0x100;\n    if ((int32_t)eax < 0) {"),
    ("tower-bits", "M32(edi + 0xd48) &= 0xffffffedu;", "M32(edi + 0xd48) &= 0xffffffefu;"),
    ("zoom-fraction", "    SET_CX(CX & 0x7f);\n    SET_CX((uint16_t)(CX << 8));", "    SET_CX(CX & 0x3f);\n    SET_CX((uint16_t)(CX << 8));"),
    ("map-heading", "if (M16(edi + 0xdc6) == 1 ||", "if (M16(edi + 0xdc6) == 2 ||"),
    ("map-clamp", "            SET_BX(0x780);\n        M16(edi + 0x2b2) = BX;", "            SET_BX(0x700);\n        M16(edi + 0x2b2) = BX;"),
    ("map-double", "            edx = edx + edx + (s >> 16);", "            edx = edx + edx;"),
    ("scale-shift", "    p >>= 0xf;\n", "    p >>= 0xe;\n"),
    ("spot-far", "    if (M16(esi + 0x28) == 0x8001) {\n        sar_dxax();", "    if (M16(esi + 0x28) == 0x8000) {\n        sar_dxax();"),
    ("spot-floor", "        uint32_t s = (uint32_t)AX + 0x200;\n\n        SET_AX(s);\n        SET_DX(DX + (s >> 16));",
     "        uint32_t s = (uint32_t)AX + 0x201;\n\n        SET_AX(s);\n        SET_DX(DX + (s >> 16));"),
    ("spot-bank", "        if (M8(esi + 0x30) == 1) {", "        if (M8(esi + 0x30) == 2) {"),
    ("spring-reach", "if ((int32_t)edx > 0x200000 ||", "if ((int32_t)edx > 0x1fffff ||"),
    ("spring-half", "            edx = (uint32_t)((int32_t)edx >> 1);\n            SET_AX((AX >> 1) | (c << 15));",
     "            (void)c;"),
    ("spring-pinned", "    if (M8(ebx + 0x31) == 0) {", "    if (M8(ebx + 0x31) != 1) {"),
    ("glide-arrive", "    if (!((int32_t)ecx < 0)) {\n        SET_AX(0);", "    if ((int32_t)ecx < 0) {\n        SET_AX(0);"),
    ("glide-all", "    SET_BP(BP & CX);", "    SET_BP(BP | CX);"),
    ("turn-limit", "        eax = delta - edi;\n        if ((int32_t)eax < 0)\n            edi += eax;",
     "        eax = delta - edi;\n        if ((int32_t)eax <= 0)\n            edi += eax;",
     "at equality it adds 0"),
    ("turn-sign", "        edi = -edi;\n        eax = delta - edi;", "        eax = delta - edi;"),
    ("snap-zeros", "eax = H_MEMMOVE(esi + 0x94, CONST_AT(0x209044e8), 0x18);", "eax = H_MEMMOVE(esi + 0x94, CONST_AT(0x209044e8), 0x16);"),
    ("state-track", "            if (BP != 0) {\n                SET_STATE(4);\n                view_snap();\n            }",
     "            if (BP != 0) {\n                SET_STATE(4);\n            }"),
    ("tower-aim-quirk", "                edi = G;\n                esi = edi + 0xd74;\n                edi = edi + 0xd50;\n                aim();\n                ebx = D + 0x2fc;",
     "                ebx = D + 0x2fc;"),
    ("to-spot-state", "            SET_STATE(8);", "            SET_STATE(2);"),
    ("multiply-carry", "            SET_BP((uint16_t)(BP << 1) | b);", "            SET_BP((uint16_t)(BP << 1));"),
    ("orient-8", "        if (M32(ebx + 0x18) == 8)\n            M32(edi + 0xd48) |= 8;",
     "        if (M32(ebx + 0x18) == 7)\n            M32(edi + 0xd48) |= 8;"),
    ("zoom-shift", "    SET_BX(BX - 4);", "    SET_BX(BX - 3);"),
    ("zoom-least", "    if ((int16_t)AX < 4)\n        SET_AX(4);\n    zoom_factor();", "    if ((int16_t)AX < 3)\n        SET_AX(3);\n    zoom_factor();"),
    ("look-cos", "    M16(D + 0x2d8) = 0x7fff;\n    ebx = keep_ebx;", "    M16(D + 0x2d8) = 0x7ffe;\n    ebx = keep_ebx;"),
    ("look-neg", "        t = CX; SET_CX(BX); SET_BX(t);\n        SET_AX(-AX);\n        M16(D + 0x2d6) = AX;",
     "        t = CX; SET_CX(BX); SET_BX(t);\n        M16(D + 0x2d6) = AX;"),
    ("look-level", "        M16(D + 0x2d6) = 0;\n        M16(D + 0x2dc) = 0x7fff;", "        M16(D + 0x2d6) = 0;\n        M16(D + 0x2dc) = 0x7ffe;"),
    ("orientation-down", "        SET_CX(0x4000);", "        SET_CX(0x3fff);"),
    ("cofactor-half", "    M32(g + 0xd10) = edx;", "    M32(g + 0xd10) = edx << 1;"),
    ("step-tower", "if (ecx == 3 && M16(edi + 0x11b0) == 0)", "if (ecx == 3 && M16(edi + 0x11b0) != 0)"),
    ("step-wrap", "    if ((int32_t)ecx < 0)\n        ecx = 4;", "    if ((int32_t)ecx < 0)\n        ecx = 3;"),
    ("angles-45", "if ((int16_t)AX <= 0x5a82 && (int16_t)AX >= (int16_t)0xa57e) {\n        SET_CX(M16(esi + 0x10));",
     "if ((int16_t)AX <= 0x5a81 && (int16_t)AX >= (int16_t)0xa57e) {\n        SET_CX(M16(esi + 0x10));"),
    ("angles-cos", "    if ((int16_t)AX < 0)\n        SET_AX(0x7fff);\n    M16(D + 0x2be) = AX;", "    M16(D + 0x2be) = AX;"),
    # the frame
    ("layer-ground", "        eax += 2;\n        if ((int32_t)eax >= (int32_t)M32(D + 0x380))",
     "        eax += 1;\n        if ((int32_t)eax >= (int32_t)M32(D + 0x380))"),
    ("layer-zero", "    return M32(D + 0x390) == 0;\n}\n\n/* .text+0x5f49", "    return 0;\n}\n\n/* .text+0x5f49"),
    ("layer-4", "if (M16(edi + 0xe8a) < 0x5db)", "if (M16(edi + 0xe8a) < 0x5dc)"),
    ("layer-ground-above", "    SET_AX(M16(D + 0x388));\n    if (!((int16_t)M16(edi + 0x21) > (int16_t)AX))",
     "    SET_AX(M16(D + 0x388));\n    if (!((int16_t)M16(edi + 0x21) >= (int16_t)AX))"),
    ("dark", "dark = DX < 0x8000;", "dark = DX < 0x7fff;"),
    ("high-map", "        if ((int16_t)CX >= 4) {", "        if ((int16_t)CX >= 3) {"),
    ("high-borrow", "        SET_CX(CX - AX - b);", "        SET_CX(CX - AX);"),
    ("model-done", "            eax = ACONTAIN_DONE(edi + 0x11e0);\n", ""),
    ("present-ram", "            if (M16(edi + 0xcba) < 0x8000)\n                eax = M32(edi + 0x11a8);",
     "            if (M16(edi + 0xcba) <= 0x8000)\n                eax = M32(edi + 0x11a8);"),
    ("sky-colour", "M32(edi + 0x3c0) = 0x6800;", "M32(edi + 0x3c0) = 0x6801;"),
    ("cross-shift", "    eax = (uint32_t)((int32_t)eax >> 4);\n", "    eax = (uint32_t)((int32_t)eax >> 3);\n"),
    ("map-e88", "    M16(edi + 0xe88) = keep_e88;\n", ""),
    ("map-39c", "    M16(edi + 0x39c) = keep_39c;\n", ""),
    ("stars-wipe", "                M32(edi + 0xce4) = 0;\n                eax = G3D_DRAW(M32(S_STARS));",
     "                eax = G3D_DRAW(M32(S_STARS));"),
]


def main():
    source = open(SRC).read()
    results = []
    for entry in BREAKS:
        name, old, new = entry[:3]
        if ONLY and name not in ONLY:
            continue
        if source.count(old) != 1:
            print("%-20s cannot apply: %d matches" % (name, source.count(old)))
            results.append((name, "unapplied"))
            continue
        open(TMP, "w").write(source.replace(old, new))
        if os.path.exists("bin/difftest/WINDOW_ours.dll"):
            os.remove("bin/difftest/WINDOW_ours.dll")
        build = subprocess.run(["make", "-s", "window-break-build", "WINDOW_SRC=" + TMP],
                               capture_output=True, text=True)
        if build.returncode != 0:
            print("%-20s does not build:\n%s" % (name, build.stderr[-2000:]))
            results.append((name, "unbuilt"))
            continue
        run = subprocess.run(["wine", "windowdiff.exe", str(N), "../../cd"], cwd="bin/difftest",
                             capture_output=True, text=True,
                             env=dict(os.environ, WINEDEBUG="-all"))
        caught = run.returncode != 0
        last = [l for l in run.stdout.splitlines() if l.startswith("windowdiff:")]
        verdict = "caught" if caught else "equivalent" if len(entry) > 3 else "MISSED"
        print("%-20s %s  %s%s" % (name, verdict, last[0] if last else run.stdout.strip()[-200:],
                                  "  (" + entry[3] + ")" if len(entry) > 3 and not caught else ""))
        results.append((name, verdict.lower()))
        sys.stdout.flush()
    subprocess.run(["make", "-s", "windowdiff"], check=True)
    missed = [n for n, r in results if r not in ("caught", "equivalent")]
    same = [n for n, r in results if r == "equivalent"]
    print("%d breaks, %d caught, %d proven equivalent (%s); missed: %s" %
          (len(results), len(results) - len(missed) - len(same), len(same), " ".join(same),
           " ".join(missed) or "none"))
    return 1 if missed else 0


if __name__ == "__main__":
    sys.exit(main())
