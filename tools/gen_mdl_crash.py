#!/usr/bin/env python3
"""Build the three crash models from the decompiled C.

The models are src/mdl/mdl_crash.c (hand-written, one source built per model)
over each model's committed data image (src/mdl/images/<NAME>/, src/mdl/mdl_image.c), into
bin/mdl-crash/models/; the metadata is src/mdl/mdl_manifest.json, and retail is read only for the resources.
bin/mdl-crash/crashes.inc describes the models to tools/difftest/mdlcrashdiff.c.
Generated artifacts stay in ignored bin/mdl-crash/.
"""
import module_flags
import json
from pathlib import Path
import subprocess

from build_mdls import generate, image_object, manifest, project

CRASH_NAMES=('CESSNA182RRG_C','CESSNA182S_C','EXTRA300S_C')


def build_hand(record,directory,model,source='src/mdl/mdl_crash.c',optimize='-O2',defines=(),entry='src/mdl/mdl_crash_entry.S',
               images=None,image_source='src/mdl/mdl_image.c'):
    destination=Path(record['rebuilt']); destination.parent.mkdir(parents=True,exist_ok=True)
    subprocess.run(['i686-w64-mingw32-windres','-J','res','-O','coff',str(directory/'model.res'),str(directory/'resource.o')],check=True)
    image=image_object(record,directory,images,image_source)
    result=subprocess.run(['i686-w64-mingw32-gcc', *module_flags.FLAGS,'-std=c99', '-Itools/difftest',optimize,'-Wall','-Wextra',
            '-ffreestanding','-fno-builtin','-fno-asynchronous-unwind-tables',f'-DCRASH_MODEL={model}',*defines,
            '-Iinclude','-Isrc/mdl','-shared','-nostdlib','-Wl,--entry=_DllMain@12',
            '-Wl,--disable-dynamicbase','-Wl,--disable-nxcompat','-Wl,--section-alignment=65536','-Wl,--image-base='+record['image_base'],
            '-o',str(destination),str(source),str(entry),str(image),
            str(directory/'model.def'),str(directory/'resource.o'),'-lkernel32','-lgcc'],text=True,capture_output=True)
    (directory/'build.log').write_text(result.stdout+result.stderr)
    if result.returncode or result.stderr: raise RuntimeError(result.stdout+result.stderr)


def main():
    output=Path('bin/mdl-crash'); records=[]; entries=manifest('crash')
    if tuple(Path(entry['installed']).stem for entry in entries)!=CRASH_NAMES: raise ValueError('expected the three reference crash models')
    for index,entry in enumerate(entries):
        name=CRASH_NAMES[index]; generate(entry,output/name)
        hand=project(entry,output)
        build_hand(hand,output/name,index)
        print(f'{name}.MDL: decompiled, src/mdl/mdl_crash.c (CRASH_MODEL={index})',flush=True)
        records.append(hand)
    (output/'modules.json').write_text(json.dumps(records,indent=2)+'\n')
    lines=['/* Generated crash model metadata, no retail payloads. */']
    for index,record in enumerate(records):
        lines += [f'static const struct model_reloc crash_relocs_{index}[] = {{']
        lines += [f'{{0x{site:x},0x{target:x}}},' for site,target in record['model_relocs']]
        lines += ['};',f'static const struct model_resource crash_resources_{index}[] = {{']
        lines += ['{%d,%d,%d,%d},' % tuple(r[key] for key in ('type','id','language','size')) for r in record['resources']]
        lines += ['};']
    lines += ['static const struct crash_spec crashes[] = {']
    for index,(record,entry) in enumerate(zip(records,entries)):
        data=record['sections']['.data']
        values=[index,data['rva'],data['size'],entry['import_rva'],record['linkage_rva'],int(record['header_rva'],16),record['active_end']]
        flag=0x80 if index==1 else 0x7e
        lines += ['{{'+json.dumps(str(Path(record['path']).resolve()))+','+json.dumps(record['rebuilt'])+','+','.join(hex(v) for v in values)
                  +f',crash_relocs_{index},{len(record["model_relocs"])},crash_resources_{index},{len(record["resources"])}}},'
                  +f'0x{record["chain_rva"]:x},0x{flag:x},0x{flag+2:x}}},']
    lines += ['};']
    (output/'crashes.inc').write_text('\n'.join(lines)+'\n')


if __name__=='__main__': main()
