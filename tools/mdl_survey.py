#!/usr/bin/env python3
"""Read MDL boundaries and fingerprints; does not load or modify the DLLs.

Usage: python3 tools/mdl_survey.py [orig/Aircraft]
Addresses in the output are RVAs unless named image_base.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from impscan import PE, u32


def survey(path):
    pe = PE(str(path))
    exports = pe.exports()
    linkage = pe.off(exports['Linkage'])
    slots = struct.unpack_from('<9I', pe.b, linkage)
    if slots[0] != 31:
        raise ValueError(f'{path}: expected module 31')
    imports = []
    cursor = pe.off(exports['ImportTable'])
    while u32(pe.b, cursor):
        imports.append(u32(pe.b, cursor))
        cursor += 8
    # Verify the attach thunk rather than assuming all models share a header RVA.
    attach = pe.va2off(slots[6])
    if (pe.b[attach:attach + 6] != bytes.fromhex('8b442404c700') or
            pe.b[attach + 10:attach + 13] != bytes.fromhex('c20400')):
        raise ValueError(f'{path}: unknown attach thunk')
    header_va = u32(pe.b, attach + 6)
    header = struct.unpack_from('<9I', pe.b, pe.va2off(header_va))
    sections = {}
    section_table = u32(pe.b, 0x3c) + 24
    section_table += struct.unpack_from('<H', pe.b, u32(pe.b, 0x3c) + 20)[0]
    for index, (size, rva, raw_size, raw_offset) in enumerate(pe.secs):
        name = pe.b[section_table + index * 40:section_table + index * 40 + 8]
        name = name.rstrip(b'\0').decode('ascii')
        sections[name] = dict(size=size, rva=rva)
    return dict(
        path=str(path), sha256=hashlib.sha256(pe.b).hexdigest(),
        image_base=hex(pe.base), sections=sections, imports=imports,
        linkage={hex(index * 4): hex(value - pe.base) if value else None
                 for index, value in enumerate(slots) if index},
        header_rva=hex(header_va - pe.base),
        header_words=[hex(value) for value in header],
        update_rva=hex(header[4] - pe.base) if header[4] else None,
        state_size=header[6], signature=struct.pack('<I', header[7]).decode('ascii'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', nargs='?', type=Path, default=Path('orig/Aircraft'))
    args = parser.parse_args()
    paths = sorted(path for path in args.root.rglob('*')
                   if path.is_file() and path.suffix.lower() == '.mdl')
    if not paths:
        parser.error(f'no MDLs in {args.root}')
    print(json.dumps([survey(path) for path in paths], indent=2))


if __name__ == '__main__':
    main()
