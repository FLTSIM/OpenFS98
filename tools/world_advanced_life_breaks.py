#!/usr/bin/env python3
"""Require observable retail differences for Advanced Weather lifetime mutations."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys


def main():
    mutations = [
        ('base-template', 'object, state.class->template_id, parent);', 'object, 0x6a5, parent);', 2),
        ('child-construction-order', '&state.class->controls[state.constructed]', '&state.class->controls[state.class->count - state.constructed - 1]', 1),
        ('temperature-ctor-separator', 'if (kind == WORLD_ADVANCED_TEMPERATURE) PTR(object, 0x195c) = s_resolve(0x25758);\n    PTR(object, 0)', 'if (0) PTR(object, 0x195c) = s_resolve(0x25758);\n    PTR(object, 0)', 1),
        ('temperature-dtor-separator', 'if (kind == WORLD_ADVANCED_TEMPERATURE) PTR(object, 0x195c) = s_resolve(0x25758);\n    advanced_destroy_remaining', 'if (0) PTR(object, 0x195c) = s_resolve(0x25758);\n    advanced_destroy_remaining', 1),
        ('parent-notify', 'if (kind == WORLD_ADVANCED_PARENT) U32(object, 0x2638) = 0;', 'if (kind == WORLD_ADVANCED_PARENT) U32(object, 0x2638) = 1;', 1),
        ('temperature-notify', 'if (kind == WORLD_ADVANCED_TEMPERATURE) U32(object, 0x1208) = 0;', 'if (kind == WORLD_ADVANCED_TEMPERATURE) U32(object, 0x1208) = 1;', 1),
        ('wind-notify', 'if (kind == WORLD_ADVANCED_WIND) U32(object, 0x1204) = 0;', 'if (kind == WORLD_ADVANCED_WIND) U32(object, 0x1204) = 1;', 1),
        ('child-destruction-order', '&state->class->controls[--state->constructed]', '&state->class->controls[state->class->count - state->constructed--]', 1),
        ('base-destruction', 's_services->destroy(state->object, state->class->base_dtor);', '(void)state;', 1),
        ('parent-dtor-vtable', 'if (!kind) PTR(object, 0) = s_vtables[0];', 'if (0) PTR(object, 0) = s_vtables[0];', 1),
        ('delete-flag', 'if (flags & 1)', 'if (flags & 2)', 1),
        ('factory-allocation', 'allocate(0x2658)', 'allocate(0x2654)', 1),
        ('factory-parent', 'fs98_world_advanced_construct(object, NULL, WORLD_ADVANCED_PARENT)', 'fs98_world_advanced_construct(object, (void *)parent, WORLD_ADVANCED_PARENT)', 1),
        ('factory-module', 'U32(object, 0x80) = s_module;', 'U32(object, 0x80) = 0;', 1),
        ('factory-create', 's_services->create(object, 0x6a5, NULL);', 's_services->create(object, 0x6a7, NULL);', 1),
        ('factory-show', 's_services->show(object, 1);', 's_services->show(object, 0);', 1),
        ('factory-owner', 'state.live = 0;\n    s_services->create', 'state.live = 1;\n    s_services->create', 1),
        ('factory-cleanup', 's_services->dialog_delete(state->object);', 's_services->ui_delete(state->object);', 1),
        ('factory-return', 'return U32(object, 0x20);', 'return U32(object, 0x24);', 1),
        ('ddx-base', 'if (!kind) s_base_exchange(object, exchange);', 'if (0) s_base_exchange(object, exchange);', 1),
        ('ddx-control', 's_advanced_classes[kind].ddx[index][1]', 's_advanced_classes[kind].ddx[index][1] ^ 1', 1),
        ('context-result', 'fs98_world_advanced_help(object, window, 10, kind);\n    return 1;', 'fs98_world_advanced_help(object, window, 10, kind);\n    return 0;', 1),
        ('post-delete-flags', 'PTR(PTR(object, 0), 4))(object, 1);', 'PTR(PTR(object, 0), 4))(object, 0);', 1),
        ('help-command', '"fs98.hlp", command,', '"fs98.hlp", command ^ 1,', 1),
    ]
    selected = [item for item in mutations if len(sys.argv) == 1 or item[0] in sys.argv[1:]]
    if set(sys.argv[1:]) - {item[0] for item in mutations}:
        raise ValueError('unknown mutation')
    output = Path('bin/world-diff/advanced-life-breaks')
    output.mkdir(parents=True, exist_ok=True)
    inputs = [Path('src/world/world_advanced_objects.c'), Path('tools/difftest/worldadvancedlifediff.c'),
              *Path('src/world').glob('*.h'), *Path('src/world').glob('world_advanced_*.inc')]
    snapshots = {path: path.read_bytes() for path in inputs}
    hashes = {str(path): hashlib.sha256(data).hexdigest() for path, data in snapshots.items()}
    (output / 'inputs.json').write_text(json.dumps(hashes, indent=2) + '\n')
    caught = 0
    results = []
    for name, before, after, expected in selected:
        work = output / name
        work.mkdir(exist_ok=True)
        for path, data in snapshots.items():
            (work / path.name).write_bytes(data.replace(b'../../src/world/', b''))
        source = work / 'world_advanced_objects.c'
        text = source.read_text()
        if text.count(before) != expected:
            raise ValueError(f'{name}: expected {expected} anchors, found {text.count(before)}')
        source.write_text(text.replace(before, after))
        executable = work / 'worldadvancedlifediff.exe'
        executable.unlink(missing_ok=True)
        build = subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                                '-fno-strict-aliasing', '-fno-delete-null-pointer-checks', '-o', str(executable),
                                str(work / 'worldadvancedlifediff.c'), str(source), '-lntdll'], capture_output=True)
        (work / 'build.log').write_bytes(build.stdout + build.stderr)
        if build.returncode:
            print(f'{name}: BUILD FAILURE', flush=True)
            results.append({'name': name, 'result': 'build failure'})
            continue
        result = subprocess.run(['timeout', '30s', 'wine', str(executable), '12', '0x4a971b63', 'unwind'], capture_output=True)
        log = result.stdout + result.stderr
        (work / 'run.log').write_bytes(log)
        detected = result.returncode == 1 and b'DIFFER ' in log and b'FAULT ' not in log
        caught += detected
        status = 'DETECTED' if detected else f'NOT DETECTED (exit {result.returncode})'
        print(f'{name}: {status}', flush=True)
        results.append({'name': name, 'result': status,
                        'executable_sha256': hashlib.sha256(executable.read_bytes()).hexdigest()})
    (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    print(f'Advanced Weather lifetime mutations: {caught}/{len(selected)} actual differences', flush=True)
    return int(caught != len(selected))


if __name__ == '__main__':
    sys.exit(main())
