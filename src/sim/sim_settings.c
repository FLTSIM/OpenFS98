#include "sim_internal.h"
#include "sim_unwind.h"
#include "sim_tables.inc"

#define D32(p, o) (*(uint32_t *)((uint8_t *)(p) + (o)))
#define PTR(p, o) (*(void **)((uint8_t *)(p) + (o)))
#define AT(p, o) ((uint8_t *)(p) + (o))
#define TC FS98_SIM_THISCALL

struct sim_page_unwind {
    struct fs98_sim_unwind frame;
    void *page;
    volatile int live;
};

static void sim_page_cleanup(struct fs98_sim_unwind *frame)
{
    struct sim_page_unwind *state = (void *)frame;
    if (state->live) {
        state->live = 0;
        g_sim_services->dialog_delete(state->page);
    }
}

static FS98_SIM_NOINLINE void *sim_make_page(void *parent, uint32_t kind)
{
    struct sim_page_unwind state;
    state.live = 0;
    fs98_sim_unwind_enter(&state.frame, sim_page_cleanup);
    state.page = g_sim_services->allocate(s_classes[kind].size);
    state.live = 1;
    if (state.page)
        fs98_sim_construct(state.page, parent, kind);
    state.live = 0;
    fs98_sim_unwind_leave(&state.frame);
    return state.page;
}

int fs98_sim_settings_validate(void *object)
{
    return PTR(object, 0x10ec) != 0;
}

int fs98_sim_settings_placeholder(void *object, uint32_t unused)
{
    (void)unused;
    g_sim_services->message_box(object, "CSimulationsAircraftSettings::ValidateData()", 0, 0);
    return 0;
}

void fs98_sim_settings_wire(void *object)
{
    void *master = PTR(object, 0x10ec);
    void *realism = PTR(object, 0x224c);
    void *reliability = PTR(object, 0x2250);
    void *damage = PTR(object, 0x2254);
    void *engines = PTR(object, 0x2258);
    void *fuel = PTR(object, 0x225c);
    void *systems = PTR(object, 0x2260);
    PTR(realism, 0x111c) = AT(master, 0x28);
    PTR(realism, 0x1120) = master;
    PTR(realism, 0x1124) = object;
    PTR(realism, 0x1128) = AT(object, 0x12e8);
    PTR(reliability, 0x1118) = AT(master, 0x9c);
    PTR(reliability, 0x111c) = master;
    PTR(damage, 0x1118) = AT(master, 0xc0);
    PTR(engines, 0x111c) = AT(master, 0xec);
    PTR(engines, 0x1120) = master;
    PTR(fuel, 0x1120) = AT(master, 0x164);
    PTR(fuel, 0x1124) = master;
    PTR(fuel, 0x1128) = object;
    PTR(fuel, 0x112c) = AT(object, 0x12e8);
    PTR(systems, 0x1118) = AT(master, 0x2c8);
    PTR(systems, 0x111c) = master;
    PTR(fuel, 0x1134) = AT(object, 0x11ec);
    PTR(fuel, 0x1138) = AT(object, 0x10f0);
    PTR(realism, 0x112c) = AT(object, 0x11ec);
    PTR(realism, 0x1130) = AT(object, 0x10f0);
    PTR(object, 0x2264) = AT(object, 0x10f0);
}

int fs98_sim_settings_init(void *object)
{
    static const uint32_t s_kinds[] = {5, 6, 1, 2, 3, 4};
    static const uint32_t s_labels[] = {0xf7c7, 0xf7c8, 0xf7c3, 0xf7c4, 0xf7c5, 0xf7c6};
    static const uint32_t s_descriptors[] = {0x28, 0x9c, 0xc0, 0xec, 0x164, 0x2c8};
    struct fs98_sim_rect rect = {0, 0, 0, 0};
    void *vtable, *tabs = AT(object, 0x12e8), *font;
    uint32_t index, tab_module;
    char caption[0x1d];

    if (!fs98_sim_settings_validate(object))
        return 0;
    g_sim_services->base_init(object);
    vtable = PTR(object, 0);
    if (!((int (TC *)(void *, uint32_t, uint32_t, uint32_t))PTR(vtable, 0xe4))
        (object, 0x14f, g_sim_resource_module, 0xf))
        return 0;
    g_sim_services->init_art(AT(object, 0x1fb0), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    g_sim_services->init_button(AT(object, 0x10f0), AT(object, 0x5a0), AT(object, 0x610),
        0x132, AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff, g_sim_resource_module, 0, 0, rect);
    g_sim_services->init_button(AT(object, 0x11ec), AT(object, 0x5a0), AT(object, 0x610),
        0x132, AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff, g_sim_resource_module, 0, 0, rect);
    PTR(object, 0x2248) = fs98_sim_make_notify();
    tab_module = g_sim_resource_module;
    font = g_sim_services->base_font(0);
    g_sim_services->init_tabs(tabs, object, font, 0x148, AT(object, 0xff4), AT(object, 0x7e8),
        0, 0xc4c4c4, 5, 0x1d, 0x47, -1, tab_module, 0);
    ((void (TC *)(void *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))PTR(vtable, 0xe8))
        (object, 0x150, 0x1d, 0x47, 0x1a8, 0x121, g_sim_resource_module);
    for (index = 0; index < 6; ++index) {
        void *page = sim_make_page(object, s_kinds[index]);
        PTR(object, 0x224c + index * 4) = page;
        g_sim_services->init_page(page, 9);
    }
    for (index = 0; index < 6; ++index) {
        g_sim_services->load_string(D32(object, 0x80), s_labels[index], caption, 0x1d);
        g_sim_services->add_page(tabs, PTR(object, 0x224c + index * 4), caption,
            D32(PTR(object, 0x10ec), s_descriptors[index]));
    }
    for (index = 0; index < 6; ++index)
        g_sim_services->init_tab_helper(AT(object, 0x2098 + index * 0x48), tabs);
    if (!fs98_sim_settings_validate(object))
        return 0;
    g_sim_services->modify_style(AT(object, 0x11ec), 4, 0, 0);
    g_sim_services->window_rect(D32(object, 0x120c), (struct fs98_sim_rect *)AT(object, 0x10dc));
    g_sim_services->screen_to_client(D32(object, 0x20), (int *)AT(object, 0x10dc));
    g_sim_services->screen_to_client(D32(object, 0x20), (int *)AT(object, 0x10e4));
    fs98_sim_settings_wire(object);
    g_sim_services->last_page(tabs, PTR(object, 0x2260));
    g_sim_services->active_tab(tabs, D32(PTR(object, 0x10ec), 8), 0);
    PTR(object, 0x7c) = tabs;
    return 1;
}
