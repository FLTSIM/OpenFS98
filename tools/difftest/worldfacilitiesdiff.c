/* Guarded facilities browser differential fixture. External services only are
 * patched; retail keeps all internal calls and its real MSVC unwind frames. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/world/world_facilities.h"
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define exit(code) do { fprintf(stderr,"fixture failure line=%d side=%u scenario=%u op=%u calls=%u\n",__LINE__,s_side,s_scenario,s_operation,s_calls); ExitProcess(code); } while(0)
#define THROW_CODE 0xe0429821
struct fixture {
    uint8_t before[32],object[0x1db8],after[32],descriptors[2][0x34],output[0x40];
    uint8_t rows[2][3*0x64],ends[3][4][0x2c],bitmap[128];
    uint32_t nodes[50][5],created,destroyed,live[200],free_count;
    char strings[200][1024];
};
static struct fixture s_fixture,s_initial,s_reference;
static HMODULE s_retail;
static uint32_t s_side,s_scenario,s_operation,s_calls,s_length,s_throw_at,s_saved_seh,s_checks,s_comparisons,s_throws,s_rng=0x4a971b63;
static char s_logs[2][65536];static void *s_jump[5],*s_output_vtable[48],*s_vtable[60],*s_radio_vtable[50],*s_spin_vtable[48];static uint8_t s_host_services[0x218];
static uint32_t rnd(void){s_rng^=s_rng<<13;s_rng^=s_rng>>17;s_rng^=s_rng<<5;return s_rng;}
static uint32_t offset(const void *p){return (uint32_t)((const uint8_t *)p-(uint8_t *)&s_fixture);}
static void log_call(const char *format,...)
{
    va_list args;int length;va_start(args,format);length=vsnprintf(s_logs[s_side]+s_length,sizeof(s_logs[0])-s_length,format,args);va_end(args);
    if(length<0||(uint32_t)length>=sizeof(s_logs[0])-s_length)exit(2);
    s_length+=(uint32_t)length;
    if(++s_calls==s_throw_at)RaiseException(THROW_CODE,0,0,NULL);
    if(s_operation==0&&(s_scenario&1)&&s_calls==3)PTR(s_fixture.object,0x10dc)=s_fixture.descriptors[1];
}
static uint32_t string_index(void *string)
{uint32_t index=(uint32_t)((char *)PTR(string,0)-s_fixture.strings[0])/1024;if(index>=200)exit(2);return index;}
static void TC string_ctor(void *string)
{log_call("string.new %u\n",s_fixture.created);if(s_fixture.created>=200)exit(2);PTR(string,0)=s_fixture.strings[s_fixture.created];s_fixture.live[s_fixture.created++]=1;}
static void TC string_dtor(void *string)
{uint32_t index=string_index(string);log_call("string.delete %u\n",index);if(!s_fixture.live[index])exit(2);s_fixture.live[index]=0;++s_fixture.destroyed;}
static void *TC string_assign(void *string,const char *text)
{log_call("assign %u [%s]\n",string_index(string),text);snprintf(PTR(string,0),1024,"%s",text);return string;}
static void *TC string_append(void *string,const char *text)
{uint32_t length=(uint32_t)strlen(PTR(string,0));log_call("append %u [%s]\n",string_index(string),text);snprintf((char *)PTR(string,0)+length,1024-length,"%s",text);return string;}
static void *TC string_copy(void *string,void *other)
{uint32_t index=string_index(string);log_call("copy %u %u [%s]\n",index,string_index(other),(char *)PTR(other,0));if(string!=other)snprintf(PTR(string,0),1024,"%s",(char *)PTR(other,0));return string;}
static void *TC string_add(void *string,void *other)
{char buffer[1024];uint32_t length=(uint32_t)strlen(PTR(string,0));snprintf(buffer,sizeof(buffer),"%s",(char *)PTR(other,0));log_call("add %u %u [%s]\n",string_index(string),string_index(other),buffer);snprintf((char *)PTR(string,0)+length,1024-length,"%s",buffer);return string;}
static void *SC string_concat(void *result,void *string,const char *text)
{log_call("concat %u [%s]\n",string_index(string),text);if(s_fixture.created>=200)exit(2);PTR(result,0)=s_fixture.strings[s_fixture.created];s_fixture.live[s_fixture.created++]=1;snprintf(PTR(result,0),1024,"%s%s",(char *)PTR(string,0),text);return result;}
static int TC string_load(void *string,uint32_t id)
{log_call("load %u %x\n",string_index(string),id);snprintf(PTR(string,0),1024,"resource-%x",id);return 1;}
static int compare_text(const char *a,const char *b){log_call("compare [%s] [%s]\n",a,b);return strcmp(a,b);}
static char *integer_text(int value,char *text,int radix){log_call("itoa %d %d\n",value,radix);sprintf(text,"%d",value);return text;}
static int SC free_pointer(void **pointer){log_call("free %x\n",offset(pointer));*pointer=NULL;++s_fixture.free_count;return -1;}
static uint32_t SC owner(void *descriptor)
{
    uint32_t index,field;log_call("owner flags=%u count=%u\n",U32(descriptor,0x20),U32(descriptor,0x10));
    for(index=0;index<U32(descriptor,0x10);++index)for(field=0;field<3;++field){const char *text=PTR(PTR(descriptor,0x14+field*4),index*4);log_call("place %u %u [%s]\n",index,field,text?text:"");}
    return 0xffffffff;
}
static int TC show_control(void *control,int show){log_call("show %x %d\n",offset(control),show);return -3;}
static int TC enable_control(void *control,int enable){log_call("enable %x %d\n",offset(control),enable);return -2;}
static void *TC focus_control(void *control){log_call("focus %x\n",offset(control));return control;}
static void TC title_control(void *control,const char *text){log_call("title %x [%s]\n",offset(control),text);}
static int TC check_control(void *control,int check){log_call("check %x %d\n",offset(control),check);return 0;}
static void TC image_control(void *control,uint32_t id,void *dib,void *palette,uint32_t module,uint16_t flags)
{log_call("image %x %x %x %x %x %x\n",offset(control),id,offset(dib),offset(palette),module,flags);}
static int SC invalid_control(uint32_t window,void *rect,int erase){log_call("invalidate %x rect=%u erase=%d\n",window,!!rect,erase);return 1;}
static int SC validate_control(uint32_t window,void *rect){log_call("validate %x rect=%u\n",window,!!rect);return 1;}
static uint32_t SC image_load(uint32_t module,uint32_t id,uint32_t type,int x,int y,uint32_t flags)
{log_call("load.image %x %x %x %d %d %x\n",module,id,type,x,y,flags);return 0xb1700000|id;}
static uint32_t SC cursor_load(uint32_t module,uint32_t id){log_call("load.cursor %x %x\n",module,id);return id^0x12340000;}
static uint32_t SC cursor_set(uint32_t cursor){log_call("set.cursor %x\n",cursor);return 0;}
static uint32_t SC send_message(uint32_t window,uint32_t message,uint32_t first,uint32_t second)
{if(message==0xc2)log_call("output %x [%s]\n",window,(char *)second);else log_call("send %x %x %x %x\n",window,message,first,second);return 0;}
static int TC output_selection(void *output,int first,int last){log_call("selection %x %d %d\n",offset(output),first,last);return 1;}
static int TC output_character(void *output,void *format){log_call("character %x %x\n",offset(output),offset(format));return 1;}
static int TC output_paragraph(void *output,void *format){log_call("paragraph %x %x\n",offset(output),offset(format));return 1;}
static int SC window_long(uint32_t window,int index){log_call("long %x %d\n",window,index);return s_scenario&2?(int)window:0;}
static int SC peek_message(void *message,uint32_t window,uint32_t first,uint32_t last,uint32_t flags)
{(void)message;log_call("peek %x %x %x %x\n",window,first,last,flags);return 0;}
static uint32_t SC get_focus(void){log_call("get.focus\n");return s_scenario&4?0xf0c05:0;}
static uint32_t SC get_active(void){log_call("get.active\n");return s_scenario&4?0xac71:0;}
static void *SC from_handle(uint32_t window){log_call("from.handle %x\n",window);return window==0xf0c05?s_fixture.object+0x1b44:window==0xac71?s_fixture.object:NULL;}
static int TC control_id(void *control){log_call("control.id %x\n",offset(control));return 0x1bf0;}
static int TC base_command(void *object,uint32_t command,uint32_t parameter){log_call("base.command %x %x %x\n",offset(object),command,parameter);return -7;}
static int SC object_delete(uint32_t bitmap){log_call("bitmap.delete %x\n",bitmap);return 1;}
static void *TC output_delete(void *output,uint32_t flags){log_call("output.delete %x %x\n",offset(output),flags);return output;}
static void TC close_control(void *object){log_call("close %x\n",offset(object));}
static int SC get_rect(uint32_t window,RECT *rect){log_call("rect %x\n",window);rect->left=-20;rect->top=10;rect->right=80;rect->bottom=60;return 0;}
static int SC point_inside(RECT *rect,int x,int y){log_call("point %ld %ld %ld %ld %d %d\n",rect->left,rect->top,rect->right,rect->bottom,x,y);return x>=rect->left&&x<rect->right&&y>=rect->top&&y<rect->bottom;}
static int SC get_bitmap(uint32_t bitmap,int size,void *out){log_call("bitmap %x size=%x\n",bitmap,size);U32(out,4)=7;U32(out,8)=8;PTR(out,0x14)=s_fixture.bitmap;return s_scenario&8?0:1;}
static int TC base_init(void *object){log_call("base.init %x\n",offset(object));return s_scenario%7!=0;}
static int TC skin(void *object,uint32_t id,uint32_t module_id,uint32_t flags){log_call("skin %x %x %x %x\n",offset(object),id,module_id,flags);return s_scenario%7!=1;}
static int TC art_init(void *control,uint32_t id,void *dib,void *palette,uint32_t flags){log_call("art %x %x %x %x %x\n",offset(control),id,offset(dib),offset(palette),flags);return 1;}
static int TC art_text(void *control,void *string){log_call("art.text %x %u\n",offset(control),string_index(string));strcpy(PTR(string,0),"fixture heading");return 15;}
static void *TC dib_ctor(void *dib){log_call("dib.new\n");U32(dib,0x60)=0xd1b;U32(dib,8)=37;U32(dib,12)=41;return dib;}
static void TC dib_dtor(void *dib){(void)dib;log_call("dib.delete\n");}
static int TC dib_load(void *dib,uint32_t id,uint32_t module_id,void *palette,uint32_t flags){(void)dib;log_call("dib.load %x %x %x %x\n",id,module_id,palette?offset(palette):0,flags);return 1;}
static int TC radio_init(void *control,void *font,uint32_t id,void *dib,void *palette,uint32_t module_id){(void)dib;log_call("radio %x %x %x %x %x\n",offset(control),offset(font),id,offset(palette),module_id);return 1;}
static void *SC base_font(int id){log_call("base.font %d\n",id);return (void *)0xf007;}
static void *TC font_ctor(void *font,void *base){log_call("font.new %x\n",(uint32_t)base);U32(font,4)=0xf007;return font;}
static void TC font_dtor(void *font){(void)font;log_call("font.delete\n");}
static void TC text_init(void *control,void *font){log_call("text.init %x color=%x\n",offset(control),U32(font,8));}
static int TC picture_init(void *control,uint32_t id,void *dib,void *palette,uint32_t module_id,uint16_t flags){image_control(control,id,dib,palette,module_id,flags);return 1;}
static int TC button_init(void *control,void *font,void *alternate,uint32_t id,void *dib,void *palette,uint32_t flags,int color,uint32_t module_id,int x,int y,struct fs98_world_rect rect){log_call("button %x %x %x %x %x %x %x %x %x %d %d %d %d %d %d\n",offset(control),offset(font),offset(alternate),id,offset(dib),offset(palette),flags,(uint32_t)color,module_id,x,y,rect.left,rect.top,rect.right,rect.bottom);return 1;}
static int SC copy_rect(void *out,void *in){log_call("copy.rect %x\n",offset(in));memcpy(out,in,16);return 1;}
static void SC remap(void *out,void *control){log_call("remap %x\n",offset(control));U32(out,0)=10;U32(out,4)=20;U32(out,8)=310;U32(out,12)=240;}
static int TC destroy_window(void *control){log_call("destroy.window %x\n",offset(control));return 1;}
static void SC rich_library(void){log_call("rich.library\n");}
static void *allocate(uint32_t size){log_call("allocate %x\n",size);return s_scenario%11?s_fixture.output:NULL;}
static void allocation_free(void *object){log_call("allocation.free %x\n",offset(object));}
static void *TC control_ctor(void *object){log_call("control.new %x\n",offset(object));U32(object,0)=0x12345678;return object;}
static int TC output_create(void *object,uint32_t flags,void *rect,void *parent,uint32_t id){log_call("output.create %x %x %x %x %x\n",offset(object),flags,offset(rect),offset(parent),id);if(object)U32(object,0x20)=0xede;return 0;}
static uint32_t SC get_dc(uint32_t window){log_call("get.dc %x\n",window);return 0xdc;}
static int SC release_dc(uint32_t window,uint32_t dc){log_call("release.dc %x %x\n",window,dc);return 1;}
static uint32_t SC device_caps(uint32_t dc,int index){log_call("caps %x %x\n",dc,index);return s_scenario&1?120:96;}
static int SC multiply_divide(int a,int b,int c){log_call("muldiv %d %d %d\n",a,b,c);return (a*b+c/2)/c;}
static char *SC copy_text(char *out,const char *text,int capacity){log_call("copy.text %x [%s] %d\n",offset(out),text,capacity);strncpy(out,text,(size_t)capacity);return out;}
static void *TC dc_ctor(void *dc){log_call("dc.new\n");U32(dc,4)=0;return dc;}
static void TC dc_dtor(void *dc){log_call("dc.delete %x\n",U32(dc,4));}
static void *TC client_ctor(void *dc,void *object){log_call("client.new %x\n",offset(object));U32(dc,4)=0xc11e;return dc;}
static void TC client_dtor(void *dc){log_call("client.delete %x\n",U32(dc,4));}
static uint32_t SC compatible_dc(uint32_t dc){log_call("compatible.dc %x\n",dc);return 0xc0dc;}
static int TC attach_dc(void *dc,uint32_t handle){log_call("dc.attach %x\n",handle);U32(dc,4)=handle;return 1;}
static void *SC select_bitmap(uint32_t dc,uint32_t bitmap){log_call("select.bitmap %x %x\n",dc,bitmap);return NULL;}
static void *TC select_palette(void *dc,void *palette,int background){log_call("select.palette %x %x %d\n",U32(dc,4),offset(palette),background);return NULL;}
static uint32_t SC realize(uint32_t dc){log_call("realize %x\n",dc);return 1;}
static int SC blit(uint32_t dc,int x,int y,int width,int height,uint32_t source,int sx,int sy,uint32_t rop){log_call("blit %x %d %d %d %d %x %d %d %x\n",dc,x,y,width,height,source,sx,sy,rop);return 1;}
static int SC update_window(uint32_t window){log_call("update %x\n",window);return 1;}
static int TC is_enabled(void *control){log_call("enabled %x\n",offset(control));return s_scenario%7!=0;}
static int SC screen_to_client(uint32_t window,int32_t *point){log_call("to.client %x %d %d\n",window,point[0],point[1]);point[0]=1;point[1]=2;return 1;}
static int TC default_handler(void *object){log_call("default %x\n",offset(object));return -5;}
static void *host(void){return s_host_services;}static uint32_t module(void){return 0x12345678;}static uint32_t resource_module(void){return 0x87654321;}
static int at_exit(void (*callback)(void)){(void)callback;log_call("at.exit\n");return 0;}
static void *dll_onexit(void *callback,void *first,void *last){(void)first;(void)last;log_call("at.exit\n");return callback;}
struct binding{uint32_t slot;void *function;};
static const struct binding s_bindings[]={
 {0x254f8,dc_ctor},{0x25538,dc_dtor},{0x25530,client_ctor},{0x25534,client_dtor},{0x254c4,compatible_dc},{0x2553c,attach_dc},{0x25540,select_bitmap},{0x25544,select_palette},{0x254d0,realize},{0x254c8,blit},{0x2587c,update_window},{0x255c4,is_enabled},{0x2586c,screen_to_client},{0x25584,default_handler},
 {0x257c0,base_init},{0x257c4,art_init},{0x255b8,art_text},{0x255bc,title_control},{0x256dc,dib_ctor},{0x256e4,dib_dtor},{0x256e0,dib_load},{0x257d0,base_font},{0x2578c,font_ctor},{0x2579c,font_dtor},{0x257dc,text_init},{0x25794,picture_init},{0x257c8,button_init},{0x25870,copy_rect},{0x25744,remap},{0x254b4,rich_library},{0x2550c,allocate},{0x25520,allocation_free},{0x255a4,control_ctor},{0x25680,output_create},{0x25874,get_dc},{0x25878,release_dc},{0x254cc,device_caps},{0x254e0,multiply_divide},{0x254e8,copy_text},
 {0x255b4,string_ctor},{0x255c0,string_dtor},{0x25578,string_assign},{0x25580,string_append},{0x2555c,string_copy},{0x25560,string_add},{0x25558,string_concat},{0x25548,string_load},
 {0x256c8,compare_text},{0x25698,integer_text},{0x25500,show_control},{0x255c8,enable_control},{0x25590,focus_control},{0x257e0,title_control},{0x2573c,check_control},{0x25858,image_control},
 {0x25894,invalid_control},{0x25868,validate_control},{0x25880,image_load},{0x25884,cursor_load},{0x25888,cursor_set},{0x258b0,send_message},{0x2554c,output_selection},{0x25554,output_character},{0x25550,output_paragraph},
 {0x2589c,window_long},{0x2588c,peek_message},{0x258bc,get_focus},{0x25864,get_active},{0x255cc,from_handle},{0x255d8,control_id},{0x257ac,base_command},{0x254bc,object_delete},{0x257b4,close_control},
 {0x258a8,get_rect},{0x258ac,point_inside},{0x254c0,get_bitmap}
};
static void *resolve(uint32_t slot){uint32_t index;for(index=0;index<sizeof(s_bindings)/sizeof(s_bindings[0]);++index)if(slot==s_bindings[index].slot)return s_bindings[index].function;fprintf(stderr,"missing import %x\n",slot);exit(2);}
static const struct fs98_world_facilities_services s_services={resolve,host,module,resource_module,s_output_vtable,at_exit};
static void patch(uint32_t rva,void *value){DWORD old;void **slot=(void **)((uint8_t *)s_retail+rva);if(!VirtualProtect(slot,4,PAGE_READWRITE,&old))exit(2);*slot=value;if(!VirtualProtect(slot,4,old,&old))exit(2);}
static void initialize(void)
{
    uint32_t index,list,field;memset(&s_fixture,0,sizeof(s_fixture));
    for(index=0;index<sizeof(s_fixture.object);++index)s_fixture.object[index]=(uint8_t)rnd();
    memset(s_fixture.before,0xa5,32);memset(s_fixture.after,0x5a,32);PTR(s_fixture.object,0x10dc)=s_fixture.descriptors[0];PTR(s_fixture.object,0x1428)=s_fixture.output;PTR(s_fixture.output,0)=s_output_vtable;
    U32(s_fixture.output,0x20)=0xede;U32(s_fixture.object,0x80)=0xabcdef;U32(s_fixture.object,0x1b64)=0x1bf0;U32(s_fixture.object,0x197c)=0x1bf5;
    U32(s_fixture.object,0x1420)=s_scenario%4;U32(s_fixture.object,0x10f4)=10;U32(s_fixture.object,0x10e8)=0xface;U32(s_fixture.object,0x10e4)=255;
    for(index=0;index<100;++index){U32(s_fixture.object,0x1100+index*8)=10;U32(s_fixture.object,0x1104+index*8)=0xf930+index%5;}
    if(s_operation==1||s_operation==2)U32(s_fixture.object,0x1420)=s_scenario%3==0?100:s_scenario%3==1?99:0xffffffff;
    for(index=0;index<2;++index){void *descriptor=s_fixture.descriptors[index];PTR(descriptor,12)=owner;
        for(list=0;list<2;++list){U32(descriptor,0x24+list*8)=s_scenario%128==127?0x80000000u:s_scenario%128==126?0xffffffffu:(s_scenario/16)%4;PTR(descriptor,0x28+list*8)=s_fixture.rows[list];}}
    for(list=0;list<2;++list)for(index=0;index<3;++index){uint8_t *row=s_fixture.rows[list]+index*(list?0x60:0x64);for(field=0;field<5;++field)PTR(row,field*4)=field==0?"region":field==1?(s_scenario&8?"resource-f933":"country"):field==2?"state":field==3?"city":"name";
        strcpy((char *)row+0x14,s_scenario&1?"":"freq");strcpy((char *)row+0x1f,"lat");strcpy((char *)row+0x34,"lon");strcpy((char *)row+0x49,"alt");strcpy((char *)row+0x51,"ID");
        U32(row,0x5c)=list?s_scenario%16:s_scenario%5;if(!list)PTR(row,0x60)=s_fixture.ends[index];
        for(field=0;field<4;++field){uint8_t *end=s_fixture.ends[index][field];U32(end,0)=(s_scenario+field)%43;U32(end,4)=(s_scenario+field)%6;U32(end,8)=(s_scenario+field)%12;strcpy((char *)end+12,"length");strcpy((char *)end+20,"ILS");strcpy((char *)end+31,"id");}}
    for(index=0;index<128;++index)s_fixture.bitmap[index]=(uint8_t)rnd();
    PTR(s_fixture.object,0x10fc)=s_fixture.nodes;U32(s_fixture.object,0x10f8)=3;
    for(index=0;index<3;++index){s_fixture.nodes[index][0]=index+10;s_fixture.nodes[index][1]=0xf930+index;s_fixture.nodes[index][2]=0x6c4+index;s_fixture.nodes[index][3]=17+index;s_fixture.nodes[index][4]=35+index;}
    if(s_operation>=15){U32(s_fixture.object,0x10e8)=10+s_scenario%3;U32(s_fixture.object,0x10e4)=10+s_scenario%3;U32(s_fixture.object,0x1c08)=0x20;}
    PTR(s_fixture.object,0)=s_vtable;PTR(s_fixture.object,0x195c)=s_radio_vtable;PTR(s_fixture.object,0x1a50)=s_radio_vtable;PTR(s_fixture.object,0x1724)=s_spin_vtable;
    s_initial=s_fixture;
}
static const uint32_t s_entries[]={0x17210,0x186e0,0x186e0,0x173b0,0x19750,0x197b0,0x19770,0x19230,0x19670,0x196c0,0x171c0,0x18a40,0x18c80,0x186e0,0x18aa0,0x19400,0x19230,0x19230};
static void invoke(void)
{
    void *object=s_fixture.object,*entry=(uint8_t *)s_retail+s_entries[s_operation];uint32_t result=0;
    if(s_operation==0){if(s_side)fs98_world_facilities_release(object);else ((void (TC *)(void *))entry)(object);}
    if(s_operation==1||s_operation==2){uint32_t mode=s_operation==1?10:20;if(s_side)fs98_world_facilities_transition(object,mode,0xf933,0);else ((void (TC *)(void *,uint32_t,uint32_t,uint32_t))entry)(object,mode,0xf933,0);}
    if(s_operation==3){uint32_t resource=0xf930+(s_scenario/3)%6,mode=s_scenario%3==0?10:s_scenario%3==1?20:40;if(s_side)fs98_world_facilities_output(object,resource,mode);else ((void (TC *)(void *,uint32_t,uint32_t))entry)(object,resource,mode);}
    if(s_operation==4){if(s_side)fs98_world_facilities_home(object);else ((void (TC *)(void *))entry)(object);}
    if(s_operation==5){if(s_side)fs98_world_facilities_back(object);else ((void (TC *)(void *))entry)(object);}
    if(s_operation==6){if(s_side)fs98_world_facilities_refresh(object);else ((void (TC *)(void *))entry)(object);}
    if(s_operation==7){uint32_t command=(s_scenario%4<<16)|0x1bf0;result=s_side?(uint32_t)fs98_world_facilities_command(object,command,0x1234):((uint32_t (TC *)(void *,uint32_t,uint32_t))entry)(object,command,0x1234);}
    if(s_operation==8){if(s_side)fs98_world_facilities_close(object);else ((void (TC *)(void *))entry)(object);}
    if(s_operation==9){if(s_side)fs98_world_facilities_accept(object);else ((void (TC *)(void *))entry)(object);}
    if(s_operation==10){result=s_side?(uint32_t)fs98_world_facilities_hit(object,(int)s_scenario%100,10):((uint32_t (TC *)(void *,int,int))entry)(object,(int)s_scenario%100,10);}
    if(s_operation==11){result=s_side?(uint32_t)fs98_world_facilities_pixel(object,0x1234,(int)s_scenario%7,(int)s_scenario%8):((uint32_t (TC *)(void *,uint32_t,int,int))entry)(object,0x1234,(int)s_scenario%7,(int)s_scenario%8);}
    if(s_operation==12)result=s_side?(uint32_t)fs98_world_facilities_init(object):((uint32_t (TC *)(void *))entry)(object);
    if(s_operation==13){if(s_side)fs98_world_facilities_transition(object,40,0xf930+(s_scenario/3)%6,0);else ((void (TC *)(void *,uint32_t,uint32_t,uint32_t))entry)(object,40,0xf930+(s_scenario/3)%6,0);}
    if(s_operation==14)result=s_side?(uint32_t)fs98_world_facilities_paint(object,s_scenario%3?10:0):((uint32_t (TC *)(void *,uint32_t))entry)(object,s_scenario%3?10:0);
    if(s_operation==15){if(s_side)fs98_world_facilities_mouse(object,1,3,14,0);else ((void (TC *)(void *,uint32_t,int,int))entry)(object,1,3,14);}
    if(s_operation==16||s_operation==17){uint32_t command=s_operation==16?(0x10000|(3+s_scenario%2)):(((s_scenario%3==0?0x100:s_scenario%3==1?0x200:0x7e9)<<16)|0x1bf0);result=s_side?(uint32_t)fs98_world_facilities_command(object,command,0x1234):((uint32_t (TC *)(void *,uint32_t,uint32_t))entry)(object,command,0x1234);}
    if(!s_side&&!s_throw_at){if(s_operation==0&&(U32(PTR(object,0x10dc),0x24)||U32(PTR(object,0x10dc),0x2c)))exit(2);if((s_operation==3||s_operation==6)&&s_fixture.created!=s_fixture.destroyed)exit(2);++s_checks;}
    log_call("return %x\n",result);
}
static void land(void){__asm__ volatile("movl %0, %%fs:0"::"r"(s_saved_seh):"memory");__builtin_longjmp(s_jump,1);}
static EXCEPTION_DISPOSITION __cdecl catcher(EXCEPTION_RECORD *record,void *frame,CONTEXT *context,void *dispatcher)
{(void)context;(void)dispatcher;if(record->ExceptionCode==THROW_CODE&&!(record->ExceptionFlags&(EXCEPTION_UNWINDING|EXCEPTION_EXIT_UNWIND))){RtlUnwind(frame,NULL,record,NULL);s_throw_at=0;log_call("caught\n");if(!s_side)++s_throws;land();}return ExceptionContinueSearch;}
static LONG WINAPI fatal(EXCEPTION_POINTERS *e){fprintf(stderr,"FAULT %lx at %p side=%u scenario=%u op=%u throw=%u\n",e->ExceptionRecord->ExceptionCode,e->ExceptionRecord->ExceptionAddress,s_side,s_scenario,s_operation,s_throw_at);ExitProcess(2);return 0;}
static void run(uint32_t throw_at)
{
    struct{void *previous,*handler;}frame;uint32_t field;void *nodes;
    s_throw_at=0;if(s_side)fs98_world_facilities_deinit();
    else {U32(s_retail,0x24b64)=0;U32(s_retail,0x24b60)=0;}
    s_fixture=s_initial;s_calls=s_length=0;s_logs[s_side][0]=0;s_throw_at=throw_at;
    __asm__ volatile("movl %%fs:0, %0":"=r"(s_saved_seh));
    if(__builtin_setjmp(s_jump)==0){frame.previous=(void *)s_saved_seh;frame.handler=catcher;__asm__ volatile("movl %0, %%fs:0"::"r"(&frame):"memory");invoke();}
    __asm__ volatile("movl %0, %%fs:0"::"r"(s_saved_seh):"memory");
    for(field=0;field<2;++field){uint32_t offset;for(offset=0x14;offset<=0x1c;offset+=4)if(PTR(s_fixture.descriptors[field],offset))U32(s_fixture.descriptors[field],offset)=1;}
    nodes=PTR(s_fixture.object,0x10fc);if(nodes!=s_fixture.nodes){uint32_t count=U32(s_fixture.object,0x10f8);if(count>50)exit(2);memcpy(s_fixture.nodes,nodes,count*20);PTR(s_fixture.object,0x10fc)=s_fixture.nodes;}
    if(s_operation==12&&PTR(s_fixture.output,0)!=(void *)0x12345678)PTR(s_fixture.output,0)=s_output_vtable;
    if(memcmp(s_fixture.before,s_initial.before,32)||memcmp(s_fixture.after,s_initial.after,32))exit(2);
}
static int compare(uint32_t at)
{
    s_side=0;run(at);s_reference=s_fixture;s_side=1;run(at);++s_comparisons;
    if(!memcmp(&s_reference,&s_fixture,sizeof(s_fixture))&&!strcmp(s_logs[0],s_logs[1]))return 0;
    fprintf(stderr,"FACILITIES DIFFER scenario=%u operation=%u throw=%u\n",s_scenario,s_operation,at);
    {uint32_t index;for(index=0;index<sizeof(s_fixture);++index)if(((uint8_t *)&s_reference)[index]!=((uint8_t *)&s_fixture)[index]){fprintf(stderr,"byte=%x retail=%x ours=%x\n",index,((uint8_t *)&s_reference)[index],((uint8_t *)&s_fixture)[index]);break;}}
    fprintf(stderr,"RETAIL:\n%sOURS:\n%s",s_logs[0],s_logs[1]);return 1;
}
int main(int argc,char **argv)
{
    uint32_t count=argc>1?strtoul(argv[1],NULL,0):200,position,limit,index;int unwind=argc>2&&!strcmp(argv[2],"unwind");
    SetErrorMode(SEM_NOGPFAULTERRORBOX);SetUnhandledExceptionFilter(fatal);if(!count)return 2;
    s_retail=LoadLibraryExA("orig/English/World.DLL",NULL,DONT_RESOLVE_DLL_REFERENCES);if(!s_retail||GetProcAddress(s_retail,"Linkage")!=(FARPROC)((uint8_t *)s_retail+0x24120))return 2;
    for(index=0;index<sizeof(s_bindings)/sizeof(s_bindings[0]);++index)patch(s_bindings[index].slot,s_bindings[index].function);
    PTR(s_host_services,0x214)=free_pointer;PTR(s_retail,0x2410c)=s_host_services;s_output_vtable[1]=output_delete;
    patch(0x256bc,(void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"),"__CxxFrameHandler"));U32(s_retail,0x24974)=module();
    patch(0x256c4,dll_onexit);
    patch(0x256a4,(void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"),"_except_handler3"));
    fs98_world_facilities_set_services(&s_services);
    s_vtable[0xe4/4]=skin;s_radio_vtable[0xc4/4]=radio_init;s_spin_vtable[0x60/4]=destroy_window;U32(s_retail,0x2495c)=resource_module();
    for(s_scenario=0;s_scenario<count;++s_scenario)for(s_operation=0;s_operation<18;++s_operation){initialize();if(compare(0))return 1;limit=s_calls-1;if(unwind)for(position=1;position<=limit;++position)if(compare(position))return 1;}
    printf("World facilities core: %u scenarios, 18 operations, %u comparisons, %u unwinds, %u witnesses, 0 differ; seed=4a971b63\n",count,s_comparisons,s_throws,s_checks);return 0;
}
