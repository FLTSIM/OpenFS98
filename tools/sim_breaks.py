#!/usr/bin/env python3
"""Require real retail differences for independent Sim mistakes.

Snapshot all inputs before building: ongoing Sim work cannot change a mutation's
headers or harness halfway through the run. Each worker has its own source tree
and process; the tests make no real registry, timer, window or game writes.
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import json
import hashlib
import threading
import subprocess
import sys


def main():
    count=int(sys.argv[1]) if len(sys.argv)>1 else 100
    requested=set(sys.argv[2:])
    if count<100:raise ValueError('use at least 100 scenarios')
    directory=Path('bin/sim-diff/breaks')
    directory.mkdir(parents=True,exist_ok=True)
    sources={p.name:p.read_text() for p in Path('src/sim').iterdir() if p.suffix in ('.c','.h','.inc')}
    sources['simdiff.c']=Path('tools/difftest/simdiff.c').read_text().replace('../../src/sim/','')
    (directory/'inputs.json').write_text(json.dumps({k:hashlib.sha256(v.encode()).hexdigest() for k,v in sources.items()},indent=2)+'\n')
    mutations=[
        ('factory-size','objects','normal','allocate(class_info->size)','allocate(class_info->size - 4)'),
        ('factory-module','objects','normal','D32(object, 0x80) = s_module;','D32(object, 0x80) = s_module ^ 1;'),
        ('factory-page','objects','normal','D32(object, 0x1fa8) = page;','D32(object, 0x1fa8) = page ^ 1;'),
        ('factory-parent','objects','normal','fs98_sim_construct(object, 0, kind)','fs98_sim_construct(object, (void *)parent, kind)'),
        ('factory-show','objects','normal','show(object, 1)','show(object, 0)'),
        ('constructor-parent','objects','normal','state.class_info->template, parent)','state.class_info->template, (void *)((uint32_t)parent & 0))'),
        ('deleting-flag','objects','normal','if (flags & 1)','if (flags & 2)'),
        ('constructor-unwind-base','objects','unwind','    state.base = 1;','    state.base = 0;'),
        ('unwind-destructor','objects','unwind','unwinding ? control->unwind_destructor : control->destructor','control->destructor'),
        ('factory-unwind-delete','objects','unwind','dialog_delete(state->object)','ui_delete(state->object)'),
        ('ddx-id','events','normal','s_ddx[kind][index][1],','s_ddx[kind][index][1] ^ 1,'),
        ('help-command','events','normal','"fs98.hlp", 0xc,','"fs98.hlp", 0xd,'),
        ('context-command','events','normal','"fs98.hlp", 0xa,','"fs98.hlp", 0xb,'),
        ('crash-tick-operation','events','normal','fs98_sim_crash_callback(object, 1)','fs98_sim_crash_callback(object, 0)'),
        ('crash-accept-answer','events','normal','D32(object, 0x16b0) = 1; D32(descriptor, 0) = 1;','D32(object, 0x16b0) = 1; D32(descriptor, 0) = 0;'),
        ('crash-cancel-state','events','normal','D32(object, 0x16b0) = 2;','D32(object, 0x16b0) = 1;'),
        ('crash-statistics','events','normal','AT(object, 0x1540), PTR(PTR(object, 0x112c), 0x14)','AT(object, 0x1540), PTR(PTR(object, 0x112c), 0x18)'),
        ('post-destroy-flags','events','normal',')(object, 1);',')(object, 0);'),
        ('damage-mode','events','normal','D32(PTR(object, 0x1118), 8) = mode;','D32(PTR(object, 0x1118), 8) = mode ^ 1;'),
        ('engine-count-threshold','events','normal','if (count >= 3','if (count > 3'),
        ('engine-count-first','events','normal','D32(object, 0x1db0) ? 1 : 0','D32(object, 0x1db0) ? 2 : 0'),
        ('engine-checkbox','events','normal','AT(object, 0x1dbc), 1','AT(object, 0x1cf4), 1'),
        ('engine-commit-gate','events','normal','if (!D32(object, 0x1100)) return 0;','if (D32(object, 0x1100)) return 0;'),
        ('combo-message','events','normal',',0x147,0,0)',',0x148,0,0)'),
        ('reliability-state','events','normal','D32(PTR(object,0x1118),0x18)=D32(object,0x1884);','D32(PTR(object,0x1118),0x18)=D32(object,0x194c);'),
        ('reliability-feature','events','normal',',0x1c)&8)',',0x1c)&4)'),
        ('settings-answer','events','normal','D32(object,0x2268)=2; D32(descriptor,0)=0;','D32(object,0x2268)=2; D32(descriptor,0)=1;'),
        ('settings-nested-clear','events','normal','PTR(PTR(object,0x2258),0x1124)=0;','PTR(PTR(object,0x2258),0x1124)=(void *)1;'),
        ('settings-validation-vtable','events','normal','PTR(vtable,0xdc))(page,error)','PTR(PTR(page,0),0xdc))(page,error)'),
        ('page-widget-bitmap','events','normal','object,0x115c,0x4c0,0x139','object,0x115c,0x4c0,0x138'),
        ('reliability-slider-cell','events','normal','6,0x1a,6,8,1,-1,g_sim_resource_module','6,0x1b,6,8,1,-1,g_sim_resource_module'),
        ('notify-allocator','crash','normal','if(flags&1)g_sim_services->ui_delete(object);','if(flags&1)g_sim_services->mfc_delete(object);'),
        ('static-allocator','crash','normal','if(flags&1)g_sim_services->mfc_delete(object);','if(flags&1)g_sim_services->ui_delete(object);'),
        ('crash-debug-pointer','crash','normal','(uint32_t)descriptor!=0xcdcdcdcd','(uint32_t)descriptor!=0xcccccccc'),
        ('crash-skin-flags','crash','normal','g_sim_resource_module,0xf','g_sim_resource_module,0xe'),
        ('crash-art','crash','normal','0xf831,AT(object,0xff4)','0xf830,AT(object,0xff4)'),
        ('crash-notify-size','crash','normal','allocate(0x29c)','allocate(0x298)'),
        ('crash-notify-unwind','crash','unwind','notify_delete(state->notify)','ui_delete(state->notify)'),
        ('crash-plot-id','crash','normal','get_control(object,0x13bc)','get_control(object,0x13bd)'),
        ('paint-first-ready','crash','normal','if(D32(descriptor,0x2c))','if(!D32(descriptor,0x2c))'),
        ('paint-caption-id','crash','normal','0xf7b4:0xf7bd','0xf7b5:0xf7bd'),
        ('paint-invalidation','crash','normal','invalidate(D32(object,0x20),0,1)','invalidate(D32(object,0x20),0,0)'),
        ('paint-axis-step','crash','normal','(D32(descriptor,0x24)-1)','(D32(descriptor,0x24)+1)'),
        ('paint-label-font','crash','normal','fs98_sim_label(AT(object,0x300)','fs98_sim_label(AT(object,0x220)'),
        ('paint-axis-layout','crash','normal','rect.left=26;rect.top=64;','rect.left=25;rect.top=64;'),
        ('paint-alternation','crash','normal','+step-(index&1)','+step-(index&0)'),
        ('paint-divide-thirds','crash','normal','((int32_t)step/3)','((int32_t)step/4)'),
        ('paint-unwind','crash','unwind','g_sim_services->paint_destroy(state->dc)','fs98_sim_empty(state->dc)'),
    ]
    mutations += [
        ('engine-caption-type','engines','normal','0xf7b2:0xf7b1','0xf7b1:0xf7b2'),
        ('engine-combo-reset','engines','normal',',0x14b,0,0)',',0x14c,0,0)'),
        ('engine-combo-insertion-count','engines','normal','for(j=0;j<4;++j)','for(j=0;j<3;++j)'),
        ('engine-turbine-recipe','engines','normal','strings=s_engine_turbine_strings','strings=s_engine_piston_strings'),
        ('engine-populate-count','engines','normal','U16(descriptor,0x24)<i+1','U16(descriptor,0x24)<=i+1'),
        ('engine-populate-type-control','engines','normal','?0x190c:0x19d4','?0x19d4:0x190c'),
        ('engine-init-bitmap','engines','normal','0x1dbc-i*0xc8,0x4c0,0x139','0x1dbc-i*0xc8,0x4c0,0x138'),
    ]
    mutations += [
        ('systems-init-font','systems','normal','AT(object,0x3e0),AT(object,label)','AT(object,0x370),AT(object,label)'),
        ('systems-commit-gate','systems','normal','if(!D32(object,0x1100))return 0;','if(D32(object,0x1100))return 0;'),
        ('systems-feature-mask','systems','normal','&s_system_features[i][0]','&(s_system_features[i][0]^1)'),
        ('systems-second-control','systems','normal','if(s_system_features[i][2])','if(!s_system_features[i][2])'),
        ('realism-edit-flags','realism','normal','AT(object,0x1584),0x15','AT(object,0x1584),0x11'),
        ('realism-first-converter','realism','normal','PTR(descriptor,0x70)','PTR(descriptor,0x6c)'),
        ('realism-mode-reset','realism','normal','D32(PTR(object,0x1120),0x20)=0;','D32(PTR(object,0x1120),0x20)=1;'),
        ('realism-error-id','realism','normal',')(object,0x13bd)',')(object,0x13be)'),
        ('realism-feature-gate','realism','normal','&0x20))','&0x10))'),
        ('fuel-availability-exact','fuel','normal','if (D32(object, 0x28f8 + index * 4) == 1) {\n            if (selection == available)','if (D32(object, 0x28f8 + index * 4) != 0) {\n            if (selection == available)'),
        ('fuel-unavailable-sentinel','fuel','normal','    return 8;','    return 7;'),
        ('fuel-manual-second-control','fuel','normal','enable_control(AT(object, 0x1c3c)','enable_control(AT(object, 0x1b84)'),
        ('fuel-validation-gate','fuel','normal','!D32(object, 0x1100) || !control_id','D32(object, 0x1100) || !control_id'),
        ('fuel-validation-raw-selection','fuel','normal','0x10) = result;','0x10) = fs98_sim_fuel_selection(object, result);'),
        ('fuel-validation-delayed-row','fuel','normal','if (group != 1 || row < 3)','if (group != 1 || row < 4)'),
        ('fuel-validation-error-order','fuel','normal','if (D32(descriptor, 0x7c))\n                group = 1;','if (D32(descriptor, 0x7c))\n                group = 0;'),
        ('fuel-validation-edit-modify','fuel','normal','0xb9, 0, 0','0xb8, 0, 0'),
        ('fuel-validation-converter','fuel','normal','mode == 2 ? 0x15c','mode == 2 ? 0x158'),
        ('fuel-error-retry-id','fuel','normal',')(object, display)',')(object, 0)'),
        ('fuel-init-final-label','fuel','normal','index == 4 ? 3 : index','index'),
        ('fuel-init-string-length','fuel','normal','0x280c - index * 0x68','0x2808 - index * 0x68'),
        ('fuel-init-edit-type','fuel','normal','offset + 0x64) = 3','offset + 0x64) = 2'),
        ('fuel-populate-units','fuel','normal','0xf7b6 : 0xf7b5','0xf7b5 : 0xf7b6'),
        ('fuel-populate-row-limit','fuel','normal','0x84 + row * 0x14) - 1','0x84 + row * 0x14) + 1'),
        ('fuel-populate-disable-label','fuel','normal','index < 5; ++index)\n                g_sim_services->enable_control','index < 4; ++index)\n                g_sim_services->enable_control'),
        ('settings-wire-fuel-record','settings','normal','AT(master, 0x164)','AT(master, 0x168)'),
        ('settings-wire-realism-owner','settings','normal','PTR(realism, 0x1124) = object','PTR(realism, 0x1124) = master'),
        ('settings-init-skin','settings','normal','0x14f, g_sim_resource_module, 0xf','0x14e, g_sim_resource_module, 0xf'),
        ('settings-init-font-module','settings','normal','0x47, -1, tab_module, 0','0x47, -1, (tab_module & 0) | g_sim_resource_module, 0'),
        ('settings-init-saved-vtable','settings','normal','PTR(vtable, 0xe8)','PTR(PTR(object, 0), 0xe8)'),
        ('settings-init-page-flags','settings','normal','init_page(page, 9)','init_page(page, 8)'),
        ('settings-init-tab-flag','settings','normal','s_descriptors[index]))','s_descriptors[index]) ^ 1)'),
        ('settings-init-helper-count','settings','normal','index < 6; ++index)\n        g_sim_services->init_tab_helper','index < 5; ++index)\n        g_sim_services->init_tab_helper'),
        ('settings-init-rectangle-point','settings','normal','(int *)AT(object, 0x10e4)','(int *)AT(object, 0x10dc)'),
        ('settings-init-unwind-delete','settings','unwind','dialog_delete(state->page)','ui_delete(state->page)'),
    ]
    wine_lock=threading.Lock()
    jobs=[]
    for name,unit,mode,before,after in mutations:
        if requested and name not in requested:continue
        filename=f'sim_{unit}.c'
        if before not in sources[filename]:raise ValueError(f'{name}: anchor missing')
        jobs.append((name,filename,mode,before,after))
    if requested-set(job[0] for job in jobs):raise ValueError('unknown mutation name')

    def check(job):
        name,filename,mode,before,after=job
        work=directory/name
        work.mkdir(exist_ok=True)
        for key,value in sources.items():
            (work/key).write_text(value.replace(before,after,1) if key==filename else value)
        executable=work/'test.exe'
        executable.unlink(missing_ok=True)
        build=subprocess.run(['i686-w64-mingw32-gcc','-std=c99','-O2','-Wall','-Wextra','-Werror',
            '-fno-strict-aliasing','-o',str(executable),str(work/'simdiff.c'),
            *(str(work/f'sim_{unit}.c') for unit in ('events','objects','crash','engines','systems','realism','fuel','settings')),'-lntdll'],text=True,capture_output=True)
        if build.returncode:
            print(f'{name}: BUILD FAILED (not counted)\n{build.stderr}',flush=True);return False
        with wine_lock:
            # Serialize Wine launches in this module's private prefix. Retry
            # a startup timeout once; only an actual DIFFER can count.
            for attempt in range(2):
                try:
                    run=subprocess.run(['wine',str(executable),str(count),'0x73a915e2',mode],text=True,capture_output=True,timeout=120)
                    break
                except subprocess.TimeoutExpired as error:
                    (work/f'timeout-{attempt}.log').write_bytes((error.stdout or b'')+(error.stderr or b''))
                    if attempt:
                        print(f'{name}: TIMEOUT (not counted)',flush=True);return False
        output=run.stdout+run.stderr
        (work/'result.log').write_text(output)
        caught=run.returncode==1 and 'DIFFER scenario=' in output
        print(f'{name}: '+('caught' if caught else f'NOT CAUGHT (exit {run.returncode})\n{output}'),flush=True)
        return caught

    with ThreadPoolExecutor(max_workers=2) as pool:
        results=list(pool.map(check,jobs))
    caught=sum(results)
    print(f'Sim mutations: {caught}/{len(jobs)} caught',flush=True)
    return 0 if caught==len(jobs) else 1


if __name__=='__main__':sys.exit(main())
