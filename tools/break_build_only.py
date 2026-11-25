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
