/* World 0x13080006, CPerformanceBoosters. The descriptor and 0x80-byte
 * rows belong to FE; all external services are replaceable for comparison. */
#include "world_cache.h"
#include "world_unwind.h"
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
static const struct fs98_world_cache_services *s_services;
void fs98_world_cache_set_services(const struct fs98_world_cache_services *services)
{ s_services = services; }
static uint32_t cache_callback(void *object, uint32_t offset)
{
    void *descriptor = PTR(object, 0x10dc);
    return ((uint32_t (SC *)(void *))PTR(descriptor, offset))(descriptor);
}
/* 0x223f99f0 is empty. Neither accepting nor an owner action collects flags. */
void fs98_world_cache_collect(void *object) { (void)object; }
/* 0x223f9c20 leaves the check callback's return in eax. */
uint32_t fs98_world_cache_check(void *object) { return cache_callback(object, 4); }
/* 0x223f9c00 discards selection and always returns zero. */
int fs98_world_cache_validate(void *object)
{
    s_services->events->send(U32(object, 0x18e4), 0x188, 0, 0);
    return 0;
}
/* 0x223f9930: retain the UI index across all three title calls, reloading
 * the descriptor/rows each time. No range check, even for LB_ERR. */
static void cache_details(void *object, uint32_t offset)
{
    const struct fs98_world_init_services *init = s_services->init;
    init->title(AT(object, 0x14fc), PTR(AT(PTR(PTR(object, 0x10dc), 0x24), offset), 0x18));
    init->title(AT(object, 0x1724), PTR(AT(PTR(PTR(object, 0x10dc), 0x24), offset), 4));
    init->title(AT(object, 0x10e0), PTR(AT(PTR(PTR(object, 0x10dc), 0x24), offset), 0x40));
}
void fs98_world_cache_selection(void *object)
{
    uint32_t selection = s_services->events->send(U32(object, 0x18e4), 0x188, 0, 0);
    cache_details(object, selection << 7);
    U32(PTR(object, 0x10dc), 0x20) = selection;
}
struct cache_string_unwind { struct fs98_world_unwind frame; void *string; volatile int live; };
static void cache_string_cleanup(struct fs98_world_unwind *frame)
{
    struct cache_string_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; s_services->init->string_destroy(state->string); }
}
/* 0x223f9a00. Rows are name<TAB>marker; only active == 1 displays '+'.
 * The zero-count branch differs from signed-negative counts, which still
 * take the details/enabled branch despite adding no list entries. */
WORLD_NOINLINE void fs98_world_cache_populate(void *object)
{
    const struct fs98_world_services *ui = s_services->events;
    const struct fs98_world_init_services *init = s_services->init;
    struct cache_string_unwind state;
    uint32_t string, row_offset = 0;
    int32_t index = 0;
    state.string = &string; state.live = 0;
    world_unwind_enter(&state.frame, cache_string_cleanup);
    init->string_construct(&string); state.live = 1;
    ui->send(U32(object, 0x18e4), 0x184, 0, 0);
    while (index < (int32_t)U32(PTR(object, 0x10dc), 0x1c)) {
        void *row = AT(PTR(PTR(object, 0x10dc), 0x24), row_offset);
        s_services->string_assign(&string, PTR(row, 0x2c));
        s_services->string_append(&string, "\t");
        row = AT(PTR(PTR(object, 0x10dc), 0x24), row_offset);
        s_services->string_append(&string, U32(row, 0x54) == 1 ? "+" : "-");
        ui->send(U32(object, 0x18e4), 0x180, 0, string);
        ++index; row_offset += 0x80;
    }
    if (!U32(PTR(object, 0x10dc), 0x1c)) {
        ui->send(U32(object, 0x18e4), 0x186, U32(PTR(object, 0x10dc), 0x20), 0);
        init->title(AT(object, 0x14fc), NULL);
        init->title(AT(object, 0x1724), NULL);
        init->title(AT(object, 0x10e0), NULL);
        ui->focus_control(AT(object, 0x18c4));
        ui->enable(AT(object, 0x1c00), 1);
        ui->enable(AT(object, 0x190c), 1);
        ui->enable(AT(object, 0x1b04), 0);
        ui->enable(AT(object, 0x1a08), 0);
    } else {
        uint32_t selection = U32(PTR(object, 0x10dc), 0x20);
        ui->send(U32(object, 0x18e4), 0x186, selection, 0);
        cache_details(object, selection << 7);
        ui->focus_control(AT(object, 0x18c4));
        ui->enable(AT(object, 0x1c00), 1);
        ui->enable(AT(object, 0x1a08), 1);
        ui->enable(AT(object, 0x1b04), 1);
        ui->enable(AT(object, 0x190c), 1);
    }
    state.live = 0; init->string_destroy(&string);
    world_unwind_leave(&state.frame);
}
/* 0x223f97d0: check is direct, validation is direct and always zero.
 * The two calls are intentionally retained for their side effects. */
void fs98_world_cache_accept(void *object)
{
    void **vtable;
    U32(object, 0x1cfc) = 1;
    fs98_world_cache_collect(object);
    if (fs98_world_cache_check(object) && fs98_world_cache_validate(object)) return;
    U32(PTR(object, 0x10dc), 0) = 1;
    vtable = PTR(object, 0);
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}
/* 0x223f9810 loads the close vtable before storing the object result. */
void fs98_world_cache_cancel(void *object)
{
    void **vtable = PTR(object, 0);
    U32(object, 0x1cfc) = 2;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}
/* Add 0x223f9830 and Flush 0x223f9910 simply dispatch then populate.
 * Edit 0x223f98c0 preserves the UI index; Delete 0x223f9850 also adjusts
 * exactly when the new LB_GETCOUNT equals that index. Both callbacks see
 * the old descriptor selection, and neither refreshes details afterwards. */
void fs98_world_cache_action(void *object, uint32_t callback_offset)
{
    const struct fs98_world_services *ui = s_services->events;
    if (callback_offset == 0xc || callback_offset == 0x10) {
        uint32_t selection = ui->send(U32(object, 0x18e4), 0x188, 0, 0);
        cache_callback(object, callback_offset);
        fs98_world_cache_populate(object);
        if (callback_offset == 0x10 && (uint32_t)ui->send(U32(object, 0x18e4), 0x18b, 0, 0) == selection)
            --selection;
        ui->send(U32(object, 0x18e4), 0x186, selection, 0);
        U32(PTR(object, 0x10dc), 0x20) = selection;
    } else {
        cache_callback(object, callback_offset);
        fs98_world_cache_populate(object);
    }
}
void fs98_world_cache_delete(void *object) { fs98_world_cache_action(object, 0x10); }
/* 0x223f9560 */
int fs98_world_cache_init(void *object)
{
    const struct fs98_world_init_services *init = s_services->init;
    static const uint32_t s_buttons[] = {0x190c, 0x1a08, 0x1b04, 0x1c00};
    struct fs98_world_rect rect = {0, 0, 0, 0};
    int tabs[1] = {0xfa};
    void **vtable;
    uint32_t index;
    init->base_init(object);
    vtable = PTR(object, 0);
    if (!((int (TC *)(void *, uint32_t, uint32_t, uint32_t))vtable[0xe4 / 4])(
            object, 0x14f, fs98_world_get_resource_module(), 15)) return 0;
    init->art(AT(object, 0x17dc), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    init->button(AT(object, 0x124c), AT(object, 0x5a0), AT(object, 0x610), 0x132, AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 0, 0, rect);
    init->button(AT(object, 0x1348), AT(object, 0x5a0), AT(object, 0x610), 0x132, AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 0, 0, rect);
    for (index = 0; index < sizeof(s_buttons) / sizeof(s_buttons[0]); ++index)
        init->button(AT(object, s_buttons[index]), AT(object, 0x680), AT(object, 0x6f0), 0x137, AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 1, 1, rect);
    init->text(AT(object, 0x15b4), AT(object, 0x370));
    s_services->list_init(AT(object, 0x18c4), AT(object, 0x450), AT(object, 0x15b4));
    init->text(AT(object, 0x1444), AT(object, 0x220));
    init->text(AT(object, 0x166c), AT(object, 0x220));
    init->text(AT(object, 0x14fc), AT(object, 0x220));
    init->text(AT(object, 0x1724), AT(object, 0x220));
    s_services->group(AT(object, 0x1198), AT(object, 0x220));
    init->text(AT(object, 0x10e0), AT(object, 0x220));
    s_services->font_tabs(AT(object, 0x450), 1, tabs);
    fs98_world_cache_populate(object);
    return 1;
}
