#!/usr/bin/env python3
"""Build independent area-core mutations and require actual retail differences."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parent.parent
output_dir = root / 'bin/world-diff/breaks'
output_dir.mkdir(parents=True, exist_ok=True)
source_text = (root / 'src/world/world_area.c').read_text()
mutations = (
    ('position-format', '"%04d"', '"%d"'),
    ('position-capacity', 'position, 0x1f)', 'position, 0x20)'),
    ('layer-conversion', '(uint32_t)(selection + 1) * 10', '(uint32_t)(selection + 1) * 20'),
    ('cancel-answer', 'WORLD_U32(descriptor, 0) = 0;', 'WORLD_U32(descriptor, 0) = 1;'),
    ('accept-answer', 'WORLD_U32(WORLD_PTR(object, 0x10e4), 0) = 1;',
     'WORLD_U32(WORLD_PTR(object, 0x10e4), 0) = 0;'),
    ('title-priority', 'return 0x1b07;', 'return 0x1aef;'),
    ('error-return', 'return ((world_validate_fn)vtable[0xe0 / 4])(object, (uint32_t)control);',
     '((world_validate_fn)vtable[0xe0 / 4])(object, (uint32_t)control); return control;'),
    ('path-capacity', '0x1938, WORLD_PTR(row, 4), 0x80);\n    WORLD_U32(row, 0x34)',
     '0x1938, WORLD_PTR(row, 4), 0x7f);\n    WORLD_U32(row, 0x34)'),
    ('protected-title', '0x1278), 0xcf, WORLD_U32(row, 0x3c), 0);',
     '0x1278), 0xcf, 0, 0);'),
    ('missing-enable', 'sizeof(s_controls) / sizeof(s_controls[0])',
     'sizeof(s_controls) / sizeof(s_controls[0]) - 1'),
    ('reload-close-vtable', '''WORLD_U32(WORLD_PTR(object, 0x10e4), 0) = 1;
    ((world_close_fn)vtable[0xec / 4])(object);''',
     '''WORLD_U32(WORLD_PTR(object, 0x10e4), 0) = 1;
    vtable = WORLD_PTR(object, 0);
    ((world_close_fn)vtable[0xec / 4])(object);'''),
)
for name, before, after in mutations:
    if source_text.count(before) != 1:
        raise SystemExit(f'{name}: mutation anchor is not unique')
    source_path = output_dir / f'{name}.c'
    executable = output_dir / f'{name}.exe'
    source_path.write_text(source_text.replace(before, after))
    # Every mutation uses a distinct executable built from the changed source.
    compile = ['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                '-fno-strict-aliasing', '-Isrc/world', '-o', str(executable),
                'tools/difftest/worlddiff.c', str(source_path)]
    subprocess.run(compile, cwd=root, check=True)
    run_result = subprocess.run(['wine', str(executable), '512', '0x4a971b63', 'normal'],
                             cwd=root, env=os.environ.copy(), capture_output=True,
                             text=True, timeout=120)
    (output_dir / f'{name}.log').write_text(run_result.stdout + run_result.stderr)
    if run_result.returncode != 1 or 'DIFFER scenario=' not in run_result.stderr:
        raise SystemExit(f'{name}: no actual difference (exit {run_result.returncode})\n{run_result.stderr}')
    print(f'{name}: detected', flush=True)
print(f'World area core: {len(mutations)} / {len(mutations)} mutations detected')
