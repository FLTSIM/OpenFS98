/* Scenery-priority class: exact control lifetimes and factory cleanup. */
#include "world_priority.h"
#include "world_unwind.h"
#include "world_exceptions.h"
#include "world_priority_tables.inc"
static const struct fs98_world_object_services *s_object_services;
static void *s_priority_vtable;
static uint32_t s_module;
void fs98_world_priority_set_object_services(const struct fs98_world_object_services *services, void *vtable)
{ s_object_services = services; s_priority_vtable = vtable; }
void fs98_world_priority_set_module(uint32_t module) { s_module = module; }
struct world_object_unwind {
    struct fs98_world_unwind frame;
    void *object;
    volatile uint32_t constructed, base;
};
static void world_destroy_remaining(struct world_object_unwind *state)
{
    while (state->constructed) {
        const struct fs98_world_control *control = &s_priority_controls[--state->constructed];
        s_object_services->destroy((uint8_t *)state->object + control->offset, control->destructor);
    }
    if (state->base) {
        state->base = 0;
        s_object_services->destroy(state->object, WORLD_PRIORITY_BASE_DTOR);
    }
}
static void world_object_cleanup(struct fs98_world_unwind *frame)
{ world_destroy_remaining((struct world_object_unwind *)frame); }
/* 0x223fc820 */
WORLD_NOINLINE void *fs98_world_priority_construct(void *object, void *parent)
{
    struct world_object_unwind state;
    state.object = object; state.constructed = state.base = 0;
    world_unwind_enter(&state.frame, world_object_cleanup);
    s_object_services->base_construct(object, 0x6b4, parent);
    state.base = 1;
    while (state.constructed < sizeof(s_priority_controls) / sizeof(s_priority_controls[0])) {
        const struct fs98_world_control *control = &s_priority_controls[state.constructed];
        s_object_services->construct((uint8_t *)object + control->offset, control->constructor);
        ++state.constructed;
    }
    WORLD_PTR(object, 0) = s_priority_vtable;
    world_unwind_leave(&state.frame);
    return object;
}
/* 0x223fc960 does not reset its vtable on entry. */
WORLD_NOINLINE void fs98_world_priority_destroy(void *object)
{
    struct world_object_unwind state;
    state.object = object; state.base = 1;
    state.constructed = sizeof(s_priority_controls) / sizeof(s_priority_controls[0]);
    world_unwind_enter(&state.frame, world_object_cleanup);
    world_destroy_remaining(&state);
    world_unwind_leave(&state.frame);
}
/* 0x223fc940 */
void *fs98_world_priority_delete(void *object, uint32_t flags)
{
    fs98_world_priority_destroy(object);
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
/* Retail catches CException* and calls SIMSUI_ExitCleanlyE, which aborts.
 * Keep this distinct from unrelated exceptions, which propagate after cleanup. */
static EXCEPTION_DISPOSITION __cdecl world_factory_handler(
    EXCEPTION_RECORD *record, void *establisher, CONTEXT *context, void *dispatcher)
{
    struct world_factory_unwind *state = establisher;
    void *exception;
    if (!(record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND)) &&
        world_exception_pointer(record, &exception)) {
        RtlUnwind(establisher, NULL, record, NULL);
        world_factory_cleanup(&state->frame);
        s_object_services->exit_cleanly(exception);
        /* SIMSUI's real implementation cannot return. */
        __builtin_trap();
    }
    return world_unwind_handler(record, establisher, context, dispatcher);
}
/* 0x223f1b90. The parent argument is ignored. */
static WORLD_NOINLINE uint32_t world_priority_create(uint32_t parent, void *descriptor)
{
    struct world_factory_unwind state;
    void *object;
    (void)parent;
    state.object = NULL; state.live = 0;
    world_unwind_enter(&state.frame, world_factory_cleanup);
    state.frame.handler = world_factory_handler;
    object = s_object_services->allocate(0x1bb4);
    state.object = object; state.live = 1;
    if (object) object = fs98_world_priority_construct(object, NULL);
    /* A null allocation faults here in retail, before Create. */
    *(void *volatile *)((uint8_t *)object + 0x10dc) = descriptor;
    WORLD_U32(object, 0x80) = s_module;
    state.live = 0;
    if (!s_object_services->create(object, 0x6b4, NULL))
        s_object_services->message("CSceneryLayers::Create() failed\n", 0, 0);
    s_object_services->show(object, 1);
    world_unwind_leave(&state.frame);
    return WORLD_U32(object, 0x20);
}
uint32_t FS98_WORLD_STDCALL fs98_world_priority_create(uint32_t parent, void *descriptor)
{ return world_priority_create(parent, descriptor); }
/* 0x223fca80 */
void fs98_world_priority_exchange(void *object, void *exchange)
{
    uint32_t index;
    for (index = 0; index < sizeof(s_priority_ddx) / sizeof(s_priority_ddx[0]); ++index)
        s_object_services->ddx(exchange, s_priority_ddx[index][1], (uint8_t *)object + s_priority_ddx[index][0]);
}
/* 0x223fcf90 leaves the WinHelp result in eax. */
int fs98_world_priority_help(void *object, uint32_t unused, void *help)
{
    (void)object; (void)unused;
    return s_object_services->help(WORLD_U32(help, 0xc), "fs98.hlp", 12, s_priority_help);
}
/* 0x223fcfb0 */
int fs98_world_priority_context(void *object, uint32_t window, uint32_t unused)
{
    (void)object; (void)unused;
    s_object_services->help(window, "fs98.hlp", 10, s_priority_help);
    return 1;
}

/* 0x223f3d30: unlike the area editor, this class deletes on PostNcDestroy. */
void fs98_world_priority_post_destroy(void *object)
{
    if (object) {
        void **vtable = WORLD_PTR(object, 0);
        ((void *(FS98_WORLD_THISCALL *)(void *, uint32_t))vtable[1])(object, 1);
    }
}
/* 0x223fe4e0 */
void fs98_world_priority_cancel(void *object)
{
    void *descriptor = WORLD_PTR(object, 0x10dc);
    void **vtable = WORLD_PTR(object, 0);
    WORLD_U32(descriptor, 0) = 0;
    ((void (FS98_WORLD_THISCALL *)(void *))vtable[0xec / 4])(object);
}
