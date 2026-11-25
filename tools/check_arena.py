#!/usr/bin/env python3
"""Story 1.8's lint (AD-15): src/ outside src/platform/arena/ may not hold a `(uint32_t)(uintptr_t)` cast or a
#define of the names include/fs98/arena.h owns, beyond the per-file counts of tools/arena_lint_allowed.txt,
which may only shrink.

    python3 -B tools/check_arena.py              the build's check
    python3 -B tools/check_arena.py --inventory  rows in the allowlist's format, plus the reverse-cast count
    python3 -B tools/check_arena.py --self-test  every row of the spec's matrix, through the same path
    options: --root DIR (default: the source root), --prior FILE (compare with an earlier allowlist, not HEAD's)

Limit: swapping one banned site for another in the same file keeps the count, so only review catches that."""
import bisect
import os
import posixpath
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ALLOW = "tools/arena_lint_allowed.txt"
EXT = (".c", ".h", ".inc", ".S")
EXEMPT = "src/platform/arena/"
NAMES = "M8 M16 M32 MF MD R8 R16 R32 RF RDB DA D RDATA fs98_ptr fs98_handle fs98_call".split()
# \x01 stands for a newline inside a block comment (see clean): whitespace, as for cpp, but still a line
W = r"[\s\x01]*"
CAST = re.compile(r"\(%suint32_t%s\)%s\(%suintptr_t%s\)" % ((W,) * 5))
REVERSE = re.compile(r"\(%suintptr_t%s\)%s\(%suint32_t%s\)" % ((W,) * 5))
B = r"[ \t\f\v\x01]"            # blank within a directive
DEFINE = re.compile(r"^%s*(?P<hash>#|%%:)%s*define%s+(?P<name>%s)(?![A-Za-z0-9_$])" % (B, B, B, "|".join(NAMES)), re.M)


def clean(text):
    """Splices joined; comments, strings and character literals blanked (newlines kept).
    Returns the cleaned text and a function from an offset in it to a physical line."""
    parts, splices, pos, total = [], [], 0, 0
    for m in re.finditer(r"\\[ \t]*\r?\n", text):          # join line splices, remembering where
        parts.append(text[pos:m.start()])
        total += m.start() - pos
        splices.append(total)
        pos = m.end()
    parts.append(text[pos:])
    t = "".join(parts)
    res, i, n = [], 0, len(t)
    while i < n:
        c = t[i]
        if t.startswith("/*", i):
            e = t.find("*/", i + 2)
            e = n if e < 0 else e + 2
            res.append(re.sub(r"[^\n]", " ", t[i:e]).replace("\n", "\x01"))
            i = e
        elif t.startswith("//", i):
            e = t.find("\n", i)
            e = n if e < 0 else e
            res.append(" " * (e - i))
            i = e
        elif c in "\"'":
            j = i + 1
            while j < n and t[j] != c and t[j] != "\n":
                j += 2 if t[j] == "\\" else 1
            if j < n and t[j] == c:               # an unterminated quote (an apostrophe in .S text) is plain text
                res.append(c + " " * (j - i - 1) + c)
                i = j + 1
            else:
                res.append(c)
                i += 1
        else:
            res.append(c)
            i += 1
    cl = "".join(res)
    nls = [m.start() for m in re.finditer("[\n\x01]", cl)]

    def line(off):
        return 1 + bisect.bisect_left(nls, off) + bisect.bisect_right(splices, off)  # a splice before it adds a line
    return cl, line


def scan_file(path):
    text = open(path, encoding="latin-1", newline="").read()
    cl, line = clean(text)
    return ([line(m.start()) for m in CAST.finditer(cl)],
            [(line(m.start("hash")), m.group("name")) for m in DEFINE.finditer(cl)],
            len(REVERSE.findall(cl)))


def scan(root):
    """{relative path: (cast lines, define sites, reverse count)} for the files with any hit, and the reverse total."""
    found, rev = {}, 0
    top = os.path.join(root, "src")
    for d, ds, fs in os.walk(top):
        ds.sort()
        for f in sorted(fs):
            if not f.endswith(EXT):
                continue
            p = os.path.join(d, f)
            rel = os.path.relpath(p, root).replace(os.sep, "/")
            if rel.startswith(EXEMPT):
                continue
            casts, defs, r = scan_file(p)
            rev += r
            if casts or defs:
                found[rel] = (casts, defs, r)
    return found, rev


def parse(text, name, errs):
    rows = {}
    for n, raw in enumerate(text.splitlines(), 1):
        s = raw.split("#", 1)[0].strip()
        if not s:
            continue
        w = s.split()
        if len(w) != 3 or not re.fullmatch(r"\d+", w[1]) or not re.fullmatch(r"\d+", w[2]):
            errs.append("%s:%d: bad row (path casts defines, counts non-negative integers): %s" % (name, n, raw.strip()))
            continue
        p, c, d = w[0], int(w[1]), int(w[2])
        if posixpath.normpath(p) != p or p.startswith("/") or "\\" in p:
            errs.append("%s:%d: path not normalised: %s" % (name, n, p))
        if p in rows:
            errs.append("%s:%d: duplicate row: %s" % (name, n, p))
        if c == 0 and d == 0:
            errs.append("%s:%d: (0, 0) row, delete it: %s" % (name, n, p))
        rows[p] = (c, d)
    return rows


def head_allowlist(root):
    try:
        top = subprocess.run(["git", "-C", root, "rev-parse", "--show-prefix"], capture_output=True, text=True)
        if top.returncode or top.stdout.strip():
            return None                           # no git, or root is only a subdirectory of some repository
        r = subprocess.run(["git", "-C", root, "show", "HEAD:" + ALLOW], capture_output=True, text=True)
        return r.stdout if r.returncode == 0 else None
    except OSError:
        return None


def rows_of(found):
    return {p: (len(c), len(d)) for p, (c, d, _) in found.items()}


def run(root, prior=None, inventory=False, out=print):
    found, rev = scan(root)
    now = rows_of(found)
    if inventory:
        for p in sorted(now):
            out("%s %d %d" % (p, *now[p]))
        out("# reverse casts: %d" % rev)
        return 0
    errs = []
    path = os.path.join(root, ALLOW)
    if os.path.exists(path):
        rows = parse(open(path).read(), ALLOW, errs)
    else:
        rows = {}
        errs.append("%s is missing" % ALLOW)
    for p in sorted(set(now) | set(rows)):
        c, d = now.get(p, (0, 0))
        rc, rd = rows.get(p, (None, None))
        if rc is None:
            for ln in found[p][0]:
                errs.append("%s:%d: banned (uint32_t)(uintptr_t) cast, file not allowlisted" % (p, ln))
            for ln, nm in found[p][1]:
                errs.append("%s:%d: banned #define of %s, file not allowlisted" % (p, ln, nm))
            continue
        if p not in now and not os.path.exists(os.path.join(root, p)):
            errs.append("%s: stale row, the file is gone" % p)
            continue
        for what, have, row, sites in (("cast", c, rc, [ln for ln in found.get(p, ([], [], 0))[0]]),
                                       ("define", d, rd, [ln for ln, _ in found.get(p, ([], [], 0))[1]])):
            if have > row:
                errs.append("%s: %d banned %ss, the row allows %d (sites at lines %s)" %
                            (p, have, what, row, ", ".join(map(str, sites))))
            elif have < row:
                errs.append("%s: only %d banned %ss left, lower the row from %d" % (p, have, what, row))
    if prior is not None:
        before, src = parse(prior, "prior", errs), "the prior allowlist"
    else:
        h = head_allowlist(root)
        before, src = (parse(h, "HEAD:" + ALLOW, errs), "HEAD's allowlist") if h is not None else (None, None)
    if before is not None:
        for p, (c, d) in sorted(rows.items()):
            if p not in before:
                errs.append("%s: row not in %s; a row may never be added" % (p, src))
            elif c > before[p][0] or d > before[p][1]:
                errs.append("%s: row (%d, %d) is above %s (%d, %d); a row may only shrink" % (p, c, d, src, *before[p]))
    else:
        out("arena lint: no prior allowlist (no git, or none in HEAD): checked only the working tree")
    if errs:
        for e in errs:
            out("arena lint: " + e)
        out("arena lint: FAILED, %d problems" % len(errs))
        return 1
    out("arena lint: %d files allowlisted, %d casts, %d defines left (reverse casts: %d)" %
        (len(rows), sum(c for c, _ in now.values()), sum(d for _, d in now.values()), rev))
    return 0


def main(argv):
    a = list(argv)
    root, prior = ROOT, None
    if "--root" in a:
        i = a.index("--root")
        root = os.path.abspath(a[i + 1])
        del a[i:i + 2]
    if "--prior" in a:
        i = a.index("--prior")
        prior = open(a[i + 1]).read()
        del a[i:i + 2]
    if a == ["--self-test"]:
        return self_test()
    if a not in ([], ["--inventory"]):
        sys.stderr.write(__doc__ + "\n")
        return 2
    return run(root, prior, inventory=bool(a))


def git_case(fails):
    """HEAD's allowlist, with no --prior, in a temporary git repository: a row raised since HEAD fails."""
    import io
    import contextlib
    d = tempfile.mkdtemp(prefix="arena_lint_git_")
    c1 = "int a = (uint32_t)(uintptr_t)p;\n"
    os.makedirs(os.path.join(d, "src/a"))
    os.makedirs(os.path.join(d, "tools"))
    open(os.path.join(d, "src/a/a.c"), "w").write(c1)
    open(os.path.join(d, ALLOW), "w").write("src/a/a.c 1 0\n")
    env = dict(os.environ, GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@t", GIT_COMMITTER_NAME="t", GIT_COMMITTER_EMAIL="t@t")
    for cmd in (["init", "-q"], ["add", "."], ["commit", "-q", "-m", "x"]):
        r = subprocess.run(["git", "-C", d] + cmd, capture_output=True, env=env)
        if r.returncode:
            fails.append("git case: git %s failed" % cmd[0])
            return
    open(os.path.join(d, "src/a/a.c"), "w").write(c1 + c1)
    open(os.path.join(d, ALLOW), "w").write("src/a/a.c 2 0\n")
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        got = main(["--root", d])
    if got != 1 or "is above HEAD's allowlist" not in buf.getvalue():
        fails.append("git HEAD lookup: exit %s\n%s" % (got, buf.getvalue()))


def self_test():
    cases = 0
    fails = []

    def tree(files, allow):
        d = tempfile.mkdtemp(prefix="arena_lint_")
        for p, t in dict(files, **({ALLOW: allow} if allow is not None else {})).items():
            os.makedirs(os.path.dirname(os.path.join(d, p)), exist_ok=True)
            open(os.path.join(d, p), "w").write(t)
        return d

    def case(name, files, allow, code, want, prior=None, no=()):
        nonlocal cases
        cases += 1
        d = tree(files, allow)
        # the CLI's own path: argv in, exit code and text out (stdout captured)
        import io
        import contextlib
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            got = main(["--root", d] + (["--prior", prior_file(prior)] if prior is not None else []))
        txt = buf.getvalue()
        ok = got == code and all(txt.count(w[0]) == w[1] if isinstance(w, tuple) else w in txt for w in want) and not any(w in txt for w in no)
        if not ok:
            fails.append("%s: exit %s (want %s)\n%s" % (name, got, code, txt))

    def prior_file(text):
        f = tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False)
        f.write(text)
        f.close()
        return f.name

    c1 = "int a = (uint32_t)(uintptr_t)p;\n"
    case("clean", {"src/a/a.c": c1}, "src/a/a.c 1 0\n", 0, ["1 files allowlisted, 1 casts, 0 defines left (reverse casts: 0)"])
    case("new cast, unlisted", {"src/a/a.c": c1}, "", 1, ["src/a/a.c:1: banned (uint32_t)(uintptr_t) cast"])
    case("new cast, listed file", {"src/a/a.c": c1 + "\n" + c1}, "src/a/a.c 1 0\n", 1, ["src/a/a.c: 2 banned casts"])
    case("several to a line", {"src/a/a.c": "x = (uint32_t)(uintptr_t)a + (uint32_t) ( uintptr_t ) b;\n"}, "", 1,
         [("a.c:1: banned", 2)])
    case("cast in .S", {"src/a/a.S": "/* x */\n  .long (uint32_t)(uintptr_t) 0\n"}, "", 1, ["src/a/a.S:2:"])
    case("cast across a newline", {"src/a/a.c": "(uint32_t)\n(uintptr_t) p\n"}, "", 1, ["a.c:1:"])
    for nm, src in (("define", "#define M32(a) x\n"), ("spaced", "# define D x\n"), ("comment form", "#define/**/D x\n"),
                    ("split", "#define \\\n  RDATA x\n"), ("indented", "  #  define fs98_ptr(a) a\n")):
        case("new definition: " + nm, {"src/a/a.h": src}, "", 1, ["banned #define of"])
    case("directive spanning a comment", {"src/a/a.h": "#define /* x\n y */ M32 1\n#define M8 2\n"}, "", 1,
         [("a.h:1: banned #define of M32", 1), ("a.h:3: banned #define of M8", 1)])
    case("form feed and vertical tab", {"src/a/a.h": "#\fdefine\vM32 1\n"}, "", 1, ["a.h:1: banned #define of M32"])
    case("digraph", {"src/a/a.h": "%:define M32 1\n  %: define RF 1\n"}, "", 1, ["a.h:1:", "a.h:2:"])
    case("dollar identifier", {"src/a/a.h": "#define M32$local 1\n#define D$ 1\n"}, "", 0, ["0 defines"])
    case("splice with trailing blanks", {"src/a/a.h": "#define A \\ \t\n  1\n#define \\  \n  M16 x\n"}, "", 1, ["a.h:3: banned #define of M16"])
    case("line of the hash", {"src/a/a.h": "\n\n   \t  #define D 1\n"}, "", 1, ["a.h:3: banned #define of D"])
    case("split line number", {"src/a/a.c": "#define A \\\n  1\n#define M8(a) x\n"}, "", 1, ["a.c:3:"])
    case("not a site: comment", {"src/a/a.c": "/* (uint32_t)(uintptr_t) #define M32 */\n// (uint32_t)(uintptr_t)\n"}, "", 0, ["0 casts"])
    case("not a site: string", {"src/a/a.c": 'char *s = "(uint32_t)(uintptr_t)"; char c = \'"\';\n'
                                                '#define MX 1\n#define MD2 2\n'}, "", 0, ["0 casts, 0 defines"])
    case("not a site: other name", {"src/a/a.h": "#define DB 1\n#define DAX 1\n#define M3 1\n"}, "", 0, ["0 defines"])
    case("shrunk, not lowered", {"src/a/a.c": c1}, "src/a/a.c 2 0\n", 1, ["lower the row"])
    case("zero-site file, row left", {"src/a/a.c": "int x;\n"}, "src/a/a.c 1 0\n", 1, ["lower the row"])
    case("raised vs prior", {"src/a/a.c": c1 + c1}, "src/a/a.c 2 0\n", 1, ["is above the prior allowlist"], prior="src/a/a.c 1 0\n")
    case("new row vs prior", {"src/a/a.c": c1}, "src/a/a.c 1 0\n", 1, ["row not in the prior allowlist"], prior="")
    case("lowered vs prior", {"src/a/a.c": c1}, "src/a/a.c 1 0\n", 0, ["1 casts"], prior="src/a/a.c 2 0\n")
    case("bad row: (0, 0)", {"src/a/a.c": "int x;\n"}, "src/a/a.c 0 0\n", 1, ["(0, 0) row"])
    case("bad row: stale", {}, "src/a/gone.c 1 0\n", 1, ["stale row"])
    case("bad row: duplicate", {"src/a/a.c": c1}, "src/a/a.c 1 0\nsrc/a/a.c 1 0\n", 1, ["duplicate row"])
    case("bad row: negative", {"src/a/a.c": c1}, "src/a/a.c -1 0\n", 1, ["bad row"])
    case("bad row: not normalised", {"src/a/a.c": c1}, "src/b/../a/a.c 1 0\n", 1, ["not normalised"])
    case("one-zero rows pass", {"src/a/a.c": c1, "src/a/b.h": "#define MF 1\n"}, "src/a/a.c 1 0\nsrc/a/b.h 0 1\n", 0,
         ["2 files allowlisted"])
    case("exempt", {"src/platform/arena/arena.c": c1 + "#define M32 1\n"}, "", 0, ["0 files allowlisted"])
    case("reverse cast", {"src/a/a.c": "x = (uintptr_t)(uint32_t)a;\n"}, "", 0, ["(reverse casts: 1)"])
    case("no git", {"src/a/a.c": c1}, "src/a/a.c 1 0\n", 0, ["checked only the working tree"])
    case("missing allowlist", {"src/a/a.c": c1}, None, 1, ["is missing"])
    git_case(fails)
    cases += 1
    if fails:
        sys.stdout.write("\n".join(fails) + "\narena lint self-test: FAILED %d of %d\n" % (len(fails), cases))
        return 1
    print("arena lint self-test: %d cases passed" % cases)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
