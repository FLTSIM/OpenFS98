#!/usr/bin/env python3
"""Break src/options/ on purpose and check optionseventsdiff reports each break.

    python3 -B tools/options_breaks.py [scenarios] [name...]

Only a reported DIFFER counts as caught: a build failure, a fault or a timeout does not.
Run from a frozen copy of the tree if the sources may change meanwhile, as
tools/flights_breaks.py says for Flights.

Left out as proven equivalent (and the like in Save video and Flight video, whose
descriptor and vtable reads cannot be moved past a call): Video message's OnCancel reading the descriptor or the
vtable after its stores (to +0x13f8 and the descriptor's answer, neither of which is
either, and no call comes between); and OnOK's cut tested with > for >= (at 0x35 characters
the byte it clears is already the terminator); and Flight video's validate leaving its name
buffer unzeroed past the first byte (it is read only as a string, and GetWindowText always
terminates what it writes); and Logbook entry's OnInitDialog ignoring its part test (the
part is MAIN's block + 0x54, never null); and the Logbook reading MAIN's block once for
all of a row's fields rather than for each (no call comes between the reads); its rows
font's guard set with 3 for 2 (bit 1 is always set by then); and its first measure of the
rows list left out (OPT-6: overwritten before anything reads it).
"""
from pathlib import Path
import subprocess
import sys

UNITS = ('events', 'objects', 'video', 'videos', 'logbook', 'book')
# the two exchange macros, each whole: their loops are alike
PLAIN = '''    uint32_t index; \\
    for (index = 0; (int32_t)index < (int32_t)s_ddx_count_##name; ++index) \\
        s_services->ddx_control(exchange, s_ddx_##name[index][1], \\
                                AT(object, s_ddx_##name[index][0])); \\
}
OPTIONS_CLASS(video_finished)'''
BASE = '''    s_services->base_exchange(object, exchange); \\
    for (index = 0; (int32_t)index < (int32_t)s_ddx_count_##name; ++index) \\
        s_services->ddx_control(exchange, s_ddx_##name[index][1], \\
                                AT(object, s_ddx_##name[index][0])); \\'''
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
    ('ddx-control-id', PLAIN, PLAIN.replace('s_ddx_##name[index][1],', 's_ddx_##name[index][1] ^ 1,')),
    ('ddx-offset', PLAIN, PLAIN.replace('AT(object, s_ddx_##name[index][0])', 'AT(object, s_ddx_##name[index][0] + 4)')),
    ('ddx-count', PLAIN, PLAIN.replace('(int32_t)s_ddx_count_##name;', '(int32_t)s_ddx_count_##name - 1;')),
    ('base-ddx-control-id', BASE, BASE.replace('s_ddx_##name[index][1],', 's_ddx_##name[index][1] ^ 1,')),
    ('base-ddx-count', BASE, BASE.replace('(int32_t)s_ddx_count_##name;', '(int32_t)s_ddx_count_##name - 1;')),
    ('base-ddx-skipped', BASE, BASE.replace('    s_services->base_exchange(object, exchange); \\\n', '')),
    ('base-ddx-last', BASE, BASE.replace('    s_services->base_exchange(object, exchange); \\\n', '').replace(
        'AT(object, s_ddx_##name[index][0])); \\', 'AT(object, s_ddx_##name[index][0])); \\\n    s_services->base_exchange(object, exchange); \\')),
    # the logbook exchanges: controls, then each CString and its limit
    ('texts-id', 's_services->ddx_text(exchange, texts[index][1],', 's_services->ddx_text(exchange, texts[index][1] ^ 1,'),
    ('texts-limit', '(int32_t)texts[index][2]);', '(int32_t)texts[index][2] + 1);'),
    ('texts-order', '        s_services->ddx_text(exchange, texts[index][1], AT(object, texts[index][0]));\n        s_services->ddv_max_chars(exchange, AT(object, texts[index][0]), (int32_t)texts[index][2]);',
     '        s_services->ddv_max_chars(exchange, AT(object, texts[index][0]), (int32_t)texts[index][2]);\n        s_services->ddx_text(exchange, texts[index][1], AT(object, texts[index][0]));'),
    ('texts-controls-first', '    for (index = 0; index < count; ++index)\n        s_services->ddx_control(exchange, controls[index][1], AT(object, controls[index][0]));\n    for (index = 0; index < text_count; ++index) {',
     '    for (index = 0; index < text_count; ++index) {'),
    ('add-logbook-name-limit', '{{0x1400, 0x11a4, 0xf0}}', '{{0x1400, 0x11a4, 0xef}}'),
    ('entry-date-limit', '{0x1ca0, 0x1199, 8}', '{0x1ca0, 0x1199, 6}'),
    ('entry-remarks-limit', '{0x1c9c, 0x1198, 0x18}', '{0x1c9c, 0x1198, 0x19}'),
    ('entry-times-order', '{0x1c90, 0x1194, 6}, {0x1c94, 0x1195, 6}', '{0x1c94, 0x1195, 6}, {0x1c90, 0x1194, 6}'),
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
    ('entry-constructor-strings', 'for (at = 0x1c90; at <= 0x1ca0; at += 4)', 'for (at = 0x1c90; at < 0x1ca0; at += 4)'),
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
VIDEOS = [
    # the CNotify
    ('notify-delete-flag', '    if (flags & 1) SERVICES->ui_delete(notify);', '    if (flags & 2) SERVICES->ui_delete(notify);'),
    ('notify-delete-destroy', '    SERVICES->notify_destroy(notify);\n    if (flags', '    if (flags'),
    ('notify-delete-return', '    if (flags & 1) SERVICES->ui_delete(notify);\n    return notify;', '    if (flags & 1) SERVICES->ui_delete(notify);\n    return 0;'),
    ('notify-size', 'SERVICES->allocate(NOTIFY_SIZE);', 'SERVICES->allocate(NOTIFY_SIZE + 4);'),
    ('notify-arguments', 'notify_construct(notify, 0x30, 1);', 'notify_construct(notify, 0x30, 0);'),
    ('notify-vtable', '        PTR(notify, 0) = s_notify_vtable;\n', ''),
    ('notify-close-flags', '    if (notify) ((void *(TC *)(void *, uint32_t))PTR(PTR(notify, 0), 4))(notify, 1);\n    SERVICES->close(object);',
     '    if (notify) ((void *(TC *)(void *, uint32_t))PTR(PTR(notify, 0), 4))(notify, 0);\n    SERVICES->close(object);'),
    ('notify-close-null', '    if (notify) ((void *(TC *)(void *, uint32_t))PTR(PTR(notify, 0), 4))(notify, 1);\n    SERVICES->close(object);',
     '    if (notify && 0) ((void *(TC *)(void *, uint32_t))PTR(PTR(notify, 0), 4))(notify, 1);\n    SERVICES->close(object);'),
    ('notify-close-base', '    SERVICES->close(object);\n}\n\n/* ---- Save video', '}\n\n/* ---- Save video'),
    # Save video's choices
    ('review-word', '    D32(SAVE_DESCRIPTOR(object), 0x10) = 1;\n    D32(SAVE_DESCRIPTOR(object), 0x14) = 0;\n    VCALL(object, 0xec);\n}\nvoid TC fs98_options_save_video_resume',
     '    D32(SAVE_DESCRIPTOR(object), 0x10) = 2;\n    D32(SAVE_DESCRIPTOR(object), 0x14) = 0;\n    VCALL(object, 0xec);\n}\nvoid TC fs98_options_save_video_resume'),
    ('resume-word', '    D32(SAVE_DESCRIPTOR(object), 0xc) = 1;', '    D32(SAVE_DESCRIPTOR(object), 0x10) = 1;'),
    ('resume-close', '    D32(SAVE_DESCRIPTOR(object), 0x14) = 0;\n    VCALL(object, 0xec);\n}\n/* 0x2239d840',
     '    D32(SAVE_DESCRIPTOR(object), 0x14) = 0;\n}\n/* 0x2239d840'),
    ('save-held', 'if (failure != 0 && ((uint32_t (TC *)(void *, uint32_t))vtable[0xdc / 4])(object, failure) != 0)\n        return;\n    D32(SAVE_DESCRIPTOR(object), 8) = 1;',
     'if (failure != 0)\n        return;\n    D32(SAVE_DESCRIPTOR(object), 8) = 1;'),
    ('save-word', '    D32(SAVE_DESCRIPTOR(object), 8) = 1;', '    D32(SAVE_DESCRIPTOR(object), 8) = 2;'),
    ('save-answer', '    D32(SAVE_DESCRIPTOR(object), 0) = 1;\n    ((void (TC *)(void *))vtable[0xec / 4])(object);\n}\n/* 0x2239d8c0',
     '    D32(SAVE_DESCRIPTOR(object), 0) = 2;\n    ((void (TC *)(void *))vtable[0xec / 4])(object);\n}\n/* 0x2239d8c0'),
    ('save-vtable-late', '    ((void (TC *)(void *))vtable[0xec / 4])(object);\n}\n/* 0x2239d8c0', '    VCALL(object, 0xec);\n}\n/* 0x2239d8c0'),
    ('cancel-style', '    D32(object, 0x94) = 1;', '    D32(object, 0x94) = 2;'),
    ('cancel-question', 'AT(object, 0x1440), 0x1011) == 0) return;', 'AT(object, 0x1440), 0x1012) == 0) return;'),
    ('cancel-answer-ignored', 'AT(object, 0x1440), 0x1011) == 0) return;', 'AT(object, 0x1440), 0x1011) == 0) {}'),
    ('cancel-discard', '    descriptor[5] = 1;', '    descriptor[4] = 1;'),
    # Save video's OnInitDialog, fill and the descriptor test
    ('save-init-skin', '    if (!skin(object)) return 0;\n    SERVICES->init_art(AT(object, 0x1358)', '    skin(object);\n    SERVICES->init_art(AT(object, 0x1358)'),
    ('save-init-art', 'init_art(AT(object, 0x1358), 0xf831,', 'init_art(AT(object, 0x1358), 0xf832,'),
    ('save-init-button-order', '    button(object, 0x1804, 0x5a0, 0x132);\n    button(object, 0x1708, 0x5a0, 0x132);',
     '    button(object, 0x1708, 0x5a0, 0x132);\n    button(object, 0x1804, 0x5a0, 0x132);'),
    ('save-init-review-art', '    button(object, 0x1510, 0x680, 0x137);', '    button(object, 0x1510, 0x5a0, 0x137);'),
    ('save-init-description-room', 'D32(SAVE_DESCRIPTOR(object), 0x34) - 1, 0);', 'D32(SAVE_DESCRIPTOR(object), 0x34), 0);'),
    ('save-init-name-room', 'D32(SAVE_DESCRIPTOR(object), 0x20) - 1, 0);', 'D32(SAVE_DESCRIPTOR(object), 0x24) - 1, 0);'),
    ('save-init-label', '    edit(object, 0x1440, 0x11e8);', '    edit(object, 0x1440, 0x12a0);'),
    ('save-init-notify', '    PTR(object, 0x10dc) = fs98_options_new_notify();\n    if (!fs98_options_save_video_has_descriptor', '    fs98_options_new_notify();\n    if (!fs98_options_save_video_has_descriptor'),
    ('save-init-return', '    fs98_options_save_video_fill(object);\n    return 1;\n}', '    fs98_options_save_video_fill(object);\n    return 0;\n}'),
    ('save-fill-text', 'set_window_text(AT(object, 0x14a8), PTR(SAVE_DESCRIPTOR(object), 0x30));', 'set_window_text(AT(object, 0x14a8), PTR(SAVE_DESCRIPTOR(object), 0x1c));'),
    ('save-has-descriptor', '{ return SAVE_DESCRIPTOR(object) != 0; }', '{ return 1; }'),
    # Save video's validate and error
    ('save-validate-room', 'PTR(descriptor, 0x30), (int32_t)D32(descriptor, 0x34));', 'PTR(descriptor, 0x30), (int32_t)D32(descriptor, 0x34) - 1);'),
    ('save-validate-reload', '    descriptor = SAVE_DESCRIPTOR(object);\n    SERVICES->get_window_text(AT(object, 0x1440)', '    SERVICES->get_window_text(AT(object, 0x1440)'),
    ('save-validate-store', '    D32(object, 0x10e0) = result;\n    if (result) return D32(SAVE', '    if (result) return D32(SAVE'),
    ('save-validate-ids', 'D32(SAVE_DESCRIPTOR(object), 0x18) ? 0x11a3 : 0x11a0;', 'D32(SAVE_DESCRIPTOR(object), 0x18) ? 0x11a0 : 0x11a3;'),
    ('save-validate-fill', '    fs98_options_save_video_fill(object);\n    return 0;\n}\n/* 0x2239dc80', '    return 0;\n}\n/* 0x2239dc80'),
    ('error-zero', '    if (code == 0) return 0;\n    if (SERVICES->window_from_handle', '    if (SERVICES->window_from_handle'),
    ('error-review-focus', '== AT(object, 0x1510)) return 0;', '== AT(object, 0x1514)) return 0;'),
    ('error-resume-focus', '== AT(object, 0x160c)) return 0;', '== AT(object, 0x1610)) return 0;'),
    ('error-description', 'AT(object, 0x14a8), D32(SAVE_DESCRIPTOR(object), 0x2c));', 'AT(object, 0x14a8), D32(SAVE_DESCRIPTOR(object), 0x18));'),
    ('error-status', '    if (status != 0x1004) {\n        SERVICES->error_dialog(object, AT(object, 0x1440), status);\n        return focus(object, code);\n    }\n    if (SERVICES->compare_text',
     '    if (status != 0x1005) {\n        SERVICES->error_dialog(object, AT(object, 0x1440), status);\n        return focus(object, code);\n    }\n    if (SERVICES->compare_text'),
    ('error-agreed', 'PTR(descriptor, 0x1c)) == 0) return 0;', 'PTR(descriptor, 0x1c)) != 0) return 0;'),
    ('error-style', '    D32(object, 0x94) = 2;\n    if (SERVICES->error_dialog(object, AT(object, 0x1440), D32(descriptor, 0x18)) == 0) {',
     '    D32(object, 0x94) = 1;\n    if (SERVICES->error_dialog(object, AT(object, 0x1440), D32(descriptor, 0x18)) == 0) {'),
    ('error-refused-clears', '        SERVICES->copy_text((char *)AT(object, 0x10e4), fs98_options_empty_text);\n', ''),
    ('error-refused-focus', '        return focus(object, 0x11a3);', '        return focus(object, code + 1);'),
    ('error-agreed-room', 'AT(object, 0x10e4), 0xff);\n    SERVICES->post_message', 'AT(object, 0x10e4), 0x100);\n    SERVICES->post_message'),
    ('error-agreed-post', 'post_message(D32(object, 0x20), 0x5fa, D32(object, 0x14c8), 0);', 'post_message(D32(object, 0x20), 0x5fa, D32(object, 0x1460), 0);'),
    # Flight video
    ('video-close-forgets', '    PTR(object, 0x10dc) = 0;\n    SERVICES->close(object);', '    SERVICES->close(object);'),
    ('video-cancel-word', '    D32(VIDEO_DESCRIPTOR(object), 0x14) = 0;\n    D32(VIDEO_DESCRIPTOR(object), 0) = 0;', '    D32(VIDEO_DESCRIPTOR(object), 0x14) = 1;\n    D32(VIDEO_DESCRIPTOR(object), 0) = 0;'),
    ('video-accept-answer', '    D32(VIDEO_DESCRIPTOR(object), 0) = 1;', '    D32(VIDEO_DESCRIPTOR(object), 0) = 2;'),
    ('video-accept-held', 'if (failure != 0 && ((uint32_t (TC *)(void *, uint32_t))vtable[0xdc / 4])(object, failure) != 0)\n        return;\n    D32(VIDEO_DESCRIPTOR(object), 0x10) = 0;',
     'if (((uint32_t (TC *)(void *, uint32_t))vtable[0xdc / 4])(object, failure) != 0)\n        return;\n    D32(VIDEO_DESCRIPTOR(object), 0x10) = 0;'),
    ('select-pick', '    uint8_t *item;\n    D32(VIDEO_DESCRIPTOR(object), 0x28) = (uint32_t)',
     '    uint8_t *item;\n    D32(VIDEO_DESCRIPTOR(object), 0x2c) = (uint32_t)'),
    ('select-title', 'set_window_text(AT(object, 0x14a8), (char *)item + 0x100);', 'set_window_text(AT(object, 0x14a8), (char *)item + 0x500);'),
    ('select-writable', 'enable_control(AT(object, 0x11a0), (int32_t)D32(item, 0xa04));', 'enable_control(AT(object, 0x11a0), 1);'),
    ('select-delete', 'enable_control(AT(object, 0x1750), (int32_t)D32(item, 0xa04));', 'enable_control(AT(object, 0x1654), (int32_t)D32(item, 0xa04));'),
    ('populate-record', 'enable_control(AT(object, 0x1558), (int32_t)D32(VIDEO_DESCRIPTOR(object), 0x18));', 'enable_control(AT(object, 0x1558), (int32_t)D32(VIDEO_DESCRIPTOR(object), 0x1c));'),
    ('populate-count-once', '        } while (index < (int32_t)D32(VIDEO_DESCRIPTOR(object), 0x2c));', '        } while (index < count);'),
    ('populate-count-signed', '    if (count > 0) {', '    if ((uint32_t)count > 0) {'),
    ('populate-stride', '            record += RECORD;', '            record += RECORD - 8;'),
    ('populate-item-data', 'LB_SETITEMDATA, entry, (uint32_t)(uintptr_t)record);', 'LB_SETITEMDATA, index, (uint32_t)(uintptr_t)record);'),
    ('populate-pick', 'LB_SETCURSEL, D32(VIDEO_DESCRIPTOR(object), 0x28), 0);', 'LB_SETCURSEL, D32(VIDEO_DESCRIPTOR(object), 0x20), 0);'),
    ('populate-rate', 'CB_SETCURSEL, D32(VIDEO_DESCRIPTOR(object), 0x20), 0);', 'CB_SETCURSEL, D32(VIDEO_DESCRIPTOR(object), 0x28), 0);'),
    ('populate-loop', 'set_checked(AT(object, 0x1948), (int32_t)D32(VIDEO_DESCRIPTOR(object), 0x24));', 'set_checked(AT(object, 0x1948), 1);'),
    ('populate-select', '    fs98_options_flight_video_select(object);\n}\n/* 0x2239d4d0', '}\n/* 0x2239d4d0'),
    ('video-init-category', '    D32(object, 0x84) = 0xb;', '    D32(object, 0x84) = 0xa;'),
    ('video-init-combo', 'init_combo(AT(object, 0x1510), AT(object, 0x3e0), AT(object, 0x1258));', 'init_combo(AT(object, 0x1510), AT(object, 0x450), AT(object, 0x1258));'),
    ('video-init-rate', 'send_message(D32(object, 0x1530), CB_SETCURSEL, 0, 0);', 'send_message(D32(object, 0x1530), CB_SETCURSEL, 1, 0);'),
    ('video-init-list', 'init_list(AT(object, 0x13f8), AT(object, 0x450), AT(object, 0x10e8));', 'init_list(AT(object, 0x13f8), AT(object, 0x3e0), AT(object, 0x10e8));'),
    ('video-init-limit', 'send_message(D32(object, 0x1460), EM_LIMITTEXT, 0xff, 0);', 'send_message(D32(object, 0x1460), EM_LIMITTEXT, 0xfe, 0);'),
    ('video-init-check', '        check, AT(object, 0x4c0), 0x139,', '        check, AT(object, 0x4c0), 0x13a,'),
    ('video-init-populate', '    fs98_options_flight_video_populate(object);\n    return 1;', '    return 1;'),
    ('delete-pick', '    D32(VIDEO_DESCRIPTOR(object), 0x28) = (uint32_t)SERVICES->send_message(D32(object, 0x1418), LB_GETCURSEL, 0, 0);\n    descriptor = VIDEO_DESCRIPTOR(object);\n    if',
     '    descriptor = VIDEO_DESCRIPTOR(object);\n    if'),
    ('delete-refused', '(descriptor) != 0) return;', '(descriptor) == 0) return;'),
    ('delete-focus', '    if (!SERVICES->window_enabled(AT(object, 0x1750))) SERVICES->set_focus(AT(object, 0x13f8));', '    SERVICES->set_focus(AT(object, 0x13f8));'),
    ('play-word', '    D32(VIDEO_DESCRIPTOR(object), 0x10) = 1;', '    D32(VIDEO_DESCRIPTOR(object), 0x10) = 2;'),
    ('play-loop', '    D32(VIDEO_DESCRIPTOR(object), 0x24) = D32(object, 0x1a04);\n    descriptor = VIDEO_DESCRIPTOR(object);\n    vtable = PTR(object, 0);\n    descriptor[0] = 0;\n    ((void (TC *)(void *))PTR(vtable, 0xec))(object);\n}\n/* 0x2239d030',
     '    descriptor = VIDEO_DESCRIPTOR(object);\n    vtable = PTR(object, 0);\n    descriptor[0] = 0;\n    ((void (TC *)(void *))PTR(vtable, 0xec))(object);\n}\n/* 0x2239d030'),
    ('record-word', '    D32(VIDEO_DESCRIPTOR(object), 0x14) = 1;', '    D32(VIDEO_DESCRIPTOR(object), 0x14) = 2;'),
    ('record-rate', '    D32(VIDEO_DESCRIPTOR(object), 0x14) = 1;\n    D32(VIDEO_DESCRIPTOR(object), 0x20) = (uint32_t)SERVICES->send_message(D32(object, 0x1530), CB_GETCURSEL, 0, 0);\n',
     '    D32(VIDEO_DESCRIPTOR(object), 0x14) = 1;\n'),
    ('double-click-order', '    fs98_options_flight_video_select(object);\n    fs98_options_flight_video_play(object);', '    fs98_options_flight_video_play(object);\n    fs98_options_flight_video_select(object);'),
    ('rate-field', '    D32(VIDEO_DESCRIPTOR(object), 0x20) = (uint32_t)SERVICES->send_message(D32(object, 0x1530), CB_GETCURSEL, 0, 0);\n}',
     '    D32(VIDEO_DESCRIPTOR(object), 0x24) = (uint32_t)SERVICES->send_message(D32(object, 0x1530), CB_GETCURSEL, 0, 0);\n}'),
    ('validate-readonly', '    if (D32(item, 0xa04) == 0) return 0;', '    if (D32(item, 0xa00) == 0) return 0;'),
    ('validate-loop', '    D32(VIDEO_DESCRIPTOR(object), 0x24) = D32(object, 0x1a04);\n    descriptor = VIDEO_DESCRIPTOR(object);\n    {',
     '    descriptor = VIDEO_DESCRIPTOR(object);\n    {'),
    ('validate-copy-stride', 'PTR(descriptor, 0x30) + D32(descriptor, 0x28) * RECORD;\n        for', 'PTR(descriptor, 0x30) + D32(descriptor, 0x28) * (RECORD - 8);\n        for'),
    ('validate-title-control', '    if (control == 0x11a1) {\n        SERVICES->get_window_text(AT(object, 0x14a8)', '    if (control == 0x11a3) {\n        SERVICES->get_window_text(AT(object, 0x14a8)'),
    ('validate-title-clears', '        record[0] = 0;\n    } else', '    } else'),
    ('validate-name-clears', '        record[0x100] = 0;\n    }', '    }'),
    ('validate-same-name', 'SERVICES->host_strcmpi(s_video->last, name) == 0 || s_video->last[0] == 0)', 'SERVICES->host_strcmpi(s_video->last, name) == 0)'),
    ('validate-same-control', '    if (control == 0x11a3 &&\n', '    if (control != 0x11a1 &&\n'),
    ('validate-same-copies', 'host_strcpy(s_video->last, (const char *)PTR(descriptor, 0x30) + D32(descriptor, 0x28) * RECORD);', 'host_strcpy(s_video->last, name);'),
    ('validate-store', '    D32(object, 0x10e0) = result;\n    if (result == 0) {', '    if (result == 0) {'),
    ('validate-kept', '        SERVICES->host_strcpy(s_video->last, name);\n        fs98_options_flight_video_populate(object);', '        fs98_options_flight_video_populate(object);'),
    ('validate-ids', '    return control == 0x11a1 ? 0x11a1 : 0x11a3;', '    return control == 0x11a1 ? 0x11a3 : 0x11a1;'),
    ('video-error-zero', '    if (code == 0) return 0;\n    status = D32(object, 0x10e0);', '    status = D32(object, 0x10e0);'),
    ('video-error-title', '        SERVICES->error_dialog(object, AT(object, 0x14a8), status);', '        SERVICES->error_dialog(object, AT(object, 0x1440), status);'),
    ('video-error-exists', '    if (status != 0x1004) {\n        SERVICES->error_dialog(object, AT(object, 0x1440), status);\n        return focus(object, code);\n    }\n    D32(object, 0x90)',
     '    if (status != 0x1005) {\n        SERVICES->error_dialog(object, AT(object, 0x1440), status);\n        return focus(object, code);\n    }\n    D32(object, 0x90)'),
    ('video-error-agreed', 'AT(object, 0x1440), status) == 0) return focus(object, code);', 'AT(object, 0x1440), status) != 0) return focus(object, code);'),
    ('video-error-refill', '    fs98_options_flight_video_populate(object);\n    return 0;\n}', '    return 0;\n}'),
]
LOGBOOK = [
    # Add logbook
    ('add-cancel-state', '    D32(object, 0x1404) = 2;', '    D32(object, 0x1404) = 1;'),
    ('add-cancel-answer', '    part[0] = 0;\n    ((void (TC *)(void *))PTR(vtable, 0xec))(object);\n}\n/* 0x2239aa70',
     '    part[0] = 2;\n    ((void (TC *)(void *))PTR(vtable, 0xec))(object);\n}\n/* 0x2239aa70'),
    ('add-read-room', 'PTR(NEW_BOOK(object), 0xc), 0x100);', 'PTR(NEW_BOOK(object), 0xc), (int32_t)D32(NEW_BOOK(object), 0x10));'),
    ('add-check-argument', '    return ((fs98_options_check_fn)PTR(part, 4))(part);\n}\n/* 0x2239aab0', '    return ((fs98_options_check_fn)PTR(part, 4))(object);\n}\n/* 0x2239aab0'),
    ('add-error-exists', '    if (status == 0x1004) return D32(part, 0x1c) == 0;', '    if (status == 0x1004) return D32(part, 0x1c) != 0;'),
    ('add-error-none', '    if (status == 0) return 0;\n    D32(object, 0x8c)', '    D32(object, 0x8c)'),
    ('add-error-store', '    D32(object, 0x8c) = (uint32_t)SERVICES->error_dialog(object, AT(object, 0x1398), status);\n    if (D32(object, 0x8c) == 0) return 0;',
     '    if (SERVICES->error_dialog(object, AT(object, 0x1398), status) == 0) return 0;'),
    ('add-error-focus', '    SERVICES->set_focus(AT(object, 0x1398));\n', ''),
    ('add-error-select', 'post_message(D32(object, 0x13b8), EM_SETSEL, 0, 0xffffffff);', 'post_message(D32(object, 0x13b8), EM_SETSEL, 0, 0);'),
    ('add-accept-state', '    D32(object, 0x1404) = 1;', '    D32(object, 0x1404) = 2;'),
    ('add-accept-read', '    D32(object, 0x1404) = 1;\n    fs98_options_add_logbook_read(object);\n', '    D32(object, 0x1404) = 1;\n'),
    ('add-accept-error-skipped', 'if (fs98_options_add_logbook_check(object) == 0 || fs98_options_add_logbook_error(object) == 0) {',
     'if (fs98_options_add_logbook_check(object) == 0) {'),
    ('add-accept-answer', '        part[0] = 1;', '        part[0] = 2;'),
    ('add-clear-text', 'set_window_text(AT(object, 0x1398), fs98_options_empty_text);', 'set_window_text(AT(object, 0x1398), " ");'),
    ('add-part-offset', '(uint8_t *)PTR(object, 0x10e4) + 0x28;', '(uint8_t *)PTR(object, 0x10e4) + 0x2c;'),
    ('add-init-skin-flags', '    if (!skin(object, 9)) return 0;', '    if (!skin(object, 0xf)) return 0;'),
    ('add-init-button-order', '    button(object, 0x10e8);\n    button(object, 0x11e4);', '    button(object, 0x11e4);\n    button(object, 0x10e8);'),
    ('add-init-notify', '    PTR(object, 0x10dc) = fs98_options_new_notify();\n    if (!fs98_options_add_logbook_has', '    fs98_options_new_notify();\n    if (!fs98_options_add_logbook_has'),
    ('add-init-limit', 'EM_LIMITTEXT, D32(NEW_BOOK(object), 0x10) - 1, 0);', 'EM_LIMITTEXT, D32(NEW_BOOK(object), 0x10), 0);'),
    ('add-init-clear', '    fs98_options_add_logbook_clear(object);\n    return 1;', '    return 1;'),
    # Logbook entry
    ('entry-close', 'void TC fs98_options_logbook_entry_close(void *object) { SERVICES->close(object); }', 'void TC fs98_options_logbook_entry_close(void *object) { (void)object; }'),
    ('entry-cancel-answer', '    part[0] = 0;\n    ((void (TC *)(void *))PTR(vtable, 0xec))(object);\n}\n/* 0x22399d00', '    part[0] = 1;\n    ((void (TC *)(void *))PTR(vtable, 0xec))(object);\n}\n/* 0x22399d00'),
    ('entry-accept-answer', '    D32(ENTRY(object), 0) = 1;', '    D32(ENTRY(object), 0) = 2;'),
    ('entry-part-offset', '(uint8_t *)PTR(object, 0x10e0) + 0x54;', '(uint8_t *)PTR(object, 0x10e0) + 0x50;'),
    ('fill-totals-label', 'SERVICES->set_sims_text(AT(object, 0x10ec),', 'SERVICES->set_sims_text(AT(object, 0x11a4),'),
    ('fill-totals-stride', '(const char *)PTR(part, 0x98) + D32(part, 0xa0) * RECORD);', '(const char *)PTR(part, 0x98) + D32(part, 0xa0) * (RECORD - 8));'),
    ('fill-count-once', '            } while (index < D32(part, 0x9c));', '            } while (index < 3);'),
    ('fill-reload', '                part = ENTRY(object);\n                ++index;', '                ++index;'),
    ('fill-stride', '                offset += RECORD;', '                offset += RECORD - 8;'),
    ('fill-empty', '        if (D32(part, 0x9c) != 0) {', '        if (1) {'),
    ('fill-pick', 'CB_SETCURSEL, D32(ENTRY(object), 0xa0), 0);', 'CB_SETCURSEL, D32(ENTRY(object), 0x9c), 0);'),
    ('fill-field', 'set_window_text(AT(object, 0x1be0), PTR(ENTRY(object), 0x48));', 'set_window_text(AT(object, 0x1be0), PTR(ENTRY(object), 0x5c));'),
    ('init-category', '    D32(object, 0x84) = 0xb;', '    D32(object, 0x84) = 0xc;'),
    ('init-totals-hides', '        SERVICES->show_window(AT(object, 0x15f4), 0);\n', ''),
    ('init-totals-title', 'load_string_a(D32(object, 0x80), 0xf744,', 'load_string_a(D32(object, 0x80), 0xf74f,'),
    ('init-entry-combo', 'init_combo(AT(object, 0x1c48), AT(object, 0x3e0), AT(object, 0x15f4));', 'init_combo(AT(object, 0x1c48), AT(object, 0x3e0), AT(object, 0x1760));'),
    ('init-title-room', 'title, 0x27);\n    } else', 'title, 0x28);\n    } else'),
    ('init-title-text', 'SERVICES->set_window_text(AT(object, 0x1454), title);', 'SERVICES->set_window_text(AT(object, 0x1454), "");'),
    ('init-group', 'init_group(AT(object, 0x16ac), AT(object, 0x220));', 'init_group(AT(object, 0x16ac), AT(object, 0x370));'),
    ('init-day-flags', '    edit(object, 0x1be0, 0x1988, 5);', '    edit(object, 0x1be0, 0x1988, 4);'),
    ('init-date-flags', '    edit(object, 0x1aa8, 0x153c, 0x80);', '    edit(object, 0x1aa8, 0x153c, 0);'),
    ('init-time-room', 'EM_LIMITTEXT, D32(ENTRY(object), 0x60) - 3, 0);', 'EM_LIMITTEXT, D32(ENTRY(object), 0x60) - 1, 0);'),
    ('init-fill', '    fs98_options_logbook_entry_fill(object);\n    if (D32(ENTRY(object), 0xa4)) {', '    if (D32(ENTRY(object), 0xa4)) {'),
    ('init-totals-disable', '        SERVICES->enable_control(AT(object, 0x1b10), 0);\n', ''),
    ('init-totals-focus', '        SERVICES->set_focus(AT(object, 0x1a40));\n', ''),
    ('validate-totals', '        SERVICES->get_window_text(AT(object, 0x1a40), PTR(part, 0x34), (int32_t)D32(part, 0x38));\n        return 0;',
     '        SERVICES->get_window_text(AT(object, 0x1a40), PTR(part, 0x34), (int32_t)D32(part, 0x38));\n        return 1;'),
    ('validate-reload', '    part = ENTRY(object);\n    SERVICES->get_window_text(AT(object, 0x1be0)', '    SERVICES->get_window_text(AT(object, 0x1be0)'),
    ('validate-pick', 'D32(ENTRY(object), 0xa0) = (uint32_t)SERVICES->send_message(D32(object, 0x1c68), CB_GETCURSEL, 0, 0);', '(void)0;'),
    ('validate-cut', '    name[0x10] = 0;', '    name[0x11] = 0;'),
    ('validate-name-at', '    out = PTR(ENTRY(object), 0x20);', '    out = PTR(ENTRY(object), 0x34);'),
    ('validate-order', '    if (D32(part, 0x30)) return 0x1198;\n    if (D32(part, 0x44)) return 0x1194;', '    if (D32(part, 0x44)) return 0x1194;\n    if (D32(part, 0x30)) return 0x1198;'),
    ('validate-answer', '    if (D32(part, 0x6c)) return 0x1195;\n    return result;', '    if (D32(part, 0x6c)) return 0x1195;\n    return 0;'),
    ('validate-reload-after-check', '    result = ((fs98_options_check_fn)PTR(part, 4))(part);\n    part = ENTRY(object);', '    result = ((fs98_options_check_fn)PTR(part, 4))(part);'),
    ('error-zero', '    if (code == 0) {\n        entry_error_cleanup', '    if (code == 1) {\n        entry_error_cleanup'),
    ('error-date-accepted', 'AT(object, 0x1aa8), D32(part, 8))) target = 0x1199;', 'AT(object, 0x1aa8), D32(part, 8))) target = 0x1198;'),
    ('error-day-value', '(uint32_t)(uintptr_t)(part + 0x44))) target = 0x1194;', 'D32(part, 0x44))) target = 0x1194;'),
    ('error-night-value', '(uint32_t)(uintptr_t)(part + 0x58))) target = 0x1196;', 'D32(part, 0x58))) target = 0x1196;'),
    ('error-total', '    } else if (D32(part, 0x6c) == 0x1013) {', '    } else if (D32(part, 0x6c) == 0x1014) {'),
    ('error-total-shows', '        SERVICES->set_sims_text(AT(object, 0x18d0), first.text);\n', ''),
    ('error-total-restores', '        SERVICES->set_sims_text(AT(object, 0x18d0), second.text);\n', ''),
    ('error-total-swapped', '        SERVICES->get_sims_text(AT(object, 0x18d0), &second);\n        SERVICES->window_text_string(AT(object, 0x1b78), &first);',
     '        SERVICES->get_sims_text(AT(object, 0x18d0), &first);\n        SERVICES->window_text_string(AT(object, 0x1b78), &second);'),
    ('error-instrument-value', '(uint32_t)(uintptr_t)(part + 0x6c))) target = 0x1195;', 'D32(part, 0x6c))) target = 0x1195;'),
    ('error-destroy-order', "    if (state->live > 1) { state->live = 1; SERVICES->string_destroy(state->second); }\n    if (state->live > 0) { state->live = 0; SERVICES->string_destroy(state->first); }",
     "    if (state->live > 1) { state->live = 1; SERVICES->string_destroy(state->first); }\n    if (state->live > 0) { state->live = 0; SERVICES->string_destroy(state->second); }"),
]
BOOK = [
    ('scroll-nothing', '{ (void)object; (void)a; (void)b; (void)c; }', '{ (void)object; (void)a; (void)b; SERVICES->ui_delete(object); (void)c; }'),
    ('list-delete-flag', '    if (flags & 1) SERVICES->ui_delete(list);', '    if (flags & 2) SERVICES->ui_delete(list);'),
    ('list-delete-destroy', '    SERVICES->list_destroy(list);\n', ''),
    ('close-notify-at', '    void *notify = PTR(object, 0x10f8);', '    void *notify = PTR(object, 0x10dc);'),
    ('cancel-state', '    D32(object, 0x1af4) = 2;', '    D32(object, 0x1af4) = 1;'),
    ('read-books', '    D32(BLOCK(object), 0x20) = (uint32_t)selection(BOOKS(object));\n    D32(BLOCK(object), 0x4c)', '    D32(BLOCK(object), 0x24) = (uint32_t)selection(BOOKS(object));\n    D32(BLOCK(object), 0x4c)'),
    ('read-rows', '    D32(BLOCK(object), 0x4c) = (uint32_t)selection(ROWS(object));\n}', '    D32(BLOCK(object), 0x4c) = (uint32_t)selection(BOOKS(object));\n}'),
    ('accept-state', '    D32(object, 0x1af4) = 1;', '    D32(object, 0x1af4) = 3;'),
    ('accept-read', '    D32(object, 0x1af4) = 1;\n    fs98_options_book_read(object);', '    D32(object, 0x1af4) = 1;'),
    ('accept-answer', '        block[0] = 1;', '        block[0] = 2;'),
    ('fill-stride', '++index, book += BOOK)', '++index, book += BOOK - 4)'),
    ('fill-count', 'index < D32(BLOCK(object), 0x1c);', 'index <= D32(BLOCK(object), 0x1c);'),
    ('fill-data', 'LB_SETITEMDATA, entry, (uint32_t)(uintptr_t)book);', 'LB_SETITEMDATA, index, (uint32_t)(uintptr_t)book);'),
    ('fill-kept', '    D32(object, 0x10dc) = D32(BLOCK(object), 0x20);', '    D32(object, 0x10dc) = D32(BLOCK(object), 0x1c);'),
    ('show-none', '    if (chosen == -1) {\n        show_cleanup', '    if (chosen == -2) {\n        show_cleanup'),
    ('show-title', 'SERVICES->set_window_text(AT(object, 0x1554), line.text);', 'SERVICES->set_window_text(AT(object, 0x1554), "");'),
    ('show-choose', '    call(object, 3, D32(BLOCK(object), 0x20));\n    row', '    row'),
    ('show-rows-early', '    row = PTR(BLOCK(object), 0x50);\n    SERVICES->enable_control(AT(object, 0x150c), 1);', '    SERVICES->enable_control(AT(object, 0x150c), 1);\n    row = PTR(BLOCK(object), 0x50);'),
    ('show-aircraft', '"%-16s", (const char *)remarks - 0x11);', '"%-16s", (const char *)remarks - 0x10);'),
    ('show-remarks-width', '"%-24s", (const char *)remarks);', '"%-23s", (const char *)remarks);'),
    ('show-short-tab', 'if (SERVICES->text_length((const char *)remarks) < 3)', 'if (SERVICES->text_length((const char *)remarks) < 2)'),
    ('show-time', '"%6s", (const char *)remarks + 0x27);', '"%6s", (const char *)remarks + 0x20);'),
    ('show-row-data', 'LB_SETITEMDATA, entry, (uint32_t)(uintptr_t)row);', 'LB_SETITEMDATA, entry, (uint32_t)(uintptr_t)line.text);'),
    ('show-row-stride', '            row += ROW;', '            row += ROW + 1;'),
    ('show-count-once', '        } while (index < D32(BLOCK(object), 0x48));', '        } while (index < 2);'),
    ('show-empty', '    if (D32(BLOCK(object), 0x48) < 1) {', '    if (D32(BLOCK(object), 0x48) < 2) {'),
    ('show-empty-rows', '        SERVICES->enable_control(AT(object, 0x150c), 0);\n', ''),
    ('show-totals-string', 'load_string_a(D32(object, 0x80), 0xf759, buffer, 0x18);', 'load_string_a(D32(object, 0x80), 0xf75a, buffer, 0x18);'),
    ('show-totals-field', 'column(&line, "%6s", PTR(BLOCK(object), 0x128));', 'column(&line, "%6s", PTR(BLOCK(object), 0x13c));'),
    ('show-first-row', '        D32(BLOCK(object), 0x4c) = 0;\n', ''),
    ('show-choose-row', '        call(object, 7, 0);', '        call(object, 7, 1);'),
    ('show-deletable', 'D32(book, 0x120) ? 1 : 0);', 'D32(book, 0x11c) ? 1 : 0);'),
    ('refresh-none', '    if (D32(BLOCK(object), 0x1c) != 0) {\n        SERVICES->enable_control(AT(object, 0x19f4), 1);', '    if (D32(BLOCK(object), 0x1c) > 1) {\n        SERVICES->enable_control(AT(object, 0x19f4), 1);'),
    ('refresh-show', '        fs98_options_book_fill_books(object);\n        fs98_options_book_show_book(object);', '        fs98_options_book_fill_books(object);'),
    ('refresh-clear', '    SERVICES->set_window_text(AT(object, 0x1554), fs98_options_empty_text);\n    SERVICES->send_message(BOOKS', '    SERVICES->send_message(BOOKS'),
    ('open-field', '{9, 0x74}', '{9, 0x60}'),
    ('new-row-mode', '    D32(BLOCK(object), 0xe8) = 4;', '    D32(BLOCK(object), 0xe8) = 5;'),
    ('new-row-template', '    if (D32(BLOCK(object), 0x48) != 0) open_row(object, 0);', '    open_row(object, 0);'),
    ('new-row-op', '    call(object, 4, (uint32_t)chosen);', '    call(object, 4, D32(BLOCK(object), 0x20) + 1);'),
    ('new-row-focus', '    SERVICES->set_focus(AT(object, 0x19f4));', '    SERVICES->set_focus(AT(object, 0x18f8));'),
    ('edit-row-none', '    if (row == -1) return;\n    D32(BLOCK(object), 0x4c) = (uint32_t)row;\n    call(object, 7, (uint32_t)row);', '    D32(BLOCK(object), 0x4c) = (uint32_t)row;\n    call(object, 7, (uint32_t)row);'),
    ('edit-row-mode', '    D32(BLOCK(object), 0xe8) = 5;', '    D32(BLOCK(object), 0xe8) = 4;'),
    ('edit-row-offset', '    open_row(object, (uint32_t)row * ROW);', '    open_row(object, (uint32_t)row * ROW + 1);'),
    ('delete-row-op', '    call(object, 6, D32(BLOCK(object), 0x4c));', '    call(object, 6, (uint32_t)row + 1);'),
    ('new-book-op', '    call(object, 1, D32(BLOCK(object), 0x20));', '    call(object, 1, 0);'),
    ('delete-book-focus', '    if (SERVICES->window_enabled(AT(object, 0x17fc)) == 0)', '    if (SERVICES->window_enabled(AT(object, 0x17fc)) != 0)'),
    ('delete-book-post', 'post_message(D32(object, 0x20), 0x5fa, D32(object, 0x1720), 0);', 'post_message(D32(object, 0x20), 0x5fa, D32(object, 0x1720), 1);'),
    ('choose-row-op', '    call(object, 7, D32(BLOCK(object), 0x4c));\n}\n/* 0x22399600', '    call(object, 7, 0);\n}\n/* 0x22399600'),
    ('rename-room', 'title, 0x1f);', 'title, 0x20);'),
    ('rename-same', 'if (SERVICES->compare_text(title, book) == 0) return;', 'if (SERVICES->compare_text(title, book) != 0) return;'),
    ('rename-op', 'if (call(object, 8, (uint32_t)chosen) != 0 && D32(BLOCK(object), 8) != 0) {', 'if (call(object, 8, (uint32_t)chosen) == 0 && D32(BLOCK(object), 8) != 0) {'),
    ('rename-status', 'if (call(object, 8, (uint32_t)chosen) != 0 && D32(BLOCK(object), 8) != 0) {', 'if (call(object, 8, (uint32_t)chosen) != 0) {'),
    ('rename-select', 'post_message(D32(object, 0x1574), EM_SETSEL, 0, 0xffffffff);', 'post_message(D32(object, 0x1574), EM_SETSEL, 0, 0);'),
    ('rename-refill', '    fs98_options_book_fill_books(object);\n}\n/* 0x22398550', '}\n/* 0x22398550'),
    ('init-stops', '    D32(object, 0x10e8) = 0x125;', '    D32(object, 0x10e8) = 0x126;'),
    ('init-total-guard', '    if (!(s_state->guard & 1)) {\n        s_state->guard |= 1;', '    if (!(s_state->guard & 2)) {\n        s_state->guard |= 1;'),
    ('init-total-base', 'font_construct(font, SERVICES->base_font(1));', 'font_construct(font, SERVICES->base_font(3));'),
    ('init-total-exit', 'register_exit(fs98_options_book_total_font_destroy);', 'register_exit(fs98_options_book_rows_font_destroy);'),
    ('init-total-field', '    D32(font, 0xc) = D32(object, 0x37c);', '    D32(font, 0xc) = D32(object, 0x378);'),
    ('init-total-stops', 'set_tab_stops(font, 4, (int32_t *)AT(object, 0x10e8));', 'set_tab_stops(font, 4, (int32_t *)AT(object, 0x10e0));'),
    ('init-rows-field', '    D32(font, 0x24) = D32(object, 0x474);', '    D32(font, 0x24) = D32(object, 0x470);'),
    ('init-rows-stops', 'set_tab_stops(font, 6, (int32_t *)AT(object, 0x10e0));', 'set_tab_stops(font, 5, (int32_t *)AT(object, 0x10e0));'),
    ('init-books-list', 'init_list(AT(object, 0x15bc), AT(object, 0x3e0), 0);', 'init_list(AT(object, 0x15bc), AT(object, 0x3e0), AT(object, 0x12e4));'),
    ('init-totals-font', 'init_text(AT(object, 0x1454), s_state->total_font);', 'init_text(AT(object, 0x1454), s_state->rows_font);'),
    ('init-rows-list', 'init_list(AT(object, 0x150c), s_state->rows_font, AT(object, 0x139c));', 'init_list(AT(object, 0x150c), s_state->total_font, AT(object, 0x139c));'),
    ('init-measure-final', '        D32(object, 0x1af8) = 0x299;', '        (void)0;'),
    ('init-label-room', 'AT(object, 0x1b00), 0xff);', 'AT(object, 0x1b00), 0xfe);'),
    ('init-label-measure', '        D32(object, 0x1afc) = (uint32_t)(length * 10 / 5);', '        D32(object, 0x1afc) = (uint32_t)(length * 10 / 4);'),
    ('init-notify', '    PTR(object, 0x10f8) = fs98_options_new_notify();', '    fs98_options_new_notify();'),
    ('init-limit', 'EM_LIMITTEXT, D32(BLOCK(object), 0x10) - 1, 0);', 'EM_LIMITTEXT, D32(BLOCK(object), 0x10), 0);'),
    ('init-refresh', '    fs98_options_book_refresh(object);\n    return 1;\n}', '    return 1;\n}'),
]
UNWIND = [
    ('show-unwind-line', 'book', "    if (state->live) { state->live = 0; SERVICES->string_destroy(state->line); }", "    (void)state;"),
    ('book-list-vtable', 'objects', '            PTR((uint8_t *)object + control->offset, 0) = s_list_vtable;', '            (void)0;'),
    ('entry-error-unwind-second', 'logbook', '    SERVICES->string_construct(&second);\n    state.live = 2;', '    SERVICES->string_construct(&second);\n    state.live = 1;'),
    ('add-constructor-string', 'objects', '    if (kind == FS98_OPTIONS_ADD_LOGBOOK) s_services->string_assign', '    if (kind == 99) s_services->string_assign'),
    ('notify-unwind', 'videos', "    if (state->live) { state->live = 0; SERVICES->notify_free(state->notify); }", "    (void)state;"),
    ('notify-unwind-free', 'videos', "SERVICES->notify_free(state->notify); }", "SERVICES->ui_delete(state->notify); }"),
    ('save-constructor-name', 'objects', '    if (kind == FS98_OPTIONS_SAVE_VIDEO) s_services->copy_text', '    if (kind == 99) s_services->copy_text'),
    ('video-constructor-notify', 'objects', '    if (kind == FS98_OPTIONS_FLIGHT_VIDEO) D32(object, 0x10dc) = 0;', '    (void)0;'),
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
    breaks += [(name, 'videos', 'normal', before, after) for name, before, after in VIDEOS]
    breaks += [(name, 'logbook', 'normal', before, after) for name, before, after in LOGBOOK]
    breaks += [(name, 'book', 'normal', before, after) for name, before, after in BOOK]
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
