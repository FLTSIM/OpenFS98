#!/usr/bin/env python3
"""Build the three crash models: the decompiled C, and the lifted oracle.

The models are src/mdl/mdl_crash.c (hand-written, one source built per model)
over each model's generated data image, into bin/mdl-crash/models/. The lifted
build -- C generated from retail's instructions, registers and flags in a
struct -- is kept as a second oracle in bin/mdl-crash/lifted/; mdlcrashdiff
runs it in the `lifted` mode. Objdump provides its instruction boundaries; it
accepts only the observed integer operations and checked service signatures.
Generated sources, model data and DLLs stay in ignored bin/mdl-crash/.
"""
from collections import deque
from bisect import bisect_right
import json
from pathlib import Path
import re
import subprocess

from build_mdls import generate
from dialogmap import Fs98Pe
from mdl_survey import survey
from gen_gauges import relocations

CRASH_NAMES=('CESSNA182RRG_C','CESSNA182S_C','EXTRA300S_C')

REGS = ('eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi')
PARTS = {name: (full, 0, 16) for name, full in zip(('ax','cx','dx','bx','sp','bp','si','di'), REGS)}
PARTS.update({name: (full, shift, 8) for full, low, high in
              [('eax','al','ah'),('ecx','cl','ch'),('edx','dl','dh'),('ebx','bl','bh')]
              for name, shift in [(low,0),(high,8)]})
CONDITIONS = {
    'je': 'f->flags & 0x40', 'jne': '!(f->flags & 0x40)',
    'jb': 'f->flags & 1', 'jae': '!(f->flags & 1)',
    'ja': '!(f->flags & 0x41)', 'jbe': 'f->flags & 0x41',
    'jl': '(!!(f->flags & 0x80) != !!(f->flags & 0x800))',
    'jg': '!(f->flags & 0x40) && (!!(f->flags & 0x80) == !!(f->flags & 0x800))',
    'jle': '(f->flags & 0x40) || (!!(f->flags & 0x80) != !!(f->flags & 0x800))',
    'jns': '!(f->flags & 0x80)', 'js': 'f->flags & 0x80'}


class CrashLift:
    def __init__(self, path):
        self.path = path
        self.pe = Fs98Pe(path)
        self.record = survey(path)
        self.instructions = {}
        output = subprocess.check_output(['objdump', '-d', '-M', 'intel', '--no-show-raw-insn', str(path)], text=True)
        for line in output.splitlines():
            match = re.fullmatch(r'\s*([0-9a-f]+):\s+(\w+)\s*(.*)', line)
            if match: self.instructions[int(match[1], 16) - self.pe.uiBase] = (match[2], match[3].strip())
        addresses = sorted(self.instructions)
        self.next = dict(zip(addresses, addresses[1:]))
        self.relocated_instructions=set()
        for site,_ in relocations(self.pe):
            index=bisect_right(addresses,site)-1
            if index>=0 and site<self.next.get(addresses[index],addresses[index]+1):
                self.relocated_instructions.add(addresses[index])
        native_calls=[]
        for address in addresses:
            op,args=self.instructions[address]
            if op=='call' and args.startswith('DWORD PTR') and 0x1020 <= address < int(self.record['linkage']['0x4'],16):
                match=re.fullmatch(r'DWORD PTR \[eax\+(0x[0-9a-f]+)\]',args)
                if not match: raise ValueError('unrecognized native call form')
                native_calls.append((address,int(match[1],16)))
        # Callee-verified contracts: CHAIN add/remove (2), SIM snapshot (1),
        # HOST memmove (3), G3D program (2), UTIL sincos/metres-to-longitude (3).
        wanted=[(0x18,2,2),(0x18,1,1),(0x1c,2,2),(0x21c,3,3),(0x21c,3,3),(0x18,2,3),(0x18,2,3),
                (0x44,3,7),(0x2c,3,6),(0x2c,3,6),(0x2c,3,6),(0x44,3,7),(0x2c,3,6),(0x44,3,7),(0x2c,3,6)]
        if [slot for _,slot in native_calls] != [slot for slot,_,_ in wanted]:
            raise ValueError('native service sequence changed')
        self.native_args={address:(argc,pointers) for (address,_),(_,argc,pointers) in zip(native_calls,wanted)}
        roots = {int(self.record['linkage'][slot], 16) for slot in ('0x4','0x8','0x18','0x1c','0x20')}
        roots.add(int(self.record['update_rva'], 16))
        # CHAIN callback is the code address pushed immediately before list 15.
        candidates = []
        for index, address in enumerate(addresses[:-1]):
            op, args = self.instructions[address]
            if op == 'push' and args == '0xf':
                prev_op, prev_arg = self.instructions[addresses[index - 1]]
                if prev_op == 'push': candidates.append(int(prev_arg, 0) - self.pe.uiBase)
        if len(set(candidates)) != 1: raise ValueError('CHAIN callback not unique')
        self.chain = candidates[0]
        roots.add(self.chain)
        self.roots = roots
        self.functions = {}
        queue = deque(roots)
        while queue:
            start = queue.popleft()
            if start in self.functions: continue
            body = self.discover(start)
            self.functions[start] = body
            for address in body:
                op, args = self.instructions[address]
                if op == 'call' and args.startswith('0x'):
                    queue.append(int(args, 16) - self.pe.uiBase)
        self.signatures = {}
        for start, body in self.functions.items():
            returns = {int(self.instructions[a][1] or '0', 0) for a in body if self.instructions[a][0] == 'ret'}
            if len(returns) != 1: raise ValueError(f'{path} {start:x}: inconsistent return')
            self.signatures[start] = returns.pop()
        self.flag_cache={}
        for root in self.roots: self.check_flags(root,15)

    def check_flags(self,start,initial):
        key=(start,initial)
        if key in self.flag_cache: return self.flag_cache[key]
        queue=deque([(start,initial)]); visited=set(); returns=set()
        requirements={'je':2,'jne':2,'jb':1,'jae':1,'ja':3,'jbe':3,'jl':12,'jg':14,'jle':14,'js':4,'jns':4,
                      'adc':1,'sbb':1,'rcl':1,'rcr':1}
        while queue:
            address,mask=queue.popleft()
            if (address,mask) in visited: continue
            visited.add((address,mask))
            op,args=self.instructions[address]
            needed=requirements.get(op,0)
            if mask&needed!=needed: raise ValueError(f'{self.path} {address:x}: {op} consumes undefined flags')
            if op in ('add','sub','adc','sbb','cmp','neg','and','or','xor'): mask=15
            elif op in ('inc','dec'): mask=(mask&1)|14
            elif op in ('mul','imul'): mask=9
            elif op in ('shl','shr','sar','shld','shrd'):
                count=int(args.split(',')[-1],0)&31
                if count: mask=15 if count==1 else 7
            elif op in ('rol','rcl','rcr'):
                count=int(args.split(',')[-1],0)&31
                if count: mask=(mask|9) if count==1 else (mask|1)&~8
            elif op in ('clc','stc'): mask|=1
            if op=='ret': returns.add(mask); continue
            if op=='call':
                outputs=self.check_flags(int(args,16)-self.pe.uiBase,mask) if args.startswith('0x') else {15}
                queue.extend((self.next[address],output) for output in outputs)
                continue
            if op=='jmp' or op in CONDITIONS:
                queue.append((int(args,16)-self.pe.uiBase,mask))
                if op=='jmp': continue
            queue.append((self.next[address],mask))
        self.flag_cache[key]=returns
        return returns

    def discover(self, start):
        queue, body = deque([start]), set()
        while queue:
            address = queue.popleft()
            if address in body: continue
            if address not in self.instructions: raise ValueError(f'{self.path}: missing instruction {address:x}')
            body.add(address)
            op, args = self.instructions[address]
            if op == 'ret': continue
            if op == 'jmp' or op in CONDITIONS:
                if not args.startswith('0x'): raise ValueError('indirect branch')
                queue.append(int(args, 16) - self.pe.uiBase)
                if op == 'jmp': continue
            queue.append(self.next[address])
        return body

    def width(self, operand, fallback=32):
        if operand in PARTS: return PARTS[operand][2]
        if operand.startswith('BYTE'): return 8
        if operand.startswith('WORD'): return 16
        if operand.startswith('DWORD') or operand in REGS: return 32
        return fallback

    def address(self, operand):
        operand = re.sub(r'^(BYTE|WORD|DWORD) PTR ', '', operand)
        if operand.startswith('ds:'):
            if self.current_address not in self.relocated_instructions:
                raise ValueError('absolute model address has no PE relocation')
            return f'mdl_address(f, 0x{int(operand[3:],16) - self.pe.uiBase:x})'
        if not operand.startswith('[') or not operand.endswith(']'): raise ValueError(f'not memory: {operand}')
        expression = operand[1:-1]
        def immediate(match):
            value = int(match[0], 16)
            if self.pe.uiBase <= value < self.pe.uiBase + 0x20000 and self.current_address in self.relocated_instructions:
                return f'mdl_address(f, 0x{value - self.pe.uiBase:x})'
            return match[0] + 'u'
        expression = re.sub(r'0x[0-9a-f]+', immediate, expression)
        expression = re.sub(r'\b(e[a-z]{2})\b', lambda m: 'f->' + m[0] if m[0] in REGS else m[0], expression)
        return '(' + expression + ')'

    def read(self, operand, width=32):
        if operand in REGS: return 'f->' + operand
        if operand in PARTS:
            reg, shift, bits = PARTS[operand]
            return f'((f->{reg} >> {shift}) & 0x{(1<<bits)-1:x}u)'
        if operand.startswith(('ds:', '[', 'BYTE ', 'WORD ', 'DWORD ')):
            return f'mdl_read(f, {self.address(operand)}, {self.width(operand,width)})'
        value = int(operand, 0)
        if self.pe.uiBase <= value < self.pe.uiBase + 0x20000 and self.current_address in self.relocated_instructions:
            return f'mdl_address(f, 0x{value - self.pe.uiBase:x})'
        return f'0x{value & 0xffffffff:x}u'

    def write(self, operand, value, width=32):
        if operand in REGS: return f'f->{operand} = (uint32_t)({value});'
        if operand in PARTS:
            reg, shift, bits = PARTS[operand]; mask = ((1<<bits)-1)<<shift
            return f'f->{reg} = (f->{reg} & 0x{~mask & 0xffffffff:x}u) | (((uint32_t)({value}) << {shift}) & 0x{mask:x}u);'
        return f'mdl_write(f, {self.address(operand)}, {value}, {self.width(operand,width)});'

    def lower(self, address):
        self.current_address=address
        op, args = self.instructions[address]
        parts = args.split(','); left = parts[0]; right = parts[1] if len(parts)>1 else None
        width = self.width(left, self.width(right or ''))
        a = lambda: self.read(left, width)
        b = lambda: self.read(right, width)
        store = lambda expression: self.write(left, expression, width)
        if op == 'nop': return ';'
        if op == 'mov': return store(b())
        if op in ('movsx','movzx'):
            value = self.read(right, self.width(right))
            if op == 'movsx': value = f'mdl_signed({value}, {self.width(right)})'
            return store(value)
        if op == 'lea': return store(self.address(right))
        if op == 'push': return f'mdl_push(f, {a()}, {16 if width==16 else 32});'
        if op == 'pop': return store(f'mdl_pop(f, {16 if width==16 else 32})')
        if op == 'leave': return 'f->esp = f->ebp; f->ebp = mdl_pop(f, 32);'
        if op in ('add','sub','adc','sbb','cmp','neg','inc','dec'):
            first = '0' if op=='neg' else a(); second = '1' if op in ('inc','dec') else a() if op=='neg' else b()
            value = f'mdl_arith(f, {first}, {second}, {width}, {int(op in ("sub","sbb","cmp","neg","dec"))}, {"f->flags & 1" if op in ("adc","sbb") else "0"})'
            if op=='cmp': return value + ';'
            if op in ('inc','dec'): return '{ uint32_t carry = f->flags & 1; ' + store(value) + ' f->flags = (f->flags & ~1u) | carry; }'
            return store(value)
        if op in ('and','or','xor'):
            operator = {'and':'&','or':'|','xor':'^'}[op]
            return store(f'mdl_logic(f, {a()} {operator} {b()}, {width})')
        if op in ('shl','shr','sar','rol','rcl','rcr','shld','shrd'):
            kind = ('shl','shr','sar','rol','rcl','rcr','shld','shrd').index(op)
            second = b() if op in ('shld','shrd') else '0'
            count = self.read(parts[2] if op in ('shld','shrd') else right)
            return store(f'mdl_shift(f, {a()}, {second}, {width}, {count}, {kind})')
        if op in ('cbw','cwd','cwde','cdq'):
            if op=='cbw': return self.write('ax','mdl_signed(f->eax & 255, 8)')
            if op=='cwd': return self.write('dx','(f->eax & 0x8000) ? 0xffff : 0')
            if op=='cwde': return 'f->eax = (uint32_t)mdl_signed(f->eax & 65535, 16);'
            return 'f->edx = (f->eax & 0x80000000u) ? UINT32_MAX : 0;'
        if op in ('mul','imul'):
            if len(parts)==1: return f'mdl_multiply(f, {a()}, {width}, {int(op=="imul")});'
            if op!='imul' or len(parts)!=3: raise ValueError('unexpected multiply')
            return '{ int64_t product = mdl_signed(' + b() + f', {width}) * mdl_signed({self.read(parts[2])}, {width}); ' + store('(uint32_t)product') + f' f->flags = (f->flags & ~MDL_ARITH) | (product == mdl_signed({a()}, {width}) ? 0 : 0x801); }}'
        if op=='xchg': return '{ uint32_t old = ' + a() + '; ' + store(b()) + self.write(right,'old',width) + ' }'
        if op in ('stc','clc'): return 'f->flags ' + ('|= 1;' if op=='stc' else '&= ~1u;')
        if op=='rep' and args=='movs BYTE PTR es:[edi],BYTE PTR ds:[esi]':
            return 'while (f->ecx) { mdl_write(f, f->edi, mdl_read(f, f->esi, 8), 8); f->esi += f->flags & 0x400 ? -1u : 1; f->edi += f->flags & 0x400 ? -1u : 1; --f->ecx; }'
        if op in CONDITIONS: return f'if ({CONDITIONS[op]}) goto label_{int(left,16)-self.pe.uiBase:x};'
        if op=='jmp': return f'goto label_{int(left,16)-self.pe.uiBase:x};'
        if op=='ret': return f'f->esp += {4+int(args or "0",0)}; return;'
        if op=='call':
            if left.startswith('0x'):
                target = int(left,16)-self.pe.uiBase
                return f'mdl_push(f, (uint32_t)(uintptr_t)(f->image + 0x{self.next[address]:x}), 32); execute_{target:x}(f);'
            match = re.fullmatch(r'DWORD PTR \[eax\+(0x[0-9a-f]+)\]',left)
            if not match: raise ValueError(f'unknown call {args}')
            argc,pointers=self.native_args[address]
            return f'mdl_native(f, {a()}, {argc}, {pointers});'
        raise ValueError(f'{self.path} {address:x}: unsupported {op} {args}')

    def emit(self, directory):
        bindings = {root: f'_mdl_entry_{root:x}' for root in self.roots}
        record = generate(self.path, directory, 8, bindings)
        code = ['/* Generated by tools/gen_mdl_crash.py: explicit C, no decoded instructions at runtime. */',
                '#include "mdl_crash_lift.h"', 'extern uint8_t mdl_image[];',
                f'uint32_t mdl_coverage[{len(self.functions)}];']
        for root in sorted(self.roots): code += [f'extern void mdl_entry_{root:x}(void);']
        code += ['static uint32_t mdl_address(struct mdl_frame *f, uint32_t rva) { switch (rva) {']
        code += [f'case 0x{root:x}: return (uint32_t)(uintptr_t)mdl_entry_{root:x};' for root in sorted(self.roots)]
        code += ['default: return (uint32_t)(uintptr_t)(f->image + rva); } }']
        code += [f'static void execute_{start:x}(struct mdl_frame *f);' for start in sorted(self.functions)]
        for index,(start, body) in enumerate(sorted(self.functions.items())):
            code += [f'static void execute_{start:x}(struct mdl_frame *f) {{',f'++mdl_coverage[{index}];',f'goto label_{start:x};']
            for address in sorted(body):
                op,args=self.instructions[address]
                code += [f'label_{address:x}: /* {self.pe.uiBase+address:08x}: {op} {args} */',
                         'if (!f->budget--) RaiseException(EXCEPTION_ILLEGAL_INSTRUCTION, 0, 0, NULL);', self.lower(address)]
            code += ['}']
        code += ['void mdl_crash_enter(uint32_t entry, uint32_t *saved) {',
                 'struct mdl_frame f; uint32_t i, argc = 0, expected, *stack;',
                 'stack = HeapAlloc(GetProcessHeap(), 0, 8192);',
                 'if (!stack) { RaiseException(0xc0000017u, 0, 0, NULL); return; }',
                 'for (i = 0; i < 2048; i++) stack[i] = 0xa5a5a5a5u;',
                 'f.eax=saved[7]; f.ecx=saved[6]; f.edx=saved[5]; f.ebx=saved[4];',
                 'f.ebp=saved[2]; f.esi=saved[1]; f.edi=saved[0]; f.flags=saved[8];',
                 'f.image=mdl_image; f.budget=100000; f.esp=saved[3]+4;',
                 'f.stack=(uint8_t *)stack; f.stack_low=f.esp-4096; f.stack_high=f.esp+64;',
                 'switch(entry) {']
        code += [f'case 0x{root:x}: argc={self.signatures[root]//4}; break;' for root in sorted(self.roots)]
        code += ['default: RaiseException(EXCEPTION_ILLEGAL_INSTRUCTION,0,0,NULL); }',
                 'expected=f.esp + 4 + argc*4;',
                 'mdl_write(&f,f.esp,*(uint32_t *)(uintptr_t)f.esp,32);',
                 'for(i=0;i<argc;i++) mdl_write(&f,f.esp+4+i*4,*(uint32_t *)(uintptr_t)(saved[3]+8+i*4),32);',
                 'switch(entry) {']
        code += [f'case 0x{root:x}: execute_{root:x}(&f); break;' for root in sorted(self.roots)]
        code += ['}', 'if(f.esp != expected) RaiseException(EXCEPTION_ILLEGAL_INSTRUCTION,0,0,NULL);',
                 'saved[7]=f.eax; saved[6]=f.ecx; saved[5]=f.edx; saved[4]=f.ebx;',
                 'saved[2]=f.ebp; saved[1]=f.esi; saved[0]=f.edi; saved[8]=f.flags;',
                 'HeapFree(GetProcessHeap(),0,stack); }',
                 'BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved) { (void)reserved; if(reason==DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(instance); return TRUE; }']
        (directory/'crash.c').write_text('\n'.join(code)+'\n')
        assembly=['.text']
        for root in sorted(self.roots):
            n=self.signatures[root]
            assembly += [f'.globl _mdl_entry_{root:x}',f'_mdl_entry_{root:x}:',
                         'pushfl','pushal','pushl %esp',f'pushl $0x{root:x}','call _mdl_crash_enter',
                         'addl $8,%esp','popal','popfl',f'ret ${n}' if n else 'ret']
        (directory/'entries.S').write_text('\n'.join(assembly)+'\n')
        with (directory/'model.def').open('a') as stream: stream.write('    MdlCoverage=mdl_coverage DATA\n')
        record.update(chain_rva=self.chain, entry_args=self.signatures,
                      function_rvas=sorted(self.functions),
                      functions=len(self.functions), instructions=sum(len(b) for b in self.functions.values()))
        return record


def build_crash(record,directory,optimize='-O2'):
    destination=Path(record['rebuilt']); destination.parent.mkdir(parents=True,exist_ok=True)
    subprocess.run(['i686-w64-mingw32-windres','-J','res','-O','coff',str(directory/'model.res'),str(directory/'resource.o')],check=True)
    result=subprocess.run(['i686-w64-mingw32-gcc','-std=c99',optimize,'-Wall','-Wextra','-Wno-unused-label','-Wno-unused-function',
            '-fno-asynchronous-unwind-tables','-Isrc/mdl','-shared','-nostdlib','-Wl,--entry=_DllMain@12',
            '-Wl,--disable-dynamicbase','-Wl,--disable-nxcompat','-Wl,--section-alignment=65536','-Wl,--image-base='+record['image_base'],
            '-o',str(destination),str(directory/'crash.c'),str(directory/'entries.S'),str(directory/'model_data.S'),
            str(directory/'model.def'),str(directory/'resource.o'),'src/mdl/mdl_crash_native.S','-lkernel32','-lgcc'],text=True,capture_output=True)
    (directory/'build.log').write_text(result.stdout+result.stderr)
    if result.returncode or result.stderr: raise RuntimeError(result.stdout+result.stderr)


def hand_record(path,directory,lift):
    """The decompiled model's data image, its code pointers bound to mdl_crash.c's entries."""
    survey_record=lift.record
    bindings={int(survey_record['linkage'][slot],16):symbol for slot,symbol in
              (('0x4','_crash_nop'),('0x8','_crash_nop'),('0x18','_crash_attach@4'),
               ('0x1c','_crash_init@4'),('0x20','_crash_detach'))}
    bindings[int(survey_record['update_rva'],16)]='_crash_update_entry'
    return generate(path,directory,8,bindings)


def build_hand(record,directory,model,source='src/mdl/mdl_crash.c',optimize='-O2',defines=(),entry='src/mdl/mdl_crash_entry.S'):
    destination=Path(record['rebuilt']); destination.parent.mkdir(parents=True,exist_ok=True)
    subprocess.run(['i686-w64-mingw32-windres','-J','res','-O','coff',str(directory/'model.res'),str(directory/'resource.o')],check=True)
    result=subprocess.run(['i686-w64-mingw32-gcc','-std=c99',optimize,'-Wall','-Wextra','-fno-strict-aliasing',
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
        lift=CrashLift(path); directory=output/'lifted-src'/path.stem
        record=lift.emit(directory)
        record['rebuilt']=str((output/'lifted'/path.name).resolve())
        build_crash(record,directory)
        print(f'{path.name}: lifted oracle, {record["functions"]} functions, {record["instructions"]} explicit C operations',flush=True)
        hand=hand_record(path,output/path.stem,lift)
        hand['rebuilt']=str((output/'models'/path.name).resolve())
        build_hand(hand,output/path.stem,CRASH_NAMES.index(path.stem))
        print(f'{path.name}: decompiled, src/mdl/mdl_crash.c (CRASH_MODEL={CRASH_NAMES.index(path.stem)})',flush=True)
        for key in ('chain_rva','entry_args','function_rvas','functions','instructions'): hand[key]=record[key]
        hand['lifted']=record['rebuilt']
        records.append(hand)
    if len(records)!=3: raise ValueError('expected three crash models')
    (output/'modules.json').write_text(json.dumps(records,indent=2)+'\n')
    lines=['/* Generated crash model metadata, no retail payloads. */']
    for index,record in enumerate(records):
        lines += [f'static const struct model_reloc crash_relocs_{index}[] = {{']
        lines += [f'{{0x{site:x},0x{target:x}}},' for site,target in record['model_relocs']]
        lines += ['};',f'static const struct model_resource crash_resources_{index}[] = {{']
        lines += ['{%d,%d,%d,%d},' % tuple(r[key] for key in ('type','id','language','size')) for r in record['resources']]
        lines += ['};',f'static const uint32_t crash_functions_{index}[] = {{'+','.join(hex(rva) for rva in record['function_rvas'])+'};']
    lines += ['static const struct crash_spec crashes[] = {']
    for index,record in enumerate(records):
        data=record['sections']['.data']; pe=Fs98Pe(Path(record['path']))
        values=[index,data['rva'],data['size'],pe.exports()['ImportTable'],record['linkage_rva'],int(record['header_rva'],16),record['active_end']]
        flag=0x80 if index==1 else 0x7e
        lines += ['{{'+json.dumps(str(Path(record['path']).resolve()))+','+json.dumps(record['rebuilt'])+','+','.join(hex(v) for v in values)
                  +f',crash_relocs_{index},{len(record["model_relocs"])},crash_resources_{index},{len(record["resources"])}}},'
                  +f'0x{record["chain_rva"]:x},0x{flag:x},0x{flag+2:x},crash_functions_{index},{len(record["function_rvas"])},'
                  +json.dumps(record['lifted'])+'},']
    lines += ['};']
    (output/'crashes.inc').write_text('\n'.join(lines)+'\n')


if __name__=='__main__': main()
