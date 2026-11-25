#!/usr/bin/env python3
"""Cleanup 2, item B: the codegen overrides of cmake/toolchains/win32-retail.cmake must stay on their compile commands.

    python3 -B tools/check_codegen_overrides.py              the build's check (ofs_codegen_check, like the arena lint)
    python3 -B tools/check_codegen_overrides.py --inventory  every gcc recipe found, with the overrides it carries
    python3 -B tools/check_codegen_overrides.py --self-test
    options: --root DIR (default: the source root)

The table is the `#@` rows in the toolchain file:
    #@ <flag>[,<flag>] | <file> | <output> | <source> | <count> | <reason>
A site is a gcc recipe: `file` is cmake/steps/*.cmake, cmake/targets.cmake or a tools/*.py runner that compiles units
itself; `output` is the token after -o and `source` the first .c/.S token (fnmatch patterns; a Python runner's output
is `*`). A row matches every recipe of that file with that output and source, and states how many that is
(PANELS_unit.dll appears three times in targets.cmake): each must carry the flag, and a different count fails. CMake
recipes are bracket arguments ([==[ ... ]==]) or the one loop-generated template (SIMSUI's units, expanded from
src/simsui/sources.txt). A row fails if it matches no recipe, or one lacks its flag; a flag that no recipe anywhere
carries fails too. A recipe carrying a flag no row lists is allowed.
Runners that delegate to CMake recipes (SIM1R through `difftests`; ANALYSIS, CONTROLS, G3D, PANELS, REALITY and
WINDOW through the break-build recipes in targets.cmake) are covered through those recipes. `ofs_revert` and a
direct `cmake --build --target <output>` of a custom command bypass the check, as they do the arena lint."""
import ast, fnmatch, os, re, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLCHAIN = "cmake/toolchains/win32-retail.cmake"
GCC = "i686-w64-mingw32-gcc"
FLAGS = ["-fno-ipa-icf", "-fno-tree-loop-distribute-patterns", "-fno-tree-vectorize", "-fno-math-errno"]
SRC = re.compile(r"\.(c|S)$")


def cmake_recipes(root, rel):
    """-> [(line, output, source, [tokens in order])] of a CMake file; commented lines do not count"""
    text = re.sub(r"(?m)^[ \t]*#.*$", "", open(os.path.join(root, rel)).read())   # same line numbers
    out = []
    for m in re.finditer(re.escape(GCC), text):
        quoted = text[m.start() - 1] == '"'      # a generated template (list(APPEND ... "gcc ...")), not a bracket argument
        end = re.compile(r'"|\n' if quoted else r"\]==\]|\n|@SEMI@|&&|\|\||\|").search(text, m.end())
        cmds = [text[m.end():end.start()]]
        if "${n}" in cmds[0]:                   # the loop of all.cmake: one compile per unit of src/simsui/sources.txt
            var = re.search(r"list\(APPEND\s+(\w+)\s*$", text[:m.start() - 1])
            refs = [r for r in re.finditer(r"\$\{%s\}" % re.escape(var.group(1)), text) if var] if var else []
            if "foreach(s IN LISTS OFS_SIMSUI_SOURCES)" not in text or not var:
                raise SystemExit("%s: an unknown generated recipe (${n}); teach check_codegen_overrides.py" % rel)
            if not any(re.search(r"ofs_step\((?:(?!\n\)).)*$", text[:r.start()], re.S) for r in refs):
                continue                        # the template is never put in a step's COMMANDS: not a compile
            units = open(os.path.join(root, "src/simsui/sources.txt")).read().split()
            cmds = [cmds[0].replace("${n}", os.path.splitext(os.path.basename(s))[0]).replace("${s}", s) for s in units]
        for c in cmds:
            toks = c.split()
            o = toks[toks.index("-o") + 1] if "-o" in toks else "-"
            s = next((t for t in toks if SRC.search(t)), "-")
            out.append((text.count("\n", 0, m.start()) + 1, o, s, toks))
    return out


def py_recipes(root, rel):
    """a runner's own compile: the string constants of the argv list (list or tuple) that holds the compiler's name"""
    tree = ast.parse(open(os.path.join(root, rel)).read())
    out = []
    for node in ast.walk(tree):
        if isinstance(node, (ast.List, ast.Tuple)) and any(isinstance(e, ast.Constant) and e.value == GCC for e in node.elts):
            strs = [n.value for n in ast.walk(node) if isinstance(n, ast.Constant) and isinstance(n.value, str)]
            out.append((node.lineno, "*", next((t for t in strs if SRC.search(t)), "-"), strs))
    return out


def recipes(root, rel):
    return (py_recipes if rel.endswith(".py") else cmake_recipes)(root, rel)


def rows(root):
    r = []
    for n, l in enumerate(open(os.path.join(root, TOOLCHAIN)), 1):
        if l.startswith("#@"):
            f = [x.strip() for x in l[2:].split(" | ")]
            if len(f) != 6 or not all(f) or not f[4].isdigit():
                raise SystemExit("%s:%d: a #@ row is flag | file | output | source | count | reason" % (TOOLCHAIN, n))
            r.append((n, f[0].split(","), f[1], f[2], f[3], int(f[4])))
    return r


def check(root):
    errs, cache, seen = [], {}, set()
    allflags = set()
    for rel in [os.path.join("cmake/steps", f) for f in sorted(os.listdir(os.path.join(root, "cmake/steps"))) if f.endswith(".cmake")] + ["cmake/targets.cmake"]:
        cache[rel] = recipes(root, rel)
    rs = rows(root)
    if not rs:
        errs.append("%s: no #@ rows" % TOOLCHAIN)
    for n, flags, rel, out, src, count in rs:
        if rel not in cache:
            cache[rel] = recipes(root, rel)
        sites = [x for x in cache[rel] if fnmatch.fnmatchcase(x[1], out) and fnmatch.fnmatchcase(x[2], src)]
        if len(sites) != count:
            errs.append("%s:%d: %d sites match %s %s <- %s, the row says %d (renamed, added or removed?)" % (TOOLCHAIN, n, len(sites), rel, out, src, count))
        for flag in flags:
            seen.add(flag)
            opposite = "-f" + flag[5:]       # -fno-X is undone by a later -fX
            for line, o, s, toks in sites:
                if flag not in toks:
                    errs.append("%s:%d: missing %s (%s <- %s)" % (rel, line, flag, o, s))
                elif opposite in toks[toks.index(flag):]:
                    errs.append("%s:%d: %s is undone by a later %s (%s <- %s)" % (rel, line, flag, opposite, o, s))
    for rel, rl in cache.items():
        for x in rl:
            allflags |= set(x[3])
    for flag in sorted(seen - allflags):
        errs.append("%s: %s is listed but no compile command carries it" % (TOOLCHAIN, flag))
    return errs, len(rs), sum(len(v) for v in cache.values())


def inventory(root):
    files = ["cmake/steps/" + f for f in sorted(os.listdir(os.path.join(root, "cmake/steps")))] + ["cmake/targets.cmake", "tools/simsui_breaks.py"]
    for rel in files:
        for line, o, s, toks in recipes(root, rel):
            print("%s:%d %s <- %s %s" % (rel, line, o, s, [f for f in FLAGS if f in toks]))


def self_test():
    with tempfile.TemporaryDirectory() as t:
        def w(rel, text):
            os.makedirs(os.path.dirname(os.path.join(t, rel)), exist_ok=True)
            open(os.path.join(t, rel), "w").write(text)
        w("src/simsui/sources.txt", "src/simsui/a.c src/simsui/b.c\n")
        base_t = ("# table\n#@ -fno-ipa-icf | cmake/targets.cmake | bin/U.dll | src/u.c | 3 | dup\n"
                  "#@ -fno-tree-vectorize | cmake/steps/all.cmake | bin/s/*.o | src/simsui/*.c | 2 | loop\n"
                  "#@ -fno-math-errno | tools/r.py | * | tools/h.c | 1 | runner\n")
        base_a = ('foreach(s IN LISTS OFS_SIMSUI_SOURCES)\n  get_filename_component(n "${s}" NAME_WE)\n'
                  '  list(APPEND u "%s -fno-tree-vectorize -c -o bin/s/${n}.o ${s}")\nendforeach()\n'
                  'ofs_step(all\n  COMMANDS\n    ${u}\n  )\n' % GCC)
        recipe = "    [==[%s -fno-ipa-icf -o bin/U.dll src/u.c]==]\n"
        base_g = (recipe % GCC) * 3 + "    [==[%s -x -o bin/other.o src/o.c]==]\n" % GCC
        base_p = "cc = ['%s', '-fno-math-errno', '-o', str(x), 'tools/h.c']\n" % GCC
        def tree(tc=base_t, a=base_a, g=base_g, p=base_p):
            w(TOOLCHAIN, tc); w("cmake/steps/all.cmake", a); w("cmake/targets.cmake", g); w("tools/r.py", p)
            return check(t)[0]
        def fails(errs, *words):
            assert errs and all(any(x in e for e in errs) for x in words), (errs, words)
        assert tree() == [], tree()
        parts = base_g.split("\n")
        fails(tree(g="\n".join([parts[0], parts[1].replace(" -fno-ipa-icf", ""), parts[2], parts[3]])), "targets.cmake:2", "missing -fno-ipa-icf")
        # one of three duplicates renamed and stripped of its flag: the siblings still match, but only two do
        fails(tree(g="\n".join([parts[0], parts[1].replace(" -fno-ipa-icf", "").replace("U.dll", "V.dll"), parts[2], parts[3]])), "2 sites match", "says 3")
        fails(tree(g=base_g.replace("bin/U.dll", "bin/V.dll")), "0 sites match")                 # all renamed
        fails(tree(g=base_g + (recipe % GCC)), "4 sites match")                                    # a duplicate added
        fails(tree(g="\n".join([parts[0], "    # " + parts[1].strip(), parts[2], parts[3]])), "2 sites match")   # commented out
        fails(tree(g=base_g.replace(" -fno-ipa-icf", " -fno-ipa-icf -fipa-icf", 1)), "undone by a later -fipa-icf")
        assert tree(g=base_g.replace(" -fno-ipa-icf", " -fipa-icf -fno-ipa-icf", 1)) == []      # re-disabled afterwards: fine
        fails(tree(a=base_a.replace(" -fno-tree-vectorize", "")), "all.cmake:3", "missing -fno-tree-vectorize")   # the loop
        fails(tree(a=base_a.replace("    ${u}\n", "")), "0 sites match")                         # the template is not in the step
        assert tree(g=base_g.replace("-x -o", "-fno-ipa-icf -fno-extra -o")) == []              # an extra use is allowed
        fails(tree(p=base_p.replace("'-fno-math-errno', ", "")), "r.py", "missing -fno-math-errno")
        fails(tree(p="cc = ['%s',\n  # '-fno-math-errno',\n  '-o', str(x), 'tools/h.c']\n" % GCC), "r.py", "missing -fno-math-errno")   # commented out
        fails(tree(tc=base_t + "#@ -fno-unused-flag | tools/r.py | * | tools/h.c | 1 | n\n"), "missing -fno-unused-flag")
        fails(tree(tc=base_t + "#@ -fno-nowhere | cmake/targets.cmake | bin/other.o | src/o.c | 1 | n\n"), "no compile command carries")
    print("check_codegen_overrides: self-test passed")


def main():
    global ROOT
    if "--root" in sys.argv:
        ROOT = sys.argv[sys.argv.index("--root") + 1]
    if "--self-test" in sys.argv:
        return self_test()
    if "--inventory" in sys.argv:
        return inventory(ROOT)
    errs, nrows, nrec = check(ROOT)
    for e in errs:
        print("error:", e)
    print("codegen overrides: %d rows, %d compile commands, %s" % (nrows, nrec, "FAIL" if errs else "ok"))
    sys.exit(1 if errs else 0)


main()
