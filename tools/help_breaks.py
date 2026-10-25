#!/usr/bin/env python3
"""An actual reported difference is required; faults/timeouts do not count."""
from pathlib import Path
import subprocess
import sys


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    if count < 100:
        raise ValueError('use at least 100 scenarios')
    directory = Path('bin/help-diff/breaks')
    directory.mkdir(parents=True, exist_ok=True)
    mutations = [
        ('about-answer', '0x10dc, 0x1978, 0)', '0x10dc, 0x1978, 1)'),
        ('credits-answer', '0x1174, 0x1498, 1)', '0x1174, 0x1498, 0)'),
        ('lessons-state', '0x10dc, 0x187c, 0)', '0x10dc, 0x187c, 1)'),
        ('completion-answer', '0x10dc, 0, 1)', '0x10dc, 0, 0)'),
        ('help-command', '"fs98.hlp", 0xc,', '"fs98.hlp", 0xd,'),
        ('ddx-control-id', 's_ddx_##name[index][1],', 's_ddx_##name[index][1] ^ 1,'),
        ('ddx-base', 'if (base)', 'if (0)'),
        ('post-destroy-flags', '))(object, 1);', '))(object, 0);'),
        ('disable-window', 'enable_window(D32(object, 0x20), 0)', 'enable_window(D32(object, 0x20), 1)'),
        ('close-timer-order', '    D32(object, 0x10e8) = 0;', '    D32(object, 0x10e8) = 1;'),
        ('close-rich-edit', 'if (handle) s_services->destroy_window(handle);', '(void)handle;'),
        ('close-resource', '        D32(object, 0x110c) = 0;', '        D32(object, 0x110c) = 1;'),
        ('stream-read-failure', '*read = 0;', '*read = 1;'),
        ('stream-return', '    return 0;\n}\n\n/* 0x22332010', '    return 1;\n}\n\n/* 0x22332010'),
        ('stream-descriptor-order', '    PTR(object, 0x1174) = AT(object, 0x1110);\n    s_services->copy_text(name, \"fltcred.rtf\");', '    s_services->copy_text(name, \"fltcred.rtf\");\n    PTR(object, 0x1174) = AT(object, 0x1110);'),
        ('stream-limit', '0x435, 0, 0x7d00)', '0x435, 0, 0x7d01)'),
        ('stream-format', '0x449, 2,', '0x449, 1,'),
        ('stream-close-file', '    s_services->close_handle((uint32_t)file);', '    (void)file;'),
        ('stream-scroll-limit', ' - 6;', ' - 5;'),
        ('timer-first-id', 'timer = 3;', 'timer = 4;'),
        ('timer-last-id', 'timer < 15;', 'timer < 14;'),
        ('timer-period', 'timer, 500, 0)', 'timer, 501, 0)'),
        ('timer-signed-bound', 'line > (int32_t)D32', '(uint32_t)line > D32'),
        ('timer-equal-bound', 'line > (int32_t)D32', 'line >= (int32_t)D32'),
        ('timer-reset-command', '0x115, 6, 0)', '0x115, 5, 0)'),
        ('toggle-bit', 'D32(object, 0x1108) ^= 1;', 'D32(object, 0x1108) ^= 2;'),
        ('lesson-invalid-selection', '0x188, 0, 0) == -1)', '0x188, 0, 0) == -2)'),
        ('lesson-radio-value', 'window == (uint32_t)checked;', 'window != (uint32_t)checked;'),
        ('lesson-radio-handle-reload', 'window = D32(object, 0x17a8);', 'window = D32(object, 0x17a8) ^ 1;'),
        ('lesson-record-stride', 'index * (uint32_t)0xa08', 'index * (uint32_t)0'),
        ('lesson-description', '(char *)AT(record, 0x100)', '(char *)AT(record, 0)'),
        ('lesson-extension', 'exists(path, 0x1d)', 'exists(path, 0x1e)'),
        ('lesson-availability', 'exists(path, 0x1d) == 0', 'exists(path, 0x1d) != 0'),
        ('lesson-double-click-order', '    fs98_help_lessons_select(object);\n    ((void (TC *)(void *))PTR(PTR(object, 0), 0xcc))(object);', '    ((void (TC *)(void *))PTR(PTR(object, 0), 0xcc))(object);\n    fs98_help_lessons_select(object);'),
        ('graph-tick-operation', 'fs98_help_graph_call(object, 1);', 'fs98_help_graph_call(object, 0);'),
        ('graph-accept-operation', 'fs98_help_graph_call(object, 2);', 'fs98_help_graph_call(object, 1);'),
        ('graph-notify-operation', 'fs98_help_graph_call(object, 0);', 'fs98_help_graph_call(object, 1);'),
        ('graph-state-write', 'D32(object, 0x10e4) = operation;', 'D32(object, 0x10e4) = operation ^ 1;'),
        ('graph-debug-pointer', '&& (uint32_t)descriptor != 0xcdcdcdcd', '&& (uint32_t)descriptor != 0xcccccccc'),
    ]
    mutations += [
        ('skin-flags', 's_resource_module, 9)', 's_resource_module, 8)'),
        ('skin-resource', 'object, 0x14f,', 'object, 0x14e,'),
        ('button-bitmap', '0x132, AT(object, 0xff4)', '0x133, AT(object, 0xff4)'),
        ('completion-font', 'init_text(AT(object, 0x10e0), AT(object, 0x220))', 'init_text(AT(object, 0x10e0), AT(object, 0x370))'),
        ('lessons-font-color', 'D32(object, 0x228) = 0;', 'D32(object, 0x228) = 1;'),
        ('lessons-edit-limit', '0xc5, 0x3ff, 0)', '0xc5, 0x3fe, 0)'),
        ('lessons-radio-bitmap', 'radio, AT(object, 0x4c0), 0x134,', 'radio, AT(object, 0x4c0), 0x135,'),
        ('lessons-edit-error', 'font, 0xffffff, 0, 0xff0000,', 'font, 0xffffff, 0, 0xfe0000,'),
        ('credits-window-style', '"RICHEDIT", "", 0x503000c4,', '"RICHEDIT", "", 0x503000c5,'),
        ('credits-window-id', 'D32(object, 0x20), 0xede,', 'D32(object, 0x20), 0xedf,'),
    ]
    mutations += [
        ('registry-name-capacity', '        *capacity = 0x63;', '        *capacity = 0x62;'),
        ('registry-product-capacity', '*capacity = 0x31;', '*capacity = 0x30;'),
        ('registry-key-access', '\\6.1", 0, 0x20019, key_out)', '\\6.1", 0, 0x20018, key_out)'),
        ('registry-name-pointer', '(char *)scratch + 0xa8', '(char *)scratch + 0xac'),
        ('registry-fallback', '"Registry Error"', '"Registry failure"'),
        ('version-minor', '6, 10, build, 0x1a2', '6, 11, build, 0x1a2'),
        ('version-build', '6, 10, build, 0x1a2', '6, 10, build, 0x1a3'),
        ('about-font-selector', 'base_font(11)', 'base_font(10)'),
        ('about-font-color', 'D32(s_about_state->font, 8) = 0xffffff;', 'D32(s_about_state->font, 8) = 0xfffffe;'),
        ('about-font-background', 'D32(s_about_state->font, 0xc) = 0;', 'D32(s_about_state->font, 0xc) = 1;'),
        ('about-font-init-bit', 's_about_state->initialized |= 1;', 's_about_state->initialized |= 2;'),
    ]
    mutations = [(name, 'events', 'normal', before, after)
                  for name, before, after in mutations]
    mutations += [
        ('constructor-parent', 'objects', 'normal', 'class_info->template, parent)', 'class_info->template, (void *)((uint32_t)parent & 0))'),
        ('credits-initial-pause', 'objects', 'normal', 'D32(object, 0x1108) = 1;', 'D32(object, 0x1108) = 0;'),
        ('delete-flags', 'objects', 'normal', 'if (flags & 1) s_services->ui_delete', 'if (flags & 2) s_services->ui_delete'),
        ('placeholder-delete-allocator', 'objects', 'normal', 'if (flags & 1) s_services->mfc_delete', 'if (flags & 1) s_services->ui_delete'),
        ('factory-module', 'objects', 'normal', 'D32(object, 0x80) = s_module;', 'D32(object, 0x80) = s_module ^ 1;'),
        ('factory-show', 'objects', 'normal', 'show(object, 1)', 'show(object, 0)'),
        ('factory-size', 'objects', 'normal', 'allocate(class_info->size)', 'allocate(class_info->size - 4)'),
        ('factory-parent', 'objects', 'normal', 'fs98_help_construct(object, 0, kind)', 'fs98_help_construct(object, (void *)parent, kind)'),
        ('constructor-unwind-base', 'objects', 'unwind', '    state.base = 1;', '    state.base = 0;'),
        ('factory-unwind-allocator', 'objects', 'unwind', 'dialog_delete(state->object)', 'ui_delete(state->object)'),
    ]
    mutations += [
        ('about-string-unwind', 'events', 'unwind', 'if (state->live) {', 'if (0) {'),
    ]
    caught = 0
    for name, unit, mode, before, after in mutations:
        original = Path(f'src/help/help_{unit}.c').read_text()
        if before not in original:
            raise ValueError(f'{name}: missing mutation anchor')
        source = directory / f'{name}.c'
        executable = directory / f'{name}.exe'
        source.write_text(original.replace(before, after, 1))
        executable.unlink(missing_ok=True)
        build = subprocess.run([
            'i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
            '-fno-strict-aliasing', '-Isrc/help', '-o', str(executable),
            'tools/difftest/helpeventsdiff.c', str(source),
            f'src/help/help_{"objects" if unit == "events" else "events"}.c', 'src/help/help_stack.S', '-lntdll'], text=True, capture_output=True)
        if build.returncode:
            print(f'{name}: BUILD FAILED (not counted)\n{build.stderr}', flush=True)
            continue
        try:
            run = subprocess.run(['wine', str(executable), str(count), '0x19a847e3', mode],
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
    print(f'Help mutations: {caught}/{len(mutations)} caught', flush=True)
    return 0 if caught == len(mutations) else 1


if __name__ == '__main__':
    sys.exit(main())
