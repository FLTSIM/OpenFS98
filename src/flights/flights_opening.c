/* The welcome dialog, 0x1109 (class at 0x22319e40, 0x1830 bytes), and the modeless
 * dialog, id 1 (class at 0x22319d10, 0x10e4 bytes), which nothing in FS98 opens.
 *
 * Welcome members: "show at start" 0x10e0 (state +0x119c), the shadow buttons More 0x11a8,
 * Lessons 0x129c, Create 0x1390 and Multiplayer 0x1484, Fly 0x1578 and Quit 0x1674, the
 * hint 0x1770. +0x10dc is set while a button's callback runs; the descriptor is +0x1828. */
#include "flights_internal.h"
#include "flights_unwind.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_FLIGHTS_THISCALL
#define SERVICES fs98_flights_services()
#define DESCRIPTOR(object) PTR(object, 0x1828)
#define BUSY(object) D32(object, 0x10dc)
#define VCALL(object, slot) ((void (TC *)(void *))PTR(PTR(object, 0), slot))(object)

typedef uint32_t (FS98_FLIGHTS_STDCALL *fs98_flights_opening_fn)(void *descriptor);

static struct fs98_flights_opening_state s_own_state;
static struct fs98_flights_opening_state *s_state = &s_own_state;
void fs98_flights_set_opening_state(struct fs98_flights_opening_state *state) { s_state = state; }

/* 0x2231a410, 0x2231a400, 0x2231a3f0, registered with the CRT on first use */
void fs98_flights_opening_shadow_destroy(void) { SERVICES->font_destroy(s_state->shadow); }
void fs98_flights_opening_alternate_destroy(void) { SERVICES->font_destroy(s_state->shadow_alternate); }
void fs98_flights_opening_hint_destroy(void) { SERVICES->font_destroy(s_state->hint); }

/* 0x2231a860: "show at start" into the descriptor */
void TC fs98_flights_opening_save(void *object) { D32(DESCRIPTOR(object), 0x20) = D32(object, 0x119c); }

/* 0x2231a820 */
void TC fs98_flights_opening_refresh(void *object)
{
    if (D32(DESCRIPTOR(object), 0x18) == 0) SERVICES->enable_control(AT(object, 0x1484), 0);
    SERVICES->set_checked(AT(object, 0x10e0), (int32_t)D32(DESCRIPTOR(object), 0x20));
}

/* 0x2231a420, 0x2231a500, 0x2231a660, 0x2231a740: a menu button. The others are disabled
 * while its callback runs; an accepted choice closes the dialog and leaves it busy, and a
 * declined one makes all five usable again, Multiplayer even when it is not available. */
static void fs98_flights_opening_choose(void *object, uint32_t slot, const uint32_t *others)
{
    static const uint32_t all[] = {0x1484, 0x129c, 0x1578, 0x11a8, 0x1390};
    void *descriptor;
    uint32_t index;
    if (PTR(DESCRIPTOR(object), slot) == 0 || BUSY(object) != 0) return;
    BUSY(object) = 1;
    for (index = 0; index < 4; ++index) SERVICES->enable_control(AT(object, others[index]), 0);
    descriptor = DESCRIPTOR(object);
    if (((fs98_flights_opening_fn)PTR(descriptor, slot))(descriptor) != 0) {
        D32(DESCRIPTOR(object), 0) = 1;
        fs98_flights_opening_save(object);
        VCALL(object, 0xec);
        return;
    }
    for (index = 0; index < 5; ++index) SERVICES->enable_control(AT(object, all[index]), 1);
    BUSY(object) = 0;
}
void TC fs98_flights_opening_multiplayer(void *object)
{
    static const uint32_t others[] = {0x129c, 0x1578, 0x11a8, 0x1390};
    fs98_flights_opening_choose(object, 0x14, others);
}
void TC fs98_flights_opening_lessons(void *object)
{
    static const uint32_t others[] = {0x1484, 0x1578, 0x11a8, 0x1390};
    fs98_flights_opening_choose(object, 0xc, others);
}
void TC fs98_flights_opening_more(void *object)
{
    static const uint32_t others[] = {0x1484, 0x129c, 0x1578, 0x1390};
    fs98_flights_opening_choose(object, 8, others);
}
void TC fs98_flights_opening_new_flight(void *object)
{
    static const uint32_t others[] = {0x1484, 0x129c, 0x1578, 0x11a8};
    fs98_flights_opening_choose(object, 0x10, others);
}

/* 0x2231a5e0 (also WM_CLOSE) and 0x2231a620: fly now, or quit */
static void fs98_flights_opening_finish(void *object, uint32_t answer, uint32_t quit)
{
    if (BUSY(object) != 0) return;
    D32(DESCRIPTOR(object), 0) = answer;
    D32(DESCRIPTOR(object), 0x1c) = quit;
    fs98_flights_opening_save(object);
    VCALL(object, 0xec);
}
void TC fs98_flights_opening_fly(void *object) { fs98_flights_opening_finish(object, 1, 0); }
void TC fs98_flights_opening_quit(void *object) { fs98_flights_opening_finish(object, 0, 1); }

/* 0x2231a880, WM_USER+100: the hint for the button with the focus, loaded only when it
 * changes; the focus is asked for afresh before each comparison. */
struct fs98_flights_hint_frame {
    struct fs98_flights_unwind frame;
    struct fs98_flights_string string;
    volatile uint32_t live;
};
static void fs98_flights_hint_cleanup(struct fs98_flights_unwind *frame)
{
    struct fs98_flights_hint_frame *state = (void *)frame;
    if (state->live) { state->live = 0; SERVICES->string_destroy(&state->string); }
}
int32_t TC fs98_flights_opening_hover(void *object, uint32_t w_param, uint32_t l_param)
{
    static const struct { uint32_t button, id, text; } hints[] = {
        {0x11a8, 0x1f8, 0xf6fd}, {0x129c, 0x1f6, 0xf700}, {0x1390, 0x1f9, 0xf6fe}, {0x1484, 0x1f5, 0xf6ff},
    };
    struct fs98_flights_hint_frame hint;
    uint32_t index;
    (void)w_param; (void)l_param;
    hint.live = 0;
    fs98_flights_unwind_enter(&hint.frame, fs98_flights_hint_cleanup);
    SERVICES->string_construct(&hint.string);
    hint.live = 1;
    for (index = 0; index < 4; ++index)
        if (SERVICES->window_from_handle(SERVICES->get_focus()) == AT(object, hints[index].button))
            break;
    if (index < 4) {
        if (s_state->shown != hints[index].id) {
            s_state->shown = hints[index].id;
            SERVICES->load_string(&hint.string, hints[index].text);
            SERVICES->set_sims_text(AT(object, 0x1770), hint.string.text);
        }
    } else if (s_state->shown != 0) {
        s_state->shown = 0;
        SERVICES->set_sims_text(AT(object, 0x1770), fs98_flights_empty_text);
    }
    hint.live = 0;
    SERVICES->string_destroy(&hint.string);
    fs98_flights_unwind_leave(&hint.frame);
    return 0;
}

/* Build a static font the first time; the guard bit is set before it is built */
static void fs98_flights_opening_font(uint8_t *font, uint8_t bit, void (*destroy)(void))
{
    if (s_state->guard & bit) return;
    s_state->guard |= bit;
    SERVICES->font_construct(font, SERVICES->base_font(9));
    SERVICES->register_exit(destroy);
}

/* 0x2231a0c0, OnInitDialog. It answers FALSE, the focus on Fly. */
int32_t TC fs98_flights_opening_init(void *object)
{
    struct fs98_flights_rect empty = {0, 0, 0, 0};
    uint32_t module = fs98_flights_resource_module();
    static const uint32_t shadows[] = {0x11a8, 0x1484, 0x129c, 0x1390};
    uint32_t index;
    void *check;
    SERVICES->base_init(object);
    if (!((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(
            object, 0x13e, module, 1)) return 0;
    fs98_flights_opening_font(s_state->shadow, 1, fs98_flights_opening_shadow_destroy);
    D32(s_state->shadow, 8) = 0xffffff;
    D32(s_state->shadow, 0x18) = 0xff00;
    *(uint16_t *)(s_state->shadow + 0x58) = 0x410;
    D32(s_state->shadow, 0x48) = 1;
    fs98_flights_opening_font(s_state->shadow_alternate, 2, fs98_flights_opening_alternate_destroy);
    D32(s_state->shadow_alternate, 8) = 0xff00;
    *(uint16_t *)(s_state->shadow_alternate + 0x58) = 0x410;
    D32(s_state->shadow_alternate, 0x48) = 1;
    fs98_flights_opening_font(s_state->hint, 4, fs98_flights_opening_hint_destroy);
    D32(s_state->hint, 4) = D32(object, 0x224);
    D32(s_state->hint, 8) = D32(object, 0x228);
    D32(s_state->hint, 0xc) = D32(object, 0x22c);
    D32(s_state->hint, 0x50) = D32(object, 0x270);
    D32(s_state->hint, 0x54) = D32(object, 0x274);
    *(uint16_t *)(s_state->hint + 0x58) = 0x410;
    SERVICES->init_button(AT(object, 0x1578), AT(object, 0x680), AT(object, 0x6f0), 0x137,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff, module, 1, 1, empty);
    SERVICES->init_button(AT(object, 0x1674), AT(object, 0x680), AT(object, 0x6f0), 0x137,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff, module, 1, 1, empty);
    for (index = 0; index < 4; ++index)
        SERVICES->init_shadow_button(AT(object, shadows[index]), s_state->shadow, s_state->shadow_alternate,
                                     0x153, AT(object, 0xff4), AT(object, 0x7e8), 0x42, 0xffffffff,
                                     module, 0, 0, empty);
    check = AT(object, 0x10e0);
    ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))PTR(PTR(check, 0), 0xc4))(
        check, AT(object, 0x4c0), 0x139, AT(object, 0xff4), AT(object, 0x7e8), module);
    SERVICES->init_text(AT(object, 0x1770), s_state->hint);
    fs98_flights_opening_refresh(object);
    BUSY(object) = 0;
    SERVICES->set_focus(AT(object, 0x1578));
    return 0;
}

/* ---- the modeless dialog, id 1 -------------------------------------------------------- */

/* 0x22319d80: whether the skin loaded */
int32_t TC fs98_flights_modeless_init(void *object)
{
    SERVICES->base_init(object);
    return ((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(
               object, 0x140, fs98_flights_resource_module(), 1) != 0;
}

/* 0x22319db0, WM_USER+100: gone once its descriptor's +8 is clear, through the vtable
 * read once: close (+0xec), then destroy the window (+0x60) */
int32_t TC fs98_flights_modeless_check(void *object, uint32_t w_param, uint32_t l_param)
{
    (void)w_param; (void)l_param;
    if (D32(PTR(object, 0x10dc), 8) == 0) {
        void **vtable = PTR(object, 0);
        ((void (TC *)(void *))vtable[0xec / 4])(object);
        ((void (TC *)(void *))vtable[0x60 / 4])(object);
    }
    return 0;
}

/* 0x22319de0, WM_PALETTECHANGED */
void TC fs98_flights_modeless_palette(void *object, void *window)
{
    (void)window;
    SERVICES->redraw_window(D32(object, 0x20), 0, 0, 0x105);
}
