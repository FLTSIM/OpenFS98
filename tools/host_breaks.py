#!/usr/bin/env python3
"""Break OpenFS98's HOST (src/host/host.c) on purpose, one place at a time, and
check that hostdiff notices.

Each break is a small edit to a copy of host.c -- the kind of slip a
transliteration makes -- built as the harness's HOST_ours.dll and run against
the real host for N iterations (hostdiff exits non-zero when any slot differs).
At the end the real build goes back.  A missed break is a gap in the harness or
an equivalence; either way it is reported.

Usage: tools/host_breaks.py [N] [name...]   (from the repository root, after
       make hostdiff)
"""
import os
import shutil
import subprocess
import sys

args = sys.argv[1:]
N = int(args.pop(0)) if args and args[0].isdigit() else 4000
ONLY = args
BREAK = "bin/difftest/host_break.c"

# (name, old, new[, why it cannot show]).  A fourth element marks a break
# proven equivalent, reported as such.
BREAKS = [
    ("const-one",   "static uint32_t FS98_STDCALL host_01c(void)\n{\n    return 1;",
                    "static uint32_t FS98_STDCALL host_01c(void)\n{\n    return 2;"),
    ("global-20",   "return M32(V(0x41719c));", "return M32(V(0x417198));"),
    ("global-34",   "return M32(V(0x418850));", "return M32(V(0x418854));"),
    ("past-table",  "return V(0x417ec0);", "return V(0x417ec4);"),
    ("struct-5c",   "return V(0x41c240);", "return V(0x41c244);"),
    ("struct-60",   "return V(0x41bb80);", "return V(0x41bb84);"),
    ("copy-18-a",   "M32(V(0x418830)) = M32(arg + 0x18);", "M32(V(0x418830)) = M32(arg + 0x1c);"),
    ("copy-18-f",   "M32(V(0x418844)) = M32(arg + 0xbc);", "M32(V(0x418844)) = M32(arg + 0xc0);"),
    ("set-2c",      "M32(V(0x4187ec)) = 1;", "M32(V(0x4187ec)) = 2;"),
    ("read-54-a",   "M32(out1) = M32(V(0x41880c));", "M32(out1) = M32(V(0x418810));"),
    ("read-54-b",   "M32(out2) = M32(V(0x418828));", "M32(out2) = M32(V(0x41882c));"),
    ("strcpy-nul",  "    while ((*d++ = *s++) != '\\0')\n        ;\n    return dst;\n}\n\n/* +0x2b0",
                    "    while ((*d++ = *s++) != '\\0')\n        ;\n    d[-1] = 'x';\n    return dst;\n}\n\n/* +0x2b0"),
    ("strcat-start","    while (*d != '\\0')\n        d++;\n    while ((*d++ = *s++) != '\\0')",
                    "    while (*d != '\\0')\n        d++;\n    d++;\n    while ((*d++ = *s++) != '\\0')"),
    ("strlen-null", "    if (p == 0)\n        return 0;", "    if (p == 0)\n        return 1;"),
    ("strlen-len",  "    while (p[n] != '\\0')\n        n++;\n    return n;",
                    "    while (p[n] != '\\0')\n        n++;\n    return n + 1;"),
    ("strcpy-const","const char *s = (const char *)(uintptr_t)V(0x41bca0);",
                    "const char *s = (const char *)(uintptr_t)V(0x41bca1);"),
    ("unc-byte",    "p[0] == 0x5c && p[1] == 0x5c", "p[0] == 0x5c && p[1] == 0x2f"),
    ("unc-null",    "return (p != 0 && p[0]", "return (p[0]"),
    ("memset-val",  "d[i] = (uint8_t)val;", "d[i] = (uint8_t)(val + 1);"),
    ("memset-count","for (i = 0; i < count; i++)\n        d[i]", "for (i = 0; i + 1 < count; i++)\n        d[i]"),
    ("memcmp-sign", "return p[i] < q[i] ? (uint32_t)-1 : 1;", "return p[i] < q[i] ? 1 : (uint32_t)-1;"),
    ("set-288",     "M32(V(0x417194)) = v;", "M32(V(0x417194)) = v + 1;"),
    ("compact-byte","M8(dst + 3 * i + 2) = M8(V(0x4171a4) + 4 * i + 2);",
                    "M8(dst + 3 * i + 2) = M8(V(0x4171a4) + 4 * i + 3);"),
    ("compact-stride", "M8(dst + 3 * i + 0) = M8(V(0x4171a4) + 4 * i + 0);",
                       "M8(dst + 3 * i + 0) = M8(V(0x4171a4) + 3 * i + 0);"),
    ("strcmp-sign", "return *p < *q ? (uint32_t)-1 : 1;", "return *p < *q ? 1 : (uint32_t)-1;"),
    ("flag-2cc",    "return M32(V(0x41a3cc)) == 0 ? 1 : 0;", "return M32(V(0x41a3cc)) == 0 ? 0 : 1;"),
    ("clear-304",   "M32(p) = 0;\n    return p;", "M32(p) = 1;\n    return p;"),
    ("stub-108",    "static uint32_t FS98_STDCALL host_zero2(uint32_t a, uint32_t b) { (void)a; (void)b; return 0; }",
                    "static uint32_t FS98_STDCALL host_zero2(uint32_t a, uint32_t b) { (void)a; (void)b; return 1; }"),
]


def main():
    results = []
    src = open("src/host/host.c").read()
    for entry in BREAKS:
        name, old, new = entry[:3]
        if ONLY and name not in ONLY:
            continue
        if src.count(old) != 1:
            print("%-16s cannot apply: %d matches" % (name, src.count(old)))
            results.append((name, "unapplied"))
            continue
        open(BREAK, "w").write(src.replace(old, new))
        if os.path.exists("bin/difftest/HOST_ours.dll"):
            os.remove("bin/difftest/HOST_ours.dll")
        build = subprocess.run(["make", "-s", "host-break-build", "HOST_BREAK=" + BREAK],
                               capture_output=True, text=True)
        if build.returncode != 0:
            print("%-16s does not build:\n%s" % (name, build.stderr[-1500:]))
            results.append((name, "unbuilt"))
            continue
        run = subprocess.run(["wine", "hostdiff.exe", str(N)], cwd="bin/difftest",
                             capture_output=True, text=True, errors="replace",
                             env=dict(os.environ, WINEDEBUG="-all"))
        caught = run.returncode != 0
        last = [l for l in run.stdout.splitlines() if l.startswith("hostdiff:")]
        verdict = "caught" if caught else "equivalent" if len(entry) > 3 else "MISSED"
        print("%-16s %s  %s%s" % (name, verdict, last[-1] if last else run.stdout.strip()[-120:],
                                  "  (" + entry[3] + ")" if len(entry) > 3 and not caught else ""))
        results.append((name, verdict.lower()))
        sys.stdout.flush()
    if os.path.exists(BREAK):
        os.remove(BREAK)
    subprocess.run(["make", "-s", "hostdiff"], check=True)
    missed = [n for n, r in results if r not in ("caught", "equivalent")]
    same = [n for n, r in results if r == "equivalent"]
    print("host: %d breaks, %d caught, %d equivalent (%s); missed: %s" %
          (len(results), len(results) - len(missed) - len(same), len(same), " ".join(same),
           " ".join(missed) or "none"))
    return 1 if missed else 0


if __name__ == "__main__":
    sys.exit(main())
