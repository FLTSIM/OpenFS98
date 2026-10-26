#!/usr/bin/env python3
"""Require initializer transcript/state differences, including CString cleanup."""
from pathlib import Path
import subprocess


def main():
    output_dir = Path('bin/world-diff/init-breaks')
    output_dir.mkdir(parents=True, exist_ok=True)
    sources = {path.name: path.read_bytes() for path in Path('src/world').iterdir()
                if path.suffix in ('.c', '.h', '.inc')}
    for filename in ('worldinitdiff.c', 'worlddiff.c'):
        sources[filename] = (Path('tools/difftest') / filename).read_bytes().replace(b'../../src/world/', b'')
    mutations = [
        ('cstring-unwind', 's_init_services->string_destroy(state->string);', '(void)state;'),
        ('cstring-ownership', 'state.live = 1;', 'state.live = 0;'),
        ('cstring-release', 'string_release(&string, -1)', 'string_release(&string, 0)'),
        ('resource-id', '0xf8d5, buffer', '0xf8d6, buffer'),
        ('empty-label-control', 'set_text(AT(object, 0x12c0)', 'set_text(AT(object, 0x13a8)'),
        ('edit-color', '0x1258), font, 0xffffff', '0x1258), font, 0x00ff00'),
        ('position-limit', '0x19c0), 0xc5, 4, 0', '0x19c0), 0xc5, 3, 0'),
        ('reload-enabled', 's_enable[index]), enabled)', 's_enable[index]), enabled && WORLD_U32(object, 0x10e0) != 0)'),
        ('skin-result', 'if (!((int (FS98_WORLD_THISCALL', 'if (((int (FS98_WORLD_THISCALL'),
        ('init-return', '    return 1;\n}', '    return 0;\n}'),
    ]
    caught = 0
    for name, before, after in mutations:
        work = output_dir / name
        work.mkdir(exist_ok=True)
        for filename, data in sources.items():
            if filename == 'world_init.c':
                if data.count(before.encode()) != 1:
                    raise ValueError(f'{name}: anchor is not unique')
                data = data.replace(before.encode(), after.encode(), 1)
            (work / filename).write_bytes(data)
        exe = work / 'worldinitdiff.exe'
        build = subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
            '-fno-strict-aliasing', '-o', str(exe), str(work / 'worldinitdiff.c'),
            str(work / 'world_init.c'), str(work / 'world_area.c'), '-lntdll'], capture_output=True, text=True)
        if build.returncode:
            raise RuntimeError(f'{name}: build failed, not counted\n{build.stderr}')
        run_result = subprocess.run(['wine', str(exe), '32', 'unwind'], capture_output=True, text=True, timeout=120)
        (work / 'result.log').write_text(run_result.stdout + run_result.stderr)
        if run_result.returncode != 1 or 'WORLD INIT DIFFER' not in run_result.stderr:
            raise RuntimeError(f'{name}: no actual difference (exit {run_result.returncode})\n{run_result.stderr}')
        caught += 1
        print(f'{name}: detected', flush=True)
    print(f'World initializer mutations: {caught}/{len(mutations)} detected')


if __name__ == '__main__':
    main()
