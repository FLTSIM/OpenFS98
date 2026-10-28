#!/usr/bin/env python3
"""Require actual priority core/lifetime/native differences for deliberate errors."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys


def main():
    output_dir = Path('bin/world-diff/priority-breaks')
    output_dir.mkdir(parents=True, exist_ok=True)
    sources = {path.name: path.read_bytes() for path in Path('src/world').iterdir()
               if path.suffix in ('.c', '.h', '.inc', '.def')}
    for name in ('worldprioritydiff.c', 'worldprioritylifediff.c', 'worldpriorityuidiff.c', 'worlduidiff.c'):
        sources[name] = (Path('tools/difftest') / name).read_bytes().replace(b'../../src/world/', b'')
    for name in ('world-res.o', 'libworld-simsui.a'):
        sources[name] = (Path('bin/world-diff') / name).read_bytes()
    (output_dir / 'inputs.json').write_text(json.dumps({name: hashlib.sha256(data).hexdigest() for name, data in sources.items()}, indent=2) + '\n')
    mutations = [
        ('validate-move', 'core', 'world_priority.c', 'priority_callback(object, 0x14);', '(void)object;'),
        ('collect-capacity', 'core', 'world_priority.c', 'int value;\n    ui->get_text(AT(object, 0x1250), text, 0x1f);', 'int value;\n    ui->get_text(AT(object, 0x1250), text, 0x1e);'),
        ('position-format', 'core', 'world_priority.c', '"%03d"', '"%04d"'),
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
    requested = set(sys.argv[1:])
    if requested - {item[0] for item in mutations}:
        raise ValueError('unknown priority mutation')
    selected = [item for item in mutations if not requested or item[0] in requested]
    caught = 0
    for name, suite, filename, before, after in selected:
        work = output_dir / name
        work.mkdir(exist_ok=True)
        for source_name, data in sources.items():
            if source_name == filename:
                if before.encode() not in data:
                    raise ValueError(f'{name}: missing mutation anchor')
                data = data.replace(before.encode(), after.encode(), 1)
            (work / source_name).write_bytes(data)
        exe = work / f'worldpriority{suite}diff.exe'
        flags = ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-strict-aliasing', '-fno-delete-null-pointer-checks']
        if suite == 'ui':
            dll = work / 'WORLD_area.dll'
            commands = [flags + ['-shared', '-nostdlib', '-Wl,--entry=_DllMain@12', '-Wl,--disable-dynamicbase', '-Wl,--image-base=0x223f0000', '-o', str(dll),
                *(str(work / f'world_{unit}.c') for unit in ('area', 'objects', 'init', 'priority', 'priority_objects', 'booster', 'booster_objects', 'booster_init', 'search', 'search_objects', 'search_init', 'list', 'list_objects', 'cache', 'cache_objects', 'files', 'files_objects', 'position', 'position_objects', 'facilities', 'facilities_objects', 'airport', 'airport_objects', 'complexity', 'complexity_objects', 'complexity_init', 'dynamic', 'dynamic_objects', 'dynamic_init', 'copy', 'copy_objects', 'copy_init', 'weather', 'weather_objects', 'runtime')),
                str(work / 'world.def'), str(work / 'world-res.o'), str(work / 'libworld-simsui.a'), '-lkernel32', '-luser32', '-lmsvcrt', '-lntdll'],
                flags + ['-o', str(exe), str(work / 'worldpriorityuidiff.c'), '-lgdi32', '-lntdll']]
            run = ['wine', str(exe), str(dll), '7']
            marker = 'PRIORITY UI DIFFER'
        elif suite == 'life':
            commands = [flags + ['-o', str(exe), str(work / 'worldprioritylifediff.c'), str(work / 'world_priority_objects.c'), '-lntdll']]
            run = ['wine', str(exe), '32', '0x4a971b63', 'typed']
            marker = 'DIFFER scenario='
        else:
            commands = [flags + ['-o', str(exe), str(work / 'worldprioritydiff.c'), *(str(work / f'world_{unit}.c') for unit in ('priority', 'priority_objects', 'init', 'area')), '-lntdll']]
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
