#!/usr/bin/env python3
"""Snapshot and mutate world-files native DLLs; require actual differences."""
import sys
sys.dont_write_bytecode = True    # before any local import, so no .pyc is written into the tree
import world_native
import sys

MUTATIONS = [
    ('list-font', 'world_files.c', 'list_init(AT(object, 0x152c), AT(object, 0x450)', 'list_init(AT(object, 0x152c), AT(object, 0x220)'),
    ('accept-collection', 'world_files.c', '    fs98_world_files_collect(object);', '    fs98_world_files_selection(object);'),
    ('accept-object-result', 'world_files.c', 'U32(object, 0x1824) = 1;', 'U32(object, 0x1824) = 2;'),
    ('cancel-answer', 'world_files.c', '    U32(object, 0x1824) = 2;', '    U32(object, 0x1824) = 2; U32(PTR(object, 0x10dc), 0) = 0;'),
    ('accept-answer', 'world_files.c', 'U32(PTR(object, 0x10dc), 0) = 1;', 'U32(PTR(object, 0x10dc), 0) = 0;'),
    ('areas-dispatch', 'world_runtime_tables.inc', 'fs98_world_files_action(object, 8);', 'fs98_world_files_action(object, 0xc);'),
    ('caches-dispatch', 'world_runtime_tables.inc', 'fs98_world_files_action(object, 0xc);', 'fs98_world_files_action(object, 8);'),
    ('selection-dispatch', 'world_runtime_tables.inc', 'fs98_world_files_selection(object);', '(void)object;'),
    ('auto-delete', 'world_runtime_tables.inc', 'fs98_world_priority_post_destroy(object);', '(void)object;'),
    ('callback-refresh', 'world_files.c', 'PTR(descriptor, callback_offset))(descriptor);', 'PTR(descriptor, callback_offset))(descriptor); fs98_world_files_populate(object);'),
    ('enable-polarity', 'world_files.c', '0x1728), U32(files_row(object, selected), 0)', '0x1728), !U32(files_row(object, selected), 0)'),
]


def main():
    return world_native.ui_main(
        'World world-files native mutations', 'bin/world-diff/files-native-breaks', 'worldfilesuidiff.c', 'FILES UI DIFFER', MUTATIONS,
        lambda name: '10', timeout=180, unknown='unknown list native mutation')


if __name__ == '__main__':
    sys.exit(main())
