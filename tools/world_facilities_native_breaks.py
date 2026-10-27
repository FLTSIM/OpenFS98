#!/usr/bin/env python3
"""Snapshot and mutate facilities native DLLs; require actual differences."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys


def main():
    output_dir = Path('bin/world-diff/facilities-native-breaks')
    output_dir.mkdir(parents=True, exist_ok=True)
    sources = {path.name: path.read_bytes() for path in Path('src/world').iterdir()
               if path.suffix in ('.c', '.h', '.inc', '.def')}
    for name in ('worldfacilitiesuidiff.c', 'worlduidiff.c'):
        sources[name] = (Path('tools/difftest') / name).read_bytes().replace(b'../../src/world/', b'')
    for name in ('world-res.o', 'libworld-simsui.a'):
        sources[name] = (Path('bin/world-diff') / name).read_bytes()
    (output_dir / 'inputs.json').write_text(json.dumps({name: hashlib.sha256(data).hexdigest() for name, data in sources.items()}, indent=2) + '\n')
    mutations = [
        ('font-cleanup', 'world_facilities.c', 's_services->at_exit(fs98_world_facilities_deinit);', '(void)s_services;'),
        ('highlight-width', 'world_facilities.c', '(int)U32(dib,8)', '(int)U32(dib,12)'),
        ('text-color', 'world_facilities.c', 's_font[2]=0xffffff;', 's_font[2]=0x00ff00;'),
        ('character-font', 'world_facilities.c', 'index?"Verdana":"Arial"', 'index?"Arial":"Verdana"'),
        ('paragraph-tab', 'world_facilities.c', '{1200,2500,4400,6300}', '{1300,2500,4400,6300}'),
        ('paragraph-count', 'world_facilities.c', '(index?5:4);', '(index?4:3);'),
        ('output-heading', 'world_facilities.c', 'string_assign(&line, PTR(row(object, 0x28, offset), 12));', 'string_assign(&line, PTR(row(object, 0x28, offset), 8));'),
        ('output-runway-prefix', 'world_facilities.c', 'string_assign(&line, "H");', 'string_assign(&line, "0");'),
        ('output-runway-surface', 'world_facilities.c', '0xf97c,0xf97d,0xf97f,0xf97e', '0xf97d,0xf97c,0xf97f,0xf97e'),
        ('navaid-type', 'world_facilities.c', 'string_load(&temporary, 0xf982);', 'string_load(&temporary, 0xf986);'),
        ('owner-flags', 'world_facilities.c', 'U32(PTR(object, 0x10dc), 0x20) = 1;', 'U32(PTR(object, 0x10dc), 0x20) = 3;'),
        ('map-image', 'world_facilities.c', 'mode == 10 ? 0x6a4 : 0x6c1', 'mode == 10 ? 0x6c1 : 0x6a4'),
        ('close-answer', 'world_facilities.c', 'U32(PTR(object,0x10dc),0)=0;', 'U32(PTR(object,0x10dc),0)=1;'),
        ('close-output-delete', 'world_facilities.c', 'PTR(PTR(output,0),4))(output,1);', 'PTR(PTR(output,0),4))(output,0);'),
        ('refresh-dispatch', 'world_runtime_tables.inc', 'fs98_world_facilities_refresh(object);', 'fs98_world_facilities_home(object);'),
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
        dll, exe = work / 'WORLD_area.dll', work / 'worldfacilitiesuidiff.exe'
        flags = ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-strict-aliasing', '-fno-delete-null-pointer-checks']
        commands = [flags + ['-shared', '-nostdlib', '-Wl,--entry=_DllMain@12', '-Wl,--disable-dynamicbase', '-Wl,--image-base=0x223f0000', '-o', str(dll),
            *(str(work / f'world_{unit}.c') for unit in ('area', 'objects', 'init', 'priority', 'priority_objects', 'booster', 'booster_objects', 'booster_init', 'search', 'search_objects', 'search_init', 'list', 'list_objects', 'cache', 'cache_objects', 'files', 'files_objects', 'position', 'position_objects', 'facilities', 'facilities_objects', 'airport', 'airport_objects', 'complexity', 'complexity_objects', 'complexity_init', 'dynamic', 'dynamic_objects', 'dynamic_init', 'runtime')),
            str(work / 'world.def'), str(work / 'world-res.o'), str(work / 'libworld-simsui.a'), '-lkernel32', '-luser32', '-lmsvcrt', '-lntdll'],
            flags + ['-o', str(exe), str(work / 'worldfacilitiesuidiff.c'), '-lgdi32', '-lntdll']]
        for command in commands:
            build = subprocess.run(command, capture_output=True, text=True)
            if build.returncode:
                print(f'{name}: BUILD FAILED (not counted)\n{build.stderr}', flush=True)
                return 1
        try:
            result = subprocess.run(['wine', str(exe), str(dll), '4'], capture_output=True, text=True, timeout=240)
        except subprocess.TimeoutExpired as error:
            (work / 'result.log').write_bytes((error.stdout or b'') + (error.stderr or b''))
            print(f'{name}: TIMEOUT (not counted)', flush=True)
            return 1
        output = result.stdout + result.stderr
        (work / 'result.log').write_text(output)
        if result.returncode == 1 and 'FACILITIES UI DIFFER' in output:
            caught += 1
            print(f'{name}: detected', flush=True)
        else:
            print(f'{name}: NOT DETECTED (exit {result.returncode})\n{output}', flush=True)
    print(f'World facilities native mutations: {caught}/{len(selected)} detected', flush=True)
    return 0 if caught == len(selected) else 1


if __name__ == '__main__':
    sys.exit(main())
