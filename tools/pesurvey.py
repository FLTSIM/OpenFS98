import struct, sys, os, datetime

def u16(b,o): return struct.unpack_from('<H',b,o)[0]
def u32(b,o): return struct.unpack_from('<I',b,o)[0]

def parse(path):
    b=open(path,'rb').read()
    e_lfanew=u32(b,0x3c)
    assert b[e_lfanew:e_lfanew+4]==b'PE\0\0', 'not PE'
    coff=e_lfanew+4
    nsec=u16(b,coff+2); ts=u32(b,coff+4); optsz=u16(b,coff+16)
    opt=coff+20
    magic=u16(b,opt)
    linkver=(b[opt+2],b[opt+3])
    imgbase=u32(b,opt+28)
    ddir=opt+96
    secs=[]
    so=opt+optsz
    for i in range(nsec):
        n=b[so:so+8].rstrip(b'\0').decode('latin1')
        vsz=u32(b,so+8); va=u32(b,so+12); rsz=u32(b,so+16); ro=u32(b,so+20); ch=u32(b,so+36)
        secs.append((n,vsz,va,rsz,ro,ch)); so+=40
    def rva2off(rva):
        for n,vsz,va,rsz,ro,ch in secs:
            if va<=rva<va+max(vsz,rsz): return ro+(rva-va)
        return None
    # rich header
    rich=[]
    dh=b[:e_lfanew]
    ri=dh.rfind(b'Rich')
    if ri>0:
        key=u32(dh,ri+4)
        # walk back to DanS
        i=ri-4
        vals=[]
        while i>=0x40:
            v=u32(dh,i)^key
            if v==0x536e6144: break
            vals.append(v); i-=4
        for j in range(0,len(vals)-1,2):
            cnt=vals[j]; cid=vals[j+1]
            rich.append((cid>>16, cid&0xffff, cnt))
        rich.reverse()
    # debug dir
    dbg=[]
    dva=u32(b,ddir+6*8); dsz=u32(b,ddir+6*8+4)
    if dva:
        o=rva2off(dva)
        for i in range(dsz//28):
            t=u32(b,o+i*28+12); sz=u32(b,o+i*28+16); rv=u32(b,o+i*28+20); fo=u32(b,o+i*28+24)
            sig=b[fo:fo+4]
            info=''
            if sig==b'RSDS': info=b[fo+24:fo+sz].split(b'\0')[0].decode('latin1')
            elif sig==b'NB10': info=b[fo+16:fo+sz].split(b'\0')[0].decode('latin1')
            dbg.append((t,sig.decode('latin1',errors='replace'),info))
    # exports
    exp=[]
    eva=u32(b,ddir+0); esz=u32(b,ddir+4)
    if eva:
        o=rva2off(eva)
        nnames=u32(b,o+24); anames=u32(b,o+32)
        no=rva2off(anames)
        if no:
            for i in range(nnames):
                nr=u32(b,no+i*4); off=rva2off(nr)
                if off: exp.append(b[off:b.index(b'\0',off)].decode('latin1'))
    # imports (dll names)
    imp=[]
    iva=u32(b,ddir+8)
    if iva:
        o=rva2off(iva)
        while True:
            nr=u32(b,o+12)
            if nr==0: break
            off=rva2off(nr)
            imp.append(b[off:b.index(b'\0',off)].decode('latin1'))
            o+=20
    return dict(path=path,ts=ts,link=linkver,imgbase=imgbase,secs=secs,rich=rich,dbg=dbg,exp=exp,imp=imp)

MSVC={}  # prodid -> rough name (partial)
for p in sys.argv[1:]:
    try: d=parse(p)
    except Exception as ex:
        print(f"{p}: ERROR {ex}"); continue
    text=[s for s in d['secs'] if s[0]=='.text']
    tsz=text[0][1] if text else 0
    rsrc=[s for s in d['secs'] if s[0]=='.rsrc']
    rsz=rsrc[0][1] if rsrc else 0
    print(f"{os.path.basename(p):16s} text={tsz:8d} rsrc={rsz:8d} base=0x{d['imgbase']:08x} link={d['link'][0]}.{d['link'][1]:02d} ts={datetime.datetime.utcfromtimestamp(d['ts']).strftime('%Y-%m-%d')} exports={len(d['exp'])} dbg={d['dbg'] if d['dbg'] else '-'}")
    if d['rich']:
        print(f"   rich: "+", ".join(f"id={pid} ver={ver} n={cnt}" for pid,ver,cnt in d['rich']))
    if d['exp']:
        print(f"   exp: {', '.join(d['exp'][:14])}{' ...' if len(d['exp'])>14 else ''}")
