#!/usr/bin/env python3
"""Snapshot and mutate drive-search native DLLs; require actual differences."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys


def main():
    output_dir = Path('bin/world-diff/search-native-breaks')
    output_dir.mkdir(parents=True, exist_ok=True)
    sources = {path.name: path.read_bytes() for path in Path('src/world').iterdir()
               if path.suffix in ('.c', '.h', '.inc', '.def')}
    for name in ('worldsearchuidiff.c', 'worlduidiff.c'):
        sources[name] = (Path('tools/difftest') / name).read_bytes().replace(b'../../src/world/', b'')
    for name in ('world-res.o', 'libworld-simsui.a'):
        sources[name] = (Path('bin/world-diff') / name).read_bytes()
    (output_dir / 'inputs.json').write_text(json.dumps({name: hashlib.sha256(data).hexdigest() for name, data in sources.items()}, indent=2) + '\n')
    mutations = [
        ('edit-color', 'world_search_init.c', 'font, 0xffffff, 0, 0xff0000,', 'font, 0x00ff00, 0, 0xff0000,'),
        ('title-limit', 'world_search_init.c', '0x3c) - 1, 0', '0x3c), 0'),
        ('active-dispatch', 'world_runtime_tables.inc', 'fs98_world_search_collect(object);', '(void)object;'),
        ('next-dispatch', 'world_runtime_tables.inc', 'fs98_world_search_next(object);', 'fs98_world_search_next_dir(object);'),
        ('layer-closeup', 'world_runtime_tables.inc', 'world_rva_e460(void *object) { fs98_world_search_layer(object); }', 'world_rva_e460(void *object) { (void)object; }'),
        ('drive-dispatch', 'world_runtime_tables.inc', 'fs98_world_search_drive(object);', '(void)object;'),
        ('missing-post', 'world_search.c', 's_services->post(U32(object, 0x20), 0x5fa, U32(object, 0x1324), 0);', '(void)object;'),
        ('synchronous-post', 'world_runtime.c', 'return PostMessageA(', 'return SendMessageA('),
        ('error-priority', 'world_search.c', 'status = U32(descriptor, 0x20);', 'status = U32(descriptor, 0x34);'),
        ('auto-delete', 'world_runtime_tables.inc', 'fs98_world_priority_post_destroy(object);', '(void)object;'),
        ('path-getter', 'world_runtime.c', 'world_slot(0x25700)', 'world_slot(0x25598)'),
        ('ok-answer', 'world_search.c', 'U32(PTR(object, 0x10dc), 0) = 0;', 'U32(PTR(object, 0x10dc), 0) = 1;'),
        ('title-enable', 'world_search.c', 'U32(descriptor, 0x78) != 0 && U32(descriptor, 0x74) != 0;', 'U32(descriptor, 0x78) != 0 || U32(descriptor, 0x74) == 0;'),
    ]
    requested = set(sys.argv[1:])
    if requested - {item[0] for item in mutations}:
        raise ValueError('unknown search native mutation')
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
        dll, exe = work / 'WORLD_area.dll', work / 'worldsearchuidiff.exe'
        flags = ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-strict-aliasing', '-fno-delete-null-pointer-checks']
        commands = [flags + ['-shared', '-nostdlib', '-Wl,--entry=_DllMain@12', '-Wl,--disable-dynamicbase', '-Wl,--image-base=0x223f0000', '-o', str(dll),
            *(str(work / f'world_{unit}.c') for unit in ('area', 'objects', 'init', 'priority', 'priority_objects', 'booster', 'booster_objects', 'booster_init', 'search', 'search_objects', 'search_init', 'list', 'list_objects', 'cache', 'cache_objects', 'files', 'files_objects', 'position', 'position_objects', 'facilities', 'facilities_objects', 'runtime')),
            str(work / 'world.def'), str(work / 'world-res.o'), str(work / 'libworld-simsui.a'), '-lkernel32', '-luser32', '-lmsvcrt', '-lntdll'],
            flags + ['-o', str(exe), str(work / 'worldsearchuidiff.c'), '-lgdi32', '-lntdll']]
        for command in commands:
            build = subprocess.run(command, capture_output=True, text=True)
            if build.returncode:
                print(f'{name}: BUILD FAILED (not counted)\n{build.stderr}', flush=True)
                return 1
        try:
            result = subprocess.run(['wine', str(exe), str(dll), '20' if name == 'path-getter' else '10'], capture_output=True, text=True, timeout=180)
        except subprocess.TimeoutExpired as error:
            (work / 'result.log').write_bytes((error.stdout or b'') + (error.stderr or b''))
            print(f'{name}: TIMEOUT (not counted)', flush=True)
            return 1
        output = result.stdout + result.stderr
        (work / 'result.log').write_text(output)
        if result.returncode == 1 and 'SEARCH UI DIFFER' in output:
            caught += 1
            print(f'{name}: detected', flush=True)
        else:
            print(f'{name}: NOT DETECTED (exit {result.returncode})\n{output}', flush=True)
    print(f'World search native mutations: {caught}/{len(selected)} detected', flush=True)
    return 0 if caught == len(selected) else 1


if __name__ == '__main__':
    sys.exit(main())
