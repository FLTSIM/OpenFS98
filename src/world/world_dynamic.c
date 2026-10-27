/* World dynamic-scenery core. Descriptor loads follow the retail service
 * boundaries; the owner callback validates through GLOBAL, not a World virtual. */
#include "world_dynamic.h"
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
static const struct fs98_world_dynamic_services *s_services;
void fs98_world_dynamic_set_services(const struct fs98_world_dynamic_services *services)
{ s_services = services; }
/* 0x223fb910 */
void fs98_world_dynamic_collect(void *object)
{
    int selection;
    U32(PTR(object, 0x10e0), 0x24) = U32(object, 0x1580);
    U32(PTR(object, 0x10e0), 0x1c) = U32(object, 0x1648);
    U32(PTR(object, 0x10e0), 0x14) = U32(object, 0x1710);
    U32(PTR(object, 0x10e0), 0xc) = U32(object, 0x17d8);
    selection = s_services->send(U32(object, 0x149c), 0x147, 0, 0);
    U32(PTR(object, 0x10e0), 0x2c) = (uint32_t)selection;
}
/* 0x223fb970: preserve raw checkbox values and reload after each service. */
void fs98_world_dynamic_populate(void *object)
{
    s_services->check(AT(object, 0x14c4), (int)U32(PTR(object, 0x10e0), 0x24));
    s_services->check(AT(object, 0x158c), (int)U32(PTR(object, 0x10e0), 0x1c));
    s_services->check(AT(object, 0x1654), (int)U32(PTR(object, 0x10e0), 0x14));
    s_services->check(AT(object, 0x171c), (int)U32(PTR(object, 0x10e0), 0xc));
    s_services->send(U32(object, 0x149c), 0x14e, U32(PTR(object, 0x10e0), 0x2c), 0);
}
/* 0x223fb360 */
int fs98_world_dynamic_has_descriptor(void *object)
{ return PTR(object, 0x10e0) != 0; }
/* 0x223fb370: stdcall callback, one descriptor argument, return is observed. */
int fs98_world_dynamic_check(void *object)
{
    void *descriptor = PTR(object, 0x10e0);
    return ((int (SC *)(void *))PTR(descriptor, 4))(descriptor);
}
/* 0x223fb9f0: first nonzero error result wins. Skipped messages leave
 * the previous +0x8c value intact; falling through returns zero regardless. */
int fs98_world_dynamic_error(void *object)
{
    static const uint32_t s_fields[] = {0x20, 0x18, 0x10, 8};
    static const uint32_t s_controls[] = {0x14c4, 0x158c, 0x1654, 0x171c};
    uint32_t index;
    for (index = 0; index < 4; ++index) {
        uint32_t message = U32(PTR(object, 0x10e0), s_fields[index]);
        if (message) {
            void *control = AT(object, s_controls[index]);
            int result = s_services->error(object, control, message);
            U32(object, 0x8c) = (uint32_t)result;
            if (result) {
                s_services->focus(control);
                s_services->post(U32(control, 0x20), 0xb1, 0, 0xffffffff);
                return (int)U32(object, 0x8c);
            }
        }
    }
    return 0;
}
/* 0x223fb850: retain the vtable sampled before writing the two results. */
void fs98_world_dynamic_cancel(void *object)
{
    void *descriptor = PTR(object, 0x10e0);
    void **vtable = PTR(object, 0);
    U32(object, 0x17e4) = 2;
    U32(descriptor, 0) = 0;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}
/* 0x223fb870: validation may veto through the ordered error routine. */
void fs98_world_dynamic_accept(void *object)
{
    void *descriptor;
    void **vtable;
    U32(object, 0x17e4) = 1;
    fs98_world_dynamic_collect(object);
    if (fs98_world_dynamic_check(object) && fs98_world_dynamic_error(object)) return;
    descriptor = PTR(object, 0x10e0);
    vtable = PTR(object, 0);
    U32(descriptor, 0) = 1;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}
