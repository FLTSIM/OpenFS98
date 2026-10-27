#!/usr/bin/env python3
"""Exercise each complexity initializer boundary and CNotify cleanup mutation."""
from pathlib import Path
import os
import subprocess

root=Path(__file__).resolve().parent.parent
output_dir=root/'bin/world-diff/complexity-init-breaks'
output_dir.mkdir(parents=True,exist_ok=True)
source_text=(root/'src/world/world_complexity_init.c').read_text()
mutations=(
 ('base-init','    ui->base_init(object);','    (void)object;'),
 ('skin-bitmap','object, 0x14f,','object, 0x14e,'),
 ('skin-flags','s_services->resources(), 15)','s_services->resources(), 14)'),
 ('skin-failure','if (!((int (TC *','if (0 && !((int (TC *'),
 ('art-bitmap','0x10e4), 0xf831,','0x10e4), 0xf830,'),
 ('art-flags','AT(object, 0x7e8), 2);','AT(object, 0x7e8), 1);'),
 ('ok-button-font','0x11cc), AT(object, 0x5a0),','0x11cc), AT(object, 0x610),'),
 ('cancel-button-art','0x12c8), AT(object, 0x5a0), AT(object, 0x610), 0x132,','0x12c8), AT(object, 0x5a0), AT(object, 0x610), 0x133,'),
 ('check-bitmap','0x4c0), 0x139,','0x4c0), 0x138,'),
 ('check-order','{0x14c4, 0x158c, 0x1654}','{0x158c, 0x14c4, 0x1654}'),
 ('check-omission','index < 3;','index < 2;'),
 ('text-font','0x13c4), AT(object, 0x370)','0x13c4), AT(object, 0x3e0)'),
 ('combo-font','0x147c), AT(object, 0x3e0)','0x147c), AT(object, 0x370)'),
 ('combo-default','0x149c), 0x14e, 0, 0)','0x149c), 0x14e, 1, 0)'),
 ('notify-size','(0x29c);','(0x298);'),
 ('notify-message','notify, 0x30, 1','notify, 0x31, 1'),
 ('notify-flags','notify, 0x30, 1','notify, 0x30, 0'),
 ('notify-vtable','PTR(notify, 0) = s_services->notify_vtable;','PTR(notify, 0) = NULL;'),
 ('notify-cleanup-operator','API(0x257f4, void (SC *)(void *))(state->object);','API(0x25800, void (*)(void *))(state->object);'),
 ('notify-ownership','    state.live = 0;\n    PTR(object, 0x10dc)','    state.live = 1;\n    PTR(object, 0x10dc)'),
 ('populate-omission','fs98_world_complexity_populate(object);','(void)object;'),
 ('init-result','    return 1;','    return 0;'),
 ('notify-delete-flags','if (flags & 1)','if (flags & 2)'),
 ('notify-delete-destructor','    API(0x257bc, void (TC *)(void *))(object);','    (void)object;'),
)
for name,before,after in mutations:
 if source_text.count(before)!=1:raise SystemExit(f'{name}: anchor count changed')
 source_path=output_dir/f'{name}.c';exe=output_dir/f'{name}.exe'
 source_path.write_text(source_text.replace(before,after))
 subprocess.run(['i686-w64-mingw32-gcc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fno-strict-aliasing','-Isrc/world','-o',str(exe),'tools/difftest/worldcomplexityinitdiff.c','src/world/world_complexity.c',str(source_path),'-lntdll'],cwd=root,check=True)
 result=subprocess.run(['wine',str(exe),'64','unwind'],cwd=root,env=os.environ.copy(),capture_output=True,text=True,timeout=120)
 (output_dir/f'{name}.log').write_text(result.stdout+result.stderr)
 if result.returncode!=1 or 'DIFFER scenario=' not in result.stderr:raise SystemExit(f'{name}: no actual difference (exit {result.returncode})\n{result.stderr}')
 print(f'{name}: detected',flush=True)
print(f'World complexity initializer: {len(mutations)}/{len(mutations)} mutations detected')
