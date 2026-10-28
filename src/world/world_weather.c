/* Basic weather 0x1302. MAIN owns the descriptor and applies accepted values. */
#include "world_weather.h"
#include "world_unwind.h"
#define AT(p, n) ((uint8_t *)(p) + (n))
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define API(slot, type) ((type)s_services->resolve(slot))
static const struct fs98_world_weather_services *s_services;
static struct fs98_world_weather_font_state s_font_state;
struct fs98_world_weather_font_state *fs98_world_weather_get_font_state(void) { return &s_font_state; }
void fs98_world_weather_set_services(const struct fs98_world_weather_services *services)
{ s_services = services; }
void fs98_world_weather_base_exchange(void *object, void *exchange)
{ API(0x257fc, void (TC *)(void *, void *))(object, exchange); }
static int world_send(void *object, uint32_t offset, uint32_t message, uint32_t first)
{ return API(0x258b0, int (SC *)(uint32_t, uint32_t, uint32_t, uint32_t))(U32(object, offset + 0x20), message, first, 0); }
static void world_enable(void *object, uint32_t offset, int enable)
{ API(0x255c8, int (TC *)(void *, int))(AT(object, offset), enable); }
/* 0x223f34f0: only the third combo's invalid index maps to 4. */
void fs98_world_weather_collect(void *object)
{
    int selection = world_send(object, 0x1634, 0x147, 0);
    U32(PTR(object, 0x10e0), 0x14) = (uint32_t)selection <= 4 ? (uint32_t)selection : 0;
    selection = world_send(object, 0x15a4, 0x147, 0);
    U32(PTR(object, 0x10e0), 0x1c) = (uint32_t)selection <= 3 ? (uint32_t)selection : 0;
    selection = world_send(object, 0x15ec, 0x147, 0);
    U32(PTR(object, 0x10e0), 0x18) = (uint32_t)selection <= 3 ? (uint32_t)selection : 4;
}
/* 0x223f3670: population only disables; it does not re-enable stale controls. */
void fs98_world_weather_populate(void *object)
{
    int selection;
    world_send(object, 0x1634, 0x14e, U32(PTR(object, 0x10e0), 0x14));
    world_send(object, 0x15a4, 0x14e, U32(PTR(object, 0x10e0), 0x1c));
    selection = world_send(object, 0x1634, 0x147, 0);
    if (!selection || world_send(object, 0x1634, 0x147, 0) == 4) {
        world_enable(object, 0x15ec, 0);
        world_enable(object, 0x1434, 0);
    } else world_send(object, 0x15ec, 0x14e, U32(PTR(object, 0x10e0), 0x18));
}
/* 0x223f3730 */
void fs98_world_weather_changed(void *object)
{
    int enable = 0;
    if (world_send(object, 0x1634, 0x147, 0))
        enable = world_send(object, 0x1634, 0x147, 0) != 4;
    world_enable(object, 0x15ec, enable);
    world_enable(object, 0x1434, enable);
}
/* 0x223fb360 and 0x223fb370. This descriptor accepts every non-null pointer. */
int fs98_world_weather_has_descriptor(void *object) { return PTR(object, 0x10e0) != NULL; }
int fs98_world_weather_validate(void *object)
{
    void *descriptor = PTR(object, 0x10e0);
    return ((int (SC *)(void *))PTR(descriptor, 4))(descriptor);
}
int fs98_world_weather_error(void *object) { (void)object; return 0; }
/* 0x223f3490: collect before the owner callback, which may replace the descriptor. */
void fs98_world_weather_accept(void *object)
{
    U32(object, 0x195c) = 1;
    fs98_world_weather_collect(object);
    if (!fs98_world_weather_validate(object) || !fs98_world_weather_error(object)) {
        void *descriptor = PTR(object, 0x10e0);
        void **vtable = PTR(object, 0);
        U32(descriptor, 0) = 1;
        ((void (TC *)(void *))vtable[0xec / 4])(object);
    }
}
/* 0x223f34d0 samples descriptor/vtable before the stores. */
void fs98_world_weather_cancel(void *object)
{
    void *descriptor = PTR(object, 0x10e0);
    void **vtable = PTR(object, 0);
    U32(object, 0x195c) = 2;
    U32(descriptor, 0) = 0;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}
/* 0x223f33e0 forwards the measure notification to the imported base. */
int fs98_world_weather_measure(void *object, uint32_t id, void *measure)
{ API(0x25788, void (TC *)(void *, uint32_t, void *))(object, id, measure); return 0; }
/* 0x223f3420 suppresses context help over the artwork. */
int fs98_world_weather_context(void *object, uint32_t window, uint32_t unused)
{
    int32_t point[2];
    struct fs98_world_rect rect;
    (void)unused;
    API(0x258a4, int (SC *)(void *))(point);
    API(0x258a8, int (SC *)(uint32_t, void *))(U32(object, 0x1894), &rect);
    if (API(0x258ac, int (SC *)(const void *, int32_t, int32_t))(&rect, point[0], point[1])) return 0;
    fs98_world_weather_context_map(window);
    return 1;
}
void fs98_world_weather_font_cleanup(void)
{ API(0x2579c, void (TC *)(void *))(s_font_state.font); }
struct world_weather_string_unwind {
    struct fs98_world_unwind frame;
    void *string;
    volatile int live;
};
static void world_weather_string_cleanup(struct fs98_world_unwind *frame)
{
    struct world_weather_string_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; API(0x255c0, void (TC *)(void *))(state->string); }
}
/* 0x223f3080: no CNotify; the temporary CString owns the artwork title. */
WORLD_NOINLINE int fs98_world_weather_init(void *object)
{
    const struct fs98_world_init_services *ui = s_services->ui;
    struct fs98_world_rect rect = {0, 0, 0, 0};
    struct world_weather_string_unwind state;
    uint32_t string = 0, index;
    void **vtable;
    static const uint32_t s_buttons[] = {0x167c, 0x1778};
    static const uint32_t s_labels[] = {0x137c, 0x1434, 0x14ec};
    static const uint32_t s_combos[] = {0x15a4, 0x15ec, 0x1634};
    ui->base_init(object);
    vtable = PTR(object, 0);
    if (!((int (TC *)(void *, uint32_t, uint32_t, uint32_t))vtable[0xe4 / 4])(
            object, 0x14f, s_services->resources(), 15)) return 0;
    U32(object, 0x84) = 11;
    U32(object, 0x10dc) = 1;
    ui->art(AT(object, 0x1874), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    state.string = &string; state.live = 0;
    world_unwind_enter(&state.frame, world_weather_string_cleanup);
    API(0x255b4, void (TC *)(void *))(&string); state.live = 1;
    API(0x255b8, void (TC *)(void *, void *))(AT(object, 0x1874), &string);
    API(0x255bc, void (TC *)(void *, const char *))(object, (char *)string);
    if (!(s_font_state.flags & 1)) {
        s_font_state.flags |= 1;
        API(0x2578c, void (TC *)(void *, void *))(s_font_state.font, ui->base_font(1));
        s_services->at_exit(fs98_world_weather_font_cleanup);
    }
    s_font_state.font[2] = 0xffffff; s_font_state.font[3] = 0; s_font_state.font[0x50 / 4] = 1; s_font_state.font[0x54 / 4] = 0;
    *(uint16_t *)AT(s_font_state.font, 0x58) = 4;
    for (index = 0; index < 2; ++index)
        ui->button(AT(object, s_buttons[index]), AT(object, 0x5a0), AT(object, 0x610), 0x132,
            AT(object, 0xff4), AT(object, 0x7e8), 1, -1, s_services->resources(), 0, 0, rect);
    for (index = 0; index < 3; ++index) ui->text(AT(object, s_labels[index]), s_font_state.font);
    ui->text(AT(object, 0x12c4), AT(object, 0x220));
    ui->title(AT(object, 0x12c4), PTR(PTR(object, 0x10e0), 8));
    for (index = 0; index < 3; ++index)
        ui->combo(AT(object, s_combos[index]), AT(object, 0x3e0), AT(object, s_labels[index]));
    for (index = 0; index < 3; ++index) world_send(object, s_combos[index], 0x14e, 0);
    if (U32(PTR(object, 0x10e0), 0xc)) {
        world_enable(object, 0x1634, 0); world_enable(object, 0x14ec, 0);
        ui->text(AT(object, 0x120c), AT(object, 0x220));
    } else API(0x25500, int (TC *)(void *, int))(AT(object, 0x120c), 0);
    if (U32(PTR(object, 0x10e0), 0x10)) {
        world_enable(object, 0x15a4, 0); world_enable(object, 0x137c, 0);
        ui->text(AT(object, 0x1154), AT(object, 0x220));
    } else API(0x25500, int (TC *)(void *, int))(AT(object, 0x1154), 0);
    if (!fs98_world_weather_has_descriptor(object)) {
        state.live = 0; API(0x255c0, void (TC *)(void *))(&string);
        world_unwind_leave(&state.frame); return 0;
    }
    fs98_world_weather_populate(object);
    state.live = 0; API(0x255c0, void (TC *)(void *))(&string);
    world_unwind_leave(&state.frame);
    return 1;
}
