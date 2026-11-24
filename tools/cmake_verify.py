#!/usr/bin/env python3
"""Run every Baselines command of docs/tools.md through CMake and record how it compares (Story 1.2, piece 2).

usage: cmake_verify.py <build-dir> <results-dir> [-j N] [--in-place] [--baselines FILE] [--clean-slots]
                       [--bin-skip-mb N] [--only SUBSTRING] [--lines N,N,...] [--factor F] [--list]

Run it from a frozen copy of the tree (break suites rewrite src/). `make [-f X.mk] T ARGS` becomes
`ARGS cmake --build <build> --target T`; a script line (tools/x.sh, python3 tools/x.py) runs as written. Each line
has a timeout of three times its documented time (30 min at least, 2 h when none is documented); at the timeout the
line's process group gets SIGTERM and, 60 s later, SIGKILL (the record keeps exit "timeout"). A line already
recorded in <results-dir> is skipped, so an interrupted run resumes; --lines runs the given line numbers again
(their earlier records move to <results-dir>/earlier/), and --factor F scales every timeout. --baselines FILE reads
the lines from FILE instead of docs/tools.md. <results-dir>/meta.json holds the sha256 of the parsed list; a results
directory with another one (or none, when it holds records) is refused unless --lines names what to re-run.

Slots (the default): every line runs in a disposable copy of the start tree, <results-dir>/slots/K (the results
directory must lie outside the start tree). -j N (default 1) runs up to N lines at once, one per slot. A slot is
prepared once (sources, game, a copy of ~/.wine made self-contained, a fresh build of `openfs98 difftests`, the
fixtures no build makes), copied aside as K.template, and rsync-reset from the template after every line, so no
mutant source, rebuilt module, installed module or game write reaches a later line. Lines of one CONFLICT group
(break runners that pkill their harness machine-wide) never overlap; SOLO lines (X display, game directory writes,
FLTSIM98.EXE, and the load-sensitive lines of docs/tools.md's "Load limits") run with no other line running. main-smoke, smoke, ofstest-run and util-trace are preceded by
ofs_install into the slot's game. After each line the processes the slot owns are stopped, and a change in the
slot's cmake, docs, include, src or tools directories (against the template) fails the line (`tree_changed`).
--in-place keeps the old behaviour: the start tree, serial, no slots. --clean-slots deletes <results-dir>/slots at
the end. Disk use, measured in the matrix runs of Story 1.4: a slot and its template each hold a fresh bin, a game
(about 0.3 GB) and a wine prefix (2.8 GB); bin/ subdirectories over N MB of the start tree (run output) are not
copied (--bin-skip-mb N, default 256, 0 copies everything; a live bin is about 106 GB; the fixtures listed in
REQUIRED_FIXTURES always are).

Verdict: PASS when the command exits 0 and every number of 100 or more in the documented expectation appears in
the output; UNCHECKED when it exits 0 and the expectation has no such number (exit 0 alone proves nothing: read
the log); CHECK when it exits 0 but one does not (read the log); FAIL on a non-zero exit or a timeout. A break
suite (`*breaks*`) exits 1 whenever a break is missed, so for one whose expectation gives its counts ("N breaks,
M caught[, K proven equivalent]") the verdict is PASS when it did not time out and its last such summary line
has exactly those counts (and every 100+ number is found), FAIL otherwise.
<results-dir>/report.md lists every line with its verdict, slot and tree_changed, and the last lines of its log.
"""
import concurrent.futures, hashlib, json, os, re, shlex, shutil, signal, subprocess, sys, threading, time

import slots    # tools/slots.py: shared with break_pool.py
from slots import under, owned_procs, cleanup, copy_prefix

root = os.path.realpath(os.getcwd())
build, results = os.path.abspath(sys.argv[1]), os.path.realpath(sys.argv[2])
only = sys.argv[sys.argv.index('--only') + 1] if '--only' in sys.argv else None
pick = {int(x) for x in sys.argv[sys.argv.index('--lines') + 1].split(',')} if '--lines' in sys.argv else None
factor = float(sys.argv[sys.argv.index('--factor') + 1]) if '--factor' in sys.argv else 1
def opt(name, default):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else default

jobs = int(opt('-j', 1))
in_place = '--in-place' in sys.argv
slots_dir = os.path.join(results, 'slots')
bin_skip_mb = int(opt('--bin-skip-mb', 256))
text = open(opt('--baselines', 'docs/tools.md')).read()
block = re.search(r'## Baselines.*?```sh\n(.*?)\n```', text, re.S).group(1)
lines = []
for raw in block.splitlines():
    if not raw.strip() or raw.lstrip().startswith('#'):
        continue
    cmd, _, note = raw.partition(' #')
    lines.append((cmd.strip(), note.strip()))


def minutes(note):
    m = re.search(r'\(~?([\d.]+)\s*(s|min|h)\b', note) or re.search(r'~([\d.]+)\s*(s|min|h)\b', note)
    if not m:
        return None
    v = float(m.group(1))
    return v / 60 if m.group(2) == 's' else v if m.group(2) == 'min' else v * 60


def plan(cmd, build=build):
    """-> (argv or shell string, env additions, via_cmake)"""
    words = shlex.split(cmd)
    env = {}
    if words[0] in ('make', 'tools/ofs'):
        i = 1
        if words[0] == 'make' and words[i] == '-f':
            i += 2
        target, args = words[i], words[i + 1:]
        for a in args:
            k, _, v = a.partition('=')
            env[k] = v
        return ['cmake', '--build', build, '--target', target], env, True
    return cmd, env, False


def need(note):
    expect = note.split('->', 1)[1] if '->' in note else note
    expect = re.sub(r'\(~[^)]*\)', '', expect)    # a documented time, "(~15 min)", is not a count
    expect = re.sub(r'\bN=\d+|\b\d{4}-\d\d-\d\d\b', '', expect)    # nor is a scenario count it was or would be run at ("N=20000") or a date
    return [n.replace(',', '') for n in re.findall(r'\d{1,3}(?:,\d{3})+|\d{3,}', expect)]


def break_counts(text, last=False):
    """(breaks, caught, equivalent or None) of the first (or last) break summary, in the runners' forms:
    "N[ breaks|mutants|sampled], M caught[, K [proven ]equivalent]", "M/N caught", "M of N caught", "N, all caught",
    and "M caught, X missed" (no total: N is None and X is returned as the fourth item)."""
    text = text.replace(',', ', ').replace(',  ', ', ')
    forms = [(r'(\d+)(?: (?:breaks|mutants|sampled))?, (\d+) caught(?:, (\d+) (?:proven )?equivalent)?', lambda m: (int(m[1]), int(m[2]), None if m[3] is None else int(m[3]))),
             (r'(\d+)/(\d+) caught', lambda m: (int(m[2]), int(m[1]), None)),
             (r'(\d+) of (\d+) caught', lambda m: (int(m[2]), int(m[1]), None)),
             (r'(\d+)(?: breaks)?, all caught', lambda m: (int(m[1]), int(m[1]), None)),
             (r'(?<![\d,] )(?<!\d)(\d+) caught, (\d+) missed', lambda m: (None, int(m[1]), None, int(m[2])))]
    found = [(m.start(), f(m)) for rx, f in forms for m in re.finditer(rx, text)]
    if not found:
        return None
    found.sort(key=lambda x: x[0])
    return (found[-1] if last else found[0])[1]


def break_verdict(cmd, note, code, out):
    """PASS/FAIL for a break suite whose expectation states its counts; None when this rule does not apply."""
    want = break_counts(note.split('->', 1)[-1]) if 'breaks' in cmd else None
    if want is None or code == 'timeout':
        return None
    got = break_counts(out, last=True)
    if got is not None and got[0] is None:    # "M caught, X missed": the rest must be the documented equivalents
        ok = got[1] == want[1] and got[3] == want[0] - want[1] - (want[2] or 0)
    else:
        ok = got is not None and got[:2] == want[:2] and (want[2] is None or got[2] is None or got[2] == want[2])
    return 'PASS' if ok else 'FAIL'


assert break_counts('its breaks -> 69, 63 caught, 6 proven equivalent (~40 min)') == (69, 63, 6)
assert break_counts('its breaks -> 12, all caught (~5 min)') == (12, 12, None)
assert break_counts('x\nmenu: 6 breaks, 5 caught, 0 proven equivalent (); missed: slot', last=True) == (6, 5, 0)
assert break_verdict('tools/ofs main-breaks SUITE=menu', 'its breaks -> 6, 5 caught; slot crashes (~2 min)', 1,
                     'menu: 6 breaks, 5 caught, 0 proven equivalent (); missed: slot') == 'PASS'
assert break_verdict('tools/ofs main-breaks SUITE=menu', 'its breaks -> 6, 5 caught; slot crashes', 1,
                     'menu: 6 breaks, 4 caught, 0 proven equivalent ()') == 'FAIL'
assert break_verdict('tools/ofs x-breaks', '-> 6 breaks, 5 caught', 'timeout', '6 breaks, 5 caught') is None


def found(number, out):
    return re.search(r'(?<![\d.])%s(?![\d])' % number, out) is not None    # 100 is not in 1000

assert break_counts('Options breaks: 859/859 caught', last=True) == (859, 859, None)
assert break_counts('SIMSUI breaks: 698/708 caught', last=True) == (708, 698, None)
assert break_counts('66 of 67 caught', last=True) == (67, 66, None)
assert break_counts('host: 505 breaks, 467 caught, 15 equivalent (a b)', last=True) == (505, 467, 15)
assert break_counts('200 mutants, 164 caught, 34 missed, 2 did not build', last=True) == (200, 164, None)
assert break_verdict('tools/ofs controls-breaks', '-> 81 breaks, 78 caught, 3 proven equivalent (~20 min)', 0, 'x caught\n78 caught, 0 missed') == 'PASS'
assert break_verdict('tools/ofs controls-breaks', '-> 81 breaks, 78 caught, 3 proven equivalent', 0, '77 caught, 1 missed') == 'FAIL'
assert need('its breaks -> 569, 557 caught (2 more at N=20000), 10 proven equivalent') == ['569', '557']
assert need('-> 67, 66 caught (re-run 2026-11-19)') == []
assert minutes('-> 317 breaks (19 missed at N=4000; ~4 h)') == 240



# ---- scheduling tables -------------------------------------------------------------------------------------------
def tgt(*names):    # a make/CMake target name as a word: "smoke" is not "main-smoke" or "smoketest.sh"
    return r'(?<![\w-])(?:%s)(?![\w-])' % '|'.join(map(re.escape, names))

# A SOLO line runs with no other line running: it needs an X display (hard-coded in its script), writes the game
# directory, or runs FLTSIM98.EXE (smoketest.sh:30 refuses to start while any exists).
SOLO = [
    r'reg_contracts\.sh',                              # runs the game, display :62
    tgt('main-smoke', 'smoke', 'flights-play', 'main-dialogs', 'sim1native-flights', 'references'),   # FLTSIM98.EXE, swap modules in game/
    r'hg2d_frames\.sh',                                # display :150+
    tgt('mdl-crash-game', 'gauges-panels'),            # private game flights, display :176/:177
    r'mdl_crash_frames', r'mdl_frames',                # displays :80+, :86+
    tgt('ofstest-run', 'util-trace'),                  # install OFSTEST/UTIL_TRACE into game/, run FLTSIM98.EXE
    # Load-sensitive: a fixed inner timeout or a hang verdict that load alone trips (docs/tools.md, "Load limits").
    tgt('acontaindiff-run', 'hostdiff-run', 'host-breaks', 'panels-breaks'),
]
# CONFLICT groups: lines of one group never run at once, in any slot. One group per harness a break runner kills
# machine-wide with pkill -f (tools/host_breaks.py:635 "hostdiff.exe"; tools/g3d_breaks.py:434 "g3ddiff_brea[k].exe").
CONFLICT = {
    'host': [tgt('host-breaks', 'hostdiff-run')],
    'g3d': [tgt('g3d-breaks', 'g3ddiff-run', 'g3ddiff-meshc-run')],
}
# Install hooks: these test our installed build, so ofs_install into the slot's game runs just before each.
INSTALL_FIRST = [tgt('main-smoke', 'smoke', 'ofstest-run', 'util-trace')]
# Fixtures no build makes: must be in a slot after setup (copied from the start tree's bin). FRAMELOCK.DLL is
# required only when the start tree has it (it comes from the `framelock` target, not from openfs98).
REQUIRED_FIXTURES = ['sim1-diff/snapshots.bin', 'difftest/replay.bin', 'pressings/reference.json', 'FRAMELOCK.DLL', 'SEHFIX.DLL']
SRC_DIRS = ['cmake', 'docs', 'include', 'src', 'tools']    # the tree check's and the manifest's source directories


def is_solo(cmd):
    return any(re.search(p, cmd) for p in SOLO)


def group_of(cmd):
    return next((g for g, ps in CONFLICT.items() if any(re.search(p, cmd) for p in ps)), None)


def needs_install(cmd):
    return any(re.search(p, cmd) for p in INSTALL_FIRST)


for c in ('tools/reg_contracts.sh', 'tools/ofs smoke', 'tools/ofs main-smoke', 'tools/ofs gauges-panels N=1000',
          'python3 tools/mdl_crash_frames.py', 'tools/ofs ofstest-run', 'tools/hg2d_frames.sh x 900',
          'tools/ofs host-breaks', 'tools/ofs panels-breaks', 'tools/ofs acontaindiff-run',
          'o=$(mktemp -d -u) && REFERENCES_OUT=$o tools/ofs references CASES="global/STGEORGE flight/PARIS script/controls/PARIS" && test "$(grep -cP \'^case\\t(global/STGEORGE|flight/PARIS|script/controls/PARIS)\\tmatch\\t\' "$o/manifest.txt")" = 3'):
    assert is_solo(c), c
for c in ('tools/smoketest.sh', 'tools/ofs gauges-panels-run N=1000', 'tools/ofs main-breaks SUITE=menu', 'tools/ofs hostrtdiff-run'):
    assert not is_solo(c), c
assert group_of('tools/ofs host-breaks') == group_of('tools/ofs hostdiff-run N=20000') == 'host'
assert group_of('tools/ofs g3d-breaks N=1000') == group_of('tools/ofs g3ddiff-meshc-run') == 'g3d'
assert group_of('tools/ofs g3dmeshdiff-run') is None and group_of('tools/ofs hostrtdiff-run') is None
assert needs_install('tools/ofs smoke') and not needs_install('tools/ofs flights-play')


# ---- slots -------------------------------------------------------------------------------------------------------
def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def tree_hash(base, dirs=SRC_DIRS):
    """{relative path: sha256 or 'link:target'} of the given directories, ignoring __pycache__."""
    d = {}
    for top in dirs:
        for dp, dn, fn in os.walk(os.path.join(base, top)):
            dn[:] = [x for x in dn if x != '__pycache__']
            for x in dn:
                if os.path.islink(os.path.join(dp, x)):
                    d[os.path.relpath(os.path.join(dp, x), base)] = 'link:' + os.readlink(os.path.join(dp, x))
            for f in fn:
                q = os.path.join(dp, f)
                d[os.path.relpath(q, base)] = 'link:' + os.readlink(q) if os.path.islink(q) else sha(q)
    return d


def tree_diff(a, b):
    return sorted(k for k in a.keys() | b.keys() if a.get(k) != b.get(k))


def rm_slot_path(path, whole=False):    # the only deletion the run makes: paths under <results>/slots (whole: the root itself)
    slots.remove(path, slots_dir, whole)


def sh(argv, log, env=None, cwd=None):
    with open(log, 'a') as f:
        f.write('$ %s\n' % ' '.join(map(str, argv)))
        f.flush()
        r = subprocess.run(argv, stdout=f, stderr=subprocess.STDOUT, env=env, cwd=cwd)
    if r.returncode:
        raise RuntimeError('%s failed (%d); see %s' % (' '.join(map(str, argv[:3])), r.returncode, log))


def slot_env(slot, extra=None):
    e = dict(os.environ, **{})
    e.update(extra or {})    # the slot's values win over a line's own VAR=value
    e.update(OFS_BUILD=os.path.join(slot, 'build/win32-retail'), PYTHONDONTWRITEBYTECODE='1',
             WINEPREFIX=os.path.join(slot, 'wineprefix'), PYTHONUNBUFFERED='1',
             # the game through the prefix's C:\FS98 link (to <slot>/game), as a serial run's default
             # $HOME/.wine/drive_c/FS98 is: a game started there sees C:, and OFSTEST checks the drive root
             FS98_DIR=os.path.join(slot, 'wineprefix', 'drive_c', 'FS98'))
    return e


def bin_filters():
    """rsync filters for the copy of the start tree's bin: skip run-output directories over --bin-skip-mb, keep
    the required fixtures inside them."""
    src = os.path.join(root, 'bin')
    out = subprocess.run(['du', '-s', '--block-size=1M'] + [os.path.join(src, d) for d in sorted(os.listdir(src))
                         if os.path.isdir(os.path.join(src, d))], capture_output=True, text=True).stdout
    big = [os.path.basename(l.split('\t')[1]) for l in out.splitlines() if bin_skip_mb > 0 and int(l.split('\t')[0]) > bin_skip_mb]
    inc = []
    for fx in REQUIRED_FIXTURES:
        parts = fx.split('/')
        if parts[0] in big:
            inc += ['--include=/' + '/'.join(parts[:k]) + ('/' if k < len(parts) else '') for k in range(1, len(parts) + 1)]
    return big, inc + ['--exclude=/%s/*' % d for d in big]


def listing(path, skip=()):
    """A cheap hash of a tree's listing: path, size, mtime and symlink target of every entry (not its content)."""
    h = hashlib.sha256()
    for dp, dn, fn in os.walk(path):
        dn[:] = sorted(x for x in dn if not (dp == path and x in skip))
        for name in sorted(dn + fn):
            q = os.path.join(dp, name)
            st = os.lstat(q)
            h.update(('%s %d %d %s\n' % (os.path.relpath(q, path), st.st_size, st.st_mtime_ns,
                                         os.readlink(q) if os.path.islink(q) else '')).encode())
    return h.hexdigest()


def prepare_slot(k, start_hash, manifest_of):
    t0 = time.time()
    slot, tmpl = os.path.join(slots_dir, str(k)), os.path.join(slots_dir, '%d.template' % k)
    log = os.path.join(slots_dir, '%d.setup.log' % k)
    for q in (slot, tmpl, slot + '.ACTIVE', log):
        rm_slot_path(q)
    os.makedirs(slot)
    for top in ('orig', 'game'):    # a symlink in what is copied must not lead out of the start tree
        t = os.path.join(root, top)
        links = [t] + [os.path.join(dp, x) for dp, dn, fn in os.walk(t) for x in dn + fn]
        for q in links:
            if os.path.islink(q) and not under(os.path.realpath(q), root):
                raise RuntimeError('%s is a symlink leading outside the start tree (%s)' % (q, os.path.realpath(q)))
    git = os.path.join(root, '.git')
    if os.path.lexists(git) and (os.path.islink(git) or not os.path.isdir(git) or not under(os.path.realpath(git), root)):
        raise RuntimeError('.git is a file or points outside the start tree; not copying it')
    for e in os.listdir(root):
        if e not in ('bin', 'build', 'game', 'cd'):
            sh(['cp', '-a', os.path.join(root, e), os.path.join(slot, e)], log)
    if os.path.lexists(os.path.join(root, 'cd')):
        os.symlink(os.path.join(root, 'cd'), os.path.join(slot, 'cd'))    # no Baselines line writes it
    sh(['cp', '-a', os.path.join(root, 'game'), os.path.join(slot, 'game')], log)
    # the wine prefix, made self-contained (tools/slots.py)
    copy_prefix('~/.wine', slot, os.path.join(slot, 'game'), root, log)
    # build from scratch into an empty bin
    b = os.path.join(slot, 'build/win32-retail')
    env = slot_env(slot)
    sh(['cmake', '-S', slot, '-B', b, '-DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/win32-retail.cmake'], log, env, slot)
    os.makedirs(os.path.join(slot, 'bin'), exist_ok=True)
    sh(['cmake', '--build', b, '--target', 'openfs98', 'difftests'], log, env, slot)
    # fixtures no build makes
    skipped, filt = bin_filters()
    with open(log, 'a') as f:
        f.write('bin directories not copied (over %d MB): %s\n' % (bin_skip_mb, ' '.join(skipped)))
    sh(['rsync', '-a', '--ignore-existing', '--exclude=/break-pool'] + filt + [os.path.join(root, 'bin') + '/', os.path.join(slot, 'bin') + '/'], log)
    names = subprocess.run(['grep', '-rIlF', '--', root, os.path.join(slot, 'bin')], capture_output=True, text=True).stdout
    with open(log, 'a') as f:
        f.write('copied text files naming the start tree (%s), kept:\n%s' % (root, names))
    for fx in REQUIRED_FIXTURES:
        if (fx != 'FRAMELOCK.DLL' or os.path.exists(os.path.join(root, 'bin', fx))) and not os.path.exists(os.path.join(slot, 'bin', fx)):
            raise RuntimeError('fixture bin/%s is missing in the slot' % fx)
    bad = tree_diff(tree_hash(slot), start_hash)
    if bad:
        raise RuntimeError('sources differ from the start tree after the build: %s' % bad[:10])
    man = manifest_of(slot)
    json.dump(man, open(os.path.join(slot, '.slot-manifest.json'), 'w'))
    sh(['cp', '-a', slot, tmpl], log)
    json.dump(man, open(os.path.join(tmpl, '.slot-manifest.json'), 'w'))
    open(os.path.join(tmpl, 'READY'), 'w').close()
    return time.time() - t0


def run_line(argv, shell, cwd, env, limit, log):
    """-> exit code or 'timeout'. SIGTERM to the process group, SIGKILL 60 s later."""
    with open(log, 'w') as f:
        # a session of its own, so that a timeout also reaches the wine and Xvfb processes it started
        p = subprocess.Popen(argv, shell=shell, cwd=cwd, env=env, stdout=f, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            return p.wait(timeout=limit)
        except subprocess.TimeoutExpired:
            def group_alive():
                try:
                    os.killpg(p.pid, 0)
                    return True
                except OSError:
                    return False
            try:
                os.killpg(p.pid, signal.SIGTERM)
            except OSError:
                pass
            end = time.time() + 60
            while time.time() < end and group_alive():    # the shell may exit at once; its children may not
                p.poll()
                time.sleep(0.5)
            try:
                os.killpg(p.pid, signal.SIGKILL)
            except OSError:
                pass
            p.wait()
            return 'timeout'


def free_bytes():
    return shutil.disk_usage(os.path.dirname(results)).free


# ---- the run -----------------------------------------------------------------------------------------------------
if '--list' in sys.argv:
    for n, (cmd, note) in enumerate(lines):
        if (not only or only in cmd) and (pick is None or n in pick):
            print(n, cmd)
    sys.exit(0)

if not in_place and under(results, root):
    sys.exit('results directory %s lies inside the start tree %s; slots would copy it' % (results, root))
os.makedirs(results, exist_ok=True)
lines_hash = hashlib.sha256(json.dumps(lines).encode()).hexdigest()
meta_path = os.path.join(results, 'meta.json')
have = any(re.fullmatch(r'\d{3}\.json', x) for x in os.listdir(results))
meta = json.load(open(meta_path)) if os.path.exists(meta_path) else None
bad = meta is None and have or meta is not None and meta.get('baselines') != lines_hash
if bad and pick is None:
    sys.exit('%s: its records come from another Baselines list (meta.json %s); use a new results directory, or --lines to re-run named lines'
             % (results, 'differs' if meta else 'is missing'))
if bad:    # --lines on another list's records: keep only this list's, so the directory holds one list
    os.makedirs(os.path.join(results, 'earlier'), exist_ok=True)
    for x in os.listdir(results):
        m = re.fullmatch(r'(\d{3})\.(json|log)', x)
        if m and int(m[1]) not in pick:
            os.replace(os.path.join(results, x), os.path.join(results, 'earlier', x))
    meta = None
if meta is None:
    meta = dict(baselines=lines_hash)
    json.dump(meta, open(meta_path, 'w'))

todo = []    # (n, cmd, note) not yet recorded, in file order
rows = []
for n, (cmd, note) in enumerate(lines):
    if (only and only not in cmd) or (pick is not None and n not in pick):
        continue
    tag = '%03d' % n
    rec = os.path.join(results, tag + '.json')
    if pick is not None and os.path.exists(rec):
        os.makedirs(os.path.join(results, 'earlier'), exist_ok=True)
        for ext in ('.json', '.log'):
            os.replace(os.path.join(results, tag + ext), os.path.join(results, 'earlier', tag + ext))
    if os.path.exists(rec):
        rows.append(json.load(open(rec)))
    else:
        todo.append((n, cmd, note))


def execute(n, cmd, note, slot):
    """Run one line (in the slot, or in the start tree when slot is None) -> the record, before the slot's reset."""
    tag = '%03d' % n
    cwd = slot or root
    b = os.path.join(cwd, 'build/win32-retail') if slot else build
    argv, env, via = plan(cmd, b)
    if slot and needs_install(cmd):
        hook = shlex.join(['cmake', '--build', b, '--target', 'ofs_install'])
        argv = hook + ' && ' + (shlex.join(argv) if via else argv)
        via_shell = True
    else:
        via_shell = not via
    doc = minutes(note)
    limit = factor * (max(30, 3 * doc) * 60 if doc else 2 * 3600)
    log = os.path.join(results, tag + '.log')
    t0 = time.time()
    e = slot_env(slot, env) if slot else dict(dict(os.environ, **env), OFS_BUILD=b)
    code = run_line(argv, via_shell, cwd, e, limit, log)
    t1 = time.time()
    out = open(log, errors='replace').read()
    missing = [x for x in need(note) if not found(x, out.replace(',', ''))]
    verdict = 'FAIL' if code != 0 else ('CHECK' if missing else ('PASS' if need(note) else 'UNCHECKED'))
    bv = break_verdict(cmd, note, code, out)
    if bv:
        verdict = 'CHECK' if bv == 'PASS' and missing else bv
    return dict(n=n, cmd=cmd, expect=note, exit=code, seconds=round(t1 - t0), verdict=verdict, missing=missing,
                via_cmake=via, tail=out.strip().splitlines()[-12:], slot=None, tree_changed=[], reset_seconds=0,
                start=round(t0, 3), end=round(t1, 3))


def record(row):
    json.dump(row, open(os.path.join(results, '%03d.json' % row['n']), 'w'), indent=1)
    rows.append(row)
    print('%03d' % row['n'], row['verdict'], row['seconds'], row['cmd'], flush=True)


peak = dict(start_free=free_bytes(), min_free=free_bytes(), setup_seconds={})
stop = threading.Event()
errors = []
if todo and in_place:
    for n, cmd, note in todo:
        record(execute(n, cmd, note, None))
elif todo:
    start_hash = tree_hash(root)
    skipped_bin, _ = bin_filters()
    manifest_base = dict(src=hashlib.sha256(json.dumps(start_hash, sort_keys=True).encode()).hexdigest(),
                         verify=sha(os.path.abspath(__file__)), bin_skip_mb=bin_skip_mb,
                         top={e: sha(os.path.join(root, e)) for e in sorted(os.listdir(root))
                              if os.path.isfile(os.path.join(root, e)) and not os.path.islink(os.path.join(root, e))},
                         game=listing(os.path.join(root, 'game')), orig=listing(os.path.join(root, 'orig')),
                         wine=listing(os.path.expanduser('~/.wine')),
                         bin=listing(os.path.join(root, 'bin'), skipped_bin))

    def manifest_of(slot):
        fx = {f: sha(os.path.join(root, 'bin', f)) for f in REQUIRED_FIXTURES if os.path.exists(os.path.join(root, 'bin', f))}
        return dict(manifest_base, fixtures=fx, slot=slot)

    n_slots = max(1, min(jobs, len(todo)))
    if os.path.islink(slots_dir) or os.path.lexists(slots_dir) and os.path.realpath(slots_dir) != os.path.join(results, 'slots'):
        sys.exit('%s is a symlink or resolves outside the results directory; refusing' % slots_dir)
    os.makedirs(slots_dir, exist_ok=True)
    tmpl_cache = {}

    def setup(k):
        slot, tmpl = os.path.join(slots_dir, str(k)), os.path.join(slots_dir, '%d.template' % k)
        try:
            ok = os.path.exists(os.path.join(tmpl, 'READY')) and json.load(open(os.path.join(tmpl, '.slot-manifest.json'))) == manifest_of(slot)
        except (OSError, ValueError):
            ok = False
        if ok:
            tmpl_cache[k] = tree_hash(tmpl)
            if os.path.exists(slot + '.ACTIVE') or not os.path.isdir(slot):
                reset(k)
            return k, 0
        if os.path.lexists(slot) and not cleanup(slot):
            raise RuntimeError('slot %d is broken: processes it owns survived cleanup' % k)
        s = prepare_slot(k, start_hash, manifest_of)
        tmpl_cache[k] = tree_hash(tmpl)
        return k, round(s)

    def reset(k):
        slot, tmpl = os.path.join(slots_dir, str(k)), os.path.join(slots_dir, '%d.template' % k)
        if not cleanup(slot):
            raise RuntimeError('slot %d is broken: processes it owns survived cleanup' % k)
        t0 = time.time()
        sh(['rsync', '-a', '--delete', '--checksum', tmpl + '/', slot + '/'], os.path.join(slots_dir, '%d.reset.log' % k))
        rm_slot_path(slot + '.ACTIVE')
        return round(time.time() - t0, 1)

    with concurrent.futures.ThreadPoolExecutor(n_slots) as ex:
        for k, secs in ex.map(setup, range(n_slots)):    # any setup failure raises here, before a line starts
            peak['setup_seconds'][k] = secs
    peak['min_free'] = min(peak['min_free'], free_bytes())

    cond = threading.Condition()
    queue = list(todo)
    running = {}    # n -> (solo, group)

    def take():
        """The next line a free slot may start, or None when nothing is eligible now; [] when the queue is empty."""
        for i, (n, cmd, note) in enumerate(queue):
            solo, g = is_solo(cmd), group_of(cmd)
            if solo:
                if not running:
                    return queue.pop(i)
                return None    # a solo line waits for every slot to be free, and nothing overtakes it
            if any(s for s, _ in running.values()) or (g and any(g == r for _, r in running.values())):
                continue
            return queue.pop(i)
        return None

    def worker(k):
        slot = os.path.join(slots_dir, str(k))
        while True:
            with cond:
                while True:
                    if stop.is_set() or not queue:
                        return
                    job = take()
                    if job:
                        break
                    cond.wait()
                n, cmd, note = job
                running[n] = (is_solo(cmd), group_of(cmd))
                open(slot + '.ACTIVE', 'w').write(str(n))
            try:
                row = execute(n, cmd, note, slot)
                row['slot'] = k
                if not cleanup(slot):
                    stop.set()
                    raise RuntimeError('slot %d is broken after line %d: processes it owns survived cleanup' % (k, n))
                row['tree_changed'] = tree_diff(tree_hash(slot), tmpl_cache[k])
                if row['tree_changed']:
                    row['verdict'] = 'FAIL'
                    row['reason'] = 'the slot\'s sources changed during the line: %s' % ', '.join(row['tree_changed'][:10])
                row['reset_seconds'] = reset(k)
                peak['min_free'] = min(peak['min_free'], free_bytes())
                with cond:
                    record(row)
            except BaseException as e:
                stop.set()
                errors.append(e)
                return
            finally:
                with cond:
                    running.pop(n, None)
                    cond.notify_all()

    threads = [threading.Thread(target=worker, args=(k,)) for k in range(n_slots)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    meta.update(setup_seconds=peak['setup_seconds'], peak_disk_bytes=peak['start_free'] - peak['min_free'])
    json.dump(meta, open(meta_path, 'w'))
    if '--clean-slots' in sys.argv and not errors:
        rm_slot_path(slots_dir, whole=True)

# the report covers every line recorded so far, not only this run's (--lines, --only)
rows = [json.load(open(os.path.join(results, x))) for x in sorted(os.listdir(results)) if re.fullmatch(r'\d{3}\.json', x)]
with open(os.path.join(results, 'report.md'), 'w') as f:
    f.write('# Baselines through CMake\n\n| # | verdict | s | slot | tree_changed | command |\n|---|---|---|---|---|---|\n')
    for r in rows:
        f.write('| %d | %s | %s | %s | %s | `%s` |\n' % (r['n'], r['verdict'], r['seconds'], r.get('slot'), ', '.join(r.get('tree_changed') or []), r['cmd']))
    for r in rows:
        if r['verdict'] != 'PASS':
            f.write('\n## %d `%s` — %s (exit %s)\nexpected: %s\nnot found in output: %s\n%s```\n%s\n```\n' % (
                r['n'], r['cmd'], r['verdict'], r['exit'], r['expect'], r['missing'],
                ('reason: %s\n' % r['reason']) if r.get('reason') else '', '\n'.join(r['tail'])))
print(len(rows), 'recorded;', sum(r['verdict'] == 'PASS' for r in rows), 'PASS;',
      sum(r['verdict'] == 'UNCHECKED' for r in rows), 'UNCHECKED')
if errors:
    sys.exit('stopped: %s' % errors[0])
