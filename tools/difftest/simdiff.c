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
    uint8_t pages[6][0x3000], notify[3][32], notify_storage[0x29c], help[32], exchange[64];
    uint32_t page_blocks;
    char axes[2][4][24];
    const char *labels[2][4];
    char strings[2][2][32];
    char number_buffers[2][2][64];
    uint8_t settings_data[2][0x380];
    char fuel_buffers[2][3][5][64];
    char fuel_capacities[2][5][32];
    char host_buffers[24][128];
    uint32_t host_blocks;
    uint32_t resource_module;
    uint8_t guard_after[32];
};
static struct fixture s_fixture, s_initial, s_reference;
static HMODULE s_retail;
static uint32_t s_rng, s_seed, s_scenario, s_operation, s_calls, s_throw_at, s_saved_seh;
static uint32_t s_log_length, s_script, s_kind, s_descriptor_offset, s_comparisons, s_throws, s_faults, s_swaps;
static int s_side, s_lifetime;
static int s_settings_init;
static char s_logs[2][LOG_SIZE];
static void *s_widget_vtable[60];
static void *s_page_vtable[60], *s_other_page_vtable[60];
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
        if(s_side)fs98_sim_set_resource_module(s_seed);else D32(s_retail,0xc130)=s_seed;
    }
}
static void *base_construct(void *object,uint32_t import_slot,int32_t template,void *parent)
{ put("base.construct %u import=%x template=%x parent=%u\n",offset(object),import_slot,template,(uint32_t)parent);D32(object,0)=0xba5e0000;D32(object,0x20)=s_seed;mutate();return object; }
static void construct(void *object,uint32_t import_slot)
{ put("construct %u import=%x\n",offset(object),import_slot);D32(object,0)=import_slot;D32(object,4)=s_seed;mutate(); }
static void destroy(void *object,uint32_t import_slot)
{ put("destroy %u import=%x vtable=%x\n",offset(object),import_slot,D32(object,0));D32(object,0)=import_slot;mutate(); }
static void *allocate(uint32_t size)
{ put("allocate %u\n",size);mutate();if(s_settings_init && size!=0x29c){if(s_fixture.page_blocks>=6)exit(2);return s_fixture.pages[s_fixture.page_blocks++];}return s_scenario%11?(size==0x29c?s_fixture.notify_storage:s_fixture.object):0; }
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
static int32_t send_message(uint32_t window,uint32_t message,uint32_t first,uint32_t second) { if(message==0x143)put("send %u %x %u text=[%s]\n",window,message,first,(const char *)second);else put("send %u %x %u/%u\n",window,message,first,second);mutate();if(s_kind==3 && message==0x147){static const int values[]={-1,0,1,2,3,4,5,6,7,8,0x7fffffff};return values[s_scenario%11];}if(s_kind==3 && message==0xb8)return s_scenario%5!=0;if(s_kind==4 && message==0x147){static const int32_t values[]={-1,0,1,0x7fffffff};return values[(s_scenario+s_calls)%4];}return (int32_t)(s_seed+s_calls); }
static void text(void *control,const char *value) { put("text %u [%s]\n",offset(control),value);mutate(); }
static void close_dialog(void *object) { put("base.close %u\n",offset(object));mutate(); }
static int32_t radio_check(void *control,int32_t checked) { put("radio.check %u %d\n",offset(control),checked);mutate();return (int32_t)s_seed; }
static void *active_page(void *tabs) { put("active.page %u\n",offset(tabs));mutate();return s_fixture.pages[s_scenario&1]; }
static uint32_t focus(void) { put("focus\n");mutate();return s_seed; }
static void *from_handle(uint32_t window) { put("from.handle %u\n",window);mutate();return s_scenario%3?s_fixture.exchange:0; }
static int32_t is_edit(void *control) { put("is.edit %u\n",offset(control));mutate();return s_kind==3?(s_scenario/4)&1:s_scenario&1; }
static int32_t control_id(void *control) { put("control.id %u\n",offset(control));mutate();return (int32_t)s_seed; }
static int32_t TC virtual_validate(void *object,uint32_t id)
{ put("virtual.validate %u id=%u\n",offset(object),id);mutate();if(s_script&1)PTR(object,0)=s_other_page_vtable;return s_scenario%3?(int32_t)s_seed:0; }
static int32_t TC virtual_other_validate(void *object,uint32_t id) { put("other.validate\n");return virtual_validate(object,id); }
static int32_t TC virtual_error(void *object,int32_t error) { put("virtual.error %u error=%d\n",offset(object),error);mutate();return s_scenario&2; }
static int32_t TC virtual_other_error(void *object,int32_t error) { put("other.error\n");return virtual_error(object,error); }
static int32_t base_init(void *object) { put("base.init %u\n",offset(object));mutate();return (int32_t)s_seed; }
static int32_t TC virtual_skin(void *object,uint32_t id,uint32_t module,uint32_t flags)
{ put("skin %u id=%x module=%u flags=%u\n",offset(object),id,module,flags);mutate();return s_scenario%4?(int32_t)(s_seed|1):0; }
static int32_t TC virtual_other_skin(void *object,uint32_t id,uint32_t module,uint32_t flags)
{ put("other.skin\n");return virtual_skin(object,id,module,flags); }
static int32_t init_button(void *control,void *font,void *alternate,uint32_t bitmap,void *dib,void *palette,
    uint32_t flags,uint32_t color,uint32_t module,int32_t x,int32_t y,struct fs98_sim_rect rect)
{
    put("init.button %u font=%u/%u bitmap=%x dib=%u pal=%u flags=%u color=%x module=%u xy=%d/%d rect=%d/%d/%d/%d\n",
        offset(control),offset(font),offset(alternate),bitmap,offset(dib),offset(palette),flags,color,module,x,y,rect.left,rect.top,rect.right,rect.bottom);
    mutate();return (int32_t)s_seed;
}
static int32_t init_art(void *control,int32_t bitmap,void *dib,void *palette,uint32_t flags)
{ put("init.art %u bitmap=%x dib=%u pal=%u flags=%u\n",offset(control),bitmap,offset(dib),offset(palette),flags);mutate();return (int32_t)s_seed; }
static void init_text(void *control,void *font) { put("init.text %u font=%u\n",offset(control),offset(font));mutate(); }
static void notify_construct(void *notify,uint32_t flags,uint32_t mode)
{ put("notify.construct %u flags=%u mode=%u\n",offset(notify),flags,mode);D32(notify,0)=0xc0001234;D32(notify,4)=flags;D32(notify,8)=mode;mutate(); }
static void notify_delete(void *notify) { put("notify.delete %u\n",offset(notify));mutate(); }
static int32_t notify_send(void *notify,const char *format,const char *title,void *argument)
{ put("notify.send %u format=[%s] title=[%s] argument=%u\n",offset(notify),format,title,(uint32_t)argument);mutate();return (int32_t)s_seed; }
static void *get_control(void *object,uint32_t id) { put("get.control %u id=%x\n",offset(object),id);mutate();return s_kind==3 && s_scenario%3==0?0:s_fixture.exchange; }
static void remap_rect(struct fs98_sim_rect *rect,void *control)
{ put("remap.rect control=%u\n",offset(control));rect->left=(int32_t)s_seed;rect->top=(int32_t)(s_seed^0xfedcba98);rect->right=(int32_t)(s_seed+0x87654321);rect->bottom=(int32_t)(s_seed^0x12345678);mutate(); }
static void move_control(void *control,int32_t x,int32_t y,int32_t width,int32_t height,int32_t repaint)
{ put("move.control %u xy=%d/%d size=%d/%d repaint=%d\n",offset(control),x,y,width,height,repaint);mutate(); }
static void paint_construct(void *dc,void *object) { put("paint.construct object=%u\n",offset(object));D32(dc,0)=s_seed;mutate(); }
static void paint_destroy(void *dc) { put("paint.destroy marker=%u\n",D32(dc,0));mutate(); }
static int32_t load_string(uint32_t module,uint32_t id,char *buffer,int32_t capacity)
{ put("load.string module=%u id=%x capacity=%d\n",module,id,capacity);snprintf(buffer,capacity,"label%x",id);mutate();return strlen(buffer); }
static int32_t invalidate(uint32_t window,const void *rect,int32_t erase)
{ put("invalidate window=%u rect=%u erase=%d\n",window,(uint32_t)rect,erase);mutate();return (int32_t)s_seed; }
static int32_t set_rect(struct fs98_sim_rect *rect,int32_t left,int32_t top,int32_t right,int32_t bottom)
{ put("set.rect %d/%d/%d/%d\n",left,top,right,bottom);rect->left=left;rect->top=top;rect->right=right;rect->bottom=bottom;mutate();return (int32_t)s_seed; }
static void string_construct_text(void *string,const char *text)
{ put("string.construct.text [%s]\n",text);PTR(string,0)=(void *)text;mutate(); }
static void draw_text(void *font,void *dc,uint32_t string,struct fs98_sim_rect rect,int32_t flags)
{ put("draw.text font=%u marker=%u text=[%s] rect=%d/%d/%d/%d flags=%d\n",offset(font),D32(dc,0),(const char *)string,rect.left,rect.top,rect.right,rect.bottom,flags);mutate(); }
static int32_t init_slider(void *control,uint32_t first_bitmap,uint32_t second_bitmap,void *dib,void *palette,
    uint32_t first,uint32_t second,uint32_t third,uint32_t fourth,int32_t x,int32_t y,uint32_t module)
{ put("init.slider %u bitmap=%x/%x dib=%u pal=%u cells=%u/%u/%u/%u xy=%d/%d module=%u\n",offset(control),first_bitmap,second_bitmap,(uint32_t)dib,(uint32_t)palette,first,second,third,fourth,x,y,module);mutate();return (int32_t)s_seed; }
static int32_t TC widget_init(void *control,void *font,uint32_t bitmap,void *dib,void *palette,uint32_t module)
{ put("widget.init %u font=%u bitmap=%x dib=%u pal=%u module=%u\n",offset(control),offset(font),bitmap,(uint32_t)dib,(uint32_t)palette,module);mutate();return (int32_t)s_seed; }
static void init_group(void *control,void *font){put("init.group %u font=%u\n",offset(control),offset(font));mutate();}
static void init_combo(void *control,void *font,void *label){put("init.combo %u font=%u label=%u\n",offset(control),offset(font),offset(label));mutate();}
static void control_text(void *control,const char *value){put("control.text %u [%s]\n",offset(control),value);mutate();}
static int32_t get_text(void *control,char *buffer,int32_t capacity)
{
    const char *value=(uint8_t *)control==s_fixture.object+0x1e04?"29.92":"1013.2";
    put("get.text %u buffer=%u capacity=%d\n",offset(control),offset(buffer),capacity);
    if(s_kind==3){put("fuel.edit row=%u mode=%u\n",D32(s_fixture.object,0x111c),D32(PTR(s_fixture.object,0x1124),0x20));if(s_script&4)D32(s_fixture.object,0x111c)=6;}
    if(capacity>0)snprintf(buffer,(size_t)capacity,"%s",value);
    mutate();return capacity>0?(int32_t)strlen(buffer):0;
}
static int32_t error_dialog(void *object,void *control,void *descriptor)
{ put("error.dialog %u control=%u descriptor=%u\n",offset(object),offset(control),offset(descriptor));mutate();return (s_kind==5?(s_scenario/3)%3:s_scenario%3)?(int32_t)(s_seed|1):0; }
static void *base_font(int32_t id){put("base.font %d\n",id);mutate();if(s_settings_init && s_script&8){if(s_side)fs98_sim_set_resource_module(s_seed^0x1234);else D32(s_retail,0xc130)=s_seed^0x1234;}return s_fixture.help;}
static void init_edit(void *control,void *font,uint32_t foreground,uint32_t background,uint32_t error_color,void *label,uint32_t flags)
{ put("init.edit %u font=%u fg=%x bg=%x error=%x label=%u flags=%x\n",offset(control),offset(font),foreground,background,error_color,offset(label),flags);mutate(); }
static int32_t SC validate_numbers(void *helper)
{
    void *object=s_fixture.object;
    put("numbers.validate helper=%u mode=%u\n",offset(helper),D32(helper,0x20));mutate();
    if(s_script&1)PTR(object,0x111c)=PTR(object,0x111c)==s_fixture.descriptor[0]?s_fixture.descriptor[1]:s_fixture.descriptor[0];
    if(s_script&4){D32(object,0x1118)=s_scenario%3;D32(helper,0x20)=(s_scenario/4)%6;}
    if(s_script&8)PTR(object,0x1120)=s_fixture.settings_data[1];
    return s_scenario%4==0?0:(int32_t)(s_seed|1);
}
static int32_t number_convert(void *helper,uint32_t operation)
{
    void *descriptor=PTR(s_fixture.object,0x111c);
    uint32_t field=operation?0x5c:0x48;
    put("numbers.convert helper=%u operation=%u mode=%u\n",offset(helper),operation,D32(helper,0x20));mutate();
    descriptor=PTR(s_fixture.object,0x111c);
    if(D32(descriptor,field+4))snprintf(PTR(descriptor,field),D32(descriptor,field+4),"converted%u",operation);
    return (int32_t)s_seed;
}
static int32_t SC number_to_first(void *helper){return number_convert(helper,0);}
static int32_t SC number_to_second(void *helper){return number_convert(helper,1);}
static int32_t SC number_to_first_other(void *helper){put("other.convert.first\n");return number_convert(helper,0);}
static int32_t SC number_to_second_other(void *helper){put("other.convert.second\n");return number_convert(helper,1);}
static int SC fuel_validate_numbers(void *helper)
{
    void *object=s_fixture.object;
    put("fuel.validate helper=%u mode=%u row=%u cache=%u\n",offset(helper),D32(helper,0x20),D32(helper,0x24),D32(object,0x111c));
    mutate();
    if(s_script&4){D32(helper,0x20)=(s_scenario/4)%6;D32(helper,0x24)=(s_scenario/7)%7;}
    if(s_script&8)PTR(object,0x1124)=s_fixture.settings_data[1];
    return s_scenario%4==0?0:(int)(s_seed|1);
}
static int fuel_convert(void *helper,uint32_t row,uint32_t mode)
{
    put("fuel.convert helper=%u row=%u mode=%u\n",offset(helper),row,mode);
    mutate();
    if(s_script&1)PTR(s_fixture.object,0x1120)=s_fixture.descriptor[1];
    if(s_script&8)PTR(s_fixture.object,0x1124)=s_fixture.settings_data[1];
    D32(PTR(s_fixture.object,0x1124),0x24)=(s_scenario/4)%5;
    return (int)s_seed;
}
static int SC fuel_convert_first(void *helper,uint32_t row){return fuel_convert(helper,row,1);}
static int SC fuel_convert_second(void *helper,uint32_t row){return fuel_convert(helper,row,2);}
static int SC fuel_convert_third(void *helper,uint32_t row){return fuel_convert(helper,row,3);}
static int fuel_is_enabled(void *control)
{ put("is.enabled %u\n",offset(control));mutate();return (s_scenario+offset(control)/4)%3!=0; }
static int fuel_window_control_id(uint32_t window)
{ put("window.control.id %u\n",window);mutate();return (int)s_seed; }
static void *fuel_host_block(uint32_t size)
{
    if(size>128 || s_fixture.host_blocks>=24)exit(2);
    return s_fixture.host_buffers[s_fixture.host_blocks++];
}
static uint32_t fuel_host_string(uint32_t id,char **text_out)
{
    put("host.string %x\n",id);
    *text_out=fuel_host_block(32);
    snprintf(*text_out,32,"label%x",id);
    mutate();return strlen(*text_out);
}
static uint32_t fuel_host_length(const char *text)
{ uint32_t length=text?(uint32_t)strlen(text):0;put("host.length [%s]\n",text?text:"(null)");mutate();return length; }
static int fuel_host_alloc(int flags,uint32_t size,void **data_out,int zero)
{ put("host.alloc %d size=%u zero=%d\n",flags,size,zero);*data_out=fuel_host_block(size);if(zero)memset(*data_out,0,size);mutate();return 0; }
static int fuel_host_realloc(int flags,uint32_t size,void **data_out)
{
    void *previous=*data_out;
    put("host.realloc %d size=%u old=%u\n",flags,size,offset(previous));
    *data_out=fuel_host_block(size);strcpy(*data_out,previous);mutate();return 0;
}
static int fuel_host_free(void **data_out)
{ put("host.free %u\n",offset(*data_out));*data_out=0;mutate();return 0; }
static char *fuel_string_copy(char *destination,const char *source)
{ put("string.copy %u [%s]\n",offset(destination),source);strcpy(destination,source);mutate();return destination; }
static char *fuel_string_cat(char *destination,const char *source)
{ put("string.cat %u [%s]\n",offset(destination),source);strcat(destination,source);mutate();return destination; }
static int fuel_string_length(const char *text)
{ int length=strlen(text);put("string.length [%s]\n",text);mutate();return length; }
static int fuel_parse_long(const char *text)
{ int value=strtol(text,0,10);put("parse.long [%s]\n",text);mutate();return value; }
static void settings_init_tabs(void *tabs,void *parent,void *font,uint32_t bitmap,
    void *dib,void *palette,uint32_t flags,uint32_t color,int inset,int height,int width,
    int selection,uint32_t module,int last)
{
    put("init.tabs %u parent=%u font=%u bitmap=%x dib=%u palette=%u flags=%x color=%x inset=%d height=%d width=%d selection=%d module=%x last=%d\n",
        offset(tabs),offset(parent),offset(font),bitmap,offset(dib),offset(palette),flags,color,inset,height,width,selection,module,last);
    mutate();if(s_script&2)PTR(parent,0)=s_other_vtable;
}
static void settings_init_page(void *page,uint32_t flags)
{ put("init.page %u flags=%u\n",offset(page),flags);mutate(); }
static void settings_add_page(void *tabs,void *page,const char *label,uint32_t available)
{ put("add.page %u page=%u label=[%s] available=%u\n",offset(tabs),offset(page),label,available);mutate(); }
static void settings_init_tab_helper(void *helper,void *tabs)
{
    put("init.tab.helper %u tabs=%u\n",offset(helper),offset(tabs));mutate();
    if(helper==s_fixture.object+0x2200 && s_script&4)PTR(s_fixture.object,0x10ec)=0;
}
static int settings_modify_style(void *control,uint32_t remove,uint32_t add,uint32_t flags)
{ put("modify.style %u remove=%x add=%x flags=%x\n",offset(control),remove,add,flags);mutate();return (int)s_seed; }
static int settings_window_rect(uint32_t window,struct fs98_sim_rect *rect)
{
    put("window.rect %u rect=%u\n",window,offset(rect));
    rect->left=s_seed;rect->top=s_seed+17;rect->right=s_seed+210;rect->bottom=s_seed+91;
    mutate();return (int)s_seed;
}
static int settings_screen_to_client(uint32_t window,int *point)
{ put("screen.to.client %u point=%u\n",window,offset(point));point[0]=(uint32_t)point[0]-123;point[1]=(uint32_t)point[1]+91;mutate();return (int)s_seed; }
static void settings_last_page(void *tabs,void *page)
{ put("last.page %u page=%u\n",offset(tabs),offset(page));mutate(); }
static void settings_active_tab(void *tabs,uint32_t index,int focus)
{ put("active.tab %u index=%u focus=%d\n",offset(tabs),index,focus);mutate(); }
static int settings_message_box(void *object,const char *text,const char *caption,uint32_t flags)
{ put("message.box %u [%s] caption=[%s] flags=%x\n",offset(object),text,caption?caption:"(null)",flags);mutate();return (int)s_seed; }
static void TC settings_virtual_art(void *object,uint32_t bitmap,uint32_t x,uint32_t y,uint32_t width,uint32_t height,uint32_t module)
{ put("virtual.art %u bitmap=%x xy=%u/%u wh=%u/%u module=%x\n",offset(object),bitmap,x,y,width,height,module);mutate(); }
static void TC settings_other_virtual_art(void *object,uint32_t bitmap,uint32_t x,uint32_t y,uint32_t width,uint32_t height,uint32_t module)
{ put("other.art\n");settings_virtual_art(object,bitmap,x,y,width,height,module); }
static const struct fs98_sim_services s_services={base_construct,destroy,construct,allocate,ui_delete,mfc_delete,dialog_delete,
    create,show,ddx,help,enable_window,enable_control,window_long,check,slider_get,slider_range,slider_set,send_message,text,close_dialog,radio_check,active_page,focus,from_handle,is_edit,control_id,
    base_init,init_button,init_art,init_text,notify_construct,notify_delete,notify_send,get_control,remap_rect,move_control,
    paint_construct,paint_destroy,load_string,invalidate,set_rect,string_construct_text,draw_text,init_group,init_combo,control_text,get_text,error_dialog,base_font,init_edit,init_slider,fuel_is_enabled,fuel_window_control_id,
    fuel_host_string,fuel_host_length,fuel_host_alloc,fuel_host_realloc,fuel_host_free,fuel_string_copy,fuel_string_cat,fuel_string_length,fuel_parse_long,
    settings_init_tabs,settings_init_page,settings_add_page,settings_init_tab_helper,settings_modify_style,settings_window_rect,settings_screen_to_client,settings_last_page,settings_active_tab,settings_message_box};
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
static int32_t TC hook_radio_check(void *control,int32_t checked) { return radio_check(control,checked); }
static void *TC hook_active_page(void *tabs) { return active_page(tabs); }
static uint32_t SC hook_focus(void) { return focus(); }
static void *SC hook_from_handle(uint32_t window) { return from_handle(window); }
static int32_t TC hook_is_edit(void *control,void *runtime_class) { if(runtime_class!=(void *)0x12345678)exit(2);return is_edit(control); }
static int32_t TC hook_control_id(void *control) { return control_id(control); }
static int32_t TC hook_base_init(void *object){return base_init(object);}
static int32_t TC hook_init_button(void *control,void *font,void *alternate,uint32_t bitmap,void *dib,void *palette,
    uint32_t flags,uint32_t color,uint32_t module,int32_t x,int32_t y,struct fs98_sim_rect rect)
{ return init_button(control,font,alternate,bitmap,dib,palette,flags,color,module,x,y,rect); }
static int32_t TC hook_init_art(void *control,int32_t bitmap,void *dib,void *palette,uint32_t flags){return init_art(control,bitmap,dib,palette,flags);}
static void TC hook_init_text(void *control,void *font){init_text(control,font);}
static void *TC hook_notify_construct(void *notify,uint32_t flags,uint32_t mode){notify_construct(notify,flags,mode);return notify;}
static void SC hook_notify_delete(void *notify){notify_delete(notify);}
static int32_t hook_notify_send(void *notify,const char *format,const char *title,void *argument){return notify_send(notify,format,title,argument);}
static void *TC hook_get_control(void *object,uint32_t id){return get_control(object,id);}
static void SC hook_remap_rect(struct fs98_sim_rect *rect,void *control){remap_rect(rect,control);}
static void TC hook_move_control(void *control,int32_t x,int32_t y,int32_t width,int32_t height,int32_t repaint){move_control(control,x,y,width,height,repaint);}
static void TC hook_paint_construct(void *dc,void *object){paint_construct(dc,object);}
static void TC hook_paint_destroy(void *dc){paint_destroy(dc);}
static int32_t SC hook_load_string(uint32_t module,uint32_t id,char *buffer,int32_t capacity){return load_string(module,id,buffer,capacity);}
static int32_t SC hook_invalidate(uint32_t window,const void *rect,int32_t erase){return invalidate(window,rect,erase);}
static int32_t SC hook_set_rect(struct fs98_sim_rect *rect,int32_t left,int32_t top,int32_t right,int32_t bottom){return set_rect(rect,left,top,right,bottom);}
static void TC hook_string_text(void *string,const char *text){string_construct_text(string,text);}
static void TC hook_draw_text(void *font,void *dc,uint32_t string,struct fs98_sim_rect rect,int32_t flags){draw_text(font,dc,string,rect,flags);}
static void TC hook_notify_destroy(void *object){destroy(object,0xd54c);}
static int32_t TC hook_init_slider(void *control,uint32_t first_bitmap,uint32_t second_bitmap,void *dib,void *palette,
    uint32_t first,uint32_t second,uint32_t third,uint32_t fourth,int32_t x,int32_t y,uint32_t module)
{ return init_slider(control,first_bitmap,second_bitmap,dib,palette,first,second,third,fourth,x,y,module); }
static void TC hook_init_group(void *control,void *font){init_group(control,font);}
static void TC hook_init_combo(void *control,void *font,void *label){init_combo(control,font,label);}
static void TC hook_control_text(void *control,const char *value){control_text(control,value);}
static int32_t TC hook_get_text(void *control,char *buffer,int32_t capacity){return get_text(control,buffer,capacity);}
static int32_t TC hook_error_dialog(void *object,void *control,void *descriptor){return error_dialog(object,control,descriptor);}
static int TC hook_fuel_is_enabled(void *control){return fuel_is_enabled(control);}
static int SC hook_fuel_window_control_id(uint32_t window){return fuel_window_control_id(window);}
static uint32_t SC hook_fuel_host_string(uint32_t id,char **text_out){return fuel_host_string(id,text_out);}
static uint32_t SC hook_fuel_host_length(const char *text){return fuel_host_length(text);}
static int SC hook_fuel_host_alloc(int flags,uint32_t size,void **data_out,int zero){return fuel_host_alloc(flags,size,data_out,zero);}
static int SC hook_fuel_host_realloc(int flags,uint32_t size,void **data_out){return fuel_host_realloc(flags,size,data_out);}
static int SC hook_fuel_host_free(void **data_out){return fuel_host_free(data_out);}
static char *SC hook_fuel_string_copy(char *destination,const char *source){return fuel_string_copy(destination,source);}
static char *SC hook_fuel_string_cat(char *destination,const char *source){return fuel_string_cat(destination,source);}
static int SC hook_fuel_string_length(const char *text){return fuel_string_length(text);}
static void *s_host_services[0x2bc/4];
static void TC hook_settings_init_tabs(void *tabs,void *parent,void *font,uint32_t bitmap,
    void *dib,void *palette,uint32_t flags,uint32_t color,int inset,int height,int width,
    int selection,uint32_t module,int last)
{ settings_init_tabs(tabs,parent,font,bitmap,dib,palette,flags,color,inset,height,width,selection,module,last); }
static void TC hook_settings_init_page(void *page,uint32_t flags){settings_init_page(page,flags);}
static void TC hook_settings_add_page(void *tabs,void *page,const char *label,uint32_t available){settings_add_page(tabs,page,label,available);}
static void TC hook_settings_init_tab_helper(void *helper,void *tabs){settings_init_tab_helper(helper,tabs);}
static int TC hook_settings_modify_style(void *control,uint32_t remove,uint32_t add,uint32_t flags){return settings_modify_style(control,remove,add,flags);}
static int SC hook_settings_window_rect(uint32_t window,struct fs98_sim_rect *rect){return settings_window_rect(window,rect);}
static int SC hook_settings_screen_to_client(uint32_t window,int *point){return settings_screen_to_client(window,point);}
static void TC hook_settings_last_page(void *tabs,void *page){settings_last_page(tabs,page);}
static void TC hook_settings_active_tab(void *tabs,uint32_t index,int focus){settings_active_tab(tabs,index,focus);}
static int TC hook_settings_message_box(void *object,const char *text,const char *caption,uint32_t flags){return settings_message_box(object,text,caption,flags);}
static void *SC hook_base_font(int32_t id){return base_font(id);}
static void TC hook_init_edit(void *control,void *font,uint32_t foreground,uint32_t background,uint32_t error_color,void *label,uint32_t flags)
{init_edit(control,font,foreground,background,error_color,label,flags);}
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
    patch(0xd508,hook_radio_check);patch(0xd4ac,hook_active_page);patch(0xd5d0,hook_focus);patch(0xd368,hook_from_handle);
    patch(0xd374,(void *)0x12345678);patch(0xd378,hook_is_edit);patch(0xd36c,hook_control_id);
    patch(0xd550,hook_base_init);patch(0xd554,hook_init_button);patch(0xd558,hook_init_art);patch(0xd55c,hook_init_text);
    patch(0xd560,hook_notify_construct);patch(0xd568,hook_notify_delete);patch(0xd544,hook_notify_send);patch(0xd3ac,hook_get_control);
    patch(0xd564,hook_remap_rect);patch(0xd3b0,hook_move_control);patch(0xd3a0,hook_paint_construct);patch(0xd3a8,hook_paint_destroy);
    patch(0xd5f0,hook_load_string);patch(0xd5f4,hook_invalidate);patch(0xd5f8,hook_set_rect);patch(0xd3a4,hook_string_text);patch(0xd540,hook_draw_text);
    patch(0xd37c,hook_get_text);patch(0xd4dc,hook_error_dialog);patch(0xd524,hook_base_font);patch(0xd4e0,hook_init_edit);
    patch(0xd4ec,hook_init_group);patch(0xd4f0,hook_init_combo);patch(0xd384,hook_control_text);
    patch(0xd4cc,hook_init_slider);s_widget_vtable[0xc4/4]=widget_init;
    patch(0xd380,hook_fuel_is_enabled);patch(0xd5e0,hook_fuel_window_control_id);
    patch(0xd33c,hook_fuel_string_cat);patch(0xd340,hook_fuel_string_copy);
    patch(0xd344,hook_fuel_string_length);patch(0xd488,fuel_parse_long);
    s_host_services[0x2a4/4]=hook_fuel_host_string;s_host_services[0x2b8/4]=hook_fuel_host_length;
    s_host_services[0x20c/4]=hook_fuel_host_alloc;s_host_services[0x210/4]=hook_fuel_host_realloc;
    s_host_services[0x214/4]=hook_fuel_host_free;patch(0xc05c,s_host_services);
    patch(0xd528,hook_settings_init_tabs);patch(0xd534,hook_settings_init_page);
    patch(0xd5bc,hook_settings_add_page);patch(0xd4b4,hook_settings_init_tab_helper);
    patch(0xd364,hook_settings_modify_style);patch(0xd5dc,hook_settings_window_rect);
    patch(0xd5d4,hook_settings_screen_to_client);patch(0xd4bc,hook_settings_last_page);patch(0xd4a4,hook_settings_active_tab);
    patch(0xd370,hook_settings_message_box);
    s_vtable[0xe8/4]=settings_virtual_art;s_other_vtable[0xe8/4]=settings_other_virtual_art;
    patch(0xd54c,hook_notify_destroy);patch(0xc130,(void *)0x27182818);
    fs98_sim_set_crash_tables((uint8_t *)s_retail+0xa1b8);
    s_vtable[0xe4/4]=virtual_skin;s_other_vtable[0xe4/4]=virtual_other_skin;
    s_vtable[0xe0/4]=virtual_validate;s_other_vtable[0xe0/4]=virtual_other_validate;
    s_page_vtable[0xd8/4]=virtual_validate;s_page_vtable[0xdc/4]=virtual_error;
    s_other_page_vtable[0xd8/4]=virtual_other_validate;s_other_page_vtable[0xdc/4]=virtual_other_error;
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
    add(0x21b0,1,37,"damage.commit");add(0x22c0,1,38,"damage.populate");add(0x7300,7,39,"settings.accept");
    add(0x19c0,0,40,"crash.validate");add(0x1750,0,41,"crash.init");add(0x1a00,0,42,"crash.paint");
    add(0x1910,0,43,"notify.delete");add(0x24b0,1,44,"static.delete");add(0x1410,0,45,"class.delete");
    add(0x1fe0,1,46,"damage.init");add(0x6dc0,6,47,"reliability.init");
    add(0x2b90,2,48,"engines.init");add(0x3010,2,49,"engines.populate");
    add(0x5990,4,50,"systems.init");add(0x5c90,4,51,"systems.commit");add(0x5e30,4,52,"systems.populate");
    add(0x65e0,5,53,"realism.init");add(0x6a70,5,54,"realism.populate");
    add(0x6860,5,55,"realism.validate.all");add(0x6860,5,56,"realism.validate.first");add(0x6860,5,57,"realism.validate.second");
    add(0x69e0,5,58,"realism.error");
    add(0x5400,3,59,"fuel.selection");add(0x53d0,3,60,"fuel.selector.changed");
    add(0x5390,3,61,"fuel.manual");add(0x4e60,3,62,"fuel.error");
    for(uint32_t edit=0;edit<15;++edit)add(0x4750,3,63+edit,"fuel.validate.edit");
    add(0x4750,3,78,"fuel.validate.unknown");add(0x4750,3,79,"fuel.validate.zero");
    add(0x4f10,3,80,"fuel.populate");add(0x4080,3,81,"fuel.init");
    add(0x7990,7,82,"settings.wire");add(0x7b00,7,83,"settings.validate");add(0x7460,7,84,"settings.init");
    add(0x71d0,7,85,"settings.placeholder");
    add(0x1d20,0,33,"empty");add(0x1d30,0,34,"empty.arg");add(0x71f0,0,35,"zero.arg");add(0x6bb0,5,36,"zero.message");
}
static void initialize(void)
{
    uint32_t i;void *object=s_fixture.object;
    for(i=0;i<sizeof(s_fixture)/4;++i)((uint32_t *)&s_fixture)[i]=next_random();
    s_kind=s_operations[s_operation].kind;
    s_lifetime=s_operations[s_operation].signature<5 || s_operations[s_operation].signature==8;
    s_settings_init=s_operations[s_operation].signature==84;s_fixture.page_blocks=0;
    s_descriptor_offset=s_kind==0?0x112c:s_kind==1||s_kind==4||s_kind==6?0x1118:s_kind==7?0x10ec:s_kind==3?0x1120:0x111c;
    PTR(object,0)=s_vtable;
    {static const uint16_t slots[]={0x17c8,0x1890,0x13b4,0x1570,0x1664,0x115c,0x1224,0x12ec,0x14a8,0x1dbc,0x1cf4,0x1c2c,0x1b64,0x1a9c,0x19d4,0x190c,0x1844,0x1fb4,0x1eec,0x1e24,0x1d5c,0x1c94,0x1bcc,0x1b04,0x1a3c,0x20c4,0x1ffc,0x1f34,0x1e6c};
     for(i=0;i<sizeof(slots)/sizeof(slots[0]);++i)PTR(object,slots[i])=s_widget_vtable;}
    s_fixture.resource_module=0x27182818;PTR(object,s_descriptor_offset)=s_fixture.descriptor[0];
    PTR(object,0x10f0)=s_scenario&1?s_fixture.notify[0]:0;
    PTR(object,0x1118)=PTR(object,0x111c)=s_fixture.descriptor[0];
    D32(object,0x1100)=s_scenario%3?1:0;
    PTR(object,0x12e8)=s_tabs_vtable;PTR(object,0x2248)=s_scenario&2?s_fixture.notify[0]:0;
    PTR(object,0x2258)=s_fixture.pages[0];PTR(object,0x225c)=s_fixture.pages[1];
    for(i=0;i<3;++i)PTR(s_fixture.notify[i],0)=s_notify_vtable;
    for(i=0;i<2;++i){
        void *d=s_fixture.descriptor[i];
        PTR(s_fixture.pages[i],0)=s_page_vtable;static const uint16_t counts[]={0,1,2,3,4,5,0xffff,0x8000};
        U16(d,0x24)=counts[s_scenario%8];
        snprintf(s_fixture.strings[i][0],32,"airspeed %u/%u",i,s_scenario);
        snprintf(s_fixture.strings[i][1],32,"vertical %x",s_seed);
        PTR(d,0x14)=s_fixture.strings[i][0];PTR(d,0x18)=s_fixture.strings[i][1];PTR(d,8)=graph_callback;
        PTR(s_fixture.pages[i],0x1124)=s_scenario&4?s_fixture.notify[1]:0;
        PTR(s_fixture.pages[i],0x1130)=s_scenario&8?s_fixture.notify[2]:0;
    }
    if(s_kind==1){D32(s_fixture.descriptor[0],8)=D32(s_fixture.descriptor[1],8)=s_scenario%4;}
    if(s_kind==4){PTR(object,0x111c)=s_fixture.descriptor[1];D32(s_fixture.descriptor[1],0x18)=s_seed;}
    if(s_kind==3){
        uint32_t row,group;
        static const uint32_t s_tank_indices[]={4,2,3,5,6};
        s_fixture.host_blocks=0;PTR(object,0x113c)=s_widget_vtable;
        for(group=0;group<5;++group)D32(object,0x2918+group*4)=s_tank_indices[group];
        PTR(object,0x1124)=s_fixture.settings_data[0];D32(object,0x111c)=s_seed;
        for(i=0;i<2;++i){void *helper=s_fixture.settings_data[i];void *data=s_fixture.descriptor[i];
            PTR(helper,4)=fuel_validate_numbers;D32(helper,0x20)=(s_scenario/4)%6;D32(helper,0x24)=(s_scenario/7)%7;
            D32(helper,0xc)=s_scenario%2;PTR(data,0x14)=s_fixture.strings[i][0];
            for(row=0;row<5;++row){
                snprintf(s_fixture.fuel_capacities[i][row],32,"%d",(s_scenario+row)%3?(int)(s_scenario+row):0);
                PTR(data,0x144+row*4)=s_fixture.fuel_capacities[i][row];
            }
            PTR(data,0x158)=fuel_convert_first;PTR(data,0x15c)=fuel_convert_second;PTR(data,0x160)=fuel_convert_third;
            for(group=0;group<3;++group)for(row=0;row<5;++row){
                uint32_t field=0x1c+group*0x64+row*0x14;
                snprintf(s_fixture.fuel_buffers[i][group][row],64,"fuel%u/%u/%u",i,group,row);
                PTR(data,field)=s_fixture.fuel_buffers[i][group][row];D32(data,field+4)=(s_scenario+row)%3?64:0;
            }
        }
        for(row=0;row<8;++row)D32(object,0x28f8+row*4)=(s_scenario>>(row%5))%4;
        for(row=0;row<5;++row){
            for(group=0;group<3;++group){
                PTR(object,0x2858+group*0x14+row*4)=s_fixture.pages[0]+group*0x300+row*0x80;
                PTR(object,0x28bc+group*0x14+row*4)=s_fixture.pages[1]+group*0x300+row*0x80;
                D32(s_fixture.descriptor[0],0x18+group*0x64+row*0x14)=(s_scenario+group+row)%7==0;
                D32(s_fixture.descriptor[1],0x18+group*0x64+row*0x14)=(s_scenario+group+row)%4==0;
            }
            PTR(object,0x2894+row*4)=s_fixture.pages[1]+0x900+row*0x80;
            PTR(object,0x28a8+row*4)=s_fixture.pages[0]+0x900+row*0x80;
        }
    }
    if(s_kind==5){
        static const uint32_t capacities[]={0,1,2,7,32,64};
        PTR(object,0x111c)=s_fixture.descriptor[0];PTR(object,0x1120)=s_fixture.settings_data[0];D32(object,0x1118)=s_scenario%3;
        for(i=0;i<2;++i){void *d=s_fixture.descriptor[i];void *h=s_fixture.settings_data[i];
            PTR(h,4)=validate_numbers;D32(h,0x20)=(s_scenario/4)%6;
            D32(h,0x10)=(s_scenario+i)%4;D32(h,0x14)=s_scenario&1;D32(h,0x1c)=s_seed;
            snprintf(s_fixture.number_buffers[i][0],64,"first%u.%u",i,s_scenario);
            snprintf(s_fixture.number_buffers[i][1],64,"second%u.%u",i,s_scenario);
            PTR(d,0x48)=s_fixture.number_buffers[i][0];D32(d,0x4c)=capacities[s_scenario%6];
            PTR(d,0x5c)=s_fixture.number_buffers[i][1];D32(d,0x60)=capacities[(s_scenario+2)%6];
            PTR(d,0x3c)=s_fixture.strings[i][0];PTR(d,0x40)=s_fixture.strings[i][1];
            D32(d,0x44)=(s_scenario+i)%3==0;D32(d,0x58)=(s_scenario+i)%5==0;
            PTR(d,0x6c)=i?number_to_first_other:number_to_first;PTR(d,0x70)=i?number_to_second_other:number_to_second;
        }
    }
    if(s_kind==6){PTR(object,0x111c)=s_fixture.descriptor[1];}
    if(s_kind==7 && s_operations[s_operation].signature>=82){
        for(i=0;i<6;++i)PTR(object,0x224c+i*4)=s_fixture.pages[i];
        if(s_operations[s_operation].signature!=82 && s_scenario%13==0)PTR(object,0x10ec)=0;
    }
    if(s_kind==2){PTR(object,0x1120)=s_fixture.descriptor[1];
        for(i=0;i<2;++i){D32(s_fixture.descriptor[i],0x58)=(s_scenario>>2)&1;D32(s_fixture.descriptor[i],0x6c)=(s_scenario>>3)&1;}
        D32(s_fixture.descriptor[1],0x10)=s_scenario&1;D32(object,0x1db0)=(s_scenario/8)&1;D32(object,0x1ce8)=(s_scenario/8)&2;D32(object,0x1c20)=(s_scenario/8)&4;D32(object,0x1b58)=(s_scenario/8)&8;D32(object,0x1e78)=s_scenario%3;}
    if(s_kind==0){
        uint32_t axis,j;
        for(axis=0;axis<2;++axis)for(j=0;j<4;++j){snprintf(s_fixture.axes[axis][j],24,"axis%u label%u",axis,j);s_fixture.labels[axis][j]=s_fixture.axes[axis][j];}
        for(i=0;i<2;++i){void *d=s_fixture.descriptor[i];
            D32(d,0x10)=(s_scenario/3)%3;D32(d,0x1c)=2+s_scenario%3;D32(d,0x24)=2+(s_scenario/3)%3;
            PTR(d,0x20)=s_fixture.labels[0];PTR(d,0x28)=s_fixture.labels[1];D32(d,0x2c)=s_scenario&1;
        }
        D32(object,0x10e0)=s_seed;D32(object,0x10e8)=s_seed+(s_scenario&1?0xf1234567:300);
        D32(object,0x10e4)=20;D32(object,0x10ec)=220;
        D32(object,0x10f4)=s_scenario%3;
        strcpy((char *)object+0x10f8,"vertical axis");strcpy((char *)object+0x1116,"time axis");
        if(s_operations[s_operation].signature==42 && s_scenario%7==1 && s_scenario%3!=1)
            D32(s_fixture.descriptor[0],0x24)=D32(s_fixture.descriptor[1],0x24)=1;
        if((s_operations[s_operation].signature==40 || s_operations[s_operation].signature==41) && s_scenario%3)
            PTR(object,0x112c)=s_scenario%3==1?0:(void *)0xcdcdcdcd;
    }
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
    if(s_operations[s_operation].signature==42 && e->ExceptionRecord->ExceptionCode==EXCEPTION_INT_DIVIDE_BY_ZERO){
        s_throw_at=0;put("divide.fault code=%x\n",(uint32_t)e->ExceptionRecord->ExceptionCode);
        e->ContextRecord->Eip=(DWORD)land;return EXCEPTION_CONTINUE_EXECUTION;
    }
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
    s_fixture=s_initial;D32(s_retail,0xc130)=s_fixture.resource_module;fs98_sim_set_resource_module(s_fixture.resource_module);s_calls=s_log_length=0;s_logs[s_side][0]=0;s_throw_at=throw_at;
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
        else if(signature==27){int32_t selection=s_scenario%6==5?(int32_t)s_seed:(int32_t)(s_scenario%5)-1;result=s_side?fs98_sim_system_selection(object,selection):((int32_t (TC *)(void *,int32_t))entry)(object,selection);}
        else if(signature>=28 && signature<=33){
            static void (*const ours[])(void *)={fs98_sim_settings_cancel,fs98_sim_settings_close,fs98_sim_post_destroy,fs98_sim_disable,fs98_sim_enable,fs98_sim_empty};
            if(signature==30 && s_scenario%2==0)object=0;
            if(s_side)ours[signature-28](object);else((void (TC *)(void *))entry)(object);
        }else if(signature==34){if(s_side)fs98_sim_empty_arg(object,s_seed);else((void (TC *)(void *,uint32_t))entry)(object,s_seed);}
        else if(signature==35){result=s_side?fs98_sim_zero(object,s_seed):((int32_t (TC *)(void *,uint32_t))entry)(object,s_seed);}
        else if(signature==36){result=s_side?fs98_sim_zero_message(object,s_seed,s_seed^1):((int32_t (TC *)(void *,uint32_t,uint32_t))entry)(object,s_seed,s_seed^1);}
        else if(signature==37){result=s_side?fs98_sim_damage_commit(object,s_seed):((int32_t (TC *)(void *,uint32_t))entry)(object,s_seed);}
        else if(signature==38){if(s_side)fs98_sim_damage_populate(object);else((void (TC *)(void *))entry)(object);}
        else if(signature==39){if(s_side)fs98_sim_settings_accept(object);else((void (TC *)(void *))entry)(object);}
        else if(signature==40){result=s_side?fs98_sim_crash_validate(object):((int32_t (TC *)(void *))entry)(object);}
        else if(signature==41){result=s_side?fs98_sim_crash_init(object):((int32_t (TC *)(void *))entry)(object);}
        else if(signature==42){if(s_side)fs98_sim_crash_paint(object);else((void (TC *)(void *))entry)(object);}
        else if(signature==43 || signature==44){void *returned;
            if(s_side)returned=signature==43?fs98_sim_notify_delete(object,s_seed):fs98_sim_static_delete(object,s_seed);
            else returned=((void *(TC *)(void *,uint32_t))entry)(object,s_seed);
            put("return.object %u\n",offset(returned));}
        else if(signature==45){if(s_side)fs98_sim_class_delete(object);else((void (SC *)(void *))entry)(object);}
        else if(signature==46){if(s_side)fs98_sim_damage_init(object);else((void (TC *)(void *))entry)(object);}
        else if(signature==47){if(s_side)fs98_sim_reliability_init(object);else((void (TC *)(void *))entry)(object);}
        else if(signature==48){if(s_side)fs98_sim_engines_init(object);else((void (TC *)(void *))entry)(object);}
        else if(signature==49){if(s_side)fs98_sim_engines_populate(object);else((void (TC *)(void *))entry)(object);}
        else if(signature==50){if(s_side)fs98_sim_systems_init(object);else((void (TC *)(void *))entry)(object);}
        else if(signature==51){result=s_side?fs98_sim_systems_commit(object,s_seed):((int32_t (TC *)(void *,uint32_t))entry)(object,s_seed);}
        else if(signature==52){if(s_side)fs98_sim_systems_populate(object);else((void (TC *)(void *))entry)(object);}
        else if(signature==53){if(s_side)fs98_sim_realism_init(object);else((void (TC *)(void *))entry)(object);}
        else if(signature==54){if(s_side)fs98_sim_realism_populate(object);else((void (TC *)(void *))entry)(object);}
        else if(signature>=55 && signature<=57){uint32_t id=signature==56?0x13bd:signature==57?0x13be:s_scenario%2?s_seed:0;result=s_side?fs98_sim_realism_validate(object,id):((int32_t (TC *)(void *,uint32_t))entry)(object,id);}
        else if(signature==58){int32_t display=s_scenario%4?(int32_t)s_seed:0;result=s_side?fs98_sim_realism_error(object,display):((int32_t (TC *)(void *,int32_t))entry)(object,display);}
        else if(signature==59){int selection=s_scenario%11==10?(int)s_seed:(int)(s_scenario%11)-1;result=s_side?fs98_sim_fuel_selection(object,selection):((int (TC *)(void *,int))entry)(object,selection);}
        else if(signature==60){if(s_side)fs98_sim_fuel_selector_changed(object);else((void (TC *)(void *))entry)(object);}
        else if(signature==61){if(s_side)fs98_sim_fuel_manual(object);else((void (TC *)(void *))entry)(object);}
        else if(signature==62){int display=s_scenario%3?(int)s_seed:0;result=s_side?fs98_sim_fuel_error(object,display):((int (TC *)(void *,int))entry)(object,display);}
        else if(signature>=63 && signature<=79){uint32_t id=signature==79?0:signature==78?(s_scenario%2?s_seed:0x13d0):s_fuel_edits[(signature-63)/5][(signature-63)%5];result=s_side?fs98_sim_fuel_validate(object,id):((int (TC *)(void *,uint32_t))entry)(object,id);}
        else if(signature==80){if(s_side)fs98_sim_fuel_populate(object);else((void (TC *)(void *))entry)(object);}
        else if(signature==81){if(s_side)fs98_sim_fuel_init(object);else((void (TC *)(void *))entry)(object);}
        else if(signature==82){if(s_side)fs98_sim_settings_wire(object);else((void (TC *)(void *))entry)(object);}
        else if(signature==83){result=s_side?fs98_sim_settings_validate(object):((int (TC *)(void *))entry)(object);}
        else if(signature==84){result=s_side?fs98_sim_settings_init(object):((int (TC *)(void *))entry)(object);}
        else if(signature==85){result=s_side?fs98_sim_settings_placeholder(object,s_seed):((int (TC *)(void *,uint32_t))entry)(object,s_seed);}
    }
    __asm__ volatile("movl %0, %%fs:0"::"r"(s_saved_seh):"memory");
    s_fixture.resource_module=s_side?g_sim_resource_module:D32(s_retail,0xc130);
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
