/* Crash analysis: initialization, notification ownership and axis-label paint.
 * Plot rendering belongs to the callback supplied by the caller descriptor. */
#include "sim_internal.h"
#include "sim_unwind.h"
#define D32(p,o) (*(uint32_t *)((uint8_t *)(p)+(o)))
#define PTR(p,o) (*(void **)((uint8_t *)(p)+(o)))
#define AT(p,o) ((uint8_t *)(p)+(o))
#define TC FS98_SIM_THISCALL
static void *s_notify_vtable;
void fs98_sim_set_crash_tables(void *notify_vtable) { s_notify_vtable=notify_vtable; }
void *fs98_sim_notify_delete(void *object,uint32_t flags)
{
    g_sim_services->destroy(object,0xd54c);
    if(flags&1)g_sim_services->ui_delete(object);
    return object;
}
void *fs98_sim_static_delete(void *object,uint32_t flags)
{
    g_sim_services->destroy(object,0xd38c);
    if(flags&1)g_sim_services->mfc_delete(object);
    return object;
}
void fs98_sim_class_delete(void *object) { g_sim_services->mfc_delete(object); }
int32_t fs98_sim_crash_validate(void *object)
{
    void *descriptor=PTR(object,0x112c);
    if(descriptor && (uint32_t)descriptor!=0xcdcdcdcd)return 1;
    g_sim_services->notify_send(PTR(object,0x10f0),
        "In CCrashAnalysis::InitializeData",
        "Bad m_pliveInfo = %lx\npassed to dialog by core code",descriptor);
    return 0;
}
struct fs98_sim_notify_unwind {
    struct fs98_sim_unwind frame;
    void *notify;
    volatile uint32_t live;
};
static void fs98_sim_notify_cleanup(struct fs98_sim_unwind *frame)
{
    struct fs98_sim_notify_unwind *state=(void *)frame;
    if(state->live){state->live=0;g_sim_services->notify_delete(state->notify);}
}
FS98_SIM_NOINLINE void *fs98_sim_make_notify(void)
{
    struct fs98_sim_notify_unwind state;
    void *notify;
    state.notify=0;state.live=0;
    fs98_sim_unwind_enter(&state.frame,fs98_sim_notify_cleanup);
    notify=g_sim_services->allocate(0x29c);state.notify=notify;state.live=1;
    if(notify){g_sim_services->notify_construct(notify,0x30,1);PTR(notify,0)=s_notify_vtable;}
    state.live=0;fs98_sim_unwind_leave(&state.frame);
    return notify;
}
FS98_SIM_NOINLINE int32_t fs98_sim_crash_init(void *object)
{
    struct fs98_sim_notify_unwind state;
    struct fs98_sim_rect rect={0,0,0,0};
    void *notify,*control;
    state.notify=0;state.live=0;
    fs98_sim_unwind_enter(&state.frame,fs98_sim_notify_cleanup);
    g_sim_services->base_init(object);
    if(!((int32_t (TC *)(void *,uint32_t,uint32_t,uint32_t))PTR(PTR(object,0),0xe4))(
        object,0x14f,g_sim_resource_module,0xf)){
        fs98_sim_unwind_leave(&state.frame);return 0;
    }
    g_sim_services->init_button(AT(object,0x1134),AT(object,0x5a0),AT(object,0x610),0x132,
        AT(object,0xff4),AT(object,0x7e8),1,0xffffffff,g_sim_resource_module,1,1,rect);
    g_sim_services->init_art(AT(object,0x1230),0xf831,AT(object,0xff4),AT(object,0x7e8),2);
    g_sim_services->init_text(AT(object,0x1318),AT(object,0x220));
    g_sim_services->init_text(AT(object,0x13d0),AT(object,0x220));
    g_sim_services->init_text(AT(object,0x1488),AT(object,0x220));
    g_sim_services->init_text(AT(object,0x1540),AT(object,0x220));
    g_sim_services->init_text(AT(object,0x15f8),AT(object,0x220));
    notify=g_sim_services->allocate(0x29c);state.notify=notify;state.live=1;
    if(notify){g_sim_services->notify_construct(notify,0x30,1);PTR(notify,0)=s_notify_vtable;}
    state.live=0;PTR(object,0x10f0)=notify;
    if(!fs98_sim_crash_validate(object)){fs98_sim_unwind_leave(&state.frame);return 0;}
    control=g_sim_services->get_control(object,0x13bc);
    D32(object,0x10dc)=D32(control,0x20);
    g_sim_services->remap_rect(&rect,control);
    g_sim_services->move_control(control,rect.left,rect.top,(int32_t)((uint32_t)rect.right-(uint32_t)rect.left),
        (int32_t)((uint32_t)rect.bottom-(uint32_t)rect.top),1);
    fs98_sim_crash_notify(object);
    fs98_sim_unwind_leave(&state.frame);
    return 1;
}
struct fs98_sim_paint_unwind {
    struct fs98_sim_unwind frame;
    uint8_t dc[0x54];
    volatile uint32_t live;
};
static void fs98_sim_paint_cleanup(struct fs98_sim_unwind *frame)
{
    struct fs98_sim_paint_unwind *state=(void *)frame;
    if(state->live){state->live=0;g_sim_services->paint_destroy(state->dc);}
}
static void fs98_sim_label(void *font,void *dc,const char *text,struct fs98_sim_rect rect)
{
    uint32_t string;
    g_sim_services->string_construct_text(&string,text);
    /* DrawSimsText consumes its CString argument by value. */
    g_sim_services->draw_text(font,dc,string,rect,0);
}
FS98_SIM_NOINLINE void fs98_sim_crash_paint(void *object)
{
    struct fs98_sim_paint_unwind state;
    struct fs98_sim_rect rect;
    uint32_t step,index;
    void *descriptor;
    const char *const *labels;
    state.live=0;fs98_sim_unwind_enter(&state.frame,fs98_sim_paint_cleanup);
    g_sim_services->paint_construct(state.dc,object);state.live=1;
    descriptor=PTR(object,0x112c);
    if(D32(object,0x10f4)==1){
        if(D32(descriptor,0x2c)){
            uint32_t id=D32(descriptor,0x10)==0?0xf7b4:0xf7bd;
            D32(object,0x10f4)=0;
            g_sim_services->load_string(D32(object,0x80),id,(char *)AT(object,0x10f8),0x1d);
            g_sim_services->load_string(D32(object,0x80),0xf7c1,(char *)AT(object,0x1116),0xb);
            g_sim_services->invalidate(D32(object,0x20),0,1);
        }
        fs98_sim_paint_cleanup(&state.frame);fs98_sim_unwind_leave(&state.frame);return;
    }
    step=(D32(object,0x10ec)-D32(object,0x10e4))/(D32(descriptor,0x24)-1);
    g_sim_services->set_rect(&rect,8,(int32_t)(D32(object,0x10ec)-12),45,(int32_t)(D32(object,0x10ec)+4));
    descriptor=PTR(object,0x112c);labels=PTR(descriptor,0x28);
    for(index=0;(int32_t)index<(int32_t)D32(PTR(object,0x112c),0x24);++index){
        fs98_sim_label(AT(object,0x300),state.dc,*labels++,rect);
        rect.top=(int32_t)((uint32_t)rect.top-step);rect.bottom=(int32_t)((uint32_t)rect.bottom-step);
    }
    rect.left=26;rect.top=64;rect.right=355;rect.bottom=80;
    fs98_sim_label(AT(object,0x220),state.dc,(char *)AT(object,0x10f8),rect);
    descriptor=PTR(object,0x112c);
    step=(D32(object,0x10e8)-D32(object,0x10e0))/(D32(descriptor,0x1c)-1);
    g_sim_services->set_rect(&rect,44,(int32_t)(D32(object,0x10ec)+7),
        (int32_t)(step-(uint32_t)((int32_t)step/3)+44),(int32_t)(D32(object,0x10ec)+27));
    descriptor=PTR(object,0x112c);labels=PTR(descriptor,0x20);
    for(index=0;(int32_t)index<(int32_t)D32(PTR(object,0x112c),0x1c);++index){
        fs98_sim_label(AT(object,0x300),state.dc,*labels++,rect);
        rect.left=(int32_t)((uint32_t)rect.left+step-(index&1));
        rect.right=(int32_t)((uint32_t)rect.right+step-(index&1));
    }
    g_sim_services->set_rect(&rect,12,(int32_t)(D32(object,0x10ec)+6),53,(int32_t)(D32(object,0x10ec)+26));
    fs98_sim_label(AT(object,0x220),state.dc,(char *)AT(object,0x1116),rect);
    fs98_sim_paint_cleanup(&state.frame);fs98_sim_unwind_leave(&state.frame);
}
