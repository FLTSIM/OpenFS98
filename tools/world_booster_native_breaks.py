#!/usr/bin/env python3
"""Booster initializer/native mutations; require mismatches, never crashes."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys


def main():
    output_dir = Path('bin/world-diff/booster-native-breaks')
    output_dir.mkdir(parents=True, exist_ok=True)
    sources = {path.name: path.read_bytes() for path in Path('src/world').iterdir()
               if path.suffix in ('.c', '.h', '.inc', '.def')}
    for name in ('worldboosterdiff.c', 'worldboosterinitdiff.c', 'worldboosteruidiff.c', 'worlduidiff.c'):
        sources[name] = (Path('tools/difftest') / name).read_bytes().replace(b'../../src/world/', b'')
    for name in ('world-res.o', 'libworld-simsui.a'):
        sources[name] = (Path('bin/world-diff') / name).read_bytes()
    (output_dir / 'inputs.json').write_text(json.dumps({name: hashlib.sha256(data).hexdigest() for name, data in sources.items()}, indent=2) + '\n')
    mutations = [
        ('string-unwind', 'init', 'world_booster_init.c', 's_services->ui->string_destroy(state->string);', '(void)state;'),
        ('empty-resource', 'init', 'world_booster_init.c', '0xf8d4, buffer', '0xf8d5, buffer'),
        ('skin-return', 'init', 'world_booster_init.c', '15)) return 0;', '15)) return 1;'),
        ('font-flag-order', 'init', 'world_booster_init.c', '''        s_font_state.flags |= 1;
        font = ui->base_font(8);
        s_services->font_construct(s_font_state.font, font);''', '''        font = ui->base_font(8);
        s_services->font_construct(s_font_state.font, font);
        s_font_state.flags |= 1;'''),
        ('font-base', 'init', 'world_booster_init.c', 'font = ui->base_font(8);', 'font = ui->base_font(2);'),
        ('font-registration', 'init', 'world_booster_init.c', 's_services->at_exit(fs98_world_booster_font_cleanup);', '(void)s_services;'),
        ('font-foreground', 'init', 'world_booster_init.c', 'U32(s_font_state.font, 8) = U32(object, 0x228);', 'U32(s_font_state.font, 8) = U32(object, 0x22c);'),
        ('font-flag-width', 'init', 'world_booster_init.c', '*(uint8_t *)AT(s_font_state.font, 0x58) |= 0x10;', 'U32(s_font_state.font, 0x58) = 0x10;'),
        ('cache-path-limit', 'init', 'world_booster_init.c', 'U32(row, 8) - 1, 0);', 'U32(row, 8), 0);'),
        ('source-path-limit', 'init', 'world_booster_init.c', 'U32(row, 0x1c) - 1, 0);', 'U32(row, 0x1c), 0);'),
        ('disable-omission', 'init', 'world_booster_init.c', 'index < 7; ++index) events->enable(AT(object, s_controls[index]), 0);', 'index < 6; ++index) events->enable(AT(object, s_controls[index]), 0);'),
        ('populate-order', 'init', 'world_booster_init.c', '''    } else {
        for (index = 0; index < 7; ++index) events->enable(AT(object, s_controls[index]), index < 5);
        fs98_world_booster_populate(object);''', '''    } else {
        fs98_world_booster_populate(object);
        for (index = 0; index < 7; ++index) events->enable(AT(object, s_controls[index]), index < 5);'''),
        ('native-color', 'ui', 'world_booster_init.c', '0x1bf8), font, 0xffffff', '0x1bf8), font, 0x00ff00'),
        ('native-size-limit', 'ui', 'world_booster_init.c', '0x1bb0), 0xc5, 7, 0', '0x1bb0), 0xc5, 6, 0'),
        ('native-source-dispatch', 'ui', 'world_runtime_tables.inc', 'fs98_world_booster_path_changed(object);', '(void)object;'),
        ('native-auto-delete', 'ui', 'world_runtime_tables.inc', 'fs98_world_priority_post_destroy(object);', '(void)object;'),
        ('native-error-focus', 'ui', 'world_booster.c', 'ui->focus_control(AT(object, control));', 'ui->focus_control(AT(object, 0x1b90));'),
        ('native-font-cleanup', 'ui', 'world_runtime.c', 's_font_exit = NULL; callback();', 's_font_exit = NULL; (void)callback;'),
    ]
    requested = set(sys.argv[1:])
    if requested - {item[0] for item in mutations}:
        raise ValueError('unknown booster mutation')
    selected = [item for item in mutations if not requested or item[0] in requested]
    caught = 0
    for name, suite, filename, before, after in selected:
        work = output_dir / name
        work.mkdir(exist_ok=True)
        for source_name, data in sources.items():
            if source_name == filename:
                if data.count(before.encode()) != 1:
                    raise ValueError(f'{name}: mutation anchor is not unique')
                data = data.replace(before.encode(), after.encode(), 1)
            (work / source_name).write_bytes(data)
        exe = work / f'worldbooster{suite}diff.exe'
        flags = ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-strict-aliasing', '-fno-delete-null-pointer-checks']
        if suite == 'ui':
            dll = work / 'WORLD_area.dll'
            commands = [flags + ['-shared', '-nostdlib', '-Wl,--entry=_DllMain@12', '-Wl,--disable-dynamicbase', '-Wl,--image-base=0x223f0000', '-o', str(dll),
                *(str(work / f'world_{unit}.c') for unit in ('area', 'objects', 'init', 'priority', 'priority_objects', 'booster', 'booster_objects', 'booster_init', 'search', 'search_objects', 'search_init', 'list', 'list_objects', 'cache', 'cache_objects', 'files', 'files_objects', 'position', 'position_objects', 'runtime')),
                str(work / 'world.def'), str(work / 'world-res.o'), str(work / 'libworld-simsui.a'), '-lkernel32', '-luser32', '-lmsvcrt', '-lntdll'],
                flags + ['-o', str(exe), str(work / 'worldboosteruidiff.c'), '-lgdi32', '-lntdll']]
            run = ['wine', str(exe), str(dll), '10']
            marker = 'BOOSTER UI DIFFER'
        else:
            commands = [flags + ['-o', str(exe), str(work / 'worldboosterinitdiff.c'),
                *(str(work / f'world_{unit}.c') for unit in ('booster_init', 'booster', 'init', 'area')), '-lntdll']]
            run = ['wine', str(exe), '64', 'unwind']
            marker = 'BOOSTER INIT DIFFER'
        for command in commands:
            build = subprocess.run(command, capture_output=True, text=True)
            if build.returncode:
                print(f'{name}: BUILD FAILED (not counted)\n{build.stderr}', flush=True)
                return 1
        try:
            result = subprocess.run(run, capture_output=True, text=True, timeout=180)
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
    print(f'World booster initializer/native mutations: {caught}/{len(selected)} detected', flush=True)
    return 0 if caught == len(selected) else 1


if __name__ == '__main__':
    sys.exit(main())
