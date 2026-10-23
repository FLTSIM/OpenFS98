#!/usr/bin/env python3
"""Build isolated, deliberately wrong HG2D DLLs and require detection.

Run from the repository root after make -f tools/hg2d.mk hg2d.
Does not edit the source or the baseline DLL. Wine needs normal host access.
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
source = (root / 'src/hg2d/hg2d.c').read_text()
source = source.replace('#include "hg2d_palette.inc"', (root / 'src/hg2d/hg2d_palette.inc').read_text())
build = root / 'bin/hg2d-breaks'
build.mkdir(parents=True, exist_ok=True)
shutil.copyfile(root / 'bin/hg2d-diff/hg2ddiff.exe', build / 'hg2ddiff.exe')
shutil.copyfile(root / 'orig/Modules/HG2D.DLL', build / 'HG2D_retail.dll')
shutil.copyfile(root / 'orig/Modules/UTIL.DLL', build / 'UTIL_retail.dll')
mutations = [
    ('descriptor width', 'descriptor[1] != 0x100', 'descriptor[1] != 0xff'),
    ('descriptor kind', 'descriptor[0] != 1', 'descriptor[0] != 0'),
    ('create flags', 'flags != 0)', 'flags > 1)'),
    ('last pool slot', 'offset < POOL_END;', 'offset < POOL_END - 0x34;'),
    ('palette byte', 'pixels[index] * 2]', 'pixels[index] * 2 + 1]'),
    ('palette last pixel', 'index < 0x10000;', 'index < 0xffff;'),
    ('FE result', 'if (!load(', 'if (load('),
    ('side texture flag', 'texture[1] |= 0x100000;', 'texture[1] |= 0x200000;'),
    ('load flags byte', '(flags ? 0xff00u : 0)', '(flags ? 0xfe00u : 0)'),
    ('destroy ownership', '!(flags & 1)', '!(flags & 2)'),
    ('destroy first flags', 'texture[0] = texture[1] = texture[2] = texture[3] = 0;', 'texture[0] = texture[2] = texture[3] = 0;'),
    ('access flag', 'texture[1] |= 0x10000;', 'texture[1] |= 0x20000;'),
    ('unaligned handle', 'return handle >= ADDRESS', 'return (handle % 4 == ADDRESS(hg2d_image + POOL) % 4) && handle >= ADDRESS'),
    ('update clear accessed', 'texture[1] &= ~0x10000u;', 'texture[1] &= ~0x20000u;'),
    ('update clear hardware', '(texture[1] & ~0x40000u) | 0x20000', '(texture[1] & ~0x80000u) | 0x20000'),
    ('update low eax', '(flags & 0xff);', '(flags & 0xffff);'),
    ('hardware accounting', 'WORD(0x8800 + index * 4)--;', 'WORD(0x8800 + index * 4)++;'),
    ('hardware scratch', 'index < 8; index++', 'index < 7; index++'),
    ('hardware tail', 'texture[3] = texture[2] = texture[7] = 0;', 'texture[3] = texture[2] = 0;'),
]
mutations = [(*mutation, 'textures') for mutation in mutations]
colour_mutations = [
    ('direct colour 69', 'mode != 0x69 &&', 'mode != 0x68 &&'),
    ('direct colour e0', 'mode != 0xe0)', 'mode != 0xe1)'),
    ('legacy colour bank', 'mode == 0x67 ? 0x34af : 0x349f', 'mode == 0x67 ? 0x349f : 0x34af'),
    ('colour nibble', 'index = colour & 15;', 'index = colour & 7;'),
    ('gradient cutoff', 'pair <= 0x1800', 'pair < 0x1800'),
    ('gradient selector', 'BYTE(0x3402 + index) == 0', 'BYTE(0x3402 + index) != 0'),
    ('gradient delta wrap', '(uint8_t)(BYTE(0x3401 + index) - BYTE(0x3400 + index))', '(uint16_t)(BYTE(0x3401 + index) - BYTE(0x3400 + index))'),
    ('gradient partial doubling', '(product >> 8) + ((product >> 7) & 1)', '(product >> 7)'),
    ('gradient pair', 'pair = HALF(0x3400 + index);', 'pair = HALF(0x3401 + index);'),
    ('current colour', 'BYTE(0x90c) = value;', 'BYTE(0x90c) = 0;'),
    ('second output word', 'out[1] = mapped_pair;', 'out[1] = 0;'),
    ('remap return width', 'return (entry_eax & 0xffff0000u) | result;', 'return result;'),
    ('remap high byte', '((colour >> 8) & 0xff) * 2)', '((colour >> 8) & 0xff) * 2 + 1)'),
    ('layout signed level', '(int16_t)level >= (int16_t)group', 'level >= group'),
    ('layout stride', '& 0xffff) / 32;', '& 0xffff) / 31;'),
    ('layout first run', 'index < 32; index++) {\n        HALF', 'index < 31; index++) {\n        HALF'),
    ('layout group boundary', '(int16_t)level >= (int16_t)(group + 8)', '(int16_t)level > (int16_t)(group + 8)'),
    ('layout distinct zero', 'HALF(0x34c2)++;', 'HALF(0x34c2) += 0;'),
]
mutations += [(*mutation, 'colours') for mutation in colour_mutations]
palette_mutations = [
    ('palette ramp source', 'BYTE(source + 0x5d) << 8', 'BYTE(source + 0x5c) << 8'),
    ('palette destination offset', '(int16_t)product >> 1', '(int16_t)product >> 2'),
    ('palette average carry', '(value + BYTE(*source + channel + 3)) >> 1', '(uint8_t)(value + BYTE(*source + channel + 3)) >> 1'),
    ('palette final run', 'palette_copy(&source, &destination, 143, 0);', 'palette_copy(&source, &destination, 142, 0);'),
    ('palette packed group', 'if (level >= (int16_t)group)', 'if (level > (int16_t)group)'),
    ('palette ramp reserved colours', 'index < 0x74 || index >= 0x80', 'index < 0x74 || index >= 0x7f'),
    ('palette ramp divisor', 'palette_brightness(index, 14)', 'palette_brightness(index, 13)'),
    ('palette shade divisor', 'palette_brightness(index, 16)', 'palette_brightness(index, 15)'),
    ('palette shade generation', 'BYTE(0x4034) ^= 0xa5;', 'BYTE(0x4034) ^= 0xa4;'),
    ('palette inverse word index', 'BYTE(0xa4e + colour) = value;', 'BYTE(0xa4e + (uint8_t)colour) = value;'),
    ('palette shade word index', 'BYTE(0xb4e + colour) = (uint8_t)result;', 'BYTE(0xb4e + (uint8_t)colour) = (uint8_t)result;'),
    ('palette mode notification', 'send(0x14, 0x70, 0);', '(void)send;'),
    ('palette green channel', 'green = BYTE(0x30a1 + index * 3)', 'green = BYTE(0x30a0 + index * 3)'),
    ('night threshold', 'uint8_t night = brightness < 0x80;', 'uint8_t night = brightness < 0x7f;'),
    ('night override', 'if (!(GLOBAL_HALF(0xc46) & 0x20))', 'if (!(GLOBAL_HALF(0xc46) & 0x10))'),
    ('ramp disabled mode', 'if (flag == 1)', 'if (flag == 2)'),
    ('palette cache carry', 'hg2d_palette_changed = 0;', 'hg2d_palette_changed = 1;'),
    ('palette colour source', 'HALF(0x3fc2) ? 0x33d0 : 0x33a0', 'HALF(0x3fc2) ? 0x33a0 : 0x33d0'),
    ('palette scale divisor', 'BYTE(0x30a0 + index) * brightness) >> 8', 'BYTE(0x30a0 + index) * brightness) >> 7'),
    ('palette count clamp', 'count > 64) count = 64;', 'count > 64) count = 63;'),
    ('palette red saturation', 'sum > 255 ? 255 : sum', 'sum > 255 ? 0 : sum'),
    ('palette sky ramp end', 'if (!value) value--;', 'if (!value) value++;'),
    ('palette hardware-count mode', 'if (!WORD(0x4005) || count > 8)', 'if (!WORD(0x4005) || count >= 8)'),
    ('palette realized dirty flag', 'HALF(0x36c0) = 0;', 'HALF(0x36c0) = 1;'),
    ('palette realize high colours', 'GLOBAL_HALF(0xcbe) == 2', 'GLOBAL_HALF(0xcbe) == 1'),
    ('palette realized full source', 'palette_pack(0x30a0, 0x3cc2);', 'palette_pack(0x36c2, 0x3cc2);'),
    ('palette selector low word', 'HALF(0x3fc2) != (uint16_t)selector', 'HALF(0x3fc2) != selector'),
]
mutations += [(*mutation, 'palette') for mutation in palette_mutations]
missed = []
for name, old, new, suite in mutations:
    assert source.count(old) == 1, (name, source.count(old))
    variant = build / 'hg2d_mutant.c'
    variant.write_text(source.replace(old, new))
    library = build / 'HG2D_ours.dll'
    library.unlink(missing_ok=True)
    subprocess.run([
        'i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra',
        '-fno-strict-aliasing', '-shared', '-nostdlib', '-Wl,--entry=_DllMain@12',
        '-Wl,--image-base=0x30000000', '-Wl,--disable-dynamicbase',
        '-I' + str(root / 'src/hg2d'), '-o', str(library), str(variant),
        str(root / 'src/hg2d/hg2d.def')], check=True)
    result = subprocess.run(['wine', 'hg2ddiff.exe', sys.argv[1] if len(sys.argv) > 1 else '64',
                             suite, 'full' if suite == 'colours' else 'scripts'],
                            cwd=build, env={**os.environ, 'WINEDEBUG': '-all'},
                            capture_output=True, text=True, timeout=120)
    # Only a reported differential mismatch counts; a broken fixture or a
    # harness crash must be investigated, not counted as a caught mutation.
    caught = result.returncode == 1 and ' differs:' in result.stdout
    print(('%s: ' % name) + ('caught' if caught else 'MISSED / ERROR'), flush=True)
    if not caught:
        print(result.stdout + result.stderr, flush=True)
        missed.append(name)
print('%d mutations, %d caught, %d unresolved' % (len(mutations), len(mutations) - len(missed), len(missed)))
sys.exit(bool(missed))
