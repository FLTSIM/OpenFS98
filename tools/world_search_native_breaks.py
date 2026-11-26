#!/usr/bin/env python3
"""Snapshot and mutate drive-search native DLLs; require actual differences."""
import sys
sys.dont_write_bytecode = True    # before any local import, so no .pyc is written into the tree
import world_native
import sys

MUTATIONS = [
    ('edit-color', 'world_search_init.c', 'font, 0xffffff, 0, 0xff0000,', 'font, 0x00ff00, 0, 0xff0000,'),
    ('title-limit', 'world_search_init.c', '0x3c) - 1, 0', '0x3c), 0'),
    ('active-dispatch', 'world_runtime_tables.inc', 'fs98_world_search_collect(object);', '(void)object;'),
    ('next-dispatch', 'world_runtime_tables.inc', 'fs98_world_search_next(object);', 'fs98_world_search_next_dir(object);'),
    ('layer-closeup', 'world_runtime_tables.inc', 'world_rva_e460(void *object) { fs98_world_search_layer(object); }', 'world_rva_e460(void *object) { (void)object; }'),
    ('drive-dispatch', 'world_runtime_tables.inc', 'fs98_world_search_drive(object);', '(void)object;'),
    ('missing-post', 'world_search.c', 's_services->post(U32(object, 0x20), 0x5fa, U32(object, 0x1324), 0);', '(void)object;'),
    ('synchronous-post', 'world_runtime.c', 'return PostMessageA(', 'return SendMessageA('),
    ('error-priority', 'world_search.c', 'status = U32(descriptor, 0x20);', 'status = U32(descriptor, 0x34);'),
    ('auto-delete', 'world_runtime_tables.inc', 'fs98_world_priority_post_destroy(object);', '(void)object;'),
    ('path-getter', 'world_runtime.c', 'world_slot(0x25700)', 'world_slot(0x25598)'),
    ('ok-answer', 'world_search.c', 'U32(PTR(object, 0x10dc), 0) = 0;', 'U32(PTR(object, 0x10dc), 0) = 1;'),
    ('title-enable', 'world_search.c', 'U32(descriptor, 0x78) != 0 && U32(descriptor, 0x74) != 0;', 'U32(descriptor, 0x78) != 0 || U32(descriptor, 0x74) == 0;'),
]


def main():
    return world_native.ui_main(
        'World search native mutations', 'bin/world-diff/search-native-breaks', 'worldsearchuidiff.c', 'SEARCH UI DIFFER', MUTATIONS,
        lambda name: '20' if name == 'path-getter' else '10', timeout=180, unknown='unknown search native mutation')


if __name__ == '__main__':
    sys.exit(main())
