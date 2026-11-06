/* Crash MDLs: loaded retail and explicit-C models, driven over the same
 * lifecycle/animation scripts. Reuse the normal harness's PE, resource,
 * register capture and pointer-normalization helpers, not its scenarios.
 */
#define MDL_STATE_CAPACITY 0x800
#define main mdl_normal_reference_main
#include "mdldlldiff.c"
#undef main

struct crash_spec {
    struct model_spec model;
    uint32_t chain_rva, flags_offset, phase_offset;
    const uint32_t *functions;
    uint32_t function_count;
};
#include "crashes.inc"

struct event { uint32_t kind, a, b, c; };
static struct event events[1024], expected_events[1024];
static uint32_t event_count, side_index, callback[2];
static uint8_t snapshot[44], chain_blob[0x24], draw_blob[0x20], sim_blob[0x20];
static uint8_t *images[2];
static uint32_t current_chain[2];
uint32_t util_sincos_target, util_longitude_target;

static uint32_t canonical(uint32_t value, uint32_t side)
{
    uint32_t origin=(uint32_t)(uintptr_t)images[side], i;
    if (callback[side] && value==callback[side]) return 0x60000000;
    for(i=1;i<9;i++) {
        uint32_t fn=read32(images[side]+model->linkage_rva+4*i);
        if(fn && value==fn) return 0x60000010+i;
    }
    if(value==read32(images[side]+model->header_rva+0x10)) return 0x60000020;
    /* BX/AX writes can replace the low half of a model pointer with a scalar.
     * Keep the complete 64-KiB page class, including values past active data. */
    if(value>=origin && value<origin+((model->active_end+65535u)&~65535u)) return 0x50000000+value-origin;
    return value;
}
static void event(uint32_t kind,uint32_t a,uint32_t b,uint32_t c)
{
    if(event_count>=1024) fail("event overflow",event_count);
    events[event_count++]=(struct event){kind,
        kind==3||kind==4||kind==5?canonical(a,side_index):a,
        kind==1||kind==2||kind==4||kind==5?canonical(b,side_index):b,c};
}
void log_util(uint32_t kind,uint32_t *saved)
{
    uint32_t *args=(uint32_t *)(uintptr_t)(saved[3]+8);
    if(kind==16) event(kind,args[0]&65535u,0,0);
    else {
        const uint8_t *metric=(const uint8_t *)(uintptr_t)args[0];
        const uint8_t *position=(const uint8_t *)(uintptr_t)args[1];
        uint16_t low;memcpy(&low,metric+2,2);
        event(kind,low,read32(metric+4),read32(position+4));
    }
}
void util_sincos_proxy(void),util_longitude_proxy(void);
__asm__(".text\n.globl _util_sincos_proxy\n_util_sincos_proxy:\n\t"
        "pushfl\n\tpushal\n\tpushl %esp\n\tpushl $16\n\tcall _log_util\n\taddl $8,%esp\n\t"
        "popal\n\tpopfl\n\tjmp *_util_sincos_target\n"
        ".globl _util_longitude_proxy\n_util_longitude_proxy:\n\t"
        "pushfl\n\tpushal\n\tpushl %esp\n\tpushl $17\n\tcall _log_util\n\taddl $8,%esp\n\t"
        "popal\n\tpopfl\n\tjmp *_util_longitude_target\n");
uint32_t __attribute__((stdcall)) add_chain_impl(uint32_t list,uint32_t fn)
{
    callback[side_index]=fn;
    current_chain[side_index]=fn;
    event(1,list,fn,0);
    return 0;
}
uint32_t __attribute__((stdcall)) remove_chain_impl(uint32_t list,uint32_t fn)
{
    event(2,list,fn,current_chain[side_index]==fn);
    if(current_chain[side_index]==fn) current_chain[side_index]=0;
    return 0;
}
void snapshot_impl(uint8_t *dst)
{
    event(3,(uint32_t)(uintptr_t)dst,0,0);
    memcpy(dst,snapshot,sizeof snapshot);
}
void __attribute__((stdcall)) snapshot_stub(uint8_t *dst);
__asm__(".text\n.globl _snapshot_stub@4\n_snapshot_stub@4:\n\t"
        "pushfl\n\tpushal\n\tmovl 40(%esp),%eax\n\tpushl %eax\n\t"
        "call _snapshot_impl\n\taddl $4,%esp\n\tpopal\n\tpopfl\n\tret $4\n");
uint32_t __attribute__((stdcall)) draw_program_impl(uint32_t program,uint32_t context)
{
    uint32_t hash=0x811c9dc5, i, size=read32(images[side_index]+model->header_rva+0x18);
    for(i=0;i<size;i++) hash=(hash^((uint8_t *)(uintptr_t)context)[i])*0x1000193;
    event(4,program,context,hash);
    return 0;
}
int32_t __attribute__((stdcall)) crash_copy_impl(void *dst,const void *src,uint32_t size)
{
    event(5,(uint32_t)(uintptr_t)dst,(uint32_t)(uintptr_t)src,size);
    memmove(dst,src,size);
    return 0;
}
/* Keep the logging fixture's side index and scratch pointers out of volatile
 * register results. Each side gets the same explicit dependency contract. */
void add_chain(void),remove_chain(void),draw_program(void),crash_copy(void);
#define STUB(name,bytes,pushes) \
__asm__(".text\n.globl _" #name "\n_" #name ":\n\t" \
        "pushfl\n\tpushal\n\tleal 40(%esp),%eax\n\t" pushes \
        "call _" #name "_impl@" #bytes "\n\tpopal\n\tpopfl\n\tmovl $0,%eax\n\tret $" #bytes "\n")
STUB(add_chain,8,"pushl 4(%eax)\n\tpushl (%eax)\n\t");
STUB(remove_chain,8,"pushl 4(%eax)\n\tpushl (%eax)\n\t");
STUB(draw_program,8,"pushl 4(%eax)\n\tpushl (%eax)\n\t");
STUB(crash_copy,12,"pushl 8(%eax)\n\tpushl 4(%eax)\n\tpushl (%eax)\n\t");
#undef STUB
static void call_op(uint32_t fn,uint32_t argc,uint8_t *arg,uint32_t registers[8])
{
    uint32_t i;
    model_call(fn,arg,argc);
    if(returned[7]) fail("ESP imbalance",returned[7]);
    for(i=0;i<8;i++) registers[i]=returned[i];
}

int main(int argc,char **argv)
{
    uint32_t count=argc>1?(uint32_t)strtoul(argv[1],NULL,0):2000;
    const char *mode=argc>2?argv[2]:"retail-first";
    HMODULE util;
    uint8_t *util_link;
    uint32_t k;
    if(!count) return 2;
    SetUnhandledExceptionFilter(fault);
    util=LoadLibraryA("orig/Modules/UTIL.DLL");
    if(!util) fail("load retail UTIL",GetLastError());
    util_link=(uint8_t *)GetProcAddress(util,"Linkage");
    if(!util_link) fail("UTIL Linkage",0);
    util_sincos_target=read32(util_link+0x2c);util_longitude_target=read32(util_link+0x44);
    write32(util_link+0x2c,(uint32_t)(uintptr_t)util_sincos_proxy);
    write32(util_link+0x44,(uint32_t)(uintptr_t)util_longitude_proxy);
    write32(host_blob+0x21c,(uint32_t)(uintptr_t)crash_copy);
    write32(chain_blob+0x18,(uint32_t)(uintptr_t)add_chain);
    write32(chain_blob+0x1c,(uint32_t)(uintptr_t)remove_chain);
    write32(draw_blob+0x18,(uint32_t)(uintptr_t)draw_program);
    write32(sim_blob+0x18,(uint32_t)(uintptr_t)snapshot_stub);
    for(k=0;k<sizeof crashes/sizeof crashes[0];k++) {
        const struct crash_spec *crash=crashes+k;
        HMODULE dlls[2];
        uint8_t *pristine[2],*normalized,*retail_data,*global_after;
        uint32_t code_targets[16],*coverage, i,j,step,steps,operation,state_size,end;
        uint32_t regs[8],expected_regs[8],expected_count;
        uint8_t craft[32],craft_saved[32];
        uint8_t *reservation=NULL;
        model=&crash->model; vector=0;
        if(!strcmp(mode,"relocate")) {
            reservation=VirtualAlloc((void *)0x22100000,0x60000,MEM_RESERVE,PAGE_NOACCESS);
            if(!reservation) fail("reserve bases",GetLastError());
        }
        dlls[0]=LoadLibraryA(model->retail); dlls[1]=LoadLibraryA(k==0&&argc>3?argv[3]:model->ours);
        if(!dlls[0]||!dlls[1]) fail("load crash models",GetLastError());
        if(!strcmp(mode,"relocate")) {
            if((uintptr_t)dlls[0]==preferred_base(dlls[0]) || (uintptr_t)dlls[1]==preferred_base(dlls[1]))
                fail("forced relocation did not move both DLLs",0);
        } else if((uintptr_t)dlls[0]!=preferred_base(dlls[0])) fail("retail preferred base",0);
        images[0]=(uint8_t *)dlls[0];
        side_index=1;
        model_call(read32((uint8_t *)GetProcAddress(dlls[1],"Linkage")+0x18),craft,1);
        images[1]=(uint8_t *)(uintptr_t)read32(craft)-model->header_rva;
        if((uintptr_t)images[1]&65535u) fail("crash image must be 64-KiB aligned",0);
        coverage=(uint32_t *)GetProcAddress(dlls[1],"MdlCoverage");
        if(!coverage) fail("coverage export",0);
        resources(dlls[0],dlls[1]);
        for(i=0;i<model->reloc_count;i++) {
            code_targets[i]=read32(images[1]+model->relocs[i].site);
            if(model->relocs[i].target<model->data_rva && !executable(dlls[1],code_targets[i]))
                fail("callback outside rebuilt code",model->relocs[i].site);
        }
        state_size=read32(images[0]+model->header_rva+0x18);
        if(state_size+32>sizeof state) fail("state capacity",state_size);
        end=model->active_end-model->data_rva;
        normalized=malloc(model->data_size); retail_data=malloc(model->data_size);global_after=malloc(sizeof global_blob);
        for(i=0;i<2;i++) {
            uint8_t *imports=images[i]+model->imports_rva;
            write32(imports+4,(uint32_t)(uintptr_t)host_blob);
            write32(imports+12,(uint32_t)(uintptr_t)global_blob);
            write32(imports+20,(uint32_t)(uintptr_t)chain_blob);
            write32(imports+28,(uint32_t)(uintptr_t)draw_blob);
            write32(imports+36,(uint32_t)(uintptr_t)util_link);
            pristine[i]=malloc(model->data_size);
            if(!pristine[i]||!normalized||!retail_data||!global_after) fail("allocation",0);
            memcpy(pristine[i],images[i]+model->data_rva,model->data_size);
        }
        memcpy(normalized,pristine[1],model->data_size);
        normalize(normalized,images[0],images[1],code_targets);
        compare(pristine[0],normalized,end,"initial crash data");
        for(vector=0;vector<count;vector++) {
            static const uint32_t pitch[]={0,0x0aaa0000,0x1c700000,0x0aa9ffff,0x1c6fffff,0x80000000};
            static const uint32_t bank[]={0,0x071c0000,0x071c0001,0x15550000,0x15550001,0xf8e3ffff,0x80000000};
            static const uint16_t velocity[]={0,1,19,20,21,49,50,51,0x7fff,0x8000,0xffff};
            fill(global_blob,sizeof global_blob); fill(initial,sizeof initial); fill(snapshot,sizeof snapshot);
            memset(global_blob+0x560,0,0x24);
            write32(global_blob+0x574,100);
            write32(global_blob+0x578,vector<126?pitch[vector%6]:rng());
            write32(global_blob+0x57c,vector<126?bank[(vector/6)%7]:rng());
            write32(global_blob+0x580,rng());
            write32(global_blob+0x20,0);
            write32(global_blob+0x834,vector%9==7?6:vector%9==8?12:0);
            global_blob[0x838]=6;
            write32(global_blob+0x11d4,(uint32_t)(uintptr_t)sim_blob);
            write32(global_blob+0x11e4,(uint32_t)(uintptr_t)(state+16));
            { uint16_t delta=vector%5==0?0:vector%5==1?1:vector%5==2?0x1fff:vector%5==3?0x7fff:0xffff;
              memcpy(global_blob+0x2aa,&delta,2); }
            memcpy(snapshot,global_blob+0x560,0x24);
            for(i=0;i<3;i++) { uint16_t v=vector<126?velocity[(vector+i)%11]:(uint16_t)rng(); memcpy(snapshot+0x24+i*2,&v,2); }
            memcpy(global_before,global_blob,sizeof global_blob);
            callback[0]=callback[1]=current_chain[0]=current_chain[1]=0;
            for(i=0;i<2;i++) memcpy(images[i]+model->data_rva,pristine[i],model->data_size);
            memcpy(state,initial,sizeof state);
            steps=vector<126?96:16;
            for(step=0;step<steps+10;step++) {
                operation=step<3?step:step==3?2:step<steps+4?5:
                    step==steps+4||step==steps+5||step==steps+8?3:
                    step==steps+6?2:step==steps+7?5:4;
                expected_count=0;
                for(side_index=0;side_index<2;side_index++) {
                    uint8_t *image=images[side_index],*table=image+model->linkage_rva;
                    uint32_t fn,args;
                    event_count=0;
                    if(side_index) {
                        memcpy(state,initial,sizeof state);
                        memcpy(global_blob,global_before,sizeof global_blob);
                    }
                    if(operation==3) {
                        static const uint8_t cleanup[]={0,5,6,7,127,128,255};
                        global_blob[0x838]=cleanup[vector%7];
                    }
                    if(operation==5 && step%13==0) global_blob[0x838]=5;
                    if(operation==5 && step%13==1) global_blob[0x838]=6;
                    switch(operation) {
                    case 0: fn=read32(table+4);args=0;break;
                    case 1: fn=read32(table+0x18);args=1;break;
                    case 2: fn=read32(table+0x1c);args=1;break;
                    case 3: fn=read32(table+0x20);args=0;break;
                    case 4: fn=read32(table+8);args=0;break;
                    default:
                        if((step&3)==0 && current_chain[side_index]) {fn=current_chain[side_index];args=2;}
                        else {fn=read32(image+model->header_rva+0x10);args=1;}
                        break;
                    }
                    if(operation==1) {
                        memset(craft,0xa5,sizeof craft);memcpy(craft_saved,craft,sizeof craft);
                        call_op(fn,args,craft,regs);
                        write32(craft_saved,(uint32_t)(uintptr_t)(image+model->header_rva));
                        compare(craft,craft_saved,sizeof craft,"attach writes");
                    } else call_op(fn,args,state+16,regs);
                    compare(state,initial,16,"leading canary");
                    compare(state+16+state_size,initial+16+state_size,sizeof state-16-state_size,"trailing canary");
                    compare(image+model->active_end,pristine[side_index]+end,model->data_size-end,"CRT tail changed");
                    if(!side_index) {
                        memcpy(expected,state,sizeof state);memcpy(global_after,global_blob,sizeof global_blob);
                        memcpy(retail_data,image+model->data_rva,model->data_size);
                        memcpy(expected_events,events,sizeof events);expected_count=event_count;
                        memcpy(expected_regs,regs,sizeof regs);
                    } else {
                        if(event_count!=expected_count) fail("event count",step);
                        if(memcmp(expected_events,events,event_count*sizeof events[0])) {
                            fprintf(stderr,"step=%u calls:",step);
                            for(j=0;j<event_count;j++) fprintf(stderr," [%u %x/%x %x/%x %x/%x]",events[j].kind,expected_events[j].a,events[j].a,expected_events[j].b,events[j].b,expected_events[j].c,events[j].c);
                            fprintf(stderr,"\n");
                        }
                        compare(expected_events,events,event_count*sizeof events[0],"crash calls");
                        if(memcmp(expected,state,sizeof state)) fprintf(stderr,"step=%u operation=%u\n",step,operation);
                        compare(expected,state,sizeof state,"crash state");
                        compare(global_after,global_blob,sizeof global_blob,"crash GLOBAL");
                        for(j=0;j<8;j++) if(expected_regs[j]!=regs[j] &&
                            canonical(expected_regs[j],0)!=canonical(regs[j],1)) {
                            fprintf(stderr,"step=%u operation=%u register %u: %08x/%08x images %p/%p\n",
                                    step,operation,j,expected_regs[j],regs[j],images[0],images[1]);
                            fail("crash registers",j);
                        }
                        memcpy(normalized,image+model->data_rva,model->data_size);
                        normalize(normalized,images[0],images[1],code_targets);
                        compare(retail_data,normalized,end,"crash data");
                    }
                }
                memcpy(initial,state,sizeof state);memcpy(global_before,global_blob,sizeof global_blob);
            }
        }
        j=0;
        for(i=0;i<crash->function_count;i++) if(!coverage[i]) {printf("unreached function 0x%x\n",crash->functions[i]);++j;}
        printf("%s: %u scenarios, 0 differences, %u/%u functions reached (%s)\n",strrchr(model->retail,'/')+1,
               count,crash->function_count-j,crash->function_count,mode);
        if(count>=126 && j) fail("unreached crash functions",j);
        free(pristine[0]);free(pristine[1]);free(normalized);free(retail_data);free(global_after);
        FreeLibrary(dlls[0]);FreeLibrary(dlls[1]);if(reservation)VirtualFree(reservation,0,MEM_RELEASE);
    }
    FreeLibrary(util);
    return 0;
}
