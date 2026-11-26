#!/usr/bin/env python3
"""Snapshot and mutate area-list native DLLs; require actual differences."""
import sys
sys.dont_write_bytecode = True    # before any local import, so no .pyc is written into the tree
import world_native
import sys

MUTATIONS = [
    ('column-tabs', 'world_list.c', 'int tabs[2] = {0x118, 0x154};', 'int tabs[2] = {0x100, 0x154};'),
    ('row-number', 'world_list.c', 'data.text, "%03d", index', 'data.text, "%02d", index'),
    ('active-marker', 'world_list.c', 'U32(row, 0x34) ? "+" : "-"', 'U32(row, 0x34) ? "-" : "+"'),
    ('protection', 'world_list.c', '), 0x3c) == 0;', '), 0x3c) != 0;'),
    ('cancel-answer', 'world_list.c', 'void fs98_world_list_cancel(void *object)\n{', 'void fs98_world_list_cancel(void *object)\n{\n    U32(PTR(object, 0x10dc), 0) = 0;'),
    ('accept-answer', 'world_list.c', 'U32(PTR(object, 0x10dc), 0) = 1;', 'U32(PTR(object, 0x10dc), 0) = 0;'),
    ('delete-selection', 'world_list.c', '    fs98_world_list_collect(object);\n    list_callback(object, 0x10);', '    list_callback(object, 0x10);'),
    ('edit-dispatch', 'world_runtime_tables.inc', 'fs98_world_list_action(object, 0xc);', 'fs98_world_list_action(object, 0x14);'),
    ('add-dispatch', 'world_runtime_tables.inc', 'fs98_world_list_action(object, 8);', 'fs98_world_list_action(object, 0xc);'),
    ('double-click-dispatch', 'world_runtime_tables.inc', 'fs98_world_list_double_click(object);', '(void)object;'),
    ('double-click-selection', 'world_list.c', '    s_services->events->send(U32(object, 0x1778), 0x188, 0, 0);', '    fs98_world_list_collect(object);'),
    ('auto-delete', 'world_runtime_tables.inc', 'fs98_world_priority_post_destroy(object);', '(void)object;'),
    ('empty-add-enable', 'world_list.c', 'ui->enable(AT(object, 0x1b90), 0);', 'ui->enable(AT(object, 0x1b90), 1);'),
]


def main():
    return world_native.ui_main(
        'World area-list native mutations', 'bin/world-diff/list-native-breaks', 'worldlistuidiff.c', 'LIST UI DIFFER', MUTATIONS,
        lambda name: '10', timeout=180, unknown='unknown list native mutation')


if __name__ == '__main__':
    sys.exit(main())
