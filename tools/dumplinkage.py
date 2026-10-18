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

# Image extent, so a dword can be tested for "is this a pointer into us?".
def is_va(v):
    return v>pe.base and pe.off(v-pe.base) is not None

# Code extent, for deciding which slots are functions.
code_lo=code_hi=None
for n,vsz,va,rsz,ro in pe.secs:
    if n=='.text':
        code_lo=pe.base+va; code_hi=pe.base+va+max(vsz,rsz)

# Tables are NOT a fixed 32 entries.  UTIL's Linkage runs to +0xc8; capping it
# at 32 hid 19 slots, including every service still to be done.  Walk instead:
# slot 0 is the module id, then accept nulls (the table has interior gaps) and
# in-image pointers, and stop at the first dword that is neither -- except at
# +0x10, which in CHAIN, FE and WINDOW holds a flag word (0xe0000000,
# 0xd0000000, 0xc0000000) with the services after it; stopping there reported
# all three as 3-entry tables.  Elsewhere a non-pointer really is the end:
# SOUND's .data goes on with 0xac and then a code pointer at +0x4c.
def extent(o, limit, kind):
    n=0
    while n<limit:
        v=u32(pe.b,o+n*4)
        if kind=='pairs':
            if n%2==0 and v==0 and u32(pe.b,o+(n+1)*4)==0: break
        elif n>0 and n!=4 and v!=0 and not is_va(v):
            break
        n+=1
    if kind!='pairs':
        # Trim trailing nulls -- but never for pairs, where a zero *link* is the
        # legitimate other half of an entry the host has yet to fill in.
        while n>1 and u32(pe.b,o+(n-1)*4)==0:
            n-=1
    return n

byrva=sorted(exp.items(), key=lambda kv: kv[1])
for i,(name,rva) in enumerate(byrva):
    o=pe.off(rva)
    nxt=byrva[i+1][1] if i+1<len(byrva) else None
    limit=(nxt-rva)//4 if nxt else 4096           # never read into the next export
    if len(sys.argv)>2:
        n=int(sys.argv[2])
    else:
        n=extent(o, limit, 'pairs' if name=='ImportTable' else 'slots')
    print(f"\n== {name} @ RVA 0x{rva:x} (file 0x{o:x})  {n} entries")

    vals=[u32(pe.b,o+j*4) for j in range(n)]
    # For a service table, a slot's code size is the gap to the next function
    # start -- which is what the module docs quote, and is an upper bound
    # because internal helpers fall inside it.
    starts=sorted({v for v in vals if code_lo and code_lo<=v<code_hi})
    def size_of(v):
        later=[x for x in starts if x>v]
        return (later[0] if later else code_hi)-v

    for j,v in enumerate(vals):
        sref=pe.cstr(v) if is_va(v) else None
        line=f"  +{j*4:04x}: 0x{v:08x}"
        if j==0 and name=='Linkage':
            line+=f"   module id {v}"
        elif name=='ImportTable':
            line+=("   wants module id %d"%v) if j%2==0 else "   (link, filled in by the host)"
        elif sref:
            line+=f'   -> "{sref}"'
        elif v in starts:
            line+=f"   {size_of(v):5d} bytes"
        print(line)

    if starts:
        print(f"  -- {len(starts)} distinct functions, {sum(size_of(v) for v in starts)} bytes of code")
