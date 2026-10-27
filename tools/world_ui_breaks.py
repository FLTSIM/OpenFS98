#!/usr/bin/env python3
"""Snapshot and mutate native area DLLs; faults/timeouts never count as caught."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys


def main():
    output_dir = Path('bin/world-diff/ui-breaks')
    output_dir.mkdir(parents=True, exist_ok=True)
    sources = {path.name: path.read_bytes() for path in Path('src/world').iterdir()
                if path.suffix in ('.c', '.h', '.inc', '.def')}
    sources['worlduidiff.c'] = Path('tools/difftest/worlduidiff.c').read_bytes().replace(b'../../src/world/', b'')
    for filename in ('world-res.o', 'libworld-simsui.a'):
        sources[filename] = (Path('bin/world-diff') / filename).read_bytes()
    (output_dir / 'inputs.json').write_text(json.dumps({name: hashlib.sha256(data).hexdigest()
        for name, data in sources.items()}, indent=2) + '\n')
    mutations = [
        ('edit-color', 'world_init.c', '0x1258), font, 0xffffff', '0x1258), font, 0x00ff00'),
        ('position-limit', 'world_init.c', '0x19c0), 0xc5, 4, 0', '0x19c0), 0xc5, 3, 0'),
        ('empty-path-enable', 'world_init.c', 'WORLD_U32(object, 0x10e0) = 0;', 'WORLD_U32(object, 0x10e0) = 1;'),
        ('layer-selection', 'world_area.c', 'layer / 10 - 1, 0', 'layer / 10, 0'),
        ('path-dispatch', 'world_runtime_tables.inc', 'fs98_world_area_path_changed(object);', '(void)object;'),
        ('cancel-answer', 'world_area.c', 'WORLD_U32(descriptor, 0) = 0;', 'WORLD_U32(descriptor, 0) = 1;'),
        ('accept-answer', 'world_area.c', 'WORLD_U32(WORLD_PTR(object, 0x10e4), 0) = 1;', 'WORLD_U32(WORLD_PTR(object, 0x10e4), 0) = 0;'),
        ('path-error-focus', 'world_area.c', 'focus_control((uint8_t *)object + 0x1258);', 'focus_control((uint8_t *)object + 0x1938);'),
        ('module-unwind-delete', 'world_runtime.c', '((void (*)(void *))world_mfc(825))(state->dynamic);', '(void)state;'),
        ('module-deinit-clear', 'world_runtime.c', 's_module = s_resource_module = 0;', 's_resource_module = 0;'),
    ]
    requested = set(sys.argv[1:])
    if requested - {mutation[0] for mutation in mutations}:
        raise ValueError('unknown World native mutation')
    jobs = [('baseline', None, None, None)] + [mutation for mutation in mutations if not requested or mutation[0] in requested]
    caught = 0
    for name, filename, before, after in jobs:
        work = output_dir / name
        work.mkdir(exist_ok=True)
        for source_text, data in sources.items():
            if source_text == filename:
                if data.count(before.encode()) != 1:
                    raise ValueError(f'{name}: mutation anchor is not unique')
                data = data.replace(before.encode(), after.encode(), 1)
            (work / source_text).write_bytes(data)
        dll, harness = work / 'WORLD_area.dll', work / 'worlduidiff.exe'
        commands = [
            ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-strict-aliasing',
             '-fno-delete-null-pointer-checks', '-shared', '-nostdlib', '-Wl,--entry=_DllMain@12',
             '-Wl,--disable-dynamicbase', '-Wl,--image-base=0x223f0000', '-o', str(dll),
             *(str(work / f'world_{unit}.c') for unit in ('area', 'objects', 'init', 'priority', 'priority_objects', 'booster', 'booster_objects', 'booster_init', 'search', 'search_objects', 'search_init', 'list', 'list_objects', 'cache', 'cache_objects', 'files', 'files_objects', 'position', 'position_objects', 'facilities', 'facilities_objects', 'airport', 'airport_objects', 'complexity', 'complexity_objects', 'complexity_init', 'runtime')),
             str(work / 'world.def'), str(work / 'world-res.o'), str(work / 'libworld-simsui.a'),
             '-lkernel32', '-luser32', '-lmsvcrt', '-lntdll'],
            ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-strict-aliasing',
             '-o', str(harness), str(work / 'worlduidiff.c'), '-lgdi32', '-lntdll'],
        ]
        for command in commands:
            build = subprocess.run(command, capture_output=True, text=True)
            if build.returncode:
                print(f'{name}: BUILD FAILED (not counted)\n{build.stderr}', flush=True)
                return 1
        try:
            run_result = subprocess.run(['wine', str(harness), str(dll), '8'], capture_output=True, text=True, timeout=120)
        except subprocess.TimeoutExpired as error:
            (work / 'result.log').write_bytes((error.stdout or b'') + (error.stderr or b''))
            print(f'{name}: TIMEOUT (not counted)', flush=True)
            return 1
        output_text = run_result.stdout + run_result.stderr
        (work / 'result.log').write_text(output_text)
        if name == 'baseline':
            if run_result.returncode or '0 differ' not in output_text:
                print(f'World native baseline FAILED\n{output_text}', flush=True)
                return 1
            print('World native baseline: passed', flush=True)
        elif run_result.returncode == 1 and ('WORLD UI DIFFER' in output_text or 'WORLD INIT DIFFER' in output_text):
            caught += 1
            print(f'{name}: detected', flush=True)
        else:
            print(f'{name}: NOT DETECTED (exit {run_result.returncode})\n{output_text}', flush=True)
    print(f'World native mutations: {caught}/{len(jobs) - 1} detected', flush=True)
    return 0 if caught == len(jobs) - 1 else 1


if __name__ == '__main__':
    sys.exit(main())
