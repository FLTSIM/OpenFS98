#!/usr/bin/env python3
"""Recover host runtime control flow as explicit C; retain only data/resources.

No executable sections are copied into the output. The existing HOST services
remain the module-facing implementation; generated routines fill the runtime
around them and share host_image. All generated artifacts live under bin/.
"""
from collections import deque
import hashlib
import json
from pathlib import Path
import re
import struct

from dialogmap import Fs98Pe
from gen_mdl_crash import CrashLift, CONDITIONS
from gen_sim1_lift import SimLift


class HostRuntime(SimLift):
    def __init__(self):
        self.path = Path('orig/FLTSIM98.EXE')
        if hashlib.sha256(self.path.read_bytes()).hexdigest() != 'f47c124a1ad44475a8a3a5ecbce83f5dc5d6e7dad78cf120b20616738bd99f6a':
            raise ValueError('host reference hash differs')
        self.pe = Fs98Pe(self.path)
        self.instructions, self.next, self.tables = {}, {}, {}
        self.relocated_instructions = set()
        self.relocs = {}
        self.islands = set()
        self.pointers = {}
        self.services = {}
        self.decode(0x1000)
        self.relocated_instructions.update(self.instructions)
        for slot in range(0x18, 0x320, 4):
            va = self.pe.va_uint32(0x417ba0 + slot)
            if va:
                self.services.setdefault(va - self.pe.uiBase, slot)
        self.roots = {0x1000, 0x10b0, 0x1504a, 0x1a70, 0x20b0, 0x2050, 0x119a0, 0x1950, 0x7cc0, 0x7d10, 0x7e40, 0x7ec0, 0x7ef0}
        for section in self.pe.aSections:
            if section['name'] not in ('.rdata', '.data'):
                continue
            for site in range(section['rva'], section['rva'] + section['raw_size'] - 3, 4):
                value = self.pe.va_uint32(self.pe.uiBase + site)
                if 0x401000 <= value < 0x41c5cc:
                    self.pointers[site] = value - self.pe.uiBase
                    if self.pe.is_code(value):
                        self.roots.add(value - self.pe.uiBase)
        # Byte-index maps, each bounded by the preceding unsigned switch check.
        for start, count in ((0x3378,16),(0x3818,106),(0x5884,17)):
            self.islands.update(range(start,start+count))
        self.functions = {}
        pending = deque(sorted(self.roots))
        while pending:
            start = pending.popleft()
            if start in self.functions:
                continue
            # Native service boundaries use the already verified C bodies.
            if start in self.services:
                self.functions[start] = set()
                continue
            body = self.discover(start)
            self.functions[start] = body
            for address in body:
                op, args = self.instructions[address]
                if op == 'call' and args.startswith('0x'):
                    pending.append(int(args, 16) - self.pe.uiBase)
                # All explicit code-address values become native callback entries.
                if op in ('push', 'mov') and re.fullmatch(r'0x[0-9a-f]+', args.split(',')[-1]):
                    for immediate in re.findall(r'0x[0-9a-f]+', args):
                        value = int(immediate, 16)
                        if self.pe.is_code(value):
                            root = value - self.pe.uiBase
                            self.roots.add(root)
                            pending.append(root)
        reached = set().union(*self.functions.values())
        occupied = {}
        for address in reached:
            for byte in range(address, self.next[address]):
                if byte in occupied and occupied[byte] != address:
                    raise ValueError(f'overlapping instructions {address:x}/{occupied[byte]:x}')
                occupied[byte] = address
        if set(occupied) & self.islands:
            raise ValueError('switch data overlaps reachable instructions')
        self.signatures = {}
        for start, body in self.functions.items():
            returns = {int(self.instructions[a][1] or '0', 0)
                       for a in body if self.instructions[a][0] == 'ret'}
            if len(returns) == 1:
                self.signatures[start] = returns.pop()
        unsupported = {}
        for body in self.functions.values():
            for address in body:
                try:
                    self.lower(address)
                except (ValueError, KeyError) as error:
                    unsupported.setdefault(str(self.instructions[address]), str(error))
        if unsupported:
            raise ValueError(json.dumps(unsupported, indent=2))

    def decode(self, start):
        super().decode(start)
        self.relocated_instructions.update(self.instructions)

    def jump_targets(self, address, args):
        match = re.fullmatch(r'DWORD PTR \[[a-z]+\*4\+(0x[0-9a-f]+)\]', args)
        if not match:
            raise ValueError(f'indirect jump {address:x}: {args}')
        table = int(match[1], 16) - self.pe.uiBase
        targets, cursor = [], table
        while True:
            value = self.pe.va_uint32(self.pe.uiBase + cursor)
            if not self.pe.is_code(value):
                break
            targets.append(value - self.pe.uiBase)
            self.islands.update(range(cursor, cursor + 4))
            cursor += 4
        if not targets:
            raise ValueError(f'empty switch table {table:x}')
        self.tables[address] = table, targets
        return targets

    def address(self, operand):
        operand = re.sub(r'^(BYTE|WORD|DWORD|QWORD|TBYTE) PTR ', '', operand)
        if operand.startswith('fs:'):
            return f'host_fs_address({self.address(operand[3:]) if operand[3:].startswith("[") else operand[3:]})'
        if operand.startswith('ds:'):
            return f'mdl_address(f, 0x{int(operand[3:],16)-self.pe.uiBase:x})'
        return CrashLift.address(self, operand)

    def read(self, operand, width=32):
        if operand.startswith('fs:'):
            return f'mdl_read(f, {self.address(operand)}, {width})'
        return super().read(operand, width)

    def lower(self, address):
        self.current_address = address
        op, args = self.instructions[address]
        if op == 'jmp' and args.startswith('0x'):
            target = int(args, 16) - self.pe.uiBase
            # Tail jumps into another routine's body are still normal CFG edges.
            return f'goto label_{target:x};'
        if op.startswith('set'):
            condition = CONDITIONS['j'+op[3:]]
            return self.write(args, f'!!({condition})', 8)
        if op in ('cpuid', 'rdtsc'):
            if op == 'rdtsc':
                return '__asm__ volatile("rdtsc" : "=a"(f->eax), "=d"(f->edx));'
            return '__asm__ volatile("cpuid" : "+a"(f->eax), "=b"(f->ebx), "+c"(f->ecx), "=d"(f->edx));'
        if op in ('rep', 'repz', 'repnz'):
            width = 32 if 'DWORD PTR' in args else 8
            step = width//8
            direction = f'(f->flags & 0x400 ? -{step}u : {step})'
            if args.startswith('stos '):
                return f'while(f->ecx) {{ mdl_write(f,f->edi,f->eax,{width}); f->edi+={direction}; --f->ecx; }}'
            if args.startswith('movs '):
                return f'while(f->ecx) {{ mdl_write(f,f->edi,mdl_read(f,f->esi,{width}),{width}); f->esi+={direction}; f->edi+={direction}; --f->ecx; }}'
            if args.startswith('scas '):
                return f'while(f->ecx) {{ mdl_arith(f,f->eax,mdl_read(f,f->edi,{width}),{width},1,0); f->edi+={direction}; --f->ecx; if(f->flags & 0x40) break; }}'
            if args.startswith('cmps '):
                return f'while(f->ecx) {{ mdl_arith(f,mdl_read(f,f->esi,{width}),mdl_read(f,f->edi,{width}),{width},1,0); f->esi+={direction}; f->edi+={direction}; --f->ecx; if(!(f->flags & 0x40)) break; }}'
        if address == 0x2171:
            return 'if(OpenFS98Host.main_loop_pass) { OpenFS98Host.main_loop_pass(); }\n' + super().lower(address).replace('sim1_lift_native','host_runtime_native')
        return super().lower(address).replace('sim1_lift_native', 'host_runtime_native').replace('sim1_unsupported()', 'host_runtime_fail(0x%x)' % address)

    def emit(self, directory):
        constants = bytearray(0x17000)
        for rva in self.islands:
            constants[rva] = self.pe.byData[self.pe.file_offset(rva)]
        rdata = next(s for s in self.pe.aSections if s['name'] == '.rdata')
        constants[rdata['rva']:rdata['rva'] + rdata['raw_size']] = self.pe.read_bytes(rdata['raw_offset'],rdata['raw_size'])
        code = ['/* Generated explicit C; no executable payload or runtime decoder. */',
                '#include "host_runtime.h"', 'extern uint8_t host_image[];',
                'extern void *HostTable[];', 'static uint8_t constants[] = {']
        for offset in range(0, len(constants), 32):
            code.append(','.join(str(v) for v in constants[offset:offset+32]) + ',')
        code += ['};', 'static uint32_t imports[0x2000/4];', 'DWORD host_runtime_tls;']
        code += ['static void *host_patch_import(const char *name,void *replacement) { void *previous=NULL;']
        for imp in self.pe.imports():
            if not imp['name'].startswith('#'):
                index=(imp['iat_va']-self.pe.uiBase-0x1d000)//4
                code += [f'if(!strcmp(name,{json.dumps(imp["name"])})) {{ previous=(void *)(uintptr_t)imports[{index}]; imports[{index}]=(uint32_t)(uintptr_t)replacement; }}']
        code += ['return previous; }', '__declspec(dllexport) struct fs98_host_runtime OpenFS98Host = {1,host_image,host_patch_import,NULL};']
        code += [f'extern void host_entry_{root:x}(void);' for root in sorted(self.roots)]
        code += ['uint32_t host_runtime_address(struct mdl_frame *f, uint32_t rva) { (void)f; switch(rva) {']
        code += [f'case 0x{root:x}: return (uint32_t)(uintptr_t)host_entry_{root:x};' for root in sorted(self.roots)]
        code += ['default: if(rva >= 0x17000 && rva < 0x1c5cc) return (uint32_t)(uintptr_t)(host_image+rva-0x17000);',
                 'if(rva >= 0x1d000 && rva < 0x1f000) return (uint32_t)(uintptr_t)((uint8_t *)imports+rva-0x1d000);',
                 'if(rva < sizeof constants) return (uint32_t)(uintptr_t)(constants+rva);',
                 'host_runtime_fail(rva); return 0; } }']
        code += [f'static void execute_{start:x}(struct mdl_frame *f);' for start in sorted(self.functions)]
        for start, body in sorted(self.functions.items()):
            code += [f'static void execute_{start:x}(struct mdl_frame *f) {{']
            if start in self.services:
                code += [f'f->esp += 4; host_runtime_native(f,(uint32_t)(uintptr_t)HostTable[0x{self.services[start]:x}/4]);', '}']
                continue
            code += [f'goto label_{start:x};']
            for address in sorted(body):
                op,args = self.instructions[address]
                code += [f'label_{address:x}: /* {self.pe.uiBase+address:08x}: {op} {args} */', self.lower(address)]
                if op not in ('jmp', 'ret'):
                    code += [f'goto label_{self.next[address]:x};']
            code += ['}']
        code += ['uint32_t *host_runtime_enter(uint32_t entry, uint32_t *saved) {',
                 'struct host_runtime_frame context; struct mdl_frame *f=&context.cpu;',
                 'uint32_t original[10], i, cleanup, base; void *previous;',
                 'uint32_t *top=(uint32_t *)(uintptr_t)host_runtime_stack();',
                 'memcpy(original,saved,sizeof original);',
                 'for(i=0;i<64;i++) top[i]=*(uint32_t *)(uintptr_t)(saved[3]+4+i*4);',
                 'f->eax=saved[7]; f->ecx=saved[6]; f->edx=saved[5]; f->ebx=saved[4];',
                 'f->ebp=saved[2]; f->esi=saved[1]; f->edi=saved[0]; f->flags=saved[8];',
                 'f->stack_low=f->stack_high=0; f->image=host_image;',
                 'base=f->esp=(uint32_t)(uintptr_t)top;',
                 'previous=TlsGetValue(host_runtime_tls); TlsSetValue(host_runtime_tls,f);',
                 'switch(entry) {']
        code += [f'case 0x{root:x}: execute_{root:x}(f); break;' for root in sorted(self.roots)]
        code += ['default: host_runtime_fail(entry); }', 'cleanup=f->esp-base-4;',
                 'if(cleanup>256 || (cleanup&3)) host_runtime_fail(entry);',
                 'TlsSetValue(host_runtime_tls,previous);',
                 'original[7]=f->eax; original[6]=f->ecx; original[5]=f->edx; original[4]=f->ebx;',
                 'original[2]=f->ebp; original[1]=f->esi; original[0]=f->edi; original[8]=f->flags;',
                 'saved=(uint32_t *)((uint8_t *)saved+cleanup);',
                 'memcpy(saved,original,sizeof original); return saved; }',
                 'int host_runtime_init(void) { HMODULE module; FARPROC fn;',
                 'host_runtime_tls=TlsAlloc(); if(host_runtime_tls==TLS_OUT_OF_INDEXES) return 0;']
        for imp in self.pe.imports():
            name = imp['name']
            if name.startswith('#'):
                name_expr = f'(LPCSTR)(uintptr_t){int(name[1:])}'
            else:
                name_expr = json.dumps(name)
            code += [f'module=LoadLibraryA({json.dumps(imp["dll"])});',
                     f'fn=module?GetProcAddress(module,{name_expr}):NULL;',
                     f'if(!fn) {{ host_runtime_import_error({json.dumps(imp["dll"])},{json.dumps(name)}); return 0; }}',
                     f'imports[0x{imp["iat_va"]-self.pe.uiBase-0x1d000:x}/4]=(uint32_t)(uintptr_t)fn;']
        for site,target in sorted(self.pointers.items()):
            code += [f'mdl_write(NULL,host_runtime_address(NULL,0x{site:x}),host_runtime_address(NULL,0x{target:x}),32);']
        code += ['memcpy(host_image+0xba0,HostTable,0x320); return 1; }']
        (directory/'host_runtime.c').write_text('\n'.join(code)+'\n')
        asm = ['.text']
        for root in sorted(self.roots):
            asm += [f'.globl _host_entry_{root:x}', f'_host_entry_{root:x}:',
                    'pushfl','pushal','pushl %esp',f'pushl $0x{root:x}',
                    'call _host_runtime_enter','movl %eax,%esp','popal','popfl','ret']
        (directory/'host_entries.S').write_text('\n'.join(asm)+'\n')
        resources=bytearray(struct.pack('<IIHHHHIHHII',0,32,0xffff,0,0xffff,0,0,0,0,0,0))
        for resource in self.pe.resources():
            if not all(isinstance(resource[key],int) for key in ('type','id','language')):
                raise ValueError('named resource')
            resources += struct.pack('<IIHHHHIHHII',resource['size'],32,0xffff,resource['type'],0xffff,resource['id'],0,0x1030,resource['language'],0,0)
            resources += self.pe.read_bytes(self.pe.file_offset(resource['rva'],resource['size']),resource['size'])
            resources += bytes(-len(resources)%4)
        (directory/'host.res').write_bytes(resources)


def main():
    directory=Path('bin/host-runtime')
    directory.mkdir(parents=True,exist_ok=True)
    runtime=HostRuntime()
    runtime.emit(directory)
    report=runtime.report()
    report['reference_sha256']=hashlib.sha256(runtime.path.read_bytes()).hexdigest()
    (directory/'survey.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':
    main()
