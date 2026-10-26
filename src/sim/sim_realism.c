#include "sim_internal.h"
#define D32(p,o) (*(uint32_t *)((uint8_t *)(p)+(o)))
#define PTR(p,o) (*(void **)((uint8_t *)(p)+(o)))
#define AT(p,o) ((uint8_t *)(p)+(o))
#define TC FS98_SIM_THISCALL

void fs98_sim_realism_init(void *object)
{
    void *font;
    g_sim_services->init_text(AT(object,0x163c),AT(object,0x760));
    g_sim_services->init_text(AT(object,0x1414),AT(object,0x760));
    g_sim_services->init_text(AT(object,0x12a4),AT(object,0x220));
    g_sim_services->init_text(AT(object,0x1134),AT(object,0x220));
    g_sim_services->init_text(AT(object,0x135c),AT(object,0x220));
    g_sim_services->init_text(AT(object,0x11ec),AT(object,0x220));
    g_sim_services->init_slider(AT(object,0x191c),0x135,0x136,PTR(object,0x10fc),PTR(object,0x10dc),6,0x1a,6,8,1,-1,g_sim_resource_module);
    g_sim_services->init_text(AT(object,0x16f4),AT(object,0x760));
    g_sim_services->init_text(AT(object,0x1864),AT(object,0x220));
    g_sim_services->init_text(AT(object,0x17ac),AT(object,0x220));
    g_sim_services->init_text(AT(object,0x1584),AT(object,0x220));
    g_sim_services->init_text(AT(object,0x14cc),AT(object,0x220));
    font=g_sim_services->base_font(2);
    g_sim_services->init_edit(AT(object,0x1e04),font,0xffffff,0,0xff0000,AT(object,0x1584),0x15);
    font=g_sim_services->base_font(2);
    g_sim_services->init_edit(AT(object,0x1d9c),font,0xffffff,0,0xff0000,AT(object,0x14cc),0x11);
    D32(object,0x1e68)=5;D32(object,0x1e00)=4;
    fs98_sim_init_page_button(object,0x20c4,0x4c0,0x139);
    fs98_sim_init_page_button(object,0x1ffc,0x4c0,0x139);
    fs98_sim_init_page_button(object,0x1f34,0x4c0,0x139);
    fs98_sim_init_page_button(object,0x1e6c,0x4c0,0x139);
    g_sim_services->init_group(AT(object,0x218c),AT(object,0x760));
    D32(PTR(object,0x1120),0x20)=0;
    g_sim_services->send(D32(object,0x1dbc),0xc5,D32(PTR(object,0x111c),0x60)-1,0);
    g_sim_services->send(D32(object,0x1e24),0xc5,D32(PTR(object,0x111c),0x4c)-1,0);
    fs98_sim_realism_populate(object);
}
void fs98_sim_realism_populate(void *object)
{
    void *descriptor=PTR(object,0x111c);
    g_sim_services->slider_range(AT(object,0x191c),D32(descriptor,0xc),D32(descriptor,0x10));
    g_sim_services->slider_set(AT(object,0x191c),D32(PTR(object,0x111c),8));
    g_sim_services->check(AT(object,0x1e6c),D32(PTR(object,0x111c),0x20));
    g_sim_services->check(AT(object,0x1f34),D32(PTR(object,0x111c),0x30));
    g_sim_services->check(AT(object,0x1ffc),D32(PTR(object,0x111c),0x38));
    g_sim_services->check(AT(object,0x20c4),D32(PTR(object,0x111c),0x18));
    g_sim_services->control_text(AT(object,0x1d9c),PTR(PTR(object,0x111c),0x5c));
    g_sim_services->control_text(AT(object,0x1e04),PTR(PTR(object,0x111c),0x48));
    g_sim_services->text(AT(object,0x12a4),PTR(PTR(object,0x111c),0x3c));
    g_sim_services->text(AT(object,0x1134),PTR(PTR(object,0x111c),0x40));
    if(!(D32(PTR(object,0x1120),0x1c)&0x20))g_sim_services->enable_control(AT(object,0x1ffc),0);
    if(D32(PTR(object,0x1120),0x10)==2)g_sim_services->enable_control(AT(object,0x1e6c),0);
    else if(D32(PTR(object,0x1120),0x10)==3)g_sim_services->enable_control(AT(object,0x20c4),0);
    if(!D32(PTR(object,0x1120),0x14))g_sim_services->enable_control(AT(object,0x1f34),0);
}
int32_t fs98_sim_realism_validate(void *object,uint32_t control_id)
{
    void *descriptor,*helper;
    int32_t position,result;
    if(!D32(object,0x1100))return 0;
    D32(PTR(object,0x111c),0x20)=D32(object,0x1f28);
    D32(PTR(object,0x111c),0x30)=D32(object,0x1ff0);
    D32(PTR(object,0x111c),0x38)=D32(object,0x20b8);
    D32(PTR(object,0x111c),0x18)=D32(object,0x2180);
    position=g_sim_services->slider_get(AT(object,0x191c));
    D32(PTR(object,0x111c),8)=(uint32_t)position;
    descriptor=PTR(object,0x111c);
    g_sim_services->get_text(AT(object,0x1e04),PTR(descriptor,0x48),D32(descriptor,0x4c));
    descriptor=PTR(object,0x111c);
    g_sim_services->get_text(AT(object,0x1d9c),PTR(descriptor,0x5c),D32(descriptor,0x60));
    helper=PTR(object,0x1120);
    if(control_id==0x13bd){D32(helper,0x20)=4;D32(object,0x1118)=1;}
    else if(control_id==0x13be){D32(helper,0x20)=5;D32(object,0x1118)=0;}
    else D32(helper,0x20)=0;
    helper=PTR(object,0x1120);
    result=((fs98_sim_settings_fn)PTR(helper,4))(helper);
    if(result){
        descriptor=PTR(object,0x111c);
        if(D32(descriptor,0x44))return 0x13bd;
        if(D32(descriptor,0x58))return 0x13be;
        return result;
    }
    helper=PTR(object,0x1120);
    if(D32(helper,0x20)){
        descriptor=PTR(object,0x111c);
        if(D32(object,0x1118)==1){
            ((fs98_sim_settings_fn)PTR(descriptor,0x70))(helper);
            g_sim_services->control_text(AT(object,0x1d9c),PTR(PTR(object,0x111c),0x5c));
        }else{
            ((fs98_sim_settings_fn)PTR(descriptor,0x6c))(helper);
            g_sim_services->control_text(AT(object,0x1e04),PTR(PTR(object,0x111c),0x48));
        }
        D32(PTR(object,0x1120),0x20)=0;
    }
    return result;
}
int32_t fs98_sim_realism_error(void *object,int32_t display)
{
    void *descriptor;
    int32_t result;
    if(!display)return display;
    descriptor=PTR(object,0x111c);
    if(D32(descriptor,0x44)){
        result=g_sim_services->error_dialog(object,AT(object,0x1e04),AT(descriptor,0x44));
        D32(object,0x8c)=(uint32_t)result;
        if(result)return ((int32_t (TC *)(void *,uint32_t))PTR(PTR(object,0),0xe0))(object,0x13bd);
    }else if(D32(descriptor,0x58)){
        result=g_sim_services->error_dialog(object,AT(object,0x1d9c),AT(descriptor,0x58));
        D32(object,0x8c)=(uint32_t)result;
        if(result)return ((int32_t (TC *)(void *,uint32_t))PTR(PTR(object,0),0xe0))(object,0x13be);
    }
    return display;
}
