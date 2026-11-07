#!/usr/bin/env python3
"""Install or restore SIM1 in a staged FS98 tree; preserve orig/ as reference."""
import argparse
import hashlib
from pathlib import Path
import shutil
import struct

from dialogmap import Fs98Pe
from gen_sim1_data import SHA256

ROOT = Path(__file__).resolve().parents[1]


def validate(path, retail=False):
    content = path.read_bytes()
    if retail and hashlib.sha256(content).hexdigest() != SHA256:
        raise ValueError('SIM1 reference hash differs')
    pe = Fs98Pe(path)
    exports = pe.exports()
    if set(exports) != {'ImportTable', 'Linkage'}:
        raise ValueError('production SIM1 must have exactly its two module exports')
    if exports['Linkage'] - exports['ImportTable'] != 0x58:
        raise ValueError('SIM1 export layout differs')
    if pe.read_uint32(pe.file_offset(exports['Linkage'], 4)) != 25:
        raise ValueError('SIM1 module identity differs')
    imports = pe.read_bytes(pe.file_offset(exports['ImportTable'], 0x58), 0x58)
    if list(struct.unpack('<22I', imports)) != [v for module in (1, 2, 23, 4, 16, 3, 32, 39, 20, 10, 0) for v in (module, 0)]:
        raise ValueError('SIM1 dependency table differs')
    if not retail:
        def resources(image):
            return {(r['type'], r['id'], r['language']): image.read_bytes(
                image.file_offset(r['rva'], r['size']), r['size']) for r in image.resources()}
        if resources(pe) != resources(Fs98Pe(ROOT / 'orig/Sim/SIM1.sim')):
            raise ValueError('SIM1 version resources differ')
    return content


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('--retail', action='store_true')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    reference = ROOT / 'orig/Sim/SIM1.sim'
    validate(reference, retail=True)
    source = reference if args.retail else ROOT / 'bin/sim1-diff/SIM1_COMPLETE.DLL'
    content = validate(source, retail=args.retail)
    directory = args.game / 'Sim'
    if not directory.is_dir() or not (args.game / 'FLTSIM98.EXE').is_file():
        raise ValueError('expected a staged FS98 game with Sim/ and FLTSIM98.EXE')
    matches = [path for path in directory.iterdir() if path.name.lower() == 'sim1.sim']
    if len(matches) != 1:
        raise ValueError('expected exactly one installed Sim/SIM1.sim')
    destination = matches[0]
    if not args.check and destination.read_bytes() != content:
        temporary = destination.with_name(destination.name + '.openfs98')
        shutil.copyfile(source, temporary)
        temporary.replace(destination)
    if destination.read_bytes() != content:
        raise ValueError('installed SIM1 does not match selected build')
    print(f'{"Verified" if args.check else "Restored" if args.retail else "Installed"} {destination.resolve()} '
          f'(module 25, two exports, sha256 {hashlib.sha256(content).hexdigest()})')


if __name__ == '__main__':
    main()
