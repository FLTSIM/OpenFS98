#!/usr/bin/env python3
"""Require actual retail mismatches for airport core and initialization breaks."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parent.parent
output_dir = root / 'bin/world-diff/airport-breaks'
output_dir.mkdir(parents=True, exist_ok=True)
source_text = (root / 'src/world/world_airport.c').read_text()
mutations = (
    ('allocation-size', 'U32(object, 0x23d4) * 2', 'U32(object, 0x23d4)'),
    ('allocation-resource', '0x8001 + index', '0x8002 + index % 8'),
    ('allocation-maximum', 'length > U32(object, 0x23d4)', 'length < U32(object, 0x23d4)'),
    ('release-count', 'index < 10;', 'index < 9;'),
    ('singular-placeholder', 'if (count == 1)', 'if (count == 2)'),
    ('row-stride', 'index * 12));', 'index * 8));'),
    ('row-signed-count', 'count > 0;', 'count > 1;'),
    ('saved-selection', 'U32(object, snapshot) = U32(descriptor, list + 8);', 'U32(object, snapshot) = 0;'),
    ('runway-callback', 'callback(object, 0x24);', 'callback(object, 0x20);'),
    ('runway-mode', '0x28) == 0x1002) return;', '0x28) == 0x1001) return;'),
    ('populate-callback', '    callback(object, 0x20);\n    mode', '    callback(object, 0x18);\n    mode'),
    ('place-placeholder', 'send(object, s_places[index], 0x143, 0, U32(object, 0x23d0));', 'send(object, s_places[index], 0x143, 0, U32(object, 0x23ac));'),
    ('clear-airport-list', 'send(object, 0x1c60, 0x184, 0, 0);', 'send(object, 0x1c60, 0x14b, 0, 0);'),
    ('mode-callback', '        callback(object, 0xc);', '        callback(object, 0x10);'),
    ('mode-show', 'show(object, 0x1de4, 5);', 'show(object, 0x1de4, 1);'),
    ('mode-button-gate', 'U32(object, 0x23dc) != mode', 'U32(object, 0x23dc) == mode'),
    ('restore-selection', '{0x4c, 0x58, 0x64, 0x70, 0x9c, 0x7c, 0xa8}', '{0x58, 0x4c, 0x64, 0x70, 0x9c, 0x7c, 0xa8}'),
    ('refresh-callback', 'callback(object, 0x10); fs98_world_airport_restore', 'callback(object, 0xc); fs98_world_airport_restore'),
    ('facilities-callback', 'callback(object, 0x14);', 'callback(object, 0x10);'),
    ('area-selection', 'index == 4 ? 0xb4 :', 'index == 4 ? 0xa8 :'),
    ('airport-suppression', 'U32(object, 0x23d8) = 1;', 'U32(object, 0x23d8) = 0;'),
    ('search-equality', 'if (!result) return middle;', 'if (!result) return middle + 1;'),
    ('search-upper', 'if (first < last && compare_text', 'if (first <= last && compare_text'),
    ('edit-capacity', 'text, 100);', 'text, 99);'),
    ('edit-notification-gate', 'if (U32(object, 0x23d8)) return;', 'if (!U32(object, 0x23d8)) return;'),
    ('name-snapshot-quirk', 'U32(PTR(object, 0x10dc), 0x9c) = selection; U32(object, 0x23a0) = selection;', 'U32(PTR(object, 0x10dc), 0x9c) = selection; U32(object, 0x239c) = selection;'),
    ('dropdown-remove', 'send(object, control, 0x144, 1, 0);', 'send(object, control, 0x144, 0, 0);'),
    ('dropdown-first-gate', 'if (index || !selection)', 'if (!index || !selection)'),
    ('accept-owner', 'callback(object, 8); U32', 'callback(object, 4); U32'),
    ('accept-answer', 'U32(PTR(object, 0x10dc), 0) = 1;', 'U32(PTR(object, 0x10dc), 0) = 0;'),
    ('accept-close-vtable', 'fs98_world_airport_release(object); ((void (TC *)(void *))vtable[0xec / 4])(object);', 'fs98_world_airport_release(object); ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);'),
    ('cancel-answer', 'void **vtable; U32(PTR(object, 0x10dc), 0) = 0;', 'void **vtable; U32(PTR(object, 0x10dc), 0) = 1;'),
    ('edit-color', 'font, 0xffffff, 0, 0xff0000,', 'font, 0x00ff00, 0, 0xff0000,'),
    ('edit-limit', 'send(object, offset, 0xc5, 100, 0);', 'send(object, offset, 0xc5, 99, 0);'),
    ('init-art', '0x1198), 0xf831,', '0x1198), 0xf830,'),
    ('init-font', 'text_init(object, 0x10e0, 0x220);', 'text_init(object, 0x10e0, 0x370);'),
    ('init-mode-cache', 'U32(object, 0x23dc) = 3;', 'U32(object, 0x23dc) = 0;'),
    ('init-focus', 'API(0x25590, void (TC *)(void *))(AT(object, 0x1b40));', 'API(0x25590, void (TC *)(void *))(AT(object, 0x1920));'),
)
for name, before, after in mutations:
    if before not in source_text:
        raise SystemExit(f'{name}: missing anchor')
    path, exe = output_dir / f'{name}.c', output_dir / f'{name}.exe'
    path.write_text(source_text.replace(before, after))
    subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fno-strict-aliasing', '-Isrc/world', '-o', str(exe),
                    'tools/difftest/worldairportdiff.c', str(path), '-lntdll'], cwd=root, check=True)
    result = subprocess.run(['wine', str(exe), '64', 'unwind'], cwd=root, env=os.environ.copy(),
                            capture_output=True, text=True, timeout=120)
    (output_dir / f'{name}.log').write_text(result.stdout + result.stderr)
    if result.returncode != 1 or 'AIRPORT DIFFER scenario=' not in result.stderr:
        raise SystemExit(f'{name}: no actual difference (exit {result.returncode})\n{result.stderr}')
    print(f'{name}: detected', flush=True)
print(f'World airport core/init: {len(mutations)} / {len(mutations)} mutations detected')
