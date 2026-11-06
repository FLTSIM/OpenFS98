#!/usr/bin/env python3
"""Require isolated bad DLL builds to fail the loaded-module comparison."""
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess

from build_mdls import build


def main():
    root = Path.cwd()
    output = root / 'bin/mdl-diff'
    record = json.loads((output / 'modules.json').read_text())[0]
    original = output / Path(record['path']).stem
    source = Path('src/mdl/mdl_module.c').read_text()
    cases = [('program-pointer', 'relocated pointer'),
             ('program-byte', 'initial model data/header/Linkage'),
             ('update-pointer', 'copy count'), ('state-init', 'state init'),
             ('detach-register', 'lifecycle ecx/edx'), ('resource-byte', 'resource bytes')]
    env = dict(os.environ, WINEPREFIX=str(output / 'wineprefix'), WINEARCH='win32', WINEDEBUG='-all')
    for name, wanted in cases:
        directory = output / 'module-breaks' / name
        directory.mkdir(parents=True, exist_ok=True)
        for file in original.iterdir():
            if file.name != 'build.log':
                shutil.copy2(file, directory / file.name)
        assembly = (directory / 'model_data.S').read_text().replace(
            str(original.resolve()), str(directory.resolve()))
        modified_source = source
        if name == 'program-pointer':
            old = '.long _mdl_image + 0x3034'
            if assembly.count(old) != 1: raise ValueError('program pointer moved')
            assembly = assembly.replace(old, '.long _mdl_image + 0x3036')
        elif name == 'program-byte':
            payload = bytearray((directory / 'model.data.bin').read_bytes())
            payload[0x34] ^= 1
            (directory / 'model.data.bin').write_bytes(payload)
        elif name == 'update-pointer':
            assembly = assembly.replace('.long _mdl_update@4', '.long _mdl_state@4')
        elif name == 'state-init':
            old = '_mdl_state@4:\\n\\tret $4'
            new = '_mdl_state@4:\\n\\tpushl %eax\\n\\tmovl 8(%esp), %eax\\n\\tmovb $1, (%eax)\\n\\tpopl %eax\\n\\tret $4'
            if modified_source.count(old) != 1: raise ValueError('state-init anchor moved')
            modified_source = modified_source.replace(old, new)
        elif name == 'detach-register':
            old = '_mdl_detach:\\n\\tret'
            if modified_source.count(old) != 1: raise ValueError('detach anchor moved')
            modified_source = modified_source.replace(old, '_mdl_detach:\\n\\txorl %ecx, %ecx\\n\\tret')
        elif name == 'resource-byte':
            payload = bytearray((directory / 'model.res').read_bytes())
            length, header_size = struct.unpack_from('<II', payload, 32)
            if length < 64: raise ValueError('version resource changed')
            payload[32 + header_size + 63] ^= 1
            (directory / 'model.res').write_bytes(payload)
        (directory / 'model_data.S').write_text(assembly)
        modified = directory / 'mdl_module.c'
        modified.write_text(modified_source)
        mutant = dict(record, rebuilt=str(directory / Path(record['rebuilt']).name))
        Path(mutant['rebuilt']).unlink(missing_ok=True)
        build(mutant, directory, modified)
        result = subprocess.run(['wine', str(output / 'mdldlldiff.exe'), '32', 'relocate', mutant['rebuilt']],
                                env=env, text=True, capture_output=True, timeout=60)
        transcript = result.stdout + result.stderr
        (directory / 'test.log').write_text(transcript)
        if result.returncode != 1 or wanted not in transcript:
            raise RuntimeError(f'{name}: expected mismatch not observed\n{transcript}')
        print(f'{name}: caught ({transcript.splitlines()[-1]})', flush=True)


if __name__ == '__main__':
    main()
