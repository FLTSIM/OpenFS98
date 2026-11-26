#!/usr/bin/env python3
"""Snapshot and mutate complexity native DLLs; require actual differences."""
import sys
sys.dont_write_bytecode = True    # before any local import, so no .pyc is written into the tree
import world_native
import sys

MUTATIONS = [
    ('artwork', 'world_complexity_init.c', '0x10e4), 0xf831,', '0x10e4), 0xf830,'),
    ('heading-font', 'world_complexity_init.c', '0x13c4), AT(object, 0x370)', '0x13c4), AT(object, 0x3e0)'),
    ('check-font', 'world_complexity_init.c', 'control, AT(object, 0x4c0), 0x139,', 'control, AT(object, 0x370), 0x139,'),
    ('combo-font', 'world_complexity_init.c', '0x147c), AT(object, 0x3e0)', '0x147c), AT(object, 0x370)'),
    ('populate-field', 'world_complexity.c', '0x158c), (int)U32(PTR(object, 0x10e0), 0xc)', '0x158c), (int)U32(PTR(object, 0x10e0), 0x14)'),
    ('collect-field', 'world_complexity.c', '= U32(object, 0x1648);', '= U32(object, 0x1580);'),
    ('collect-selection', 'world_complexity.c', '= (uint32_t)selection;', '= selection != 0;'),
    ('accept-callback', 'world_complexity.c', 'if (fs98_world_complexity_check(object) &&', 'if (0 &&'),
    ('accept-veto', 'world_complexity.c', 'if (fs98_world_complexity_check(object) && fs98_world_complexity_error(object)) return;', 'if (fs98_world_complexity_check(object)) return;'),
    ('accept-answer', 'world_complexity.c', 'U32(descriptor, 0) = 1;', 'U32(descriptor, 0) = 0;'),
    ('cancel-state', 'world_complexity.c', '0x171c) = 2;', '0x171c) = 1;'),
    ('notify-delete', 'world_complexity_objects.c', 'vtable[1])(auxiliary, 1);', 'vtable[1])(auxiliary, 0);'),
    ('post-destroy', 'world_runtime_tables.inc', 'fs98_world_priority_post_destroy(object);', '(void)object;'),
]


def main():
    return world_native.ui_main(
        'World complexity native mutations', 'bin/world-diff/complexity-native-breaks', 'worldcomplexityuidiff.c', 'COMPLEXITY UI DIFFER', MUTATIONS,
        lambda name: '3', timeout=180, unknown='unknown complexity native mutation')


if __name__ == '__main__':
    sys.exit(main())
