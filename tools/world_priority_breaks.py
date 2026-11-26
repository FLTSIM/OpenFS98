#!/usr/bin/env python3
"""Require actual priority core/lifetime/native differences for deliberate errors."""
import sys
sys.dont_write_bytecode = True    # before any local import, so no .pyc is written into the tree
import break_build_only as bo
import world_native
from world_native import FLAGS
from pathlib import Path
import subprocess
import sys

MUTATIONS = [
    ('validate-move', 'core', 'world_priority.c', 'priority_callback(object, 0x14);', '(void)object;'),
    ('collect-capacity', 'core', 'world_priority.c', 'int value;\n    ui->get_text(AT(object, 0x1250), text, 0x1f);', 'int value;\n    ui->get_text(AT(object, 0x1250), text, 0x1e);'),
    ('position-format', 'core', 'world_priority.c', '"%03d", (int)(selected + 1)', '"%04d", (int)(selected + 1)'),
    ('first-invalid-layer', 'core', 'world_priority.c', 'init->string_construct(&data.string); state.live = 1;', 'init->string_construct(&data.string); state.live = 1; data.text[0] = 0;'),
    ('active-indicator', 'core', 'world_priority.c', 'U32(row, 0x34) ? "+" : "-"', 'U32(row, 0x34) ? "-" : "+"'),
    ('error-count', 'core', 'world_priority.c', '0x38) = (uint32_t)count;', '0x38) = (uint32_t)count + 1;'),
    ('error-return', 'core', 'world_priority.c', 'return ((int (TC *)(void *, uint32_t))PTR(PTR(object, 0), 0xe0))(object, control);', '((int (TC *)(void *, uint32_t))PTR(PTR(object, 0), 0xe0))(object, control); return (int)control;'),
    ('accept-vtable', 'core', 'world_priority.c', '((void (TC *)(void *))vtable[0xec / 4])(object);', '((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);'),
    ('cstring-unwind', 'core', 'world_priority.c', 'fs98_world_get_init_services()->string_destroy(state->string);', '(void)state;'),
    ('last-selection-enable', 'core', 'world_priority.c', 'U32(PTR(object, 0x10dc), 0x20) != selected', 'U32(PTR(object, 0x10dc), 0x20) == selected'),
    ('constructor-control', 'life', 'world_priority_objects.c', 'state.constructed < sizeof(s_priority_controls) / sizeof(s_priority_controls[0])', 'state.constructed + 1 < sizeof(s_priority_controls) / sizeof(s_priority_controls[0])'),
    ('base-destructor', 'life', 'world_priority_objects.c', 's_object_services->destroy(state->object, WORLD_PRIORITY_BASE_DTOR);', '(void)state;'),
    ('delete-flag', 'life', 'world_priority_objects.c', 'if (flags & 1)', 'if (flags & 2)'),
    ('allocation-cleanup', 'life', 'world_priority_objects.c', 's_object_services->dialog_delete(state->object);', 's_object_services->ui_delete(state->object);'),
    ('factory-parent', 'life', 'world_priority_objects.c', 'create(object, 0x6b4, NULL)', 'create(object, 0x6b4, (void *)parent)'),
    ('create-message', 'life', 'world_priority_objects.c', 'if (!s_object_services->create', 'if (s_object_services->create'),
    ('typed-catch-cleanup', 'life', 'world_priority_objects.c', 'world_factory_cleanup(&state->frame);', 'state->live = 0; world_factory_cleanup(&state->frame);'),
    ('native-edit-color', 'ui', 'world_priority.c', 'font, 0xffffff, 0, 0xff0000', 'font, 0x00ff00, 0, 0xff0000'),
    ('native-list-tabs', 'ui', 'world_priority.c', 'int tabs[2] = {0x118, 0x154}', 'int tabs[2] = {0x100, 0x154}'),
    ('native-up-dispatch', 'ui', 'world_runtime_tables.inc', 'fs98_world_priority_action(object, 8);', 'fs98_world_priority_action(object, 0xc);'),
    ('native-post-destroy', 'ui', 'world_runtime_tables.inc', 'fs98_world_priority_post_destroy(object);', '(void)object;'),
    ('native-edit-limit', 'ui', 'world_priority.c', '0x1270), 0xc5, 3, 0', '0x1270), 0xc5, 4, 0'),
]


def present(blob, name, filename, before, after):
    """The anchor must match exactly once (it was a presence check; position-format's anchor now holds the whole call)."""
    if blob.count(before.encode()) != 1:
        raise ValueError(f'{name}: mutation anchor is not unique')
    return blob.replace(before.encode(), after.encode(), 1)


def make_case(data, row):
    name, suite, filename, before, after = row
    exe_name = f'worldpriority{suite}diff.exe'

    def builds(work):
        exe = work / exe_name
        if suite == 'ui':
            return [world_native.dll_command(work, work / 'WORLD_area.dll'), world_native.ui_command(work, exe, 'worldpriorityuidiff.c')]
        if suite == 'life':
            return [[*FLAGS, '-o', str(exe), str(work / 'worldprioritylifediff.c'), str(work / 'world_priority_objects.c'), '-lntdll']]
        return [[*FLAGS, '-o', str(exe), str(work / 'worldprioritydiff.c'), *(str(work / f'world_{unit}.c') for unit in ('priority', 'priority_objects', 'init', 'area')), '-lntdll']]
    return world_native.case(f'{suite}.{name}', data, filename, lambda blob: present(blob, name, filename, before, after),
                             builds, ['WORLD_area.dll', exe_name] if suite == 'ui' else [exe_name])


def main():
    harnesses = ('worldprioritydiff.c', 'worldprioritylifediff.c', 'worldpriorityuidiff.c', 'worlduidiff.c')
    args = sys.argv[1:]
    if '--build-only' in args:
        def factory():
            data = world_native.sources(harnesses)
            return [make_case(data, row) for row in MUTATIONS], data
        return bo.build_only(args[args.index('--build-only') + 1:], factory)
    output_dir = Path('bin/world-diff/priority-breaks')
    output_dir.mkdir(parents=True, exist_ok=True)
    data = world_native.sources(harnesses)
    world_native.write_inputs_json(output_dir, data)
    requested = set(args)
    if requested - {item[0] for item in MUTATIONS}:
        raise ValueError('unknown priority mutation')
    selected = [item for item in MUTATIONS if not requested or item[0] in requested]
    caught = 0
    for row in selected:
        name, suite = row[0], row[1]
        case = make_case(data, row)
        work = output_dir / name
        work.mkdir(exist_ok=True)
        bo.write_case(work, data, case.overlay())
        exe = work / f'worldpriority{suite}diff.exe'
        commands = case.builds(work)
        if suite == 'ui':
            dll = work / 'WORLD_area.dll'
            run = ['wine', str(exe), str(dll), '7']
            marker = 'PRIORITY UI DIFFER'
        elif suite == 'life':
            run = ['wine', str(exe), '32', '0x4a971b63', 'typed']
            marker = 'DIFFER scenario='
        else:
            run = ['wine', str(exe), '128', 'unwind']
            marker = 'PRIORITY DIFFER'
        for command in commands:
            build = subprocess.run(command, capture_output=True, text=True)
            if build.returncode:
                print(f'{name}: BUILD FAILED (not counted)\n{build.stderr}', flush=True)
                return 1
        try:
            result = subprocess.run(run, capture_output=True, text=True, timeout=120)
        except subprocess.TimeoutExpired as error:
            (work / 'result.log').write_bytes((error.stdout or b'') + (error.stderr or b''))
            print(f'{name}: TIMEOUT (not counted)', flush=True)
            return 1
        output = result.stdout + result.stderr
        (work / 'result.log').write_text(output)
        if result.returncode == 1 and marker in output:
            caught += 1
            print(f'{name}: detected', flush=True)
        else:
            print(f'{name}: NOT DETECTED (exit {result.returncode})\n{output}', flush=True)
    print(f'World priority mutations: {caught}/{len(selected)} detected', flush=True)
    return 0 if caught == len(selected) else 1


if __name__ == '__main__':
    sys.exit(main())
