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
#define SPIN_CONSTRUCT 0x1d528     /* MFC CWnd(), under an inline CSimsSpinButtonCtrl */
#define SPIN_DESTROY 0x1d524       /* ~CWnd */
#define SPIN_UNWIND 0x1d6cc        /* ~CSimsSpinButtonCtrl: the cleanup funclets' (0x223a4202) */
static void *s_spin_vtable;
void fs98_options_set_spin_vtable(void *vtable) { s_spin_vtable = vtable; }
struct fs98_options_object_unwind {
    struct fs98_options_unwind frame;
    void *object;
    const struct fs98_options_class *class_info;
    volatile uint32_t constructed, base;
    uint32_t direct;           /* the destructor's own teardown, not an unwind */
};
static void fs98_options_object_cleanup(struct fs98_options_unwind *frame)
{
    struct fs98_options_object_unwind *state = (void *)frame;
    while (state->constructed) {
        const struct fs98_options_subobject *control = &state->class_info->controls[--state->constructed];
        if (control->destructor == SPIN_DESTROY && !state->direct) {
            /* 0x223a4202: an unwind destroys a spin button through SIMSUI's destructor */
            s_services->destroy((uint8_t *)state->object + control->offset, SPIN_UNWIND);
            continue;
        }
        /* 0x22396b82: the destructor puts SIMSUI's vtable back first */
        if (control->destructor == SPIN_DESTROY) PTR((uint8_t *)state->object + control->offset, 0) = s_spin_vtable;
        s_services->destroy((uint8_t *)state->object + control->offset, control->destructor);
    }
    if (state->base) {
        state->base = 0;
        if (state->class_info->page) s_services->page_destroy(state->object);
        else s_services->dialog_destroy(state->object);
    }
}
/* the same, entered by an unwind: what is left is torn down as the funclets do */
static void fs98_options_object_unwind(struct fs98_options_unwind *frame)
{
    ((struct fs98_options_object_unwind *)(void *)frame)->direct = 0;
    fs98_options_object_cleanup(frame);
}
void fs98_options_set_object_services(const struct fs98_options_object_services *services,
                                      void *const *vtables)
{
    uint32_t index;
    s_services = services;
    for (index = 0; index < FS98_OPTIONS_CLASS_COUNT; ++index) s_vtables[index] = vtables[index];
}
void fs98_options_set_object_module(uint32_t module) { s_module = module; }
void fs98_options_dialog_delete(void *object) { s_services->dialog_delete(object); }
static void *s_list_vtable;
void fs98_options_set_object_list_vtable(void *vtable) { s_list_vtable = vtable; }
#define LIST_CONSTRUCT 0x1d6a8     /* CSimsListBox() */

/* 0x2239de00 and the like: CSimsDialog(template, parent), then each member in order */
static FS98_OPTIONS_NOINLINE void *fs98_options_construct(void *object, void *parent, uint32_t kind)
{
    struct fs98_options_object_unwind state;
    state.object = object; state.class_info = &s_classes[kind];
    state.constructed = state.base = 0; state.direct = 0;
    fs98_options_unwind_enter(&state.frame, fs98_options_object_unwind);
    if (state.class_info->page) s_services->page_construct(object, state.class_info->template, parent);
    else s_services->dialog_construct(object, state.class_info->template, parent);
    state.base = 1;
    while (state.constructed < state.class_info->count) {
        const struct fs98_options_subobject *control = &state.class_info->controls[state.constructed];
        s_services->construct((uint8_t *)object + control->offset, control->constructor);
        /* 0x22398189: the Logbook's rows list is Options' own class, its vtable at once */
        /* 0x22396a38: a spin button, after CWnd(): Options' CSpinButtonCtrl vtable, at once
         * replaced by SIMSUI's. The first store is never seen and is left out. */
        if (control->constructor == SPIN_CONSTRUCT) PTR((uint8_t *)object + control->offset, 0) = s_spin_vtable;
        if (kind == FS98_OPTIONS_BOOK && control->offset == 0x150c && control->constructor == LIST_CONSTRUCT)
            PTR((uint8_t *)object + control->offset, 0) = s_list_vtable;
        ++state.constructed;
    }
    PTR(object, 0) = s_vtables[kind];
    /* 0x2239d5e3: Save video forgets the name it last agreed to, inside the frame */
    if (kind == FS98_OPTIONS_SAVE_VIDEO) s_services->copy_text((char *)object + 0x10e4, fs98_options_empty_text);
    /* 0x2239c906: Flight video starts with no CNotify */
    if (kind == FS98_OPTIONS_FLIGHT_VIDEO) D32(object, 0x10dc) = 0;
    /* 0x2239a6e7, 0x223998c3..0x223998fb: the CStrings start empty */
    if (kind == FS98_OPTIONS_ADD_LOGBOOK) s_services->string_assign((char *)object + 0x1400, fs98_options_empty_text);
    /* 0x223979b1: the landing graph's caption is still to be made; 0x2239ac0d: the
     * manoeuvre's scale label too */
    if (kind == FS98_OPTIONS_LANDING) D32(object, 0x10e0) = 1;
    if (kind == FS98_OPTIONS_MANOEUVRE) D32(object, 0x1440) = 0;
    /* 0x22396ac1: Instant replay's speed starts at 0 */
    if (kind == FS98_OPTIONS_REPLAY) D32(object, 0x1a34) = 0;
    /* 0x22393bd7: Display options starts with no CNotify */
    if (kind == FS98_OPTIONS_DISPLAY) D32(object, 0x21ac) = 0;
    /* 0x2239b55e: and Preferences */
    if (kind == FS98_OPTIONS_PREFS) D32(object, 0x2334) = 0;
    if (kind == FS98_OPTIONS_BOOK) s_services->string_assign((char *)object + 0x1af0, fs98_options_empty_text);
    if (kind == FS98_OPTIONS_LOGBOOK_ENTRY) {
        uint32_t at;
        for (at = 0x1c90; at <= 0x1ca0; at += 4) s_services->string_assign((char *)object + at, fs98_options_empty_text);
    }
    fs98_options_unwind_leave(&state.frame);
    return object;
}
/* 0x2239de90 and the like: the members in reverse, then CSimsDialog */
static FS98_OPTIONS_NOINLINE void fs98_options_destroy(void *object, uint32_t kind)
{
    struct fs98_options_object_unwind state;
    state.object = object; state.class_info = &s_classes[kind];
    state.constructed = state.class_info->count; state.base = 1; state.direct = 1;
    fs98_options_unwind_enter(&state.frame, fs98_options_object_unwind);
    /* 0x22393c3e, 0x2239b5be: Display options and Preferences declare their destructors,
     * which store their vtables first */
    if (kind == FS98_OPTIONS_DISPLAY || kind == FS98_OPTIONS_PREFS) PTR(object, 0) = s_vtables[kind];
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
/* 0x22391810 and the like. RETAIL BUG (kept): FLT-3 (docs/retail-bugs.md). A failed
 * allocation leaves the object null and the stores fault, as retail's do; the parent argument is ignored. Video finished stores no
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
    if (kind == FS98_OPTIONS_LANDING || kind == FS98_OPTIONS_MANOEUVRE) {
        /* 0x223916b9, 0x22391749: the graphs store their block after the frame's end and
         * no module at +0x80, so their LoadStrings use what CSimsDialog's constructor left.
         * RETAIL BUG (kept, suspected): OPT-7 (docs/retail-bugs.md). */
        state.live = 0;
        *(void *volatile *)((uint8_t *)object + class_info->descriptor) = descriptor;
    } else {
        if (class_info->descriptor)
            *(void *volatile *)((uint8_t *)object + class_info->descriptor) = descriptor;
        state.live = 0;
        D32(object, 0x80) = s_module;
    }
    /* 0x2239162f, 0x22391595: Display options and Preferences say when their windows could
     * not be made */
    if (s_services->create(object, class_info->template, 0) == 0) {
        if (kind == FS98_OPTIONS_DISPLAY) s_services->message_box("CDisplayOptions::Create() failed", 0, 0);
        if (kind == FS98_OPTIONS_PREFS) s_services->message_box("CPreferences::Create() failed", 0, 0);
    }
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
OBJECT_CLASS(save_video, FS98_OPTIONS_SAVE_VIDEO)
OBJECT_CLASS(flight_video, FS98_OPTIONS_FLIGHT_VIDEO)
OBJECT_CLASS(add_logbook, FS98_OPTIONS_ADD_LOGBOOK)
OBJECT_CLASS(logbook_entry, FS98_OPTIONS_LOGBOOK_ENTRY)
OBJECT_CLASS(book, FS98_OPTIONS_BOOK)
OBJECT_CLASS(landing, FS98_OPTIONS_LANDING)
OBJECT_CLASS(manoeuvre, FS98_OPTIONS_MANOEUVRE)
OBJECT_CLASS(analysis, FS98_OPTIONS_ANALYSIS)
OBJECT_CLASS(replay, FS98_OPTIONS_REPLAY)
OBJECT_CLASS(demo, FS98_OPTIONS_DEMO)
OBJECT_CLASS(display, FS98_OPTIONS_DISPLAY)
OBJECT_CLASS(display_aircraft, FS98_OPTIONS_DISPLAY_AIRCRAFT)
OBJECT_CLASS(display_map, FS98_OPTIONS_DISPLAY_MAP)
OBJECT_CLASS(display_scenery, FS98_OPTIONS_DISPLAY_SCENERY)
OBJECT_CLASS(display_hardware, FS98_OPTIONS_DISPLAY_HARDWARE)
OBJECT_CLASS(prefs, FS98_OPTIONS_PREFS)
OBJECT_CLASS(prefs_general, FS98_OPTIONS_PREFS_GENERAL)
OBJECT_CLASS(prefs_instrument, FS98_OPTIONS_PREFS_INSTRUMENT)
OBJECT_CLASS(prefs_international, FS98_OPTIONS_PREFS_INTERNATIONAL)
OBJECT_CLASS(prefs_display, FS98_OPTIONS_PREFS_DISPLAY)
OBJECT_CLASS(prefs_sound, FS98_OPTIONS_PREFS_SOUND)
OBJECT_CLASS(prefs_flight, FS98_OPTIONS_PREFS_FLIGHT)
#undef OBJECT_CLASS
