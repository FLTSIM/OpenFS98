#!/usr/bin/env python3
"""Snapshot and mutate dynamic native DLLs; require actual differences."""
import sys
sys.dont_write_bytecode = True    # before any local import, so no .pyc is written into the tree
import world_native
import sys

MUTATIONS = [
    ('artwork', 'world_dynamic_init.c', '0x12dc), 0xf831,', '0x12dc), 0xf830,'),
    ('heading-font', 'world_dynamic_init.c', '0x13c4), AT(object, 0x370)', '0x13c4), AT(object, 0x3e0)'),
    ('check-font', 'world_dynamic_init.c', 'control, AT(object, 0x4c0), 0x139,', 'control, AT(object, 0x370), 0x139,'),
    ('combo-font', 'world_dynamic_init.c', '0x147c), AT(object, 0x3e0)', '0x147c), AT(object, 0x370)'),
    ('populate-field', 'world_dynamic.c', '0x158c), (int)U32(PTR(object, 0x10e0), 0x1c)', '0x158c), (int)U32(PTR(object, 0x10e0), 0x24)'),
    ('collect-field', 'world_dynamic.c', '= U32(object, 0x1648);', '= U32(object, 0x1580);'),
    ('collect-selection', 'world_dynamic.c', '= (uint32_t)selection;', '= selection != 0;'),
    ('accept-callback', 'world_dynamic.c', 'if (fs98_world_dynamic_check(object) &&', 'if (0 &&'),
    ('accept-veto', 'world_dynamic.c', 'if (fs98_world_dynamic_check(object) && fs98_world_dynamic_error(object)) return;', 'if (fs98_world_dynamic_check(object)) return;'),
    ('accept-answer', 'world_dynamic.c', 'U32(descriptor, 0) = 1;', 'U32(descriptor, 0) = 0;'),
    ('cancel-state', 'world_dynamic.c', '0x17e4) = 2;', '0x17e4) = 1;'),
    ('error-order', 'world_dynamic.c', '{0x20, 0x18, 0x10, 8}', '{8, 0x18, 0x10, 0x20}'),
    ('error-focus', 'world_dynamic.c', 's_services->focus(control);', '(void)control;'),
    ('error-accept', 'world_dynamic.c', 'if (fs98_world_dynamic_check(object) && fs98_world_dynamic_error(object)) return;', 'if (fs98_world_dynamic_check(object) && 0) return;'),
    ('notify-delete', 'world_complexity_objects.c', 'vtable[1])(auxiliary, 1);', 'vtable[1])(auxiliary, 0);'),
    ('post-destroy', 'world_runtime_tables.inc', 'fs98_world_priority_post_destroy(object);', '(void)object;'),
]


def main():
    return world_native.ui_main(
        'World dynamic native mutations', 'bin/world-diff/dynamic-native-breaks', 'worlddynamicuidiff.c', 'DYNAMIC UI DIFFER', MUTATIONS,
        lambda name: '9' if name == 'populate-field' else '3', timeout=180, unknown='unknown dynamic native mutation')


if __name__ == '__main__':
    sys.exit(main())
