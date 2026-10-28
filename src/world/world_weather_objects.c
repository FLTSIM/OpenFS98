/* Basic-weather class: exact control lifetimes and factory cleanup. */
#include "world_weather.h"
#include "world_unwind.h"
#include "world_weather_tables.inc"
static const struct fs98_world_object_services *s_object_services;
static void *s_weather_vtable;
static uint32_t s_module;
void fs98_world_weather_set_object_services(const struct fs98_world_object_services *services, void *vtable)
{ s_object_services = services; s_weather_vtable = vtable; }
void fs98_world_weather_set_module(uint32_t module) { s_module = module; }
struct world_object_unwind {
    struct fs98_world_unwind frame;
    void *object;
    volatile uint32_t constructed, base;
};
static void world_destroy_remaining(struct world_object_unwind *state, int unwinding)
{
    while (state->constructed) {
        const struct fs98_world_control *control = &s_weather_controls[--state->constructed];
        uint32_t slot = unwinding ? s_weather_cleanup[state->constructed] : control->destructor;
        s_object_services->destroy((uint8_t *)state->object + control->offset, slot);
    }
    if (state->base) {
        state->base = 0;
        s_object_services->destroy(state->object, WORLD_WEATHER_BASE_DTOR);
    }
}
static void world_object_cleanup(struct fs98_world_unwind *frame)
{ world_destroy_remaining((struct world_object_unwind *)frame, 1); }
/* 0x223f2d40 */
WORLD_NOINLINE void *fs98_world_weather_construct(void *object, void *parent)
{
    struct world_object_unwind state;
    state.object = object; state.constructed = state.base = 0;
    world_unwind_enter(&state.frame, world_object_cleanup);
    s_object_services->base_construct(object, 0x6a6, parent);
    state.base = 1;
    while (state.constructed < sizeof(s_weather_controls) / sizeof(s_weather_controls[0])) {
        const struct fs98_world_control *control = &s_weather_controls[state.constructed];
        s_object_services->construct((uint8_t *)object + control->offset, control->constructor);
        ++state.constructed;
    }
    WORLD_PTR(object, 0) = s_weather_vtable;
    world_unwind_leave(&state.frame);
    return object;
}
/* 0x223f2e70 does not reset its vtable on entry. */
WORLD_NOINLINE void fs98_world_weather_destroy(void *object)
{
    struct world_object_unwind state;
    state.object = object; state.base = 1;
    state.constructed = sizeof(s_weather_controls) / sizeof(s_weather_controls[0]);
    world_unwind_enter(&state.frame, world_object_cleanup);
    world_destroy_remaining(&state, 0);
    world_unwind_leave(&state.frame);
}
/* 0x223f2e50 */
void *fs98_world_weather_scalar_delete(void *object, uint32_t flags)
{
    fs98_world_weather_destroy(object);
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
/* 0x223f1590. The parent argument is ignored. */
static WORLD_NOINLINE uint32_t world_weather_create(uint32_t parent, void *descriptor)
{
    struct world_factory_unwind state;
    void *object;
    (void)parent;
    state.object = NULL; state.live = 0;
    world_unwind_enter(&state.frame, world_factory_cleanup);
    object = s_object_services->allocate(0x1960);
    state.object = object; state.live = 1;
    if (object) object = fs98_world_weather_construct(object, NULL);
    /* A null allocation faults here in retail, before Create. */
    *(void *volatile *)((uint8_t *)object + 0x10e0) = descriptor;
    WORLD_U32(object, 0x80) = s_module;
    state.live = 0;
    s_object_services->create(object, 0x6a6, NULL);
    s_object_services->show(object, 1);
    world_unwind_leave(&state.frame);
    return WORLD_U32(object, 0x20);
}
uint32_t FS98_WORLD_STDCALL fs98_world_weather_create(uint32_t parent, void *descriptor)
{ return world_weather_create(parent, descriptor); }
/* 0x223f2f80 */
void fs98_world_weather_exchange(void *object, void *exchange)
{
    uint32_t index;
    fs98_world_weather_base_exchange(object, exchange);
    for (index = 0; index < sizeof(s_weather_ddx) / sizeof(s_weather_ddx[0]); ++index)
        s_object_services->ddx(exchange, s_weather_ddx[index][1], (uint8_t *)object + s_weather_ddx[index][0]);
}
/* 0x223f3400 leaves the WinHelp result in eax. */
int fs98_world_weather_help(void *object, uint32_t unused, void *help)
{
    (void)object; (void)unused;
    return s_object_services->help(WORLD_U32(help, 0xc), "fs98.hlp", 12, s_weather_help);
}
/* 0x223f3d30: unlike the area editor, this class deletes on PostNcDestroy. */
void fs98_world_weather_post_destroy(void *object)
{
    if (object) {
        void **vtable = WORLD_PTR(object, 0);
        ((void *(FS98_WORLD_THISCALL *)(void *, uint32_t))vtable[1])(object, 1);
    }
}

void fs98_world_weather_context_map(uint32_t window)
{ s_object_services->help(window, "fs98.hlp", 10, s_weather_help); }
