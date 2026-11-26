#!/usr/bin/env python3
"""Require actual retail mismatches for search initialization mutations."""
import sys
sys.dont_write_bytecode = True    # before any local import, so no .pyc is written into the tree
import break_build_only as bo
import module_flags
from pathlib import Path
import os
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
output_dir = root / 'bin/world-diff/search-init-breaks'
mutations = (
    ('skin-bitmap', 'object, 0x14f,', 'object, 0x14e,'),
    ('failed-skin-result', '15)) return 0;', '15)) return 1;'),
    ('skin-vtable-cache', '    ui->base_init(object);\n    vtable = PTR(object, 0);',
     '    vtable = PTR(object, 0);\n    ui->base_init(object);'),
    ('art-control', 'ui->art(AT(object, 0x15d8)', 'ui->art(AT(object, 0x1520)'),
    ('missing-button', 'index < sizeof(s_buttons) / sizeof(s_buttons[0])', 'index + 1 < sizeof(s_buttons) / sizeof(s_buttons[0])'),
    ('button-order', '{0x1d78, 0x1c7c,', '{0x1c7c, 0x1d78,'),
    ('button-coordinates', '(), 1, 1, rect);', '(), 0, 0, rect);'),
    ('button-module-cache', '    ui->art(', '    uint32_t module = fs98_world_get_resource_module();\n    ui->art('),
    ('drive-label-font', 'ui->text(AT(object, 0x1778), AT(object, 0x220))', 'ui->text(AT(object, 0x1778), AT(object, 0x370))'),
    ('layer-combo-label', 'ui->combo(AT(object, 0x1830), AT(object, 0x3e0), AT(object, 0x1468))', 'ui->combo(AT(object, 0x1830), AT(object, 0x3e0), AT(object, 0x1778))'),
    ('drive-selection', 'U32(object, 0x1898), 0x14e, 0, 0', 'U32(object, 0x1898), 0x14e, 1, 0'),
    ('layer-selection', 'U32(object, 0x1850), 0x14e, 0, 0', 'U32(object, 0x1850), 0x14e, 1, 0'),
    ('checkbox-bitmap', 'AT(object, 0x4c0), 0x139,', 'AT(object, 0x4c0), 0x138,'),
    ('group-binding', 's_services->group(AT(object, 0x1198)', 'ui->text(AT(object, 0x1198)'),
    ('edit-font', 'ui->base_font(2)', 'ui->base_font(8)'),
    ('edit-color', 'font, 0xffffff, 0, 0xff0000,', 'font, 0xeeeeee, 0, 0xff0000,'),
    ('title-limit', '0x3c) - 1, 0', '0x3c), 0'),
    ('limit-window', 'U32(object, 0x1324), 0xc5,', 'U32(object, 0x1850), 0xc5,'),
    ('initial-disable', '    events->enable(AT(object, 0x124c), 0);', '    /* missing initial disable */'),
    ('title-descriptor-cache', '    void *font;', '    void *font, *descriptor = PTR(object, 0x10dc);'),
    ('missing-populate', '    fs98_world_search_populate(object);', '    /* missing population */'),
    ('implicit-color-field', '    return 1;', '    U32(object, 0x84) = 12;\n    return 1;'),
    ('implicit-close-pointer', '    return 1;', '    PTR(object, 0x10e0) = AT(object, 0x136c);\n    return 1;'),
    ('successful-result', '    return 1;', '    return 0;'),
)


def search_init_case(name, before, after):
    def overlay():
        source_text = (root / 'src/world/world_search_init.c').read_text()
        if source_text.count(before) != 1:
            raise SystemExit(f'{name}: mutation anchor is not unique')
        mutated = source_text.replace(before, after)
        if name == 'button-module-cache':
            if mutated.count('fs98_world_get_resource_module(), 1, 1, rect)') != 1:
                raise SystemExit(f'{name}: follow-up anchor count changed')
            mutated = mutated.replace('fs98_world_get_resource_module(), 1, 1, rect)', 'module, 1, 1, rect)')
        if name == 'title-descriptor-cache':
            if mutated.count('PTR(PTR(object, 0x10dc), 0x1c)') != 1:
                raise SystemExit(f'{name}: follow-up anchor count changed')
            mutated = mutated.replace('PTR(PTR(object, 0x10dc), 0x1c)', 'PTR(descriptor, 0x1c)')
        return {f'{name}.c': mutated}

    def builds(work):
        return [['i686-w64-mingw32-gcc', *module_flags.FLAGS, '-std=c99', '-Itools/difftest', '-O2', '-Wall', '-Wextra', '-Werror', '-Isrc/world',
                 '-o', str(work / f'{name}.exe'), 'tools/difftest/worldsearchinitdiff.c', str(work / f'{name}.c'),
                 'src/world/world_search.c', 'src/world/world_init.c', 'src/world/world_area.c',
                 '-lntdll']]
    return bo.Case(name, overlay, builds, [f'{name}.exe'])


def main():
    if '--build-only' in sys.argv[1:]:
        return bo.build_only(sys.argv[sys.argv.index('--build-only') + 1:], lambda: ([search_init_case(*m) for m in mutations], None))
    output_dir.mkdir(parents=True, exist_ok=True)
    for name, before, after in mutations:
        case = search_init_case(name, before, after)
        source_path = output_dir / f'{name}.c'
        executable = output_dir / f'{name}.exe'
        source_path.write_text(case.overlay()[f'{name}.c'])
        subprocess.run(case.builds(output_dir)[0], cwd=root, check=True)
        result = subprocess.run(['wine', str(executable), '2048', 'normal'],
                                cwd=root, env=os.environ.copy(), capture_output=True,
                                text=True, timeout=120)
        (output_dir / f'{name}.log').write_text(result.stdout + result.stderr)
        if result.returncode != 1 or 'DIFFER scenario=' not in result.stderr:
            raise SystemExit(f'{name}: no actual difference (exit {result.returncode})\n{result.stderr}')
        print(f'{name}: detected', flush=True)
    print(f'World search initialization: {len(mutations)} / {len(mutations)} mutations detected')


if __name__ == '__main__':
    sys.exit(main())
