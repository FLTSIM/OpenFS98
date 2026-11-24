#!/usr/bin/env bash
# Record what retail does, once, as reference logs for the native builds (AD-8, Story 1.6):
# every case flown twice by tools/hg2d_frames.sh (retail only, no sound, software renderer,
# 900 passes) in a game staged fresh from the CD.  Run it as tools/ofs references.
#
#   tools/record_references.sh [case...]            record (no case: all 131)
#   tools/record_references.sh --verify DIRECTORY   check a published set against its manifest.txt
#
# case     flight/<NAME> (74), script/<scenario>/<NAME> (7 scenarios x 8 flights = 56),
#          global/STGEORGE (GLOBAL dumped each pass).  CASES="a b" does the same as arguments.
#          A subset also records the unscripted flight/ case of each script case, to check the
#          script reached the sim.
#
# REFERENCES_OUT     subset only: where it publishes (must not exist; outside bin/references*,
#                    game/, cd/, orig/, src/ and not containing them); manifest.txt goes there too
# REFERENCES_FORCE=1 replace a non-empty bin/references/
# REFERENCES_JOBS    cases at once (default 4)
# REFERENCES_DISPLAY_BASE  first Xvfb display (default 120; job n uses base+n)
# REFERENCES_TEST_PLANT=file  copy a PE not on the CD into the stage before the check
# REFERENCES_TEST_EXE=file    replace the staged EXE before the check
# REFERENCES_TEST_SKEW=1      subset only: change one blit hash in run b's log after it returns;
#                    =global changes one byte of run b's global.bin instead (global case only)
# REFERENCES_TEST_TRUNCATE=1  subset only: remove run b's last line after it returns
#
# A full run publishes bin/references/ (the logs, config/ and manifest.txt, which is
# authoritative) and mirrors the manifest to tools/references.txt, the only committed part.
# Outcomes: match, NONDETERMINISTIC (both runs kept), FAILED (nothing is published, exit 1).
# Layout, grammar and the full-run procedure: docs/tools.md ("Retail reference logs").
set -uo pipefail
if [ -z "${BASH_EXECUTION_STRING:-}" ]; then
    exec bash -c "$(cat -- "$0")" "$0" "$@"
fi
cd "$(dirname "$0")/.."
root=$PWD
PASSES=900
SCENARIOS="controls systems views avionics slew toggles keys"
SCRIPT_FLIGHTS="PARIS STGEORGE KAUAI NEWYORK LEARLON HONGKONG BELLWTC CVINSON"

die() { echo "references: $*" >&2; exit 1; }
usage() { echo "references: $*" >&2; exit 2; }

read -r -d '' PYLIB <<'PY'
import hashlib, os, re, shutil, sys

PASSES = 900
GLOBAL_BYTES = 4298400

def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()

def files(top):
    for dp, dn, fn in os.walk(top):
        dn.sort()
        for n in sorted(fn):
            p = os.path.join(dp, n)
            if os.path.isfile(p):
                yield p

def is_pe(p):
    with open(p, 'rb') as f:
        return f.read(2) == b'MZ'

def check_stage(stage, cd, ref):
    cdpe = {sha(p) for p in files(cd) if is_pe(p)}
    bad, pe, data = [], [], []
    for p in files(stage):
        rel = os.path.relpath(p, stage).replace(os.sep, '/')
        h = sha(p)
        if is_pe(p):
            pe.append('%s %s' % (rel, h))
            if h not in cdpe:
                bad.append(rel)
        else:
            data.append('%s %s' % (rel, h))
    if bad:
        print('references: not a pure retail stage, PEs not on the CD: ' + ', '.join(bad), file=sys.stderr)
        sys.exit(1)
    exe = sha(os.path.join(stage, 'FLTSIM98.EXE'))
    orig = sha('orig/FLTSIM98.EXE')
    if exe != ref or orig != ref:
        print('references: wrong pressing: staged EXE %s, orig/ EXE %s, FRAMELOCK needs %s' % (exe, orig, ref), file=sys.stderr)
        sys.exit(1)
    d = lambda L: hashlib.sha256(('\n'.join(sorted(L)) + '\n').encode()).hexdigest()
    print('exe_sha256', exe)
    print('pe_sha256', d(pe), len(pe))
    print('data_sha256', d(data), len(data))

def flights(stage):
    top = os.path.join(stage, 'Situatio')
    seen = {}
    for n in sorted(os.listdir(top)):
        if n.lower().endswith('.stn') and os.path.isfile(os.path.join(top, n)):
            c = n[:-4].upper()
            if c in seen:
                print('references: two flights named %s' % c, file=sys.stderr); sys.exit(1)
            seen[c] = n
    if len(seen) != 74:
        print('references: %d flights in the staged Situatio/, not 74' % len(seen), file=sys.stderr); sys.exit(1)
    for c, n in seen.items():
        print(c, n)

def script_ok(path):
    try:
        text = open(path).read()
    except OSError as e:
        return 'no script file: %s' % e
    n = last = 0
    for i, line in enumerate(text.splitlines(), 1):
        if not line.strip():
            continue
        f = line.split()
        if len(f) != 4 or not all(re.fullmatch('[0-9a-fA-F]+', x) for x in f):
            return 'line %d: not four bare hex fields' % i
        p = int(f[0], 16)
        n += 1
        if not 1 <= p <= 899:
            return 'line %d: pass %d outside 1..899' % (i, p)
        if p < last:
            return 'line %d: passes not ascending' % i
        last = p
    return None if n else 'no events'

def read_file(p):
    try:
        return open(p, 'rb').read()
    except OSError:
        return None

def blits(data):
    for line in data.decode('latin1').split('\n'):
        f = line.split()
        if len(f) == 10 and f[0] not in ('end', 'fault', 'sim1') and re.fullmatch('[0-9a-f]+', f[0]):
            yield f

def judge_run(d, tag, glob):
    """-> list of failure reasons: completion, then frames, then dump size"""
    st = read_file(os.path.join(d, 'status'))
    if st is None:
        return ['%s: no status' % tag]
    m = re.match(rb'exit (\d+) end (\d)', st)
    log = read_file(os.path.join(d, 'frames.log'))
    last = log.decode('latin1').rstrip('\n').split('\n')[-1] if log else None
    if not (m and int(m.group(1)) == 0) or last != 'end %08x' % PASSES:
        return ['%s: did not complete (status %r, last line %r)' % (tag, st.strip(), last)]
    why = []
    gl = (read_file(os.path.join(d, 'game.log')) or b'').decode('latin1')
    game = 'game-retail-%s' % tag[-1]
    pats = [(r'Loaded L"[^"]*\\%s\\+msvcrt\.dll" at \S+: builtin' % game, 'builtin msvcrt.dll'),
            (r'Loaded L"[^"]*\\%s\\+modules\\+framelock\.dll" at \S+: native' % game, 'FRAMELOCK'),
            (r'Loaded L"[^"]*\\%s\\+modules\\+sehfix\.dll" at \S+: native' % game, 'SEHFIX')]
    for pat, what in pats:
        if not re.search(pat, gl, re.I):
            why.append('%s: %s not seen loaded in game.log' % (tag, what))
    n = len({f[8] for f in blits(log) if int(f[7], 16)})
    if n < 20:
        why.append('%s: %d distinct frames, under 20' % (tag, n))
    if glob:
        g = os.path.join(d, 'global.bin')
        sz = os.path.getsize(g) if os.path.exists(g) else -1
        if sz != GLOBAL_BYTES:
            why.append('%s: global.bin is %d bytes, not %d' % (tag, sz, GLOBAL_BYTES))
    return why

def same(a, b):
    x = read_file(a)
    return x is not None and x == read_file(b)

def build(work, cases_tsv, header, out, full='0'):
    cases = [l.rstrip('\n').split('\t') for l in open(cases_tsv)]
    res = {}
    for id_, kind, actual, script in cases:
        d = os.path.join(work, 'runs', id_.replace('/', '_'))
        pre = read_file(os.path.join(d, 'PRECHECK_FAILED'))
        if pre is not None:
            res[id_] = ('FAILED', [pre.decode().strip()], d)
            continue
        why = []
        for t in 'ab':
            why += judge_run(os.path.join(d, 'out', 'retail-' + t), 'run ' + t, kind == 'global')
        if why:
            res[id_] = ('FAILED', why, d)
            continue
        ra, rb = (os.path.join(d, 'out', 'retail-' + t) for t in 'ab')
        eq = same(ra + '/frames.log', rb + '/frames.log') and (kind != 'global' or same(ra + '/global.bin', rb + '/global.bin'))
        res[id_] = ('match' if eq else 'NONDETERMINISTIC', [], d)
    delivery = {}
    for id_, kind, actual, script in cases:
        if kind != 'script':
            continue
        fid = 'flight/' + actual[:-4].upper()
        o, w, d = res[id_]
        fo = res[fid][0]
        if o == 'FAILED' or fo == 'FAILED':
            delivery[id_] = '-'
        elif o == 'match' and fo == 'match':
            if same(d + '/out/retail-a/frames.log', res[fid][2] + '/out/retail-a/frames.log'):
                res[id_] = ('FAILED', ['the script changed nothing: the log equals the unscripted flight'], d)
                delivery[id_] = '-'
            else:
                delivery[id_] = 'checked'
        else:
            delivery[id_] = 'delivery unchecked'
    for id_, (o, w, d) in sorted(res.items()):
        print('%-36s %s%s' % (id_, o, ''.join('\n    ' + x for x in w)))
    bad = any(r[0] == 'FAILED' for r in res.values())
    if full == '1' and (len(cases) != 131 or 'global/STGEORGE' not in res):
        print('references: FAILED: a full recording has %d cases or lacks global/STGEORGE, not 131' % len(cases), file=sys.stderr)
        bad = True
    if 'global/STGEORGE' in res and res['global/STGEORGE'][0] != 'match':
        print('references: global/STGEORGE is not a match: the recording fails', file=sys.stderr)
        bad = True
    if bad:
        print('references: FAILED, nothing published; work kept in %s' % work, file=sys.stderr)
        sys.exit(1)
    os.makedirs(out, exist_ok=True)
    lines = []
    for id_, kind, actual, script in cases:
        o, w, d = res[id_]
        kept = []
        for tag, sub in (('a', ''),) if o == 'match' else (('a', 'a/'), ('b', 'b/')):
            for n in ('frames.log', 'global.bin'):
                src = os.path.join(d, 'out', 'retail-' + tag, n)
                if os.path.exists(src):
                    dst = os.path.join(out, id_, sub + n)
                    os.makedirs(os.path.dirname(dst), exist_ok=True)
                    shutil.copyfile(src, dst)
                    kept.append('%s/%s%s=%s' % (id_, sub, n, sha(dst)))
        lines.append('\t'.join(['case', id_, o, actual[:-4].upper(), sha(script) if script != '-' else '-',
                                str(PASSES), delivery.get(id_, '-')] + kept))
    cfg = []
    os.makedirs(os.path.join(out, 'config'))
    for n in ('FLTSIM98.CFG', 'devices.cfg'):
        dst = os.path.join(out, 'config', n)
        shutil.copyfile(os.path.join(work, 'game', n), dst)
        cfg.append('file config/%s %s' % (n, sha(dst)))
    with open(os.path.join(out, 'manifest.txt'), 'w') as f:
        f.write(open(header).read())
        f.write('\n'.join(cfg) + '\ncases %d\n' % len(lines))
        f.write('\n'.join(lines) + '\n')
    verify(out)

def verify(top):
    man = os.path.join(top, 'manifest.txt')
    text = (read_file(man) or b'').decode().split('\n')
    if text[0] != 'references v1':
        print('references: %s: not a references v1 manifest' % man, file=sys.stderr); sys.exit(1)
    want = {}
    ids, rows, declared, commit = set(), 0, None, False
    problems = []
    for line in text[1:]:
        if line.startswith('commit '):
            commit = True
        if line.startswith('cases '):
            declared = line[6:].strip()
        if line.startswith('case\t'):
            f = line.split('\t')
            rows += 1
            if len(f) < 8 or f[1] in ids:
                problems.append('bad or repeated case row %r' % f[:2])
                continue
            ids.add(f[1])
            sub = ('',) if f[2] == 'match' else ('a/', 'b/') if f[2] == 'NONDETERMINISTIC' else None
            if sub is None:
                problems.append('%s: outcome %s' % (f[1], f[2]))
                continue
            need = {'%s/%sframes.log' % (f[1], x) for x in sub}
            if f[1].startswith('global/'):
                need |= {'%s/%sglobal.bin' % (f[1], x) for x in sub}
            if {k.rpartition('=')[0] for k in f[7:]} != need:
                problems.append('%s: kept files are not %s' % (f[1], sorted(need)))
        if line.startswith('file '):
            _, p, h = line.split(' ')
            want[p] = h
        elif line.startswith('case\t'):
            for k in line.split('\t')[7:]:
                p, _, h = k.rpartition('=')
                want[p] = h
    have = {os.path.relpath(p, top).replace(os.sep, '/') for p in files(top)} - {'manifest.txt'}
    if not commit:
        problems.append('no commit line')
    if rows == 0 or declared != str(rows):
        problems.append('cases line says %s, the manifest has %d case rows' % (declared, rows))
    problems += ['missing ' + p for p in sorted(set(want) - have)] + ['unlisted ' + p for p in sorted(have - set(want))]
    problems += ['hash differs ' + p for p in sorted(set(want) & have) if sha(os.path.join(top, p)) != want[p]]
    if problems:
        print('references: %s does not match its manifest:\n  %s' % (top, '\n  '.join(problems[:20])), file=sys.stderr)
        sys.exit(1)
    print('verified %d files against %s' % (len(want), man))

def skew(path):
    data = open(path, 'rb').read().decode('latin1').split('\n')
    for i, line in enumerate(data):
        f = line.split()
        if len(f) == 10 and f[0] not in ('end', 'fault', 'sim1') and re.fullmatch('[0-9a-f]+', f[0]) and int(f[7], 16):
            f[8] = f[8][:-1] + ('0' if f[8][-1] != '0' else '1')
            data[i] = ' '.join(f)
            open(path, 'wb').write('\n'.join(data).encode('latin1'))
            return
    sys.exit('no blit to skew')

def skewbin(path):
    with open(path, 'r+b') as f:
        f.seek(1000)
        b = f.read(1)
        f.seek(1000)
        f.write(bytes([b[0] ^ 0xff]))

def safe_out(out):
    out = os.path.realpath(out)
    for p in ('bin/references', 'game', 'cd', 'orig', 'src'):
        p = os.path.realpath(p)
        # a string prefix also covers bin/references.new, .old, .lock, .publishing and -work
        if out.startswith(p) or p.startswith(out + os.sep) or p == out:
            print('references: REFERENCES_OUT %s is, lies in or contains %s' % (out, p), file=sys.stderr)
            sys.exit(1)

cmd, a = sys.argv[1], sys.argv[2:]
if cmd == 'script-ok':
    r = script_ok(a[0])
    if r:
        print(r); sys.exit(1)
else:
    globals()[{'check-stage': 'check_stage', 'flights': 'flights', 'build': 'build', 'verify': 'verify',
               'skew': 'skew', 'skewbin': 'skewbin', 'safe-out': 'safe_out'}[cmd]](*a)
PY
py() { python3 -I -c "$PYLIB" "$@"; }

# ---- arguments ----------------------------------------------------------------------------
if [ "${1:-}" = --verify ]; then
    [ $# -eq 2 ] || usage "--verify DIRECTORY"
    py verify "$2"; exit $?
fi
cases_arg=("$@")
[ ${#cases_arg[@]} -gt 0 ] || read -r -a cases_arg <<<"${CASES:-}"
subset=0; [ ${#cases_arg[@]} -eq 0 ] || subset=1
# nothing inherited may change a run
for v in $(compgen -v | grep -E '^(HG2D_FRAMES_|FRAMELOCK_)'); do unset "$v"; done
export WINEDLLOVERRIDES=dsound=d
jobs=${REFERENCES_JOBS:-4}
base=${REFERENCES_DISPLAY_BASE:-120}

if [ $subset = 1 ]; then
    [ -n "${REFERENCES_OUT:-}" ] || usage "a subset needs REFERENCES_OUT (a directory that does not exist yet)"
    [ ! -e "$REFERENCES_OUT" ] || die "REFERENCES_OUT exists already: $REFERENCES_OUT"
    py safe-out "$REFERENCES_OUT" || exit 1
    final=$(realpath -m "$REFERENCES_OUT")
else
    [ -z "${REFERENCES_OUT:-}${REFERENCES_TEST_SKEW:-}${REFERENCES_TEST_TRUNCATE:-}" ] || usage "REFERENCES_OUT and the SKEW/TRUNCATE fixtures are for subsets only"
    mkdir -p bin
    exec 9>bin/references.lock
    flock -n 9 || die "bin/references.lock is held: another recording or publish is running"
    left=$(ls -d bin/references.publishing bin/references.old bin/references.new 2>/dev/null | tr '\n' ' ')
    [ -z "$left" ] || die "an earlier publish was interrupted, found: $left(inspect, then remove by hand)"
    if [ -n "$(ls -A bin/references 2>/dev/null)" ] && [ "${REFERENCES_FORCE:-}" != 1 ]; then
        die "bin/references/ is not empty: REFERENCES_FORCE=1 replaces it"
    fi
fi
[ -f bin/hg2d-diff/FRAMELOCK.DLL ] && [ -f bin/SEHFIX.DLL ] || usage "needs bin/hg2d-diff/FRAMELOCK.DLL and bin/SEHFIX.DLL (tools/ofs framelock sehfix)"
for c in Xvfb xdpyinfo wine git; do command -v $c >/dev/null || usage "needs $c"; done
git rev-parse HEAD >/dev/null 2>&1 || die "not a git checkout: the manifest needs the commit"

mkdir -p bin/references-work
work=$(mktemp -d "$root/bin/references-work/run.XXXXXX") || exit 1
echo "work: $work"
ok=0 publishing=0 cand_created= warm_pids= warm_prefix=
cleanup() {
    if [ -n "$warm_prefix" ]; then WINEPREFIX=$warm_prefix wineserver -k 2>/dev/null; fi
    if [ -n "$warm_pids" ]; then kill $warm_pids 2>/dev/null; fi
    if [ "$ok" = 1 ]; then rm -rf "$work"; fi
    # only a candidate this invocation created, and only until the renames start
    if [ "$publishing" = 0 ] && [ -n "$cand_created" ]; then rm -rf "$cand_created"; fi
}
trap cleanup EXIT

# ---- the staged game ----------------------------------------------------------------------
nice -n 19 python3 -B tools/stage_game.py --cd cd --out "$work/game" >"$work/stage.log" 2>&1 || { tail -5 "$work/stage.log" >&2; die "staging failed (log $work/stage.log)"; }
cp game/FLTSIM98.CFG game/devices.cfg "$work/game/" || die "no game/FLTSIM98.CFG and devices.cfg"
[ -z "${REFERENCES_TEST_PLANT:-}" ] || cp "$REFERENCES_TEST_PLANT" "$work/game/Modules/PLANT.DLL"
[ -z "${REFERENCES_TEST_EXE:-}" ] || cp "$REFERENCES_TEST_EXE" "$work/game/FLTSIM98.EXE"
device=$(awk '/^\[/{s=($0 ~ /^\[HARDWARE\]/)} s && /^Device=/{sub(/^Device=/,""); gsub(/\r/,""); print "set:" $0; exit}' "$work/game/FLTSIM98.CFG")
[ "$device" = "set:" ] || die "[HARDWARE] Device= is not empty (${device:-absent}): the software renderer is Device= with no value"
ref=$(sed -n "s/^REFERENCE_SHA256 = '\([0-9a-f]*\)'.*/\1/p" tools/gen_host_rt_tables.py)
[ ${#ref} = 64 ] || die "no REFERENCE_SHA256 in tools/gen_host_rt_tables.py"
py check-stage "$work/game" cd "$ref" >"$work/pressing.txt" || exit 1
mapfile -t flight_rows < <(py flights "$work/game")
[ ${#flight_rows[@]} -eq 74 ] || die "flight listing failed"
declare -A actual
for r in "${flight_rows[@]}"; do actual[${r%% *}]=${r#* }; done

# ---- the scenery index ----------------------------------------------------------------------
# A fresh install builds INDEX_FILE on its first run, which never ends on FRAMELOCK's virtual clock.
# Build it once in a copy, on plain wine, and put it in the stage: it is the same file retail builds
# every time, and every run of every case then starts from it.
warm=$work/warm display=:$((base + 9))
mkdir -p "$warm"
! xdpyinfo -display "$display" >/dev/null 2>&1 || die "display $display is in use"
cp -a "${WINEPREFIX:-$HOME/.wine}" "$warm/prefix" || die "wine prefix copy failed"
warm_prefix=$warm/prefix
rm -f "$warm/prefix/drive_c/FS98"
[ ! -e "$warm/prefix/drive_c/FS98" ] && [ ! -L "$warm/prefix/drive_c/FS98" ] || die "prefix copy still has drive_c/FS98"
cp -a "$work/game" "$warm/game" || die "copy failed"
Xvfb "$display" -screen 0 1024x768x24 -ac -noreset >"$warm/xvfb.log" 2>&1 &
xvfb=$!
warm_pids=$xvfb
for _ in $(seq 20); do xdpyinfo -display "$display" >/dev/null 2>&1 && break; sleep 0.5; done
xdpyinfo -display "$display" >/dev/null 2>&1 && kill -0 $xvfb 2>/dev/null || die "Xvfb did not start on $display"
( cd "$warm/game" && DISPLAY=$display WINEPREFIX=$warm/prefix WINEDEBUG=-all timeout 120 wine FLTSIM98.EXE '/STN:Situatio\PARIS.STN' >"$warm/game.log" 2>&1 ) &
wine_pid=$!
warm_pids="$xvfb $wine_pid"
stable=0 last=
for _ in $(seq 100); do
    now=$(stat -c %s "$warm/game/INDEX_FILE" 2>/dev/null || echo none)
    if [ "$now" != none ] && [ "$now" = "$last" ]; then stable=$((stable + 1)); else stable=0; fi
    last=$now
    [ $stable -ge 10 ] && break
    kill -0 $wine_pid 2>/dev/null || break
    sleep 1
done
alive=0; kill -0 $wine_pid 2>/dev/null && alive=1
WINEPREFIX=$warm/prefix wineserver -k 2>/dev/null
kill $wine_pid $xvfb 2>/dev/null
wait $wine_pid 2>/dev/null
warm_pids= warm_prefix=
[ $stable -ge 10 ] && [ $alive = 1 ] || die "the first run did not build the scenery index (see $warm/game.log)"
cp "$warm/game/INDEX_FILE" "$work/game/INDEX_FILE" || die "no INDEX_FILE"
rm -rf "$warm"

# ---- cases --------------------------------------------------------------------------------
all=()
for f in "${!actual[@]}"; do all+=("flight/$f"); done
for s in $SCENARIOS; do for f in $SCRIPT_FLIGHTS; do all+=("script/$s/$f"); done; done
all+=(global/STGEORGE)
[ ${#all[@]} -eq 131 ] || die "case table has ${#all[@]} entries, not 131"
declare -A known; for c in "${all[@]}"; do known[$c]=1; done
for f in $SCRIPT_FLIGHTS STGEORGE; do [ -n "${actual[$f]:-}" ] || die "no flight $f in the stage"; done
if [ $subset = 1 ]; then
    want=()
    for c in "${cases_arg[@]}"; do
        [ -n "${known[$c]:-}" ] || usage "unknown case: $c"
        want+=("$c")
        case $c in script/*/*) want+=("flight/${c##*/}") ;; esac
    done
else
    want=("${all[@]}")
fi
mapfile -t want < <(printf '%s\n' "${want[@]}" | sort -u)
nice -n 19 python3 -B tools/fly_scripts.py "$work/scripts" || die "fly_scripts.py failed"
: >"$work/cases.tsv"
for c in "${want[@]}"; do
    kind=${c%%/*}; name=${c##*/}; script=-
    [ "$kind" != script ] || { sc=${c#script/}; script=$work/scripts/${sc%%/*}.txt; }
    printf '%s\t%s\t%s\t%s\n' "$c" "$kind" "${actual[$name]}" "$script" >>"$work/cases.tsv"
done

# ---- the header ---------------------------------------------------------------------------
dirty=; [ -z "$(git status --porcelain --untracked-files=no)" ] || dirty=" +dirty"
{
    echo "references v1"
    echo "commit $(git rev-parse HEAD)$dirty"
    echo "passes $PASSES"
    sed 's/^/pressing /' "$work/pressing.txt"
    echo "pressing index_sha256 $(sha256sum <"$work/game/INDEX_FILE" | cut -d' ' -f1) (the scenery index a first run builds)"
    echo "config wine $(wine --version)"
    echo "config WINEDLLOVERRIDES $WINEDLLOVERRIDES"
    echo "config msvcrt wine's builtin, not the CD's 1997 MSVCRT.DLL: host +0x21c memmove leaves 0 in ecx, not 7; Epic 3 must allow for this at ANA-1 (docs/retail-bugs.md)"
    echo "config renderer software (Device= empty), no sound (dsound=d)"
    echo "config sha256 FLTSIM98.CFG $(sha256sum <"$work/game/FLTSIM98.CFG" | cut -d' ' -f1)"
    echo "config sha256 devices.cfg $(sha256sum <"$work/game/devices.cfg" | cut -d' ' -f1)"
    echo "config sha256 FRAMELOCK.DLL $(sha256sum <bin/hg2d-diff/FRAMELOCK.DLL | cut -d' ' -f1)"
    echo "config sha256 SEHFIX.DLL $(sha256sum <bin/SEHFIX.DLL | cut -d' ' -f1)"
    for s in $(cut -f4 "$work/cases.tsv" | grep -v '^-$' | sort -u); do
        echo "config sha256 script/$(basename "$s") $(sha256sum <"$s" | cut -d' ' -f1)"
    done
} >"$work/header.txt"

# ---- the runs -----------------------------------------------------------------------------
export work PASSES base
one() {     # case
    local id=$1 line kind actual script dir display why
    line=$(awk -F'\t' -v i="$id" '$1 == i' "$work/cases.tsv")
    IFS=$'\t' read -r _ kind actual script <<<"$line"
    dir=$work/runs/${id//\//_}
    mkdir -p "$dir"
    local args=("Situatio\\$actual" "$PASSES")
    if [ "$script" != - ]; then
        why=$(py script-ok "$script") || { echo "$why" >"$dir/PRECHECK_FAILED"; echo "$id: FAILED ($why)"; return 0; }
        args+=("$script")
    fi
    display=:$((base + ${SLOT:-0}))
    HG2D_FRAMES_RETAIL_ONLY=1 HG2D_FRAMES_OUT="$dir/out" HG2D_FRAMES_DISPLAY=$display FS98_DIR="$work/game" \
        HG2D_FRAMES_GLOBAL=$([ "$kind" = global ] && echo 1) tools/hg2d_frames.sh "${args[@]}" >"$dir/hg2d.txt" 2>&1
    if [ ! -f "$dir/out/retail-b/status" ]; then
        echo "hg2d_frames.sh stopped early: $(tail -1 "$dir/hg2d.txt")" >"$dir/PRECHECK_FAILED"
    else
        if [ "${REFERENCES_TEST_SKEW:-}" = global ]; then
            [ "$kind" != global ] || py skewbin "$dir/out/retail-b/global.bin"
        elif [ -n "${REFERENCES_TEST_SKEW:-}" ]; then
            py skew "$dir/out/retail-b/frames.log"
        fi
        [ -z "${REFERENCES_TEST_TRUNCATE:-}" ] || sed -i '$d' "$dir/out/retail-b/frames.log"
    fi
    echo "ran $id"
}
export -f one py; export PYLIB
export REFERENCES_TEST_SKEW="${REFERENCES_TEST_SKEW:-}" REFERENCES_TEST_TRUNCATE="${REFERENCES_TEST_TRUNCATE:-}"
cut -f1 "$work/cases.tsv" | xargs -P "$jobs" --process-slot-var=SLOT -I{} bash -c 'one "$1"' _ {}

# ---- classify and publish -----------------------------------------------------------------
if [ $subset = 1 ]; then
    mkdir -p "$(dirname "$final")" || die "cannot create $(dirname "$final")"
    cand=$(mktemp -d "$final.XXXXXX") || die "cannot create a candidate beside $final"
    cand_created=$cand
    chmod 755 "$cand"
else
    cand=$root/bin/references.new
    [ ! -e "$cand" ] || die "$cand exists"
    cand_created=$cand
fi
py build "$work" "$work/cases.tsv" "$work/header.txt" "$cand" $((1 - subset)) || exit 1
if [ $subset = 1 ]; then
    mv "$cand" "$final" || die "could not move $cand to $final"
    ok=1
    echo "published $final (subset: ${#want[@]} cases of $PASSES passes; tools/references.txt untouched)"
    exit 0
fi
touch bin/references.publishing || die "cannot create bin/references.publishing: nothing renamed"
publishing=1
if [ -e bin/references ]; then mv bin/references bin/references.old || die "rename failed"; fi
mv bin/references.new bin/references || die "rename failed, bin/references.old holds the old set"
cp bin/references/manifest.txt tools/references.txt.tmp && mv tools/references.txt.tmp tools/references.txt \
    && cmp -s bin/references/manifest.txt tools/references.txt || die "mirror differs from the manifest; bin/references.publishing left in place"
rm -rf bin/references.old || die "cannot remove bin/references.old; bin/references.publishing left in place"
rm -f bin/references.publishing || die "cannot remove bin/references.publishing"
ok=1
echo "published bin/references and tools/references.txt"
