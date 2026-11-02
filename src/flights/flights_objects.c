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
static void *s_static_vtable;
#define STATIC_CONSTRUCT 0x1338c     /* MFC CWnd(), under an inline CSimsStatic */
#define STATIC_DESTROY 0x13388       /* ~CWnd */
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
        /* 0x22319611: ~CSimsStatic, inline, puts its own vtable back first */
        if (control->destructor == STATIC_DESTROY) PTR((uint8_t *)state->object + control->offset, 0) = s_static_vtable;
        s_services->destroy((uint8_t *)state->object + control->offset, control->destructor);
    }
    if (state->base) {
        state->base = 0;
        if (state->class_info->page) s_services->page_destroy(state->object);
        else s_services->dialog_destroy(state->object);
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
void fs98_flights_set_static_vtable(void *vtable) { s_static_vtable = vtable; }

/* 0x22312c10 and the like: CSimsDialog(template, parent), then each member in order */
static FS98_FLIGHTS_NOINLINE void *fs98_flights_construct(void *object, void *parent, uint32_t kind)
{
    struct fs98_flights_object_unwind state;
    state.object = object; state.class_info = &s_classes[kind];
    state.constructed = state.base = 0;
    fs98_flights_unwind_enter(&state.frame, fs98_flights_object_cleanup);
    if (state.class_info->page) s_services->page_construct(object, state.class_info->template, parent);
    else s_services->dialog_construct(object, state.class_info->template, parent);
    state.base = 1;
    while (state.constructed < state.class_info->count) {
        const struct fs98_flights_subobject *control = &state.class_info->controls[state.constructed];
        s_services->construct((uint8_t *)object + control->offset, control->constructor);
        /* 0x22319588: CSimsStatic(), inline, after CWnd(): CStatic's vtable (0x2231f7d0),
         * at once replaced by its own. The first store is never seen and is left out. */
        if (control->constructor == STATIC_CONSTRUCT) PTR((uint8_t *)object + control->offset, 0) = s_static_vtable;
        ++state.constructed;
    }
    PTR(object, 0) = s_vtables[kind];
    /* 0x2231602b: Save forgets the name it last confirmed, inside the constructor's frame */
    if (kind == FS98_FLIGHTS_SAVE) s_services->copy_text((char *)object + 0x10dc, fs98_flights_empty_text);
    /* 0x22319ef7, 0x2231aa6b, 0x2231b2db: the menus start with no callback running */
    if (kind == FS98_FLIGHTS_OPENING || kind == FS98_FLIGHTS_FLIGHT_MENU || kind == FS98_FLIGHTS_LEARNING_MENU)
        D32(object, 0x10dc) = 0;
    /* 0x2231716d: the third tab page's CString starts empty */
    if (kind == FS98_FLIGHTS_NAV_EFIS) s_services->string_assign((uint8_t *)object + 0x1b60, fs98_flights_empty_text);
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
    /* 0x22317e7e: Navigation's destructor stores its own vtable back first */
    if (kind == FS98_FLIGHTS_NAVIGATION) PTR(object, 0) = s_vtables[kind];
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
/* 0x22318146, 0x22318186, 0x223181c0: Navigation's OnInitDialog makes its pages so, with
 * the block in its frame (state 0, 1, 2) only while the constructor runs */
void *fs98_flights_new_page(uint32_t kind, void *parent)
{
    struct fs98_flights_factory_unwind state;
    void *object;
    state.object = 0; state.live = 0;
    fs98_flights_unwind_enter(&state.frame, fs98_flights_factory_cleanup);
    object = s_services->allocate(s_classes[kind].size);
    state.object = object; state.live = 1;
    if (object) object = fs98_flights_construct(object, parent, kind);
    state.live = 0;
    fs98_flights_unwind_leave(&state.frame);
    return object;
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
OBJECT_CLASS(flight_menu, FS98_FLIGHTS_FLIGHT_MENU)
OBJECT_CLASS(learning_menu, FS98_FLIGHTS_LEARNING_MENU)
OBJECT_CLASS(select, FS98_FLIGHTS_SELECT)
OBJECT_CLASS(autopilot, FS98_FLIGHTS_AUTOPILOT)
OBJECT_CLASS(navigation, FS98_FLIGHTS_NAVIGATION)
#undef OBJECT_CLASS
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
}
OBJECT_CLASS(nav_efis, FS98_FLIGHTS_NAV_EFIS)
OBJECT_CLASS(nav_radios, FS98_FLIGHTS_NAV_RADIOS)
OBJECT_CLASS(nav_adf, FS98_FLIGHTS_NAV_ADF)
#undef OBJECT_CLASS
