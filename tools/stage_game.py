#!/usr/bin/env python3
r"""Build a runnable FS98 install in game/ from the CD, without running Setup.exe.

The CD layout is not the installed layout.  Setup renames 263 files (and three
directories) as it copies, and FS98 opens them by their *installed* names:
aircraft.cfg's `sim=X` becomes AIRCRAFT\<dir>\X.AIR, model.cfg's `normal=Y`
becomes Y.MDL, and panel.cfg's `gauge00=Cessna_182.Suction` wants
GAUGES\CESSNA_182.SUCTION.GAU.  Miss the renames and the sim starts and then
dies -- "Aircraft initialization failure", then "Visual model could not be
displayed", and onward through panels and gauges.

The map is read out of Setup.exe by tools/extract_setup_map.py, so this applies
the installer's own table rather than a reconstruction of it.

Paths in that table vary in case (AIRCRAFT, Aircraft and aircraft all appear)
and Linux is case-sensitive, so lookups resolve case-insensitively.

Usage: tools/stage_game.py [--cd DIR] [--map FILE] [--out DIR]
"""
import argparse, os, re, shutil, sys

ap = argparse.ArgumentParser()
ap.add_argument('--cd',  default='cd')
ap.add_argument('--map', default='docs/setup_map.tsv')
ap.add_argument('--out', default='game')
args = ap.parse_args()

CD, OUT = os.path.join(args.cd, 'fs98'), args.out
if not os.path.isdir(CD):
    sys.exit('no %s -- extract the ISO there first: 7z x ISO -o%s' % (CD, args.cd))


def split_win(relwin):
    return [p for p in relwin.replace('\\', '/').split('/') if p]


def resolve(root, relwin):
    """Existing path under root matching a Windows-style path, ignoring case."""
    cur = root
    for part in split_win(relwin):
        try:
            hit = next((e for e in os.listdir(cur) if e.lower() == part.lower()), None)
        except OSError:
            return None
        if hit is None:
            return None
        cur = os.path.join(cur, hit)
    return cur


def dest_path(root, relwin):
    """Destination path, reusing existing directories whose case differs."""
    parts = split_win(relwin)
    cur = root
    for part in parts[:-1]:
        hit = None
        if os.path.isdir(cur):
            hit = next((e for e in os.listdir(cur) if e.lower() == part.lower()), None)
        cur = os.path.join(cur, hit or part)
        os.makedirs(cur, exist_ok=True)
    return os.path.join(cur, parts[-1])


print('copying CD tree ...')
if os.path.isdir(OUT):
    shutil.rmtree(OUT)
shutil.copytree(CD, OUT)

print('applying setup renames ...')
moved = missing = 0
with open(args.map) as f:
    for line in f:
        if line.startswith('#') or '\t' not in line:
            continue
        s, d = line.rstrip('\n').split('\t', 1)
        src = resolve(OUT, s)
        if src is None or not os.path.isfile(src):
            missing += 1
            continue
        dst = dest_path(OUT, d)
        if os.path.abspath(src) != os.path.abspath(dst):
            shutil.move(src, dst)
            moved += 1

# prune directories setup empties out by renaming their contents elsewhere
for dirpath, dirnames, files in os.walk(OUT, topdown=False):
    if not files and not dirnames and os.path.abspath(dirpath) != os.path.abspath(OUT):
        os.rmdir(dirpath)

for dll in ('mfc42.dll', 'MSVCRT.DLL'):
    src = resolve(args.cd, dll)
    if src and not resolve(OUT, os.path.basename(src)):
        shutil.copy2(src, os.path.join(OUT, os.path.basename(src)))

# The intro is Indeo 4.  Wine has no codec for it, and the MCI failure is a
# *modal* dialog that blocks startup before the sim is exercised at all, which
# makes unattended smoke tests impossible.  The CD carries an Indeo4 installer
# if you want the logo back.
avi = resolve(OUT, 'mslogo.avi')
if avi:
    os.rename(avi, avi + '.disabled')

print('  renamed %d files; %d map entries had no source on the CD' % (moved, missing))

# ---- verify the lookups that actually broke the sim -------------------------
bad = []
acdir = resolve(OUT, 'Aircraft')
for d in sorted(os.listdir(acdir)):
    adir = os.path.join(acdir, d)
    if not os.path.isdir(adir):
        continue
    cfg = resolve(adir, 'aircraft.cfg')
    if cfg:
        txt = open(cfg, encoding='latin1').read()
        for sim in re.findall(r'(?im)^\s*sim\s*=\s*(\S+)', txt):
            if not resolve(adir, sim + '.AIR'):
                bad.append('%s: sim=%s -> no %s.AIR' % (d, sim, sim))
    for sub in sorted(os.listdir(adir)):
        sdir = os.path.join(adir, sub)
        if not os.path.isdir(sdir):
            continue
        mcfg = resolve(sdir, 'model.cfg')
        if mcfg:
            mtxt = open(mcfg, encoding='latin1').read()
            for mdl in re.findall(r'(?im)^\s*(?:normal|crash)\s*=\s*(\S+)', mtxt):
                if not resolve(sdir, mdl + '.MDL'):
                    bad.append('%s/%s: %s -> no %s.MDL' % (d, sub, mdl, mdl))
        pcfg = resolve(sdir, 'panel.cfg')
        if pcfg:
            ptxt = open(pcfg, encoding='latin1').read()
            for g in re.findall(r'(?im)^\s*gauge\d+\s*=\s*([^,\r\n]+)', ptxt):
                g = g.strip()
                if not resolve(OUT, 'Gauges/' + g + '.GAU'):
                    bad.append('%s/%s: gauge %s -> no .GAU' % (d, sub, g))

if bad:
    print('FAILED: %d unresolved references' % len(bad))
    for b in bad[:15]:
        print('   ' + b)
    sys.exit(1)
print('OK: every sim=, model and gauge reference resolves')
