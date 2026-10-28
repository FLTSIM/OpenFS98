#!/usr/bin/env python3
"""Require retail differences for basic-weather events, initialization and lifetime."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys


def main():
    mutations = [
        ('collect-cloud-range', 'core', 'selection <= 4 ?', 'selection <= 3 ?'),
        ('collect-wind-default', 'core', 'selection <= 3 ? (uint32_t)selection : 0;', 'selection <= 3 ? (uint32_t)selection : 4;'),
        ('collect-visibility-default', 'core', 'selection <= 3 ? (uint32_t)selection : 4;', 'selection <= 3 ? (uint32_t)selection : 0;'),
        ('collect-message', 'core', 'int selection = world_send(object, 0x1634, 0x147, 0);', 'int selection = world_send(object, 0x1634, 0x14e, 0);'),
        ('collect-field', 'core', '0x1c) = (uint32_t)selection', '0x18) = (uint32_t)selection'),
        ('populate-selection', 'core', '0x1634, 0x14e, U32(PTR(object, 0x10e0), 0x14)', '0x1634, 0x14e, U32(PTR(object, 0x10e0), 0x18)'),
        ('populate-disable', 'core', 'if (!selection || world_send(object, 0x1634, 0x147, 0) == 4)', 'if (!selection)'),
        ('populate-dependent', 'core', 'else world_send(object, 0x15ec, 0x14e,', 'else world_send(object, 0x15a4, 0x14e,'),
        ('changed-enable', 'core', 'enable = world_send(object, 0x1634, 0x147, 0) != 4;', 'enable = world_send(object, 0x1634, 0x147, 0) == 4;'),
        ('changed-control', 'core', '    world_enable(object, 0x1434, enable);', '    world_enable(object, 0x137c, enable);'),
        ('descriptor-poison', 'core', 'return PTR(object, 0x10e0) != NULL;', 'return PTR(object, 0x10e0) != NULL && U32(object, 0x10e0) != 0xcdcdcdcd;'),
        ('validate-result', 'core', 'return ((int (SC *)(void *))PTR(descriptor, 4))(descriptor);', 'return !!((int (SC *)(void *))PTR(descriptor, 4))(descriptor);'),
        ('accept-mode', 'core', '0x195c) = 1;', '0x195c) = 2;'),
        ('accept-collect', 'core', '    fs98_world_weather_collect(object);', '    (void)object;'),
        ('accept-validation', 'core', 'if (!fs98_world_weather_validate(object) || !fs98_world_weather_error(object))', 'if (!fs98_world_weather_validate(object))'),
        ('accept-answer', 'core', 'U32(descriptor, 0) = 1;', 'U32(descriptor, 0) = 0;'),
        ('cancel-mode', 'core', '0x195c) = 2;', '0x195c) = 1;'),
        ('cancel-answer', 'core', 'U32(descriptor, 0) = 0;', 'U32(descriptor, 0) = 1;'),
        ('init-base', 'init', '    ui->base_init(object);', '    (void)object;'),
        ('init-skin', 'init', 'object, 0x14f,', 'object, 0x14e,'),
        ('init-skin-flags', 'init', 's_services->resources(), 15)', 's_services->resources(), 14)'),
        ('init-skin-failure', 'init', 'if (!((int (TC *', 'if (0 && !((int (TC *'),
        ('init-artwork', 'init', '0x1874), 0xf831,', '0x1874), 0xf830,'),
        ('init-mode', 'init', 'U32(object, 0x10dc) = 1;', 'U32(object, 0x10dc) = 0;'),
        ('init-help-id', 'init', 'U32(object, 0x84) = 11;', 'U32(object, 0x84) = 12;'),
        ('init-button-font', 'init', 's_buttons[index]), AT(object, 0x5a0)', 's_buttons[index]), AT(object, 0x610)'),
        ('init-combo-font', 'init', 's_combos[index]), AT(object, 0x3e0)', 's_combos[index]), AT(object, 0x370)'),
        ('init-combo-label', 'init', 's_combos[index]), AT(object, 0x3e0), AT(object, s_labels[index])', 's_combos[index]), AT(object, 0x3e0), AT(object, 0x12c4)'),
        ('init-font-color', 'init', 'font[2] = 0xffffff;', 'font[2] = 0xff00;'),
        ('init-font-style', 'init', 'AT(s_font_state.font, 0x58) = 4;', 'AT(s_font_state.font, 0x58) = 5;'),
        ('init-font-register', 'init', '        s_services->at_exit(fs98_world_weather_font_cleanup);', '        (void)object;'),
        ('init-font-flags', 'init', 's_font_state.flags |= 1;', 's_font_state.flags |= 2;'),
        ('init-disable-group', 'init', 'if (U32(PTR(object, 0x10e0), 0xc))', 'if (U32(PTR(object, 0x10e0), 0x10))'),
        ('init-hidden-label', 'init', 'AT(object, 0x120c), 0);', 'AT(object, 0x120c), 1);'),
        ('init-populate', 'init', '    fs98_world_weather_populate(object);', '    (void)object;'),
        ('init-string-cleanup', 'init', 'if (state->live) { state->live = 0;', 'if (0 && state->live) { state->live = 0;'),
        ('init-string-transfer', 'init', '\n    state.live = 0; API(0x255c0, void (TC *)(void *))(&string);', '\n    API(0x255c0, void (TC *)(void *))(&string); state.live = 0;'),
        ('context-result', 'life-core', 'if (API(0x258ac, int (SC *)(const void *, int32_t, int32_t))(&rect, point[0], point[1])) return 0;', 'if (API(0x258ac, int (SC *)(const void *, int32_t, int32_t))(&rect, point[0], point[1])) return 1;'),
        ('context-artwork', 'life-core', 'U32(object, 0x1894), &rect', 'U32(object, 0x1654), &rect'),
        ('constructor-template', 'life', 'base_construct(object, 0x6a6, parent)', 'base_construct(object, 0x6a8, parent)'),
        ('constructor-parent', 'life', 'base_construct(object, 0x6a6, parent)', 'base_construct(object, 0x6a6, ((void)parent, NULL))'),
        ('constructor-vtable', 'life', 'state.base = 1;\n    while', 'state.base = 1; WORLD_PTR(object, 0) = s_weather_vtable;\n    while'),
        ('constructor-control', 'life', 'state.constructed < sizeof(s_weather_controls) / sizeof(s_weather_controls[0])', 'state.constructed + 1 < sizeof(s_weather_controls) / sizeof(s_weather_controls[0])'),
        ('constructor-descriptor', 'life', 'WORLD_PTR(object, 0) = s_weather_vtable;', 'WORLD_PTR(object, 0) = s_weather_vtable; WORLD_PTR(object, 0x10e0) = NULL;'),
        ('destructor-vtable', 'life', 'state.object = object; state.base = 1;', 'state.object = object; state.base = 1; WORLD_PTR(object, 0) = s_weather_vtable;'),
        ('destructor-order', 'life', '&s_weather_controls[--state->constructed]', '&s_weather_controls[sizeof(s_weather_controls) / sizeof(s_weather_controls[0]) - state->constructed--]'),
        ('destructor-base', 'life', 's_object_services->destroy(state->object, WORLD_WEATHER_BASE_DTOR);', '(void)state;'),
        ('delete-flags', 'life', 'if (flags & 1)', 'if (flags & 2)'),
        ('factory-size', 'life', 'allocate(0x1960)', 'allocate(0x195c)'),
        ('factory-module', 'life', 'WORLD_U32(object, 0x80) = s_module;', 'WORLD_U32(object, 0x80) = 0;'),
        ('factory-show', 'life', 'show(object, 1)', 'show(object, 0)'),
        ('factory-template', 'life', 'create(object, 0x6a6, NULL)', 'create(object, 0x6a8, NULL)'),
        ('factory-ownership', 'life', 'state.live = 0;\n    s_object_services->create', 'state.live = 1;\n    s_object_services->create'),
        ('factory-cleanup', 'life', 's_object_services->dialog_delete(state->object);', 's_object_services->ui_delete(state->object);'),
        ('exchange-base', 'life', '    fs98_world_weather_base_exchange(object, exchange);', '    (void)object;'),
        ('exchange-control', 'life', 'index < sizeof(s_weather_ddx) / sizeof(s_weather_ddx[0])', 'index + 1 < sizeof(s_weather_ddx) / sizeof(s_weather_ddx[0])'),
        ('help-command', 'life', '"fs98.hlp", 12,', '"fs98.hlp", 10,'),
        ('context-command', 'life', '"fs98.hlp", 10,', '"fs98.hlp", 12,'),
        ('post-destroy', 'life', 'vtable[1])(object, 1);', 'vtable[1])(object, 0);'),
    ]
    selected = [item for item in mutations if len(sys.argv) == 1 or item[0] in sys.argv[1:]]
    if len(sys.argv) > 1 and set(sys.argv[1:]) - {item[0] for item in mutations}:
        raise ValueError('unknown weather mutation')
    output = Path('bin/world-diff/weather-breaks')
    output.mkdir(parents=True, exist_ok=True)
    inputs = [*Path('src/world').glob('world_weather*'), *Path('tools/difftest').glob('worldweather*diff.c')]
    (output / 'inputs.json').write_text(json.dumps({str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs}, indent=2) + '\n')
    caught = 0
    for name, unit, before, after in selected:
        work = output / name
        work.mkdir(exist_ok=True)
        for path in inputs:
            (work / path.name).write_bytes(path.read_bytes().replace(b'../../src/world/', b''))
        filename = 'world_weather_objects.c' if unit == 'life' else 'world_weather.c'
        source = (work / filename).read_text()
        if source.count(before) != 1:
            raise ValueError(f'{name}: anchor count {source.count(before)}')
        (work / filename).write_text(source.replace(before, after, 1))
        harness = 'worldweatherinitdiff.c' if unit == 'init' else 'worldweatherlifediff.c' if unit.startswith('life') else 'worldweatherdiff.c'
        sources = [str(work / 'world_weather.c')]
        if unit.startswith('life'):
            sources.append(str(work / 'world_weather_objects.c'))
        exe = work / 'test.exe'
        result = subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-strict-aliasing',
                                 '-fno-delete-null-pointer-checks', '-Isrc/world', '-o', str(exe), str(work / harness), *sources, '-lntdll'], capture_output=True, text=True)
        (work / 'build.log').write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f'{name}: build failure, not counted\n{result.stderr}')
        args = ['64', '0x4a971b63', 'unwind'] if unit == 'init' else ['32', '0x4a971b63', 'typed'] if unit.startswith('life') else ['256', '0x4a971b63', 'interrupt']
        result = subprocess.run(['wine', str(exe), *args], capture_output=True, text=True, timeout=120)
        (work / 'result.log').write_text(result.stdout + result.stderr)
        (work / 'artifact.sha256').write_text(hashlib.sha256(exe.read_bytes()).hexdigest() + '\n')
        if result.returncode != 1 or 'DIFFER' not in result.stderr:
            raise RuntimeError(f'{name}: no actual difference (exit {result.returncode})\n{result.stdout}{result.stderr}')
        print(f'{name}: detected', flush=True)
        caught += 1
    print(f'World weather guarded mutations: {caught}/{len(selected)} actual differences', flush=True)


if __name__ == '__main__':
    main()
