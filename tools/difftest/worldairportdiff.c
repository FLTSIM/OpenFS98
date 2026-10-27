/* Independent guarded airport fixtures. Patch only external UI/CRT/HOST
 * services; retail keeps its complete internal call graph and dispatch. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/world/world_airport.h"
#include "../../src/world/world_init.h"
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define THROW_CODE 0xe0429822
#define exit(code) do { fprintf(stderr,"fixture failure line=%d side=%u scenario=%u op=%u calls=%u\n",__LINE__,s_side,s_scenario,s_operation,s_calls); ExitProcess(code); } while(0)
struct fixture {
    uint8_t before[32], object[0x23e0], after[32], descriptors[2][0xc0];
    uint32_t rows[2][8][5][3];
    char names[2][8][5][48], selected[2][128], prefix[2][128], edits[2][128];
    char resources[9][128], work[512];
    uint32_t allocated, freed, callbacks[10], sends, enabled[29], shown[29], selection[29];
};
static struct fixture s_fixture,s_initial,s_reference;
static uint32_t s_side,s_scenario,s_operation,s_calls,s_length,s_throw_at,s_saved_seh,s_comparisons,s_throws,s_checks,s_rng=0x4a971b63;
static HMODULE s_retail;
static char s_logs[2][32768];
static void *s_jump[5], *s_vtable[2][60], *s_radio[50];
static uint8_t s_host_services[0x2ac];
static const uint32_t s_controls[]={0x10e0,0x1198,0x1280,0x1374,0x142c,0x1474,0x152c,0x1574,0x162c,0x1674,0x172c,0x1774,0x1868,0x1920,0x1988,0x1a40,0x1a88,0x1b40,0x1ba8,0x1c60,0x1ca8,0x1d9c,0x1de4,0x1e9c,0x1f54,0x1f9c,0x2098,0x2194,0x2290};
static const uint32_t s_lists[]={0x44,0x50,0x5c,0x68,0x74,0x94,0xa0,0xac};
static uint32_t rnd(void){s_rng^=s_rng<<13;s_rng^=s_rng>>17;s_rng^=s_rng<<5;return s_rng;}
static uint32_t offset(const void *p){return (uint32_t)((const uint8_t *)p-(uint8_t *)&s_fixture);}
static void log_call(const char *format,...)
{
    va_list args;int length;va_start(args,format);length=vsnprintf(s_logs[s_side]+s_length,sizeof(s_logs[0])-s_length,format,args);va_end(args);
    if(length<0||(uint32_t)length>=sizeof(s_logs[0])-s_length)exit(2);
    s_length+=(uint32_t)length;
    if(++s_calls==s_throw_at)RaiseException(THROW_CODE,0,0,NULL);
    if((s_scenario&8)&&s_calls==3&&s_operation!=35)PTR(s_fixture.object,0x10dc)=s_fixture.descriptors[1];
}
static uint32_t control_index(void *control)
{uint32_t i;for(i=0;i<29;++i)if(control==s_fixture.object+s_controls[i])return i;exit(2);}
static uint32_t window_index(uint32_t window){if(window<0x400||window>=0x400+29)exit(2);return window-0x400;}
static uint32_t SC send_message(uint32_t window,uint32_t message,uint32_t first,uint32_t second)
{
    uint32_t i=window_index(window);
    if(message==0x143||message==0x180||message==0x14a)log_call("send %x %x %x [%s]\n",window,message,first,(char *)second);
    else log_call("send %x %x %x %x\n",window,message,first,second);
    ++s_fixture.sends;
    if(message==0x147||message==0x188)return s_fixture.selection[i];
    if(message==0x14e||message==0x186)s_fixture.selection[i]=first;
    return 0xffffffff;
}
static int TC show_control(void *control,int value){uint32_t i=control_index(control);log_call("show %x %d\n",offset(control),value);s_fixture.shown[i]=(uint32_t)value;return -7;}
static int TC enable_control(void *control,int value){uint32_t i=control_index(control);log_call("enable %x %d\n",offset(control),value);s_fixture.enabled[i]=(uint32_t)value;return -3;}
static int TC is_enabled(void *control){uint32_t i=control_index(control);log_call("enabled %x\n",offset(control));return s_fixture.enabled[i];}
static void TC set_text(void *control,const char *text){log_call("text %x [%s]\n",offset(control),text);if(control==s_fixture.object+0x1b40)snprintf(s_fixture.edits[1],128,"%s",text);}
static int TC get_text(void *control,char *text,int capacity){uint32_t i=control==s_fixture.object+0x1920?0:1;log_call("get.text %x capacity=%d [%s]\n",offset(control),capacity,s_fixture.edits[i]);snprintf(text,(size_t)capacity,"%s",s_fixture.edits[i]);return (int)strlen(text);}
static int compare_text(const char *a,const char *b){log_call("compare [%s] [%s]\n",a,b);return _stricmp(a,b);}
static int format_text(char *out,const char *format,uint32_t count){log_call("format %x [%s] %x\n",offset(out),format,count);return sprintf(out,format,count);}
static uint32_t SC resource_text(uint32_t id,void **out){uint32_t i=id-0x8001;log_call("resource %x %x\n",id,offset(out));if(i>=9)exit(2);*out=s_fixture.resources[i];++s_fixture.allocated;return (uint32_t)strlen(*out)+1;}
static uint32_t SC allocate(uint32_t type,uint32_t size,void **out,uint32_t flags){log_call("allocate %x %x %x %x\n",type,size,offset(out),flags);*out=s_fixture.work;++s_fixture.allocated;return 1;}
static uint32_t SC free_pointer(void **out){log_call("free %x\n",offset(out));*out=NULL;++s_fixture.freed;return -1;}
static char *SC copy_text(char *out,const char *text){log_call("copy %x [%s]\n",offset(out),text);strcpy(out,text);return out;}
static uint32_t owner(void *descriptor,uint32_t slot)
{
    uint32_t i;log_call("callback %x desc=%x mode=%x text=[%s] prefix=[%s]",slot,offset(descriptor),U32(descriptor,0x28),(char *)PTR(descriptor,0x34),(char *)PTR(descriptor,0x84));
    for(i=0;i<8;++i){int length=snprintf(s_logs[s_side]+s_length,sizeof(s_logs[0])-s_length," %x:%x",s_lists[i],U32(descriptor,s_lists[i]+8));if(length<0||(uint32_t)length>=sizeof(s_logs[0])-s_length)exit(2);s_length+=(uint32_t)length;}
    if(s_length+2>=sizeof(s_logs[0]))exit(2);
    strcpy(s_logs[s_side]+s_length,"\n");++s_length;
    ++s_fixture.callbacks[slot/4];return 0xffffffff;
}
#define OWNER(n) static uint32_t SC owner_##n(void *descriptor){return owner(descriptor,0x##n);}
OWNER(4) OWNER(8) OWNER(c) OWNER(10) OWNER(14) OWNER(18) OWNER(1c) OWNER(20) OWNER(24)
static int TC validate(void *object,uint32_t unused){log_call("validate %x %x\n",offset(object),unused);if(s_scenario&4)PTR(object,0)=s_vtable[1];return 0x1234;}
static int TC error(void *object,uint32_t value){log_call("error %x %x\n",offset(object),value);return s_scenario&2;}
static void TC close_first(void *object){log_call("close.first %x\n",offset(object));}
static void TC close_second(void *object){log_call("close.second %x\n",offset(object));}
static int TC base_init(void *object){log_call("base.init %x\n",offset(object));return s_scenario%7!=0;}
static int TC skin(void *object,uint32_t id,uint32_t module,uint32_t flags){log_call("skin %x %x %x %x\n",offset(object),id,module,flags);return s_scenario%7!=1;}
static int TC art_init(void *control,uint32_t id,void *dib,void *palette,uint32_t flags){log_call("art %x %x %x %x %x\n",offset(control),id,offset(dib),offset(palette),flags);return 1;}
static int TC radio_init(void *control,void *font,uint32_t id,void *dib,void *palette,uint32_t module){log_call("radio %x %x %x %x %x %x\n",offset(control),offset(font),id,offset(dib),offset(palette),module);return 1;}
static int TC check_control(void *control,int check){log_call("check %x %d\n",offset(control),check);return 7;}
static void TC text_init(void *control,void *font){log_call("text.init %x %x\n",offset(control),offset(font));}
static void TC combo_init(void *control,void *font,void *label){log_call("combo.init %x %x %x\n",offset(control),offset(font),offset(label));}
static void TC list_init(void *control,void *font,void *label){log_call("list.init %x %x %x\n",offset(control),offset(font),offset(label));}
static void TC edit_init(void *control,void *font,uint32_t back,uint32_t ink,uint32_t selected,void *label,uint32_t flags){log_call("edit.init %x %x %x %x %x %x %x\n",offset(control),(uint32_t)font,back,ink,selected,offset(label),flags);}
static void *SC base_font(uint32_t id){log_call("font %x\n",id);return (void *)0xf007;}
static int TC button_init(void *control,void *font,void *other,uint32_t id,void *dib,void *palette,uint32_t flags,int color,uint32_t module,int x,int y,struct fs98_world_rect rect){log_call("button %x %x %x %x %x %x %x %x %x %d %d %d %d %d %d\n",offset(control),offset(font),offset(other),id,offset(dib),offset(palette),flags,(uint32_t)color,module,x,y,rect.left,rect.top,rect.right,rect.bottom);return 1;}
static void *TC focus_control(void *control){log_call("focus %x\n",offset(control));return control;}
static int SC message(const char *text,uint32_t flags,uint32_t help){log_call("message [%s] %x %x\n",text,flags,help);return 0;}
static void *host(void){return s_host_services;}static uint32_t resource_module(void){return 0x87654321;}
struct binding{uint32_t slot;void *function;};
static const struct binding s_bindings[]={
 {0x258b0,send_message},{0x25500,show_control},{0x255c8,enable_control},{0x255c4,is_enabled},{0x255bc,set_text},{0x25598,get_text},{0x256a0,compare_text},{0x256b8,format_text},
 {0x257c0,base_init},{0x257c4,art_init},{0x2573c,check_control},{0x257dc,text_init},{0x25790,combo_init},{0x25718,list_init},{0x25740,edit_init},{0x257d0,base_font},{0x257c8,button_init},{0x25590,focus_control},{0x2551c,message}
};
static void *resolve(uint32_t slot){uint32_t i;for(i=0;i<sizeof(s_bindings)/sizeof(s_bindings[0]);++i)if(s_bindings[i].slot==slot)return s_bindings[i].function;fprintf(stderr,"missing slot %x\n",slot);exit(2);}
static const struct fs98_world_airport_services s_services={resolve,host,resource_module};
static void patch(uint32_t rva,void *value){DWORD old;void **slot=(void **)((uint8_t *)s_retail+rva);if(!VirtualProtect(slot,4,PAGE_READWRITE,&old))exit(2);*slot=value;if(!VirtualProtect(slot,4,old,&old))exit(2);}
static const uint32_t s_entries[]={0x167c0,0x16950,0x15690,0x16720,0x15d10,0x155e0,0x15ea0,0x14a80,0x15de0,0x15e70,0x15ec0,0x16550,0x163a0,0x16680,0x16700,0x15190,0x16a10,0x16bb0,0x16d50,0x15a80,0x15aa0,0x15ac0,0x15ae0,0x15b50,0x15bc0,0x15c30,0x15ca0,0x16070,0x16090,0x160b0,0x160d0,0x16140,0x161d0,0x16250,0x162d0,0x15190,0x15fb0,0x163a0,0x15ec0};
static void initialize(void)
{
    uint32_t i,d,l,r;memset(&s_fixture,0,sizeof(s_fixture));
    for(i=0;i<sizeof(s_fixture.object);++i)s_fixture.object[i]=(uint8_t)rnd();
    memset(s_fixture.before,0xa5,32);memset(s_fixture.after,0x5a,32);
    PTR(s_fixture.object,0x10dc)=s_fixture.descriptors[0];PTR(s_fixture.object,0)=s_vtable[0];
    for(i=0;i<29;++i){U32(s_fixture.object,s_controls[i]+0x20)=0x400+i;s_fixture.selection[i]=s_scenario%2;s_fixture.enabled[i]=s_scenario&1;}
    PTR(s_fixture.object,0x1280)=PTR(s_fixture.object,0x1774)=PTR(s_fixture.object,0x1ca8)=s_radio;
    for(d=0;d<2;++d){void *descriptor=s_fixture.descriptors[d];
        PTR(descriptor,4)=owner_4;PTR(descriptor,8)=owner_8;PTR(descriptor,0xc)=owner_c;PTR(descriptor,0x10)=owner_10;PTR(descriptor,0x14)=owner_14;PTR(descriptor,0x18)=owner_18;PTR(descriptor,0x1c)=owner_1c;PTR(descriptor,0x20)=owner_20;PTR(descriptor,0x24)=owner_24;
        U32(descriptor,0x28)=0x1000+s_scenario%4;PTR(descriptor,0x34)=s_fixture.selected[d];PTR(descriptor,0x84)=s_fixture.prefix[d];
        snprintf(s_fixture.selected[d],128,"original-%u",d);snprintf(s_fixture.prefix[d],128,"prefix-%u",d);
        for(l=0;l<8;++l){U32(descriptor,s_lists[l])=l<4?s_scenario%6:5;PTR(descriptor,s_lists[l]+4)=s_fixture.rows[d][l];U32(descriptor,s_lists[l]+8)=(s_scenario+l)%5;
            if(s_scenario%32==31&&(s_operation==2||s_operation==3)&&l!=5)U32(descriptor,s_lists[l])=0x80000000;
            for(r=0;r<5;++r){snprintf(s_fixture.names[d][l][r],48,"%c-%u-%u",'B'+r*3,d,l);s_fixture.rows[d][l][r][0]=(uint32_t)s_fixture.names[d][l][r];s_fixture.rows[d][l][r][1]=(uint32_t)s_fixture.names[d][l][(r+1)%5];s_fixture.rows[d][l][r][2]=(uint32_t)s_fixture.names[d][l][(r+2)%5];}
        }
    }
    strcpy(s_fixture.resources[0],"all");
    for(i=1;i<9;++i)snprintf(s_fixture.resources[i],128,i&1?"one-%u":"%%d-places-%u",i);
    for(i=0;i<9;++i)PTR(s_fixture.object,0x23ac+i*4)=s_fixture.resources[i];
    PTR(s_fixture.object,0x23d0)=s_fixture.work;strcpy(s_fixture.work,"work-before");
    U32(s_fixture.object,0x23d4)=32;U32(s_fixture.object,0x23d8)=s_scenario%3==0;U32(s_fixture.object,0x23dc)=s_scenario%4;
    for(i=0;i<8;++i)U32(s_fixture.object,0x238c+i*4)=s_scenario%2;
    {static const char *texts[]={"","A","B-0-5","C","H-0-5","Z","b-0-5","K-0-4"};strcpy(s_fixture.edits[0],texts[s_scenario%8]);strcpy(s_fixture.edits[1],texts[(s_scenario/8)%8]);}
    if(s_operation==35)PTR(s_fixture.object,0x10dc)=s_scenario&1?NULL:(void *)0xcdcdcdcd;
    if(s_operation==37)for(d=0;d<2;++d)U32(s_fixture.descriptors[d],0x94)=0;
    if(s_operation==38)for(d=0;d<2;++d)PTR(s_fixture.descriptors[d],0x78)=NULL;
    s_initial=s_fixture;
}
static void invoke(void)
{
    void *object=s_fixture.object,*entry=(uint8_t *)s_retail+s_entries[s_operation];uint32_t result=0;
    if(!s_side){
        if(s_operation==15||s_operation==35)result=((uint32_t (TC *)(void *))entry)(object);
        else if(s_operation==36){int32_t low=0,high=s_scenario&1?0:4;result=((uint32_t (TC *)(void *,int32_t *,int32_t *,const char *,void *))entry)(object,&low,&high,s_fixture.edits[0],s_fixture.rows[0][5]);}
        else ((void (TC *)(void *))entry)(object);
    } else {
        switch(s_operation){
        case 0:fs98_world_airport_allocate(object);break;case 1:fs98_world_airport_release(object);break;case 2:fs98_world_airport_populate(object);break;case 3:fs98_world_airport_runways(object);break;case 4:fs98_world_airport_clear(object);break;case 5:fs98_world_airport_restore(object);break;case 6:fs98_world_airport_refresh(object);break;case 7:fs98_world_airport_facilities(object);break;case 8:fs98_world_airport_airport(object);break;case 9:fs98_world_airport_double_click(object);break;case 10:case 38:fs98_world_airport_airport_edit(object);break;case 11:fs98_world_airport_name(object);break;case 12:case 37:fs98_world_airport_name_edit(object);break;case 13:fs98_world_airport_accept(object);break;case 14:fs98_world_airport_cancel(object);break;case 15:case 35:result=fs98_world_airport_init(object);break;
        case 16:case 17:case 18:fs98_world_airport_mode(object,s_operation-16);break;
        case 19:case 20:case 21:fs98_world_airport_mode_button(object,s_operation-19);break;
        case 22:case 23:case 24:case 25:case 26:fs98_world_airport_place(object,s_operation-22);break;
        case 27:case 28:case 29:case 30:fs98_world_airport_dropdown(object,s_operation-27,0);break;
        case 31:case 32:case 33:case 34:fs98_world_airport_dropdown(object,s_operation-31,1);break;
        case 36:{int32_t low=0,high=s_scenario&1?0:4;result=(uint32_t)fs98_world_airport_search(object,&low,&high,s_fixture.edits[0],s_fixture.rows[0][5]);break;}
        default:exit(2);
        }
    }
    if(!s_side&&!s_throw_at){
        if(s_operation==0&&(s_fixture.allocated!=10||U32(object,0x23d4)!=strlen(s_fixture.resources[8])+1))exit(2);
        if(s_operation==1&&(s_fixture.freed!=10||U32(object,0x23d0)))exit(2);
        if(s_operation==8&&U32(object,0x23d8))exit(2);
        if(s_operation>=16&&s_operation<=18&&U32(PTR(object,0x10dc),0x28)!=0x1000+s_operation-16)exit(2);
        if(s_operation==14&&(U32(PTR(object,0x10dc),0)||s_fixture.freed!=10))exit(2);
        if(s_operation==37&&s_calls)exit(2);
        ++s_checks;
    }
    log_call("return %x\n",result);
}
static void TC accept_virtual(void *object){log_call("virtual.accept %x\n",offset(object));}
static void land(void){__asm__ volatile("movl %0, %%fs:0"::"r"(s_saved_seh):"memory");__builtin_longjmp(s_jump,1);}
static EXCEPTION_DISPOSITION __cdecl catcher(EXCEPTION_RECORD *record,void *frame,CONTEXT *context,void *dispatcher)
{(void)context;(void)dispatcher;if(record->ExceptionCode==THROW_CODE&&!(record->ExceptionFlags&(EXCEPTION_UNWINDING|EXCEPTION_EXIT_UNWIND))){RtlUnwind(frame,NULL,record,NULL);s_throw_at=0;log_call("caught\n");if(!s_side)++s_throws;land();}return ExceptionContinueSearch;}
static LONG WINAPI fatal(EXCEPTION_POINTERS *e){fprintf(stderr,"FAULT %lx at %p side=%u scenario=%u op=%u throw=%u\n",e->ExceptionRecord->ExceptionCode,e->ExceptionRecord->ExceptionAddress,s_side,s_scenario,s_operation,s_throw_at);ExitProcess(2);return 0;}
static void run(uint32_t throw_at)
{
    struct{void *previous,*handler;}frame;
    s_fixture=s_initial;s_calls=s_length=0;s_logs[s_side][0]=0;s_throw_at=throw_at;
    __asm__ volatile("movl %%fs:0, %0":"=r"(s_saved_seh));
    if(__builtin_setjmp(s_jump)==0){frame.previous=(void *)s_saved_seh;frame.handler=catcher;__asm__ volatile("movl %0, %%fs:0"::"r"(&frame):"memory");invoke();}
    __asm__ volatile("movl %0, %%fs:0"::"r"(s_saved_seh):"memory");
    if(memcmp(s_fixture.before,s_initial.before,32)||memcmp(s_fixture.after,s_initial.after,32))exit(2);
}
static int compare(uint32_t at)
{
    s_side=0;run(at);s_reference=s_fixture;s_side=1;run(at);++s_comparisons;
    if(!memcmp(&s_reference,&s_fixture,sizeof(s_fixture))&&!strcmp(s_logs[0],s_logs[1]))return 0;
    fprintf(stderr,"AIRPORT DIFFER scenario=%u operation=%u throw=%u\n",s_scenario,s_operation,at);
    {uint32_t i;for(i=0;i<sizeof(s_fixture);++i)if(((uint8_t *)&s_reference)[i]!=((uint8_t *)&s_fixture)[i]){fprintf(stderr,"byte=%x retail=%x ours=%x\n",i,((uint8_t *)&s_reference)[i],((uint8_t *)&s_fixture)[i]);break;}}
    fprintf(stderr,"RETAIL:\n%sOURS:\n%s",s_logs[0],s_logs[1]);return 1;
}
int main(int argc,char **argv)
{
    uint32_t count=argc>1?strtoul(argv[1],NULL,0):200,index,position,limit;int unwind=argc>2&&!strcmp(argv[2],"unwind");
    SetErrorMode(SEM_NOGPFAULTERRORBOX);SetUnhandledExceptionFilter(fatal);if(!count)return 2;
    s_retail=LoadLibraryExA("orig/English/World.DLL",NULL,DONT_RESOLVE_DLL_REFERENCES);if(!s_retail||GetProcAddress(s_retail,"Linkage")!=(FARPROC)((uint8_t *)s_retail+0x24120))return 2;
    for(index=0;index<sizeof(s_bindings)/sizeof(s_bindings[0]);++index)patch(s_bindings[index].slot,s_bindings[index].function);
    PTR(s_host_services,0x214)=free_pointer;PTR(s_host_services,0x20c)=allocate;PTR(s_host_services,0x2a4)=resource_text;PTR(s_host_services,0x2a8)=copy_text;PTR(s_retail,0x2410c)=s_host_services;
    U32(s_retail,0x2495c)=resource_module();fs98_world_airport_set_services(&s_services);
    for(index=0;index<2;++index){s_vtable[index][0xd8/4]=validate;s_vtable[index][0xdc/4]=error;s_vtable[index][0xec/4]=index?close_second:close_first;s_vtable[index][0xe4/4]=skin;s_vtable[index][0xcc/4]=accept_virtual;}
    s_radio[0xc4/4]=radio_init;
    for(s_scenario=0;s_scenario<count;++s_scenario)for(s_operation=0;s_operation<sizeof(s_entries)/sizeof(s_entries[0]);++s_operation){initialize();if(compare(0))return 1;limit=s_calls-1;if(unwind)for(position=1;position<=limit;++position)if(compare(position))return 1;}
    printf("World airport core: %u scenarios, %u operations, %u comparisons, %u unwinds, %u witnesses, 0 differ; seed=4a971b63\n",count,(unsigned)(sizeof(s_entries)/sizeof(s_entries[0])),s_comparisons,s_throws,s_checks);return 0;
}
