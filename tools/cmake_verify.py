#!/usr/bin/env python3
"""Run every Baselines command of docs/tools.md through CMake and record how it compares (Story 1.2, piece 2).

usage: cmake_verify.py <build-dir> <results-dir> [-j N] [--in-place] [--baselines FILE] [--clean-slots]
                       [--bin-skip-mb N] [--only SUBSTRING] [--lines N,N,...] [--factor F] [--list] [--clear-stop]
       cmake_verify.py --self-test

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

The protected inventory (protected_hash, both modes): the content, symlink targets and untracked files of cmake, docs,
include, src and tools, plus every top-level file and symlink. Not protected, as run outputs, fixtures or too large:
bin/, build*/, game/, orig/, cd/, wineprefix/, .git/. The verifier's own paths are skipped (the results directory
when inside the tree, a slot's READY and .slot-manifest.json, *.ACTIVE). A process that called setsid itself is not
tracked by the session scan below. The protection covers reuse of one results directory: a new one starts clean.
--in-place: before each line the inventory is taken and <results>/GUARD.json (state running, the line, the list hash,
the inventory) is written atomically; after the line (timeout included) its session is emptied (survivors get SIGKILL;
any left 10 s later stops the run as unverifiable) and the inventory is taken again. Unchanged: the row is recorded and
the marker deleted. Changed: the row is FAIL (tree_changed, reason), the marker becomes `stopped` with the paths, no
further line starts, the report is written and the exit is `stopped: line N changed the tree: ...`. Slots: a slot is
also checked against its template after the reset (a mismatch stops the queue), and the start tree is checked after
every line (a write through a symlink; it stops the queue and leaves a stopped GUARD.json). On every start, before any
file moves, a GUARD.json is honoured in either mode: its list hash must equal the current one and the tree must equal
its inventory, else the run refuses (naming the paths); if both match, line N's record moves to earlier/ and the
marker goes (line N runs again when selected, else it stays unrecorded). --clear-stop does that without comparing,
for a deliberate edit. --lines and --only neither bypass nor clear the marker. Rows and meta.json are written
atomically; a record that does not parse or has no .log moves to earlier/ as .partial and counts as not recorded.
Each line runs with an inherited UPDATE dropped (UPDATE=1 rewrites tools/data_images.txt; verification runs with it unset).
Every line starts behind a pipe gate (its shell waits for a byte) that is released only after GUARD.json holds the line's
session (pid, boot_id, the leader's starttime): a verifier killed before the release leaves a child that exits without running
the line. A start empties each recorded session (SIGKILL) first, so an orphan cannot write after the check: another boot_id means
the session is gone, a leader pid in use with another starttime means it was reused, otherwise every process of that session id
counts (the id alone identifies members once the leader is gone). An unreadable /proc entry is never killed and makes the start
(or the line's check) fail closed. Once a start-tree change is recorded, every line in flight stays in the guard and is requeued
on resume. A guard that is not valid refuses; --clear-stop empties the sessions it can identify, then removes it.
 slots keep a guard for the whole run, deleted only after
the final start-tree check, and every worker failure checks the start tree before the guard is left stopped. A second
verifier on one results directory is refused (flock on <results>/.lock); results may not be the start tree or hold a
protected directory. The inventory records a FIFO, socket or device as special:<mode> without opening it, and an
unreadable subtree stops the run. --self-test runs the module's asserts and exits 0.

Verdict: PASS when the command exits 0 and every number of 100 or more in the documented expectation appears in
the output; UNCHECKED when it exits 0 and the expectation has no such number (exit 0 alone proves nothing: read
the log); CHECK when it exits 0 but one does not (read the log); FAIL on a non-zero exit or a timeout. A break
suite (`*breaks*`) exits 1 whenever a break is missed, so for one whose expectation gives its counts ("N breaks,
M caught[, K proven equivalent]") the verdict is PASS when it did not time out and its last such summary line
has exactly those counts (and every 100+ number is found), FAIL otherwise.
<results-dir>/report.md lists every line with its verdict, slot and tree_changed, and the last lines of its log.
"""
import concurrent.futures, fcntl, hashlib, json, os, re, shlex, shutil, signal, stat, subprocess, sys, threading, time

import slots    # tools/slots.py: shared with break_pool.py
from slots import under, owned_procs, cleanup, copy_prefix

root = os.path.realpath(os.getcwd())


def minutes(note):
    m = re.search(r'\(~?([\d.]+)\s*(s|min|h)\b', note) or re.search(r'~([\d.]+)\s*(s|min|h)\b', note)
    if not m:
        return None
    v = float(m.group(1))
    return v / 60 if m.group(2) == 's' else v if m.group(2) == 'min' else v * 60


def plan(cmd, build):
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


def entry_hash(q):
    """'link:target' for a symlink, the sha256 of a regular file, 'special:<mode>' for a FIFO, socket or device (never opened)."""
    m = os.lstat(q).st_mode
    return 'link:' + os.readlink(q) if stat.S_ISLNK(m) else sha(q) if stat.S_ISREG(m) else 'special:%o' % stat.S_IFMT(m)


def tree_hash(base, dirs=SRC_DIRS, skip=()):
    """{relative path: entry_hash} of the given directories, ignoring __pycache__ and the relative paths in skip. An
    unreadable subtree raises (it is unverifiable, not skipped); a missing top directory is empty."""
    d = {}
    for top in dirs:
        t = os.path.join(base, top)

        def die(e, t=t):
            if not (isinstance(e, FileNotFoundError) and e.filename == t):
                raise e
        for dp, dn, fn in os.walk(t, onerror=die):
            dn[:] = [x for x in dn if x != '__pycache__' and os.path.relpath(os.path.join(dp, x), base) not in skip]
            for x in dn:
                if os.path.islink(os.path.join(dp, x)):
                    d[os.path.relpath(os.path.join(dp, x), base)] = entry_hash(os.path.join(dp, x))
            for f in fn:
                q = os.path.join(dp, f)
                if os.path.relpath(q, base) not in skip:
                    d[os.path.relpath(q, base)] = entry_hash(q)
    return d


# The one protected inventory (in place and slots): SRC_DIRS plus every top-level file and symlink. Not protected, as run
# outputs, fixtures or too large: bin/, build*/, game/, orig/, cd/, wineprefix/, .git/.
SLOT_SKIP = ('READY', '.slot-manifest.json')    # the verifier's own files in a slot


def protected_hash(base, skip=()):
    """tree_hash of SRC_DIRS plus the top-level files and symlinks of base. skip: paths (relative to base, or absolute
    ones inside it) the verifier owns: the results directory, a slot's READY and manifest; any *.ACTIVE is skipped too."""
    base = os.path.realpath(base)
    skip = {os.path.relpath(x, base) if os.path.isabs(x) else x for x in skip}
    d = tree_hash(base, skip=skip)
    for e in os.listdir(base):
        q = os.path.join(base, e)
        if e not in skip and not e.endswith('.ACTIVE') and not stat.S_ISDIR(os.lstat(q).st_mode):
            d[e] = entry_hash(q)
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
    e = update_env(os.environ)
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


def cd_problem(d):
    """None when d's cd is what a slot has: a symlink to <root>/cd (or absent when the start tree has none)."""
    link, want = os.path.join(d, 'cd'), os.path.join(root, 'cd')
    if not os.path.lexists(want):
        return 'cd exists but the start tree has none' if os.path.lexists(link) else None
    return None if os.path.islink(link) and os.readlink(link) == want else '%s is not a symlink to %s' % (link, want)


def no_cd(h):    # the inventory without the cd link (a directory in the start tree), which cd_problem checks instead
    return {k: v for k, v in h.items() if k != 'cd'}


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
    bad = tree_diff(no_cd(protected_hash(slot, SLOT_SKIP)), no_cd(start_hash)) + ([cd_problem(slot)] if cd_problem(slot) else [])
    if bad:
        raise RuntimeError('sources differ from the start tree after the build: %s' % bad[:10])
    man = manifest_of(slot)
    json.dump(man, open(os.path.join(slot, '.slot-manifest.json'), 'w'))
    sh(['cp', '-a', slot, tmpl], log)
    json.dump(man, open(os.path.join(tmpl, '.slot-manifest.json'), 'w'))
    open(os.path.join(tmpl, 'READY'), 'w').close()
    return time.time() - t0


GATE_LOCK = threading.Lock()


def gate_fd():
    """A free single-digit fd (dash cannot close fd 10 and up with `exec N<&-`); caller holds GATE_LOCK."""
    for g in range(9, 2, -1):
        try:
            os.fstat(g)
        except OSError:
            return g
    raise RuntimeError('no free file descriptor below 10 for the launch gate')


def run_line(argv, shell, cwd, env, limit, log, on_start=None):
    """-> exit code or 'timeout'. SIGTERM to the process group, SIGKILL 60 s later (and at most 30 s more for the
    child). The line starts behind a pipe gate: its shell waits for a byte, which is written only after
    on_start(pid, boot_id, starttime) has returned (the guard holds the session). The verifier dying before that
    closes the pipe, and the shell exits without running the line."""
    cmd = argv if shell else shlex.join(argv)
    with open(log, 'w') as f, GATE_LOCK:
        r, w = os.pipe()
        g = gate_fd()
        os.dup2(r, g)
        os.close(r)
        try:
            # a session of its own, so that a timeout also reaches the wine and Xvfb processes it started
            p = subprocess.Popen('read -r _ <&%d || exit 1; exec %d<&-; %s' % (g, g, cmd), shell=True, cwd=cwd, env=env, stdout=f,
                                 stderr=subprocess.STDOUT, start_new_session=True, pass_fds=(g,))
        except BaseException:
            os.close(w)
            raise
        finally:
            os.close(g)
        try:
            if on_start:
                on_start(p.pid, boot_id(), read_stat(p.pid)[3])
            try:
                os.write(w, b'\n')
            except BrokenPipeError:
                pass    # the shell already exited (a syntax error ends it before the gate): nothing left to release
        except BaseException:
            try:
                os.killpg(p.pid, signal.SIGKILL)
            except OSError:
                pass
            raise
        finally:
            os.close(w)
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
            try:
                p.wait(30)
            except subprocess.TimeoutExpired:
                pass    # the quiesce after the line finds it and stops the run
            return 'timeout'


def free_bytes():
    return shutil.disk_usage(os.path.dirname(results)).free


# ---- stopping on a changed tree ----------------------------------------------------------------------------------------
def atomic_json(path, obj, **kw):
    """Write obj to path through a temp file and os.replace, so a crash leaves the old file or the new one."""
    with open(path + '.tmp', 'w') as f:
        json.dump(obj, f, **kw)
        f.flush()
        os.fsync(f.fileno())
    os.replace(path + '.tmp', path)


def boot_id():
    return open('/proc/sys/kernel/random/boot_id').read().strip()


def read_stat(pid):
    """(state, process group, session id, starttime) of /proc/<pid>/stat; OSError if it cannot be read."""
    f = open('/proc/%s/stat' % pid).read().rsplit(')', 1)[1].split()    # state ppid pgrp session ... starttime (index 19)
    return f[0], int(f[2]), int(f[3]), int(f[19])


def session_pids(sid, boot, start, listing=None, read=read_stat):
    """-> ({pid: process group}, {unreadable pids}) of the live processes of the session recorded at launch: its id
    (the leader's pid), the boot_id and the leader's starttime. Another boot: the session is gone. The leader's pid in
    use with another starttime: reused, so not ours. Otherwise every process whose session id matches counts; once the
    leader is gone the id alone identifies the members (a new session cannot take the id while members remain). A process
    that called setsid itself is not tracked. A /proc entry that cannot be read is reported apart: it cannot be shown
    to be outside the session, and it is never killed."""
    if boot != boot_id():
        return {}, set()
    live, unreadable = {}, set()
    for d in listing if listing is not None else os.listdir('/proc'):
        if not d.isdigit():
            continue
        try:
            state, pgrp, session, st = read(int(d))
        except (FileNotFoundError, ProcessLookupError, IndexError, ValueError):
            continue    # gone
        except OSError:
            unreadable.add(int(d))
            continue
        if int(d) == sid and st != start:
            return {}, set()
        if session == sid and state != 'Z':
            live[int(d)] = pgrp
    return live, unreadable


def quiesce(sid, boot, start, grace=2, wait=10, **scan):
    """Empty the line's session: after a short grace the survivors get SIGKILL. -> True when it is empty `wait` s later.
    An unreadable /proc entry makes it False at once, without killing anything."""
    def gone():
        live, unreadable = session_pids(sid, boot, start, **scan)
        return not live and not unreadable
    if session_pids(sid, boot, start, **scan)[1]:
        return False
    end = time.time() + grace
    while not gone() and time.time() < end:
        time.sleep(0.2)
    for _ in range(3):
        live, unreadable = session_pids(sid, boot, start, **scan)
        if unreadable:
            return False
        for pid in live:
            try:
                os.kill(pid, signal.SIGKILL)
            except OSError:
                pass
        end = time.time() + wait / 3
        while not gone() and time.time() < end:
            time.sleep(0.2)
    return gone()


def move_aside(results, tag, suffix=''):
    """Move record tag's .json and .log to <results>/earlier/ (suffix appended); a missing one is skipped."""
    os.makedirs(os.path.join(results, 'earlier'), exist_ok=True)
    for ext in ('.json', '.log'):
        if os.path.exists(os.path.join(results, tag + ext)):
            os.replace(os.path.join(results, tag + ext), os.path.join(results, 'earlier', tag + ext + suffix))


ROW_TYPES = dict(n=int, cmd=str, expect=str, seconds=(int, float), verdict=str)


def strings(x):
    return isinstance(x, list) and all(isinstance(i, str) for i in x)


def valid_row(row):
    return (isinstance(row, dict) and all(isinstance(row.get(k), t) and not isinstance(row.get(k), bool) for k, t in ROW_TYPES.items())
            and isinstance(row.get('exit'), (int, str)) and not isinstance(row.get('exit'), bool)
            and all(strings(row[k]) for k in ('tree_changed', 'missing', 'tail') if k in row)
            and all(isinstance(row[k], str) for k in ('reason', 'expect') if k in row)
            and (row.get('slot') is None or isinstance(row['slot'], int) and not isinstance(row['slot'], bool))
            and all(isinstance(row[k], (int, float)) and not isinstance(row[k], bool) for k in ('reset_seconds', 'start', 'end') if k in row))


def read_record(results, tag):
    """The row of record tag when it parses, has the fields of a row and its .log; else None."""
    try:
        row = json.load(open(os.path.join(results, tag + '.json')))
    except (OSError, ValueError):
        return None
    return row if valid_row(row) and row['n'] == int(tag) and os.path.exists(os.path.join(results, tag + '.log')) else None


def load_record(results, tag):
    """The row of record tag, or None. A .json that is not a whole record is moved to earlier/ (.partial)."""
    if not os.path.exists(os.path.join(results, tag + '.json')):
        return None
    row = read_record(results, tag)
    if row is None:
        move_aside(results, tag, '.partial')
    return row


class Guard:
    """<results>/GUARD.json: state (running or stopped), the lines it covers, the sessions of the running ones (pid, boot_id,
    leader starttime, line), the Baselines list hash and the inventory before. Every write is atomic; lines call it from threads."""

    def __init__(self, path, baselines, before):
        self.path, self.lock = path, threading.Lock()
        self.d = dict(state='running', lines=[], sids=[], baselines=baselines, before=before, tree_changed=[], reason='')

    def _write(self):
        atomic_json(self.path, self.d)

    def begin(self, n):
        with self.lock:
            self.d['lines'].append(n)
            self._write()

    def started(self, n, pid, boot, start):
        with self.lock:
            self.d['sids'].append([pid, boot, start, n])
            self._write()

    def done(self, n):    # the line finished and its tree check passed: its sessions are empty and it is not in doubt
        with self.lock:
            self.d['lines'] = [x for x in self.d['lines'] if x != n]
            self.d['sids'] = [x for x in self.d['sids'] if x[3] != n]
            self._write()

    def stop(self, paths, reason):
        with self.lock:
            self.d.update(state='stopped', tree_changed=sorted(set(self.d['tree_changed']) | set(paths)),
                          reason=(self.d['reason'] + '; ' if self.d['reason'] else '') + reason)
            self._write()

    def clear(self):
        with self.lock:
            if os.path.exists(self.path):
                os.unlink(self.path)


def valid_sid(x):
    return (isinstance(x, list) and len(x) == 4 and isinstance(x[0], int) and isinstance(x[1], str) and isinstance(x[2], int)
            and isinstance(x[3], int) and not any(isinstance(i, bool) for i in x))


def read_guard(path):
    """The guard at path; ValueError when it is not one."""
    try:
        g = json.load(open(path))
    except (OSError, ValueError) as e:
        raise ValueError(e)
    if not (isinstance(g, dict) and g.get('state') in ('running', 'stopped') and isinstance(g.get('baselines'), str)
            and isinstance(g.get('before'), dict) and all(isinstance(k, str) and isinstance(v, str) for k, v in g['before'].items())
            and isinstance(g.get('lines'), list) and all(isinstance(x, int) and not isinstance(x, bool) for x in g['lines'])
            and isinstance(g.get('sids'), list) and all(valid_sid(x) for x in g['sids'])
            and strings(g.get('tree_changed', [])) and isinstance(g.get('reason', ''), str)):
        raise ValueError('not a guard')
    return g


def salvage_sids(path):
    """The well-formed session entries of a guard that is otherwise invalid (for --clear-stop to empty first)."""
    try:
        x = json.load(open(path))
        return [e for e in x['sids'] if valid_sid(e)]
    except (OSError, ValueError, KeyError, TypeError):
        return []


def guard_start(results, lines_hash, inventory, clear=False, veto=None):
    """The GUARD.json rule at every start. -> None to go on (the marker is gone), else the refusal text, with nothing
    consumed. Order: the sessions of the stopped lines are emptied first (before the tree is compared or --clear-stop
    honoured); then the list and the tree are compared (--clear-stop skips both); then veto (another refusal, such as a
    changed Baselines list with records) is asked; only then do the lines' records move and the marker go."""
    path = os.path.join(results, 'GUARD.json')
    if not os.path.exists(path):
        return veto() if veto else None
    try:
        g = read_guard(path)
    except ValueError as e:
        if not clear:
            return 'GUARD.json is unreadable (%s); restore it, or pass --clear-stop to remove it' % e
        g = dict(lines=[], baselines=lines_hash, sids=salvage_sids(path))    # what can be identified is emptied first
    for pid, boot, st, n in g['sids']:
        if not quiesce(pid, boot, st):
            return 'processes of line %d (session %d) are still running or cannot be read, and could not be killed' % (n, pid)
    where = ('line %s' % g['lines'][0] if len(g['lines']) == 1 else 'lines %s' % ', '.join(map(str, g['lines']))) if g['lines'] else 'the run'
    if not clear:
        if g['baselines'] != lines_hash:
            return 'the Baselines list changed since %s stopped; use a new results directory, or --clear-stop' % where
        bad = tree_diff(g['before'], inventory())
        if bad:
            return '%s (%s) left the tree changed: %s\nrestore them, or pass --clear-stop after a deliberate edit' % (where, g['state'], ', '.join(bad[:20]))
    r = veto() if veto else None
    if r:
        return r
    for n in g['lines']:
        move_aside(results, '%03d' % n)
    os.unlink(path)
    return None


def update_env(base):
    """An inherited UPDATE=1 rewrites tools/data_images.txt; verification runs with it unset unless the line sets it."""
    return {k: v for k, v in base.items() if k != 'UPDATE'}


def read_meta(path):
    """The meta.json object, None when there is none; SystemExit with a message when it is not an object."""
    if not os.path.exists(path):
        return None
    try:
        m = json.load(open(path))
    except (OSError, ValueError):
        m = None
    if not isinstance(m, dict):
        sys.exit('%s is not a JSON object; use a new results directory' % path)
    return m


def meta_refusal(results, meta, lines_hash, have, pick):
    bad = meta is None and have or meta is not None and meta.get('baselines') != lines_hash
    if bad and pick is None:
        return ('%s: its records come from another Baselines list (meta.json %s); use a new results directory, or --lines to re-run named lines'
                % (results, 'differs' if meta else 'is missing'))


def results_refusal(results, base):
    """Why results may not be the verifier's output directory (the start tree or a directory holding a protected one)."""
    if results == base:
        return 'it is the start tree'
    for top in SRC_DIRS:
        if under(os.path.join(base, top), results):
            return 'it is or contains %s' % top


if '--self-test' in sys.argv:
    import tempfile
    with tempfile.TemporaryDirectory() as t:
        def put(rel, data='x'):
            os.makedirs(os.path.dirname(os.path.join(t, rel)) or t, exist_ok=True)
            open(os.path.join(t, rel), 'w').write(data)
        for rel in ('CMakeLists.txt', 'src/a.c', 'tools/t.py', 'bin/o', 'results/r', 'READY', '.slot-manifest.json', 'x.ACTIVE', 'build/b'):
            put(rel)
        skip = ('results', 'READY', '.slot-manifest.json')
        h0 = protected_hash(t, skip)
        assert set(h0) == {'CMakeLists.txt', 'src/a.c', 'tools/t.py'}, set(h0)
        for rel in ('bin/o', 'results/r', 'READY', '.slot-manifest.json', 'x.ACTIVE', 'build/b', 'game/g'):
            put(rel, 'changed')
            assert protected_hash(t, skip) == h0, rel
        assert protected_hash(os.path.join(t, '.'), [os.path.join(t, 'results')] + list(skip[1:])) == h0
        for rel in ('CMakeLists.txt', 'src/a.c', 'src/new.c', 'tools/sub/new.txt', 'top.txt'):
            h = protected_hash(t, skip)
            put(rel, 'changed')
            assert tree_diff(h, protected_hash(t, skip)) == [rel], rel
        os.symlink('src/a.c', os.path.join(t, 'lnk'))
        assert protected_hash(t, skip)['lnk'] == 'link:src/a.c'
        os.mkfifo(os.path.join(t, 'src/fifo'))    # a FIFO is recorded, not opened (it would hang)
        os.mkfifo(os.path.join(t, 'top.fifo'))
        h = protected_hash(t, skip)
        assert h['src/fifo'] == h['top.fifo'] == 'special:10000', h
        if os.getuid():    # an unreadable subtree is unverifiable, not skipped
            os.mkdir(os.path.join(t, 'src/locked'))
            os.chmod(os.path.join(t, 'src/locked'), 0)
            try:
                protected_hash(t, skip)
                raise AssertionError('an unreadable subtree was skipped')
            except PermissionError:
                pass
            os.chmod(os.path.join(t, 'src/locked'), 0o700)
    assert results_refusal('/a/b', '/a/b') and results_refusal('/a', '/a/b') and results_refusal('/a/b/src', '/a/b')
    assert results_refusal('/a/b/src/x', '/a/b') is None and results_refusal('/c/r', '/a/b') is None
    good = dict(n=1, cmd='c', expect='e', exit=0, seconds=1.5, verdict='PASS')
    assert valid_row(good) and valid_row(dict(good, tree_changed=['a'], missing=[], tail=['x'], slot=None, reason='r'))
    assert not valid_row(dict(good, seconds='1'))
    for k, v in (('tree_changed', 'a'), ('tree_changed', [1]), ('missing', [None]), ('tail', 'x'), ('slot', '0'), ('reason', 1), ('start', 'x')):
        assert not valid_row(dict(good, **{k: v})), k
    assert not valid_row([]) and not valid_row(dict(n=True, cmd='c', expect='e', exit=0, seconds=1, verdict='PASS'))
    with tempfile.TemporaryDirectory() as r:
        inv = {'src/a.c': 'h'}
        G = os.path.join(r, 'GUARD.json')
        def mark(sids=(), lines=(3,), base='L'):
            g = Guard(G, base, inv)
            g.d.update(lines=list(lines), sids=[list(x) for x in sids], state='stopped')
            g._write()
            for n in lines:
                open(os.path.join(r, '%03d.json' % n), 'w').write('{}')
                open(os.path.join(r, '%03d.log' % n), 'w').write('')
        assert guard_start(r, 'L', lambda: 1 / 0) is None    # no marker: the tree is not even read
        assert guard_start(r, 'L', lambda: 1 / 0, veto=lambda: 'veto') == 'veto'
        mark()
        assert 'Baselines list changed since line 3' in guard_start(r, 'M', lambda: inv) and os.path.exists(G)
        assert 'src/a.c' in guard_start(r, 'L', lambda: {'src/a.c': 'g'}) and 'restore them' in guard_start(r, 'L', lambda: {})
        assert guard_start(r, 'L', lambda: inv, veto=lambda: 'veto') == 'veto' and os.path.exists(G) and os.path.exists(os.path.join(r, '003.json'))
        assert guard_start(r, 'L', lambda: inv) is None and not os.path.exists(G)
        assert os.path.exists(os.path.join(r, 'earlier', '003.json')) and not os.path.exists(os.path.join(r, '003.json'))
        mark(lines=(3, 4))
        assert guard_start(r, 'M', lambda: {}, clear=True) is None and not os.path.exists(G)
        assert all(os.path.exists(os.path.join(r, 'earlier', x)) for x in ('003.log', '004.json', '004.log'))
        open(G, 'w').write('{"state": ')    # an unreadable guard refuses; --clear-stop removes it
        assert 'unreadable' in guard_start(r, 'L', lambda: inv) and os.path.exists(G)
        assert guard_start(r, 'L', lambda: inv, clear=True) is None and not os.path.exists(G)
        open(G, 'w').write('[]')
        assert 'unreadable' in guard_start(r, 'L', lambda: inv)
        os.unlink(G)
        # the guard's session is emptied before anything else, --clear-stop included
        def ident(p):
            return [p.pid, boot_id(), read_stat(p.pid)[3]]
        orphan = subprocess.Popen(['sleep', '60'], start_new_session=True)
        who = ident(orphan)
        mark(sids=[who + [3]])
        assert guard_start(r, 'M', lambda: 1 / 0, clear=True) is None and orphan.wait(10) is not None and not any(session_pids(*who))
        orphan = subprocess.Popen(['sleep', '60'], start_new_session=True)
        pid, boot, st = ident(orphan)
        mark(sids=[[pid, boot, st + 1, 3]])    # the leader's pid is in use with another starttime: not the line's session, left alone
        assert guard_start(r, 'L', lambda: inv) is None and orphan.poll() is None
        mark(sids=[[pid, 'another-boot', st, 3]])    # another boot: the session is gone
        assert guard_start(r, 'L', lambda: inv) is None and orphan.poll() is None
        # an unreadable /proc entry is never killed and makes the start refuse
        def deny(pid_):
            if pid_ == orphan.pid + 1:
                raise PermissionError(13, 'denied')
            return read_stat(pid_)
        assert session_pids(pid, boot, st, listing=[str(pid), str(pid + 1)], read=deny) == ({pid: pid}, {pid + 1})
        assert quiesce(pid, boot, st, listing=[str(pid + 1)], read=deny) is False and orphan.poll() is None
        assert quiesce(pid, boot, st, listing=[str(pid), str(pid + 1)], read=deny) is False and orphan.poll() is None
        mark(sids=[[pid, boot, st, 3]])
        assert guard_start(r, 'L', lambda: inv) is None and orphan.wait(10) is not None    # readable members are killed
        # an invalid guard is fail-closed; --clear-stop empties what can be identified, then removes it
        orphan = subprocess.Popen(['sleep', '60'], start_new_session=True)
        open(G, 'w').write(json.dumps(dict(state='stopped', sids=[ident(orphan) + [3], ['x', 1, 2, 3]], lines='bad')))
        assert 'unreadable' in guard_start(r, 'L', lambda: inv) and orphan.poll() is None
        assert guard_start(r, 'L', lambda: inv, clear=True) is None and orphan.wait(10) is not None and not os.path.exists(G)
        for bad in (dict(sids=[[1, 'b', 2]]), dict(sids=[[1, 'b', '2', 3]]), dict(sids=[[True, 'b', 2, 3]]), dict(before={'a': 1}),
                    dict(lines=['1']), dict(tree_changed=[1]), dict(state='x')):
            g = Guard(G, 'L', inv)
            g.d.update(bad)
            g._write()
            try:
                read_guard(G)
                raise AssertionError(bad)
            except ValueError:
                pass
        g = Guard(G, 'L', inv)
        g._write()
        g.started(2, 5, 'b', 7)
        assert read_guard(G)['sids'] == [[5, 'b', 7, 2]]
        os.unlink(G)
        # partial records: no log, unparseable json, a wrong schema; a whole one stays
        open(os.path.join(r, '001.json'), 'w').write(json.dumps(dict(n=1, cmd='c', expect='', exit=0, seconds=1, verdict='PASS')))
        open(os.path.join(r, '002.json'), 'w').write('{"n": ')
        open(os.path.join(r, '002.log'), 'w').write('')
        for n in (4, 5):
            open(os.path.join(r, '00%d.json' % n), 'w').write(json.dumps(dict(n=n, cmd='c', expect='', exit=0, seconds=1, verdict='PASS') if n == 4 else dict(n=5)))
            open(os.path.join(r, '00%d.log' % n), 'w').write('')
        assert load_record(r, '001') is None and load_record(r, '002') is None and load_record(r, '005') is None and load_record(r, '004')['n'] == 4
        assert sorted(x for x in os.listdir(os.path.join(r, 'earlier')) if 'partial' in x) == ['001.json.partial', '002.json.partial', '002.log.partial', '005.json.partial', '005.log.partial']
        mp = os.path.join(r, 'meta.json')
        assert read_meta(mp) is None
        open(mp, 'w').write('{"baselines": "L"}')
        assert read_meta(mp) == {'baselines': 'L'} and meta_refusal(r, read_meta(mp), 'M', True, None) and meta_refusal(r, read_meta(mp), 'M', True, {1}) is None
        open(mp, 'w').write('[]')
        try:
            read_meta(mp)
            raise AssertionError('a non-object meta.json was accepted')
        except SystemExit:
            pass
    p = subprocess.Popen(['sh', '-c', 'python3 -c "import os,time; os.setpgid(0,0); time.sleep(60)" & sleep 60'], start_new_session=True)
    try:
        who = (boot_id(), read_stat(p.pid)[3])
        end = time.time() + 10
        while time.time() < end and not any(g != p.pid for g in session_pids(p.pid, *who)[0].values()):
            time.sleep(0.1)
        assert any(g != p.pid for g in session_pids(p.pid, *who)[0].values()), 'a child in its own process group is not found'
        assert quiesce(p.pid, *who, grace=0) and session_pids(p.pid, *who) == ({}, set())
    finally:
        p.kill()
    p.wait()
    # the launch gate: the line does not run when on_start fails (the verifier would be dead before the release)
    with tempfile.TemporaryDirectory() as t:
        seen = []
        assert run_line(['touch', t + '/ran'], False, t, dict(os.environ), 30, t + '/log', lambda pid, b, st: seen.append((pid, b, st))) == 0
        assert os.path.exists(t + '/ran') and seen[0][1] == boot_id()
        os.unlink(t + '/ran')
        def boom(pid, b, st):
            raise OSError(28, 'injected')
        try:
            run_line('touch %s/ran' % t, True, t, dict(os.environ), 30, t + '/log', boom)
            raise AssertionError('on_start error swallowed')
        except OSError:
            pass
        time.sleep(1)
        assert not os.path.exists(t + '/ran')
    assert update_env({'UPDATE': '1', 'A': 'b'}) == {'A': 'b'}
    print('cmake_verify self-test: ok')
    sys.exit(0)

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


# ---- the run -----------------------------------------------------------------------------------------------------
if '--list' in sys.argv:
    for n, (cmd, note) in enumerate(lines):
        if (not only or only in cmd) and (pick is None or n in pick):
            print(n, cmd)
    sys.exit(0)

if not in_place and under(results, root):
    sys.exit('results directory %s lies inside the start tree %s; slots would copy it' % (results, root))
if results_refusal(results, root):
    sys.exit('results directory %s: %s; refusing' % (results, results_refusal(results, root)))
os.makedirs(results, exist_ok=True)
lock = open(os.path.join(results, '.lock'), 'w')    # held until the process exits
try:
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
except OSError:
    sys.exit('%s: another cmake_verify is using this results directory' % results)
lines_hash = hashlib.sha256(json.dumps(lines).encode()).hexdigest()
root_skip = [results] if under(results, root) else []
meta_path = os.path.join(results, 'meta.json')
meta = read_meta(meta_path)
have = any(re.fullmatch(r'\d{3}\.json', x) and read_record(results, x[:3]) for x in os.listdir(results))
bad = meta is None and have or meta is not None and meta.get('baselines') != lines_hash
refusal = guard_start(results, lines_hash, lambda: protected_hash(root, root_skip), '--clear-stop' in sys.argv,
                      veto=lambda: meta_refusal(results, meta, lines_hash, have, pick))
if refusal:
    sys.exit('%s: %s' % (results, refusal))
for x in sorted(os.listdir(results)):    # partial records are moved aside before anything counts records
    if re.fullmatch(r'\d{3}\.json', x):
        load_record(results, x[:3])
if bad:
    os.makedirs(os.path.join(results, 'earlier'), exist_ok=True)    # --lines on another list's records: keep only this list's
    for x in os.listdir(results):
        m = re.fullmatch(r'(\d{3})\.(json|log)', x)
        if m and int(m[1]) not in pick:
            os.replace(os.path.join(results, x), os.path.join(results, 'earlier', x))
    meta = None
if meta is None:
    meta = dict(baselines=lines_hash)
    atomic_json(meta_path, meta)

todo = []    # (n, cmd, note) not yet recorded, in file order
rows = []
for n, (cmd, note) in enumerate(lines):
    if (only and only not in cmd) or (pick is not None and n not in pick):
        continue
    tag = '%03d' % n
    if pick is not None and os.path.exists(os.path.join(results, tag + '.json')):
        move_aside(results, tag)
    row = load_record(results, tag)
    if row:
        rows.append(row)
    else:
        todo.append((n, cmd, note))


def execute(n, cmd, note, slot, on_start=None):
    """Run one line (in the slot, or in the start tree when slot is None) -> the record, before the slot's reset.
    The line's session is emptied before its log is read; _quiet says whether that worked."""
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
    e = slot_env(slot, env) if slot else {**update_env(os.environ), **env, 'OFS_BUILD': b}
    started = []
    code = run_line(argv, via_shell, cwd, e, limit, log, lambda pid, b, st: (started.append((pid, b, st)), on_start and on_start(pid, b, st)))
    quiet = quiesce(*started[0])
    t1 = time.time()
    out = open(log, errors='replace').read()
    missing = [x for x in need(note) if not found(x, out.replace(',', ''))]
    verdict = 'FAIL' if code != 0 else ('CHECK' if missing else ('PASS' if need(note) else 'UNCHECKED'))
    bv = break_verdict(cmd, note, code, out)
    if bv:
        verdict = 'CHECK' if bv == 'PASS' and missing else bv
    return dict(n=n, cmd=cmd, expect=note, exit=code, seconds=round(t1 - t0), verdict=verdict, missing=missing,
                via_cmake=via, tail=out.strip().splitlines()[-12:], slot=None, tree_changed=[], reset_seconds=0,
                start=round(t0, 3), end=round(t1, 3), _quiet=quiet)


def record(row):
    row = {k: v for k, v in row.items() if not k.startswith('_')}
    atomic_json(os.path.join(results, '%03d.json' % row['n']), row, indent=1)
    rows.append(row)
    print('%03d' % row['n'], row['verdict'], row['seconds'], row['cmd'], flush=True)


def read_tree(base, skip):
    """-> (inventory, None), or (None, why) when part of the tree cannot be read (unverifiable)."""
    try:
        return protected_hash(base, skip), None
    except OSError as e:
        return None, 'the tree cannot be read (%s)' % e


peak = dict(start_free=free_bytes(), min_free=free_bytes(), setup_seconds={})
stop = threading.Event()
errors = []
guard_path = os.path.join(results, 'GUARD.json')
if todo and in_place:
    for n, cmd, note in todo:
        before = protected_hash(root, root_skip)
        guard = Guard(guard_path, lines_hash, before)
        guard.begin(n)
        row = execute(n, cmd, note, None, lambda pid, b, st: guard.started(n, pid, b, st))
        after, why = read_tree(root, root_skip)
        changed = tree_diff(before, after) if after is not None else []
        if not row.pop('_quiet'):
            why = 'processes of the line survived SIGKILL; the tree cannot be verified'
        if why is None and not changed:
            record(row)
            guard.clear()
            continue
        row.update(verdict='FAIL', tree_changed=changed, reason=why or 'the tree changed during the line: %s' % ', '.join(changed[:10]))
        guard.stop(changed, row['reason'])    # published before the row
        record(row)
        errors.append(RuntimeError('line %d changed the tree: %s' % (n, ', '.join(changed[:10])) if changed else
                                   'line %d: %s' % (n, why)))
        break
elif todo:
    start_hash = protected_hash(root, root_skip)
    skipped_bin, _ = bin_filters()
    manifest_base = dict(src=hashlib.sha256(json.dumps(start_hash, sort_keys=True).encode()).hexdigest(),
                         verify=sha(os.path.abspath(__file__)), bin_skip_mb=bin_skip_mb,
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

    def reset(k):
        """Restore the slot from its template and check it equals the template; .ACTIVE goes only after the check passes."""
        slot, tmpl = os.path.join(slots_dir, str(k)), os.path.join(slots_dir, '%d.template' % k)
        if not cleanup(slot):
            raise RuntimeError('slot %d is broken: processes it owns survived cleanup' % k)
        t0 = time.time()
        sh(['rsync', '-a', '--delete', '--checksum', tmpl + '/', slot + '/'], os.path.join(slots_dir, '%d.reset.log' % k))
        left = tree_diff(protected_hash(slot, SLOT_SKIP), tmpl_cache[k])
        if left:
            open(slot + '.ACTIVE', 'w').write('reset mismatch')    # the next start resets it again
            raise RuntimeError('slot %d differs from its template after the reset: %s' % (k, ', '.join(left[:10])))
        rm_slot_path(slot + '.ACTIVE')
        return round(time.time() - t0, 1)

    def setup(k):
        slot, tmpl = os.path.join(slots_dir, str(k)), os.path.join(slots_dir, '%d.template' % k)
        try:
            ok = os.path.exists(os.path.join(tmpl, 'READY')) and json.load(open(os.path.join(tmpl, '.slot-manifest.json'))) == manifest_of(slot)
        except (OSError, ValueError):
            ok = False
        if ok:    # a reused template is checked like a fresh one
            tmpl_cache[k] = protected_hash(tmpl, SLOT_SKIP)
            ok = not tree_diff(no_cd(tmpl_cache[k]), no_cd(start_hash)) and not cd_problem(tmpl)
        if ok:
            if os.path.exists(slot + '.ACTIVE') or not os.path.isdir(slot) or tree_diff(protected_hash(slot, SLOT_SKIP), tmpl_cache[k]):
                reset(k)    # a reused working slot must equal its template before its first line
            return k, 0
        if os.path.lexists(slot) and not cleanup(slot):
            raise RuntimeError('slot %d is broken: processes it owns survived cleanup' % k)
        s = prepare_slot(k, start_hash, manifest_of)
        tmpl_cache[k] = protected_hash(tmpl, SLOT_SKIP)
        return k, round(s)

    guard = Guard(guard_path, lines_hash, start_hash)    # lives until the last start-tree check passes; it covers the setup too
    atomic_json(guard_path, guard.d)
    try:
        with concurrent.futures.ThreadPoolExecutor(n_slots) as ex:
            for k, secs in ex.map(setup, range(n_slots)):    # any setup failure raises here, before a line starts
                peak['setup_seconds'][k] = secs
    except BaseException as e:
        moved = tree_diff(start_hash, protected_hash(root, root_skip))
        if moved:
            guard.stop(moved, 'slot setup failed (%s) and the start tree changed' % e)
        else:
            guard.clear()
        raise
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

    tainted = threading.Event()    # a start-tree change has been recorded: every line then in flight is in doubt

    def worker(k):
        try:
            work(k)
        except BaseException as e:    # whatever escapes the line's own handling: still a stop, still reported
            stop.set()
            errors.append(e)

    def work(k):
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
            try:
                open(slot + '.ACTIVE', 'w').write(str(n))
                guard.begin(n)
                row = execute(n, cmd, note, slot, lambda pid, b, st: guard.started(n, pid, b, st))
                row['slot'] = k
                if not row.pop('_quiet'):
                    raise RuntimeError('line %d: processes of its session survived SIGKILL' % n)
                if not cleanup(slot):
                    raise RuntimeError('slot %d is broken after line %d: processes it owns survived cleanup' % (k, n))
                row['tree_changed'] = tree_diff(protected_hash(slot, SLOT_SKIP), tmpl_cache[k])
                if row['tree_changed']:
                    row['verdict'] = 'FAIL'
                    row['reason'] = 'the slot\'s sources changed during the line: %s' % ', '.join(row['tree_changed'][:10])
                row['reset_seconds'] = reset(k)
                peak['min_free'] = min(peak['min_free'], free_bytes())
                with cond:    # the start-tree check, the guard and the row, one at a time
                    was = tainted.is_set()
                    moved = tree_diff(start_hash, protected_hash(root, root_skip))    # a write through a symlink into the start tree
                    if moved or was:    # in doubt: it stays in the guard (requeued on resume) even if its own slot is clean
                        tainted.set()
                        stop.set()
                        row.update(verdict='FAIL', tree_changed=moved or row['tree_changed'],
                                   reason='the start tree changed during the line: %s' % ', '.join(moved[:10]) if moved else
                                   'a start-tree change was recorded while the line ran')
                        guard.stop(moved, row['reason'])    # published before the row
                    record(row)
                    if moved:
                        raise RuntimeError('line %d changed the tree: %s' % (n, ', '.join(moved[:10])))
                    if not was:
                        guard.done(n)
            except BaseException as e:
                stop.set()
                errors.append(e)
                with cond:    # the start tree is checked on every way out; the guard stays
                    try:
                        moved = tree_diff(start_hash, protected_hash(root, root_skip))
                    except OSError as x:
                        moved = ['(unreadable: %s)' % x]
                    if moved:
                        tainted.set()
                    try:
                        guard.stop(moved, str(e))
                    except OSError as x:    # the guard cannot be written: the run is stopped and reported all the same
                        errors.append(x)
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
    if not errors:    # the last start-tree check, then the guard goes
        moved = tree_diff(start_hash, protected_hash(root, root_skip))
        if moved:
            guard.stop(moved, 'the start tree changed by the end of the run')
            errors.append(RuntimeError('the run changed the tree: %s' % ', '.join(moved[:10])))
        else:
            guard.clear()
    meta.update(setup_seconds=peak['setup_seconds'], peak_disk_bytes=peak['start_free'] - peak['min_free'])
    atomic_json(meta_path, meta)
    if '--clean-slots' in sys.argv and not errors:
        rm_slot_path(slots_dir, whole=True)

# the report covers every line recorded so far, not only this run's (--lines, --only)
rows = [r for r in (load_record(results, x[:3]) for x in sorted(os.listdir(results)) if re.fullmatch(r'\d{3}\.json', x)) if r]
with open(os.path.join(results, 'report.md'), 'w') as f:
    f.write('# Baselines through CMake\n\n| # | verdict | s | slot | tree_changed | command |\n|---|---|---|---|---|---|\n')
    for r in rows:
        f.write('| %d | %s | %s | %s | %s | `%s` |\n' % (r['n'], r['verdict'], r['seconds'], r.get('slot'), ', '.join(r.get('tree_changed') or []), r['cmd']))
    for r in rows:
        if r['verdict'] != 'PASS':
            f.write('\n## %d `%s` — %s (exit %s)\nexpected: %s\nnot found in output: %s\n%s```\n%s\n```\n' % (
                r['n'], r['cmd'], r['verdict'], r['exit'], r['expect'], r.get('missing'),
                ('reason: %s\n' % r['reason']) if r.get('reason') else '', '\n'.join(r.get('tail') or [])))
print(len(rows), 'recorded;', sum(r['verdict'] == 'PASS' for r in rows), 'PASS;',
      sum(r['verdict'] == 'UNCHECKED' for r in rows), 'UNCHECKED')
if errors:
    sys.exit('stopped: %s' % errors[0])
