/* World.DLL 0x223fc2c0..0x223fc811: shared add/edit scenery-area events.
 * Offsets describe the retail x86 ABI; services form the differential boundary.
 * Do not cache descriptor/vtable loads across callbacks unless retail does. */
#include "world_internal.h"
static const struct fs98_world_services *s_services;
typedef void (FS98_WORLD_THISCALL *world_close_fn)(void *object);
typedef int (FS98_WORLD_THISCALL *world_validate_fn)(void *object, uint32_t value);
typedef uint32_t (FS98_WORLD_STDCALL *world_check_fn)(void *descriptor);
void fs98_world_set_services(const struct fs98_world_services *services)
{
    s_services = services;
}
static void world_check(void *object)
{
    void *descriptor = WORLD_PTR(object, 0x10e4);
    ((world_check_fn)WORLD_PTR(descriptor, 4))(descriptor);
}
/* 0x223fc360 */
void fs98_world_area_collect(void *object)
{
    char position[0x20];
    void *descriptor = WORLD_PTR(object, 0x10e4);
    void *row = WORLD_PTR(descriptor, 0xc);
    int selection;
    s_services->get_text((uint8_t *)object + 0x19a0, position, 0x1f);
    selection = s_services->parse_int(position);
    WORLD_U32(WORLD_PTR(object, 0x10e4), 0x14) = (uint32_t)selection;
    s_services->get_text((uint8_t *)object + 0x1938, WORLD_PTR(row, 4), 0x80);
    WORLD_U32(row, 0x34) = WORLD_U32(object, 0x1b0c);
    selection = s_services->send(WORLD_U32(object, 0x1a28), 0x147, 0, 0);
    if ((uint32_t)selection <= 4)
        WORLD_U32(row, 0x2c) = (uint32_t)(selection + 1) * 10;
    s_services->get_text((uint8_t *)object + 0x1258, WORLD_PTR(row, 0x18), 0x80);
}
/* 0x223fc440 */
void fs98_world_area_populate(void *object)
{
    char position[0x20];
    void *descriptor = WORLD_PTR(object, 0x10e4);
    void *row = WORLD_PTR(descriptor, 0xc);
    uint32_t layer;
    s_services->format_int(position, "%04d", (int)WORLD_U32(descriptor, 0x14));
    layer = WORLD_U32(row, 0x2c);
    if (layer >= 10 && layer <= 50 && layer % 10 == 0)
        s_services->send(WORLD_U32(object, 0x1a28), 0x14e, layer / 10 - 1, 0);
    s_services->check((uint8_t *)object + 0x1a50, (int)WORLD_U32(row, 0x34));
    s_services->set_text((uint8_t *)object + 0x19a0, position);
    s_services->set_text((uint8_t *)object + 0x1938, WORLD_PTR(row, 4));
    s_services->set_text((uint8_t *)object + 0x1258, WORLD_PTR(row, 0x18));
    s_services->send(WORLD_U32(object, 0x1958), 0xcf, WORLD_U32(row, 0x3c), 0);
    s_services->send(WORLD_U32(object, 0x1278), 0xcf, WORLD_U32(row, 0x3c), 0);
    s_services->enable((uint8_t *)object + 0x1a50, WORLD_U32(row, 0x3c) == 0);
    s_services->enable((uint8_t *)object + 0x1a08, WORLD_U32(row, 0x3c) == 0);
    s_services->enable((uint8_t *)object + 0x1710, WORLD_U32(row, 0x3c) == 0);
}
/* 0x223fc2c0 */
void fs98_world_area_cancel(void *object)
{
    void *descriptor = WORLD_PTR(object, 0x10e4);
    void **vtable = WORLD_PTR(object, 0);
    WORLD_U32(descriptor, 0) = 0;
    ((world_close_fn)vtable[0xec / 4])(object);
}
/* 0x223fc2e0: keep the vtable used before validate, including its error/close. */
void fs98_world_area_accept(void *object)
{
    void *descriptor, *row;
    void **vtable;
    int error;
    fs98_world_area_collect(object);
    world_check(object);
    descriptor = WORLD_PTR(object, 0x10e4);
    row = WORLD_PTR(descriptor, 0xc);
    if (WORLD_U32(row, 0) != 0) {
        s_services->error(object, (uint8_t *)object + 0x1938, (void *)WORLD_U32(row, 0));
        s_services->focus_control((uint8_t *)object + 0x1258);
        return;
    }
    vtable = WORLD_PTR(object, 0);
    error = ((world_validate_fn)vtable[0xd8 / 4])(object, 0);
    if (error && ((world_validate_fn)vtable[0xdc / 4])(object, (uint32_t)error))
        return;
    WORLD_U32(WORLD_PTR(object, 0x10e4), 0) = 1;
    ((world_close_fn)vtable[0xec / 4])(object);
}
/* 0x223fc610: title error wins over position error; path checked by OK. */
int fs98_world_area_validate(void *object, uint32_t unused)
{
    void *descriptor, *row;
    (void)unused;
    fs98_world_area_collect(object);
    world_check(object);
    descriptor = WORLD_PTR(object, 0x10e4);
    row = WORLD_PTR(descriptor, 0xc);
    if (WORLD_U32(row, 0x14))
        return 0x1b07;
    return WORLD_U32(descriptor, 0x10) ? 0x1aef : 0;
}
/* 0x223fc660: unused is ignored even when zero. The pointer at +0x1c
 * deliberately outlives this stack buffer in retail; only consumed in Error. */
int fs98_world_area_error(void *object, uint32_t unused)
{
    char position[0x40];
    void *descriptor = WORLD_PTR(object, 0x10e4);
    void *row = WORLD_PTR(descriptor, 0xc);
    void **vtable;
    int control = 0;
    (void)unused;
    if (WORLD_U32(row, 0x14)) {
        if (s_services->error(object, (uint8_t *)object + 0x1258, (void *)WORLD_U32(row, 0x14)))
            control = 0x1b07;
    } else if (WORLD_U32(descriptor, 0x10)) {
        s_services->get_text((uint8_t *)object + 0x19a0, position, 0x40);
        descriptor = WORLD_PTR(object, 0x10e4);
        WORLD_U32(descriptor, 0x18) = WORLD_U32(descriptor, 0x10);
        WORLD_PTR(WORLD_PTR(object, 0x10e4), 0x1c) = position;
        descriptor = WORLD_PTR(object, 0x10e4);
        WORLD_U32(descriptor, 0x20) = WORLD_U32(descriptor, 0x2c);
        descriptor = WORLD_PTR(object, 0x10e4);
        if (s_services->error(object, (uint8_t *)object + 0x19a0, (uint8_t *)descriptor + 0x18))
            control = 0x1aef;
    }
    vtable = WORLD_PTR(object, 0);
    return ((world_validate_fn)vtable[0xe0 / 4])(object, (uint32_t)control);
}
/* 0x223fc720: no title/position collection here. */
void fs98_world_area_path_changed(void *object)
{
    static const uint32_t s_controls[] = {0x11a0, 0x1258, 0x1710, 0x1a08, 0x1a50, 0x17c8, 0x19a0};
    void *descriptor = WORLD_PTR(object, 0x10e4);
    void *row = WORLD_PTR(descriptor, 0xc);
    uint32_t index;
    int enabled;
    s_services->get_text((uint8_t *)object + 0x1938, WORLD_PTR(row, 4), 0x80);
    world_check(object);
    descriptor = WORLD_PTR(object, 0x10e4);
    row = WORLD_PTR(descriptor, 0xc);
    enabled = WORLD_U32(row, 0) == 0;
    for (index = 0; index < sizeof(s_controls) / sizeof(s_controls[0]); ++index)
        s_services->enable((uint8_t *)object + s_controls[index], enabled);
}

const struct fs98_world_services *fs98_world_get_services(void) { return s_services; }
