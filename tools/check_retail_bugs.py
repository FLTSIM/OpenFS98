#!/usr/bin/env python3
"""Check docs/retail-bugs.md against the RETAIL BUG markers in src/ in both directions (Story 1.5).

A marker is `RETAIL BUG (<kind>): <ID>` inside a comment of a file under src/ that git tracks or
would track.  Each register entry's Id-cell annotation sets the marker it requires:
  ID -> (kept); ID (suspected[, not used]) -> (kept, suspected); ID (fixed, not kept) -> (fixed, not kept);
  ID (no longer applies) -> no marker, not allowlisted.
Every entry that still applies must be marked.  The allowlist tools/retail_bugs_unmarked.txt is gone (end of
Story 1.5) and its return is an error.
Usage: tools/check_retail_bugs.py [--self-test]
"""
import os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ID = r"[A-Z][A-Z0-9]*(?:-[A-Z0-9]+)+"
KINDS = {"kept": "kept", "kept, suspected": "suspected", "fixed, not kept": "fixed"}
STATUS = {"": "kept", "suspected": "suspected", "suspected, not used": "suspected",
          "fixed, not kept": "fixed", "no longer applies": "obsolete"}
HEAD = re.compile(r"RETAIL BUG\s*\(")
MARK = re.compile(r"RETAIL BUG \(([a-z, ]+)\):\s*(%s)(?!\w|-)(?!\s*,\s*%s)" % (ID, ID))


def register(text):
    """-> ({id: status}, errors)"""
    ents, errs, table = {}, [], False
    for line in text.splitlines():
        cells = line.split("|")
        if not line.startswith("|") or len(cells) < 3:
            table = False
            continue
        first = cells[1].strip()
        if first == "Id":
            table = True
            continue
        if not table or re.fullmatch(r"-+|:?-+:?", first):
            continue
        m = re.fullmatch(r"(%s)(?: \(([^)]*)\))?" % ID, first)
        if not m:
            errs.append("register: unparseable Id cell: %s" % line[:60])
            continue
        i, ann = m.group(1), m.group(2) or ""
        if ann not in STATUS:
            errs.append("register: %s has unknown annotation (%s)" % (i, ann))
        elif i in ents:
            errs.append("register: duplicate id %s" % i)
        else:
            ents[i] = STATUS[ann]
    return ents, errs


def comments(path, text):
    """-> ([(line, comment text)], [line of marker text outside a comment])"""
    asm = path.endswith((".S", ".def"))
    out, bad, i, n, line = [], [], 0, len(text), 1
    last = None   # consecutive line comments are one comment
    while i < n:
        c, two = text[i], text[i:i + 2]
        if two == "/*":
            j = text.find("*/", i + 2); j = n if j < 0 else j
            out.append((line, text[i + 2:j])); last = None
            line += text.count("\n", i, j); i = j + 2
        elif asm and c == "#" and re.match(r"[ \t]*$", text[text.rfind("\n", 0, i) + 1:i]) and text[i + 1:i + 2].isalpha():
            i += 1; last = None   # a preprocessor directive is code
        elif two == "//" or (asm and c in "#;"):
            j = text.find("\n", i); j = n if j < 0 else j
            if last is not None and last[0] == line - 1:
                out[-1] = (out[-1][0], out[-1][1] + "\n" + text[i + (2 if two == "//" else 1):j])
            else:
                out.append((line, text[i + (2 if two == "//" else 1):j]))
            last = (line,); i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            if HEAD.search(text[i:j]):
                bad.append(line)
            i = j + 1; last = None
        elif c == "R" and HEAD.match(text, i):
            bad.append(line); i += 10; last = None   # bare marker text in code
        else:
            if c == "\n":
                line += 1
            i += 1
            if not c.isspace():
                last = None
    return out, bad


def check(reg_text, files):
    """files {path: text} -> (errors, warnings, summary)"""
    ents, errs = register(reg_text)
    warns, marked = [], {}
    for path, text in sorted(files.items()):
        cs, bad = comments(path, text)
        for ln in bad:
            warns.append("%s:%d: marker text outside a comment" % (path, ln))
        for ln, c in cs:
            c = re.sub(r"\s*\n\s*(?:\*+|//+|[#;]+)?\s*", " ", c)
            heads = [m.start() for m in HEAD.finditer(c)]
            ok = {m.start(): m for m in MARK.finditer(c)}
            for h in heads:
                m = ok.get(h)
                if not m or m.group(1) not in KINDS:
                    errs.append("%s:%d: malformed marker: %s" % (path, ln, c[h:h + 50].strip()))
                    continue
                marked.setdefault(m.group(2), []).append((path, ln, KINDS[m.group(1)]))
    for i, sites in sorted(marked.items()):
        for path, ln, kind in sites:
            if i not in ents:
                errs.append("%s:%d: marker names %s, not in the register" % (path, ln, i))
            elif ents[i] == "obsolete":
                errs.append("%s:%d: %s no longer applies but still has a marker" % (path, ln, i))
            elif ents[i] != kind:
                errs.append("%s:%d: %s is a %s marker, the entry is %s" % (path, ln, i, kind, ents[i]))
    for i, s_ in sorted(ents.items()):
        if s_ != "obsolete" and i not in marked:
            errs.append("unmarked: %s" % i)
    nobs = sum(s == "obsolete" for s in ents.values())
    return errs, warns, "retail bugs: %d entries, %d marked, %d no longer apply" % (len(ents), len(marked), nobs)


def run_root(root):
    """The CLI's work on a source root -> (errors, warnings, summary)"""
    errs = []
    if os.path.lexists(os.path.join(root, "tools/retail_bugs_unmarked.txt")):
        errs.append("tools/retail_bugs_unmarked.txt exists: the allowlist is gone, mark the entry instead")
    reg_text = open(os.path.join(root, "docs/retail-bugs.md"), encoding="utf-8").read()
    try:
        names = subprocess.check_output(["git", "ls-files", "-z", "-co", "--exclude-standard", "src"], text=True,
                                        cwd=root, stderr=subprocess.DEVNULL).split("\0")
    except (subprocess.CalledProcessError, OSError):   # not a git tree (the self-test's temporary root)
        names = [os.path.join(d, f) for d, _, fs in os.walk(os.path.join(root, "src")) for f in fs]
        names = [os.path.relpath(n, root) for n in names]
    files = {f: open(os.path.join(root, f), encoding="latin-1").read() for f in names
             if f and os.path.isfile(os.path.join(root, f))}
    e, warns, summary = check(reg_text, files)
    return errs + e, warns, summary


def self_test():
    def reg(*rows):
        return "| Id | Where |\n| --- | --- |\n" + "".join("| %s | x |\n" % r for r in rows)
    def run(rows, files):
        return check(reg(*rows), {"a.c": files} if isinstance(files, str) else files)
    def fails(res, *words):
        assert res[0] and all(any(w in e for e in res[0]) for w in words), (res, words)
    def clean(res):
        assert not res[0], res
    clean(run(["SS-1"], "/* RETAIL BUG (kept): SS-1 */"))                                   # consistent
    fails(run(["OPT-1"], "/* RETAIL BUG (kept): OPT-99 */"), "a.c:1", "OPT-99")             # unknown
    fails(run(["SS-1"], ""), "unmarked: SS-1")                                              # unmarked
    fails(run(["SS-1 (suspected)"], "/* RETAIL BUG (kept): SS-1 */"), "a.c:1", "kept", "suspected")
    fails(run(["SS-1 (fixed, not kept)"], "/* RETAIL BUG (kept): SS-1 */"), "fixed")
    clean(run(["SS-1 (suspected, not used)"], "// RETAIL BUG (kept, suspected): SS-1"))
    fails(run(["SS-1 (no longer applies)"], "/* RETAIL BUG (kept): SS-1 */"), "no longer applies")
    clean(run(["SS-1 (no longer applies)"], ""))
    clean(run(["FLT-3"], "/* RETAIL BUG (kept):\n * FLT-3. */"))                            # wrapped, trailing dot
    clean(run(["FLT-3"], "// RETAIL BUG (kept):\n// FLT-3,"))
    fails(run(["MAIN-1", "MAIN-10"], "/* RETAIL BUG (kept): MAIN-10 */"), "unmarked: MAIN-1")
    r = run(["MAIN-6", "MAIN-18"], "/* see MAIN-6, MAIN-18 */\n/* RETAIL BUG (kept): MAIN-6 */")
    fails(r, "unmarked: MAIN-18"); assert len(r[0]) == 1, r                                # prose is not a marker
    r = run(["SS-1", "SS-2"], 'char *s = "RETAIL BUG (kept): SS-1";\n/* RETAIL BUG (kept): SS-2 */')   # string
    fails(r, "unmarked: SS-1"); assert len(r[0]) == 1, r
    assert r[1] and "a.c:1" in r[1][0], r
    fails(run(["SS-1"], "/* RETAIL BUG (kept): */"), "a.c:1", "malformed")
    fails(run(["MAIN-6", "MAIN-18"], "/* RETAIL BUG (kept): MAIN-6, MAIN-18 */"), "a.c:1", "malformed")
    clean(run(["SS-1", "SS-2"], "/* RETAIL BUG (kept): SS-1, and RETAIL BUG (kept): SS-2 */"))   # two in one
    clean(run(["SS-1", "SS-2"], {"b.S": "# RETAIL BUG (kept): SS-1\n  nop ; RETAIL BUG (kept): SS-2\n"}))
    fails(run(["SS-1", "SS-1"], "/* RETAIL BUG (kept): SS-1 */"), "duplicate id SS-1")
    fails(run(["SS-1 (odd)"], "/* RETAIL BUG (kept): SS-1 */"), "annotation")
    clean(run(["SS-1"], "/* RETAIL BUG (kept): SS-1 */\n/* RETAIL BUG (kept): SS-1 */"))   # one id, two sites
    res = run(["SS-1", "SS-2 (no longer applies)"], "/* RETAIL BUG (kept): SS-1 */")
    assert res[2] == "retail bugs: 2 entries, 1 marked, 1 no longer apply", res
    fails(run(["SS-1 (suspected)"], "", ["SS-1"]) if False else
          check(reg("SS-1  (suspected)"), {"a.c": ""}), "unparseable")             # bad Id cell
    fails(check(reg("SS-1 (suspected"), {"a.c": ""}), "unparseable")
    r = run(["SS-1", "SS-2"], {"b.S": '#define TEXT "RETAIL BUG (kept): SS-1"\n/* RETAIL BUG (kept): SS-2 */\n'})
    fails(r, "unmarked: SS-1"); assert len(r[0]) == 1 and r[1], r                           # directive is code
    r = run(["SS-1", "SS-2"], "int RETAIL BUG (kept): SS-1;\n/* RETAIL BUG (kept): SS-2 */")  # bare code
    fails(r, "unmarked: SS-1"); assert len(r[0]) == 1 and r[1] and "a.c:1" in r[1][0], r
    import tempfile                                    # the allowlist file is refused, through the CLI's own path
    with tempfile.TemporaryDirectory() as root:
        for path, text in (("docs/retail-bugs.md", reg("SS-1")), ("src/a.c", "/* RETAIL BUG (kept): SS-1 */"),
                           ("tools/retail_bugs_unmarked.txt", "")):
            os.makedirs(os.path.dirname(os.path.join(root, path)), exist_ok=True)
            open(os.path.join(root, path), "w").write(text)
        fails(run_root(root), "retail_bugs_unmarked.txt")
        os.remove(os.path.join(root, "tools/retail_bugs_unmarked.txt"))
        clean(run_root(root))
        link = os.path.join(root, "tools/retail_bugs_unmarked.txt")
        try:
            os.symlink(os.path.join(root, "missing-target"), link)   # a dangling link is refused too
        except OSError:
            link = None
        if link:
            fails(run_root(root), "retail_bugs_unmarked.txt")
    print("check_retail_bugs: self-test passed")


def main():
    if "--self-test" in sys.argv:
        return self_test()
    errs, warns, summary = run_root(ROOT)
    for w in warns:
        print("warning:", w)
    for e in errs:
        print("error:", e)
    print(summary)
    sys.exit(1 if errs else 0)


main()
