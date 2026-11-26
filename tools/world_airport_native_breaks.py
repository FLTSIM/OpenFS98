#!/usr/bin/env python3
"""Snapshot and mutate airport native DLLs; require actual differences."""
import sys
sys.dont_write_bytecode = True    # before any local import, so no .pyc is written into the tree
import world_native
import sys

MUTATIONS = [
    ('edit-color', 'world_airport.c', 'font, 0xffffff, 0, 0xff0000,', 'font, 0x00ff00, 0, 0xff0000,'),
    ('edit-limit', 'world_airport.c', 'send(object, offset, 0xc5, 100, 0);', 'send(object, offset, 0xc5, 99, 0);'),
    ('heading-font', 'world_airport.c', 'text_init(object, 0x10e0, 0x220);', 'text_init(object, 0x10e0, 0x370);'),
    ('mode-cache', 'world_airport.c', 'U32(object, 0x23dc) = 3;', 'U32(object, 0x23dc) = 0;'),
    ('accept-owner', 'world_airport.c', 'callback(object, 8); U32', 'callback(object, 4); U32'),
    ('accept-answer', 'world_airport.c', 'U32(PTR(object, 0x10dc), 0) = 1;', 'U32(PTR(object, 0x10dc), 0) = 0;'),
    ('dropdown-placeholder', 'world_airport.c', 'send(object, control, 0x14a, 0, U32(object, 0x23ac));', 'send(object, control, 0x14a, 0, U32(object, 0x23b0));'),
    ('runway-owner', 'world_airport.c', 'callback(object, 0x24);', 'callback(object, 0x20);'),
    ('name-snapshot', 'world_airport.c', 'U32(PTR(object, 0x10dc), 0x9c) = selection; U32(object, 0x23a0) = selection;', 'U32(PTR(object, 0x10dc), 0x9c) = selection; U32(object, 0x239c) = selection;'),
    ('allocation-size', 'world_airport.c', 'U32(object, 0x23d4) * 2', 'U32(object, 0x23d4)'),
    ('release-count', 'world_airport.c', 'index < 10;', 'index < 9;'),
    ('refresh-dispatch', 'world_runtime_tables.inc', 'fs98_world_airport_refresh(object);', 'fs98_world_airport_mode_button(object, 1);'),
    ('facilities-dispatch', 'world_runtime_tables.inc', 'fs98_world_position_facilities(object);', 'fs98_world_airport_refresh(object);'),
    ('auto-delete', 'world_runtime_tables.inc', 'fs98_world_priority_post_destroy(object);', '(void)object;'),
]


def main():
    return world_native.ui_main(
        'World airport native mutations', 'bin/world-diff/airport-native-breaks', 'worldairportuidiff.c', 'AIRPORT UI DIFFER', MUTATIONS,
        lambda name: '3', timeout=180, unknown='unknown list native mutation')


if __name__ == '__main__':
    sys.exit(main())
