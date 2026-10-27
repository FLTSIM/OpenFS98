#!/usr/bin/env python3
"""Snapshot and mutate area-list native DLLs; require actual differences."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys


def main():
    output_dir = Path('bin/world-diff/list-native-breaks')
    output_dir.mkdir(parents=True, exist_ok=True)
    sources = {path.name: path.read_bytes() for path in Path('src/world').iterdir()
               if path.suffix in ('.c', '.h', '.inc', '.def')}
    for name in ('worldlistuidiff.c', 'worlduidiff.c'):
        sources[name] = (Path('tools/difftest') / name).read_bytes().replace(b'../../src/world/', b'')
    for name in ('world-res.o', 'libworld-simsui.a'):
        sources[name] = (Path('bin/world-diff') / name).read_bytes()
    (output_dir / 'inputs.json').write_text(json.dumps({name: hashlib.sha256(data).hexdigest() for name, data in sources.items()}, indent=2) + '\n')
    mutations = [
        ('column-tabs', 'world_list.c', 'int tabs[2] = {0x118, 0x154};', 'int tabs[2] = {0x100, 0x154};'),
        ('row-number', 'world_list.c', 'data.text, "%03d", index', 'data.text, "%02d", index'),
        ('active-marker', 'world_list.c', 'U32(row, 0x34) ? "+" : "-"', 'U32(row, 0x34) ? "-" : "+"'),
        ('protection', 'world_list.c', '), 0x3c) == 0;', '), 0x3c) != 0;'),
        ('cancel-answer', 'world_list.c', 'void fs98_world_list_cancel(void *object)\n{', 'void fs98_world_list_cancel(void *object)\n{\n    U32(PTR(object, 0x10dc), 0) = 0;'),
        ('accept-answer', 'world_list.c', 'U32(PTR(object, 0x10dc), 0) = 1;', 'U32(PTR(object, 0x10dc), 0) = 0;'),
        ('delete-selection', 'world_list.c', '    fs98_world_list_collect(object);\n    list_callback(object, 0x10);', '    list_callback(object, 0x10);'),
        ('edit-dispatch', 'world_runtime_tables.inc', 'fs98_world_list_action(object, 0xc);', 'fs98_world_list_action(object, 0x14);'),
        ('add-dispatch', 'world_runtime_tables.inc', 'fs98_world_list_action(object, 8);', 'fs98_world_list_action(object, 0xc);'),
        ('double-click-dispatch', 'world_runtime_tables.inc', 'fs98_world_list_double_click(object);', '(void)object;'),
        ('double-click-selection', 'world_list.c', '    s_services->events->send(U32(object, 0x1778), 0x188, 0, 0);', '    fs98_world_list_collect(object);'),
        ('auto-delete', 'world_runtime_tables.inc', 'fs98_world_priority_post_destroy(object);', '(void)object;'),
        ('empty-add-enable', 'world_list.c', 'ui->enable(AT(object, 0x1b90), 0);', 'ui->enable(AT(object, 0x1b90), 1);'),
    ]
    requested = set(sys.argv[1:])
    if requested - {item[0] for item in mutations}:
        raise ValueError('unknown list native mutation')
    selected = [item for item in mutations if not requested or item[0] in requested]
    caught = 0
    for name, filename, before, after in selected:
        work = output_dir / name
        work.mkdir(exist_ok=True)
        for source_name, data in sources.items():
            if source_name == filename:
                if data.count(before.encode()) != 1:
                    raise ValueError(f'{name}: mutation anchor is not unique')
                data = data.replace(before.encode(), after.encode(), 1)
            (work / source_name).write_bytes(data)
        dll, exe = work / 'WORLD_area.dll', work / 'worldlistuidiff.exe'
        flags = ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-strict-aliasing', '-fno-delete-null-pointer-checks']
        commands = [flags + ['-shared', '-nostdlib', '-Wl,--entry=_DllMain@12', '-Wl,--disable-dynamicbase', '-Wl,--image-base=0x223f0000', '-o', str(dll),
            *(str(work / f'world_{unit}.c') for unit in ('area', 'objects', 'init', 'priority', 'priority_objects', 'booster', 'booster_objects', 'booster_init', 'search', 'search_objects', 'search_init', 'list', 'list_objects', 'cache', 'cache_objects', 'files', 'files_objects', 'position', 'position_objects', 'runtime')),
            str(work / 'world.def'), str(work / 'world-res.o'), str(work / 'libworld-simsui.a'), '-lkernel32', '-luser32', '-lmsvcrt', '-lntdll'],
            flags + ['-o', str(exe), str(work / 'worldlistuidiff.c'), '-lgdi32', '-lntdll']]
        for command in commands:
            build = subprocess.run(command, capture_output=True, text=True)
            if build.returncode:
                print(f'{name}: BUILD FAILED (not counted)\n{build.stderr}', flush=True)
                return 1
        try:
            result = subprocess.run(['wine', str(exe), str(dll), '10'], capture_output=True, text=True, timeout=180)
        except subprocess.TimeoutExpired as error:
            (work / 'result.log').write_bytes((error.stdout or b'') + (error.stderr or b''))
            print(f'{name}: TIMEOUT (not counted)', flush=True)
            return 1
        output = result.stdout + result.stderr
        (work / 'result.log').write_text(output)
        if result.returncode == 1 and 'LIST UI DIFFER' in output:
            caught += 1
            print(f'{name}: detected', flush=True)
        else:
            print(f'{name}: NOT DETECTED (exit {result.returncode})\n{output}', flush=True)
    print(f'World area-list native mutations: {caught}/{len(selected)} detected', flush=True)
    return 0 if caught == len(selected) else 1


if __name__ == '__main__':
    sys.exit(main())
