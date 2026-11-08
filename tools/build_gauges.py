#!/usr/bin/env python3
"""Build the complete gauge collection and a metadata-only harness include."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import subprocess
import time

ROOT = Path('bin/gauges-all')


def build(item):
    index, gauge = item
    directory = ROOT / gauge['name']
    output = ROOT / 'Gauges' / gauge['installed']
    # a decompiled gauge (src/gauges/<name>.c over the generated image.inc) replaces its lift
    hand = Path('src/gauges') / (gauge['name'].lower() + '.c')
    source = hand if hand.exists() else directory / 'gauge.c'
    dependencies = [source, directory / 'gauge.def', directory / 'gauge.res',
                    Path('src/gauges/gauge_lift.h'), Path('src/gauges/gauge.h')]
    if hand.exists():
        dependencies.append(directory / 'image.inc')
    if output.exists() and output.stat().st_mtime >= max(path.stat().st_mtime for path in dependencies):
        return gauge['name'] + ' up to date'
    resource = directory / 'resource.o'
    subprocess.run(['i686-w64-mingw32-windres', '-J', 'res', '-O', 'coff',
                    str(directory / 'gauge.res'), str(resource)], check=True, capture_output=True)
    command = ['i686-w64-mingw32-gcc', '-std=gnu99', '-O2', '-Wall', '-Wextra',
               '-Wno-unused-function', '-Wno-unused-label', '-fno-strict-aliasing',
               '-fno-asynchronous-unwind-tables', '-Isrc/gauges', f'-I{directory}', '-shared', '-nostdlib',
               '-Wl,--entry=_DllMain@12', '-Wl,--disable-dynamicbase', '-Wl,--disable-nxcompat',
               f'-Wl,--image-base={0x30000000 + index * 0x200000:#x}', '-o', str(output),
               str(source), str(directory / 'gauge.def'), str(resource),
               '-luser32', '-lkernel32', '-lmsvcrt', '-lgcc']
    if hand.exists():   # a cast or assignment to double rounds, as retail's stores do
        command[1:1] = ['-fexcess-precision=standard', '-Werror']
    run = subprocess.run(command, capture_output=True, text=True)
    (directory / 'build.log').write_text(run.stdout + run.stderr)
    if run.returncode:
        raise RuntimeError(f'{gauge["name"]}: {run.stderr}')
    return gauge['name'] + ' built'


def harness_include(gauges):
    lines = ['/* Generated gauge ABI metadata; no retail payload. */']
    for index, gauge in enumerate(gauges):
        lines.append(f'static const struct function_spec functions_{index}[] = {{')
        for function in gauge['functions']:
            lines.append(f'{{0x{function["rva"]:x}, {function["args"]}, {function["fp"]}, {function["pointer"]}, {int(function["is_void"])}, {int(function["arg_pointer"])}}},')
        lines.append('};')
        for field in ['relocs', 'float_slots', 'print_slots', 'bitmaps', 'indirect_slots', 'int_slots']:
            if field == 'relocs':
                values = [offset for offset, target in gauge[field]]
            elif field == 'int_slots':   # (offset, width) pairs
                values = [value for slot in gauge.get(field, []) for value in slot]
            elif field == 'bitmaps':
                values = [value for bitmap in gauge[field] for value in bitmap]
            else:
                values = gauge[field]
            lines.append(f'static const uint32_t {field}_{index}[] = {{' + ','.join(str(value) for value in values or [0]) + '};')
    lines.append('static const struct module_spec modules[] = {')
    for index, gauge in enumerate(gauges):
        lines.append('{' + f'"{gauge["name"]}", "{gauge["installed"]}", '
                     f'0x{gauge["base"]:x}, {gauge["data_size"]}, {gauge["imports"]}, {gauge["linkage"]}, {gauge["header"]}, '
                     f'functions_{index}, {len(gauge["functions"])}, '
                     f'relocs_{index}, {len(gauge["relocs"])}, '
                     f'float_slots_{index}, {len(gauge["float_slots"])}, '
                     f'print_slots_{index}, {len(gauge["print_slots"])}, '
                     f'bitmaps_{index}, {len(gauge["bitmaps"])}, '
                     f'indirect_slots_{index}, {len(gauge["indirect_slots"])}, '
                     f'int_slots_{index}, {len(gauge.get("int_slots", []))}' + '},')
    lines.append('};')
    (ROOT / 'all_gauges.inc').write_text('\n'.join(lines) + '\n')
    panels = ['/* Generated callback pointer pairs for whole-PANELS comparison. */']
    for index, gauge in enumerate(gauges):
        pairs = [(int(offset), target) for offset, target in gauge['code_pointers'].items()]
        if len(pairs) > 64:
            raise ValueError('increase the PANELS gauge code-pointer capacity')
        panels.append(f'static const uint32_t code_offsets_{index}[] = {{' + ','.join(str(offset) for offset, target in pairs) + '};')
        panels.append(f'static const uint32_t code_rvas_{index}[] = {{' + ','.join(str(target) for offset, target in pairs) + '};')
        mask = [int(offset in gauge['pointer_words']) for offset in range(0, gauge['data_size'], 4)]
        panels.append(f'static const uint8_t pointer_mask_{index}[] = {{' + ','.join(str(value) for value in mask) + '};')
    panels.append('static const struct gauge_spec gauge_specs[] = {')
    for index, gauge in enumerate(gauges):
        panels.append('{' + f'"{gauge["installed"]}", {gauge["data_size"]}, {gauge["linkage"]}, '
                      f'code_offsets_{index}, code_rvas_{index}, {len(gauge["code_pointers"])}, pointer_mask_{index}' + '},')
    panels.append('};')
    (ROOT / 'all_panels.inc').write_text('\n'.join(panels) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    gauges = json.loads((ROOT / 'manifest.json').read_text())
    (ROOT / 'Gauges').mkdir(exist_ok=True)
    harness_include(gauges)
    start = time.monotonic()
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for result in pool.map(build, enumerate(gauges)):
            print(result, flush=True)
    print(f'{len(gauges)} gauges ready in {time.monotonic() - start:.1f}s')


if __name__ == '__main__':
    main()
