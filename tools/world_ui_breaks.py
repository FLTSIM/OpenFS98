#!/usr/bin/env python3
"""Snapshot and mutate native area DLLs; faults/timeouts never count as caught."""
import sys
sys.dont_write_bytecode = True    # before any local import, so no .pyc is written into the tree
import break_build_only as bo
import world_native
from pathlib import Path
import subprocess
import sys

MUTATIONS = [
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


def make_case(data, name, filename, before, after):
    builds = lambda work: [world_native.dll_command(work, work / 'WORLD_area.dll'), world_native.ui_command(work, work / 'worlduidiff.exe', 'worlduidiff.c')]
    if name == 'baseline':
        return bo.Case(name, lambda: {}, builds, ['WORLD_area.dll', 'worlduidiff.exe'])
    return world_native.case(name, data, filename, lambda blob: world_native.mutate(blob, name, filename, before, after),
                             builds, ['WORLD_area.dll', 'worlduidiff.exe'])


def main():
    args = sys.argv[1:]
    jobs = [('baseline', None, None, None)] + MUTATIONS
    if '--build-only' in args:
        def factory():
            data = world_native.sources(('worlduidiff.c',))
            return [make_case(data, *job) for job in jobs], data
        return bo.build_only(args[args.index('--build-only') + 1:], factory)
    output_dir = Path('bin/world-diff/ui-breaks')
    output_dir.mkdir(parents=True, exist_ok=True)
    data = world_native.sources(('worlduidiff.c',))
    world_native.write_inputs_json(output_dir, data)
    requested = set(args)
    if requested - {mutation[0] for mutation in MUTATIONS}:
        raise ValueError('unknown World native mutation')
    jobs = [('baseline', None, None, None)] + [mutation for mutation in MUTATIONS if not requested or mutation[0] in requested]
    caught = 0
    for name, filename, before, after in jobs:
        case = make_case(data, name, filename, before, after)
        work = output_dir / name
        work.mkdir(exist_ok=True)
        bo.write_case(work, data, case.overlay())
        dll, harness = work / 'WORLD_area.dll', work / 'worlduidiff.exe'
        for command in case.builds(work):
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
