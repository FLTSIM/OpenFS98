/* World 0x223fbef0: native control setup for the shared scenery-area class. */
#include "world_init.h"
#include "world_unwind.h"
#define AT(object, offset) ((uint8_t *)(object) + (offset))
static const struct fs98_world_init_services *s_init_services;
static uint32_t s_resource_module;
void fs98_world_set_init_services(const struct fs98_world_init_services *services) { s_init_services = services; }
void fs98_world_set_resource_module(uint32_t module) { s_resource_module = module; }
struct world_string_unwind { struct fs98_world_unwind frame; void *string; volatile int live; };
static void world_string_cleanup(struct fs98_world_unwind *frame)
{
    struct world_string_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; s_init_services->string_destroy(state->string); }
}
WORLD_NOINLINE int fs98_world_area_init(void *object)
{
    const struct fs98_world_services *services = fs98_world_get_services();
    void *descriptor, *row, *font;
    void **vtable;
    uint32_t index, string = 0;
    int enabled;
    struct fs98_world_rect rect = {0, 0, 0, 0};
    struct world_string_unwind state;
    static const uint32_t s_enable[] = {0x11a0, 0x1258, 0x1710, 0x1a08, 0x1a50, 0x17c8, 0x19a0};
    s_init_services->base_init(object);
    vtable = WORLD_PTR(object, 0);
    if (!((int (FS98_WORLD_THISCALL *)(void *, uint32_t, uint32_t, uint32_t))vtable[0xe4 / 4])(object, 0x14f, s_resource_module, 15)) return 0;
    descriptor = WORLD_PTR(object, 0x10e4);
    row = WORLD_PTR(descriptor, 0xc);
    if (!*(char *)WORLD_PTR(row, 4)) {
        state.string = &string; state.live = 0;
        world_unwind_enter(&state.frame, world_string_cleanup);
        s_init_services->string_construct(&string); state.live = 1;
        {
            char *buffer = s_init_services->string_buffer(&string, 0x100);
            s_init_services->load_string(WORLD_U32(object, 0x80), 0xf8d5, buffer, 0x100);
        }
        s_init_services->string_release(&string, -1);
        services->set_text(AT(object, 0x12c0), (char *)string);
        WORLD_U32(object, 0x10e0) = 0;
        state.live = 0; s_init_services->string_destroy(&string);
        world_unwind_leave(&state.frame);
    }
    s_init_services->art(AT(object, 0x12c0), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    s_init_services->button(AT(object, 0x155c), AT(object, 0x5a0), AT(object, 0x610), 0x132,
        AT(object, 0xff4), AT(object, 0x7e8), 1, -1, s_resource_module, 0, 0, rect);
    s_init_services->button(AT(object, 0x1460), AT(object, 0x5a0), AT(object, 0x610), 0x132,
        AT(object, 0xff4), AT(object, 0x7e8), 1, -1, s_resource_module, 0, 0, rect);
    s_init_services->text(AT(object, 0x1710), AT(object, 0x370));
    s_init_services->combo(AT(object, 0x1a08), AT(object, 0x3e0), AT(object, 0x1710));
    services->send(WORLD_U32(object, 0x1a28), 0x14e, 0, 0);
    s_init_services->text(AT(object, 0x1658), AT(object, 0x220));
    s_init_services->text(AT(object, 0x10e8), AT(object, 0x220));
    vtable = WORLD_PTR(AT(object, 0x1a50), 0);
    ((int (FS98_WORLD_THISCALL *)(void *, void *, uint32_t, void *, void *, uint32_t))vtable[0xc4 / 4])(
        AT(object, 0x1a50), AT(object, 0x4c0), 0x139, AT(object, 0xff4), AT(object, 0x7e8), s_resource_module);
    s_init_services->text(AT(object, 0x11a0), AT(object, 0x220));
    font = s_init_services->base_font(2);
    s_init_services->edit(AT(object, 0x1258), font, 0xffffff, 0, 0xff0000, AT(object, 0x17c8), 0);
    s_init_services->text(AT(object, 0x1880), AT(object, 0x220));
    font = s_init_services->base_font(2);
    s_init_services->edit(AT(object, 0x1938), font, 0xffffff, 0, 0xff0000, AT(object, 0x1880), 0);
    s_init_services->text(AT(object, 0x17c8), AT(object, 0x220));
    font = s_init_services->base_font(2);
    s_init_services->edit(AT(object, 0x19a0), font, 0xffffff, 0, 0xff0000, AT(object, 0x17c8), 1);
    descriptor = WORLD_PTR(object, 0x10e4); row = WORLD_PTR(descriptor, 0xc);
    services->send(WORLD_U32(object, 0x19c0), 0xc5, 4, 0);
    services->send(WORLD_U32(object, 0x1958), 0xc5, WORLD_U32(row, 8) - 1, 0);
    services->send(WORLD_U32(object, 0x1278), 0xc5, WORLD_U32(row, 0x1c) - 1, 0);
    descriptor = WORLD_PTR(object, 0x10e4);
    s_init_services->title(AT(object, 0x10e8), WORLD_PTR(descriptor, 8));
    fs98_world_area_populate(object);
    enabled = WORLD_U32(object, 0x10e0) != 0;
    for (index = 0; index < sizeof(s_enable) / sizeof(s_enable[0]); ++index)
        services->enable(AT(object, s_enable[index]), enabled);
    return 1;
}

const struct fs98_world_init_services *fs98_world_get_init_services(void) { return s_init_services; }
uint32_t fs98_world_get_resource_module(void) { return s_resource_module; }
