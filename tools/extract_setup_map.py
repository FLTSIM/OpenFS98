#!/usr/bin/env python3
"""Recover the CD-to-install file map from FS98's Setup.exe.

The CD layout is not the installed layout: setup renames files as it copies,
and FS98 then opens them by the installed name (aircraft.cfg's `sim=X` becomes
AIRCRAFT\\<dir>\\X.AIR, model.cfg's `normal=Y` becomes Y.MDL, panel.cfg's
`gauge00=Cessna_182.Suction` wants GAUGES\\CESSNA_182.SUCTION.GAU, and so on).
Without the renames the sim starts and then fails with "Aircraft
initialization failure" or "Visual model could not be displayed".

Setup.exe carries the table as plain ASCII: a source path beginning "fs98\\"
immediately followed by its destination path, in file order.  That is what this
reads.  It beats running the installer -- no GUI, reproducible, and the result
is diffable.

Usage: tools/extract_setup_map.py [path/to/Setup.exe] > docs/setup_map.tsv
"""
import re, sys

src = sys.argv[1] if len(sys.argv) > 1 else 'cd/Setup.exe'
blob = open(src, 'rb').read()
strings = [m.group().decode('latin1')
           for m in re.finditer(rb'[\x20-\x7e]{3,}', blob)]

rows = []
for i, s in enumerate(strings):
    if s.lower().startswith('fs98\\') and i + 1 < len(strings):
        dest = strings[i + 1]
        # a destination is a path or bare filename, never another source
        if dest.lower().startswith('fs98\\'):
            continue
        rows.append((s[len('fs98\\'):], dest))

print('# source (under fs98/ on the CD)\tdestination (under the install dir)')
for s, d in rows:
    print(f'{s}\t{d}')
