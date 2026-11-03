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
terminates what it writes).
"""
from pathlib import Path
import subprocess
import sys

UNITS = ('events', 'objects', 'video', 'videos')
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
    ('save-init-notify', '    PTR(object, 0x10dc) = new_notify();\n    if (!fs98_options_save_video_has_descriptor', '    new_notify();\n    if (!fs98_options_save_video_has_descriptor'),
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
UNWIND = [
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
