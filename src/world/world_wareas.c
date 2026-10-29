/* Weather areas 0x1303. The independent owner retains weather application. */
#include "world_wareas.h"
#include "world_unwind.h"
#define AT(p, n) ((uint8_t *)(p) + (n))
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define API(slot, type) ((type)s_services->resolve(slot))
static const struct fs98_world_wareas_services *s_services;
static struct fs98_world_wareas_font_state s_font_state;
struct fs98_world_wareas_font_state *fs98_world_wareas_get_font_state(void) { return &s_font_state; }
void fs98_world_wareas_set_services(const struct fs98_world_wareas_services *services) { s_services = services; }
static int world_send(void *object, uint32_t message, uint32_t first, uint32_t second)
{ return API(0x258b0, int (SC *)(uint32_t, uint32_t, uint32_t, uint32_t))(U32(object, 0x1728), message, first, second); }
static void world_enable(void *object, uint32_t offset, int enable)
{ API(0x255c8, int (TC *)(void *, int))(AT(object, offset), enable); }
static void world_text(void *object, uint32_t offset, uint32_t field)
{ API(0x255bc, void (TC *)(void *, const char *))(AT(object, offset), PTR(PTR(object, 0x10fc), field)); }
static const uint32_t s_edits[] = {0x2388,0x2320,0x22b8,0x2250,0x23f0,0x2458,0x2528,0x24c0};
static const uint32_t s_fields[] = {0x1c,0x30,0x44,0x58,0x6c,0x80,0x94,0xa8};
static const uint32_t s_labels[] = {0x1bd8,0x1b20,0x1a68,0x19b0,0x1d48,0x1eb8,0x2198,0x2028,0x1c90,0x1e00,0x20e0,0x1f70};
/* The status priority differs from the physical order of the edit controls. */
static const uint32_t s_priority[] = {6,7,5,4,0,1,2,3};
static const uint32_t s_ids[] = {0x1ae6,0x1ae7,0x1af0,0x1af1,0x1ae5,0x1ae4,0x1ae2,0x1ae3};
void fs98_world_wareas_check(void *object, uint32_t field, uint32_t offset)
{ U32(PTR(object, 0x10fc), field) = U32(object, offset); }
/* 0x22401070: the item data is the live row, including after list rebuilding. */
void fs98_world_wareas_selection(void *object)
{
    uint32_t index, selection = world_send(object, 0x188, 0, 0);
    U32(object, 0x10f4) = selection;
    PTR(object, 0x10fc) = (void *)(uint32_t)world_send(object, 0x199, selection, 0);
    U32(PTR(object, 0x10f8), 0xc) = U32(object, 0x10f4);
    world_text(object, 0x1750, 8);
    for (index = 0; index < 8; ++index) world_text(object, s_edits[index], s_fields[index]);
    for (index = 0; index < 8; ++index) world_enable(object, s_edits[index], U32(PTR(object, 0x10fc), 0));
    for (index = 0; index < 12; ++index) world_enable(object, s_labels[index], U32(PTR(object, 0x10fc), 0));
    if (!U32(PTR(object, 0x10f8), 0xc)) {
        world_enable(object, 0x17b8, 0); world_enable(object, 0x1750, 0);
    } else {
        world_enable(object, 0x17b8, world_send(object, 0x18b, 0, 0) > 1);
        world_enable(object, 0x1750, 1);
    }
    API(0x2577c, int (TC *)(void *, int))(AT(object, 0x2658), U32(PTR(object, 0x10fc), 0xbc));
    API(0x2577c, int (TC *)(void *, int))(AT(object, 0x2590), U32(PTR(object, 0x10fc), 0xc4));
}
static void world_rows(void *object)
{
    int32_t index = 0;
    world_send(object, 0x184, 0, 0);
    PTR(object, 0x10fc) = PTR(PTR(object, 0x10f8), 0x14);
    while (index < (int32_t)U32(PTR(object, 0x10f8), 0x10)) {
        uint32_t row = world_send(object, 0x180, 0, U32(PTR(object, 0x10fc), 8));
        world_send(object, 0x19a, row, U32(object, 0x10fc));
        PTR(object, 0x10fc) = AT(PTR(object, 0x10fc), 0xc8);
        ++index;
    }
    world_send(object, 0x186, U32(PTR(object, 0x10f8), 0xc), 0);
    fs98_world_wareas_selection(object);
}
/* 0x22401860 */
void fs98_world_wareas_populate(void *object)
{
    U32(object, 0x10f4) = U32(PTR(object, 0x10f8), 0xc);
    world_rows(object);
    world_enable(object, 0x18b4, U32(PTR(object, 0x10f8), 0x18));
    world_enable(object, 0x1274, U32(PTR(object, 0x10f8), 0x1c));
    world_enable(object, 0x17b8, world_send(object, 0x18b, 0, 0) > 1);
}
void fs98_world_wareas_action(void *object, uint32_t action)
{
    void *descriptor;
    if (action == 0x28) world_enable(object, 0, 0);
    descriptor = PTR(object, 0x10f8);
    ((void (SC *)(void *))PTR(descriptor, action))(descriptor);
    if (action == 0x28) {
        world_enable(object, 0, 1);
        API(0x25590, void (TC *)(void *))(AT(object, 0x1274));
    }
    fs98_world_wareas_populate(object);
    if ((action == 0x20 && !U32(PTR(object, 0x10f8), 0x18)) ||
        (action == 0x24 && world_send(object, 0x18b, 0, 0) == 1))
        API(0x258a0, int (SC *)(uint32_t, uint32_t, uint32_t, uint32_t))(
            U32(object, 0x20), 0x5fa, U32(object, (action == 0x20 ? 0x17b8 : 0x18b4) + 0x20), 0);
}
void fs98_world_wareas_cancel(void *object)
{
    void *descriptor = PTR(object, 0x10f8);
    void **vtable = PTR(object, 0);
    U32(descriptor, 0) = 0;
    ((void (TC *)(void *))vtable[0xec/4])(object);
}
void fs98_world_wareas_accept(void *object)
{
    void **vtable = PTR(object, 0);
    int result = ((int (TC *)(void *, uint32_t))vtable[0xd8/4])(object, 0);
    if (!result || !((int (TC *)(void *, uint32_t))vtable[0xdc/4])(object, result)) {
        U32(PTR(object, 0x10f8), 0) = 1;
        ((void (TC *)(void *))vtable[0xec/4])(object);
    }
}
int fs98_world_wareas_validate(void *object, uint32_t id)
{
    uint32_t index;
    int result;
    void *descriptor;
    if (!U32(PTR(object, 0x10fc), 0)) return 0;
    if (id == 0x1b0b) {
        void *row = PTR(object, 0x10fc);
        API(0x25598, int (TC *)(void *, char *, int))(AT(object, 0x1750), PTR(row, 8), U32(row, 0xc));
        descriptor = PTR(object, 0x10f8);
        if (((int (SC *)(void *))PTR(descriptor, 4))(descriptor)) return 0x1b0b;
        world_rows(object);
        return 0;
    }
    for (index = 0; index < 8; ++index) {
        uint32_t edit = s_priority[index], field = s_fields[edit];
        void *row = PTR(object, 0x10fc);
        API(0x25598, int (TC *)(void *, char *, int))(AT(object, s_edits[edit]), PTR(row, field), U32(row, field + 4));
    }
    descriptor = PTR(object, 0x10f8);
    result = ((int (SC *)(void *))PTR(descriptor, 4))(descriptor);
    descriptor = PTR(object, 0x10fc);
    for (index = 0; index < 8; ++index) {
        uint32_t edit = s_priority[index];
        if (U32(descriptor, s_fields[edit] - 4)) return s_ids[edit];
    }
    return result;
}
int fs98_world_wareas_error(void *object, uint32_t id)
{
    uint32_t index, control = 0;
    void *row = PTR(object, 0x10fc);
    (void)id;
    for (index = 0; index < 9; ++index) {
        uint32_t edit = index ? s_priority[index - 1] : 0;
        uint32_t field = index ? s_fields[edit] - 4 : 4;
        if (U32(row, field)) {
            int result = API(0x25778, int (TC *)(void *, void *, void *))(
                object, AT(object, index ? s_edits[edit] : 0x1750), AT(row, field));
            U32(object, 0x8c) = result;
            if (result) control = index ? s_ids[edit] : 0x1b0b;
            break;
        }
    }
    return ((int (TC *)(void *, uint32_t))((void **)PTR(object, 0))[0xe0/4])(object, control);
}
int fs98_world_wareas_has_descriptor(void *object)
{
    uint32_t descriptor = U32(object, 0x10f8);
    if (descriptor && descriptor != 0xcdcdcdcd) return 1;
    API(0x257a8, void (*)(void *, const char *, const char *, uint32_t))(
        PTR(object, 0x10dc), "In WeatherArea::InitializeData", "Bad m_pliveInfo = %lx\npassed to dialog by core code", descriptor);
    return 0;
}
void fs98_world_wareas_font_cleanup(void)
{ API(0x2579c, void (TC *)(void *))(s_font_state.font); }
struct world_notify_unwind { struct fs98_world_unwind frame; void *object; volatile int live; };
static void world_notify_cleanup(struct fs98_world_unwind *frame)
{
    struct world_notify_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; API(0x257f4, void (SC *)(void *))(state->object); }
}
WORLD_NOINLINE int fs98_world_wareas_init(void *object)
{
    const struct fs98_world_init_services *ui = s_services->ui;
    struct fs98_world_rect rect = {0,0,0,0};
    struct world_notify_unwind state;
    uint32_t index;
    void *notify, *row;
    void **vtable;
    static const uint32_t s_text_offsets[] = {0x2198,0x20e0,0x2028,0x1f70,0x1eb8,0x1e00,0x1d48,0x1c90,0x1bd8,0x1b20,0x1a68,0x19b0};
    static const uint32_t s_text_fonts[] = {0x300,0x220,0x300,0x220,0x300,0x220,0x300,0x220,0x300,0x300,0x300,0x300};
    static const uint32_t s_setup_edits[] = {0x2388,0x2320,0x22b8,0x2250,0x23f0,0x2458,0x24c0,0x2528,0x1750};
    static const uint32_t s_edit_labels[] = {0x1bd8,0x1b20,0x1a68,0x19b0,0x1d48,0x1eb8,0x2028,0x2198,0};
    static const uint32_t s_edit_flags[] = {0x30,0x50,0x30,0x50,0x11,0x11,0x11,0x15,0};
    ui->base_init(object);
    vtable = PTR(object, 0);
    if (!((int (TC *)(void *, uint32_t, uint32_t, uint32_t))vtable[0xe4/4])(object, 0x14f, s_services->resources(), 15)) return 0;
    if (!(s_font_state.flags & 1)) {
        s_font_state.flags |= 1;
        API(0x2578c, void (TC *)(void *, void *))(s_font_state.font, ui->base_font(8));
        s_services->at_exit(fs98_world_wareas_font_cleanup);
    }
    s_font_state.font[2] = U32(object, 0x228); s_font_state.font[3] = U32(object, 0x22c);
    s_font_state.font[0x50/4] = U32(object, 0x270); s_font_state.font[0x54/4] = U32(object, 0x274);
    *(uint16_t *)AT(s_font_state.font, 0x58) = 0x110;
    ui->art(AT(object, 0x1620), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    for (index = 0; index < 5; ++index) {
        static const uint32_t s_buttons[] = {0x1370,0x146c,0x17b8,0x18b4,0x1274};
        int small = index >= 2;
        ui->button(AT(object, s_buttons[index]), AT(object, small ? 0x680 : 0x5a0), AT(object, small ? 0x6f0 : 0x610), small ? 0x130 : 0x132,
            AT(object, 0xff4), AT(object, 0x7e8), 1, -1, s_services->resources(), small, small, rect);
    }
    PTR(object, 0x1100) = AT(object, 0x146c);
    API(0x25890, int (SC *)(uint32_t, uint32_t, char *, int))(U32(object, 0x80), U32(PTR(object, 0x10f8), 8) ? 0xf8f6 : 0xf90f, (char *)AT(object, 0x10e0), 0x13);
    ui->title(AT(object, 0x1e00), (char *)AT(object, 0x10e0));
    ui->title(AT(object, 0x1c90), (char *)AT(object, 0x10e0));
    for (index = 0; index < 12; ++index) ui->text(AT(object, s_text_offsets[index]), AT(object, s_text_fonts[index]));
    ui->text(AT(object, 0x11bc), s_font_state.font); ui->text(AT(object, 0x1568), AT(object, 0x370));
    API(0x25718, void (TC *)(void *, void *, void *))(AT(object, 0x1708), AT(object, 0x450), AT(object, 0x1568));
    for (index = 0; index < 9; ++index)
        ui->edit(AT(object, s_setup_edits[index]), ui->base_font(2), 0xffffff, 0, 0xff0000,
            s_edit_labels[index] ? AT(object, s_edit_labels[index]) : NULL, s_edit_flags[index]);
    API(0x25730, void (TC *)(void *, void *))(AT(object, 0x1108), AT(object, 0x4c0));
    for (index = 0; index < 2; ++index) {
        void *check = AT(object, index ? 0x2658 : 0x2590);
        ((int (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))((void **)PTR(check,0))[0xc4/4])(
            check, AT(object,0x4c0),0x139,AT(object,0xff4),AT(object,0x7e8),s_services->resources());
    }
    state.object = NULL; state.live = 0;
    world_unwind_enter(&state.frame, world_notify_cleanup);
    notify = API(0x2584c, void *(*)(uint32_t))(0x29c);
    state.object = notify; state.live = 1;
    if (notify) {
        API(0x257cc, void *(TC *)(void *, uint32_t, uint32_t))(notify,0x30,1);
        PTR(notify,0) = s_services->notify_vtable;
    }
    state.live = 0; PTR(object,0x10dc) = notify;
    if (!fs98_world_wareas_has_descriptor(object)) { world_unwind_leave(&state.frame); return 0; }
    row = PTR(PTR(object,0x10f8),0x14); PTR(object,0x10fc) = row;
    U32(object,0x17b4) = U32(row,0xc)-1;
    U32(object,0x23ec) = U32(PTR(object,0x10fc),0x20)-1;
    U32(object,0x2384) = U32(PTR(object,0x10fc),0x34)-1;
    U32(object,0x231c) = U32(PTR(object,0x10fc),0x48)-1;
    U32(object,0x22b4) = U32(PTR(object,0x10fc),0x5c)-1;
    fs98_world_wareas_populate(object);
    U32(object,0x2454) = API(0x254dc, int (SC *)(const char *))(PTR(PTR(object,0x10fc),0x6c));
    U32(object,0x24bc) = API(0x254dc, int (SC *)(const char *))(PTR(PTR(object,0x10fc),0x80));
    U32(object,0x258c) = API(0x254dc, int (SC *)(const char *))(PTR(PTR(object,0x10fc),0x94));
    U32(object,0x2524) = API(0x254dc, int (SC *)(const char *))(PTR(PTR(object,0x10fc),0xa8));
    PTR(object,0x1100) = AT(object,0x146c); PTR(object,0x1104) = AT(object,0x1370);
    world_unwind_leave(&state.frame); return 1;
}

int fs98_world_wareas_context(void *object, uint32_t window, uint32_t unused)
{ (void)object; (void)unused; fs98_world_wareas_context_map(window); return 1; }
