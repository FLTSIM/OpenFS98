#!/usr/bin/env python3
"""Prove collection-wide comparisons fail on altered code and resources.

Each mutation is built separately. Compiler errors, crashes, hangs and timeouts
are failures of the check, not evidence that a semantic mutation was caught.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import re
import subprocess

ROOT = Path('bin/gauges-all')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    gauges = json.loads((ROOT / 'manifest.json').read_text())
    mutations = []
    for index, gauge in enumerate(gauges):
        source = (ROOT / gauge['name'] / 'gauge.c').read_text()
        match = re.search(r'gauge_panel_call\(f, gauge_u32\(\(f->(?:eax|ecx|edx)\+0x(64|40)u\)\), 2\)', source)
        if not match: raise ValueError(f'{gauge["name"]}: install boundary missing')
        replacement = match[0].replace('0x64u', '0x70u') if match[1] == '64' else match[0].replace('0x40u', '0x4cu')
        changed = source[:match.start()] + replacement + source[match.end():]
        mutations.append((index, gauge, 'install-service', changed))
        mutations.append((index, gauge, 'bitmap', source))
        if 'if (f->depth) {' in source:
            changed = source.replace('if (f->depth) {', 'if (f->depth) { f->fp[f->depth - 1] += 1.0L;', 1)
            mutations.append((index, gauge, 'numeric-return', changed))
        elif 'gauge_fp_push(f, ' in source:
            start = source.index('gauge_fp_push(f, ')
            end = source.index(');', start)
            expression = source[start + len('gauge_fp_push(f, '):end]
            changed = source[:start] + f'gauge_fp_push(f, ({expression}) + 1.0L);' + source[end + 2:]
            mutations.append((index, gauge, 'numeric-operation', changed))

    def run(item):
        index, gauge, name, source = item
        directory = ROOT / 'mutations' / gauge['name'] / name
        directory.mkdir(parents=True, exist_ok=True)
        code, dll = directory / 'gauge.c', directory / 'gauge.dll'
        code.write_text(source)
        resource = ROOT / gauge['name'] / 'resource.o'
        if name == 'bitmap':
            data = bytearray((ROOT / gauge['name'] / 'gauge.res').read_bytes())
            data[104] ^= 1
            res = directory / 'gauge.res'
            res.write_bytes(data)
            resource = directory / 'resource.o'
            subprocess.run(['i686-w64-mingw32-windres', '-J', 'res', '-O', 'coff', str(res), str(resource)], check=True, capture_output=True)
        command = ['i686-w64-mingw32-gcc', '-std=gnu99', '-O2', '-fno-strict-aliasing',
                   '-fno-asynchronous-unwind-tables', '-Isrc/gauges', '-shared', '-nostdlib',
                   '-Wl,--entry=_DllMain@12', '-Wl,--disable-dynamicbase',
                   f'-Wl,--image-base={0x30000000 + index * 0x200000:#x}', '-o', str(dll),
                   str(code), str(ROOT / gauge['name'] / 'gauge.def'), str(resource),
                   '-luser32', '-lkernel32', '-lmsvcrt', '-lgcc']
        build = subprocess.run(command, capture_output=True, text=True)
        if build.returncode:
            return gauge['name'], name, False, 'BUILD ERROR\n' + build.stderr
        log_path = directory / 'test.log'
        try:
            with log_path.open('w') as log:
                result = subprocess.run(['wine', str(ROOT / 'allgaugesdiff.exe'), '1000', gauge['name'], str(dll)],
                                        stdout=log, stderr=subprocess.STDOUT, timeout=90)
            output = log_path.read_text()
            caught = result.returncode == 1 and (' mismatch' in output or 'data+' in output) and 'fault ' not in output
            return gauge['name'], name, caught, output
        except subprocess.TimeoutExpired:
            return gauge['name'], name, False, 'TIMEOUT'
    failed = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for gauge, name, caught, output in pool.map(run, mutations):
            print(f'{gauge}/{name}: {"caught" if caught else "FAILED"}', flush=True)
            if not caught:
                failed.append([gauge, name, output])
    (ROOT / 'mutations-summary.json').write_text(json.dumps({'mutations': len(mutations), 'failed': failed}, indent=2))
    print(f'{len(mutations) - len(failed)}/{len(mutations)} mutations caught')
    return bool(failed)


if __name__ == '__main__':
    raise SystemExit(main())
