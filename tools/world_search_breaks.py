#!/usr/bin/env python3
"""Build separate search-core mutants; faults/build failures never count."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parent.parent
output_dir = root / 'bin/world-diff/search-breaks'
output_dir.mkdir(parents=True, exist_ok=True)
source_text = (root / 'src/world/world_search.c').read_text()
mutations = (
    ('title-capacity', '0x38), 0x80);', '0x38), 0x104);'),
    ('path-capacity', '0x24), 0x80);', '0x24), 0x104);'),
    ('checkbox-width', 'U32(object, 0x1c70);', '(U32(object, 0x1c70) != 0);'),
    ('populate-path', 's_services->title(AT(object, 0x16c0),', 's_services->title(AT(object, 0x1304),'),
    ('layer-selection', 'layer / 10 - 1, 0);', 'layer / 10, 0);'),
    ('populate-checkbox', '(int)U32(PTR(object, 0x10dc), 0x54)', '(U32(PTR(object, 0x10dc), 0x54) == 1)'),
    ('drive-stride', '(uint32_t)index * 4)', '0)'),
    ('drive-count', 'index < (int32_t)U32(PTR(object, 0x10dc), 0x68)', 'index + 1 < (int32_t)U32(PTR(object, 0x10dc), 0x68)'),
    ('drive-restore', '0x14e, U32(PTR(object, 0x10dc), 0x6c), 0)', '0x14e, 0, 0)'),
    ('enable-raw-value', '(int)U32(PTR(object, 0x10dc), 0x7c)', '(U32(PTR(object, 0x10dc), 0x7c) != 0)'),
    ('enable-condition', '!= 0 && U32(descriptor, 0x74)', '!= 0 || U32(descriptor, 0x74)'),
    ('enable-cache-decision', 'ui->enable(AT(object, 0x1304), enabled);', 'ui->enable(AT(object, 0x1304), enabled && U32(PTR(object, 0x10dc), 0x74) != 0);'),
    ('validation-priority', 'status = U32(descriptor, 0x20);', 'status = U32(descriptor, 0x34);'),
    ('validation-error-result', '    return 0;\n}', '    return 1;\n}'),
    ('add-callback', 'search_callback(object, 8);', 'search_callback(object, 0xc);'),
    ('add-all-validation', '    fs98_world_search_validate(object);', '    /* omitted validation */'),
    ('next-callback', 'search_callback(object, 0x14);', 'search_callback(object, 0x18);'),
    ('search-post-message', '0x5fa, U32(object, 0x1324)', '0x5fb, U32(object, 0x1324)'),
    ('drive-descriptor-cache', '    int selected = s_services->ui->send', '    void *descriptor = PTR(object, 0x10dc);\n    int selected = s_services->ui->send'),
    ('layer-descriptor-reload', 'if (selected <= 4) U32(descriptor, 0x4c)', 'if (selected <= 4) U32(PTR(object, 0x10dc), 0x4c)'),
    ('layer-range', 'if (selected <= 4)', 'if (selected < 4)'),
    ('cancel-answer', 'U32(PTR(object, 0x10dc), 0) = 0;', 'U32(PTR(object, 0x10dc), 0) = 1;'),
)

for name, before, after in mutations:
    # Add and Add All independently invoke validation: omit both.
    expected = 2 if name == 'add-all-validation' else 1
    if source_text.count(before) != expected:
        raise SystemExit(f'{name}: mutation anchor count changed')
    source_path = output_dir / f'{name}.c'
    executable = output_dir / f'{name}.exe'
    mutated = source_text.replace(before, after)
    if name == 'drive-descriptor-cache':
        mutated = mutated.replace('U32(PTR(object, 0x10dc), 0x6c) = (uint32_t)selected;',
                                  'U32(descriptor, 0x6c) = (uint32_t)selected;')
    if name == 'layer-descriptor-reload':
        mutated = mutated.replace('    void *descriptor = PTR(object, 0x10dc);\n    uint32_t selected', '    uint32_t selected')
    source_path.write_text(mutated)
    subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fno-strict-aliasing', '-Isrc/world', '-o', str(executable),
                    'tools/difftest/worldsearchdiff.c', str(source_path)], cwd=root, check=True)
    result = subprocess.run(['wine', str(executable), '4096', '0x4a971b63', 'normal'],
                            cwd=root, env=os.environ.copy(), capture_output=True,
                            text=True, timeout=120)
    (output_dir / f'{name}.log').write_text(result.stdout + result.stderr)
    if result.returncode != 1 or 'DIFFER scenario=' not in result.stderr:
        raise SystemExit(f'{name}: no actual difference (exit {result.returncode})\n{result.stderr}')
    print(f'{name}: detected', flush=True)
print(f'World search core: {len(mutations)} / {len(mutations)} mutations detected')
