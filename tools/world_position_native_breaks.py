#!/usr/bin/env python3
"""Snapshot and mutate position native DLLs; require actual differences."""
import sys
sys.dont_write_bytecode = True    # before any local import, so no .pyc is written into the tree
import world_native
import sys

MUTATIONS = [
    ('edit-color', 'world_position.c', 'font, 0xffffff, 0, 0xff0000,', 'font, 0x00ff00, 0, 0xff0000,'),
    ('edit-limit', 'world_position.c', 'field->status + 8) - 1, 0', 'field->status + 8), 0'),
    ('spin-range', 'world_position.c', 'position_spin(object, 0x1d44, 0x10168);', 'position_spin(object, 0x1d44, 0x10167);'),
    ('accept-apply', 'world_position.c', '        position_callback(object, 8);', '        position_callback(object, 0xc);'),
    ('accept-answer', 'world_position.c', 'U32(PTR(object, 0x10dc), 0) = 1;', 'U32(PTR(object, 0x10dc), 0) = 0;'),
    ('changed-flag', 'world_position.c', 'offset) = 1;', 'offset) = 0;'),
    ('snapshot-buffer', 'world_position.c', '0x64, 0x1145', '0x64, 0x1146'),
    ('error-focus', 'world_position.c', 'control = s_controls[index];', 'control = s_controls[(index + 1) % 8];'),
    ('reload-dispatch', 'world_runtime_tables.inc', 'fs98_world_position_reload(object);', 'fs98_world_position_revert(object);'),
    ('facilities-dispatch', 'world_runtime_tables.inc', 'fs98_world_position_facilities(object);', 'fs98_world_position_revert(object);'),
    ('auto-delete', 'world_runtime_tables.inc', 'fs98_world_priority_post_destroy(object);', '(void)object;'),
    ('reload-enable', 'world_position.c', '0x18) == 0);', '0x18) != 0);'),
]


def main():
    return world_native.ui_main(
        'World position native mutations', 'bin/world-diff/position-native-breaks', 'worldpositionuidiff.c', 'POSITION UI DIFFER', MUTATIONS,
        lambda name: '10', timeout=180, unknown='unknown list native mutation')


if __name__ == '__main__':
    sys.exit(main())
