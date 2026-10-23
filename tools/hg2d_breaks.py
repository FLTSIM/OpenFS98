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
build = root / 'bin/hg2d-breaks'
build.mkdir(parents=True, exist_ok=True)
shutil.copyfile(root / 'bin/hg2d-diff/hg2ddiff.exe', build / 'hg2ddiff.exe')
shutil.copyfile(root / 'orig/Modules/HG2D.DLL', build / 'HG2D_retail.dll')
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
missed = []
for name, old, new in mutations:
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
    result = subprocess.run(['wine', 'hg2ddiff.exe', sys.argv[1] if len(sys.argv) > 1 else '32'],
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
