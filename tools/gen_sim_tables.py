#!/usr/bin/env python3
"""Extract Sim control lifetimes, exchange and help metadata; no code/assets."""
from pathlib import Path
import re
import subprocess
from dialogmap import Fs98Pe, SurveyError

# Entry points are independently checked against the vtables and instruction
# patterns below. Ordering is crash, six settings pages, outer settings dialog.
classes = (
    ('crash', 0x1420, 0x14f0, 0x1510, 0x15c0, 0x1650, 0x16b4),
    ('damage', 0x1d60, 0x1e40, 0x1e60, 0x1f30, 0x1fd0, 0x1758),
    ('engines', 0x24d0, 0x2660, 0x2680, 0x27f0, 0x2980, 0x1e84),
    ('fuel', 0x3630, 0x39c0, 0x39e0, 0x3cf0, 0x4070, 0x292c),
    ('systems', 0x5430, 0x55e0, 0x5600, 0x57b0, 0x5980, 0x207c),
    ('realism', 0x6170, 0x62d0, 0x62f0, 0x6450, 0x65d0, 0x2240),
    ('reliability', 0x6bc0, 0x6c70, 0x6c90, 0x6d30, 0x6db0, 0x1958),
    ('settings', 0x6fd0, 0x70d0, 0x70f0, 0x7200, 0x72d0, 0x226c))


def instructions(start, end):
    asm = subprocess.check_output(['objdump', '-d', '--no-show-raw-insn', '-M', 'intel',
        f'--start-address={0x223c0000 + start}', f'--stop-address={0x223c0000 + end}',
        'orig/English/Sim.DLL'], text=True)
    return [match[1].strip() for line in asm.splitlines()
            if (match := re.match(r'\s*[0-9a-f]+:\s+(.+)', line))]


def object_calls(instructions):
    registers = {'ecx': ('object', 0)}
    calls = []
    for op in instructions:
        match = re.fullmatch(r'lea\s+(\w+),\[esi\+0x([0-9a-f]+)\]', op)
        if match:
            registers[match[1]] = ('object', int(match[2], 16)); continue
        match = re.fullmatch(r'mov\s+(e\w+),(e\w+)', op)
        if match:
            registers[match[1]] = registers.get(match[2]); continue
        match = re.fullmatch(r'mov\s+(\w+),DWORD PTR ds:0x223c([0-9a-f]+)', op)
        if match:
            registers[match[1]] = ('import', int(match[2], 16)); continue
        if op.startswith('call'):
            match = re.search(r'ds:0x223c([0-9a-f]+)', op)
            target = op.split()[-1]
            if match: import_slot = int(match[1], 16)
            elif target in ('0x223c7cde', '0x223c7ce4'):
                import_slot = 0xd390 if target.endswith('de') else 0xd38c
            else:
                value = registers.get(target)
                if not value or value[0] != 'import': raise SurveyError(f'unknown object call {op}')
                import_slot = value[1]
            object = registers.get('ecx')
            if not object or object[0] != 'object': raise SurveyError('unknown subobject offset')
            calls.append((object[1], import_slot))
    return calls


def control_vtables(asm):
    registers = {'ecx': ('object', 0)}
    stores = {}
    for op in asm:
        match = re.fullmatch(r'lea\s+(\w+),\[esi\+0x([0-9a-f]+)\]', op)
        if match:
            registers[match[1]] = ('object', int(match[2], 16)); continue
        match = re.fullmatch(r'mov\s+(e\w+),(e\w+)', op)
        if match:
            registers[match[1]] = registers.get(match[2]); continue
        match = re.fullmatch(r'mov\s+(e\w+),(?:DWORD PTR )?ds:0x223cd514', op)
        if match:
            registers[match[1]] = ('vtable', 2); continue
        match = re.fullmatch(r'mov\s+DWORD PTR \[(e\w+)\],(0x223ca450|e\w+)', op)
        if match:
            obj = registers.get(match[1])
            value = ('vtable', 1) if match[2] == '0x223ca450' else registers.get(match[2])
            if obj and obj[0] == 'object' and value and value[0] == 'vtable':
                stores[obj[1]] = value[1]
    return stores


def cleanup_imports(pe, asm):
    handlers = [int(m[1],16) for op in asm[:5]
                if (m := re.fullmatch(r'push\s+0x223c([0-9a-f]+)',op))]
    if len(handlers) != 1: raise SurveyError('unknown EH registration')
    handler = handlers[0]
    metadata = pe.va_uint32(pe.uiBase + handler + 1)
    if pe.va_uint32(metadata) != 0x19930520: raise SurveyError('unknown EH metadata')
    count, table = pe.va_uint32(metadata+4), pe.va_uint32(metadata+8)
    if count > 80: raise SurveyError('unreasonable unwind state count')
    imports = {}
    for i in range(count):
        previous, action = pe.va_uint32(table+i*8), pe.va_uint32(table+i*8+4)
        if previous != (i-1)&0xffffffff: raise SurveyError('nonlinear unwind states')
        code = instructions(action-pe.uiBase, action-pe.uiBase+24)
        offset = 0
        for op in code:
            m = re.fullmatch(r'add\s+ecx,0x([0-9a-f]+)',op)
            if m: offset=int(m[1],16)
            m = re.fullmatch(r'jmp\s+DWORD PTR ds:0x223c([0-9a-f]+)',op)
            if m: imports[offset]=int(m[1],16);break
            if op == 'jmp    0x223c7ce4': imports[offset]=0xd38c;break
            if op.startswith('jmp'): raise SurveyError(f'unknown cleanup target {op}')
        else: raise SurveyError('unterminated unwind action')
    return imports


def descriptor_copies(start, end, descriptor_offset):
    registers = {}
    writes, combos = [], []
    window = None
    for op in instructions(start,end):
        m = re.fullmatch(r'mov\s+(e\w+),DWORD PTR \[esi\+0x([0-9a-f]+)\]',op)
        if m:
            field=int(m[2],16)
            registers[m[1]]=('descriptor',0) if field==descriptor_offset else ('field',field)
            if field!=descriptor_offset:window=field
        if op=='call   edi': registers['eax']=('selection',window)
        m = re.fullmatch(r'mov\s+DWORD PTR \[(e\w+)\+0x([0-9a-f]+)\],(e\w+)',op)
        if m and registers.get(m[1])==('descriptor',0):
            source=registers.get(m[3])
            if source and source[0]=='field': writes.append((source[1],int(m[2],16)))
            if source and source[0]=='selection': combos.append((source[1],int(m[2],16)))
    return writes,combos


def main():
    pe = Fs98Pe('orig/English/Sim.DLL')
    if pe.uiBase != 0x223c0000 or pe.linkage()['id'] != 0x80000004:
        raise SurveyError('expected reference Sim')
    lines = ['/* Generated by tools/gen_sim_tables.py; ABI metadata only. */']
    metadata = []
    base_hooks, ctor_hooks, dtor_hooks = set(), set(), set()
    for name, ctor_rva, delete, dtor_rva, ddx_rva, getter, size in classes:
        ctor_asm = instructions(ctor_rva, delete)
        dtor_asm = instructions(dtor_rva, ddx_rva)
        dtor_asm = dtor_asm[:next(i+1 for i,s in enumerate(dtor_asm) if s.startswith('ret'))]
        ctor = object_calls(ctor_asm)
        dtor = list(reversed(object_calls(dtor_asm)))
        if [a[0] for a in ctor] != [a[0] for a in dtor]:
            raise SurveyError(f'{name}: destruction is not reversed construction')
        base_ctor, base_dtor = ctor[0][1], dtor[0][1]
        base_hooks.add(base_ctor)
        ctor_hooks.update(a[1] for a in ctor[1:])
        dtor_hooks.update(a[1] for a in dtor)
        unwind = cleanup_imports(pe,ctor_asm)
        other_unwind = cleanup_imports(pe,dtor_asm)
        if unwind != other_unwind: raise SurveyError(f'{name}: constructor/destructor EH differ')
        dtor_hooks.update(unwind.values())
        if base_ctor not in (0xd578, 0xd510) or base_dtor not in (0xd588, 0xd520):
            raise SurveyError(f'{name}: unexpected base')
        vtable = [int(p[1],16) for s in ctor_asm
                   if (p := re.fullmatch(r'mov\s+DWORD PTR \[esi\],0x223c([0-9a-f]+)',s))]
        if len(vtable) != 1 or pe.va_uint32(pe.uiBase + vtable[0] + 4) != pe.uiBase + delete:
            raise SurveyError(f'{name}: vtable moved')
        template = [int(ctor_asm[i-2].split()[-1],16) for i,s in enumerate(ctor_asm)
                     if s == f'call   DWORD PTR ds:0x223c{base_ctor:x}']
        if len(template) != 1: raise SurveyError('unknown constructor template')
        stores = [(int(p[1],16),int(p[2],16)) for s in ctor_asm
                   if (p := re.fullmatch(r'mov\s+DWORD PTR \[esi\+0x([0-9a-f]+)\],0x([0-9a-f]+)',s))]
        reset = int(any(re.fullmatch(r'mov\s+DWORD PTR \[esi\],0x223c[0-9a-f]+',s) for s in dtor_asm))
        ctor_vtables = control_vtables(ctor_asm)
        dtor_vtables = control_vtables(dtor_asm)
        lines.append(f'static const struct fs98_sim_control s_controls_{name}[] = {{')
        lines += [f'    {{0x{offset:x}, 0x{ctor:x}, 0x{dtor:x}, {ctor_vtables.get(offset,0)}, {dtor_vtables.get(offset,0)}, 0x{unwind.get(offset,dtor):x}}},'
                   for (offset,ctor),(_,dtor) in zip(ctor[1:],dtor[1:])]
        lines += ['};', f'static const struct fs98_sim_store s_stores_{name}[] = {{']
        lines += [f'    {{0x{offset:x}, 0x{value:x}}},' for offset,value in stores]
        lines += ['    {0, 0}', '};']
        ddx = []; offset=id=None
        for op in instructions(ddx_rva, getter):
            match=re.search(r'(?:lea\s+\w+,\[esi\+0x|add\s+esi,0x)([0-9a-f]+)',op)
            if match: offset=int(match[1],16)
            match=re.fullmatch(r'push\s+0x([0-9a-f]+)',op)
            if match: id=int(match[1],16)
            if op == 'call   0x223c7c78':
                if offset is None or id is None: raise SurveyError('unrecognised DDX')
                ddx.append((offset,id)); offset=id=None
        if sorted(r[0] for r in ddx) != sorted(r[0] for r in ctor[1:]):
            raise SurveyError(f'{name}: DDX differs from embedded controls')
        lines.append(f'static const uint16_t s_ddx_{name}[][2] = {{')
        lines += [f'    {{0x{offset:x}, 0x{id:x}}},' for offset,id in ddx]
        lines.append('};')
        # The first WM_HELP handler directly names the terminated help table.
        map=pe.va_uint32(pe.uiBase+getter+1)-pe.uiBase
        entries=pe.va_uint32(pe.uiBase+map+4)
        help_rva=None
        for i in range(64):
            if pe.va_uint32(entries+i*24)==0x53:
                help_rva=pe.va_uint32(entries+i*24+20)-pe.uiBase;break
        if help_rva is None: raise SurveyError('missing WM_HELP')
        help_asm=instructions(help_rva,help_rva+0x20)
        help=[int(p[1],16) for s in help_asm if (p:=re.fullmatch(r'push\s+0x223c([0-9a-f]+)',s))]
        if len(help)!=2: raise SurveyError('help table moved')
        table=help[0]
        lines.append(f'static const uint32_t s_help_{name}[] = {{')
        for i in range(128):
            control=pe.va_uint32(pe.uiBase+table+i*8); topic=pe.va_uint32(pe.uiBase+table+i*8+4)
            lines.append(f'    0x{control:x}, 0x{topic:x},')
            if not control and not topic: break
        else: raise SurveyError('unterminated help')
        lines.append('};')
        metadata.append((name,[size,template[0],ctor_rva,dtor_rva,delete,vtable[0],base_ctor,base_dtor,reset,len(ctor)-1,ddx_rva,help_rva,help_rva+0x20]))
    for name,start,end,expected in (('piston',0x3079,0x3232,5),('turbine',0x3232,0x3314,3)):
        ids=[int(m[1],16) for op in instructions(start,end)
             if (m:=re.fullmatch(r'push\s+0x(f[0-9a-f]{3})',op)) and int(m[1],16)!=0xffffffff]
        if len(ids)!=expected: raise SurveyError('engine string recipe changed')
        lines.append(f'static const uint16_t s_engine_{name}_strings[] = {{'+', '.join(f'0x{x:x}' for x in ids)+'};')
    engine_writes,engine_combos=descriptor_copies(0x2ee3,0x3008,0x111c)
    if len(engine_writes)!=8 or len(engine_combos)!=5: raise SurveyError('engine exchange moved')
    for name,rows in (('engine_writes',engine_writes),('engine_combos',engine_combos)):
        lines.append(f'static const uint16_t s_{name}[][2] = {{')
        lines += [f'    {{0x{source:x},0x{target:x}}},' for source,target in rows]
        lines.append('};')
    lines.append('static const struct fs98_sim_class s_classes[] = {')
    for name,values in metadata:
        lines.append('    {'+', '.join(f'0x{x:x}' for x in values)+f', s_controls_{name}, s_stores_{name}'+'},')
    lines.append('};')
    # Shared static's custom vtable store is always overwritten with SIMSUI's
    # imported CSimsStatic vtable before any external call; retain that value.
    Path('src/sim/sim_tables.inc').write_text('\n'.join(lines)+'\n')
    hooks = ['/* Generated by tools/gen_sim_tables.py; import ABI metadata. */']
    for name, slots in (('BASE_HOOK',base_hooks),('CTOR_HOOK',ctor_hooks),('DTOR_HOOK',dtor_hooks)):
        hooks += [f'{name}({slot:x})' for slot in sorted(slots)]
    Path('src/sim/sim_hooks.inc').write_text('\n'.join(hooks)+'\n')


if __name__ == '__main__': main()
