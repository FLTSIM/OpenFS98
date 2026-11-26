#!/usr/bin/env python3
"""Snapshot and mutate facilities native DLLs; require actual differences."""
import sys
sys.dont_write_bytecode = True    # before any local import, so no .pyc is written into the tree
import world_native
import sys

MUTATIONS = [
    ('font-cleanup', 'world_facilities.c', 's_services->at_exit(fs98_world_facilities_deinit);', '(void)s_services;'),
    ('highlight-width', 'world_facilities.c', '(int)U32(dib,8)', '(int)U32(dib,12)'),
    ('text-color', 'world_facilities.c', 's_font[2]=0xffffff;', 's_font[2]=0x00ff00;'),
    ('character-font', 'world_facilities.c', 'index?"Verdana":"Arial"', 'index?"Arial":"Verdana"'),
    ('paragraph-tab', 'world_facilities.c', '{1200,2500,4400,6300}', '{1300,2500,4400,6300}'),
    ('paragraph-count', 'world_facilities.c', '(index?5:4);', '(index?4:3);'),
    ('output-heading', 'world_facilities.c', 'string_assign(&line, PTR(row(object, 0x28, offset), 12));', 'string_assign(&line, PTR(row(object, 0x28, offset), 8));'),
    ('output-runway-prefix', 'world_facilities.c', 'string_assign(&line, "H");', 'string_assign(&line, "0");'),
    ('output-runway-surface', 'world_facilities.c', '0xf97c,0xf97d,0xf97f,0xf97e', '0xf97d,0xf97c,0xf97f,0xf97e'),
    ('navaid-type', 'world_facilities.c', 'string_load(&temporary, 0xf982);', 'string_load(&temporary, 0xf986);'),
    ('owner-flags', 'world_facilities.c', 'U32(PTR(object, 0x10dc), 0x20) = 1;', 'U32(PTR(object, 0x10dc), 0x20) = 3;'),
    ('map-image', 'world_facilities.c', 'mode == 10 ? 0x6a4 : 0x6c1', 'mode == 10 ? 0x6c1 : 0x6a4'),
    ('close-answer', 'world_facilities.c', 'U32(PTR(object,0x10dc),0)=0;', 'U32(PTR(object,0x10dc),0)=1;'),
    ('close-output-delete', 'world_facilities.c', 'PTR(PTR(output,0),4))(output,1);', 'PTR(PTR(output,0),4))(output,0);'),
    ('refresh-dispatch', 'world_runtime_tables.inc', 'fs98_world_facilities_refresh(object);', 'fs98_world_facilities_home(object);'),
    ('auto-delete', 'world_runtime_tables.inc', 'fs98_world_priority_post_destroy(object);', '(void)object;'),
]


def main():
    return world_native.ui_main(
        'World facilities native mutations', 'bin/world-diff/facilities-native-breaks', 'worldfacilitiesuidiff.c', 'FACILITIES UI DIFFER', MUTATIONS,
        lambda name: '4', timeout=240, unknown='unknown list native mutation')


if __name__ == '__main__':
    sys.exit(main())
