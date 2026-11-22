#!/usr/bin/env python3
"""Build-only World resources from retail: bin/world-diff/world.res (every resource but type 16)
and simsui-anchor.def (SIMSUI's ui_new ordinal). The native-binding tables it once wrote are
committed in src/world/world_runtime_tables.inc."""
from pathlib import Path
import struct
from dialogmap import Fs98Pe, SurveyError, resource_name


def main():
    pe = Fs98Pe('orig/English/World.DLL')
    output_dir = Path('bin/world-diff')
    output_dir.mkdir(parents=True, exist_ok=True)
    ui = Fs98Pe('orig/SIMSUI.DLL')
    export = ui.file_offset(ui.directory(0)[0], 40)
    base = ui.read_uint32(export + 16)
    names, ordinals = ui.read_uint32(export + 32), ui.read_uint32(export + 36)
    for index in range(ui.read_uint32(export + 24)):
        name = ui.read_uint32(ui.file_offset(names + index * 4, 4))
        if ui.c_string(name) == '?ui_new@@YAPAXI@Z':
            ordinal = base + ui.read_uint16(ui.file_offset(ordinals + index * 2, 2))
            break
    else:
        raise SurveyError('SIMSUI anchor missing')
    (output_dir / 'simsui-anchor.def').write_text(f'LIBRARY SIMSUI.DLL\nEXPORTS\n    fs98_world_simsui_anchor @{ordinal} NONAME\n')
    resource_bytes = bytearray(struct.pack('<IIHHHHIHHII', 0, 32, 0xffff, 0, 0xffff, 0, 0, 0, 0, 0, 0))
    for resource in pe.resources():
        if resource['type'] == 16:
            continue
        encoded_names = resource_name(resource['type']) + resource_name(resource['id'])
        encoded_names += b'\0' * (-len(encoded_names) % 4)
        resource_bytes += struct.pack('<II', resource['size'], 8 + len(encoded_names) + 16)
        resource_bytes += encoded_names + struct.pack('<IHHII', 0, 0x1030, resource['language'], 0, 0)
        resource_bytes += pe.read_bytes(pe.file_offset(resource['rva'], resource['size']), resource['size'])
        resource_bytes += b'\0' * (-len(resource_bytes) % 4)
    (output_dir / 'world.res').write_bytes(resource_bytes)


if __name__ == '__main__':
    main()
