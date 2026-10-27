#!/usr/bin/env python3
"""Snapshot and mutate airport native DLLs; require actual differences."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys


def main():
    output_dir = Path('bin/world-diff/airport-native-breaks')
    output_dir.mkdir(parents=True, exist_ok=True)
    sources = {path.name: path.read_bytes() for path in Path('src/world').iterdir()
               if path.suffix in ('.c', '.h', '.inc', '.def')}
    for name in ('worldairportuidiff.c', 'worlduidiff.c'):
        sources[name] = (Path('tools/difftest') / name).read_bytes().replace(b'../../src/world/', b'')
    for name in ('world-res.o', 'libworld-simsui.a'):
        sources[name] = (Path('bin/world-diff') / name).read_bytes()
    (output_dir / 'inputs.json').write_text(json.dumps({name: hashlib.sha256(data).hexdigest() for name, data in sources.items()}, indent=2) + '\n')
    mutations = [
        ('edit-color', 'world_airport.c', 'font, 0xffffff, 0, 0xff0000,', 'font, 0x00ff00, 0, 0xff0000,'),
        ('edit-limit', 'world_airport.c', 'send(object, offset, 0xc5, 100, 0);', 'send(object, offset, 0xc5, 99, 0);'),
        ('heading-font', 'world_airport.c', 'text_init(object, 0x10e0, 0x220);', 'text_init(object, 0x10e0, 0x370);'),
        ('mode-cache', 'world_airport.c', 'U32(object, 0x23dc) = 3;', 'U32(object, 0x23dc) = 0;'),
        ('accept-owner', 'world_airport.c', 'callback(object, 8); U32', 'callback(object, 4); U32'),
        ('accept-answer', 'world_airport.c', 'U32(PTR(object, 0x10dc), 0) = 1;', 'U32(PTR(object, 0x10dc), 0) = 0;'),
        ('dropdown-placeholder', 'world_airport.c', 'send(object, control, 0x14a, 0, U32(object, 0x23ac));', 'send(object, control, 0x14a, 0, U32(object, 0x23b0));'),
        ('runway-owner', 'world_airport.c', 'callback(object, 0x24);', 'callback(object, 0x20);'),
        ('name-snapshot', 'world_airport.c', 'U32(PTR(object, 0x10dc), 0x9c) = selection; U32(object, 0x23a0) = selection;', 'U32(PTR(object, 0x10dc), 0x9c) = selection; U32(object, 0x239c) = selection;'),
        ('allocation-size', 'world_airport.c', 'U32(object, 0x23d4) * 2', 'U32(object, 0x23d4)'),
        ('release-count', 'world_airport.c', 'index < 10;', 'index < 9;'),
        ('refresh-dispatch', 'world_runtime_tables.inc', 'fs98_world_airport_refresh(object);', 'fs98_world_airport_mode_button(object, 1);'),
        ('facilities-dispatch', 'world_runtime_tables.inc', 'fs98_world_position_facilities(object);', 'fs98_world_airport_refresh(object);'),
        ('auto-delete', 'world_runtime_tables.inc', 'fs98_world_priority_post_destroy(object);', '(void)object;'),
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
        dll, exe = work / 'WORLD_area.dll', work / 'worldairportuidiff.exe'
        flags = ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-strict-aliasing', '-fno-delete-null-pointer-checks']
        commands = [flags + ['-shared', '-nostdlib', '-Wl,--entry=_DllMain@12', '-Wl,--disable-dynamicbase', '-Wl,--image-base=0x223f0000', '-o', str(dll),
            *(str(work / f'world_{unit}.c') for unit in ('area', 'objects', 'init', 'priority', 'priority_objects', 'booster', 'booster_objects', 'booster_init', 'search', 'search_objects', 'search_init', 'list', 'list_objects', 'cache', 'cache_objects', 'files', 'files_objects', 'position', 'position_objects', 'facilities', 'facilities_objects', 'airport', 'airport_objects', 'complexity', 'complexity_objects', 'complexity_init', 'runtime')),
            str(work / 'world.def'), str(work / 'world-res.o'), str(work / 'libworld-simsui.a'), '-lkernel32', '-luser32', '-lmsvcrt', '-lntdll'],
            flags + ['-o', str(exe), str(work / 'worldairportuidiff.c'), '-lgdi32', '-lntdll']]
        for command in commands:
            build = subprocess.run(command, capture_output=True, text=True)
            if build.returncode:
                print(f'{name}: BUILD FAILED (not counted)\n{build.stderr}', flush=True)
                return 1
        try:
            result = subprocess.run(['wine', str(exe), str(dll), '3'], capture_output=True, text=True, timeout=180)
        except subprocess.TimeoutExpired as error:
            (work / 'result.log').write_bytes((error.stdout or b'') + (error.stderr or b''))
            print(f'{name}: TIMEOUT (not counted)', flush=True)
            return 1
        output = result.stdout + result.stderr
        (work / 'result.log').write_text(output)
        if result.returncode == 1 and 'AIRPORT UI DIFFER' in output:
            caught += 1
            print(f'{name}: detected', flush=True)
        else:
            print(f'{name}: NOT DETECTED (exit {result.returncode})\n{output}', flush=True)
    print(f'World airport native mutations: {caught}/{len(selected)} detected', flush=True)
    return 0 if caught == len(selected) else 1


if __name__ == '__main__':
    sys.exit(main())
