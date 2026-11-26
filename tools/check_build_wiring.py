#!/usr/bin/env python3
"""Recurring check of the CMake build's wiring: cleanup 1's rebuild edges and the lint wiring (Cleanup 3a).

usage: tools/check_build_wiring.py [--self-test]
Copies the source tree into a scratch directory (tempfile.mkdtemp, or under $OFS_WIRING_SCRATCH; never inside the tree),
configures and builds it once (`--target openfs98`, then `--target difftests`) with OFS_STEP_LOG set (cmake/OpenFS98.cmake's
ofs_step appends a recipe's step id, its first output, to that file when the recipe runs), then for each case mutates the
copy, builds the same two targets and requires the exact set of step ids that ran, then builds twice more and requires none.
File timestamps cannot say which recipes ran (`cmp -s ... || cp` steps leave their output untouched); the log can.
Prints `build wiring: <k> cases, <N> steps, ok`, or the failing case with its unexpected and missing ids and exits 1."""
import os, re, shutil, signal, subprocess, sys, tempfile
from collections import Counter

SRC = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
B = "build/win32-retail"
GAUGES, MDL = "bin/gauges-all/built.stamp", "bin/mdl-diff/modules.json"
HARNESSES = ["bin/gauges-all/allgaugesdiff.exe", "bin/gauges-all/allpaneldiff.exe"]
# a copy without git: everything but run output and the machine's fixtures (shortcut: names, not a manifest; revisit if a slot layout adds more)
NOGIT_SKIP = ("bin", "game", "cd", "wineprefix", "slots", "RESULTS", ".git", "__pycache__")


def parse_log(text):
    """-> sorted step ids, one per line (a step that ran twice shows twice)"""
    return sorted(l.strip() for l in text.splitlines() if l.strip())


def diff_sets(got, want):
    """-> (unexpected, missing) as sorted lists, counting repeats"""
    g, w = Counter(got), Counter(want)
    return sorted((g - w).elements()), sorted((w - g).elements())


# the ids of the 28 single-pair --hash steps (25 modules, SIMSUI, SIM1, HOST), pinned by name from cleanup 1's notes
SINGLES = ["bin/%s.DLL" % m for m in (
    "ACONTAIN ADVDRV ANALYSIS ATC BOOSTER CONTROLS DEMO DYNAMIC FACIL FE G3D GLOBAL HG2D LANDME MAIN MPCHAT MULTIP NUMERIC "
    "PANELS REALITY SOUND UTIL WEATHERM WINDOW WINMSG").split()] + ["bin/SIMSUI.DLL", "bin/SIM1.sim", "bin/OPENFS98.EXE"]
LINT_OK = (r"(?m)^arena lint: \d+ files allowlisted, \d+ casts, \d+ defines left", r"(?m)^codegen overrides: .*, ok$")


def lint_ran(out):
    return all(re.search(r, out) for r in LINT_OK)


def self_test():
    assert parse_log("b\na\n\n b \n") == ["a", "b", "b"]
    assert diff_sets(["a", "b"], ["a", "b"]) == ([], [])
    assert diff_sets(["a", "b", "b"], ["a", "b"]) == (["b"], [])      # a repeat is unexpected
    assert diff_sets(["a"], ["a", "c"]) == ([], ["c"])
    assert diff_sets(["x", "a"], ["a", "c"]) == (["x"], ["c"])
    assert len(SINGLES) == 28 and len(set(SINGLES)) == 28
    assert lint_ran("arena lint: 41 files allowlisted, 100 casts, 27 defines left (reverse casts: 23)\ncodegen overrides: 22 rows, 279 compile commands, ok\n")
    assert not lint_ran("arena lint: 41 files allowlisted, 100 casts, 27 defines left (reverse casts: 23)\ncodegen overrides: 22 rows, 279 compile commands, FAIL\n")
    assert not lint_ran("codegen overrides: 22 rows, 1 compile commands, ok\n")
    assert not lint_ran("arena lint: no prior allowlist (no git, or none in HEAD): checked only the working tree\ncodegen overrides: 22 rows, 279 compile commands, ok\n")
    assert configures("-- Configuring done\n-- Generating done\n[ 1%] x\n-- Configuring done (0.1s)\n") == 2
    print("check_build_wiring: self-test passed")


def configures(out):
    return len(re.findall(r"(?m)^-- Configuring done", out))


SIGS = {signal.SIGTERM, signal.SIGHUP, signal.SIGINT}


class Build:
    def __init__(self, scratch):
        self.scratch = scratch
        self.dir = os.path.join(scratch, "tree")
        self.log = os.path.join(scratch, "steps.log")
        self.env = dict(os.environ, OFS_STEP_LOG=self.log, PYTHONDONTWRITEBYTECODE="1")
        self.env.pop("UPDATE", None)
        self.procs = set()

    def run(self, argv):
        """-> (rc, output); its own process group (same session as the caller), which a signal or an exception kills"""
        old = signal.pthread_sigmask(signal.SIG_BLOCK, SIGS)    # no handler runs between Popen and the registration
        try:
            def child():
                os.setpgid(0, 0)
                signal.pthread_sigmask(signal.SIG_SETMASK, old)
            p = subprocess.Popen(argv, cwd=self.dir, env=self.env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 text=True, errors="replace", preexec_fn=child)
            self.procs.add(p)
        finally:
            signal.pthread_sigmask(signal.SIG_SETMASK, old)
        try:
            out = p.communicate()[0]
        finally:
            self.kill(p)
            self.procs.discard(p)
        return p.returncode, out

    @staticmethod
    def kill(p):
        try:
            os.killpg(p.pid, signal.SIGKILL)    # also takes stragglers of a finished build
        except (ProcessLookupError, PermissionError):
            pass
        try:
            p.wait()    # reaped before the scratch directory goes
        except Exception:
            pass

    def kill_all(self):
        for p in list(self.procs):
            self.kill(p)

    def target(self, t):
        """build one target -> (rc, output, ids logged)"""
        open(self.log, "w").close()
        rc, out = self.run(["cmake", "--build", B, "--target", t, "-j", "2"])
        return rc, out, parse_log(open(self.log).read())

    def pair(self):
        """openfs98 then difftests -> (rc of the first failing build or 0, output, ids)"""
        ids, outs = [], ""
        for t in ("openfs98", "difftests"):
            rc, out, got = self.target(t)
            ids += got
            outs += out
            if rc:
                return rc, outs, sorted(ids)
        return 0, outs, sorted(ids)

    def p(self, rel):
        """a path to mutate: it must resolve inside the scratch directory (a symlink out of it would edit the real tree)"""
        f = os.path.join(self.dir, rel)
        r, root = os.path.realpath(f), os.path.realpath(self.scratch)
        if os.path.commonpath([r, root]) != root:
            raise Fail("%s resolves outside the scratch directory (%s)" % (rel, r))
        return f

    def touch(self, rel):
        os.utime(self.p(rel))

    def remove(self, rel):
        f = self.p(rel)
        shutil.rmtree(f) if os.path.isdir(f) else os.remove(f)

    def edit(self, rel, old, new):
        f = self.p(rel)
        s = open(f).read()
        assert s.count(old) == 1, "%s: %r found %d times" % (rel, old, s.count(old))
        open(f, "w").write(s.replace(old, new))


def copy_tree(dst):
    """The working-tree files of the source tree (git's list, read from disk, symlinks followed so every file is private),
    or everything bar run output; then orig/ once."""
    git = None
    if os.path.lexists(os.path.join(SRC, ".git")):
        r = subprocess.run(["git", "-C", SRC, "ls-files", "-z"], capture_output=True)
        if r.returncode == 0:
            git = [x.decode() for x in r.stdout.split(b"\0") if x]
    if git is not None:
        for rel in git:
            s = os.path.join(SRC, rel)
            if os.path.isfile(s):    # deleted in the working tree (or a dangling link): skipped
                os.makedirs(os.path.dirname(os.path.join(dst, rel)) or dst, exist_ok=True)
                shutil.copy2(s, os.path.join(dst, rel))
    else:
        for e in os.listdir(SRC):
            if e in NOGIT_SKIP or e == "orig" or e.startswith("build"):
                continue
            s = os.path.join(SRC, e)
            if os.path.isdir(s):
                shutil.copytree(s, os.path.join(dst, e), ignore=shutil.ignore_patterns("__pycache__", "*.pyc"), ignore_dangling_symlinks=True)
            elif os.path.isfile(s):
                shutil.copy2(s, os.path.join(dst, e))
    shutil.copytree(os.path.join(SRC, "orig"), os.path.join(dst, "orig"), ignore_dangling_symlinks=True)


class Fail(Exception):
    pass


def expect(case, what, got, want, extra=""):
    un, miss = diff_sets(got, want)
    if un or miss:
        raise Fail("%s: %s ran the wrong steps\n  unexpected: %s\n  missing: %s%s" % (case, what, un, miss, extra))


def tail(out):
    return "\n" + "\n".join(out.splitlines()[-15:])


class State:
    scratch = None
    b = None


def cleanup():
    if State.b:
        State.b.kill_all()
    if State.scratch:
        shutil.rmtree(State.scratch, ignore_errors=True)
        State.scratch = None


def on_signal(sig, frm):
    raise SystemExit(128 + sig)    # no waiting here (communicate() may hold the child's wait lock); cleanup runs once the stack has unwound


def main_run():
    for sg in SIGS:
        signal.signal(sg, on_signal)
    base = os.environ.get("OFS_WIRING_SCRATCH")
    old = signal.pthread_sigmask(signal.SIG_BLOCK, SIGS)    # a signal must not land between mkdtemp and its registration
    try:
        scratch = os.path.realpath(tempfile.mkdtemp(prefix="ofs-wiring-", dir=os.path.abspath(base) if base else None))
        State.scratch = scratch
    finally:
        signal.pthread_sigmask(signal.SIG_SETMASK, old)
    if os.path.commonpath([scratch, os.path.realpath(SRC)]) == os.path.realpath(SRC):
        cleanup()
        sys.exit("check_build_wiring: the scratch directory %s is inside the source tree" % scratch)
    b = State.b = Build(scratch)
    try:
        os.makedirs(b.dir)
        copy_tree(b.dir)
        rc, out = b.run(["cmake", "-S", ".", "-B", B, "-DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/win32-retail.cmake"])
        if rc:
            raise Fail("configure failed" + tail(out))
        rc, out, first = b.pair()
        if rc:
            raise Fail("the first build failed" + tail(out))
        inventory = set(first)
        if len(first) != len(inventory):
            raise Fail("the first build ran a step twice: %s" % [k for k, v in Counter(first).items() if v > 1])
        n = len(inventory)
        k = 0
        DATA = SINGLES + [GAUGES, MDL] + HARNESSES    # what touching tools/data_images.txt reruns
        for i in DATA + ["bin/difftest/advdrvdiff.exe", "bin/FE.DLL"]:
            if i not in inventory:
                raise Fail("step id %s is not in the first build's log (the inventory changed?)" % i)

        def quiet(name, what):
            """a build that must run no step, reconfigure nothing and still run both lints"""
            rc, out, got = b.pair()
            if rc or got or configures(out) or not lint_ran(out):
                raise Fail("%s: %s: rc %d, steps %s, %d configure passes, lints %sshown%s" % (
                    name, what, rc, got, configures(out), "" if lint_ran(out) else "not ", tail(out)))

        quiet("first build", "the second build")

        def case(name, mutate, want, conf=0):
            nonlocal k
            k += 1
            mutate()
            rc, out, got = b.pair()
            if rc:
                raise Fail("%s: the build failed%s" % (name, tail(out)))
            expect(name, "the build", got, want)
            if configures(out) != conf:
                raise Fail("%s: %d configure passes, expected %d" % (name, configures(out), conf))
            quiet(name, "the second build")

        case("touch data_images.txt", lambda: b.touch("tools/data_images.txt"), DATA, conf=1)
        case("touch advdrv_relocs.inc", lambda: b.touch("src/advdrv/advdrv_relocs.inc"), ["bin/ADVDRV.DLL", "bin/difftest/advdrvdiff.exe"])
        case("touch fe_library.c", lambda: b.touch("src/fe/fe_library.c"), ["bin/FE.DLL"])
        gau = sorted(f for f in os.listdir(b.p("src/gauges")) if f.endswith(".c"))[0]
        case("touch src/gauges/" + gau, lambda: b.touch("src/gauges/" + gau), [GAUGES] + HARNESSES)
        case("delete all_panels.inc", lambda: b.remove("bin/gauges-all/all_panels.inc"), [GAUGES] + HARNESSES)
        case("remove bin/gauges-all", lambda: b.remove("bin/gauges-all"), [GAUGES] + HARNESSES)

        # a wrong pinned hash on WINMSG's row, edited normally. The edit dirties every hash step (and reconfigures), so the
        # failing pass runs the steps before WINMSG's and stops there: only WINMSG's presence and "nothing outside the
        # data_images set" are asserted, the order of a failing pass is not.
        k += 1
        name = "wrong WINMSG hash"
        row = [l for l in open(b.p("tools/data_images.txt")) if l.startswith("WINMSG ")][0]
        bad = re.sub(r"sha256=([0-9a-f])", lambda m: "sha256=" + ("0" if m[1] != "0" else "1"), row.rstrip("\n")) + "\n"
        diag = "bin/WINMSG.DLL: our .data image (or its relocations) differs from the manifest"

        def failing(which, conf):
            rc, out, got = b.target("openfs98")
            if rc == 0:
                raise Fail("%s: the %s build passed with a wrong pin (a failed check left an output that passes)%s" % (name, which, tail(out)))
            if diag not in out or "CMake Error" in out:
                raise Fail("%s: the %s build did not fail with WINMSG's hash-mismatch diagnostic%s" % (name, which, tail(out)))
            if "bin/WINMSG.DLL" not in got or set(got) - set(DATA):
                raise Fail("%s: the %s failing build logged %s (WINMSG's step expected, nothing outside the data_images set)" % (name, which, got))
            if configures(out) != conf:
                raise Fail("%s: %d configure passes in the %s build, expected %d" % (name, configures(out), which, conf))
        b.edit("tools/data_images.txt", row, bad)
        failing("first", 1)
        failing("repeated", 0)
        b.edit("tools/data_images.txt", bad, row)
        rc, out, got = b.pair()
        if rc:
            raise Fail("%s: the build failed after the row was restored%s" % (name, tail(out)))
        expect(name, "the build after the restore", got, DATA)
        if configures(out) != 1:
            raise Fail("%s: %d configure passes after the restore, expected 1" % (name, configures(out)))
        quiet(name, "the build after that")

        # lint wiring: ADVDRV's recipe loses -fno-ipa-icf, so the codegen check (riding on ofs_arena_lint) must stop everything
        k += 1
        name = "lint wiring"
        allc = "cmake/steps/all.cmake"
        lines = open(b.p(allc)).read().split("\n")
        ln = [i for i, l in enumerate(lines, 1) if "-Wl,--image-base=0x20610000 -o bin/ADVDRV.DLL" in l]
        if len(ln) != 1:
            raise Fail("%s: ADVDRV's compile line not found in %s" % (name, allc))
        msg = "error: %s:%d: missing -fno-ipa-icf (bin/ADVDRV.DLL <- src/advdrv/advdrv.c)" % (allc, ln[0])
        rc, graph_out = b.run(["python3", "-B", "tools/modgraph.py", "orig/Modules"])
        marks = [l.strip() for l in graph_out.splitlines() if l.strip()]
        if rc or len(marks) < 3:
            raise Fail("%s: tools/modgraph.py gave no output" % name)

        def lines_of(out):
            return {l.strip() for l in out.splitlines()}

        def probe(target, conf):
            rc, out, got = b.target(target)
            if rc == 0 or msg not in lines_of(out) or got:
                raise Fail("%s: %s with the flag dropped (rc %d, steps %s) must fail with the line %r and run no step%s" % (name, target, rc, got, msg, tail(out)))
            if set(marks) & lines_of(out):
                raise Fail("%s: %s with the flag dropped printed graph output: %s" % (name, target, sorted(set(marks) & lines_of(out))))
            if configures(out) != conf:
                raise Fail("%s: %d configure passes in the %s probe, expected %d" % (name, configures(out), target, conf))
        line = lines[ln[0] - 1]
        b.edit(allc, line, line.replace(" -fno-ipa-icf", "", 1))
        probe("graph", 1)
        probe("graph", 0)
        b.touch("tools/data_images.txt")    # most steps dirty: none may start
        probe("openfs98", 1)
        b.edit(allc, line.replace(" -fno-ipa-icf", "", 1), line)
        rc, out, got = b.target("graph")
        if rc or got or not set(marks) <= lines_of(out) or configures(out) != 1:
            raise Fail("%s: graph with the flag restored (rc %d, steps %s, %d configure passes)%s" % (name, rc, got, configures(out), tail(out)))
        rc, out, got = b.pair()
        if rc:
            raise Fail("%s: the build with the flag restored failed%s" % (name, tail(out)))
        expect(name, "the build with the flag restored", got, DATA)
        quiet(name, "the build after that")
        print("build wiring: %d cases, %d steps, ok" % (k, n))
    except Fail as e:
        print("build wiring: FAIL: %s" % e)
        sys.exit(1)
    finally:
        old = signal.pthread_sigmask(signal.SIG_BLOCK, SIGS)
        try:
            cleanup()
        finally:
            signal.pthread_sigmask(signal.SIG_SETMASK, old)


if __name__ == "__main__":
    if sys.argv[1:] == ["--self-test"]:
        self_test()
    elif len(sys.argv) == 1:
        main_run()
    else:
        sys.exit(__doc__)
