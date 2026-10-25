/* Original Sim PE32 routines versus the independent C implementations. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/sim/sim_internal.h"
#include "../../src/sim/sim_tables.inc"
#define TC FS98_SIM_THISCALL
#define SC FS98_SIM_STDCALL
#define D32(p, o) (*(uint32_t *)((uint8_t *)(p)+(o)))
#define U16(p, o) (*(uint16_t *)((uint8_t *)(p)+(o)))
#define PTR(p, o) (*(void **)((uint8_t *)(p)+(o)))
#define THROW_CODE 0x0e041a24
#define LOG_SIZE 65536
struct fixture {
    uint8_t guard_before[32], object[0x3000], descriptor[2][0x300];
    uint8_t pages[2][0x1200], notify[3][32], help[32], exchange[32];
    char strings[2][2][32];
    uint8_t guard_after[32];
};
static struct fixture s_fixture, s_initial, s_reference;
static HMODULE s_retail;
static uint32_t s_rng, s_seed, s_scenario, s_operation, s_calls, s_throw_at, s_saved_seh;
static uint32_t s_log_length, s_script, s_kind, s_descriptor_offset, s_comparisons, s_throws, s_faults, s_swaps;
static int s_side, s_lifetime;
static char s_logs[2][LOG_SIZE];
static void *s_jump[5], *s_vtable[60], *s_other_vtable[60], *s_notify_vtable[60], *s_tabs_vtable[60];
static uint32_t next_random(void) { s_rng^=s_rng<<13; s_rng^=s_rng>>17; s_rng^=s_rng<<5; return s_rng; }
static uint32_t offset(const void *p) { return (uint32_t)((const uint8_t *)p-(const uint8_t *)&s_fixture); }
static void put(const char *format, ...)
{
    va_list args; int count;
    if(s_log_length>LOG_SIZE-2048)exit(2);
    va_start(args,format);count=vsnprintf(s_logs[s_side]+s_log_length,LOG_SIZE-s_log_length,format,args);va_end(args);
    if(count<0 || (uint32_t)count>=LOG_SIZE-s_log_length)exit(2);
    s_log_length+=(uint32_t)count;
    if(++s_calls==s_throw_at)RaiseException(THROW_CODE,0,0,0);
}
static void mutate(void)
{
    void *object=s_fixture.object, *descriptor;
    if(s_lifetime || s_calls!=1+s_scenario%4)return;
    descriptor=PTR(object,s_descriptor_offset);
    if((s_script&1) && (descriptor==s_fixture.descriptor[0] || descriptor==s_fixture.descriptor[1])) {
        PTR(object,s_descriptor_offset)=descriptor==s_fixture.descriptor[0]?s_fixture.descriptor[1]:s_fixture.descriptor[0];
        if(!s_side)++s_swaps;
    }
    if(s_script&2)PTR(object,0)=s_other_vtable;
    if(s_script&4) {
        D32(object,0x13d4)^=0x12345678;D32(object,0x1590)^=0x12345678;D32(object,0x1684)^=0x12345678;
    }
    if(s_script&8) {
        PTR(object,0x2258)=s_fixture.pages[1];PTR(object,0x225c)=s_fixture.pages[0];
    }
}
static void *base_construct(void *object,uint32_t import_slot,int32_t template,void *parent)
{ put("base.construct %u import=%x template=%x parent=%u\n",offset(object),import_slot,template,(uint32_t)parent);D32(object,0)=0xba5e0000;D32(object,0x20)=s_seed;mutate();return object; }
static void construct(void *object,uint32_t import_slot)
{ put("construct %u import=%x\n",offset(object),import_slot);D32(object,0)=import_slot;D32(object,4)=s_seed;mutate(); }
static void destroy(void *object,uint32_t import_slot)
{ put("destroy %u import=%x vtable=%x\n",offset(object),import_slot,D32(object,0));D32(object,0)=import_slot;mutate(); }
static void *allocate(uint32_t size)
{ put("allocate %u\n",size);mutate();return s_scenario%11?s_fixture.object:0; }
static void ui_delete(void *object) { put("ui.delete %u\n",offset(object));mutate(); }
static void mfc_delete(void *object) { put("mfc.delete %u\n",offset(object));mutate(); }
static void dialog_delete(void *object) { put("dialog.delete %u\n",offset(object));mutate(); }
static int32_t create(void *object,uint32_t template,void *parent)
{ put("create %u template=%x parent=%u module=%u page=%u\n",offset(object),template,(uint32_t)parent,D32(object,0x80),D32(object,0x1fa8));mutate();return s_scenario&1?0:(int32_t)s_seed; }
static int32_t show(void *object,int32_t value) { put("show %u %d\n",offset(object),value);mutate();return s_scenario&2?0:(int32_t)s_seed; }
static void ddx(void *exchange,uint32_t id,void *control) { put("ddx %u id=%x control=%u\n",offset(exchange),id,offset(control));mutate(); }
static int32_t help(uint32_t window,const char *file,uint32_t command,const uint32_t *map)
{
    char text[2048];uint32_t index=0;int length=0;
    do { if(index>=256 || length>1900)exit(2);length+=snprintf(text+length,sizeof(text)-length,"%x:%x,",map[index],map[index+1]);index+=2; }while(map[index-2]||map[index-1]);
    put("help %u %s %u [%s]\n",window,file,command,text);mutate();return (int32_t)s_seed;
}
static int32_t enable_window(uint32_t window,int32_t enabled) { put("enable.window %u %d\n",window,enabled);mutate();return (int32_t)s_seed; }
static int32_t enable_control(void *control,int32_t enabled) { put("enable.control %u %d\n",offset(control),enabled);mutate();return (int32_t)s_seed; }
static int32_t window_long(uint32_t window,int32_t index) { put("window.long %u %d\n",window,index);mutate();return (int32_t)(s_scenario&1?window:window^1); }
static int32_t check(void *control,int32_t checked) { put("check %u %d\n",offset(control),checked);D32(control,0xbc)=(uint32_t)checked;mutate();return (int32_t)s_seed; }
static int32_t slider_get(void *control) { put("slider.get %u\n",offset(control));mutate();return (int32_t)s_seed; }
static void slider_range(void *control,int32_t min,int32_t max) { put("slider.range %u %d/%d\n",offset(control),min,max);mutate(); }
static int32_t slider_set(void *control,int32_t position) { put("slider.set %u %d\n",offset(control),position);mutate();return (int32_t)s_seed; }
static int32_t send_message(uint32_t window,uint32_t message,uint32_t first,uint32_t second) { put("send %u %x %u/%u\n",window,message,first,second);mutate();return (int32_t)(s_seed+s_calls); }
static void text(void *control,const char *value) { put("text %u [%s]\n",offset(control),value);mutate(); }
static void close_dialog(void *object) { put("base.close %u\n",offset(object));mutate(); }
static const struct fs98_sim_services s_services={base_construct,destroy,construct,allocate,ui_delete,mfc_delete,dialog_delete,
    create,show,ddx,help,enable_window,enable_control,window_long,check,slider_get,slider_range,slider_set,send_message,text,close_dialog};
#define BASE_HOOK(slot) static void *TC hook_##slot(void *object,int32_t template,void *parent){return base_construct(object,0x##slot,template,parent);}
#define CTOR_HOOK(slot) static void *TC hook_##slot(void *object){construct(object,0x##slot);return object;}
#define DTOR_HOOK(slot) static void TC hook_##slot(void *object){destroy(object,0x##slot);}
#include "../../src/sim/sim_hooks.inc"
#undef BASE_HOOK
#undef CTOR_HOOK
#undef DTOR_HOOK
static int32_t TC hook_create(void *object,uint32_t template,void *parent){return create(object,template,parent);}
static int32_t TC hook_show(void *object,int32_t value){return show(object,value);}
static void SC hook_ddx(void *exchange,uint32_t id,void *control){ddx(exchange,id,control);}
static void SC hook_dialog_delete(void *object){dialog_delete(object);}
static int32_t SC hook_help(uint32_t window,const char *file,uint32_t command,const uint32_t *map){return help(window,file,command,map);}
static int32_t SC hook_enable_window(uint32_t window,int32_t enabled){return enable_window(window,enabled);}
static int32_t TC hook_enable_control(void *control,int32_t enabled){return enable_control(control,enabled);}
static int32_t SC hook_window_long(uint32_t window,int32_t index){return window_long(window,index);}
static int32_t TC hook_check(void *control,int32_t checked){return check(control,checked);}
static int32_t TC hook_slider_get(void *control){return slider_get(control);}
static void TC hook_slider_range(void *control,int32_t min,int32_t max){slider_range(control,min,max);}
static int32_t TC hook_slider_set(void *control,int32_t position){return slider_set(control,position);}
static int32_t SC hook_send(uint32_t window,uint32_t message,uint32_t first,uint32_t second){return send_message(window,message,first,second);}
static void TC hook_text(void *control,const char *value){text(control,value);}
static void TC hook_close(void *object){close_dialog(object);}
static void TC virtual_close(void *object) { put("virtual.close %u answer=%u/%u\n",offset(object),D32(s_fixture.descriptor[0],0),D32(s_fixture.descriptor[1],0));mutate(); }
static void TC virtual_other_close(void *object){put("other.close\n");virtual_close(object);}
static void *TC virtual_delete(void *object,uint32_t flags) { put("virtual.delete %u flags=%u\n",offset(object),flags);mutate();return object; }
static void TC virtual_tabs_close(void *object) { put("tabs.close %u\n",offset(object));mutate(); }
static int32_t SC graph_callback(uint32_t operation,void *descriptor,uint32_t window)
{ put("graph.callback %u descriptor=%u window=%u state=%u answer=%u\n",operation,offset(descriptor),window,D32(s_fixture.object,0x1130),D32(descriptor,0));mutate();return (int32_t)s_seed; }
static void patch(uint32_t rva,void *value)
{ DWORD old;void **slot=(void **)((uint8_t *)s_retail+rva);if(!VirtualProtect(slot,4,PAGE_READWRITE,&old))exit(2);*slot=value;VirtualProtect(slot,4,old,&old); }
static void setup(void)
{
    void *vtables[8];uint32_t i;
#define BASE_HOOK(slot) patch(0x##slot,hook_##slot);
#define CTOR_HOOK(slot) patch(0x##slot,hook_##slot);
#define DTOR_HOOK(slot) patch(0x##slot,hook_##slot);
#include "../../src/sim/sim_hooks.inc"
#undef BASE_HOOK
#undef CTOR_HOOK
#undef DTOR_HOOK
    patch(0xd514,s_notify_vtable);patch(0xd5b4,allocate);patch(0xd574,ui_delete);patch(0xd358,mfc_delete);
    patch(0xd5b8,hook_dialog_delete);patch(0xd478,hook_create);patch(0xd47c,hook_show);patch(0xd3d4,hook_ddx);
    patch(0xd5d8,hook_help);patch(0xd5ec,hook_enable_window);patch(0xd388,hook_enable_control);
    patch(0xd5e8,hook_window_long);patch(0xd504,hook_check);patch(0xd4c8,hook_slider_get);
    patch(0xd4c0,hook_slider_range);patch(0xd4c4,hook_slider_set);patch(0xd5e4,hook_send);
    patch(0xd548,hook_text);patch(0xd56c,hook_close);patch(0xc14c,(void *)0x31415926);
    patch(0xd48c,(void *)GetProcAddress(LoadLibraryA("msvcrt.dll"),"__CxxFrameHandler"));
    for(i=0;i<8;++i)vtables[i]=(uint8_t *)s_retail+s_classes[i].vtable;
    fs98_sim_set_services(&s_services,vtables,s_notify_vtable,(uint8_t *)s_retail+0xa450);fs98_sim_set_module(0x31415926);
    s_vtable[59]=virtual_close;s_other_vtable[59]=virtual_other_close;
    s_vtable[1]=s_other_vtable[1]=s_notify_vtable[1]=virtual_delete;s_tabs_vtable[0xc4/4]=virtual_tabs_close;
}
struct operation {uint32_t rva,kind,signature;const char *name;};
static struct operation s_operations[160];static uint32_t s_operation_count;
static void add(uint32_t rva,uint32_t kind,uint32_t signature,const char *name)
{ struct operation *op=&s_operations[s_operation_count++];op->rva=rva;op->kind=kind;op->signature=signature;op->name=name; }
static void operations(void)
{
    uint32_t kind;
    for(kind=0;kind<8;++kind){const struct fs98_sim_class *c=&s_classes[kind];
        add(c->constructor,kind,0,"construct");add(c->destructor,kind,1,"destroy");add(c->delete,kind,2,"delete");
        add(c->constructor,kind,3,"roundtrip");add(c->constructor,kind,4,"construct.delete");
        add(c->exchange,kind,5,"exchange");add(c->help,kind,6,"help");add(c->context,kind,7,"context");}
    add(0x1380,0,8,"crash.factory");add(0x11a0,7,8,"settings.0.factory");add(0x1100,7,8,"settings.2.factory");add(0x1240,7,8,"settings.3.factory");add(0x12e0,7,8,"settings.5.factory");
    add(0x1660,0,9,"crash.tick");add(0x1690,0,10,"crash.close");add(0x16c0,0,11,"crash.accept");add(0x1710,0,12,"crash.cancel");add(0x1970,0,13,"crash.notify");
    add(0x2360,1,14,"damage.ignore");add(0x23d0,1,15,"damage.reset");add(0x2440,1,16,"damage.graph");
    add(0x35b0,2,17,"engine.count");add(0x3580,2,18,"engine.match");
    add(0x2990,2,19,"engine.all");add(0x2a10,2,20,"engine.1");add(0x2a70,2,21,"engine.2");add(0x2ad0,2,22,"engine.3");add(0x2b30,2,23,"engine.4");
    add(0x2ed0,2,24,"engines.commit");add(0x6ee0,6,25,"reliability.commit");add(0x6f30,6,26,"reliability.populate");
    add(0x5c60,4,27,"systems.selection");add(0x72e0,7,28,"settings.cancel");add(0x7380,7,29,"settings.close");
    add(0x7410,0,30,"post.destroy");add(0x1d40,0,31,"disable");add(0x1d50,0,32,"enable");
    add(0x1d20,0,33,"empty");add(0x1d30,0,34,"empty.arg");add(0x71f0,0,35,"zero.arg");add(0x6bb0,5,36,"zero.message");
}
static void initialize(void)
{
    uint32_t i;void *object=s_fixture.object;
    for(i=0;i<sizeof(s_fixture)/4;++i)((uint32_t *)&s_fixture)[i]=next_random();
    s_kind=s_operations[s_operation].kind;
    s_lifetime=s_operations[s_operation].signature<5 || s_operations[s_operation].signature==8;
    s_descriptor_offset=s_kind==0?0x112c:s_kind==1||s_kind==6?0x1118:s_kind==7?0x10ec:0x111c;
    PTR(object,0)=s_vtable;PTR(object,s_descriptor_offset)=s_fixture.descriptor[0];
    PTR(object,0x10f0)=s_scenario&1?s_fixture.notify[0]:0;
    PTR(object,0x1118)=PTR(object,0x111c)=s_fixture.descriptor[0];
    D32(object,0x1100)=s_scenario%3?1:0;
    PTR(object,0x12e8)=s_tabs_vtable;PTR(object,0x2248)=s_scenario&2?s_fixture.notify[0]:0;
    PTR(object,0x2258)=s_fixture.pages[0];PTR(object,0x225c)=s_fixture.pages[1];
    for(i=0;i<3;++i)PTR(s_fixture.notify[i],0)=s_notify_vtable;
    for(i=0;i<2;++i){
        void *d=s_fixture.descriptor[i];static const uint16_t counts[]={0,1,2,3,4,5,0xffff,0x8000};
        U16(d,0x24)=counts[s_scenario%8];
        snprintf(s_fixture.strings[i][0],32,"airspeed %u/%u",i,s_scenario);
        snprintf(s_fixture.strings[i][1],32,"vertical %x",s_seed);
        PTR(d,0x14)=s_fixture.strings[i][0];PTR(d,0x18)=s_fixture.strings[i][1];PTR(d,8)=graph_callback;
        PTR(s_fixture.pages[i],0x1124)=s_scenario&4?s_fixture.notify[1]:0;
        PTR(s_fixture.pages[i],0x1130)=s_scenario&8?s_fixture.notify[2]:0;
    }
    if(s_kind==6){PTR(object,0x111c)=s_fixture.descriptor[1];}
    if(s_kind==2){D32(object,0x1db0)=s_scenario&1;D32(object,0x1ce8)=s_scenario&2;D32(object,0x1c20)=s_scenario&4;D32(object,0x1b58)=s_scenario&8;D32(object,0x1e78)=s_scenario%3;}
    s_initial=s_fixture;
}
static LONG WINAPI fatal(EXCEPTION_POINTERS *e)
{fprintf(stderr,"FAULT scenario=%u op=%u (%s) kind=%u side=%d code=%lx address=%p\n",s_scenario,s_operation,s_operations[s_operation].name,s_kind,s_side,e->ExceptionRecord->ExceptionCode,e->ExceptionRecord->ExceptionAddress);fflush(stderr);ExitProcess(2);return EXCEPTION_EXECUTE_HANDLER;}
static void land(void){__asm__ volatile("movl %0, %%fs:0"::"r"(s_saved_seh):"memory");__builtin_longjmp(s_jump,1);}
static LONG CALLBACK expected_fault(EXCEPTION_POINTERS *e)
{
    if(s_operations[s_operation].signature==8 && s_scenario%11==0 && e->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION){
        s_throw_at=0;put("allocation.fault access=%u address=%u\n",(uint32_t)e->ExceptionRecord->ExceptionInformation[0],(uint32_t)e->ExceptionRecord->ExceptionInformation[1]);
        if(!s_side)++s_faults;
        e->ContextRecord->Eip=(DWORD)land;return EXCEPTION_CONTINUE_EXECUTION;}
    return EXCEPTION_CONTINUE_SEARCH;
}
static EXCEPTION_DISPOSITION __cdecl catch_throw(EXCEPTION_RECORD *record,void *frame,CONTEXT *context,void *dispatcher)
{
    (void)context;(void)dispatcher;
    if(record->ExceptionCode==THROW_CODE && !(record->ExceptionFlags&(EXCEPTION_UNWINDING|EXCEPTION_EXIT_UNWIND))){
        RtlUnwind(frame,0,record,0);s_throw_at=0;put("CAUGHT\n");if(!s_side)++s_throws;land();}
    return ExceptionContinueSearch;
}
static __attribute__((noinline)) int32_t run(uint32_t throw_at)
{
    struct {void *previous,*handler;} catcher;
    struct operation *op=&s_operations[s_operation];void *object=s_fixture.object,*entry=(uint8_t *)s_retail+op->rva;
    volatile int32_t result=0;uint32_t signature=op->signature;
    s_fixture=s_initial;s_calls=s_log_length=0;s_logs[s_side][0]=0;s_throw_at=throw_at;
    __asm__ volatile("movl %%fs:0, %0":"=r"(s_saved_seh));
    if(__builtin_setjmp(s_jump)==0){
        catcher.previous=(void *)s_saved_seh;catcher.handler=catch_throw;
        __asm__ volatile("movl %0, %%fs:0"::"r"(&catcher):"memory");
        if(signature==0 || signature==3 || signature==4){
            void *returned=s_side?fs98_sim_construct(object,(void *)s_seed,s_kind):((void *(TC *)(void *,void *))entry)(object,(void *)s_seed);
            put("return.object %u\n",offset(returned));
            if(signature==3){if(s_side)fs98_sim_destroy(object,s_kind);else((void (TC *)(void *))((uint8_t *)s_retail+s_classes[s_kind].destructor))(object);}
            if(signature==4){if(s_side)returned=fs98_sim_delete(object,s_seed,s_kind);else returned=((void *(TC *)(void *,uint32_t))((uint8_t *)s_retail+s_classes[s_kind].delete))(object,s_seed);put("return.object %u\n",offset(returned));}
        }else if(signature==1){if(s_side)fs98_sim_destroy(object,s_kind);else((void (TC *)(void *))entry)(object);}
        else if(signature==2){void *returned=s_side?fs98_sim_delete(object,s_seed,s_kind):((void *(TC *)(void *,uint32_t))entry)(object,s_seed);put("return.object %u\n",offset(returned));}
        else if(signature==5){if(s_side)fs98_sim_exchange(object,s_fixture.exchange,s_kind);else((void (TC *)(void *,void *))entry)(object,s_fixture.exchange);}
        else if(signature==6){result=s_side?fs98_sim_help(object,s_seed,s_fixture.help,s_kind):((int32_t (TC *)(void *,uint32_t,void *))entry)(object,s_seed,s_fixture.help);}
        else if(signature==7){result=s_side?fs98_sim_context(object,s_seed,s_seed^1,s_kind):((int32_t (TC *)(void *,uint32_t,uint32_t))entry)(object,s_seed,s_seed^1);}
        else if(signature==8){
            static void *ours[]={fs98_sim_create_crash,fs98_sim_create_settings_0,fs98_sim_create_settings_2,fs98_sim_create_settings_3,fs98_sim_create_settings_5};
            if(s_side)entry=ours[s_operation-64];
            result=((int32_t (SC *)(uint32_t,void *))entry)(s_seed,s_fixture.descriptor[0]);
        }else if(signature==9){result=s_side?fs98_sim_crash_tick(object,s_seed,s_seed^1):((int32_t (TC *)(void *,uint32_t,uint32_t))entry)(object,s_seed,s_seed^1);}
        else if(signature>=10 && signature<=13){
            static void (*const ours[])(void *)={fs98_sim_crash_close,fs98_sim_crash_accept,fs98_sim_crash_cancel,fs98_sim_crash_notify};
            if(s_side)ours[signature-10](object);else((void (TC *)(void *))entry)(object);
        }else if(signature>=14 && signature<=16){if(s_side)fs98_sim_damage_radio(object,signature-14);else((void (TC *)(void *))entry)(object);}
        else if(signature==17){result=s_side?(int32_t)fs98_sim_engine_count(object):((int32_t (TC *)(void *))entry)(object);}
        else if(signature==18){if(s_side)fs98_sim_engines_match(object,(int32_t)s_seed);else((void (TC *)(void *,int32_t))entry)(object,(int32_t)s_seed);}
        else if(signature>=19 && signature<=23){if(s_side)fs98_sim_engine_toggle(object,signature-19);else((void (TC *)(void *))entry)(object);}
        else if(signature==24){result=s_side?fs98_sim_engines_commit(object,s_seed):((int32_t (TC *)(void *,uint32_t))entry)(object,s_seed);}
        else if(signature==25){result=s_side?fs98_sim_reliability_commit(object,s_seed):((int32_t (TC *)(void *,uint32_t))entry)(object,s_seed);}
        else if(signature==26){if(s_side)fs98_sim_reliability_populate(object);else((void (TC *)(void *))entry)(object);}
        else if(signature==27){result=s_side?fs98_sim_system_selection(object,(int32_t)s_seed):((int32_t (TC *)(void *,int32_t))entry)(object,(int32_t)s_seed);}
        else if(signature>=28 && signature<=33){
            static void (*const ours[])(void *)={fs98_sim_settings_cancel,fs98_sim_settings_close,fs98_sim_post_destroy,fs98_sim_disable,fs98_sim_enable,fs98_sim_empty};
            if(signature==30 && s_scenario%2==0)object=0;
            if(s_side)ours[signature-28](object);else((void (TC *)(void *))entry)(object);
        }else if(signature==34){if(s_side)fs98_sim_empty_arg(object,s_seed);else((void (TC *)(void *,uint32_t))entry)(object,s_seed);}
        else if(signature==35){result=s_side?fs98_sim_zero(object,s_seed):((int32_t (TC *)(void *,uint32_t))entry)(object,s_seed);}
        else if(signature==36){result=s_side?fs98_sim_zero_message(object,s_seed,s_seed^1):((int32_t (TC *)(void *,uint32_t,uint32_t))entry)(object,s_seed,s_seed^1);}
    }
    __asm__ volatile("movl %0, %%fs:0"::"r"(s_saved_seh):"memory");
    return result;
}
static int compare(uint32_t throw_at)
{
    int32_t a,b;uint32_t i;
    s_side=0;a=run(throw_at);s_reference=s_fixture;s_side=1;b=run(throw_at);++s_comparisons;
    if(a==b && !memcmp(&s_reference,&s_fixture,sizeof(s_fixture)) && !strcmp(s_logs[0],s_logs[1]))return 0;
    fprintf(stderr,"DIFFER scenario=%u op=%u (%s) kind=%u throw=%u seed=%x result=%d/%d\n",s_scenario,s_operation,s_operations[s_operation].name,s_kind,throw_at,s_seed,a,b);
    for(i=0;i<sizeof(s_fixture);++i)if(((uint8_t *)&s_reference)[i]!=((uint8_t *)&s_fixture)[i]){fprintf(stderr,"byte=%x reference=%02x ours=%02x\n",i,((uint8_t *)&s_reference)[i],((uint8_t *)&s_fixture)[i]);break;}
    fprintf(stderr,"RETAIL:\n%sOURS:\n%s",s_logs[0],s_logs[1]);return 1;
}
int main(int argc,char **argv)
{
    uint32_t count=argc>1?strtoul(argv[1],0,0):20000,seed=argc>2?strtoul(argv[2],0,0):0x73a915e2;
    int unwind=argc>3 && !strcmp(argv[3],"unwind");uint32_t position,limit;
    if(!count || !seed)return 2;
    s_rng=seed;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);SetUnhandledExceptionFilter(fatal);AddVectoredExceptionHandler(1,expected_fault);
    s_retail=LoadLibraryExA("orig/English/Sim.DLL",0,DONT_RESOLVE_DLL_REFERENCES);
    if(!s_retail || GetProcAddress(s_retail,"Linkage")!=(FARPROC)((uint8_t *)s_retail+0xc070))return 2;
    setup();operations();
    for(s_scenario=0;s_scenario<count;++s_scenario){s_seed=next_random();s_script=s_scenario&15;
        for(s_operation=0;s_operation<s_operation_count;++s_operation){initialize();if(compare(0))return 1;limit=s_calls;
            if(unwind)for(position=1;position<=limit;++position)if(compare(position))return 1;
        }
    }
    printf("Sim: %u scenarios, %u operations, %u comparisons, 0 differ; seed=%x throws=%u allocation_faults=%u swaps=%u\n",count,s_operation_count,s_comparisons,seed,s_throws,s_faults,s_swaps);
    FreeLibrary(s_retail);return 0;
}
