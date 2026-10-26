#!/usr/bin/env python3
"""Require actual differences for area lifetime, factory and unwind mutations."""
from pathlib import Path
import os
import subprocess

def main():
    root = Path(__file__).resolve().parent.parent
    output_dir = root / 'bin/world-diff/life-breaks'
    output_dir.mkdir(parents=True, exist_ok=True)
    source_text = (root / 'src/world/world_objects.c').read_text()
    mutations = (
        ('base-template', 'base_construct(object, 0x6b3, parent)',
         'base_construct(object, 0x6b4, parent)', 'normal'),
        ('constructor-state', 'WORLD_U32(object, 0x10e0) = 1;',
         'WORLD_U32(object, 0x10e0) = 0;', 'normal'),
        ('missing-last-constructor', 'state.constructed < sizeof(s_area_controls) / sizeof(s_area_controls[0])',
         'state.constructed + 1 < sizeof(s_area_controls) / sizeof(s_area_controls[0])', 'normal'),
        ('missing-base-destructor', 's_object_services->destroy(state->object, WORLD_AREA_BASE_DTOR);',
         'if (0) s_object_services->destroy(state->object, WORLD_AREA_BASE_DTOR);', 'normal'),
        ('delete-flag', 'if (flags & 1)', 'if (flags & 2)', 'normal'),
        ('skip-unwind-control', 'world_destroy_remaining((struct world_object_unwind *)frame);',
         '((struct world_object_unwind *)frame)->constructed = 0; world_destroy_remaining((struct world_object_unwind *)frame);', 'unwind'),
        ('wrong-unwind-delete', 's_object_services->dialog_delete(state->object);',
         's_object_services->ui_delete(state->object);', 'unwind'),
        ('lost-allocation-ownership', 'state.object = object; state.live = 1;',
         'state.object = object; state.live = 0;', 'unwind'),
        ('late-ownership-release', 'state.live = 0;\n    if (!s_object_services->create',
         'state.live = 1;\n    if (!s_object_services->create', 'unwind'),
        ('factory-parent', 'create(object, 0x6b3, NULL)', 'create(object, 0x6b3, (void *)parent)', 'normal'),
        ('create-failure-message', 'if (!s_object_services->create', 'if (s_object_services->create', 'normal'),
        ('show-command', 'show(object, 1)', 'show(object, 0)', 'normal'),
        ('missing-ddx', 'index < sizeof(s_area_ddx) / sizeof(s_area_ddx[0])',
         'index + 1 < sizeof(s_area_ddx) / sizeof(s_area_ddx[0])', 'normal'),
        ('help-command', '"fs98.hlp", 12, s_area_help', '"fs98.hlp", 10, s_area_help', 'normal'),
        ('context-result', '    return 1;\n}', '    return 0;\n}', 'normal'),
        ('typed-catch-cleanup', 'world_factory_cleanup(&state->frame);',
         'state->live = 0; world_factory_cleanup(&state->frame);', 'typed'),
        ('typed-catch-dispatch', 's_object_services->exit_cleanly(exception);',
         's_object_services->exit_cleanly((uint8_t *)exception + 4);', 'typed'),
    )
    header_mutations = (
        ('derived-pointer-adjustment', 'exception_bytes += (int)type[2];',
         'exception_bytes += 0;', 'typed', 'world_exceptions.h'),
        ('virtual-pointer-adjustment', 'type[3] + *(int *)(virtual_base_table + type[4])',
         '*(int *)(virtual_base_table + type[4])', 'typed', 'world_exceptions.h'),
        ('typed-catch-type', '".PAVCException@@"', '".PBVCException@@"', 'typed', 'world_exceptions.h'),
    )
    all_mutations = [(*mutation, 'world_objects.c') for mutation in mutations] + list(header_mutations)
    for name, before, after, mode, filename in all_mutations:
        original_text = (root / 'src/world' / filename).read_text()
        if original_text.count(before) != 1:
            raise SystemExit(f'{name}: mutation anchor is not unique')
        case_dir = output_dir / name
        case_dir.mkdir(exist_ok=True)
        source_path = case_dir / 'world_objects.c'
        executable = case_dir / 'mutant.exe'
        changed_text = original_text.replace(before, after)
        source_path.write_text(changed_text if filename == 'world_objects.c' else source_text)
        if filename != 'world_objects.c':
            (case_dir / filename).write_text(changed_text)
        subprocess.run(['i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-fno-strict-aliasing', '-fno-delete-null-pointer-checks', '-Isrc/world',
                        '-o', str(executable), 'tools/difftest/worldlifediff.c', str(source_path), '-lntdll'],
                       cwd=root, check=True)
        run_result = subprocess.run(['wine', str(executable), '32', '0x4a971b63', mode],
                                 cwd=root, env=os.environ.copy(), capture_output=True,
                                 text=True, timeout=120)
        (output_dir / f'{name}.log').write_text(run_result.stdout + run_result.stderr)
        if run_result.returncode != 1 or 'DIFFER scenario=' not in run_result.stderr:
            raise SystemExit(f'{name}: no actual difference (exit {run_result.returncode})\n{run_result.stderr}')
        print(f'{name}: detected', flush=True)
    print(f'World area lifetime: {len(all_mutations)} / {len(all_mutations)} mutations detected')


if __name__ == '__main__':
    main()
