#!/usr/bin/env python3
"""Story 1.9b's lint: every FS98_KEEP_* / FS98_ENTRY invocation and FS98_KEPT binding in src/ (src/util/ excluded until 1.9c)
is a row of tools/keep_entries.txt, and each row's classification still holds in the win32-retail expansion.

    python3 -B tools/check_keep_entries.py              the build's check
    python3 -B tools/check_keep_entries.py --self-test  the failing and passing fixtures
    options: --root DIR (default: the source root), --prior FILE (an earlier table, not HEAD's)

Each owning translation unit is preprocessed with i686-w64-mingw32-gcc -E and the flags of its recipe in cmake/steps. Every
class-c target and every thunk body must be defined exactly once there, with no asm/__asm/__asm__ in it (a macro hiding one is
expanded); every thunk label must be defined by exactly one top-level asm block, whose instructions must equal the row. The
thunk-in rows (a thunk that reads a register: 1.9e converts them) may only shrink, by full row, against the prior table.
The expansion is also reconciled with the table: every keep wrapper the expanded units emit (found by its `.globl _x` + `.rept N/4` +
`call` + `ret $` signature, so a macro hiding an invocation shows) is a row, and every row's entry is used in an active initialiser."""
import os
import re
import subprocess
import sys
import tempfile
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_div_asm import ASM, QUAL, decode  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TABLE = "tools/keep_entries.txt"
CC = "i686-w64-mingw32-gcc"
COLS = "file macro entry symbol args kept target class body".split()
CLASS = re.compile(r"^(c|thunk-out|thunk-in:(eax|ecx|edx|ebx|esi|edi|ebp))$")
STRIP = re.compile(r'"(?:[^"\\\n]|\\.)*"|\'(?:[^\'\\\n]|\\.)*\'|/\*.*?\*/|//[^\n]*', re.S)
TOK = re.compile(r'"(?:[^"\\\n]|\\.)*"|\'(?:[^\'\\\n]|\\.)*\'|[A-Za-z_$][\w$]*|\d[\w.]*|\S')
INVOKE = re.compile(r"\bFS98_(?:KEEP_\w+|ENTRY|KEPT)\s*\(")


def norm(s):
    return " ".join(s.split())


def canon_asm(text):
    """Statements split on newline and ';', whitespace-normalised, joined by '; '."""
    return "; ".join(s for s in (norm(x) for x in re.split(r"[\n;]", text)) if s)


# ---- the table ----

def parse_table(text):
    """(entry rows as dicts, thunk rows as (label, asm)); ValueError on a malformed line."""
    entries, thunks = [], []
    for n, ln in enumerate(text.splitlines(), 1):
        if not ln.strip() or ln.lstrip().startswith("#"):
            continue
        f = ln.split("|", 8)
        if f[0] == "thunk":
            f = ln.split("|", 2)
            if len(f) != 3 or not f[1].strip() or not f[2].strip():
                raise ValueError("line %d: a thunk row is thunk|label|asm" % n)
            thunks.append((f[1].strip(), canon_asm(f[2])))
        else:
            if len(f) != 9 or any(not x.strip() for x in f):
                raise ValueError("line %d: an entry row has 9 fields (%s)" % (n, ", ".join(COLS)))
            row = dict(zip(COLS, (x.strip() for x in f)))
            if not CLASS.match(row["class"]):
                raise ValueError("line %d: unknown class %r" % (n, row["class"]))
            row["line"] = "|".join(row[c] for c in COLS)
            entries.append(row)
    return entries, thunks


def row_errors(entries):
    errs = []
    for r in entries:
        if r["kept"] != "%s->%s" % (r["entry"], r["target"]):
            errs.append("row %s: kept %r is not entry->target" % (r["entry"], r["kept"]))
        if r["class"] == "c" and r["body"] != r["target"]:
            errs.append("row %s: a class-c row's body is its target" % r["entry"])
    return errs


def validate(entries, thunks):
    errs = row_errors(entries)
    labels = Counter(l for l, _ in thunks)
    want = Counter("_" + r["target"] for r in entries if r["class"] != "c")
    for l in sorted((labels - want).elements()):
        errs.append("thunk row %s belongs to no thunk-out/thunk-in entry" % l)
    for l in sorted((want - labels).elements()):
        errs.append("entry target %s has no thunk row" % l)
    return errs


# ---- the source ----

def blank_defines(text):
    return re.sub(r"^[ \t]*#[ \t]*define\b(?:[^\n]*\\\n)*[^\n]*", lambda m: re.sub(r"[^\n]", " ", m.group()), text, flags=re.M)


def blank(text):
    """(comments blanked, comments and string literals blanked): same length as `text`, so offsets agree."""
    sp = lambda m: re.sub(r"[^\n]", " ", m.group())
    return (STRIP.sub(lambda m: m.group() if m.group()[0] in "\"'" else sp(m), text),
            STRIP.sub(sp, text))


def split_args(text, i):
    """text[i] is '(': (the top-level argument strings, the index after ')')."""
    args, cur, level, j = [], [], 0, i
    for m in TOK.finditer(text, i):
        v = m.group()
        j = m.end()
        if v == "(":
            level += 1
            if level == 1:
                continue
        elif v == ")":
            level -= 1
            if level == 0:
                args.append(" ".join(cur))
                return [norm(a) for a in args], j
        elif v == "," and level == 1:
            args.append(" ".join(cur))
            cur = []
            continue
        cur.append(v)
    return [norm(a) for a in args + [" ".join(cur)]], j


def scan_source(root):
    """(invocations, bindings, mentions) of src/ (not src/util/): the first two tagged with their file; `mentions` are the files
    that name a keep macro or keep.h anywhere (a macro definition included), whose owning units are expanded too."""
    inv, kept, mention = Counter(), Counter(), set()
    for d, ds, fs in os.walk(os.path.join(root, "src")):
        ds[:] = sorted(x for x in ds if os.path.join(d, x) != os.path.join(root, "src", "util"))
        for f in sorted(fs):
            if not f.endswith((".c", ".inc", ".h")):
                continue
            rel = os.path.relpath(os.path.join(d, f), root).replace(os.sep, "/")
            raw = open(os.path.join(d, f), errors="replace").read()
            if re.search(r"FS98_(?:KEEP|ENTRY|KEPT)|keep\.h", raw):
                mention.add(rel)
            a, b = blank(raw)
            a, b = blank_defines(a), blank_defines(b)
            for m in INVOKE.finditer(b):     # found outside strings, comments and #define lines; read with their string arguments
                args, _ = split_args(a, m.end() - 1)
                name = m.group()[:-1].strip()
                if name == "FS98_KEPT":
                    kept[(rel, "%s->%s" % tuple(args) if len(args) == 2 else "?(" + ", ".join(args) + ")")] += 1
                elif len(args) >= 2:
                    inv[(rel, name, args[0], args[1], ", ".join(args[2:]))] += 1
                else:
                    inv[(rel, name, "?", "?", ", ".join(args))] += 1
    return inv, kept, mention


# ---- the expansion ----

def recipe_flags(root, tu):
    """The flags the build compiles `tu` with: its recipe line's every flag (@MODULE_FLAGS@ expanded), less -c, -o and its
    output, the link-only options, and the files."""
    import module_flags
    d = os.path.join(root, "cmake", "steps")
    files = sorted(os.listdir(d), key=lambda x: (x != "all.cmake", x)) if os.path.isdir(d) else []
    pat = re.compile(r"(?<![\w/.])" + re.escape(tu) + r"(?![\w/.])")
    for f in files:
        for ln in open(os.path.join(d, f), errors="replace"):
            if CC in ln and pat.search(ln):
                flags, skip = [], False
                for t in ln.replace("[==[", " ").replace("]==]", " ").split():
                    if skip:
                        skip = False
                    elif t == "@MODULE_FLAGS@":
                        flags += module_flags.FLAGS
                    elif t == "-o":
                        skip = True
                    elif t.startswith("-") and not (t == "-c" or t in ("-shared", "-nostdlib") or t.startswith(("-l", "-Wl,"))):
                        flags.append(t)
                return flags
    return None


def preprocess(root, tu):
    """({'toks', 'included', 'labels', 'aliased', 'wrappers', 'used'}, None) or (None, error)."""
    flags = recipe_flags(root, tu)
    if flags is None:
        return None, "no recipe in cmake/steps compiles %s" % tu
    env = {k: v for k, v in os.environ.items() if k != "GCCLOG"}   # the gate's gcc wrapper logs every compile as a build unit; a preprocess-only run is not one
    try:
        r = subprocess.run([CC, "-E"] + flags + [tu], cwd=root, capture_output=True, text=True, errors="replace", env=env)
    except OSError as e:
        return None, "cannot run %s (not installed or not on PATH?): %s" % (CC, e)
    if r.returncode:
        return None, "%s -E %s failed: %s" % (CC, tu, r.stderr.strip()[-300:])
    included = set()

    def mark(m):
        included.add(os.path.normpath(m.group(1)))
        return ""
    toks = tokenize(re.sub(r'^# \d+ "([^"\n]*)"[^\n]*$', mark, r.stdout, flags=re.M))
    u = {"toks": toks, "included": included}
    u["labels"], u["aliased"], u["wrappers"] = scan_asm(toks)
    u["used"] = {v for i, (k, v) in enumerate(toks) if k == "id" and not (i + 1 < len(toks) and toks[i + 1] == ("p", "("))}
    return u, None


def tokenize(text):
    """(kind, value): 'str' (adjacent literals joined, decoded), 'id', 'p'; numbers and character literals are 'p'."""
    out = []
    for m in TOK.finditer(text):
        v = m.group()
        if v[0] == '"':
            s = decode(v[1:-1])
            if out and out[-1][0] == "str":
                out[-1] = ("str", out[-1][1] + s)
            else:
                out.append(("str", s))
        elif v[0].isalpha() or v[0] in "_$":
            out.append(("id", v))
        else:
            out.append(("p", v))
    return out


def match_close(toks, i, open_, close):
    """Index of the token closing the one at i, or None if it never closes."""
    level = 0
    for j in range(i, len(toks)):
        if toks[j][0] == "p":
            level += toks[j][1] == open_
            level -= toks[j][1] == close
            if level == 0:
                return j
    return None


def definitions(toks, name):
    """The token slices of the bodies of every file-scope definition of function `name`; None for one that never closes."""
    out, depth = [], 0
    for i, (k, v) in enumerate(toks):
        if k == "p":
            depth += v == "{"
            depth -= v == "}"
        elif v == name and k == "id" and depth == 0 and i + 1 < len(toks) and toks[i + 1][1] == "(" and (i == 0 or toks[i - 1][1] not in (".", ">")):
            j = match_close(toks, i + 1, "(", ")")
            j = len(toks) if j is None else j + 1
            while j + 1 < len(toks) and toks[j][1] == "__attribute__" and toks[j + 1][1] == "(":
                j = match_close(toks, j + 1, "(", ")")
                j = len(toks) if j is None else j + 1
            if j < len(toks) and toks[j][1] == "{":
                e = match_close(toks, j, "{", "}")
                out.append(None if e is None else toks[j:e + 1])
    return out


DIRECTIVE = re.compile(r"\.(if\w*|else\w*|elseif|endif|set|equ\w*|eqv|macro|endm|rept|irp\w*|altmacro|purgem)\b")
ALIAS = re.compile(r"(?:\.(?:set|equ|equiv|eqv)\s+([\w.$@]+)\s*,|([\w.$@]+)\s*=)")


def scan_asm(toks):
    """Over every file-scope asm block: ({label: [(instructions, tainted)]}, aliased names, Counter of keep wrappers).
    A label's instructions run to the next label or the block's end; a block is tainted by an assembler conditional, macro,
    repeat or alias directive. A wrapper is a block with `.globl _x`, `.rept N/4` of `pushl 4*C+N(%esp)`, `call` and `ret $` (keep.h's signature;
    HG2D's own DEVICE_*_ENTRY and DRAW_ENTRY shims are a different shape and 1.9e's)."""
    labels, aliased, wrappers, depth, i = {}, set(), Counter(), 0, 0
    while i < len(toks):
        k, v = toks[i]
        if k == "p":
            depth += v == "{"
            depth -= v == "}"
        elif k == "id" and v in ASM and depth == 0:
            j = i + 1
            while j < len(toks) and toks[j][0] == "id" and toks[j][1] in QUAL:
                j += 1
            end = match_close(toks, j, "(", ")") if j < len(toks) and toks[j][1] == "(" else None
            if end is not None:
                inner = toks[j + 1:end]
                if inner and all(t[0] == "str" for t in inner):
                    text = "".join(t[1] for t in inner)
                    sts = [norm(x) for x in re.split(r"[\n;]", text)]
                    tainted = any(DIRECTIVE.match(st) or re.match(r"[\w.$@]+\s*=", st) for st in sts)
                    for st in sts:
                        for m in ALIAS.finditer(st):
                            aliased.add(m.group(1) or m.group(2))
                    g = re.search(r"\.globl\s+(_\w+)", text)
                    if g and re.search(r"\.rept\s+\d+/4\s+pushl\s+4\*\d+\+\d+\(%esp\)", text) and re.search(r"\bcall\b", text) and re.search(r"\bret\s+\$", text):
                        wrappers[g.group(1)[1:]] += 1
                    cur = None
                    for st in sts:
                        while True:
                            m = re.match(r"([A-Za-z_.$][\w.$@]*):\s*(.*)", st)
                            if not m:
                                break
                            cur = []
                            labels.setdefault(m.group(1), []).append((cur, tainted))
                            st = m.group(2)
                        if st and cur is not None:
                            cur.append(st)
                i = end
        i += 1
    return labels, aliased, wrappers


# ---- the checks ----

def head_table(root):
    try:
        top = subprocess.run(["git", "-C", root, "rev-parse", "--show-prefix"], capture_output=True, text=True)
        if top.returncode or top.stdout.strip():
            return None
        r = subprocess.run(["git", "-C", root, "show", "HEAD:" + TABLE], capture_output=True, text=True)
        return r.stdout if r.returncode == 0 else None
    except OSError:
        return None


def check(root, prior=None, out=print):
    errs = []
    path = os.path.join(root, TABLE)
    if not os.path.exists(path):
        out("keep lint: %s is missing" % TABLE)
        return 1
    try:
        entries, thunks = parse_table(open(path).read())
    except ValueError as e:
        out("keep lint: %s: %s" % (TABLE, e))
        return 1
    errs += validate(entries, thunks)
    # the source against the rows, as multisets
    inv, kept, mention = scan_source(root)
    rows_inv = Counter((r["file"], r["macro"], r["entry"], r["symbol"], r["args"]) for r in entries)
    rows_kept = Counter((r["file"], r["kept"]) for r in entries)
    for t in sorted((inv - rows_inv).elements()):
        errs.append("untabled invocation: %s: %s(%s, %s%s)" % (t[0], t[1], t[2], t[3], ", " + t[4] if t[4] else ""))
    for t in sorted((rows_inv - inv).elements()):
        errs.append("row has no matching invocation (changed variant, symbol or arguments?): %s: %s(%s, %s, %s)" % t)
    for t in sorted((kept - rows_kept).elements()):
        errs.append("untabled FS98_KEPT binding: %s: %s" % t)
    for t in sorted((rows_kept - kept).elements()):
        errs.append("row binding not in the source (FS98_KEPT miswired?): %s: %s" % t)
    # the expansion: the built units that include a row's file (honouring only active includes) or name a keep macro
    cache = {}

    def unit(tu):
        if tu not in cache:
            cache[tu] = preprocess(root, tu)
        return cache[tu]

    def including(rel):
        if rel.endswith(".c"):
            cand = [rel]
        else:
            base, here = os.path.basename(rel), os.path.dirname(rel)
            cand = []
            for d, ds, fs in os.walk(os.path.join(root, "src")):
                for f in sorted(fs):
                    c = os.path.relpath(os.path.join(d, f), root).replace(os.sep, "/")
                    if f.endswith(".c") and (os.path.dirname(c) == here or base in open(os.path.join(d, f), errors="replace").read()):
                        cand.append(c)
        got = []
        for tu in cand:
            u, err = unit(tu)
            if u is not None and os.path.normpath(rel) in u["included"]:
                got.append(tu)
            elif u is None and not err.startswith("no recipe"):
                errs.append(err)
                return got
        return got

    row_files = {r["file"] for r in entries}
    tus = {}
    for f in sorted(row_files | mention):
        got = including(f)
        if not got and f in row_files:
            errs.append("%s: no built unit includes it (a commented-out or inactive include?)" % f)
        for tu in got:
            tus[tu] = None
    thunk_rows = {}
    for l, a in thunks:
        thunk_rows.setdefault(l, []).append(a)
    for tu in sorted(tus):
        u, err = unit(tu)
        if err:
            errs.append(err)
            continue
        rows = [r for r in entries if os.path.normpath(r["file"]) in u["included"]]
        want, have = Counter(r["entry"] for r in rows), u["wrappers"]
        for e in sorted((have - want).elements()):
            errs.append("%s: untabled keep wrapper emitted for %s (a macro hiding an FS98_ENTRY?)" % (tu, e))
        for e in sorted((want - have).elements()):
            errs.append("%s: tabled entry %s emits no wrapper" % (tu, e))
        for r in rows:
            if r["entry"] not in u["used"]:
                errs.append("%s: %s has no active FS98_KEPT use in the expansion (moved into #if 0?)" % (tu, r["entry"]))
            defs = definitions(u["toks"], r["body"])
            if len(defs) != 1:
                errs.append("%s: %s: %d definitions of %s, expected exactly one" % (tu, r["entry"], len(defs), r["body"]))
            elif defs[0] is None:
                errs.append("%s: %s: unclosed body of %s" % (tu, r["entry"], r["body"]))
            elif any(k == "id" and v in ASM for k, v in defs[0]):
                errs.append("%s: %s: asm in the body of %s" % (tu, r["entry"], r["body"]))
            if r["class"] != "c":
                label = "_" + r["target"]
                got = u["labels"].get(label, [])
                if len(got) != 1:
                    errs.append("%s: %s: %d asm definitions of %s, expected exactly one" % (tu, r["entry"], len(got), label))
                elif got[0][1] or label in u["aliased"]:
                    errs.append("%s: thunk %s sits in asm with assembler conditionals, macros, repeats or aliases (cannot be read as written)" % (tu, label))
                elif len(thunk_rows.get(label, [])) == 1 and "; ".join(got[0][0]) != thunk_rows[label][0]:
                    errs.append("%s: thunk %s changed:\n    row    %s\n    source %s" % (tu, label, thunk_rows[label][0], "; ".join(got[0][0])))
    # thunk-in rows may only shrink
    if prior is None:
        prior = head_table(root)
    if prior is None:
        out("keep lint: no prior table (no git, or none in HEAD): the thunk-in shrink rule compared the table with itself")
        prior = open(path).read()
    try:
        prior_rows = parse_table(prior)[0]
        bad = row_errors(prior_rows)
        if bad:
            raise ValueError("; ".join(bad))
    except ValueError as e:
        out("keep lint: invalid prior table: %s" % e)
        return 1
    old = Counter(r["line"] for r in prior_rows if r["class"].startswith("thunk-in"))
    now = Counter(r["line"] for r in entries if r["class"].startswith("thunk-in"))
    for s in sorted((now - old).elements()):
        errs.append("thunk-in row not in the prior table (they may only shrink): %s" % s)
    for e in errs:
        out("keep lint: " + e)
    if not errs:
        c = Counter(r["class"].split(":")[0] for r in entries)
        out("keep lint: %d entries (%d c, %d thunk-out, %d thunk-in), %d thunks, all as tabled" % (len(entries), c["c"], c["thunk-out"], c["thunk-in"], len(thunks)))
    return 1 if errs else 0


# ---- self-test ----

FIX_SRC = r'''#include "fs98/keep.h"
#if __STDC_HOSTED__
#error the recipe's -ffreestanding was dropped
#endif
#define NOPE __asm__("nop")
int body_c(void) { return 1; }
int body_e(void) { return 2; }
int body_t(void) { return 3; }
void thunk_o(void);
void thunk_i(void);
__asm__(".text\n_thunk_o:\n\tcall _body_t\n\tbtl $0,_flag\n\tret\n");
#define IN(n) __asm__(".text\n_" #n ":\n\tpushl %eax\n\tcall _body_t\n\tret\n")
IN(thunk_i);
FS98_KEEP_ECX(c_entry, "_body_c@4", 4)
FS98_ENTRY(e_entry, "_body_e@4", 4, 0, "", "movl 12(%esp), %ecx\n\t")
FS98_KEEP_EDX(o_entry, "_thunk_o", 0)
FS98_KEEP_EDX(i_entry, "_thunk_i", 0)
void *slots[] = { FS98_KEPT(c_entry, body_c), FS98_KEPT(e_entry, body_e), FS98_KEPT(o_entry, thunk_o), FS98_KEPT(i_entry, thunk_i) };
'''
FIX_TABLE = r'''src/m/m.c|FS98_KEEP_ECX|c_entry|"_body_c@4"|4|c_entry->body_c|body_c|c|body_c
src/m/m.c|FS98_ENTRY|e_entry|"_body_e@4"|4, 0, "", "movl 12(%esp), %ecx\n\t"|e_entry->body_e|body_e|c|body_e
src/m/m.c|FS98_KEEP_EDX|o_entry|"_thunk_o"|0|o_entry->thunk_o|thunk_o|thunk-out|body_t
src/m/m.c|FS98_KEEP_EDX|i_entry|"_thunk_i"|0|i_entry->thunk_i|thunk_i|thunk-in:eax|body_t
thunk|_thunk_o|call _body_t; btl $0,_flag; ret
thunk|_thunk_i|pushl %eax; call _body_t; ret
'''
FIX_CMAKE = '    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -ffreestanding -Iinclude -shared -o bin/M.DLL src/m/m.c -lgcc]==]\n'


def self_test():
    bad = 0
    with tempfile.TemporaryDirectory() as d:
        for sub in ("src/m", "src/n", "tools", "cmake/steps"):
            os.makedirs(os.path.join(d, sub))
        os.symlink(os.path.join(ROOT, "include"), os.path.join(d, "include"))
        open(os.path.join(d, "cmake/steps/all.cmake"), "w").write(FIX_CMAKE)

        def case(name, want, src=lambda s: s, table=lambda t: t, prior=None, files=(), path=None):
            nonlocal bad
            open(os.path.join(d, "src/m/m.c"), "w").write(src(FIX_SRC))
            open(os.path.join(d, TABLE), "w").write(table(FIX_TABLE))
            for f, text in files:
                open(os.path.join(d, f), "w").write(text)
            lines = []
            old = os.environ["PATH"]
            if path is not None:
                os.environ["PATH"] = path
            try:
                rc = check(d, FIX_TABLE if prior is None else prior, out=lines.append)
            except Exception as e:     # a traceback is a failed fixture, and so is any exception
                rc, lines = -1, ["exception %r" % e]
            finally:
                os.environ["PATH"] = old
                for f, _ in files:
                    os.remove(os.path.join(d, f))
            text = "\n".join(lines)
            if (want is None and rc != 0) or (want is not None and (rc != 1 or want not in text)):
                bad += 1
                print("self-test FAIL: %s (want %r, rc %d)\n  %s" % (name, want, rc, text.replace("\n", "\n  ")))

        def sub(a, b):
            def f(s):
                assert a in s, a
                return s.replace(a, b, 1)
            return f
        case("base fixture passes", None)
        open(os.path.join(d, "src/n/n.c"), "w").write('int body_c(void) { __asm__("nop"); return 0; }\n')
        case("a same-named definition in another module passes", None)
        os.remove(os.path.join(d, "src/n/n.c"))
        real = check(ROOT, out=print)
        if real:
            bad += 1
            print("self-test FAIL: the real table")
        case("untabled invocation", "untabled invocation", src=lambda s: s + 'FS98_KEEP_ECX(x_entry, "_x@4", 4)\n')
        case("missing row", "untabled invocation", table=lambda t: t.replace(t.splitlines()[0] + "\n", ""))
        case("row without invocation", "no matching invocation", table=lambda t: t + 'src/m/m.c|FS98_KEEP_ECX|z_entry|"_z"|0|z_entry->body_c|body_c|c|body_c\n')
        case("swapped KEEP variant", "no matching invocation", src=sub("FS98_KEEP_ECX(c_entry", "FS98_KEEP_EDX(c_entry"))
        case("changed byte count", "no matching invocation", src=sub('"_body_c@4", 4)', '"_body_c@4", 8)'))
        case("changed raw restore", "no matching invocation", src=sub('"movl 12(%esp), %ecx', '"movl 8(%esp), %ecx'))
        case("miswired FS98_KEPT", "FS98_KEPT miswired?", src=sub("FS98_KEPT(c_entry, body_c)", "FS98_KEPT(c_entry, body_t)"))
        case("asm in a c body", "asm in the body of body_c", src=sub("int body_c(void) { return 1; }", 'int body_c(void) { __asm__("nop"); return 1; }'))
        case("asm behind a macro", "asm in the body of body_c", src=sub("int body_c(void) { return 1; }", "int body_c(void) { NOPE; return 1; }"))
        case("asm in a thunk body", "asm in the body of body_t", src=sub("int body_t(void) { return 3; }", 'int body_t(void) { __asm__("nop"); return 3; }'))
        case("%eax became %ecx", "thunk _thunk_i changed", src=sub("pushl %eax", "pushl %ecx"))
        case("register input added to a thunk-out", "thunk _thunk_o changed", src=sub("call _body_t\\n\\tbtl", "movl %eax,%ecx\\n\\tcall _body_t\\n\\tbtl"))
        case("body call redirected", "thunk _thunk_o changed", src=sub("call _body_t\\n\\tbtl", "call _body_c\\n\\tbtl"))
        case("two definitions in one unit", "2 definitions of body_c", src=lambda s: s + "int body_c(void) { return 4; }\n")
        case("no definition", "0 definitions of body_t", src=sub("int body_t(void) { return 3; }", "int body_t(void);"))
        case("thunk label missing", "0 asm definitions", src=sub("_thunk_o:", "_thunk_x:"))
        case("invalid prior", "invalid prior table", prior="garbage\n")
        newrow = 'src/m/m.c|FS98_KEEP_EDX|j_entry|"_thunk_j"|0|j_entry->thunk_j|thunk_j|thunk-in:ecx|body_t\nthunk|_thunk_j|pushl %ecx; call _body_t; ret\n'
        newsrc = 'void thunk_j(void);\n__asm__(".text\\n_thunk_j:\\n\\tpushl %ecx\\n\\tcall _body_t\\n\\tret\\n");\nFS98_KEEP_EDX(j_entry, "_thunk_j", 0)\nvoid *more[] = { FS98_KEPT(j_entry, thunk_j) };\n'
        case("a thunk-in row added with its source", "thunk-in row not in the prior", src=lambda s: s + newsrc, table=lambda t: t + newrow)
        case("a same-count thunk-in replacement", "thunk-in row not in the prior",
             src=sub("pushl %eax\\n\\tcall _body_t\\n\\tret\\n\")", "pushl %ecx\\n\\tcall _body_t\\n\\tret\\n\")"),
             table=lambda t: t.replace("thunk-in:eax", "thunk-in:ecx").replace("pushl %eax; call", "pushl %ecx; call"))

        # reconciliation with the expansion, owning units, unclosed bodies, thunk decoding, tables, environment
        case("a macro hiding a wrapper", "untabled keep wrapper emitted for z_entry",
             src=lambda s: '#define NEW_KEEP(e) FS98_KEEP_EDX(e, "_z", 0)\n' + s + "NEW_KEEP(z_entry)\n")
        case("a binding moved into #if 0", "i_entry has no active FS98_KEPT use",
             src=sub("FS98_KEPT(i_entry, thunk_i) }", "\n#if 0\nFS98_KEPT(i_entry, thunk_i)\n#endif\n}"))
        inc_move = lambda s: s.replace('FS98_KEEP_ECX(c_entry, "_body_c@4", 4)\n', '#include "m.inc"\n').replace("FS98_KEPT(c_entry, body_c), ", "")
        inc_table = lambda t: t.replace("src/m/m.c|FS98_KEEP_ECX|c_entry", "src/m/m.inc|FS98_KEEP_ECX|c_entry", 1)
        minc = ('src/m/m.inc', 'FS98_KEEP_ECX(c_entry, "_body_c@4", 4)\nvoid *c_slot = (void *)FS98_KEPT(c_entry, body_c);\n')
        case("an active include of a .inc row file passes", None, src=inc_move, table=inc_table, files=[minc])
        case("a commented-out include", "no built unit includes it",
             src=lambda s: inc_move(s).replace('#include "m.inc"', '/* #include "m.inc" */'), table=inc_table, files=[minc])
        case("an unclosed body", "unclosed body of body_c", src=sub("int body_c(void) { return 1; }", "int body_c(void) { return 1;"))
        case("an .if 0 in a thunk", "assembler conditionals",
             src=sub('":\\n\\tpushl %eax\\n\\tcall', '":\\n.if 0\\n\\tpushl %ecx\\n.endif\\n\\tpushl %eax\\n\\tcall'))
        case("a .set alias of a thunk label", "assembler conditionals",
             src=lambda s: s + '__asm__(".set _thunk_o, _body_c\\n");\n')
        case("duplicate thunk rows", "belongs to no thunk-out/thunk-in entry", table=lambda t: t + "thunk|_thunk_o|call _body_t; btl $0,_flag; ret\n")
        case("a prior row that is not entry->target", "invalid prior table",
             prior='src/m/m.c|FS98_KEEP_EDX|i_entry|"_thunk_i"|0|i_entry->other|thunk_i|thunk-in:eax|body_t\n')
        case("gcc missing", "cannot run", path="/nonexistent")
    print("keep lint self-test: %s" % ("FAIL" if bad else "pass"))
    return 1 if bad else 0


def main(argv):
    if "--self-test" in argv:
        return self_test()
    root, prior = ROOT, None
    if "--root" in argv:
        root = argv[argv.index("--root") + 1]
    if "--prior" in argv:
        try:
            prior = open(argv[argv.index("--prior") + 1]).read()
        except OSError as e:
            print("keep lint: invalid prior table: %s" % e)
            return 1
    return check(root, prior)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
