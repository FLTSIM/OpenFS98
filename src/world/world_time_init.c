/* Parent tab ownership and the exact control initialization order of both pages. */
#include "world_time.h"
#include "world_unwind.h"
#define AT(p,n) ((uint8_t *)(p)+(n))
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define API(slot,type) ((type)services->resolve(slot))
struct time_allocation_unwind {struct fs98_world_unwind frame;void *object;volatile int live;uint32_t cleanup;};
static void time_allocation_cleanup(struct fs98_world_unwind *frame)
{
    struct time_allocation_unwind *state=(void *)frame;
    if(state->live){state->live=0;((void(SC *)(void *))fs98_world_time_api(state->cleanup))(state->object);}
}
static WORLD_NOINLINE void *time_allocate_page(void *parent,uint32_t kind)
{
    const struct fs98_world_time_services *services=fs98_world_time_get_services();
    struct time_allocation_unwind state;void *object;
    state.object=NULL;state.live=0;state.cleanup=kind?0x25850:0x257f4;
    world_unwind_enter(&state.frame,time_allocation_cleanup);
    object=API(0x2584c,void *(*)(uint32_t))(kind==1?0x1c08:kind==2?0x1df0:0x29c);
    state.object=object;state.live=1;
    if(object){
        if(kind)object=fs98_world_time_construct(object,parent,kind);
        else {API(0x257cc,void *(TC *)(void *,uint32_t,uint32_t))(object,0x30,1);PTR(object,0)=services->notify_vtable;}
    }
    state.live=0;world_unwind_leave(&state.frame);return object;
}
WORLD_NOINLINE int fs98_world_time_init(void *object)
{
    const struct fs98_world_time_services *services=fs98_world_time_get_services();
    const struct fs98_world_init_services *ui=services->ui;
    struct fs98_world_rect rect={0,0,0,0};void **vtable;void *page,*tab=AT(object,0x1450);char title[29];
    ui->base_init(object);vtable=PTR(object,0);
    if(!((int(TC *)(void *,uint32_t,uint32_t,uint32_t))vtable[0xe4/4])(object,0x14f,services->resources(),15))return 0;
    ui->art(AT(object,0x1368),0xf831,AT(object,0xff4),AT(object,0x7e8),2);
    ui->button(AT(object,0x126c),AT(object,0x5a0),AT(object,0x610),0x132,AT(object,0xff4),AT(object,0x7e8),1,-1,services->resources(),0,0,rect);
    ui->button(AT(object,0x1170),AT(object,0x5a0),AT(object,0x610),0x132,AT(object,0xff4),AT(object,0x7e8),1,-1,services->resources(),0,0,rect);
    API(0x257d4,int(TC *)(void *,void *,void *,uint32_t,void *,void *,uint32_t,uint32_t,int,int,int,int,uint32_t,uint32_t))(
        tab,object,ui->base_font(0),0x148,AT(object,0xff4),AT(object,0x7e8),0,0xc4c4c4,5,0x1d,0x47,-1,services->resources(),0);
    ((void(TC *)(void *,uint32_t,int,int,int,int,uint32_t))vtable[0xe8/4])(object,0x150,0x1d,0x47,0x1a8,0x121,services->resources());
    page=time_allocate_page(object,1);PTR(object,0x2118)=page;API(0x257d8,int(TC *)(void *,int))(page,9);
    page=time_allocate_page(object,2);PTR(object,0x211c)=page;API(0x257d8,int(TC *)(void *,int))(page,9);
    PTR(object,0x2128)=time_allocate_page(object,0);
    if(!fs98_world_time_has_descriptor(object))return 0;
    fs98_world_time_bind_pages(object);
    ui->load_string(U32(object,0x80),0xf927,title,29);
    API(0x257e4,void(TC *)(void *,void *,char *,int))(tab,PTR(object,0x211c),title,(int)U32(PTR(object,0x10dc),0x10));
    ui->load_string(U32(object,0x80),0xf925,title,29);
    API(0x257e4,void(TC *)(void *,void *,char *,int))(tab,PTR(object,0x2118),title,(int)U32(PTR(object,0x10dc),0x74));
    API(0x257e8,void(TC *)(void *,void *))(AT(object,0x1128),tab);
    API(0x257e8,void(TC *)(void *,void *))(AT(object,0x10e0),tab);
    API(0x257ec,int(TC *)(void *,void *))(tab,PTR(object,0x2118));
    API(0x257f0,int(TC *)(void *,int,uint32_t))(tab,0,0);
    API(0x25500,int(TC *)(void *,int))(object,1);
    API(0x257f0,int(TC *)(void *,int,uint32_t))(tab,0,0);
    PTR(object,0x7c)=tab;return 1;
}
static void time_page_send(void *object,uint32_t control,uint32_t message,uint32_t value)
{
    const struct fs98_world_time_services *services=fs98_world_time_get_services();
    API(0x258b0,int(SC *)(uint32_t,uint32_t,uint32_t,uint32_t))(U32(object,control+0x20),message,value,0);
}
static void time_init_edit(void *object,uint32_t label,uint32_t control)
{
    const struct fs98_world_time_services *services=fs98_world_time_get_services();
    services->ui->edit(AT(object,control),services->ui->base_font(2),0xffffff,0,0xff0000,AT(object,label),1);
}
static void time_init_radio(void *object,uint32_t control,uint32_t bitmap,uint32_t font)
{
    const struct fs98_world_time_services *services=fs98_world_time_get_services();void *child=AT(object,control);void **vtable=PTR(child,0);
    ((int(TC *)(void *,void *,uint32_t,uint32_t,uint32_t,uint32_t))vtable[0xc4/4])(child,AT(object,font),bitmap,U32(object,0x10fc),U32(object,0x10dc),services->resources());
}
void fs98_world_time_page_init(void *object,uint32_t kind)
{
    const struct fs98_world_time_services *services=fs98_world_time_get_services();const struct fs98_world_init_services *ui=services->ui;
    uint32_t index,descriptor=kind==1?0x1118:0x111c;struct fs98_world_rect rect;void *separator;
    static const uint32_t season_labels[]={0x11e8,0x12a0,0x1358},season_edits[]={0x1a48,0x1ab0,0x1b18};
    static const uint32_t clock_text[]={0x1130,0x11e8,0x12a0,0x1358,0x1410,0x1580,0x16f0,0x1860,0x14c8,0x1638,0x17a8};
    static const uint32_t clock_labels[]={0x11e8,0x12a0,0x1358,0x1410},clock_edits[]={0x1b00,0x1b68,0x1bd0,0x1c38};
    if(kind==1){
        for(index=0;index<3;++index){ui->text(AT(object,season_labels[index]),AT(object,0x220));time_init_edit(object,season_labels[index],season_edits[index]);}
        time_init_radio(object,0x1860,0x134,0x530);time_init_radio(object,0x1954,0x134,0x530);
        ui->text(AT(object,0x1130),AT(object,0x370));ui->combo(AT(object,0x1b80),AT(object,0x3e0),AT(object,0x1130));time_page_send(object,0x1b80,0x14e,0);
        ui->text(AT(object,0x14c8),AT(object,0x220));ui->text(AT(object,0x1638),AT(object,0x220));ui->text(AT(object,0x17a8),AT(object,0x220));
        ui->text(AT(object,0x1410),AT(object,0x530));ui->text(AT(object,0x1580),AT(object,0x530));ui->text(AT(object,0x16f0),AT(object,0x530));
        separator=AT(object,0x1bc8);
    }else{
        for(index=0;index<11;++index)ui->text(AT(object,clock_text[index]),AT(object,index==0?0x370:index<8?0x220:0x530));
        for(index=0;index<4;++index)time_init_edit(object,clock_labels[index],clock_edits[index]);
        time_init_radio(object,0x1918,0x134,0x530);time_init_radio(object,0x1a0c,0x134,0x530);time_init_radio(object,0x1ce8,0x139,0x4c0);
        ui->combo(AT(object,0x1ca0),AT(object,0x3e0),AT(object,0x1130));time_page_send(object,0x1ca0,0x14e,0);separator=AT(object,0x1db0);
    }
    API(0x25744,void(SC *)(void *,void *))(&rect,separator);
    API(0x2559c,void(TC *)(void *,int,int,int,int,int))(separator,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,1);
    if(!PTR(object,descriptor)||U32(object,descriptor)==0xcdcdcdcd){
        API(0x2551c,int(SC *)(const char *,uint32_t,uint32_t))(kind==1?"Bad m_pliveInfo In CSDate InitControls":"Bad m_pliveInfo",0,0);return;
    }
    if(kind==1){
        time_page_send(object,0x1ab0,0xc5,U32(PTR(object,descriptor),0x14)-1);
        time_page_send(object,0x1b18,0xc5,U32(PTR(object,descriptor),0x28)-1);
        time_page_send(object,0x1a48,0xc5,U32(PTR(object,descriptor),0x3c)-1);
    }else{
        time_page_send(object,0x1c38,0xc5,U32(PTR(object,descriptor),0x6c)-1);
        time_page_send(object,0x1bd0,0xc5,U32(PTR(object,descriptor),0x80)-1);
        time_page_send(object,0x1b68,0xc5,U32(PTR(object,descriptor),0x44)-1);
        time_page_send(object,0x1b00,0xc5,U32(PTR(object,descriptor),0x58)-1);
    }
    fs98_world_time_populate(object,kind);
}
