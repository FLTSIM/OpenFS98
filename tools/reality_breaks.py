#!/usr/bin/env python3
"""Break OpenFS98's REALITY on purpose, one place at a time, and check that
realitydiff notices.

Each break is a small edit to a copy of src/reality/reality.c -- the kind of
slip a transliteration makes: a boundary, an offset, an order, a dropped
call.  The copy is built as the harness's REALITY_ours.dll and run for N
scenarios (realitydiff exits non-zero when any differ).  At the end the real
build goes back.  A missed break is a gap in the harness or an equivalence;
either way it is reported, and docs/modules/reality.md says which.

Usage: tools/reality_breaks.py [N] [name...]       (from the repository root,
       after make realitydiff)
"""
import os
import subprocess
import sys

N = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 2000
ONLY = [a for a in sys.argv[1:] if not a.isdigit()]
SRC = "src/reality/reality.c"
TMP = "bin/difftest/reality_break.c"

# (name, old, new[, why it cannot show]).  A fourth element marks a break
# proven equivalent, reported as such.
BREAKS = [
    # the crash
    ("crash-gate-760", "    if (GLB32(0x760) == 0 && GLB16(0x18) == 5)", "    if (GLB16(0x18) == 5)"),
    ("crash-gate-838", "    if (GLB8(0x365) != 0 || GLB32(0x270) != 0 || GLB8(0x838) != 0)",
     "    if (GLB8(0x365) != 0 || GLB32(0x270) != 0)"),
    ("crash-hold", "    if (GLB32(0x83c) != 0)\n        goto steady;", "    if (GLB32(0x83c) != 0)\n        goto done;"),
    ("crash-e", "    if (code == 0xe)\n        code = 6;", "    if (code == 0xe)\n        code = 8;"),
    ("crash-84a", "(code == 0x10 || code == 0x12 || code == 0x14) && GLB16(0x84a) == 0",
     "(code == 0x10 || code == 0x12) && GLB16(0x84a) == 0"),
    ("crash-odd", "    if ((code & 1) || code > 0x14)", "    if ((code & 1) || code > 0x16)"),
    ("crash-string", "    H_LOADSTRING(code + 0x258, ADDR(text), 0x40);", "    H_LOADSTRING(code + 0x259, ADDR(text), 0x40);"),
    ("crash-time", "    WINMSG_SHOW(ADDR(text), 5, 0x3c, 0);", "    WINMSG_SHOW(ADDR(text), 5, 0x3d, 0);"),
    ("crash-b24", "        M32(S_SOUND) |= 0x10;\n", ""),
    ("crash-sound-result", "    if (SOUND_EVENT(0xe, S_SOUND) != 0)\n        M32(S_PLAYING) = 0;",
     "    SOUND_EVENT(0xe, S_SOUND);"),
    ("crash-count", "    GLB8(0x838) = 5;\n    GLB8(0x840) = 1;", "    GLB8(0x838) = 6;\n    GLB8(0x840) = 1;"),
    ("crash-spot-kind", "M32(rec + 4) == 3 && M32(GLB32(0x1198) + 0x10) == 0", "M32(rec + 4) == 3"),
    ("crash-spot-82c", "    if (GLB16(0x846) != 0 && GLB32(0x82c) != 0) {", "    if (GLB16(0x846) != 0) {"),
    ("crash-clear", "done:\n    GLB16(0x832) = 0;", "done:\n    GLB8(0x832) = 0;"),
    ("descent-half", "        v = (int32_t)(((uint32_t)v & 0xffff0000u) | 0xa674);   /* mov ax */",
     "        v = 0xa674;"),
    ("descent-low", "    if (v < -0xa674)\n        v = -0xa674;", "    if (v <= -0xa674)\n        v = -0xa673;",
     "-0xa674 and -0xa673 both give 9845"),
    ("descent-low-value", "    if (v < -0xa674)\n        v = -0xa674;", "    if (v < -0xa674)\n        v = -0xa600;"),
    ("descent-mul", ">> 32);\n    GLB16(0x844)", ">> 31);\n    GLB16(0x844)"),
    ("descent-844", "GLB32(0x2b8) >> 7);", "GLB32(0x2b8) >> 6);"),
    # the countdown
    ("hold-sign", "    if (n < 0)\n        return;\n    GLB32(0x83c)", "    if (n < -1)\n        return;\n    GLB32(0x83c)"),
    ("hold-8", "    else if (GLB16(0x832) == 8)", "    else if (GLB16(0x832) == 7)"),
    ("count-sign", "    if ((int8_t)al < 0)\n        return;", "    if (al == 0xff)\n        return;"),
    ("count-6", "    if (ah == 6 && GLB16(0x846) != 0)", "    if (ah == 5 && GLB16(0x846) != 0)"),
    ("count-playing", "    if (ah == 3 && M32(S_PLAYING) != 0)\n        return;\n", ""),
    ("count-unpause", "    if (ah == 4) {\n        CONTROLS_PAUSE(8, 0);", "    if (ah == 4) {\n        CONTROLS_PAUSE(8, 1);"),
    ("count-held", "        GLB16(0x846) = held;\n", ""),
    ("count-ah", "        ah = (uint8_t)(analysis() >> 8);", "        analysis();"),
    ("count-code", "    if (ah == 2 && M16(S_CODE) == 2)", "    if (ah == 2 && M16(S_CODE) != 0)"),
    ("count-resume", "    GLB16(0x846) = M16(S_HELD);\n", ""),
    ("count-post", "        H_POST(0x100c3, 0);\n    else\n        H_POST(0x100c2, 0);",
     "        H_POST(0x100c2, 0);\n    else\n        H_POST(0x100c3, 0);"),
    ("list3-348", "GLB32(0x2b4) <= 0x7fff", "GLB32(0x2b4) < 0x7fff"),
    # stress
    ("stress-reset", "    M32(S_STRESS) = 0x78;", "    M32(S_STRESS) = 0x77;"),
    ("stress-ah", "    r = UTIL_RAND() & 0xffff00ffu;      /* xor ah, ah */", "    r = UTIL_RAND();"),
    ("stress-odds", "    if ((uint8_t)r >= 0x64)", "    if ((uint8_t)r > 0x64)"),
    ("stress-threshold", "    if ((uint16_t)r < GLB16(0x372))", "    if ((uint16_t)r <= GLB16(0x372))"),
    ("stress-table", "M32(S_DAMAGE + (GLB32(0x258) & 0xe) * 2)", "M32(S_DAMAGE + (GLB32(0x258) & 0x7) * 4)"),
    ("stress-32", "        if (UTIL_RAND32() <= 0xd1b71)", "        if (UTIL_RAND32() < 0xd1b71)",
     "no state of UTIL's generator draws 0xd1b71"),
    ("damage-set", "    if (M8(flag) == 0)\n        M8(flag) = 1;", "    M8(flag) = 1;"),
    ("clear-only-1", "        if (GLB8(flags[i]) == 1)\n            GLB8(flags[i]) = 0;", "        GLB8(flags[i]) = 0;"),
    ("clear-b6f", "        0xb70, 0xb71, 0xb72, 0xb73, 0xb6e, 0xb6f,", "        0xb70, 0xb71, 0xb72, 0xb73, 0xb6e,"),
    # the crack
    ("crack-mode", "    if (M32(view + 0x14) != 0 || M32(view + 0x10) == 1)", "    if (M32(view + 0x10) == 1)"),
    ("crack-ylimit", "    M32(S_YLIMIT) = (0 - M32(view + 8) * 2) - 1;", "    M32(S_YLIMIT) = (0 - M32(view + 8) * 2);"),
    ("crack-clamp", "    if (v >= limit)\n        v = limit;", "    if (v > limit)\n        v = limit;",
     "at equality both store the limit"),
    ("crack-place", "    *x = (M32(view) * 2 * *x >> 8) + M32(view + 4) - M32(view);",
     "    *x = (M32(view) * 2 * *x >> 8) + M32(view + 4);"),
    ("crack-shadow", "        crack_line(x, y + 1, px, py + 1);\n", ""),
    ("crack-colour", "        GLB32(0x2ec) = 0xf;", "        GLB32(0x2ec) = 0xe;"),
    # the analysis
    ("collect-count", "    for (i = 0; i <= 0x18; i++) {", "    for (i = 0; i < 0x18; i++) {"),
    ("collect-high", "M16(e + 0x12) > M16(S_HIGHEST + 2))) {", "M16(e + 0x12) >= M16(S_HIGHEST + 2))) {",
     "on a tie it copies what is there"),
    ("collect-x", "        MD(pt) = (double)(long double)(uint64_t)index;", "        MD(pt) = (double)(int32_t)index;"),
    ("collect-frac", "(long double)(int32_t)M16(e + 0x12) * (long double)RD(0xc8)",
     "(long double)(int16_t)M16(e + 0x12) * (long double)RD(0xc8)"),
    ("prepare-descent", "(long double)(int16_t)GLB16(0x842) * (long double)RD(0xc0)",
     "(long double)GLB16(0x842) * (long double)RD(0xc0)"),
    ("analysis-one", "        if (r == 0 && M32(S_BLOCK) == 1)\n            r = nothing();", "",
     "nothing() returns the 0 r already is"),
    ("rate-units", "    if ((int32_t)units > 0 && (int32_t)units <= 2)", "    if ((int32_t)units > 0 && (int32_t)units <= 1)"),
    ("plot-sub", "    xx = xx - (long double)RD(0xd0);", "    xx = xx + (long double)RD(0xd0);"),
    ("plot-order", "    t = xx * (long double)(int32_t)M32(S_WIDTH);\n    t = t * (long double)RD(0x128);",
     "    t = xx * ((long double)(int32_t)M32(S_WIDTH) * (long double)RD(0x128));"),
    ("labels-hi", "        NUM_FORMAT(ADDR(&v.hi), 0x1001b, p);", "        NUM_FORMAT(ADDR(&v.lo), 0x1001b, p);"),
    ("labels-blank", "        if ((i & 1) == 1) {", "        if ((i & 1) == 0) {"),
    ("scale-metric-stride", "M32(R + 0x60 + 0x30 * i))", "M32(R + 0x60 + 8 * i))"),
    ("scale-feet-equal", "            while (range > (long double)(uint64_t)M32(R + 8 * i))",
     "            while (range >= (long double)(uint64_t)M32(R + 8 * i))"),
    ("scale-zero", "    if (range == (long double)RD(0xf0)) {", "    if (range == (long double)RD(0xf8)) {"),
    ("scale-double", "    long double range = (long double)MD(S_MAX) - (long double)MD(S_MIN);",
     "    long double range = (long double)(double)(MD(S_MAX) - MD(S_MIN));",
     "two 32.16 values differ by at most 49 bits, exact in a double"),
    ("grid-vertical", "    for (x = 0x29; x < 0x2270; x += 0x2a3) {", "    for (x = 0x29; x <= 0x2270; x += 0x2a3) {"),
    ("grid-horizontal-y", "(0x17e1 - i) * h / 6144);", "(uint32_t)((int32_t)((0x17e1 - i) * h) / 6144));"),
    ("curve-first", "    plot(0, -0x16, ADDR(from));", "    plot(0, -0x15, ADDR(from));"),
    ("paint-fill", "    if (H_MEMSET(M32(S_BITS), 0xff, M32(S_BITS_SIZE)) != 0)",
     "    if (H_MEMSET(M32(S_BITS), 0xfe, M32(S_BITS_SIZE)) != 0)"),
    ("attach-top", "    M32(view + 8) = (uint32_t)-((int32_t)M32(S_HEIGHT) / 2);",
     "    M32(view + 8) = (uint32_t)(-(int32_t)M32(S_HEIGHT) / 2 - 1);"),
    ("control-other", "    default:\n        return 0xffffffffu;", "    default:\n        return 0;"),
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
        if os.path.exists("bin/difftest/REALITY_ours.dll"):
            os.remove("bin/difftest/REALITY_ours.dll")
        build = subprocess.run(["make", "-s", "reality-break-build", "REALITY_SRC=" + TMP],
                               capture_output=True, text=True)
        if build.returncode != 0:
            print("%-20s does not build:\n%s" % (name, build.stderr[-2000:]))
            results.append((name, "unbuilt"))
            continue
        run = subprocess.run(["wine", "realitydiff.exe", str(N)], cwd="bin/difftest",
                             capture_output=True, text=True,
                             env=dict(os.environ, WINEDEBUG="-all"))
        caught = run.returncode != 0
        last = [l for l in run.stdout.splitlines() if l.startswith("realitydiff:")]
        verdict = "caught" if caught else "equivalent" if len(entry) > 3 else "MISSED"
        print("%-20s %s  %s%s" % (name, verdict, last[0] if last else run.stdout.strip()[-200:],
                                  "  (" + entry[3] + ")" if len(entry) > 3 and not caught else ""))
        results.append((name, verdict.lower()))
        sys.stdout.flush()
    subprocess.run(["make", "-s", "realitydiff"], check=True)
    missed = [n for n, r in results if r not in ("caught", "equivalent")]
    same = [n for n, r in results if r == "equivalent"]
    print("%d breaks, %d caught, %d proven equivalent (%s); missed: %s" %
          (len(results), len(results) - len(missed) - len(same), len(same), " ".join(same),
           " ".join(missed) or "none"))
    return 1 if missed else 0


if __name__ == "__main__":
    sys.exit(main())
