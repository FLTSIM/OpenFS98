/* Retail Alerts' object lifecycle and three factory functions, including
 * partial-construction cleanup across the Microsoft C++ dependency boundary. */
#include "alerts_objects.h"
#include "alerts_unwind.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))

static const struct fs98_alerts_object_services *s_services;
static struct fs98_alerts_object_tables s_tables;
static uint32_t s_module;

struct fs98_alerts_object_unwind {
    struct fs98_alerts_unwind frame;
    void *object;
    uint32_t kind;
    volatile uint32_t stage, buttons;
};

static void fs98_alerts_object_cleanup(struct fs98_alerts_unwind *frame)
{
    struct fs98_alerts_object_unwind *state = (void *)frame;
    uint8_t *object = state->object;
    if (state->kind == 3) {
        if (state->stage) {
            state->stage = 0;
            s_services->dialog_delete(object);
        }
        return;
    }
    if (state->kind == 0) {
        while (state->buttons) {
            state->buttons--;
            s_services->button_destroy(object + 0x11b4 + state->buttons * 0xfc);
        }
        if (state->stage == 3) {
            state->stage = 2;
            s_services->text_destroy(object + 0x10fc);
        }
        if (state->stage == 2) {
            state->stage = 1;
            s_services->string_destroy(object + 0x10f0);
        }
    } else if (state->kind == 1) {
        if (state->stage == 3) {
            state->stage = 2;
            s_services->text_destroy(object + 0x1124);
        }
        if (state->stage == 2) {
            state->stage = 1;
            s_services->window_destroy(object + 0x10e4);
        }
    } else {
        if (state->stage == 7) {
            state->stage = 6;
            s_services->check_destroy(object + 0x1494);
        }
        if (state->stage == 6) {
            state->stage = 5;
            s_services->button_destroy(object + 0x1398);
        }
        if (state->stage == 5) {
            state->stage = 4;
            s_services->text_destroy(object + 0x12e0);
        }
        if (state->stage == 4) {
            state->stage = 3;
            s_services->button_destroy(object + 0x11e4);
        }
        if (state->stage == 3) {
            state->stage = 2;
            s_services->button_destroy(object + 0x10e8);
        }
        if (state->stage == 2) {
            state->stage = 1;
            s_services->string_destroy(object + 0x10e4);
        }
    }
    if (state->stage == 1) {
        state->stage = 0;
        s_services->dialog_destroy(object);
    }
}

static void fs98_alerts_object_scope(struct fs98_alerts_object_unwind *state,
                                     void *object, uint32_t kind, uint32_t stage)
{
    state->object = object;
    state->kind = kind;
    state->stage = stage;
    state->buttons = 0;
    fs98_alerts_unwind_enter(&state->frame, fs98_alerts_object_cleanup);
}

void fs98_alerts_set_object_services(const struct fs98_alerts_object_services *services,
                                    const struct fs98_alerts_object_tables *tables)
{
    s_services = services;
    s_tables = *tables;
}

void fs98_alerts_set_object_module(uint32_t module)
{
    s_module = module;
}

/* 0x223012f0: the three buttons are constructed in ascending order. */
FS98_ALERTS_NOINLINE void *FS98_ALERTS_THISCALL fs98_alerts_modal_construct(void *object, void *parent)
{
    uint32_t index;
    struct fs98_alerts_object_unwind state;
    fs98_alerts_object_scope(&state, object, 0, 0);
    s_services->dialog_construct(object, 0x8a, parent);
    state.stage = 1;
    s_services->string_construct((uint8_t *)object + 0x10f0);
    state.stage = 2;
    s_services->text_construct((uint8_t *)object + 0x10fc);
    state.stage = 3;
    for (index = 0; index < 3; index++) {
        s_services->button_construct((uint8_t *)object + 0x11b4 + index * 0xfc);
        state.buttons++;
    }
    D32(object, 0x10dc) = 0;
    D32(object, 0x10e0) = 0;
    D32(object, 0x10e4) = 0;
    D32(object, 0x10e8) = 0;
    D32(object, 0x10ec) = 0;
    D32(object, 0x10f8) = 0xffffffff;
    D32(object, 0x10f4) = 0xffffffff;
    PTR(object, 0) = s_tables.modal;
    fs98_alerts_unwind_leave(&state.frame);
    return object;
}

/* 0x22301a50: the embedded CWnd gets a derived progress vtable before text. */
FS98_ALERTS_NOINLINE void *FS98_ALERTS_THISCALL fs98_alerts_modeless_construct(void *object, void *parent)
{
    struct fs98_alerts_object_unwind state;
    fs98_alerts_object_scope(&state, object, 1, 0);
    s_services->dialog_construct(object, 0x8b, parent);
    state.stage = 1;
    s_services->window_construct((uint8_t *)object + 0x10e4);
    state.stage = 2;
    PTR(object, 0x10e4) = s_tables.progress;
    s_services->text_construct((uint8_t *)object + 0x1124);
    state.stage = 3;
    PTR(object, 0) = s_tables.modeless;
    D32(object, 0x10dc) = 0;
    D32(object, 0x10e0) = 0;
    fs98_alerts_unwind_leave(&state.frame);
    return object;
}

/* 0x223020d0: a CString, Quit, Help, message, OK, then checkbox. */
FS98_ALERTS_NOINLINE void *FS98_ALERTS_THISCALL fs98_alerts_advisory_construct(void *object, void *parent)
{
    struct fs98_alerts_object_unwind state;
    fs98_alerts_object_scope(&state, object, 2, 0);
    s_services->dialog_construct(object, 0x8c, parent);
    state.stage = 1;
    s_services->string_construct((uint8_t *)object + 0x10e4);
    state.stage = 2;
    s_services->button_construct((uint8_t *)object + 0x10e8);
    state.stage = 3;
    s_services->button_construct((uint8_t *)object + 0x11e4);
    state.stage = 4;
    s_services->text_construct((uint8_t *)object + 0x12e0);
    state.stage = 5;
    s_services->button_construct((uint8_t *)object + 0x1398);
    state.stage = 6;
    s_services->check_construct((uint8_t *)object + 0x1494);
    state.stage = 7;
    PTR(object, 0) = s_tables.advisory;
    D32(object, 0x10dc) = 0;
    D32(object, 0x10e0) = 0;
    fs98_alerts_unwind_leave(&state.frame);
    return object;
}

/* 0x223013d0 / 0x22301b00 / 0x223021b0: the derived vtable is not reset on
 * entry. This can matter to callbacks made by the dependency destructors. */
FS98_ALERTS_NOINLINE void FS98_ALERTS_THISCALL fs98_alerts_modal_destroy(void *object)
{
    struct fs98_alerts_object_unwind state;
    fs98_alerts_object_scope(&state, object, 0, 3);
    state.buttons = 3;
    fs98_alerts_object_cleanup(&state.frame);
    fs98_alerts_unwind_leave(&state.frame);
}

FS98_ALERTS_NOINLINE void FS98_ALERTS_THISCALL fs98_alerts_modeless_destroy(void *object)
{
    struct fs98_alerts_object_unwind state;
    fs98_alerts_object_scope(&state, object, 1, 3);
    fs98_alerts_object_cleanup(&state.frame);
    fs98_alerts_unwind_leave(&state.frame);
}

FS98_ALERTS_NOINLINE void FS98_ALERTS_THISCALL fs98_alerts_advisory_destroy(void *object)
{
    struct fs98_alerts_object_unwind state;
    fs98_alerts_object_scope(&state, object, 2, 7);
    fs98_alerts_object_cleanup(&state.frame);
    fs98_alerts_unwind_leave(&state.frame);
}

/* Scalar deleting destructors: only low-byte bit 0 controls deallocation. */
void *FS98_ALERTS_THISCALL fs98_alerts_modal_delete(void *object, uint32_t flags)
{
    fs98_alerts_modal_destroy(object);
    if (flags & 1)
        s_services->ui_delete(object);
    return object;
}

void *FS98_ALERTS_THISCALL fs98_alerts_modeless_delete(void *object, uint32_t flags)
{
    fs98_alerts_modeless_destroy(object);
    if (flags & 1)
        s_services->ui_delete(object);
    return object;
}

void *FS98_ALERTS_THISCALL fs98_alerts_advisory_delete(void *object, uint32_t flags)
{
    fs98_alerts_advisory_destroy(object);
    if (flags & 1)
        s_services->ui_delete(object);
    return object;
}

void *FS98_ALERTS_THISCALL fs98_alerts_progress_delete(void *object, uint32_t flags)
{
    s_services->window_destroy(object);
    if (flags & 1)
        s_services->mfc_delete(object);
    return object;
}

void FS98_ALERTS_THISCALL fs98_alerts_post_destroy(void *object)
{
    typedef void *(FS98_ALERTS_THISCALL *fs98_delete_fn)(void *self, uint32_t flags);
    if (object) {
        fs98_delete_fn *vtable = PTR(object, 0);
        vtable[1](object, 1);
    }
}

void FS98_ALERTS_THISCALL fs98_alerts_disable(void *object)
{
    s_services->enable(D32(object, 0x20), 0);
}

void FS98_ALERTS_THISCALL fs98_alerts_enable(void *object)
{
    s_services->enable(D32(object, 0x20), 1);
}

void FS98_ALERTS_THISCALL fs98_alerts_empty_arg(void *object, uint32_t argument)
{
    (void)object;
    (void)argument;
}

typedef void *(FS98_ALERTS_THISCALL *fs98_construct_fn)(void *object, void *parent);

static FS98_ALERTS_NOINLINE uint32_t fs98_alerts_create(void *descriptor, uint32_t size,
                                 uint32_t template, fs98_construct_fn construct)
{
    struct fs98_alerts_object_unwind state;
    void *object;
    fs98_alerts_object_scope(&state, 0, 3, 0);
    object = s_services->allocate(size);
    state.object = object;
    state.stage = 1;
    if (object)
        object = construct(object, 0);
    /* Retail deliberately continues through NULL on allocation failure. */
    *(void *volatile *)((uint8_t *)object + 0x10dc) = descriptor;
    D32(object, 0x10e0) = s_module;
    state.stage = 0;
    s_services->create(object, template, 0);
    s_services->show(object, 1);
    fs98_alerts_unwind_leave(&state.frame);
    return D32(object, 0x20);
}

uint32_t FS98_ALERTS_STDCALL fs98_alerts_modal_create(uint32_t parent, void *descriptor)
{
    (void)parent;
    return fs98_alerts_create(descriptor, 0x14a8, 0x8a, fs98_alerts_modal_construct);
}

uint32_t FS98_ALERTS_STDCALL fs98_alerts_modeless_create(uint32_t parent, void *descriptor)
{
    (void)parent;
    return fs98_alerts_create(descriptor, 0x11dc, 0x8b, fs98_alerts_modeless_construct);
}

uint32_t FS98_ALERTS_STDCALL fs98_alerts_advisory_create(uint32_t parent, void *descriptor)
{
    (void)parent;
    return fs98_alerts_create(descriptor, 0x155c, 0x8c, fs98_alerts_advisory_construct);
}
