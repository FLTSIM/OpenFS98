#!/usr/bin/env python3
"""Path, process and wine-prefix helpers shared by cmake_verify.py (slots) and break_pool.py (workers).

    python3 tools/slots.py --self-test
"""
import os, shutil, signal, subprocess, sys, tempfile, time


def under(path, base):
    return path == base or path.startswith(base.rstrip('/') + '/')


def remove(path, base, whole=False):
    """The guarded deletion: only paths under base (base itself only with whole=True and path == base)."""
    real = os.path.realpath(path) if not os.path.islink(path) else os.path.join(os.path.realpath(os.path.dirname(path)), os.path.basename(path))
    assert under(real, base) and (real != base or whole and path == base), real
    if os.path.isdir(real) and not os.path.islink(real):
        subprocess.run(['chmod', '-R', 'u+rwX', real], check=False)
        shutil.rmtree(real)
    elif os.path.lexists(real):
        os.unlink(real)


def owned_procs(slot):
    """{pid: WINEPREFIX or None} of every process whose cwd, executable or WINEPREFIX lies under the slot."""
    out = {}
    for d in os.listdir('/proc'):
        if not d.isdigit() or int(d) == os.getpid():
            continue
        try:
            wp = None
            for kv in open('/proc/%s/environ' % d, 'rb').read().split(b'\0'):
                if kv.startswith(b'WINEPREFIX='):
                    wp = kv[11:].decode(errors='replace')
        except OSError:
            wp = None
        try:
            paths = [os.readlink('/proc/%s/cwd' % d), os.readlink('/proc/%s/exe' % d).replace(' (deleted)', '')]
        except OSError:
            paths = []    # gone, a zombie, or not ours
        if any(under(x, slot) for x in paths) or (wp and under(os.path.realpath(wp), slot)):
            out[int(d)] = wp
    return out


def cleanup(slot):
    """Stop every process the slot owns; False if one survives 120 s overall. Never touches ~/.wine or other slots."""
    end = time.time() + 120
    slot = os.path.realpath(slot)
    while owned_procs(slot):
        if time.time() > end:
            return False
        for wp in {w for w in owned_procs(slot).values() if w}:
            r = os.path.realpath(wp)
            if not under(r, slot):
                print('cleanup: not running wineserver -k in %s (%s is not under the slot)' % (wp, r), file=sys.stderr)
            elif os.path.isdir(r):
                try:
                    subprocess.run(['wineserver', '-k'], env=dict(os.environ, WINEPREFIX=r), stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL, timeout=max(1, min(30, end - time.time())))
                except subprocess.TimeoutExpired:
                    pass
        for sig, wait in ((signal.SIGTERM, 30), (signal.SIGKILL, 10)):
            for pid in owned_procs(slot):
                try:
                    os.kill(pid, sig)
                except OSError:
                    pass
            t = min(time.time() + wait, end)
            while owned_procs(slot) and time.time() < t:
                time.sleep(0.5)
            if not owned_procs(slot):
                break
    return True


def copy_prefix(src, slot, game, forbid, log=None):
    """Copy the wine prefix src to <slot>/wineprefix (which must not exist) and make it self-contained: dosdevices/c:
    -> ../drive_c, other links into src moved to the copy, system links (z: -> /, devices) kept. game: the directory
    drive_c/FS98 points at, or None (it is removed: the worker never runs the game). Raises RuntimeError if a link
    still resolves into src, or into the tree forbid outside the slot."""
    wine = os.path.realpath(os.path.expanduser(src))
    slot = os.path.realpath(slot)
    pfx = os.path.join(slot, 'wineprefix')
    if os.path.lexists(pfx):
        raise RuntimeError('%s exists already' % pfx)
    with open(log or os.devnull, 'a') as f:
        r = subprocess.run(['cp', '-a', wine, pfx], stdout=f, stderr=subprocess.STDOUT)
    if r.returncode:
        raise RuntimeError('cp -a %s %s failed (%d)' % (wine, pfx, r.returncode))
    for dp, dn, fn in os.walk(pfx):
        for name in dn + fn:
            q = os.path.join(dp, name)
            if not os.path.islink(q):
                continue
            tgt = os.readlink(q)
            if os.path.relpath(q, pfx) == 'dosdevices/c:':
                new = '../drive_c'
            elif os.path.isabs(tgt) and under(tgt, wine):
                new = os.path.join(pfx, os.path.relpath(tgt, wine))
            else:
                continue
            os.unlink(q)
            os.symlink(new, q)
    fs98 = os.path.join(pfx, 'drive_c', 'FS98')    # OFSTEST needs C: mapped to the slot's game, whatever it was
    if os.path.islink(fs98) or os.path.isfile(fs98):
        os.unlink(fs98)
    elif os.path.isdir(fs98):
        remove(fs98, pfx)
    if game:
        os.makedirs(os.path.dirname(fs98), exist_ok=True)
        os.symlink(game, fs98)
        assert os.path.realpath(fs98) == os.path.realpath(game), fs98
    for dp, dn, fn in os.walk(pfx):
        for name in dn + fn:
            q = os.path.join(dp, name)
            if os.path.islink(q):
                r = os.path.realpath(q)
                if under(r, wine) or under(r, forbid) and not under(r, slot):
                    raise RuntimeError('prefix symlink %s still resolves into %s' % (q, r))


def self_test():
    assert under('/a/b', '/a') and under('/a', '/a') and not under('/ab', '/a') and under('/a/b', '/a/')
    t = os.path.realpath(tempfile.mkdtemp())
    try:
        base = os.path.join(t, 'base')
        os.makedirs(os.path.join(base, 'x'))
        os.makedirs(os.path.join(t, 'other'))
        remove(os.path.join(base, 'x'), base)
        assert not os.path.exists(os.path.join(base, 'x'))
        for bad in (os.path.join(t, 'other'), base, os.path.join(base, '..', 'other')):
            try:
                remove(bad, base)
            except AssertionError:
                continue
            raise SystemExit('remove accepted %s' % bad)
        os.symlink(os.path.join(t, 'other'), os.path.join(base, 'lnk'))
        remove(os.path.join(base, 'lnk'), base)    # unlinks the link, not the target
        assert os.path.isdir(os.path.join(t, 'other')) and not os.path.lexists(os.path.join(base, 'lnk'))
        remove(base, base, whole=True)
        assert not os.path.exists(base)
        # a tiny fake prefix inside a runner tree
        tree = os.path.join(t, 'tree')
        src = os.path.join(tree, 'wineprefix')
        os.makedirs(os.path.join(src, 'dosdevices'))
        os.makedirs(os.path.join(src, 'drive_c', 'FS98'))
        os.symlink(os.path.join(src, 'drive_c'), os.path.join(src, 'dosdevices', 'c:'))     # absolute, into src
        os.symlink(os.path.join(src, 'drive_c'), os.path.join(src, 'abs'))
        os.symlink('/', os.path.join(src, 'dosdevices', 'z:'))
        slot = os.path.join(tree, 'bin', 'w', '0')
        os.makedirs(slot)
        copy_prefix(src, slot, None, tree)
        p = os.path.join(slot, 'wineprefix')
        assert os.readlink(os.path.join(p, 'dosdevices', 'c:')) == '../drive_c'
        assert os.readlink(os.path.join(p, 'abs')) == os.path.join(p, 'drive_c')
        assert os.readlink(os.path.join(p, 'dosdevices', 'z:')) == '/'
        assert not os.path.lexists(os.path.join(p, 'drive_c', 'FS98'))
        for dp, dn, fn in os.walk(p):
            for n in dn + fn:
                q = os.path.join(dp, n)
                if os.path.islink(q) and os.readlink(q) != '/':
                    assert under(os.path.realpath(q), p), q
        try:
            copy_prefix(src, slot, None, tree)
        except RuntimeError:
            pass
        else:
            raise SystemExit('copy_prefix overwrote an existing prefix')
        os.symlink(os.path.join(tree, 'src'), os.path.join(src, 'escape'))     # into the runner tree, outside the slot
        os.makedirs(os.path.join(tree, 'src'))
        os.makedirs(os.path.join(tree, 'bin', 'w', '1'))
        try:
            copy_prefix(src, os.path.join(tree, 'bin', 'w', '1'), None, tree)
        except RuntimeError as e:
            assert 'still resolves' in str(e), e
        else:
            raise SystemExit('copy_prefix accepted a link into the forbidden tree')
    finally:
        subprocess.run(['chmod', '-R', 'u+rwX', t])
        shutil.rmtree(t)
    print('slots self-test: ok')


if __name__ == '__main__':
    if sys.argv[1:] == ['--self-test']:
        self_test()
    else:
        sys.exit(__doc__)
