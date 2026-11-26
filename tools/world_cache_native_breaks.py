#!/usr/bin/env python3
"""Snapshot and mutate cache-list native DLLs; require actual differences."""
import sys
sys.dont_write_bytecode = True    # before any local import, so no .pyc is written into the tree
import world_native
import sys

MUTATIONS = [
    ('column-tabs', 'world_cache.c', 'int tabs[1] = {0xfa};', 'int tabs[1] = {0x100};'),
    ('active-marker', 'world_cache.c', '== 1 ? "+" : "-"', '== 1 ? "-" : "+"'),
    ('active-equality', 'world_cache.c', 'U32(row, 0x54) == 1', 'U32(row, 0x54) != 0'),
    ('accept-collection', 'world_cache.c', '    fs98_world_cache_collect(object);', '    fs98_world_cache_selection(object);'),
    ('accept-object-result', 'world_cache.c', 'U32(object, 0x1cfc) = 1;', 'U32(object, 0x1cfc) = 2;'),
    ('cancel-answer', 'world_cache.c', '    U32(object, 0x1cfc) = 2;', '    U32(object, 0x1cfc) = 2; U32(PTR(object, 0x10dc), 0) = 0;'),
    ('accept-answer', 'world_cache.c', 'U32(PTR(object, 0x10dc), 0) = 1;', 'U32(PTR(object, 0x10dc), 0) = 0;'),
    ('delete-adjustment', 'world_cache.c', '            --selection;', '            selection = 0;'),
    ('edit-dispatch', 'world_runtime_tables.inc', 'fs98_world_cache_action(object, 0xc);', 'fs98_world_cache_action(object, 0x14);'),
    ('add-dispatch', 'world_runtime_tables.inc', 'fs98_world_cache_action(object, 8);', 'fs98_world_cache_action(object, 0x14);'),
    ('flush-dispatch', 'world_runtime_tables.inc', 'fs98_world_cache_action(object, 0x14);', '(void)object;'),
    ('selection-dispatch', 'world_runtime_tables.inc', 'fs98_world_cache_selection(object);', '(void)object;'),
    ('auto-delete', 'world_runtime_tables.inc', 'fs98_world_priority_post_destroy(object);', '(void)object;'),
    ('empty-edit-enable', 'world_cache.c', 'ui->enable(AT(object, 0x1a08), 0);', 'ui->enable(AT(object, 0x1a08), 1);'),
]


def main():
    return world_native.ui_main(
        'World cache-list native mutations', 'bin/world-diff/cache-native-breaks', 'worldcacheuidiff.c', 'CACHE UI DIFFER', MUTATIONS,
        lambda name: '10', timeout=180, unknown='unknown list native mutation')


if __name__ == '__main__':
    sys.exit(main())
