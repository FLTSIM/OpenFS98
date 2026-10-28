/* Independent basic-weather fixtures. Only imported controls and the
 * caller-owned validation/close callbacks are patched on the retail side. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/world/world_weather.h"
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define THROW_CODE 0xe0429811
struct world_fixture {
    uint8_t before[32], object[0x1960], descriptors[2][0x54], after[32];
};
static struct world_fixture s_fixture, s_initial, s_reference;
static HMODULE s_retail;
static void *s_jump[5], *s_vtable[60], *s_other_vtable[60];
static char s_logs[2][8192];
static uint32_t s_rng, s_scenario, s_operation, s_calls, s_length;
static uint32_t s_throw_at, s_comparisons, s_throws, s_self_checks;
static int s_side;
static int s_selection[3];

static uint32_t world_random(void)
{ s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return s_rng; }
static uint32_t world_offset(const void *pointer)
{ return (uint32_t)((const uint8_t *)pointer - (const uint8_t *)&s_fixture); }
static void world_log(const char *format, ...)
{
    va_list args;
    int length;
    va_start(args, format);
    length = vsnprintf(s_logs[s_side] + s_length, 8192 - s_length, format, args);
    va_end(args);
    if (length < 0 || (uint32_t)length >= 8192 - s_length) exit(2);
    s_length += (uint32_t)length;
    if (++s_calls == s_throw_at) RaiseException(THROW_CODE, 0, 0, NULL);
    /* Change the descriptor and vtable at every possible external boundary.
     * Distinct descriptor payloads expose caching across control calls. */
    if ((s_scenario & 1) && s_calls == 1 + (s_scenario / 8) % 6)
        PTR(s_fixture.object, 0x10e0) = s_fixture.descriptors[1];
    if (s_scenario & 2) PTR(s_fixture.object, 0) = s_other_vtable;
}
static int SC world_send(uint32_t window,uint32_t message,uint32_t first,uint32_t second)
{
    world_log("send %x %x %x %x\n",window,message,first,second);
    return s_selection[window%3];
}
static int TC world_enable(void *control,int enable)
{ world_log("enable %x %d\n",world_offset(control),enable);U32(control,8)=(uint32_t)enable;return -1; }
static int SC world_validate(void *descriptor)
{
    world_log("validate %x answer=%x fields=%x/%x/%x\n",world_offset(descriptor),U32(descriptor,0),U32(descriptor,0x14),U32(descriptor,0x18),U32(descriptor,0x1c));
    return s_scenario&4 ? (int)0x80000001 : 0;
}
static void TC world_close(void *object)
{ world_log("close answer=%x mode=%x\n",U32(PTR(object,0x10e0),0),U32(object,0x195c)); }
static void TC world_other_close(void *object)
{ world_log("other.close\n");world_close(object); }
static void *world_resolve(uint32_t slot)
{
    switch(slot){case 0x258b0:return world_send;case 0x255c8:return world_enable;default:exit(2);}
}
static const struct fs98_world_weather_services s_services={NULL,world_resolve,NULL,NULL};
static void world_patch(uint32_t rva,void *value)
{
    DWORD old;void **slot=(void **)((uint8_t *)s_retail+rva);
    if(!VirtualProtect(slot,4,PAGE_READWRITE,&old))exit(2);
    *slot=value;if(!VirtualProtect(slot,4,old,&old))exit(2);
}
static const uint32_t s_entries[]={0x34f0,0x3670,0x34d0,0x3490,0xb360,0xb370,0x3720,0x3730};
static void world_initialize(void)
{
    uint32_t byte,index,row;
    static const uint32_t values[]={0,1,2,3,4,5,0xffffffff,0x80000000,65536,65535};
    for(byte=0;byte<sizeof(s_fixture);++byte)((uint8_t *)&s_fixture)[byte]=(uint8_t)world_random();
    PTR(s_fixture.object,0)=s_vtable;PTR(s_fixture.object,0x10e0)=s_fixture.descriptors[0];
    if(s_operation==4&&s_scenario%3==0)PTR(s_fixture.object,0x10e0)=NULL;
    if(s_operation==4&&s_scenario%3==1)U32(s_fixture.object,0x10e0)=0xcdcdcdcd;
    for(index=0;index<3;++index){
        U32(s_fixture.object,0x1654-index*0x48)=index;
        s_selection[index]=(int)values[(s_scenario/4+index)%10];
    }
    for(row=0;row<2;++row){
        PTR(s_fixture.descriptors[row],4)=world_validate;
        U32(s_fixture.descriptors[row],0x14)=values[(s_scenario+row)%10];
        U32(s_fixture.descriptors[row],0x18)=values[(s_scenario/10+row)%10];
        U32(s_fixture.descriptors[row],0x1c)=values[(s_scenario/100+row)%10];
    }
    s_initial=s_fixture;
}
static void world_witness(int result)
{
    int bad=0;
    if(s_operation==4)bad=result!=(PTR(s_initial.object,0x10e0)!=NULL);
    if(s_operation==5)bad=result!=((s_scenario&4)?(int)0x80000001:0);
    if(s_operation==6)bad=result!=0;
    if(s_operation==0&&!(s_scenario&1)){
        bad|=U32(s_fixture.descriptors[0],0x14)!=((uint32_t)s_selection[0]<=4?(uint32_t)s_selection[0]:0);
        bad|=U32(s_fixture.descriptors[0],0x1c)!=((uint32_t)s_selection[2]<=3?(uint32_t)s_selection[2]:0);
        bad|=U32(s_fixture.descriptors[0],0x18)!=((uint32_t)s_selection[1]<=3?(uint32_t)s_selection[1]:4);
    }
    if(s_operation==2)bad|=U32(s_fixture.object,0x195c)!=2||U32(s_fixture.descriptors[0],0)!=0;
    if(s_operation==3)bad|=U32(s_fixture.object,0x195c)!=1||!strstr(s_logs[0],"close answer=");
    if(s_operation==7)bad|=s_calls!=((s_selection[0]==0)?3u:4u);
    if(bad){fprintf(stderr,"fixture witness failed case=%u op=%u\n%s",s_scenario,s_operation,s_logs[0]);exit(2);}
    ++s_self_checks;
}
static void world_invoke(void)
{
    void *object=s_fixture.object,*entry=(uint8_t *)s_retail+s_entries[s_operation];int result=0;
    if(!s_side){
        if(s_operation>=4&&s_operation<=6)result=((int(TC *)(void *))entry)(object);
        else ((void(TC *)(void *))entry)(object);
    }else switch(s_operation){
        case 0:fs98_world_weather_collect(object);break;
        case 1:fs98_world_weather_populate(object);break;
        case 2:fs98_world_weather_cancel(object);break;
        case 3:fs98_world_weather_accept(object);break;
        case 4:result=fs98_world_weather_has_descriptor(object);break;
        case 5:result=fs98_world_weather_validate(object);break;
        case 6:result=fs98_world_weather_error(object);break;
        case 7:fs98_world_weather_changed(object);break;
    }
    if(!s_side&&!s_throw_at)world_witness(result);
    world_log("return %x\n",(uint32_t)result);
}
/* Unused class services are supplied by the independent lifetime fixture. */
void fs98_world_weather_context_map(uint32_t window){(void)window;exit(2);}
static LONG CALLBACK world_exception(EXCEPTION_POINTERS *exception)
{
    if (exception->ExceptionRecord->ExceptionCode == THROW_CODE) {
        s_throw_at = 0;
        if (!s_side) ++s_throws;
        /* The eight routines have no local EH frames or cleanup. This is
         * interruption acceptance, not an MSVC class-lifetime unwind test. */
        __builtin_longjmp(s_jump, 1);
    }
    fprintf(stderr, "FAULT %lx at %p side=%d scenario=%u operation=%u\n",
        exception->ExceptionRecord->ExceptionCode, exception->ExceptionRecord->ExceptionAddress,
        s_side, s_scenario, s_operation);
    ExitProcess(2);
}
static void world_run(uint32_t throw_at)
{
    s_fixture = s_initial;
    s_calls = s_length = 0;
    s_logs[s_side][0] = 0;
    s_throw_at = throw_at;
    if (__builtin_setjmp(s_jump) == 0) world_invoke();
    else world_log("caught\n");
    if (memcmp(s_fixture.before, s_initial.before, sizeof(s_fixture.before)) ||
        memcmp(s_fixture.after, s_initial.after, sizeof(s_fixture.after))) exit(2);
}
static int world_compare(uint32_t throw_at)
{
    uint32_t byte;
    s_side = 0; world_run(throw_at); s_reference = s_fixture;
    s_side = 1; world_run(throw_at); ++s_comparisons;
    if (!memcmp(&s_reference, &s_fixture, sizeof(s_fixture)) && !strcmp(s_logs[0], s_logs[1])) return 0;
    fprintf(stderr, "DIFFER scenario=%u operation=%u throw=%u\n", s_scenario, s_operation, throw_at);
    for (byte = 0; byte < sizeof(s_fixture); ++byte)
        if (((uint8_t *)&s_reference)[byte] != ((uint8_t *)&s_fixture)[byte]) {
            fprintf(stderr, "byte=%x retail=%x ours=%x\n", byte, ((uint8_t *)&s_reference)[byte], ((uint8_t *)&s_fixture)[byte]); break;
        }
    fprintf(stderr, "RETAIL:\n%sOURS:\n%s", s_logs[0], s_logs[1]);
    return 1;
}
int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? strtoul(argv[1], NULL, 0) : 2000;
    uint32_t seed = argc > 2 ? strtoul(argv[2], NULL, 0) : 0x4a971b63;
    uint32_t position, limit;
    int interrupt = argc > 3 && !strcmp(argv[3], "interrupt");
    if (!count || !seed || (argc > 3 && strcmp(argv[3], "normal") && !interrupt)) return 2;
    s_rng = seed;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    AddVectoredExceptionHandler(1, world_exception);
    s_retail = LoadLibraryExA("orig/English/World.DLL", NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_retail || GetProcAddress(s_retail, "Linkage") != (FARPROC)((uint8_t *)s_retail + 0x24120)) return 2;
    world_patch(0x258b0,world_send);world_patch(0x255c8,world_enable);
    s_vtable[0xec/4]=world_close;
    memcpy(s_other_vtable,s_vtable,sizeof(s_vtable));s_other_vtable[0xec/4]=world_other_close;
    fs98_world_weather_set_services(&s_services);
    for (s_scenario = 0; s_scenario < count; ++s_scenario) {
        for (s_operation = 0; s_operation < 8; ++s_operation) {
            world_initialize();
            if (world_compare(0)) return 1;
            limit = s_calls - 1;
            if (interrupt) for (position = 1; position <= limit; ++position)
                if (world_compare(position)) return 1;
        }
    }
    printf("World weather core: %u scenarios, 8 operations, %u comparisons, 0 differ; seed=%x interrupts=%u witnesses=%u\n",
        count, s_comparisons, seed, s_throws, s_self_checks);
    FreeLibrary(s_retail);
    return 0;
}
