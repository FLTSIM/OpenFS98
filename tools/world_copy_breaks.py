#!/usr/bin/env python3
"""Require actual differences for copy-weather core and initializer mutations."""
from pathlib import Path
import subprocess


def main():
    output = Path('bin/world-diff/copy-breaks')
    output.mkdir(parents=True, exist_ok=True)
    mutations = [
        ('collect-first', 'core', '0x1c) = (uint32_t)selection;', '0x1c) = selection != 0;'),
        ('collect-second', 'core', '0x20) = (uint32_t)selection;', '0x20) = selection != 0;'),
        ('collect-check', 'core', '= U32(object, 0x180c);', '= U32(object, 0x18d4);'),
        ('collect-message', 'core', 'int selection = s_services->send(U32(object, 0x14b0), 0x147,', 'int selection = s_services->send(U32(object, 0x14b0), 0x14e,'),
        ('populate-check-order', 'core', '{0x28, 0x38, 0x50, 0x40, 0x30, 0x48}', '{0x38, 0x28, 0x50, 0x40, 0x30, 0x48}'),
        ('populate-check-raw', 'core', '(int)U32(PTR(object, 0x10dc), s_fields[index])', 'U32(PTR(object, 0x10dc), s_fields[index]) != 0'),
        ('populate-reset', 'core', 's_services->send(U32(object, 0x1468), 0x14b, 0, 0);', '(void)object;'),
        ('row-reload', 'core', 'row += 0xc8;', 'row = (uint8_t *)PTR(PTR(object, 0x10dc), 0x10) + (uint32_t)index * 0xc8;'),
        ('row-stride', 'core', 'row += 0xc8;', 'row += 0xc4;'),
        ('row-count-cache', 'core', 'int index;', 'int index; int count = (int32_t)U32(PTR(object, 0x10dc), 0xc);'),
        ('row-pointer-cache', 'core', '    uint8_t *row;', '    uint8_t *row; void *descriptor = PTR(object, 0x10dc);'),
        ('row-data', 'core', '0x151, (uint32_t)added, (uint32_t)row);', '0x151, (uint32_t)added, (uint32_t)(row + 4));'),
        ('row-selection', 'core', '0x1c), 0);', '0x20), 0);'),
        ('disabled-order', 'core', '{0x18e0, 0x1818, 0x19a8, 0x1750, 0x1818}', '{0x1818, 0x18e0, 0x19a8, 0x1750, 0x1818}'),
        ('disabled-second', 'core', 'if (U32(PTR(object, 0x10dc), 0x18))', 'if (0)'),
        ('descriptor-poison', 'core', ' && U32(object, 0x10dc) != 0xcdcdcdcd', ''),
        ('check-result', 'core', 'first == second ? 0x1012 : 0', 'first != second ? 0x1012 : 0'),
        ('error-code', 'core', '0x1448), 0x1012);', '0x1448), 0x1013);'),
        ('error-result', 'core', '    return 1;', '    return 0;'),
        ('cancel-mode', 'core', '0x1a70) = 2;', '0x1a70) = 1;'),
        ('cancel-answer', 'core', 'U32(descriptor, 0) = 0;', 'U32(descriptor, 0) = 1;'),
        ('accept-mode', 'core', '0x1a70) = 1;', '0x1a70) = 2;'),
        ('accept-answer', 'core', 'U32(descriptor, 0) = 1;', 'U32(descriptor, 0) = 0;'),
        ('accept-collect', 'core', '    fs98_world_copy_collect(object);', '    (void)object;'),
        ('accept-veto', 'core', 'if (!fs98_world_copy_check(object))', 'if (1)'),
        ('error-post-target', 'core', '0x5fa, U32(object, 0x1468)', '0x5fa, U32(object, 0x14b0)'),
        ('base-init', 'init', '    ui->base_init(object);', '    (void)object;'),
        ('skin-bitmap', 'init', 'object, 0x14f,', 'object, 0x14e,'),
        ('skin-flags', 'init', 's_services->resources(), 15)', 's_services->resources(), 14)'),
        ('skin-failure', 'init', 'if (!((int (TC *', 'if (0 && !((int (TC *'),
        ('art-bitmap', 'init', '0x14d8), 0xf831,', '0x14d8), 0xf830,'),
        ('art-flags', 'init', 'AT(object, 0x7e8), 2);', 'AT(object, 0x7e8), 1);'),
        ('button-font', 'init', '0x10e0), AT(object, 0x5a0)', '0x10e0), AT(object, 0x610)'),
        ('button-art', 'init', '0x11dc), AT(object, 0x5a0), AT(object, 0x610), 0x132,', '0x11dc), AT(object, 0x5a0), AT(object, 0x610), 0x133,'),
        ('check-bitmap', 'init', '0x4c0), 0x139,', '0x4c0), 0x138,'),
        ('check-order', 'init', '{0x15c0, 0x1688, 0x1750, 0x1818, 0x18e0, 0x19a8}', '{0x1688, 0x15c0, 0x1750, 0x1818, 0x18e0, 0x19a8}'),
        ('check-omission', 'init', 'index < 6;', 'index < 5;'),
        ('heading-font', 'init', '0x1390), AT(object, 0x370)', '0x1390), AT(object, 0x3e0)'),
        ('combo-font', 'init', '0x1490), AT(object, 0x3e0)', '0x1490), AT(object, 0x370)'),
        ('combo-label', 'init', '0x1490), AT(object, 0x3e0), AT(object, 0x1390)', '0x1490), AT(object, 0x3e0), AT(object, 0x12d8)'),
        ('populate-omission', 'init', '    fs98_world_copy_populate(object);', '    (void)object;'),
        ('init-result', 'init', '    return 1;', '    return 0;'),
    ]
    for name, unit, before, after in mutations:
        original = Path('src/world/world_copy' + ('_init' if unit == 'init' else '') + '.c')
        source = original.read_text()
        # Both list item-data stores must be changed together for this mutation.
        expected = 2 if name == 'row-data' else 1
        if source.count(before) != expected:
            raise ValueError(f'{name}: anchor count changed')
        path = output / f'{name}.c'
        mutated = source.replace(before, after)
        if name == 'row-count-cache':
            mutated = mutated.replace('(int32_t)U32(PTR(object, 0x10dc), 0xc)', 'count')
            mutated = mutated.replace('int count = count;', 'int count = (int32_t)U32(PTR(object, 0x10dc), 0xc);')
        if name == 'row-pointer-cache':
            mutated = mutated.replace('row = PTR(PTR(object, 0x10dc), 0x10);', 'row = PTR(descriptor, 0x10);')
        path.write_text(mutated)
        sources = [str(path) if filename == original else str(filename) for filename in
                   (Path('src/world/world_copy.c'), Path('src/world/world_copy_init.c'))]
        exe = output / f'{name}.exe'
        subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-fno-strict-aliasing', '-Isrc/world', '-o', str(exe),
                        'tools/difftest/worldcopydiff.c', *sources], check=True)
        result = subprocess.run(['wine', str(exe), '256', '0x4a971b63', 'interrupt'],
                                capture_output=True, text=True, timeout=120)
        (output / f'{name}.log').write_text(result.stdout + result.stderr)
        if result.returncode != 1 or 'DIFFER scenario=' not in result.stderr:
            raise RuntimeError(f'{name}: no actual difference (exit {result.returncode})\n{result.stdout}{result.stderr}')
        print(f'{name}: detected', flush=True)
    print(f'World copy core/initializer: {len(mutations)}/{len(mutations)} detected', flush=True)


if __name__ == '__main__':
    main()
