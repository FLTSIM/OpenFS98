/* Flights' dialog classes: construction, destruction, deleting destructors and the
 * factories the host's dialog registry calls. Each member is built and torn down
 * through its own import, with cleanup-only SEH frames that undo a partial
 * construction in reverse, as the retail compiler's funclets do. */
#include "flights_objects.h"
#include "flights_unwind.h"
#include "flights_internal.h"
#include "flights_objects_tables.inc"
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define TC FS98_FLIGHTS_THISCALL
#define SC FS98_FLIGHTS_STDCALL

static const struct fs98_flights_object_services *s_services;
static void *s_vtables[FS98_FLIGHTS_CLASS_COUNT];
static uint32_t s_module;
struct fs98_flights_object_unwind {
    struct fs98_flights_unwind frame;
    void *object;
    const struct fs98_flights_class *class_info;
    volatile uint32_t constructed, base;
};
static void fs98_flights_object_cleanup(struct fs98_flights_unwind *frame)
{
    struct fs98_flights_object_unwind *state = (void *)frame;
    while (state->constructed) {
        const struct fs98_flights_subobject *control = &state->class_info->controls[--state->constructed];
        s_services->destroy((uint8_t *)state->object + control->offset, control->destructor);
    }
    if (state->base) {
        state->base = 0;
        s_services->dialog_destroy(state->object);
    }
}
void fs98_flights_set_object_services(const struct fs98_flights_object_services *services,
                                      void *const *vtables)
{
    uint32_t index;
    s_services = services;
    for (index = 0; index < FS98_FLIGHTS_CLASS_COUNT; ++index) s_vtables[index] = vtables[index];
}
void fs98_flights_set_object_module(uint32_t module) { s_module = module; }

/* 0x22312c10 and the like: CSimsDialog(template, parent), then each member in order */
static FS98_FLIGHTS_NOINLINE void *fs98_flights_construct(void *object, void *parent, uint32_t kind)
{
    struct fs98_flights_object_unwind state;
    state.object = object; state.class_info = &s_classes[kind];
    state.constructed = state.base = 0;
    fs98_flights_unwind_enter(&state.frame, fs98_flights_object_cleanup);
    s_services->dialog_construct(object, state.class_info->template, parent);
    state.base = 1;
    while (state.constructed < state.class_info->count) {
        const struct fs98_flights_subobject *control = &state.class_info->controls[state.constructed];
        s_services->construct((uint8_t *)object + control->offset, control->constructor);
        ++state.constructed;
    }
    PTR(object, 0) = s_vtables[kind];
    /* 0x2231602b: Save forgets the name it last confirmed, inside the constructor's frame */
    if (kind == FS98_FLIGHTS_SAVE) s_services->copy_text((char *)object + 0x10dc, fs98_flights_empty_text);
    /* 0x22319ef7: the welcome dialog starts with no button's callback running */
    if (kind == FS98_FLIGHTS_OPENING) D32(object, 0x10dc) = 0;
    fs98_flights_unwind_leave(&state.frame);
    return object;
}
/* 0x22312cf0 and the like: the members in reverse, then CSimsDialog */
static FS98_FLIGHTS_NOINLINE void fs98_flights_destroy(void *object, uint32_t kind)
{
    struct fs98_flights_object_unwind state;
    state.object = object; state.class_info = &s_classes[kind];
    state.constructed = state.class_info->count; state.base = 1;
    fs98_flights_unwind_enter(&state.frame, fs98_flights_object_cleanup);
    fs98_flights_object_cleanup(&state.frame);
    fs98_flights_unwind_leave(&state.frame);
}
struct fs98_flights_factory_unwind {
    struct fs98_flights_unwind frame;
    void *object;
    volatile uint32_t live;
};
static void fs98_flights_factory_cleanup(struct fs98_flights_unwind *frame)
{
    struct fs98_flights_factory_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; s_services->dialog_delete(state->object); }
}
/* 0x223112f0 and the like. A failed allocation leaves the object null and the
 * descriptor store faults, as retail's does; the parent argument is ignored. */
static FS98_FLIGHTS_NOINLINE uint32_t fs98_flights_create(uint32_t parent, void *descriptor, uint32_t kind)
{
    struct fs98_flights_factory_unwind state;
    const struct fs98_flights_class *class_info = &s_classes[kind];
    void *object;
    (void)parent;
    state.object = 0; state.live = 0;
    fs98_flights_unwind_enter(&state.frame, fs98_flights_factory_cleanup);
    object = s_services->allocate(class_info->size);
    state.object = object; state.live = 1;
    if (object) object = fs98_flights_construct(object, 0, kind);
    *(void *volatile *)((uint8_t *)object + class_info->descriptor) = descriptor;
    state.live = 0;
    D32(object, 0x80) = s_module;
    s_services->create(object, class_info->template, 0);
    s_services->show(object, 1);
    fs98_flights_unwind_leave(&state.frame);
    return D32(object, 0x20);
}
#define OBJECT_CLASS(name, kind) \
void *TC fs98_flights_##name##_construct(void *object, void *parent) \
{ return fs98_flights_construct(object, parent, kind); } \
void TC fs98_flights_##name##_destroy(void *object) \
{ fs98_flights_destroy(object, kind); } \
void *TC fs98_flights_##name##_delete(void *object, uint32_t flags) \
{ \
    fs98_flights_destroy(object, kind); \
    if (flags & 1) s_services->ui_delete(object); \
    return object; \
} \
uint32_t SC fs98_flights_##name##_create(uint32_t parent, void *descriptor) \
{ return fs98_flights_create(parent, descriptor, kind); }
OBJECT_CLASS(adventures, FS98_FLIGHTS_ADVENTURES)
OBJECT_CLASS(challenges, FS98_FLIGHTS_CHALLENGES)
OBJECT_CLASS(save, FS98_FLIGHTS_SAVE)
OBJECT_CLASS(comms, FS98_FLIGHTS_COMMS)
OBJECT_CLASS(create, FS98_FLIGHTS_CREATE)
OBJECT_CLASS(aircraft, FS98_FLIGHTS_AIRCRAFT)
OBJECT_CLASS(modeless, FS98_FLIGHTS_MODELESS)
OBJECT_CLASS(opening, FS98_FLIGHTS_OPENING)
#undef OBJECT_CLASS
