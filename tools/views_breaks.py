#!/usr/bin/env python3
"""Require actual retail differences for isolated Views logic/cleanup errors."""
from pathlib import Path
import subprocess
import sys


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    if count < 100:
        raise ValueError('use at least 100 scenarios')
    directory = Path('bin/views-diff/breaks')
    directory.mkdir(parents=True, exist_ok=True)
    mutations = [
        ('record-stride', 'events', 'normal', '* (uint32_t)0xb4;', '* (uint32_t)0;'),
        ('direction-control', 'events', 'normal', 'fs98_views_combo(object, 0x1f54, 0x74)', 'fs98_views_combo(object, 0x2404, 0x74)'),
        ('transition-field', 'events', 'normal', 'fs98_views_combo(object, 0x2404, 0xa4)', 'fs98_views_combo(object, 0x2404, 0xac)'),
        ('altitude-field', 'events', 'normal', 'fs98_views_combo(object, 0x25cc, 0xac)', 'fs98_views_combo(object, 0x25cc, 0x74)'),
        ('axis-field', 'events', 'normal', '0x20) = selection;', '0x24) = selection;'),
        ('fullscreen-field', 'events', 'normal', 'checked = D32(object, 0x1d50)', 'checked = D32(object, 0x24e8)'),
        ('gradual-field', 'events', 'normal', 'checked = D32(object, 0x24e8)', 'checked = D32(object, 0x1d50)'),
        ('titles-field', 'events', 'normal', '0x24) = D32(object, 0x27b0)', '0x20) = D32(object, 0x27b0)'),
        ('help-command', 'events', 'normal', '"fs98.hlp", 0xc,', '"fs98.hlp", 0xd,'),
        ('preview-update', 'events', 'normal', 'preview(3, descriptor, 0)', 'preview(1, descriptor, 0)'),
        ('preview-close', 'events', 'normal', 'preview(2, descriptor, 0)', 'preview(3, descriptor, 0)'),
        ('close-order', 'events', 'normal', 'preview(2, descriptor, 0);\n    s_services->close(object);', 's_services->close(object);\n    preview(2, descriptor, 0);'),
        ('sync-field', 'events', 'normal', 'case 0: offset = 0x10;', 'case 0: offset = 0x24;'),
        ('skip-validation-callback', 'events', 'normal', '    validate(descriptor);', '    (void)validate;'),
        ('stale-validation-selection', 'events', 'normal', 'validate(descriptor);\n    record = fs98_views_record(object);', 'record = fs98_views_record(object);\n    validate(descriptor);'),
        ('error-display-gate', 'events', 'normal', 'if (display) {', 'if (display || 1) {'),
        ('error-return', 'events', 'normal', 'return ((error_fn *)PTR(object, 0))[0xe0 / 4](object, control);', '((error_fn *)PTR(object, 0))[0xe0 / 4](object, control);\n    return (int32_t)control;'),
        ('mode-map-flag', 'events', 'normal', 'D32(record, 4) = 0;', 'D32(record, 4) = 1;'),
        ('spot-preview-gate', 'events', 'normal', 'enable_control(preview, mode == 4)', 'enable_control(preview, mode == 3)'),
        ('missing-text-clear', 'events', 'normal', 's_services->set_text((uint8_t *)object + 0x1e14, "");', '(void)object;'),
        ('radio-numbering', 'events', 'normal', 'radio = mode == 3 ? 4 : mode == 4 ? 3 : mode;', 'radio = mode;'),
        ('map-mode-gate', 'events', 'normal', 'mode = D32(record, 4) == 1 ? 5', 'mode = D32(record, 4) != 0 ? 5'),
        ('cancel-answer', 'events', 'normal', 'D32(PTR(object, 0x2804), 0) = 0;', 'D32(PTR(object, 0x2804), 0) = 1;'),
        ('accept-vtable', 'events', 'normal', 'fs98_views_sync(object);\n        ((close_fn)vtable[0xec / 4])(object);', 'fs98_views_sync(object);\n        ((close_fn)((void **)PTR(object, 0))[0xec / 4])(object);'),
        ('post-destroy-flag', 'events', 'normal', '[1](object, 1)', '[1](object, 0)'),
        ('ddx-id', 'events', 'normal', 's_exchange_controls[index][1],', 's_exchange_controls[index][1] ^ 1,'),
        ('map-choice-count', 'events', 'normal', 'metric ? 12 : 15', 'metric ? 11 : 14'),
        ('map-tail-label', 'events', 'normal', 'string_append(state.strings[3], state.strings[2]);', 'string_append(state.strings[3], state.strings[1]);'),
        ('zoom-offset-sign', 'events', 'normal', '(long double)parsed.value - s_zoom_offset', '(long double)parsed.value + s_zoom_offset'),
        ('zoom-stack-leftover', 'events', 'normal', 'parsed.bits = legacy_bits;', 'parsed.bits = legacy_bits & 0;'),
        ('map-stack-leftover', 'events', 'normal', 'metric = (int32_t)legacy_bits;', 'metric = (int32_t)(legacy_bits & 0);'),
        ('zoom-mask', 'events', 'normal', 'position & 0xffff', 'position & 0xfffe'),
        ('spin-signed-position', 'events', 'normal', '(long double)(uint64_t)position', '(long double)(int64_t)(int32_t)position'),
        ('spin-fraction', 'events', 'normal', '(uint32_t)dot + 2), 2)', '(uint32_t)dot + 2), 1)'),
        ('init-flags', 'events', 'normal', '0x14f, s_resource_module, 0xf', '0x14f, s_resource_module, 0xe'),
        ('preview-create', 'events', 'normal', 'callback(1, PTR(object, 0x2804), window)', 'callback(3, PTR(object, 0x2804), window)'),
        ('preview-callback-capture', 'events', 'normal', 'callback(1, PTR(object, 0x2804), window);', 'callback = PTR(PTR(object, 0x2804), 8);\n    callback(1, PTR(object, 0x2804), window);'),
        ('constructor-parent', 'objects', 'normal', 'dialog_construct(object, 0x5e1, parent)', 'dialog_construct(object, 0x5e1, (void *)((uint32_t)parent & 0))'),
        ('destructor-order', 'objects', 'normal', '&s_subobjects[--state->constructed]', '&s_subobjects[SUBOBJECT_COUNT - 1 - --state->constructed]'),
        ('deleting-flag', 'objects', 'normal', 'if (flags & 1)', 'if (flags & 2)'),
        ('spin-allocator', 'objects', 'normal', 'if (flags & 1) s_services->mfc_delete(object)', 'if (flags & 1) s_services->ui_delete(object)'),
        ('factory-size', 'objects', 'normal', 'allocate(0x2808)', 'allocate(0x2804)'),
        ('factory-template', 'objects', 'normal', 'create(object, 0x5e1, 0)', 'create(object, 0x5e2, 0)'),
        ('factory-module', 'objects', 'normal', '0x80) = s_module', '0x80) = s_module ^ 1'),
        ('constructor-unwind-base', 'objects', 'unwind', '    state.base = 1;', '    state.base = 0;'),
        ('factory-unwind-delete', 'objects', 'unwind', 's_services->dialog_delete(state->object)', 's_services->ui_delete(state->object)'),
        ('temporary-unwind', 'events', 'unwind', 'if (state->temporary)', 'if (0)'),
    ]
    caught = 0
    for name, unit, mode, before, after in mutations:
        source_text = Path(f'src/views/views_{unit}.c').read_text()
        if before not in source_text:
            raise ValueError(f'{name}: mutation anchor missing')
        source = directory / f'{name}.c'
        executable = directory / f'{name}.exe'
        source.write_text(source_text.replace(before, after, 1))
        sources = [str(source) if other == unit else f'src/views/views_{other}.c' for other in ('events', 'objects')]
        build = subprocess.run([
            'i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
            '-fno-strict-aliasing', '-fno-delete-null-pointer-checks', '-Isrc/views',
            '-o', str(executable), 'tools/difftest/viewseventsdiff.c', *sources, 'src/views/views_stack.S', 'tools/difftest/viewsstack.S', '-lntdll'],
            text=True, capture_output=True)
        if build.returncode:
            print(f'{name}: BUILD FAILED\n{build.stderr}', flush=True)
            continue
        try:
            run = subprocess.run(['wine', str(executable), str(count), '0x7e51a23b', mode],
                                 text=True, capture_output=True, timeout=60)
        except subprocess.TimeoutExpired:
            print(f'{name}: TIMEOUT (not counted)', flush=True)
            continue
        output = run.stdout + run.stderr
        (directory / f'{name}.log').write_text(output)
        if run.returncode == 1 and 'DIFFER scenario=' in output:
            caught += 1
            print(f'{name}: caught', flush=True)
        else:
            print(f'{name}: NOT CAUGHT (exit {run.returncode})\n{output}', flush=True)
    print(f'Views mutations: {caught}/{len(mutations)} caught')
    return 0 if caught == len(mutations) else 1


if __name__ == '__main__':
    sys.exit(main())
