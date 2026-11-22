#!/usr/bin/env python3
"""Build-only Windows resources and the SIMSUI ordinal anchor for MP_DIALOG.

Looks the resources up by type and id and the SIMSUI export by name; reads no code and
needs no particular pressing. The vtable and message-map tables are committed source
(src/mpdialog/mpdialog_runtime_tables.inc), once written by this script.
"""
from pathlib import Path
import struct

from dialogmap import Fs98Pe, SurveyError


def resource_name(name):
    if isinstance(name, int):
        return struct.pack('<HH', 0xffff, name)
    return name.encode('utf-16-le') + b'\0\0'


def make_resource(pe, resource):
    resources = bytearray(struct.pack('<IIHHHHIHHII', 0, 32, 0xffff, 0,
                                      0xffff, 0, 0, 0, 0, 0, 0))
    names = resource_name(resource['type']) + resource_name(resource['id'])
    names += b'\0' * (-len(names) % 4)
    resources += struct.pack('<II', resource['size'], 8 + len(names) + 16)
    resources += names + struct.pack('<IHHII', 0, 0x1030,
                                     resource['language'], 0, 0)
    resources += pe.read_bytes(pe.file_offset(resource['rva'], resource['size']),
                               resource['size'])
    resources += b'\0' * (-len(resources) % 4)
    return resources


def combine_resources(resources):
    if not resources:
        raise SurveyError('cannot emit an empty MP_DIALOG support resource')
    combined = bytearray(resources[0][:32])
    for resource in resources:
        combined += resource[32:]
    return bytes(combined)


def simsui_anchor(ui):
    export_rva, _ = ui.directory(0)
    export = ui.file_offset(export_rva, 40)
    base = ui.read_uint32(export + 16)
    name_count = ui.read_uint32(export + 24)
    names_rva = ui.read_uint32(export + 32)
    ordinals_rva = ui.read_uint32(export + 36)
    for index in range(name_count):
        name_rva = ui.read_uint32(ui.file_offset(names_rva + index * 4, 4))
        if ui.c_string(name_rva) == '?ui_new@@YAPAXI@Z':
            return base + ui.read_uint16(ui.file_offset(ordinals_rva + index * 2, 2))
    raise SurveyError('SIMSUI ui_new export missing')


def main():
    pe = Fs98Pe('orig/English/mpdialog.dll')
    out_dir = Path('bin/mpdialog-diff')
    out_dir.mkdir(parents=True, exist_ok=True)
    ui = Fs98Pe('orig/SIMSUI.DLL')
    ordinal = simsui_anchor(ui)
    (out_dir / 'simsui-anchor.def').write_text(
        f'LIBRARY SIMSUI.DLL\nEXPORTS\n    fs98_mpdialog_simsui_anchor @{ordinal} NONAME\n')
    for resource_id in (0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b):
        template = next((item for item in pe.resources()
                         if item['type'] == 5 and item['id'] == resource_id), None)
        if template is None:
            raise SurveyError(f'MP_DIALOG RT_DIALOG {resource_id:#x} missing')
        suffix = {0x65: '110a-intro', 0x66: '110a-choice', 0x67: '110a-host',
                  0x68: '110a-connection', 0x69: '110a-browser',
                  0x6a: '110b', 0x6b: '110c'}[resource_id]
        (out_dir / f'mpdialog-0x{suffix}.res').write_bytes(make_resource(pe, template))
    support = [item for item in pe.resources() if item['type'] in (2, 6, 16)]
    if not support:
        raise SurveyError('MP_DIALOG bitmap/string/version support resources missing')
    (out_dir / 'mpdialog-flow-support.res').write_bytes(
        combine_resources([make_resource(pe, item) for item in support]))


if __name__ == '__main__':
    main()
