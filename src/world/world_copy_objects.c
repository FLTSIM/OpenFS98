/* Copy-weather class: exact control lifetimes and factory cleanup. */
#include "world_copy.h"
#include "world_unwind.h"
#include "world_copy_tables.inc"
static const struct fs98_world_object_services *s_object_services;
static void *s_copy_vtable;
static uint32_t s_module;
void fs98_world_copy_set_object_services(const struct fs98_world_object_services *services, void *vtable)
{ s_object_services = services; s_copy_vtable = vtable; }
void fs98_world_copy_set_module(uint32_t module) { s_module = module; }
struct world_object_unwind {
    struct fs98_world_unwind frame;
    void *object;
    volatile uint32_t constructed, base;
};
static void world_destroy_remaining(struct world_object_unwind *state, int unwinding)
{
    while (state->constructed) {
        const struct fs98_world_control *control = &s_copy_controls[--state->constructed];
        uint32_t slot = unwinding ? s_copy_cleanup[state->constructed] : control->destructor;
        s_object_services->destroy((uint8_t *)state->object + control->offset, slot);
    }
    if (state->base) {
        state->base = 0;
        s_object_services->destroy(state->object, WORLD_COPY_BASE_DTOR);
    }
}
static void world_object_cleanup(struct fs98_world_unwind *frame)
{ world_destroy_remaining((struct world_object_unwind *)frame, 1); }
/* 0x223f37c0 */
WORLD_NOINLINE void *fs98_world_copy_construct(void *object, void *parent)
{
    struct world_object_unwind state;
    state.object = object; state.constructed = state.base = 0;
    world_unwind_enter(&state.frame, world_object_cleanup);
    s_object_services->base_construct(object, 0x6a8, parent);
    state.base = 1;
    while (state.constructed < sizeof(s_copy_controls) / sizeof(s_copy_controls[0])) {
        const struct fs98_world_control *control = &s_copy_controls[state.constructed];
        s_object_services->construct((uint8_t *)object + control->offset, control->constructor);
        ++state.constructed;
    }
    WORLD_PTR(object, 0) = s_copy_vtable;
    world_unwind_leave(&state.frame);
    return object;
}
/* 0x223f3900 does not reset its vtable on entry. */
WORLD_NOINLINE void fs98_world_copy_destroy(void *object)
{
    struct world_object_unwind state;
    state.object = object; state.base = 1;
    state.constructed = sizeof(s_copy_controls) / sizeof(s_copy_controls[0]);
    world_unwind_enter(&state.frame, world_object_cleanup);
    world_destroy_remaining(&state, 0);
    world_unwind_leave(&state.frame);
}
/* 0x223f38e0 */
void *fs98_world_copy_scalar_delete(void *object, uint32_t flags)
{
    fs98_world_copy_destroy(object);
    if (flags & 1) s_object_services->ui_delete(object);
    return object;
}
struct world_factory_unwind {
    struct fs98_world_unwind frame;
    void *object;
    volatile uint32_t live;
};
static void world_factory_cleanup(struct fs98_world_unwind *frame)
{
    struct world_factory_unwind *state = (void *)frame;
    if (state->live) {
        state->live = 0;
        s_object_services->dialog_delete(state->object);
    }
}
/* 0x223f11c0. The parent argument is ignored. */
static WORLD_NOINLINE uint32_t world_copy_create(uint32_t parent, void *descriptor)
{
    struct world_factory_unwind state;
    void *object;
    (void)parent;
    state.object = NULL; state.live = 0;
    world_unwind_enter(&state.frame, world_factory_cleanup);
    object = s_object_services->allocate(0x1a74);
    state.object = object; state.live = 1;
    if (object) object = fs98_world_copy_construct(object, NULL);
    /* A null allocation faults here in retail, before Create. */
    *(void *volatile *)((uint8_t *)object + 0x10dc) = descriptor;
    WORLD_U32(object, 0x80) = s_module;
    state.live = 0;
    s_object_services->create(object, 0x6a8, NULL);
    s_object_services->show(object, 1);
    world_unwind_leave(&state.frame);
    return WORLD_U32(object, 0x20);
}
uint32_t FS98_WORLD_STDCALL fs98_world_copy_create(uint32_t parent, void *descriptor)
{ return world_copy_create(parent, descriptor); }
/* 0x223f3a10 */
void fs98_world_copy_exchange(void *object, void *exchange)
{
    uint32_t index;
    (void)object;
    for (index = 0; index < sizeof(s_copy_ddx) / sizeof(s_copy_ddx[0]); ++index)
        s_object_services->ddx(exchange, s_copy_ddx[index][1], (uint8_t *)object + s_copy_ddx[index][0]);
}
/* 0x223f3dc0 leaves the WinHelp result in eax. */
int fs98_world_copy_help(void *object, uint32_t unused, void *help)
{
    (void)object; (void)unused;
    return s_object_services->help(WORLD_U32(help, 0xc), "fs98.hlp", 12, s_copy_help);
}
/* 0x223f3de0 */
int fs98_world_copy_context(void *object, uint32_t window, uint32_t unused)
{
    (void)object; (void)unused;
    s_object_services->help(window, "fs98.hlp", 10, s_copy_help);
    return 1;
}

/* 0x223f3d30: unlike the area editor, this class deletes on PostNcDestroy. */
void fs98_world_copy_post_destroy(void *object)
{
    if (object) {
        void **vtable = WORLD_PTR(object, 0);
        ((void *(FS98_WORLD_THISCALL *)(void *, uint32_t))vtable[1])(object, 1);
    }
}
