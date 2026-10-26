#!/usr/bin/env python3
"""Require actual retail mismatches for area-list core/init mutations."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parent.parent
output_dir = root / 'bin/world-diff/list-breaks'
output_dir.mkdir(parents=True, exist_ok=True)
source_text = (root / 'src/world/world_list.c').read_text()
mutations = (
    ('string-unwind', 's_services->init->string_destroy(state->string);', '(void)state;'),
    ('title-control', 'init->title(AT(object, 0x1500)', 'init->title(AT(object, 0x1390)'),
    ('missing-reset', '    ui->send(U32(object, 0x1778), 0x184, 0, 0);', '    /* missing reset */'),
    ('saved-selection', 'data.selection = ui->send(U32(object, 0x1778), 0x188', 'data.selection = ui->send(U32(object, 0x1778), 0x18b'),
    ('row-count', 'index < (int32_t)U32(PTR(object, 0x10dc), 0x20)', 'index + 1 < (int32_t)U32(PTR(object, 0x10dc), 0x20)'),
    ('row-stride', 'row_offset += 0x40;', 'row_offset += 0x80;'),
    ('position-format', 'data.text, "%03d", index', 'data.text, "%02d", index'),
    ('layer-capacity', 'data.text, 100);', 'data.text, 99);'),
    ('descriptor-reload', '        descriptor = PTR(object, 0x10dc);\n        row =', '        descriptor = descriptor;\n        row ='),
    ('invalid-layer-scratch', 'data.text, 100);', 'data.text, 100);\n        else data.text[0] = 0;'),
    ('active-normalization', 'U32(row, 0x34) ? "+" : "-"', 'U32(row, 0x34) == 1 ? "+" : "-"'),
    ('missing-add-enable', '        ui->enable(AT(object, 0x1b90), 1);', '        /* missing enable */'),
    ('empty-edit-enable', 'ui->enable(AT(object, 0x189c), 0);', 'ui->enable(AT(object, 0x189c), 1);'),
    ('selection-default', 'data.selection == -1 ? 0 :', 'data.selection == -1 ? 1 :'),
    ('protection-polarity', '), 0x3c) == 0;', '), 0x3c) != 0;'),
    ('protection-reload', 's_services->events->enable(AT(object, 0x189c), enabled);',
     's_services->events->enable(AT(object, 0x189c), !U32((void *)((uintptr_t)PTR(PTR(object, 0x10dc), 0x24) + (U32(PTR(object, 0x10dc), 0x28) << 6)), 0x3c));'),
    ('accept-answer', 'U32(PTR(object, 0x10dc), 0) = 1;', 'U32(PTR(object, 0x10dc), 0) = 0;'),
    ('cancel-answer', 'void fs98_world_list_cancel(void *object)\n{', 'void fs98_world_list_cancel(void *object)\n{\n    U32(PTR(object, 0x10dc), 0) = 0;'),
    ('accept-vtable-cache', '    fs98_world_list_collect(object);\n    U32(PTR(object, 0x10dc), 0) = 1;\n    vtable = PTR(object, 0);',
     '    vtable = PTR(object, 0);\n    fs98_world_list_collect(object);\n    U32(PTR(object, 0x10dc), 0) = 1;'),
    ('action-callback', 'list_callback(object, callback_offset);', 'list_callback(object, 8);'),
    ('delete-collect', '    fs98_world_list_collect(object);\n    list_callback(object, 0x10);', '    list_callback(object, 0x10);'),
    ('double-click-selection', '    s_services->events->send(U32(object, 0x1778), 0x188, 0, 0);', '    fs98_world_list_collect(object);'),
    ('double-click-protection', 'if (!U32((void *)((uintptr_t)PTR(descriptor, 0x24) + offset), 0x3c))', 'if (U32((void *)((uintptr_t)PTR(descriptor, 0x24) + offset), 0x3c))'),
    ('skin-vtable-cache', '    init->base_init(object);\n    vtable = PTR(object, 0);', '    vtable = PTR(object, 0);\n    init->base_init(object);'),
    ('button-module-cache', '    init->art(', '    uint32_t module = fs98_world_get_resource_module();\n    init->art('),
    ('skin-result', '15)) return 0;', '15)) return 1;'),
    ('art-control', 'init->art(AT(object, 0x15b8)', 'init->art(AT(object, 0x1500)'),
    ('button-count', 'index < sizeof(s_buttons) / sizeof(s_buttons[0])', 'index + 1 < sizeof(s_buttons) / sizeof(s_buttons[0])'),
    ('button-coordinate', '(), 1, 1, rect);', '(), 0, 0, rect);'),
    ('list-font', 'list_init(AT(object, 0x1758), AT(object, 0x450)', 'list_init(AT(object, 0x1758), AT(object, 0x220)'),
    ('tabs', 'int tabs[2] = {0x118, 0x154};', 'int tabs[2] = {0x100, 0x154};'),
)

for name, before, after in mutations:
    if source_text.count(before) != (2 if name == 'descriptor-reload' else 1):
        raise SystemExit(f'{name}: mutation anchor is not unique')
    source_path = output_dir / f'{name}.c'
    executable = output_dir / f'{name}.exe'
    mutated = source_text.replace(before, after)
    if name == 'button-module-cache': mutated = mutated.replace('fs98_world_get_resource_module(), 1, 1, rect)', 'module, 1, 1, rect)')
    if name == 'action-callback': mutated = mutated.replace('list_callback(object, 8);', '(void)callback_offset; list_callback(object, 8);')
    source_path.write_text(mutated)
    subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fno-strict-aliasing', '-fno-delete-null-pointer-checks', '-Isrc/world',
                    '-o', str(executable), 'tools/difftest/worldlistdiff.c', str(source_path),
                    'src/world/world_init.c', 'src/world/world_area.c',
                    '-lntdll'], cwd=root, check=True)
    result = subprocess.run(['wine', str(executable), '1024', 'unwind'],
                            cwd=root, env=os.environ.copy(), capture_output=True,
                            text=True, timeout=120)
    (output_dir / f'{name}.log').write_text(result.stdout + result.stderr)
    if result.returncode != 1 or 'LIST DIFFER scenario=' not in result.stderr:
        raise SystemExit(f'{name}: no actual difference (exit {result.returncode})\n{result.stderr}')
    print(f'{name}: detected', flush=True)
print(f'World area-list core/init: {len(mutations)} / {len(mutations)} mutations detected')
