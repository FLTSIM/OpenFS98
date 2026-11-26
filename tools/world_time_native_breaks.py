#!/usr/bin/env python3
"""Snapshot and mutate time native DLLs; require actual differences."""
import sys
sys.dont_write_bytecode = True    # before any local import, so no .pyc is written into the tree
import world_native
import sys

MUTATIONS = [
    ('artwork', 'world_time_init.c', '0x1368),0xf831,', '0x1368),0xf830,'),
    ('tab-frame', 'world_time_init.c', '(object,0x150,0x1d,', '(object,0x14f,0x1d,'),
    ('combo-font', 'world_time_init.c', '0x1b80),AT(object,0x3e0)', '0x1b80),AT(object,0x370)'),
    ('combo-label', 'world_time_init.c', '0x1ca0),AT(object,0x3e0),AT(object,0x1130)', '0x1ca0),AT(object,0x3e0),AT(object,0x11e8)'),
    ('clock-heading-font', 'world_time_init.c', 'index==0?0x370:', 'index==0?0x220:'),
    ('edit-limit', 'world_time_init.c', '0x14)-1);', '0x14));'),
    ('season-enable', 'world_time.c', 'index==3||index==7?!manual:(int)manual', 'index==3||index==7?(int)manual:(int)manual'),
    ('clock-enable', 'world_time.c', 'index==4||index==9?!manual:(int)manual', 'index==4||index==9?(int)manual:(int)manual'),
    ('checkbox-raw', 'world_time.c', '0x90)=U32(object,0x1da4);', '0x90)=!!U32(object,0x1da4);'),
    ('checkbox-populate', 'world_time.c', '(int)U32(PTR(object,0x111c),0x90)', '!!U32(PTR(object,0x111c),0x90)'),
    ('season-callback', 'world_time.c', 'kind==1?0x110:0x118);fs98_world_time_populate', 'kind==1?0x114:0x118);fs98_world_time_populate'),
    ('clock-callback', 'world_time.c', 'kind==1?0x110:0x118);fs98_world_time_populate', 'kind==1?0x110:0x11c);fs98_world_time_populate'),
    ('date-action', 'world_time.c', 'id==0x1afc?7:id==0x1afb?5:id==0x1afa?6:0', 'id==0x1afc?5:id==0x1afb?7:id==0x1afa?6:0'),
    ('gmt-conversion', 'world_time.c', 'time_callback(object,0x1120,0x120);', 'time_callback(object,0x1120,0x11c);'),
    ('error-target', 'world_time.c', 'clock_status[]={0x64,0x78,0x3c,0x50}', 'clock_status[]={0x78,0x64,0x3c,0x50}'),
    ('accept-veto', 'world_time.c', 'if(!status||!((int(TC *', 'if(1||!((int(TC *'),
    ('accept-mode', 'world_time.c', 'void fs98_world_time_accept(void *object)\n{\n', 'void fs98_world_time_accept(void *object)\n{\n    U32(object,0x2124)=1;\n'),
    ('accept-answer', 'world_time.c', 'U32(data,0)=1;', 'U32(data,0)=0;'),
    ('accept-descriptor-cache', 'world_time.c', 'void fs98_world_time_accept(void *object)\n{\n', 'void fs98_world_time_accept(void *object)\n{\n    void *saved_descriptor=PTR(object,0x10dc);\n'),
    ('cancel-mode', 'world_time.c', 'U32(object,0x2124)=2;', 'U32(object,0x2124)=1;'),
    ('close-tab', 'world_time.c', '((void(TC *)(void *))vtable[0xc4/4])(tab);', '(void)tab;'),
    ('close-notify-flags', 'world_time.c', 'vtable[1])(notify,1);', 'vtable[1])(notify,0);'),
    ('post-destroy', 'world_runtime_tables.inc', 'fs98_world_priority_post_destroy(object);', '(void)object;'),
]
POSTS = {'accept-descriptor-cache': [(b'        void *data=PTR(object,0x10dc);vtable=PTR(object,0);', b'        void *data=saved_descriptor;vtable=PTR(object,0);')]}


def main():
    return world_native.ui_main(
        'World time native mutations', 'bin/world-diff/time-native-breaks', 'worldtimeuidiff.c', 'TIME UI DIFFER', MUTATIONS,
        lambda name: '3', timeout=180, baseline_dll=True, artifacts=True, posts=POSTS, unknown='unknown time native mutation')


if __name__ == '__main__':
    sys.exit(main())
