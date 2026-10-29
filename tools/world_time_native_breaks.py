#!/usr/bin/env python3
"""Snapshot and mutate time native DLLs; require actual differences."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys


def main():
    output_dir = Path('bin/world-diff/time-native-breaks')
    output_dir.mkdir(parents=True, exist_ok=True)
    sources = {path.name: path.read_bytes() for path in Path('src/world').iterdir()
               if path.suffix in ('.c', '.h', '.inc', '.def')}
    for name in ('worldtimeuidiff.c', 'worlduidiff.c'):
        sources[name] = (Path('tools/difftest') / name).read_bytes().replace(b'../../src/world/', b'')
    for name in ('world-res.o', 'libworld-simsui.a'):
        sources[name] = (Path('bin/world-diff') / name).read_bytes()
    sources['baseline.dll'] = Path('bin/world-diff/WORLD_area.dll').read_bytes()
    (output_dir / 'inputs.json').write_text(json.dumps({name: hashlib.sha256(data).hexdigest() for name, data in sources.items()}, indent=2) + '\n')
    mutations = [
        ('artwork', 'world_time_init.c', '0x1368),0xf831,', '0x1368),0xf830,'),
        ('tab-frame', 'world_time_init.c', '(object,0x150,0x1d,', '(object,0x14f,0x1d,'),
        ('combo-font', 'world_time_init.c', '0x1b80),AT(object,0x3e0)', '0x1b80),AT(object,0x370)'),
        ('combo-label', 'world_time_init.c', '0x1ca0),AT(object,0x3e0),AT(object,0x1130)', '0x1ca0),AT(object,0x3e0),AT(object,0x11e8)'),
        ('clock-heading-font', 'world_time_init.c', 'index==0?0x370:', 'index==0?0x220:'),
        ('edit-limit', 'world_time_init.c', '0x14)-1);', '0x14));'),
        ('season-enable', 'world_time.c', 'index==3||index==7?!manual:(int)manual', 'index==3||index==7?(int)manual:(int)manual'),
        ('clock-enable', 'world_time.c', 'index==4||index==9?!manual:(int)manual', 'index==4||index==9?(int)manual:(int)manual'),
        ('checkbox-raw', 'world_time.c', '0x90)=U32(object,0x1da4);', '0x90)=!!U32(object,0x1da4);'),
        ('checkbox-populate', 'world_time.c', '(int)U32(PTR(object,0x111c),0x90)', '!!U32(PTR(object,0x111c),0x90)'),
        ('season-callback', 'world_time.c', 'kind==1?0x110:0x118);fs98_world_time_populate', 'kind==1?0x114:0x118);fs98_world_time_populate'),
        ('clock-callback', 'world_time.c', 'kind==1?0x110:0x118);fs98_world_time_populate', 'kind==1?0x110:0x11c);fs98_world_time_populate'),
        ('date-action', 'world_time.c', 'id==0x1afc?7:id==0x1afb?5:id==0x1afa?6:0', 'id==0x1afc?5:id==0x1afb?7:id==0x1afa?6:0'),
        ('gmt-conversion', 'world_time.c', 'time_callback(object,0x1120,0x120);', 'time_callback(object,0x1120,0x11c);'),
        ('error-target', 'world_time.c', 'clock_status[]={0x64,0x78,0x3c,0x50}', 'clock_status[]={0x78,0x64,0x3c,0x50}'),
        ('accept-veto', 'world_time.c', 'if(!status||!((int(TC *', 'if(1||!((int(TC *'),
        ('accept-mode', 'world_time.c', 'void fs98_world_time_accept(void *object)\n{\n', 'void fs98_world_time_accept(void *object)\n{\n    U32(object,0x2124)=1;\n'),
        ('accept-answer', 'world_time.c', 'U32(data,0)=1;', 'U32(data,0)=0;'),
        ('accept-descriptor-cache', 'world_time.c', 'void fs98_world_time_accept(void *object)\n{\n', 'void fs98_world_time_accept(void *object)\n{\n    void *saved_descriptor=PTR(object,0x10dc);\n'),
        ('cancel-mode', 'world_time.c', 'U32(object,0x2124)=2;', 'U32(object,0x2124)=1;'),
        ('close-tab', 'world_time.c', '((void(TC *)(void *))vtable[0xc4/4])(tab);', '(void)tab;'),
        ('close-notify-flags', 'world_time.c', 'vtable[1])(notify,1);', 'vtable[1])(notify,0);'),
        ('post-destroy', 'world_runtime_tables.inc', 'fs98_world_priority_post_destroy(object);', '(void)object;'),
    ]
    requested = set(sys.argv[1:])
    if requested - {item[0] for item in mutations}:
        raise ValueError('unknown time native mutation')
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
                if name == 'accept-descriptor-cache':
                    data = data.replace(b'        void *data=PTR(object,0x10dc);vtable=PTR(object,0);', b'        void *data=saved_descriptor;vtable=PTR(object,0);')
            (work / source_name).write_bytes(data)
        dll, exe = work / 'WORLD_area.dll', work / 'worldtimeuidiff.exe'
        flags = ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-strict-aliasing', '-fno-delete-null-pointer-checks']
        commands = [flags + ['-shared', '-nostdlib', '-Wl,--entry=_DllMain@12', '-Wl,--disable-dynamicbase', '-Wl,--image-base=0x223f0000', '-o', str(dll),
            *(str(work / Path(unit).name) for unit in Path('tools/world.mk').read_text().split('WORLD_SOURCES := ',1)[1].splitlines()[0].split()),
            str(work / 'world.def'), str(work / 'world-res.o'), str(work / 'libworld-simsui.a'), '-lkernel32', '-luser32', '-lmsvcrt', '-lntdll'],
            flags + ['-o', str(exe), str(work / 'worldtimeuidiff.c'), '-lgdi32', '-lntdll']]
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
        if result.returncode == 1 and 'TIME UI DIFFER' in output:
            caught += 1
            print(f'{name}: detected', flush=True)
        else:
            print(f'{name}: NOT DETECTED (exit {result.returncode})\n{output}', flush=True)
    print(f'World time native mutations: {caught}/{len(selected)} detected', flush=True)
    return 0 if caught == len(selected) else 1


if __name__ == '__main__':
    sys.exit(main())
