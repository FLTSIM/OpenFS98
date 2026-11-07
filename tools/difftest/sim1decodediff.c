/* Real AIR records, original decoder and rebuilt explicit C side by side.
 * Unlike sim1airdiff this never patches or substitutes the decoder.
 */
#define main sim1_service_reference_main
#include "sim1diff.c"
#undef main

static uint8_t global[0x4000], global_expected[0x4000];
static uint32_t host[0xc0], scenery[0x20], util_table[0x40];
uint32_t decode_random;
void decode_rand(void);
static uint8_t aircraft[16384], aircraft_expected[16384];
static uint8_t aircraft_full[16384];
static uint32_t copies, expected_copies, messages, expected_messages;
static uint32_t decode_active, decode_exception, decode_access, expected_exception, expected_access;
static uint32_t fault_cases;
void decode_recover(void);
void decode_probe_0(void), decode_probe_4(void), decode_probe_12(void);
void decode_probe_36(void), decode_probe_256(void);
void decode_probe_nested(void), decode_probe_cdecl(void), decode_probe_fp(void);
static void (*native_probe_fn)(uint32_t *,uint32_t,uint32_t,const uint32_t *);
static uint32_t nested_probes;

void decode_nested_core(void)
{
    uint32_t registers[9]={0}, args[]={1,2,3};
    uint32_t expected[]={6,0x11112222,0x33334444,0x55556666,12,
                         0x77778888,0x9999aaaa,0xbbbbcccc,0x8d7};
    registers[8]=0x202;
    native_probe_fn(registers,(uintptr_t)decode_probe_12,3,args);
    registers[8]&=0xcd7;
    compare(expected,registers,sizeof expected,"native probe nested context");
    ++nested_probes;
}

static void native_probe(void)
{
    void (*probe)(uint32_t *,uint32_t,uint32_t,const uint32_t *) =
        (void *)GetProcAddress(sides[1].module,"Sim1NativeProbe");
    const uint32_t targets[] = {(uintptr_t)decode_probe_0,(uintptr_t)decode_probe_4,
        (uintptr_t)decode_probe_12,(uintptr_t)decode_probe_36,(uintptr_t)decode_probe_256,
        (uintptr_t)decode_probe_nested,(uintptr_t)decode_probe_cdecl,(uintptr_t)decode_probe_fp};
    const uint32_t sizes[] = {0,1,3,9,64,0,3,0}, cleanup[] = {0,4,12,36,256,0,0,0};
    uint32_t args[64], registers[9], expected[9], i, j, sum;
    uint8_t input_fp[108] __attribute__((aligned(16)));
    role=1;
    if(!probe) fail("native probe export",0);
    native_probe_fn=probe;
    for(i=0;i<8;i++) {
        sum=0;
        for(j=0;j<64;j++) { args[j]=0x1020304u*(j+1); if(j<sizes[i]) sum+=args[j]; }
        for(j=0;j<9;j++) registers[j]=0x13579bdu*(j+1);
        registers[8]=i==6?0xad7:0x202;
        expected[0]=sum; expected[1]=0x11112222; expected[2]=0x33334444;
        expected[3]=0x55556666; expected[4]=cleanup[i];
        expected[5]=0x77778888; expected[6]=0x9999aaaa; expected[7]=0xbbbbcccc;
        expected[8]=i==6?0x202:0xad7;
        if(i==7) {
            __asm__ volatile("fninit; fldpi; fld1; fnsave %0; frstor %0; fld1; fnsave %1; frstor %0"
                : "=m"(input_fp), "=m"(expected_fp) : : "st", "st(1)", "st(2)", "memory");
        }
        probe(registers,targets[i],sizes[i],args);
        if(i==7) {
            __asm__ volatile("fnsave %0" : "=m"(call_fp) : : "st", "st(1)", "st(2)", "memory");
            compare_fp();
        }
        registers[8]&=0xcd7u; expected[8]&=0xcd7u;
        compare(expected,registers,sizeof expected,"native probe registers/flags/ESP");
    }
    if(nested_probes!=1) fail("native probe nested coverage",nested_probes);
    printf("Native bridge: five cleanup sizes, cdecl, nested callback, GPRs/flags/x87 match known callees\n");
}

static LONG CALLBACK decode_fault(EXCEPTION_POINTERS *pointers)
{
    if (!decode_active || pointers->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;
    decode_exception = pointers->ExceptionRecord->ExceptionCode;
    decode_access = pointers->ExceptionRecord->ExceptionInformation[0];
    if(!role && !mode)
        fprintf(stderr,"AIR invalid-table fault: vector=%u fault-rva=%x table-rva=%x esi=%x access=%x\n",vector,
                (uint32_t)pointers->ContextRecord->Eip-(uint32_t)(uintptr_t)sides[0].module,
                (uint32_t)pointers->ContextRecord->Ebx-(uint32_t)(uintptr_t)sides[0].module,
                (uint32_t)pointers->ContextRecord->Esi,(uint32_t)pointers->ExceptionRecord->ExceptionInformation[1]);
    decode_active = 0;
    pointers->ContextRecord->Eip = (uintptr_t)decode_recover;
    pointers->ContextRecord->Esp = call_sp;
    return EXCEPTION_CONTINUE_EXECUTION;
}

uint32_t __stdcall decode_move(void *out, const void *in, uint32_t size);
uint32_t __stdcall decode_message(uint32_t message, uint32_t value);
uint32_t __stdcall decode_scenery(void *record);
uint32_t __stdcall decode_move_core(void *out, const void *in, uint32_t size)
{
    ++copies;
    memmove(out, in, size);
    return 0;
}
uint32_t __stdcall decode_message_core(uint32_t message, uint32_t value)
{
    messages = messages * 33 + message * 7 + value;
    return 0;
}
uint32_t __stdcall decode_scenery_core(void *record)
{
    messages = messages * 33 + read32(record);
    return 0;
}

int main(int argc, char **argv)
{
    static const char *files[] = {
        "cd/fs98/Aircraft/B737_400/boeing.air", "cd/fs98/Aircraft/Bell206B/bell.air",
        "cd/fs98/Aircraft/C182/cessna.air", "cd/fs98/Aircraft/C182/cessnarg.air",
        "cd/fs98/Aircraft/Extra300/extra.air", "cd/fs98/Aircraft/Lear45/Lear45.air",
        "cd/fs98/Aircraft/Schweizr/sail.air", "cd/fs98/Aircraft/Sopcamel/sopwith.air"};
    const char *paths[2] = {"orig/Sim/SIM1.sim", argc > 2 ? argv[2] : "bin/sim1-diff/SIM1_COMPLETE_TEST.DLL"};
    uint32_t entries[2], file, i, size, registers[8], first = argc > 1 && !strcmp(argv[1], "ours-first");
    uint32_t cases = 0, trial, chunks[128], lengths[128], chunk_count, original_size, position, order[128], j;
    void *reservation = NULL;
    int orientation = argc > 3 && !strcmp(argv[3], "orientation");
    int setup = argc > 3 && !strcmp(argv[3], "setup");
    HMODULE util;
    SetUnhandledExceptionFilter(fault);
    AddVectoredExceptionHandler(1, decode_fault);
    if(argc > 1 && !strcmp(argv[1], "relocate")) {
        reservation = VirtualAlloc((void *)0x22000000,0x100000,MEM_RESERVE,PAGE_NOACCESS);
        if(!reservation) fail("reserve SIM1 image base",GetLastError());
    }
    util=LoadLibraryA("orig/Modules/UTIL.DLL");
    if(!util) fail("decoder UTIL load",GetLastError());
    host[0x21c/4]=(uintptr_t)decode_move;
    host[0x3c/4]=(uintptr_t)decode_message;
    host[0x40/4]=(uintptr_t)decode_message;
    scenery[0x18/4]=(uintptr_t)decode_scenery;
    memcpy(util_table,GetProcAddress(util,"Linkage"),sizeof util_table);
    util_table[0x70/4]=(uintptr_t)decode_rand;
    for(i=0;i<2;i++) {
        uint32_t side=first^i;
        struct side *s=sides+side;
        s->module=LoadLibraryA(paths[side]);
        if(!s->module) fail("decoder module load",GetLastError());
        s->image=(uint8_t *)GetProcAddress(s->module,"ImportTable");
        s->link=(uint32_t *)GetProcAddress(s->module,"Linkage");
        if(!s->image || !s->link || s->link[0]!=25) fail("decoder exports",side);
        if(((uintptr_t)s->module==0x22000000) != (!reservation && i==0)) fail("decoder load placement",side);
        write32(s->image+4,(uintptr_t)host);
        write32(s->image+12,(uintptr_t)global);
        write32(s->image+36,(uintptr_t)scenery);
        write32(s->image+68,(uintptr_t)util_table);
        entries[side]=side?(uintptr_t)GetProcAddress(s->module,"Sim1AirDecoder"):(uintptr_t)s->module+0xaf90;
        if(orientation) entries[side]=side?(uintptr_t)GetProcAddress(s->module,"Sim1Orientation"):(uintptr_t)s->module+0x40d0;
        if(!owns_code(s->module,entries[side])) fail("decoder code ownership",side);
        memcpy(s->pristine,s->image,DATA_SIZE);
    }
    if(sides[0].module==sides[1].module) fail("independent decoder",0);
    native_probe();
    for(file=0;file<8;file++) {
        FILE *stream=fopen(files[file],"rb");
        if(!stream) fail("AIR fixture open",file);
        original_size=fread(aircraft_full,1,sizeof aircraft_full,stream); fclose(stream);
        if(original_size==sizeof aircraft_full || original_size<8) fail("AIR fixture size",file);
        chunk_count=position=0;
        while(position<original_size) {
            if(chunk_count==128 || position+8>original_size) fail("AIR chunk header",position);
            chunks[chunk_count]=position;
            lengths[chunk_count]=8+read32(aircraft_full+position+4);
            position+=lengths[chunk_count++];
            if(position>original_size) fail("AIR chunk payload",position);
        }
        /* Complete inputs, changed record order, repeated records, each
         * individual record omitted, and a zero-length file. All payloads
         * retain their actual record lengths and remain fully backed.
         */
        for(trial=0;trial<chunk_count+35;trial++) {
            if(setup && trial) continue;
            size=0; memset(aircraft,0xa5,sizeof aircraft);
            for(i=0;i<chunk_count;i++) order[i]=i;
            if(trial>=3 && trial<34)
                for(i=chunk_count-1;i;i--) { uint32_t swap=rng()%(i+1), temp=order[i]; order[i]=order[swap]; order[swap]=temp; }
            if(trial==1) for(i=0;i<chunk_count;i++) order[i]=chunk_count-1-i;
            if(trial!=chunk_count+34)
                for(i=0;i<chunk_count;i++) {
                    if(trial>=34 && trial-34==order[i]) continue;
                    for(j=0;j<(trial==2?2u:1u);j++) {
                        if(size+lengths[order[i]]>sizeof aircraft) fail("AIR repeated fixture capacity",size);
                        memcpy(aircraft+size,aircraft_full+chunks[order[i]],lengths[order[i]]);
                        size+=lengths[order[i]];
                    }
                }
            memcpy(aircraft_expected,aircraft,sizeof aircraft);
        for(mode=0;mode<12;mode++) {
            static const uint32_t precision[]={0,2,3};
            vector=file*1000+trial; slot=0xaf90;
            call_control=0x7f|(precision[mode/4]<<8)|((mode%4)<<10);
            call_preload=0;
            for(i=0;i<7;i++) call_inputs[i]=0x12340000+0x101*i;
            for(role=0;role<2;role++) {
                memcpy(sides[role].image,sides[role].pristine,DATA_SIZE);
                memset(global,0,sizeof global); copies=messages=0; decode_random=(mode&1)?0x80000000u:0;
                if(orientation) write32(global+0x57c,0xfff50000u);
                call_target=entries[role]; call_argc=orientation?0:2;
                call_args[0]=(uintptr_t)aircraft; call_args[1]=size;
                decode_exception=decode_access=0; decode_active=1;
                sim1_call();
                decode_active=0;
                if(setup && !decode_exception) {
                    /* Seed nonzero state so clearing already-zero defaults
                     * cannot make a missing setup call look equivalent. */
                    static const uint32_t fields[]={0x1538,0x1540,0x1548,0x13a0,0x13a8,0x13b0,
                        0x13b8,0x13c0,0x13c8,0x13e8,0x13f0,0x13f8,0x1358,0x1360,0x1368};
                    for(i=0;i<sizeof fields/sizeof fields[0];i++) {
                        write32(sides[role].image+fields[i],0x12345678u+i);
                        write32(sides[role].image+fields[i]+4,0x3ff00000u+i);
                    }
                    write32(global+0xbe8,mode&1?0x3ffe:0x3fff);
                    call_target=sides[role].link[7]; call_argc=0;
                    sim1_call();
                }
                if(!orientation && !setup && file==6 && trial==0 && read32(global+0x57c)!=(decode_random?0xb0000u:0xfff50000u))
                    fail("glider initial pitch oracle",read32(global+0x57c));
                compare(sides[role].pristine,sides[role].image,HEADER_SIZE,"decoder import/Linkage prefix");
                compare(aircraft_expected,aircraft,sizeof aircraft,"AIR input modified");
                memcpy(registers,call_registers,sizeof registers);
                if(registers[7]) fail("decoder ESP",registers[7]);
                if(!role) {
                    memcpy(expected_image,sides[0].image,DATA_SIZE);
                    memcpy(global_expected,global,sizeof global);
                    memcpy(expected_fp,call_fp,sizeof call_fp);
                    memcpy(expected_output,registers,sizeof registers);
                    expected_copies=copies; expected_messages=messages;
                    expected_exception=decode_exception; expected_access=decode_access;
                } else {
                    if(expected_exception!=decode_exception || expected_access!=decode_access)
                        fail("decoder fault outcome",decode_exception);
                    /* Interrupted x87 condition codes and GPRs are not
                     * function results. An invalid table can read outside
                     * the image before its eventual access fault, so compare
                     * its fault class, state prefix and imports instead.
                     */
                    if(!decode_exception) compare_fp();
                    compare(expected_image+HEADER_SIZE,sides[1].image+HEADER_SIZE,DATA_SIZE-HEADER_SIZE,"decoded image");
                    compare(global_expected,global,sizeof global,"decoded GLOBAL");
                    if(!decode_exception) compare(expected_output,registers,sizeof registers,"decoder registers");
                    if(copies!=expected_copies || messages!=expected_messages) fail("decoder imports",copies);
                    if(decode_exception) ++fault_cases;
                }
            }
            ++cases;
        }
        }
        printf("AIR %s: %u record arrangements, 12 FP modes, real decoder%s matches\n",files[file],setup?1:chunk_count+35,setup?" + setup":"");
    }
    printf("PASS: %u real AIR%s comparisons (%u paired invalid-input access faults)\n",cases,setup?" + aircraft setup":" decoder",fault_cases);
    FreeLibrary(sides[1].module); FreeLibrary(sides[0].module); FreeLibrary(util);
    if(reservation) VirtualFree(reservation,0,MEM_RELEASE);
    return 0;
}
