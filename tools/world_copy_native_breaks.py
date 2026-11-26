#!/usr/bin/env python3
"""Snapshot and mutate copy native DLLs; require actual differences."""
import sys
sys.dont_write_bytecode = True    # before any local import, so no .pyc is written into the tree
import world_native
import sys

MUTATIONS = [
    ('artwork', 'world_copy_init.c', '0x14d8), 0xf831,', '0x14d8), 0xf830,'),
    ('heading-font', 'world_copy_init.c', '0x1390), AT(object, 0x370)', '0x1390), AT(object, 0x3e0)'),
    ('check-font', 'world_copy_init.c', 'control, AT(object, 0x4c0), 0x139,', 'control, AT(object, 0x370), 0x139,'),
    ('combo-font', 'world_copy_init.c', '0x1490), AT(object, 0x3e0)', '0x1490), AT(object, 0x370)'),
    ('populate-field', 'world_copy.c', '{0x28, 0x38, 0x50, 0x40, 0x30, 0x48}', '{0x38, 0x28, 0x50, 0x40, 0x30, 0x48}'),
    ('collect-field', 'world_copy.c', '= U32(object, 0x1744);', '= U32(object, 0x167c);'),
    ('collect-selection', 'world_copy.c', '0x20) = (uint32_t)selection;', '0x20) = selection != 0;'),
    ('row-data', 'world_copy.c', 's_services->send(U32(object, 0x14b0), 0x151, (uint32_t)added, (uint32_t)row);', 's_services->send(U32(object, 0x14b0), 0x151, (uint32_t)added, (uint32_t)(row + 4));'),
    ('disabled-control', 'world_copy.c', '{0x18e0, 0x1818, 0x19a8, 0x1750, 0x1818}', '{0x1688, 0x1818, 0x19a8, 0x1750, 0x1818}'),
    ('accept-veto', 'world_copy.c', 'if (!fs98_world_copy_check(object))', 'if (1)'),
    ('accept-answer', 'world_copy.c', 'U32(descriptor, 0) = 1;', 'U32(descriptor, 0) = 0;'),
    ('cancel-state', 'world_copy.c', '0x1a70) = 2;', '0x1a70) = 1;'),
    ('error-focus', 'world_copy.c', '0x5fa, U32(object, 0x1468)', '0x5fa, U32(object, 0x14b0)'),
    ('post-destroy', 'world_runtime_tables.inc', 'fs98_world_priority_post_destroy(object);', '(void)object;'),
]


def main():
    return world_native.ui_main(
        'World copy native mutations', 'bin/world-diff/copy-native-breaks', 'worldcopyuidiff.c', 'COPY UI DIFFER', MUTATIONS,
        lambda name: '3', timeout=180, baseline_dll=True, artifacts=True, unknown='unknown copy native mutation')


if __name__ == '__main__':
    sys.exit(main())
