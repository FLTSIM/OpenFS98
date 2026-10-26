#!/usr/bin/env python3
"""Require actual retail mismatches for search-class lifetime mutations."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parent.parent
output_dir = root / 'bin/world-diff/search-life-breaks'
output_dir.mkdir(parents=True, exist_ok=True)
source_text = (root / 'src/world/world_search_objects.c').read_text()
mutations = (
    ('constructor-vtable-timing', 'state.base = 1;\n    while',
     'state.base = 1; WORLD_PTR(object, 0) = s_search_vtable;\n    while'),
    ('destructor-vtable-reset', 'state.object = object; state.base = 1;',
     'state.object = object; state.base = 1; WORLD_PTR(object, 0) = s_search_vtable;'),
    ('factory-allocation-size', 'allocate(0x1e78)', 'allocate(0x1e74)'),
    ('factory-template', 'create(object, 0x6b0, NULL)', 'create(object, 0x6b4, NULL)'),
    ('factory-show-handle', '    s_object_services->show(object, 1);',
     '    uint32_t handle = WORLD_U32(object, 0x20);\n    s_object_services->show(object, 1);\n    WORLD_U32(object, 0x20) = handle;'),
    ('constructor-control', 'state.constructed < sizeof(s_search_controls) / sizeof(s_search_controls[0])',
     'state.constructed + 1 < sizeof(s_search_controls) / sizeof(s_search_controls[0])'),
    ('base-destructor', 's_object_services->destroy(state->object, WORLD_SEARCH_BASE_DTOR);', '(void)state;'),
    ('delete-flag', 'if (flags & 1)', 'if (flags & 2)'),
    ('allocation-cleanup', 's_object_services->dialog_delete(state->object);', 's_object_services->ui_delete(state->object);'),
    ('factory-parent', 'create(object, 0x6b0, NULL)', 'create(object, 0x6b0, (void *)parent)'),
    ('create-message', 'if (!s_object_services->create', 'if (s_object_services->create'),
    ('typed-catch-cleanup', 'world_factory_cleanup(&state->frame);', 'state->live = 0; world_factory_cleanup(&state->frame);'),
    ('factory-ownership', 'state.live = 0;\n    if (!s_object_services->create',
     'state.live = 1;\n    if (!s_object_services->create'),
    ('ddx-omission', 'index < sizeof(s_search_ddx) / sizeof(s_search_ddx[0])',
     'index + 1 < sizeof(s_search_ddx) / sizeof(s_search_ddx[0])'),
    ('help-command', '"fs98.hlp", 12,', '"fs98.hlp", 10,'),
    ('context-result', '    return 1;', '    return 0;'),
    ('post-destroy-delete', 'vtable[1])(object, 1);', 'vtable[1])(object, 0);'),
)
for name, before, after in mutations:
    if source_text.count(before) != 1:
        raise SystemExit(f'{name}: mutation anchor is not unique')
    source_path = output_dir / f'{name}.c'
    executable = output_dir / f'{name}.exe'
    source_path.write_text(source_text.replace(before, after))
    subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fno-strict-aliasing', '-fno-delete-null-pointer-checks', '-Isrc/world',
                    '-o', str(executable), 'tools/difftest/worldsearchlifediff.c', str(source_path),
                    '-lntdll'], cwd=root, check=True)
    result = subprocess.run(['wine', str(executable), '32', '0x4a971b63', 'typed'],
                            cwd=root, env=os.environ.copy(), capture_output=True,
                            text=True, timeout=120)
    (output_dir / f'{name}.log').write_text(result.stdout + result.stderr)
    if result.returncode != 1 or 'DIFFER scenario=' not in result.stderr:
        raise SystemExit(f'{name}: no actual difference (exit {result.returncode})\n{result.stderr}')
    print(f'{name}: detected', flush=True)
print(f'World search lifetime: {len(mutations)} / {len(mutations)} mutations detected')
