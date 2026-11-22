#!/usr/bin/env python3
"""Fail on raw retail data in git (Story 1.4 / AD-12), against a shrinking allowlist.

Offenders: (a) a tracked file under src/ or include/ with RUN or more consecutive
lines of at least PER_LINE integer literals and nothing else; (b) a tracked file
byte-identical to a file under orig/ (skipped when orig/ is absent); (c) every
tools/<name>.py run from cmake/steps/*.cmake or cmake/targets.cmake whose command, source or
transitively imported tools/ module mentions orig as a path segment.  tools/raw_dumps_allowed.txt
lists (a)/(b) files by path and (c) scripts as gen:/resource:/oracle:<script>.py  # reason.  A
new offender, or a listed entry that is no longer one, exits 1.
Usage: tools/check_raw_dumps.py
"""
import glob, hashlib, os, re, subprocess, sys

RUN, PER_LINE = 8, 8
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
INT = r"(?:0[xX][0-9a-fA-F]+|-?\d+)[uUlL]*"
LINE = re.compile(r"^\s*(?:\[[^\]]*\]\s*=\s*)?(%s)(?:\s*,\s*%s)*\s*,?\s*$" % (INT, INT))
LIT = re.compile(INT)
RUN_LITERALS = 128   # rule 2: literals in a run of one-element-per-line style lines
ELEM = re.compile(r"^(?:%s|[A-Za-z_]\w*)$" % INT)

def rule1(lines):
    run = 0
    for line in lines:
        line = re.sub(r"/\*.*?\*/|//.*", "", line).rstrip()
        line = line[:-1] if line.endswith("\\") else line
        dump = LINE.match(line) and len(LIT.findall(re.sub(r"^\s*\[[^\]]*\]\s*=", "", line))) >= PER_LINE
        run = run + 1 if dump else 0
        if run >= RUN:
            return True
    return False

def rule2(lines):
    total = 0
    for line in lines:
        t = re.sub(r"/\*.*?\*/|//.*", "", line).rstrip()
        t = t[:-1] if t.endswith("\\") else t   # macro continuation
        t = re.sub(r"\[[^\]]*\]\s*=|\(\s*[A-Za-z_][\w ]*\*?\s*\)", "", t).strip().rstrip(",")
        elems = [e.strip() for e in t.split(",")] if t else []
        if elems and all(ELEM.match(e) for e in elems):
            total += sum(1 for e in elems if LIT.fullmatch(e))
            if total >= RUN_LITERALS:
                return True
        else:
            total = 0
    return False

def is_dump(lines):
    return rule1(lines) or rule2(lines)

assert is_dump(["    " + ", ".join(["0x%02x" % i] * 8) + "," for i in range(8)])
assert is_dump(["    /* +%04x */ (void *)0x%08xu," % (4 * i, i) for i in range(130)])
assert is_dump(["    /* x */ " + ", ".join(["0x%02x" % i] * 8) + "," for i in range(8)])
assert is_dump(["    [%d] = 0x%02x, \\" % (i, i) for i in range(130)])   # the old global_state.inc form
assert not is_dump(["    [%d] = 0x%02x, \\" % (i, i) for i in range(10)])
assert not is_dump(["    { %d, \"name%d\", flags_%d }," % (i, i, i) for i in range(20)])

def sha(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()

tracked = subprocess.check_output(["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"], text=True).split("\0")
tracked = [f for f in tracked if f and os.path.isfile(f)]
found = set()

for f in tracked:                                    # (a)
    if f.startswith(("src/", "include/")):
        if is_dump(open(f, encoding="latin-1").read().splitlines()):
            found.add(f)

have_orig = os.path.isdir("orig")
if have_orig:                                        # (b)
    sizes = {os.path.getsize(os.path.join(d, n))
             for d, _, names in os.walk("orig") for n in names}
    origs = {sha(os.path.join(d, n)) for d, _, names in os.walk("orig") for n in names}
    for f in tracked:
        if os.path.getsize(f) in sizes and sha(f) in origs:
            found.add(f)
else:
    print("note: orig/ absent, hash check (b) and its stale src/ entries skipped")

ORIG = re.compile(r"\borig\b")

def tools_imports(name, seen):
    """Transitive closure of tools/ modules a script imports (anywhere in its source)."""
    if name in seen or not os.path.isfile("tools/%s.py" % name):
        return
    seen.add(name)
    for m in re.finditer(r"^\s*(?:from\s+(\w+)\s+import|import\s+([\w, ]+))", open("tools/%s.py" % name).read(), re.M):
        for mod in re.sub(r"\s+as\s+\w+", "", m.group(1) or m.group(2)).replace(" ", "").split(","):
            tools_imports(mod, seen)

def reads_orig(command, script):
    mods = set()
    tools_imports(script, mods)
    return bool(ORIG.search(command)) or any(ORIG.search(open("tools/%s.py" % x).read()) for x in mods)

def cmake_scripts():
    """{script: command} for every tools/<name>.py run from a scanned CMake file; commands are joined over backslash continuations."""
    found = {}
    for c in glob.glob("cmake/steps/*.cmake") + ["cmake/targets.cmake"]:
        for command in re.sub(r"\\\n\s*", " ", open(c).read()).splitlines():
            for m in re.finditer(r"tools/(\w+)\.py", command):
                if os.path.isfile("tools/%s.py" % m.group(1)):
                    found.setdefault(m.group(1), []).append(command)
    return found

_s = set(); tools_imports("gen_world_runtime", _s); assert "dialogmap" in _s
assert ORIG.search("Path('orig')") and ORIG.search("x orig/Modules") and not ORIG.search("original")

reading = {name for name, cmds in cmake_scripts().items() if any(reads_orig(c, name) for c in cmds)}
entries = {}   # allowlist: path or kind:script -> reason
for l in open("tools/raw_dumps_allowed.txt"):
    e, _, why = l.partition("#")
    if e.strip():
        entries[e.strip()] = why.strip()
KINDS = ("gen", "resource", "oracle")
by_kind = {}
for e in entries:
    if e.split(":")[0] in KINDS:
        by_kind.setdefault(e.split(":", 1)[1], []).append(e.split(":")[0])
bad = False
for e in sorted(entries):
    if e.split(":")[0] in KINDS:
        continue
    if not e.startswith(("src/", "include/")) or ":" in e:
        print("malformed allowlist entry (unknown kind, or not a src/ or include/ path):", e); bad = True
for f in sorted(found - set(entries)):
    print("NEW raw dump:", f); bad = True
for f in sorted(f for f in entries if f.startswith(("src/", "include/")) and f not in found):
    if have_orig or not entries[f].startswith("(b)"):   # without orig/, only (b)-tagged entries are unknowable
        print("allowlist entry is no longer an offender (remove it):", f); bad = True
for n in sorted(reading):
    kinds = by_kind.get(n + ".py", [])
    if len(kinds) != 1:
        print("script reads orig/ and needs exactly one of gen:/resource:/oracle: in the allowlist:", n + ".py", kinds); bad = True
for n in sorted(by_kind):
    if n[:-3] not in reading:
        print("allowlist entry is stale, the script no longer reads orig/ or is no longer run by CMake:", n); bad = True
if bad:
    sys.exit(1)
print("raw dumps: %d allowed dump files, %d scripts reading orig/ (%s), none new" % (len(found),
      len(reading), ", ".join("%d %s" % (sum(k[0] == x for k in by_kind.values()), x) for x in KINDS)))
