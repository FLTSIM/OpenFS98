#!/usr/bin/env python3
"""Require paired real x86 constructor/destructor exception cleanup."""
import mpdialog_ui_breaks as ui
from mpdialog_review_breaks import compare
from mpdialog_construction_breaks import replace_once


def main():
    session = (ui.ROOT / 'src/mpdialog/mpdialog_session.c').read_text()
    flow = (ui.ROOT / 'src/mpdialog/mpdialog_flow.c').read_text()
    details = (ui.ROOT / 'src/mpdialog/mpdialog_details.c').read_text()
    tables = (ui.ROOT / 'src/mpdialog/mpdialog_runtime_tables.inc').read_text()
    unwind = (ui.ROOT / 'src/mpdialog/mpdialog_unwind.h').read_text()
    with ui.isolated_wine_prefix():
        compare('module init exceptions baseline', ui.OUT / 'MP_DIALOG_ours.dll', ['--module-init-exceptions'])
        compare('constructor exceptions baseline', ui.OUT / 'MP_DIALOG_ours.dll', ['--constructor-exceptions'])
        compare('destructor exceptions baseline', ui.OUT / 'MP_DIALOG_ours.dll', ['--destructor-exceptions'])
        compare('typed constructor exceptions baseline', ui.OUT / 'MP_DIALOG_ours.dll', ['--constructor-exceptions', '--exception-cpp'])
        compare('typed destructor exceptions baseline', ui.OUT / 'MP_DIALOG_ours.dll', ['--destructor-exceptions', '--exception-cpp'])
        compare('ordinary construction baseline', ui.OUT / 'MP_DIALOG_ours.dll', ['--construction-only'])
        for family, source, page in [('flow', flow, 0), ('settings', session, 5), ('details', details, 6)]:
            for suffix, old, new in [
                ('base', '\n    state.base = 1;', '\n    state.base = 0;'),
                ('completed', 'state.completed = index + 1;', 'state.completed = index;'),
                ('allocation', 'state.object = object; state.live = 1;', 'state.object = object; state.live = 0;'),
            ]:
                name = f'exception-{family}-{suffix}'
                mutant = replace_once(source, old, new, name)
                dll = (ui.build_flow(name, mutant, tables) if family == 'flow' else
                       ui.build(name, mutant if family == 'settings' else session, tables,
                                mutant if family == 'details' else details))
                compare(name, dll, ['--constructor-exceptions'],
                        f'MP_DIALOG CONSTRUCTOR EXCEPTION DIFFER page={page} ')
        name = 'exception-unwind-disabled'
        mutant = replace_once(unwind,
            'if (record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND)) scope->cleanup(scope);',
            'if (0 && (record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) scope->cleanup(scope);', name)
        dll = ui.build(name, session, tables, details, mutant)
        compare(name, dll, ['--constructor-exceptions'], 'MP_DIALOG CONSTRUCTOR EXCEPTION DIFFER page=0 ')
        compare(name, dll, ['--destructor-exceptions'], 'MP_DIALOG DESTRUCTOR EXCEPTION DIFFER page=0 ')
        compare(name, dll, ['--module-init-exceptions'], 'MP_DIALOG MODULE INIT DIFFER case=3 ')


if __name__ == '__main__':
    main()
