#include "sim_internal.h"
#include "sim_tables.inc"
#define D32(p,o) (*(uint32_t *)((uint8_t *)(p)+(o)))
#define PTR(p,o) (*(void **)((uint8_t *)(p)+(o)))
#define AT(p,o) ((uint8_t *)(p)+(o))
void fs98_sim_systems_init(void *object)
{
    uint32_t i;
    g_sim_services->init_group(AT(object,0x1344),AT(object,0x760));
    g_sim_services->init_group(AT(object,0x1790),AT(object,0x760));
    g_sim_services->init_group(AT(object,0x1290),AT(object,0x760));
    for(i=0;i<5;++i){
        uint32_t label=0x16d8-i*0xb8;
        g_sim_services->init_text(AT(object,label),AT(object,0x370));
        g_sim_services->init_combo(AT(object,0x19f4-i*0x48),AT(object,0x3e0),AT(object,label));
    }
    g_sim_services->init_text(AT(object,0x11d8),AT(object,0x370));
    g_sim_services->init_combo(AT(object,0x188c),AT(object,0x3e0),AT(object,0x11d8));
    g_sim_services->init_text(AT(object,0x1120),AT(object,0x370));
    g_sim_services->init_combo(AT(object,0x1844),AT(object,0x3e0),AT(object,0x1120));
    for(i=0;i<8;++i)fs98_sim_init_page_button(object,0x1fb4-i*0xc8,0x4c0,0x139);
    fs98_sim_systems_populate(object);
}
int32_t fs98_sim_systems_commit(void *object,uint32_t unused)
{
    uint32_t i;
    (void)unused;
    if(!D32(object,0x1100))return 0;
    for(i=0;i<8;++i)D32(PTR(object,0x1118),s_system_writes[i][1])=D32(object,s_system_writes[i][0]);
    for(i=0;i<7;++i){
        int32_t selection=g_sim_services->send(D32(object,s_system_combos[i][0]),0x147,0,0);
        D32(PTR(object,0x1118),s_system_combos[i][1])=(uint32_t)fs98_sim_system_selection(object,selection);
    }
    return 0;
}
void fs98_sim_systems_populate(void *object)
{
    uint32_t i;
    for(i=0;i<7;++i)g_sim_services->send(D32(object,s_system_combos[i][0]),0x14e,D32(PTR(object,0x1118),s_system_combos[i][1]),0);
    for(i=0;i<8;++i)g_sim_services->check(AT(object,s_system_writes[i][0]-0xbc),D32(PTR(object,0x1118),s_system_writes[i][1]));
    for(i=0;i<sizeof(s_system_features)/sizeof(s_system_features[0]);++i){
        if(!(D32(PTR(object,0x111c),0x18)&s_system_features[i][0])){
            g_sim_services->enable_control(AT(object,s_system_features[i][1]),0);
            if(s_system_features[i][2])g_sim_services->enable_control(AT(object,s_system_features[i][2]),0);
        }
    }
}
