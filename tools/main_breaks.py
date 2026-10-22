#!/usr/bin/env python3
"""Break OpenFS98's MAIN on purpose, one place at a time, and check that
maindiff notices.

MAIN is done a slice at a time, so the breaks are grouped by the suite
that verifies them.  Each is a small edit to one file of a copy of
src/main -- the kind of slip a transliteration makes -- built as the
harness's MAIN_ours.dll and run for N scenarios of its suite (maindiff
exits non-zero when any differ).  At the end the real build goes back.  A
missed break is a gap in the harness or an equivalence; either way it is
reported, and docs/modules/main.md says which.

Usage: tools/main_breaks.py [N] [suite] [name...]   (from the repository
       root, after make maindiff)
"""
import os
import shutil
import subprocess
import sys

args = sys.argv[1:]
N = int(args.pop(0)) if args and args[0].isdigit() else 2000
SUITE = args.pop(0) if args else "records"
ONLY = args
COPY = "bin/difftest/main_break"

# suite -> [(name, file, old, new[, why it cannot show])].  A fifth element
# marks a break proven equivalent, reported as such.
BREAKS = {
    "records": [
        ("end-empty", "main_records.inc", "    if (M32(S_PENDING) == 0)\n        return 0;\n    H_ALLOC(1, M32(S_PENDING), ADDR(&zeros), 1);",
         "    if (M32(S_PENDING) == 0)\n        return 1;\n    H_ALLOC(1, M32(S_PENDING), ADDR(&zeros), 1);"),
        ("end-zero", "main_records.inc", "ADDR(&zeros), 1);", "ADDR(&zeros), 0);"),
        ("end-clear", "main_records.inc", "    r = H_FREE(ADDR(&zeros));\n    M32(S_PENDING) = 0;", "    r = H_FREE(ADDR(&zeros));"),
        ("current-none", "main_records.inc", "    if (M32(S_RECORD) == 0)\n        return 0xffffffffu;\n    M32(out)",
         "    if (M32(S_RECORD) == 0)\n        return 0;\n    M32(out)"),
        ("read-zero", "main_records.inc", "    if (take == 0 || n == 0) {", "    if (take == 0) {"),
        ("read-got", "main_records.inc", "        if (got != 0)\n            M32(got) = 0;\n        return 0xffffffffu;",
         "        return 0xffffffffu;"),
        ("read-clamp", "main_records.inc", "    if (take >= n)\n        take = n;", "    if (take > n + 1)\n        take = n;"),
        ("read-advance", "main_records.inc", "    M32(S_READ_AT) = M32(S_READ_AT) + take;\n", ""),
        ("read-left", "main_records.inc", "    M32(S_READ_LEFT) = M32(S_READ_LEFT) - take;\n", ""),
        ("dispatch-left", "main_records.inc", "    M32(S_READ_LEFT) = M32(rec + 8);", "    M32(S_READ_LEFT) = M32(rec + 4);"),
        ("dispatch-at", "main_records.inc", "    M32(S_READ_AT) = M32(rec + 0xc);", "    M32(S_READ_AT) = M32(rec + 0xc) + 1;"),
        ("dispatch-clear", "main_records.inc", "    r = CHAIN_SEND(0x14, 0x22, rec);\n    M32(S_RECORD) = 0;", "    r = CHAIN_SEND(0x14, 0x22, rec);"),
        ("dispatch-message", "main_records.inc", "CHAIN_SEND(0x14, 0x22, rec)", "CHAIN_SEND(0x14, 0x22, 0)"),
        ("save-pad", "main_records.inc", "    CHAIN_SEND(0x14, 0x23, 0);\n    r = record_end();", "    CHAIN_SEND(0x14, 0x23, 0);\n    r = 0;"),
        ("save-clear", "main_records.inc", "    r = record_end();\n    M32(S_FILE) = 0;", "    r = record_end();"),
        ("begin-size", "main_records.inc", "    if (M32(S_FILE) == 0 || want == 0)", "    if (M32(S_FILE) == 0)"),
        ("begin-end", "main_records.inc", "        return 0xffffffffu;\n    record_end();\n    word = tag;", "        return 0xffffffffu;\n    word = tag;"),
        ("begin-small", "main_records.inc", "    if ((int32_t)tag < 1000) {", "    if ((int32_t)tag <= 1000) {"),
        ("begin-signed", "main_records.inc", "    if ((int32_t)tag < 1000) {", "    if (tag < 1000) {"),
        ("begin-short-len", "main_records.inc", "        word = want + 4;", "        word = want + 2;"),
        ("begin-long-len", "main_records.inc", "        word = want + 6;", "        word = want + 4;"),
        ("begin-long-width", "main_records.inc", "ADDR(&word), 4, ADDR(&size));", "ADDR(&word), 2, ADDR(&size));"),
        ("begin-pending", "main_records.inc", "    M32(S_PENDING) = want;", "    M32(S_PENDING) = size;"),
        ("write-none", "main_records.inc", "    if (M32(S_FILE) == 0 || owed == 0) {", "    if (owed == 0) {",
         "+0x9760 is set non-zero only by +0x5c, which needs a file, and the save zeroes it before it clears the file"),
        ("write-clamp", "main_records.inc", "    if (n < owed) {", "    if (n <= owed) {",
         "at n == owed both branches leave nothing owed and n as it was"),
        ("write-count", "main_records.inc", "        M32(wrote) = done;", "        M32(wrote) = n;"),
        ("find-size-zero", "main_records.inc", "    if (size != 0)\n        M32(size) = 0;\n    M32(out) = 0;", "    M32(out) = 0;"),
        ("find-ext", "main_records.inc", "    if (H_EXISTS(path, 2) != 0)", "    if (H_EXISTS(path, 0) != 0)"),
        ("find-open-mode", "main_records.inc", "H_FILE_OPEN(ADDR(&file), path, 2, 0)", "H_FILE_OPEN(ADDR(&file), path, 2, 1)"),
        ("find-small", "main_records.inc", "            if ((int32_t)id < 1000) {", "            if ((int32_t)id < 999) {"),
        ("find-header", "main_records.inc", "M8(buf + at) - 4;", "M8(buf + at) - 2;"),
        ("find-end", "main_records.inc", "            } else if (id == 0xffff) {", "            } else if (id == 0xfffe) {"),
        ("find-long", "main_records.inc", "                         M8(buf + at + 1)) << 8) + M8(buf + at) - 6;", "                         M8(buf + at + 1)) << 8) + M8(buf + at) - 4;"),
        ("find-long-step", "main_records.inc", "                at += 4;", "                at += 2;"),
        ("find-copy-at", "main_records.inc", "H_MEMMOVE(M32(out), buf + at, body);", "H_MEMMOVE(M32(out), buf + at - 2, body);"),
        ("find-size-out", "main_records.inc", "                if (size != 0)\n                    M32(size) = body;\n", ""),
        ("find-step", "main_records.inc", "            at += body;\n        } while (at < length);", "            at += body + 1;\n        } while (at < length);"),
        ("find-bound", "main_records.inc", "        } while (at < length);", "        } while ((int32_t)at < (int32_t)length);"),
        ("find-free", "main_records.inc", "    H_FREE(ADDR(&buf));\n    return 0xffffffffu;\n}", "    return 0xffffffffu;\n}"),
        ("flight-path", "main_records.inc", "#define S_FIND_PATH   V(0x208040dc)", "#define S_FIND_PATH   V(0x208040dd)"),
    ],
}


def main():
    results = []
    for entry in BREAKS[SUITE]:
        name, path, old, new = entry[:4]
        if ONLY and name not in ONLY:
            continue
        source = open(os.path.join("src/main", path)).read()
        if source.count(old) != 1:
            print("%-20s cannot apply: %d matches" % (name, source.count(old)))
            results.append((name, "unapplied"))
            continue
        shutil.rmtree(COPY, ignore_errors=True)
        shutil.copytree("src/main", COPY)
        open(os.path.join(COPY, path), "w").write(source.replace(old, new))
        if os.path.exists("bin/difftest/MAIN_ours.dll"):
            os.remove("bin/difftest/MAIN_ours.dll")
        build = subprocess.run(["make", "-s", "main-break-build", "MAIN_BREAK_DIR=" + COPY],
                               capture_output=True, text=True)
        if build.returncode != 0:
            print("%-20s does not build:\n%s" % (name, build.stderr[-2000:]))
            results.append((name, "unbuilt"))
            continue
        run = subprocess.run(["wine", "maindiff.exe", str(N), SUITE], cwd="bin/difftest",
                             capture_output=True, text=True,
                             env=dict(os.environ, WINEDEBUG="-all"))
        caught = run.returncode != 0
        last = [l for l in run.stdout.splitlines() if l.startswith("maindiff")]
        verdict = "caught" if caught else "equivalent" if len(entry) > 4 else "MISSED"
        print("%-20s %s  %s%s" % (name, verdict, last[0] if last else run.stdout.strip()[-200:],
                                  "  (" + entry[4] + ")" if len(entry) > 4 and not caught else ""))
        results.append((name, verdict.lower()))
        sys.stdout.flush()
    shutil.rmtree(COPY, ignore_errors=True)
    subprocess.run(["make", "-s", "maindiff"], check=True)
    missed = [n for n, r in results if r not in ("caught", "equivalent")]
    same = [n for n, r in results if r == "equivalent"]
    print("%s: %d breaks, %d caught, %d proven equivalent (%s); missed: %s" %
          (SUITE, len(results), len(results) - len(missed) - len(same), len(same), " ".join(same),
           " ".join(missed) or "none"))
    return 1 if missed else 0


if __name__ == "__main__":
    sys.exit(main())
