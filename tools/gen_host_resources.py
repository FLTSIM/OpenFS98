#!/usr/bin/env python3
"""FLTSIM98.EXE's resources (dialogs, menus, icons, cursors, bitmaps, strings, accelerators,
version) as a .res file, for the executables built from src/host/.

    python3 -B tools/gen_host_resources.py [OUT=bin/host-rt/host.res]

Only the .rsrc tree is read; no code. (The reference-EXE sha256 pin is in gen_host_rt_tables.py, run by tools/ofs host-rt-tables-check.)
"""
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dialogmap import Fs98Pe  # noqa: E402

REFERENCE = 'orig/FLTSIM98.EXE'


def resources(pe):
    """Every resource (type, id and language all numeric) as .res file bytes."""
    out = bytearray(struct.pack('<IIHHHHIHHII', 0, 32, 0xffff, 0, 0xffff, 0, 0, 0, 0, 0, 0))
    for resource in pe.resources():
        if not all(isinstance(resource[key], int) for key in ('type', 'id', 'language')):
            raise ValueError('named resource')
        out += struct.pack('<IIHHHHIHHII', resource['size'], 32, 0xffff, resource['type'], 0xffff, resource['id'], 0,
                           0x1030, resource['language'], 0, 0)
        out += pe.read_bytes(pe.file_offset(resource['rva'], resource['size']), resource['size'])
        out += bytes(-len(out) % 4)
    return bytes(out)


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else 'bin/host-rt/host.res')
    path = Path(REFERENCE)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(resources(Fs98Pe(path)))
    print(f'{out}: {out.stat().st_size} bytes')


if __name__ == '__main__':
    main()
