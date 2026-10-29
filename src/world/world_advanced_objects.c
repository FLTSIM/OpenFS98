/* Advanced Weather parent and five owned pages; no retail class code is reused. */
#include "world_advanced.h"
#include "world_unwind.h"
#include "world_advanced_tables.inc"
#define AT(p, n) ((uint8_t *)(p) + (n))
#define PTR WORLD_PTR
#define U32 WORLD_U32
#define TC FS98_WORLD_THISCALL

static const struct fs98_world_object_services *s_services;
static void *s_vtables[WORLD_ADVANCED_CLASS_COUNT];
static uint32_t s_module;
static void *(*s_resolve)(uint32_t);
static void (*s_base_exchange)(void *, void *);

void fs98_world_advanced_set_resolver(void *(*resolve)(uint32_t))
{ s_resolve = resolve; }

void fs98_world_advanced_set_base_exchange(void (*exchange)(void *, void *))
{ s_base_exchange = exchange; }

void fs98_world_advanced_set_object_services(
    const struct fs98_world_object_services *services, void *const *vtables)
{
    uint32_t kind;
    s_services = services;
    for (kind = 0; kind < WORLD_ADVANCED_CLASS_COUNT; ++kind)
        s_vtables[kind] = vtables[kind];
}

void fs98_world_advanced_set_module(uint32_t module)
{ s_module = module; }

struct advanced_object_unwind {
    struct fs98_world_unwind frame;
    void *object;
    const struct world_advanced_class *class;
    volatile uint32_t constructed, base;
};

static void advanced_destroy_remaining(struct advanced_object_unwind *state, int unwinding)
{
    while (state->constructed) {
        const struct fs98_world_control *control = &state->class->controls[--state->constructed];
        s_services->destroy(AT(state->object, control->offset),
            unwinding ? state->class->cleanup[state->constructed] : control->destructor);
    }
    if (state->base) {
        state->base = 0;
        s_services->destroy(state->object, state->class->base_dtor);
    }
}

static void advanced_object_cleanup(struct fs98_world_unwind *frame)
{ advanced_destroy_remaining((void *)frame, 1); }

WORLD_NOINLINE void *fs98_world_advanced_construct(void *object, void *parent, uint32_t kind)
{
    struct advanced_object_unwind state;
    state.object = object;
    state.class = &s_advanced_classes[kind];
    state.constructed = state.base = 0;
    world_unwind_enter(&state.frame, advanced_object_cleanup);
    if (kind)
        ((void *(TC *)(void *, uint32_t, void *))s_resolve(state.class->base_ctor))(
            object, state.class->template_id, parent);
    else
        s_services->base_construct(object, state.class->template_id, parent);
    state.base = 1;
    while (state.constructed < state.class->count) {
        const struct fs98_world_control *control = &state.class->controls[state.constructed];
        s_services->construct(AT(object, control->offset), control->constructor);
        ++state.constructed;
    }
    /* Retail overwrites its intermediate local separator vtable immediately
     * with SIMSUI's vtable. The intermediate store has no observable call. */
    if (kind == WORLD_ADVANCED_TEMPERATURE) PTR(object, 0x195c) = s_resolve(0x25758);
    PTR(object, 0) = s_vtables[kind];
    if (kind == WORLD_ADVANCED_PARENT) U32(object, 0x2638) = 0;
    if (kind == WORLD_ADVANCED_TEMPERATURE) U32(object, 0x1208) = 0;
    if (kind == WORLD_ADVANCED_WIND) U32(object, 0x1204) = 0;
    world_unwind_leave(&state.frame);
    return object;
}

WORLD_NOINLINE void fs98_world_advanced_destroy(void *object, uint32_t kind)
{
    struct advanced_object_unwind state;
    state.object = object;
    state.class = &s_advanced_classes[kind];
    state.constructed = state.class->count;
    state.base = 1;
    world_unwind_enter(&state.frame, advanced_object_cleanup);
    if (!kind) PTR(object, 0) = s_vtables[0];
    if (kind == WORLD_ADVANCED_TEMPERATURE) PTR(object, 0x195c) = s_resolve(0x25758);
    advanced_destroy_remaining(&state, 0);
    world_unwind_leave(&state.frame);
}

void *fs98_world_advanced_delete(void *object, uint32_t flags, uint32_t kind)
{
    fs98_world_advanced_destroy(object, kind);
    if (flags & 1) s_services->ui_delete(object);
    return object;
}

struct advanced_factory_unwind {
    struct fs98_world_unwind frame;
    void *object;
    volatile int live;
};

static void advanced_factory_cleanup(struct fs98_world_unwind *frame)
{
    struct advanced_factory_unwind *state = (void *)frame;
    if (state->live) {
        state->live = 0;
        s_services->dialog_delete(state->object);
    }
}

static WORLD_NOINLINE uint32_t advanced_create(uint32_t parent, void *descriptor)
{
    struct advanced_factory_unwind state;
    void *object;
    (void)parent;
    state.object = NULL;
    state.live = 0;
    world_unwind_enter(&state.frame, advanced_factory_cleanup);
    object = s_services->allocate(0x2658);
    state.object = object;
    state.live = 1;
    if (object) object = fs98_world_advanced_construct(object, NULL, WORLD_ADVANCED_PARENT);
    *(void *volatile *)AT(object, 0x1104) = descriptor;
    U32(object, 0x80) = s_module;
    state.live = 0;
    s_services->create(object, 0x6a5, NULL);
    s_services->show(object, 1);
    world_unwind_leave(&state.frame);
    return U32(object, 0x20);
}

uint32_t FS98_WORLD_STDCALL fs98_world_advanced_create(uint32_t parent, void *descriptor)
{ return advanced_create(parent, descriptor); }

void fs98_world_advanced_exchange(void *object, void *exchange, uint32_t kind)
{
    uint32_t index;
    if (!kind) s_base_exchange(object, exchange);
    for (index = 0; index < s_advanced_classes[kind].count; ++index)
        s_services->ddx(exchange, s_advanced_classes[kind].ddx[index][1],
            AT(object, s_advanced_classes[kind].ddx[index][0]));
}

int fs98_world_advanced_help(void *object, uint32_t window, uint32_t command, uint32_t kind)
{
    (void)object;
    return s_services->help(window, "fs98.hlp", command, s_advanced_classes[kind].help);
}

int fs98_world_advanced_context(void *object, uint32_t window, uint32_t kind)
{
    fs98_world_advanced_help(object, window, 10, kind);
    return 1;
}

void fs98_world_advanced_post_destroy(void *object)
{
    if (object)
        ((void *(TC *)(void *, uint32_t))PTR(PTR(object, 0), 4))(object, 1);
}

const struct world_advanced_class *fs98_world_advanced_class(uint32_t kind)
{ return &s_advanced_classes[kind]; }
