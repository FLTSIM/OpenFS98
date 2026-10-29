#!/usr/bin/env python3
"""Snapshot and build native Advanced Weather mutations; faults never count."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys


def main():
    mutations = [
        ('parent-artwork', 'world_advanced.c', '0x2358),0xf831,', '0x2358),0xf830,', 0),
        ('parent-tab-order', 'world_advanced.c', '0x263c,0x2648,0x2640,0x2644,0x264c', '0x2648,0x263c,0x2640,0x2644,0x264c', 0),
        ('parent-tab-close', 'world_advanced.c', '((void(TC *)(void *))PTR(PTR(page,0),0xc4))(page);', '(void)page;', 0),
        ('parent-notify-delete', 'world_advanced.c', 'if(notify)((void *(TC *)(void *,uint32_t))PTR(PTR(notify,0),4))(notify,1);', 'if(0)((void *(TC *)(void *,uint32_t))PTR(PTR(notify,0),4))(notify,1);', 0),
        ('accept-answer', 'world_advanced.c', 'U32(PTR(object,0x1104),0)=1;', 'U32(PTR(object,0x1104),0)=0;', 5),
        ('cancel-mode', 'world_advanced.c', 'U32(object,0x2654)=2;', 'U32(object,0x2654)=1;', 0),
        ('visibility-selection', 'world_advanced.c', '20-value:20;', 'value:20;', 45),
        ('pressure-raw', 'world_advanced.c', '0xd8)=U32(object,0x1598);', '0xd8)=!!U32(object,0x1598);', 40),
        ('pressure-style', 'world_advanced.c', '0x1474,0x1354,0x15,5', '0x1474,0x1354,0x11,5', 4),
        ('pressure-error-priority', 'world_advanced.c', 'result=control==0x1474?0x1b03:0x1b04;', 'result=control==0x1474?0x1b04:0x1b03;', 55),
        ('cloud-layer-title', 'world_advanced_cloud.c', 'index==0?0xf8db:index==1?0xf8d9:0xf8d7', 'index==0?0xf8d9:index==1?0xf8db:0xf8d7', 0),
        ('cloud-raw-checkbox', 'world_advanced_cloud.c', 'case 0x89d0:U32(PTR(object,0x1190),(cloud_selected(object)+1)*0x58)=U32(object,0x1b14);', 'case 0x89d0:(void)object;', 38),
        ('cloud-promotion', 'world_advanced_cloud.c', 'cloud_copy(object,0,target);', 'cloud_copy(object,2,target);', 18),
        ('cloud-edit-limit', 'world_advanced_cloud.c', '0x1920,0x11f0,0x11,4', '0x1920,0x11f0,0x11,3', 0),
        ('cloud-ddx-labels', 'world_advanced_tables.inc', '{0x11f0, 0x1b3f},\n    {0x12a8, 0x1b35}', '{0x11f0, 0x1b35},\n    {0x12a8, 0x1b3f}', 18),
        ('temperature-units', 'world_advanced_temperature.c', 'units?0xf8ef:0xf8f0', 'units?0xf8f0:0xf8ef', 2),
        ('temperature-edit-limit', 'world_advanced_temperature.c', '0x182c,0x16a4,0x11,6', '0x182c,0x16a4,0x11,5', 2),
        ('temperature-raw', 'world_advanced_temperature.c', '0xbc)=U32(object,0x1950);', '0xbc)=!!U32(object,0x1950);', 39),
        ('temperature-add-callback', 'world_advanced_temperature.c', 'advanced_callback(object,0x11f4,0x3fc);', 'advanced_callback(object,0x11f4,0x400);', 17),
        ('wind-ground-label', 'world_advanced_wind.c', '"000000\\t"', '"999999\\t"', 1),
        ('wind-edit-limit', 'world_advanced_wind.c', '0x1ff0,0x13c0,1,6', '0x1ff0,0x13c0,1,5', 1),
        ('wind-turbulence', 'world_advanced_wind.c', 'case 0x12e00:case 0x12e60:', 'case 0x12e60:return 0;\n    case 0x12e00:', 36),
        ('wind-notification', 'world_advanced_wind.c', 'PTR(object,0x1204)=fs98_world_advanced_allocate(object,0);', 'PTR(object,0x1204)=NULL;', 1),
    ]
    chosen = [row for row in mutations if len(sys.argv) == 1 or row[0] in sys.argv[1:]]
    if set(sys.argv[1:]) - {row[0] for row in mutations}:
        raise ValueError('unknown native mutation')
    output = Path('bin/world-diff/advanced-native-breaks')
    output.mkdir(parents=True, exist_ok=True)
    paths = [*Path('src/world').glob('*'), Path('tools/difftest/worldadvanceduidiff.c'), Path('tools/difftest/worlduidiff.c'),
             Path('bin/world-diff/worldadvanceduidiff.exe'), Path('bin/world-diff/world-res.o'), Path('bin/world-diff/libworld-simsui.a')]
    snapshots = {path.name: path.read_bytes() for path in paths if path.is_file()}
    (output / 'inputs.json').write_text(json.dumps({name: hashlib.sha256(data).hexdigest() for name, data in snapshots.items()}, indent=2) + '\n')
    source_names = next(line.split(':=', 1)[1].split() for line in Path('tools/world.mk').read_text().splitlines() if line.startswith('WORLD_SOURCES :='))
    results = []
    for name, filename, before, after, step in chosen:
        work = output / name
        work.mkdir(exist_ok=True)
        for base, data in snapshots.items():
            (work / base).write_bytes(data)
        path = work / filename
        source = path.read_text()
        expected = {'parent-notify-delete': 3, 'pressure-raw': 2, 'temperature-raw': 2, 'temperature-add-callback': 2}.get(name, 1)
        if source.count(before) != expected:
            raise ValueError(f'{name}: {source.count(before)} anchors, expected {expected}')
        path.write_text(source.replace(before, after))
        dll = work / 'mutant.dll'
        dll.unlink(missing_ok=True)
        command = ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-strict-aliasing',
                   '-fno-delete-null-pointer-checks', '-shared', '-nostdlib', '-Wl,--entry=_DllMain@12', '-Wl,--disable-dynamicbase',
                   '-Wl,--image-base=0x223f0000', '-o', str(dll)]
        command += [str(work / Path(path).name) for path in source_names]
        command += [str(work / 'world.def'), str(work / 'world-res.o'), str(work / 'libworld-simsui.a'), '-lkernel32', '-luser32', '-lmsvcrt', '-lntdll']
        build = subprocess.run(command, capture_output=True)
        (work / 'build.log').write_bytes(build.stdout + build.stderr)
        if build.returncode:
            result = 'BUILD FAILURE'
        else:
            # Ground row requires case 1; others include case 0 and case 1 too.
            run = subprocess.run(['timeout', '90s', 'wine', str(work / 'worldadvanceduidiff.exe'), str(dll), '2', str(step)], capture_output=True)
            log = run.stdout + run.stderr
            (work / 'run.log').write_bytes(log)
            result = 'DETECTED' if run.returncode == 1 and b'ADVANCED UI DIFFER' in log and b'FAULT' not in log else f'NOT DETECTED (exit {run.returncode})'
        print(f'{name}: {result}', flush=True)
        results.append({'name': name, 'step': step, 'result': result, 'dll_sha256': hashlib.sha256(dll.read_bytes()).hexdigest() if dll.exists() else None})
    (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    caught = sum(row['result'] == 'DETECTED' for row in results)
    print(f'Advanced Weather native mutations: {caught}/{len(results)} actual differences', flush=True)
    return int(caught != len(results))


if __name__ == '__main__':
    sys.exit(main())
