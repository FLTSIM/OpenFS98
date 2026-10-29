/* Advanced Weather parent, Visibility and Pressure behavior. */
#include "world_advanced_internal.h"
#include "world_unwind.h"
static const struct fs98_world_advanced_services *s_services;
void fs98_world_advanced_set_services(const struct fs98_world_advanced_services *services)
{ s_services=services;fs98_world_advanced_set_resolver(services->resolve); }
const struct fs98_world_advanced_services *fs98_world_advanced_services(void) { return s_services; }
void *fs98_world_advanced_api(uint32_t slot) { return s_services->resolve(slot); }
struct advanced_allocation {
    struct fs98_world_unwind frame;
    void *object;
    uint32_t cleanup;
    volatile int live;
};
static void advanced_allocation_cleanup(struct fs98_world_unwind *frame)
{
    struct advanced_allocation *state=(void *)frame;
    if(state->live){state->live=0;API(state->cleanup,void(SC *)(void *))(state->object);}
}
WORLD_NOINLINE void *fs98_world_advanced_allocate(void *parent,uint32_t kind)
{
    struct advanced_allocation state;
    void *object;
    state.object=NULL;state.live=0;state.cleanup=kind?0x25850:0x257f4;
    world_unwind_enter(&state.frame,advanced_allocation_cleanup);
    object=API(0x2584c,void *(*)(uint32_t))(kind?fs98_world_advanced_class(kind)->size:0x29c);
    state.object=object;state.live=1;
    if(object){
        if(kind)object=fs98_world_advanced_construct(object,parent,kind);
        else {API(0x257cc,void *(TC *)(void *,uint32_t,uint32_t))(object,0x30,1);PTR(object,0)=s_services->notify_vtable;}
    }
    state.live=0;world_unwind_leave(&state.frame);return object;
}
static uint32_t advanced_parent_init(void *object)
{
    const struct fs98_world_init_services *ui=s_services->ui;
    struct fs98_world_rect rect={0,0,0,0};void **vtable;void *page,*tab=AT(object,0x1690);char title[29];uint32_t index;
    static const uint32_t buttons[]={0x2440,0x253c,0x1498,0x1594};
    static const uint32_t page_offsets[]={0x263c,0x2648,0x2640,0x2644,0x264c};
    static const uint32_t titles[]={0xf923,0xf929,0xf926,0xf928,0xf924};
    static const uint32_t tabs[]={0x13c0,0x1378,0x1330,0x1408,0x1450};
    ui->base_init(object);vtable=PTR(object,0);
    if(!((int(TC *)(void *,uint32_t,uint32_t,uint32_t))vtable[0xe4/4])(object,0x14f,s_services->resources(),15))return 0;
    ui->art(AT(object,0x2358),0xf831,AT(object,0xff4),AT(object,0x7e8),2);
    for(index=0;index<4;++index)
        ui->button(AT(object,buttons[index]),AT(object,index<2?0x5a0:0x680),AT(object,index<2?0x610:0x6f0),index<2?0x132:0x137,AT(object,0xff4),AT(object,0x7e8),1,-1,s_services->resources(),index<2?0:1,index<2?0:1,rect);
    PTR(object,0x2638)=fs98_world_advanced_allocate(object,0);
    API(0x257d4,int(TC *)(void *,void *,void *,uint32_t,void *,void *,uint32_t,uint32_t,int,int,int,int,uint32_t,uint32_t))(
        tab,object,ui->base_font(0),0x148,AT(object,0xff4),AT(object,0x7e8),0,0xc4c4c4,5,0x1d,0x47,-1,s_services->resources(),0);
    ((void(TC *)(void *,uint32_t,int,int,int,int,uint32_t))vtable[0xe8/4])(object,0x150,0x1d,0x47,0x1a8,0x121,s_services->resources());
    for(index=1;index<6;++index){page=fs98_world_advanced_allocate(object,index);PTR(object,0x2638+index*4)=page;API(0x257d8,int(TC *)(void *,int))(page,9);}
    if(!RUN(0x2cc0,0,0))return 0;
    if(U32(PTR(object,0x1104),0x14))advanced_label(object,0x1278,0x220);else API(0x25500,int(TC *)(void *,int))(AT(object,0x1278),0);
    if(U32(PTR(object,0x1104),0x18))advanced_label(object,0x11c0,0x220);else API(0x25500,int(TC *)(void *,int))(AT(object,0x11c0),0);
    advanced_label(object,0x1108,0x220);advanced_title(object,0x1108,PTR(PTR(object,0x1104),8));
    RUN(0x2a20,0,0);
    for(index=0;index<5;++index){
        ui->load_string(U32(object,0x80),titles[index],title,29);
        API(0x257e4,void(TC *)(void *,void *,char *,int))(tab,PTR(object,page_offsets[index]),title,0);
    }
    for(index=0;index<5;++index)API(0x257e8,void(TC *)(void *,void *))(AT(object,tabs[index]),tab);
    API(0x257ec,int(TC *)(void *,void *))(tab,PTR(object,0x264c));
    API(0x257f0,int(TC *)(void *,int,uint32_t))(tab,0,0);
    API(0x25500,int(TC *)(void *,int))(object,1);PTR(object,0x7c)=tab;return 1;
}
static void advanced_bind(void *object)
{
    uint32_t index;void *page;
    static const uint32_t bases[]={0x1190,0x11f0,0x1118,0x11ec,0x1118};
    static const uint32_t data[]={0x1c,0x2f0,0x1c,0x160,0x2f0};
    for(index=0;index<5;++index){
        page=PTR(object,0x263c+index*4);
        PTR(page,bases[index])=AT(PTR(object,0x1104),data[index]);
        PTR(page,bases[index]+4)=PTR(object,0x1104);
        PTR(page,bases[index]+16)=object;
        PTR(page,bases[index]+8)=AT(object,0x253c);
        PTR(page,bases[index]+12)=AT(object,0x2440);
    }
    PTR(PTR(object,0x263c),0x11a4)=AT(object,0x1594);
    PTR(PTR(object,0x2648),0x1200)=AT(object,0x1594);
    PTR(PTR(object,0x2640),0x1204)=AT(object,0x1594);
    PTR(object,0x2650)=AT(object,0x2440);
}
static void advanced_parent_action(void *object,uint32_t rva,uint32_t id)
{
    uint32_t selection=API(0x257b0,uint32_t(TC *)(void *))(AT(object,0x1690));
    U32(PTR(object,0x1104),0x404)=selection;
    if(rva==0x2990)fs98_world_advanced_call(PTR(object,selection==0?0x263c:selection==1?0x2648:0x2640),selection==0?0x8c10:selection==1?0x12c80:0xf500,0,0);
    else fs98_world_advanced_call(PTR(object,selection==0?0x263c:selection==1?0x2648:0x2640),selection==0?0x8b00:selection==1?0x12930:0xf2b0,
        rva==0x2870?0xffffffff:selection==0?id-0x1e62:selection==1?(id==0x1e61?3:id-0x1e62):id-0x1e61,0);
}
static uint32_t advanced_pressure(void *object,uint32_t rva,uint32_t id)
{
    uint32_t value=0;void *data;void *status;uint32_t control=0,result=0;
    switch(rva){
    case 0x9e60:
        advanced_label(object,0x112c,0x220);advanced_label(object,0x11e4,0x220);advanced_label(object,0x129c,0x220);advanced_label(object,0x1354,0x220);
        advanced_edit_init(object,0x140c,0x112c,0x11,4);advanced_edit_init(object,0x1474,0x1354,0x15,5);advanced_sprite(object,0x14dc,0x139,0x4c0);
        data=PTR(object,0x111c);advanced_enable(object,0x14dc,!U32(data,0x14)||!U32(data,0x18));RUN(0x9fa0,0,0);break;
    case 0x9fa0:
        advanced_text(object,0x1474,PTR(PTR(object,0x1118),0xe0));advanced_text(object,0x140c,PTR(PTR(object,0x1118),0xf4));
        advanced_check(object,0x14dc,U32(PTR(object,0x1118),0xd8));advanced_buttons(PTR(object,0x1128),5);break;
    case 0xa000:U32(PTR(object,0x1118),0xd8)=U32(object,0x1598);break;
    case 0xa020:
        if(!U32(object,0x1100))return 0;
        U32(PTR(object,0x1118),0xd8)=U32(object,0x1598);
        advanced_get(object,0x140c,AT(PTR(object,0x1118),0xf0));advanced_get(object,0x1474,AT(PTR(object,0x1118),0xdc));
        advanced_callback(object,0x111c,4);data=PTR(object,0x1118);
        if(U32(data,0xdc))result=0x1b03;else if(U32(data,0xf0))result=0x1b04;
        if(!result&&(id==0x1b03||id==0x1b04)){
            if(*(char *)PTR(data,id==0x1b03?0xe0:0xf4))U32(data,0xd8)|=1;
            advanced_callback(object,0x111c,id==0x1b03?0x3f8:0x3f4);
            advanced_text(object,id==0x1b03?0x140c:0x1474,PTR(PTR(object,0x1118),id==0x1b03?0xf4:0xe0));
        }return result;
    case 0xa190:
        data=PTR(object,0x1118);status=AT(data,0xdc);control=0x1474;
        if(!U32(status,0)){status=AT(data,0xf0);control=0x140c;}
        if(U32(status,0)){value=API(0x25778,uint32_t(TC *)(void *,void *,void *))(object,AT(object,control),status);U32(object,0x8c)=value;if(value)result=control==0x1474?0x1b03:0x1b04;}
        return advanced_validate_again(object,result);
    case 0xa220:advanced_buttons(PTR(object,0x1128),2);if(!(id&1))advanced_post(object,U32(object,0x14fc));break;
    }return 0;
}
static uint32_t advanced_visibility(void *object,uint32_t rva,uint32_t first)
{
    uint32_t index,value;
    switch(rva){
    case 0xfd50:advanced_label(object,0x112c,0x220);advanced_label(object,0x11e4,0x370);s_services->ui->combo(AT(object,0x129c),AT(object,0x3e0),AT(object,0x11e4));RUN(0xfde0,0,0);break;
    case 0xfde0:advanced_send(object,0x129c,0x14e,20-U32(PTR(object,0x1118),0x140),0);advanced_buttons(PTR(object,0x1128),5);break;
    case 0xfe20:case 0x10020:value=advanced_send(object,0x129c,0x147,0,0);U32(PTR(object,0x1118),0x140)=value<=20?20-value:20;break;
    case 0x10030:if(!U32(object,0x1100))return 0;RUN(0xfe20,0,0);return advanced_validate_again(object,0);
    case 0x10060:
        advanced_buttons(PTR(object,0x1128),2);
        for(index=0;index<3;++index)if(U32(PTR(object,0x1118),0x5c+index*0x58))break;
        advanced_enable(object,0x129c,index!=3);advanced_enable(object,0x11e4,index!=3);
        if(!(first&1))advanced_post(object,index!=3?U32(object,0x12bc):U32(PTR(object,0x1124),0x20));
        break;
    }return 0;
}
uint32_t fs98_world_advanced_call(void *object,uint32_t rva,uint32_t first,uint32_t second)
{
    uint32_t index,value;void *page,*notify,*focus;void **vtable;
    if(rva==0xfc40)return 1;
    if(rva>=0x76e0&&rva<0x91c0)return fs98_world_advanced_cloud(object,rva,first,second);
    if(rva>=0xeb40&&rva<0xfbc0)return fs98_world_advanced_temperature(object,rva,first,second);
    if(rva>=0x11e40&&rva<0x137a0)return fs98_world_advanced_wind(object,rva,first,second);
    if(rva>=0x9e60&&rva<0xa260)return advanced_pressure(object,rva,first);
    if(rva>=0xfd50&&rva<0x10130)return advanced_visibility(object,rva,first);
    switch(rva){
    case 0x21d0:return advanced_parent_init(object);
    case 0x2090:API(0x2551c,int(SC *)(const char *,uint32_t,uint32_t))("CAdvancedWeather::ValidateData()",0,0);return 0;
    case 0x2cc0:value=U32(object,0x1104);if(value&&value!=0xcdcdcdcd)return 1;
        API(0x257a8,int(*)(void *,const char *,const char *,uint32_t))(PTR(object,0x2638),"In CAdvancedWeather::InitializeData","Bad m_pliveInfo = %lx",value);return 0;
    case 0x2a20:advanced_bind(object);break;
    case 0x2720:
        API(0x258c0,int(SC *)(int))(0x1b);API(0x258c0,int(SC *)(int))(0x73);U32(object,0x2654)=2;U32(PTR(object,0x1104),0)=0;
        ((void(TC *)(void *))PTR(PTR(object,0),0xec))(object);break;
    case 0x2760:
        page=API(0x257b8,void *(TC *)(void *))(AT(object,0x1690));focus=API(0x258bc,void *(SC *)(void))( );
        focus=API(0x255cc,void *(SC *)(void *))(focus);
        if(focus&&API(0x255d4,int(TC *)(void *,void *))(focus,fs98_world_advanced_api(0x255d0)))value=API(0x255d8,uint32_t(TC *)(void *))(focus);else value=U32(page,0x7d0);
        vtable=PTR(page,0);value=((uint32_t(TC *)(void *,uint32_t))vtable[0xd8/4])(page,value);
        if(value&&((uint32_t(TC *)(void *,uint32_t))vtable[0xdc/4])(page,value))break;
        U32(PTR(object,0x1104),0)=1;((void(TC *)(void *))PTR(PTR(object,0),0xec))(object);break;
    case 0x27e0:
        page=AT(object,0x1690);((void(TC *)(void *))PTR(PTR(page,0),0xc4))(page);
        notify=PTR(object,0x2638);if(notify)((void *(TC *)(void *,uint32_t))PTR(PTR(notify,0),4))(notify,1);PTR(object,0x2638)=NULL;
        page=PTR(object,0x2640);notify=PTR(page,0x1208);if(notify)((void *(TC *)(void *,uint32_t))PTR(PTR(notify,0),4))(notify,1);PTR(PTR(object,0x2640),0x1208)=NULL;
        page=PTR(object,0x2648);notify=PTR(page,0x1204);if(notify)((void *(TC *)(void *,uint32_t))PTR(PTR(notify,0),4))(notify,1);PTR(PTR(object,0x2648),0x1204)=NULL;
        API(0x257b4,void(TC *)(void *))(object);break;
    case 0x2870:case 0x2990:advanced_parent_action(object,rva,first);break;
    case 0x28c0:
        if((int)(first&0xffff)>=0x1e61&&(int)(first&0xffff)<0x1e65){advanced_parent_action(object,rva,first&0xffff);return 1;}
        return API(0x257ac,uint32_t(TC *)(void *,uint32_t,uint32_t))(object,first,second);
    case 0x2bf0:
        if((int)first==1){if(!API(0x255c4,int(TC *)(void *))(AT(object,0x1498))){advanced_enable(object,0x1498,1);advanced_enable(object,0x1594,0);}else advanced_enable(object,0x1594,0);}
        else if(!first){advanced_enable(object,0x1498,0);if(!API(0x255c4,int(TC *)(void *))(AT(object,0x1594)))advanced_enable(object,0x1594,1);}
        else if((int)first>0){advanced_enable(object,0x1498,0);advanced_enable(object,0x1594,0);}
        else{for(index=0;index<2;++index)if(!API(0x255c4,int(TC *)(void *))(AT(object,index?0x1594:0x1498)))advanced_enable(object,index?0x1594:0x1498,1);}break;
    }return 0;
}
