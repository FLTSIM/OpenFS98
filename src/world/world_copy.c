/* Copy weather (0x13050001). Raw selections/check states and service ordering
 * follow retail; applying the weather belongs to MAIN. */
#include "world_copy.h"
#define AT(p, n) ((uint8_t *)(p) + (n))
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
static const struct fs98_world_copy_services *s_services;
void fs98_world_copy_set_services(const struct fs98_world_copy_services *services)
{ s_services = services; }
/* 0x223f3e00 */
void fs98_world_copy_collect(void *object)
{
    int selection = s_services->send(U32(object, 0x14b0), 0x147, 0, 0);
    U32(PTR(object, 0x10dc), 0x1c) = (uint32_t)selection;
    selection = s_services->send(U32(object, 0x1468), 0x147, 0, 0);
    U32(PTR(object, 0x10dc), 0x20) = (uint32_t)selection;
    U32(PTR(object, 0x10dc), 0x28) = U32(object, 0x167c);
    U32(PTR(object, 0x10dc), 0x38) = U32(object, 0x1744);
    U32(PTR(object, 0x10dc), 0x50) = U32(object, 0x180c);
    U32(PTR(object, 0x10dc), 0x40) = U32(object, 0x18d4);
    U32(PTR(object, 0x10dc), 0x30) = U32(object, 0x199c);
    U32(PTR(object, 0x10dc), 0x48) = U32(object, 0x1a64);
}
/* 0x223f3ea0. Row +8 is a text pointer; both lists retain the row itself
 * as item data. Do not sort or normalize indices in World. */
void fs98_world_copy_populate(void *object)
{
    uint8_t *row;
    int index;
    static const uint32_t s_controls[] = {0x15c0, 0x1688, 0x1750, 0x1818, 0x18e0, 0x19a8};
    static const uint32_t s_fields[] = {0x28, 0x38, 0x50, 0x40, 0x30, 0x48};
    static const uint32_t s_first_disabled[] = {0x18e0, 0x1818, 0x19a8, 0x1750, 0x1818};
    static const uint32_t s_second_disabled[] = {0x1688, 0x1818, 0x19a8, 0x1750, 0x1818};
    s_services->send(U32(object, 0x14b0), 0x14b, 0, 0);
    s_services->send(U32(object, 0x1468), 0x14b, 0, 0);
    row = PTR(PTR(object, 0x10dc), 0x10);
    if ((int32_t)U32(PTR(object, 0x10dc), 0xc) > 0) {
        index = 0;
        do {
            int added = s_services->send(U32(object, 0x14b0), 0x143, 0, U32(row, 8));
            s_services->send(U32(object, 0x14b0), 0x151, (uint32_t)added, (uint32_t)row);
            added = s_services->send(U32(object, 0x1468), 0x143, 0, U32(row, 8));
            s_services->send(U32(object, 0x1468), 0x151, (uint32_t)added, (uint32_t)row);
            ++index;
            row += 0xc8;
        } while (index < (int32_t)U32(PTR(object, 0x10dc), 0xc));
    }
    s_services->send(U32(object, 0x14b0), 0x14e, U32(PTR(object, 0x10dc), 0x1c), 0);
    s_services->send(U32(object, 0x1468), 0x14e, U32(PTR(object, 0x10dc), 0x20), 0);
    for (index = 0; index < 6; ++index)
        s_services->check(AT(object, s_controls[index]), (int)U32(PTR(object, 0x10dc), s_fields[index]));
    if (U32(PTR(object, 0x10dc), 0x14))
        for (index = 0; index < 5; ++index) s_services->enable(AT(object, s_first_disabled[index]), 0);
    if (U32(PTR(object, 0x10dc), 0x18))
        for (index = 0; index < 5; ++index) s_services->enable(AT(object, s_second_disabled[index]), 0);
}
/* 0x223f40a0 */
int fs98_world_copy_has_descriptor(void *object)
{ return PTR(object, 0x10dc) != NULL && U32(object, 0x10dc) != 0xcdcdcdcd; }
/* 0x223f40c0: equal selections, including two -1s, are rejected. */
int fs98_world_copy_check(void *object)
{
    int first = s_services->send(U32(object, 0x14b0), 0x147, 0, 0);
    int second = s_services->send(U32(object, 0x1468), 0x147, 0, 0);
    return first == second ? 0x1012 : 0;
}
/* 0x223f4080: ignore the imported error result. */
int fs98_world_copy_error(void *object)
{
    s_services->error(object, AT(object, 0x1448), 0x1012);
    return 1;
}
/* 0x223f3d40 */
void fs98_world_copy_cancel(void *object)
{
    void *descriptor = PTR(object, 0x10dc);
    void **vtable = PTR(object, 0);
    U32(object, 0x1a70) = 2;
    U32(descriptor, 0) = 0;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}
/* 0x223f3d60 */
void fs98_world_copy_accept(void *object)
{
    U32(object, 0x1a70) = 1;
    fs98_world_copy_collect(object);
    if (!fs98_world_copy_check(object)) {
        void *descriptor = PTR(object, 0x10dc);
        void **vtable = PTR(object, 0);
        U32(descriptor, 0) = 1;
        ((void (TC *)(void *))vtable[0xec / 4])(object);
    } else {
        fs98_world_copy_error(object);
        s_services->post(U32(object, 0x20), 0x5fa, U32(object, 0x1468), 0);
    }
}
