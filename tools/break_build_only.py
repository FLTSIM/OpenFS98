"""The --build-only mode of the Story 1.8 break runners (WINDOW, WINMSG, MPCHAT): build every case's mutant DLL, one at a
time, and keep it as DEST/<case>/mutant.dll with meta.json; run nothing.  The temporary source and the built output go
back (bytes, mtimes, existence) whatever happens.  Exit 1 on no matching case, an anchor that does not match exactly
once, or a build failure not on the expected list; all of that is checked before any directory is made, except a build."""
import json
import os
import shutil
import signal
import subprocess
import sys


def _save(path):
    if not os.path.exists(path):
        return None
    st = os.stat(path)
    return open(path, "rb").read(), st.st_atime_ns, st.st_mtime_ns


def _restore(path, saved):
    if saved is None:
        if os.path.exists(path):
            os.remove(path)
        return
    open(path, "wb").write(saved[0])
    os.utime(path, ns=(saved[1], saved[2]))


def run(source, entries, only, tmp, out, build_cmd, dest, expected_unbuilt=()):
    """entries: BREAKS rows (name, old, new[, why]); build_cmd: argv that turns `tmp` into `out`."""
    cases = [e for e in entries if not only or e[0] in only]
    unknown = [n for n in only if n not in {e[0] for e in entries}]
    if not cases or unknown:               # every named filter must match a case
        print("no case matches %s" % " ".join(unknown or only))
        return 1
    bad = ["%s (anchor appears %d times)" % (e[0], source.count(e[1])) for e in cases if source.count(e[1]) != 1]
    if bad:
        print("anchors not unique: " + ", ".join(bad))
        return 1
    saved = {p: _save(p) for p in (tmp, out)}
    built, failed = 0, []
    for sig, code in ((signal.SIGTERM, 143), (signal.SIGHUP, 129)):     # a kill still runs the restore below
        signal.signal(sig, lambda *_, code=code: sys.exit(code))
    try:
        os.makedirs(dest, exist_ok=True)
        for e in cases:
            name, old, new = e[:3]
            d = os.path.join(dest, name)
            shutil.rmtree(d, ignore_errors=True)
            os.makedirs(d)
            open(tmp, "w").write(source.replace(old, new, 1))
            if os.path.exists(out):
                os.remove(out)
            r = subprocess.run(build_cmd, capture_output=True, text=True)
            meta = {"name": name, "equivalent": len(e) > 3, "why": e[3] if len(e) > 3 else ""}
            if r.returncode != 0 or not os.path.exists(out):
                if name not in expected_unbuilt:
                    print("%-20s BUILD FAILED\n%s" % (name, r.stderr[-800:]))
                    failed.append(name)
                    continue
                meta["unbuilt"] = True
                meta["stderr"] = r.stderr
            else:
                shutil.copy(out, os.path.join(d, "mutant.dll"))
            json.dump(meta, open(os.path.join(d, "meta.json"), "w"), sort_keys=True)
            built += 1
    finally:
        errors = []
        for p, s in saved.items():         # one failed restore must not stop the others
            try:
                _restore(p, s)
            except BaseException as e:
                errors.append(e)
        if errors:
            raise errors[0]
    print("%d mutants built into %s of %d%s" % (built, dest, len(cases), "; failed: " + " ".join(failed) if failed else ""))
    return 1 if failed else 0


def _compile(argv):
    """subprocess.run, but an interruption (SIGTERM and SIGHUP raise SystemExit) kills and reaps the compiler first."""
    p = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        out, err = p.communicate()
    except BaseException:
        p.kill()
        p.wait()
        raise
    return subprocess.CompletedProcess(argv, p.returncode, out, err)


def run_compile(cases, only, dest, compile_argv, baselines=()):
    """The object-file mode (Story 1.8 slice 4, HOST and the host runtime): every case's mutated unit is compiled with -c,
    from a copy with the same basename in a temporary directory, to DEST/<case>/mutant.o with meta.json; DEST/baseline/
    holds the unmutated units.  Nothing under the tree is written.  cases: (name, basename, source, old, new, meta);
    compile_argv(source_path, object_path) -> argv; baselines: (object name, argv).  Same exit codes as run()."""
    import tempfile
    picked = [c for c in cases if not only or c[0] in only]
    unknown = [n for n in only if n not in {c[0] for c in cases}]
    if not picked or unknown:
        print("no case matches %s" % " ".join(unknown or only))
        return 1
    bad = ["%s (anchor appears %d times)" % (c[0], c[2].count(c[3])) for c in picked if c[2].count(c[3]) != 1]
    if bad:
        print("anchors not unique: " + ", ".join(bad))
        return 1
    for sig, code in ((signal.SIGTERM, 143), (signal.SIGHUP, 129)):     # a kill still runs the temporary directory's cleanup
        signal.signal(sig, lambda *_, code=code: sys.exit(code))
    os.makedirs(os.path.join(dest, "baseline"), exist_ok=True)
    failed = []
    for obj, argv in baselines:
        target = os.path.join(dest, "baseline", obj)
        r = _compile(argv + ["-o", target])
        if r.returncode != 0 or not os.path.isfile(target):
            print("baseline %s BUILD FAILED\n%s" % (obj, r.stderr[-800:]))
            failed.append("baseline/" + obj)
    for name, base, source, old, new, meta in picked:
        d = os.path.join(dest, name)
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, base)
            open(path, "w").write(source.replace(old, new, 1))
            r = _compile(compile_argv(path, os.path.join(d, "mutant.o")))
        if r.returncode != 0 or not os.path.isfile(os.path.join(d, "mutant.o")):
            print("%-20s BUILD FAILED\n%s" % (name, r.stderr[-800:]))
            failed.append(name)
            continue
        json.dump(dict(meta, name=name), open(os.path.join(d, "meta.json"), "w"), sort_keys=True)
    print("%d mutants built into %s of %d%s" % (len(picked) - len([f for f in failed if not f.startswith("baseline/")]), dest, len(picked),
                                               "; failed: " + " ".join(failed) if failed else ""))
    return 1 if failed else 0


class Case:
    """Story 1.8 slice 6b0: one mutant of a break runner that builds PEs.  overlay() -> {file name: text or bytes}, the
    mutated files laid over a copy of the inputs (it raises on an anchor whose count is wrong); builds(work) -> the compile
    argvs, run in order from the tree's root; outputs: the PE names (relative to work) those argvs leave."""

    def __init__(self, id, overlay, builds, outputs):
        self.id, self.overlay, self.builds, self.outputs = id, overlay, builds, outputs


def write_case(work, inputs, overlay):
    """Lay the inputs, then the overlay over them, into `work` (names may hold directories).  Overlay text is read and
    written as universal-newline text by the runners; bytes are written as given."""
    import pathlib
    for name, data in {**(inputs or {}), **overlay}.items():
        path = pathlib.Path(work) / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data if isinstance(data, bytes) else data.encode())


ROOT = os.path.realpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
_SIGS = {signal.SIGTERM, signal.SIGHUP}


def _inside(path):
    return os.path.commonpath([os.path.realpath(path), ROOT]) == ROOT


def _compile_group(argv, scratch):
    """Run a compiler from the tree's root with TMPDIR=scratch, in its own process group.  SIGTERM and SIGHUP are blocked
    across the launch and the registration, and the handler only raises, so an interruption always reaches the cleanup
    below, which kills and reaps the whole group.  A compiler that cannot start is a failed build, not a traceback."""
    old = signal.pthread_sigmask(signal.SIG_BLOCK, _SIGS)
    p = None
    try:
        try:
            p = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, start_new_session=True,
                                 cwd=ROOT, env=dict(os.environ, TMPDIR=scratch))
        except OSError as e:
            return subprocess.CompletedProcess(argv, 127, "", "%s: %s" % (argv[0], e))
        try:
            signal.pthread_sigmask(signal.SIG_SETMASK, old)     # a signal that arrived meanwhile is delivered here
            old = None
            out, err = p.communicate()
        except BaseException:
            try:
                os.killpg(p.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            p.wait()
            raise
        return subprocess.CompletedProcess(argv, p.returncode, out, err)
    finally:
        if old is not None:
            signal.pthread_sigmask(signal.SIG_SETMASK, old)


def _is_pe(path):
    """The DOS and PE signatures of a built output."""
    import struct
    with open(path, "rb") as f:
        head = f.read(0x40)
        if len(head) < 0x40 or head[:2] != b"MZ":
            return False
        f.seek(struct.unpack_from("<I", head, 0x3c)[0])
        return f.read(4) == b"PE\0\0"


def validate(argv):
    """--build-only DIR [CASE...]: everything checkable before any input is loaded or any case recorded.  Returns
    (code, dest, names, base): code 2 for a bad DIR or TMPDIR (nothing was probed or made), else None.  base is the
    directory outside the tree where the compilers' scratch directories go; no gettempdir() probe is ever made."""
    os.environ.pop("MPD_PROVENANCE_DIR", None)
    if not argv or not argv[0]:
        print("--build-only needs DIR: a missing or empty directory outside the tree")
        return 2, None, None, None
    dest = os.path.abspath(argv[0])
    if os.path.isfile(dest) or (os.path.isdir(dest) and os.listdir(dest)) or _inside(dest):
        print("--build-only needs DIR: a missing or empty directory outside the tree")
        return 2, None, None, None
    base = "/tmp"
    for var in ("TMPDIR", "TEMP", "TMP"):
        if os.environ.get(var):
            base = os.path.abspath(os.environ[var])
            break
    if _inside(base) or not os.path.isdir(base):
        print("TMPDIR is inside the tree or is not a directory")
        return 2, None, None, None
    return None, dest, argv[1:], base


def build_only(argv, factory):
    """--build-only DIR [CASE...] for the runners built on Case.  factory() -> (cases, inputs) is called only after DIR and
    TMPDIR are validated.  Every selected case (all by default) is built in a scratch directory outside the tree and staged
    beside DIR/<id> (PEs and meta.json), renamed into place only when every write succeeded.  No wine runs and nothing is
    written under the tree.  Exit 0 all built; 1 a bad CASE or anchor (before any output) or a failed build; 2 a bad DIR."""
    import tempfile
    code, dest, names, base = validate(argv)
    if code:
        return code
    os.chdir(ROOT)                                  # the runners read src/ and tools/ relative to the tree
    try:
        cases, inputs = factory()
    except (Exception, SystemExit) as e:            # an anchor that does not match
        print("anchor: %s" % e)
        return 1
    ids = [c.id for c in cases]
    unknown = [n for n in names if n not in ids]
    dupes = sorted({n for n in names if names.count(n) > 1} | {i for i in ids if ids.count(i) > 1})
    if unknown or dupes:
        print("no case %s" % " ".join(unknown) if unknown else "duplicate case %s" % " ".join(dupes))
        return 1
    picked = [c for c in cases if not names or c.id in names]
    try:
        prepared = [(c, c.overlay()) for c in picked]
    except (Exception, SystemExit) as e:
        print("anchor: %s" % e)
        return 1
    for sig, code in ((signal.SIGTERM, 143), (signal.SIGHUP, 129)):
        signal.signal(sig, lambda *_, code=code: sys.exit(code))      # the handler only raises
    os.makedirs(dest, exist_ok=True)
    failed = []
    for c, overlay in prepared:
        stage = os.path.join(dest, ".%s.partial" % c.id)
        shutil.rmtree(stage, ignore_errors=True)
        try:
            with tempfile.TemporaryDirectory(prefix="break-", dir=base) as tmp:
                write_case(tmp, inputs, overlay)
                bad = None
                for cmd in c.builds(__import__("pathlib").Path(tmp)):
                    r = _compile_group(cmd, tmp)
                    if r.returncode:
                        bad = r.stderr[-800:]
                        break
                if bad is None:
                    for o in c.outputs:
                        if not os.path.isfile(os.path.join(tmp, o)) or not _is_pe(os.path.join(tmp, o)):
                            bad = "%s: not built, or not a PE" % o
                            break
                if bad is not None:
                    print("%-30s BUILD FAILED\n%s" % (c.id, bad))
                    failed.append(c.id)
                    continue
                os.makedirs(stage)
                for o in c.outputs:
                    shutil.copy(os.path.join(tmp, o), os.path.join(stage, os.path.basename(o)))
                json.dump({"name": c.id}, open(os.path.join(stage, "meta.json"), "w"), sort_keys=True)
            os.rename(stage, os.path.join(dest, c.id))
        finally:
            shutil.rmtree(stage, ignore_errors=True)
    print("%d cases built into %s of %d%s" % (len(prepared) - len(failed), dest, len(prepared), "; failed: " + " ".join(failed) if failed else ""))
    return 1 if failed else 0
