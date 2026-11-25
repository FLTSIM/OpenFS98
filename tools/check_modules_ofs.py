#!/usr/bin/env python3
"""Regression test for tools/modules.sh, tools/modules.txt and tools/ofs (Cleanup 2, item A). No wine, nothing built.

    python3 -B tools/check_modules_ofs.py              the check
    python3 -B tools/check_modules_ofs.py --self-test  broken copies of modules.sh and ofs must each fail
    python3 -B tools/check_modules_ofs.py --update     rewrite tools/modules_golden.txt from the current tree

modules.sh: the five list queries, and for every module in modules.txt the installed-path-of, orig-module-of and
installed-name-of answers (the eight forms the callers use), against tools/modules_golden.txt. The golden file
is generated from the tree, so editing modules.txt on purpose means --update and reviewing the diff.
install.cmake: its own parse of modules.txt (cut out of the file and run with `cmake -P`) must give the rows
modules.sh gives.
ofs: run against a fake build directory with a `cmake` shim first on PATH: argv, exported VAR=value, target order,
a shim's unknown-target diagnostic and exit status passing through, the missing build directory message.
Limit: the default OFS_BUILD (build/win32-retail under the source root) is not exercised, only OFS_BUILD."""
import os, re, shutil, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLDEN = os.path.join(ROOT, "tools/modules_golden.txt")
LISTS = ["modules-of", "english-modules-of", "english-candidates-of", "shared-modules-of", "shared-candidates-of"]
PER_MODULE = ["installed-path-of", "orig-module-of", "installed-name-of"]
SHIM = """#!/bin/sh
prev=
for a in "$@"; do echo "arg: $a" >> "$SHIM_LOG"; [ "$prev" = --target ] && [ "$a" = nosuch ] && bad=1; prev=$a; done
echo "env: X=${X-unset} Y=${Y-unset}" >> "$SHIM_LOG"
[ -z "$bad" ] || { echo "gmake: *** No rule to make target 'nosuch'.  Stop." >&2; exit 7; }
"""


def run(args, **kw):
    p = subprocess.run(args, capture_output=True, text=True, **kw)
    return p.returncode, p.stdout, p.stderr


def modules_rows(root):
    return [l.split() for l in open(os.path.join(root, "tools/modules.txt")) if l.strip() and not l.startswith("#")]


def sh_answers(root):
    """modules.sh's answers as golden-file lines"""
    sh = os.path.join(root, "tools/modules.sh")
    out = []
    for q in LISTS:
        rc, o, e = run([sh, q])
        out.append("%s => %s%s" % (q, o.rstrip("\n"), "" if rc == 0 else " (exit %d)" % rc))
    for r in modules_rows(root):
        for q in PER_MODULE:
            rc, o, e = run([sh, q, r[0]])
            out.append("%s %s => %s%s" % (q, r[0], o.rstrip("\n"), "" if rc == 0 else " (exit %d)" % rc))
    rc, o, e = run([sh, "installed-path-of", "NOSUCH"])      # an unknown name is an error, not an empty answer
    out.append("installed-path-of NOSUCH => exit %d, %s" % (rc, re.sub(r" in .*", "", e.strip().split(": ", 1)[-1])))
    return out


def check_modules(root, golden):
    want = open(golden).read().splitlines()
    got = sh_answers(root)
    errs = ["modules.sh: got %r, golden has %r" % (g, w) for g, w in zip(got, want) if g != w]
    if len(got) != len(want):
        errs.append("modules.sh: %d answers, golden has %d" % (len(got), len(want)))
    return errs


def check_install_parse(root):
    """cmake/install.cmake's own parse of modules.txt against modules.sh"""
    text = open(os.path.join(root, "cmake/install.cmake")).read()
    m = re.search(r'set\(OFS_MODULES ""\).*?\nendforeach\(\)', text, re.S)
    if not m:
        return ["install.cmake: the modules.txt parse (set(OFS_MODULES \"\") .. endforeach()) was not found"]
    with tempfile.TemporaryDirectory() as t:
        script = os.path.join(t, "p.cmake")
        open(script, "w").write('set(CMAKE_SOURCE_DIR "%s")\n%s\nmessage("MODULES=${OFS_MODULES}")\n'
                                'message("SHARED=${OFS_SHARED_AND_ENGLISH}")\n' % (root, m.group(0)))
        rc, o, e = run(["cmake", "-P", script])
    if rc:
        return ["install.cmake parse failed: " + e.strip()]
    sh = os.path.join(root, "tools/modules.sh")
    def rows(names):
        r = []
        for n in names.split():
            r += [n, run([sh, "installed-path-of", n])[1].strip(), run([sh, "orig-module-of", n])[1].strip()[len("orig/"):]]
        return ";".join(r)
    want = {"MODULES": rows(run([sh, "modules-of"])[1]),
            "SHARED": rows(run([sh, "english-modules-of"])[1] + " " + run([sh, "shared-modules-of"])[1])}
    errs = []
    for k, v in re.findall(r"^(MODULES|SHARED)=(.*)$", e, re.M):
        if v != want[k]:
            errs.append("install.cmake %s: %s\n    modules.sh gives %s" % (k, v, want[k]))
    return errs or ([] if len(re.findall(r"^(?:MODULES|SHARED)=", e, re.M)) == 2 else ["install.cmake: no output"])


def check_ofs(ofs):
    errs = []
    with tempfile.TemporaryDirectory() as t:
        bd, bindir, log = (os.path.join(t, n) for n in ("build dir", "bin", "log"))   # a space: argument boundaries matter
        os.makedirs(bd); os.makedirs(bindir)
        open(os.path.join(bd, "CMakeCache.txt"), "w").close()
        shim = os.path.join(bindir, "cmake")
        open(shim, "w").write(SHIM); os.chmod(shim, 0o755)
        env = dict(os.environ, PATH=bindir + os.pathsep + os.environ["PATH"], OFS_BUILD=bd, SHIM_LOG=log)
        env.pop("X", None); env.pop("Y", None)
        def go(*a, **e):
            if os.path.exists(log): os.remove(log)
            rc, o, er = run([ofs, *a], env=dict(env, **e))
            return rc, o, er, (open(log).read().splitlines() if os.path.exists(log) else [])
        def call(target, y="two"):   # what the shim logs for one cmake call, one argument per line
            return ["arg: --build", "arg: " + bd, "arg: --target", "arg: " + target, "env: X=1 Y=" + y]
        rc, o, er, lg = go("X=1", "alpha", "Y=two", "beta")
        want = call("alpha") + call("beta")
        if rc != 0 or lg != want:
            errs.append("ofs: two targets and VARs: exit %d, shim saw %r, want %r" % (rc, lg, want))
        rc, o, er, lg = go("X=1", "alpha", "nosuch", "beta", "Y=two")
        if rc != 7 or "No rule to make target 'nosuch'" not in er or lg != call("alpha") + call("nosuch"):
            errs.append("ofs: unknown target: exit %d (want 7), stderr %r, shim saw %r" % (rc, er, lg))
        rc, o, er, lg = go("alpha", OFS_BUILD=os.path.join(t, "none"))
        if rc != 2 or not er.startswith("no CMake build in %s:" % os.path.join(t, "none")) or lg:
            errs.append("ofs: missing build dir: exit %d, stderr %r, shim saw %r" % (rc, er, lg))
        for a in ((), ("X=1",), ("-j", "alpha")):
            rc, o, er, lg = go(*a)
            if rc != 2 or not ("usage" in er or "no options" in er) or lg:
                errs.append("ofs %r: exit %d, stderr %r, shim saw %r (want exit 2, no cmake call)" % (a, rc, er, lg))
    return errs


def check_candidates(root):
    """the two candidate queries have empty answers in the tree: use a temporary modules.txt with one of each kind"""
    with tempfile.TemporaryDirectory() as t:
        os.makedirs(os.path.join(t, "tools"))
        shutil.copy(os.path.join(root, "tools/modules.sh"), os.path.join(t, "tools/modules.sh"))
        open(os.path.join(t, "tools/modules.txt"), "w").write(
            "# fixture\nAAA module Modules/AAA.DLL Modules/AAA.DLL\nBBB english-candidate English/B.DLL English/b.dll\n"
            "CCC shared-candidate C.DLL C.DLL\nDDD english English/D.DLL English/D.DLL\nEEE shared E.DLL E.DLL\n")
        errs = []
        for q, want in (("english-candidates-of", "BBB"), ("shared-candidates-of", "CCC"), ("modules-of", "AAA"),
                        ("english-modules-of", "DDD"), ("shared-modules-of", "EEE")):
            rc, o, e = run([os.path.join(t, "tools/modules.sh"), q])
            if rc != 0 or o != want + "\n":
                errs.append("modules.sh %s on a fixture: exit %d, got %r, want %r" % (q, rc, o, want))
        return errs


def tree(root):
    """a copy of tools/modules.sh, modules.txt and ofs under a temporary root, for the self-test to break"""
    os.makedirs(os.path.join(root, "tools"))
    for f in ("modules.sh", "modules.txt", "ofs"):
        shutil.copy(os.path.join(ROOT, "tools", f), os.path.join(root, "tools", f))


def self_test():
    def expect(errs, word, what):
        assert errs and any(word in e for e in errs), (what, errs)
    assert not check_candidates(ROOT) and not check_modules(ROOT, GOLDEN) and not check_ofs(os.path.join(ROOT, "tools/ofs")) and not check_install_parse(ROOT)
    def edit(path, old, new):
        s = open(path).read(); assert old in s, (path, old)
        open(path, "w").write(s.replace(old, new, 1))
    with tempfile.TemporaryDirectory() as t:
        tree(t)
        edit(os.path.join(t, "tools/modules.sh"), "echo \"orig/$5\"", "echo \"orig/$4\"")
        expect(check_modules(t, GOLDEN), "orig-module-of", "orig column swapped")
    with tempfile.TemporaryDirectory() as t:
        tree(t)
        edit(os.path.join(t, "tools/modules.txt"), "MP_DIALOG english English/MP_DIALOG.DLL English/mpdialog.dll",
             "MP_DIALOG english English/MP_DIALOG.DLL English/MP_DIALOG.DLL")
        expect(check_modules(t, GOLDEN), "MP_DIALOG", "case-sensitive mapping changed")
    with tempfile.TemporaryDirectory() as t:
        tree(t); os.makedirs(os.path.join(t, "cmake"))
        shutil.copy(os.path.join(ROOT, "cmake/install.cmake"), os.path.join(t, "cmake"))
        assert not check_install_parse(t)
        edit(os.path.join(t, "cmake/install.cmake"), "list(GET f 3 retail)", "list(GET f 2 retail)")
        expect(check_install_parse(t), "install.cmake", "install parse reads the wrong column")
    with tempfile.TemporaryDirectory() as t:
        tree(t)
        edit(os.path.join(t, "tools/modules.sh"), "kind=english-candidate", "kind=shared-candidate")
        expect(check_candidates(t), "english-candidates-of", "candidate kinds swapped")
    for old, new, word in (("export \"$a\"", ":", "ofs: two targets"),
                           ("cmake --build \"$build\" --target \"$t\" ||", "cmake \"--build $build\" --target \"$t\" ||", "ofs: two targets"),
                           ("cmake --build \"$build\" --target \"$t\" ||", "cmake --build \"$build\" \"--target $t\" ||", "ofs: two targets"),
                           ("targets=\"$targets $a\"", "targets=\"$a $targets\"", "ofs: two targets"),
                           ("cmake --build \"$build\" --target \"$t\" || exit $?", "cmake --build \"$build\" --target \"$t\"", "unknown target"),
                           ("exit 2\n}", "exit 1\n}", "missing build dir")):
        with tempfile.TemporaryDirectory() as t:
            tree(t); edit(os.path.join(t, "tools/ofs"), old, new)
            expect(check_ofs(os.path.join(t, "tools/ofs")), word, old)
    print("check_modules_ofs: self-test passed")


def main():
    if "--self-test" in sys.argv:
        return self_test()
    if "--update" in sys.argv:
        lines = sh_answers(ROOT)       # collected and checked before anything is written
        bad = [l for l in lines if "(exit" in l or ("=> exit" in l and "NOSUCH" not in l)]
        if bad or not any("NOSUCH => exit 2" in l for l in lines):
            sys.exit("check_modules_ofs: --update refused, unexpected answers: %r" % bad[:3])
        with tempfile.NamedTemporaryFile("w", dir=os.path.dirname(GOLDEN), delete=False) as f:
            f.write("\n".join(lines) + "\n")
        os.replace(f.name, GOLDEN)
        return print("wrote", GOLDEN)
    errs = check_candidates(ROOT) + check_modules(ROOT, GOLDEN) + check_install_parse(ROOT) + check_ofs(os.path.join(ROOT, "tools/ofs"))
    for e in errs:
        print("error:", e)
    print("modules/ofs: %d golden answers, %s" % (len(open(GOLDEN).read().splitlines()), "FAIL" if errs else "ok"))
    sys.exit(1 if errs else 0)


main()
