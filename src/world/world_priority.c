/* World 0x223fcba0..0x223fd419: scenery-priority UI and owner callbacks. */
#include "world_priority.h"
#include "world_unwind.h"
#include "world_priority_tables.inc"
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
static const struct fs98_world_priority_services *s_services;
void fs98_world_priority_set_services(const struct fs98_world_priority_services *services) { s_services = services; }
static void priority_callback(void *object, uint32_t offset)
{
    void *descriptor = PTR(object, 0x10dc);
    ((uint32_t (SC *)(void *))PTR(descriptor, offset))(descriptor);
}
/* 0x223fcba0 */
int fs98_world_priority_init(void *object)
{
    const struct fs98_world_init_services *init = fs98_world_get_init_services();
    const struct fs98_world_services *ui = fs98_world_get_services();
    struct fs98_world_rect rect = {0, 0, 0, 0};
    int tabs[2] = {0x118, 0x154};
    void **vtable;
    void *font, *descriptor;
    init->base_init(object);
    vtable = PTR(object, 0);
    if (!((int (TC *)(void *, uint32_t, uint32_t, uint32_t))vtable[0xe4 / 4])(object, 0x14f, fs98_world_get_resource_module(), 15)) return 0;
    s_services->font_tabs(AT(object, 0x450), 2, tabs);
    init->art(AT(object, 0x14b0), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    init->button(AT(object, 0x12b8), AT(object, 0x5a0), AT(object, 0x610), 0x132, AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 0, 0, rect);
    init->button(AT(object, 0x13b4), AT(object, 0x5a0), AT(object, 0x610), 0x132, AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 0, 0, rect);
    init->text(AT(object, 0x1198), AT(object, 0x220));
    font = init->base_font(2);
    init->edit(AT(object, 0x1250), font, 0xffffff, 0, 0xff0000, AT(object, 0x1198), 0);
    init->button(AT(object, 0x1ab8), AT(object, 0x680), AT(object, 0x6f0), 0x137, AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 1, 1, rect);
    init->button(AT(object, 0x19bc), AT(object, 0x680), AT(object, 0x6f0), 0x137, AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 1, 1, rect);
    init->button(AT(object, 0x18c0), AT(object, 0x680), AT(object, 0x6f0), 0x137, AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 1, 1, rect);
    init->text(AT(object, 0x17c0), AT(object, 0x370));
    s_services->list_init(AT(object, 0x1878), AT(object, 0x450), AT(object, 0x17c0));
    init->text(AT(object, 0x10e0), AT(object, 0x220));
    init->text(AT(object, 0x1598), AT(object, 0x220));
    init->text(AT(object, 0x1708), AT(object, 0x220));
    init->text(AT(object, 0x1650), AT(object, 0x220));
    ui->send(U32(object, 0x1270), 0xc5, 3, 0);
    descriptor = PTR(object, 0x10dc);
    init->title(AT(object, 0x10e0), PTR(descriptor, 0x18));
    fs98_world_priority_populate(object);
    return 1;
}
void fs98_world_priority_action(void *object, uint32_t callback_offset)
{
    priority_callback(object, callback_offset);
    fs98_world_priority_populate(object);
}
/* 0x223fce90 */
void fs98_world_priority_selection(void *object)
{
    const struct fs98_world_services *ui = fs98_world_get_services();
    char text[32];
    uint32_t selected = (uint32_t)ui->send(U32(object, 0x1898), 0x188, 0, 0);
    void *descriptor = PTR(object, 0x10dc);
    U32(descriptor, 0x20) = selected;
    selected = U32(PTR(object, 0x10dc), 0x20);
    ui->format_int(text, "%03d", (int)(selected + 1));
    ui->set_text(AT(object, 0x1250), text);
    ui->enable(AT(object, 0x18c0), U32(PTR(object, 0x10dc), 0x20) != 0);
    selected = (uint32_t)ui->send(U32(object, 0x1898), 0x18b, 0, 0) - 1;
    ui->enable(AT(object, 0x19bc), U32(PTR(object, 0x10dc), 0x20) != selected);
}
/* 0x223fcfd0 */
void fs98_world_priority_collect(void *object)
{
    const struct fs98_world_services *ui = fs98_world_get_services();
    char text[32];
    int value;
    ui->get_text(AT(object, 0x1250), text, 0x1f);
    value = ui->parse_int(text);
    U32(PTR(object, 0x10dc), 0x2c) = (uint32_t)value;
    value = ui->send(U32(object, 0x1898), 0x188, 0, 0);
    U32(PTR(object, 0x10dc), 0x20) = (uint32_t)value;
}
struct priority_string_unwind { struct fs98_world_unwind frame; void *string; volatile int live; };
static void priority_string_cleanup(struct fs98_world_unwind *frame)
{
    struct priority_string_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; fs98_world_get_init_services()->string_destroy(state->string); }
}
/* 0x223fd030: the scratch buffer intentionally survives iterations. Invalid
 * later layers reuse the previous formatted position; an invalid first layer
 * reads uninitialized stack text. Keep CString followed by that text so the
 * guarded fixture can paint this indeterminate local at the constructor call. */
WORLD_NOINLINE void fs98_world_priority_populate(void *object)
{
    const struct fs98_world_init_services *init = fs98_world_get_init_services();
    const struct fs98_world_services *ui = fs98_world_get_services();
    struct priority_string_unwind state;
    struct { uint32_t string; char text[100]; } data;
    uint32_t row_offset = 0;
    int32_t index = 0;
    void *descriptor;
    ui->send(U32(object, 0x1898), 0x184, 0, 0);
    state.string = &data.string; state.live = 0;
    world_unwind_enter(&state.frame, priority_string_cleanup);
    init->string_construct(&data.string); state.live = 1;
    while (index < (int32_t)U32(PTR(object, 0x10dc), 0x1c)) {
        void *row;
        uint32_t layer;
        descriptor = PTR(object, 0x10dc);
        row = (uint8_t *)PTR(descriptor, 0x24) + row_offset;
        s_services->string_assign(&data.string, PTR(row, 0x18));
        descriptor = PTR(object, 0x10dc);
        row = (uint8_t *)PTR(descriptor, 0x24) + row_offset;
        layer = U32(row, 0x2c);
        if (layer >= 10 && layer <= 50 && layer % 10 == 0)
            init->load_string(U32(object, 0x80), s_priority_layer_strings[layer / 10 - 1], data.text, 100);
        s_services->string_append(&data.string, "\t");
        s_services->string_append(&data.string, data.text);
        s_services->string_append(&data.string, "\t");
        ++index;
        ui->format_int(data.text, "%03d", index);
        s_services->string_append(&data.string, data.text);
        descriptor = PTR(object, 0x10dc);
        row = (uint8_t *)PTR(descriptor, 0x24) + row_offset;
        s_services->string_append(&data.string, U32(row, 0x34) ? "+" : "-");
        ui->send(U32(object, 0x1898), 0x180, 0, data.string);
        row_offset += 0x40;
    }
    descriptor = PTR(object, 0x10dc);
    ui->send(U32(object, 0x1898), 0x186, U32(descriptor, 0x20), 0);
    if (!U32(PTR(object, 0x10dc), 0x20)) {
        ui->enable(AT(object, 0x18c0), 0);
        ui->focus_control(AT(object, 0x19bc));
    } else ui->enable(AT(object, 0x18c0), 1);
    {
        uint32_t last = (uint32_t)ui->send(U32(object, 0x1898), 0x18b, 0, 0) - 1;
        if (U32(PTR(object, 0x10dc), 0x20) == last) {
            ui->enable(AT(object, 0x19bc), 0);
            ui->focus_control(AT(object, 0x18c0));
        } else ui->enable(AT(object, 0x19bc), 1);
    }
    ui->format_int(data.text, "%03d", (int)(U32(PTR(object, 0x10dc), 0x20) + 1));
    ui->set_text(AT(object, 0x1250), data.text);
    state.live = 0; init->string_destroy(&data.string);
    world_unwind_leave(&state.frame);
}
/* 0x223fd390: the first LB_GETCURSEL result is intentionally discarded. */
int fs98_world_priority_validate(void *object, uint32_t unused)
{
    const struct fs98_world_services *ui = fs98_world_get_services();
    char text[32];
    int value;
    (void)unused;
    fs98_world_priority_collect(object);
    priority_callback(object, 4);
    if (U32(PTR(object, 0x10dc), 0x28)) return 0x1b08;
    ui->send(U32(object, 0x1898), 0x188, 0, 0);
    ui->get_text(AT(object, 0x1250), text, 0x1f);
    value = ui->parse_int(text);
    U32(PTR(object, 0x10dc), 0x2c) = (uint32_t)value;
    priority_callback(object, 0x14);
    fs98_world_priority_populate(object);
    return 0;
}
/* 0x223fd2f0 */
int fs98_world_priority_error(void *object, uint32_t unused)
{
    const struct fs98_world_services *ui = fs98_world_get_services();
    void *descriptor = PTR(object, 0x10dc);
    char text[64];
    uint32_t control = 0;
    (void)unused;
    if (U32(descriptor, 0x28)) {
        ui->get_text(AT(object, 0x1250), text, 0x40);
        descriptor = PTR(object, 0x10dc);
        U32(descriptor, 0x30) = U32(descriptor, 0x28);
        descriptor = PTR(object, 0x10dc); PTR(descriptor, 0x34) = text;
        {
            int count = ui->send(U32(object, 0x1898), 0x18b, 0, 0);
            U32(PTR(object, 0x10dc), 0x38) = (uint32_t)count;
        }
        descriptor = PTR(object, 0x10dc);
        U32(object, 0x8c) = (uint32_t)ui->error(object, AT(object, 0x1250), AT(descriptor, 0x30));
        if (U32(object, 0x8c)) control = 0x1b08;
    }
    return ((int (TC *)(void *, uint32_t))PTR(PTR(object, 0), 0xe0))(object, control);
}
/* 0x223fcf50 keeps its original virtual table across validation/error/close. */
void fs98_world_priority_accept(void *object)
{
    void **vtable = PTR(object, 0);
    int error = ((int (TC *)(void *, uint32_t))vtable[0xd8 / 4])(object, 0);
    if (error && ((int (TC *)(void *, uint32_t))vtable[0xdc / 4])(object, (uint32_t)error)) return;
    fs98_world_priority_collect(object);
    U32(PTR(object, 0x10dc), 0) = 1;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}
