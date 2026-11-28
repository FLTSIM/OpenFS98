#!/usr/bin/env python3
"""Story 1.10's lint: src/ may hold no hand-written integer divide (div/idiv, any suffix, AT&T or Intel) except the
per-site records of tools/div_asm_allowed.txt, which may only shrink. Divides go through include/fs98/divide.h.

    python3 -B tools/check_div_asm.py              the build's check
    python3 -B tools/check_div_asm.py --self-test  the scanner's cases
    options: --root DIR (default: the source root), --prior FILE (compare with an earlier allowlist, not HEAD's)

It runs no compiler (generated headers do not exist on a clean build) but expands the macros itself: the #define lines of every
file are collected, each asm/__asm/__asm__ statement is expanded (object and function-like macros, `#` stringification, `##`
pasting) and its template joined from its literals, dialect alternatives `{a|b}` are expanded before the template is split into
statements, and any number of leading labels (`1:`) are stripped. A statement whose mnemonic is div/idiv with an optional
b/w/l/q suffix is a site. A template that does not resolve to literals alone is a site with mnemonic '?'. Every other string
literal is scanned too, and every .S file is parsed as assembly.
x87 fdiv/fidiv are out of scope. A site is `file|function|mnemonic|normalised statement`; the function is the
nearest enclosing C function (heuristic) or '-'."""
import os
import re
import subprocess
import sys
import tempfile
from collections import Counter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ALLOW = "tools/div_asm_allowed.txt"
EXT = (".c", ".h", ".inc", ".S", ".s")
DIV = re.compile(r"^i?div[bwlq]?$")
ASM = {"asm", "__asm", "__asm__"}
QUAL = {"volatile", "__volatile__", "__volatile", "inline", "__inline__", "goto"}
ESC = {"n": "\n", "t": "\t", "r": "\r", "\\": "\\", '"': '"', "'": "'", "a": "\a", "b": "\b", "f": "\f", "v": "\v", "?": "?"}


def decode(body):
    out, i = [], 0
    while i < len(body):
        c = body[i]
        if c != "\\" or i + 1 >= len(body):
            out.append(c)
            i += 1
            continue
        n = body[i + 1]
        if n == "x":
            m = re.match(r"[0-9a-fA-F]+", body[i + 2:])
            out.append(chr(int(m.group(), 16) & 255) if m else "x")
            i += 2 + (len(m.group()) if m else 0)
        elif n in "01234567":
            m = re.match(r"[0-7]{1,3}", body[i + 1:])
            out.append(chr(int(m.group(), 8) & 255))
            i += 1 + len(m.group())
        else:
            out.append(ESC.get(n, n))
            i += 2
    return "".join(out)


def tokens(text):
    """(kind, value, offset): 'str' (decoded), 'id', 'p' (punctuation); comments and character literals dropped."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
            if out and out[-1][0] == "str":        # adjacent literals are one
                out[-1] = ("str", out[-1][1] + decode(text[i + 1:j]), out[-1][2])
            else:
                out.append(("str", decode(text[i + 1:j]), i))
            i = j + 1
        elif c == "'":
            j = i + 1
            while j < n and text[j] != "'":
                j += 2 if text[j] == "\\" else 1
            i = j + 1
        elif c.isalpha() or c in "_$":
            m = re.match(r"[A-Za-z0-9_$]+", text[i:])
            out.append(("id", m.group(), i))
            i += len(m.group())
        elif c.isspace():
            i += 1
        else:
            out.append(("p", c, i))
            i += 1
    return out


def alternatives(st):
    """Every dialect alternative of a template or statement: 'idivl {%ecx|ecx}' is two (GCC picks one by -masm).
    Done before the template is split into statements, so an alternative may hold a newline."""
    m = re.search(r"\{([^{}]*)\}", st)
    if not m or "|" not in m.group(1):
        return [st]
    out = []
    for alt in m.group(1).split("|"):
        out += alternatives(st[:m.start()] + alt + st[m.end():])
        if len(out) > 256:
            break
    return out[:256]


def strip_labels(st):
    """Leading labels, named or numeric ('1:'), any number of them."""
    while True:
        n = re.sub(r"^\s*(?:[A-Za-z_.$][\w.$]*|\d+):(?!:)\s*", "", st)
        if n == st:
            return st
        st = n


def mnemonic(st):
    return st.split(" ", 1)[0]


def div_sites(template):
    """(mnemonic, statement) of each statement with a divide, over every dialect alternative of the whole template: a statement
    repeated in one variant counts each time, one in several variants counts once (the most any variant has)."""
    best = Counter()
    for variant in alternatives(template):
        here = Counter()
        for st in re.split(r"[\n;]", variant):
            st = " ".join(strip_labels(st.replace("\t", " ")).lower().split())
            mn = mnemonic(st)
            if st and DIV.match(mn):
                here[(mn, st)] += 1
        for key, n in here.items():
            best[key] = max(best[key], n)
    return list(best.elements())


def asm_file(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*|#[^\n]*", " ", text)
    return div_sites(text)


# ---- a small preprocessor: macros are expanded where they reach an asm template ----

def spell(toks):
    out = ""
    for t in toks:
        v = t[1]
        if out and (out[-1].isalnum() or out[-1] == "_") and (v[:1].isalpha() or v[:1] == "_"):
            out += " "
        out += v if t[0] != "str" else '"%s"' % v
    return out


def parse_defines(text):
    """{name: (params, body tokens, function-like, variadic)} of the #define lines of one file (splices already joined)."""
    macros = {}
    for m in re.finditer(r"^[ \t]*#[ \t]*define[ \t]+(\w+)(\(([^)]*)\))?([^\n]*)", text, re.M):
        params = [x.strip() for x in m.group(3).split(",")] if m.group(2) and m.group(3).strip() else []
        variadic = bool(params) and params[-1] == "..."
        if variadic:
            params[-1] = "__VA_ARGS__"
        macros[m.group(1)] = (params, tokens(m.group(4)), bool(m.group(2)), variadic)
    return macros


def parse_args(toks, i):
    """toks[i] is '(': the argument token lists and the index after the closing ')'."""
    args, cur, level, j = [], [], 0, i
    while j < len(toks):
        t = toks[j]
        if t[0] == "p" and t[1] == "(":
            level += 1
            if level > 1:
                cur.append(t)
        elif t[0] == "p" and t[1] == ")":
            level -= 1
            if level == 0:
                args.append(cur)
                return args, j + 1
            cur.append(t)
        elif t[0] == "p" and t[1] == "," and level == 1:
            args.append(cur)
            cur = []
        else:
            cur.append(t)
        j += 1
    return args, j


def expand(toks, macros, hide=(), only=None):
    """Macro-expand a token list ('only': just those macro names, else all). A macro is not re-expanded inside itself."""
    out, i = [], 0
    while i < len(toks):
        t = toks[i]
        if t[0] == "id" and t[1] in macros and t[1] not in hide and (only is None or t[1] in only):
            params, body, func, variadic = macros[t[1]]
            if not func:
                out += expand(body, macros, hide + (t[1],), only)
                i += 1
                continue
            if i + 1 < len(toks) and toks[i + 1][0] == "p" and toks[i + 1][1] == "(":
                args, j = parse_args(toks, i + 1)
                if variadic and len(args) > len(params):
                    args = args[:len(params) - 1] + [sum((a + [("p", ",", 0)] for a in args[len(params) - 1:]), [])[:-1]]
                bound = dict(zip(params, args))
                sub, k = [], 0
                while k < len(body):
                    b = body[k]
                    if b[0] == "p" and b[1] == "#" and k + 2 < len(body) + 1 and k + 1 < len(body) and body[k + 1][0] == "p" and body[k + 1][1] == "#":
                        # token pasting: the previous and next tokens, spelled together
                        nxt = body[k + 2] if k + 2 < len(body) else None
                        right = bound.get(nxt[1], [nxt]) if nxt and nxt[0] == "id" else ([nxt] if nxt else [])
                        left = sub.pop() if sub else None
                        sub.append(("id", (left[1] if left else "") + spell(right).replace(" ", ""), 0))
                        k += 3
                        continue
                    if b[0] == "p" and b[1] == "#" and k + 1 < len(body) and body[k + 1][0] == "id" and body[k + 1][1] in bound:
                        sub.append(("str", spell(bound[body[k + 1][1]]), 0))      # stringification
                        k += 2
                        continue
                    if b[0] == "id" and b[1] in bound:
                        sub += expand(bound[b[1]], macros, hide, only)
                    else:
                        sub.append(b)
                    k += 1
                out += expand(sub, macros, hide + (t[1],), only)
                i = j
                continue
        out.append(t)
        i += 1
    return out


def asmish(macros):
    """The macros whose expansion can hold an asm statement (directly or through another macro)."""
    names = {n for n, (ps, body, f, v) in macros.items() if any(t[0] == "id" and t[1] in ASM for t in body)}
    while True:
        more = {n for n, (ps, body, f, v) in macros.items() if n not in names and any(t[0] == "id" and t[1] in names for t in body)}
        if not more:
            return names
        names |= more


def scan_text(text, rel, macros=None):
    """Sites as (function, mnemonic, statement), sorted. `macros` are those of every file (default: this file's own)."""
    if rel.endswith((".S", ".s")):
        return sorted(("-", mn, st) for mn, st in asm_file(text))
    text = re.sub(r"\\[ \t]*\r?\n", "", text)             # splices
    own = parse_defines(text)
    macros = dict(macros if macros is not None else {}, **own)
    body = re.sub(r"^[ \t]*#[^\n]*", lambda m: " " * len(m.group()), text, flags=re.M)      # no preprocessor line stays in the stream
    toks = expand(tokens(body), macros, only=asmish(macros))
    found, depth, func, last_call, k = [], 0, "-", "-", 0
    while k < len(toks):
        kind, v, _ = toks[k]
        if kind == "p":
            if v == "{":
                if depth == 0:
                    func = last_call
                depth += 1
            elif v == "}":
                depth = max(0, depth - 1)
                if depth == 0:
                    func = "-"
            elif v == "(" and k and toks[k - 1][0] == "id" and depth == 0:
                last_call = toks[k - 1][1]
        elif kind == "id" and v in ASM:
            j = k + 1
            while j < len(toks) and toks[j][0] == "id" and toks[j][1] in QUAL:
                j += 1
            if j < len(toks) and toks[j][1] == "(":
                j += 1
                first, level = j, 0
                while j < len(toks) and not (level == 0 and toks[j][0] == "p" and toks[j][1] in ":)"):
                    level += (toks[j][0] == "p" and toks[j][1] == "(") - (toks[j][0] == "p" and toks[j][1] == ")")
                    j += 1
                tmpl = expand(toks[first:j], macros)          # the template, every macro in it expanded
                bad = [t for t in tmpl if not (t[0] == "str" or (t[0] == "p" and t[1] in "()"))]
                if bad:
                    found.append((func, "?", "unresolved template: " + " ".join(str(t[1]) for t in tmpl)))
                else:
                    for mn, st in div_sites("".join(t[1] for t in tmpl if t[0] == "str")):
                        found.append((func, mn, st))
                k = j
                continue
        elif kind == "str":
            for mn, st in div_sites(v):
                found.append((func, mn, st))
        k += 1
    return sorted(found)


def scan(root):
    files = []
    for d, ds, fs in os.walk(os.path.join(root, "src")):
        ds.sort()
        for f in sorted(fs):
            if f.endswith(EXT):
                path = os.path.join(d, f)
                files.append((os.path.relpath(path, root).replace(os.sep, "/"), open(path, errors="replace").read()))
    macros = {}
    for rel, text in files:
        if not rel.endswith((".S", ".s")):
            macros.update(parse_defines(re.sub(r"\\[ \t]*\r?\n", "", text)))
    sites = Counter()
    for rel, text in files:
        for fn, mn, st in scan_text(text, rel, macros):
            sites["%s|%s|%s|%s" % (rel, fn, mn, st)] += 1
    return sites


def parse(text):
    """The allowlist rows, with multiplicity: a row repeated n times allows n sites."""
    return Counter(ln.split("#", 1)[0].strip() for ln in text.splitlines() if ln.split("#", 1)[0].strip())


def head_allowlist(root):
    try:
        top = subprocess.run(["git", "-C", root, "rev-parse", "--show-prefix"], capture_output=True, text=True)
        if top.returncode or top.stdout.strip():
            return None
        r = subprocess.run(["git", "-C", root, "show", "HEAD:" + ALLOW], capture_output=True, text=True)
        return r.stdout if r.returncode == 0 else None
    except OSError:
        return None


def run(root, prior=None, out=print):
    now = scan(root)
    path = os.path.join(root, ALLOW)
    if not os.path.exists(path):
        out("div lint: %s is missing" % ALLOW)
        return 1
    rows = parse(open(path).read())
    errs = ["divide not allowlisted (use fs98/divide.h): %s" % s for s in sorted((now - rows).elements())]
    errs += ["%s lists a site no longer in src/ (delete the row): %s" % (ALLOW, s) for s in sorted((rows - now).elements())]
    if prior is None:
        prior = head_allowlist(root)
    if prior is not None:
        errs += ["%s row not in the prior allowlist (it may only shrink): %s" % (ALLOW, s) for s in sorted((rows - parse(prior)).elements())]
    else:
        out("div lint: no prior allowlist (no git, or none in HEAD): checked only the working tree")
    for e in errs:
        out("div lint: " + e)
    if not errs:
        out("div lint: %d allowlisted sites, no other divide" % sum(now.values()))
    return 1 if errs else 0


def self_test():
    cases = [
        ('void f(void){ __asm__ volatile ("idivl %2" : "=a"(q) : "0"(a), "r"(b)); }', "x.c", [("f", "idivl", "idivl %2")]),
        ('void f(void){ __asm__("cltd\\n\\tidivl %3"); }', "x.c", [("f", "idivl", "idivl %3")]),
        ('void f(void){ __asm__("cltd; idivl %3"); }', "x.c", [("f", "idivl", "idivl %3")]),
        ('void f(void){ __asm__("xorl %%edx,%%edx\\n\\t" "divl %2"); }', "x.c", [("f", "divl", "divl %2")]),
        ('void f(void){ __asm__(".intel_syntax noprefix\\n idiv ecx"); }', "x.c", [("f", "idiv", "idiv ecx")]),
        ('void f(void){ __asm__("divb %0"); }', "x.c", [("f", "divb", "divb %0")]),
        ('/* idivl %1 */ // divl\nvoid f(void){ __asm__("fdivp\\n\\tfidivl %1\\n\\tfidiv %0"); }', "x.c", []),
        ('void f(void){ __asm__("l1: idivl %1"); }', "x.c", [("f", "idivl", "idivl %1")]),
        ('void f(void){ __asm__("mov $1, %0 # idiv\\n"); }', "x.c", []),
        ('#define D(t) __asm__ volatile (t : "=a"(q))\nvoid f(void){ D("idivl %2"); }', "x.c", [("f", "idivl", "idivl %2")]),
        ('#define D(t) \\\n  __asm__ volatile (t \\\n : "=a"(q))\n', "x.c", []),
        ('#define D(name) __asm__(".globl _" #name "\\n_" #name ":\\n\\tret")\nD(foo)\n', "x.c", []),
        ('__asm__(".text\\n\\tdivl %ecx");', "x.c", [("-", "divl", "divl %ecx")]),
        ('int g(int a, int b){ return a / b; }', "x.c", []),
        ('  idivl %ecx # idiv\n  /* div */ fdiv %st\nlbl: divl (%esp)\n  cltd; idivl 4(%esp)\n', "x.S",
         [("-", "divl", "divl (%esp)"), ("-", "idivl", "idivl 4(%esp)"), ("-", "idivl", "idivl %ecx")]),
        ('void f(void){ __asm__("\\x69\\x64iv"); }', "x.c", [("f", "idiv", "idiv")]),
        # joined literals, macro halves, stringification, dialect alternatives (with a newline), many labels
        ('void f(void){ __asm__("i" "di" "vl %ecx"); }', "x.c", [("f", "idivl", "idivl %ecx")]),
        ('#define OP "id"\nvoid f(void){ __asm__(OP "ivl %%ecx"); }', "x.c", [("f", "idivl", "idivl %%ecx")]),
        ('#define M(op) __asm__("nop\\n\\t" " " #op " %%ecx")\nvoid f(void){ M(idivl); }', "x.c", [("f", "idivl", "idivl %%ecx")]),
        ('#define S(x) S_(x)\n#define S_(x) #x\nvoid f(void){ __asm__(S(id) "ivl %ecx"); }', "x.c", [("f", "idivl", "idivl %ecx")]),
        ('void f(void){ __asm__("{idivl %%ecx\\n|nop}"); }', "x.c", [("f", "idivl", "idivl %%ecx")]),
        ('void f(void){ __asm__("{idivl %%ecx|idiv %%ecx}"); }', "x.c", [("f", "idiv", "idiv %%ecx"), ("f", "idivl", "idivl %%ecx")]),
        ('void f(void){ __asm__("1: 2: 3: 4: 5: idivl %ecx"); }', "x.c", [("f", "idivl", "idivl %ecx")]),
        ('1: 2: 3: 4: 5: idivl %ecx\n', "x.S", [("-", "idivl", "idivl %ecx")]),
        ('1: idivl %ecx\n  2: divl %ebx\n', "x.S", [("-", "divl", "divl %ebx"), ("-", "idivl", "idivl %ecx")]),
        ('void f(void){ __asm__("a: idivl %ecx\\n\\tidivl %ecx"); }', "x.c", [("f", "idivl", "idivl %ecx"), ("f", "idivl", "idivl %ecx")]),
        # unresolved templates are sites
        ('void f(void){ __asm__(WHAT " ret"); }', "x.c", [("f", "?", "unresolved template: WHAT  ret")]),
        ('#define OP(x) #x\nvoid f(void){ __asm__(OP(idivl) " %ecx"); }', "x.c", [("f", "idivl", "idivl %ecx")]),
        ('#define D(t) __asm__ volatile (t)\nvoid f(void){ D(OP); }', "x.c", [("f", "?", "unresolved template: OP")]),
        ('#define NOP ".nop"\nvoid f(void){ __asm__(NOP); }', "x.c", []),
        ('#define E(n) ".globl _" #n\n#define D(n) __asm__ (E(n))\nvoid f(void){ D(x); }', "x.c", []),
        ('#define S(x) S_(x)\n#define S_(x) #x\nvoid f(void){ __asm__(".set a, " S(4)); }', "x.c", []),
    ]
    bad = 0
    for text, rel, want in cases:
        got = scan_text(text, rel)
        if got != sorted(want):
            bad += 1
            print("self-test FAIL: %r\n  want %r\n  got  %r" % (text, sorted(want), got))
    with tempfile.TemporaryDirectory() as d:
        os.makedirs(d + "/src/a")
        os.makedirs(d + "/tools", exist_ok=True)
        open(d + "/src/a/a.c", "w").write('void f(void){ __asm__("divl %1\\n\\tdivl %1"); }')
        twice = "src/a/a.c|f|divl|divl %1"
        for allow, prior, code, why in ((twice, twice, 1, "a repeated statement needs its own row"), (twice + "\n" + twice, twice + "\n" + twice, 0, "two rows allow two sites"),
                                         (twice + "\n" + twice, twice, 1, "a repeated row is a raise against --prior")):
            open(d + "/" + ALLOW, "w").write(allow + "\n")
            if run(d, prior, out=lambda s: None) != code:
                bad += 1
                print("self-test FAIL: multiplicity: %s" % why)
        open(d + "/src/a/a.c", "w").write('void f(void){ __asm__("divl %1"); }')
        site = "src/a/a.c|f|divl|divl %1"
        for allow, prior, code in ((site, site, 0), ("", "", 1), (site, "", 1), (site + "\nsrc/a/b.c|g|divl|divl %1", site, 1)):
            open(d + "/" + ALLOW, "w").write(allow + "\n")
            if run(d, prior, out=lambda s: None) != code:
                bad += 1
                print("self-test FAIL: allowlist %r prior %r want exit %d" % (allow, prior, code))
        open(d + "/src/a/a.c", "w").write("")
        open(d + "/" + ALLOW, "w").write(site + "\n")
        if run(d, site, out=lambda s: None) != 1:
            bad += 1
            print("self-test FAIL: a stale row must fail")
        # a macro defined in one file, used in another
        open(d + "/src/a/a.c", "w").write('#define OP "id"\n')
        open(d + "/src/a/b.c", "w").write('void g(void){ __asm__(OP "ivl %ecx"); }')
        open(d + "/" + ALLOW, "w").write("\n")
        if run(d, "", out=lambda s: None) != 1:
            bad += 1
            print("self-test FAIL: a macro half from another file")
    print("div lint self-test: %s" % ("FAIL" if bad else "pass"))
    return 1 if bad else 0


def main(argv):
    if "--self-test" in argv:
        return self_test()
    root, prior = ROOT, None
    if "--root" in argv:
        root = argv[argv.index("--root") + 1]
    if "--prior" in argv:
        prior = open(argv[argv.index("--prior") + 1]).read()
    return run(root, prior)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
