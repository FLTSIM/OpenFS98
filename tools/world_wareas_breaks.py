#!/usr/bin/env python3
"""Snapshot weather-area mutations and count only explicit retail differences."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native', action='store_true')
    parser.add_argument('names', nargs='*')
    args = parser.parse_args()
    core = 'world_wareas.c'
    life = 'world_wareas_objects.c'
    runtime = 'world_runtime_tables.inc'
    mutations = [
        ('selection-store', 'core', core, 'U32(object, 0x10f4) = selection;', 'U32(object, 0x10f4) = selection + 1;'),
        ('selection-owner', 'core', core, '0xc) = U32(object, 0x10f4);', '0xc) = 0;'),
        ('selection-edit', 'core', core, 'world_text(object, 0x1750, 8);', 'world_text(object, 0x1750, 0x1c);'),
        ('selection-disable', 'core', core, 'world_enable(object, 0x1750, 0);', 'world_enable(object, 0x1750, 1);'),
        ('selection-label', 'core', core, 'index < 12; ++index) world_enable', 'index < 11; ++index) world_enable'),
        ('selection-check', 'core', core, '0x2658), U32(PTR(object, 0x10fc), 0xbc)', '0x2658), U32(PTR(object, 0x10fc), 0xc4)'),
        ('row-stride', 'core', core, 'AT(PTR(object, 0x10fc), 0xc8)', 'AT(PTR(object, 0x10fc), 0)'),
        ('row-item-data', 'core', core, '0x19a, row, U32(object, 0x10fc)', '0x19a, row, 0'),
        ('populate-add', 'core', core, '0x18b4, U32(PTR(object, 0x10f8), 0x18)', '0x18b4, U32(PTR(object, 0x10f8), 0x1c)'),
        ('advanced-enable', 'core', core, 'if (action == 0x28) world_enable(object, 0, 0);', 'if (action == 0x28) world_enable(object, 0, 1);'),
        ('advanced-focus', 'core', core, 'AT(object, 0x1274));', 'AT(object, 0x18b4));'),
        ('add-focus-post', 'core', core, 'U32(object, 0x20), 0x5fa,', 'U32(object, 0x20), 0x5fb,'),
        ('cancel-answer', 'core', core, 'U32(descriptor, 0) = 0;', 'U32(descriptor, 0) = 1;'),
        ('accept-owner-cache', 'core', core, 'void fs98_world_wareas_accept(void *object)\n{\n', 'void fs98_world_wareas_accept(void *object)\n{\n    void *saved_descriptor = PTR(object, 0x10f8);\n'),
        ('accept-answer', 'core', core, 'U32(PTR(object, 0x10f8), 0) = 1;', 'U32(PTR(object, 0x10f8), 0) = 0;'),
        ('accept-error', 'core', core, 'if (!result || !((int', 'if (!result && !((int'),
        ('accept-vtable-cache', 'core', core, 'vtable[0xec/4])(object);\n    }', '((void **)PTR(object, 0))[0xec/4])(object);\n    }'),
        ('validation-readiness', 'core', core, 'if (!U32(PTR(object, 0x10fc), 0)) return 0;', 'if (0 && !U32(PTR(object, 0x10fc), 0)) return 0;'),
        ('validation-name', 'core', core, 'if (id == 0x1b0b)', 'if (id == 0x1b0c)'),
        ('validation-priority', 'core', core, '{6,7,5,4,0,1,2,3}', '{7,6,5,4,0,1,2,3}'),
        ('validation-result', 'core', core, '    return result;\n}', '    return !!result;\n}'),
        ('error-result', 'core', core, 'U32(object, 0x8c) = result;', 'U32(object, 0x8c) = !!result;'),
        ('error-redispatch', 'core', core, ')(object, control);', ')(object, (control & 0));'),
        ('descriptor-poison', 'core', core, 'descriptor && descriptor != 0xcdcdcdcd', 'descriptor'),
        ('check-field', 'core', core, 'U32(PTR(object, 0x10fc), field) =', 'U32(PTR(object, 0x10fc), field + 4) ='),
        ('init-base', 'init', core, '    ui->base_init(object);', '    (void)object;'),
        ('init-skin', 'init', core, 'object, 0x14f,', 'object, 0x14e,'),
        ('init-font', 'init', core, 'ui->base_font(8)', 'ui->base_font(1)'),
        ('init-font-register', 'init', core, 's_services->at_exit(fs98_world_wareas_font_cleanup);', '(void)object;'),
        ('init-font-style', 'init', core, '= 0x110;', '= 0x111;'),
        ('init-art', 'init', core, '0x1620), 0xf831,', '0x1620), 0xf830,'),
        ('init-button', 'init', core, 'small ? 0x130 : 0x132', 'small ? 0x132 : 0x130'),
        ('init-units', 'init', core, '? 0xf8f6 : 0xf90f', '? 0xf90f : 0xf8f6'),
        ('init-edit', 'init', core, '0x11,0x11,0x11,0x15,0', '0x11,0x11,0x11,0x11,0'),
        ('init-edit-label', 'init', core, 'AT(object, s_edit_labels[index]) : NULL', 'AT(object, 0x1bd8) : NULL'),
        ('init-notify-cleanup', 'init', core, 'if (state->live) { state->live = 0;', 'if (0 && state->live) { state->live = 0;'),
        ('init-notify-ownership', 'init', core, 'state.live = 0; PTR(object,0x10dc)', 'state.live = 1; PTR(object,0x10dc)'),
        ('init-name-limit', 'init', core, 'U32(row,0xc)-1', 'U32(row,0xc)'),
        ('init-length', 'init', core, '0x258c) = API', '0x2524) = API'),
        ('class-template', 'life', life, 'base_construct(object, 0x6ba, parent)', 'base_construct(object, 0x6bb, parent)'),
        ('class-vtable-timing', 'life', life, '    state.base = 1;\n    while', '    state.base = 1; WORLD_PTR(object, 0) = s_wareas_vtable;\n    while'),
        ('class-last-control', 'life', life, 'state.constructed < sizeof(s_wareas_controls)', 'state.constructed + 1 < sizeof(s_wareas_controls)'),
        ('class-destructor-order', 'life', life, '&s_wareas_controls[--state->constructed]', '&s_wareas_controls[sizeof(s_wareas_controls)/sizeof(s_wareas_controls[0])-state->constructed--]'),
        ('class-destructor-vtable', 'life', life, 'state.object = object; state.base = 1;', 'state.object = object; state.base = 1; WORLD_PTR(object, 0) = s_wareas_vtable;'),
        ('class-scalar-flags', 'life', life, 'if (flags & 1)', 'if (flags & 2)'),
        ('factory-size', 'life', life, 'allocate(0x2720)', 'allocate(0x271c)'),
        ('factory-parent', 'life', life, 'construct(object, NULL)', 'construct(object, (void *)parent)'),
        ('factory-ownership', 'life', life, 'state.live = 0;\n    s_object_services->create', 'state.live = 1;\n    s_object_services->create'),
        ('factory-cleanup', 'life', life, 'dialog_delete(state->object)', 'ui_delete(state->object)'),
        ('factory-show', 'life', life, 'show(object, 1)', 'show(object, 0)'),
        ('ddx-last-control', 'life', life, 'index < sizeof(s_wareas_ddx)', 'index + 1 < sizeof(s_wareas_ddx)'),
        ('ddx-base', 'life', life, '    uint32_t index;\n    for (index', '    uint32_t index;\n    s_object_services->ddx(exchange, 0, object);\n    for (index'),
        ('help-command', 'life', life, '"fs98.hlp", 12,', '"fs98.hlp", 10,'),
    ]
    if args.native:
        native_names = {'selection-edit','selection-disable','selection-check','populate-add','advanced-enable','cancel-answer','accept-answer','accept-owner-cache','validation-priority','init-art','init-units','init-font-style','init-edit-label','init-name-limit'}
        mutations = [m for m in mutations if m[0] in native_names]
        mutations += [
            ('ddx-checkbox-bindings','native','world_wareas_tables.inc','{0x2590, 0x1ac7},\n    {0x2658, 0x1ac6}', '{0x2590, 0x1ac6},\n    {0x2658, 0x1ac7}'),
            ('selection-routing','native',runtime,'fs98_world_wareas_selection(object);','(void)object;'),
            ('check-routing','native',runtime,'fs98_world_wareas_check(object, 0xbc, 0x2714);','fs98_world_wareas_check(object, 0xc4, 0x2714);'),
            ('notify-deletion','native','world_complexity_objects.c','if (auxiliary) {','if (auxiliary && 0) {'),
            ('parent-deletion','native',runtime,'fs98_world_priority_post_destroy(object);','(void)object;'),
            ('font-cleanup','native','world_runtime.c','s_font_exit = NULL; callback();','s_font_exit = NULL; (void)callback;'),
        ]
    requested=set(args.names)
    if requested - {m[0] for m in mutations}: raise ValueError('unknown mutation')
    selected=[m for m in mutations if not requested or m[0] in requested]
    output=Path('bin/world-diff/wareas-'+('native-breaks' if args.native else 'breaks'))
    output.mkdir(parents=True,exist_ok=True)
    sources={p.name:p.read_bytes() for p in Path('src/world').iterdir() if p.suffix in ('.c','.h','.inc','.def')}
    for name in ('worldwareasdiff.c','worldwareasinitdiff.c','worldwareaslifediff.c','worldwareasuidiff.c','worlduidiff.c'):
        sources[name]=(Path('tools/difftest')/name).read_bytes().replace(b'../../src/world/',b'')
    for name in ('world-res.o','libworld-simsui.a','WORLD_area.dll'):
        sources[name]=(Path('bin/world-diff')/name).read_bytes()
    (output/'inputs.json').write_text(json.dumps({name:hashlib.sha256(data).hexdigest() for name,data in sources.items()},indent=2)+'\n')
    world_sources=next(line.split(':=',1)[1].split() for line in Path('tools/world.mk').read_text().splitlines() if line.startswith('WORLD_SOURCES :='))
    caught=0
    for name,unit,filename,before,after in selected:
        work=output/name;work.mkdir(exist_ok=True)
        for source,data in sources.items():
            if source==filename:
                if data.count(before.encode())!=1: raise ValueError(f'{name}: anchor count {data.count(before.encode())}')
                data=data.replace(before.encode(),after.encode())
                if name=='accept-owner-cache':
                    data=data.replace(b'U32(PTR(object, 0x10f8), 0) = 1;',b'U32(saved_descriptor, 0) = 1;')
            (work/source).write_bytes(data)
        flags=['i686-w64-mingw32-gcc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fno-strict-aliasing','-fno-delete-null-pointer-checks']
        if args.native:
            dll=work/'mutant.dll';exe=work/'worldwareasuidiff.exe'
            commands=[flags+['-shared','-nostdlib','-Wl,--entry=_DllMain@12','-Wl,--disable-dynamicbase','-Wl,--image-base=0x223f0000','-o',str(dll),*[str(work/Path(p).name) for p in world_sources],str(work/'world.def'),str(work/'world-res.o'),str(work/'libworld-simsui.a'),'-lkernel32','-luser32','-lmsvcrt','-lntdll'],flags+['-o',str(exe),str(work/'worldwareasuidiff.c'),'-lgdi32','-lntdll']]
            run=['wine',str(exe),str(dll),'1','0' if name=='selection-disable' else '5']
        else:
            harness='worldwareas'+{'core':'','init':'init','life':'life'}[unit]+'diff.c';exe=work/'mutant.exe'
            inputs=[str(work/harness),str(work/core)]
            if unit=='life': inputs.append(str(work/life))
            commands=[flags+['-o',str(exe),*inputs,'-lntdll']]
            run=['wine',str(exe),'100','0x4a971b63','unwind' if unit!='core' else 'interrupt']
        for command in commands:
            result=subprocess.run(command,capture_output=True,text=True)
            if result.returncode:
                (work/'build.log').write_text(result.stdout+result.stderr)
                print(f'{name}: BUILD FAILED (not counted)',flush=True);return 1
        (work/'artifacts.json').write_text(json.dumps({p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in work.iterdir() if p.suffix in ('.dll','.exe')},indent=2)+'\n')
        try: result=subprocess.run(run,capture_output=True,text=True,timeout=180)
        except subprocess.TimeoutExpired:
            print(f'{name}: TIMEOUT (not counted)',flush=True);return 1
        log=result.stdout+result.stderr;(work/'result.log').write_text(log)
        if result.returncode==1 and 'DIFFER' in log:
            caught+=1;print(f'{name}: detected',flush=True)
        else:
            print(f'{name}: NOT DETECTED (exit {result.returncode})\n{log}',flush=True)
    print(f'World weather areas {"native" if args.native else "guarded"} mutations: {caught}/{len(selected)} detected',flush=True)
    return 0 if caught==len(selected) else 1


if __name__=='__main__':
    raise SystemExit(main())
