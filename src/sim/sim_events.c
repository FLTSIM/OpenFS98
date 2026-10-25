/* Sim's independent event and descriptor paths. Vtables/descriptors are
 * reloaded at the same external-call boundaries as in the retail routines. */
#include "sim_internal.h"
#include "sim_tables.inc"
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define U16(object, offset) (*(uint16_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_SIM_THISCALL
#define SC FS98_SIM_STDCALL

void fs98_sim_exchange(void *object, void *exchange, uint32_t kind)
{
    uint32_t index;
    static const uint16_t (*const s_ddx[])[2] = {
        s_ddx_crash, s_ddx_damage, s_ddx_engines, s_ddx_fuel,
        s_ddx_systems, s_ddx_realism, s_ddx_reliability, s_ddx_settings
    };
    for (index = 0; index < s_classes[kind].control_count; ++index)
        g_sim_services->ddx(exchange, s_ddx[kind][index][1], AT(object, s_ddx[kind][index][0]));
}
static const uint32_t *fs98_sim_help_map(uint32_t kind)
{
    static const uint32_t *const s_help[] = {s_help_crash, s_help_damage, s_help_engines,
        s_help_fuel, s_help_systems, s_help_realism, s_help_reliability, s_help_settings};
    return s_help[kind];
}
int32_t fs98_sim_help(void *object, uint32_t unused, void *info, uint32_t kind)
{
    (void)object; (void)unused;
    return g_sim_services->help(D32(info, 0xc), "fs98.hlp", 0xc, fs98_sim_help_map(kind));
}
int32_t fs98_sim_context(void *object, uint32_t window, uint32_t unused, uint32_t kind)
{
    (void)object; (void)unused;
    g_sim_services->help(window, "fs98.hlp", 0xa, fs98_sim_help_map(kind));
    return 1;
}
static void fs98_sim_crash_callback(void *object, uint32_t operation)
{
    void *descriptor = PTR(object, 0x112c);
    D32(object, 0x1130) = operation;
    ((int32_t (SC *)(uint32_t, void *, uint32_t))PTR(descriptor, 8))(
        operation, descriptor, D32(object, 0x10dc));
}
int32_t fs98_sim_crash_tick(void *object, uint32_t w_param, uint32_t l_param)
{ (void)w_param; (void)l_param; fs98_sim_crash_callback(object, 1); return 0; }
void fs98_sim_crash_accept(void *object)
{
    void *descriptor = PTR(object, 0x112c);
    D32(object, 0x16b0) = 1; D32(descriptor, 0) = 1;
    fs98_sim_crash_callback(object, 2);
    ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
}
void fs98_sim_crash_cancel(void *object)
{
    void *descriptor = PTR(object, 0x112c);
    D32(object, 0x16b0) = 2; D32(descriptor, 0) = 0;
    fs98_sim_crash_callback(object, 2);
    ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
}
void fs98_sim_crash_close(void *object)
{
    void *notify = PTR(object, 0x10f0);
    if (notify) {
        ((void *(TC *)(void *, uint32_t))PTR(PTR(notify, 0), 4))(notify, 1);
        PTR(object, 0x10f0) = 0;
    }
    g_sim_services->close(object);
}
void fs98_sim_crash_notify(void *object)
{
    fs98_sim_crash_callback(object, 0);
    g_sim_services->text(AT(object, 0x1540), PTR(PTR(object, 0x112c), 0x14));
    g_sim_services->text(AT(object, 0x15f8), PTR(PTR(object, 0x112c), 0x18));
}
void fs98_sim_post_destroy(void *object)
{ if (object) ((void *(TC *)(void *, uint32_t))PTR(PTR(object, 0), 4))(object, 1); }
void fs98_sim_disable(void *object) { g_sim_services->enable_window(D32(object, 0x20), 0); }
void fs98_sim_enable(void *object) { g_sim_services->enable_window(D32(object, 0x20), 1); }
void fs98_sim_empty(void *object) { (void)object; }
void fs98_sim_empty_arg(void *object, uint32_t unused) { (void)object; (void)unused; }
int32_t fs98_sim_zero(void *object, uint32_t unused) { (void)object; (void)unused; return 0; }
int32_t fs98_sim_zero_message(void *object, uint32_t first, uint32_t second)
{ (void)object; (void)first; (void)second; return 0; }

/* 2360/23d0/2440: radio state is HWND == GWL_USERDATA, with a handle reload. */
void fs98_sim_damage_radio(void *object, uint32_t mode)
{
    const uint32_t handle_offset = mode == 0 ? 0x13d4 : mode == 1 ? 0x1590 : 0x1684;
    int32_t checked = g_sim_services->window_long(D32(object, handle_offset), -21);
    if ((uint32_t)checked != D32(object, handle_offset)) return;
    D32(PTR(object, 0x1118), 8) = mode;
    g_sim_services->enable_control(AT(object, 0x115c), mode != 0);
    g_sim_services->enable_control(AT(object, 0x1224), mode != 0);
    g_sim_services->enable_control(AT(object, 0x12ec), mode != 0);
    g_sim_services->enable_control(AT(object, 0x14a8), mode != 0);
}

/* 35b0: the first engine always counts, even for a reported engine count of
 * zero; later engines are tested using unsigned 16-bit comparisons. */
uint32_t fs98_sim_engine_count(void *object)
{
    void *descriptor;
    uint16_t count;
    D32(object, 0x1118) = D32(object, 0x1db0) ? 1 : 0;
    descriptor = PTR(object, 0x111c); count = U16(descriptor, 0x24);
    if (count >= 2 && D32(object, 0x1ce8)) ++D32(object, 0x1118);
    if (count >= 3 && D32(object, 0x1c20)) ++D32(object, 0x1118);
    if (count >= 4 && D32(object, 0x1b58)) ++D32(object, 0x1118);
    return D32(object, 0x1118) == count;
}
void fs98_sim_engines_match(void *object, int32_t count)
{
    if (count >= 2 && fs98_sim_engine_count(object))
        g_sim_services->check(AT(object, 0x1dbc), 1);
}
void fs98_sim_engine_toggle(void *object, uint32_t index)
{
    uint32_t all = fs98_sim_engine_count(object);
    if (index == 0) {
        if (all) { g_sim_services->check(AT(object, 0x1dbc), 1); return; }
        if (!D32(object, 0x1e78)) return;
        g_sim_services->check(AT(object, 0x1cf4), 1);
        g_sim_services->check(AT(object, 0x1c2c), 1);
        if (U16(PTR(object, 0x111c), 0x24) > 2) g_sim_services->check(AT(object, 0x1b64), 1);
        if (U16(PTR(object, 0x111c), 0x24) > 3) g_sim_services->check(AT(object, 0x1a9c), 1);
    } else {
        const uint32_t check = 0x1cf4 - (index - 1) * 0xc8;
        const uint32_t value = 0x1db0 - (index - 1) * 0xc8;
        if (!D32(object, 0x1118)) { g_sim_services->check(AT(object, check), 1); return; }
        if (!D32(object, value)) { g_sim_services->check(AT(object, 0x1dbc), 0); return; }
        fs98_sim_engines_match(object, U16(PTR(object, 0x111c), 0x24));
    }
}
int32_t fs98_sim_engines_commit(void *object, uint32_t unused)
{
    uint32_t index;
    (void)unused;
    if (!D32(object, 0x1100)) return 0;
    for (index=0; index<8; ++index)
        D32(PTR(object,0x111c),s_engine_writes[index][1])=D32(object,s_engine_writes[index][0]);
    for (index=0; index<4; ++index) {
        int32_t selection=g_sim_services->send(D32(object,s_engine_combos[index][0]),0x147,0,0);
        D32(PTR(object,0x111c),s_engine_combos[index][1])=(uint32_t)selection;
    }
    if (D32(PTR(object,0x111c),0x6c)) {
        int32_t selection=g_sim_services->send(D32(object,0x16fc),0x147,0,0);
        D32(PTR(object,0x111c),0x74)=(uint32_t)selection;
    } else D32(PTR(object,0x111c),0x74)=0;
    return 0;
}
int32_t fs98_sim_reliability_commit(void *object, uint32_t unused)
{
    int32_t position;
    (void)unused;
    if (!D32(object,0x1100)) return 0;
    D32(PTR(object,0x1118),0x18)=D32(object,0x1884);
    D32(PTR(object,0x1118),0x20)=D32(object,0x194c);
    position=g_sim_services->slider_get(AT(object,0x1348));
    D32(PTR(object,0x1118),8)=(uint32_t)position;
    return 0;
}
void fs98_sim_reliability_populate(void *object)
{
    void *descriptor;
    g_sim_services->check(AT(object,0x17c8),D32(PTR(object,0x1118),0x18));
    g_sim_services->check(AT(object,0x1890),D32(PTR(object,0x1118),0x20));
    descriptor=PTR(object,0x1118);
    g_sim_services->slider_range(AT(object,0x1348),D32(descriptor,0xc),D32(descriptor,0x10));
    g_sim_services->slider_set(AT(object,0x1348),D32(PTR(object,0x1118),8));
    if (!(D32(PTR(object,0x111c),0x1c)&8)) g_sim_services->enable_control(AT(object,0x17c8),0);
    if (!(D32(PTR(object,0x111c),0x1c)&4)) g_sim_services->enable_control(AT(object,0x1890),0);
}
int32_t fs98_sim_system_selection(void *object, int32_t selection)
{ (void)object; return selection == 0 ? 0 : selection == 1 ? 1 : 2; }
void fs98_sim_settings_cancel(void *object)
{
    void *descriptor=PTR(object,0x10ec), *vtable=PTR(object,0);
    D32(object,0x2268)=2; D32(descriptor,0)=0;
    ((void (TC *)(void *))PTR(vtable,0xec))(object);
}
void fs98_sim_settings_close(void *object)
{
    void *tabs=AT(object,0x12e8), *notify, *page;
    ((void (TC *)(void *))PTR(PTR(tabs,0),0xc4))(tabs);
    notify=PTR(object,0x2248);
    if (notify) ((void *(TC *)(void *,uint32_t))PTR(PTR(notify,0),4))(notify,1);
    PTR(object,0x2248)=0;
    page=PTR(object,0x2258); notify=PTR(page,0x1124);
    if (notify) ((void *(TC *)(void *,uint32_t))PTR(PTR(notify,0),4))(notify,1);
    PTR(PTR(object,0x2258),0x1124)=0;
    page=PTR(object,0x225c); notify=PTR(page,0x1130);
    if (notify) ((void *(TC *)(void *,uint32_t))PTR(PTR(notify,0),4))(notify,1);
    PTR(PTR(object,0x225c),0x1130)=0;
    g_sim_services->close(object);
}

/* 21b0: the first two radio matches return the virtual Validate(0) result.
 * The third branch commits mode 2 and returns zero directly. */
int32_t fs98_sim_damage_commit(void *object, uint32_t unused)
{
    uint32_t mode;
    (void)unused;
    if (!D32(object,0x1100))
        return ((int32_t (TC *)(void *,uint32_t))PTR(PTR(object,0),0xe0))(object,0);
    D32(PTR(object,0x1118),0x10)=D32(object,0x1218);
    D32(PTR(object,0x1118),0x18)=D32(object,0x12e0);
    D32(PTR(object,0x1118),0x20)=D32(object,0x13a8);
    D32(PTR(object,0x1118),0x28)=D32(object,0x1564);
    for (mode=0;mode<3;++mode) {
        uint32_t offset=mode==0?0x13d4:mode==1?0x1590:0x1684;
        int32_t checked=g_sim_services->window_long(D32(object,offset),-21);
        if ((uint32_t)checked==D32(object,offset)) {
            D32(PTR(object,0x1118),8)=mode;
            if (mode<2) return ((int32_t (TC *)(void *,uint32_t))PTR(PTR(object,0),0xe0))(object,0);
            break;
        }
    }
    return 0;
}
void fs98_sim_damage_populate(void *object)
{
    uint32_t mode;
    g_sim_services->check(AT(object,0x115c),D32(PTR(object,0x1118),0x10));
    g_sim_services->check(AT(object,0x1224),D32(PTR(object,0x1118),0x18));
    g_sim_services->check(AT(object,0x12ec),D32(PTR(object,0x1118),0x20));
    g_sim_services->check(AT(object,0x14a8),D32(PTR(object,0x1118),0x28));
    mode=D32(PTR(object,0x1118),8);
    if (mode==2) g_sim_services->radio_check(AT(object,0x1664),1);
    else if (mode==1) g_sim_services->radio_check(AT(object,0x1570),1);
    else if (mode==0) {
        g_sim_services->radio_check(AT(object,0x13b4),1);
        fs98_sim_damage_radio(object,0);
    }
}
/* 7300: retain the page vtable across validation/error callbacks, then reload
 * the outer descriptor and close vtable after the callbacks have returned. */
void fs98_sim_settings_accept(void *object)
{
    void *page=g_sim_services->active_page(AT(object,0x12e8));
    uint32_t focused=g_sim_services->focus();
    void *control=g_sim_services->from_handle(focused);
    void *vtable;
    int32_t id, error;
    if (control && g_sim_services->is_edit(control)) id=g_sim_services->control_id(control);
    else id=(int32_t)D32(page,0x7d0);
    vtable=PTR(page,0);
    error=((int32_t (TC *)(void *,uint32_t))PTR(vtable,0xd8))(page,(uint32_t)id);
    if (error && ((int32_t (TC *)(void *,int32_t))PTR(vtable,0xdc))(page,error)) return;
    D32(PTR(object,0x10ec),0)=1;
    ((void (TC *)(void *))PTR(PTR(object,0),0xec))(object);
}

void fs98_sim_init_page_button(void *object,uint32_t offset,uint32_t font,uint32_t bitmap)
{
    void *control=AT(object,offset);
    ((int32_t (TC *)(void *,void *,uint32_t,void *,void *,uint32_t))PTR(PTR(control,0),0xc4))(
        control,AT(object,font),bitmap,PTR(object,0x10fc),PTR(object,0x10dc),g_sim_resource_module);
}
void fs98_sim_damage_init(void *object)
{
    struct fs98_sim_rect rect;
    void *control=AT(object,0x111c);
    fs98_sim_init_page_button(object,0x1664,0x530,0x134);
    fs98_sim_init_page_button(object,0x1570,0x530,0x134);
    fs98_sim_init_page_button(object,0x13b4,0x530,0x134);
    fs98_sim_init_page_button(object,0x115c,0x4c0,0x139);
    fs98_sim_init_page_button(object,0x1224,0x4c0,0x139);
    fs98_sim_init_page_button(object,0x12ec,0x4c0,0x139);
    fs98_sim_init_page_button(object,0x14a8,0x4c0,0x139);
    g_sim_services->remap_rect(&rect,control);
    g_sim_services->move_control(control,rect.left,rect.top,
        (int32_t)((uint32_t)rect.right-(uint32_t)rect.left),
        (int32_t)((uint32_t)rect.bottom-(uint32_t)rect.top),1);
    fs98_sim_damage_populate(object);
}
void fs98_sim_reliability_init(void *object)
{
    g_sim_services->init_slider(AT(object,0x1348),0x135,0x136,PTR(object,0x10fc),PTR(object,0x10dc),
        6,0x1a,6,8,1,-1,g_sim_resource_module);
    g_sim_services->init_text(AT(object,0x1290),AT(object,0x760));
    g_sim_services->init_text(AT(object,0x1120),AT(object,0x220));
    g_sim_services->init_text(AT(object,0x11d8),AT(object,0x220));
    fs98_sim_init_page_button(object,0x17c8,0x4c0,0x139);
    fs98_sim_init_page_button(object,0x1890,0x4c0,0x139);
    fs98_sim_reliability_populate(object);
}
