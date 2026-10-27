/* World 0x1306: FE-owned world-files container, template 0x6b6. */
#include "world_files.h"
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
static const struct fs98_world_files_services *s_services;
void fs98_world_files_set_services(const struct fs98_world_files_services *services)
{ s_services = services; }
/* Shared retail Collect 0x223f99f0 is empty; Check is 0x223f9c20. */
void fs98_world_files_collect(void *object) { (void)object; }
uint32_t fs98_world_files_check(void *object)
{
    void *descriptor = PTR(object, 0x10dc);
    return ((uint32_t (SC *)(void *))PTR(descriptor, 4))(descriptor);
}
/* 0x223f3720 always returns zero and has no external calls. */
int fs98_world_files_validate(void *object) { (void)object; return 0; }
/* 0x223fd870 / 0x223fd880: no collect or population after the callback. */
void fs98_world_files_action(void *object, uint32_t callback_offset)
{
    void *descriptor = PTR(object, 0x10dc);
    ((uint32_t (SC *)(void *))PTR(descriptor, callback_offset))(descriptor);
}
static void *files_row(void *object, uint32_t index)
{ return (void *)((uintptr_t)PTR(PTR(object, 0x10dc), 0x14) + (index << 6)); }
/* 0x223fd9c0 reloads selection, descriptor and rows between enables and
 * forwards the raw row flag to EnableWindow, including noncanonical values. */
void fs98_world_files_enable(void *object)
{
    uint32_t selected = U32(PTR(object, 0x10dc), 0x18);
    s_services->events->enable(AT(object, 0x1728), U32(files_row(object, selected), 0));
    selected = U32(PTR(object, 0x10dc), 0x18);
    s_services->events->enable(AT(object, 0x162c), U32(files_row(object, selected), 0));
}
/* 0x223fd930: append without reset; retain original signed count and index,
 * but reload descriptor/rows for each append and the final detail read. */
void fs98_world_files_populate(void *object)
{
    const struct fs98_world_services *ui = s_services->events;
    void *descriptor = PTR(object, 0x10dc);
    int32_t count = U32(descriptor, 0x10), index;
    uint32_t selected = U32(descriptor, 0x18);
    for (index = 0; index < count; ++index)
        ui->send(U32(object, 0x154c), 0x180, 0, U32(files_row(object, (uint32_t)index), 0x1c));
    ui->send(U32(object, 0x154c), 0x186, selected, 0);
    s_services->init->title(AT(object, 0x1574), PTR(files_row(object, selected), 0x30));
    fs98_world_files_enable(object);
}
/* 0x223fda10 updates detail before storing the saved UI selection. */
void fs98_world_files_selection(void *object)
{
    uint32_t selected = s_services->events->send(U32(object, 0x154c), 0x188, 0, 0);
    s_services->init->title(AT(object, 0x1574), PTR(files_row(object, selected), 0x30));
    U32(PTR(object, 0x10dc), 0x18) = selected;
    fs98_world_files_enable(object);
}
/* 0x223fd8b0 */
void fs98_world_files_accept(void *object)
{
    void **vtable;
    U32(object, 0x1824) = 1;
    fs98_world_files_collect(object);
    if (fs98_world_files_check(object) && fs98_world_files_validate(object)) return;
    U32(PTR(object, 0x10dc), 0) = 1;
    vtable = PTR(object, 0);
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}
/* 0x223fd890 retains the vtable across the local-result store. */
void fs98_world_files_cancel(void *object)
{
    void **vtable = PTR(object, 0);
    U32(object, 0x1824) = 2;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}
/* 0x223fda60: WM_ERASEBKGND posts WM_SETFOCUS to the list, then calls the
 * imported CSimsDialog background handler, retaining its return value. */
int fs98_world_files_erase(void *object, void *dc)
{
    s_services->post(U32(object, 0x154c), 7, 0, 0);
    return s_services->erase(object, dc);
}
/* 0x223fd6c0 */
int fs98_world_files_init(void *object)
{
    const struct fs98_world_init_services *init = s_services->init;
    static const uint32_t s_buttons[] = {0x1728, 0x162c};
    struct fs98_world_rect rect = {0, 0, 0, 0};
    void **vtable;
    uint32_t index;
    init->base_init(object);
    vtable = PTR(object, 0);
    if (!((int (TC *)(void *, uint32_t, uint32_t, uint32_t))vtable[0xe4 / 4])(
            object, 0x14f, fs98_world_get_resource_module(), 15)) return 0;
    init->art(AT(object, 0x1390), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    init->button(AT(object, 0x10e0), AT(object, 0x5a0), AT(object, 0x610), 0x132, AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 0, 0, rect);
    init->button(AT(object, 0x11dc), AT(object, 0x5a0), AT(object, 0x610), 0x132, AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 0, 0, rect);
    for (index = 0; index < sizeof(s_buttons) / sizeof(s_buttons[0]); ++index)
        init->button(AT(object, s_buttons[index]), AT(object, 0x680), AT(object, 0x6f0), 0x137, AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 1, 1, rect);
    init->text(AT(object, 0x12d8), AT(object, 0x370));
    s_services->list_init(AT(object, 0x152c), AT(object, 0x450), AT(object, 0x12d8));
    s_services->group(AT(object, 0x1478), AT(object, 0x220));
    init->text(AT(object, 0x1574), AT(object, 0x220));
    fs98_world_files_populate(object);
    return 1;
}
