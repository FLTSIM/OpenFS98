/* Display options (0x14090001, template 0x385, class CDisplayOptions at 0x22393b20, 0x21c4
 * bytes), MAIN's pf_display, and its four tab pages, each a CSimsTabPage: Aircraft (0x386),
 * Map (0x387), Scenery (0x388) and Hardware Acceleration (0x3a6). The block is described in
 * options_internal.h.
 *
 * The dialog's members: OK 0x10e4 and Cancel 0x11e0, the tab control 0x12dc, title art
 * 0x1fa4 and four CSimsTabCtrlHelpers 0x208c, 0x20d4, 0x211c and 0x2164. +0x10dc holds the
 * block, +0x10e0 OK once it exists, +0x21ac the CNotify, +0x21b0..+0x21bc the four pages
 * (made here, not members), +0x21c0 the answer state.
 *
 * A page's own words: +0x10dc and +0x10fc the palette and the bitmap its controls are drawn
 * from, +0x1100 set once the page has been shown (its controls exist), and +0x1118 its part
 * of the block. A check box's state is at +0xbc of it. */
#include "options_internal.h"
#include "options_objects.h"
#include "options_unwind.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
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
#define VK_RETURN_ 0xd
#define TAB 0x12dc

typedef uint32_t (SC *fs98_options_block_fn)(void *block);

/* a page's check box: Init through its own vtable, from the page's bitmap and palette */
static void check_init(void *page, uint32_t at)
{
    void *check = AT(page, at);
    uint32_t module = fs98_options_resource_module();
    ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))PTR(PTR(check, 0), 0xc4))(
        check, AT(page, 0x4c0), 0x139, PTR(page, 0x10fc), PTR(page, 0x10dc), module);
}

/* ---- Aircraft (0x386, class at 0x22392aa0, 0x168c bytes) --------------------------------
 * The label 0x1124 and six check boxes, four of them used: 0x11dc (+0x10 of its part),
 * 0x136c (+0x18), 0x1434 (+0x28) and 0x14fc (+0x20). */

/* 0x22392e70: the boxes into the part, once the page has been shown */
void TC fs98_options_display_aircraft_read(void *page)
{
    if (D32(page, 0x1100) == 0) return;
    D32(PART(page), 0x10) = D32(page, 0x1298);
    D32(PART(page), 0x18) = D32(page, 0x1428);
    D32(PART(page), 0x28) = D32(page, 0x14f0);
    D32(PART(page), 0x20) = D32(page, 0x15b8);
}
/* 0x22392ec0 */
void TC fs98_options_display_aircraft_populate(void *page)
{
    SERVICES->set_checked(AT(page, 0x11dc), (int32_t)D32(PART(page), 0x10));
    SERVICES->set_checked(AT(page, 0x1434), (int32_t)D32(PART(page), 0x28));
    SERVICES->set_checked(AT(page, 0x136c), (int32_t)D32(PART(page), 0x18));
    SERVICES->set_checked(AT(page, 0x14fc), (int32_t)D32(PART(page), 0x20));
}
/* 0x22392d50, vtable +0xf0: the page's controls once they exist */
void TC fs98_options_display_aircraft_init(void *page)
{
    SERVICES->init_text(AT(page, 0x1124), AT(page, 0x220));
    check_init(page, 0x11dc);
    check_init(page, 0x136c);
    check_init(page, 0x1434);
    check_init(page, 0x14fc);
    fs98_options_display_aircraft_populate(page);
}

/* ---- Map (0x387, class at 0x22392f30, 0x13a4 bytes) -------------------------------------
 * Two labels 0x1124 and 0x11dc, a combo 0x1294 (window +0x12b4; +8 of its part) and a
 * check box 0x12dc (+0x10). */

/* 0x223931c0 */
void TC fs98_options_display_map_read(void *page)
{
    uint32_t pick;
    if (D32(page, 0x1100) == 0) return;
    D32(PART(page), 0x10) = D32(page, 0x1398);
    pick = (uint32_t)SERVICES->send_message(D32(page, 0x12b4), CB_GETCURSEL_, 0, 0);
    D32(PART(page), 8) = pick;
}
/* 0x22393200 */
void TC fs98_options_display_map_populate(void *page)
{
    SERVICES->set_checked(AT(page, 0x12dc), (int32_t)D32(PART(page), 0x10));
    SERVICES->send_message(D32(page, 0x12b4), CB_SETCURSEL_, D32(PART(page), 8), 0);
}
/* 0x223930e0, vtable +0xf0 */
void TC fs98_options_display_map_init(void *page)
{
    SERVICES->init_text(AT(page, 0x1124), AT(page, 0x220));
    SERVICES->init_text(AT(page, 0x11dc), AT(page, 0x370));
    SERVICES->init_combo(AT(page, 0x1294), AT(page, 0x3e0), AT(page, 0x11dc));
    SERVICES->send_message(D32(page, 0x12b4), CB_SETCURSEL_, 0, 0);
    check_init(page, 0x12dc);
    fs98_options_display_map_populate(page);
}

/* ---- Scenery (0x388, class at 0x223943e0, 0x19ac bytes) ---------------------------------
 * The label 0x1124 and ten check boxes, each with its word of the part. 0x136c turns two
 * others on and off. */
static const uint32_t s_scenery[][2] = {    /* a box, and its word, in the order read */
    {0x11dc, 0x28}, {0x12a4, 0x50}, {0x136c, 0x20}, {0x1434, 0x10}, {0x14fc, 0x18},
    {0x15c4, 0x40}, {0x168c, 0x48}, {0x1754, 8}, {0x181c, 0x38}, {0x18e4, 0x30}
};
/* 0x223948e0 */
void TC fs98_options_display_scenery_read(void *page)
{
    uint32_t index;
    if (D32(page, 0x1100) == 0) return;
    for (index = 0; index < 10; ++index)
        D32(PART(page), s_scenery[index][1]) = D32(page, s_scenery[index][0] + 0xbc);
}
/* 0x22394a60 (and BN_CLICKED on 0x1178): 0x11dc and 0x18e4 usable while 0x136c is on */
void TC fs98_options_display_scenery_enable(void *page)
{
    int32_t on = D32(page, 0x1428) != 0;
    SERVICES->enable_control(AT(page, 0x11dc), on);
    SERVICES->enable_control(AT(page, 0x18e4), on);
}
/* 0x22394990: the second box set second */
void TC fs98_options_display_scenery_populate(void *page)
{
    static const uint32_t order[] = {0, 9, 1, 2, 3, 4, 5, 6, 7, 8};
    uint32_t index;
    for (index = 0; index < 10; ++index)
        SERVICES->set_checked(AT(page, s_scenery[order[index]][0]), (int32_t)D32(PART(page), s_scenery[order[index]][1]));
    fs98_options_display_scenery_enable(page);
}
/* 0x223946c0, vtable +0xf0 */
void TC fs98_options_display_scenery_init(void *page)
{
    uint32_t index;
    SERVICES->init_text(AT(page, 0x1124), AT(page, 0x220));
    for (index = 0; index < 10; ++index) check_init(page, s_scenery[index][0]);
    fs98_options_display_scenery_populate(page);
}

/* ---- Hardware Acceleration (0x3a6, class at 0x22393240, 0x18ac bytes) -------------------
 * Labels 0x1124, 0x11dc and 0x12dc, the drivers' combo 0x1294 (window +0x12b4) and the
 * modes' 0x1394 (+0x13b4), the acceleration box 0x13dc, Refresh 0x14a4, three boxes 0x15a0,
 * 0x1668 and 0x1730 that need acceleration, and a group 0x17f8. Its part is MAIN's driver
 * block (main_prefdlg.inc): three stdcall callbacks given the part, +0 list the drivers,
 * +4 list the driver's modes, +8 refresh; +0x10 acceleration on, +0x14 a count of changes,
 * +0x18 the drivers' room and +0x1c the drivers (a null-ended list of names), +0x20 the one
 * chosen, +0x24/+0x28/+0x2c the same for the modes, +0x30 whether acceleration can be had,
 * +0x34/+0x38/+0x3c the three boxes. */

/* 0x22393870 and 0x223938a0: the boxes acceleration needs */
static void accelerated(void *page, int32_t enable)
{
    SERVICES->enable_control(AT(page, 0x15a0), enable);
    SERVICES->enable_control(AT(page, 0x1730), enable);
    SERVICES->enable_control(AT(page, 0x1668), enable);
}
/* 0x22393750 */
void TC fs98_options_display_hardware_read(void *page)
{
    uint32_t pick;
    if (D32(page, 0x1100) == 0) return;
    D32(PART(page), 0x10) = D32(page, 0x1498);
    D32(PART(page), 0x34) = D32(page, 0x165c);
    D32(PART(page), 0x38) = D32(page, 0x17ec);
    D32(PART(page), 0x3c) = D32(page, 0x1724);
    pick = (uint32_t)SERVICES->send_message(D32(page, 0x13b4), CB_GETCURSEL_, 0, 0);
    D32(PART(page), 0x2c) = pick;
    pick = (uint32_t)SERVICES->send_message(D32(page, 0x12b4), CB_GETCURSEL_, 0, 0);
    D32(PART(page), 0x20) = pick;
}
/* 0x223938d0 and 0x22393990: a combo filled from a list (its room, the names, the one
 * chosen), up to the room or the list's end, the room read again after each */
static void fill(void *page, uint32_t window, uint32_t room, uint32_t names, uint32_t chosen)
{
    uint32_t index = 0;
    SERVICES->send_message(D32(page, window), CB_RESETCONTENT_, 0, 0);
    if (D32(PART(page), room) != 0) {
        do {
            uint32_t name = ((uint32_t *)PTR(PART(page), names))[index];
            if (name == 0) break;
            SERVICES->send_message(D32(page, window), CB_ADDSTRING_, 0, name);
            ++index;
        } while (index < D32(PART(page), room));
    }
    SERVICES->send_message(D32(page, window), CB_SETCURSEL_, D32(PART(page), chosen), 0);
}
void TC fs98_options_display_hardware_fill_drivers(void *page) { fill(page, 0x12b4, 0x18, 0x1c, 0x20); }
void TC fs98_options_display_hardware_fill_modes(void *page) { fill(page, 0x13b4, 0x24, 0x28, 0x2c); }
/* 0x22393950: MAIN lists the drivers again */
void TC fs98_options_display_hardware_list_drivers(void *page)
{
    D32(PART(page), 0x20) = 0;
    ((fs98_options_block_fn)PTR(PART(page), 0))(PART(page));
    SERVICES->enable_control(AT(page, 0x13dc), (int32_t)D32(PART(page), 0x30));
    fs98_options_display_hardware_fill_drivers(page);
}
/* 0x22393a10: and the chosen driver's modes */
void TC fs98_options_display_hardware_list_modes(void *page)
{
    D32(PART(page), 0x2c) = 0;
    ((fs98_options_block_fn)PTR(PART(page), 4))(PART(page));
    SERVICES->enable_control(AT(page, 0x15a0), (int32_t)D32(PART(page), 0x30));
    SERVICES->enable_control(AT(page, 0x1730), (int32_t)D32(PART(page), 0x30));
    SERVICES->enable_control(AT(page, 0x1668), (int32_t)D32(PART(page), 0x30));
    fs98_options_display_hardware_fill_modes(page);
}
/* 0x223937e0 */
void TC fs98_options_display_hardware_populate(void *page)
{
    if (D32(PART(page), 0x10) != 0) {
        SERVICES->set_checked(AT(page, 0x13dc), 1);
        accelerated(page, 1);
    } else {
        SERVICES->set_checked(AT(page, 0x13dc), 0);
        accelerated(page, 0);
    }
    SERVICES->set_checked(AT(page, 0x15a0), (int32_t)D32(PART(page), 0x34));
    SERVICES->set_checked(AT(page, 0x1730), (int32_t)D32(PART(page), 0x38));
    SERVICES->set_checked(AT(page, 0x1668), (int32_t)D32(PART(page), 0x3c));
    fs98_options_display_hardware_fill_drivers(page);
    fs98_options_display_hardware_fill_modes(page);
}
/* 0x22393a80 (BN_CLICKED on 0x416, the acceleration box) */
void TC fs98_options_display_hardware_accelerate(void *page)
{
    accelerated(page, D32(page, 0x1498) != 0);
    fs98_options_display_hardware_read(page);
    fs98_options_display_hardware_list_drivers(page);
    fs98_options_display_hardware_list_modes(page);
    ++D32(PART(page), 0x14);
}
/* 0x22393ac0 (BN_CLICKED on 0x1be8, Refresh): MAIN refreshes, then both lists again */
void TC fs98_options_display_hardware_refresh(void *page)
{
    ((fs98_options_block_fn)PTR(PART(page), 8))(PART(page));
    fs98_options_display_hardware_list_drivers(page);
    fs98_options_display_hardware_list_modes(page);
    ++D32(PART(page), 0x14);
}
/* 0x22393af0 (CBN_SELCHANGE on 0x123b, the drivers): that driver's modes */
void TC fs98_options_display_hardware_driver(void *page)
{
    fs98_options_display_hardware_read(page);
    fs98_options_display_hardware_list_modes(page);
    ++D32(PART(page), 0x14);
}
/* 0x22393b10 (the three boxes, and CBN_SELCHANGE on the modes): a change */
void TC fs98_options_display_hardware_changed(void *page) { ++D32(PART(page), 0x14); }
/* 0x22393540, vtable +0xf0 */
void TC fs98_options_display_hardware_init(void *page)
{
    struct fs98_options_rect empty = {0, 0, 0, 0};
    SERVICES->init_text(AT(page, 0x1124), AT(page, 0x220));
    SERVICES->init_text(AT(page, 0x11dc), AT(page, 0x370));
    SERVICES->init_combo(AT(page, 0x1294), AT(page, 0x3e0), AT(page, 0x11dc));
    SERVICES->send_message(D32(page, 0x12b4), CB_SETCURSEL_, 0, 0);
    SERVICES->init_text(AT(page, 0x12dc), AT(page, 0x370));
    SERVICES->init_combo(AT(page, 0x1394), AT(page, 0x3e0), AT(page, 0x12dc));
    SERVICES->send_message(D32(page, 0x13b4), CB_SETCURSEL_, 0, 0);
    check_init(page, 0x15a0);
    check_init(page, 0x1730);
    check_init(page, 0x1668);
    SERVICES->init_button(AT(page, 0x14a4), AT(page, 0x5a0), AT(page, 0x610), 0x137, PTR(page, 0x10fc),
                          PTR(page, 0x10dc), 1, 0xffffffff, fs98_options_resource_module(), 1, 1, empty);
    check_init(page, 0x13dc);
    SERVICES->init_group(AT(page, 0x17f8), AT(page, 0x760));
    fs98_options_display_hardware_populate(page);
    fs98_options_display_hardware_list_drivers(page);
    fs98_options_display_hardware_list_modes(page);
}

/* ---- the dialog ------------------------------------------------------------------------- */

/* 0x22393da0, OnCancel: the block and the vtable read before the stores */
void TC fs98_options_display_cancel(void *object)
{
    uint32_t *block = BLOCK(object);
    void *vtable = PTR(object, 0);
    D32(object, 0x21c0) = 2;
    block[0] = 0;
    ((void (TC *)(void *))PTR(vtable, 0xec))(object);
}
/* 0x223942b0: each page's controls into its part */
void TC fs98_options_display_read(void *object)
{
    fs98_options_display_aircraft_read(PTR(object, 0x21b0));
    fs98_options_display_map_read(PTR(object, 0x21b4));
    fs98_options_display_scenery_read(PTR(object, 0x21b8));
    fs98_options_display_hardware_read(PTR(object, 0x21bc));
}
/* 0x22394360: the pages' error checks, each Options' empty one (0x22392f20) */
uint32_t TC fs98_options_display_error(void *object)
{
    if (fs98_options_no_error(PTR(object, 0x21b0))) return 1;
    if (fs98_options_no_error(PTR(object, 0x21b4))) return 1;
    if (fs98_options_no_error(PTR(object, 0x21b8))) return 1;
    return fs98_options_no_error(PTR(object, 0x21bc)) != 0;
}
/* 0x22393dc0, OnOK: with Return down, OK takes the focus first */
void TC fs98_options_display_accept(void *object)
{
    if (SERVICES->async_key_state(VK_RETURN_) & 1) SERVICES->set_focus(PTR(object, 0x10e0));
    D32(object, 0x21c0) = 1;
    fs98_options_display_read(object);
    if (fs98_options_check(object) == 0 || fs98_options_display_error(object) == 0) {
        D32(BLOCK(object), 0) = 1;
        ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
    }
}
/* 0x22393e20, vtable +0xec: the tab control's own +0xc4, the CNotify deleted, then
 * CSimsDialog::Close */
void TC fs98_options_display_close(void *object)
{
    void *notify;
    ((void (TC *)(void *))PTR(PTR(object, TAB), 0xc4))(AT(object, TAB));
    notify = PTR(object, 0x21ac);
    if (notify) ((void *(TC *)(void *, uint32_t))PTR(PTR(notify, 0), 4))(notify, 1);
    D32(object, 0x21ac) = 0;
    SERVICES->close(object);
}
/* 0x223942f0: each page its part of the block, and OK's address */
void TC fs98_options_display_attach(void *object)
{
    static const uint32_t parts[] = {0xc, 0x38, 0x4c, 0xa0};
    uint32_t index;
    for (index = 0; index < 4; ++index)
        PTR(PTR(object, 0x21b0 + index * 4), 0x1118) = (uint8_t *)BLOCK(object) + parts[index];
    PTR(object, 0x10e0) = AT(object, 0x10e4);
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
/* a page made: ui_new, its constructor given the dialog, CSimsTabPage::Init(9).
 * RETAIL BUG (kept): FLT-3 (docs/retail-bugs.md): a failed allocation is stored and Init
 * is called on it all the same. */
static FS98_OPTIONS_NOINLINE void *new_page(void *object, uint32_t size, page_constructor construct, uint32_t at)
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
    return page;
}
/* 0x22393ea0, OnInitDialog: the skin's vtable read once and used again for the frame */
int32_t TC fs98_options_display_init(void *object)
{
    static const uint32_t titles[][2] = {{0xf75a, 0xc}, {0xf760, 0x38}, {0xf762, 0x4c}, {0xf765, 0xac}};
    struct fs98_options_rect empty = {0, 0, 0, 0};
    char title[0x20];
    void *vtable, *font;
    uint32_t index, module;
    SERVICES->base_init(object);
    vtable = PTR(object, 0);
    if (!((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(vtable, 0xe4))(
            object, 0x14f, fs98_options_resource_module(), 0xf)) return 0;
    if (!fs98_options_has_descriptor(object)) return 0;
    SERVICES->init_art(AT(object, 0x1fa4), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    SERVICES->init_button(AT(object, 0x10e4), AT(object, 0x5a0), AT(object, 0x610), 0x132, AT(object, 0xff4),
                          AT(object, 0x7e8), 1, 0xffffffff, fs98_options_resource_module(), 0, 0, empty);
    SERVICES->init_button(AT(object, 0x11e0), AT(object, 0x5a0), AT(object, 0x610), 0x132, AT(object, 0xff4),
                          AT(object, 0x7e8), 1, 0xffffffff, fs98_options_resource_module(), 0, 0, empty);
    PTR(object, 0x21ac) = fs98_options_new_notify();
    module = fs98_options_resource_module();
    font = SERVICES->base_font(0xa);
    SERVICES->tab_init(AT(object, TAB), object, font, 0x148, AT(object, 0xff4), AT(object, 0x7e8), 0, 0xc4c4c4,
                       5, 0x1d, 0x47, -1, module, 0);
    ((void (TC *)(void *, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t))PTR(vtable, 0xe8))(
        object, 0x150, 0x1d, 0x47, 0x1a8, 0x121, fs98_options_resource_module());
    new_page(object, 0x168c, fs98_options_display_aircraft_construct, 0x21b0);
    new_page(object, 0x13a4, fs98_options_display_map_construct, 0x21b4);
    new_page(object, 0x19ac, fs98_options_display_scenery_construct, 0x21b8);
    new_page(object, 0x18ac, fs98_options_display_hardware_construct, 0x21bc);
    fs98_options_display_attach(object);
    for (index = 0; index < 4; ++index) {
        SERVICES->load_string_a(D32(object, 0x80), titles[index][0], title, 0x1d);
        SERVICES->add_page(AT(object, TAB), PTR(object, 0x21b0 + index * 4), title,
                           (int32_t)D32(BLOCK(object), titles[index][1]));
    }
    SERVICES->helper_init(AT(object, 0x208c), AT(object, TAB));
    SERVICES->helper_init(AT(object, 0x20d4), AT(object, TAB));
    SERVICES->helper_init(AT(object, 0x211c), AT(object, TAB));
    SERVICES->helper_init(AT(object, 0x2164), AT(object, TAB));
    SERVICES->set_last_page(AT(object, TAB), PTR(object, 0x21bc));
    SERVICES->set_active_tab(AT(object, TAB), (int32_t)D32(BLOCK(object), 8), 0);
    PTR(object, 0x7c) = AT(object, TAB);
    SERVICES->show_window(object, 1);
    return 1;
}
