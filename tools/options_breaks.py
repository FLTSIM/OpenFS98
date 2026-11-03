#!/usr/bin/env python3
"""Break src/options/ on purpose and check optionseventsdiff reports each break.

    python3 -B tools/options_breaks.py [scenarios] [name...]

Only a reported DIFFER counts as caught: a build failure, a fault or a timeout does not.
Run from a frozen copy of the tree if the sources may change meanwhile, as
tools/flights_breaks.py says for Flights.

Left out as proven equivalent: Video message's OnCancel reading the descriptor or the
vtable after its stores (to +0x13f8 and the descriptor's answer, neither of which is
either, and no call comes between); and OnOK's cut tested with > for >= (at 0x35 characters
the byte it clears is already the terminator).
"""
from pathlib import Path
import subprocess
import sys

UNITS = ('events', 'objects', 'video')
EVENTS = [
    # the shared helpers
    ('disable-window', 'enable_window(D32(object, 0x20), 0)', 'enable_window(D32(object, 0x20), 1)'),
    ('enable-window', 'enable_window(D32(object, 0x20), 1)', 'enable_window(D32(object, 0x20), 0)'),
    ('disable-handle', 'enable_window(D32(object, 0x20), 0)', 'enable_window(D32(object, 0x24), 0)'),
    ('post-destroy-flags', '))(object, 1);\n}', '))(object, 0);\n}'),
    ('post-destroy-slot', 'PTR(PTR(object, 0), 4))(object, 1)', 'PTR(PTR(object, 0), 8))(object, 1)'),
    ('post-destroy-null', '    if (object) ((void *(TC', '    if (object || !object) ((void *(TC'),
    ('no-error-value', '{ (void)object; return 0; }', '{ (void)object; return 1; }'),
    ('check-argument', 'return ((fs98_options_check_fn)PTR(descriptor, 4))(descriptor);',
     'return ((fs98_options_check_fn)PTR(descriptor, 4))(object);'),
    ('check-slot', 'return ((fs98_options_check_fn)PTR(descriptor, 4))(descriptor);',
     'return ((fs98_options_check_fn)PTR(descriptor, 8))(descriptor);'),
    ('check-value', 'return ((fs98_options_check_fn)PTR(descriptor, 4))(descriptor);',
     'return !((fs98_options_check_fn)PTR(descriptor, 4))(descriptor);'),
    # help and the data exchange
    ('help-command', 's_help_file, 0xc,', 's_help_file, 0xd,'),
    ('help-window', 'win_help(D32(info, 0xc)', 'win_help(D32(info, 0x8)'),
    ('help-table', 's_help_file, 0xc, s_help_##name);', 's_help_file, 0xc, s_help_##name + 2);'),
    ('context-command', 's_help_file, 0xa,', 's_help_file, 0xb,'),
    ('context-return', '    return 1; \\\n}\n/* DoDataExchange', '    return 0; \\\n}\n/* DoDataExchange'),
    ('ddx-control-id', 's_ddx_##name[index][1],', 's_ddx_##name[index][1] ^ 1,'),
    ('ddx-offset', 'AT(object, s_ddx_##name[index][0])', 'AT(object, s_ddx_##name[index][0] + 4)'),
    ('ddx-count', '(int32_t)index < (int32_t)s_ddx_count_##name;', '(int32_t)index < (int32_t)s_ddx_count_##name - 1;'),
]
OBJECTS = [
    ('constructor-template', 'dialog_construct(object, state.class_info->template, parent)',
     'dialog_construct(object, state.class_info->template + 1, parent)'),
    ('constructor-parent', 'dialog_construct(object, state.class_info->template, parent)',
     'dialog_construct(object, state.class_info->template, 0)'),
    ('constructor-member-import', 's_services->construct((uint8_t *)object + control->offset, control->constructor);',
     's_services->construct((uint8_t *)object + control->offset, control->destructor);'),
    ('constructor-member-count', 'while (state.constructed < state.class_info->count) {',
     'while (state.constructed + 1 < state.class_info->count) {'),
    ('destructor-member-import', 's_services->destroy((uint8_t *)state->object + control->offset, control->destructor);',
     's_services->destroy((uint8_t *)state->object + control->offset, control->constructor);'),
    ('destructor-base', '        else s_services->dialog_destroy(state->object);', '        else (void)0;'),
    ('delete-flag', '    if (flags & 1) s_services->ui_delete(object);', '    if (flags & 2) s_services->ui_delete(object);'),
    ('delete-return', '    return object; \\\n} \\\nuint32_t', '    return 0; \\\n} \\\nuint32_t'),
    ('factory-size', 'allocate(class_info->size)', 'allocate(class_info->size + 4)'),
    ('factory-descriptor-always', '    if (class_info->descriptor)\n        *(void', '    if (1)\n        *(void'),
    ('factory-descriptor-value', '+ class_info->descriptor) = descriptor;', '+ class_info->descriptor) = 0;'),
    ('factory-module', 'D32(object, 0x80) = s_module;', 'D32(object, 0x84) = s_module;'),
    ('factory-create-template', 's_services->create(object, class_info->template, 0);',
     's_services->create(object, class_info->template + 1, 0);'),
    ('factory-show', 's_services->show(object, 1);', 's_services->show(object, 5);'),
    ('factory-order', '    s_services->create(object, class_info->template, 0);\n    s_services->show(object, 1);',
     '    s_services->show(object, 1);\n    s_services->create(object, class_info->template, 0);'),
    ('factory-return', 'return D32(object, 0x20);', 'return D32(object, 0x1c);'),
]
ACCEPT_BODY = '''    D32(object, 0x13f8) = 1;
    SERVICES->get_window_text(AT(object, 0x10e0), buffer, MESSAGE_ROOM);
    for (length = 0; buffer[length]; ++length) {}
    if (length >= 0x35) buffer[0x35] = 0;
    for (length = 0; buffer[length]; ++length) {}
    if (SERVICES->is_lead_byte(length ? (signed char)buffer[length - 1] : 0) && length)
        buffer[length - 1] = 0;
    message = (char *)PTR(object, 0x10dc) + 8;
'''
# the descriptor read before the calls that may swap it, not after
ACCEPT_BODY_EARLY = '''    message = (char *)PTR(object, 0x10dc) + 8;
''' + ACCEPT_BODY.replace('''    message = (char *)PTR(object, 0x10dc) + 8;
''', '')
VIDEO = [
    # the shared pieces of OnInitDialog
    ('skin-resource', '        object, 0x14f, module, 0xf);', '        object, 0x150, module, 0xf);'),
    ('skin-flags', '        object, 0x14f, module, 0xf);', '        object, 0x14f, module, 0xe);'),
    ('skin-slot', 'PTR(PTR(object, 0), 0xe4))(', 'PTR(PTR(object, 0), 0xe8))('),
    ('button-bitmap', 'AT(object, 0x610), 0x132,', 'AT(object, 0x610), 0x133,'),
    ('button-fonts', 'SERVICES->init_button(AT(object, at), AT(object, 0x5a0), AT(object, 0x610),',
     'SERVICES->init_button(AT(object, at), AT(object, 0x610), AT(object, 0x5a0),'),
    ('button-position', 'fs98_options_resource_module(), 1, 1, empty);', 'fs98_options_resource_module(), 0, 0, empty);'),
    ('button-color', '1, 0xffffffff,\n                          fs98_options_resource_module()', '1, 0xfffffffe,\n                          fs98_options_resource_module()'),
    ('button-rect', 'struct fs98_options_rect empty = {0, 0, 0, 0};', 'struct fs98_options_rect empty = {0, 0, 1, 0};'),
    # Video finished
    ('finished-base-init', '    SERVICES->base_init(object);\n    if (!skin(object, fs98_options_resource_module())) return 0;\n    button(object, 0x1194);',
     '    if (!skin(object, fs98_options_resource_module())) return 0;\n    button(object, 0x1194);'),
    ('finished-skin-fails', '    if (!skin(object, fs98_options_resource_module())) return 0;\n    button(object, 0x1194);',
     '    skin(object, fs98_options_resource_module());\n    button(object, 0x1194);'),
    ('finished-button', 'button(object, 0x1194);', 'button(object, 0x1190);'),
    ('finished-text', 'init_text(AT(object, 0x10dc), AT(object, 0x220));', 'init_text(AT(object, 0x10dc), AT(object, 0x290));'),
    ('finished-order', '    button(object, 0x1194);\n    SERVICES->init_text(AT(object, 0x10dc), AT(object, 0x220));',
     '    SERVICES->init_text(AT(object, 0x10dc), AT(object, 0x220));\n    button(object, 0x1194);'),
    ('finished-return', '    SERVICES->init_text(AT(object, 0x10dc), AT(object, 0x220));\n    return 1;',
     '    SERVICES->init_text(AT(object, 0x10dc), AT(object, 0x220));\n    return 0;'),
    ('finished-accept-slot', '((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);\n}\n\n/* ---- Video message',
     '((void (TC *)(void *))PTR(PTR(object, 0), 0xe8))(object);\n}\n\n/* ---- Video message'),
    # Video message: populate and OnInitDialog
    ('populate-null', '    if (PTR(object, 0x10dc) == 0) return 0;', '    if (PTR(object, 0x10dc) == (void *)1) return 0;'),
    ('populate-text', 'set_window_text(AT(object, 0x10e0), fs98_options_empty_text);', 'set_window_text(AT(object, 0x10e0), " ");'),
    ('populate-return', '    SERVICES->set_window_text(AT(object, 0x10e0), fs98_options_empty_text);\n    return 1;',
     '    SERVICES->set_window_text(AT(object, 0x10e0), fs98_options_empty_text);\n    return 0;'),
    ('message-skin-fails', '    if (!skin(object, fs98_options_resource_module())) return 0;\n    button(object, 0x12fc);',
     '    if (!skin(object, fs98_options_resource_module())) return 1;\n    button(object, 0x12fc);'),
    ('message-button-order', '    button(object, 0x12fc);\n    button(object, 0x1200);', '    button(object, 0x1200);\n    button(object, 0x12fc);'),
    ('message-label', 'init_text(AT(object, 0x1148), AT(object, 0x220));', 'init_text(AT(object, 0x1148), AT(object, 0x370));'),
    ('message-edit-font', 'SERVICES->base_font(2), 0xffffff, 0, 0xff0000,', 'SERVICES->base_font(1), 0xffffff, 0, 0xff0000,'),
    ('message-edit-colors', 'SERVICES->base_font(2), 0xffffff, 0, 0xff0000,', 'SERVICES->base_font(2), 0xffffff, 0, 0xff00,'),
    ('message-edit-label', '                        AT(object, 0x1148), 0);', '                        AT(object, 0x1200), 0);'),
    ('message-limit', 'EM_LIMITTEXT, MESSAGE_ROOM, 0);', 'EM_LIMITTEXT, MESSAGE_ROOM + 1, 0);'),
    ('message-limit-window', 'send_message(D32(object, 0x1100), EM_LIMITTEXT', 'send_message(D32(object, 0x10e0), EM_LIMITTEXT'),
    ('message-focus', 'if (fs98_options_video_message_populate(object)) SERVICES->set_focus(AT(object, 0x10e0));',
     'fs98_options_video_message_populate(object); SERVICES->set_focus(AT(object, 0x10e0));'),
    ('message-init-return', '    if (fs98_options_video_message_populate(object)) SERVICES->set_focus(AT(object, 0x10e0));\n    return 0;',
     '    if (fs98_options_video_message_populate(object)) SERVICES->set_focus(AT(object, 0x10e0));\n    return 1;'),
    # Video message: OnOK
    ('accept-state', '    D32(object, 0x13f8) = 1;', '    D32(object, 0x13f8) = 2;'),
    ('accept-room', 'buffer, MESSAGE_ROOM);', 'buffer, MESSAGE_ROOM - 1);'),
    ('accept-cut', '    if (length >= 0x35) buffer[0x35] = 0;', '    if (length > 0x36) buffer[0x35] = 0;'),
    ('accept-cut-at', '    if (length >= 0x35) buffer[0x35] = 0;', '    if (length >= 0x35) buffer[0x34] = 0;'),
    ('accept-lead-byte', 'if (SERVICES->is_lead_byte(length ? (signed char)buffer[length - 1] : 0) && length)',
     'if (SERVICES->is_lead_byte(length ? (unsigned char)buffer[length - 1] : 0) && length)'),
    ('accept-lead-byte-kept', '        buffer[length - 1] = 0;\n    message', '        (void)0;\n    message'),
    ('accept-message-at', 'message = (char *)PTR(object, 0x10dc) + 8;', 'message = (char *)PTR(object, 0x10dc) + 9;'),
    ('accept-descriptor-early', ACCEPT_BODY, ACCEPT_BODY_EARLY),
    ('accept-check-skipped', 'if (fs98_options_check(object) == 0 || fs98_options_no_error(object) == 0) {',
     'if (fs98_options_no_error(object) == 0) {'),
    ('accept-check-fails-closed', 'if (fs98_options_check(object) == 0 || fs98_options_no_error(object) == 0) {',
     'if (fs98_options_check(object) == 0) {'),
    ('accept-answer', '        D32(PTR(object, 0x10dc), 0) = 1;', '        D32(PTR(object, 0x10dc), 0) = 2;'),
    ('accept-close-slot', '        ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);\n    }\n}',
     '        ((void (TC *)(void *))PTR(PTR(object, 0), 0xe8))(object);\n    }\n}'),
    # Video message: OnCancel
    ('cancel-state', '    D32(object, 0x13f8) = 2;', '    D32(object, 0x13f8) = 1;'),
    ('cancel-answer', '    descriptor[0] = 0;', '    descriptor[0] = 1;'),
    ('cancel-slot', '((void (TC *)(void *))PTR(vtable, 0xec))(object);', '((void (TC *)(void *))PTR(vtable, 0xe8))(object);'),
]
UNWIND = [
    ('constructor-vtable-early', 'objects', '    state.base = 1;\n    while', '    state.base = 1;\n    PTR(object, 0) = s_vtables[kind];\n    while'),
    ('constructor-unwind-base', 'objects', '    state.base = 1;', '    state.base = 0;'),
    ('cleanup-order', 'objects', '&state->class_info->controls[--state->constructed];',
     '&state->class_info->controls[state->class_info->count - state->constructed--];'),
    ('factory-unwind-allocator', 'objects', 'dialog_delete(state->object)', 'ui_delete(state->object)'),
    ('factory-unwind-live', 'objects', '    state.live = 0;\n    D32(object, 0x80)', '    D32(object, 0x80)'),
]


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    only = set(sys.argv[2:])
    if count < 100:
        raise ValueError('use at least 100 scenarios')
    directory = Path('bin/options-diff/breaks')
    directory.mkdir(parents=True, exist_ok=True)
    breaks = [(name, 'events', 'normal', before, after) for name, before, after in EVENTS]
    breaks += [(name, 'objects', 'normal', before, after) for name, before, after in OBJECTS]
    breaks += [(name, 'video', 'normal', before, after) for name, before, after in VIDEO]
    breaks += [(name, unit, 'unwind', before, after) for name, unit, before, after in UNWIND]
    if only:
        breaks = [b for b in breaks if b[0] in only]
    caught = 0
    for name, unit, mode, before, after in breaks:
        original = Path(f'src/options/options_{unit}.c').read_text()
        if original.count(before) != 1:
            raise ValueError(f'{name}: the anchor occurs {original.count(before)} times')
        source = directory / f'{name}.c'
        executable = directory / f'{name}.exe'
        source.write_text(original.replace(before, after, 1))
        executable.unlink(missing_ok=True)
        others = [f'src/options/options_{u}.c' for u in UNITS if u != unit]
        build = subprocess.run([
            'i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Wno-unused',
            '-fno-strict-aliasing', '-fno-delete-null-pointer-checks', '-Isrc/options', '-o', str(executable),
            'tools/difftest/optionseventsdiff.c', str(source), *others, '-lntdll'],
            text=True, capture_output=True)
        if build.returncode:
            print(f'{name}: BUILD FAILED (not counted)\n{build.stderr}', flush=True)
            continue
        scenarios = str(count if mode == 'normal' else max(100, count // 10))
        try:
            run = subprocess.run(['wine', str(executable), scenarios, '0x5f1a6c2d', mode],
                                 text=True, errors='replace', capture_output=True, timeout=300)
        except subprocess.TimeoutExpired:
            print(f'{name}: TIMEOUT (not counted)', flush=True)
            continue
        output = run.stdout + run.stderr
        (directory / f'{name}.log').write_text(output)
        if run.returncode == 1 and 'DIFFER scenario=' in output:
            caught += 1
            print(f'{name}: caught', flush=True)
        else:
            print(f'{name}: NOT CAUGHT (exit {run.returncode})\n{output[-600:]}', flush=True)
    print(f'Options breaks: {caught}/{len(breaks)} caught', flush=True)
    return 0 if caught == len(breaks) else 1


if __name__ == '__main__':
    sys.exit(main())
