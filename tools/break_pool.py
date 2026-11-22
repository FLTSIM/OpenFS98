#!/usr/bin/env python3
"""A worker pool for the long break runners (host, g3d, options, sim1r): BREAK_JOBS=N runs their breaks on N workers.

A worker is a copy of the runner's tree under <tree>/bin/break-pool/<runner>-<pid>/<k> with its own build directory
and a private copy of the runner's wine prefix (tools/slots.py). The runner calls go(): with BREAK_JOBS unset or 1 the
breaks run serially in the tree as before; otherwise on min(N, breaks) workers. Either way each break is
one(entry, worker) -> (result, text); the main thread prints the texts in the serial order, and the results come back
in that order, so a runner's summary is unchanged.

  BREAK_JOBS=N          workers (default 1: serial, in the tree)
  BREAK_POOL_KEEP=1     keep the workers' files (their processes are still stopped)
  BREAK_POOL_RECORD=F   write one JSON line per break to F (serial or pooled): index, name, unit, build_exit, exit,
                        summary, differ -- what a runner puts in worker.rec

A break's own artifacts (runner's `artifacts(entry)`: paths relative to the tree) are compared by mtime and size
around the break: a written file is snapshotted, a deleted one is noted, an untouched one is left alone; the changes
reach the runner's tree when the break's text is printed, in serial order. Files a break writes under a fixed name
(g3d's g3d_retail.txt / g3d_ours.txt) are not copied back.

    python3 tools/break_pool.py --self-test
"""
import contextlib, io, json, os, shutil, signal, subprocess, sys, tempfile, threading, time
from collections import deque

import slots
from slots import under, remove, owned_procs, cleanup, copy_prefix

SKIP_TOP = ('bin', 'build', 'game', 'cd', '.git', 'wineprefix')


class SetupFailed(Exception):
    pass


class Worker:
    pooled = True

    def __init__(self, k, root, base):
        self.k, self.root, self.base = k, root, base
        self.log = os.path.join(base, '%d.log' % k)
        self.prefix = os.path.join(root, 'wineprefix')
        self.env = dict(os.environ, WINEPREFIX=self.prefix, OFS_BUILD=os.path.join(root, 'build/win32-retail'),
                        FS98_DIR=os.path.join(self.prefix, 'drive_c', 'FS98'))
        self.rec = None

    def run(self, argv, **kw):    # a setup command, its output in the worker log
        with open(self.log, 'a') as f:
            f.write('$ %s\n' % ' '.join(map(str, argv)))
            f.flush()
            r = subprocess.run(argv, stdout=f, stderr=subprocess.STDOUT, **kw)
        if r.returncode:
            raise SetupFailed('%s failed (%d); see %s' % (' '.join(map(str, argv[:3])), r.returncode, self.log))

    def kill_hang(self):    # a hung break: the worker's own wine, never pkill
        assert under(os.path.realpath(self.prefix), os.path.realpath(self.root))
        try:
            subprocess.run(['wineserver', '-k'], env=self.env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=30)
        except subprocess.TimeoutExpired:
            pass
        cleanup(self.root)


class Serial:    # the runner's own tree, as before
    pooled = False
    root = '.'
    rec = None

    def __init__(self):
        self.env = dict(os.environ)


def first_differ(text):
    """The first line of a harness's output that reports a DIFFER, or None."""
    return next((l for l in text.splitlines() if 'DIFFER' in l), None)


def _alive(pid):
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False
    except OSError:
        return True


def _reap_stale(pool_dir):
    """Clean up and remove run directories whose owner marker names a dead PID; a live run's is never touched."""
    for d in sorted(os.listdir(pool_dir)):
        p = os.path.join(pool_dir, d)
        try:
            pid = int(open(os.path.join(p, 'owner')).read().strip())
        except (OSError, ValueError):
            continue
        if not _alive(pid):
            cleanup(os.path.realpath(p))
            remove(p, os.path.realpath(pool_dir))


def workers(runner, n, tree, pool_bin=(), pool_link=(), skip=(), prereq=(), prepare=None, prefix=None, configure=True):
    """Create n workers (in parallel). Raises SetupFailed naming the log or file; the workers made so far are in
    workers.made for the teardown."""
    tree = os.path.realpath(tree)
    pool_dir = os.path.join(tree, 'bin', 'break-pool')
    base = os.path.join(pool_dir, '%s-%d' % (runner, os.getpid()))
    ws = []
    workers.made = ws
    try:
        os.makedirs(pool_dir, exist_ok=True)
        _reap_stale(pool_dir)
        os.mkdir(base)
        open(os.path.join(base, 'owner'), 'w').write('%d\n' % os.getpid())
    except OSError as e:
        raise SetupFailed('cannot create %s: %s' % (base, e))
    prefix = prefix or os.environ.get('WINEPREFIX') or '~/.wine'
    for k in range(n):
        ws.append(Worker(k, os.path.join(base, str(k)), base))
    errors = []

    def setup(w):
        try:
            os.mkdir(w.root)
            for e in os.listdir(tree):
                if e not in SKIP_TOP:
                    w.run(['cp', '-a', os.path.join(tree, e), os.path.join(w.root, e)])
            if os.path.lexists(os.path.join(tree, 'cd')):
                os.symlink(os.path.realpath(os.path.join(tree, 'cd')), os.path.join(w.root, 'cd'))
            os.makedirs(os.path.join(w.root, 'bin'))
            for p in pool_bin:
                if not os.path.lexists(os.path.join(tree, 'bin', p)):
                    raise SetupFailed('missing %s in %s' % (os.path.join('bin', p), tree))
                os.makedirs(os.path.dirname(os.path.join(w.root, 'bin', p)), exist_ok=True)
                ex = ['--exclude=/' + s[len(p) + 1:] for s in skip if s.startswith(p + '/')]
                src = os.path.join(tree, 'bin', p)
                w.run(['rsync', '-a'] + ex + [src + ('/' if os.path.isdir(src) else ''), os.path.join(w.root, 'bin', p)])
            for p in pool_link:
                os.makedirs(os.path.dirname(os.path.join(w.root, 'bin', p)), exist_ok=True)
                os.symlink(os.path.realpath(os.path.join(tree, 'bin', p)), os.path.join(w.root, 'bin', p))
            if configure:
                w.run(['cmake', '-S', w.root, '-B', w.env['OFS_BUILD'], '-DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/win32-retail.cmake'],
                      env=w.env, cwd=w.root)
            copy_prefix(prefix, w.root, None, tree, w.log)
            if prepare:
                prepare(w)
            for p in prereq:
                if not os.path.exists(os.path.join(w.root, p)):
                    raise SetupFailed('worker %d lacks %s (%s)' % (w.k, p, os.path.join(w.root, p)))
        except SetupFailed as e:
            errors.append(e)
        except Exception as e:    # cp, symlink, copy_prefix and the like
            errors.append(SetupFailed('worker %d setup failed: %s (see %s)' % (w.k, e, w.log)))

    ts = [threading.Thread(target=setup, args=(w,)) for w in ws]
    for t in ts:
        t.start()
    for t in ts:
        t.join()
    if errors:
        raise errors[0]
    return ws


def kill(ws, within=20):
    """SIGKILL every process the workers own (a signal's teardown); scans until none is left or `within` s pass."""
    end = time.time() + within
    while time.time() < end:
        left = 0
        for w in ws:
            for pid in owned_procs(os.path.realpath(w.root)):
                left += 1
                try:
                    os.kill(pid, signal.SIGKILL)
                except OSError:
                    pass
        if not left:
            return
        time.sleep(0.5)


def teardown(ws, failed=False):
    """Stop the workers' processes, delete the workers unless BREAK_POOL_KEEP=1 (a failed setup keeps its logs).
    -> the pids that survived."""
    survivors = []
    for w in ws:
        if os.path.isdir(w.root) and not cleanup(w.root):
            survivors += list(owned_procs(os.path.realpath(w.root)))
    if survivors or not ws:
        return survivors
    base = ws[0].base
    if os.environ.get('BREAK_POOL_KEEP') != '1':
        for w in ws:
            if os.path.lexists(w.root):
                remove(w.root, os.path.realpath(base))
        if not failed:
            remove(base, os.path.realpath(os.path.dirname(base)))
        else:
            os.unlink(os.path.join(base, 'owner'))    # nothing of it is live any more
    return survivors


def _stamp(path):
    try:
        st = os.stat(path)
        return (st.st_mtime_ns, st.st_size)
    except OSError:
        return None


def run(entries, one, ws, tree, artifacts=None):
    """-> [result of one(entry, worker)] in entry order, printing each text as soon as everything before it is
    printed. A signal or an exception stops the queue; the workers' processes are SIGKILLed and it re-raises."""
    n = len(entries)
    cond = threading.Condition()
    todo, done = deque(range(n)), {}
    state = {'stop': False, 'sig': None, 'err': None}
    snap_dir = os.path.join(ws[0].base, 'snap')
    os.makedirs(snap_dir, exist_ok=True)

    def work(w):
        while True:
            with cond:
                if state['stop'] or not todo:
                    return
                i = todo.popleft()
            try:
                files = artifacts(entries[i]) if artifacts else []
                before = {p: _stamp(os.path.join(w.root, p)) for p in files}
                w.rec = None
                result, text = one(entries[i], w)
                changes = []
                for p in files:
                    after = _stamp(os.path.join(w.root, p))
                    if after == before[p]:
                        continue
                    if after is None:
                        changes.append((p, None))
                    else:
                        keep = os.path.join(snap_dir, '%d-%d' % (i, len(changes)))
                        shutil.copy2(os.path.join(w.root, p), keep)
                        changes.append((p, keep))
                with cond:
                    done[i] = (result, text, w.rec, changes)
                    cond.notify_all()
            except BaseException as e:
                with cond:
                    if not state['stop']:
                        state['err'] = e
                    state['stop'] = True
                    cond.notify_all()
                return

    def on_signal(sig, frame):
        state['sig'] = sig
        state['stop'] = True

    old = {s: signal.signal(s, on_signal) for s in (signal.SIGINT, signal.SIGTERM)}
    ts = [threading.Thread(target=work, args=(w,), daemon=True) for w in ws]
    results, recs, nxt = [], [], 0
    try:
        for t in ts:
            t.start()
        with cond:
            while nxt < n and not state['stop']:
                while nxt in done:
                    result, text, rec, changes = done.pop(nxt)
                    for p, keep in changes:    # publish in serial order
                        dst = os.path.join(tree, p)
                        if keep is None:
                            if os.path.lexists(dst):
                                os.unlink(dst)
                        else:
                            os.makedirs(os.path.dirname(dst), exist_ok=True)
                            shutil.copy2(keep, dst)
                            os.unlink(keep)
                    print(text, end='', flush=True)
                    results.append(result)
                    recs.append(dict(rec or {}, index=nxt))
                    nxt += 1
                if nxt < n:
                    cond.wait(0.5)
        if state['stop']:
            kill(ws, 25)
        for t in ts:
            t.join(30)
    finally:
        for s, h in old.items():
            signal.signal(s, h)
        state['stop'] = True
        run.recs = recs
    if state['sig']:
        raise SystemExit(128 + state['sig'])
    if state['err']:
        raise state['err']
    return results


def go(runner, entries, one, artifacts=None, tree=None, pool_bin=(), pool_link=(), skip=(), prereq=(), prepare=None,
       prefix=None, configure=True):
    """The runner's loop: serial in the tree (BREAK_JOBS unset or 1) or on a pool. -> results in entry order."""
    tree = os.path.realpath(tree or os.getcwd())
    jobs = min(int(os.environ.get('BREAK_JOBS') or 1), len(entries))
    rec_path = os.environ.get('BREAK_POOL_RECORD')
    recs = []
    try:
        if jobs <= 1:
            results, w = [], Serial()
            for i, entry in enumerate(entries):
                w.rec = None
                result, text = one(entry, w)
                print(text, end='', flush=True)
                results.append(result)
                recs.append(dict(w.rec or {}, index=i))
            return results
        ws = []
        try:
            ws = workers(runner, jobs, tree, pool_bin, pool_link, skip, prereq, prepare, prefix, configure)
            results = run(entries, one, ws, tree, artifacts)
            recs = run.recs
        except SetupFailed as e:
            print('break_pool: %s' % e, file=sys.stderr)
            teardown(getattr(workers, 'made', []), failed=True)
            sys.exit(2)
        finally:
            workers.made = []
            survivors = teardown(ws) if ws else []
            if survivors:
                print('break_pool: processes survived the teardown: %s' % ' '.join(map(str, survivors)), file=sys.stderr)
                os._exit(2)
        return results
    finally:
        if rec_path:
            with open(rec_path, 'w') as f:
                json.dump(recs, f, indent=1)


# ---- self-test: a fake `one`, no wine ---------------------------------------------------------------------------
def self_test():
    t = os.path.realpath(tempfile.mkdtemp())
    os.environ.pop('BREAK_POOL_KEEP', None)
    os.environ.pop('BREAK_POOL_RECORD', None)
    try:
        tree = os.path.join(t, 'tree')
        os.makedirs(os.path.join(tree, 'src'))
        os.makedirs(os.path.join(tree, 'bin', 'diff'))
        os.makedirs(os.path.join(tree, 'bin', 'ro'))
        open(os.path.join(tree, 'src', 'a.c'), 'w').write('a')
        open(os.path.join(tree, 'bin', 'diff', 'x'), 'w').write('x')
        open(os.path.join(tree, 'bin', 'ro', 'snap'), 'w').write('s')
        pfx = os.path.join(t, 'pfx')
        os.makedirs(os.path.join(pfx, 'drive_c'))
        kw = dict(tree=tree, prefix=pfx, configure=False, pool_bin=['diff'], pool_link=['ro/snap'], prereq=['bin/diff/x'])

        def capture(f):
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                r = f()
            return r, buf.getvalue()

        # 9 entries on 3 workers: results and text in order, each on a worker of its own tree
        roots = set()

        def one(entry, w):
            roots.add(w.root)
            assert os.getcwd() != w.root and os.path.exists(os.path.join(w.root, 'src', 'a.c'))
            assert os.path.islink(os.path.join(w.root, 'bin', 'ro', 'snap')) and not os.path.exists(os.path.join(w.root, 'game'))
            time.sleep((9 - entry) * 0.02)
            w.rec = {'name': 'e%d' % entry}
            return entry * 10, 'line %d\n' % entry
        os.environ['BREAK_JOBS'] = '3'
        os.environ['BREAK_POOL_RECORD'] = os.path.join(t, 'rec.json')
        r, out = capture(lambda: go('fake', list(range(9)), one, **kw))
        assert r == [i * 10 for i in range(9)] and out == ''.join('line %d\n' % i for i in range(9)), (r, out)
        assert len(roots) == 3, roots
        assert [x['index'] for x in json.load(open(os.environ['BREAK_POOL_RECORD']))] == list(range(9))
        assert not os.path.exists(os.path.join(tree, 'bin', 'break-pool', 'fake-%d' % os.getpid())), 'workers not deleted'
        del os.environ['BREAK_POOL_RECORD']
        # more jobs than entries: as many workers as entries
        roots.clear()
        os.environ['BREAK_JOBS'] = '8'
        r, out = capture(lambda: go('fake', [1, 2, 3], one, **kw))
        assert r == [10, 20, 30] and len(roots) <= 3
        # serial: the tree itself
        os.environ['BREAK_JOBS'] = '1'
        seen = []
        capture(lambda: go('fake', [1], lambda e, w: (seen.append((w.pooled, w.root)) or (0, ''))))
        assert seen == [(False, '.')]
        # an exception in one: reported, torn down
        os.environ['BREAK_JOBS'] = '3'

        def boom(entry, w):
            if entry == 4:
                raise RuntimeError('boom')
            time.sleep(0.1)
            return entry, 'x\n'
        try:
            capture(lambda: go('fake', list(range(9)), boom, **kw))
        except RuntimeError as e:
            assert str(e) == 'boom'
        else:
            raise SystemExit('exception swallowed')
        assert not os.path.exists(os.path.join(tree, 'bin', 'break-pool', 'fake-%d' % os.getpid()))
        # published artifacts in serial order: two writers of one file (the slower first), a deletion, an untouched file
        open(os.path.join(tree, 'src', 'gone.c'), 'w').write('g')
        open(os.path.join(tree, 'src', 'keep.c'), 'w').write('k')

        def art(entry):
            return ['src/f.c', 'src/gone.c', 'src/keep.c']

        def one2(entry, w):
            if entry == 0:
                time.sleep(0.5)
                open(os.path.join(w.root, 'src', 'f.c'), 'w').write('first')
            elif entry == 1:
                open(os.path.join(w.root, 'src', 'f.c'), 'w').write('second')
                os.unlink(os.path.join(w.root, 'src', 'gone.c'))
            return entry, 'e%d\n' % entry
        open(os.path.join(tree, 'src', 'keep.c'), 'w').write('runner')    # the runner's current file, not the worker's copy
        os.environ['BREAK_JOBS'] = '2'
        r, out = capture(lambda: go('fake', [0, 1, 2], one2, artifacts=art, **kw))
        assert out == 'e0\ne1\ne2\n'
        assert open(os.path.join(tree, 'src', 'f.c')).read() == 'second'
        assert not os.path.exists(os.path.join(tree, 'src', 'gone.c'))
        assert open(os.path.join(tree, 'src', 'keep.c')).read() == 'runner'
        # a missing prerequisite and an unwritable bin/break-pool: exit 2, no break run
        ran = []
        for bad in ('prereq', 'perm'):
            if bad == 'perm':
                if os.geteuid() == 0:
                    continue
                os.chmod(os.path.join(tree, 'bin', 'break-pool'), 0o555)
            k2 = dict(kw, prereq=['bin/nope'] if bad == 'prereq' else kw['prereq'])
            try:
                with contextlib.redirect_stderr(io.StringIO()) as err:
                    capture(lambda: go('fake', [1, 2], lambda e, w: ran.append(e) or (e, ''), **k2))
            except SystemExit as e:
                assert e.code == 2, e.code
                assert ('bin/nope' if bad == 'prereq' else 'break-pool') in err.getvalue(), err.getvalue()
            else:
                raise SystemExit('setup failure did not exit 2 (%s)' % bad)
        assert not ran
    finally:
        subprocess.run(['chmod', '-R', 'u+rwX', t])
        shutil.rmtree(t)
    print('break_pool self-test: ok')


if __name__ == '__main__':
    if sys.argv[1:] == ['--self-test']:
        self_test()
    else:
        sys.exit(__doc__)
