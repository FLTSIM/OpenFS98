#!/usr/bin/env python3
"""Break Alerts event logic in isolated copies and require an actual mismatch.

Run via bash tools/run_alertsdiff.sh --breaks [N] to keep Wine state isolated.
A compile error, runtime fault or timeout is not accepted as a caught mutation.
The baseline source and executable are never edited or overwritten.
"""
from pathlib import Path
import subprocess
import sys


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    if count < 100:
        raise ValueError('use at least 100 scenarios for mutations')
    directory = Path('bin/alerts-diff/breaks')
    directory.mkdir(parents=True, exist_ok=True)
    source_text = Path('src/alerts/alerts_events.c').read_text()
    breaks = [
        ('first-answer', 'fs98_alerts_modal_answer(object, 0x10e4);',
         'fs98_alerts_modal_answer(object, 0x10e8);'),
        ('second-answer', 'fs98_alerts_modal_answer(object, 0x10e8);',
         'fs98_alerts_modal_answer(object, 0x10ec);'),
        ('third-answer', 'fs98_alerts_modal_answer(object, 0x10ec);',
         'fs98_alerts_modal_answer(object, 0x10e4);'),
        ('suppression-field', 'D32(descriptor, 0x10) = D32(object, 0x1550);',
         'D32(descriptor, 0x10) = D32(object, 0x1550) ^ 1;'),
        ('advisory-quit', 'D32(descriptor, 0x0c) = 1;', 'D32(descriptor, 0x0c) = 0;'),
        ('advisory-help', '"fs98.hlp", 8, 0xbc2', '"fs98.hlp", 8, 0xbc3'),
        ('close-noop', '    (void)object;\n}', '    fs98_alerts_virtual(object, 0xec);\n}'),
        ('live-gate', 'if (D32(descriptor, 0x0c) == 0)',
         'if (D32(descriptor, 0x0c) != 0)'),
        ('progress-gate', 'if (D32(descriptor, 0x10) == 0)',
         'if (D32(descriptor, 0x10) != 0)'),
        ('saved-vtable', 'functions[0x60 / 4](object);',
         'fs98_alerts_virtual(object, 0x60);'),
        ('total-floor', '        total = 1;', '        total = 2;'),
        ('current-cap', '        current = total;', '        current = total - 1;'),
        ('limit-cap', 'D32(descriptor, 0x20) = 100;', 'D32(descriptor, 0x20) = 99;'),
        ('limit-floor', 'D32(descriptor, 0x20) = 1;', 'D32(descriptor, 0x20) = 2;'),
        ('start-clamp', 'D32(descriptor, 0x1c) >= D32(descriptor, 0x20)',
         'D32(descriptor, 0x1c) > D32(descriptor, 0x20)'),
        ('unsigned-progress', '(uint32_t)((limit - start) * (uint32_t)current) / (uint32_t)total',
         '(int32_t)((limit - start) * (uint32_t)current) / total'),
        ('widen-product', '(uint32_t)((limit - start) * (uint32_t)current) / (uint32_t)total',
         '(uint32_t)(((uint64_t)(limit - start) * (uint32_t)current) / (uint32_t)total)'),
        ('split-divisor', 'split / 100', 'split / 101'),
        ('right-edge', 'fs98_alerts_sub(width, 1)', 'fs98_alerts_sub(width, 0)'),
        ('bottom-edge', 'fs98_alerts_sub(height, 1)', 'fs98_alerts_sub(height, 0)'),
        ('clip-rectangle', 'x, y, 6, &filled, text',
         'x, y, 6, &remaining, text'),
        ('draw-options', 'x, y, 6, &remaining, text',
         'x, y, 4, &remaining, text'),
        ('foreground', 'text_color(dc_storage, 0xffffff)', 'text_color(dc_storage, 0xfffffe)'),
        ('text-metrics-dc', 'text_extent(D32(dc_storage, 8)', 'text_extent(D32(dc_storage, 4)'),
        ('modal-ddx-id', 'ddx_control(exchange, 0x7d1,', 'ddx_control(exchange, 0x7d2,'),
        ('modeless-ddx-offset', 'ddx_control(exchange, 0x7d6, (uint8_t *)object + 0x10e4)',
         'ddx_control(exchange, 0x7d6, (uint8_t *)object + 0x1124)'),
        ('advisory-ddx-id', 'ddx_control(exchange, 0x7d7,', 'ddx_control(exchange, 0x7d8,'),
        ('modeless-init-flags', 'object, 0x14f, s_resource_module, 9',
         'object, 0x14f, s_resource_module, 8'),
        ('pointer-as-resource', 'if ((int32_t)text_value <= 0xffff)', 'if (1)'),
        ('modeless-icon', 'tag == 1 ? 0x86', 'tag == 1 ? 0x87'),
        ('zero-tag-load', '    if (tag)\n', '    if (1)\n'),
        ('sprite-position', '+ 0xff4, 0x1e, 0x22,', '+ 0xff4, 0x1f, 0x22,'),
        ('modal-button-count', '&& count < 3', '&& count < 2'),
        ('modal-button-shift', 'if (D32(object, 0x10e8) == 0)', 'if (0)'),
        ('modal-default-answer', 'D32(object, 0x10f4) = index;',
         'D32(object, 0x10f4) = index + 1;'),
        ('modal-label', 's_button_text[bit],', 's_button_text[bit] ^ 1,'),
        ('advisory-check', 'set_checked(check, (int32_t)D32(descriptor, 0x10))',
         'set_checked(check, (int32_t)D32(descriptor, 0x0c))'),
        ('advisory-message', 'load_string(0xf6f0,', 'load_string(0xf6f1,'),
        ('advisory-button-order', 'index == 0 ? 0x1398', 'index == 0 ? 0x11e4'),
    ]
    caught = 0
    for name, before, after in breaks:
        if before not in source_text:
            raise ValueError(f'{name}: mutation anchor missing')
        source = directory / f'{name}.c'
        executable = directory / f'{name}.exe'
        source.write_text(source_text.replace(before, after, 1))
        # Always compile the copy directly to its own executable; never depend
        # on make timestamps and never replace the verified baseline output.
        build = subprocess.run([
            'i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra',
            '-fno-strict-aliasing', '-Isrc/alerts', '-o', str(executable),
            'tools/difftest/alertseventsdiff.c', str(source), '-lntdll'],
            text=True, capture_output=True)
        if build.returncode:
            print(f'{name}: BUILD FAILED\n{build.stderr}')
            continue
        try:
            run = subprocess.run(['wine', str(executable), str(count)],
                                  text=True, capture_output=True, timeout=45)
        except subprocess.TimeoutExpired:
            print(f'{name}: TIMEOUT (not counted)')
            continue
        output_text = run.stdout + run.stderr
        (directory / f'{name}.log').write_text(output_text)
        if run.returncode == 1 and 'DIFFER scenario=' in output_text:
            caught += 1
            print(f'{name}: caught', flush=True)
        else:
            print(f'{name}: NOT CAUGHT (exit {run.returncode})\n{output_text}', flush=True)
    print(f'Alerts events mutations: {caught}/{len(breaks)} caught')
    return 0 if caught == len(breaks) else 1


if __name__ == '__main__':
    sys.exit(main())
