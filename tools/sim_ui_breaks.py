#!/usr/bin/env python3
"""Require native Sim differences for rendering, dispatch and lifetime errors.

Snapshots all module sources and the harness before mutation builds. Only exit
1 with an actual comparison mismatch counts; faults and timeouts fail the run.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import sys


UNITS = ('events', 'objects', 'crash', 'engines', 'systems', 'realism', 'fuel', 'settings', 'runtime')


def main():
    directory = Path('bin/sim-diff/ui-breaks')
    directory.mkdir(parents=True, exist_ok=True)
    sources = {path.name: path.read_bytes() for path in Path('src/sim').iterdir()
               if path.suffix in ('.c', '.h', '.inc', '.def')}
    sources['simuidiff.c'] = Path('tools/difftest/simuidiff.c').read_bytes().replace(b'../../src/sim/', b'')
    for name in ('sim-res.o', 'libsim-simsui.a'):
        sources[name] = (Path('bin/sim-diff') / name).read_bytes()
    (directory / 'inputs.json').write_text(json.dumps({name: hashlib.sha256(data).hexdigest()
        for name, data in sources.items()}, indent=2) + '\n')
    breaks = [
        ('realism-edit-color', 'sim_realism.c', 'font,0xffffff,0,0xff0000', 'font,0x00ff00,0,0xff0000'),
        ('fuel-edit-color', 'sim_fuel.c', 'font, 0xffffff, 0, 0xff0000', 'font, 0x00ff00, 0, 0xff0000'),
        ('crash-axis-layout', 'sim_crash.c', 'rect.left=26;rect.top=64;', 'rect.left=36;rect.top=64;'),
        ('engine-combo-recipe', 'sim_engines.c', 'strings=s_engine_turbine_strings', 'strings=s_engine_piston_strings'),
        ('crash-message-dispatch', 'sim_runtime_tables.inc',
         'return fs98_sim_crash_tick(object, first, second);',
         'return fs98_sim_zero_message(object, first, second);'),
        ('settings-cancel-answer', 'sim_events.c',
         'D32(object,0x2268)=2; D32(descriptor,0)=0;',
         'D32(object,0x2268)=2; D32(descriptor,0)=1;'),
        ('module-unwind-delete', 'sim_runtime.c', 'rt_mfc_delete(state->dynamic);', '(void)state;'),
        ('module-deinit-clear', 'sim_runtime.c', 's_module = s_resource_module = 0;', 's_resource_module = 0;'),
    ]
    requested = set(sys.argv[1:])
    if requested - {item[0] for item in breaks}:
        raise ValueError('unknown native mutation')
    jobs = [('baseline', None, None, None)] + [item for item in breaks if not requested or item[0] in requested]
    caught = 0
    for name, filename, before, after in jobs:
        work = directory / name
        work.mkdir(exist_ok=True)
        for source_name, data in sources.items():
            if source_name == filename:
                if before.encode() not in data:
                    raise ValueError(f'{name}: mutation anchor missing')
                data = data.replace(before.encode(), after.encode(), 1)
            (work / source_name).write_bytes(data)
        dll = work / 'SIM_ours.dll'
        harness = work / 'simuidiff.exe'
        commands = [
            ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
             '-fno-strict-aliasing', '-fno-delete-null-pointer-checks', '-shared', '-nostdlib',
             '-Wl,--entry=_DllMain@12', '-Wl,--disable-dynamicbase', '-Wl,--image-base=0x223c0000',
             '-o', str(dll), *(str(work / f'sim_{unit}.c') for unit in UNITS), str(work / 'sim.def'),
             str(work / 'sim-res.o'), str(work / 'libsim-simsui.a'), '-lkernel32', '-luser32', '-lmsvcrt'],
            ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
             '-fno-strict-aliasing', '-o', str(harness), str(work / 'simuidiff.c'), '-lgdi32', '-lntdll'],
        ]
        built = True
        for command in commands:
            build = subprocess.run(command, text=True, capture_output=True)
            if build.returncode:
                print(f'{name}: BUILD FAILED (not counted)\n{build.stderr}', flush=True)
                built = False
                break
        if not built:
            return 1
        try:
            run = subprocess.run(['wine', str(harness), str(dll), '10'],
                                 text=True, capture_output=True, timeout=120)
        except subprocess.TimeoutExpired as error:
            (work / 'result.log').write_bytes((error.stdout or b'') + (error.stderr or b''))
            print(f'{name}: TIMEOUT (not counted)', flush=True)
            return 1
        output = run.stdout + run.stderr
        (work / 'result.log').write_text(output)
        if name == 'baseline':
            if run.returncode or '0 differ' not in output:
                print(f'baseline: FAILED\n{output}', flush=True)
                return 1
            print('baseline: passed', flush=True)
        elif run.returncode == 1 and ('SIM UI DIFFER' in output or 'INIT DIFFER' in output):
            caught += 1
            print(f'{name}: caught', flush=True)
        else:
            print(f'{name}: NOT CAUGHT (exit {run.returncode})\n{output}', flush=True)
    print(f'Sim native UI/module mutations: {caught}/{len(jobs) - 1} caught', flush=True)
    return 0 if caught == len(jobs) - 1 else 1


if __name__ == '__main__':
    sys.exit(main())
