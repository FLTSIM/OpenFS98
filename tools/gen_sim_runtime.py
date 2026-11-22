#!/usr/bin/env python3
"""Build-only Windows resources and the SIMSUI ordinal anchor for Sim.

    python3 -B tools/gen_sim_runtime.py

Looks the resources up by type and id and the SIMSUI export by name; reads no code and
needs no particular pressing. The vtable and message-map tables are committed source
(src/sim/sim_runtime_tables.inc), once written by this script. Nothing retail is written to
tracked source; the .res and the anchor go under bin/sim-diff/.
"""
from pathlib import Path
import struct
from dialogmap import Fs98Pe, SurveyError, resource_name


def main():
    pe = Fs98Pe('orig/English/Sim.DLL')
    directory = Path('bin/sim-diff')
    directory.mkdir(parents=True, exist_ok=True)
    ui = Fs98Pe('orig/SIMSUI.DLL')
    export = ui.file_offset(ui.directory(0)[0],40)
    base = ui.read_uint32(export+16)
    names = ui.read_uint32(export+32)
    ordinals = ui.read_uint32(export+36)
    for index in range(ui.read_uint32(export+24)):
        name = ui.read_uint32(ui.file_offset(names+index*4,4))
        if ui.c_string(name)=='?ui_new@@YAPAXI@Z':
            ordinal = base+ui.read_uint16(ui.file_offset(ordinals+index*2,2))
            break
    else:raise SurveyError('SIMSUI anchor missing')
    (directory/'simsui-anchor.def').write_text(f'LIBRARY SIMSUI.DLL\nEXPORTS\n    fs98_sim_simsui_anchor @{ordinal} NONAME\n')
    resources = bytearray(struct.pack('<IIHHHHIHHII',0,32,0xffff,0,0xffff,0,0,0,0,0,0))
    for resource in pe.resources():
        if resource['type']==16:continue
        names = resource_name(resource['type'])+resource_name(resource['id'])
        names += b'\0'*(-len(names)%4)
        resources += struct.pack('<II',resource['size'],8+len(names)+16)
        resources += names+struct.pack('<IHHII',0,0x1030,resource['language'],0,0)
        resources += pe.read_bytes(pe.file_offset(resource['rva'],resource['size']),resource['size'])
        resources += b'\0'*(-len(resources)%4)
    (directory/'sim.res').write_bytes(resources)


if __name__=='__main__':main()
