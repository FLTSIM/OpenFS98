/* World 0x13080001: scenery-area list core and control initialization.
 * Factories, embedded lifetimes and native binding remain separate milestones. */
#include "world_list.h"
#include "world_unwind.h"
#include "world_list_tables.inc"
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
static const struct fs98_world_list_services *s_services;
void fs98_world_list_set_services(const struct fs98_world_list_services *services)
{ s_services = services; }
static void list_callback(void *object, uint32_t offset)
{
    void *descriptor = PTR(object, 0x10dc);
    ((uint32_t (SC *)(void *))PTR(descriptor, offset))(descriptor);
}
/* 0x223fa870 */
void fs98_world_list_collect(void *object)
{
    int selection = s_services->events->send(U32(object, 0x1778), 0x188, 0, 0);
    U32(PTR(object, 0x10dc), 0x28) = (uint32_t)selection;
}
/* 0x223fac70. Compute protection once, retaining it across both enables. */
void fs98_world_list_selection(void *object)
{
    void *descriptor;
    uint32_t offset;
    int enabled;
    fs98_world_list_collect(object);
    descriptor = PTR(object, 0x10dc);
    offset = U32(descriptor, 0x28) << 6;
    enabled = U32((void *)((uintptr_t)PTR(descriptor, 0x24) + offset), 0x3c) == 0;
    s_services->events->enable(AT(object, 0x1998), enabled);
    s_services->events->enable(AT(object, 0x189c), enabled);
}
struct list_string_unwind { struct fs98_world_unwind frame; void *string; volatile int live; };
static void list_string_cleanup(struct fs98_world_unwind *frame)
{
    struct list_string_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; s_services->init->string_destroy(state->string); }
}
/* 0x223fa8a0. CString +0, saved selection +4, scratch +8 match retail's
 * adjacent locals. Invalid layers retain scratch, including an indeterminate
 * first row. Fixtures paint that owned local through the CString constructor. */
WORLD_NOINLINE void fs98_world_list_populate(void *object)
{
    const struct fs98_world_services *ui = s_services->events;
    const struct fs98_world_init_services *init = s_services->init;
    struct list_string_unwind state;
    struct { uint32_t string; int32_t selection; char text[100]; } data;
    uint32_t row_offset = 0;
    int32_t index = 0;
    state.string = &data.string; state.live = 0;
    world_unwind_enter(&state.frame, list_string_cleanup);
    init->string_construct(&data.string); state.live = 1;
    init->title(AT(object, 0x1500), PTR(PTR(object, 0x10dc), 0x1c));
    data.selection = ui->send(U32(object, 0x1778), 0x188, 0, 0);
    ui->send(U32(object, 0x1778), 0x184, 0, 0);
    while (index < (int32_t)U32(PTR(object, 0x10dc), 0x20)) {
        void *descriptor = PTR(object, 0x10dc);
        void *row = AT(PTR(descriptor, 0x24), row_offset);
        uint32_t layer;
        s_services->string_assign(&data.string, PTR(row, 0x18));
        descriptor = PTR(object, 0x10dc);
        row = AT(PTR(descriptor, 0x24), row_offset);
        layer = U32(row, 0x2c);
        if (layer >= 10 && layer <= 50 && layer % 10 == 0)
            init->load_string(U32(object, 0x80), s_list_layer_strings[layer / 10 - 1], data.text, 100);
        s_services->string_append(&data.string, "\t");
        s_services->string_append(&data.string, data.text);
        s_services->string_append(&data.string, "\t");
        ++index;
        ui->format_int(data.text, "%03d", index);
        s_services->string_append(&data.string, data.text);
        descriptor = PTR(object, 0x10dc);
        row = AT(PTR(descriptor, 0x24), row_offset);
        s_services->string_append(&data.string, U32(row, 0x34) ? "+" : "-");
        ui->send(U32(object, 0x1778), 0x180, 0, data.string);
        row_offset += 0x40;
    }
    if (ui->send(U32(object, 0x1778), 0x18b, 0, 0) > 0) {
        ui->enable(AT(object, 0x1b90), 1);
        ui->enable(AT(object, 0x189c), 1);
        ui->enable(AT(object, 0x1998), 1);
        U32(PTR(object, 0x10dc), 0x28) = data.selection == -1 ? 0 : (uint32_t)data.selection;
        ui->send(U32(object, 0x1778), 0x186, U32(PTR(object, 0x10dc), 0x28), 0);
        fs98_world_list_selection(object);
    } else {
        ui->enable(AT(object, 0x1b90), 0);
        ui->enable(AT(object, 0x189c), 0);
        ui->enable(AT(object, 0x1998), 0);
    }
    state.live = 0; init->string_destroy(&data.string);
    world_unwind_leave(&state.frame);
}
/* 0x223fab80 */
void fs98_world_list_accept(void *object)
{
    void **vtable;
    fs98_world_list_collect(object);
    U32(PTR(object, 0x10dc), 0) = 1;
    vtable = PTR(object, 0);
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}
/* 0x223faba0 leaves the descriptor answer unchanged. */
void fs98_world_list_cancel(void *object)
{
    void **vtable = PTR(object, 0);
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}
/* Add 0x223fabb0 (+8), Priority 0x223fabd0 (+0x18),
 * Edit 0x223fac30 (+0xc), Browse 0x223fac50 (+0x14). */
void fs98_world_list_action(void *object, uint32_t callback_offset)
{
    list_callback(object, callback_offset);
    fs98_world_list_populate(object);
}
/* 0x223fabf0 alone collects selection before invoking its owner. */
void fs98_world_list_delete(void *object)
{
    fs98_world_list_collect(object);
    list_callback(object, 0x10);
    fs98_world_list_populate(object);
}
/* 0x223fad10 discards LB_GETCURSEL and tests the descriptor's old selection. */
void fs98_world_list_double_click(void *object)
{
    void *descriptor;
    uint32_t offset;
    s_services->events->send(U32(object, 0x1778), 0x188, 0, 0);
    descriptor = PTR(object, 0x10dc);
    offset = U32(descriptor, 0x28) << 6;
    if (!U32((void *)((uintptr_t)PTR(descriptor, 0x24) + offset), 0x3c))
        fs98_world_list_action(object, 0xc);
}
/* 0x223fa5d0 */
int fs98_world_list_init(void *object)
{
    const struct fs98_world_init_services *init = s_services->init;
    static const uint32_t s_buttons[] = {0x17a0, 0x189c, 0x1998, 0x1a94, 0x1b90};
    struct fs98_world_rect rect = {0, 0, 0, 0};
    int tabs[2] = {0x118, 0x154};
    void **vtable;
    uint32_t index;
    init->base_init(object);
    vtable = PTR(object, 0);
    if (!((int (TC *)(void *, uint32_t, uint32_t, uint32_t))vtable[0xe4 / 4])(
            object, 0x14f, fs98_world_get_resource_module(), 15)) return 0;
    init->art(AT(object, 0x15b8), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    init->button(AT(object, 0x1198), AT(object, 0x5a0), AT(object, 0x610), 0x132, AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 0, 0, rect);
    init->button(AT(object, 0x1294), AT(object, 0x5a0), AT(object, 0x610), 0x132, AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 0, 0, rect);
    for (index = 0; index < sizeof(s_buttons) / sizeof(s_buttons[0]); ++index)
        init->button(AT(object, s_buttons[index]), AT(object, 0x680), AT(object, 0x6f0), 0x137, AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 1, 1, rect);
    init->text(AT(object, 0x16a0), AT(object, 0x370));
    s_services->list_init(AT(object, 0x1758), AT(object, 0x450), AT(object, 0x16a0));
    init->text(AT(object, 0x10e0), AT(object, 0x220));
    init->text(AT(object, 0x1500), AT(object, 0x220));
    init->text(AT(object, 0x1448), AT(object, 0x220));
    init->text(AT(object, 0x1390), AT(object, 0x220));
    s_services->font_tabs(AT(object, 0x450), 2, tabs);
    fs98_world_list_populate(object);
    return 1;
}
