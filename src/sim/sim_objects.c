/* Sim's dialog and tab-page lifetimes. Shared controls retain their exact
 * SIMSUI/MFC import identifiers; the runtime adapter supplies their ABI. */
#include "sim_internal.h"
#include "sim_unwind.h"
#include "sim_tables.inc"
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))

const struct fs98_sim_services *g_sim_services;
static void *s_vtables[FS98_SIM_CLASS_COUNT], *s_static_vtable, *s_control_vtable;
static uint32_t s_module;
uint32_t g_sim_resource_module;
void fs98_sim_set_resource_module(uint32_t module) { g_sim_resource_module = module; }
void fs98_sim_set_services(const struct fs98_sim_services *services, void *const *vtables, void *static_vtable, void *control_vtable)
{
    uint32_t index;
    g_sim_services = services;
    for (index = 0; index < FS98_SIM_CLASS_COUNT; ++index) s_vtables[index] = vtables[index];
    s_static_vtable = static_vtable;
    s_control_vtable = control_vtable;
}
void fs98_sim_set_module(uint32_t module) { s_module = module; }
struct fs98_sim_object_unwind {
    struct fs98_sim_unwind frame;
    void *object;
    const struct fs98_sim_class *class_info;
    volatile uint32_t constructed, base;
};
static void fs98_sim_destroy_remaining(struct fs98_sim_object_unwind *state, int unwinding)
{
    while (state->constructed) {
        const struct fs98_sim_control *control = &state->class_info->controls[--state->constructed];
        void *child = (uint8_t *)state->object + control->offset;
        if (!unwinding && control->destructor_vtable) PTR(child, 0) = control->destructor_vtable == 2 ? s_static_vtable : s_control_vtable;
        g_sim_services->destroy(child, unwinding ? control->unwind_destructor : control->destructor);
    }
    if (state->base) {
        state->base = 0;
        g_sim_services->destroy(state->object, state->class_info->base_destructor);
    }
}
static void fs98_sim_object_cleanup(struct fs98_sim_unwind *frame)
{ fs98_sim_destroy_remaining((struct fs98_sim_object_unwind *)frame, 1); }
FS98_SIM_NOINLINE void *fs98_sim_construct(void *object, void *parent, uint32_t kind)
{
    struct fs98_sim_object_unwind state;
    const struct fs98_sim_store *store;
    state.object = object; state.class_info = &s_classes[kind];
    state.constructed = state.base = 0;
    fs98_sim_unwind_enter(&state.frame, fs98_sim_object_cleanup);
    g_sim_services->base_construct(object, state.class_info->base_constructor,
                                  state.class_info->template, parent);
    state.base = 1;
    while (state.constructed < state.class_info->control_count) {
        const struct fs98_sim_control *control = &state.class_info->controls[state.constructed];
        void *child = (uint8_t *)object + control->offset;
        g_sim_services->construct(child, control->constructor);
        ++state.constructed;
        if (control->constructor_vtable) PTR(child, 0) = control->constructor_vtable == 2 ? s_static_vtable : s_control_vtable;
    }
    PTR(object, 0) = s_vtables[kind];
    for (store = state.class_info->stores; store->offset; ++store)
        D32(object, store->offset) = store->value;
    fs98_sim_unwind_leave(&state.frame);
    return object;
}
FS98_SIM_NOINLINE void fs98_sim_destroy(void *object, uint32_t kind)
{
    struct fs98_sim_object_unwind state;
    state.object = object; state.class_info = &s_classes[kind];
    state.constructed = state.class_info->control_count; state.base = 1;
    if (state.class_info->reset_vtable) PTR(object, 0) = s_vtables[kind];
    fs98_sim_unwind_enter(&state.frame, fs98_sim_object_cleanup);
    fs98_sim_destroy_remaining(&state, 0);
    fs98_sim_unwind_leave(&state.frame);
}
void *fs98_sim_delete(void *object, uint32_t flags, uint32_t kind)
{
    fs98_sim_destroy(object, kind);
    if (flags & 1) g_sim_services->ui_delete(object);
    return object;
}
struct fs98_sim_factory_unwind {
    struct fs98_sim_unwind frame;
    void *object;
    volatile uint32_t live;
};
static void fs98_sim_factory_cleanup(struct fs98_sim_unwind *frame)
{
    struct fs98_sim_factory_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; g_sim_services->dialog_delete(state->object); }
}
static FS98_SIM_NOINLINE uint32_t fs98_sim_create(uint32_t parent, void *descriptor, uint32_t kind, uint32_t page)
{
    struct fs98_sim_factory_unwind state;
    const struct fs98_sim_class *class_info = &s_classes[kind];
    void *object;
    (void)parent;
    state.object = 0; state.live = 0;
    fs98_sim_unwind_enter(&state.frame, fs98_sim_factory_cleanup);
    object = g_sim_services->allocate(class_info->size);
    state.object = object; state.live = 1;
    if (object) object = fs98_sim_construct(object, 0, kind);
    *(void *volatile *)((uint8_t *)object + (kind == 0 ? 0x112c : 0x10ec)) = descriptor;
    state.live = 0;
    D32(object, 0x80) = s_module;
    if (kind == 7) D32(object, 0x1fa8) = page;
    g_sim_services->create(object, class_info->template, 0);
    g_sim_services->show(object, 1);
    fs98_sim_unwind_leave(&state.frame);
    return D32(object, 0x20);
}
uint32_t FS98_SIM_STDCALL fs98_sim_create_crash(uint32_t parent, void *descriptor)
{ return fs98_sim_create(parent, descriptor, 0, 0); }
#define SETTINGS_FACTORY(page) \
uint32_t FS98_SIM_STDCALL fs98_sim_create_settings_##page(uint32_t parent, void *descriptor) \
{ return fs98_sim_create(parent, descriptor, 7, page); }
SETTINGS_FACTORY(0)
SETTINGS_FACTORY(2)
SETTINGS_FACTORY(3)
SETTINGS_FACTORY(5)
