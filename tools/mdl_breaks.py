#!/usr/bin/env python3
"""Build fresh, isolated callback mutations and require a behavioral mismatch.

Run after make -f tools/mdl.mk mdl-normal-diff. Uses its private Wine prefix.
Neither tracked source nor the normal harness executable is changed.
"""
import os
from pathlib import Path
import subprocess


def main():
    root = Path(__file__).resolve().parents[1]
    source = (root / 'src/mdl/mdl_normal.c').read_text()
    output = root / 'bin/mdl-diff/breaks'
    output.mkdir(parents=True, exist_ok=True)
    cases = [
        ('copy-source', 'global + 0x560, 0x24', 'global + 0x561, 0x24', 'copy calls'),
        ('program-word', 'I16(0xea12, altitude);', 'I16(0xea12, altitude + 1);', '.data'),
        ('partial-eax', 'eax = (G32(0xd48) & 0xffff0000u) | ax;', 'eax = ax;', 'state/canaries'),
        ('jet-threshold', 'if (*dx >= 0x4000)', 'if (*dx >= 0x3fff)', 'state/canaries'),
        ('cessna-threshold', 'if (ax > 0x4000)', 'if (ax > 0x4001)', 'state/canaries'),
        ('sopwith-pointer', '(uintptr_t)(image + 0x1037)', '(uintptr_t)(image + 0x1038)', 'state/canaries'),
    ]
    env = dict(os.environ, WINEPREFIX=str(root / 'bin/mdl-diff/wineprefix'),
               WINEARCH='win32', WINEDEBUG='-all')
    for name, before, after, wanted in cases:
        if source.count(before) != 1:
            raise RuntimeError(f'{name}: mutation anchor is not unique')
        modified = output / (name + '.c')
        executable = output / (name + '.exe')
        modified.write_text(source.replace(before, after))
        executable.unlink(missing_ok=True)
        subprocess.run([
            'i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra',
            '-Iinclude', '-Isrc/mdl', '-Ibin/mdl-diff', '-o', str(executable),
            'tools/difftest/mdldiff.c', str(modified)], cwd=root, check=True)
        result = subprocess.run(['wine', str(executable), '65536'], cwd=root,
                                env=env, text=True, capture_output=True, timeout=180)
        transcript = result.stdout + result.stderr
        (output / (name + '.log')).write_text(transcript)
        if result.returncode != 1 or wanted not in transcript:
            raise RuntimeError(f'{name}: no expected mismatch\n{transcript}')
        print(f'{name}: caught ({transcript.splitlines()[-1]})', flush=True)


if __name__ == '__main__':
    main()
