/* Preferences (0x1409, template 0x39b, class CPreferences at 0x2239b480, 0x2358 bytes), MAIN's
 * General Preferences (main_prefdlg.inc), and its six tab pages, each a CSimsTabPage: General
 * (0x398), Instrument (0x399), International (0x39a), Display (0x396), Sound (0x395) and
 * Flight Options (0x397). The block is described in options_internal.h.
 *
 * The dialog's members: Defaults 0x10e0, OK 0x11dc, Cancel 0x12d8, the tab control 0x13d4,
 * title art 0x209c and six CSimsTabCtrlHelpers 0x2184..0x22ec. +0x10dc holds the block,
 * +0x2334 the CNotify, +0x2338..+0x234c the six pages (made here, not members), +0x2350 OK
 * once it exists, +0x2354 the answer state.
 *
 * A page's own words, as in Display options: +0x10dc and +0x10fc the palette and bitmap,
 * +0x1100 set once shown, +0x1108 its tab and +0x110c the tab control (CSimsTabPage's),
 * and from +0x1118 what the dialog gives it (options_prefs_attach). A check box's state is at
 * +0xbc of it. */
#include <math.h>
#include "options_internal.h"
#include "options_objects.h"
#include "options_unwind.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define F64(object, offset) (*(double *)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_OPTIONS_THISCALL
#define SC FS98_OPTIONS_STDCALL
#define SERVICES fs98_options_services()
#define BLOCK(object) PTR(object, 0x10dc)
#define PART(page) ((uint8_t *)PTR(page, 0x1118))
#define CB_ADDSTRING_ 0x143
#define CB_GETCURSEL_ 0x147
#define CB_RESETCONTENT_ 0x14b
#define CB_SETCURSEL_ 0x14e
#define EM_SETSEL_ 0xb1
#define VK_RETURN_ 0xd
#define TAB 0x13d4

typedef uint32_t (SC *fs98_options_block_fn)(void *block);

static void check_init(void *page, uint32_t at)
{
    void *check = AT(page, at);
    uint32_t module = fs98_options_resource_module();
    ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))PTR(PTR(check, 0), 0xc4))(
        check, AT(page, 0x4c0), 0x139, PTR(page, 0x10fc), PTR(page, 0x10dc), module);
}
/* a page's own button, from the page's bitmap and palette */
static void page_button(void *page, uint32_t at)
{
    struct fs98_options_rect empty = {0, 0, 0, 0};
    SERVICES->init_button(AT(page, at), AT(page, 0x680), AT(page, 0x6f0), 0x137, PTR(page, 0x10fc),
                          PTR(page, 0x10dc), 1, 0xffffffff, fs98_options_resource_module(), 1, 1, empty);
}
static void combo(void *page, uint32_t at, uint32_t label)
{
    SERVICES->init_combo(AT(page, at), AT(page, 0x3e0), AT(page, label));
    SERVICES->send_message(D32(page, at + 0x20), CB_SETCURSEL_, 0, 0);
}
static uint32_t pick(void *page, uint32_t window)
{
    return (uint32_t)SERVICES->send_message(D32(page, window), CB_GETCURSEL_, 0, 0);
}

/* ---- General (0x398, class at 0x223957f0, 0x1c4c bytes) ---------------------------------
 * Labels 0x11f0, 0x12a8 and 0x1360, a group 0x1418, three combos 0x14cc, 0x1514 and 0x155c
 * (windows +0x20), seven check boxes 0x1128, 0x15a4, 0x1864, 0x192c, 0x19f4, 0x1abc and
 * 0x1b84, and two buttons 0x166c and 0x1768. Its part is the block from +0x10; +0x111c the
 * block itself, +0x1120 Cancel, +0x1124 OK. The part's +4 and +8 are MAIN's callbacks for the
 * two buttons, given the block. */

/* 0x22396390 (and CBN_SELCHANGE on 0x118f): one button or the other by the second combo */
void TC fs98_options_prefs_general_choose(void *page)
{
    int32_t first = SERVICES->send_message(D32(page, 0x1534), CB_GETCURSEL_, 0, 0) == 0;
    SERVICES->enable_control(AT(page, 0x166c), first);
    SERVICES->enable_control(AT(page, 0x1768), !first);
}
/* 0x22395bc0 (BN_CLICKED on 0x1173) and 0x22395be0 (on 0x1174): MAIN's dialog, then the
 * focus back to the button */
void TC fs98_options_prefs_general_first(void *page)
{
    ((fs98_options_block_fn)PTR(PART(page), 4))(PTR(page, 0x111c));
    SERVICES->set_focus(AT(page, 0x1768));
}
void TC fs98_options_prefs_general_second(void *page)
{
    ((fs98_options_block_fn)PTR(PART(page), 8))(PTR(page, 0x111c));
    SERVICES->set_focus(AT(page, 0x166c));
}
/* 0x22395ee0: two boxes are kept the other way round */
void TC fs98_options_prefs_general_read(void *page)
{
    uint32_t value;
    if (D32(page, 0x1100) == 0) return;
    D32(PART(page), 0x28) = D32(page, 0x1660);
    D32(PART(page), 0x40) = D32(page, 0x1920) == 0;
    D32(PART(page), 0x38) = D32(page, 0x19e8) == 0;
    D32(PART(page), 0x30) = D32(page, 0x1ab0);
    D32(PART(page), 0x48) = D32(page, 0x11e4);
    D32(PART(page), 0x58) = D32(page, 0x1b78);
    D32(PART(page), 0x50) = D32(page, 0x1c40);
    value = pick(page, 0x14ec);
    D32(PART(page), 0x18) = value;
    value = pick(page, 0x1534);
    D32(PART(page), 0x10) = value;
    value = pick(page, 0x157c);
    D32(PART(page), 0x20) = value;
}
/* 0x22395fd0 */
void TC fs98_options_prefs_general_populate(void *page)
{
    SERVICES->set_checked(AT(page, 0x15a4), (int32_t)D32(PART(page), 0x28));
    SERVICES->set_checked(AT(page, 0x1864), D32(PART(page), 0x40) == 0);
    SERVICES->set_checked(AT(page, 0x192c), D32(PART(page), 0x38) == 0);
    SERVICES->set_checked(AT(page, 0x19f4), (int32_t)D32(PART(page), 0x30));
    SERVICES->set_checked(AT(page, 0x1128), (int32_t)D32(PART(page), 0x48));
    SERVICES->set_checked(AT(page, 0x1abc), (int32_t)D32(PART(page), 0x58));
    SERVICES->set_checked(AT(page, 0x1b84), (int32_t)D32(PART(page), 0x50));
    SERVICES->send_message(D32(page, 0x14ec), CB_SETCURSEL_, D32(PART(page), 0x18), 0);
    SERVICES->send_message(D32(page, 0x1534), CB_SETCURSEL_, D32(PART(page), 0x10), 0);
    SERVICES->send_message(D32(page, 0x157c), CB_SETCURSEL_, D32(PART(page), 0x20), 0);
    fs98_options_prefs_general_choose(page);
}
/* 0x223960d0: the first failed field (its status in the part) reported on its box; once
 * dismissed, its tab shown, the focus and the selection to it */
uint32_t TC fs98_options_prefs_general_error(void *page)
{
    static const uint32_t fields[][3] = {
        {0x24, 0x15a4, 0x15c4}, {0x2c, 0x19f4, 0x1a14}, {0x34, 0x192c, 0x194c}, {0x3c, 0x1864, 0x1884},
        {0x44, 0x1128, 0x1148}, {0x54, 0x1abc, 0x1adc}, {0x4c, 0x1b84, 0x1ba4}
    };
    uint32_t index;
    if (D32(page, 0x1100) == 0) return 0;
    for (index = 0; index < 7; ++index) {
        uint32_t status = D32(PART(page), fields[index][0]);
        if (status == 0) continue;
        D32(page, 0x8c) = (uint32_t)SERVICES->error_dialog(page, AT(page, fields[index][1]), status);
        if (D32(page, 0x8c) == 0) continue;
        SERVICES->set_active_tab(PTR(page, 0x110c), (int32_t)D32(page, 0x1108), 0);
        SERVICES->set_focus(AT(page, fields[index][1]));
        SERVICES->post_message(D32(page, fields[index][2]), EM_SETSEL_, 0, 0xffffffff);
        return D32(page, 0x8c);
    }
    return 0;
}
/* 0x22395c00, vtable +0xf0. RETAIL BUG (kept): OPT-10 (docs/retail-bugs.md): the third
 * combo is given the first combo's label. */
void TC fs98_options_prefs_general_init(void *page)
{
    static const uint32_t checks[] = {0x15a4, 0x1864, 0x192c, 0x19f4, 0x1128, 0x1abc, 0x1b84};
    uint32_t index;
    page_button(page, 0x1768);
    page_button(page, 0x166c);
    SERVICES->init_text(AT(page, 0x11f0), AT(page, 0x370));
    combo(page, 0x14cc, 0x11f0);
    SERVICES->init_text(AT(page, 0x12a8), AT(page, 0x370));
    combo(page, 0x1514, 0x12a8);
    SERVICES->init_text(AT(page, 0x1360), AT(page, 0x370));
    combo(page, 0x155c, 0x11f0);
    for (index = 0; index < 7; ++index) check_init(page, checks[index]);
    SERVICES->init_group(AT(page, 0x1418), AT(page, 0x760));
    fs98_options_prefs_general_populate(page);
}

/* ---- Instrument (0x399, class at 0x22397500, 0x150c bytes) ------------------------------
 * Five check boxes 0x1124, 0x11ec, 0x12b4, 0x137c and 0x1444; the first two kept the other
 * way round. Its part is the block from +0x6c. */

/* 0x22397810 */
void TC fs98_options_prefs_instrument_read(void *page)
{
    if (D32(page, 0x1100) == 0) return;
    D32(PART(page), 0x28) = D32(page, 0x11e0) == 0;
    D32(PART(page), 0x20) = D32(page, 0x12a8) == 0;
    D32(PART(page), 8) = D32(page, 0x1370);
    D32(PART(page), 0x10) = D32(page, 0x1438);
    D32(PART(page), 0x18) = D32(page, 0x1500);
}
/* 0x22397880 */
void TC fs98_options_prefs_instrument_populate(void *page)
{
    SERVICES->set_checked(AT(page, 0x1124), D32(PART(page), 0x28) == 0);
    SERVICES->set_checked(AT(page, 0x11ec), D32(PART(page), 0x20) == 0);
    SERVICES->set_checked(AT(page, 0x12b4), (int32_t)D32(PART(page), 8));
    SERVICES->set_checked(AT(page, 0x137c), (int32_t)D32(PART(page), 0x10));
    SERVICES->set_checked(AT(page, 0x1444), (int32_t)D32(PART(page), 0x18));
}
/* 0x223976d0, vtable +0xf0 */
void TC fs98_options_prefs_instrument_init(void *page)
{
    check_init(page, 0x1124);
    check_init(page, 0x11ec);
    check_init(page, 0x1444);
    check_init(page, 0x137c);
    check_init(page, 0x12b4);
    fs98_options_prefs_instrument_populate(page);
}

/* ---- International (0x39a, class at 0x223963f0, 0x1714 bytes) ---------------------------
 * Labels 0x1124, 0x11dc, 0x1294, 0x134c, 0x1404 and 0x14bc, three combos 0x1574, 0x15bc and
 * 0x1604 (windows +0x20) and a check box 0x164c, hidden unless the part's +0x1c is set. The
 * second and third combos stand for codes: 0 and 1 for 1 and 0, and for 2 and 3. Its part
 * is the block from +0x98. */

/* 0x22396820 */
void TC fs98_options_prefs_international_read(void *page)
{
    uint32_t value;
    if (D32(page, 0x1100) == 0) return;
    D32(PART(page), 0x24) = D32(page, 0x1708);
    value = pick(page, 0x1594);
    D32(PART(page), 8) = value;
    switch (pick(page, 0x15dc)) {
    case 0: D32(PART(page), 0x18) = 1; break;
    case 1: D32(PART(page), 0x18) = 0; break;
    default: break;
    }
    switch (pick(page, 0x1624)) {
    case 0: D32(PART(page), 0x10) = 2; break;
    case 1: D32(PART(page), 0x10) = 3; break;
    default: break;
    }
}
/* 0x223968e0 */
void TC fs98_options_prefs_international_populate(void *page)
{
    SERVICES->set_checked(AT(page, 0x164c), (int32_t)D32(PART(page), 0x24));
    SERVICES->send_message(D32(page, 0x1594), CB_SETCURSEL_, D32(PART(page), 8), 0);
    switch (D32(PART(page), 0x18)) {
    case 0: SERVICES->send_message(D32(page, 0x15dc), CB_SETCURSEL_, 1, 0); break;
    case 1: SERVICES->send_message(D32(page, 0x15dc), CB_SETCURSEL_, 0, 0); break;
    default: break;
    }
    switch (D32(PART(page), 0x10)) {
    case 2: SERVICES->send_message(D32(page, 0x1624), CB_SETCURSEL_, 0, 0); break;
    case 3: SERVICES->send_message(D32(page, 0x1624), CB_SETCURSEL_, 1, 0); break;
    default: break;
    }
}
/* 0x223966b0, vtable +0xf0 */
void TC fs98_options_prefs_international_init(void *page)
{
    SERVICES->init_text(AT(page, 0x134c), AT(page, 0x220));
    SERVICES->init_text(AT(page, 0x14bc), AT(page, 0x220));
    SERVICES->init_text(AT(page, 0x1404), AT(page, 0x220));
    SERVICES->init_text(AT(page, 0x1294), AT(page, 0x370));
    combo(page, 0x1604, 0x1294);
    SERVICES->init_text(AT(page, 0x11dc), AT(page, 0x370));
    combo(page, 0x15bc, 0x11dc);
    SERVICES->init_text(AT(page, 0x1124), AT(page, 0x370));
    combo(page, 0x1574, 0x1124);
    check_init(page, 0x164c);
    if (D32(PART(page), 0x1c) == 0) SERVICES->show_window(AT(page, 0x164c), 0);
    fs98_options_prefs_international_populate(page);
}

/* ---- Display (0x396, class at 0x22392430, 0x16ec bytes) ---------------------------------
 * A check box 0x1128, labels 0x11f0, 0x12a8, 0x1360 and 0x1418, combos 0x14d0, 0x1518, 0x1560
 * and 0x15a8 (windows +0x20; 0x1560 unused) and a button 0x15f0. Its part is the block from
 * +0xc0, MAIN's presets: +4 its Display button's callback and +0x14 a preset changed (each
 * given the block, +0x111c), +8 the presets (0x80 bytes each), +0xc their count, +0x10 the
 * one chosen, +0x1c and +0x24 the other two combos, +0x34 the box. */

/* 0x22392890 */
void TC fs98_options_prefs_display_read(void *page)
{
    uint32_t value;
    if (D32(page, 0x1100) == 0) return;
    value = pick(page, 0x1538);
    D32(PART(page), 0x10) = value;
    D32(PART(page), 0x34) = D32(page, 0x11e4);
    value = pick(page, 0x15c8);
    D32(PART(page), 0x1c) = value;
    value = pick(page, 0x14f0);
    D32(PART(page), 0x24) = value;
}
/* 0x22392910: the presets into their combo, the count read again after each */
void TC fs98_options_prefs_display_populate(void *page)
{
    int32_t index = 0;
    SERVICES->send_message(D32(page, 0x1538), CB_RESETCONTENT_, 0, 0);
    if ((int32_t)D32(PART(page), 0xc) > 0) {
        do {
            SERVICES->send_message(D32(page, 0x1538), CB_ADDSTRING_, 0, D32(PART(page), 8) + (uint32_t)index * 0x80);
            ++index;
        } while (index < (int32_t)D32(PART(page), 0xc));
    }
    SERVICES->send_message(D32(page, 0x1538), CB_SETCURSEL_, D32(PART(page), 0x10), 0);
    SERVICES->set_checked(AT(page, 0x1128), (int32_t)D32(PART(page), 0x34));
    SERVICES->send_message(D32(page, 0x15c8), CB_SETCURSEL_, D32(PART(page), 0x1c), 0);
    SERVICES->send_message(D32(page, 0x14f0), CB_SETCURSEL_, D32(PART(page), 0x24), 0);
}
static void changed(void *page)
{
    ((fs98_options_block_fn)PTR(PART(page), 0x14))(PTR(page, 0x111c));
    fs98_options_prefs_display_populate(page);
}
/* 0x223926d0 (BN_CLICKED on 0x1141): MAIN's Display options, then the presets again */
void TC fs98_options_prefs_display_button(void *page)
{
    fs98_options_prefs_display_read(page);
    ((fs98_options_block_fn)PTR(PART(page), 4))(PTR(page, 0x111c));
    SERVICES->set_focus(AT(page, 0x15f0));
    fs98_options_prefs_display_populate(page);
}
/* 0x223929d0 (CBN_SELCHANGE on 0x118d): another preset */
void TC fs98_options_prefs_display_preset(void *page)
{
    uint32_t value;
    fs98_options_prefs_display_read(page);
    value = pick(page, 0x1538);
    D32(PART(page), 0x10) = value;
    changed(page);
}
/* 0x22392a10 (CBN_SELCHANGE on 0x1187) and 0x22392a60 (BN_CLICKED on 0x115d): a setting
 * changed, so no preset */
void TC fs98_options_prefs_display_other(void *page)
{
    uint32_t value;
    fs98_options_prefs_display_read(page);
    value = pick(page, 0x15c8);
    D32(PART(page), 0x1c) = value;
    D32(PART(page), 0x10) = 0;
    changed(page);
}
void TC fs98_options_prefs_display_box(void *page)
{
    fs98_options_prefs_display_read(page);
    D32(PART(page), 0x34) = D32(page, 0x11e4);
    D32(PART(page), 0x10) = 0;
    changed(page);
}
/* 0x22392700, vtable +0xf0 */
void TC fs98_options_prefs_display_init(void *page)
{
    D32(page, 0x84) = 0xd;
    page_button(page, 0x15f0);
    SERVICES->init_text(AT(page, 0x1418), AT(page, 0x370));
    combo(page, 0x15a8, 0x1418);
    SERVICES->init_text(AT(page, 0x12a8), AT(page, 0x370));
    combo(page, 0x1518, 0x12a8);
    SERVICES->init_text(AT(page, 0x11f0), AT(page, 0x370));
    combo(page, 0x14d0, 0x11f0);
    check_init(page, 0x1128);
    fs98_options_prefs_display_populate(page);
}

/* ---- Sound (0x395, class at 0x223919d0, 0x2dbc bytes) -----------------------------------
 * Labels 0x112c, 0x11e4 and 0x129c, and five check boxes each with a slider: 0x1354/0x141c,
 * 0x189c/0x1964, 0x1de4/0x1eac, 0x232c/0x23f4, 0x2874/0x293c. Its part is +0x111c, the block
 * from +0xf8: five volumes as doubles, positive while on, at +4, +0x14, +0x24, +0xc, +0x1c
 * (in the boxes' order). +0x1120 the block, +0x1124 OK, +0x1128 Cancel. */
#define SOUND(page) ((uint8_t *)PTR(page, 0x111c))
static const uint32_t s_sound[][2] = {      /* a box, its volume, in the read's order */
    {0x1354, 4}, {0x189c, 0x14}, {0x1de4, 0x24}, {0x232c, 0xc}, {0x2874, 0x1c}
};
/* .rdata 0x223a6138 and 0x223a6140, as doubles: a C99 x87 build would otherwise evaluate a
 * literal 0.01 in long double */
static const double s_hundredth = 0.01, s_hundred = 100.0;
/* 0x22392050: each slider's position in hundredths, negative while its box is off */
void TC fs98_options_prefs_sound_read(void *page)
{
    uint32_t index;
    if (D32(page, 0x1100) == 0) return;
    for (index = 0; index < 5; ++index) {
        uint32_t box = s_sound[index][0];
        int32_t sign = D32(page, box + 0xbc) != 0 ? 1 : -1;
        int32_t position = SERVICES->get_scroll_pos(AT(page, box + 0xc8));
        long double volume = (long double)position * (long double)s_hundredth;
        volume = volume * (long double)sign;
        F64(SOUND(page), s_sound[index][1]) = (double)volume;
    }
}
/* 0x223a2f70, MSVCRT _ftol: toward zero, as 64 bits (fistp qword), of which the low 32 are
 * used: out of range wraps rather than saturating */
static int32_t ftol(long double value) { return (int32_t)(uint32_t)(uint64_t)(int64_t)value; }
/* 0x22392190: the sliders first, then each box (on while its volume is above 0) and its
 * slider usable with it */
void TC fs98_options_prefs_sound_populate(void *page)
{
    static const uint32_t order[] = {0, 1, 2, 3, 4};
    uint32_t index;
    for (index = 0; index < 5; ++index) {
        double volume = F64(SOUND(page), s_sound[order[index]][1]);
        SERVICES->set_scroll_pos(AT(page, s_sound[index][0] + 0xc8),
                                 ftol(fabsl((long double)volume) * (long double)s_hundred));
    }
    for (index = 0; index < 5; ++index) {
        uint32_t box = s_sound[index][0];
        SERVICES->set_checked(AT(page, box), F64(SOUND(page), s_sound[index][1]) > 0.0);
        SERVICES->enable_control(AT(page, box + 0xc8), F64(SOUND(page), s_sound[index][1]) > 0.0);
    }
}
/* 0x22391fb0, 0x22391fd0, 0x22391ff0, 0x22392010, 0x22392030 (BN_CLICKED on 0x1184..0x1188):
 * a box's slider usable while it is on */
void TC fs98_options_prefs_sound_box(void *page, uint32_t index)
{
    uint32_t box = s_sound[index][0];
    SERVICES->enable_control(AT(page, box + 0xc8), (int32_t)D32(page, box + 0xbc));
}
#define SOUND_BOX(n) void TC fs98_options_prefs_sound_box##n(void *page) { fs98_options_prefs_sound_box(page, n); }
SOUND_BOX(0) SOUND_BOX(1) SOUND_BOX(2) SOUND_BOX(3) SOUND_BOX(4)
#undef SOUND_BOX
/* 0x22391d10, vtable +0xf0: each slider from 1 to 100 */
void TC fs98_options_prefs_sound_init(void *page)
{
    uint32_t index;
    SERVICES->init_text(AT(page, 0x112c), AT(page, 0x220));
    SERVICES->init_text(AT(page, 0x11e4), AT(page, 0x220));
    SERVICES->init_text(AT(page, 0x129c), AT(page, 0x220));
    for (index = 0; index < 5; ++index) {
        uint32_t box = s_sound[index][0];
        check_init(page, box);
        SERVICES->slider_init(AT(page, box + 0xc8), 0x135, 0x136, PTR(page, 0x10fc), PTR(page, 0x10dc),
                              6, 0x1a, 6, 8, 0, -1, fs98_options_resource_module());
        SERVICES->set_scroll_range(AT(page, box + 0xc8), 1, 0x64);
    }
    fs98_options_prefs_sound_populate(page);
}

/* ---- Flight Options (0x397, class at 0x2239b0e0, 0x14f8 bytes) --------------------------
 * A group 0x1124 and four check boxes 0x11d8, 0x12a0, 0x1368 and 0x1430. Its part is the
 * block from +0x124. */

/* 0x2239b3d0 */
void TC fs98_options_prefs_flight_read(void *page)
{
    if (D32(page, 0x1100) == 0) return;
    D32(PART(page), 0x20) = D32(page, 0x1294);
    D32(PART(page), 0x18) = D32(page, 0x135c);
    D32(PART(page), 0x10) = D32(page, 0x1424);
    D32(PART(page), 8) = D32(page, 0x14ec);
}
/* 0x2239b420 */
void TC fs98_options_prefs_flight_populate(void *page)
{
    SERVICES->set_checked(AT(page, 0x11d8), (int32_t)D32(PART(page), 0x20));
    SERVICES->set_checked(AT(page, 0x12a0), (int32_t)D32(PART(page), 0x18));
    SERVICES->set_checked(AT(page, 0x1368), (int32_t)D32(PART(page), 0x10));
    SERVICES->set_checked(AT(page, 0x1430), (int32_t)D32(PART(page), 8));
}
/* 0x2239b2b0, vtable +0xf0 */
void TC fs98_options_prefs_flight_init(void *page)
{
    SERVICES->init_group(AT(page, 0x1124), AT(page, 0x220));
    check_init(page, 0x1430);
    check_init(page, 0x1368);
    check_init(page, 0x12a0);
    check_init(page, 0x11d8);
    fs98_options_prefs_flight_populate(page);
}

/* ---- the dialog ------------------------------------------------------------------------- */

/* 0x2239b780, OnCancel */
void TC fs98_options_prefs_cancel(void *object)
{
    uint32_t *block = BLOCK(object);
    void *vtable = PTR(object, 0);
    D32(object, 0x2354) = 2;
    block[0] = 0;
    ((void (TC *)(void *))PTR(vtable, 0xec))(object);
}
/* 0x2239bef0 */
void TC fs98_options_prefs_read(void *object)
{
    fs98_options_prefs_general_read(PTR(object, 0x2338));
    fs98_options_prefs_instrument_read(PTR(object, 0x233c));
    fs98_options_prefs_international_read(PTR(object, 0x2340));
    fs98_options_prefs_display_read(PTR(object, 0x2344));
    fs98_options_prefs_sound_read(PTR(object, 0x2348));
    fs98_options_prefs_flight_read(PTR(object, 0x234c));
}
/* 0x2239c0b0: General's check, then the others' empty ones */
uint32_t TC fs98_options_prefs_error(void *object)
{
    if (fs98_options_prefs_general_error(PTR(object, 0x2338))) return 1;
    if (fs98_options_no_error(PTR(object, 0x233c))) return 1;
    if (fs98_options_no_error(PTR(object, 0x2340))) return 1;
    if (fs98_options_no_error(PTR(object, 0x2344))) return 1;
    if (fs98_options_no_error(PTR(object, 0x2348))) return 1;
    return fs98_options_no_error(PTR(object, 0x234c)) != 0;
}
/* 0x2239b7a0, OnOK */
void TC fs98_options_prefs_accept(void *object)
{
    if (SERVICES->async_key_state(VK_RETURN_) & 1) SERVICES->set_focus(PTR(object, 0x2350));
    D32(object, 0x2354) = 1;
    fs98_options_prefs_read(object);
    if (fs98_options_check(object) == 0 || fs98_options_prefs_error(object) == 0) {
        D32(BLOCK(object), 0) = 1;
        ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
    }
}
/* 0x2239b800, vtable +0xec */
void TC fs98_options_prefs_close(void *object)
{
    void *notify;
    ((void (TC *)(void *))PTR(PTR(object, TAB), 0xc4))(AT(object, TAB));
    notify = PTR(object, 0x2334);
    if (notify) ((void *(TC *)(void *, uint32_t))PTR(PTR(notify, 0), 4))(notify, 1);
    D32(object, 0x2334) = 0;
    SERVICES->close(object);
}
/* 0x2239b880 (BN_CLICKED on 0x1131, Defaults): the tab shown into the block, MAIN's defaults
 * (the block's +0xc), then every page shown so far filled again */
void TC fs98_options_prefs_defaults(void *object)
{
    static const uint32_t pages[] = {0x2338, 0x233c, 0x2340, 0x2348, 0x234c, 0x2344};
    static void (TC *const populate[])(void *) = {
        fs98_options_prefs_general_populate, fs98_options_prefs_instrument_populate,
        fs98_options_prefs_international_populate, fs98_options_prefs_sound_populate,
        fs98_options_prefs_flight_populate, fs98_options_prefs_display_populate
    };
    uint32_t defaults = D32(BLOCK(object), 0xc), tab, index;
    if (defaults == 0 || defaults == 0xcdcdcdcd) {
        SERVICES->message_box("Bad m_pliveInfo->set_defaults in OnBnDefaults", 0, 0);
        return;
    }
    tab = (uint32_t)SERVICES->get_active_tab(AT(object, TAB));
    if (tab <= 5) D32(BLOCK(object), 8) = tab;
    ((fs98_options_block_fn)PTR(BLOCK(object), 0xc))(BLOCK(object));
    for (index = 0; index < 6; ++index)
        if (D32(PTR(object, pages[index]), 0x1100) != 0) populate[index](PTR(object, pages[index]));
}
/* 0x2239bf40: each page its part, the block, OK and Cancel */
void TC fs98_options_prefs_attach(void *object)
{
    void *ok = AT(object, 0x11dc), *cancel = AT(object, 0x12d8);
    uint8_t *page;
    page = PTR(object, 0x2338);
    PTR(page, 0x1118) = (uint8_t *)BLOCK(object) + 0x10;
    PTR(PTR(object, 0x2338), 0x111c) = BLOCK(object);
    PTR(PTR(object, 0x2338), 0x1120) = cancel;
    PTR(PTR(object, 0x2338), 0x1124) = ok;
    PTR(PTR(object, 0x233c), 0x1118) = (uint8_t *)BLOCK(object) + 0x6c;
    PTR(PTR(object, 0x233c), 0x111c) = cancel;
    PTR(PTR(object, 0x233c), 0x1120) = ok;
    PTR(PTR(object, 0x2340), 0x1118) = (uint8_t *)BLOCK(object) + 0x98;
    PTR(PTR(object, 0x2340), 0x111c) = cancel;
    PTR(PTR(object, 0x2340), 0x1120) = ok;
    PTR(PTR(object, 0x2344), 0x1118) = (uint8_t *)BLOCK(object) + 0xc0;
    PTR(PTR(object, 0x2344), 0x111c) = BLOCK(object);
    PTR(PTR(object, 0x2344), 0x1120) = cancel;
    PTR(PTR(object, 0x2344), 0x1124) = ok;
    PTR(PTR(object, 0x2348), 0x111c) = (uint8_t *)BLOCK(object) + 0xf8;
    PTR(PTR(object, 0x2348), 0x1120) = BLOCK(object);
    PTR(PTR(object, 0x2348), 0x1128) = cancel;
    PTR(PTR(object, 0x2348), 0x1124) = ok;
    PTR(PTR(object, 0x234c), 0x1118) = (uint8_t *)BLOCK(object) + 0x124;
    PTR(PTR(object, 0x234c), 0x111c) = cancel;
    PTR(PTR(object, 0x234c), 0x1120) = ok;
    PTR(object, 0x2350) = ok;
}
struct page_unwind {
    struct fs98_options_unwind frame;
    void *page;
    volatile uint32_t live;
};
static void page_cleanup(struct fs98_options_unwind *frame)
{
    struct page_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; fs98_options_dialog_delete(state->page); }
}
typedef void *(TC *page_constructor)(void *page, void *parent);
/* as Display options' (options_display.c). RETAIL BUG (kept): FLT-3. */
static FS98_OPTIONS_NOINLINE void new_page(void *object, uint32_t size, page_constructor construct, uint32_t at)
{
    struct page_unwind state;
    void *page;
    state.page = 0; state.live = 0;
    fs98_options_unwind_enter(&state.frame, page_cleanup);
    page = SERVICES->allocate(size);
    state.page = page; state.live = 1;
    if (page) page = construct(page, object);
    state.live = 0;
    fs98_options_unwind_leave(&state.frame);
    PTR(object, at) = page;
    SERVICES->page_init(page, 9);
}
/* 0x2239b9d0, OnInitDialog */
int32_t TC fs98_options_prefs_init(void *object)
{
    static const uint32_t titles[][3] = {
        {0xf75e, 0x10, 0x2338}, {0xf761, 0x6c, 0x233c}, {0xf75f, 0x98, 0x2340},
        {0xf75c, 0xc0, 0x2344}, {0xf75b, 0xf8, 0x2348}, {0xf75d, 0x124, 0x234c}
    };
    struct fs98_options_rect empty = {0, 0, 0, 0};
    char title[0x20];
    void *vtable, *font;
    uint32_t index, module;
    SERVICES->base_init(object);
    vtable = PTR(object, 0);
    if (!((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(vtable, 0xe4))(
            object, 0x14f, fs98_options_resource_module(), 0xf)) return 0;
    if (!fs98_options_has_descriptor(object)) return 0;
    SERVICES->init_art(AT(object, 0x209c), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    SERVICES->init_button(AT(object, 0x11dc), AT(object, 0x5a0), AT(object, 0x610), 0x132, AT(object, 0xff4),
                          AT(object, 0x7e8), 1, 0xffffffff, fs98_options_resource_module(), 0, 0, empty);
    SERVICES->init_button(AT(object, 0x12d8), AT(object, 0x5a0), AT(object, 0x610), 0x132, AT(object, 0xff4),
                          AT(object, 0x7e8), 1, 0xffffffff, fs98_options_resource_module(), 0, 0, empty);
    SERVICES->init_button(AT(object, 0x10e0), AT(object, 0x680), AT(object, 0x6f0), 0x137, AT(object, 0xff4),
                          AT(object, 0x7e8), 1, 0xffffffff, fs98_options_resource_module(), 1, 1, empty);
    PTR(object, 0x2334) = fs98_options_new_notify();
    module = fs98_options_resource_module();
    font = SERVICES->base_font(0xa);
    SERVICES->tab_init(AT(object, TAB), object, font, 0x148, AT(object, 0xff4), AT(object, 0x7e8), 0, 0xc4c4c4,
                       5, 0x1d, 0x47, -1, module, 0);
    ((void (TC *)(void *, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t))PTR(vtable, 0xe8))(
        object, 0x150, 0x1d, 0x47, 0x1a8, 0x121, fs98_options_resource_module());
    new_page(object, 0x1c4c, fs98_options_prefs_general_construct, 0x2338);
    new_page(object, 0x150c, fs98_options_prefs_instrument_construct, 0x233c);
    new_page(object, 0x1714, fs98_options_prefs_international_construct, 0x2340);
    new_page(object, 0x16ec, fs98_options_prefs_display_construct, 0x2344);
    new_page(object, 0x2dbc, fs98_options_prefs_sound_construct, 0x2348);
    new_page(object, 0x14f8, fs98_options_prefs_flight_construct, 0x234c);
    fs98_options_prefs_attach(object);
    for (index = 0; index < 6; ++index) {
        SERVICES->load_string_a(D32(object, 0x80), titles[index][0], title, 0x1d);
        SERVICES->add_page(AT(object, TAB), PTR(object, titles[index][2]), title,
                           (int32_t)D32(BLOCK(object), titles[index][1]));
    }
    for (index = 0; index < 6; ++index) SERVICES->helper_init(AT(object, 0x2184 + index * 0x48), AT(object, TAB));
    SERVICES->set_last_page(AT(object, TAB), PTR(object, 0x234c));
    SERVICES->set_active_tab(AT(object, TAB), (int32_t)D32(BLOCK(object), 8), 0);
    PTR(object, 0x7c) = AT(object, TAB);
    SERVICES->show_window(object, 1);
    return 1;
}
