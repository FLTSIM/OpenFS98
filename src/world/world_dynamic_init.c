/* Retail 0x223fb610: common skin, nine controls and owned CNotify. */
#include "world_dynamic.h"
#include "world_unwind.h"
#define AT(p, n) ((uint8_t *)(p) + (n))
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define API(slot, type) ((type)s_services->resolve(slot))
static const struct fs98_world_dynamic_init_services *s_services;
void fs98_world_dynamic_set_init_services(const struct fs98_world_dynamic_init_services *services)
{ s_services = services; }
struct world_notify_unwind {
    struct fs98_world_unwind frame;
    void *object;
    volatile int live;
};
static void world_notify_cleanup(struct fs98_world_unwind *frame)
{
    struct world_notify_unwind *state = (void *)frame;
    if (state->live) {
        state->live = 0;
        API(0x257f4, void (SC *)(void *))(state->object);
    }
}
/* 0x223f2700 */
void *fs98_world_dynamic_notify_delete(void *object, uint32_t flags)
{
    API(0x257bc, void (TC *)(void *))(object);
    if (flags & 1) API(0x25800, void (*)(void *))(object);
    return object;
}
WORLD_NOINLINE int fs98_world_dynamic_init(void *object)
{
    const struct fs98_world_init_services *ui = s_services->ui;
    struct fs98_world_rect rect = {0, 0, 0, 0};
    struct world_notify_unwind state;
    void **vtable;
    void *notify;
    uint32_t index;
    static const uint32_t s_checks[] = {0x171c, 0x1654, 0x158c, 0x14c4};
    ui->base_init(object);
    vtable = PTR(object, 0);
    if (!((int (TC *)(void *, uint32_t, uint32_t, uint32_t))vtable[0xe4 / 4])(
            object, 0x14f, s_services->resources(), 15)) return 0;
    ui->art(AT(object, 0x12dc), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    ui->button(AT(object, 0x10e4), AT(object, 0x5a0), AT(object, 0x610), 0x132,
        AT(object, 0xff4), AT(object, 0x7e8), 1, -1, s_services->resources(), 0, 0, rect);
    ui->button(AT(object, 0x11e0), AT(object, 0x5a0), AT(object, 0x610), 0x132,
        AT(object, 0xff4), AT(object, 0x7e8), 1, -1, s_services->resources(), 0, 0, rect);
    for (index = 0; index < 4; ++index) {
        void *control = AT(object, s_checks[index]);
        vtable = PTR(control, 0);
        ((int (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))vtable[0xc4 / 4])(
            control, AT(object, 0x4c0), 0x139, AT(object, 0xff4), AT(object, 0x7e8), s_services->resources());
    }
    ui->text(AT(object, 0x13c4), AT(object, 0x370));
    ui->combo(AT(object, 0x147c), AT(object, 0x3e0), AT(object, 0x13c4));
    API(0x258b0, int (SC *)(uint32_t, uint32_t, uint32_t, uint32_t))(U32(object, 0x149c), 0x14e, 0, 0);
    state.object = NULL; state.live = 0;
    world_unwind_enter(&state.frame, world_notify_cleanup);
    notify = API(0x2584c, void *(*)(uint32_t))(0x29c);
    state.object = notify; state.live = 1;
    if (notify) {
        API(0x257cc, void *(TC *)(void *, uint32_t, uint32_t))(notify, 0x30, 1);
        PTR(notify, 0) = s_services->notify_vtable;
    }
    state.live = 0;
    PTR(object, 0x10dc) = notify;
    if (!fs98_world_dynamic_has_descriptor(object)) {
        world_unwind_leave(&state.frame);
        return 0;
    }
    fs98_world_dynamic_populate(object);
    world_unwind_leave(&state.frame);
    return 1;
}
