#!/usr/bin/env python3
"""Require actual retail mismatches for cache-list core/init mutations."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parent.parent
output_dir = root / 'bin/world-diff/cache-breaks'
output_dir.mkdir(parents=True, exist_ok=True)
source_text = (root / 'src/world/world_cache.c').read_text()
mutations = (
    ('string-unwind', 's_services->init->string_destroy(state->string);', '(void)state;'),
    ('missing-reset', '    ui->send(U32(object, 0x18e4), 0x184, 0, 0);', '    /* missing reset */'),
    ('row-count', 'index < (int32_t)U32(PTR(object, 0x10dc), 0x1c)', 'index + 1 < (int32_t)U32(PTR(object, 0x10dc), 0x1c)'),
    ('row-stride', 'row_offset += 0x80;', 'row_offset += 0;'),
    ('active-equality', 'U32(row, 0x54) == 1', 'U32(row, 0x54) != 0'),
    ('column-marker', '== 1 ? "+" : "-"', '== 1 ? "-" : "+"'),
    ('descriptor-reload', '        row = AT(PTR(PTR(object, 0x10dc), 0x24), row_offset);', '        row = row;'),
    ('empty-branch', 'if (!U32(PTR(object, 0x10dc), 0x1c))', 'if ((int32_t)U32(PTR(object, 0x10dc), 0x1c) <= 0)'),
    ('empty-edit-enable', 'ui->enable(AT(object, 0x1a08), 0);', 'ui->enable(AT(object, 0x1a08), 1);'),
    ('accept-answer', 'U32(PTR(object, 0x10dc), 0) = 1;', 'U32(PTR(object, 0x10dc), 0) = 0;'),
    ('accept-object-result', 'U32(object, 0x1cfc) = 1;', 'U32(object, 0x1cfc) = 0;'),
    ('cancel-object-result', 'U32(object, 0x1cfc) = 2;', 'U32(object, 0x1cfc) = 0;'),
    ('cancel-answer', '    U32(object, 0x1cfc) = 2;', '    U32(object, 0x1cfc) = 2; U32(PTR(object, 0x10dc), 0) = 0;'),
    ('accept-check', 'if (fs98_world_cache_check(object) && fs98_world_cache_validate(object)) return;', 'if (fs98_world_cache_validate(object)) return;'),
    ('accept-validation-short-circuit', 'fs98_world_cache_check(object) &&', 'fs98_world_cache_check(object) ||'),
    ('check-callback', 'return cache_callback(object, 4);', 'return cache_callback(object, 8);'),
    ('action-callback', '        cache_callback(object, callback_offset);', '        cache_callback(object, 8);'),
    ('delete-adjustment', '            --selection;', '            selection = 0;'),
    ('detail-source', 'offset), 0x18));', 'offset), 0x2c));'),
    ('detail-path', 'AT(object, 0x1724)', 'AT(object, 0x1444)'),
    ('detail-selection', 'selection << 7', '0 * selection'),
    ('skin-vtable-cache', '    init->base_init(object);\n    vtable = PTR(object, 0);', '    vtable = PTR(object, 0);\n    init->base_init(object);'),
    ('button-module-cache', '    init->art(', '    uint32_t module = fs98_world_get_resource_module();\n    init->art('),
    ('skin-result', '15)) return 0;', '15)) return 1;'),
    ('art-control', 'init->art(AT(object, 0x17dc)', 'init->art(AT(object, 0x1444)'),
    ('button-count', 'index < sizeof(s_buttons) / sizeof(s_buttons[0])', 'index + 1 < sizeof(s_buttons) / sizeof(s_buttons[0])'),
    ('button-coordinate', '(), 1, 1, rect);', '(), 0, 0, rect);'),
    ('list-font', 'list_init(AT(object, 0x18c4), AT(object, 0x450)', 'list_init(AT(object, 0x18c4), AT(object, 0x220)'),
    ('tabs', 'int tabs[1] = {0xfa};', 'int tabs[1] = {0xfb};'),
)

for name, before, after in mutations:
    if source_text.count(before) != (3 if name == 'detail-path' else 2 if name in ('detail-selection', 'action-callback') else 1):
        raise SystemExit(f'{name}: mutation anchor is not unique')
    source_path = output_dir / f'{name}.c'
    executable = output_dir / f'{name}.exe'
    mutated = source_text.replace(before, after)
    if name == 'button-module-cache': mutated = mutated.replace('fs98_world_get_resource_module(), 1, 1, rect)', 'module, 1, 1, rect)')
    source_path.write_text(mutated)
    subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fno-strict-aliasing', '-fno-delete-null-pointer-checks', '-Isrc/world',
                    '-o', str(executable), 'tools/difftest/worldcachediff.c', str(source_path),
                    'src/world/world_init.c', 'src/world/world_area.c',
                    '-lntdll'], cwd=root, check=True)
    result = subprocess.run(['wine', str(executable), '1024', 'unwind'],
                            cwd=root, env=os.environ.copy(), capture_output=True,
                            text=True, timeout=120)
    (output_dir / f'{name}.log').write_text(result.stdout + result.stderr)
    if result.returncode != 1 or 'CACHE DIFFER scenario=' not in result.stderr:
        raise SystemExit(f'{name}: no actual difference (exit {result.returncode})\n{result.stderr}')
    print(f'{name}: detected', flush=True)
print(f'World cache-list core/init: {len(mutations)} / {len(mutations)} mutations detected')
