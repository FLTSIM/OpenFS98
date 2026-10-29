#!/usr/bin/env python3
"""Deliberate Advanced Weather core/initializer changes; require real differences."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys


def main():
    mutations = [
        ('parent-base-init', 'parent', 'ui->base_init(object);vtable=', '(void)object;vtable='),
        ('parent-skin', 'parent', 's_services->resources(),15)', 's_services->resources(),14)'),
        ('parent-art', 'parent', '0x2358),0xf831,', '0x2358),0xf830,'),
        ('parent-buttons', 'parent', 'index<2?0x132:0x137', 'index<2?0x137:0x132'),
        ('parent-tab-font', 'parent', 'tab,object,ui->base_font(0)', 'tab,object,ui->base_font(1)'),
        ('parent-tab-order', 'parent', '0x263c,0x2648,0x2640,0x2644,0x264c', '0x2648,0x263c,0x2640,0x2644,0x264c'),
        ('parent-bind-owner', 'parent', 'PTR(page,bases[index]+4)=PTR(object,0x1104);', 'PTR(page,bases[index]+4)=object;'),
        ('parent-cancel-mode', 'parent', 'U32(object,0x2654)=2;', 'U32(object,0x2654)=1;'),
        ('parent-accept-answer', 'parent', 'U32(PTR(object,0x1104),0)=1;', 'U32(PTR(object,0x1104),0)=0;'),
        ('parent-error-vtable', 'parent', 'vtable[0xdc/4])(page,value)', '((void **)PTR(page,0))[0xdc/4])(page,value)'),
        ('parent-notify-deletion', 'parent', 'PTR(PTR(notify,0),4))(notify,1);', 'PTR(PTR(notify,0),4))(notify,0);'),
        ('visibility-index', 'parent', '20-value:20;', 'value:20;'),
        ('pressure-check-raw', 'parent', '0xd8)=U32(object,0x1598);', '0xd8)=!!U32(object,0x1598);'),
        ('pressure-error', 'parent', 'result=control==0x1474?0x1b03:0x1b04;', 'result=control==0x1474?0x1b04:0x1b03;'),
        ('pressure-edit-flags', 'parent', '0x1474,0x1354,0x15,5', '0x1474,0x1354,0x11,5'),
        ('temperature-list-item', 'temperature', '0x19a,row,(uint32_t)index', '0x19a,row,(uint32_t)index ^ 1'),
        ('temperature-layer-clamp', 'temperature', 'return index<=2?index:3;', 'return index<=2?index:0;'),
        ('temperature-check-raw', 'temperature', '0xbc)=U32(object,0x1950);', '0xbc)=!!U32(object,0x1950);'),
        ('temperature-owner-add', 'temperature', 'advanced_callback(object,0x11f4,0x3fc);', 'advanced_callback(object,0x11f4,0x400);'),
        ('temperature-validator-result', 'temperature', 'U32(object,0x11ec)=value;', 'U32(object,0x11ec)=!!value;'),
        ('temperature-error-id', 'temperature', 'if(value==0x100e)result=0x1ae1;', 'if(value==0x100e)result=0x1aed;'),
        ('temperature-edit-limit', 'temperature', '0x182c,0x16a4,0x11,6', '0x182c,0x16a4,0x11,5'),
        ('temperature-units', 'temperature', 'units?0xf8ef:0xf8f0', 'units?0xf8f0:0xf8ef'),
        ('temperature-notify-cleanup', 'temperature', 'state.cleanup=kind?0x25850:0x257f4;', 'state.cleanup=0x25850;'),
    ]
    chosen = [row for row in mutations if len(sys.argv) == 1 or row[0] in sys.argv[1:]]
    if set(sys.argv[1:]) - {row[0] for row in mutations}:
        raise ValueError('unknown core mutation')
    output = Path('bin/world-diff/advanced-core-breaks')
    output.mkdir(parents=True, exist_ok=True)
    inputs = [*Path('src/world').glob('*.h'), *Path('src/world').glob('world_advanced*'),
              Path('tools/difftest/worldadvancedparentdiff.c'), Path('tools/difftest/worldadvancedtemperaturediff.c')]
    snapshots = {path.name: path.read_bytes().replace(b'../../src/world/', b'') for path in inputs}
    (output / 'inputs.json').write_text(json.dumps({name: hashlib.sha256(data).hexdigest() for name, data in snapshots.items()}, indent=2) + '\n')
    results = []
    for name, kind, before, after in chosen:
        work = output / name
        work.mkdir(exist_ok=True)
        for filename, data in snapshots.items():
            (work / filename).write_bytes(data)
        filename = 'world_advanced.c' if kind == 'parent' or name == 'temperature-notify-cleanup' else 'world_advanced_temperature.c'
        path = work / filename
        source = path.read_text()
        expected = {'parent-notify-deletion': 3, 'pressure-check-raw': 2, 'temperature-check-raw': 2, 'temperature-owner-add': 2}.get(name, 1)
        if source.count(before) != expected:
            raise ValueError(f'{name}: {source.count(before)} anchors, expected {expected}')
        path.write_text(source.replace(before, after))
        executable = work / 'mutant.exe'
        executable.unlink(missing_ok=True)
        build = subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                                '-fno-strict-aliasing', '-fno-delete-null-pointer-checks', '-o', str(executable),
                                str(work / f'worldadvanced{kind}diff.c'), str(work / 'world_advanced.c'),
                                str(work / 'world_advanced_temperature.c'), str(work / 'world_advanced_objects.c'), '-lntdll'], capture_output=True)
        (work / 'build.log').write_bytes(build.stdout + build.stderr)
        if build.returncode:
            result = 'BUILD FAILURE'
        else:
            run = subprocess.run(['timeout', '60s', 'wine', str(executable), '64', '0x4a971b63', 'unwind'], capture_output=True)
            log = run.stdout + run.stderr
            (work / 'run.log').write_bytes(log)
            result = 'DETECTED' if run.returncode == 1 and b'DIFFER ' in log and b'FAULT ' not in log else f'NOT DETECTED (exit {run.returncode})'
        print(f'{name}: {result}', flush=True)
        results.append({'name': name, 'result': result, 'executable_sha256': hashlib.sha256(executable.read_bytes()).hexdigest() if executable.exists() else None})
    (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    caught = sum(row['result'] == 'DETECTED' for row in results)
    print(f'Advanced Weather core/init mutations: {caught}/{len(results)} actual differences', flush=True)
    return int(caught != len(results))


if __name__ == '__main__':
    sys.exit(main())
