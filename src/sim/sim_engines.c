#include "sim_internal.h"
#include "sim_tables.inc"
#define D32(p,o) (*(uint32_t *)((uint8_t *)(p)+(o)))
#define U16(p,o) (*(uint16_t *)((uint8_t *)(p)+(o)))
#define PTR(p,o) (*(void **)((uint8_t *)(p)+(o)))
#define AT(p,o) ((uint8_t *)(p)+(o))

void fs98_sim_engines_init(void *object)
{
    char caption[0x1d];
    uint32_t i,id;
    g_sim_services->init_group(AT(object,0x1290),AT(object,0x760));
    id=D32(PTR(object,0x1120),0x10)&1?0xf7b2:0xf7b1;
    g_sim_services->load_string(D32(object,0x80),id,caption,0x1d);
    g_sim_services->control_text(AT(object,0x11dc),caption);
    g_sim_services->init_group(AT(object,0x11dc),AT(object,0x760));
    g_sim_services->init_group(AT(object,0x1128),AT(object,0x760));
    g_sim_services->init_text(AT(object,0x1344),AT(object,0x370));
    g_sim_services->init_combo(AT(object,0x16dc),AT(object,0x3e0),AT(object,0x1344));
    for(i=0;i<4;++i){
        uint32_t label=0x1624-i*0xb8;
        g_sim_services->init_text(AT(object,label),AT(object,0x370));
        g_sim_services->init_combo(AT(object,0x17fc-i*0x48),AT(object,0x3e0),AT(object,label));
    }
    for(i=0;i<8;++i)fs98_sim_init_page_button(object,0x1dbc-i*0xc8,0x4c0,0x139);
    PTR(object,0x1124)=fs98_sim_make_notify();
    fs98_sim_engines_populate(object);
}
void fs98_sim_engines_populate(void *object)
{
    char caption[0x1d];
    uint32_t i,j,count;
    const uint16_t *strings;
    for(i=0;i<4;++i)g_sim_services->send(D32(object,s_engine_combos[i][0]),0x14b,0,0);
    if(D32(PTR(object,0x1120),0x10)&1){strings=s_engine_turbine_strings;count=sizeof(s_engine_turbine_strings)/sizeof(strings[0]);}
    else {strings=s_engine_piston_strings;count=sizeof(s_engine_piston_strings)/sizeof(strings[0]);}
    for(i=0;i<count;++i){
        g_sim_services->load_string(D32(object,0x80),strings[i],caption,0x1d);
        for(j=0;j<4;++j)g_sim_services->send(D32(object,s_engine_combos[j][0]),0x143,0,(uint32_t)caption);
    }
    for(i=0;i<4;++i)g_sim_services->send(D32(object,s_engine_combos[i][0]),0x14e,D32(PTR(object,0x111c),s_engine_combos[i][1]),0);
    for(i=0;i<4;++i){
        void *descriptor=PTR(object,0x111c);
        if(U16(descriptor,0x24)<i+1){
            g_sim_services->enable_control(AT(object,0x1cf4-i*0xc8),0);
            g_sim_services->enable_control(AT(object,0x17fc-i*0x48),0);
            if(i==0)g_sim_services->enable_control(AT(object,0x1dbc),0);
        } else g_sim_services->check(AT(object,0x1cf4-i*0xc8),D32(descriptor,0x2c+i*8));
    }
    g_sim_services->check(AT(object,0x190c),D32(PTR(object,0x111c),0x54));
    g_sim_services->check(AT(object,0x19d4),D32(PTR(object,0x111c),0x68));
    if(D32(PTR(object,0x111c),0x58))g_sim_services->check(AT(object,0x1844),D32(PTR(object,0x111c),0x60));
    else g_sim_services->enable_control(AT(object,0x1844),0);
    if(D32(PTR(object,0x111c),0x6c))g_sim_services->send(D32(object,0x16fc),0x14e,D32(PTR(object,0x111c),0x74),0);
    else {g_sim_services->enable_control(AT(object,0x16dc),0);g_sim_services->enable_control(AT(object,0x1344),0);}
    g_sim_services->enable_control(AT(object,D32(PTR(object,0x1120),0x10)&1?0x190c:0x19d4),0);
    g_sim_services->check(AT(object,0x1dbc),D32(PTR(object,0x111c),0x4c));
}
