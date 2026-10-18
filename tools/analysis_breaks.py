#!/usr/bin/env python3
"""Break OpenFS98's ANALYSIS on purpose, one place at a time, and check that
analysisdiff notices.

Each break is a small edit to a copy of src/analysis/analysis.c -- the kind
of slip a transliteration makes: a boundary, an offset, an order, a dropped
call, a register quirk tidied away.  The copy is built as the harness's
ANALYSIS_ours.dll and run for N scenarios (analysisdiff exits non-zero when
any differ).  At the end the real build goes back.  A missed break is a gap
in the harness or an equivalence; either way it is reported, and
docs/modules/analysis.md says which.

Usage: tools/analysis_breaks.py [N] [name...]      (from the repository root,
       after make analysisdiff)
"""
import os
import subprocess
import sys

N = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 2000
ONLY = [a for a in sys.argv[1:] if not a.isdigit()]
SRC = "src/analysis/analysis.c"
TMP = "bin/difftest/analysis_break.c"

# (name, old, new[, why it cannot show]).  A fourth element marks a break
# proven equivalent, reported as such.
BREAKS = [
    # registers
    ("memmove-ecx", "#define AN_MEMMOVE_ECX 7u", "#define AN_MEMMOVE_ECX 0u"),
    ("cmpsw-cx-only", "    uint32_t n = (ecx & 0xffff0000u) | 0xc,", "    uint32_t n = 0xc,"),
    ("cmpsw-ecx-reset", "        ecx = reset();\n    if (GLB32(0x814) == 0)", "        reset();\n    if (GLB32(0x814) == 0)"),
    ("cmpsw-ecx-add", "            ecx = CHAIN_ADD_ECX(0xc, on_command);",
     "            CHAIN_ADD_ECX(0xc, on_command);"),
    ("cmpsw-ecx-trail", "            ecx = CHAIN_ADD_ECX(0xe, on_trail);",
     "            CHAIN_ADD_ECX(0xe, on_trail);"),
    ("cmpsw-ecx-armed", "        M8(S_LANDING) = M8(S_LANDING) + 1;\n        ecx = reset();",
     "        M8(S_LANDING) = M8(S_LANDING) + 1;\n        reset();"),
    # the frame
    ("frame-reset-820", "    if (M32(S_RESET) != 0 || GLB32(0x820) != 0)", "    if (M32(S_RESET) != 0)"),
    ("frame-crash-4", "    if (c != 0 && c <= 4) {", "    if (c != 0 && c < 4) {"),
    ("frame-crash-empty", "        if (M32(S_COUNT) == 0)\n            return;\n        if (GLB32(0x814) == 3)",
     "        if (GLB32(0x814) == 3)"),
    ("frame-268", "    if (GLB32(0x268) != 0)\n        return;\n", ""),
    ("frame-365", "        if (GLB8(0x365) != 0)\n            return;\n", ""),
    ("landing-borrow", "        edx = edx - (uint32_t)((int32_t)ebx >> 8) - ((ebx >> 7) & 1);",
     "        edx = edx - (uint32_t)((int32_t)ebx >> 8) - ((ax >> 8) < (ebx & 0xff) ? 0 : 0);"),
    ("landing-ah", "        ax = (uint16_t)((ax & 0xff) | (((ax >> 8) - (ebx & 0xff)) & 0xff) << 8);",
     "        ax = (uint16_t)(ax - (ebx & 0xff));"),
    ("landing-sar", "(uint32_t)((int32_t)ebx >> 8) - ((ebx >> 7) & 1);",
     "(ebx >> 8) - ((ebx >> 7) & 1);"),
    ("landing-arm", "            if (edx < 0x22)", "            if (edx <= 0x22)"),
    ("landing-ff", "        if (edx > 0xff)\n            return;\n", "",
     "anything over 0xff is over 0x1e, the next test"),
    ("landing-1e", "        if (edx > 0x1e)\n            return;", "        if (edx >= 0x1e)\n            return;"),
    ("landing-7ae5", "        if (edx == 0x1e && ax > 0x7ae5)", "        if (edx == 0x1e && ax >= 0x7ae5)"),
    ("landing-state", "        if (M8(S_LANDING) > 1)\n            goto take;", "        if (M8(S_LANDING) > 2)\n            goto take;"),
    ("trail-818", "        if (GLB32(0x818) == 0)\n            return;\n", ""),
    ("countdown-sign", "    if ((int8_t)M8(S_COUNTDOWN) >= 0)", "    if ((int8_t)M8(S_COUNTDOWN) > 0)"),
    ("countdown-landing", "        al = 2;\n    else if (mode == 2)", "        al = 3;\n    else if (mode == 2)"),
    ("countdown-table", "        al = M8(S_FRAMES + GLB32(0x824));", "        al = M8(S_FRAMES + 1 + GLB32(0x824));"),
    ("ground-2b8", "        if (GLB32(0x2b8) > 0xc7f)", "        if (GLB32(0x2b8) >= 0xc7f)"),
    ("ground-366", "        if (GLB8(0x366) == 0)\n            return;\n        goto landed;", "        goto landed;"),
    ("ended-crash", "        if (al == 0 || al > 4)", "        if (al == 0 || al > 3)"),
    ("ended-clear", "manoeuvre_end:\n    M32(S_ENDED) = 0;", "manoeuvre_end:"),
    ("landed-state", "landed:\n    M8(S_LANDING) = 0;", "landed:"),
    ("off-1a", "off:\n    GLB32(0x814) = 0;\n    GLB16(0x1a) &= 0xfdff;", "off:\n    GLB32(0x814) = 0;"),
    ("command-arg", "M32(arg) == 0 && command == 0x1002b", "command == 0x1002b"),
    # samples
    ("sample-flag", "        flag |= 2;", "        flag |= 4;"),
    ("sample-limit", "    if (M32(S_COUNT) >= 1000)\n        compact();", "    if (M32(S_COUNT) > 1000)\n        compact();"),
    ("compact-stride", "    for (ebx = 2; ebx < 1000; ebx += 2) {", "    for (ebx = 1; ebx < 1000; ebx += 2) {"),
    ("compact-rol", "(uint8_t)((M8(S_INTERVAL) << 1) | (M8(S_INTERVAL) >> 7));",
     "(uint8_t)(M8(S_INTERVAL) << 1);"),
    ("reset-scratch", "    H_MEMSET_ECX(S_SCRATCH, 0, 0x1c);\n", ""),
    ("get-bound", "static void FS98_STDCALL get_position(uint32_t index, uint32_t out)\n{\n    if (index < 1000)",
     "static void FS98_STDCALL get_position(uint32_t index, uint32_t out)\n{\n    if (index <= 1000)"),
    ("get-offset", "        sample_out(index, out, 4, 0x18);", "        sample_out(index, out, 0, 0x18);"),
    ("setcount-bound", "    if (n < 1000)\n        M32(S_COUNT) = n;", "    if (n <= 1000)\n        M32(S_COUNT) = n;"),
    # the trail
    ("list-segments", "        len = M32(S_SEGMENTS + 4 * GLB32(0x828));", "        len = M32(S_SEGMENTS + 4 * GLB32(0x828)) + 1;"),
    ("list-two", "    if (n >= 2) {", "    if (n >= 1) {"),
    ("list-toggle", "            t ^= 1;\n", ""),
    ("list-close", "            M16(p) = 0x10;", "            M16(p) = 0xf;"),
    ("segment-200", "    M32(p + 0xa) = 0x200;", "    M32(p + 0xa) = 0x100;"),
    ("segment-lon", "    M16(p + 0x14) = M16(q + 0xa);", "    M16(p + 0x14) = M16(q + 0x8);"),
    ("trail-mode", "    if (GLB32(0x81c) != 0 && GLB32(0x814) == 2) {", "    if (GLB32(0x81c) != 0) {"),
    # the landing graph
    ("lcurve-one", "    if (ebx == 1) {", "    if (ebx <= 1) {",
     "differs only with no samples, retail's uninitialised frame, where the harness stops"),
    ("lcurve-mark", "        M32(b + 0x40) = M32(b + 0x40) + 2;", "        M32(b + 0x40) = M32(b + 0x40) + 1;"),
    ("lcurve-base", "    M32(S_BASE) = ((M32(b + 0x34) & 0xff) << 16) + (M32(b + 0x32) & 0xffff);",
     "    M32(S_BASE) = ((M32(b + 0x34) & 0xffff) << 16) + (M32(b + 0x32) & 0xffff);"),
    ("lcurve-scale", "    M32(b + 0x1c) = ((M32(S_RANGE) << 8) / 5) >> 8;", "    M32(b + 0x1c) = M32(S_RANGE) / 5;"),
    ("lcurve-120", "            v *= 120;", "            v *= 100;"),
    ("lcurve-sign", "            if ((int32_t)v > 0)\n                M32(b + 4) = 0 - v;", ""),
    ("lcurve-first-only", "        if (al == 0x83 && edi == 0) {", "        if (al == 0x83) {"),
    ("lcurve-frac", "- ((M32(b + 0x7e) & 0xffff) >> 8) +", "- ((M32(b + 0x7e) & 0xffff) >> 7) +"),
    ("lcurve-stop", "        if (al == 0x81 || al == 0x83) {", "        if (al & 0x80) {"),
    ("lcurve-from", "            M32(b + 0x14) = M32(b + 0x0c);\n", ""),
    ("lpoint-negative", "    if (ecx & 0x80000000u)\n        v = 0;", "    if (ecx & 0x80000000u)\n        v = 1;",
     "1/256 truncates to 0 as 0 does"),
    ("lpoint-100", "    else if ((int32_t)ecx > 0x100)", "    else if ((int32_t)ecx >= 0x100)"),
    ("lpoint-839", "    edi = (uint32_t)v * 839;", "    edi = (uint32_t)v * 838;"),
    ("lpoint-clamp", "    else if ((int32_t)edi > 0x1000000)\n        edi = 0x1000000;", ""),
    ("lpoint-range", "    if (ecx > esi)\n        ecx = esi;", ""),
    ("lpoint-divfix", "    eax = esi != 0 ? eax / esi : 0xffffffffu;", "    eax = esi != 0 ? eax / esi : 0;"),
    ("lpoint-7692", "    eax = ecx * 7692;", "    eax = ecx * 7693;"),
    ("lpoint-20", "    v = (int32_t)((uint32_t)v * 20) / 256;", "    v = (int32_t)((uint32_t)v * 20) >> 8;",
     "edi is clamped to 0..0x1000000, so v*20 is never negative"),
    ("lpoint-clamp-x", "    if ((int32_t)M32(out) > (int32_t)W)\n        M32(out) = W - 1;",
     "    if ((int32_t)M32(out) >= (int32_t)W)\n        M32(out) = W - 1;"),
    ("lgrid-5644", "(uint32_t)((int32_t)(H * 500) / 6144), (uint32_t)((int32_t)(H * 5644) / 6144));",
     "(uint32_t)((int32_t)(H * 500) / 6144), (uint32_t)((int32_t)(H * 5645) / 6144));"),
    ("lgrid-unsigned", "(W * 300) >> 13, (H * 3635) / 6144);", "(W * 300) >> 13, (uint32_t)((int32_t)(H * 3635) / 6144));"),
    ("lgrid-ticks", "    for (esi = 0x1e0c; esi <= 0x7830; esi += 0x1e0c)", "    for (esi = 0x1e0c; esi < 0x7830; esi += 0x1e0c)"),
    ("lgrid-1788", "    edi = (W * 1788) >> 13;", "    edi = (W * 1787) >> 13;"),
    ("lprep-flag", "    M32(S_LBLOCK + 0xc) = 0;\n", ""),
    ("control-other", "        detach(hwnd);\n        return 0;\n    default:\n        return 0;\n    }\n}\n\n/* .text+0x960 */",
     "        detach(hwnd);\n        return 0;\n    default:\n        return 0xffffffffu;\n    }\n}\n\n/* .text+0x960 */"),
    ("attach-top", "    M32(view + 8) = (uint32_t)-((int32_t)H / 2);", "    M32(view + 8) = (uint32_t)(-(int32_t)H / 2 - 1);"),
    # the manoeuvre graph
    ("extents-tiebreak", "(ecx == M32(S_LATMAX + 4) && (int32_t)ecx > (int32_t)(uint32_t)M16(S_LATMAX + 2))) {",
     "(ecx == M32(S_LATMAX + 4) && M16(p + 2) > M16(S_LATMAX + 2))) {"),
    ("extents-min-frac", "(ecx == M32(S_LATMIN + 4) && M16(p + 2) < M16(S_LATMIN + 2))) {",
     "(ecx == M32(S_LATMIN + 4) && M16(p + 2) <= M16(S_LATMIN + 2))) {",
     "on a full tie it stores what is there"),
    ("extents-window", "    ecx = M32(p + 0xc);\n    if (ecx > 0x4000 && ecx < 0xc000) {\n        if (ecx > M32(S_LONA + 4) ||",
     "    ecx = M32(p + 0xc);\n    if (ecx >= 0x4000 && ecx < 0xc000) {\n        if (ecx > M32(S_LONA + 4) ||"),
    ("extents-signed", "        if (ecx < M32(S_LONB + 4) ||", "        if ((int32_t)ecx < (int32_t)M32(S_LONB + 4) ||"),
    ("extents-flag", "        M32(S_UNSIGNED) = 0;\n        M32(S_LONA + 4) = M32(p + 0xc);",
     "        M32(S_LONA + 4) = M32(p + 0xc);"),
    ("extents-twice", "        get_position(esi - 1, q);", "        get_position(esi, q);"),
    ("compute-fst", "    MD(S_MIDLAT) = (double)(t * (long double)RD(0x00) + (long double)MD(b + 8));",
     "    MD(S_MIDLAT) = (double)((long double)MD(S_LATSPAN) * (long double)RD(0x00) + (long double)MD(b + 8));",
     "two 32.16 values differ by at most 49 bits: the register's value is a double already"),
    ("compute-unsigned", "    if (M32(S_UNSIGNED) == 1) {\n        UTIL_C0(S_LONSPAN, S_LONA);",
     "    if (M32(S_UNSIGNED) == 2) {\n        UTIL_C0(S_LONSPAN, S_LONA);"),
    ("scale-compare", "    if (a > (long double)MD(ADDR(q))) {", "    if (a >= (long double)MD(ADDR(q))) {",
     "at equality a is the double both branches use"),
    ("scale-extended", "        m = a;\n    } else {", "        m = (long double)(double)a;\n    } else {"),
    ("scale-low", "    M32(ADDR(q)) = (uint32_t)e;", "    MD(ADDR(q)) = (double)e;"),
    ("scale-flag", "    M32(S_MBLOCK + 0x10) = 1;\n", ""),
    ("mpoint-order", "    y = y - (long double)RD(0x38);", "    y = (long double)RD(0x38) - y;"),
    ("mpoint-double", "    MD(b + 0x20) = (double)((long double)(int32_t)W * (long double)RD(0x28));",
     "    MD(b + 0x20) = (double)((long double)W * (long double)RD(0x28));"),
    ("mcurve-drawn", "    if (M32(S_DRAWN) != 1 || M32(S_MCOUNT) == 0)", "    if (M32(S_MCOUNT) == 0)",
     "+0x170 is 1 in the image and only ever set to 1"),
    ("mgrid-ticks", "    for (esi = 0x1630; esi <= 0x9b50; esi += 0x1630)", "    for (esi = 0x1630; esi < 0x9b50; esi += 0x1630)"),
    ("mgrid-tenth", "        HG2D_LINE_V(S_SURFACE, ((esi / 10 + 0x222) * W) >> 13, y1, y2);",
     "        HG2D_LINE_V(S_SURFACE, ((esi / 10 + 0x223) * W) >> 13, y1, y2);"),
    ("mgrid-1966", "    x2 = (uint32_t)((int32_t)(W * 1966) / 8192);", "    x2 = (uint32_t)((int32_t)(W * 1967) / 8192);"),
    ("mprep-units", "    M32(S_MBLOCK + 0xc) = GLB16(0xc18);", "    M32(S_MBLOCK + 0xc) = GLB8(0xc18);"),
    # the options
    ("oread-0", "        M32(p + 0xc) = 0;\n        M32(p + 0x14) = M32(S_CHOICE);",
     "        M32(p + 0xc) = 0;\n        M32(p + 0x14) = 0;"),
    ("oread-3", "    case 3:\n        M32(p + 0xc) = 1;\n        M32(p + 0x14) = 1;",
     "    case 3:\n        M32(p + 0xc) = 1;\n        M32(p + 0x14) = 2;"),
    ("oread-status", "    M32(p + 0x38) = 0;\n}", "}",
     "UTIL's read of the record that follows clears its status anyway"),
    ("ovalidate-2", "    if ((int32_t)M32(p + 0x3c) > 2) {", "    if ((int32_t)M32(p + 0x3c) > 3) {"),
    ("ovalidate-signed", "    if ((int32_t)M32(p + 0x34) > 2) {", "    if (M32(p + 0x34) > 2) {"),
    ("ocommit-off", "    if (M32(p + 0xc) == 0) {\n        GLB32(0x814) = 0;\n        GLB16(0x1a) &= 0xfdff;\n    }\n    switch",
     "    if (M32(p + 0xc) == 0) {\n        GLB32(0x814) = 0;\n        GLB16(0x1a) &= 0xfdff;\n        return 0;\n    }\n    switch"),
    ("ocommit-course", "        GLB32(0x81c) = M32(p + 0x2c);\n        break;", "        break;"),
    ("ocommit-reset", "    if (r == 0)\n        GLB32(0x820) = M32(p + 0x1c);", "    GLB32(0x820) = M32(p + 0x1c);"),
    ("ocommit-choice", "    M32(S_CHOICE) = M32(p + 0x14);\n    return r;", "    return r;"),
    ("oexchange-modes", "    case 1:\n        return o_commit(base);\n    case 2:\n        return o_validate(base);",
     "    case 2:\n        return o_commit(base);\n    case 1:\n        return o_validate(base);"),
    ("owrite-mode", "        r = UTIL_PUT(S_OTABLE, 3, S_OBLOCK, 0);", "        r = UTIL_PUT(S_OTABLE, 3, S_OBLOCK, 2);",
     "UTIL treats write modes 0 and 2 alike: dword stores and a commit callback"),
    ("ocheck-mode", "    return UTIL_PUT(S_OTABLE, 3, block, 1);", "    return UTIL_PUT(S_OTABLE, 3, block, 2);"),
    ("deinit-1a", "    GLB16(0x1a) &= 0xfdff;\n    GLB32(0x814) = 0;\n}", "    GLB32(0x814) = 0;\n}"),
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
        if os.path.exists("bin/difftest/ANALYSIS_ours.dll"):
            os.remove("bin/difftest/ANALYSIS_ours.dll")
        build = subprocess.run(["make", "-s", "analysis-break-build", "ANALYSIS_SRC=" + TMP],
                               capture_output=True, text=True)
        if build.returncode != 0:
            print("%-20s does not build:\n%s" % (name, build.stderr[-2000:]))
            results.append((name, "unbuilt"))
            continue
        run = subprocess.run(["wine", "analysisdiff.exe", str(N)], cwd="bin/difftest",
                             capture_output=True, text=True,
                             env=dict(os.environ, WINEDEBUG="-all"))
        caught = run.returncode != 0
        last = [l for l in run.stdout.splitlines() if l.startswith("analysisdiff:")]
        verdict = "caught" if caught else "equivalent" if len(entry) > 3 else "MISSED"
        print("%-20s %s  %s%s" % (name, verdict, last[0] if last else run.stdout.strip()[-200:],
                                  "  (" + entry[3] + ")" if len(entry) > 3 and not caught else ""))
        results.append((name, verdict.lower()))
        sys.stdout.flush()
    subprocess.run(["make", "-s", "analysisdiff"], check=True)
    missed = [n for n, r in results if r not in ("caught", "equivalent")]
    same = [n for n, r in results if r == "equivalent"]
    print("%d breaks, %d caught, %d proven equivalent (%s); missed: %s" %
          (len(results), len(results) - len(missed) - len(same), len(same), " ".join(same),
           " ".join(missed) or "none"))
    return 1 if missed else 0


if __name__ == "__main__":
    sys.exit(main())
