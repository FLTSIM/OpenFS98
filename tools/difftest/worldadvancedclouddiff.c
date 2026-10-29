/* Guarded Advanced Weather events and initialization. Only imported services and
 * independent owner callbacks are patched; no MAIN code executes. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/world/world_advanced_internal.h"
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define THROW_CODE 0xe0429813
struct time_fixture {
    uint8_t before[32],object[0x2658],pages[5][0x2658],owners[2][0x428],buffers[2][20][32],notify[0x29c],font[0x70],string[1024],after[32];
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
{PTR(s_fixture.object,0x1190)=s_fixture.owners[1]+0x1c;PTR(s_fixture.object,0x1194)=s_fixture.owners[1];}
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
{
    if(message==0x180||message==0x143)time_log("send %x %x [%s] %x\n",window,message,(char *)second,first);
    else if(message==0x148)time_log("send %x %x %x text\n",window,message,first);
    else time_log("send %x %x %x %x\n",window,message,first,second);
    if(message==0x148){snprintf((char *)second,39,"turbulence-%u",first);return 12;}
    if(message==0x147&&window==U32(s_fixture.object,0x20c0))return s_scenario%11;
    switch(message){case 0x18b:return (int)(s_scenario%5);case 0x188:return s_scenario%3;case 0x199:return (int)first%3;case 0x180:return s_calls%4;default:return 0;}
}
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
{ time_log("owner.validate %x action=%x\n",time_offset(owner),U32(owner,0xc));return s_scenario%7==3?0x100e:s_scenario&16?(int)0x80000001:0; }
#define OWNER_CALLBACK(name,slot) static int SC name(void *owner){time_log("owner.callback %x slot=%x\n",time_offset(owner),slot);U32(owner,0x424)^=3;if(slot==0x400&&!(s_scenario&2))U32(owner,0x1c+U32(owner,0x20)*0x58+0x5c)=0;return slot==0x3fc?(s_scenario%4?0:0x1008):0;}
OWNER_CALLBACK(time_convert_season,0x3f4)
OWNER_CALLBACK(time_convert_date,0x3f8)
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
    static const uint32_t sizes[]={0x21a0,0x199c,0x12e4,0x2210,0x15a4};
    uint32_t index;
    for(index=0;index<5;++index)if(size==sizes[index])return s_fixture.pages[index];
    if(size==0x29c)return s_fixture.notify;
    exit(2);
}
static void SC time_dialog_delete(void *object){time_log("dialog.delete %x\n",time_offset(object));}
static void SC time_notify_delete(void *object){time_log("notify.delete %x\n",time_offset(object));}
static void *time_base_construct(void *object,uint32_t template_id,void *parent)
{ time_log("base.construct %x template=%x parent=%x\n",time_offset(object),template_id,time_offset(parent));U32(object,4)=0x1a2b3c4d;return object; }
#define WORLD_ADVANCED_BASE_HOOK(slot) static void *TC time_hook_##slot(void *object,uint32_t template_id,void *parent){return time_base_construct(object,template_id,parent);}
#define WORLD_CTOR_HOOK(slot) static void TC time_hook_##slot(void *object){time_log("construct %x import=%x\n",time_offset(object),0x##slot);U32(object,0)=0x##slot;}
#define WORLD_DTOR_HOOK(slot) static void TC time_hook_##slot(void *object){time_log("destroy %x import=%x\n",time_offset(object),0x##slot);U32(object,0)=0x##slot;}
#include "../../src/world/world_advanced_hooks.inc"
#undef WORLD_ADVANCED_BASE_HOOK
#undef WORLD_CTOR_HOOK
#undef WORLD_DTOR_HOOK
static void time_construct(void *object,uint32_t slot)
{
    switch(slot){
#define WORLD_ADVANCED_BASE_HOOK(slot)
#define WORLD_CTOR_HOOK(slot) case 0x##slot:time_hook_##slot(object);break;
#define WORLD_DTOR_HOOK(slot)
#include "../../src/world/world_advanced_hooks.inc"
#undef WORLD_ADVANCED_BASE_HOOK
#undef WORLD_CTOR_HOOK
#undef WORLD_DTOR_HOOK
        default:exit(2);
    }
}
static void time_destroy(void *object,uint32_t slot)
{
    switch(slot){
#define WORLD_ADVANCED_BASE_HOOK(slot)
#define WORLD_CTOR_HOOK(slot)
#define WORLD_DTOR_HOOK(slot) case 0x##slot:time_hook_##slot(object);break;
#include "../../src/world/world_advanced_hooks.inc"
#undef WORLD_ADVANCED_BASE_HOOK
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
static uint32_t TC time_command(void *object,uint32_t a,uint32_t b){time_log("base.command %x %x %x\n",time_offset(object),a,b);return s_scenario|1;}
static void TC time_string_ctor(void *string){time_log("string.construct\n");PTR(string,0)=s_fixture.string;s_fixture.string[0]=0;}
static void TC time_string_dtor(void *string){(void)string;time_log("string.destroy\n");}
static void *TC time_string_assign(void *string,const char *text){time_log("string.assign [%s]\n",text);strcpy(PTR(string,0),text);return string;}
static void *TC time_string_append(void *string,const char *text){time_log("string.append [%s]\n",text);strcat(PTR(string,0),text);return string;}
static void *TC time_string_char(void *string,int ch){size_t len;time_log("string.char %x\n",(uint32_t)ch);len=strlen(PTR(string,0));((char *)PTR(string,0))[len]=(char)ch;((char *)PTR(string,0))[len+1]=0;return string;}
static int TC time_tabs(void *font,int count,const int *tabs){int index;time_log("tabs %x count=%d\n",time_offset(font),count);for(index=0;index<count;++index)time_log("tab %d\n",tabs[index]);return -1;}
static void TC time_menu_create(void *button,uint32_t module){time_log("menu.create %x %x\n",time_offset(button),module);}
static void TC time_menu_append(void *button,uint16_t id,uint32_t resource,uint16_t flags){time_log("menu.append %x %x %x %x\n",time_offset(button),id,resource,flags);}
static void TC time_menu_track(void *button){time_log("menu.track %x\n",time_offset(button));}
static void TC time_menu_delete(void *button){time_log("menu.delete %x\n",time_offset(button));}
static int TC time_tab_enable(void *tab,void *page,int enable){time_log("tab.enable %x %x %d\n",time_offset(tab),time_offset(page),enable);return -1;}
static void *TC time_child(void *object,uint32_t id){time_log("get.child %x %x\n",time_offset(object),id);return s_fixture.font;}
static int SC time_beep(uint32_t value){time_log("beep %x\n",value);return -1;}
static int SC time_cloud_changed(void *owner){time_log("cloud.changed %x\n",time_offset(owner));return 0;}
static uint32_t TC time_direction_get(void *control){time_log("direction.get %x\n",time_offset(control));return U32(control,0x80);}
static void TC time_direction_set(void *control,uint32_t value){time_log("direction.set %x %x\n",time_offset(control),value);U32(control,0x80)=value;}
static void TC time_direction_init(void *control,uint32_t a,uint32_t b,uint32_t c,uint32_t d,int e,int f,int g,int h,int i,int j,uint32_t module)
{time_log("direction.init %x %x %x %x %x %d/%d/%d/%d/%d/%d %x\n",time_offset(control),a,b,c,d,e,f,g,h,i,j,module);}
static int SC time_invalidate(uint32_t window,void *rect,int erase){(void)rect;time_log("invalidate %x %d\n",window,erase);return 1;}
static int TC time_enabled(void *control){time_log("enabled %x\n",time_offset(control));return (int)(s_scenario&1);}
static int SC time_async(int key){time_log("async %x\n",(uint32_t)key);return 0;}
struct time_hook {uint32_t slot;void *function;};
static const struct time_hook s_hooks[]={
    {0x2570c,time_tab_enable},{0x25574,time_child},{0x25898,time_beep},{0x25708,time_direction_get},{0x25710,time_direction_set},{0x2571c,time_direction_init},{0x25894,time_invalidate},
    {0x255b4,time_string_ctor},{0x255c0,time_string_dtor},{0x25578,time_string_assign},{0x25580,time_string_append},{0x2557c,time_string_char},
    {0x25714,time_tabs},{0x25718,time_combo_hook},{0x256f4,time_menu_create},{0x256f8,time_menu_append},{0x256fc,time_menu_track},{0x2585c,time_menu_delete},
    {0x258a0,time_send},{0x257ac,time_command},{0x255c4,time_enabled},{0x258c0,time_async},{0x258b0,time_send},{0x255c8,time_enable},{0x2589c,time_window_long},{0x255bc,time_set_edit},{0x257e0,time_title},{0x25598,time_get_edit},
    {0x2573c,time_check},{0x2577c,time_check},{0x25778,time_error},{0x257b8,time_get_page},{0x258bc,time_get_focus},{0x255cc,time_from_handle},
    {0x255d4,time_is_kind},{0x255d8,time_control_id},{0x257b4,time_base_close},{0x257a8,time_notify},{0x2584c,time_allocate},
    {0x25850,time_dialog_delete},{0x257f4,time_notify_delete},{0x257cc,time_notify_construct},{0x257d8,time_page_init},
    {0x257c0,time_base_init_hook},{0x257c4,time_art_hook},{0x257c8,time_button_hook},{0x257dc,time_text_hook},{0x25790,time_combo_hook},
    {0x257d0,time_font},{0x25740,time_edit_hook},{0x25744,time_remap},{0x2559c,time_move},{0x2551c,time_message},
    {0x25890,time_load_hook},{0x257d4,time_tab_init},{0x257e4,time_add_page},{0x257e8,time_helper},{0x257ec,time_last},{0x257f0,time_active},{0x25500,time_show},
#define WORLD_ADVANCED_BASE_HOOK(slot) {0x##slot,time_hook_##slot},
#define WORLD_CTOR_HOOK(slot) {0x##slot,time_hook_##slot},
#define WORLD_DTOR_HOOK(slot) {0x##slot,time_hook_##slot},
#include "../../src/world/world_advanced_hooks.inc"
#undef WORLD_ADVANCED_BASE_HOOK
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
static struct fs98_world_advanced_services s_services={&s_ui,time_resolve,time_resources,s_control_vtable};
static const struct fs98_world_object_services s_objects={.base_construct=time_base_construct,.construct=time_construct,.destroy=time_destroy};
static void time_patch(uint32_t rva,void *function){DWORD old;void **slot=(void **)((uint8_t *)s_retail+rva);if(!VirtualProtect(slot,4,PAGE_READWRITE,&old))exit(2);*slot=function;if(!VirtualProtect(slot,4,old,&old))exit(2);}
static const uint32_t s_entries[]={0x7720,0x7a60,0x7a80,0x7a90,0x7ae0,0x7c00,0x7c70,0x7df0,0x8140,0x8390,0x8410,0x84e0,0x89d0,0x8a00,0x8a70,0x8b00,0x8c10,0x8e50,0x8f70,0x9050};
static void time_initialize(void)
{
    uint32_t byte,row,index;void *object=s_fixture.object;
    for(byte=0;byte<sizeof(s_fixture);++byte)((uint8_t *)&s_fixture)[byte]=(uint8_t)time_random();
    s_kind=1;PTR(object,0)=s_vtable;U32(object,0x1100)=s_scenario&1;
    PTR(object,0x1190)=s_fixture.owners[0]+0x1c;PTR(object,0x1194)=s_fixture.owners[0];PTR(object,0x11a0)=s_fixture.pages[0];PTR(object,0x119c)=s_fixture.pages[1];PTR(object,0x11a4)=s_fixture.pages[2];
    for(row=0;row<2;++row){
        void *owner=s_fixture.owners[row],*data=(uint8_t *)owner+0x1c;
        memset(owner,0,0x428);PTR(owner,4)=time_validate_owner;PTR(owner,0x3fc)=time_convert_date;PTR(owner,0x400)=time_convert_season;PTR(data,0x13c)=time_cloud_changed;
        U32(owner,0x10)=s_scenario%3;U32(owner,0x14)=s_scenario&8;U32(data,4)=s_scenario%3;
        for(index=0;index<20;++index)snprintf((char *)s_fixture.buffers[row][index],32,"cloud-%u-%u",row,index);
        for(index=0;index<9;++index){uint32_t off=(index/3)*0x58+8+(index%3)*0x14;U32(data,off)=(s_scenario>>(index%6+1))&1;PTR(data,off+4)=s_fixture.buffers[row][index];U32(data,off+8)=32;}
        for(index=0;index<3;++index){U32(data,index*0x58+0x5c)=(s_scenario>>index)&1;U32(data,index*0x58+0x48)=index?5:10;*(uint16_t *)((uint8_t *)data+index*0x58+0x44)=s_scenario%3;}
        for(index=0;index<11;++index){*(uint16_t *)((uint8_t *)data+0x110+index*4)=0;*(uint16_t *)((uint8_t *)data+0x112+index*4)=8;}
    }
    for(index=0;index<5;++index){PTR(s_fixture.pages[index],0)=s_vtable;PTR(s_fixture.pages[0],0x2644)=s_fixture.pages[3];}
    PTR(object,0x1a58)=s_control_vtable;PTR(s_fixture.notify,0)=s_control_vtable;
    for(index=0;index<3;++index)snprintf((char *)object+0x1118+index*0x28,40,"layer-%u",index);
    s_initial=s_fixture;
}
uint32_t fs98_world_advanced_wind(void *object,uint32_t rva,uint32_t a,uint32_t b)
{(void)object;(void)rva;(void)a;(void)b;exit(2);}
static void time_invoke(void)
{
    void *object=s_fixture.object,*entry=AT(s_retail,s_entries[s_operation]);uint32_t result=0,first=s_scenario%3;
    if(s_side)result=fs98_world_advanced_call(object,s_entries[s_operation],first,s_scenario&1);
    else if(s_operation==19)result=((uint32_t(TC *)(void *,uint32_t,uint32_t))entry)(object,first,s_scenario&1);
    else if(s_operation==3)((void(TC *)(void *,uint32_t))entry)(object,first);
    else if(s_operation==15)((void(TC *)(void *,uint32_t))entry)(object,0xffffffff);
    else if(s_operation==17||s_operation==18)result=((uint32_t(TC *)(void *,uint32_t))entry)(object,first);
    else if(s_operation==2)result=((uint32_t(TC *)(void *))entry)(object);
    else ((void(TC *)(void *))entry)(object);
    if(s_operation!=2&&s_operation!=17&&s_operation!=18&&s_operation!=19)result=0;
    if(!s_side&&!s_throw_at)++s_checks;
    time_log("return %x\n",result);
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
    s_vtable[0xc4/4]=time_base_close;s_vtable[0xe0/4]=time_focus;s_vtable[0xec/4]=time_virtual_close;s_vtable[0xd8/4]=time_virtual_validate;s_vtable[0xdc/4]=time_virtual_error;s_vtable[0xe4/4]=time_skin;s_vtable[0xe8/4]=time_frame;
    memcpy(s_other_vtable,s_vtable,sizeof(s_vtable));s_other_vtable[0xec/4]=time_other_close;s_other_vtable[0xdc/4]=time_other_error;
    s_control_vtable[1]=time_virtual_delete;s_control_vtable[0xc4/4]=time_radio_init;
    s_services.notify_vtable=(uint8_t *)s_retail+0x1e1e8;
    fs98_world_advanced_set_services(&s_services);
    {
        void *tables[6];uint32_t kind;static const uint32_t vtables[]={0x1e0f8,0x1f0f8,0x20538,0x206e8,0x20c38,0x1f4e8};
        for(kind=0;kind<6;++kind)tables[kind]=AT(s_retail,vtables[kind]);
        fs98_world_advanced_set_object_services(&s_objects,tables);
    }
    for(s_scenario=0;s_scenario<count;++s_scenario)for(s_operation=0;s_operation<20;++s_operation){
        time_initialize();if(time_compare(0))return 1;limit=s_calls-1;
        if(unwind)for(call=1;call<=limit;++call)if(time_compare(call))return 1;
    }
    printf("Advanced Weather Clouds core/init: %u scenarios, 20 operations, %u comparisons, 0 differ; seed=%x unwinds=%u baselines=%u\n",count,s_comparisons,seed,s_unwinds,s_checks);
    FreeLibrary(s_retail);return 0;
}
