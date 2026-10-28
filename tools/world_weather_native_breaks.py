#!/usr/bin/env python3
"""Snapshot and mutate weather native DLLs; require actual differences."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys


def main():
    output_dir = Path('bin/world-diff/weather-native-breaks')
    output_dir.mkdir(parents=True, exist_ok=True)
    sources = {path.name: path.read_bytes() for path in Path('src/world').iterdir()
               if path.suffix in ('.c', '.h', '.inc', '.def')}
    for name in ('worldweatheruidiff.c', 'worlduidiff.c'):
        sources[name] = (Path('tools/difftest') / name).read_bytes().replace(b'../../src/world/', b'')
    for name in ('world-res.o', 'libworld-simsui.a'):
        sources[name] = (Path('bin/world-diff') / name).read_bytes()
    sources['baseline.dll'] = Path('bin/world-diff/WORLD_area.dll').read_bytes()
    (output_dir / 'inputs.json').write_text(json.dumps({name: hashlib.sha256(data).hexdigest() for name, data in sources.items()}, indent=2) + '\n')
    mutations = [
        ('artwork', 'world_weather.c', '0x1874), 0xf831,', '0x1874), 0xf830,'),
        ('heading-color', 'world_weather.c', 'font[2] = 0xffffff;', 'font[2] = 0x00ff00;'),
        ('combo-font', 'world_weather.c', 's_combos[index]), AT(object, 0x3e0)', 's_combos[index]), AT(object, 0x370)'),
        ('combo-label', 'world_weather.c', 's_combos[index]), AT(object, 0x3e0), AT(object, s_labels[index])', 's_combos[index]), AT(object, 0x3e0), AT(object, 0x12c4)'),
        ('populate-field', 'world_weather.c', '0x1634, 0x14e, U32(PTR(object, 0x10e0), 0x14)', '0x1634, 0x14e, U32(PTR(object, 0x10e0), 0x18)'),
        ('collect-default', 'world_weather.c', 'selection <= 3 ? (uint32_t)selection : 4;', 'selection <= 3 ? (uint32_t)selection : 0;'),
        ('disabled-group', 'world_weather.c', 'if (U32(PTR(object, 0x10e0), 0xc))', 'if (U32(PTR(object, 0x10e0), 0x10))'),
        ('changed-dispatch', 'world_runtime_tables.inc', 'fs98_world_weather_changed(object);', '(void)object;'),
        ('accept-validation', 'world_weather.c', 'if (!fs98_world_weather_validate(object) || !fs98_world_weather_error(object))', 'if (1)'),
        ('accept-answer', 'world_weather.c', 'U32(descriptor, 0) = 1;', 'U32(descriptor, 0) = 0;'),
        ('cancel-mode', 'world_weather.c', '0x195c) = 2;', '0x195c) = 1;'),
        ('measure-dispatch', 'world_runtime_tables.inc', '(void)fs98_world_weather_measure(object, id, measure);', '(void)object; (void)id; (void)measure;'),
        ('post-destroy', 'world_runtime_tables.inc', 'fs98_world_priority_post_destroy(object);', '(void)object;'),
        ('font-cleanup', 'world_runtime.c', 's_font_exit = NULL; callback();', 's_font_exit = NULL; (void)callback;'),
    ]
    requested = set(sys.argv[1:])
    if requested - {item[0] for item in mutations}:
        raise ValueError('unknown weather native mutation')
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
        dll, exe = work / 'WORLD_area.dll', work / 'worldweatheruidiff.exe'
        flags = ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-strict-aliasing', '-fno-delete-null-pointer-checks']
        commands = [flags + ['-shared', '-nostdlib', '-Wl,--entry=_DllMain@12', '-Wl,--disable-dynamicbase', '-Wl,--image-base=0x223f0000', '-o', str(dll),
            *(str(work / f'world_{unit}.c') for unit in ('area', 'objects', 'init', 'priority', 'priority_objects', 'booster', 'booster_objects', 'booster_init', 'search', 'search_objects', 'search_init', 'list', 'list_objects', 'cache', 'cache_objects', 'files', 'files_objects', 'position', 'position_objects', 'facilities', 'facilities_objects', 'airport', 'airport_objects', 'complexity', 'complexity_objects', 'complexity_init', 'dynamic', 'dynamic_objects', 'dynamic_init', 'copy', 'copy_objects', 'copy_init', 'weather', 'weather_objects', 'runtime')),
            str(work / 'world.def'), str(work / 'world-res.o'), str(work / 'libworld-simsui.a'), '-lkernel32', '-luser32', '-lmsvcrt', '-lntdll'],
            flags + ['-o', str(exe), str(work / 'worldweatheruidiff.c'), '-lgdi32', '-lntdll']]
        for command in commands:
            build = subprocess.run(command, capture_output=True, text=True)
            if build.returncode:
                print(f'{name}: BUILD FAILED (not counted)\n{build.stderr}', flush=True)
                return 1
        (work / 'artifacts.json').write_text(json.dumps({path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in (dll, exe)}, indent=2) + '\n')
        try:
            result = subprocess.run(['wine', str(exe), str(dll), '3'], capture_output=True, text=True, timeout=180)
        except subprocess.TimeoutExpired as error:
            (work / 'result.log').write_bytes((error.stdout or b'') + (error.stderr or b''))
            print(f'{name}: TIMEOUT (not counted)', flush=True)
            return 1
        output = result.stdout + result.stderr
        (work / 'result.log').write_text(output)
        if result.returncode == 1 and 'WEATHER UI DIFFER' in output:
            caught += 1
            print(f'{name}: detected', flush=True)
        else:
            print(f'{name}: NOT DETECTED (exit {result.returncode})\n{output}', flush=True)
    print(f'World weather native mutations: {caught}/{len(selected)} detected', flush=True)
    return 0 if caught == len(selected) else 1


if __name__ == '__main__':
    sys.exit(main())
