#!/usr/bin/env python3
"""Build-only Windows resources and the SIMSUI ordinal anchor for Flights.

    python3 -B tools/gen_flights_runtime.py

Looks the resources up by type and id and the SIMSUI export by name; reads no code and
needs no particular pressing. The vtable and message-map tables are committed source
(src/flights/flights_runtime_tables.inc), once written by this script. Nothing retail is written to
tracked source; the .res and the anchor go under bin/flights-diff/.
"""
from pathlib import Path
import struct

from dialogmap import Fs98Pe, SurveyError, resource_name


def main():
    pe = Fs98Pe('orig/English/Flights.DLL')
    directory = Path('bin/flights-diff')
    directory.mkdir(parents=True, exist_ok=True)
    # A PE import by ordinal makes the loader resolve SIMSUI and its MFC before our DllMain.
    ui = Fs98Pe('orig/SIMSUI.DLL')
    export = ui.file_offset(ui.directory(0)[0], 40)
    base = ui.read_uint32(export + 16)
    names_rva = ui.read_uint32(export + 32)
    ordinals_rva = ui.read_uint32(export + 36)
    anchor_ordinal = None
    for index in range(ui.read_uint32(export + 24)):
        name_rva = ui.read_uint32(ui.file_offset(names_rva + index * 4, 4))
        if ui.c_string(name_rva) == '?ui_new@@YAPAXI@Z':
            anchor_ordinal = base + ui.read_uint16(ui.file_offset(ordinals_rva + index * 2, 2))
    if anchor_ordinal is None:
        raise SurveyError('SIMSUI ui_new export missing')
    (directory / 'simsui-anchor.def').write_text(
        f'LIBRARY SIMSUI.DLL\nEXPORTS\n    fs98_flights_simsui_anchor @{anchor_ordinal} NONAME\n')

    resources = bytearray(struct.pack('<IIHHHHIHHII', 0, 32, 0xffff, 0, 0xffff, 0, 0, 0, 0, 0, 0))
    for resource in pe.resources():
        if resource['type'] == 16:
            continue  # Do not copy the retail company's version resource.
        resource_names = resource_name(resource['type']) + resource_name(resource['id'])
        resource_names += b'\0' * (-len(resource_names) % 4)
        header = struct.pack('<II', resource['size'], 8 + len(resource_names) + 16)
        header += resource_names + struct.pack('<IHHII', 0, 0x1030, resource['language'], 0, 0)
        resources += header
        resources += pe.read_bytes(pe.file_offset(resource['rva'], resource['size']), resource['size'])
        resources += b'\0' * (-len(resources) % 4)
    (directory / 'flights.res').write_bytes(resources)


if __name__ == '__main__':
    main()
