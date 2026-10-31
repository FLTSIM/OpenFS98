#!/usr/bin/env python3
"""Build MP_DIALOG mutants and prove the native retail comparison catches them."""
from pathlib import Path
import os
import contextlib
import tempfile
import signal
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'bin/mpdialog-diff'
WORK = OUT / 'ui-breaks'
CC = 'i686-w64-mingw32-gcc'
_private_wine_prefix = None
FLAGS = [
    '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
    '-fno-strict-aliasing', '-fno-delete-null-pointer-checks',
]


def run(command, **kwargs):
    return subprocess.run(command, check=True, text=True, **kwargs)


@contextlib.contextmanager
def isolated_wine_prefix():
    """Own a fresh Wine server; never stop a server from the caller's prefix."""
    OUT.mkdir(parents=True, exist_ok=True)
    global _private_wine_prefix
    if _private_wine_prefix is not None:
        raise RuntimeError('nested private Wine prefixes are not supported')
    previous = os.environ.get('WINEPREFIX')
    with tempfile.TemporaryDirectory(prefix='wine-mutation-', dir=OUT) as prefix:
        os.environ['WINEPREFIX'] = prefix
        _private_wine_prefix = prefix
        try:
            yield prefix
        finally:
            try:
                subprocess.run(['wineserver', '-k'], cwd=ROOT, env=os.environ.copy(),
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                               timeout=20, check=False)
                subprocess.run(['wineserver', '-w'], cwd=ROOT, env=os.environ.copy(),
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                               timeout=20, check=False)
            finally:
                _private_wine_prefix = None
                if previous is None:
                    os.environ.pop('WINEPREFIX', None)
                else:
                    os.environ['WINEPREFIX'] = previous


def run_wine(command, label, timeout=90):
    if _private_wine_prefix is None or os.environ.get('WINEPREFIX') != _private_wine_prefix:
        raise RuntimeError('Wine comparison requires its own private prefix')
    process = subprocess.Popen(command, cwd=ROOT, env=os.environ.copy(), text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               start_new_session=True)
    try:
        output, _ = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired as error:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        # The server belongs exclusively to this invocation, including its children.
        subprocess.run(['wineserver', '-k'], cwd=ROOT, env=os.environ.copy(),
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       timeout=20, check=False)
        try:
            output, _ = process.communicate(timeout=10)
        except subprocess.TimeoutExpired:
            process.stdout.close()
            process.wait(timeout=10)
            output = error.stdout or ''
        if isinstance(output, bytes):
            output = output.decode(errors='replace')
        phases = [line for line in output.splitlines() if line.startswith('FLOW LIFETIME PHASE ')]
        phase = phases[-1] if phases else 'no lifetime checkpoint reached'
        raise RuntimeError(f'{label} timed out; last checkpoint: {phase}; '
                           f'timeouts do not count as detected mutations:\n{output}') from error
    return subprocess.CompletedProcess(command, process.returncode, output)


def build(name, session_text, tables_text, details_text=None):
    work = WORK / name
    source = work / 'src/mpdialog'
    source.mkdir(parents=True, exist_ok=True)
    for filename in ('mpdialog_runtime.c', 'mpdialog_internal.h', 'mpdialog.def'):
        shutil.copyfile(ROOT / 'src/mpdialog' / filename, source / filename)
    shutil.copyfile(ROOT / 'src/mpdialog/mpdialog_flow.c', source / 'mpdialog_flow.c')
    (source / 'mpdialog_session.c').write_text(session_text)
    if details_text is None:
        details_text = (ROOT / 'src/mpdialog/mpdialog_details.c').read_text()
    (source / 'mpdialog_details.c').write_text(details_text)
    (source / 'mpdialog_runtime_tables.inc').write_text(tables_text)
    resources = [
        OUT / 'mpdialog-res.o', OUT / 'mpdialog-details-res.o',
        OUT / 'mpdialog-flow-intro-res.o', OUT / 'mpdialog-flow-choice-res.o',
        OUT / 'mpdialog-flow-host-res.o', OUT / 'mpdialog-flow-connection-res.o',
        OUT / 'mpdialog-flow-browser-res.o', OUT / 'mpdialog-flow-support-res.o',
    ]
    dll = work / 'MP_DIALOG_mutant.dll'
    command = [
        CC, *FLAGS, '-shared', '-nostdlib', '-Wl,--entry=_DllMain@12',
        '-Wl,--disable-dynamicbase', '-Wl,--image-base=0x22340000',
        '-o', str(dll), str(source / 'mpdialog_flow.c'),
        str(source / 'mpdialog_session.c'),
        str(source / 'mpdialog_details.c'), str(source / 'mpdialog_runtime.c'),
        str(source / 'mpdialog.def'),
        *(str(resource) for resource in resources),
        str(OUT / 'libmpdialog-simsui.a'),
        '-lkernel32', '-luser32', '-lcomctl32', '-lmsvcrt',
    ]
    run(command, cwd=ROOT)
    return dll


def build_flow(name, flow_text, tables_text):
    work = WORK / name
    source = work / 'src/mpdialog'
    source.mkdir(parents=True, exist_ok=True)
    for filename in ('mpdialog_runtime.c', 'mpdialog_internal.h', 'mpdialog.def'):
        shutil.copyfile(ROOT / 'src/mpdialog' / filename, source / filename)
    shutil.copyfile(ROOT / 'src/mpdialog/mpdialog_session.c', source / 'mpdialog_session.c')
    shutil.copyfile(ROOT / 'src/mpdialog/mpdialog_details.c', source / 'mpdialog_details.c')
    (source / 'mpdialog_flow.c').write_text(flow_text)
    (source / 'mpdialog_runtime_tables.inc').write_text(tables_text)
    resources = [
        OUT / 'mpdialog-res.o', OUT / 'mpdialog-details-res.o',
        OUT / 'mpdialog-flow-intro-res.o', OUT / 'mpdialog-flow-choice-res.o',
        OUT / 'mpdialog-flow-host-res.o', OUT / 'mpdialog-flow-connection-res.o',
        OUT / 'mpdialog-flow-browser-res.o', OUT / 'mpdialog-flow-support-res.o',
    ]
    dll = work / 'MP_DIALOG_mutant.dll'
    command = [
        CC, *FLAGS, '-shared', '-nostdlib', '-Wl,--entry=_DllMain@12',
        '-Wl,--disable-dynamicbase', '-Wl,--image-base=0x22340000',
        '-o', str(dll), str(source / 'mpdialog_flow.c'),
        str(source / 'mpdialog_session.c'), str(source / 'mpdialog_details.c'),
        str(source / 'mpdialog_runtime.c'), str(source / 'mpdialog.def'),
        *(str(resource) for resource in resources),
        str(OUT / 'libmpdialog-simsui.a'),
        '-lkernel32', '-luser32', '-lcomctl32', '-lmsvcrt',
    ]
    run(command, cwd=ROOT)
    return dll


def check_mutant(name, dll, expected_group, flow_case=None,
                 mutation_suite=None, mutation_case=None,
                 cleanup_flow_lifetime=False):
    command = ['wine', str(OUT / 'mpdialoguidiff.exe'), str(dll), '12']
    if flow_case is not None:
        command.extend(['--flow-mutation-case', str(flow_case)])
    elif mutation_suite is not None:
        command.extend([f'--{mutation_suite}-mutation-case', str(mutation_case)])
    if cleanup_flow_lifetime:
        command.append('--cleanup-flow-lifetime-leaks')
    result = run_wine(command, name)
    if flow_case is None:
        output_lines = result.stdout.splitlines()
        detected = any(
            'DIFFER' in line and
            f'{expected_group}=1' in '\n'.join(output_lines[index:index + 3])
            for index, line in enumerate(output_lines))
        if not detected:
            raise RuntimeError(f'{name} did not differ in {expected_group}:\n{result.stdout}')
    else:
        flow_lines = [line for line in result.stdout.splitlines()
                      if 'MP_DIALOG 0x110a DIFFER cycle=0' in line and
                      f'case={flow_case} groups' in line]
        if not any(f'{expected_group}=1' in line for line in flow_lines):
            raise RuntimeError(f'{name} did not differ in {expected_group} in flow case '
                               f'{flow_case}:\n{result.stdout}')
    if result.returncode != 1:
        raise RuntimeError(f'{name} comparison exited unexpectedly '
                           f'(exit {result.returncode}):\n{result.stdout}')
    if 'MP_DIALOG UI FAULT' in result.stdout:
        raise RuntimeError(f'{name} was detected only after a harness fault:\n{result.stdout}')
    if cleanup_flow_lifetime and 'FLOW HARNESS CLEANUP' not in result.stdout:
        raise RuntimeError(f'{name} did not exercise controlled leaked-object cleanup:\n'
                           f'{result.stdout}')
    if flow_case is not None or mutation_suite is not None:
        if 'modules deinitialized and unloaded' not in result.stdout:
            raise RuntimeError(f'{name} did not complete targeted dialog cleanup and module unload:\n'
                           f'{result.stdout}')
    suffix = ' with controlled cleanup' if cleanup_flow_lifetime else ''
    print(f'MUTATION CAUGHT {name}: {expected_group} comparison differs from retail{suffix}',
          flush=True)


def check_render_mutant(name, dll):
    result = run_wine(
        ['wine', str(OUT / 'mpdialoguidiff.exe'), str(dll), '12', '--render-only'], name)
    if result.returncode != 1 or 'MP_DIALOG RENDER DIFFER ' not in result.stdout:
        raise RuntimeError(f'{name} was not caught by the native render gate:\n{result.stdout}')
    if 'MP_DIALOG RENDER MUTATION DIFFER cycle=0 groups rendering=1' not in result.stdout:
        raise RuntimeError(f'{name} did not report the rendering difference group:\n{result.stdout}')
    if 'MP_DIALOG UI FAULT' in result.stdout:
        raise RuntimeError(f'{name} was detected only after a harness fault:\n{result.stdout}')
    if 'modules deinitialized and unloaded' not in result.stdout:
        raise RuntimeError(f'{name} did not complete module cleanup:\n{result.stdout}')
    print(f'MUTATION CAUGHT {name}: per-control native render differs from retail',
          flush=True)


def check_help_mutant(name, dll):
    result = run_wine(['wine', str(OUT / 'mpdialoguidiff.exe'), str(dll),
                       '12', '--help-only'], name)
    if (result.returncode != 1 or 'MP_DIALOG HELP DIFFER ' not in result.stdout or
            'modules deinitialized and unloaded' not in result.stdout or
            'MP_DIALOG UI FAULT' in result.stdout):
        raise RuntimeError(f'{name} did not produce a controlled help difference:\n{result.stdout}')
    print(f'MUTATION CAUGHT {name}: help comparison differs from retail', flush=True)


def run_suite():
    WORK.mkdir(parents=True, exist_ok=True)
    baseline = run_wine(
        ['wine', str(OUT / 'mpdialoguidiff.exe'),
         str(OUT / 'MP_DIALOG_ours.dll'), '12'], 'native baseline')
    required_baseline = (
        'MP_DIALOG 0x110a isolated native comparison: 24 paired scripts, 2 load/unload cycles, 0 differing scripts',
        '0x110a complete child-control trees: 24/24 paired scripts match across all five pages',
        '0x110a native page rendering: 1/1 paired page sets match; 5/5 visible WM_PRINT client/child captures match',
        'MP_DIALOG 0x110b native comparison: 24 paired scripts, 2 load/unload cycles, 0 differing scripts',
        'visibility checkpoints: 24/24 pairs match',
        '0x110b complete child-control trees: 24/24 paired session-settings dialogs match',
        '0x110b native rendering: 1/1 paired initial session-settings dialogs match',
        'MP_DIALOG 0x110c native comparison: 20 paired scripts, 2 load/unload cycles, 0 differing scripts',
        '0x110c complete child-control trees: 20/20 paired session-details dialogs match',
        '0x110c native rendering: 1/1 paired initial session-details dialogs match',
        'native rendering repeatability: 14/14 identical same-window captures match under unchanged state (0 unstable pairs)',
        'MP_DIALOG help comparison: seven dialogs, four messages each, two cycles, 0 differing cases',
    )
    missing_baseline = [line for line in required_baseline if line not in baseline.stdout]
    if baseline.returncode != 0 or missing_baseline:
        raise RuntimeError(f'0x110a/0x110b/0x110c native baseline failed '
                           f'(exit {baseline.returncode}, missing gates: {missing_baseline}):\n'
                           f'{baseline.stdout}')
    render_differences = [line for line in baseline.stdout.splitlines()
                          if line.startswith('MP_DIALOG RENDER DIFFER ')]
    control_tree_differences = [line for line in baseline.stdout.splitlines()
                                if line.startswith('MP_DIALOG CONTROL TREE DIFFER ')]
    behavior_difference = any(
        'MP_DIALOG 0x110a DIFFER' in line or
        'MP_DIALOG UI DIFFER' in line or
        'MP_DIALOG 0x110c DIFFER' in line or
        'MP_DIALOG UI FAULT' in line
        for line in baseline.stdout.splitlines())
    if behavior_difference:
        raise RuntimeError(f'0x110a/0x110b/0x110c native baseline failed:\n{baseline.stdout}')
    if render_differences or control_tree_differences:
        raise RuntimeError('native baseline has unaccepted control-tree or rendering differences:\n' +
                           baseline.stdout)
    print('BASELINE PASSED 0x110a/0x110b/0x110c native behavior, control-tree, rendering, '
          'and repeatability gates across two load/unload cycles', flush=True)

    session = (ROOT / 'src/mpdialog/mpdialog_session.c').read_text()
    details = (ROOT / 'src/mpdialog/mpdialog_details.c').read_text()
    tables = (ROOT / 'src/mpdialog/mpdialog_runtime_tables.inc').read_text()
    flow = (ROOT / 'src/mpdialog/mpdialog_flow.c').read_text()

    route = '(void *)fs98_mpdialog_help_intro'
    if tables.count(route) != 1:
        raise RuntimeError('intro help routing mutation anchor missing or ambiguous')
    check_help_mutant('help-wrong-dialog', build('help-wrong-dialog', session,
        tables.replace(route, '(void *)fs98_mpdialog_help_choice', 1)))
    for name, old_help, new_help in (
        ('help-wrong-command', '"fs98.hlp", HELP_WM_HELP, (ULONG_PTR)map',
         '"fs98.hlp", HELP_CONTEXTPOPUP, (ULONG_PTR)map'),
        ('help-wrong-window', '(HWND)help_info->hItemHandle, "fs98.hlp"',
         '(HWND)((uintptr_t)help_info->hItemHandle ^ 1), "fs98.hlp"'),
        ('help-wrong-table', '0x3fa, 0x40b42a9,', '0x3fa, 0x40b42a8,'),
        ('help-context-return', '    WinHelpA((HWND)(uintptr_t)window, "fs98.hlp", HELP_CONTEXTMENU, (ULONG_PTR)map);\n    return 1;',
         '    return WinHelpA((HWND)(uintptr_t)window, "fs98.hlp", HELP_CONTEXTMENU, (ULONG_PTR)map);'),
        ('help-context-payload', 'HELP_CONTEXTMENU, (ULONG_PTR)map);',
         'HELP_CONTEXTMENU, (ULONG_PTR)0);'),
    ):
        if session.count(old_help) != 1:
            raise RuntimeError(f'{name} mutation anchor missing or ambiguous')
        check_help_mutant(name, build(name, session.replace(old_help, new_help, 1), tables))

    old = '    return ((int32_t (TC *)(void *, int32_t))PTR(PTR(object, 0), 0xe0))(object, field);'
    if old not in flow:
        raise RuntimeError('Host error return mutation anchor missing')
    host_start = flow.index('int32_t TC fs98_mpdialog_flow_error_host(')
    host_return = flow.index(old, host_start)
    host_mutant = (flow[:host_return] +
        '    ((void (TC *)(void *, int32_t))PTR(PTR(object, 0), 0xe0))(object, field);\n    return 1;' +
        flow[host_return + len(old):])
    error_return_mutant = build_flow('flow-host-error-return', host_mutant, tables)
    check_mutant('flow-host-error-return', error_return_mutant, 'validation', flow_case=2)
    check_mutant('flow-host-error-return-real-modal', error_return_mutant, 'validation', flow_case=7)

    choice_start = flow.index('int32_t TC fs98_mpdialog_flow_error_choice(')
    choice_end = flow.index('\nstatic void flow_choice_route_update', choice_start)
    choice = flow[choice_start:choice_end]
    choice_field = '        if (result) field = 0x419;\n    } else field = 0;'
    if choice_field not in choice or old not in choice:
        raise RuntimeError('Choice error mutation anchors missing')
    for name, mutated_choice in (
        ('flow-choice-error-no-status', choice.replace(choice_field,
            '        if (result) field = 0x419;\n    }', 1)),
        ('flow-choice-error-helper-zero', choice.replace(choice_field,
            '        field = result ? 0x419 : 0;\n    } else field = 0;', 1)),
        ('flow-choice-error-return', choice.replace(old,
            '    ((void (TC *)(void *, int32_t))PTR(PTR(object, 0), 0xe0))(object, field);\n    return 1;', 1)),
    ):
        mutant = flow[:choice_start] + mutated_choice + flow[choice_end:]
        check_mutant(name, build_flow(name, mutant, tables), 'validation', flow_case=6)

    old = 'EM_SETLIMITTEXT, D32(block, 0x64) - 1, 0);'
    if old not in flow:
        raise RuntimeError('Host initialization limit mutation anchor missing')
    for name, expression, case in (
        ('flow-host-init-zero', 'D32(block, 0x64) ? D32(block, 0x64) - 1 : 0', 1),
        ('flow-host-init-one', 'D32(block, 0x64)', 11),
    ):
        check_mutant(name, build_flow(name, flow.replace(old,
            'EM_SETLIMITTEXT, ' + expression + ', 0);', 1), tables),
            'host_update', flow_case=case)

    old = '    set_checked(AT(object, 0x19b4), D32(block, 0x40) == 2);'
    first_range = '    SendMessageA((HWND)D32(object, 0x1714), UDM_SETRANGE, 0,'
    if old not in flow or first_range not in flow:
        raise RuntimeError('Host update order mutation anchor missing')
    mutant = flow.replace(old, '', 1).replace(first_range, old + "\n" + first_range, 1)
    check_mutant('flow-host-update-order', build_flow('flow-host-update-order',
        mutant, tables), 'host_update', flow_case=0)

    old = '    set_checked(AT(object, 0x19b4), D32(block, 0x40) == 2);'
    new = old + '\n    SendMessageA((HWND)D32(object, 0x1b54), EM_SETLIMITTEXT, 63, 0);'
    check_mutant('flow-host-update-limit', build_flow('flow-host-update-limit',
        flow.replace(old, new, 1), tables), 'host_update', flow_case=0)

    old = '        if (!result) field = original_field;\n    } else field = 0;'
    if old not in flow:
        raise RuntimeError('Host error field mutation anchor missing')
    for name, replacement in (
        ('flow-host-error-empty', '        if (!result) field = original_field;\n    }'),
        ('flow-host-error-helper-failure', '        if (!result) field = 0;\n    } else field = 0;\n    (void)original_field;'),
    ):
        check_mutant(name, build_flow(name, flow.replace(old, replacement, 1), tables),
                     'validation', flow_case=2)

    old = '    if (D32(block, 0x5c)) return 0x40f;'
    if old not in flow:
        raise RuntimeError('Host validation priority mutation anchor missing')
    check_mutant('flow-host-validation-status', build_flow('flow-host-validation-status',
        flow.replace(old, '    if (D32(block, 0x5c)) return 0x410;', 1), tables),
        'validation', flow_case=2)

    old = 'PostMessageA((HWND)s_flow_hwnds[0], WM_COMMAND, 0x3e8, 0);'
    new = 'SendMessageA((HWND)s_flow_hwnds[0], WM_COMMAND, 0x3e8, 0);'
    if old not in flow:
        raise RuntimeError('0x110a initial-command timing mutation anchor missing')
    check_mutant('flow-initial-command-timing', build_flow('flow-initial-command-timing',
        flow.replace(old, new, 1), tables), 'timing', flow_case=0)

    old = 'case 0: target = 1; break;'
    new = 'case 0: target = 2; break; /* mutation: skip Choice */'
    if old not in flow:
        raise RuntimeError('0x110a page-transition mutation anchor missing')
    check_mutant('flow-page-transition', build_flow('flow-page-transition',
        flow.replace(old, new, 1), tables), 'transitions', flow_case=0)

    old = 'return result && D32(block, 0x28) ? 0x41a : 0;'
    new = 'result = 0; return result; /* mutation: suppress pending validation command */'
    if old not in flow:
        raise RuntimeError('0x110a validation suppression mutation anchor missing')
    check_mutant('flow-validation-command-suppression', build_flow(
        'flow-validation-command-suppression', flow.replace(old, new, 1), tables),
        'validation', flow_case=6)

    old = 'if (((int32_t (SC *)(void *))D32(block, 0x0c))(block) == 0) flow_finish(block, 1);'
    new = ('if (((int32_t (SC *)(void *))D32(block, 0x0c))((void *)(uintptr_t)object) == 0) '
           'flow_finish(block, 1); /* mutation: pass page instead of descriptor */')
    if old not in flow:
        raise RuntimeError('0x110a callback argument mutation anchor missing')
    check_mutant('flow-callback-argument', build_flow('flow-callback-argument',
        flow.replace(old, new, 1), tables), 'callbacks', flow_case=5)

    flow_tables = tables
    flow_start = flow_tables.index('s_flow_virtual_bindings_0')
    old = '{43, -1, (void *)fs98_mpdialog_details_post_destroy},'
    position = flow_tables.find(old, flow_start)
    if position < 0:
        raise RuntimeError('0x110a WM_NCDESTROY lifetime mutation anchor missing')
    flow_tables = flow_tables[:position] + flow_tables[position:].replace(
        old, '{43, -1, (void *)fs98_mpdialog_empty_arg}, /* mutation: skip deleting dtor */', 1)
    check_mutant('flow-lifetime-order', build_flow('flow-lifetime-order',
        flow, flow_tables), 'lifetime', flow_case=0,
        cleanup_flow_lifetime=True)

    old = 'for (index = 0; index < D32(block, 0xb0); ++index) {'
    new = 'for (index = 0; index < D32(block, 0xb0) && index < 128; ++index) {'
    if old not in flow:
        raise RuntimeError('Browser row cap mutation anchor missing')
    check_mutant('flow-browser-row-cap', build_flow(
        'flow-browser-row-cap', flow.replace(old, new, 1), tables),
        'initial', flow_case=0)

    old = 'SendMessageA(list, LB_SETCURSEL, count ? 0 : (WPARAM)-1, 0);'
    new = 'SendMessageA(list, LB_SETCURSEL, count ? 1 : (WPARAM)-1, 0);'
    if old not in flow:
        raise RuntimeError('Browser rebuild selection mutation anchor missing')
    check_mutant('flow-browser-rebuild-selection', build_flow(
        'flow-browser-rebuild-selection', flow.replace(old, new, 1), tables),
        'browser_dispatch', flow_case=3)

    old = ("    fs98_mpdialog_enable(AT(object, 0x11d8), selection != (uint32_t)-1);\n"
           "    selection = (uint32_t)SendMessageA((HWND)D32(object, 0x190c), LB_GETCURSEL, 0, 0);\n"
           "    ((int32_t (SC *)(void *, uint32_t))D32(block, 0x98))(block, selection);")
    new = ("    selection = (uint32_t)SendMessageA((HWND)D32(object, 0x190c), LB_GETCURSEL, 0, 0);\n"
           "    ((int32_t (SC *)(void *, uint32_t))D32(block, 0x98))(block, selection);\n"
           "    fs98_mpdialog_enable(AT(object, 0x11d8), selection != (uint32_t)-1);")
    if old not in flow:
        raise RuntimeError('Browser callback ordering mutation anchor missing')
    check_mutant('flow-browser-callback-order', build_flow(
        'flow-browser-callback-order', flow.replace(old, new, 1), tables),
        'browser_dispatch', flow_case=3)

    old = '"\\t%d of %d\\t%d of %d"'
    new = '"\\t%u of %u\\t%u of %u"'
    if old not in flow:
        raise RuntimeError('Browser signed formatting mutation anchor missing')
    check_mutant('flow-browser-unsigned-format', build_flow(
        'flow-browser-unsigned-format', flow.replace(old, new, 1), tables),
        'browser_dispatch', flow_case=3)

    old = '    if (block) D32(block, 0x40) = D32(object, 0x1ad8) ? 2 : 1;'
    new = '    if (block) flow_capture(object);'
    if old not in flow:
        raise RuntimeError('Browser observer fields mutation anchor missing')
    check_mutant('flow-browser-observer-fields', build_flow(
        'flow-browser-observer-fields', flow.replace(old, new, 1), tables),
        'browser_dispatch', flow_case=3)

    old = ('    D32(block, 0xb8) = (uint32_t)SendMessageA((HWND)D32(object, 0x190c), LB_GETCURSEL, 0, 0);\n'
           '    if (D32(block, 0xb8) != (uint32_t)-1)')
    new = ('    flow_capture(object);\n'
           '    if (D32(block, 0xb8) != (uint32_t)-1)')
    if old not in flow:
        raise RuntimeError('Browser details fields mutation anchor missing')
    check_mutant('flow-browser-details-fields', build_flow(
        'flow-browser-details-fields', flow.replace(old, new, 1), tables),
        'browser_dispatch', flow_case=3)

    old = ('    D32(object, 0x1aec) = 1;\n'
           '    D32(block, 0xb8) = (uint32_t)SendMessageA((HWND)D32(object, 0x190c), LB_GETCURSEL, 0, 0);')
    new = '    D32(object, 0x1aec) = 1;\n    flow_capture(object);'
    if old not in flow:
        raise RuntimeError('Browser refresh fields mutation anchor missing')
    check_mutant('flow-browser-refresh-fields', build_flow(
        'flow-browser-refresh-fields', flow.replace(old, new, 1), tables),
        'browser_dispatch', flow_case=3)

    old = ('    ((int32_t (SC *)(void *))D32(block, 0x0c))(block);\n'
           '    flow_finish(block, 1);')
    new = '    if (((int32_t (SC *)(void *))D32(block, 0x0c))(block) == 0) flow_finish(block, 1);'
    if old not in flow:
        raise RuntimeError('Browser accept return mutation anchor missing')
    check_mutant('flow-browser-accept-return', build_flow(
        'flow-browser-accept-return', flow.replace(old, new, 1), tables),
        'transitions', flow_case=3)

    old = ('    SendMessageA(list, LB_RESETCONTENT, 0, 0);\n'
           '    SendMessageA(list, LB_SETCURSEL, (WPARAM)-1, 0);\n'
           '    fs98_mpdialog_enable(AT(object, 0x11d8), 0);')
    new = '    (void)list;\n    flow_capture(object);'
    if old not in flow:
        raise RuntimeError('Browser back callback mutation anchor missing')
    check_mutant('flow-browser-back-order', build_flow(
        'flow-browser-back-order', flow.replace(old, new, 1), tables),
        'browser_dispatch', flow_case=10)

    old = ('    for (index = 0; index < D32(block, 0xb0); ++index) {\n'
           '        uint8_t *row;\n'
           '        rows = (uint8_t *)(uintptr_t)D32(block, 0xac);')
    new = ('    count = D32(block, 0xb0);\n'
           '    for (index = 0; index < count; ++index) {\n'
           '        uint8_t *row;\n'
           '        rows = (uint8_t *)(uintptr_t)D32(block, 0xac);')
    if old not in flow:
        raise RuntimeError('Browser reentrant row count mutation anchor missing')
    check_mutant('flow-browser-reentrant-row-count', build_flow(
        'flow-browser-reentrant-row-count', flow.replace(old, new, 1), tables),
        'browser_dispatch', flow_case=3)

    old = ('    for (index = 0; index < D32(block, 0xb0); ++index) {\n'
           '        uint8_t *row;\n'
           '        rows = (uint8_t *)(uintptr_t)D32(block, 0xac);')
    new = ('    rows = (uint8_t *)(uintptr_t)D32(block, 0xac);\n'
           '    for (index = 0; index < D32(block, 0xb0); ++index) {\n'
           '        uint8_t *row;')
    if old not in flow:
        raise RuntimeError('Browser reentrant row pointer mutation anchor missing')
    check_mutant('flow-browser-reentrant-row-pointer', build_flow(
        'flow-browser-reentrant-row-pointer', flow.replace(old, new, 1), tables),
        'browser_dispatch', flow_case=3)

    browser_handlers = (
        ('void TC fs98_mpdialog_flow_show_browser(void *object, uint32_t show, uint32_t status)\n'
         '{\n    HWND window = (HWND)D32(object, 0x20);\n    (void)status;\n'
         '    ((void (TC *)(void *))fs98_mpdialog_mfc(2379))(object);\n'
         '    (void)KillTimer(window, 1);\n'
         '    if (show) (void)SetTimer(window, 1, 0x1f4, 0);\n}',
         'void TC fs98_mpdialog_flow_show_browser(void *object, uint32_t show, uint32_t status)\n'
         '{ (void)object; (void)show; (void)status; }'),
        ('void TC fs98_mpdialog_flow_timer_browser(void *object, uint32_t timer_id)\n'
         '{\n    (void)KillTimer((HWND)D32(object, 0x20), timer_id);\n'
         '    fs98_mpdialog_flow_refresh_browser(object);\n}',
         'void TC fs98_mpdialog_flow_timer_browser(void *object, uint32_t timer_id)\n'
         '{ (void)object; (void)timer_id; }'),
    )
    for old, new in browser_handlers:
        if old not in flow:
            raise RuntimeError('Browser timer-handler mutation anchor missing')
        flow = flow.replace(old, new, 1)
    old = ('int32_t TC fs98_mpdialog_flow_mouse_activate_browser(void *object, uint32_t active,\n'
           '                                                     uint32_t hit_test, uint32_t message)\n'
           '{\n    int32_t result = ((int32_t (TC *)(void *))fs98_mpdialog_mfc(2379))(object);\n'
           '    (void)active;\n    (void)hit_test;\n    (void)message;\n'
           '    if (D32(object, 0x1aec)) return 4;\n    return result;\n}')
    new = ('int32_t TC fs98_mpdialog_flow_mouse_activate_browser(void *object, uint32_t active,\n'
           '                                                     uint32_t hit_test, uint32_t message)\n'
           '{ (void)object; (void)active; (void)hit_test; (void)message; return 0; }')
    if old not in flow:
        raise RuntimeError('Browser mouse-activation mutation anchor missing')
    flow = flow.replace(old, new, 1)
    check_mutant('flow-browser-message-handlers', build_flow(
        'flow-browser-message-handlers', flow, tables), 'browser_dispatch', flow_case=3)

    old = 'if (!block || D32(object, 0x1aec)) return;'
    new = 'if (!block) return; /* mutation: permit nested refresh callback */'
    if old not in flow:
        raise RuntimeError('Browser refresh reentry-guard mutation anchor missing')
    check_mutant('flow-browser-refresh-reentry-guard', build_flow(
        'flow-browser-refresh-reentry-guard', flow.replace(old, new, 1), tables),
        'browser_dispatch', flow_case=3)

    old = '    block->result = 1;\n    ((fs98_mpdialog_callback)block->accept)(block);'
    new = ('    block->result = 0;\n'
           '    ((fs98_mpdialog_callback)block->accept)(block);\n'
           '    block->result = 1;')
    if old not in session:
        raise RuntimeError('accept-result mutation anchor missing')
    check_mutant('accept-result-order', build('accept-result-order',
        session.replace(old, new, 1), tables), 'callbacks',
        mutation_suite='session', mutation_case=0)

    old = ('{0x111, 0x0, 0x3eb, 0x3eb, 0xc, (void *)fs98_mpdialog_get_conditions},\n'
           '    {0x111, 0x0, 0x3ee, 0x3ee, 0xc, (void *)fs98_mpdialog_set_conditions},')
    new = ('{0x111, 0x0, 0x3eb, 0x3eb, 0xc, (void *)fs98_mpdialog_set_conditions},\n'
           '    {0x111, 0x0, 0x3ee, 0x3ee, 0xc, (void *)fs98_mpdialog_get_conditions},')
    if old not in tables:
        raise RuntimeError('callback-map mutation anchor missing')
    check_mutant('flight-callback-map', build('flight-callback-map',
        session, tables.replace(old, new, 1)), 'callbacks',
        mutation_suite='session', mutation_case=0)

    old = 'fs98_mpdialog_check(AT(object, 0x1b80), block->dme_lock != 0);'
    new = 'fs98_mpdialog_check(AT(object, 0x1b80), block->dme_lock == 0);'
    if old not in session:
        raise RuntimeError('DME-state mutation anchor missing')
    check_mutant('dme-state-invert', build('dme-state-invert',
        session.replace(old, new, 1), tables), 'checks',
        mutation_suite='session', mutation_case=0)

    old = ("    } else if (block->flight_conditions == 0) {\n"
           "        fs98_mpdialog_show_control(AT(object, 0x13d0), 1);\n"
           "        fs98_mpdialog_show_control(AT(object, 0x14cc), 0);\n"
           "    } else {\n"
           "        fs98_mpdialog_show_control(AT(object, 0x14cc), 1);\n"
           "        fs98_mpdialog_show_control(AT(object, 0x13d0), 0);\n"
           "    }\n")
    new = ("    } else {\n"
           "        /* mutation: omit retail's second visibility decision */\n"
           "    }\n")
    if old not in session:
        raise RuntimeError('visibility mutation anchor missing')
    check_mutant('visibility-final-branch', build('visibility-final-branch',
        session.replace(old, new, 1), tables), 'visibility',
        mutation_suite='session', mutation_case=2)

    old = ('    if (((fs98_mpdialog_callback)D32(block, 0x10))(block))\n'
           '        ((void (TC *)(void *))PTR(PTR(object, 0), 0xf8))(object);')
    new = ('    if (!((fs98_mpdialog_callback)D32(block, 0x10))(block))\n'
           '        ((void (TC *)(void *))PTR(PTR(object, 0), 0xf8))(object);')
    if old not in details:
        raise RuntimeError('0x110c refresh mutation anchor missing')
    check_mutant('details-refresh-polarity', build('details-refresh-polarity',
        session, tables, details.replace(old, new, 1)), 'refresh',
        mutation_suite='details', mutation_case=0)

    old = '    if (((fs98_mpdialog_callback)D32(block, 4))(block)) {'
    new = '    if (0) { /* mutation: bypass capacity validation result */'
    if old not in details:
        raise RuntimeError('0x110c validation mutation anchor missing')
    check_mutant('details-capacity-validation', build('details-capacity-validation',
        session, tables, details.replace(old, new, 1)), 'capacity',
        mutation_suite='details', mutation_case=4)

    old = '        if (D32(block, 0x30)) return 0x40d;'
    new = ('        if (D32(block, 0x30)) {\n'
           '            SetFocus(GetDlgItem((HWND)D32(object, 0x20), 0x40d));\n'
           '            return 0x40d;\n'
           '        }')
    if old not in details:
        raise RuntimeError('0x110c focus mutation anchor missing')
    check_mutant('details-validation-focus', build('details-validation-focus',
        session, tables, details.replace(old, new, 1)), 'capacity',
        mutation_suite='details', mutation_case=5)

    old = '    ((fs98_mpdialog_callback)D32(block, 0x0c))(block);'
    new = '    (void)block; /* mutation: skip MULTIP removal confirmation callback */'
    if old not in details:
        raise RuntimeError('0x110c removal mutation anchor missing')
    check_mutant('details-removal-callback', build('details-removal-callback',
        session, tables, details.replace(old, new, 1)), 'removal',
        mutation_suite='details', mutation_case=6)

    old = ('    if (object)\n'
           '        ((void *(TC *)(void *, uint32_t))PTR(PTR(object, 0), 4))(object, 1);')
    new = '    (void)object; /* mutation: omit the WM_NCDESTROY deleting path */'
    if old not in details:
        raise RuntimeError('0x110c lifetime mutation anchor missing')
    check_mutant('details-lifetime-order', build('details-lifetime-order',
        session, tables, details.replace(old, new, 1)), 'lifetime',
        mutation_suite='details', mutation_case=0)

    flow = (ROOT / 'src/mpdialog/mpdialog_flow.c').read_text()
    old = '?GetSimsBaseFont@@YGPAVCFont@@H@Z"))(2);'
    new = '?GetSimsBaseFont@@YGPAVCFont@@H@Z"))(0); /* mutation: wrong font */'
    if old not in flow:
        raise RuntimeError('render font mutation anchor missing')
    check_render_mutant('render-font-selector', build_flow('render-font-selector',
        flow.replace(old, new, 1), tables))
    return 0


def main():
    with isolated_wine_prefix():
        return run_suite()


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f'MP_DIALOG mutation check failed: {error}', file=sys.stderr)
        sys.exit(1)
