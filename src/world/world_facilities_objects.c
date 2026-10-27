/* Facilities directory class: exact control lifetimes and factory cleanup. */
#include "world_facilities.h"
#include "world_unwind.h"
#include "world_facilities_tables.inc"
static const struct fs98_world_object_services *s_object_services;
static void *s_facilities_vtable;
static uint32_t s_module;
static void *s_spin_vtable;
static void (*s_base_exchange)(void *, void *);
void fs98_world_facilities_set_object_services(const struct fs98_world_object_services *services, void *vtable,
    void *spin_vtable, void (*base_exchange)(void *, void *))
{ s_object_services = services; s_facilities_vtable = vtable; s_spin_vtable = spin_vtable; s_base_exchange = base_exchange; }
void fs98_world_facilities_set_module(uint32_t module) { s_module = module; }
struct world_object_unwind {
    struct fs98_world_unwind frame;
    void *object;
    volatile uint32_t constructed, base;
};
static void world_destroy_remaining(struct world_object_unwind *state, int unwinding)
{
    while (state->constructed) {
        const struct fs98_world_control *control = &s_facilities_controls[--state->constructed];
        uint32_t slot = unwinding ? s_facilities_cleanup[state->constructed] : control->destructor;
        s_object_services->destroy((uint8_t *)state->object + control->offset, slot);
    }
    if (state->base) {
        state->base = 0;
        s_object_services->destroy(state->object, WORLD_FACILITIES_BASE_DTOR);
    }
}
static void world_object_cleanup(struct fs98_world_unwind *frame)
{ world_destroy_remaining((struct world_object_unwind *)frame, 1); }
/* 0x22406f00 */
WORLD_NOINLINE void *fs98_world_facilities_construct(void *object, void *parent)
{
    struct world_object_unwind state;
    state.object = object; state.constructed = state.base = 0;
    world_unwind_enter(&state.frame, world_object_cleanup);
    s_object_services->base_construct(object, 0x6bf, parent);
    state.base = 1;
    while (state.constructed < sizeof(s_facilities_controls) / sizeof(s_facilities_controls[0])) {
        const struct fs98_world_control *control = &s_facilities_controls[state.constructed];
        s_object_services->construct((uint8_t *)object + control->offset, control->constructor);
        /* Facilities retains its local spin vtable after construction. */
        if (control->constructor == 0x255a4) WORLD_PTR(object, control->offset) = s_spin_vtable;
        ++state.constructed;
    }
    WORLD_PTR(object, 0) = s_facilities_vtable;
    WORLD_U32(object, 0x1428) = 0;
    world_unwind_leave(&state.frame);
    return object;
}
/* 0x22407010 does not reset its vtable on entry. */
WORLD_NOINLINE void fs98_world_facilities_destroy(void *object)
{
    struct world_object_unwind state;
    state.object = object; state.base = 1;
    state.constructed = sizeof(s_facilities_controls) / sizeof(s_facilities_controls[0]);
    world_unwind_enter(&state.frame, world_object_cleanup);
    world_destroy_remaining(&state, 0);
    world_unwind_leave(&state.frame);
}
/* 0x22406ff0 */
void *fs98_world_facilities_scalar_delete(void *object, uint32_t flags)
{
    fs98_world_facilities_destroy(object);
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
/* 0x223f1d80. The parent argument is ignored. */
static WORLD_NOINLINE uint32_t world_facilities_create(uint32_t parent, void *descriptor)
{
    struct world_factory_unwind state;
    void *object;
    (void)parent;
    state.object = NULL; state.live = 0;
    world_unwind_enter(&state.frame, world_factory_cleanup);
    object = s_object_services->allocate(0x1db8);
    state.object = object; state.live = 1;
    if (object) object = fs98_world_facilities_construct(object, NULL);
    /* A null allocation faults here in retail, before Create. */
    *(void *volatile *)((uint8_t *)object + 0x10dc) = descriptor;
    WORLD_U32(object, 0x80) = s_module;
    state.live = 0;
    s_object_services->create(object, 0x6bf, NULL);
    s_object_services->show(object, 1);
    world_unwind_leave(&state.frame);
    return WORLD_U32(object, 0x20);
}
uint32_t FS98_WORLD_STDCALL fs98_world_facilities_create(uint32_t parent, void *descriptor)
{ return world_facilities_create(parent, descriptor); }
/* 0x224070f0 */
void fs98_world_facilities_exchange(void *object, void *exchange)
{
    uint32_t index;
    s_base_exchange(object, exchange);
    for (index = 0; index < sizeof(s_facilities_ddx) / sizeof(s_facilities_ddx[0]); ++index)
        s_object_services->ddx(exchange, s_facilities_ddx[index][1], (uint8_t *)object + s_facilities_ddx[index][0]);
}
/* 0x224095e0 leaves the WinHelp result in eax. */
int fs98_world_facilities_help(void *object, uint32_t unused, void *help)
{
    (void)object; (void)unused;
    return s_object_services->help(WORLD_U32(help, 0xc), "fs98.hlp", 12, s_facilities_help);
}
/* 0x223f3d30: unlike the area editor, this class deletes on PostNcDestroy. */
void fs98_world_facilities_post_destroy(void *object)
{
    if (object) {
        void **vtable = WORLD_PTR(object, 0);
        ((void *(FS98_WORLD_THISCALL *)(void *, uint32_t))vtable[1])(object, 1);
    }
}

static const struct fs98_world_facilities_help_services *s_help_services;
void fs98_world_facilities_set_help_services(const struct fs98_world_facilities_help_services *services)
{ s_help_services = services; }
/* 0x22409600 uses the current cursor, ignoring the message coordinates.
 * Context help is suppressed when that cursor lies over the artwork. */
int fs98_world_facilities_context(void *object, uint32_t window, uint32_t unused)
{
    int32_t point[2];
    struct fs98_world_rect rect;
    (void)unused;
    s_help_services->cursor(point);
    s_help_services->window_rect(WORLD_U32(object, 0x1c34), &rect);
    if (s_help_services->contains(&rect, point[0], point[1])) return 0;
    s_object_services->help(window, "fs98.hlp", 10, s_facilities_help);
    return 1;
}
