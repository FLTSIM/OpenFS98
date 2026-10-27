#!/usr/bin/env python3
"""Require actual retail mismatches for position core/init mutations."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parent.parent
output_dir = root / 'bin/world-diff/position-breaks'
output_dir.mkdir(parents=True, exist_ok=True)
source_text = (root / 'src/world/world_position.c').read_text()
mutations = (
    ('init-populate-vtable', '((void (TC *)(void *))vtable[0xf0 / 4])(object);', '((void (TC *)(void *))PTR(PTR(object, 0), 0xf0))(object);'),
    ('collect-order', '{2, 0, 1, 3, 4, 7, 5, 6}', '{0, 2, 1, 3, 4, 7, 5, 6}'),
    ('collect-capacity', 'U32(descriptor, field->status + 8));', 'U32(descriptor, field->status + 8) - 1);'),
    ('snapshot-disabled', 'if (!enabled) return;', 'if (enabled) return;'),
    ('snapshot-buffer', '0x64, 0x1145', '0x64, 0x1146'),
    ('populate-button', '0x18) == 0);', '0x18) != 0);'),
    ('validation-priority', '{0x50, 0x94, 0xac,', '{0x94, 0x50, 0xac,'),
    ('validation-collect', '    ((void (TC *)(void *))vtable[0xf4 / 4])(object);', '    (void)vtable; /* missing virtual collection */'),
    ('validation-check', '    position_callback(object, 4);', '    /* missing owner check */'),
    ('error-priority', '{2, 7, 3, 5, 6, 4, 0, 1}', '{7, 2, 3, 5, 6, 4, 0, 1}'),
    ('error-focus', 'if (s_services->events->error(object, AT(object, field->control), AT(descriptor, field->status))) control = s_controls[index];', 'if (s_services->events->error(object, AT(object, field->control), AT(descriptor, field->status))) control = 0 * s_controls[index];'),
    ('reload-callback', '    position_callback(object, 0x10);', '    position_callback(object, 0xc);'),
    ('revert-notifications', '    U32(object, 0x26e8) = 0;', '    U32(object, 0x26e8) = 1;'),
    ('changed-exact-one', 'if (U32(object, 0x26e8) == 1)', 'if (U32(object, 0x26e8) != 0)'),
    ('accept-apply', '        position_callback(object, 8);', '        position_callback(object, 0xc);'),
    ('accept-answer', 'U32(PTR(object, 0x10dc), 0) = 1;', 'U32(PTR(object, 0x10dc), 0) = 0;'),
    ('accept-close-vtable', '((void (TC *)(void *))vtable[0xec / 4])(object);', '((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);'),
    ('cancel-answer', '    U32(descriptor, 0) = 0;', '    U32(descriptor, 0) = 1;'),
    ('facilities-callback', 'position_callback(object, 0x14);', 'position_callback(object, 0xc);'),
    ('spin-range', 'position_spin(object, 0x1d44, 0x10168);', 'position_spin(object, 0x1d44, 0x10167);'),
    ('spin-dimensions', 'rect.right - (uint32_t)rect.left', 'rect.right - (uint32_t)rect.left + 1'),
    ('speed-range', 'position_spin(object, 0x1ea4, 0x1f4);', 'position_spin(object, 0x1ea4, 0x1f3);'),
    ('edit-limit', 'field->status + 8) - 1, 0', 'field->status + 8), 0'),
    ('edit-color', 'font, 0xffffff, 0, 0xff0000,', 'font, 0x00ff00, 0, 0xff0000,'),
    ('edit-flags', 'position_edit(object, 0x1bbc, 0x1b04, 3);', 'position_edit(object, 0x1bbc, 0x1b04, 0);'),
    ('init-return', '    ui->focus_control(AT(object, 0x197c));\n    return 0;', '    ui->focus_control(AT(object, 0x197c));\n    return 1;'),
    ('init-invalid-descriptor', '|| U32(object, 0x10dc) == 0xcdcdcdcd', '|| U32(object, 0x10dc) == 0xcdcdcdcd || U32(object, 0x10dc) != 0'),
)

for name, before, after in mutations:
    if source_text.count(before) != (3 if name == 'init-populate-vtable' else 2 if name in ('revert-notifications', 'accept-close-vtable') else 1):
        raise SystemExit(f'{name}: mutation anchor is not unique')
    source_path = output_dir / f'{name}.c'
    executable = output_dir / f'{name}.exe'
    mutated = source_text.replace(before, after)
    if name == 'accept-close-vtable': mutated = mutated.replace('void **vtable = PTR(object, 0);', 'void **vtable = PTR(object, 0); (void)vtable;')
    if name == 'init-populate-vtable': mutated = mutated.replace('void **vtable;', 'void **vtable __attribute__((unused));')
    source_path.write_text(mutated)
    subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fno-strict-aliasing', '-fno-delete-null-pointer-checks', '-Isrc/world',
                    '-o', str(executable), 'tools/difftest/worldpositiondiff.c', str(source_path),
                    'src/world/world_init.c', 'src/world/world_area.c',
                    '-lntdll'], cwd=root, check=True)
    result = subprocess.run(['wine', str(executable), '1024', 'unwind'],
                            cwd=root, env=os.environ.copy(), capture_output=True,
                            text=True, timeout=120)
    (output_dir / f'{name}.log').write_text(result.stdout + result.stderr)
    if result.returncode != 1 or 'POSITION DIFFER scenario=' not in result.stderr:
        raise SystemExit(f'{name}: no actual difference (exit {result.returncode})\n{result.stderr}')
    print(f'{name}: detected', flush=True)
print(f'World position core/init: {len(mutations)} / {len(mutations)} mutations detected')
