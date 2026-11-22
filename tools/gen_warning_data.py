#!/usr/bin/env python3
"""Extract the test gauges' bitmaps into ignored build outputs (bin/gauges-diff/<name>.res).

The data image and pointer list are committed C (src/gauges/*_data.inc, *_pointers.inc);
this only re-asserts the retail layout they were decompiled from. Fail closed on an
unexpected image, export, import, pointer or bitmap.
"""
from pathlib import Path
import hashlib
import struct

from dialogmap import Fs98Pe, SurveyError


def extract(name, path, base, data_size, imports, linkage, functions, pointer_count, bitmap_layout):
    pe = Fs98Pe(path)
    if pe.uiBase != base or pe.exports() != {
            'ImportTable': 0x3000 + imports, 'Linkage': 0x3000 + linkage}:
        raise SurveyError(f'reference {name} gauge layout changed')
    section = next(section for section in pe.aSections if section['name'] == '.data')
    if section['rva'] != 0x3000 or section['virtual_size'] != data_size:
        raise SurveyError(f'reference {name} gauge data moved or resized')
    data = pe.read_bytes(pe.file_offset(0x3000, data_size), data_size)
    if struct.unpack_from('<IIII', data, imports) != (15, 0, 0, 0):
        raise SurveyError('expected only PANELS import')
    reloc_rva, reloc_size = pe.directory(5)
    cursor = pe.file_offset(reloc_rva, reloc_size)
    end = cursor + reloc_size
    pointers = 0
    while cursor < end:
        page, size = struct.unpack('<II', pe.read_bytes(cursor, 8))
        if size < 8 or size % 2 or cursor + size > end:
            raise SurveyError('invalid relocation block')
        for offset in range(cursor + 8, cursor + size, 2):
            entry = pe.read_uint16(offset)
            if entry >> 12 == 0:
                continue
            if entry >> 12 != 3:
                raise SurveyError('unexpected relocation type')
            rva = page + (entry & 0xfff)
            if not 0x3000 <= rva < 0x3000 + data_size:
                continue
            target = struct.unpack_from('<I', data, rva - 0x3000)[0] - pe.uiBase
            if target not in functions and not 0x3000 <= target < 0x3000 + data_size:
                raise SurveyError(f'unrecognised data pointer {target:#x}')
            pointers += 1
        cursor += size
    if pointers != pointer_count:
        raise SurveyError(f'expected {pointer_count} data pointers')
    directory = Path('bin/gauges-diff')
    directory.mkdir(parents=True, exist_ok=True)
    # Standard Win32 .res stream, as in gen_alerts_runtime.py.
    resources = bytearray(struct.pack('<IIHHHHIHHII', 0, 32, 0xffff, 0, 0xffff, 0,
                                      0, 0, 0, 0, 0))
    bitmaps = [resource for resource in pe.resources() if resource['type'] == 2]
    if [(resource['id'], resource['language'], resource['size']) for resource in bitmaps] != bitmap_layout:
        raise SurveyError(f'reference {name} bitmaps changed')
    for resource in bitmaps:
        resources += struct.pack('<IIHHHHIHHII', resource['size'], 32,
                                 0xffff, 2, 0xffff, resource['id'], 0,
                                 0x1030, resource['language'], 0, 0)
        resources += pe.read_bytes(pe.file_offset(resource['rva'], resource['size']),
                                   resource['size'])
        resources += bytes(-len(resources) % 4)
    (directory / f'{name}.res').write_bytes(resources)
    print(f'{name}: {data_size} data bytes, {pointers} pointers, {len(bitmaps)} bitmaps; retail SHA-256 ' +
          hashlib.sha256(pe.byData).hexdigest())


def main():
    extract('warning', 'orig/Gauges/737WARNG.GAU', 0x20d20000, 0xfc, 0xc8, 0xd8, {
        0x1020: 'warning_install', 0x1040: 'warning_unmark',
        0x1050: 'warning_prepare', 0x1060: 'warning_update',
        0x1080: 'warning_draw', 0x1090: 'warning_release',
        0x10a0: 'warning_noop', 0x10b0: 'warning_failures',
    }, 14, [(0x1000, 0x409, 0x533e)])


if __name__ == '__main__':
    main()
