/* Options' dialog classes: construction, destruction, deleting destructors and the
 * factories the host's dialog registry calls. Each member is built and torn down
 * through its own import, with cleanup-only SEH frames that undo a partial
 * construction in reverse, as the retail compiler's funclets do. */
#include "options_objects.h"
#include "options_unwind.h"
#include "options_internal.h"
#include "options_objects_tables.inc"
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define TC FS98_OPTIONS_THISCALL
#define SC FS98_OPTIONS_STDCALL

static const struct fs98_options_object_services *s_services;
static void *s_vtables[FS98_OPTIONS_CLASS_COUNT];
static uint32_t s_module;
struct fs98_options_object_unwind {
    struct fs98_options_unwind frame;
    void *object;
    const struct fs98_options_class *class_info;
    volatile uint32_t constructed, base;
};
static void fs98_options_object_cleanup(struct fs98_options_unwind *frame)
{
    struct fs98_options_object_unwind *state = (void *)frame;
    while (state->constructed) {
        const struct fs98_options_subobject *control = &state->class_info->controls[--state->constructed];
        s_services->destroy((uint8_t *)state->object + control->offset, control->destructor);
    }
    if (state->base) {
        state->base = 0;
        if (state->class_info->page) s_services->page_destroy(state->object);
        else s_services->dialog_destroy(state->object);
    }
}
void fs98_options_set_object_services(const struct fs98_options_object_services *services,
                                      void *const *vtables)
{
    uint32_t index;
    s_services = services;
    for (index = 0; index < FS98_OPTIONS_CLASS_COUNT; ++index) s_vtables[index] = vtables[index];
}
void fs98_options_set_object_module(uint32_t module) { s_module = module; }

/* 0x2239de00 and the like: CSimsDialog(template, parent), then each member in order */
static FS98_OPTIONS_NOINLINE void *fs98_options_construct(void *object, void *parent, uint32_t kind)
{
    struct fs98_options_object_unwind state;
    state.object = object; state.class_info = &s_classes[kind];
    state.constructed = state.base = 0;
    fs98_options_unwind_enter(&state.frame, fs98_options_object_cleanup);
    if (state.class_info->page) s_services->page_construct(object, state.class_info->template, parent);
    else s_services->dialog_construct(object, state.class_info->template, parent);
    state.base = 1;
    while (state.constructed < state.class_info->count) {
        const struct fs98_options_subobject *control = &state.class_info->controls[state.constructed];
        s_services->construct((uint8_t *)object + control->offset, control->constructor);
        ++state.constructed;
    }
    PTR(object, 0) = s_vtables[kind];
    fs98_options_unwind_leave(&state.frame);
    return object;
}
/* 0x2239de90 and the like: the members in reverse, then CSimsDialog */
static FS98_OPTIONS_NOINLINE void fs98_options_destroy(void *object, uint32_t kind)
{
    struct fs98_options_object_unwind state;
    state.object = object; state.class_info = &s_classes[kind];
    state.constructed = state.class_info->count; state.base = 1;
    fs98_options_unwind_enter(&state.frame, fs98_options_object_cleanup);
    fs98_options_object_cleanup(&state.frame);
    fs98_options_unwind_leave(&state.frame);
}
struct fs98_options_factory_unwind {
    struct fs98_options_unwind frame;
    void *object;
    volatile uint32_t live;
};
static void fs98_options_factory_cleanup(struct fs98_options_unwind *frame)
{
    struct fs98_options_factory_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; s_services->dialog_delete(state->object); }
}
/* 0x22391810 and the like. A failed allocation leaves the object null and the stores
 * fault, as retail's do; the parent argument is ignored. Video finished stores no
 * descriptor. */
static FS98_OPTIONS_NOINLINE uint32_t fs98_options_create(uint32_t parent, void *descriptor, uint32_t kind)
{
    struct fs98_options_factory_unwind state;
    const struct fs98_options_class *class_info = &s_classes[kind];
    void *object;
    (void)parent;
    state.object = 0; state.live = 0;
    fs98_options_unwind_enter(&state.frame, fs98_options_factory_cleanup);
    object = s_services->allocate(class_info->size);
    state.object = object; state.live = 1;
    if (object) object = fs98_options_construct(object, 0, kind);
    if (class_info->descriptor)
        *(void *volatile *)((uint8_t *)object + class_info->descriptor) = descriptor;
    state.live = 0;
    D32(object, 0x80) = s_module;
    s_services->create(object, class_info->template, 0);
    s_services->show(object, 1);
    fs98_options_unwind_leave(&state.frame);
    return D32(object, 0x20);
}
#define OBJECT_CLASS(name, kind) \
void *TC fs98_options_##name##_construct(void *object, void *parent) \
{ return fs98_options_construct(object, parent, kind); } \
void TC fs98_options_##name##_destroy(void *object) \
{ fs98_options_destroy(object, kind); } \
void *TC fs98_options_##name##_delete(void *object, uint32_t flags) \
{ \
    fs98_options_destroy(object, kind); \
    if (flags & 1) s_services->ui_delete(object); \
    return object; \
} \
uint32_t SC fs98_options_##name##_create(uint32_t parent, void *descriptor) \
{ return fs98_options_create(parent, descriptor, kind); }
OBJECT_CLASS(video_finished, FS98_OPTIONS_VIDEO_FINISHED)
OBJECT_CLASS(video_message, FS98_OPTIONS_VIDEO_MESSAGE)
#undef OBJECT_CLASS
