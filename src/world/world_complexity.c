/* World scenery-complexity core. Descriptor loads follow the retail service
 * boundaries; the owner callback validates through GLOBAL, not a World virtual. */
#include "world_complexity.h"
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
static const struct fs98_world_complexity_services *s_services;
void fs98_world_complexity_set_services(const struct fs98_world_complexity_services *services)
{ s_services = services; }
/* 0x223fb290 */
void fs98_world_complexity_collect(void *object)
{
    int selection;
    U32(PTR(object, 0x10e0), 0xc) = U32(object, 0x1648);
    U32(PTR(object, 0x10e0), 0x14) = U32(object, 0x1580);
    U32(PTR(object, 0x10e0), 0x1c) = U32(object, 0x1710);
    selection = s_services->send(U32(object, 0x149c), 0x147, 0, 0);
    U32(PTR(object, 0x10e0), 0x24) = (uint32_t)selection;
}
/* 0x223fb2f0: integers are passed through without boolean normalization. */
void fs98_world_complexity_populate(void *object)
{
    s_services->check(AT(object, 0x158c), (int)U32(PTR(object, 0x10e0), 0xc));
    s_services->check(AT(object, 0x14c4), (int)U32(PTR(object, 0x10e0), 0x14));
    s_services->check(AT(object, 0x1654), (int)U32(PTR(object, 0x10e0), 0x1c));
    s_services->send(U32(object, 0x149c), 0x14e, U32(PTR(object, 0x10e0), 0x24), 0);
}
/* 0x223fb360 */
int fs98_world_complexity_has_descriptor(void *object)
{ return PTR(object, 0x10e0) != 0; }
/* 0x223fb370: stdcall callback, one descriptor argument, return is observed. */
int fs98_world_complexity_check(void *object)
{
    void *descriptor = PTR(object, 0x10e0);
    return ((int (SC *)(void *))PTR(descriptor, 4))(descriptor);
}
/* 0x223f3720: shared inert error routine. */
int fs98_world_complexity_error(void *object)
{ (void)object; return 0; }
/* 0x223fb1f0: retain the vtable sampled before writing the two results. */
void fs98_world_complexity_cancel(void *object)
{
    void *descriptor = PTR(object, 0x10e0);
    void **vtable = PTR(object, 0);
    U32(object, 0x171c) = 2;
    U32(descriptor, 0) = 0;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}
/* 0x223fb210: nonzero validation calls the inert error routine, so every
 * normally returning callback still accepts. Reload descriptor/vtable after it. */
void fs98_world_complexity_accept(void *object)
{
    void *descriptor;
    void **vtable;
    U32(object, 0x171c) = 1;
    fs98_world_complexity_collect(object);
    if (fs98_world_complexity_check(object) && fs98_world_complexity_error(object)) return;
    descriptor = PTR(object, 0x10e0);
    vtable = PTR(object, 0);
    U32(descriptor, 0) = 1;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}
