#!/usr/bin/env python3
"""Require native Help differences for rendering, dispatch and lifetime errors.

Snapshots all module sources and the harness before mutation builds. Only exit
1 with an actual comparison mismatch counts; faults and timeouts fail the run.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import sys


UNITS = ('events', 'objects', 'runtime')


def main():
    directory = Path('bin/help-diff/ui-breaks')
    directory.mkdir(parents=True, exist_ok=True)
    sources = {path.name: path.read_bytes() for path in Path('src/help').iterdir()
               if path.suffix in ('.c', '.h', '.inc', '.def', '.S')}
    sources['helpuidiff.c'] = Path('tools/difftest/helpuidiff.c').read_bytes().replace(b'../../src/help/', b'')
    for name in ('help-res.o', 'libhelp-simsui.a'):
        sources[name] = (Path('bin/help-diff') / name).read_bytes()
    (directory / 'inputs.json').write_text(json.dumps({name: hashlib.sha256(data).hexdigest()
        for name, data in sources.items()}, indent=2) + '\n')
    breaks = [
        ('about-font-color', 'help_events.c', 'D32(s_about_state->font, 8) = 0xffffff;',
         'D32(s_about_state->font, 8) = 0x00ff00;'),
        ('registry-failure-text', 'help_events.c', 'copy_text(name, "Registry Error")',
         'copy_text(name, "Broken Registry")'),
        ('lessons-edit-color', 'help_events.c', 'font, 0xffffff, 0, 0xff0000', 'font, 0x00ff00, 0, 0xff0000'),
        ('credits-timer-counter', 'help_events.c', '++D32(object, 0x10f4);', 'D32(object, 0x10f4) += 2;'),
        ('graph-message-operation', 'help_events.c', 'fs98_help_graph_call(object, 1);',
         'fs98_help_graph_call(object, 0);'),
        ('completion-answer', 'help_events.c',
         'fs98_help_completion_accept(void *object) { fs98_help_answer(object, 0x10dc, 0, 1); }',
         'fs98_help_completion_accept(void *object) { fs98_help_answer(object, 0x10dc, 0, 0); }'),
        ('module-unwind-delete', 'help_runtime.c', 'rt_mfc_delete(state->dynamic);', '(void)state;'),
        ('module-deinit-clear', 'help_runtime.c', 's_module = s_resource_module = 0;', 's_resource_module = 0;'),
        ('cached-font-unload', 'help_runtime.c', 's_exit_handler = 0; handler();',
         's_exit_handler = 0; (void)handler;'),
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
        dll = work / 'HELP_ours.dll'
        harness = work / 'helpuidiff.exe'
        commands = [
            ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
             '-fno-strict-aliasing', '-fno-delete-null-pointer-checks', '-shared', '-nostdlib',
             '-Wl,--entry=_DllMain@12', '-Wl,--disable-dynamicbase', '-Wl,--image-base=0x22330000',
             '-o', str(dll), *(str(work / f'help_{unit}.c') for unit in UNITS), str(work / 'help_stack.S'), str(work / 'help.def'),
             str(work / 'help-res.o'), str(work / 'libhelp-simsui.a'), '-lkernel32', '-luser32', '-ladvapi32', '-lcomctl32', '-lmsvcrt'],
            ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
             '-fno-strict-aliasing', '-o', str(harness), str(work / 'helpuidiff.c'), '-lgdi32', '-lntdll'],
        ]
        built = True
        for command in commands:
            build = subprocess.run(command, text=True, errors='replace', capture_output=True)
            if build.returncode:
                print(f'{name}: BUILD FAILED (not counted)\n{build.stderr}', flush=True)
                built = False
                break
        if not built:
            return 1
        try:
            run = subprocess.run(['wine', str(harness), str(dll), '10'],
                                 text=True, errors='replace', capture_output=True, timeout=120)
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
        elif run.returncode == 1 and ('HELP UI DIFFER' in output or 'INIT DIFFER' in output):
            caught += 1
            print(f'{name}: caught', flush=True)
        else:
            print(f'{name}: NOT CAUGHT (exit {run.returncode})\n{output}', flush=True)
    print(f'Help native UI/module mutations: {caught}/{len(jobs) - 1} caught', flush=True)
    return 0 if caught == len(jobs) - 1 else 1


if __name__ == '__main__':
    sys.exit(main())
