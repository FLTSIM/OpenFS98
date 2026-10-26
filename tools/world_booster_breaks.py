#!/usr/bin/env python3
"""Build separate booster-core mutants; faults/build failures never count."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parent.parent
output_dir = root / 'bin/world-diff/booster-breaks'
output_dir.mkdir(parents=True, exist_ok=True)
source_text = (root / 'src/world/world_booster.c').read_text()
mutations = (
    ('collect-cache-capacity', 'PTR(booster_row(object), 0x18), 0x104);',
     'PTR(booster_row(object), 0x18), 0x80);'),
    ('collect-source-capacity', 'PTR(booster_row(object), 4), 0x104);',
     'PTR(booster_row(object), 4), 0x80);'),
    ('layer-capacity', 'text, 0x1f);', 'text, 0x20);'),
    ('combo-normalization', 'value == 1;', 'value != 0;'),
    ('active-normalization', 'U32(object, 0x1d64) != 0;', 'U32(object, 0x1d64) == 1;'),
    ('original-word-width', 'ui->format_int(original, "%5d", *(uint16_t *)AT(booster_row(object), 0x74));',
     'ui->format_int(original, "%5d", U32(booster_row(object), 0x74));'),
    ('original-format', 'ui->format_int(original, "%5d",', 'ui->format_int(original, "%d",'),
    ('populate-active', 'U32(booster_row(object), 0x54) == 1);',
     'U32(booster_row(object), 0x54) != 0);'),
    ('source-priority', 'status = U32(row, 0);', 'status = U32(row, 0x14);'),
    ('combo-error-control', 'control = 0x1c60;', 'control = 0x1b28;'),
    ('zero-error-accept', 'if (control && ui->error(object, AT(object, control), (void *)status)) {',
     'if (control) { ui->error(object, AT(object, control), (void *)status);'),
    ('accept-answer', 'U32(descriptor, 0) = 1;', 'U32(descriptor, 0) = 0;'),
    ('accept-layer-record', 'U32(row, 0x60) = U32(row, 0x58);',
     'U32(row, 0x60) = 0;'),
    ('path-capacity', 'PTR(booster_row(object), 4), 0x80);',
     'PTR(booster_row(object), 4), 0x104);'),
    ('missing-enable', 'sizeof(s_controls) / sizeof(s_controls[0])',
     'sizeof(s_controls) / sizeof(s_controls[0]) - 1'),
    ('reload-enable-decision', 'AT(object, s_controls[index]), enabled);',
     'AT(object, s_controls[index]), enabled && U32(booster_row(object), 0) == 0);'),
    ('validate-status', 'return U32(booster_row(object), 0x58) ? 0x1ae9 : 0;',
     'return U32(booster_row(object), 0x14) ? 0x1ae9 : 0;'),
    ('error-return', 'return ((int (TC *)(void *, uint32_t))vtable[0xe0 / 4])(object, control);',
     '((int (TC *)(void *, uint32_t))vtable[0xe0 / 4])(object, control); return control;'),
)
for name, before, after in mutations:
    # The record store appears in accept and virtual error: change both.
    expected = 2 if name == 'accept-layer-record' else 1
    if source_text.count(before) != expected:
        raise SystemExit(f'{name}: mutation anchor count changed')
    source_path = output_dir / f'{name}.c'
    executable = output_dir / f'{name}.exe'
    source_path.write_text(source_text.replace(before, after))
    subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fno-strict-aliasing', '-Isrc/world', '-o', str(executable),
                    'tools/difftest/worldboosterdiff.c', str(source_path)], cwd=root, check=True)
    result = subprocess.run(['wine', str(executable), '4096', '0x4a971b63', 'normal'],
                            cwd=root, env=os.environ.copy(), capture_output=True,
                            text=True, timeout=120)
    (output_dir / f'{name}.log').write_text(result.stdout + result.stderr)
    if result.returncode != 1 or 'DIFFER scenario=' not in result.stderr:
        raise SystemExit(f'{name}: no actual difference (exit {result.returncode})\n{result.stderr}')
    print(f'{name}: detected', flush=True)
print(f'World booster core: {len(mutations)} / {len(mutations)} mutations detected')
