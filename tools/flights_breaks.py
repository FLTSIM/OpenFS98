#!/usr/bin/env python3
"""Break src/flights/ on purpose and check flightseventsdiff reports each break.

    python3 -B tools/flights_breaks.py [scenarios] [name...]

Only a reported DIFFER counts as caught: a build failure, a fault or a timeout does not.
Run from a frozen copy of the tree if the sources may change meanwhile.

Left out as proven equivalent: a null check on post_destroy's object made to depend on its
window handle (never zero in the fixture), and cancel's descriptor read moved after the
state write, and select's second read of the descriptor folded into its first (no call
between them can observe either). A removed null check faults on
both sides rather than differing, which is not counted.
"""
from pathlib import Path
import subprocess
import sys

EVENTS = [
    # the shared helpers
    ('disable-window', 'enable_window(D32(object, 0x20), 0)', 'enable_window(D32(object, 0x20), 1)'),
    ('enable-window', 'enable_window(D32(object, 0x20), 1)', 'enable_window(D32(object, 0x20), 0)'),
    ('post-destroy-flags', '))(object, 1);\n}', '))(object, 0);\n}'),
    ('help-command', 's_help_file, 0xc,', 's_help_file, 0xd,'),
    ('help-window', 'win_help(D32(info, 0xc)', 'win_help(D32(info, 0x8)'),
    ('context-command', 's_help_file, 0xa,', 's_help_file, 0xb,'),
    ('context-return', '    return 1; \\\n} \\\nvoid', '    return 0; \\\n} \\\nvoid'),
    ('ddx-control-id', 's_ddx_##name[index][1],', 's_ddx_##name[index][1] ^ 1,'),
    ('ddx-base', '    s_services->base_exchange(object, exchange); \\', '    (void)exchange; \\'),
    # cancel and accept
    ('cancel-state', '    D32(object, 0x15e0) = 2;\n    descriptor[0] = 0;', '    D32(object, 0x15e0) = 3;\n    descriptor[0] = 0;'),
    ('cancel-answer', '    descriptor[0] = 0;', '    descriptor[0] = 2;'),
    ('cancel-slot', '((void (TC *)(void *))PTR(vtable, 0xec))(object);\n}\n\n/* 0x22313040',
     '((void (TC *)(void *))PTR(vtable, 0xe8))(object);\n}\n\n/* 0x22313040'),
    ('accept-state', 'D32(object, 0x15e0) = 1;', 'D32(object, 0x15e0) = 2;'),
    ('accept-answer', '        descriptor[0] = 1;', '        descriptor[0] = 2;'),
    ('accept-sync', '    D32(object, 0x15e0) = 1;\n    fs98_flights_picker_sync(object);', '    D32(object, 0x15e0) = 1;'),
    ('accept-descriptor-early', '    D32(object, 0x15e0) = 1;\n    fs98_flights_picker_sync(object);',
     '    uint32_t *early = PTR(object, 0x10dc);\n    D32(object, 0x15e0) = 1;\n    fs98_flights_picker_sync(object);\n    PTR(object, 0x10dc) = early;'),
    ('accept-error-check', 'if (fs98_flights_picker_no_error(object) && fs98_flights_picker_no_error(object)) return;',
     'if (!fs98_flights_picker_no_error(object)) return;'),
    ('no-error-value', '{ (void)object; return 0; }', '{ (void)object; return 1; }'),
    ('validate-value', 'return PTR(object, 0x10dc) != 0;', 'return 1;'),
    ('adventures-empty-cancel', '        VCALL(object, 0xd0);\n', '        fs98_flights_picker_cancel(object);\n'),
    ('challenges-empty-cancel', '        fs98_flights_picker_cancel(object);\n        return;\n    }\n    fs98_flights_picker_accept(object);\n}\n\n/* Fill',
     '        VCALL(object, 0xd0);\n        return;\n    }\n    fs98_flights_picker_accept(object);\n}\n\n/* Fill'),
    # selection
    ('select-none', '    if (selection == -1) return;\n    D32(PTR', '    if (selection == -2) return;\n    D32(PTR'),
    ('select-stride', 'D32(descriptor, 0xc) * (uint32_t)0xa08 + 0x100', 'D32(descriptor, 0xc) * (uint32_t)0xa00 + 0x100'),
    ('select-description', 'D32(descriptor, 0xc) * (uint32_t)0xa08 + 0x100', 'D32(descriptor, 0xc) * (uint32_t)0xa08'),
    ('select-control', 'set_window_text(AT(object, 0x1298),', 'set_window_text(AT(object, 0x1250),'),
    ('sync-none', '    if (selection != -1) D32', '    D32'),
    # filling the list
    ('fill-reset', 'LB_RESETCONTENT, 0, 0);', 'LB_RESETCONTENT, 1, 0);'),
    ('fill-reload', '                                 (uint32_t)(uintptr_t)AT(PTR(descriptor, 0x10), offset));\n        descriptor = PTR(object, 0x10dc);',
     '                                 (uint32_t)(uintptr_t)AT(PTR(descriptor, 0x10), offset));'),
    ('fill-stride', 'offset += 0xa08)', 'offset += 0xa00)'),
    ('fill-count', 'index < D32(descriptor, 8);', 'index <= D32(descriptor, 8);'),
    ('adventures-range', 'if (D32(descriptor, 0xc) < D32(descriptor, 8))', 'if (D32(descriptor, 0xc) <= D32(descriptor, 8))'),
    ('adventures-signed-range', 'if (D32(descriptor, 0xc) < D32(descriptor, 8))', 'if ((int32_t)D32(descriptor, 0xc) < (int32_t)D32(descriptor, 8))'),
    ('adventures-select-always', '        s_services->send_message(D32(object, 0x1270), LB_SETCURSEL, D32(descriptor, 0xc), 0);\n    fs98_flights_picker_select(object);\n}',
     '    {\n        s_services->send_message(D32(object, 0x1270), LB_SETCURSEL, D32(descriptor, 0xc), 0);\n        fs98_flights_picker_select(object);\n    }\n}'),
    ('challenges-empty', 'if (D32(descriptor, 8) != 0) {', 'if (D32(descriptor, 0xc) < D32(descriptor, 8)) {'),
    ('double-click-order', '    fs98_flights_picker_select(object);\n    VCALL(object, 0xcc);\n}\nvoid',
     '    VCALL(object, 0xcc);\n    fs98_flights_picker_select(object);\n}\nvoid'),
    ('double-click-slot', '    VCALL(object, 0xcc);\n}\n\n/* 0x22312ec0', '    VCALL(object, 0xd0);\n}\n\n/* 0x22312ec0'),
    # OnInitDialog
    ('init-skin-flags', 's_resource_module, 7)', 's_resource_module, 6)'),
    ('init-adventures-skin', '(object, 0x13b,', '(object, 0x13c,'),
    ('init-challenges-skin', '(object, 0x13c,', '(object, 0x13b,'),
    ('init-art-flags', 'AT(object, 0x7e8), 2);', 'AT(object, 0x7e8), 1);'),
    ('init-button-bitmap', '0x132,\n                            AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff,\n                            s_resource_module, 1, 1, empty);\n    s_services->init_text',
     '0x133,\n                            AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff,\n                            s_resource_module, 1, 1, empty);\n    s_services->init_text'),
    ('init-button-color', '1, 0xffffffff,\n                            s_resource_module, 1, 1, empty);\n    s_services->init_button',
     '1, 0xfffffffe,\n                            s_resource_module, 1, 1, empty);\n    s_services->init_button'),
    ('init-list-label', 'AT(object, 0x450), AT(object, 0x10e0));', 'AT(object, 0x450), AT(object, 0x1198));'),
    ('init-edit-font', 'base_font(2)', 'base_font(1)'),
    ('init-edit-error', '0xff0000,\n                          AT(object, 0x1198), 0);', '0xfe0000,\n                          AT(object, 0x1198), 0);'),
    ('init-text-order', '    s_services->init_text(AT(object, 0x10e0), AT(object, 0x370));\n    s_services->init_list(AT(object, 0x1250), AT(object, 0x450), AT(object, 0x10e0));',
     '    s_services->init_list(AT(object, 0x1250), AT(object, 0x450), AT(object, 0x10e0));\n    s_services->init_text(AT(object, 0x10e0), AT(object, 0x370));'),
    ('init-limit', 'EM_LIMITTEXT, 0x3ff, 0);', 'EM_LIMITTEXT, 0x400, 0);'),
    ('init-return', '    populate(object);\n    return 1;', '    populate(object);\n    return 2;'),
    ('init-skin-failure', '            object, skin, s_resource_module, 7)) return 0;', '            object, skin, s_resource_module, 7)) return 1;'),
]
OBJECTS = [
    ('constructor-parent', 'class_info->template, parent)', 'class_info->template, (void *)0)'),
    ('constructor-vtable', 'PTR(object, 0) = s_vtables[kind];', 'PTR(object, 0) = s_vtables[kind ^ 1];'),
    ('delete-flags', 'if (flags & 1) s_services->ui_delete', 'if (flags & 2) s_services->ui_delete'),
    ('factory-module', 'D32(object, 0x80) = s_module;', 'D32(object, 0x80) = s_module ^ 1;'),
    ('factory-show', 'show(object, 1)', 'show(object, 0)'),
    ('factory-size', 'allocate(class_info->size)', 'allocate(class_info->size - 4)'),
    ('factory-template', 'create(object, class_info->template, 0)', 'create(object, class_info->template + 1, 0)'),
    ('factory-return', 'return D32(object, 0x20);', 'return D32(object, 0x24);'),
]
UNWIND = [
    ('constructor-unwind-base', 'objects', '    state.base = 1;', '    state.base = 0;'),
    ('cleanup-order', 'objects', '&state->class_info->controls[--state->constructed];',
     '&state->class_info->controls[state->class_info->count - state->constructed--];'),
    ('factory-unwind-allocator', 'objects', 'dialog_delete(state->object)', 'ui_delete(state->object)'),
    ('factory-unwind-live', 'objects', '    state.live = 0;\n    D32(object, 0x80)', '    D32(object, 0x80)'),
]


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    only = set(sys.argv[2:])
    if count < 100:
        raise ValueError('use at least 100 scenarios')
    directory = Path('bin/flights-diff/breaks')
    directory.mkdir(parents=True, exist_ok=True)
    breaks = [(name, 'events', 'normal', before, after) for name, before, after in EVENTS]
    breaks += [(name, 'objects', 'normal', before, after) for name, before, after in OBJECTS]
    breaks += [(name, unit, 'unwind', before, after) for name, unit, before, after in UNWIND]
    if only:
        breaks = [b for b in breaks if b[0] in only]
    caught = 0
    for name, unit, mode, before, after in breaks:
        original = Path(f'src/flights/flights_{unit}.c').read_text()
        if original.count(before) != 1:
            raise ValueError(f'{name}: the anchor occurs {original.count(before)} times')
        source = directory / f'{name}.c'
        executable = directory / f'{name}.exe'
        source.write_text(original.replace(before, after, 1))
        executable.unlink(missing_ok=True)
        other = 'objects' if unit == 'events' else 'events'
        build = subprocess.run([
            'i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Wno-unused',
            '-fno-strict-aliasing', '-fno-delete-null-pointer-checks', '-Isrc/flights', '-o', str(executable),
            'tools/difftest/flightseventsdiff.c', str(source), f'src/flights/flights_{other}.c', '-lntdll'],
            text=True, capture_output=True)
        if build.returncode:
            print(f'{name}: BUILD FAILED (not counted)\n{build.stderr}', flush=True)
            continue
        scenarios = str(count if mode == 'normal' else max(100, count // 10))
        try:
            run = subprocess.run(['wine', str(executable), scenarios, '0x5f1a6c2d', mode],
                                 text=True, errors='replace', capture_output=True, timeout=300)
        except subprocess.TimeoutExpired:
            print(f'{name}: TIMEOUT (not counted)', flush=True)
            continue
        output = run.stdout + run.stderr
        (directory / f'{name}.log').write_text(output)
        if run.returncode == 1 and 'DIFFER scenario=' in output:
            caught += 1
            print(f'{name}: caught', flush=True)
        else:
            print(f'{name}: NOT CAUGHT (exit {run.returncode})\n{output[-600:]}', flush=True)
    print(f'Flights breaks: {caught}/{len(breaks)} caught', flush=True)
    return 0 if caught == len(breaks) else 1


if __name__ == '__main__':
    sys.exit(main())
