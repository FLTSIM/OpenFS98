#!/usr/bin/env python3
"""Prove native Alerts comparisons detect drawing and module-cleanup errors."""
from pathlib import Path
import subprocess
import sys


def main():
    directory = Path('bin/alerts-diff/ui-breaks')
    directory.mkdir(parents=True, exist_ok=True)
    breaks = [
        ('progress-color', 'events', '0x7f0000', '0x7e0000', 'first pixel difference'),
        ('module-unwind-delete', 'runtime', 'fs98_mfc_delete(state->dynamic);',
         '(void)state;', 'INIT DIFFER'),
    ]
    caught = 0
    for name, unit, before, after, witness in breaks:
        source_text = Path(f'src/alerts/alerts_{unit}.c').read_text()
        if before not in source_text:
            raise ValueError(f'{name}: mutation anchor missing')
        source = directory / f'{name}.c'
        dll = directory / f'{name}.dll'
        source.write_text(source_text.replace(before, after, 1))
        sources = [str(source) if other == unit else f'src/alerts/alerts_{other}.c'
                    for other in ('events', 'objects', 'runtime')]
        build = subprocess.run([
            'i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
            '-fno-strict-aliasing', '-fno-delete-null-pointer-checks', '-Isrc/alerts',
            '-shared', '-nostdlib', '-Wl,--entry=_DllMain@12', '-Wl,--disable-dynamicbase',
            '-Wl,--image-base=0x22300000', '-o', str(dll), *sources,
            'src/alerts/alerts.def', 'bin/alerts-diff/alerts-res.o',
            'bin/alerts-diff/libalerts-simsui.a', '-lkernel32', '-luser32', '-lgdi32',
            '-lmsvcrt'], text=True, capture_output=True)
        if build.returncode:
            print(f'{name}: BUILD FAILED\n{build.stderr}', flush=True)
            continue
        try:
            run = subprocess.run(['wine', 'bin/alerts-diff/alertsuidiff.exe', str(dll)],
                                  text=True, capture_output=True, timeout=60)
        except subprocess.TimeoutExpired:
            print(f'{name}: TIMEOUT (not counted)', flush=True)
            continue
        output_text = run.stdout + run.stderr
        (directory / f'{name}.log').write_text(output_text)
        if run.returncode == 1 and witness in output_text:
            caught += 1
            print(f'{name}: caught ({witness})', flush=True)
        else:
            print(f'{name}: NOT CAUGHT (exit {run.returncode})\n{output_text}', flush=True)
    print(f'Alerts native UI/module mutations: {caught}/{len(breaks)} caught')
    return 0 if caught == len(breaks) else 1


if __name__ == '__main__':
    sys.exit(main())
