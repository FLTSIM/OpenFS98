#!/usr/bin/env python3
"""Build isolated complexity-core mutants; require actual DIFFER transcripts."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parent.parent
output_dir = root / 'bin/world-diff/complexity-breaks'
output_dir.mkdir(parents=True, exist_ok=True)
source_text = (root / 'src/world/world_complexity.c').read_text()
mutations = (
    ('collect-first-source', '= U32(object, 0x1648);', '= U32(object, 0x1580);'),
    ('collect-second-source', '= U32(object, 0x1580);', '= U32(object, 0x1710);'),
    ('collect-third-destination', '0x1c) = U32(object, 0x1710);', '0x14) = U32(object, 0x1710);'),
    ('collect-message', '0x147, 0, 0);', '0x14e, 0, 0);'),
    ('collect-selection-normalized', '= (uint32_t)selection;', '= selection != 0;'),
    ('collect-descriptor-reload', 'int selection;', 'int selection; void *saved = PTR(object, 0x10e0);'),
    ('populate-first-control', 'AT(object, 0x158c),', 'AT(object, 0x14c4),'),
    ('populate-second-field', '0x14c4), (int)U32(PTR(object, 0x10e0), 0x14)',
     '0x14c4), (int)U32(PTR(object, 0x10e0), 0xc)'),
    ('populate-third-normalized', '0x1654), (int)U32(PTR(object, 0x10e0), 0x1c)',
     '0x1654), U32(PTR(object, 0x10e0), 0x1c) != 0'),
    ('populate-descriptor-cache', 's_services->check(AT(object, 0x158c),',
     'void *saved = PTR(object, 0x10e0); s_services->check(AT(object, 0x158c),'),
    ('populate-message', '0x14e, U32', '0x147, U32'),
    ('cancel-state', '0x171c) = 2;', '0x171c) = 1;'),
    ('cancel-answer', 'U32(descriptor, 0) = 0;', 'U32(descriptor, 0) = 1;'),
    ('accept-state', '0x171c) = 1;', '0x171c) = 2;'),
    ('accept-answer', 'U32(descriptor, 0) = 1;', 'U32(descriptor, 0) = 0;'),
    ('accept-skip-collection', 'fs98_world_complexity_collect(object);', '(void)object;'),
    ('accept-skip-validation', 'if (fs98_world_complexity_check(object) &&', 'if (0 &&'),
    ('accept-reject-validation', 'if (fs98_world_complexity_check(object) && fs98_world_complexity_error(object)) return;',
     'if (fs98_world_complexity_check(object)) return;'),
    ('accept-cache-close', 'void *descriptor;\n    void **vtable;',
     'void *descriptor;\n    void **vtable;\n    void **saved = PTR(object, 0);'),
    ('descriptor-validity', 'return PTR(object, 0x10e0) != 0;', 'return PTR(object, 0x10e0) == 0;'),
    ('callback-result', 'return ((int (SC *)(void *))PTR(descriptor, 4))(descriptor);',
     '((int (SC *)(void *))PTR(descriptor, 4))(descriptor); return 0;'),
    ('error-result', '(void)object; return 0;', '(void)object; return 1;'),
)
for name, before, after in mutations:
    if source_text.count(before) != 1:
        raise SystemExit(f'{name}: mutation anchor count changed')
    mutant = source_text.replace(before, after)
    if name == 'collect-descriptor-reload':
        mutant = mutant.replace('U32(PTR(object, 0x10e0), 0x24) =', 'U32(saved, 0x24) =')
    if name == 'populate-descriptor-cache':
        start = mutant.index('void fs98_world_complexity_populate')
        end = mutant.index('/* 0x223fb360 */', start)
        body = mutant[start:end].replace('U32(PTR(object, 0x10e0),', 'U32(saved,')
        mutant = mutant[:start] + body + mutant[end:]
    if name == 'accept-cache-close':
        # Only the accept routine has an uninitialized descriptor declaration.
        mutant = mutant.replace('vtable = PTR(object, 0);\n    U32(descriptor, 0) = 1;',
                                'vtable = saved;\n    U32(descriptor, 0) = 1;')
    source_path = output_dir / f'{name}.c'
    executable = output_dir / f'{name}.exe'
    source_path.write_text(mutant)
    subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fno-strict-aliasing', '-Isrc/world', '-o', str(executable),
                    'tools/difftest/worldcomplexitydiff.c', str(source_path)], cwd=root, check=True)
    result = subprocess.run(['wine', str(executable), '4096', '0x4a971b63', 'normal'],
                            cwd=root, env=os.environ.copy(), capture_output=True,
                            text=True, timeout=120)
    (output_dir / f'{name}.log').write_text(result.stdout + result.stderr)
    if result.returncode != 1 or 'DIFFER scenario=' not in result.stderr:
        raise SystemExit(f'{name}: no actual difference (exit {result.returncode})\n{result.stderr}')
    print(f'{name}: detected', flush=True)
print(f'World complexity core: {len(mutations)} / {len(mutations)} mutations detected')
