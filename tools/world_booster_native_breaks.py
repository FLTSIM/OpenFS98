#!/usr/bin/env python3
"""Booster initializer/native mutations; require mismatches, never crashes."""
import sys
sys.dont_write_bytecode = True    # before any local import, so no .pyc is written into the tree
import break_build_only as bo
import world_native
from world_native import FLAGS
from pathlib import Path
import subprocess
import sys

MUTATIONS = [
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


def make_case(data, row):
    name, suite, filename, before, after = row
    exe_name = f'worldbooster{suite}diff.exe'

    def builds(work):
        exe = work / exe_name
        if suite == 'ui':
            return [world_native.dll_command(work, work / 'WORLD_area.dll'), world_native.ui_command(work, exe, 'worldboosteruidiff.c')]
        return [[*FLAGS, '-o', str(exe), str(work / 'worldboosterinitdiff.c'),
                 *(str(work / f'world_{unit}.c') for unit in ('booster_init', 'booster', 'init', 'area')), '-lntdll']]
    return world_native.case(f'{suite}.{name}', data, filename, lambda blob: world_native.mutate(blob, name, filename, before, after),
                             builds, ['WORLD_area.dll', exe_name] if suite == 'ui' else [exe_name])


def main():
    harnesses = ('worldboosterdiff.c', 'worldboosterinitdiff.c', 'worldboosteruidiff.c', 'worlduidiff.c')
    args = sys.argv[1:]
    if '--build-only' in args:
        def factory():
            data = world_native.sources(harnesses)
            return [make_case(data, row) for row in MUTATIONS], data
        return bo.build_only(args[args.index('--build-only') + 1:], factory)
    output_dir = Path('bin/world-diff/booster-native-breaks')
    output_dir.mkdir(parents=True, exist_ok=True)
    data = world_native.sources(harnesses)
    world_native.write_inputs_json(output_dir, data)
    requested = set(args)
    if requested - {item[0] for item in MUTATIONS}:
        raise ValueError('unknown booster mutation')
    selected = [item for item in MUTATIONS if not requested or item[0] in requested]
    caught = 0
    for row in selected:
        name, suite = row[0], row[1]
        case = make_case(data, row)
        work = output_dir / name
        work.mkdir(exist_ok=True)
        bo.write_case(work, data, case.overlay())
        exe = work / f'worldbooster{suite}diff.exe'
        commands = case.builds(work)
        if suite == 'ui':
            dll = work / 'WORLD_area.dll'
            run = ['wine', str(exe), str(dll), '10']
            marker = 'BOOSTER UI DIFFER'
        else:
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
