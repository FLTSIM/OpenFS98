/* Five derived dialogs and their exact embedded control lifetimes. */
#include "help_objects.h"
#include "help_unwind.h"
#include "help_objects_tables.inc"
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define TC FS98_HELP_THISCALL
#define SC FS98_HELP_STDCALL

static const struct fs98_help_object_services *s_services;
static void *s_vtables[5], *s_placeholder_vtable;
static uint32_t s_module;
struct fs98_help_object_unwind {
    struct fs98_help_unwind frame;
    void *object;
    const struct fs98_help_class *class_info;
    volatile uint32_t constructed, base;
};
static void fs98_help_object_cleanup(struct fs98_help_unwind *frame)
{
    struct fs98_help_object_unwind *state = (void *)frame;
    while (state->constructed) {
        const struct fs98_help_subobject *control = &state->class_info->controls[--state->constructed];
        s_services->destroy((uint8_t *)state->object + control->offset, control->destructor);
    }
    if (state->base) {
        state->base = 0;
        s_services->dialog_destroy(state->object);
    }
}
void fs98_help_set_object_services(const struct fs98_help_object_services *services,
                                   void *const *vtables, void *placeholder_vtable)
{
    uint32_t index;
    s_services = services;
    for (index = 0; index < 5; ++index) s_vtables[index] = vtables[index];
    s_placeholder_vtable = placeholder_vtable;
}
void fs98_help_set_object_module(uint32_t module) { s_module = module; }
static FS98_HELP_NOINLINE void *fs98_help_construct(void *object, void *parent, uint32_t kind)
{
    struct fs98_help_object_unwind state;
    state.object = object; state.class_info = &s_classes[kind];
    state.constructed = state.base = 0;
    fs98_help_unwind_enter(&state.frame, fs98_help_object_cleanup);
    s_services->dialog_construct(object, state.class_info->template, parent);
    state.base = 1;
    while (state.constructed < state.class_info->count) {
        const struct fs98_help_subobject *control = &state.class_info->controls[state.constructed];
        void *child = (uint8_t *)object + control->offset;
        s_services->construct(child, control->constructor);
        ++state.constructed;
        if (kind == 1 && control->offset == 0x1274) PTR(child, 0) = s_placeholder_vtable;
    }
    PTR(object, 0) = s_vtables[kind];
    if (kind == 1) {
        D32(object, 0x1108) = 1;
        D32(object, 0x10e8) = 0;
        D32(object, 0x10f4) = 0;
    }
    fs98_help_unwind_leave(&state.frame);
    return object;
}
static FS98_HELP_NOINLINE void fs98_help_destroy(void *object, uint32_t kind)
{
    struct fs98_help_object_unwind state;
    state.object = object; state.class_info = &s_classes[kind];
    state.constructed = state.class_info->count; state.base = 1;
    fs98_help_unwind_enter(&state.frame, fs98_help_object_cleanup);
    fs98_help_object_cleanup(&state.frame);
    fs98_help_unwind_leave(&state.frame);
}
struct fs98_help_factory_unwind {
    struct fs98_help_unwind frame;
    void *object;
    volatile uint32_t live;
};
static void fs98_help_factory_cleanup(struct fs98_help_unwind *frame)
{
    struct fs98_help_factory_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; s_services->dialog_delete(state->object); }
}
static FS98_HELP_NOINLINE uint32_t fs98_help_create(uint32_t parent, void *descriptor, uint32_t kind)
{
    struct fs98_help_factory_unwind state;
    const struct fs98_help_class *class_info = &s_classes[kind];
    void *object;
    (void)parent;
    state.object = 0; state.live = 0;
    fs98_help_unwind_enter(&state.frame, fs98_help_factory_cleanup);
    object = s_services->allocate(class_info->size);
    state.object = object; state.live = 1;
    if (object) object = fs98_help_construct(object, 0, kind);
    *(void *volatile *)((uint8_t *)object + class_info->descriptor) = descriptor;
    state.live = 0;
    D32(object, 0x80) = s_module;
    s_services->create(object, class_info->template, 0);
    s_services->show(object, 1);
    fs98_help_unwind_leave(&state.frame);
    return D32(object, 0x20);
}
#define OBJECT_CLASS(name, kind) \
void *TC fs98_help_##name##_construct(void *object, void *parent) \
{ return fs98_help_construct(object, parent, kind); } \
void TC fs98_help_##name##_destroy(void *object) \
{ fs98_help_destroy(object, kind); } \
void *TC fs98_help_##name##_delete(void *object, uint32_t flags) \
{ \
    fs98_help_destroy(object, kind); \
    if (flags & 1) s_services->ui_delete(object); \
    return object; \
} \
uint32_t SC fs98_help_##name##_create(uint32_t parent, void *descriptor) \
{ return fs98_help_create(parent, descriptor, kind); }
OBJECT_CLASS(about, 0)
OBJECT_CLASS(credits, 1)
OBJECT_CLASS(lessons, 2)
OBJECT_CLASS(graph, 3)
OBJECT_CLASS(completion, 4)
#undef OBJECT_CLASS
void *TC fs98_help_placeholder_delete(void *object, uint32_t flags)
{
    s_services->destroy(object, 0x634c);
    if (flags & 1) s_services->mfc_delete(object);
    return object;
}
void SC fs98_help_class_delete(void *object) { s_services->mfc_delete(object); }
