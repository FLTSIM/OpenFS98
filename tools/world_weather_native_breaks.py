#!/usr/bin/env python3
"""Snapshot and mutate weather native DLLs; require actual differences."""
import sys
sys.dont_write_bytecode = True    # before any local import, so no .pyc is written into the tree
import world_native
import sys

MUTATIONS = [
    ('artwork', 'world_weather.c', '0x1874), 0xf831,', '0x1874), 0xf830,'),
    ('heading-color', 'world_weather.c', 'font[2] = 0xffffff;', 'font[2] = 0x00ff00;'),
    ('combo-font', 'world_weather.c', 's_combos[index]), AT(object, 0x3e0)', 's_combos[index]), AT(object, 0x370)'),
    ('combo-label', 'world_weather.c', 's_combos[index]), AT(object, 0x3e0), AT(object, s_labels[index])', 's_combos[index]), AT(object, 0x3e0), AT(object, 0x12c4)'),
    ('populate-field', 'world_weather.c', '0x1634, 0x14e, U32(PTR(object, 0x10e0), 0x14)', '0x1634, 0x14e, U32(PTR(object, 0x10e0), 0x18)'),
    ('collect-default', 'world_weather.c', 'selection <= 3 ? (uint32_t)selection : 4;', 'selection <= 3 ? (uint32_t)selection : 0;'),
    ('disabled-group', 'world_weather.c', 'if (U32(PTR(object, 0x10e0), 0xc))', 'if (U32(PTR(object, 0x10e0), 0x10))'),
    ('changed-dispatch', 'world_runtime_tables.inc', 'fs98_world_weather_changed(object);', '(void)object;'),
    ('accept-descriptor-cache', 'world_weather.c', 'void fs98_world_weather_accept(void *object)\n{\n', 'void fs98_world_weather_accept(void *object)\n{\n    void *saved_descriptor = PTR(object, 0x10e0);\n'),
    ('accept-validation', 'world_weather.c', 'if (!fs98_world_weather_validate(object) || !fs98_world_weather_error(object))', 'if (1)'),
    ('accept-answer', 'world_weather.c', 'U32(descriptor, 0) = 1;', 'U32(descriptor, 0) = 0;'),
    ('cancel-mode', 'world_weather.c', '0x195c) = 2;', '0x195c) = 1;'),
    ('measure-dispatch', 'world_runtime_tables.inc', '(void)fs98_world_weather_measure(object, id, measure);', '(void)object; (void)id; (void)measure;'),
    ('post-destroy', 'world_runtime_tables.inc', 'fs98_world_priority_post_destroy(object);', '(void)object;'),
    ('font-cleanup', 'world_runtime.c', 's_font_exit = NULL; callback();', 's_font_exit = NULL; (void)callback;'),
]
POSTS = {'accept-descriptor-cache': [(b'        void *descriptor = PTR(object, 0x10e0);', b'        void *descriptor = saved_descriptor;')]}


def main():
    return world_native.ui_main(
        'World weather native mutations', 'bin/world-diff/weather-native-breaks', 'worldweatheruidiff.c', 'WEATHER UI DIFFER', MUTATIONS,
        lambda name: '3', timeout=180, baseline_dll=True, artifacts=True, posts=POSTS, unknown='unknown weather native mutation')


if __name__ == '__main__':
    sys.exit(main())
