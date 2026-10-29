#!/usr/bin/env python3
"""Snapshot deliberate time/season changes; require actual retail differences."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys


def main():
    mutations = [
        ('bind-date', 'core', 'PTR(page,0x111c)=AT(PTR(object,0x10dc),0x74);', 'PTR(page,0x111c)=AT(PTR(object,0x10dc),0x10);'),
        ('bind-buttons', 'core', 'PTR(object,0x2120)=AT(object,0x126c);', 'PTR(object,0x2120)=AT(object,0x1170);'),
        ('descriptor-poison', 'core', 'if(descriptor&&descriptor!=0xcdcdcdcd)', 'if(descriptor)'),
        ('diagnostic-result', 'core', '"Bad m_pliveInfo = %LX",descriptor);\n    return 0;', '"Bad m_pliveInfo = %LX",descriptor);\n    return 1;'),
        ('season-selection', 'core', '0x1b80,0x14e,U32(PTR(object,0x1118),0x60)', '0x1b80,0x14e,U32(PTR(object,0x1118),8)'),
        ('date-text', 'core', 'time_set_edit(object,0x1ab0,0x1118,0x10);', 'time_set_edit(object,0x1ab0,0x1118,0x24);'),
        ('clock-text', 'core', 'time_set_edit(object,0x1c38,0x111c,0x68);', 'time_set_edit(object,0x1c38,0x111c,0x7c);'),
        ('display-inline', 'core', 'time_set_text(object,0x16f0,0x1120,8,1);', 'time_set_text(object,0x16f0,0x1120,0x11,1);'),
        ('season-radio-state', 'core', 'if(time_checked(object,0x1860))U32(PTR(object,0x1118),8)=0;', 'if(time_checked(object,0x1860))U32(PTR(object,0x1118),8)=1;'),
        ('clock-radio-state', 'core', 'U32(PTR(object,0x111c),0x28)=manual;', 'U32(PTR(object,0x111c),0x28)=!manual;'),
        ('ready-gate', 'core', 'if(!U32(object,0x1100))return 0;', 'if(0)return 0;'),
        ('date-collection', 'core', 'time_get_edit(object,0x1ab0,0x1118,0x10);', 'time_get_edit(object,0x1ab0,0x1118,0x24);'),
        ('clock-collection', 'core', 'time_get_edit(object,0x1b00,0x111c,0x54);', 'time_get_edit(object,0x1b00,0x111c,0x40);'),
        ('checkbox-raw', 'core', '0x90)=U32(object,0x1da4);', '0x90)=!!U32(object,0x1da4);'),
        ('season-action', 'core', 'id==0x1afc?7:id==0x1afb?5:id==0x1afa?6:0', 'id==0x1afc?5:id==0x1afb?7:id==0x1afa?6:0'),
        ('clock-action', 'core', 'id==0x1af8?3:4;', 'id==0x1af8?4:3;'),
        ('clock-convert-direction', 'core', 'time_callback(object,0x1120,0x120);', 'time_callback(object,0x1120,0x11c);'),
        ('date-convert-direction', 'core', 'uint32_t offset=time_checked(object,0x1954)?0x114:0x110;', 'uint32_t offset=time_checked(object,0x1954)?0x110:0x114;'),
        ('date-priority', 'core', 'if(U32(data,0x34))result=0x1afc;', 'if(U32(data,0x34))result=0x1afb;'),
        ('clock-priority', 'core', 'if(U32(data,0x64))result=0x1af8;', 'if(U32(data,0x64))result=0x1af9;'),
        ('error-status-order', 'core', 'clock_status[]={0x64,0x78,0x3c,0x50}', 'clock_status[]={0x78,0x64,0x3c,0x50}'),
        ('error-result', 'core', 'return ((int(TC *)(void *,uint32_t))PTR(PTR(object,0),0xe0))(object,result);', '((int(TC *)(void *,uint32_t))PTR(PTR(object,0),0xe0))(object,result); return (int)result;'),
        ('error-state-raw', 'core', 'U32(object,0x8c)=(uint32_t)value;', 'U32(object,0x8c)=!!value;'),
        ('season-callback', 'core', 'kind==1?0x110:0x118);fs98_world_time_populate', 'kind==1?0x114:0x118);fs98_world_time_populate'),
        ('clock-callback', 'core', 'kind==1?0x110:0x118);fs98_world_time_populate', 'kind==1?0x110:0x11c);fs98_world_time_populate'),
        ('accept-id', 'core', 'else id=U32(page,0x7d0);', 'else id=0;'),
        ('accept-error-vtable', 'core', 'vtable[0xdc/4])(page,(uint32_t)status)', '((void **)PTR(page,0))[0xdc/4])(page,(uint32_t)status)'),
        ('accept-veto', 'core', 'if(!status||!((int(TC *', 'if(1||!((int(TC *'),
        ('accept-answer', 'core', 'U32(data,0)=1;', 'U32(data,0)=0;'),
        ('cancel-mode', 'core', 'U32(object,0x2124)=2;', 'U32(object,0x2124)=1;'),
        ('cancel-answer', 'core', 'U32(data,0)=0;', 'U32(data,0)=1;'),
        ('close-notify-flags', 'core', 'vtable[1])(notify,1);', 'vtable[1])(notify,0);'),
        ('close-tab', 'core', '((void(TC *)(void *))vtable[0xc4/4])(tab);', '(void)tab;'),
        ('frame-validate-result', 'core', ',0,0);return 0; }', ',0,0);return 1; }'),
        ('base-init', 'init', 'ui->base_init(object);vtable=', '(void)object;vtable='),
        ('skin-flags', 'init', 'services->resources(),15)', 'services->resources(),14)'),
        ('skin-failure', 'init', 'if(!((int(TC *', 'if(0&&!((int(TC *'),
        ('artwork', 'init', '0x1368),0xf831,', '0x1368),0xf830,'),
        ('button-font', 'init', '0x126c),AT(object,0x5a0)', '0x126c),AT(object,0x610)'),
        ('tab-base-font', 'init', 'ui->base_font(0)', 'ui->base_font(1)'),
        ('tab-background', 'init', '0,0xc4c4c4,5,', '0,0xffffff,5,'),
        ('frame-bitmap', 'init', '(object,0x150,0x1d,', '(object,0x151,0x1d,'),
        ('page-init-flags', 'init', '(page,9);', '(page,8);'),
        ('notify-size', 'init', ':0x29c);', ':0x298);'),
        ('page-allocation-cleanup', 'init', 'kind?0x25850:0x257f4', 'kind?0x257f4:0x25850'),
        ('tab-add-flags', 'init', '0xf927,title,29);', '0xf925,title,29);'),
        ('last-page', 'init', '(tab,PTR(object,0x2118));', '(tab,PTR(object,0x211c));'),
        ('initial-tab', 'init', '(tab,0,0);', '(tab,1,0);'),
        ('active-tab-owner', 'init', 'PTR(object,0x7c)=tab;', 'PTR(object,0x7c)=object;'),
        ('page-radio-bitmap', 'init', '0x1860,0x134,0x530', '0x1860,0x135,0x530'),
        ('page-check-font', 'init', '0x1ce8,0x139,0x4c0', '0x1ce8,0x139,0x530'),
        ('page-edit-flags', 'init', '0xff0000,AT(object,label),1);', '0xff0000,AT(object,label),0);'),
        ('season-text-font', 'init', '0x14c8),AT(object,0x220)', '0x14c8),AT(object,0x530)'),
        ('clock-text-font', 'init', 'index<8?0x220:0x530', 'index<8?0x530:0x220'),
        ('page-limit', 'init', '0x14)-1);', '0x14));'),
        ('separator-x-y', 'init', 'separator,rect.left,rect.top,', 'separator,rect.top,rect.left,'),
        ('separator-width', 'init', 'rect.right-rect.left,rect.bottom-rect.top', 'rect.bottom-rect.top,rect.right-rect.left'),
        ('page-descriptor-validity', 'init', '||U32(object,descriptor)==0xcdcdcdcd', '||1'),
        ('base-template', 'life', 'object,state.class->template_id,parent);', 'object,0x6aa,parent);'),
        ('constructor-order', 'life', '&state.class->controls[state.constructed]', '&state.class->controls[state.class->count-state.constructed-1]'),
        ('constructor-descriptor', 'life', 'if(kind==2)U32(object,0x1118)=0;', 'if(kind==2)U32(object,0x1118)=1;'),
        ('constructor-spin-vtable', 'life', 'if(kind)PTR(object,kind==1?0x1bc8:0x1db0)=fs98_world_time_api(0x25758);', 'if(0)PTR(object,kind==1?0x1bc8:0x1db0)=fs98_world_time_api(0x25758);'),
        ('parent-dtor-vtable', 'life', 'if(!kind)PTR(object,0)=s_vtables[0];', 'if(0)PTR(object,0)=s_vtables[0];'),
        ('page-dtor-vtable', 'life', 'else PTR(object,kind==1?0x1bc8:0x1db0)=fs98_world_time_api(0x25758);', 'else (void)object;'),
        ('destructor-order', 'life', '&state->class->controls[--state->constructed]', '&state->class->controls[state->class->count-state->constructed--]'),
        ('destructor-base', 'life', 's_services->destroy(state->object,state->class->base_dtor);', '(void)state;'),
        ('delete-flags', 'life', 'if(flags&1)', 'if(flags&2)'),
        ('factory-size', 'life', 'allocate(0x212c)', 'allocate(0x2128)'),
        ('factory-module', 'life', 'U32(object,0x80)=s_module;', 'U32(object,0x80)=0;'),
        ('factory-show', 'life', 's_services->show(object,1);', 's_services->show(object,0);'),
        ('factory-template', 'life', 'create(object,0x6aa,NULL)', 'create(object,0x6a9,NULL)'),
        ('factory-cleanup', 'life', 's_services->dialog_delete(state->object);', 's_services->ui_delete(state->object);'),
        ('ddx-omission', 'life', 'index<s_time_classes[kind].count;', 'index+1<s_time_classes[kind].count;'),
        ('help-command', 'life', '"fs98.hlp",command,', '"fs98.hlp",12+0*command,'),
    ]
    chosen=[item for item in mutations if len(sys.argv)==1 or item[0] in sys.argv[1:]]
    if len(sys.argv)>1 and set(sys.argv[1:])-{item[0] for item in mutations}:raise ValueError('unknown time mutation')
    output=Path('bin/world-diff/time-breaks');output.mkdir(parents=True,exist_ok=True)
    inputs=[*Path('src/world').glob('world_time*'),*Path('tools/difftest').glob('worldtime*diff.c')]
    (output/'inputs.json').write_text(json.dumps({str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},indent=2)+'\n')
    caught=0
    for name,unit,before,after in chosen:
        work=output/name;work.mkdir(exist_ok=True)
        for p in inputs:(work/p.name).write_bytes(p.read_bytes().replace(b'../../src/world/',b''))
        filename='world_time_objects.c' if unit=='life' else 'world_time_init.c' if unit=='init' else 'world_time.c'
        source=(work/filename).read_text()
        expected=2 if name in ('base-template','page-init-flags','initial-tab','clock-text') else 1
        if source.count(before)!=expected:raise ValueError(f'{name}: anchor count {source.count(before)}')
        mutated=source.replace(before,after)
        (work/filename).write_text(mutated)
        harness='worldtimelifediff.c' if unit=='life' else 'worldtimediff.c'
        units=['world_time.c','world_time_objects.c']+([] if unit=='life' else ['world_time_init.c'])
        exe=work/'test.exe'
        build=subprocess.run(['i686-w64-mingw32-gcc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fno-strict-aliasing','-fno-delete-null-pointer-checks','-Isrc/world','-o',str(exe),str(work/harness),*(str(work/name) for name in units),'-lntdll'],capture_output=True,text=True)
        (work/'build.log').write_text(build.stdout+build.stderr)
        if build.returncode:raise RuntimeError(f'{name}: build failure, not counted\n{build.stderr}')
        args=['32','0x4a971b63','typed'] if unit=='life' else ['128','0x4a971b63','unwind']
        result=subprocess.run(['wine',str(exe),*args],capture_output=True,text=True,timeout=180)
        (work/'result.log').write_text(result.stdout+result.stderr)
        (work/'artifact.sha256').write_text(hashlib.sha256(exe.read_bytes()).hexdigest()+'\n')
        if result.returncode!=1 or 'DIFFER' not in result.stderr:raise RuntimeError(f'{name}: no actual difference (exit {result.returncode})\n{result.stdout}{result.stderr}')
        caught+=1;print(f'{name}: detected',flush=True)
    print(f'World time guarded mutations: {caught}/{len(chosen)} actual differences',flush=True)


if __name__=='__main__':main()
