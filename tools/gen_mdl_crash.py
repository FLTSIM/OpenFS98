#!/usr/bin/env python3
"""Build the three crash models from the decompiled C.

The models are src/mdl/mdl_crash.c (hand-written, one source built per model)
over each model's generated data image, into bin/mdl-crash/models/, and
bin/mdl-crash/crashes.inc describes them to tools/difftest/mdlcrashdiff.c.
Generated model data and DLLs stay in ignored bin/mdl-crash/.
"""
import module_flags
import json
from pathlib import Path
import re
import subprocess

from build_mdls import generate
from dialogmap import Fs98Pe
from mdl_survey import survey

CRASH_NAMES=('CESSNA182RRG_C','CESSNA182S_C','EXTRA300S_C')


def chain_callback(path, pe):
    """The CHAIN callback: the code address pushed just before list 15 (push 0xf)."""
    output = subprocess.check_output(['objdump', '-d', '-M', 'intel', '--no-show-raw-insn', str(path)], text=True)
    instructions = []
    for line in output.splitlines():
        match = re.fullmatch(r'\s*([0-9a-f]+):\s+(\w+)\s*(.*)', line)
        if match:
            instructions.append((match[2], match[3].strip()))
    candidates = {int(previous[1], 0) - pe.uiBase
                  for previous, (op, args) in zip(instructions, instructions[1:])
                  if op == 'push' and args == '0xf' and previous[0] == 'push'}
    if len(candidates) != 1:
        raise ValueError('CHAIN callback not unique')
    return candidates.pop()


def hand_record(path,directory,survey_record):
    """The decompiled model's data image, its code pointers bound to mdl_crash.c's entries."""
    bindings={int(survey_record['linkage'][slot],16):symbol for slot,symbol in
              (('0x4','_crash_nop'),('0x8','_crash_nop'),('0x18','_crash_attach@4'),
               ('0x1c','_crash_init@4'),('0x20','_crash_detach'))}
    bindings[int(survey_record['update_rva'],16)]='_crash_update_entry'
    return generate(path,directory,8,bindings)


def build_hand(record,directory,model,source='src/mdl/mdl_crash.c',optimize='-O2',defines=(),entry='src/mdl/mdl_crash_entry.S'):
    destination=Path(record['rebuilt']); destination.parent.mkdir(parents=True,exist_ok=True)
    subprocess.run(['i686-w64-mingw32-windres','-J','res','-O','coff',str(directory/'model.res'),str(directory/'resource.o')],check=True)
    result=subprocess.run(['i686-w64-mingw32-gcc', *module_flags.FLAGS,'-std=c99', '-Itools/difftest',optimize,'-Wall','-Wextra',
            '-ffreestanding','-fno-builtin','-fno-asynchronous-unwind-tables',f'-DCRASH_MODEL={model}',*defines,
            '-Iinclude','-Isrc/mdl','-shared','-nostdlib','-Wl,--entry=_DllMain@12',
            '-Wl,--disable-dynamicbase','-Wl,--disable-nxcompat','-Wl,--section-alignment=65536','-Wl,--image-base='+record['image_base'],
            '-o',str(destination),str(source),str(entry),str(directory/'model_data.S'),
            str(directory/'model.def'),str(directory/'resource.o'),'-lkernel32','-lgcc'],text=True,capture_output=True)
    (directory/'build.log').write_text(result.stdout+result.stderr)
    if result.returncode or result.stderr: raise RuntimeError(result.stdout+result.stderr)


def main():
    output=Path('bin/mdl-crash'); records=[]
    paths=sorted(Path('orig/Aircraft').rglob('*_C.MDL'))
    if tuple(path.stem for path in paths)!=CRASH_NAMES: raise ValueError('expected the three reference crash models')
    for path in paths:
        hand=hand_record(path,output/path.stem,survey(path))
        hand['rebuilt']=str((output/'models'/path.name).resolve())
        build_hand(hand,output/path.stem,CRASH_NAMES.index(path.stem))
        print(f'{path.name}: decompiled, src/mdl/mdl_crash.c (CRASH_MODEL={CRASH_NAMES.index(path.stem)})',flush=True)
        hand['chain_rva']=chain_callback(path,Fs98Pe(path))
        records.append(hand)
    if len(records)!=3: raise ValueError('expected three crash models')
    (output/'modules.json').write_text(json.dumps(records,indent=2)+'\n')
    lines=['/* Generated crash model metadata, no retail payloads. */']
    for index,record in enumerate(records):
        lines += [f'static const struct model_reloc crash_relocs_{index}[] = {{']
        lines += [f'{{0x{site:x},0x{target:x}}},' for site,target in record['model_relocs']]
        lines += ['};',f'static const struct model_resource crash_resources_{index}[] = {{']
        lines += ['{%d,%d,%d,%d},' % tuple(r[key] for key in ('type','id','language','size')) for r in record['resources']]
        lines += ['};']
    lines += ['static const struct crash_spec crashes[] = {']
    for index,record in enumerate(records):
        data=record['sections']['.data']; pe=Fs98Pe(Path(record['path']))
        values=[index,data['rva'],data['size'],pe.exports()['ImportTable'],record['linkage_rva'],int(record['header_rva'],16),record['active_end']]
        flag=0x80 if index==1 else 0x7e
        lines += ['{{'+json.dumps(str(Path(record['path']).resolve()))+','+json.dumps(record['rebuilt'])+','+','.join(hex(v) for v in values)
                  +f',crash_relocs_{index},{len(record["model_relocs"])},crash_resources_{index},{len(record["resources"])}}},'
                  +f'0x{record["chain_rva"]:x},0x{flag:x},0x{flag+2:x}}},']
    lines += ['};']
    (output/'crashes.inc').write_text('\n'.join(lines)+'\n')


if __name__=='__main__': main()
