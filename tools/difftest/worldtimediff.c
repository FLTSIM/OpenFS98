/* Guarded time/season events and initialization. Only imported services and
 * independent owner callbacks are patched; no MAIN code executes. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/world/world_time.h"
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define AT(p,n) ((uint8_t *)(p)+(n))
#define THROW_CODE 0xe0429813
struct time_fixture {
    uint8_t before[32],object[0x212c],pages[2][0x212c],owners[2][0x124],buffers[2][7][32],notify[0x29c],font[0x70],after[32];
};
static struct time_fixture s_fixture,s_initial,s_reference;
static HMODULE s_retail;
static uint32_t s_rng,s_scenario,s_operation,s_kind,s_calls,s_length,s_throw_at,s_comparisons,s_unwinds,s_saved_seh,s_checks;
static int s_side;
static char s_logs[2][16384];
static void *s_jump[5],*s_vtable[62],*s_other_vtable[62],*s_control_vtable[62];
static uint32_t time_random(void){s_rng^=s_rng<<13;s_rng^=s_rng>>17;s_rng^=s_rng<<5;return s_rng;}
static uint32_t time_offset(const void *pointer){return (uint32_t)((const uint8_t *)pointer-(const uint8_t *)&s_fixture);}
static void time_redirect(void)
{
    PTR(s_fixture.object,0x10dc)=s_fixture.owners[1];
    if(s_kind==1){PTR(s_fixture.object,0x1118)=s_fixture.owners[1]+0x10;PTR(s_fixture.object,0x111c)=s_fixture.owners[1];PTR(s_fixture.object,0x1120)=s_fixture.owners[1]+0x74;}
    if(s_kind==2){PTR(s_fixture.object,0x111c)=s_fixture.owners[1]+0x74;PTR(s_fixture.object,0x1120)=s_fixture.owners[1];}
}
static void time_log(const char *format,...)
{
    va_list args;int length;va_start(args,format);length=vsnprintf(s_logs[s_side]+s_length,16384-s_length,format,args);va_end(args);
    if(length<0||(uint32_t)length>=16384-s_length)exit(2);
    s_length+=(uint32_t)length;
    if(++s_calls==s_throw_at)RaiseException(THROW_CODE,0,0,NULL);
    if((s_scenario&1)&&s_calls==1+(s_scenario/8)%8)time_redirect();
    if(s_scenario&2)PTR(s_fixture.object,0)=s_other_vtable;
}
static int SC time_send(uint32_t window,uint32_t message,uint32_t first,uint32_t second)
{ time_log("send %x %x %x %x\n",window,message,first,second);return (int)(s_scenario%7)-1; }
static int TC time_enable(void *control,int enable)
{ time_log("enable %x %x\n",time_offset(control),(uint32_t)enable);U32(control,8)=(uint32_t)enable;return -1; }
static uint32_t SC time_window_long(uint32_t window,int index)
{ time_log("window.long %x %d\n",window,index);return s_scenario&4?window:window^0x10; }
static void TC time_set_edit(void *control,const char *text)
{ time_log("set.edit %x [%s]\n",time_offset(control),text); }
static void time_text(void *control,void *font)
{ time_log("init.text %x %x\n",time_offset(control),time_offset(font)); }
static void TC time_text_hook(void *control,void *font){time_text(control,font);}
static void TC time_title(void *control,const char *text)
{ time_log("set.text %x [%s]\n",time_offset(control),text); }
static void time_title_service(void *control,const char *text){time_title(control,text);}
static int TC time_get_edit(void *control,char *text,int capacity)
{
    time_log("get.edit %x buffer=%x cap=%d\n",time_offset(control),time_offset(text),capacity);
    if(capacity!=32)exit(2);
    snprintf(text,(size_t)capacity,"edit-%x",time_offset(control));return 7;
}
static int TC time_check(void *control,int value)
{ time_log("check %x %x\n",time_offset(control),(uint32_t)value);U32(control,0xbc)=(uint32_t)value;return -1; }
static int TC time_error(void *object,void *control,void *status)
{ time_log("error %x control=%x status=%x\n",time_offset(object),time_offset(control),time_offset(status));return s_scenario&8?(int)0x80000001:0; }
static int TC time_focus(void *object,uint32_t id)
{ time_log("validate.dispatch %x id=%x\n",time_offset(object),id);return s_scenario&32?0:0x7bad; }
static int SC time_validate_owner(void *owner)
{ time_log("owner.validate %x action=%x\n",time_offset(owner),U32(owner,0xc));return s_scenario&16?(int)0x80000001:0; }
#define OWNER_CALLBACK(name,slot) static int SC name(void *owner){time_log("owner.callback %x slot=%x\n",time_offset(owner),slot);U32(owner,0xa4)^=3;return -1;}
OWNER_CALLBACK(time_convert_season,0x110)
OWNER_CALLBACK(time_convert_date,0x114)
OWNER_CALLBACK(time_convert_day,0x118)
OWNER_CALLBACK(time_convert_local,0x11c)
OWNER_CALLBACK(time_convert_gmt,0x120)
static void TC time_virtual_close(void *object){time_log("virtual.close %x\n",time_offset(object));}
static void TC time_other_close(void *object){time_log("other.close %x\n",time_offset(object));}
static int TC time_virtual_validate(void *object,uint32_t id)
{ time_log("virtual.validate %x %x\n",time_offset(object),id);if(s_scenario&2)PTR(object,0)=s_other_vtable;return s_scenario&8?(int)0x80000001:0; }
static int TC time_virtual_error(void *object,uint32_t id)
{ time_log("virtual.error %x %x\n",time_offset(object),id);return s_scenario&16?-1:0; }
static int TC time_other_error(void *object,uint32_t id)
{ time_log("other.error %x %x\n",time_offset(object),id);return s_scenario&32?-1:0; }
static void *TC time_get_page(void *tab)
{ time_log("get.page %x\n",time_offset(tab));return s_fixture.pages[s_scenario&1]; }
static uint32_t SC time_get_focus(void){time_log("get.focus\n");return 0x87654321;}
static void *SC time_from_handle(uint32_t window)
{ time_log("from.handle %x\n",window);return s_scenario&4?s_fixture.font:NULL; }
static int TC time_is_kind(void *window,void *type)
{ (void)type;time_log("is.kind %x\n",time_offset(window));return s_scenario&8; }
static uint32_t TC time_control_id(void *window){time_log("control.id %x\n",time_offset(window));return 0x1af8;}
static void *TC time_virtual_delete(void *object,uint32_t flags)
{ time_log("virtual.delete %x %x\n",time_offset(object),flags);U32(object,4)=flags;return object; }
static void TC time_base_close(void *object){time_log("base.close %x\n",time_offset(object));}
static int time_notify(void *object,const char *first,const char *second,uint32_t value)
{ time_log("notify %x [%s] [%s] %x\n",time_offset(object),first,second,value);return -1; }
static void *time_allocate(uint32_t size)
{
    time_log("allocate %x\n",size);
    if(size==0x1c08)return s_fixture.pages[0];
    if(size==0x1df0)return s_fixture.pages[1];
    if(size==0x29c)return s_fixture.notify;
    if(size<=sizeof(s_fixture.notify))return s_fixture.notify;
    if(size<=0x1c08)return s_fixture.pages[0];
    if(size<=sizeof(s_fixture.pages[1]))return s_fixture.pages[1];
    exit(2);
}
static void SC time_dialog_delete(void *object){time_log("dialog.delete %x\n",time_offset(object));}
static void SC time_notify_delete(void *object){time_log("notify.delete %x\n",time_offset(object));}
static void *time_base_construct(void *object,uint32_t template_id,void *parent)
{ time_log("base.construct %x template=%x parent=%x\n",time_offset(object),template_id,time_offset(parent));U32(object,4)=0x1a2b3c4d;return object; }
#define WORLD_TIME_BASE_HOOK(slot) static void *TC time_hook_##slot(void *object,uint32_t template_id,void *parent){return time_base_construct(object,template_id,parent);}
#define WORLD_CTOR_HOOK(slot) static void TC time_hook_##slot(void *object){time_log("construct %x import=%x\n",time_offset(object),0x##slot);U32(object,0)=0x##slot;}
#define WORLD_DTOR_HOOK(slot) static void TC time_hook_##slot(void *object){time_log("destroy %x import=%x\n",time_offset(object),0x##slot);U32(object,0)=0x##slot;}
#include "../../src/world/world_time_hooks.inc"
#undef WORLD_TIME_BASE_HOOK
#undef WORLD_CTOR_HOOK
#undef WORLD_DTOR_HOOK
static void time_construct(void *object,uint32_t slot)
{
    switch(slot){
#define WORLD_TIME_BASE_HOOK(slot)
#define WORLD_CTOR_HOOK(slot) case 0x##slot:time_hook_##slot(object);break;
#define WORLD_DTOR_HOOK(slot)
#include "../../src/world/world_time_hooks.inc"
#undef WORLD_TIME_BASE_HOOK
#undef WORLD_CTOR_HOOK
#undef WORLD_DTOR_HOOK
        default:exit(2);
    }
}
static void time_destroy(void *object,uint32_t slot)
{
    switch(slot){
#define WORLD_TIME_BASE_HOOK(slot)
#define WORLD_CTOR_HOOK(slot)
#define WORLD_DTOR_HOOK(slot) case 0x##slot:time_hook_##slot(object);break;
#include "../../src/world/world_time_hooks.inc"
#undef WORLD_TIME_BASE_HOOK
#undef WORLD_CTOR_HOOK
#undef WORLD_DTOR_HOOK
        default:exit(2);
    }
}
static void *TC time_notify_construct(void *object,uint32_t first,uint32_t second)
{ time_log("notify.construct %x %x %x\n",time_offset(object),first,second);return object; }
static int TC time_page_init(void *object,int flags)
{ time_log("page.init %x %x\n",time_offset(object),(uint32_t)flags);return -1; }
static int time_base_init(void *object){time_log("base.init %x\n",time_offset(object));return -1;}
static int TC time_base_init_hook(void *object){return time_base_init(object);}
static uint32_t time_resources(void){return 0x12345678;}
static int TC time_skin(void *object,uint32_t bitmap,uint32_t module,uint32_t flags)
{time_log("skin %x %x %x %x\n",time_offset(object),bitmap,module,flags);return s_scenario%5!=0;}
static int time_art(void *control,uint32_t bitmap,void *dib,void *palette,uint32_t flags)
{time_log("art %x %x %x %x %x\n",time_offset(control),bitmap,time_offset(dib),time_offset(palette),flags);return -1;}
static int TC time_art_hook(void *control,uint32_t bitmap,void *dib,void *palette,uint32_t flags){return time_art(control,bitmap,dib,palette,flags);}
static int time_button(void *control,void *font,void *alternate,uint32_t bitmap,void *dib,void *palette,uint32_t flags,int color,uint32_t module,int x,int y,struct fs98_world_rect rect)
{
    time_log("button %x %x/%x %x %x/%x flags=%x color=%x mod=%x xy=%d/%d rect=%d/%d/%d/%d\n",time_offset(control),time_offset(font),time_offset(alternate),bitmap,time_offset(dib),time_offset(palette),flags,(uint32_t)color,module,x,y,rect.left,rect.top,rect.right,rect.bottom);return 0;
}
static int TC time_button_hook(void *control,void *font,void *alternate,uint32_t bitmap,void *dib,void *palette,uint32_t flags,int color,uint32_t module,int x,int y,struct fs98_world_rect rect)
{return time_button(control,font,alternate,bitmap,dib,palette,flags,color,module,x,y,rect);}
static void *SC time_font(int index){time_log("font %d\n",index);return s_fixture.font;}
static void *time_font_service(int index){return time_font(index);}
static void time_combo(void *control,void *font,void *label){time_log("combo %x %x %x\n",time_offset(control),time_offset(font),time_offset(label));}
static void TC time_combo_hook(void *control,void *font,void *label){time_combo(control,font,label);}
static void time_edit(void *control,void *font,uint32_t fg,uint32_t bg,uint32_t error,void *label,uint32_t flags)
{time_log("edit %x %x colors=%x/%x/%x label=%x flags=%x\n",time_offset(control),time_offset(font),fg,bg,error,time_offset(label),flags);}
static void TC time_edit_hook(void *control,void *font,uint32_t fg,uint32_t bg,uint32_t error,void *label,uint32_t flags){time_edit(control,font,fg,bg,error,label,flags);}
static int TC time_radio_init(void *control,void *font,uint32_t bitmap,uint32_t dib,uint32_t palette,uint32_t module)
{time_log("radio.init %x font=%x bitmap=%x dib=%x palette=%x mod=%x\n",time_offset(control),time_offset(font),bitmap,dib,palette,module);return -1;}
static void SC time_remap(void *rect,void *control){time_log("remap %x\n",time_offset(control));U32(rect,0)=2;U32(rect,4)=3;U32(rect,8)=22;U32(rect,12)=33;}
static void TC time_move(void *control,int x,int y,int width,int height,int repaint){time_log("move %x %d %d %d %d %d\n",time_offset(control),x,y,width,height,repaint);}
static int SC time_message(const char *text,uint32_t flags,uint32_t help){time_log("message [%s] %x %x\n",text,flags,help);return -1;}
static int time_load(uint32_t module,uint32_t id,char *text,int cap){time_log("load %x %x %d\n",module,id,cap);snprintf(text,(size_t)cap,"title-%x",id);return 10;}
static int SC time_load_hook(uint32_t module,uint32_t id,char *text,int cap){return time_load(module,id,text,cap);}
static int TC time_tab_init(void *tab,void *object,void *font,uint32_t bitmap,void *dib,void *palette,uint32_t color,uint32_t bg,int a,int b,int c,int d,uint32_t module,uint32_t flags)
{time_log("tab.init %x %x %x %x %x/%x color=%x/%x dims=%d/%d/%d/%d mod=%x flags=%x\n",time_offset(tab),time_offset(object),time_offset(font),bitmap,time_offset(dib),time_offset(palette),color,bg,a,b,c,d,module,flags);return 0;}
static void TC time_frame(void *object,uint32_t bitmap,int a,int b,int c,int d,uint32_t module){time_log("frame %x %x %d/%d/%d/%d %x\n",time_offset(object),bitmap,a,b,c,d,module);}
static void TC time_add_page(void *tab,void *page,char *title,int flags){time_log("add.page %x %x [%s] %x\n",time_offset(tab),time_offset(page),title,(uint32_t)flags);}
static void TC time_helper(void *control,void *tab){time_log("helper %x %x\n",time_offset(control),time_offset(tab));}
static int TC time_last(void *tab,void *page){time_log("last %x %x\n",time_offset(tab),time_offset(page));return -1;}
static int TC time_active(void *tab,int index,uint32_t window){time_log("active %x %d %x\n",time_offset(tab),index,window);return -1;}
static int TC time_show(void *object,int show){time_log("show %x %d\n",time_offset(object),show);return -1;}
struct time_hook {uint32_t slot;void *function;};
static const struct time_hook s_hooks[]={
    {0x258b0,time_send},{0x255c8,time_enable},{0x2589c,time_window_long},{0x255bc,time_set_edit},{0x257e0,time_title},{0x25598,time_get_edit},
    {0x2573c,time_check},{0x2577c,time_check},{0x25778,time_error},{0x257b8,time_get_page},{0x258bc,time_get_focus},{0x255cc,time_from_handle},
    {0x255d4,time_is_kind},{0x255d8,time_control_id},{0x257b4,time_base_close},{0x257a8,time_notify},{0x2584c,time_allocate},
    {0x25850,time_dialog_delete},{0x257f4,time_notify_delete},{0x257cc,time_notify_construct},{0x257d8,time_page_init},
    {0x257c0,time_base_init_hook},{0x257c4,time_art_hook},{0x257c8,time_button_hook},{0x257dc,time_text_hook},{0x25790,time_combo_hook},
    {0x257d0,time_font},{0x25740,time_edit_hook},{0x25744,time_remap},{0x2559c,time_move},{0x2551c,time_message},
    {0x25890,time_load_hook},{0x257d4,time_tab_init},{0x257e4,time_add_page},{0x257e8,time_helper},{0x257ec,time_last},{0x257f0,time_active},{0x25500,time_show},
#define WORLD_TIME_BASE_HOOK(slot) {0x##slot,time_hook_##slot},
#define WORLD_CTOR_HOOK(slot) {0x##slot,time_hook_##slot},
#define WORLD_DTOR_HOOK(slot) {0x##slot,time_hook_##slot},
#include "../../src/world/world_time_hooks.inc"
#undef WORLD_TIME_BASE_HOOK
#undef WORLD_CTOR_HOOK
#undef WORLD_DTOR_HOOK
};
static void *time_resolve(uint32_t slot)
{
    uint32_t index;if(slot==0x25758||slot==0x255d0)return s_control_vtable;
    for(index=0;index<sizeof(s_hooks)/sizeof(s_hooks[0]);++index)if(s_hooks[index].slot==slot)return s_hooks[index].function;
    fprintf(stderr,"unresolved %x\n",slot);exit(2);
}
static const struct fs98_world_init_services s_ui={.base_init=time_base_init,.load_string=time_load,.art=time_art,.button=time_button,.text=time_text,.combo=time_combo,.base_font=time_font_service,.edit=time_edit,.title=time_title_service};
static struct fs98_world_time_services s_services={&s_ui,time_resolve,time_resources,s_control_vtable};
static const struct fs98_world_object_services s_objects={.base_construct=time_base_construct,.construct=time_construct,.destroy=time_destroy};
static void time_patch(uint32_t rva,void *function){DWORD old;void **slot=(void **)((uint8_t *)s_retail+rva);if(!VirtualProtect(slot,4,PAGE_READWRITE,&old))exit(2);*slot=function;if(!VirtualProtect(slot,4,old,&old))exit(2);}
static const uint32_t s_entries[]={0x6170,0x6240,0x6060,0x6080,0x6100,0x4790,0x54d0,0x4870,0x55e0,0x4a20,0x5870,0x4af0,0x4bc0,0x5980,0x5a20,0x4ca0,0x5ac0,0x4cf0,0x5b10,0x4520,0x51e0,0x5d60,0x5ca0};
static void time_initialize(void)
{
    static const uint32_t limits[]={0,1,32,65536,0xffffffff,0x80000000,65535,2};
    static const uint32_t fields[]={0x20,0x34,0x48,0xb4,0xc8,0xdc,0xf0};
    static const uint32_t controls[]={0x1130,0x11e8,0x12a0,0x1358,0x1410,0x14c8,0x1580,0x1638,0x16f0,0x17a8,0x1860,0x1954,0x1a48,0x1ab0,0x1b18,0x1b80,0x1918,0x1a0c,0x1b00,0x1b68,0x1bd0,0x1c38,0x1ca0,0x1ce8};
    uint32_t byte,row,index;
    for(byte=0;byte<sizeof(s_fixture);++byte)((uint8_t *)&s_fixture)[byte]=(uint8_t)time_random();
    s_kind=s_operation==5||s_operation==7||s_operation==9||s_operation==11||s_operation==12||s_operation==15||s_operation==17||s_operation==19?1:
        s_operation==6||s_operation==8||s_operation==10||s_operation==13||s_operation==14||s_operation==16||s_operation==18||s_operation==20?2:0;
    PTR(s_fixture.object,0)=s_vtable;PTR(s_fixture.object,0x10dc)=s_fixture.owners[0];
    if(s_kind==1){PTR(s_fixture.object,0x1118)=s_fixture.owners[0]+0x10;PTR(s_fixture.object,0x111c)=s_fixture.owners[0];PTR(s_fixture.object,0x1120)=s_fixture.owners[0]+0x74;}
    if(s_kind==2){PTR(s_fixture.object,0x111c)=s_fixture.owners[0]+0x74;PTR(s_fixture.object,0x1120)=s_fixture.owners[0];U32(s_fixture.object,0x1118)=s_scenario&1;}
    for(row=0;row<2;++row){
        PTR(s_fixture.pages[row],0)=s_vtable;U32(s_fixture.pages[row],0x7d0)=0x1afd+row;
        memset(s_fixture.owners[row],0,sizeof(s_fixture.owners[row]));PTR(s_fixture.owners[row],4)=time_validate_owner;
        PTR(s_fixture.owners[row],0x110)=time_convert_season;PTR(s_fixture.owners[row],0x114)=time_convert_date;PTR(s_fixture.owners[row],0x118)=time_convert_day;
        PTR(s_fixture.owners[row],0x11c)=time_convert_local;PTR(s_fixture.owners[row],0x120)=time_convert_gmt;
        for(index=0;index<7;++index){snprintf((char *)s_fixture.buffers[row][index],32,"owner-%u-field-%u",row,index);PTR(s_fixture.owners[row],fields[index])=s_fixture.buffers[row][index];U32(s_fixture.owners[row],fields[index]+4)=s_operation==19||s_operation==20?limits[(s_scenario+index+row)%8]:32;U32(s_fixture.owners[row],fields[index]-4)=(s_scenario>>(index%5))&1;}
        PTR(s_fixture.owners[row],0x78)=s_fixture.buffers[row][0];strcpy((char *)s_fixture.owners[row]+0x7c,"12:34");strcpy((char *)s_fixture.owners[row]+0x85,"DST");
        U32(s_fixture.owners[row],0x18)=(s_scenario+row)%4;U32(s_fixture.owners[row],0x9c)=(s_scenario+row)%4;
        U32(s_fixture.owners[row],0x70)=s_scenario%7-1;U32(s_fixture.owners[row],0xa4)=s_scenario%5-1;U32(s_fixture.owners[row],0x104)=time_random();
    }
    PTR(s_fixture.object,0x2118)=s_fixture.pages[0];PTR(s_fixture.object,0x211c)=s_fixture.pages[1];PTR(s_fixture.object,0x2128)=s_fixture.notify;
    PTR(s_fixture.notify,0)=s_control_vtable;PTR(s_fixture.object,0x1450)=s_control_vtable;
    for(index=0;index<sizeof(controls)/sizeof(controls[0]);++index){PTR(s_fixture.object,controls[index])=s_control_vtable;U32(s_fixture.object,controls[index]+0x20)=0x10000+controls[index];}
    U32(s_fixture.object,0x1100)=s_scenario%3;
    if(s_operation==1||s_operation==21){if(s_scenario%3==0)PTR(s_fixture.object,0x10dc)=NULL;else if(s_scenario%3==1)U32(s_fixture.object,0x10dc)=0xcdcdcdcd;}
    if(s_operation==19||s_operation==20){uint32_t off=s_operation==19?0x1118:0x111c;if(s_scenario%3==0)PTR(s_fixture.object,off)=NULL;else if(s_scenario%3==1)U32(s_fixture.object,off)=0xcdcdcdcd;}
    s_control_vtable[0xc4/4]=s_operation==4?(void *)time_virtual_close:(void *)time_radio_init;
    s_initial=s_fixture;
}
static uint32_t time_id(void)
{static const uint32_t ids[]={0,0x1af8,0x1af9,0x1afa,0x1afb,0x1afc,0x1afd,0x1afe,0xffffffff};return ids[(s_scenario/3)%9];}
static void time_invoke(void)
{
    void *object=s_fixture.object,*entry=(uint8_t *)s_retail+s_entries[s_operation];uint32_t id=time_id();int result=0;
    if(!s_side){
        if(s_operation==1||s_operation==21)result=((int(TC *)(void *))entry)(object);
        else if(s_operation==7||s_operation==8||s_operation==9||s_operation==10||s_operation==22)result=((int(TC *)(void *,uint32_t))entry)(object,id);
        else if(s_operation==17||s_operation==18)result=((int(TC *)(void *,uint32_t,uint32_t))entry)(object,id,id^1);
        else ((void(TC *)(void *))entry)(object);
    }else switch(s_operation){
        case 0:fs98_world_time_bind_pages(object);break;case 1:result=fs98_world_time_has_descriptor(object);break;
        case 2:fs98_world_time_cancel(object);break;case 3:fs98_world_time_accept(object);break;case 4:fs98_world_time_close(object);break;
        case 5:case 6:fs98_world_time_populate(object,s_kind);break;
        case 7:case 8:result=fs98_world_time_validate(object,id,s_kind);break;
        case 9:case 10:result=fs98_world_time_error(object,id,s_kind);break;
        case 11:case 12:case 13:case 14:fs98_world_time_radio(object,s_operation==11||s_operation==13,s_kind);break;
        case 15:case 16:fs98_world_time_selection(object,s_kind);break;
        case 17:case 18:result=fs98_world_time_reload(object,s_kind);break;
        case 19:case 20:fs98_world_time_page_init(object,s_kind);break;
        case 21:result=fs98_world_time_init(object);break;
        case 22:result=fs98_world_time_frame_validate(object,id);break;
    }
    if(!s_side&&!s_throw_at){
        if((s_operation==2&&(U32(s_fixture.object,0x2124)!=2||U32(s_fixture.owners[0],0)!=0))||
            ((s_operation==7||s_operation==8)&&!U32(s_initial.object,0x1100)&&s_calls)||
            ((s_operation==17||s_operation==18)&&result)||
            (s_operation==0&&PTR(s_fixture.pages[0],0x111c)!=s_fixture.owners[0])){fprintf(stderr,"TIME witness failed op=%u\n",s_operation);exit(2);}
        ++s_checks;
    }
    time_log("return %x\n",(uint32_t)result);
}
static EXCEPTION_DISPOSITION __cdecl time_catch(EXCEPTION_RECORD *record,void *frame,CONTEXT *context,void *dispatcher)
{
    (void)context;(void)dispatcher;
    if(record->ExceptionCode==THROW_CODE&&!(record->ExceptionFlags&(EXCEPTION_UNWINDING|EXCEPTION_EXIT_UNWIND))){
        RtlUnwind(frame,NULL,record,NULL);s_throw_at=0;if(!s_side)++s_unwinds;time_log("caught\n");
        __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");__builtin_longjmp(s_jump,1);
    }
    return ExceptionContinueSearch;
}
static LONG CALLBACK time_fault(EXCEPTION_POINTERS *exception)
{
    if(exception->ExceptionRecord->ExceptionCode==THROW_CODE)return EXCEPTION_CONTINUE_SEARCH;
    fprintf(stderr,"TIME FAULT side=%d case=%u op=%u call=%u pc=%p code=%lx\n%s",s_side,s_scenario,s_operation,s_calls,
        exception->ExceptionRecord->ExceptionAddress,exception->ExceptionRecord->ExceptionCode,s_logs[s_side]);ExitProcess(2);
}
static __attribute__((noinline)) void time_run(uint32_t throw_at)
{
    struct{void *previous,*handler;}catcher;s_fixture=s_initial;s_calls=s_length=0;s_logs[s_side][0]=0;s_throw_at=throw_at;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_saved_seh));
    if(__builtin_setjmp(s_jump)==0){catcher.previous=(void *)s_saved_seh;catcher.handler=time_catch;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&catcher) : "memory");time_invoke();}
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
}
static int time_compare(uint32_t throw_at)
{
    uint32_t byte;s_side=0;time_run(throw_at);s_reference=s_fixture;s_side=1;time_run(throw_at);++s_comparisons;
    if(!memcmp(&s_reference,&s_fixture,sizeof(s_fixture))&&!strcmp(s_logs[0],s_logs[1])&&
        !memcmp(s_fixture.before,s_initial.before,32)&&!memcmp(s_fixture.after,s_initial.after,32))return 0;
    fprintf(stderr,"TIME DIFFER case=%u op=%u throw=%u\n",s_scenario,s_operation,throw_at);
    for(byte=0;byte<sizeof(s_fixture);++byte)if(((uint8_t *)&s_reference)[byte]!=((uint8_t *)&s_fixture)[byte]){fprintf(stderr,"byte=%x retail=%x ours=%x\n",byte,((uint8_t *)&s_reference)[byte],((uint8_t *)&s_fixture)[byte]);break;}
    fprintf(stderr,"RETAIL:\n%sOURS:\n%s",s_logs[0],s_logs[1]);return 1;
}
int main(int argc,char **argv)
{
    uint32_t count=argc>1?strtoul(argv[1],NULL,0):2000,seed=argc>2?strtoul(argv[2],NULL,0):0x4a971b63,index,limit,call;
    int unwind=argc>3&&(!strcmp(argv[3],"unwind")||!strcmp(argv[3],"interrupt"));if(!count||!seed)return 2;s_rng=seed;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);AddVectoredExceptionHandler(1,time_fault);
    s_retail=LoadLibraryExA("orig/English/World.DLL",NULL,DONT_RESOLVE_DLL_REFERENCES);if(!s_retail)return 2;
    for(index=0;index<sizeof(s_hooks)/sizeof(s_hooks[0]);++index)time_patch(s_hooks[index].slot,s_hooks[index].function);
    time_patch(0x25758,s_control_vtable);time_patch(0x255d0,s_control_vtable);time_patch(0x256bc,(void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"),"__CxxFrameHandler"));U32(s_retail,0x2495c)=time_resources();
    s_vtable[0xe0/4]=time_focus;s_vtable[0xec/4]=time_virtual_close;s_vtable[0xd8/4]=time_virtual_validate;s_vtable[0xdc/4]=time_virtual_error;s_vtable[0xe4/4]=time_skin;s_vtable[0xe8/4]=time_frame;
    memcpy(s_other_vtable,s_vtable,sizeof(s_vtable));s_other_vtable[0xec/4]=time_other_close;s_other_vtable[0xdc/4]=time_other_error;
    s_control_vtable[1]=time_virtual_delete;s_control_vtable[0xc4/4]=time_radio_init;
    s_services.notify_vtable=(uint8_t *)s_retail+0x1e1e8;
    fs98_world_time_set_services(&s_services);
    fs98_world_time_set_object_services(&s_objects,(uint8_t *)s_retail+0x1ec48,(uint8_t *)s_retail+0x1e7a0,(uint8_t *)s_retail+0x1eac8);
    for(s_scenario=0;s_scenario<count;++s_scenario)for(s_operation=0;s_operation<23;++s_operation){
        time_initialize();if(time_compare(0))return 1;limit=s_calls-1;
        if(unwind)for(call=1;call<=limit;++call)if(time_compare(call))return 1;
    }
    printf("World time core/init: %u scenarios, 23 operations, %u comparisons, 0 differ; seed=%x unwinds=%u witnesses=%u\n",count,s_comparisons,seed,s_unwinds,s_checks);
    FreeLibrary(s_retail);return 0;
}
