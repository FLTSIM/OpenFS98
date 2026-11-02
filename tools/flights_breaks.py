#!/usr/bin/env python3
"""Break src/flights/ on purpose and check flightseventsdiff reports each break.

    python3 -B tools/flights_breaks.py [scenarios] [name...]

Only a reported DIFFER counts as caught: a build failure, a fault or a timeout does not.
Run from a frozen copy of the tree if the sources may change meanwhile.

Left out as proven equivalent: a null check on post_destroy's object made to depend on its
window handle (never zero in the fixture), and cancel's descriptor read moved after the
state write, and select's second read of the descriptor folded into its first (no call
between them can observe either). A removed null check faults on
both sides rather than differing, which is not counted.
"""
from pathlib import Path
import subprocess
import sys

UNITS = ('events', 'objects', 'save', 'comms', 'create', 'aircraft', 'opening', 'select')
EVENTS = [
    # the shared helpers
    ('disable-window', 'enable_window(D32(object, 0x20), 0)', 'enable_window(D32(object, 0x20), 1)'),
    ('enable-window', 'enable_window(D32(object, 0x20), 1)', 'enable_window(D32(object, 0x20), 0)'),
    ('post-destroy-flags', '))(object, 1);\n}', '))(object, 0);\n}'),
    ('help-command', 's_help_file, 0xc,', 's_help_file, 0xd,'),
    ('help-window', 'win_help(D32(info, 0xc)', 'win_help(D32(info, 0x8)'),
    ('context-command', 's_help_file, 0xa,', 's_help_file, 0xb,'),
    ('context-return', '    return 1; \\\n} \\\nvoid', '    return 0; \\\n} \\\nvoid'),
    ('ddx-control-id', 's_ddx_##name[index][1],', 's_ddx_##name[index][1] ^ 1,'),
    ('ddx-base', 'if (base) s_services->base_exchange', 'if (!base) s_services->base_exchange'),
    # cancel and accept
    ('cancel-state', '    D32(object, 0x15e0) = 2;\n    descriptor[0] = 0;', '    D32(object, 0x15e0) = 3;\n    descriptor[0] = 0;'),
    ('cancel-answer', '    descriptor[0] = 0;', '    descriptor[0] = 2;'),
    ('cancel-slot', '((void (TC *)(void *))PTR(vtable, 0xec))(object);\n}\n\n/* 0x22313040',
     '((void (TC *)(void *))PTR(vtable, 0xe8))(object);\n}\n\n/* 0x22313040'),
    ('accept-state', 'D32(object, 0x15e0) = 1;', 'D32(object, 0x15e0) = 2;'),
    ('accept-answer', '        descriptor[0] = 1;', '        descriptor[0] = 2;'),
    ('accept-sync', '    D32(object, 0x15e0) = 1;\n    fs98_flights_picker_sync(object);', '    D32(object, 0x15e0) = 1;'),
    ('accept-descriptor-early', '    D32(object, 0x15e0) = 1;\n    fs98_flights_picker_sync(object);',
     '    uint32_t *early = PTR(object, 0x10dc);\n    D32(object, 0x15e0) = 1;\n    fs98_flights_picker_sync(object);\n    PTR(object, 0x10dc) = early;'),
    ('accept-error-check', 'if (fs98_flights_picker_no_error(object) && fs98_flights_picker_no_error(object)) return;',
     'if (!fs98_flights_picker_no_error(object)) return;'),
    ('no-error-value', '{ (void)object; return 0; }', '{ (void)object; return 1; }'),
    ('validate-value', 'return PTR(object, 0x10dc) != 0;', 'return 1;'),
    ('adventures-empty-cancel', '        VCALL(object, 0xd0);\n', '        fs98_flights_picker_cancel(object);\n'),
    ('challenges-empty-cancel', '        fs98_flights_picker_cancel(object);\n        return;\n    }\n    fs98_flights_picker_accept(object);\n}\n\n/* Fill',
     '        VCALL(object, 0xd0);\n        return;\n    }\n    fs98_flights_picker_accept(object);\n}\n\n/* Fill'),
    # selection
    ('select-none', '    if (selection == -1) return;\n    D32(PTR', '    if (selection == -2) return;\n    D32(PTR'),
    ('select-stride', 'D32(descriptor, 0xc) * (uint32_t)0xa08 + 0x100', 'D32(descriptor, 0xc) * (uint32_t)0xa00 + 0x100'),
    ('select-description', 'D32(descriptor, 0xc) * (uint32_t)0xa08 + 0x100', 'D32(descriptor, 0xc) * (uint32_t)0xa08'),
    ('select-control', 'set_window_text(AT(object, 0x1298),', 'set_window_text(AT(object, 0x1250),'),
    ('sync-none', '    if (selection != -1) D32', '    D32'),
    # filling the list
    ('fill-reset', 'LB_RESETCONTENT, 0, 0);', 'LB_RESETCONTENT, 1, 0);'),
    ('fill-reload', '                                 (uint32_t)(uintptr_t)AT(PTR(descriptor, 0x10), offset));\n        descriptor = PTR(object, 0x10dc);',
     '                                 (uint32_t)(uintptr_t)AT(PTR(descriptor, 0x10), offset));'),
    ('fill-stride', 'offset += 0xa08)', 'offset += 0xa00)'),
    ('fill-count', 'index < D32(descriptor, 8);', 'index <= D32(descriptor, 8);'),
    ('adventures-range', 'if (D32(descriptor, 0xc) < D32(descriptor, 8))', 'if (D32(descriptor, 0xc) <= D32(descriptor, 8))'),
    ('adventures-signed-range', 'if (D32(descriptor, 0xc) < D32(descriptor, 8))', 'if ((int32_t)D32(descriptor, 0xc) < (int32_t)D32(descriptor, 8))'),
    ('adventures-select-always', '        s_services->send_message(D32(object, 0x1270), LB_SETCURSEL, D32(descriptor, 0xc), 0);\n    fs98_flights_picker_select(object);\n}',
     '    {\n        s_services->send_message(D32(object, 0x1270), LB_SETCURSEL, D32(descriptor, 0xc), 0);\n        fs98_flights_picker_select(object);\n    }\n}'),
    ('challenges-empty', 'if (D32(descriptor, 8) != 0) {', 'if (D32(descriptor, 0xc) < D32(descriptor, 8)) {'),
    ('double-click-order', '    fs98_flights_picker_select(object);\n    VCALL(object, 0xcc);\n}\nvoid',
     '    VCALL(object, 0xcc);\n    fs98_flights_picker_select(object);\n}\nvoid'),
    ('double-click-slot', '    VCALL(object, 0xcc);\n}\n\n/* 0x22312ec0', '    VCALL(object, 0xd0);\n}\n\n/* 0x22312ec0'),
    # OnInitDialog
    ('init-skin-flags', 's_resource_module, 7)', 's_resource_module, 6)'),
    ('init-adventures-skin', '(object, 0x13b,', '(object, 0x13c,'),
    ('init-challenges-skin', '(object, 0x13c,', '(object, 0x13b,'),
    ('init-art-flags', 'AT(object, 0x7e8), 2);', 'AT(object, 0x7e8), 1);'),
    ('init-button-bitmap', '0x132,\n                            AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff,\n                            s_resource_module, 1, 1, empty);\n    s_services->init_text',
     '0x133,\n                            AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff,\n                            s_resource_module, 1, 1, empty);\n    s_services->init_text'),
    ('init-button-color', '1, 0xffffffff,\n                            s_resource_module, 1, 1, empty);\n    s_services->init_button',
     '1, 0xfffffffe,\n                            s_resource_module, 1, 1, empty);\n    s_services->init_button'),
    ('init-list-label', 'AT(object, 0x450), AT(object, 0x10e0));', 'AT(object, 0x450), AT(object, 0x1198));'),
    ('init-edit-font', 'base_font(2)', 'base_font(1)'),
    ('init-edit-error', '0xff0000,\n                          AT(object, 0x1198), 0);', '0xfe0000,\n                          AT(object, 0x1198), 0);'),
    ('init-text-order', '    s_services->init_text(AT(object, 0x10e0), AT(object, 0x370));\n    s_services->init_list(AT(object, 0x1250), AT(object, 0x450), AT(object, 0x10e0));',
     '    s_services->init_list(AT(object, 0x1250), AT(object, 0x450), AT(object, 0x10e0));\n    s_services->init_text(AT(object, 0x10e0), AT(object, 0x370));'),
    ('init-limit', 'EM_LIMITTEXT, 0x3ff, 0);', 'EM_LIMITTEXT, 0x400, 0);'),
    ('init-return', '    populate(object);\n    return 1;', '    populate(object);\n    return 2;'),
    ('init-skin-failure', '            object, skin, s_resource_module, 7)) return 0;', '            object, skin, s_resource_module, 7)) return 1;'),
]
OBJECTS = [
    ('constructor-parent', 'class_info->template, parent)', 'class_info->template, (void *)0)'),
    ('constructor-vtable', 'PTR(object, 0) = s_vtables[kind];', 'PTR(object, 0) = s_vtables[kind ^ 1];'),
    ('delete-flags', 'if (flags & 1) s_services->ui_delete', 'if (flags & 2) s_services->ui_delete'),
    ('factory-module', 'D32(object, 0x80) = s_module;', 'D32(object, 0x80) = s_module ^ 1;'),
    ('factory-show', 'show(object, 1)', 'show(object, 0)'),
    ('factory-size', 'allocate(class_info->size)', 'allocate(class_info->size - 4)'),
    ('factory-template', 'create(object, class_info->template, 0)', 'create(object, class_info->template + 1, 0)'),
    ('save-constructor-name', 'if (kind == FS98_FLIGHTS_SAVE) s_services->copy_text', 'if (kind == FS98_FLIGHTS_CHALLENGES) s_services->copy_text'),
    ('factory-return', 'return D32(object, 0x20);', 'return D32(object, 0x24);'),
]
SAVE = [
    ('save-limit-name', "0x10) - 1, 0);", "0x10) - 2, 0);"),
    ('save-limit-description', "0x24) - 1, 0);", "0x24), 0);"),
    ('save-populate-text', 'set_window_text(AT(object, 0x15a0), PTR(PTR(object, 0x17cc), 0x20));',
     'set_window_text(AT(object, 0x1538), PTR(PTR(object, 0x17cc), 0x20));'),
    ('save-populate-check', '(int32_t)D32(PTR(object, 0x17cc), 0x30));', '(int32_t)D32(PTR(object, 0x17cc), 0x30) & 1);'),
    ('save-read-room', '(int32_t)D32(descriptor, 0x10));', '(int32_t)D32(descriptor, 0x10) - 1);'),
    ('save-read-reload', '    descriptor = PTR(object, 0x17cc);\n    SERVICES->get_window_text(AT(object, 0x15a0)',
     '    SERVICES->get_window_text(AT(object, 0x15a0)'),
    ('save-read-default', 'D32(PTR(object, 0x17cc), 0x30) = D32(object, 0x17c0);', 'D32(PTR(object, 0x17cc), 0x30) = D32(object, 0x17bc);'),
    ('save-validate-zero', '    if (result == 0) return 0;', '    if (result == 0) return 1;'),
    ('save-validate-name', '    if (D32(descriptor, 8) != 0) return 0xcc8;', '    if (D32(descriptor, 8) != 0) return 0xcbd;'),
    ('save-validate-order', '    if (D32(descriptor, 8) != 0) return 0xcc8;\n    if (D32(descriptor, 0x1c) != 0) return 0xcbd;',
     '    if (D32(descriptor, 0x1c) != 0) return 0xcbd;\n    if (D32(descriptor, 8) != 0) return 0xcc8;'),
    ('save-validate-own-answer', '    return result;\n}', '    return 0;\n}'),
    ('save-validate-reload', '    descriptor = PTR(object, 0x17cc);\n    if (D32(descriptor, 8)', '    if (D32(descriptor, 8)'),
    ('save-error-description-zero', '        if (status == 0) return 0;', '        if (status == 0) return 1;'),
    ('save-error-description-code', '? 0xcbd : 0;', '? 0xcc8 : 0;'),
    ('save-error-exists', 'if (status != 0x1004)', 'if (status != 0x1003)'),
    ('save-error-name-code', '? 0xcc8 : 0;\n    if (SERVICES', '? 0xcc9 : 0;\n    if (SERVICES'),
    ('save-error-same-name', "PTR(descriptor, 0xc)) == 0) return 0;", "PTR(descriptor, 0xc)) != 0) return 0;"),
    ('save-error-fields', '    D32(object, 0x94) = 2;', '    D32(object, 0x94) = 3;'),
    ('save-error-reload', 'AT(object, 0x1538), D32(PTR(object, 0x17cc), 8));', 'AT(object, 0x1538), D32(descriptor, 8));'),
    ('save-error-answer-field', '    D32(object, 0x8c) = answer == 0;', '    D32(object, 0x8c) = answer != 0;'),
    ('save-error-forget', '        SERVICES->copy_text((char *)AT(object, 0x10dc), fs98_flights_empty_text);\n        return 0xcc8;',
     '        return 0xcc8;'),
    ('save-error-remember-room', '(char *)AT(object, 0x10dc), 0xff);', '(char *)AT(object, 0x10dc), 0x100);'),
    ('save-error-focus', 'post_message(D32(object, 0x20), 0x5fa, D32(object, 0x15c0), 0);', 'post_message(D32(object, 0x20), 0x5fa, D32(object, 0x1558), 0);'),
    ('save-accept-vtable', '    if (failure != 0 && ((uint32_t (TC *)(void *, uint32_t))vtable[0xdc / 4])',
     '    vtable = PTR(object, 0);\n    if (failure != 0 && ((uint32_t (TC *)(void *, uint32_t))vtable[0xdc / 4])'),
    ('save-accept-handled', '(object, failure) != 0)\n        return;', '(object, failure) == 0)\n        return;'),
    ('save-accept-argument', 'vtable[0xd8 / 4])(object, 0);', 'vtable[0xd8 / 4])(object, 1);'),
    ('save-accept-answer', '    D32(PTR(object, 0x17cc), 0) = 1;', '    D32(PTR(object, 0x17cc), 0) = 2;'),
    ('save-cancel-answer', '    descriptor[0] = 0;', '    descriptor[0] = 1;'),
    ('save-init-skin', 'object, 0x14f, module, 0xf)', 'object, 0x14f, module, 7)'),
    ('save-init-button-position', 'module, 0, 0, empty);', 'module, 1, 1, empty);'),
    ('save-init-check-bitmap', 'AT(object, 0x4c0), 0x139,', 'AT(object, 0x4c0), 0x13a,'),
    ('save-init-edit-label', 'AT(object, 0x13c8), 0);', 'AT(object, 0x1480), 0);'),
    ('save-init-limit', 'EM_LIMITTEXT, 0x104, 0);', 'EM_LIMITTEXT, 0x103, 0);'),
    ('save-init-buttons', '    PTR(object, 0x11e0) = AT(object, 0x1608);', '    PTR(object, 0x11e0) = AT(object, 0x11e4);'),
]
COMMS = [
    ('comms-land-check', "    SERVICES->set_checked(AT(object, 0x1e60), 0);\n    SERVICES->enable_control(AT(object, 0x1e60), 0);\n    SERVICES->post_message(D32(object, 0x20), WM_FOCUS_NEXT, D32(object, 0x1d24), 0);",
     "    SERVICES->enable_control(AT(object, 0x1e60), 0);\n    SERVICES->post_message(D32(object, 0x20), WM_FOCUS_NEXT, D32(object, 0x1d24), 0);"),
    ('comms-none-enable', 'SERVICES->enable_control(AT(object, 0x1e60), 1);', 'SERVICES->enable_control(AT(object, 0x1e60), 0);'),
    ('comms-take-off-focus', 'WM_FOCUS_NEXT, D32(object, 0x1c30), 0);\n}', 'WM_FOCUS_NEXT, D32(object, 0x1b3c), 0);\n}'),
    ('comms-cancel-answer', '    descriptor[0] = 0;', '    descriptor[0] = 1;'),
    ('comms-read-land', 'D32(DESCRIPTOR(object), 0x34) = 2;', 'D32(DESCRIPTOR(object), 0x34) = 1;'),
    ('comms-read-reload', '    if (D32(object, 0x1d24) == (uint32_t)data) {', '    if (D32(object, 0x1c30) == (uint32_t)data) {'),
    ('comms-read-index', 'get_window_long(D32(object, 0x1c30), GWL_USERDATA);', 'get_window_long(D32(object, 0x1c30), -16);'),
    ('comms-read-none', '        else D32(DESCRIPTOR(object), 0x34) = 0;', '        else D32(DESCRIPTOR(object), 0x34) = 3;'),
    ('comms-read-slow', 'D32(DESCRIPTOR(object), 0x2c) = D32(object, 0x1f1c);', 'D32(DESCRIPTOR(object), 0x2c) = D32(object, 0x1f18);'),
    ('comms-read-room', 'PTR(DESCRIPTOR(object), 0x10), 0xff);', 'PTR(DESCRIPTOR(object), 0x10), 0x100);'),
    ('comms-read-rate', 'D32(DESCRIPTOR(object), 0x3c) = (uint32_t)position;', 'D32(DESCRIPTOR(object), 0x38) = (uint32_t)position;'),
    ('comms-accept-read-first', '    fs98_flights_comms_read(object);\n    vtable = PTR(object, 0);', '    vtable = PTR(object, 0);\n    fs98_flights_comms_read(object);'),
    ('comms-accept-handled', '(object, failure) != 0)\n        return;', '(object, failure) == 0)\n        return;'),
    ('comms-accept-answer', '    D32(DESCRIPTOR(object), 0) = 1;', '    D32(DESCRIPTOR(object), 0) = 2;'),
    ('comms-populate-none-radio', "        SERVICES->set_radio(AT(object, 0x1b1c), 1);", "        SERVICES->set_radio(AT(object, 0x1b1c), 2);"),
    ('comms-populate-none-slow', "(int32_t)D32(DESCRIPTOR(object), 0x2c));\n        break;", "0);\n        break;"),
    ('comms-populate-take-off', 'SERVICES->set_radio(AT(object, 0x1c10), !land);', 'SERVICES->set_radio(AT(object, 0x1c10), land);'),
    ('comms-populate-disable', "        SERVICES->enable_control(AT(object, 0x1e60), 0);\n        break;", "        break;"),
    ('comms-populate-default', '    default:\n        break;', '    default:\n        SERVICES->set_radio(AT(object, 0x1b1c), 1);\n        break;'),
    ('comms-populate-focus', 'uint32_t focus = SERVICES->get_focus();', 'uint32_t focus = 0; (void)SERVICES->get_focus;'),
    ('comms-populate-rate', '(int32_t)D32(DESCRIPTOR(object), 0x3c));', '(int32_t)D32(DESCRIPTOR(object), 0x34));'),
    ('comms-validate-status', 'if (D32(DESCRIPTOR(object), 0xc) != 0) failure = 0xcb3;', 'if (D32(descriptor, 0xc) != 0) failure = 0xcb3;'),
    ('comms-validate-code', 'failure = 0xcb3;', 'failure = 0xcb4;'),
    ('comms-validate-control', '    if (control == 0xcb3) {', '    if (failure == 0xcb3) {'),
    ('comms-validate-room', 'text, 0x3f);', 'text, 0x40);'),
    ('comms-validate-format', 'format_number(text, s_number_format, value);', 'format_number(text, "%.2f", value);'),
    ('comms-error-pointer', '(uint32_t)(uintptr_t)status))', '*status))'),
    ('comms-error-code', '        handled = 0xcb3;', '        handled = 0xcb4;'),
    ('comms-error-slot', 'PTR(PTR(object, 0), 0xe0))(object, handled);', 'PTR(PTR(object, 0), 0xdc))(object, handled);'),
    ('comms-init-slider', '6, 0x1a, 6, 8, 0, -1, module);', '6, 0x1a, 6, 8, 0, 0, module);'),
    ('comms-init-range', 'slider_range(AT(object, 0x169c), 0, 0xe);', 'slider_range(AT(object, 0x169c), 0, 0xf);'),
    ('comms-init-radio-order', 'radios[] = {0x1d04, 0x1b1c, 0x1c10};', 'radios[] = {0x1b1c, 0x1c10, 0x1d04};'),
    ('comms-init-edit-flags', 'AT(object, 0x15e4), 5);', 'AT(object, 0x15e4), 0);'),
    ('comms-init-group-font', 'init_group(AT(object, 0x1f28), AT(object, 0x4c0));', 'init_group(AT(object, 0x1f28), AT(object, 0x220));'),
    ('comms-init-limit', 'EM_LIMITTEXT, D32(DESCRIPTOR(object), 0x14) - 1, 0);', 'EM_LIMITTEXT, D32(DESCRIPTOR(object), 0x14), 0);'),
]
CREATE = [
    ('create-refresh-null', '    if (DESCRIPTOR(object) == 0) return;\n    ready', '    if (DESCRIPTOR(object) == 0 && 0) return;\n    ready'),
    ('create-refresh-ready', 'ready = D32(DESCRIPTOR(object), 0x2c) != 0;', 'ready = D32(DESCRIPTOR(object), 0x28) != 0;'),
    ('create-refresh-save', 'SERVICES->enable_control(AT(object, 0x1b64), ready);', 'SERVICES->enable_control(AT(object, 0x1b64), 1);'),
    ('create-refresh-order', '    SERVICES->enable_control(AT(object, 0x1b64), ready);\n    SERVICES->enable_control(AT(object, 0x10e4), ready);',
     '    SERVICES->enable_control(AT(object, 0x10e4), ready);\n    SERVICES->enable_control(AT(object, 0x1b64), ready);'),
    ('create-refresh-summary', 'set_sims_text(AT(object, 0x144c), PTR(DESCRIPTOR(object), 0x24));', 'set_sims_text(AT(object, 0x144c), PTR(DESCRIPTOR(object), 0x28));'),
    ('create-save-slot', 'if (PTR(descriptor, 8)) ((fs98_flights_create_fn)PTR(descriptor, 8))(descriptor);', 'if (PTR(descriptor, 0xc)) ((fs98_flights_create_fn)PTR(descriptor, 0xc))(descriptor);'),
    ('create-save-refresh', '    if (PTR(descriptor, 8)) ((fs98_flights_create_fn)PTR(descriptor, 8))(descriptor);\n}',
     '    if (PTR(descriptor, 8)) ((fs98_flights_create_fn)PTR(descriptor, 8))(descriptor);\n    fs98_flights_create_refresh(object);\n}'),
    ('create-choose-missing', '    if (PTR(descriptor, slot)) ((fs98_flights_create_fn)', '    if (1) ((fs98_flights_create_fn)'),
    ('create-choose-refresh', '((fs98_flights_create_fn)PTR(descriptor, slot))(descriptor);\n    fs98_flights_create_refresh(object);',
     '((fs98_flights_create_fn)PTR(descriptor, slot))(descriptor);'),
    ('create-location-slot', 'fs98_flights_create_choose(object, 0x10); }', 'fs98_flights_create_choose(object, 0x14); }'),
    ('create-time-slot', 'fs98_flights_create_choose(object, 0x18); }', 'fs98_flights_create_choose(object, 0x1c); }'),
    ('create-cancel-state', 'D32(object, 0x1c60) = 2;', 'D32(object, 0x1c60) = 1;'),
    ('create-cancel-null', '    if (DESCRIPTOR(object)) D32(DESCRIPTOR(object), 0) = 0;', '    if (DESCRIPTOR(object)) D32(DESCRIPTOR(object), 0) = 2;'),
    ('create-accept-answer', '    if (DESCRIPTOR(object)) D32(DESCRIPTOR(object), 0) = 1;', '    if (DESCRIPTOR(object)) D32(DESCRIPTOR(object), 0) = 0;'),
    ('create-accept-state', 'D32(object, 0x1c60) = 1;', 'D32(object, 0x1c60) = 3;'),
    ('create-init-guard', '    if (!(s_state->guard & 1)) {', '    if (!(s_state->guard & 2)) {'),
    ('create-init-guard-set', '        s_state->guard |= 1;', '        s_state->guard |= 2;'),
    ('create-init-guard-order', '        s_state->guard |= 1;\n        SERVICES->font_construct(font, SERVICES->base_font(8));',
     '        SERVICES->font_construct(font, SERVICES->base_font(8));\n        s_state->guard |= 1;'),
    ('create-init-base-font', 'SERVICES->base_font(8)', 'SERVICES->base_font(2)'),
    ('create-init-exit', '        SERVICES->register_exit(fs98_flights_create_font_destroy);', '        (void)fs98_flights_create_font_destroy;'),
    ('create-init-font-color', '    D32(font, 8) = 0xff00;', '    D32(font, 8) = 0xff01;'),
    ('create-init-font-copy', '    D32(font, 0x50) = D32(object, 0x270);', '    D32(font, 0x50) = D32(object, 0x274);'),
    ('create-init-font-height', '*(uint16_t *)(font + 0x58) = 0x410;', '*(uint32_t *)(font + 0x58) = 0x410;'),
    ('create-init-picture', '0xf100);', '0xf101);'),
    ('create-init-choice-bitmap', 'fs98_flights_create_button(object, 0x1870, 0x680, 0x6f0, 0x137);', 'fs98_flights_create_button(object, 0x1870, 0x680, 0x6f0, 0x132);'),
    ('create-init-button-order', '    fs98_flights_create_button(object, 0x1b64, 0x680, 0x6f0, 0x137);\n    fs98_flights_create_button(object, 0x1774, 0x680, 0x6f0, 0x137);',
     '    fs98_flights_create_button(object, 0x1774, 0x680, 0x6f0, 0x137);\n    fs98_flights_create_button(object, 0x1b64, 0x680, 0x6f0, 0x137);'),
    ('create-init-text-font', 'SERVICES->init_text(AT(object, 0x1504), font);', 'SERVICES->init_text(AT(object, 0x1504), AT(object, 0x370));'),
    ('create-init-focus', 'SERVICES->set_focus(AT(object, 0x1774));', 'SERVICES->set_focus(AT(object, 0x10e4));'),
    ('create-init-clear', '    D32(object, 0x10dc) = 0;\n    return 0;', '    return 0;'),
    ('create-init-return', '    D32(object, 0x10dc) = 0;\n    return 0;', '    D32(object, 0x10dc) = 0;\n    return 1;'),
    ('create-font-destroy', 'SERVICES->font_destroy(s_state->font); }', 'SERVICES->font_destroy(s_state->font + 4); }'),
]
AIRCRAFT = [
    ('aircraft-record-stride', 'D32(descriptor, 0x14) * (uint32_t)0xa08);', 'D32(descriptor, 0x14) * (uint32_t)0xa00);'),
    ('aircraft-preview-operation-close', '        fs98_flights_aircraft_preview(object, 2);\n    }\n    D32(object, 0x1860) = 2;', '        fs98_flights_aircraft_preview(object, 3);\n    }\n    D32(object, 0x1860) = 2;'),
    ('aircraft-cancel-state', 'D32(object, 0x1860) = 2;', 'D32(object, 0x1860) = 1;'),
    ('aircraft-cancel-answer', '        D32(DESCRIPTOR(object), 0) = 0;', '        D32(DESCRIPTOR(object), 0) = 1;'),
    ('aircraft-cancel-null', '    if (DESCRIPTOR(object)) {\n        D32(DESCRIPTOR(object), 0) = 0;', '    if (1) {\n        D32(DESCRIPTOR(object), 0) = 0;'),
    ('aircraft-accept-answer', '        D32(DESCRIPTOR(object), 0) = 1;', '        D32(DESCRIPTOR(object), 0) = 2;'),
    ('aircraft-accept-order', '        D32(DESCRIPTOR(object), 0) = 1;\n        selection = fs98_flights_aircraft_selection(object);',
     '        selection = fs98_flights_aircraft_selection(object);\n        D32(DESCRIPTOR(object), 0) = 1;'),
    ('aircraft-accept-state', 'D32(object, 0x1860) = 1;', 'D32(object, 0x1860) = 0;'),
    ('aircraft-frame-operation', '(3, descriptor, 0);', '(1, descriptor, 0);'),
    ('aircraft-frame-return', '    return 0;\n}\n\n/* 0x22311f40', '    return 1;\n}\n\n/* 0x22311f40'),
    ('aircraft-populate-signed', 'index < (int32_t)D32(DESCRIPTOR(object), 0x18);', '(uint32_t)index < D32(DESCRIPTOR(object), 0x18);'),
    ('aircraft-populate-string-first', '    fs98_flights_string_enter(&title);\n    SERVICES->send_message(D32(object, 0x151c), LB_SETCURSEL',
     '    SERVICES->send_message(D32(object, 0x151c), LB_SETCURSEL'),
    ('aircraft-show-second-text', '(char *)fs98_flights_aircraft_record(object) + 0x500);', '(char *)fs98_flights_aircraft_record(object) + 0x400);'),
    ('aircraft-show-title-index', 'list_text(AT(object, 0x14fc), (int32_t)D32(DESCRIPTOR(object), 0x14), title);', 'list_text(AT(object, 0x14fc), 0, title);'),
    ('aircraft-show-title-edit', 'set_window_text(AT(object, 0x1544), title->text);', 'set_window_text(AT(object, 0x15ac), title->text);'),
    ('aircraft-show-details', 'D32(fs98_flights_aircraft_record(object), 0xa04));\n}', 'D32(fs98_flights_aircraft_record(object), 0xa00));\n}'),
    ('aircraft-select-null', '    if (DESCRIPTOR(object) == 0) return;\n    fs98_flights_string_enter(&title);', '    fs98_flights_string_enter(&title);'),
    ('aircraft-select-order', '    fs98_flights_string_enter(&title);\n    selection = fs98_flights_aircraft_selection(object);',
     '    selection = fs98_flights_aircraft_selection(object);\n    fs98_flights_string_enter(&title);'),
    ('aircraft-double-click-slot', '    fs98_flights_aircraft_select(object);\n    ((void (TC *)(void *))PTR(PTR(object, 0), 0xcc))(object);',
     '    fs98_flights_aircraft_select(object);\n    ((void (TC *)(void *))PTR(PTR(object, 0), 0xd0))(object);'),
    ('aircraft-details-answer', 'PTR(descriptor, 8))(descriptor) == 0) {', 'PTR(descriptor, 8))(descriptor) != 0) {'),
    ('aircraft-details-focus', 'SERVICES->set_focus(AT(object, 0x1544));', 'SERVICES->set_focus(AT(object, 0x14fc));'),
    ('aircraft-details-refill', '        ((void (TC *)(void *))PTR(PTR(object, 0), 0xf0))(object);\n        selection',
     '        selection'),
    ('aircraft-init-vtable', '    SERVICES->base_init(object);\n    vtable = PTR(object, 0);', '    vtable = PTR(object, 0);\n    SERVICES->base_init(object);'),
    ('aircraft-init-style', 'D32(object, 0x84) = 0xb;', 'D32(object, 0x84) = 0xa;'),
    ('aircraft-init-details-button', '0x137,\n                          AT(object, 0xff4)', '0x132,\n                          AT(object, 0xff4)'),
    ('aircraft-init-edit-error', '0xffffff, 0, 0xffffff, AT(object, label), 0);', '0xffffff, 0, 0xff0000, AT(object, label), 0);'),
    ('aircraft-init-list-label', 'AT(object, 0x450), AT(object, 0x12d4));', 'AT(object, 0x450), AT(object, 0x138c));'),
    ('aircraft-init-limit', 'EM_LIMITTEXT, 0x100, 0);', 'EM_LIMITTEXT, 0xff, 0);'),
    ('aircraft-init-item', 'dialog_item(object, 0xc87);', 'dialog_item(object, 0xc86);'),
    ('aircraft-init-width', 'rect.right - rect.left, rect.bottom - rect.top, 1);', 'rect.right, rect.bottom - rect.top, 1);'),
    ('aircraft-init-callback-early', '((fs98_flights_aircraft_preview_fn)PTR(early, 0x10))', '((fs98_flights_aircraft_preview_fn)PTR(DESCRIPTOR(object), 0x10))'),
    ('aircraft-init-open', '(1, descriptor, window);', '(1, early, window);'),
    ('aircraft-init-focus', 'SERVICES->set_focus(AT(object, 0x14fc));\n    return 0;', 'SERVICES->set_focus(AT(object, 0x1544));\n    return 0;'),
    ('aircraft-init-return', 'SERVICES->set_focus(AT(object, 0x14fc));\n    return 0;', 'SERVICES->set_focus(AT(object, 0x14fc));\n    return 1;'),
]
OPENING = [
    # shared by the three menus
    ('menu-font-guard', '    if (*guard & bit) return;', '    if (*guard & 1) return;'),
    ('menu-font-guard-order', '    *guard |= bit;\n    SERVICES->font_construct(font, SERVICES->base_font(9));',
     '    SERVICES->font_construct(font, SERVICES->base_font(9));\n    *guard |= bit;'),
    ('menu-font-base', 'SERVICES->base_font(9)', 'SERVICES->base_font(8)'),
    ('menu-font-shadow-colour', 'D32(fonts->shadow, 0x18) = 0xff00;', 'D32(fonts->shadow, 0x14) = 0xff00;'),
    ('menu-font-alternate-flag', 'D32(fonts->alternate, 0x48) = 1;', 'D32(fonts->alternate, 0x48) = 0;'),
    ('menu-font-hint-copy', 'D32(fonts->hint, 0xc) = D32(object, 0x22c);', 'D32(fonts->hint, 0xc) = D32(object, 0x228);'),
    ('menu-font-exit', 'fs98_flights_menu_font(fonts->guard, fonts->hint, 4, fonts->hint_destroy);',
     'fs98_flights_menu_font(fonts->guard, fonts->hint, 4, fonts->shadow_destroy);'),
    ('menu-choose-missing', '    if (PTR(PTR(object, descriptor_at), button->slot) == 0 || BUSY(object) != 0) return;', '    if (BUSY(object) != 0) return;'),
    ('menu-choose-busy', '    if (PTR(PTR(object, descriptor_at), button->slot) == 0 || BUSY(object) != 0) return;',
     '    if (PTR(PTR(object, descriptor_at), button->slot) == 0) return;'),
    ('menu-choose-busy-first', '    BUSY(object) = 1;\n    for (index = 0; index < button->count; ++index)', '    for (index = 0; index < button->count; ++index)'),
    ('menu-choose-answer', '        D32(PTR(object, descriptor_at), 0) = 1;', '        D32(PTR(object, descriptor_at), 0) = 2;'),
    ('menu-choose-save', '        if (save) save(object);', '        (void)save;'),
    ('menu-choose-stay-busy', '        VCALL(object, 0xec);\n        return;\n    }', '        VCALL(object, 0xec);\n        BUSY(object) = 0;\n        return;\n    }'),
    ('menu-choose-not-busy', '    if (button->focus) SERVICES->set_focus(AT(object, button->focus));\n    BUSY(object) = 0;', '    if (button->focus) SERVICES->set_focus(AT(object, button->focus));'),
    ('menu-choose-focus', '    if (button->focus) SERVICES->set_focus(AT(object, button->focus));', '    (void)0;'),
    ('menu-choose-focus-order', '    for (index = 0; index < all_count; ++index) SERVICES->enable_control(AT(object, all[index]), 1);\n    if (button->focus) SERVICES->set_focus(AT(object, button->focus));',
     '    if (button->focus) SERVICES->set_focus(AT(object, button->focus));\n    for (index = 0; index < all_count; ++index) SERVICES->enable_control(AT(object, all[index]), 1);'),
    ('menu-hover-unchanged', '        if (*shown != hints[index].id) {', '        if (1) {'),
    ('menu-hover-clear', '    } else if (*shown != 0) {', '    } else if (1) {'),
    ('menu-hover-focus-once', '    for (index = 0; index < count; ++index)\n        if (SERVICES->window_from_handle(SERVICES->get_focus()) == AT(object, hints[index].button))\n            break;',
     '    {\n        void *focused = SERVICES->window_from_handle(SERVICES->get_focus());\n        for (index = 0; index < count; ++index)\n            if (focused == AT(object, hints[index].button)) break;\n    }'),
    ('menu-hover-return', '    fs98_flights_unwind_leave(&hint.frame);\n    return 0;', '    fs98_flights_unwind_leave(&hint.frame);\n    return 1;'),
    ('menu-shadow-flags', '0x42, 0xffffffff, fs98_flights_resource_module(), 0, 0, empty);', '0x40, 0xffffffff, fs98_flights_resource_module(), 0, 0, empty);'),
    ('menu-button-position', 'AT(object, 0x7e8), 1, 0xffffffff, fs98_flights_resource_module(), 1, 1, empty);', 'AT(object, 0x7e8), 1, 0xffffffff, fs98_flights_resource_module(), 0, 0, empty);'),
    # the welcome dialog
    ('opening-save-field', 'D32(WELCOME_DESCRIPTOR(object), 0x20) = D32(object, 0x119c);', 'D32(WELCOME_DESCRIPTOR(object), 0x20) = D32(object, 0x1198);'),
    ('opening-refresh-available', 'if (D32(WELCOME_DESCRIPTOR(object), 0x18) == 0) SERVICES', 'if (D32(WELCOME_DESCRIPTOR(object), 0x18) != 0) SERVICES'),
    ('opening-refresh-check', '(int32_t)D32(WELCOME_DESCRIPTOR(object), 0x20));', '(int32_t)D32(WELCOME_DESCRIPTOR(object), 0x1c));'),
    ('opening-reenable-multiplayer', 's_welcome_all[] = {0x1484, 0x129c, 0x1578, 0x11a8, 0x1390};', 's_welcome_all[] = {0x1390, 0x129c, 0x1578, 0x11a8, 0x1390};'),
    ('opening-multiplayer-others', 'button = {0x14, 4, {0x129c, 0x1578, 0x11a8, 0x1390}, 0};', 'button = {0x14, 4, {0x1484, 0x1578, 0x11a8, 0x1390}, 0};'),
    ('opening-lessons-slot', 'button = {0xc, 4, {0x1484, 0x1578, 0x11a8, 0x1390}, 0};', 'button = {0x10, 4, {0x1484, 0x1578, 0x11a8, 0x1390}, 0};'),
    ('opening-more-slot', 'button = {8, 4, {0x1484, 0x129c, 0x1578, 0x1390}, 0};', 'button = {0xc, 4, {0x1484, 0x129c, 0x1578, 0x1390}, 0};'),
    ('opening-fly-answer', 'fs98_flights_opening_finish(object, 1, 0); }', 'fs98_flights_opening_finish(object, 1, 1); }'),
    ('opening-quit-answer', 'fs98_flights_opening_finish(object, 0, 1); }', 'fs98_flights_opening_finish(object, 1, 1); }'),
    ('opening-finish-busy', '    if (BUSY(object) != 0) return;\n    D32(WELCOME_DESCRIPTOR(object), 0) = answer;', '    D32(WELCOME_DESCRIPTOR(object), 0) = answer;'),
    ('opening-finish-save', '    D32(WELCOME_DESCRIPTOR(object), 0x1c) = quit;\n    fs98_flights_opening_save(object);', '    D32(WELCOME_DESCRIPTOR(object), 0x1c) = quit;'),
    ('opening-hover-text', '{0x129c, 0x1f6, 0xf700}', '{0x129c, 0x1f6, 0xf701}'),
    ('opening-hover-id', '{0x1390, 0x1f9, 0xf6fe}', '{0x1390, 0x1f7, 0xf6fe}'),
    ('opening-init-skin', 'fs98_flights_menu_skin(object, 0x13e, 1)', 'fs98_flights_menu_skin(object, 0x13e, 2)'),
    ('opening-init-shadow-order', 'shadows[] = {0x11a8, 0x1484, 0x129c, 0x1390};', 'shadows[] = {0x1484, 0x11a8, 0x129c, 0x1390};'),
    ('opening-init-check-font', 'check, AT(object, 0x4c0), 0x139,', 'check, AT(object, 0x370), 0x139,'),
    ('opening-init-not-busy', '    BUSY(object) = 0;\n    SERVICES->set_focus(AT(object, 0x1578));', '    SERVICES->set_focus(AT(object, 0x1578));'),
    ('opening-init-focus', 'SERVICES->set_focus(AT(object, 0x1578));\n    return 0;', 'SERVICES->set_focus(AT(object, 0x1674));\n    return 0;'),
    # the sub-menus
    ('menu-back-busy', '    if (BUSY(object) != 0) return;\n    D32(PTR(object, 0x1658), 0) = 0;', '    D32(PTR(object, 0x1658), 0) = 0;'),
    ('menu-back-answer', '    D32(PTR(object, 0x1658), 0) = 0;', '    D32(PTR(object, 0x1658), 0) = 1;'),
    ('flight-menu-all', 's_flight_all[] = {0x12c4, 0x14ac, 0x13b8, 0x11c8};', 's_flight_all[] = {0x12c4, 0x14ac, 0x13b8};'),
    ('flight-menu-dual-others', 'button = {8, 3, {0x14ac, 0x13b8, 0x11c8}, 0};', 'button = {8, 2, {0x14ac, 0x13b8, 0x11c8}, 0};'),
    ('flight-menu-video-slot', 'button = {0x14, 3, {0x12c4, 0x13b8, 0x11c8}, 0};', 'button = {0x10, 3, {0x12c4, 0x13b8, 0x11c8}, 0};'),
    ('flight-menu-hover-text', '{0x14ac, 0xc81, 0xf706}', '{0x14ac, 0xc81, 0xf705}'),
    ('flight-menu-hover-static', 'return fs98_flights_menu_hover(object, hints, 3, &s_menus->flight_shown, 0x15a0);', 'return fs98_flights_menu_hover(object, hints, 3, &s_menus->learning_shown, 0x15a0);'),
    ('learning-menu-all', 's_learning_all[] = {0x12c4, 0x14ac, 0x13b8};', 's_learning_all[] = {0x12c4, 0x13b8, 0x14ac};'),
    ('learning-menu-focus', 'button = {0x10, 2, {0x12c4, 0x13b8}, 0x14ac};', 'button = {0x10, 2, {0x12c4, 0x13b8}, 0x12c4};'),
    ('learning-menu-quit-slot', 'button = {0xc, 2, {0x12c4, 0x14ac}, 0x13b8};', 'button = {0x8, 2, {0x12c4, 0x14ac}, 0x13b8};'),
    ('learning-menu-hover-id', '{0x13b8, 0xc84, 0xf704}', '{0x13b8, 0xc85, 0xf704}'),
    ('submenu-init-flags', 'if (!fs98_flights_menu_skin(object, skin, 7)) return 0;', 'if (!fs98_flights_menu_skin(object, skin, 1)) return 0;'),
    ('submenu-init-art', 'SERVICES->init_art(AT(object, 0x10e0), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);', 'SERVICES->init_art(AT(object, 0x10e0), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 1);'),
    ('submenu-init-hint-font', 'SERVICES->init_text(AT(object, 0x15a0), fonts->hint);', 'SERVICES->init_text(AT(object, 0x15a0), fonts->shadow);'),
    ('submenu-init-return', '    BUSY(object) = 0;\n    return 1;', '    BUSY(object) = 0;\n    return 0;'),
    ('flight-menu-skin', 'fs98_flights_submenu_init(object, 0x155, &fonts);', 'fs98_flights_submenu_init(object, 0x156, &fonts);'),
    ('learning-menu-fonts', '&s_menus->learning_guard, s_menus->learning_shadow, s_menus->learning_alternate, s_menus->learning_hint,',
     '&s_menus->learning_guard, s_menus->learning_shadow, s_menus->learning_hint, s_menus->learning_alternate,'),
    # the modeless dialog
    ('modeless-init-skin', 'fs98_flights_menu_skin(object, 0x140, 1) != 0; }', 'fs98_flights_menu_skin(object, 0x140, 1); }'),
    ('modeless-check-flag', 'if (D32(PTR(object, 0x10dc), 8) == 0) {', 'if (D32(PTR(object, 0x10dc), 8) != 0) {'),
    ('modeless-check-vtable', '        ((void (TC *)(void *))vtable[0x60 / 4])(object);', '        ((void (TC *)(void *))PTR(PTR(object, 0), 0x60))(object);'),
    ('modeless-check-order', '        ((void (TC *)(void *))vtable[0xec / 4])(object);\n        ((void (TC *)(void *))vtable[0x60 / 4])(object);',
     '        ((void (TC *)(void *))vtable[0x60 / 4])(object);\n        ((void (TC *)(void *))vtable[0xec / 4])(object);'),
    ('modeless-palette-flags', 'redraw_window(D32(object, 0x20), 0, 0, 0x105);', 'redraw_window(D32(object, 0x20), 0, 0, 0x101);'),
]
SELECT = [
    ('select-cancel-answer', '    descriptor[0] = 0;', '    descriptor[0] = 1;'),
    ('select-accept-default', 'D32(DESCRIPTOR(object), 0x10) = D32(object, 0x1800);', 'D32(DESCRIPTOR(object), 0x10) = D32(object, 0x17fc);'),
    ('select-accept-handled', '(object, failure) != 0)\n        return;', '(object, failure) == 0)\n        return;'),
    ('select-choose-itemdata', 'LB_GETITEMDATA,\n                                                                    D32(DESCRIPTOR(object), 0x14), 0);',
     'LB_GETITEMDATA,\n                                                                    0, 0);'),
    ('select-choose-description', '(char *)record + 0x100);\n    SERVICES->send_message', '(char *)record + 0x80);\n    SERVICES->send_message'),
    ('select-choose-readonly', 'EM_SETREADONLY, D32(record, 0xa04) == 0, 0);', 'EM_SETREADONLY, D32(record, 0xa04) != 0, 0);'),
    ('select-choose-delete-flag', '    SERVICES->enable_control(AT(object, 0x1648), (int32_t)D32(record, 0xa04));', '    SERVICES->enable_control(AT(object, 0x1648), 1);'),
    ('select-choose-protected', 'if (D32(DESCRIPTOR(object), 0x14) == D32(DESCRIPTOR(object), 0x20)) {', 'if (D32(DESCRIPTOR(object), 0x14) == D32(DESCRIPTOR(object), 0x18)) {'),
    ('select-choose-protected-title', '        SERVICES->enable_control(AT(object, 0x15e0), 0);\n        SERVICES->enable_control(AT(object, 0x1648), 0);\n    }',
     '        SERVICES->enable_control(AT(object, 0x1648), 0);\n    }'),
    ('select-double-click-slot', '    fs98_flights_select_choose(object);\n    ((void (TC *)(void *))PTR(PTR(object, 0), 0xcc))(object);',
     '    fs98_flights_select_choose(object);\n    ((void (TC *)(void *))PTR(PTR(object, 0), 0xd0))(object);'),
    ('select-populate-base-once', 'index < D32(DESCRIPTOR(object), 0x18); ++index, record += 0xa08) {', 'index < D32(DESCRIPTOR(object), 0x18); ++index, record = (uint8_t *)PTR(DESCRIPTOR(object), 0x1c) + index * 0xa08) {'),
    ('select-populate-itemdata', 'LB_SETITEMDATA, (uint32_t)item,', 'LB_SETITEMDATA, index,'),
    ('select-populate-fallback', '        D32(DESCRIPTOR(object), 0x14) = 0;\n    }', '    }'),
    ('select-populate-check', 'set_checked(AT(object, 0x1744), (int32_t)D32(DESCRIPTOR(object), 0x10));', 'set_checked(AT(object, 0x1744), (int32_t)D32(DESCRIPTOR(object), 0x14));'),
    ('select-remove-protected', 'if (D32(descriptor, 0x14) < D32(descriptor, 0x20)) D32(descriptor, 0x20) -= 1;', 'if (D32(descriptor, 0x14) <= D32(descriptor, 0x20)) D32(descriptor, 0x20) -= 1;'),
    ('select-remove-reload', '    descriptor = DESCRIPTOR(object);\n    if (D32(descriptor, 0x14) < D32(descriptor, 0x20))', '    if (D32(descriptor, 0x14) < D32(descriptor, 0x20))'),
    ('select-remove-focus', 'if (!SERVICES->window_enabled(AT(object, 0x1648))) SERVICES->set_focus(AT(object, 0x10e0));', 'if (SERVICES->window_enabled(AT(object, 0x1648))) SERVICES->set_focus(AT(object, 0x10e0));'),
    ('select-validate-enabled', '    if (!SERVICES->window_enabled(AT(object, 0x1648))) return 0;\n    if (control', '    if (control'),
    ('select-validate-control', 'if (control != 0 && control != 0xcbe) return 0;', 'if (control != 0 && control != 0xcbf) return 0;'),
    ('select-validate-title-room', 'get_window_text(AT(object, 0x15e0), (char *)record, 0xff);', 'get_window_text(AT(object, 0x15e0), (char *)record, 0x100);'),
    ('select-validate-cut', '    record[0x100] = 0;\n    D32(object, 0x10dc) = 0;', '    D32(object, 0x10dc) = 0;'),
    ('select-validate-same', 'RECORD(DESCRIPTOR(object))) != 0) {', 'RECORD(DESCRIPTOR(object))) == 0) {'),
    ('select-validate-status', '        if (control == 0 || D32(object, 0x10dc) != 0x1004) return 0xcbe;', '        if (D32(object, 0x10dc) != 0x1004) return 0xcbe;'),
    ('select-validate-clear-title', '        record[0] = 0;\n        ((fs98_flights_select_edit_fn)PTR(descriptor, 0xc))(descriptor, record);\n    }\n    return 0;',
     '        ((fs98_flights_select_edit_fn)PTR(descriptor, 0xc))(descriptor, record);\n    }\n    return 0;'),
    ('select-validate-description-room', 'get_window_text(AT(object, 0x1578), (char *)record + 0x100, 0x3ff);\n    {', 'get_window_text(AT(object, 0x1578), (char *)record + 0x100, 0x400);\n    {'),
    ('select-error-zero', '    if (code == 0) return 0;', '    if (code == 0) return 1;'),
    ('select-error-exists', '    if (status == 0x1004) {', '    if (status == 0x1003) {'),
    ('select-error-fields', '        D32(object, 0x94) = 2;', '        D32(object, 0x94) = 1;'),
    ('select-error-agree', 'if (SERVICES->error_dialog(object, AT(object, 0x15e0), status) != 0) {', 'if (SERVICES->error_dialog(object, AT(object, 0x15e0), status) == 0) {'),
    ('select-error-slot', 'PTR(PTR(object, 0), 0xe0))(object, code);', 'PTR(PTR(object, 0), 0xe0))(object, status);'),
    ('select-init-limits', '    D32(object, 0x1644) = 0xff;', '    D32(object, 0x1644) = 0x100;'),
    ('select-init-edit-flags', 'AT(object, 0x1390), 0x10);', 'AT(object, 0x1390), 0);'),
    ('select-init-list-label', 'init_list(AT(object, 0x1530), AT(object, 0x450), 0);', 'init_list(AT(object, 0x1530), AT(object, 0x450), AT(object, 0x1390));'),
    ('select-init-ok-position', 'AT(object, 0x10e0), AT(object, 0x5a0), AT(object, 0x610), 0x132,\n                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff, module, 0, 0, empty);',
     'AT(object, 0x10e0), AT(object, 0x5a0), AT(object, 0x610), 0x132,\n                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff, module, 1, 1, empty);'),
    ('select-init-focus', 'SERVICES->set_focus(AT(object, 0x1530));', 'SERVICES->set_focus(AT(object, 0x15e0));'),
    ('select-init-null', '    if (DESCRIPTOR(object)) {\n        fs98_flights_select_populate(object);', '    {\n        fs98_flights_select_populate(object);'),
]
UNWIND = [
    ('opening-hint-cleanup', 'opening', "    if (state->live) { state->live = 0; SERVICES->string_destroy(&state->string); }", "    (void)state;"),
    ('menu-constructor-busy', 'objects', '        D32(object, 0x10dc) = 0;', '        (void)0;'),
    ('aircraft-details-string', 'aircraft', '    fs98_flights_string_enter(&unused);\n', '    fs98_flights_string_enter(&unused);\n    unused.live = 0;\n'),
    ('aircraft-string-cleanup', 'aircraft', "    if (state->live) { state->live = 0; SERVICES->string_destroy(&state->string); }", "    (void)state;"),
    ('save-init-populate-last', 'save', '    PTR(object, 0x11dc) = AT(object, 0x11e4);\n    PTR(object, 0x11e0) = AT(object, 0x1608);\n    fs98_flights_save_populate(object);',
     '    fs98_flights_save_populate(object);\n    PTR(object, 0x11dc) = AT(object, 0x11e4);\n    PTR(object, 0x11e0) = AT(object, 0x1608);'),
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
    directory = Path('bin/flights-diff/breaks')
    directory.mkdir(parents=True, exist_ok=True)
    breaks = [(name, 'events', 'normal', before, after) for name, before, after in EVENTS]
    breaks += [(name, 'objects', 'normal', before, after) for name, before, after in OBJECTS]
    breaks += [(name, 'save', 'normal', before, after) for name, before, after in SAVE]
    breaks += [(name, 'comms', 'normal', before, after) for name, before, after in COMMS]
    breaks += [(name, 'create', 'normal', before, after) for name, before, after in CREATE]
    breaks += [(name, 'aircraft', 'normal', before, after) for name, before, after in AIRCRAFT]
    breaks += [(name, 'opening', 'normal', before, after) for name, before, after in OPENING]
    breaks += [(name, 'select', 'normal', before, after) for name, before, after in SELECT]
    breaks += [(name, unit, 'unwind', before, after) for name, unit, before, after in UNWIND]
    if only:
        breaks = [b for b in breaks if b[0] in only]
    caught = 0
    for name, unit, mode, before, after in breaks:
        original = Path(f'src/flights/flights_{unit}.c').read_text()
        if original.count(before) != 1:
            raise ValueError(f'{name}: the anchor occurs {original.count(before)} times')
        source = directory / f'{name}.c'
        executable = directory / f'{name}.exe'
        source.write_text(original.replace(before, after, 1))
        executable.unlink(missing_ok=True)
        others = [f'src/flights/flights_{u}.c' for u in UNITS if u != unit]
        build = subprocess.run([
            'i686-w64-mingw32-gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Wno-unused',
            '-fno-strict-aliasing', '-fno-delete-null-pointer-checks', '-Isrc/flights', '-o', str(executable),
            'tools/difftest/flightseventsdiff.c', str(source), *others, '-lntdll'],
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
    print(f'Flights breaks: {caught}/{len(breaks)} caught', flush=True)
    return 0 if caught == len(breaks) else 1


if __name__ == '__main__':
    sys.exit(main())
