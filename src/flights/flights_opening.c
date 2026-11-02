/* The menus at start: the welcome dialog 0x1109 (class 0x22319e40, 0x1830 bytes) and its
 * two sub-menus, flights 0x11090002 (0x2231a9d0) and learning 0x11090001 (0x2231b240),
 * 0x165c bytes each; and the modeless dialog, id 1 (0x22319d10, 0x10e4 bytes), which
 * nothing in FS98 opens.
 *
 * The three menus are one design: shadow buttons whose callbacks run one at a time
 * (+0x10dc is set while one runs), a hint for the focused button, and three static fonts
 * each. Welcome members: "show at start" 0x10e0 (state +0x119c), shadow buttons More 0x11a8,
 * Lessons 0x129c, Create 0x1390, Multiplayer 0x1484, Fly 0x1578, Quit 0x1674, the hint
 * 0x1770; descriptor +0x1828. Sub-menu members: title art 0x10e0, Back 0x11c8, shadow
 * buttons 0x12c4 0x13b8 0x14ac, the hint 0x15a0; descriptor +0x1658. */
#include "flights_internal.h"
#include "flights_unwind.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_FLIGHTS_THISCALL
#define SERVICES fs98_flights_services()
#define BUSY(object) D32(object, 0x10dc)
#define VCALL(object, slot) ((void (TC *)(void *))PTR(PTR(object, 0), slot))(object)
#define WELCOME_DESCRIPTOR(object) PTR(object, 0x1828)

typedef uint32_t (FS98_FLIGHTS_STDCALL *fs98_flights_menu_fn)(void *descriptor);

static struct fs98_flights_opening_state s_own_state;
static struct fs98_flights_opening_state *s_state = &s_own_state;
static struct fs98_flights_menus_state s_own_menus;
static struct fs98_flights_menus_state *s_menus = &s_own_menus;
void fs98_flights_set_opening_state(struct fs98_flights_opening_state *state) { s_state = state; }
void fs98_flights_set_menus_state(struct fs98_flights_menus_state *state) { s_menus = state; }

/* The fonts' exit destructors, registered with the CRT on first use */
void fs98_flights_opening_shadow_destroy(void) { SERVICES->font_destroy(s_state->shadow); }        /* 0x2231a410 */
void fs98_flights_opening_alternate_destroy(void) { SERVICES->font_destroy(s_state->shadow_alternate); } /* 0x2231a400 */
void fs98_flights_opening_hint_destroy(void) { SERVICES->font_destroy(s_state->hint); }            /* 0x2231a3f0 */
void fs98_flights_flight_menu_shadow_destroy(void) { SERVICES->font_destroy(s_menus->flight_shadow); }       /* 0x2231ae90 */
void fs98_flights_flight_menu_alternate_destroy(void) { SERVICES->font_destroy(s_menus->flight_alternate); } /* 0x2231ae80 */
void fs98_flights_flight_menu_hint_destroy(void) { SERVICES->font_destroy(s_menus->flight_hint); }           /* 0x2231ae70 */
void fs98_flights_learning_menu_shadow_destroy(void) { SERVICES->font_destroy(s_menus->learning_shadow); }       /* 0x2231b700 */
void fs98_flights_learning_menu_alternate_destroy(void) { SERVICES->font_destroy(s_menus->learning_alternate); } /* 0x2231b6f0 */
void fs98_flights_learning_menu_hint_destroy(void) { SERVICES->font_destroy(s_menus->learning_hint); }           /* 0x2231b6e0 */

/* ---- what the three menus share --------------------------------------------------------- */

/* A static font built the first time; the guard bit is set before it is built */
static void fs98_flights_menu_font(uint8_t *guard, uint8_t *font, uint8_t bit, void (*destroy)(void))
{
    if (*guard & bit) return;
    *guard |= bit;
    SERVICES->font_construct(font, SERVICES->base_font(9));
    SERVICES->register_exit(destroy);
}

/* The three fonts, and what every OnInitDialog sets in them: the buttons' colours, and
 * the hint's metrics copied from the dialog's own font */
struct fs98_flights_menu_fonts {
    uint8_t *guard, *shadow, *alternate, *hint;
    void (*shadow_destroy)(void), (*alternate_destroy)(void), (*hint_destroy)(void);
};
static void fs98_flights_menu_fonts(void *object, const struct fs98_flights_menu_fonts *fonts)
{
    fs98_flights_menu_font(fonts->guard, fonts->shadow, 1, fonts->shadow_destroy);
    D32(fonts->shadow, 8) = 0xffffff;
    D32(fonts->shadow, 0x18) = 0xff00;
    *(uint16_t *)(fonts->shadow + 0x58) = 0x410;
    D32(fonts->shadow, 0x48) = 1;
    fs98_flights_menu_font(fonts->guard, fonts->alternate, 2, fonts->alternate_destroy);
    D32(fonts->alternate, 8) = 0xff00;
    *(uint16_t *)(fonts->alternate + 0x58) = 0x410;
    D32(fonts->alternate, 0x48) = 1;
    fs98_flights_menu_font(fonts->guard, fonts->hint, 4, fonts->hint_destroy);
    D32(fonts->hint, 4) = D32(object, 0x224);
    D32(fonts->hint, 8) = D32(object, 0x228);
    D32(fonts->hint, 0xc) = D32(object, 0x22c);
    D32(fonts->hint, 0x50) = D32(object, 0x270);
    D32(fonts->hint, 0x54) = D32(object, 0x274);
    *(uint16_t *)(fonts->hint + 0x58) = 0x410;
}

/* A menu button. It runs only when its callback exists and no other is running; the
 * other buttons are disabled meanwhile. An accepted choice answers 1, saves when the menu
 * has something to save, closes and stays busy; a declined one makes the buttons in
 * `all` usable again, whatever they were before. */
struct fs98_flights_menu_button {
    uint32_t slot;
    uint32_t count;             /* how many of `others` */
    uint32_t others[4];
    uint32_t focus;             /* the learning menu's buttons take the focus back; 0 none */
};
static void fs98_flights_menu_choose(void *object, uint32_t descriptor_at, const struct fs98_flights_menu_button *button,
                                     const uint32_t *all, uint32_t all_count, void (TC *save)(void *))
{
    void *descriptor;
    uint32_t index;
    if (PTR(PTR(object, descriptor_at), button->slot) == 0 || BUSY(object) != 0) return;
    BUSY(object) = 1;
    for (index = 0; index < button->count; ++index)
        SERVICES->enable_control(AT(object, button->others[index]), 0);
    descriptor = PTR(object, descriptor_at);
    if (((fs98_flights_menu_fn)PTR(descriptor, button->slot))(descriptor) != 0) {
        D32(PTR(object, descriptor_at), 0) = 1;
        if (save) save(object);
        VCALL(object, 0xec);
        return;
    }
    for (index = 0; index < all_count; ++index) SERVICES->enable_control(AT(object, all[index]), 1);
    if (button->focus) SERVICES->set_focus(AT(object, button->focus));
    BUSY(object) = 0;
}

/* WM_USER+100: the hint for the shadow button with the focus, loaded only when it
 * changes; the focus is asked for afresh before each comparison. */
struct fs98_flights_menu_hint { uint32_t button, id, text; };
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
static int32_t fs98_flights_menu_hover(void *object, const struct fs98_flights_menu_hint *hints, uint32_t count,
                                       uint32_t *shown, uint32_t text)
{
    struct fs98_flights_hint_frame hint;
    uint32_t index;
    hint.live = 0;
    fs98_flights_unwind_enter(&hint.frame, fs98_flights_hint_cleanup);
    SERVICES->string_construct(&hint.string);
    hint.live = 1;
    for (index = 0; index < count; ++index)
        if (SERVICES->window_from_handle(SERVICES->get_focus()) == AT(object, hints[index].button))
            break;
    if (index < count) {
        if (*shown != hints[index].id) {
            *shown = hints[index].id;
            SERVICES->load_string(&hint.string, hints[index].text);
            SERVICES->set_sims_text(AT(object, text), hint.string.text);
        }
    } else if (*shown != 0) {
        *shown = 0;
        SERVICES->set_sims_text(AT(object, text), fs98_flights_empty_text);
    }
    hint.live = 0;
    SERVICES->string_destroy(&hint.string);
    fs98_flights_unwind_leave(&hint.frame);
    return 0;
}

static void fs98_flights_menu_shadow(void *object, uint32_t at, const uint8_t *shadow, const uint8_t *alternate)
{
    struct fs98_flights_rect empty = {0, 0, 0, 0};
    SERVICES->init_shadow_button(AT(object, at), (void *)shadow, (void *)alternate, 0x153, AT(object, 0xff4),
                                 AT(object, 0x7e8), 0x42, 0xffffffff, fs98_flights_resource_module(), 0, 0, empty);
}
static void fs98_flights_menu_button(void *object, uint32_t at, uint32_t font, uint32_t alternate, uint32_t bitmap)
{
    struct fs98_flights_rect empty = {0, 0, 0, 0};
    SERVICES->init_button(AT(object, at), AT(object, font), AT(object, alternate), bitmap, AT(object, 0xff4),
                          AT(object, 0x7e8), 1, 0xffffffff, fs98_flights_resource_module(), 1, 1, empty);
}
static int32_t fs98_flights_menu_skin(void *object, int32_t skin, uint32_t flags)
{
    SERVICES->base_init(object);
    return ((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(
               object, skin, fs98_flights_resource_module(), flags);
}

/* ---- the welcome dialog ------------------------------------------------------------------ */

static struct fs98_flights_menu_fonts fs98_flights_welcome_fonts(void)
{
    struct fs98_flights_menu_fonts fonts = {
        &s_state->guard, s_state->shadow, s_state->shadow_alternate, s_state->hint,
        fs98_flights_opening_shadow_destroy, fs98_flights_opening_alternate_destroy, fs98_flights_opening_hint_destroy
    };
    return fonts;
}

/* 0x2231a860: "show at start" into the descriptor */
void TC fs98_flights_opening_save(void *object) { D32(WELCOME_DESCRIPTOR(object), 0x20) = D32(object, 0x119c); }

/* 0x2231a820 */
void TC fs98_flights_opening_refresh(void *object)
{
    if (D32(WELCOME_DESCRIPTOR(object), 0x18) == 0) SERVICES->enable_control(AT(object, 0x1484), 0);
    SERVICES->set_checked(AT(object, 0x10e0), (int32_t)D32(WELCOME_DESCRIPTOR(object), 0x20));
}

/* 0x2231a420, 0x2231a500, 0x2231a660, 0x2231a740. A declined choice re-enables all five,
 * Multiplayer even when it is not available. */
static const uint32_t s_welcome_all[] = {0x1484, 0x129c, 0x1578, 0x11a8, 0x1390};
static void fs98_flights_welcome_choose(void *object, const struct fs98_flights_menu_button *button)
{ fs98_flights_menu_choose(object, 0x1828, button, s_welcome_all, 5, fs98_flights_opening_save); }
void TC fs98_flights_opening_multiplayer(void *object)
{
    static const struct fs98_flights_menu_button button = {0x14, 4, {0x129c, 0x1578, 0x11a8, 0x1390}, 0};
    fs98_flights_welcome_choose(object, &button);
}
void TC fs98_flights_opening_lessons(void *object)
{
    static const struct fs98_flights_menu_button button = {0xc, 4, {0x1484, 0x1578, 0x11a8, 0x1390}, 0};
    fs98_flights_welcome_choose(object, &button);
}
void TC fs98_flights_opening_more(void *object)
{
    static const struct fs98_flights_menu_button button = {8, 4, {0x1484, 0x129c, 0x1578, 0x1390}, 0};
    fs98_flights_welcome_choose(object, &button);
}
void TC fs98_flights_opening_new_flight(void *object)
{
    static const struct fs98_flights_menu_button button = {0x10, 4, {0x1484, 0x129c, 0x1578, 0x11a8}, 0};
    fs98_flights_welcome_choose(object, &button);
}

/* 0x2231a5e0 (also WM_CLOSE) and 0x2231a620: fly now, or quit */
static void fs98_flights_opening_finish(void *object, uint32_t answer, uint32_t quit)
{
    if (BUSY(object) != 0) return;
    D32(WELCOME_DESCRIPTOR(object), 0) = answer;
    D32(WELCOME_DESCRIPTOR(object), 0x1c) = quit;
    fs98_flights_opening_save(object);
    VCALL(object, 0xec);
}
void TC fs98_flights_opening_fly(void *object) { fs98_flights_opening_finish(object, 1, 0); }
void TC fs98_flights_opening_quit(void *object) { fs98_flights_opening_finish(object, 0, 1); }

/* 0x2231a880. The last hint shown is shared by every welcome dialog (0x22322320). */
int32_t TC fs98_flights_opening_hover(void *object, uint32_t w_param, uint32_t l_param)
{
    static const struct fs98_flights_menu_hint hints[] = {
        {0x11a8, 0x1f8, 0xf6fd}, {0x129c, 0x1f6, 0xf700}, {0x1390, 0x1f9, 0xf6fe}, {0x1484, 0x1f5, 0xf6ff},
    };
    (void)w_param; (void)l_param;
    return fs98_flights_menu_hover(object, hints, 4, &s_state->shown, 0x1770);
}

/* 0x2231a0c0, OnInitDialog. It answers FALSE, the focus on Fly. */
int32_t TC fs98_flights_opening_init(void *object)
{
    static const uint32_t shadows[] = {0x11a8, 0x1484, 0x129c, 0x1390};
    struct fs98_flights_menu_fonts fonts = fs98_flights_welcome_fonts();
    uint32_t index;
    void *check;
    if (!fs98_flights_menu_skin(object, 0x13e, 1)) return 0;
    fs98_flights_menu_fonts(object, &fonts);
    fs98_flights_menu_button(object, 0x1578, 0x680, 0x6f0, 0x137);
    fs98_flights_menu_button(object, 0x1674, 0x680, 0x6f0, 0x137);
    for (index = 0; index < 4; ++index)
        fs98_flights_menu_shadow(object, shadows[index], s_state->shadow, s_state->shadow_alternate);
    check = AT(object, 0x10e0);
    ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))PTR(PTR(check, 0), 0xc4))(
        check, AT(object, 0x4c0), 0x139, AT(object, 0xff4), AT(object, 0x7e8), fs98_flights_resource_module());
    SERVICES->init_text(AT(object, 0x1770), s_state->hint);
    fs98_flights_opening_refresh(object);
    BUSY(object) = 0;
    SERVICES->set_focus(AT(object, 0x1578));
    return 0;
}

/* ---- the two sub-menus ------------------------------------------------------------------- */

/* 0x2231b920: OK, Cancel and WM_CLOSE alike, unless a callback is running */
void TC fs98_flights_menu_back(void *object)
{
    if (BUSY(object) != 0) return;
    D32(PTR(object, 0x1658), 0) = 0;
    VCALL(object, 0xec);
}

/* Flights. Its buttons disable Back too, and a declined choice re-enables all four. */
static const uint32_t s_flight_all[] = {0x12c4, 0x14ac, 0x13b8, 0x11c8};
void TC fs98_flights_flight_menu_dual(void *object)        /* 0x2231aea0 */
{
    static const struct fs98_flights_menu_button button = {8, 3, {0x14ac, 0x13b8, 0x11c8}, 0};
    fs98_flights_menu_choose(object, 0x1658, &button, s_flight_all, 4, 0);
}
void TC fs98_flights_flight_menu_video(void *object)       /* 0x2231af60 */
{
    static const struct fs98_flights_menu_button button = {0x14, 3, {0x12c4, 0x13b8, 0x11c8}, 0};
    fs98_flights_menu_choose(object, 0x1658, &button, s_flight_all, 4, 0);
}
void TC fs98_flights_flight_menu_review(void *object)      /* 0x2231b020 */
{
    static const struct fs98_flights_menu_button button = {0x10, 3, {0x12c4, 0x14ac, 0x11c8}, 0};
    fs98_flights_menu_choose(object, 0x1658, &button, s_flight_all, 4, 0);
}
int32_t TC fs98_flights_flight_menu_hover(void *object, uint32_t w_param, uint32_t l_param)   /* 0x2231b120 */
{
    static const struct fs98_flights_menu_hint hints[] = {
        {0x13b8, 0xc82, 0xf702}, {0x14ac, 0xc81, 0xf706}, {0x12c4, 0xcb9, 0xf705},
    };
    (void)w_param; (void)l_param;
    return fs98_flights_menu_hover(object, hints, 3, &s_menus->flight_shown, 0x15a0);
}

/* Learning. Back stays usable, and a declined choice puts the focus back on its button. */
static const uint32_t s_learning_all[] = {0x12c4, 0x14ac, 0x13b8};
void TC fs98_flights_learning_menu_lessons(void *object)   /* 0x2231b710 */
{
    static const struct fs98_flights_menu_button button = {8, 2, {0x14ac, 0x13b8}, 0x12c4};
    fs98_flights_menu_choose(object, 0x1658, &button, s_learning_all, 3, 0);
}
void TC fs98_flights_learning_menu_about(void *object)     /* 0x2231b7c0 */
{
    static const struct fs98_flights_menu_button button = {0x10, 2, {0x12c4, 0x13b8}, 0x14ac};
    fs98_flights_menu_choose(object, 0x1658, &button, s_learning_all, 3, 0);
}
void TC fs98_flights_learning_menu_quit(void *object)      /* 0x2231b870 */
{
    static const struct fs98_flights_menu_button button = {0xc, 2, {0x12c4, 0x14ac}, 0x13b8};
    fs98_flights_menu_choose(object, 0x1658, &button, s_learning_all, 3, 0);
}
int32_t TC fs98_flights_learning_menu_hover(void *object, uint32_t w_param, uint32_t l_param)   /* 0x2231b980 */
{
    static const struct fs98_flights_menu_hint hints[] = {
        {0x12c4, 0xc83, 0xf701}, {0x13b8, 0xc84, 0xf704}, {0x14ac, 0xc85, 0xf703},
    };
    (void)w_param; (void)l_param;
    return fs98_flights_menu_hover(object, hints, 3, &s_menus->learning_shown, 0x15a0);
}

/* 0x2231abf0, 0x2231b460: OnInitDialog. They answer TRUE and set no focus. */
static int32_t fs98_flights_submenu_init(void *object, int32_t skin, const struct fs98_flights_menu_fonts *fonts)
{
    static const uint32_t shadows[] = {0x12c4, 0x13b8, 0x14ac};
    uint32_t index;
    if (!fs98_flights_menu_skin(object, skin, 7)) return 0;
    fs98_flights_menu_fonts(object, fonts);
    SERVICES->init_art(AT(object, 0x10e0), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    fs98_flights_menu_button(object, 0x11c8, 0x5a0, 0x610, 0x132);
    for (index = 0; index < 3; ++index)
        fs98_flights_menu_shadow(object, shadows[index], fonts->shadow, fonts->alternate);
    SERVICES->init_text(AT(object, 0x15a0), fonts->hint);
    BUSY(object) = 0;
    return 1;
}
int32_t TC fs98_flights_flight_menu_init(void *object)
{
    struct fs98_flights_menu_fonts fonts = {
        &s_menus->flight_guard, s_menus->flight_shadow, s_menus->flight_alternate, s_menus->flight_hint,
        fs98_flights_flight_menu_shadow_destroy, fs98_flights_flight_menu_alternate_destroy,
        fs98_flights_flight_menu_hint_destroy
    };
    return fs98_flights_submenu_init(object, 0x155, &fonts);
}
int32_t TC fs98_flights_learning_menu_init(void *object)
{
    struct fs98_flights_menu_fonts fonts = {
        &s_menus->learning_guard, s_menus->learning_shadow, s_menus->learning_alternate, s_menus->learning_hint,
        fs98_flights_learning_menu_shadow_destroy, fs98_flights_learning_menu_alternate_destroy,
        fs98_flights_learning_menu_hint_destroy
    };
    return fs98_flights_submenu_init(object, 0x156, &fonts);
}

/* ---- the modeless dialog, id 1 -------------------------------------------------------- */

/* 0x22319d80: whether the skin loaded */
int32_t TC fs98_flights_modeless_init(void *object) { return fs98_flights_menu_skin(object, 0x140, 1) != 0; }

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
