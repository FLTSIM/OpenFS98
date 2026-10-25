/* Views' derived dialog, 34 embedded controls, and public factory. */
#include "views_objects.h"
#include "views_unwind.h"
#include "views_objects_tables.inc"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define SUBOBJECT_COUNT (sizeof(s_subobjects) / sizeof(s_subobjects[0]))

static const struct fs98_views_object_services *s_services;
static void *s_dialog_vtable, *s_spin_vtable;
static uint32_t s_module;

struct fs98_views_object_unwind {
    struct fs98_views_unwind frame;
    void *object;
    volatile uint32_t constructed, base;
};
static void fs98_views_object_cleanup(struct fs98_views_unwind *frame)
{
    struct fs98_views_object_unwind *state = (void *)frame;
    while (state->constructed) {
        const struct fs98_views_subobject *control = &s_subobjects[--state->constructed];
        void *object = (uint8_t *)state->object + control->offset;
        if (control->offset == 0x27bc) PTR(object, 0) = s_spin_vtable;
        s_services->destroy(object, control->destructor);
    }
    if (state->base) {
        state->base = 0;
        s_services->dialog_destroy(state->object);
    }
}
void fs98_views_set_object_services(const struct fs98_views_object_services *services,
                                   void *dialog_vtable, void *spin_vtable)
{
    s_services = services;
    s_dialog_vtable = dialog_vtable;
    s_spin_vtable = spin_vtable;
}
void fs98_views_set_object_module(uint32_t module) { s_module = module; }

/* 0x223e11d0: no initialization of descriptor +0x2804 here. The transient
 * private spin vtable store is overwritten immediately, without a call or
 * branch between stores; only the final SIMSUI spin vtable is observable. */
FS98_VIEWS_NOINLINE void *FS98_VIEWS_THISCALL fs98_views_construct(void *object, void *parent)
{
    struct fs98_views_object_unwind state;
    state.object = object; state.constructed = state.base = 0;
    fs98_views_unwind_enter(&state.frame, fs98_views_object_cleanup);
    s_services->dialog_construct(object, 0x5e1, parent);
    state.base = 1;
    while (state.constructed < SUBOBJECT_COUNT) {
        const struct fs98_views_subobject *control = &s_subobjects[state.constructed];
        s_services->construct((uint8_t *)object + control->offset, control->constructor);
        state.constructed++;
    }
    PTR(object, 0x27bc) = s_spin_vtable;
    PTR(object, 0) = s_dialog_vtable;
    fs98_views_unwind_leave(&state.frame);
    return object;
}

/* 0x223e1450: the main vtable is retained; the spin resets its shared one. */
FS98_VIEWS_NOINLINE void FS98_VIEWS_THISCALL fs98_views_destroy(void *object)
{
    struct fs98_views_object_unwind state;
    state.object = object; state.constructed = SUBOBJECT_COUNT; state.base = 1;
    fs98_views_unwind_enter(&state.frame, fs98_views_object_cleanup);
    fs98_views_object_cleanup(&state.frame);
    fs98_views_unwind_leave(&state.frame);
}
void *FS98_VIEWS_THISCALL fs98_views_delete(void *object, uint32_t flags)
{
    fs98_views_destroy(object);
    if (flags & 1) s_services->ui_delete(object);
    return object;
}
void *FS98_VIEWS_THISCALL fs98_views_spin_delete(void *object, uint32_t flags)
{
    s_services->destroy(object, 0x7314);
    if (flags & 1) s_services->mfc_delete(object);
    return object;
}
void FS98_VIEWS_STDCALL fs98_views_class_delete(void *object)
{ s_services->mfc_delete(object); }

struct fs98_views_factory_unwind {
    struct fs98_views_unwind frame;
    void *object;
    volatile uint32_t live;
};
static void fs98_views_factory_cleanup(struct fs98_views_unwind *frame)
{
    struct fs98_views_factory_unwind *state = (void *)frame;
    if (state->live) {
        state->live = 0;
        s_services->dialog_delete(state->object);
    }
}

/* 0x223e1130: parent ignored, null allocation still writes at 0x2804, and
 * Create/Show failures do not change the final HWND return. */
FS98_VIEWS_NOINLINE uint32_t FS98_VIEWS_STDCALL fs98_views_create(uint32_t parent, void *descriptor)
{
    struct fs98_views_factory_unwind state;
    void *object;
    (void)parent;
    state.object = 0; state.live = 0;
    fs98_views_unwind_enter(&state.frame, fs98_views_factory_cleanup);
    object = s_services->allocate(0x2808);
    state.object = object; state.live = 1;
    if (object) object = fs98_views_construct(object, 0);
    *(void *volatile *)((uint8_t *)object + 0x2804) = descriptor;
    state.live = 0;
    D32(object, 0x80) = s_module;
    s_services->create(object, 0x5e1, 0);
    s_services->show(object, 1);
    fs98_views_unwind_leave(&state.frame);
    return D32(object, 0x20);
}
