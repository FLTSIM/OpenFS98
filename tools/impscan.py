#!/usr/bin/env python3
"""List which imported services a module uses, by import and slot offset.

    python3 tools/impscan.py orig/Modules/ATC.DLL [MODDIR]

Every cross-module call in FS98 goes through the ImportTable: the module loads
the link the host filled in (`mov reg, ds:link`) and then reaches through it
(`call [reg+slot]`, or a read/write of `[reg+field]` for GLOBAL).  This finds
each load of a link and follows the register forward until it is overwritten.

It is a heuristic, and under-reports rather than over-reports: a link held in a
register across a call, or loaded by an instruction other than `mov`, is missed.
A load whose register is never dereferenced nearby is listed as `passed` --
usually the pointer handed to a callee.  Check a surprising gap by hand.
"""
import collections, glob, os, re, struct, subprocess, sys


def u16(b, o): return struct.unpack_from('<H', b, o)[0]
def u32(b, o): return struct.unpack_from('<I', b, o)[0]


class PE:
    def __init__(s, p):
        s.b = open(p, 'rb').read(); b = s.b
        e = u32(b, 0x3c); nsec = u16(b, e + 6); optsz = u16(b, e + 20); opt = e + 24
        s.base = u32(b, opt + 28); s.ddir = opt + 96; s.secs = []
        so = opt + optsz
        for _ in range(nsec):
            s.secs.append((u32(b, so + 8), u32(b, so + 12), u32(b, so + 16), u32(b, so + 20))); so += 40

    def off(s, rva):
        for vsz, va, rsz, ro in s.secs:
            if va <= rva < va + max(vsz, rsz): return ro + (rva - va)

    def va2off(s, va): return s.off(va - s.base)

    def exports(s):
        eva = u32(s.b, s.ddir)
        if not eva: return {}
        o = s.off(eva); nn = u32(s.b, o + 24); af = u32(s.b, o + 28); an = u32(s.b, o + 32); ao = u32(s.b, o + 36)
        out = {}; no = s.off(an); oo = s.off(ao); fo = s.off(af)
        for i in range(nn):
            off = s.off(u32(s.b, no + i * 4))
            out[s.b[off:s.b.index(b'\0', off)].decode('latin1')] = u32(s.b, fo + u16(s.b, oo + i * 2) * 4)
        return out


REGS = 'eax|ebx|ecx|edx|esi|edi|ebp'


def module_names(moddir):
    names = {1: 'HOST'}
    for p in glob.glob(os.path.join(moddir, '*')):
        try:
            pe = PE(p)
            lk = pe.exports().get('Linkage')
        except Exception:
            continue
        if lk is not None:
            names[u32(pe.b, pe.off(lk))] = os.path.basename(p).upper().replace('.DLL', '')
    return names


def scan(path):
    pe = PE(path)
    it = pe.exports()['ImportTable'] + pe.base
    links = {}
    o = pe.va2off(it)
    i = 0
    while u32(pe.b, o + i * 8):
        links[it + i * 8 + 4] = u32(pe.b, o + i * 8)
        i += 1

    dis = subprocess.run(['objdump', '-d', '-M', 'intel', '--no-show-raw-insn', path],
                         capture_output=True, text=True, check=True).stdout
    ins = []
    for line in dis.splitlines():
        m = re.match(r'\s*([0-9a-f]+):\s+(.*)', line)
        if m:
            ins.append((int(m.group(1), 16), m.group(2).strip()))

    uses = collections.defaultdict(lambda: collections.defaultdict(list))
    for k, (addr, text) in enumerate(ins):
        m = re.match(rf'mov\s+({REGS}),(?:DWORD PTR )?ds:0x([0-9a-f]+)$', text)
        if not m or int(m.group(2), 16) not in links:
            continue
        reg, mid = m.group(1), links[int(m.group(2), 16)]
        found = False
        for _, use in ins[k + 1:k + 25]:
            mm = re.search(rf'\[{reg}(?:\+0x([0-9a-f]+))?\]', use)
            if mm:
                off = int(mm.group(1) or '0', 16)
                if use.startswith('call'):
                    kind = 'call'
                elif re.match(rf'\w+\s+(?:\w+ PTR )?\[{reg}', use):
                    kind = 'write'
                else:
                    kind = 'read'
                uses[mid][(off, kind)].append(addr)
                found = True
            if (re.match(rf'(mov|lea|xor|pop|movsx|movzx)\s+{reg},', use)
                    or use.startswith(('call', 'ret', 'jmp'))):
                break
        if not found:
            uses[mid][(-1, 'passed')].append(addr)
    return uses


def main():
    path = sys.argv[1]
    names = module_names(sys.argv[2] if len(sys.argv) > 2 else os.path.dirname(path))
    uses = scan(path)
    print(f'--- {path}: imported services used')
    for mid in sorted(uses):
        print(f'-- module {mid} {names.get(mid, "?")}')
        for (off, kind), addrs in sorted(uses[mid].items()):
            slot = '   --  ' if off < 0 else f'+0x{off:03x}'
            print(f'   {slot} {kind:6s} x{len(addrs):<3d} first@{addrs[0]:08x}')


if __name__ == '__main__':
    main()
