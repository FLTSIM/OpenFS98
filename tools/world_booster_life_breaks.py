#!/usr/bin/env python3
"""Require actual retail mismatches for booster-class lifetime mutations."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parent.parent
output_dir = root / 'bin/world-diff/booster-life-breaks'
output_dir.mkdir(parents=True, exist_ok=True)
source_text = (root / 'src/world/world_booster_objects.c').read_text()
mutations = (
    ('constructor-control', 'state.constructed < sizeof(s_booster_controls) / sizeof(s_booster_controls[0])',
     'state.constructed + 1 < sizeof(s_booster_controls) / sizeof(s_booster_controls[0])'),
    ('base-destructor', 's_object_services->destroy(state->object, WORLD_BOOSTER_BASE_DTOR);', '(void)state;'),
    ('delete-flag', 'if (flags & 1)', 'if (flags & 2)'),
    ('allocation-cleanup', 's_object_services->dialog_delete(state->object);', 's_object_services->ui_delete(state->object);'),
    ('factory-parent', 'create(object, 0x6ab, NULL)', 'create(object, 0x6ab, (void *)parent)'),
    ('create-message', 'if (!s_object_services->create', 'if (s_object_services->create'),
    ('typed-catch-cleanup', 'world_factory_cleanup(&state->frame);', 'state->live = 0; world_factory_cleanup(&state->frame);'),
    ('factory-ownership', 'state.live = 0;\n    if (!s_object_services->create',
     'state.live = 1;\n    if (!s_object_services->create'),
    ('ddx-omission', 'index < sizeof(s_booster_ddx) / sizeof(s_booster_ddx[0])',
     'index + 1 < sizeof(s_booster_ddx) / sizeof(s_booster_ddx[0])'),
    ('help-command', '"fs98.hlp", 12,', '"fs98.hlp", 10,'),
    ('context-result', '    return 1;', '    return 0;'),
    ('post-destroy-delete', 'vtable[1])(object, 1);', 'vtable[1])(object, 0);'),
    ('cancel-answer', 'WORLD_U32(descriptor, 0) = 0;', 'WORLD_U32(descriptor, 0) = 1;'),
)
for name, before, after in mutations:
    if source_text.count(before) != 1:
        raise SystemExit(f'{name}: mutation anchor is not unique')
    source_path = output_dir / f'{name}.c'
    executable = output_dir / f'{name}.exe'
    source_path.write_text(source_text.replace(before, after))
    subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fno-strict-aliasing', '-fno-delete-null-pointer-checks', '-Isrc/world',
                    '-o', str(executable), 'tools/difftest/worldboosterlifediff.c', str(source_path),
                    '-lntdll'], cwd=root, check=True)
    result = subprocess.run(['wine', str(executable), '32', '0x4a971b63', 'typed'],
                            cwd=root, env=os.environ.copy(), capture_output=True,
                            text=True, timeout=120)
    (output_dir / f'{name}.log').write_text(result.stdout + result.stderr)
    if result.returncode != 1 or 'DIFFER scenario=' not in result.stderr:
        raise SystemExit(f'{name}: no actual difference (exit {result.returncode})\n{result.stderr}')
    print(f'{name}: detected', flush=True)
print(f'World booster lifetime: {len(mutations)} / {len(mutations)} mutations detected')
