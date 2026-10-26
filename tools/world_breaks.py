#!/usr/bin/env python3
"""Build independent area-core mutations and require actual retail differences."""
from pathlib import Path
import os
import subprocess

pRoot = Path(__file__).resolve().parent.parent
pOutput = pRoot / 'bin/world-diff/breaks'
pOutput.mkdir(parents=True, exist_ok=True)
pcSource = (pRoot / 'src/world/world_area.c').read_text()
aMutations = (
    ('position-format', '"%04d"', '"%d"'),
    ('position-capacity', 'acPosition, 0x1f)', 'acPosition, 0x20)'),
    ('layer-conversion', '(uint32)(iSelection + 1) * 10', '(uint32)(iSelection + 1) * 20'),
    ('cancel-answer', 'WORLD_U32(pDescriptor, 0) = 0;', 'WORLD_U32(pDescriptor, 0) = 1;'),
    ('accept-answer', 'WORLD_U32(WORLD_PTR(pObject, 0x10e4), 0) = 1;',
     'WORLD_U32(WORLD_PTR(pObject, 0x10e4), 0) = 0;'),
    ('title-priority', 'return 0x1b07;', 'return 0x1aef;'),
    ('error-return', 'return ((world_validate_fn)ppVtable[0xe0 / 4])(pObject, (uint32)iControl);',
     '((world_validate_fn)ppVtable[0xe0 / 4])(pObject, (uint32)iControl); return iControl;'),
    ('path-capacity', '0x1938, WORLD_PTR(pRow, 4), 0x80);\n    WORLD_U32(pRow, 0x34)',
     '0x1938, WORLD_PTR(pRow, 4), 0x7f);\n    WORLD_U32(pRow, 0x34)'),
    ('protected-title', '0x1278), 0xcf, WORLD_U32(pRow, 0x3c), 0);',
     '0x1278), 0xcf, 0, 0);'),
    ('missing-enable', 'sizeof(s_auiControls) / sizeof(s_auiControls[0])',
     'sizeof(s_auiControls) / sizeof(s_auiControls[0]) - 1'),
    ('reload-close-vtable', '''WORLD_U32(WORLD_PTR(pObject, 0x10e4), 0) = 1;
    ((world_close_fn)ppVtable[0xec / 4])(pObject);''',
     '''WORLD_U32(WORLD_PTR(pObject, 0x10e4), 0) = 1;
    ppVtable = WORLD_PTR(pObject, 0);
    ((world_close_fn)ppVtable[0xec / 4])(pObject);'''),
)
for pcName, pcBefore, pcAfter in aMutations:
    if pcSource.count(pcBefore) != 1:
        raise SystemExit(f'{pcName}: mutation anchor is not unique')
    pSource = pOutput / f'{pcName}.c'
    pExecutable = pOutput / f'{pcName}.exe'
    pSource.write_text(pcSource.replace(pcBefore, pcAfter))
    # Every mutation uses a distinct executable built from the changed source.
    aCompile = ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                '-fno-strict-aliasing', '-Isrc/world', '-o', str(pExecutable),
                'tools/difftest/worlddiff.c', str(pSource)]
    subprocess.run(aCompile, cwd=pRoot, check=True)
    oResult = subprocess.run(['wine', str(pExecutable), '512', '0x4a971b63', 'normal'],
                             cwd=pRoot, env=os.environ.copy(), capture_output=True,
                             text=True, timeout=120)
    (pOutput / f'{pcName}.log').write_text(oResult.stdout + oResult.stderr)
    if oResult.returncode != 1 or 'DIFFER scenario=' not in oResult.stderr:
        raise SystemExit(f'{pcName}: no actual difference (exit {oResult.returncode})\n{oResult.stderr}')
    print(f'{pcName}: detected', flush=True)
print(f'World area core: {len(aMutations)} / {len(aMutations)} mutations detected')
