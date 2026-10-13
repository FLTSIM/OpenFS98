import struct,sys
def u16(b,o): return struct.unpack_from('<H',b,o)[0]
def u32(b,o): return struct.unpack_from('<I',b,o)[0]
class PE:
    def __init__(s,p):
        s.b=open(p,'rb').read(); b=s.b
        e=u32(b,0x3c); s.nsec=u16(b,e+6); optsz=u16(b,e+20); s.opt=e+24
        s.base=u32(b,s.opt+28); s.ddir=s.opt+96
        s.secs=[]
        so=s.opt+optsz
        for i in range(s.nsec):
            n=b[so:so+8].rstrip(b'\0').decode('latin1')
            s.secs.append((n,u32(b,so+8),u32(b,so+12),u32(b,so+16),u32(b,so+20))); so+=40
    def off(s,rva):
        for n,vsz,va,rsz,ro in s.secs:
            if va<=rva<va+max(vsz,rsz): return ro+(rva-va)
    def va2off(s,va): return s.off(va-s.base)
    def exports(s):
        eva=u32(s.b,s.ddir); o=s.off(eva)
        nf=u32(s.b,o+20); nn=u32(s.b,o+24); af=u32(s.b,o+28); an=u32(s.b,o+32); ao=u32(s.b,o+36)
        out={}
        no=s.off(an); oo=s.off(ao); fo=s.off(af)
        for i in range(nn):
            nr=u32(s.b,no+i*4); off=s.off(nr)
            name=s.b[off:s.b.index(b'\0',off)].decode('latin1')
            idx=u16(s.b,oo+i*2)
            out[name]=u32(s.b,fo+idx*4)
        return out
    def cstr(s,va,maxlen=64):
        o=s.va2off(va)
        if o is None or o>=len(s.b): return None
        end=s.b.find(b'\0',o,o+maxlen)
        if end<0: return None
        t=s.b[o:end]
        if t and all(32<=c<127 for c in t): return t.decode('latin1')
        return None

p=sys.argv[1]; pe=PE(p)
exp=pe.exports()
print(f"--- {p}  base=0x{pe.base:08x} exports={exp}")
for name,rva in exp.items():
    o=pe.off(rva)
    print(f"\n== {name} @ RVA 0x{rva:x} (file 0x{o:x})")
    n=int(sys.argv[2]) if len(sys.argv)>2 else 32
    for i in range(n):
        v=u32(pe.b,o+i*4)
        sref=pe.cstr(v) if v>pe.base else None
        print(f"  +{i*4:04x}: 0x{v:08x}" + (f"   -> \"{sref}\"" if sref else ""))
