#!/usr/bin/env python3
"""Build isolated dynamic-core mutants; require actual DIFFER transcripts."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parent.parent
output_dir = root / 'bin/world-diff/dynamic-breaks'
output_dir.mkdir(parents=True, exist_ok=True)
source_text = (root / 'src/world/world_dynamic.c').read_text()
mutations = (
    ('collect-first', '= U32(object, 0x1580);', '= U32(object, 0x1648);'),
    ('collect-second', '= U32(object, 0x1648);', '= U32(object, 0x1710);'),
    ('collect-third', '= U32(object, 0x1710);', '= U32(object, 0x17d8);'),
    ('collect-fourth', '= U32(object, 0x17d8);', '= U32(object, 0x1580);'),
    ('collect-selection', '= (uint32_t)selection;', '= selection != 0;'),
    ('collect-message', '0x147, 0, 0);', '0x14e, 0, 0);'),
    ('populate-control', 'check(AT(object, 0x171c),', 'check(AT(object, 0x1654),'),
    ('populate-raw', '0x171c), (int)U32(PTR(object, 0x10e0), 0xc)', '0x171c), U32(PTR(object, 0x10e0), 0xc) != 0'),
    ('populate-field', '0x158c), (int)U32(PTR(object, 0x10e0), 0x1c)', '0x158c), (int)U32(PTR(object, 0x10e0), 0x24)'),
    ('populate-message', '0x14e, U32', '0x147, U32'),
    ('cancel-mode', '0x17e4) = 2;', '0x17e4) = 1;'),
    ('cancel-answer', 'U32(descriptor, 0) = 0;', 'U32(descriptor, 0) = 1;'),
    ('accept-mode', '0x17e4) = 1;', '0x17e4) = 2;'),
    ('accept-answer', 'U32(descriptor, 0) = 1;', 'U32(descriptor, 0) = 0;'),
    ('accept-collect', 'fs98_world_dynamic_collect(object);', '(void)object;'),
    ('accept-check', 'if (fs98_world_dynamic_check(object) &&', 'if (0 &&'),
    ('accept-veto', 'if (fs98_world_dynamic_check(object) && fs98_world_dynamic_error(object)) return;', 'if (fs98_world_dynamic_check(object)) return;'),
    ('accept-no-errors', 'if (fs98_world_dynamic_check(object) && fs98_world_dynamic_error(object)) return;', 'if (fs98_world_dynamic_check(object) && 0) return;'),
    ('descriptor', 'return PTR(object, 0x10e0) != 0;', 'return PTR(object, 0x10e0) == 0;'),
    ('check-result', 'return ((int (SC *)(void *))PTR(descriptor, 4))(descriptor);', '((int (SC *)(void *))PTR(descriptor, 4))(descriptor); return 0;'),
    ('error-order', '{0x20, 0x18, 0x10, 8}', '{8, 0x18, 0x10, 0x20}'),
    ('error-control', '{0x14c4, 0x158c, 0x1654, 0x171c}', '{0x158c, 0x14c4, 0x1654, 0x171c}'),
    ('error-focus', 's_services->focus(control);', '(void)control;'),
    ('error-post', '0xb1, 0, 0xffffffff', '0xb1, 0, 0'),
    ('error-store', 'U32(object, 0x8c) = (uint32_t)result;', 'U32(object, 0x8c) = result != 0;'),
    ('error-reload', 'return (int)U32(object, 0x8c);', 'return result;'),
)
for name, before, after in mutations:
    if source_text.count(before) != 1:
        raise SystemExit(f'{name}: mutation anchor count changed')
    mutant = source_text.replace(before, after)
    source_path = output_dir / f'{name}.c'
    executable = output_dir / f'{name}.exe'
    source_path.write_text(mutant)
    subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fno-strict-aliasing', '-Isrc/world', '-o', str(executable),
                    'tools/difftest/worlddynamicdiff.c', str(source_path)], cwd=root, check=True)
    result = subprocess.run(['wine', str(executable), '4096', '0x4a971b63', 'normal'],
                            cwd=root, env=os.environ.copy(), capture_output=True,
                            text=True, timeout=120)
    (output_dir / f'{name}.log').write_text(result.stdout + result.stderr)
    if result.returncode != 1 or 'DIFFER scenario=' not in result.stderr:
        raise SystemExit(f'{name}: no actual difference (exit {result.returncode})\n{result.stderr}')
    print(f'{name}: detected', flush=True)
print(f'World dynamic core: {len(mutations)} / {len(mutations)} mutations detected')
