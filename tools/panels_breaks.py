#!/usr/bin/env python3
"""Break OpenFS98's PANELS on purpose, one place at a time, and check that
panelsdiff notices.

PANELS is 5,800 lines, so the breaks are generated rather than listed: the
kind of slip a transliteration makes -- an offset one field out, a boundary
off by one, a test inverted, && for ||, a sign, a constant -- is applied at
one place of one line of src/panels/panels.c at a time, from a fixed
sample.  Each mutant is built as the harness's ours and run: first the unit
mode for the routine it is in, if it is one of the image routines (fast),
then the whole-module mode for N scenarios.  A mutant that neither run
tells from retail is MISSED: dead code, an equivalence, or a gap in the
harness, and docs/modules/panels.md says which.

The mutants of a run are independent, so they run in parallel, each in a
directory of its own (bin/difftest_brk<k>, beside bin/difftest so that the
game's path is the same).

Usage: tools/panels_breaks.py [N] [COUNT] [SEED] [name...]   (from the
       repository root, after make panelsdiff panelsunit)
  N      scenarios of the whole-module mode per mutant (default 150)
  COUNT  how many mutants to try (default 200)
  SEED   which sample (default 1)
  name   try only these (names are printed)
"""
import concurrent.futures
import os
import random
import re
import shutil
import subprocess
import sys

args = [a for a in sys.argv[1:]]
nums = [int(a) for a in args if a.isdigit()]
N = nums[0] if len(nums) > 0 else 150
COUNT = nums[1] if len(nums) > 1 else 200
SEED = nums[2] if len(nums) > 2 else 1
ONLY = [a for a in args if not a.isdigit()]
SRC = "src/panels/panels.c"
WORKERS = 8
UNIT_FUNCS = {
    "scale_24", "scale_8", "smooth_8", "smooth_24", "shrink_24", "shrink_8", "shrink_8_to_8",
    "shrink_8_to_8_plain", "surface_from_bitmap", "to_8", "to_8_host", "to_555", "to_565",
    "light_24", "light_32", "mask_make", "copy_8", "surface_convert", "clip", "edges_24", "blit",
    "compose", "dib_new", "surface_over", "surface_over_free", "bitmap_size", "bitmap_load",
    "bounds_build", "colour_8",
}
# helpers the unit routines call, tried under the routine that matters most
UNIT_FOR = {"colour_8": "shrink_8_to_8", "var_lookup": "var_lookup", "var_init": "var_lookup"}


def functions(lines):
    """(start line, end line, name) of each function body"""
    out = []
    i = 0
    while i < len(lines):
        m = re.match(r"static [^;{]*?\b(\w+)\(", lines[i])
        if m and not lines[i].rstrip().endswith(";"):
            j = i
            while j < len(lines) and "{" not in lines[j] and not lines[j].rstrip().endswith(";"):
                j += 1
            if j < len(lines) and lines[j].startswith("{"):
                k = j
                while k < len(lines) and lines[k] != "}":
                    k += 1
                out.append((j + 1, k, m.group(1)))
                i = k
        i += 1
    return out


# (what, pattern, replacement function)
OPS = [
    ("off4", re.compile(r"\b(R32|RFL)\((\w+(?:\s*\+\s*\w+)?), 0x([0-9a-f]+)\)"),
     lambda m: "%s(%s, 0x%x)" % (m.group(1), m.group(2), int(m.group(3), 16) + 4)),
    ("off2", re.compile(r"\b(R16)\((\w+), 0x([0-9a-f]+)\)"),
     lambda m: "R16(%s, 0x%x)" % (m.group(2), int(m.group(3), 16) + 2)),
    ("off1", re.compile(r"\b(R8)\((\w+), 0x([0-9a-f]+)\)"),
     lambda m: "R8(%s, 0x%x)" % (m.group(2), int(m.group(3), 16) + 1)),
    ("eq", re.compile(r" == "), lambda m: " != "),
    ("ne", re.compile(r" != "), lambda m: " == "),
    ("lt", re.compile(r" < (?!<)"), lambda m: " <= "),
    ("gt", re.compile(r" > (?!>)"), lambda m: " >= "),
    ("le", re.compile(r" <= "), lambda m: " < "),
    ("ge", re.compile(r" >= "), lambda m: " > "),
    ("and", re.compile(r" && "), lambda m: " || "),
    ("or", re.compile(r" \|\| "), lambda m: " && "),
    ("add", re.compile(r"(?<=[\w)\]]) \+ (?=[\w(])"), lambda m: " - "),
    ("sub", re.compile(r"(?<=[\w)\]]) - (?=[\w(])"), lambda m: " + "),
    ("shr", re.compile(r" >> (\d+)"), lambda m: " >> %d" % (int(m.group(1)) + 1)),
    ("shl", re.compile(r" << (\d+)"), lambda m: " << %d" % (int(m.group(1)) + 1)),
    ("const", re.compile(r"(?<![\w.])0x([0-9a-f]{1,3})\b(?!\))"),
     lambda m: "0x%x" % (int(m.group(1), 16) ^ 1)),
]


def candidates(lines):
    funcs = functions(lines)
    out = []
    for start, end, name in funcs:
        if name in ("main", "panels_install", "routine", "DllMain"):
            continue
        for ln in range(start, end):
            text = lines[ln]
            s = text.strip()
            if not s or s.startswith(("/*", "*", "//", "#")) or "(void)" in s:
                continue
            code = text
            for what, pat, rep in OPS:
                for m in pat.finditer(code):
                    new = code[:m.start()] + rep(m) + code[m.end():]
                    if new != code:
                        out.append(("%s:%d:%s%d" % (name, ln + 1, what, m.start()), ln, new, name))
    return out


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors="replace", **kw)


def attempt(item, k, source_lines):
    name, ln, new, func = item
    d = "bin/difftest_brk%d" % k
    os.makedirs(d, exist_ok=True)
    for f in ("panelsdiff.exe", "PANELS_retail.dll"):
        if not os.path.exists(os.path.join(d, f)):
            shutil.copy(os.path.join("bin/difftest", f), os.path.join(d, f))
    tmp = os.path.join(d, "panels_break.c")
    lines = list(source_lines)
    lines[ln] = new
    open(tmp, "w").write("\n".join(lines) + "\n")
    outs = []
    unit_name = UNIT_FOR.get(func, func)
    for flavour, dll, extra, deff in (("unit", "PANELS_unit.dll", "-DPANELS_UNIT", "src/panels/panels_unit.def"),
                                      ("module", "PANELS_ours.dll", "", "src/panels/panels.def")):
        if flavour == "unit" and unit_name not in UNIT_FUNCS and unit_name != "var_lookup":
            continue
        out = os.path.join(d, dll)
        if os.path.exists(out):
            os.remove(out)
        build = run(["make", "-s", "panels-break-build", "PANELS_SRC=" + tmp, "PANELS_OUT=" + out,
                     "PANELS_DEF=" + deff, "PANELS_BREAK_FLAGS=" + extra])
        if build.returncode != 0:
            return name, "unbuilt", build.stderr[-300:]
        env = dict(os.environ, WINEDEBUG="-all")
        if flavour == "unit":
            env["PANELS_UNIT"] = unit_name
            r = run(["wine", "panelsdiff.exe", "unit", "400"], cwd=d, env=env, timeout=900)
        else:
            r = run(["wine", "panelsdiff.exe", str(N)], cwd=d, env=env, timeout=1800)
        outs.append((flavour, r.returncode))
        if r.returncode != 0:
            last = [l for l in r.stdout.splitlines() if "differ" in l or "HANG" in l or "fault" in l]
            return name, "caught", "%s: %s" % (flavour, (last[-1] if last else "exit %d" % r.returncode)[:100])
    return name, "MISSED", " ".join("%s ok" % f for f, _ in outs)


def main():
    import queue
    lines = open(SRC).read().split("\n")
    cands = candidates(lines)
    if ONLY:
        picked = [c for c in cands if c[0] in ONLY]
    else:
        rnd = random.Random(SEED)
        rnd.shuffle(cands)
        picked = cands[:COUNT]
    print("%d candidate mutants, %d tried" % (len(cands), len(picked)))
    sys.stdout.flush()
    slots = queue.Queue()
    for k in range(WORKERS):
        slots.put(k)

    def task(item):
        k = slots.get()
        try:
            try:
                return attempt(item, k, lines) + (item,)
            except subprocess.TimeoutExpired:
                return item[0], "caught", "hang (timeout)", item
        finally:
            slots.put(k)

    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=WORKERS) as ex:
        for name, verdict, note, item in ex.map(task, picked):
            print("%-34s %-8s %s" % (name, verdict, note))
            sys.stdout.flush()
            results.append((name, verdict, item))
    missed = [(n, i) for n, v, i in results if v == "MISSED"]
    unbuilt = [n for n, v, i in results if v == "unbuilt"]
    print("%d mutants, %d caught, %d missed, %d did not build" %
          (len(results), sum(v == "caught" for _, v, _ in results), len(missed), len(unbuilt)))
    for n, i in missed:
        print("  MISSED %-30s %s" % (n, lines[i[1]].strip()[:100]))
    return 1 if missed else 0


if __name__ == "__main__":
    sys.exit(main())
