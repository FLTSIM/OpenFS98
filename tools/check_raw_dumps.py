#!/usr/bin/env python3
"""Fail on raw retail data in git (Story 1.4 / AD-12), against a shrinking allowlist.

Offenders: (a) a tracked file under src/ or include/ with RUN or more consecutive
lines of at least PER_LINE integer literals and nothing else; (b) a tracked file
byte-identical to a file under orig/ (skipped when orig/ is absent); (c) every
tools/<name>.py run from cmake/steps/*.cmake or cmake/targets.cmake whose command, source or
transitively imported tools/ module mentions orig as a path segment.  tools/raw_dumps_allowed.txt
lists (a)/(b) files by path and (c) scripts as gen:/resource:/oracle:<script>.py  # reason.  A
new offender, or a listed entry that is no longer one, exits 1.  (c) also follows the tools/*.sh
wrappers CMake runs: each python3 tools/x.py (or "$D"/x.py) line in a wrapper, and each other wrapper it runs;
a script reached that way reads orig/ when its own invocation line, source or imports say so.
Ratchet: dump-file and gen: entries may only be removed relative to HEAD's allowlist (pending edits, not
history; skipped with a note when HEAD has none).  The one exception, a dump file newly caught by a stricter
rule in this same change, needs a "stricter rule:" reason, the file unchanged since HEAD, a pending edit to
this file, and HEAD's own rules (extracted from HEAD's copy of this file) not flagging it.
Usage: tools/check_raw_dumps.py [--self-test]
"""
import glob, hashlib, os, re, shlex, subprocess, sys

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

# ---- the detector rules end here; the ratchet runs HEAD's copy of everything above this line (keep it first) ----
def sha(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()

tracked = subprocess.check_output(["git", "-C", ROOT, "ls-files", "-z", "--cached", "--others", "--exclude-standard"], text=True,
                                  env={k: v for k, v in os.environ.items() if k not in ("GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE")}).split("\0")
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

HERE = re.compile(r'\$\(\s*dirname\s+(?:--\s+)?"\$0"\s*\)')
ASSIGN = re.compile(r"^([A-Za-z_]\w*)=(.*)$", re.S)
SCRIPT_WORD = re.compile(r"^(?:\./)?(?:tools|\$D|\$\{D\}|__HERE__)/(\w+)\.(py|sh)$")
LEAD = {"exec", "command", "time", "nohup", "then", "do", "else", "elif", "if", "while", "until", "!", "{"}

def shell_commands(text):
    """The commands of a shell script as word lists: backslash-newline removed, quotes and comments handled by shlex,
    here-doc bodies skipped, split on ; & && || | and parentheses.
    shortcut: a `#` inside a word (`${#x}`) ends the line and an unterminated quote ends the scan, upgrade if a wrapper needs either."""
    lines = HERE.sub("__HERE__", text.replace("\\\n", "")).split("\n")
    out, i = [], 0
    while i < len(lines):
        buf, i = lines[i], i + 1
        while True:
            lex = shlex.shlex(buf, posix=True, punctuation_chars=";&|()")
            lex.whitespace_split = True
            try:
                toks = list(lex)
                break
            except ValueError:
                if i >= len(lines):
                    return out
                buf, i = buf + "\n" + lines[i], i + 1
        doc = None
        for k, t in enumerate(toks):
            m = re.fullmatch(r"<<(?!<)-?(\S+)", t)   # not <<<, a here-string
            if m:
                doc = m.group(1)
            elif t in ("<<", "<<-") and k + 1 < len(toks):
                doc = toks[k + 1]
        cmd = []
        for t in toks + [";"]:
            if set(t) <= set(";&|()"):
                if cmd:
                    out.append(cmd)
                cmd = []
            else:
                cmd.append(t)
        while doc is not None and i < len(lines):
            i += 1
            if lines[i - 1].strip() == doc:
                break
    return out

def python_script(args):
    """(script, its arguments) of a python3 command line, or (None, []): options (those taking a value too) are skipped; -c, -m (bundled or attached) and - mean no script."""
    i = 0
    while i < len(args):
        a = args[i]
        if a == "--":
            return (args[i + 1], args[i + 2:]) if i + 1 < len(args) else (None, [])
        if a == "-":
            return None, []
        if a.startswith("--"):
            i += 2 if a == "--check-hash-based-pycs" else 1
            continue
        if a.startswith("-"):
            for j, c in enumerate(a[1:], 1):
                if c in "cm":
                    return None, []
                if c in "WX":
                    i += 1 if j == len(a) - 1 else 0   # the value is the next word unless attached
                    break
            i += 1
            continue
        return a, args[i + 1:]
    return None, []

def subst(w, env):
    return re.sub(r"\$(?:\{(\w+)\}|(\w+))", lambda m: env.get(m.group(1) or m.group(2), m.group(0)), w)

def wrapper_scripts(name, found, seen):
    """Add {script: [its own arguments]} for what tools/<name>.sh runs (python3 x.py, direct x.py or x.sh, bash x.sh, source x.sh),
    and recurse into the wrappers it runs; each wrapper is visited once."""
    if name in seen or not os.path.isfile("tools/%s.sh" % name):
        return
    seen.add(name)
    env = {}
    for cmd in shell_commands(open("tools/%s.sh" % name).read()):
        while cmd and (cmd[0] in LEAD or ASSIGN.match(cmd[0])):
            m = ASSIGN.match(cmd[0])
            if m and "$" not in subst(m.group(2), env):
                env[m.group(1)] = subst(m.group(2), env)
            cmd = cmd[1:]
        cmd = [subst(w, env) for w in cmd]
        while cmd and (cmd[0] in LEAD or cmd[0] == "nice"):
            n = 1
            if cmd[0] == "nice" and len(cmd) > 2 and cmd[1] == "-n":
                n = 3
            elif cmd[0] == "nice" and len(cmd) > 1 and re.fullmatch(r"-\d+", cmd[1]):
                n = 2
            cmd = cmd[n:]
        if not cmd:
            continue
        base, rest = os.path.basename(cmd[0]), cmd[1:]
        if re.fullmatch(r"python3?(?:\.\d+)?", base):
            script, args = python_script(rest)
        elif base in ("bash", "sh", "dash", "source", "."):
            while rest and rest[0].startswith("-") and rest[0] != "-c":
                rest = rest[1:]
            script, args = (rest[0], rest[1:]) if rest and rest[0] != "-c" else (None, [])
        else:
            script, args = cmd[0], rest
        m = SCRIPT_WORD.match(script or "")
        if m and m.group(2) == "py" and os.path.isfile("tools/%s.py" % m.group(1)):
            found.setdefault(m.group(1), []).append(" ".join(args))
        elif m and m.group(2) == "sh":
            wrapper_scripts(m.group(1), found, seen)

def cmake_scripts():
    """{script: [commands]} for every tools/<name>.py run from a scanned CMake file, or by a tools/*.sh wrapper it runs (that command's own arguments); CMake commands are joined over backslash continuations."""
    found, seen = {}, set()
    for c in sorted(glob.glob("cmake/steps/*.cmake")) + ["cmake/targets.cmake"]:
        for command in re.sub(r"\\\n\s*", " ", open(c).read()).splitlines():
            for m in re.finditer(r"tools/(\w+)\.py", command):
                if os.path.isfile("tools/%s.py" % m.group(1)):
                    found.setdefault(m.group(1), []).append(command)
            for m in re.finditer(r"tools/(\w+)\.sh", command):
                wrapper_scripts(m.group(1), found, seen)
    return found

KINDS = ("gen", "resource", "oracle")
STRICTER = "stricter rule:"
KEY = re.compile(r"\w+\.py")

def parse_allowlist(text):
    """-> ({entry: reason}, [duplicate entries])"""
    entries, dups = {}, []
    for l in text.splitlines():
        e, _, why = l.partition("#")
        if e.strip():
            if e.strip() in entries:
                dups.append(e.strip())
            entries[e.strip()] = why.strip()
    return entries, dups

def stricter_ok(reason, same_blob, checker_changed, current_flags, head_flags):
    """The exception for a new dump-file entry: every condition must hold; head_flags None (HEAD's detector could not run) fails."""
    return reason.startswith(STRICTER) and same_blob and checker_changed and current_flags and head_flags is False

def ratchet(entries, base, stricter, renames=lambda: {}):
    """Messages for entries added since base; stricter(path, reason) -> bool decides the dump-file exception.
    Only dump-file and gen: entries are ratcheted; resource: and oracle: are not, but a gen: script may not be relabelled."""
    bad = []
    for e in sorted(set(entries) - set(base)):
        kind = e.split(":")[0]
        if kind in ("resource", "oracle"):
            key = e.split(":", 1)[1]
            olds = [key] + [o for o, n in renames().items() if n == key]   # a git-detected rename carries the category
            if any("gen:" + o in base and "gen:" + o not in entries for o in olds):
                bad.append("gen: entry relabelled or renamed (it may only be removed): " + e)
            continue
        if kind == "gen" or not stricter(e, entries[e]):
            bad.append("allowlist entry added (dump-file and gen: entries may only be removed): " + e)
    return bad

GIT_ROOT = ROOT
GIT_ENV = {k: v for k, v in os.environ.items() if k not in ("GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE")}   # a pre-commit hook sets these

def git_run(*args, env=None):
    """git run from the tool's root with the hook variables removed from its environment"""
    return subprocess.run(("git", "-C", GIT_ROOT) + args, capture_output=True, text=True, env=env or GIT_ENV)

def git(*args, ok=(0,)):
    r = git_run(*args)
    if r.returncode not in ok:
        sys.exit("git %s failed: %s" % (" ".join(args), r.stderr.strip()))
    return r

def head_base(root):
    """HEAD's allowlist text, or None (with a note) for an unborn HEAD or one without the allowlist.  A repository
    whose top level is not root, or a HEAD that does not resolve and is not unborn, fails."""
    global GIT_ROOT
    GIT_ROOT = root
    top = git("rev-parse", "--show-toplevel").stdout.strip()
    if os.path.realpath(top) != os.path.realpath(root):
        sys.exit("git top level %s is not this tool's root %s; the ratchet needs the tool's own repository" % (top, root))
    if git("rev-parse", "--verify", "-q", "HEAD", ok=(0, 1)).returncode:
        ref = git("symbolic-ref", "--no-recurse", "-q", "HEAD", ok=(0, 1)).stdout.strip()
        if (not ref or git("show-ref", "--verify", "--quiet", ref, ok=(0, 1)).returncode != 1
                or git("rev-list", "-n1", "--all").stdout.strip()):
            sys.exit("HEAD does not resolve and the repository is not unborn (no commits at all)")
        print("note: HEAD is unborn, allowlist ratchet skipped")
        return None
    if not git("ls-tree", "HEAD", "tools/raw_dumps_allowed.txt").stdout.strip():
        print("note: HEAD has no tools/raw_dumps_allowed.txt, allowlist ratchet skipped")
        return None
    return git("show", "HEAD:tools/raw_dumps_allowed.txt").stdout

def rules_flags(prefix, text):
    """Whether the detector rules in the source `prefix` (HEAD's file up to `def sha(`) flag text, run unchanged in a
    subprocess with __file__ in its globals; None when they cannot be run (fail closed)."""
    import tempfile
    with tempfile.NamedTemporaryFile("w", suffix=".py", delete=False) as f:
        f.write(prefix)
    driver = ("import sys\nns = {'__file__': sys.argv[2]}\nexec(compile(open(sys.argv[1]).read(), 'HEAD', 'exec'), ns)\n"
              "print('VERDICT', bool(ns['is_dump'](sys.stdin.read().splitlines())))\n")
    try:
        r = subprocess.run([sys.executable, "-I", "-c", driver, f.name, os.path.join(ROOT, "tools/check_raw_dumps.py")],
                           input=text, capture_output=True, text=True, timeout=60, cwd=ROOT)
    except subprocess.TimeoutExpired:
        return None
    finally:
        os.unlink(f.name)
    last = r.stdout.strip().splitlines()[-1:] if r.returncode == 0 else []
    return {"VERDICT True": True, "VERDICT False": False}.get(last[0] if last else "")

def head_flags(text):
    src = git_run("show", "HEAD:tools/check_raw_dumps.py")
    if src.returncode or "\ndef sha(" not in src.stdout:
        return None
    return rules_flags(src.stdout.split("\ndef sha(")[0], text)

def head_renames():
    """{old: new} for tools/*.py that git sees as renamed since HEAD, untracked files included (a temporary index; the real one is untouched)."""
    import tempfile
    with tempfile.TemporaryDirectory() as t:
        env = dict(GIT_ENV, GIT_INDEX_FILE=os.path.join(t, "index"))
        for args in (("read-tree", "HEAD"), ("add", "-A", "--", "tools")):
            r = git_run(*args, env=env)
            if r.returncode:
                sys.exit("git %s failed: %s" % (" ".join(args), r.stderr.strip()))
        r = git_run("diff", "-M", "--name-status", "--cached", "HEAD", "--", "tools", env=env)
        if r.returncode:
            sys.exit("git diff failed: " + r.stderr.strip())
    out = {}
    for l in r.stdout.splitlines():
        f = l.split("\t")
        if f[0].startswith("R") and re.fullmatch(r"tools/\w+\.py", f[1]) and re.fullmatch(r"tools/\w+\.py", f[2]):
            out[f[1][6:]] = f[2][6:]
    return out

def real_stricter(path, reason):
    if not os.path.isfile(path):
        print("allowlist entry %s: no such file in the working tree" % path)
        return False
    text = open(path, encoding="latin-1").read()
    head_blob = git("rev-parse", "-q", "--verify", "HEAD:" + path, ok=(0, 1)).stdout.strip()
    same = bool(head_blob) and head_blob == git("hash-object", path).stdout.strip()
    changed = git("diff", "--quiet", "HEAD", "--", "tools/check_raw_dumps.py", ok=(0, 1)).returncode == 1
    current = is_dump(text.splitlines())
    flags = head_flags(text) if reason.startswith(STRICTER) and same and changed and current else None
    return stricter_ok(reason, same, changed, current, flags)

if "--self-test" in sys.argv:
    import tempfile
    with tempfile.TemporaryDirectory() as t:
        def put(rel, text):
            os.makedirs(os.path.dirname(os.path.join(t, rel)), exist_ok=True)
            open(os.path.join(t, rel), "w").write(text)
        os.chdir(t)
        put("cmake/targets.cmake", "COMMAND tools/top.sh a\nCOMMAND python3 tools/direct.py\n")
        put("tools/top.sh", "#!/bin/sh\n# python3 tools/commented.py orig\npython3 -B tools/one.py orig/x\nexec tools/mid.sh\n")
        put("tools/mid.sh", '"$(dirname "$0")/low.sh"\npython3 -B -u "$D"/two.py\n')
        put("tools/low.sh", 'python3 tools/three.py \\\n  orig\ntools/top.sh\ncase $1 in\n a) python3 tools/sib_a.py orig;;\n b) python3 tools/sib_b.py;;\nesac\n. tools/forms.sh\n')
        put("tools/forms.sh", """retail=orig/Modules
python3 tools/assigned.py "$retail"
python3 "tools/quoted.py" orig
python3 "${D}"/braced.py orig
python3 "$(dirname "$0")"/here.py orig
tools/direct_sh.py orig
exec python3 -W ignore -X dev tools/optioned.py orig
nice -n 19 python3 -B tools/niced.py orig
python3 -cpass tools/dash_c.py orig
python3 -m foo tools/dash_m.py orig
python3 tools/semi_a.py; cp orig/x y
python3 tools/commentarg.py # orig
python3 tools/joined_a\\
b.py orig
python3 tools/heredoc_a.py <<EOF
python3 tools/in_heredoc.py orig
EOF
python3 tools/after_heredoc.py orig
read -r x <<<"$changed"
python3 tools/after_herestring.py orig
python3 -- tools/dashdash.py orig
source tools/src.sh
bash -x tools/bashed.sh
""")
        put("tools/src.sh", "python3 tools/sourced.py orig\n")
        put("tools/bashed.sh", "python3 tools/bashed_py.py orig\n")
        names = ("one two three direct commented sib_a sib_b assigned quoted braced here direct_sh optioned niced dash_c dash_m "
                 "semi_a commentarg joined_ab heredoc_a in_heredoc after_heredoc after_herestring dashdash sourced bashed_py").split()
        for n in names:
            put("tools/%s.py" % n, "pass\n")
        cs = cmake_scripts()
        orig_readers = {"one", "three", "sib_a", "assigned", "quoted", "braced", "here", "direct_sh", "optioned", "niced", "joined_ab",
                        "after_heredoc", "after_herestring", "dashdash", "sourced", "bashed_py"}
        got = {n for n, c in cs.items() if any(reads_orig(x, n) for x in c)}
        assert got == orig_readers, (sorted(got ^ orig_readers))
        assert set(cs) == set(names) - {"commented", "dash_c", "dash_m", "in_heredoc"}, sorted(set(cs) ^ set(names))
        assert python_script(["-Wignore", "x.py"]) == ("x.py", []) and python_script(["-c", "x"]) == (None, []) and python_script(["-"]) == (None, [])
        # repository checks: unborn HEAD skips, another repository's subdirectory and a corrupt HEAD fail
        subprocess.run(["git", "init", "-q", "repo"], check=True)
        os.chdir("repo")
        assert head_base(os.path.realpath(".")) is None
        os.makedirs("sub")
        for root, why in ((os.path.realpath("sub"), "root mismatch"),):
            try:
                os.chdir(root); head_base(root); assert 0, why
            except SystemExit:
                pass
            os.chdir("..")
        open(".git/HEAD", "w").write("garbage\n")
        try:
            head_base(os.path.realpath(".")); assert 0, "corrupt HEAD"
        except SystemExit:
            pass
        # a dangling branch symref in a repository that has commits is not unborn; renames carry across untracked files
        os.chdir(t)
        env = dict(GIT_ENV, GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@t", GIT_COMMITTER_NAME="t", GIT_COMMITTER_EMAIL="t@t")
        subprocess.run(["git", "init", "-q", "-b", "main", "repo2"], check=True)
        GIT_ROOT = os.path.realpath("repo2")
        put("repo2/tools/g.py", "".join("line %d of the generator\n" % i for i in range(30)))
        subprocess.run(["git", "-C", "repo2", "add", "-A"], check=True)
        subprocess.run(["git", "-C", "repo2", "commit", "-qm", "x"], check=True, env=env)
        os.rename("repo2/tools/g.py", "repo2/tools/h.py")
        assert head_renames() == {"g.py": "h.py"}, head_renames()
        subprocess.run(["git", "-C", "repo2", "symbolic-ref", "HEAD", "refs/heads/master"], check=True)
        try:
            head_base(GIT_ROOT); assert 0, "dangling symref"
        except SystemExit:
            pass
        os.chdir("/")
    pre = "from __future__ import annotations\ndef is_dump(l): return len(l) > 1 and __file__.endswith('.py')\n"
    assert rules_flags(pre, "a\nb\n") is True and rules_flags(pre, "a\n") is False
    assert rules_flags("import sys\nsys.exit(0)\n", "a\n") is None and rules_flags("def nope(:\n", "a\n") is None
    base = {"src/a.c": "(a) x", "gen:g.py": "x", "oracle:o.py": "x"}
    no = lambda p, r: False
    yes = lambda p, r: True
    assert ratchet(base, base, no) == []
    assert ratchet({k: v for k, v in base.items() if k != "gen:g.py"}, base, no) == []                  # removed
    assert ratchet(dict(base, **{"src/a.c": "(b) other reason"}), base, no) == []                        # reason-only edit
    assert len(ratchet(dict(base, **{"src/b.c": "(a) y"}), base, no)) == 1                              # added dump file
    assert len(ratchet(dict(base, **{"src/b.c": "(a) y"}), base, yes)) == 0                             # ... unless the exception holds
    assert len(ratchet(dict(base, **{"gen:n.py": "y"}), base, yes)) == 1                                # never a new gen:
    renamed = {k: v for k, v in base.items() if k != "src/a.c"}; renamed["src/c.c"] = "(a) y"
    assert ratchet(renamed, base, no) == ["allowlist entry added (dump-file and gen: entries may only be removed): src/c.c"]   # a rename is an added key
    assert ratchet(dict(base, **{"resource:r.py": "y", "oracle:p.py": "y"}), base, no) == []
    relabelled = {k: v for k, v in base.items() if k != "gen:g.py"}; relabelled["oracle:g.py"] = "y"
    assert len(ratchet(relabelled, base, yes)) == 1                                                       # gen: only disappears
    assert len(ratchet({k: v for k, v in base.items() if k != "gen:g.py"} | {"oracle:h.py": "y"}, base, no, lambda: {"g.py": "h.py"})) == 1   # renamed, then relabelled
    assert ratchet({k: v for k, v in base.items() if k != "gen:g.py"} | {"oracle:h.py": "y"}, base, no, lambda: {"x.py": "h.py"}) == []
    assert parse_allowlist("a  # x\n# c\na  # y\nb\n") == ({"a": "y", "b": ""}, ["a"])
    assert stricter_ok("stricter rule: x", True, True, True, False)
    for args in (("(a) x", True, True, True, False), ("stricter rule: x", False, True, True, False), ("stricter rule: x", True, False, True, False),
                 ("stricter rule: x", True, True, False, False), ("stricter rule: x", True, True, True, True), ("stricter rule: x", True, True, True, None)):
        assert not stricter_ok(*args), args
    print("check_raw_dumps: self-test passed")
    sys.exit(0)

reading = {name for name, cmds in cmake_scripts().items() if any(reads_orig(c, name) for c in cmds)}
entries, dups = parse_allowlist(open("tools/raw_dumps_allowed.txt").read())   # path or kind:script -> reason
reading = {n + ".py" for n in reading}
by_kind = {}
bad = False
for e in entries:
    if e.split(":")[0] in KINDS:
        if KEY.fullmatch(e.split(":", 1)[1]):
            by_kind.setdefault(e.split(":", 1)[1], []).append(e.split(":")[0])
        else:
            print("malformed script entry (the key must be <name>.py):", e); bad = True
for e in dups:
    print("duplicate allowlist entry:", e); bad = True
for e in sorted(entries):
    if e.split(":")[0] in KINDS:
        continue
    if not e.startswith(("src/", "include/")) or ":" in e:
        print("malformed allowlist entry (unknown kind, or not a src/ or include/ path):", e); bad = True
    elif not entries[e].startswith(("(a)", "(b)", STRICTER)):
        print("dump-file entry needs a reason starting (a), (b) or %s: %s" % (STRICTER, e)); bad = True
base_text = head_base(ROOT)
if base_text is not None:
    for m in ratchet(entries, parse_allowlist(base_text)[0], real_stricter, head_renames):
        print(m); bad = True
for f in sorted(found - set(entries)):
    print("NEW raw dump:", f); bad = True
for f in sorted(f for f in entries if f.startswith(("src/", "include/")) and f not in found):
    if have_orig or not entries[f].startswith("(b)"):   # without orig/, only (b)-tagged entries are unknowable
        print("allowlist entry is no longer an offender (remove it):", f); bad = True
for n in sorted(reading):
    kinds = by_kind.get(n, [])
    if len(kinds) != 1:
        print("script reads orig/ and needs exactly one of gen:/resource:/oracle: in the allowlist:", n, kinds); bad = True
for n in sorted(by_kind):
    if n not in reading:
        print("allowlist entry is stale, the script no longer reads orig/ or is no longer run by CMake:", n); bad = True
if bad:
    sys.exit(1)
print("raw dumps: %d allowed dump files, %d scripts reading orig/ (%s), none new" % (len(found),
      len(reading), ", ".join("%d %s" % (sum(k[0] == x for k in by_kind.values()), x) for x in KINDS)))
