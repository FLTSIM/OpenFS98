#!/usr/bin/env python3
"""Require actual differential failures for facilities core/init mutations."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parent.parent
output = root / 'bin/world-diff/facilities-breaks'
output.mkdir(parents=True, exist_ok=True)
source = (root / 'src/world/world_facilities.c').read_text()
mutations = (
    ('release-row-stride', 'offset += list ? 0x60 : 0x64;', 'offset += list ? 0x60 : 0x60;'),
    ('release-count', 'U32(PTR(object, 0x10dc), 0x24) = 0;', 'U32(PTR(object, 0x10dc), 0x24) = 1;'),
    ('release-runways', '{0, 4, 8, 12, 16, 0x60}', '{0, 4, 8, 12, 16, 0x5c}'),
    ('string-cleanup-omission', 'uint32_t index = --life->count;\n        call0(life->items[index].slot, life->items[index].object);', 'uint32_t index = --life->count;\n        if (index != 100) call0(life->items[index].slot, life->items[index].object);'),
    ('output-owner-flags', 'U32(PTR(object, 0x10dc), 0x20) = 1;', 'U32(PTR(object, 0x10dc), 0x20) = 3;'),
    ('output-mode-place', 'mode == 20 ? 8 : 4', 'mode == 20 ? 4 : 8'),
    ('output-title-city', 'string_assign(&line, PTR(row(object, 0x28, offset), 12));', 'string_assign(&line, PTR(row(object, 0x28, offset), 8));'),
    ('output-usa-state', '? 4 : 8;', '? 8 : 4;'),
    ('output-placeholder', 'string_load(&temporary, 0xf97a); index =', 'string_load(&temporary, 0xf97b); index ='),
    ('output-navaid-mask', '(index & 8) && (index & 0xf982)', '(index & 8)'),
    ('output-runway-prefix', 'string_assign(&line, "H");', 'string_assign(&line, "0");'),
    ('output-runway-suffix', 'suffix == 1 ? "L"', 'suffix == 1 ? "R"'),
    ('output-surface-order', '0xf97c,0xf97d,0xf97f,0xf97e', '0xf97d,0xf97c,0xf97f,0xf97e'),
    ('output-empty-airports', '!airports_present ? 0xf97b : 0xf991', '!airports_present ? 0xf991 : 0xf97b'),
    ('output-canada-place', 'places[1][0] = (char *)countries[0];\n            string_load(&countries[1]', 'places[1][0] = 0;\n            string_load(&countries[1]'),
    ('history-boundary', '(int32_t)next <= 100', '(int32_t)next < 100'),
    ('history-reload', 'U32(object,0x1104+U32(object,0x1420)*8) = resource;', 'U32(object,0x1104+index*8) = resource;'),
    ('transition-back-history', '    if (!back) {', '    if (back) {'),
    ('close-answer', 'U32(PTR(object,0x10dc),0)=0;', 'U32(PTR(object,0x10dc),0)=1;'),
    ('close-delete-flags', 'PTR(PTR(output,0),4))(output,1);', 'PTR(PTR(output,0),4))(output,0);'),
    ('home-resource', '0x1104+U32(object,0x1420)*8),0);', '0x1100+U32(object,0x1420)*8),0);'),
    ('back-clamp', 'if((int32_t)(index-1)>=0)--index;', 'if((int32_t)(index-1)>0)--index;'),
    ('command-return', 'return API(0x257ac,int', 'return 1+API(0x257ac,int'),
    ('pixel-stride', '(U32(data,4)+3)&~3u', '(U32(data,4)+7)&~3u'),
    ('pixel-bottom-up', 'U32(data,8)-(uint32_t)y-1', 'U32(data,8)-(uint32_t)y-2'),
    ('paint-width', '(int)U32(dib,8)', '(int)U32(dib,12)'),
    ('paint-rop', '0xcc0020);', '0xcc0021);'),
    ('mouse-selection', 'U32(object,0x10e4)=selected;', 'U32(object,0x10e4)=selected+1;'),
    ('mouse-redraw-flag', 'U32(object,0x1424)=0;', 'U32(object,0x1424)=1;'),
    ('keyboard-wrap', '(int32_t)index>=(int32_t)count)index=0;', '(int32_t)index>=(int32_t)count)index=1;'),
    ('init-base-result', 'if(!API(0x257c0,int (TC *)(void *))(object))return 1;', 'if(!API(0x257c0,int (TC *)(void *))(object))return 0;'),
    ('init-radio-resource', 'AT(object,0x530),0x134', 'AT(object,0x530),0x135'),
    ('init-font-color', 's_font[2]=0xffffff;', 's_font[2]=0x00ff00;'),
    ('init-output-style', 'output_control,0x403108c4,', 'output_control,0x403108c5,'),
    ('init-format-font', 'index?"Verdana":"Arial"', 'index?"Arial":"Verdana"'),
    ('init-paragraph-tabs', '{1200,2500,4400,6300}', '{1201,2500,4400,6300}'),
    ('init-cleanup-delete', 'API(0x25520,void (*)(void *))(((struct facilities_allocation *)frame)->object);', 'API(0x254bc,int (SC *)(uint32_t))((uint32_t)((struct facilities_allocation *)frame)->object);'),
)
for name, before, after in mutations:
    if source.count(before) != 1:
        raise SystemExit(f'{name}: anchor count {source.count(before)}')
for name, before, after in mutations:
    path = output / f'{name}.c'
    executable = output / f'{name}.exe'
    path.write_text(source.replace(before, after))
    subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-strict-aliasing', '-Isrc/world', '-o', str(executable), 'tools/difftest/worldfacilitiesdiff.c', str(path), '-lntdll'], cwd=root, check=True)
    result = subprocess.run(['wine', str(executable), '64', 'unwind'], cwd=root, env=os.environ.copy(), capture_output=True, text=True, timeout=180)
    (output / f'{name}.log').write_text(result.stdout + result.stderr)
    if result.returncode != 1 or 'FACILITIES DIFFER scenario=' not in result.stderr:
        raise SystemExit(f'{name}: no actual difference (exit {result.returncode})\n{result.stderr}')
    print(f'{name}: detected', flush=True)
print(f'World facilities core/init: {len(mutations)}/{len(mutations)} mutations detected')
