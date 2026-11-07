/* Reuse the host harness's mapped retail image, API models and call log.
 * This adds runtime routines which have no service-table slot. */
#define main host_service_main
#include "hostdiff.c"
#undef main
#include "fs98/host_runtime.h"

static uint32_t (*runtime_address)(void *,uint32_t);
static struct fs98_host_runtime *runtime;
static uint32_t comparisons;

static void *runtime_fn(int side, uint32_t rva)
{
    return side ? (void *)(uintptr_t)runtime_address(NULL,rva)
                : (void *)(uintptr_t)(REAL_BASE+rva);
}

static void mirror_import_hooks(void)
{
    uint8_t *base=(uint8_t *)REAL_BASE;
    IMAGE_NT_HEADERS *nt=(IMAGE_NT_HEADERS *)(base+((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR *library=(IMAGE_IMPORT_DESCRIPTOR *)(base+
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for(;library->Name;library++) {
        uint32_t *names=(uint32_t *)(base+library->OriginalFirstThunk);
        void **slots=(void **)(base+library->FirstThunk);
        unsigned index;
        for(index=0;names[index];index++)
            if(!(names[index]&0x80000000u))
                runtime->patch_import((const char *)(base+names[index]+2),slots[index]);
    }
}

static void set_word(uint32_t va, uint32_t value)
{
    *(uint32_t *)(uintptr_t)va=value;
    *(uint32_t *)(our_data+va-DATA_VA)=value;
}

static void WINAPI timer_tick(void) { log_call(80,0,0,0,0); }

static void check_timer_runtime(void)
{
    static const uint32_t inputs[]={0x4187f0,0x4187ec,0x418848,0x41c5a0,
        0x41bfe4,0x417020,0x41c224,0x41c5a4,0x41c5a8,0x41c5ac};
    struct logged saved[32];
    uint32_t output[3],fields[]={0x4187f0,0x41bfe4,0x417020};
    uint32_t i,count;
    for(i=0;i<sizeof inputs/sizeof inputs[0];i++) {
        uint32_t value=rnd()%4 ? 0 : rnd();
        if(i==0) value=rnd()%30;
        set_word(inputs[i],value);
    }
    set_word(0x418838,rnd()%4 ? P(timer_tick) : 0);
    ncalls=0; memset(calls,0,sizeof calls);
    ((void (WINAPI *)(UINT,UINT,DWORD_PTR,DWORD_PTR,DWORD_PTR))runtime_fn(0,0x1950))(0,0,0,0,0);
    count=ncalls; memcpy(saved,calls,sizeof saved);
    for(i=0;i<3;i++) output[i]=*(uint32_t *)(uintptr_t)fields[i];
    ncalls=0; memset(calls,0,sizeof calls);
    ((void (WINAPI *)(UINT,UINT,DWORD_PTR,DWORD_PTR,DWORD_PTR))runtime_fn(1,0x1950))(0,0,0,0,0);
    if(count!=ncalls || memcmp(saved,calls,sizeof saved)) {
        printf("timer transcript differs\n"); differ++;
    }
    for(i=0;i<3;i++) if(output[i]!=*(uint32_t *)(our_data+fields[i]-DATA_VA)) {
        printf("timer state differs at %x\n",fields[i]); differ++;
    }
    comparisons++;
}

static void check_lifecycle_runtime(void)
{
    uint32_t records[20],priorities[4],side,i,first_count=0;
    uint32_t first_links[4][8],first_imports[4][8];
    struct logged saved[32];
    for(i=0;i<4;i++) {
        records[i*5]=0;
        records[i*5+1]=0x40+i;
        records[i*5+2]=0x6000+0x10*i;
        records[i*5+3]=P(fake_link[i]);
        records[i*5+4]=1;
        priorities[i]=rnd()%3 ? (rnd()%5+1)*0x10000000u : 0;
    }
    seed_list(MODS_OFF,4,0x14,records);
    for(side=0;side<2;side++) {
        uint8_t *image=side ? our_data : (uint8_t *)DATA_VA;
        uint32_t *list=*(uint32_t **)(image+MODS_OFF+0x18);
        modules_reset();
        for(i=0;i<4;i++) fake_link[i][4]=priorities[i];
        ncalls=0; memset(calls,0,sizeof calls);
        for(i=0;i<4;i++)
            ((fn2)runtime_fn(side,0x7e40))(P(list+i*5),0);
        /* Every import must be bound before the first init callback. */
        if(fake_imp[0][1]!=P(fake_link[1]) || fake_imp[1][1]!=P(fake_link[2]) ||
           fake_imp[3][1]!=P(fake_link[0]) || fake_imp[3][3]!=P(fake_link[1])) {
            printf("startup imports incomplete on side %u\n",side); differ++;
        }
        ((fn0)runtime_fn(side,0x7cc0))();
        for(i=0;i<4;i++) ((fn2)runtime_fn(side,0x7ec0))(P(list+i*5),0);
        for(i=0;i<4;i++) ((fn2)runtime_fn(side,0x7ef0))(P(list+i*5),0);
        ((fn0)runtime_fn(side,0x7d10))();
        if(!side) {
            first_count=ncalls; memcpy(saved,calls,sizeof saved);
            memcpy(first_links,fake_link,sizeof first_links);
            memcpy(first_imports,fake_imp,sizeof first_imports);
        } else if(first_count!=ncalls || memcmp(saved,calls,sizeof saved) ||
                  memcmp(first_links,fake_link,sizeof first_links) ||
                  memcmp(first_imports,fake_imp,sizeof first_imports)) {
            printf("startup/shutdown transcript or module state differs\n"); differ++;
        }
    }
    free(*(void **)(DATA_VA+MODS_OFF+0x18));
    free(*(void **)(our_data+MODS_OFF+0x18));
    comparisons++;
}

static void check_filter_runtime(void)
{
    uint8_t instruction[8]={0};
    EXCEPTION_RECORD record;
    CONTEXT original,real,ours;
    EXCEPTION_POINTERS pair;
    uint32_t i,r,o,prefix=rnd()%3==0;
    uint32_t *words=(uint32_t *)&original;
    for(i=0;i<sizeof original/4;i++) words[i]=rnd();
    instruction[prefix?1:0]=rnd()&1 ? 0xf6 : 0xf7;
    instruction[(prefix?1:0)+1]=(uint8_t)(0xf0+(rnd()%16));
    if(prefix) instruction[0]=0x66;
    original.Eip=P(instruction);
    memset(&record,0,sizeof record);
    record.ExceptionCode=rnd()%5 ? EXCEPTION_INT_DIVIDE_BY_ZERO : EXCEPTION_ACCESS_VIOLATION;
    real=ours=original;
    pair.ExceptionRecord=&record;
    pair.ContextRecord=&real;
    r=((fn1)runtime_fn(0,0x119a0))(P(&pair));
    pair.ContextRecord=&ours;
    o=((fn1)runtime_fn(1,0x119a0))(P(&pair));
    if(r!=o || memcmp(&real,&ours,sizeof real)) {
        printf("divide repair differs: %02x %02x %02x returns %x/%x\n",
               instruction[0],instruction[1],instruction[2],r,o); differ++;
    }
    comparisons++;
}

int main(int argc,char **argv)
{
    HMODULE candidate;
    int (*initialize)(void);
    uint32_t count=argc>1 ? strtoul(argv[1],NULL,0) : 20000,i;
    setvbuf(stdout,NULL,_IONBF,0);
    SetUnhandledExceptionFilter(on_fault);
    {
        MEMORY_BASIC_INFORMATION region;
        if (VirtualQuery((void *)REAL_BASE,&region,sizeof region)) {
            DWORD flags=MEM_COMMIT | (region.State==MEM_FREE ? MEM_RESERVE : 0);
            if (!VirtualAlloc((void *)REAL_BASE,0x21000,flags,PAGE_EXECUTE_READWRITE)) {
                printf("cannot commit retail region: state=%lx error=%lu\n",region.State,GetLastError());
                return 2;
            }
        }
    }
    map_real();
    candidate=LoadLibraryA("HOST_ours.dll");
    if(!candidate) { printf("cannot load runtime DLL: %lu\n",GetLastError()); return 2; }
    initialize=(void *)GetProcAddress(candidate,"host_runtime_init");
    runtime_address=(void *)GetProcAddress(candidate,"host_runtime_address");
    runtime=(void *)GetProcAddress(candidate,"OpenFS98Host");
    if(!initialize || !runtime_address || !runtime || !initialize()) return 2;
    load_ours();
    mirror_import_hooks();
    for(i=0;i<count && !differ;i++) {
        check_timer_runtime();
        check_lifecycle_runtime();
        check_filter_runtime();
    }
    printf("hostruntime: %u timer/lifecycle/divide comparisons, %d differ\n",comparisons,differ);
    return differ ? 1 : 0;
}
