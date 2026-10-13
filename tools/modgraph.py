import struct,glob,os
def u16(b,o): return struct.unpack_from('<H',b,o)[0]
def u32(b,o): return struct.unpack_from('<I',b,o)[0]
class PE:
    def __init__(s,p):
        s.b=open(p,'rb').read(); b=s.b
        e=u32(b,0x3c); nsec=u16(b,e+6); optsz=u16(b,e+20); s.opt=e+24
        s.base=u32(b,s.opt+28); s.ddir=s.opt+96; s.secs=[]
        so=s.opt+optsz
        for i in range(nsec):
            s.secs.append((b[so:so+8].rstrip(b'\0').decode('latin1'),u32(b,so+8),u32(b,so+12),u32(b,so+16),u32(b,so+20))); so+=40
    def off(s,rva):
        for n,vsz,va,rsz,ro in s.secs:
            if va<=rva<va+max(vsz,rsz): return ro+(rva-va)
    def exports(s):
        eva=u32(s.b,s.ddir)
        if not eva: return {}
        o=s.off(eva); nn=u32(s.b,o+24); af=u32(s.b,o+28); an=u32(s.b,o+32); ao=u32(s.b,o+36)
        out={}; no=s.off(an); oo=s.off(ao); fo=s.off(af)
        for i in range(nn):
            nr=u32(s.b,no+i*4); off=s.off(nr)
            out[s.b[off:s.b.index(b'\0',off)].decode('latin1')]=u32(s.b,fo+u16(s.b,oo+i*2)*4)
        return out

mods={}
import sys
MODDIR = sys.argv[1] if len(sys.argv) > 1 else 'orig/Modules'
for p in sorted(glob.glob(MODDIR + '/*')):
    pe=PE(p); exp=pe.exports()
    name=os.path.basename(p).upper().replace('.DLL','')
    lk=exp.get('Linkage'); it=exp.get('ImportTable')
    mid=None; imps=[]; nfn=0
    if lk is not None:
        o=pe.off(lk); mid=u32(pe.b,o)
        # count non-zero function pointers after id
        i=1
        while True:
            v=u32(pe.b,o+i*4)
            if v!=0 and not (pe.base<=v<pe.base+0x200000): break
            if v: nfn+=1
            i+=1
            if i>256: break
        slots=i-1
    if it is not None:
        o=pe.off(it)
        i=0
        while True:
            a=u32(pe.b,o+i*8); bb=u32(pe.b,o+i*8+4)
            if a==0 and bb==0: break
            imps.append(a); i+=1
            if i>64: break
    mods[name]=dict(id=mid,imports=imps,fnptrs=nfn,slots=slots if lk is not None else 0,text=[x for x in pe.secs if x[0]=='.text'][0][1])

byid={v['id']:k for k,v in mods.items()}
print(f"{'MODULE':12s} {'ID':>4s} {'text':>8s} {'slots':>6s} {'fns':>4s}  imports")
for name,v in sorted(mods.items(), key=lambda kv: kv[1]['id'] if kv[1]['id'] is not None else 999):
    imp=' '.join(f"{byid.get(i,'?'+str(i))}" for i in v['imports'])
    print(f"{name:12s} {str(v['id']):>4s} {v['text']:8d} {v['slots']:6d} {v['fnptrs']:4d}  {imp}")
print()
ids=[v['id'] for v in mods.values()]
print("unique ids:",len(set(ids)),"of",len(ids),"| id range:",min(i for i in ids if i is not None),"-",max(i for i in ids if i is not None))
print("ids referenced but not present:",sorted({i for v in mods.values() for i in v['imports']} - set(ids)))
