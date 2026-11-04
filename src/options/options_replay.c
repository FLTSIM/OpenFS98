/* Instant replay (0x1403, template 0x391, class at 0x22396990, 0x1a38 bytes), DEMO's
 * replay_dialog, its block described in options_internal.h.
 *
 * Members: the labels 0x10ec and 0x11a4 (in the bold font), 0x125c (the speed's), 0x1314
 * (the length's), 0x13cc and 0x1484 (the caption), title art 0x153c, two spin buttons 0x1624
 * (window +0x1644) and 0x1664 (+0x1684), the speed edit 0x16a4 (+0x16c4) and the length
 * edit 0x170c (+0x172c), the loop box 0x1774 (state +0x1830), OK 0x183c and Cancel
 * 0x1938, and an int 0x1a34 that DDX binds to the speed edit. +0x10dc holds the CNotify,
 * +0x10e0 the block, +0x10e4 and +0x10e8 Cancel and OK once they exist. */
#include "options_internal.h"
#include "options_unwind.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_OPTIONS_THISCALL
#define SERVICES fs98_options_services()
#define BLOCK(object) PTR(object, 0x10e0)
#define UDM_SETRANGE_ 0x465       /* the spin buttons (CSimsSpinButtonCtrl): their range */
#define UDM_SETPOS_ 0x467         /* and their position */

static void button(void *object, uint32_t at)
{
    struct fs98_options_rect empty = {0, 0, 0, 0};
    SERVICES->init_button(AT(object, at), AT(object, 0x5a0), AT(object, 0x610), 0x132, AT(object, 0xff4),
                          AT(object, 0x7e8), 1, 0xffffffff, fs98_options_resource_module(), 1, 1, empty);
}
static void edit(void *object, uint32_t at, uint32_t label)
{
    SERVICES->init_edit(AT(object, at), SERVICES->base_font(2), 0xffffff, 0, 0xff0000, AT(object, label), 0);
}
static void fit(void *object, uint32_t at)
{
    struct fs98_options_rect rect;
    SERVICES->remap_rect(&rect, AT(object, at));
    SERVICES->move_window(AT(object, at), rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, 1);
}

/* 0x22396d90, OnCancel */
void TC fs98_options_replay_cancel(void *object)
{
    uint32_t *block = BLOCK(object);
    void *vtable = PTR(object, 0);
    block[0] = 0;
    ((void (TC *)(void *))PTR(vtable, 0xec))(object);
}
/* 0x22396db0, OnOK: validate and report through the vtable read once */
void TC fs98_options_replay_accept(void *object)
{
    void **vtable = PTR(object, 0);
    uint32_t failure = ((uint32_t (TC *)(void *, uint32_t))vtable[0xd8 / 4])(object, 0);
    if (failure != 0 && ((uint32_t (TC *)(void *, uint32_t))vtable[0xdc / 4])(object, failure) != 0)
        return;
    D32(BLOCK(object), 0) = 1;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}
/* 0x223972d0 */
uint32_t TC fs98_options_replay_has_descriptor(void *object) { return BLOCK(object) != 0; }
/* 0x22397150: the speed and the length into their edits, the loop box, the spin buttons (the
 * second's range up to the length's maximum), and the caption: string 0xf757 while a
 * replay runs, when everything else is off, or 0xf752 */
void TC fs98_options_replay_populate(void *object)
{
    char text[0x14];
    SERVICES->long_to_text((int32_t)D32(BLOCK(object), 0x30), text, 10);
    SERVICES->set_window_text(AT(object, 0x16a4), text);
    SERVICES->long_to_text((int32_t)D32(BLOCK(object), 0x14), text, 10);
    SERVICES->set_window_text(AT(object, 0x170c), text);
    SERVICES->set_checked(AT(object, 0x1774), (int32_t)D32(BLOCK(object), 0x4c));
    SERVICES->send_message(D32(object, 0x1644), UDM_SETPOS_, 0, D32(BLOCK(object), 0x30) & 0xffff);
    SERVICES->send_message(D32(object, 0x1684), UDM_SETRANGE_, 0, (D32(BLOCK(object), 0xc) & 0xffff) | 0x10000);
    SERVICES->send_message(D32(object, 0x1684), UDM_SETPOS_, 0, D32(BLOCK(object), 0x14) & 0xffff);
    SERVICES->load_string_a(D32(object, 0x80), D32(BLOCK(object), 8) ? 0xf757 : 0xf752, text, 0x13);
    SERVICES->set_sims_text(AT(object, 0x1484), text);
    if (D32(BLOCK(object), 8) == 0) return;
    SERVICES->enable_control(AT(object, 0x1774), 0);
    SERVICES->enable_control(AT(object, 0x16a4), 0);
    SERVICES->enable_control(AT(object, 0x170c), 0);
    SERVICES->enable_control(AT(object, 0x1624), 0);
    SERVICES->enable_control(AT(object, 0x1664), 0);
}
/* 0x22396df0, OnInitDialog */
int32_t TC fs98_options_replay_init(void *object)
{
    void *check;
    SERVICES->base_init(object);
    if (!((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(
            object, 0x14f, fs98_options_resource_module(), 0xf)) return 0;
    D32(object, 0x84) = 0xb;
    SERVICES->init_art(AT(object, 0x153c), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    button(object, 0x183c);
    button(object, 0x1938);
    SERVICES->init_text(AT(object, 0x13cc), AT(object, 0x220));
    SERVICES->init_text(AT(object, 0x1484), AT(object, 0x220));
    SERVICES->init_text(AT(object, 0x10ec), AT(object, 0x4c0));
    SERVICES->init_text(AT(object, 0x11a4), AT(object, 0x4c0));
    SERVICES->init_text(AT(object, 0x1314), AT(object, 0x220));
    edit(object, 0x170c, 0x1314);
    SERVICES->init_text(AT(object, 0x125c), AT(object, 0x220));
    edit(object, 0x16a4, 0x125c);
    check = AT(object, 0x1774);
    ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))PTR(PTR(check, 0), 0xc4))(
        check, AT(object, 0x4c0), 0x139, AT(object, 0xff4), AT(object, 0x7e8), fs98_options_resource_module());
    SERVICES->send_message(D32(object, 0x1644), UDM_SETRANGE_, 0, 0x1400ff);
    fit(object, 0x1624);
    fit(object, 0x1664);
    PTR(object, 0x10dc) = fs98_options_new_notify();
    if (!fs98_options_replay_has_descriptor(object)) return 0;
    SERVICES->send_message(D32(object, 0x172c), EM_LIMITTEXT, 7, 0);
    SERVICES->send_message(D32(object, 0x16c4), EM_LIMITTEXT, 3, 0);
    fs98_options_replay_populate(object);
    PTR(object, 0x10e8) = AT(object, 0x183c);
    PTR(object, 0x10e4) = AT(object, 0x1938);
    return 1;
}
struct validate_unwind {
    struct fs98_options_unwind frame;
    struct fs98_options_string *text;
    volatile uint32_t live;
};
static void validate_cleanup(struct fs98_options_unwind *frame)
{
    struct validate_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; SERVICES->string_destroy(state->text); }
}
/* 0x223972e0, vtable +0xd8: the length and the speed as numbers and, unless a replay
 * runs, the loop box into the block; DEMO's check, its answer unused; a failure names
 * the speed's edit first, then the length's */
FS98_OPTIONS_NOINLINE uint32_t TC fs98_options_replay_validate(void *object, uint32_t unused)
{
    struct validate_unwind state;
    struct fs98_options_string text;
    void *block;
    uint32_t failure = 0;
    (void)unused;
    state.text = &text; state.live = 0;
    fs98_options_unwind_enter(&state.frame, validate_cleanup);
    SERVICES->string_construct(&text);
    state.live = 1;
    SERVICES->window_text_string(AT(object, 0x170c), &text);
    D32(BLOCK(object), 0x14) = (uint32_t)SERVICES->to_long(SERVICES->get_buffer(&text, 0));
    SERVICES->window_text_string(AT(object, 0x16a4), &text);
    D32(BLOCK(object), 0x30) = (uint32_t)SERVICES->to_long(SERVICES->get_buffer(&text, 0));
    if (D32(BLOCK(object), 8) == 0) D32(BLOCK(object), 0x4c) = D32(object, 0x1830);
    block = BLOCK(object);
    ((fs98_options_check_fn)PTR(block, 4))(block);
    block = BLOCK(object);
    if (D32(block, 0x2c) != 0) failure = 0x119d;
    else if (D32(block, 0x10) != 0) failure = 0x119c;
    validate_cleanup(&state.frame);
    fs98_options_unwind_leave(&state.frame);
    return failure;
}
/* 0x223973d0, vtable +0xdc: the length's or the speed's failure reported on its edit with
 * the value as text and the limit (the length's maximum, or 100), the focus to it when
 * accepted.
 *
 * RETAIL BUG (kept): OPT-8 (docs/retail-bugs.md). The text is this frame's buffer, and its
 * address is left in DEMO's block after the dialog has gone. */
uint32_t TC fs98_options_replay_error(void *object, uint32_t code)
{
    char value[0x40];
    uint32_t target = 0;
    if (code != 0) {
        uint8_t *block = BLOCK(object);
        if (D32(block, 0x10) != 0) {
            SERVICES->format_int(value, "%d", (int32_t)D32(block, 0x14));
            D32(BLOCK(object), 0x18) = D32(BLOCK(object), 0x10);
            PTR(BLOCK(object), 0x1c) = value;
            D32(BLOCK(object), 0x20) = D32(BLOCK(object), 0xc);
            if (SERVICES->error_dialog(object, AT(object, 0x170c), (uint32_t)(uintptr_t)((uint8_t *)BLOCK(object) + 0x18)))
                target = 0x119c;
        } else if (D32(block, 0x2c) != 0) {
            SERVICES->format_int(value, "%d", (int32_t)D32(block, 0x30));
            D32(BLOCK(object), 0x34) = D32(BLOCK(object), 0x2c);
            PTR(BLOCK(object), 0x38) = value;
            D32(BLOCK(object), 0x3c) = 0x64;
            if (SERVICES->error_dialog(object, AT(object, 0x16a4), (uint32_t)(uintptr_t)((uint8_t *)BLOCK(object) + 0x34)))
                target = 0x119d;
        }
    }
    return ((uint32_t (TC *)(void *, uint32_t))PTR(PTR(object, 0), 0xe0))(object, target);
}
