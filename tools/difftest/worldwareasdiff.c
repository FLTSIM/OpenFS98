/* Independent weather-area fixtures. Only imported controls and the
 * caller-owned validation/close callbacks are patched on the retail side. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/world/world_wareas.h"
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define THROW_CODE 0xe0429811

struct world_fixture {
    uint8_t before[32], object[0x2720], descriptors[2][0x2c], rows[3][0xc8], text[3][9][80], after[32];
};
static struct world_fixture s_fixture,s_initial,s_reference;
static HMODULE s_retail;
static void *s_jump[5], *s_vtable[60], *s_other_vtable[60];
static char s_logs[2][8192];
static uint32_t s_rng,s_scenario,s_operation,s_calls,s_length,s_throw_at,s_comparisons,s_throws,s_self_checks;
static int s_side;
static const uint32_t s_fields[]={8,0x1c,0x30,0x44,0x58,0x6c,0x80,0x94,0xa8};
static uint32_t world_random(void) { s_rng^=s_rng<<13;s_rng^=s_rng>>17;s_rng^=s_rng<<5;return s_rng; }
static uint32_t world_offset(const void *p) { return (uint32_t)((const uint8_t *)p-(const uint8_t *)&s_fixture); }
static void world_log(const char *format,...)
{
    va_list args;int length;va_start(args,format);length=vsnprintf(s_logs[s_side]+s_length,8192-s_length,format,args);va_end(args);
    if(length<0||(uint32_t)length>=8192-s_length)exit(2);
    s_length+=length;
    if(++s_calls==s_throw_at)RaiseException(THROW_CODE,0,0,NULL);
    if((s_scenario&1)&&s_calls==1+s_scenario%9)PTR(s_fixture.object,0x10f8)=s_fixture.descriptors[1];
    if((s_scenario&2)&&s_calls==1+s_scenario%11)PTR(s_fixture.object,0x10fc)=s_fixture.rows[1];
    if(s_scenario&4)PTR(s_fixture.object,0)=s_other_vtable;
}
static int SC world_send(uint32_t window,uint32_t message,uint32_t first,uint32_t second)
{
    if(message==0x180)world_log("send %x %x %x [%s]\n",window,message,first,(char *)second);
    else if(message==0x19a)world_log("send %x %x %x row=%x\n",window,message,first,world_offset((void *)second));
    else world_log("send %x %x %x %x\n",window,message,first,second);
    switch(message){case 0x188:return s_scenario%3;case 0x199:return (int)s_fixture.rows[s_scenario%3];case 0x18b:return s_scenario%4;case 0x180:return (s_scenario&8)?-1:1;default:return 0;}
}
static int TC world_enable(void *control,int enable)
{ world_log("enable %x %x\n",world_offset(control),(uint32_t)enable);U32(control,8)=enable;return -1; }
static void TC world_set_text(void *control,const char *text)
{ world_log("text %x [%s]\n",world_offset(control),text); }
static int TC world_get_text(void *control,char *text,int capacity)
{ world_log("get %x text=%x capacity=%x\n",world_offset(control),world_offset(text),(uint32_t)capacity);if(capacity>1)strcpy(text,"7");return 1; }
static int TC world_check(void *control,int value)
{ world_log("check %x %x\n",world_offset(control),(uint32_t)value);U32(control,8)=value;return -1; }
static void TC world_focus(void *control) { world_log("focus %x\n",world_offset(control)); }
static int SC world_post(uint32_t window,uint32_t message,uint32_t first,uint32_t second)
{ world_log("post %x %x %x %x\n",window,message,first,second);return -1; }
static int TC world_error(void *object,void *control,void *status)
{ world_log("error %x %x %x\n",world_offset(object),world_offset(control),world_offset(status));return s_scenario&8?0:(int)0x80000001; }
static void world_diagnostic(void *notify,const char *where,const char *format,uint32_t descriptor)
{ world_log("diagnostic %x [%s] [%s] %x\n",world_offset(notify),where,format,descriptor); }
static int SC world_validate(void *descriptor)
{ world_log("validate %x\n",world_offset(descriptor));return s_scenario&8?(int)0x80000001:0; }
static void SC world_action(void *descriptor) { world_log("action %x\n",world_offset(descriptor)); }
static int TC world_virtual_validate(void *object,uint32_t id)
{ world_log("virtual.validate %x %x\n",world_offset(object),id);return s_scenario&8?123:0; }
static int TC world_virtual_error(void *object,uint32_t id)
{ world_log("virtual.error %x %x\n",world_offset(object),id);return s_scenario&16?321:0; }
static int TC world_dispatch(void *object,uint32_t id)
{ world_log("dispatch %x %x\n",world_offset(object),id);return (int)(id^0x73); }
static void TC world_close(void *object) { world_log("close %x\n",U32(PTR(object,0x10f8),0)); }
static void TC world_other_close(void *object) { world_log("other.close\n");world_close(object); }
static void *world_resolve(uint32_t slot)
{
    switch(slot){case 0x258b0:return world_send;case 0x255c8:return world_enable;case 0x255bc:return world_set_text;
    case 0x25598:return world_get_text;case 0x2577c:return world_check;case 0x25778:return world_error;
    case 0x25590:return world_focus;case 0x258a0:return world_post;case 0x257a8:return world_diagnostic;default:exit(2);}
}
static const struct fs98_world_wareas_services s_services={NULL,world_resolve,NULL,NULL,NULL};
static void world_patch(uint32_t rva,void *value)
{ DWORD old;void **slot=(void **)((uint8_t *)s_retail+rva);if(!VirtualProtect(slot,4,PAGE_READWRITE,&old))exit(2);*slot=value;VirtualProtect(slot,4,old,&old); }
static void world_initialize(void)
{
    uint32_t byte,row,field;
    static const uint32_t values[]={0,1,2,3,0xffffffff,0x80000000,65536};
    for(byte=0;byte<sizeof(s_fixture);++byte)((uint8_t *)&s_fixture)[byte]=(uint8_t)world_random();
    PTR(s_fixture.object,0)=s_vtable;PTR(s_fixture.object,0x10f8)=s_fixture.descriptors[0];PTR(s_fixture.object,0x10fc)=s_fixture.rows[0];
    if(s_operation==4&&s_scenario%3!=2)U32(s_fixture.object,0x10f8)=s_scenario%3?0xcdcdcdcd:0;
    U32(s_fixture.object,0x1728)=0x33;
    for(row=0;row<2;++row){void *owner=s_fixture.descriptors[row];PTR(owner,4)=world_validate;
        PTR(owner,0x20)=PTR(owner,0x24)=PTR(owner,0x28)=world_action;PTR(owner,0x14)=s_fixture.rows;
        U32(owner,0x10)=1+(s_scenario+row)%3;U32(owner,0xc)=s_scenario%3;
    }
    for(row=0;row<3;++row){
        U32(s_fixture.rows[row],0)=values[(s_scenario/8+row)%7];
        for(field=0;field<9;++field){uint32_t offset=s_fields[field];
            U32(s_fixture.rows[row],offset-4)=s_scenario%11==field+1||(s_scenario%11==10&&field%2)?0x1007:0;
            PTR(s_fixture.rows[row],offset)=s_fixture.text[row][field];U32(s_fixture.rows[row],offset+4)=values[(s_scenario/16+field)%7];
            memset(s_fixture.text[row][field],0,80);snprintf((char *)s_fixture.text[row][field],80,"%u",field+row*10);
        }
    }
    s_initial=s_fixture;
}
static void world_invoke(void)
{
    static const uint32_t entries[]={0x11070,0x11860,0x10f90,0x10fb0,0x11970,0x113a0,0x11620,0x10e80,0x10ee0,0x10f50,0x11030,0x11050};
    void *object=s_fixture.object,*entry=(uint8_t *)s_retail+entries[s_operation];int result=0;
    uint32_t id=s_scenario%4==0?0x1b0b:s_scenario%4==1?0:0x1ae2;
    if(!s_side){
        if(s_operation==4)result=((int(TC *)(void *))entry)(object);
        else if(s_operation==5||s_operation==6)result=((int(TC *)(void *,uint32_t))entry)(object,id);
        else ((void(TC *)(void *))entry)(object);
    }else switch(s_operation){
        case 0:fs98_world_wareas_selection(object);break;case 1:fs98_world_wareas_populate(object);break;
        case 2:fs98_world_wareas_cancel(object);break;case 3:fs98_world_wareas_accept(object);break;
        case 4:result=fs98_world_wareas_has_descriptor(object);break;case 5:result=fs98_world_wareas_validate(object,id);break;
        case 6:result=fs98_world_wareas_error(object,id);break;
        case 7:case 8:case 9:fs98_world_wareas_action(object,0x20+(s_operation-7)*4);break;
        case 10:fs98_world_wareas_check(object,0xbc,0x2714);break;case 11:fs98_world_wareas_check(object,0xc4,0x264c);break;
    }
    if(!s_side&&!s_throw_at)++s_self_checks;
    world_log("return %x\n",(uint32_t)result);
}
void fs98_world_wareas_context_map(uint32_t window) { (void)window;exit(2); }
static LONG CALLBACK world_exception(EXCEPTION_POINTERS *exception)
{
    if (exception->ExceptionRecord->ExceptionCode == THROW_CODE) {
        s_throw_at = 0;
        if (!s_side) ++s_throws;
        /* These event routines have no local EH frames or cleanup. This is
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
    world_patch(0x255bc,world_set_text);world_patch(0x25598,world_get_text);
    world_patch(0x2577c,world_check);world_patch(0x25778,world_error);
    world_patch(0x25590,world_focus);world_patch(0x258a0,world_post);world_patch(0x257a8,world_diagnostic);
    s_vtable[0xd8/4]=world_virtual_validate;s_vtable[0xdc/4]=world_virtual_error;s_vtable[0xe0/4]=world_dispatch;
    s_vtable[0xec/4]=world_close;
    memcpy(s_other_vtable,s_vtable,sizeof(s_vtable));s_other_vtable[0xec/4]=world_other_close;
    fs98_world_wareas_set_services(&s_services);
    for (s_scenario = 0; s_scenario < count; ++s_scenario) {
        for (s_operation = 0; s_operation < 12; ++s_operation) {
            world_initialize();
            if (world_compare(0)) return 1;
            limit = s_calls - 1;
            if (interrupt) for (position = 1; position <= limit; ++position)
                if (world_compare(position)) return 1;
        }
    }
    printf("World wareas core: %u scenarios, 12 operations, %u comparisons, 0 differ; seed=%x interrupts=%u witnesses=%u\n",
        count, s_comparisons, seed, s_throws, s_self_checks);
    FreeLibrary(s_retail);
    return 0;
}
