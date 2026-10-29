#!/usr/bin/env python3
"""Snapshot and mutate dynamic native DLLs; require actual differences."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys


def main():
    output_dir = Path('bin/world-diff/dynamic-native-breaks')
    output_dir.mkdir(parents=True, exist_ok=True)
    sources = {path.name: path.read_bytes() for path in Path('src/world').iterdir()
               if path.suffix in ('.c', '.h', '.inc', '.def')}
    for name in ('worlddynamicuidiff.c', 'worlduidiff.c'):
        sources[name] = (Path('tools/difftest') / name).read_bytes().replace(b'../../src/world/', b'')
    for name in ('world-res.o', 'libworld-simsui.a'):
        sources[name] = (Path('bin/world-diff') / name).read_bytes()
    (output_dir / 'inputs.json').write_text(json.dumps({name: hashlib.sha256(data).hexdigest() for name, data in sources.items()}, indent=2) + '\n')
    mutations = [
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
    requested = set(sys.argv[1:])
    if requested - {item[0] for item in mutations}:
        raise ValueError('unknown dynamic native mutation')
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
        dll, exe = work / 'WORLD_area.dll', work / 'worlddynamicuidiff.exe'
        flags = ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-strict-aliasing', '-fno-delete-null-pointer-checks']
        commands = [flags + ['-shared', '-nostdlib', '-Wl,--entry=_DllMain@12', '-Wl,--disable-dynamicbase', '-Wl,--image-base=0x223f0000', '-o', str(dll),
            *(str(work / f'world_{unit}.c') for unit in ('area', 'objects', 'init', 'priority', 'priority_objects', 'booster', 'booster_objects', 'booster_init', 'search', 'search_objects', 'search_init', 'list', 'list_objects', 'cache', 'cache_objects', 'files', 'files_objects', 'position', 'position_objects', 'facilities', 'facilities_objects', 'airport', 'airport_objects', 'complexity', 'complexity_objects', 'complexity_init', 'dynamic', 'dynamic_objects', 'dynamic_init', 'copy', 'copy_objects', 'copy_init', 'weather', 'weather_objects', 'time', 'time_init', 'time_objects', 'runtime')),
            str(work / 'world.def'), str(work / 'world-res.o'), str(work / 'libworld-simsui.a'), '-lkernel32', '-luser32', '-lmsvcrt', '-lntdll'],
            flags + ['-o', str(exe), str(work / 'worlddynamicuidiff.c'), '-lgdi32', '-lntdll']]
        for command in commands:
            build = subprocess.run(command, capture_output=True, text=True)
            if build.returncode:
                print(f'{name}: BUILD FAILED (not counted)\n{build.stderr}', flush=True)
                return 1
        try:
            result = subprocess.run(['wine', str(exe), str(dll), '9' if name == 'populate-field' else '3'], capture_output=True, text=True, timeout=180)
        except subprocess.TimeoutExpired as error:
            (work / 'result.log').write_bytes((error.stdout or b'') + (error.stderr or b''))
            print(f'{name}: TIMEOUT (not counted)', flush=True)
            return 1
        output = result.stdout + result.stderr
        (work / 'result.log').write_text(output)
        if result.returncode == 1 and 'DYNAMIC UI DIFFER' in output:
            caught += 1
            print(f'{name}: detected', flush=True)
        else:
            print(f'{name}: NOT DETECTED (exit {result.returncode})\n{output}', flush=True)
    print(f'World dynamic native mutations: {caught}/{len(selected)} detected', flush=True)
    return 0 if caught == len(selected) else 1


if __name__ == '__main__':
    sys.exit(main())
