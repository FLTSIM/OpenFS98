#!/usr/bin/env python3
"""Require actual retail mismatches for world-files core/init mutations."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parent.parent
output_dir = root / 'bin/world-diff/files-breaks'
output_dir.mkdir(parents=True, exist_ok=True)
source_text = (root / 'src/world/world_files.c').read_text()
mutations = (
    ('append-reset', '    for (index = 0; index < count; ++index)', '    ui->send(U32(object, 0x154c), 0x184, 0, 0);\n    for (index = 0; index < count; ++index)'),
    ('cached-count', 'index < count;', 'index < (int32_t)U32(PTR(object, 0x10dc), 0x10);'),
    ('row-index', 'files_row(object, (uint32_t)index)', 'files_row(object, 0)'),
    ('row-title', '0x180, 0, U32(files_row(object, (uint32_t)index), 0x1c)', '0x180, 0, U32(files_row(object, (uint32_t)index), 0x30)'),
    ('population-selection', '0x186, selected, 0', '0x186, 0, 0'),
    ('detail-selection', 'PTR(files_row(object, selected), 0x30)', 'PTR(files_row(object, 0), 0x30)'),
    ('enable-raw-flag', 'U32(files_row(object, selected), 0));', '!!U32(files_row(object, selected), 0));'),
    ('enable-reload', '    selected = U32(PTR(object, 0x10dc), 0x18);', '    /* retain old index */'),
    ('selection-order', '    s_services->init->title(AT(object, 0x1574), PTR(files_row(object, selected), 0x30));\n    U32(PTR(object, 0x10dc), 0x18) = selected;', '    U32(PTR(object, 0x10dc), 0x18) = selected;\n    s_services->init->title(AT(object, 0x1574), PTR(files_row(object, selected), 0x30));'),
    ('action-refresh', 'PTR(descriptor, callback_offset))(descriptor);', 'PTR(descriptor, callback_offset))(descriptor); fs98_world_files_populate(object);'),
    ('action-choice', 'PTR(descriptor, callback_offset)', 'PTR(descriptor, 8)'),
    ('accept-answer', 'U32(PTR(object, 0x10dc), 0) = 1;', 'U32(PTR(object, 0x10dc), 0) = 0;'),
    ('accept-result', 'U32(object, 0x1824) = 1;', 'U32(object, 0x1824) = 0;'),
    ('cancel-result', 'U32(object, 0x1824) = 2;', 'U32(object, 0x1824) = 0;'),
    ('cancel-answer', '    U32(object, 0x1824) = 2;', '    U32(object, 0x1824) = 2; U32(PTR(object, 0x10dc), 0) = 0;'),
    ('accept-check', 'if (fs98_world_files_check(object) && fs98_world_files_validate(object)) return;', 'if (fs98_world_files_validate(object)) return;'),
    ('check-return', 'return ((uint32_t (SC *)(void *))PTR(descriptor, 4))(descriptor);', '((uint32_t (SC *)(void *))PTR(descriptor, 4))(descriptor); return 0;'),
    ('erase-message', '0x154c), 7, 0, 0', '0x154c), 8, 0, 0'),
    ('erase-return', 'return s_services->erase(object, dc);', 's_services->erase(object, dc); return 0;'),
    ('skin-vtable-cache', '    init->base_init(object);\n    vtable = PTR(object, 0);', '    vtable = PTR(object, 0);\n    init->base_init(object);'),
    ('button-module-cache', '    init->art(', '    uint32_t module = fs98_world_get_resource_module();\n    init->art('),
    ('skin-result', '15)) return 0;', '15)) return 1;'),
    ('art-control', 'init->art(AT(object, 0x1390)', 'init->art(AT(object, 0x12d8)'),
    ('button-count', 'index < sizeof(s_buttons) / sizeof(s_buttons[0])', 'index + 1 < sizeof(s_buttons) / sizeof(s_buttons[0])'),
    ('button-coordinate', '(), 1, 1, rect);', '(), 0, 0, rect);'),
    ('list-font', 'list_init(AT(object, 0x152c), AT(object, 0x450)', 'list_init(AT(object, 0x152c), AT(object, 0x220)'),
)

for name, before, after in mutations:
    if source_text.count(before) != (2 if name in ('enable-raw-flag', 'detail-selection') else 1):
        raise SystemExit(f'{name}: mutation anchor is not unique')
    source_path = output_dir / f'{name}.c'
    executable = output_dir / f'{name}.exe'
    mutated = source_text.replace(before, after)
    if name == 'button-module-cache': mutated = mutated.replace('fs98_world_get_resource_module(), 1, 1, rect)', 'module, 1, 1, rect)')
    if name == 'action-choice': mutated = mutated.replace('    void *descriptor = PTR(object, 0x10dc);\n    ((uint32_t', '    (void)callback_offset; void *descriptor = PTR(object, 0x10dc);\n    ((uint32_t')
    if name == 'cached-count': mutated = mutated.replace('    for (index =', '    (void)count; for (index =', 1)
    source_path.write_text(mutated)
    subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fno-strict-aliasing', '-fno-delete-null-pointer-checks', '-Isrc/world',
                    '-o', str(executable), 'tools/difftest/worldfilesdiff.c', str(source_path),
                    'src/world/world_init.c', 'src/world/world_area.c',
                    '-lntdll'], cwd=root, check=True)
    result = subprocess.run(['wine', str(executable), '1024', 'unwind'],
                            cwd=root, env=os.environ.copy(), capture_output=True,
                            text=True, timeout=120)
    (output_dir / f'{name}.log').write_text(result.stdout + result.stderr)
    if result.returncode != 1 or 'FILES DIFFER scenario=' not in result.stderr:
        raise SystemExit(f'{name}: no actual difference (exit {result.returncode})\n{result.stderr}')
    print(f'{name}: detected', flush=True)
print(f'World world-files core/init: {len(mutations)} / {len(mutations)} mutations detected')
