/* Flight analysis (0x1405, template 0x38e, class at 0x22394ab0, 0x1b24 bytes), ANALYSIS's
 * options, its block described in options_internal.h.
 *
 * Members: title art 0x10e0, the combos' labels 0x11c8 and 0x1280, the combos 0x1338
 * (window +0x1358) and 0x1380 (+0x13a0), the radios off 0x1834 (+0x1854, 0x1137) and the
 * modes 0x14bc (+0x14dc, 0x1151), 0x13c8 (+0x13e8, 0x115e) and 0x1678 (+0x1698, 0x113d),
 * the boxes 0x15b0 (+0x15d0, state +0x166c, 0x1140) and 0x176c (+0x178c, state +0x1828,
 * 0x1138), Cancel 0x1928 and OK 0x1a24. The block is at +0x10dc, the answer state at
 * +0x1b20. A radio is read as SIMSUI's are: on when its window's user data is its own
 * handle. */
#include "options_internal.h"
#include "options_unwind.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_OPTIONS_THISCALL
#define SERVICES fs98_options_services()
#define BLOCK(object) PTR(object, 0x10dc)
#define GWL_USERDATA_ (-21)

/* a radio's window: on when its user data is itself, the handle read again after */
static int32_t on(void *object, uint32_t window)
{
    int32_t data = SERVICES->get_window_long(D32(object, window), GWL_USERDATA_);
    return D32(object, window) == (uint32_t)data;
}
/* the controls the mode's choices depend on */
static void dependents(void *object, int32_t enable)
{
    static const uint32_t controls[] = {0x176c, 0x15b0, 0x1338, 0x1380, 0x11c8, 0x1280};
    uint32_t index;
    for (index = 0; index < 6; ++index) SERVICES->enable_control(AT(object, controls[index]), enable);
}
static void check_init(void *object, uint32_t at, uint32_t bitmap)
{
    void *check = AT(object, at);
    ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))PTR(PTR(check, 0), 0xc4))(
        check, AT(object, 0x4c0), bitmap, AT(object, 0xff4), AT(object, 0x7e8), fs98_options_resource_module());
}

/* 0x223957e0 */
uint32_t TC fs98_options_has_descriptor(void *object) { return BLOCK(object) != 0; }
/* 0x22395380: the boxes' clicks do nothing */
void TC fs98_options_analysis_nothing(void *object) { (void)object; }
/* 0x22395180 (off), 0x22395280 (the first mode), 0x22395300 (the second): when on, the
 * dependents off; 0x22395200 (the third): when on, the dependents on */
void TC fs98_options_analysis_off(void *object) { if (on(object, 0x1854)) dependents(object, 0); }
void TC fs98_options_analysis_first(void *object) { if (on(object, 0x14dc)) dependents(object, 0); }
void TC fs98_options_analysis_second(void *object) { if (on(object, 0x13e8)) dependents(object, 0); }
void TC fs98_options_analysis_third(void *object) { if (on(object, 0x1698)) dependents(object, 1); }
/* 0x22394e10, OnCancel: the block and the vtable read before either store */
void TC fs98_options_analysis_cancel(void *object)
{
    uint32_t *block = BLOCK(object);
    void *vtable = PTR(object, 0);
    D32(object, 0x1b20) = 2;
    block[0] = 0;
    ((void (TC *)(void *))PTR(vtable, 0xec))(object);
}
/* 0x22395390: the choices into the block */
void TC fs98_options_analysis_read(void *object)
{
    int32_t off = on(object, 0x1854);
    D32(BLOCK(object), 0xc) = off == 0;
    D32(BLOCK(object), 0x14) = 0xffffffff;
    if (on(object, 0x14dc)) D32(BLOCK(object), 0x14) = 0;
    else if (on(object, 0x13e8)) D32(BLOCK(object), 0x14) = 1;
    else if (on(object, 0x1698)) D32(BLOCK(object), 0x14) = 2;
    D32(BLOCK(object), 0x24) = (uint32_t)on(object, 0x1698);
    D32(BLOCK(object), 0x2c) = D32(object, 0x166c);
    D32(BLOCK(object), 0x1c) = D32(object, 0x1828);
    D32(BLOCK(object), 0x34) = (uint32_t)SERVICES->send_message(D32(object, 0x1358), CB_GETCURSEL, 0, 0);
    D32(BLOCK(object), 0x3c) = (uint32_t)SERVICES->send_message(D32(object, 0x13a0), CB_GETCURSEL, 0, 0);
}
/* 0x223954e0: the block into the choices; unless the third mode is on, its dependents off */
void TC fs98_options_analysis_populate(void *object)
{
    SERVICES->set_radio(AT(object, 0x1834), D32(BLOCK(object), 0xc) == 0);
    if (D32(BLOCK(object), 0xc) != 0) {
        switch (D32(BLOCK(object), 0x14)) {
        case 0: SERVICES->set_radio(AT(object, 0x14bc), 1); break;
        case 1: SERVICES->set_radio(AT(object, 0x13c8), 1); break;
        case 2: SERVICES->set_radio(AT(object, 0x1678), 1); break;
        default: break;
        }
    }
    SERVICES->set_checked(AT(object, 0x15b0), (int32_t)D32(BLOCK(object), 0x2c));
    SERVICES->set_checked(AT(object, 0x176c), (int32_t)D32(BLOCK(object), 0x1c));
    SERVICES->send_message(D32(object, 0x1358), CB_SETCURSEL, D32(BLOCK(object), 0x34), 0);
    SERVICES->send_message(D32(object, 0x13a0), CB_SETCURSEL, D32(BLOCK(object), 0x3c), 0);
    if (!on(object, 0x1698)) dependents(object, 0);
}
/* one field's failure: reported on its control, and once dismissed the control has the
 * focus (and, with a window to select in, the selection); 0 to go on to the next */
static uint32_t report(void *object, uint32_t control, uint32_t status, uint32_t window)
{
    D32(object, 0x8c) = (uint32_t)SERVICES->error_dialog(object, control ? AT(object, control) : 0, status);
    if (D32(object, 0x8c) == 0) return 0;
    SERVICES->set_focus(AT(object, control ? control : 0x1834));
    if (window) SERVICES->post_message(D32(object, window), EM_SETSEL, 0, 0xffffffff);
    return 1;
}
/* 0x22395620: the first field whose status is set and whose report is dismissed; the
 * mode's is reported against no control and then focuses off */
uint32_t TC fs98_options_analysis_error(void *object)
{
    static const uint32_t fields[][3] = {
        {8, 0x1834, 0x1854}, {0x10, 0, 0}, {0x18, 0x176c, 0x178c}, {0x28, 0x15b0, 0x15d0},
        {0x30, 0x1338, 0x1358}, {0x38, 0x1380, 0x13a0}
    };
    uint32_t index;
    for (index = 0; index < 6; ++index) {
        uint32_t status = D32(BLOCK(object), fields[index][0]);
        if (status != 0 && report(object, fields[index][1], status, fields[index][2])) return D32(object, 0x8c);
    }
    return 0;
}
/* 0x22394e30, OnOK */
void TC fs98_options_analysis_accept(void *object)
{
    D32(object, 0x1b20) = 1;
    fs98_options_analysis_read(object);
    if (fs98_options_check(object) == 0 || fs98_options_analysis_error(object) == 0) {
        uint32_t *block = BLOCK(object);
        void *vtable = PTR(object, 0);
        block[0] = 1;
        ((void (TC *)(void *))PTR(vtable, 0xec))(object);
    }
}
struct title_unwind {
    struct fs98_options_unwind frame;
    struct fs98_options_string *title;
    volatile uint32_t live;
};
static void title_cleanup(struct fs98_options_unwind *frame)
{
    struct title_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; SERVICES->string_destroy(state->title); }
}
/* 0x22394e70, OnInitDialog: the dialog's caption is its title art's text */
FS98_OPTIONS_NOINLINE int32_t TC fs98_options_analysis_init(void *object)
{
    struct title_unwind state;
    struct fs98_options_string title;
    struct fs98_options_rect empty = {0, 0, 0, 0};
    SERVICES->base_init(object);
    if (!((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(
            object, 0x14f, fs98_options_resource_module(), 0xf)) return 0;
    D32(object, 0x84) = 0xb;
    SERVICES->init_art(AT(object, 0x10e0), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    SERVICES->init_button(AT(object, 0x1a24), AT(object, 0x5a0), AT(object, 0x610), 0x132, AT(object, 0xff4),
                          AT(object, 0x7e8), 1, 0xffffffff, fs98_options_resource_module(), 1, 1, empty);
    SERVICES->init_button(AT(object, 0x1928), AT(object, 0x5a0), AT(object, 0x610), 0x132, AT(object, 0xff4),
                          AT(object, 0x7e8), 1, 0xffffffff, fs98_options_resource_module(), 1, 1, empty);
    SERVICES->init_text(AT(object, 0x1280), AT(object, 0x370));
    SERVICES->init_combo(AT(object, 0x1380), AT(object, 0x3e0), AT(object, 0x1280));
    SERVICES->send_message(D32(object, 0x13a0), CB_SETCURSEL, 0, 0);
    SERVICES->init_text(AT(object, 0x11c8), AT(object, 0x370));
    SERVICES->init_combo(AT(object, 0x1338), AT(object, 0x3e0), AT(object, 0x11c8));
    SERVICES->send_message(D32(object, 0x1358), CB_SETCURSEL, 0, 0);
    check_init(object, 0x1834, 0x134);
    check_init(object, 0x14bc, 0x134);
    check_init(object, 0x13c8, 0x134);
    check_init(object, 0x1678, 0x134);
    check_init(object, 0x15b0, 0x139);
    check_init(object, 0x176c, 0x139);
    if (!fs98_options_has_descriptor(object)) return 0;
    fs98_options_analysis_populate(object);
    state.title = &title; state.live = 0;
    fs98_options_unwind_enter(&state.frame, title_cleanup);
    SERVICES->string_construct(&title);
    state.live = 1;
    SERVICES->window_text_string(AT(object, 0x10e0), &title);
    SERVICES->set_window_text(object, title.text);
    title_cleanup(&state.frame);
    fs98_options_unwind_leave(&state.frame);
    return 1;
}
