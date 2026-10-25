#!/usr/bin/env python3
"""Require reported differences for isolated Alerts lifecycle/cleanup mutations."""
from pathlib import Path
import subprocess
import sys


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    if count < 100:
        raise ValueError('use at least 100 scenarios')
    directory = Path('bin/alerts-diff/object-breaks')
    directory.mkdir(parents=True, exist_ok=True)
    breaks = [
        ('modal-template', 'objects', 'normal', '0x8a, parent', '0x8b, parent'),
        ('modeless-template', 'objects', 'normal', '0x8b, parent', '0x8c, parent'),
        ('advisory-template', 'objects', 'normal', '0x8c, parent', '0x8a, parent'),
        ('modal-button-order', 'objects', 'normal', '0x11b4 + index * 0xfc', '0x11b4 + (2 - index) * 0xfc'),
        ('modal-answer-initial', 'objects', 'normal', '0x10f8) = 0xffffffff', '0x10f8) = 0'),
        ('modeless-progress-vtable', 'objects', 'normal', '= s_tables.progress;', '= s_tables.modeless;'),
        ('advisory-vtable', 'objects', 'normal', '= s_tables.advisory;', '= s_tables.modal;'),
        ('deleting-flag', 'objects', 'normal', 'if (flags & 1)', 'if (flags & 2)'),
        ('progress-allocator', 'objects', 'normal', 's_services->mfc_delete(object);', 's_services->ui_delete(object);'),
        ('post-destroy-flag', 'objects', 'normal', 'vtable[1](object, 1)', 'vtable[1](object, 0)'),
        ('enable-argument', 'objects', 'normal', 'enable(D32(object, 0x20), 1)', 'enable(D32(object, 0x20), 0)'),
        ('factory-size', 'objects', 'normal', 'descriptor, 0x14a8,', 'descriptor, 0x14a4,'),
        ('factory-parent', 'objects', 'normal', 'construct(object, 0)', 'construct(object, (void *)1)'),
        ('factory-show', 'objects', 'normal', 'show(object, 1)', 'show(object, 0)'),
        ('factory-module', 'objects', 'normal', '0x10e0) = s_module;', '0x10e0) = s_module ^ 1;'),
        ('factory-unwind-delete', 'objects', 'unwind', 'dialog_delete(object)', 'ui_delete(object)'),
        ('modal-unwind-buttons', 'objects', 'unwind', 'state.buttons++;', 'state.buttons = 0;'),
        ('modeless-unwind-window', 'objects', 'unwind', 'state.stage = 2;\n    PTR(object, 0x10e4)', 'state.stage = 1;\n    PTR(object, 0x10e4)'),
        ('advisory-unwind-text', 'objects', 'unwind', 'state.stage = 5;', 'state.stage = 4;'),
        ('event-unwind-dib', 'events', 'unwind', 'if (state->kind == 1) s_services->dib_destroy(state->second);', 'if (state->kind == 1) s_services->dib_destroy(state->first);'),
        ('event-unwind-string', 'events', 'unwind', 'else s_services->string_destroy(state->first);', 'else s_services->string_destroy(state->second);'),
        ('event-unwind-dc', 'events', 'unwind', 'else s_services->dc_destroy(state->second);', 'else s_services->dc_destroy(state->first);'),
    ]
    caught = 0
    for name, suite, mode, before, after in breaks:
        source_text = Path(f'src/alerts/alerts_{suite}.c').read_text()
        if before not in source_text:
            raise ValueError(f'{name}: mutation anchor missing')
        source = directory / f'{name}.c'
        executable = directory / f'{name}.exe'
        source.write_text(source_text.replace(before, after, 1))
        build = subprocess.run([
            'i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra',
            '-fno-strict-aliasing', '-fno-delete-null-pointer-checks', '-Isrc/alerts',
            '-o', str(executable), f'tools/difftest/alerts{suite}diff.c',
            str(source), '-lntdll'], text=True, capture_output=True)
        if build.returncode:
            print(f'{name}: BUILD FAILED\n{build.stderr}', flush=True)
            continue
        try:
            run = subprocess.run(['wine', str(executable), str(count),
                                   '0x587a41e9', mode],
                                  text=True, capture_output=True, timeout=45)
        except subprocess.TimeoutExpired:
            print(f'{name}: TIMEOUT (not counted)', flush=True)
            continue
        output_text = run.stdout + run.stderr
        (directory / f'{name}.log').write_text(output_text)
        if run.returncode == 1 and 'DIFFER scenario=' in output_text:
            caught += 1
            print(f'{name}: caught', flush=True)
        else:
            print(f'{name}: NOT CAUGHT (exit {run.returncode})\n{output_text}', flush=True)
    print(f'Alerts lifecycle/cleanup mutations: {caught}/{len(breaks)} caught')
    return 0 if caught == len(breaks) else 1


if __name__ == '__main__':
    sys.exit(main())
