/* World 0x1308: exact-position text exchanges and owner dispatch.
 * Numeric parsing and aircraft placement belong to the owner, not World. */
#include "world_position.h"
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
static const struct fs98_world_position_services *s_services;
void fs98_world_position_set_services(const struct fs98_world_position_services *services)
{ s_services = services; }
struct position_field { uint32_t control, status, snapshot; };
static const struct position_field s_fields[] = {
    {0x197c, 0x64, 0x1145}, {0x1a9c, 0x7c, 0x11aa},
    {0x1bbc, 0x94, 0x10e0}, {0x1cdc, 0xac, 0x120f},
    {0x1e3c, 0xc0, 0x1274}, {0x2050, 0x20, 0x133e},
    {0x2170, 0x38, 0x13a3}, {0x2290, 0x50, 0x12d9}
};
static void position_callback(void *object, uint32_t offset)
{
    void *descriptor = PTR(object, 0x10dc);
    ((uint32_t (SC *)(void *))PTR(descriptor, offset))(descriptor);
}
/* 0x22404950: descriptors, pointers and capacities are reloaded per call. */
void fs98_world_position_collect(void *object)
{
    static const uint8_t s_order[] = {2, 0, 1, 3, 4, 7, 5, 6};
    uint32_t index;
    for (index = 0; index < 8; ++index) {
        const struct position_field *field = &s_fields[s_order[index]];
        void *descriptor = PTR(object, 0x10dc);
        s_services->events->get_text(AT(object, field->control), PTR(descriptor, field->status + 4), U32(descriptor, field->status + 8));
    }
}
/* 0x224043f0 */
void fs98_world_position_populate(void *object)
{
    uint32_t index;
    for (index = 0; index < 8; ++index)
        s_services->events->set_text(AT(object, s_fields[index].control), PTR(PTR(object, 0x10dc), s_fields[index].status + 4));
    s_services->events->enable(AT(object, 0x1714), U32(PTR(object, 0x10dc), 0x18) == 0);
}
/* 0x224047c0: retail inlines eight unbounded copies, including their NULs. */
void fs98_world_position_snapshot(void *object, uint32_t enabled)
{
    uint32_t index;
    if (!enabled) return;
    for (index = 0; index < 8; ++index) {
        const char *source = PTR(PTR(object, 0x10dc), s_fields[index].status + 4);
        char *destination = (char *)AT(object, s_fields[index].snapshot);
        do { *destination++ = *source; } while (*source++);
    }
}
/* 0x224044e0: latitude/longitude/altitude precede other coordinates. */
int fs98_world_position_validate(void *object, uint32_t unused)
{
    static const uint16_t s_status[] = {0x50, 0x94, 0xac, 0x20, 0x38, 0x64, 0x7c, 0xc0};
    static const uint16_t s_controls[] = {0x119d, 0x11a4, 0x119f, 0x11a1, 0x11a2, 0x119c, 0x11a3, 0x119e};
    uint32_t index; void *descriptor; void **vtable = PTR(object, 0);
    (void)unused;
    ((void (TC *)(void *))vtable[0xf4 / 4])(object);
    position_callback(object, 4);
    descriptor = PTR(object, 0x10dc);
    for (index = 0; index < 8; ++index) if (U32(descriptor, s_status[index])) return s_controls[index];
    return 0;
}
/* 0x224045a0 has a different priority from Validate. Its supplied argument
 * is ignored. Earlier statuses capture the descriptor once; focus uses the
 * current vtable after DoErrorDialog, including when no error exists. */
int fs98_world_position_error(void *object, uint32_t unused)
{
    static const uint8_t s_order[] = {2, 7, 3, 5, 6, 4, 0, 1};
    static const uint16_t s_controls[] = {0x11a4, 0x119d, 0x119f, 0x11a1, 0x11a2, 0x119e, 0x119c, 0x11a3};
    uint32_t index, control = 0;
    void *descriptor = PTR(object, 0x10dc);
    void **vtable;
    (void)unused;
    for (index = 0; index < 8; ++index) {
        const struct position_field *field = &s_fields[s_order[index]];
        if (U32(descriptor, field->status)) {
            if (s_services->events->error(object, AT(object, field->control), AT(descriptor, field->status))) control = s_controls[index];
            break;
        }
    }
    vtable = PTR(object, 0);
    return ((int (TC *)(void *, uint32_t))vtable[0xe0 / 4])(object, control);
}
/* 0x22404770 */
void fs98_world_position_reload(void *object)
{
    void **vtable;
    position_callback(object, 0x10);
    vtable = PTR(object, 0);
    ((void (TC *)(void *))vtable[0xf0 / 4])(object);
}
/* 0x22404790 suppresses five change notifications during virtual population. */
void fs98_world_position_revert(void *object)
{
    void **vtable;
    position_callback(object, 0xc);
    vtable = PTR(object, 0);
    U32(object, 0x26e8) = 0;
    ((void (TC *)(void *))vtable[0xf0 / 4])(object);
    U32(object, 0x26e8) = 1;
}
void fs98_world_position_facilities(void *object) { position_callback(object, 0x14); }
/* Five EN_CHANGE handlers, 0x22404a90..0x22404b10. */
void fs98_world_position_changed(void *object, uint32_t offset)
{ if (U32(object, 0x26e8) == 1) U32(PTR(object, 0x10dc), offset) = 1; }
/* 0x22404a40 retains one vtable across Validate, Error and Close. Error is
 * called even when validation returns zero, and its return decides closing. */
void fs98_world_position_accept(void *object)
{
    void **vtable = PTR(object, 0);
    int control = ((int (TC *)(void *, uint32_t))vtable[0xd8 / 4])(object, 0);
    if (!((int (TC *)(void *, uint32_t))vtable[0xdc / 4])(object, control)) {
        position_callback(object, 8);
        U32(PTR(object, 0x10dc), 0) = 1;
        ((void (TC *)(void *))vtable[0xec / 4])(object);
    }
}
/* Shared retail Cancel 0x223fe4e0. */
void fs98_world_position_cancel(void *object)
{
    void *descriptor = PTR(object, 0x10dc); void **vtable = PTR(object, 0);
    U32(descriptor, 0) = 0;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}
static void position_edit(void *object, uint32_t control, uint32_t label, uint32_t flags)
{
    void *font = s_services->init->base_font(2);
    s_services->init->edit(AT(object, control), font, 0xffffff, 0, 0xff0000, AT(object, label), flags);
}
static void position_spin(void *object, uint32_t offset, uint32_t range)
{
    struct fs98_world_rect rect;
    s_services->remap(&rect, AT(object, offset));
    s_services->move(AT(object, offset), rect.left, rect.top,
        (uint32_t)rect.right - (uint32_t)rect.left, (uint32_t)rect.bottom - (uint32_t)rect.top, 1);
    s_services->events->send(U32(object, offset + 0x20), 0x465, 0, range);
}
/* 0x22403e10 returns zero on both success and failure. It retains the skin
 * vtable for its later virtual Populate, but focuses the first edit directly. */
int fs98_world_position_init(void *object)
{
    const struct fs98_world_init_services *init = s_services->init;
    const struct fs98_world_services *ui = s_services->events;
    static const uint32_t s_buttons[] = {0x1714, 0x23f4, 0x24f0, 0x25ec};
    static const uint8_t s_limits[] = {0, 1, 2, 3, 4, 5, 6, 7};
    struct fs98_world_rect rect = {0, 0, 0, 0};
    uint32_t index;
    void **vtable;
    init->base_init(object); vtable = PTR(object, 0);
    if (!((int (TC *)(void *, uint32_t, uint32_t, uint32_t))vtable[0xe4 / 4])(
            object, 0x14f, fs98_world_get_resource_module(), 15)) return 0;
    init->art(AT(object, 0x1408), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    init->text(AT(object, 0x14f0), AT(object, 0x220));
    s_services->group(AT(object, 0x15a8), AT(object, 0x220));
    init->text(AT(object, 0x165c), AT(object, 0x220));
    for (index = 0; index < 4; ++index)
        init->button(AT(object, s_buttons[index]), AT(object, 0x680), AT(object, 0x6f0), index < 2 ? 0x137 : 0x132,
            AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 1, 1, rect);
    s_services->group(AT(object, 0x1810), AT(object, 0x220));
    init->text(AT(object, 0x18c4), AT(object, 0x220)); position_edit(object, 0x197c, 0x18c4, 0);
    init->text(AT(object, 0x19e4), AT(object, 0x220)); position_edit(object, 0x1a9c, 0x19e4, 0);
    init->text(AT(object, 0x1b04), AT(object, 0x370)); position_edit(object, 0x1bbc, 0x1b04, 3);
    init->text(AT(object, 0x1c24), AT(object, 0x370)); position_edit(object, 0x1cdc, 0x1c24, 0);
    position_spin(object, 0x1d44, 0x10168);
    init->text(AT(object, 0x1d84), AT(object, 0x370)); position_edit(object, 0x1e3c, 0x1d84, 0);
    position_spin(object, 0x1ea4, 0x1f4);
    s_services->group(AT(object, 0x1ee4), AT(object, 0x220));
    init->text(AT(object, 0x1f98), AT(object, 0x220)); position_edit(object, 0x2050, 0x1f98, 0);
    init->text(AT(object, 0x20b8), AT(object, 0x220)); position_edit(object, 0x2170, 0x20b8, 0);
    init->text(AT(object, 0x21d8), AT(object, 0x370)); position_edit(object, 0x2290, 0x21d8, 3);
    init->button(AT(object, 0x22f8), AT(object, 0x680), AT(object, 0x6f0), 0x137, AT(object, 0xff4), AT(object, 0x7e8), 1, -1, fs98_world_get_resource_module(), 1, 1, rect);
    if (!PTR(object, 0x10dc) || U32(object, 0x10dc) == 0xcdcdcdcd) {
        s_services->message("Bad m_pliveInfo In CGoToCoordinates OnInitDialog", 0, 0);
        return 0;
    }
    for (index = 0; index < 8; ++index) {
        const struct position_field *field = &s_fields[s_limits[index]];
        ui->send(U32(object, field->control + 0x20), 0xc5, U32(PTR(object, 0x10dc), field->status + 8) - 1, 0);
    }
    U32(object, 0x26e8) = 0;
    ((void (TC *)(void *))vtable[0xf0 / 4])(object);
    U32(object, 0x26e8) = 1;
    fs98_world_position_snapshot(object, 1);
    ui->focus_control(AT(object, 0x197c));
    return 0;
}
