#!/usr/bin/env python3
"""Require flag-0 destructor calls, returns and retained storage to match retail."""
import mpdialog_ui_breaks as ui
from mpdialog_review_breaks import compare
from mpdialog_construction_breaks import replace_once


def main():
    session = (ui.ROOT / 'src/mpdialog/mpdialog_session.c').read_text()
    flow = (ui.ROOT / 'src/mpdialog/mpdialog_flow.c').read_text()
    details = (ui.ROOT / 'src/mpdialog/mpdialog_details.c').read_text()
    tables = (ui.ROOT / 'src/mpdialog/mpdialog_runtime_tables.inc').read_text()
    args = ['--retained-destruction']
    with ui.isolated_wine_prefix():
        compare('retained destruction baseline', ui.OUT / 'MP_DIALOG_ours.dll', args)
        compare('ordinary destruction baseline', ui.OUT / 'MP_DIALOG_ours.dll', ['--construction-only'])
        for family, source, page in [('flow', flow, 0), ('settings', session, 5), ('details', details, 6)]:
            for suffix, old, new in [
                ('free', 'if (flags & 1) fs98_mpdialog_free(object);',
                 'if ((flags & 1) || flags == 0) fs98_mpdialog_free(object);'),
                ('return', 'if (flags & 1) fs98_mpdialog_free(object); return object;' if family == 'flow' else
                 'if (flags & 1) fs98_mpdialog_free(object);\n    return object;',
                 'if (flags & 1) fs98_mpdialog_free(object); return flags ? object : 0;' if family == 'flow' else
                 'if (flags & 1) fs98_mpdialog_free(object);\n    return flags ? object : 0;'),
            ]:
                name = f'retained-{family}-{suffix}'
                mutant = replace_once(source, old, new, name)
                dll = (ui.build_flow(name, mutant, tables) if family == 'flow' else
                       ui.build(name, mutant if family == 'settings' else session, tables,
                                mutant if family == 'details' else details))
                compare(name, dll, args, f'MP_DIALOG CONSTRUCTION DIFFER page={page} ')
        for name, old, new in [
            ('retained-flow-order',
             '            flow_call_control(control->dtor, object, control->offset);',
             '            flow_call_control(control->dtor, object, page == 0 && control->offset == 0x10dc ? 0x11d8 :\n'
             '                page == 0 && control->offset == 0x11d8 ? 0x10dc : control->offset);'),
            ('retained-flow-omitted-control',
             '    else if (control->dtor)',
             '    else if (control->dtor && !(page == 0 && control->offset == 0x10dc))'),
        ]:
            dll = ui.build_flow(name, replace_once(flow, old, new, name), tables)
            compare(name, dll, args, 'MP_DIALOG CONSTRUCTION DIFFER page=0 ')


if __name__ == '__main__':
    main()
