#!/usr/bin/env python3
"""Build isolated warning-gauge mutations and require explicit mismatches.

A compile failure, crash or timeout is a failed check, never a caught break.
The accepted source, DLL and extracted resources stay untouched.
"""
from pathlib import Path
import subprocess
import struct
import sys


def main():
    suction = len(sys.argv) > 1 and sys.argv[1] == '--suction'
    gauge = 'suction' if suction else 'warning'
    directory = Path('bin/gauges-diff/suction-breaks' if suction else 'bin/gauges-diff/breaks')
    directory.mkdir(parents=True, exist_ok=True)
    source = Path(f'src/gauges/{gauge}.c').read_text()
    breaks = [
        ('failures-service', 'panel_service(0x60)', 'panel_service(0x68)'),
        ('install-service', 'panel_service(0x64)', 'panel_service(0x70)'),
        ('unmark-service', 'panel_service(0x68)', 'panel_service(0x6c)'),
        ('prepare-service', 'panel_service(0x6c)', 'panel_service(0x7c)'),
        ('update-service', 'panel_service(0x70)', 'panel_service(0x64)'),
        ('draw-service', 'panel_service(0x74)', 'panel_service(0x68)'),
        ('release-service', 'panel_service(0x7c)', 'panel_service(0x74)'),
        ('failure-indirection', 'panel_service(0x60))(*root)',
         'panel_service(0x60))(*root ^ 1)'),
        ('failure-root-address', 'warning_image.words[0x28 / 4]',
         '(uint32_t)(uintptr_t)(warning_image.bytes + 0xc0)'),
        ('install-context', 'warning_image.words[0xc0 / 4], context);',
         'warning_image.words[0xc0 / 4], context ^ 1);'),
        ('root-value', 'warning_image.words[0xc0 / 4]);',
         '(warning_image.words[0xc0 / 4] ^ 1));'),
        ('return-value', 'return ((panel_two)', 'return 0 * ((panel_two)'),
        ('noop-state', 'warning_noop(void) { }',
         'warning_noop(void) { warning_image.words[0x4c / 4] ^= 1; }'),
        ('initial-data', '        DisableThreadLibraryCalls(instance);',
         '        warning_image.words[0x4c / 4] ^= 1;\n        DisableThreadLibraryCalls(instance);'),
    ]
    if suction:
        breaks = [(name, before.replace('warning_', 'suction_').replace('0xc0', '0x1d0').replace('0x28', '0x20'),
                   after.replace('warning_', 'suction_').replace('0xc0', '0x1d0').replace('0x28', '0x20'))
                  for name, before, after in breaks]
        breaks += [
            ('clamp-upper', 'value > 8.0', 'value > 9.0'),
            ('clamp-lower', 'value = 2.0;', 'value = 3.0;'),
            ('clamp-nan', '!(value >= 2.0)', 'value < 2.0'),
            ('mouse-point-step', 'suction_image.words[0x1bc / 4] + 4',
             'suction_image.words[0x1bc / 4] + 8'),
            ('mouse-point-field', 'suction_image.words[0x1bc / 4] + 4',
             'suction_image.words[0x1b8 / 4] + 4'),
            ('mouse-record', 'suction_image.bytes + 0x1d8', 'suction_image.bytes + 0x1dc'),
            ('mouse-header', 'suction_image.bytes + 0x18', 'suction_image.bytes + 0x1c'),
            ('mouse-add-service', 'panel_service(0x84)', 'panel_service(0x88)'),
            ('mouse-remove-service', 'panel_service(0x88)', 'panel_service(0x84)'),
            ('skip-install', '((panel_two)(uintptr_t)panel_service(0x64))(suction_image.words[0x1d0 / 4], context);',
             '(void)context;'),
        ]
    passed = 0
    for name, before, after in breaks + [('bitmap', '', '')] + ([('bitmap-second', '', '')] if suction else []):
        text = source
        if before:
            if before not in source:
                raise ValueError(f'{name}: mutation site missing')
            text = source.replace(before, after, 1)
        path = directory / f'{name}.c'
        path.write_text(text)
        dll = directory / f'{name}.dll'
        resource = Path(f'bin/gauges-diff/{gauge}-res.o')
        if name.startswith('bitmap'):
            data = bytearray(Path(f'bin/gauges-diff/{gauge}.res').read_bytes())
            offset = 32
            if name == 'bitmap-second':
                size, header = struct.unpack_from('<II', data, offset)
                offset = (offset + header + size + 3) & ~3
            size, header = struct.unpack_from('<II', data, offset)
            data[offset + header + 40] ^= 1  # first palette byte of the selected bitmap
            res = directory / f'{name}.res'
            res.write_bytes(data)
            resource = directory / f'{name}-res.o'
            subprocess.run(['i686-w64-mingw32-windres', '-J', 'res', '-O', 'coff',
                            str(res), str(resource)], check=True)
        subprocess.run([
            'i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
            '-fno-strict-aliasing', '-Ibin/gauges-diff', '-shared', '-nostdlib',
            '-Wl,--entry=_DllMain@12', '-Wl,--disable-dynamicbase',
            '-Wl,--disable-nxcompat', '-Wl,--image-base=' + ('0x32010000' if suction else '0x32000000'),
            '-o', str(dll), str(path), f'src/gauges/{gauge}.def', str(resource),
            '-lkernel32'], check=True, capture_output=True, text=True)
        run = subprocess.run(['wine', 'bin/gauges-diff/' + ('suctiondiff.exe' if suction else 'gaugesdiff.exe'), '100', str(dll)],
                             capture_output=True, text=True, timeout=30)
        output = run.stdout + run.stderr
        caught = run.returncode == 1 and any(message in output for message in (
            'callback mismatch', '.data+', 'initial data/bitmap comparison failed', 'clamp mismatch'))
        (directory / f'{name}.log').write_text(output)
        print(f'{name}: {"caught" if caught else "FAILED"}', flush=True)
        if not caught:
            raise RuntimeError(f'{name}: exit {run.returncode}\n{output}')
        passed += 1
    print(f'{passed} deliberate gauge breaks, all caught')


if __name__ == '__main__':
    main()
