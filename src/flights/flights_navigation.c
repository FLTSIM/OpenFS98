/* Navigation, dialog 0x1202 (class at 0x22317d80, 0x2178 bytes), and its tab pages.
 *
 * The dialog: OK 0x10e0, Cancel 0x11dc, the tab control 0x12d8, title art 0x1fa0, three tab
 * helpers 0x2088 0x20d0 0x2118; the pages at +0x2160 +0x2164 +0x2168, the answer state
 * +0x2174, the descriptor +0x10dc (flights_internal.h).
 *
 * The pages are NAV 1 and NAV 2 (template 0x203, class 0x22318560), the transponder and ADF
 * (0x204, 0x22319490) and EFIS (0x202, 0x22317040).
 *
 * The EFIS page (0x1b64 bytes): labels 0x1128 0x11e0 0x1298 for the combos 0x1528 (window
 * +0x1548) 0x1570 (+0x1590) 0x15b8 (+0x15d8), the type, density and range; labels 0x1350
 * 0x1408; the altitude edit 0x14c0 (window +0x14e0); the master switch box 0x16c8 (state
 * +0x1784) and the intercepting-path box 0x1600 (+0x16bc); the lock radios, VOR and
 * altitude 0x1978 and ILS/LOC 0x1a6c (window +0x1a8c); VOR 1 0x1884 (window +0x18a4) and
 * VOR 2 0x1790; and a CString 0x1b60 exchanged as the altitude. */
#include "flights_internal.h"
#include "flights_unwind.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_FLIGHTS_THISCALL
#define SERVICES fs98_flights_services()
#define CB_SETCURSEL_ 0x14e
#define CB_GETCURSEL_ 0x147
#define GWL_USERDATA_ (-21)

/* ---- the dialog --------------------------------------------------------------------------- */

/* 0x22317f20, vtable +0xd8: a debugging leftover that says so and passes */
int32_t TC fs98_flights_navigation_validate(void *object, uint32_t control)
{
    (void)object; (void)control;
    SERVICES->message_box("CNavigation::ValidateData()", 0, 0);
    return 0;
}

/* 0x22318350, vtable +0xec: the tab control lets its pages go (its +0xc4), then Close */
void TC fs98_flights_navigation_close(void *object)
{
    void *tabs = AT(object, 0x12d8);
    ((void (TC *)(void *))PTR(PTR(tabs, 0), 0xc4))(tabs);
    SERVICES->close(object);
}

/* 0x22318370, OnCancel */
void TC fs98_flights_navigation_cancel(void *object)
{
    uint32_t *descriptor = PTR(object, 0x10dc);
    void **vtable = PTR(object, 0);
    D32(object, 0x2174) = 2;
    descriptor[0] = 0;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}

/* 0x22318390, OnOK: each page is asked in turn, a failure it reports keeping the dialog */
void TC fs98_flights_navigation_accept(void *object)
{
    static const uint32_t pages[] = {0x2160, 0x2164, 0x2168};
    uint32_t index;
    for (index = 0; index < 3; ++index) {
        void *page = PTR(object, pages[index]);
        uint32_t failure = ((uint32_t (TC *)(void *, uint32_t))PTR(PTR(page, 0), 0xd8))(page, 0);
        if (failure != 0) {
            page = PTR(object, pages[index]);
            if (((uint32_t (TC *)(void *, uint32_t))PTR(PTR(page, 0), 0xdc))(page, failure) != 0) return;
        }
    }
    D32(PTR(object, 0x10dc), 0) = 1;
    ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
}

/* 0x22318460: each page given its slice, the descriptor, the dialog and its buttons */
void TC fs98_flights_navigation_wire(void *object)
{
    static const uint32_t pages[] = {0x2160, 0x2164, 0x2168}, slices[] = {0x10, 0x130, 0x1bc};
    uint32_t index;
    for (index = 0; index < 3; ++index) {
        PTR(PTR(object, pages[index]), 0x1118) = AT(PTR(object, 0x10dc), slices[index]);
        PTR(PTR(object, pages[index]), 0x111c) = PTR(object, 0x10dc);
        if (index != 2) PTR(PTR(object, pages[index]), 0x1128) = object;   /* not the EFIS page */
        PTR(PTR(object, pages[index]), 0x1120) = AT(object, 0x11dc);
        PTR(PTR(object, pages[index]), 0x1124) = AT(object, 0x10e0);
    }
    PTR(object, 0x216c) = AT(object, 0x10e0);
    PTR(object, 0x2170) = AT(object, 0x11dc);
}

/* ---- the EFIS page -------------------------------------------------------------------- */

#define SLICE(page) PTR(page, 0x1118)
#define S32(page, at) D32(SLICE(page), at)
static void enable(void *page, uint32_t at, int32_t on) { SERVICES->enable_control(AT(page, at), on); }
static void radio(void *page, uint32_t at, int32_t on) { SERVICES->set_radio(AT(page, at), on); }

/* 0x223176b0, 0x223176f0: lock to ILS/LOC (4), lock to VOR and altitude (2) */
void TC fs98_flights_nav_efis_four(void *page)
{
    enable(page, 0x1790, 0);
    enable(page, 0x1884, 0);
    enable(page, 0x14c0, 0);
    S32(page, 0x30) = 4;
}
void TC fs98_flights_nav_efis_two(void *page)
{
    enable(page, 0x1790, S32(page, 0x48) != 0);
    enable(page, 0x1884, S32(page, 0x44) != 0);
    enable(page, 0x14c0, 1);
    S32(page, 0x30) = 2;
}
/* 0x22317750, 0x22317760: VOR 1 and VOR 2 */
void TC fs98_flights_nav_efis_first(void *page) { S32(page, 0x40) = 0; }
void TC fs98_flights_nav_efis_second(void *page) { S32(page, 0x40) = 1; }

/* 0x22317770: the master switch, which makes the rest usable */
void TC fs98_flights_nav_efis_enable(void *page)
{
    static const uint32_t controls[] = {0x1600, 0x1528, 0x1570, 0x15b8, 0x1128, 0x11e0, 0x1298};
    uint32_t index;
    if (D32(page, 0x1784) == 0) {
        static const uint32_t all[] = {0x1600, 0x1528, 0x1570, 0x15b8, 0x1128, 0x11e0, 0x1298,
                                       0x1978, 0x1a6c, 0x1790, 0x1884, 0x14c0};
        for (index = 0; index < 12; ++index) enable(page, all[index], 0);
        return;
    }
    for (index = 0; index < 7; ++index) enable(page, controls[index], 1);
    enable(page, 0x1978, S32(page, 0x34) != 0);
    enable(page, 0x1a6c, S32(page, 0x38) != 0);
    enable(page, 0x1884, S32(page, 0x44) != 0);
    enable(page, 0x1790, S32(page, 0x48) != 0);
    if (S32(page, 0x30) == 2) {
        radio(page, 0x1978, 1);
        fs98_flights_nav_efis_two(page);
        if (S32(page, 0x40) == 0) {
            radio(page, 0x1884, 1);
            fs98_flights_nav_efis_first(page);
        } else {
            radio(page, 0x1790, 1);
            fs98_flights_nav_efis_second(page);
        }
    } else if (S32(page, 0x30) == 4) {
        radio(page, 0x1a6c, 1);
        fs98_flights_nav_efis_four(page);
    }
}

/* One radio of a pair: usable when available, checked or not */
static void fs98_flights_nav_efis_offer(void *page, uint32_t at, uint32_t available, int32_t checked)
{
    if (available) {
        enable(page, at, 1);
        radio(page, at, checked);
    } else {
        enable(page, at, 0);
    }
}

/* 0x22317b00: the slice into the controls */
void TC fs98_flights_nav_efis_refill(void *page)
{
    SERVICES->set_checked(AT(page, 0x16c8), (int32_t)S32(page, 8));
    SERVICES->set_checked(AT(page, 0x1600), S32(page, 0x10) == 2);
    if (S32(page, 0x30) == 2) {
        uint32_t second;
        fs98_flights_nav_efis_offer(page, 0x1978, S32(page, 0x34), 1);
        fs98_flights_nav_efis_offer(page, 0x1a6c, S32(page, 0x38), 0);
        second = S32(page, 0x40);
        fs98_flights_nav_efis_offer(page, 0x1884, S32(page, 0x44), second == 0);
        fs98_flights_nav_efis_offer(page, 0x1790, S32(page, 0x48), second != 0);
    } else {
        fs98_flights_nav_efis_offer(page, 0x1a6c, S32(page, 0x38), 1);
        fs98_flights_nav_efis_offer(page, 0x1978, S32(page, 0x34), 0);
        enable(page, 0x1790, 0);
        enable(page, 0x1884, 0);
        enable(page, 0x14c0, 0);
    }
    SERVICES->send_message(D32(page, 0x1548), CB_SETCURSEL_, S32(page, 0x18), 0);
    SERVICES->send_message(D32(page, 0x1590), CB_SETCURSEL_, S32(page, 0x28), 0);
    SERVICES->send_message(D32(page, 0x15d8), CB_SETCURSEL_, S32(page, 0x20), 0);
    SERVICES->set_window_text(AT(page, 0x14c0), PTR(SLICE(page), 0x50));
    fs98_flights_nav_efis_enable(page);
}

/* 0x22317d70: the message 0x5f9 asks for a refill */
int32_t TC fs98_flights_nav_efis_message(void *page, uint32_t w_param, uint32_t l_param)
{
    (void)w_param; (void)l_param;
    fs98_flights_nav_efis_refill(page);
    return 0;
}

/* 0x22317980, vtable +0xd8: only once the page exists. The controls into the slice, then
 * the dialog's check; a failure with the altitude's status set names its edit, any other
 * is the check's own answer. The controls are filled again either way. */
uint32_t TC fs98_flights_nav_efis_validate(void *page, uint32_t control)
{
    uint32_t failure, data;
    void *descriptor;
    (void)control;
    if (D32(page, 0x1100) == 0) return 0;
    S32(page, 8) = D32(page, 0x1784);
    /* each radio is checked when its window's user data is its own handle, read after */
    data = (uint32_t)SERVICES->get_window_long(D32(page, 0x1a8c), GWL_USERDATA_);
    if (D32(page, 0x1a8c) == data) {
        S32(page, 0x30) = 4;
    } else {
        S32(page, 0x30) = 2;
        data = (uint32_t)SERVICES->get_window_long(D32(page, 0x18a4), GWL_USERDATA_);
        if (D32(page, 0x18a4) == data)
            S32(page, 0x40) = 0;
        else
            S32(page, 0x40) = 1;
    }
    {
        int32_t choice = SERVICES->send_message(D32(page, 0x1548), CB_GETCURSEL_, 0, 0);
        S32(page, 0x18) = (uint32_t)choice;
        choice = SERVICES->send_message(D32(page, 0x1590), CB_GETCURSEL_, 0, 0);
        S32(page, 0x28) = (uint32_t)choice;
        choice = SERVICES->send_message(D32(page, 0x15d8), CB_GETCURSEL_, 0, 0);
        S32(page, 0x20) = (uint32_t)choice;
    }
    S32(page, 0x10) = D32(page, 0x16bc) ? 2 : 0;
    SERVICES->get_window_text(AT(page, 0x14c0), PTR(SLICE(page), 0x50), 6);
    descriptor = PTR(page, 0x111c);
    failure = ((fs98_flights_navigation_check_fn)PTR(descriptor, 4))(descriptor);
    if (failure != 0 && S32(page, 0x4c) != 0) failure = 0xcb4;
    fs98_flights_nav_efis_refill(page);
    return failure;
}

/* 0x22317af0, vtable +0xdc: whatever the failure, vtable +0xe0 is told none */
uint32_t TC fs98_flights_nav_efis_error(void *page, uint32_t code)
{
    (void)code;
    return ((uint32_t (TC *)(void *, uint32_t))PTR(PTR(page, 0), 0xe0))(page, 0);
}

/* 0x22317480, vtable +0xf0: the page's controls, its boxes and radios drawn with the
 * parent's art (+0x10fc DIB, +0x10dc palette) */
void TC fs98_flights_nav_efis_init(void *page)
{
    static const uint32_t boxes[] = {0x16c8, 0x1600}, radios[] = {0x1a6c, 0x1978, 0x1884, 0x1790};
    static const uint32_t combos[][3] = {{0x1298, 0x15b8, 0x15d8}, {0x11e0, 0x1570, 0x1590}, {0x1128, 0x1528, 0x1548}};
    uint32_t module = fs98_flights_resource_module(), index;
    SERVICES->init_text(AT(page, 0x1350), AT(page, 0x220));
    SERVICES->init_text(AT(page, 0x1408), AT(page, 0x370));
    SERVICES->init_edit(AT(page, 0x14c0), SERVICES->base_font(2), 0xffffff, 0, 0xff0000, AT(page, 0x1408), 1);
    for (index = 0; index < 2; ++index) {
        void *box = AT(page, boxes[index]);
        ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))PTR(PTR(box, 0), 0xc4))(
            box, AT(page, 0x4c0), 0x139, PTR(page, 0x10fc), PTR(page, 0x10dc), module);
    }
    for (index = 0; index < 4; ++index) {
        void *button = AT(page, radios[index]);
        ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))PTR(PTR(button, 0), 0xc4))(
            button, AT(page, 0x530), 0x134, PTR(page, 0x10fc), PTR(page, 0x10dc), module);
    }
    for (index = 0; index < 3; ++index) {
        SERVICES->init_text(AT(page, combos[index][0]), AT(page, 0x370));
        SERVICES->init_combo(AT(page, combos[index][1]), AT(page, 0x3e0), AT(page, combos[index][0]));
        SERVICES->send_message(D32(page, combos[index][2]), CB_SETCURSEL_, 0, 0);
    }
    SERVICES->send_message(D32(page, 0x14e0), EM_LIMITTEXT, S32(page, 0x54) - 1, 0);
    fs98_flights_nav_efis_refill(page);
}

/* ---- the NAV 1 and NAV 2 page ---------------------------------------------------------
 * Labels 0x112c..0x21ac (CSimsText), group boxes 0x1cac and 0x2040, the edits NAV 1
 * frequency 0x239c (label 0x1f88), NAV 1 OBS 0x2334 (0x1d60), NAV 2 frequency 0x22cc
 * (0x1bf4), NAV 2 OBS 0x2264 (0x19cc), and the localizer combo 0x2404 (window +0x2424,
 * label 0x20f4). Its slice: the radios fitted +4 (NAV 1) +8 (its OBS) +0xc (NAV 2) +0x10
 * (its OBS); statuses +0x18 +0x30 +0xa0 +0xb8 and texts +0x1c +0x34 +0xa4 +0xbc with room
 * +0x20 +0x38 +0xa8 +0xc0; NAV 1's station: ident in place +0x44, a name in place +0x4a,
 * a 16-bit "received" +0x2c, names +0x6c +0x80; the localizer choice +0x9c, two of them
 * +0x90 (with a 16-bit +0x94); NAV 2's station: +0xcc +0xd2 in place, received +0xb4,
 * names +0xf4 +0x108. */
static const void *s_edit_class;
void fs98_flights_set_edit_class(const void *runtime_class) { s_edit_class = runtime_class; }
const void *fs98_flights_edit_class(void) { return s_edit_class; }
#define EM_GETMODIFY_ 0xb8
#define EM_SETMODIFY_ 0xb9
#define W16(object, offset) (*(uint16_t *)((uint8_t *)(object) + (offset)))

static void enable_all(void *page, const uint32_t *controls, uint32_t count, int32_t on)
{
    uint32_t index;
    for (index = 0; index < count; ++index) enable(page, controls[index], on);
}

/* 0x22319090: the slice into the controls */
void TC fs98_flights_nav_radios_fill(void *page)
{
    static const uint32_t nav1[] = {0x1e18, 0x1ed0, 0x185c, 0x1914, 0x11e4, 0x112c, 0x1634, 0x157c};
    static const uint32_t nav2[] = {0x1a84, 0x1b3c, 0x16ec, 0x17a4, 0x1354, 0x129c, 0x14c4, 0x140c};
    int32_t show;
    SERVICES->send_message(D32(page, 0x2424), CB_SETCURSEL_, S32(page, 0x9c), 0);
    SERVICES->set_window_text(AT(page, 0x239c), PTR(SLICE(page), 0x1c));
    if (S32(page, 4) == 0) {
        enable(page, 0x2040, 0);
        enable(page, 0x1f88, 0);
        enable(page, 0x239c, 0);
    }
    SERVICES->set_window_text(AT(page, 0x2334), PTR(SLICE(page), 0x34));
    if (S32(page, 8) == 0) {
        enable(page, 0x1d60, 0);
        enable(page, 0x2334, 0);
    }
    SERVICES->set_window_text(AT(page, 0x22cc), PTR(SLICE(page), 0xa4));
    if (S32(page, 0xc) == 0) {
        enable(page, 0x1cac, 0);
        enable(page, 0x1bf4, 0);
        enable(page, 0x22cc, 0);
    }
    SERVICES->set_window_text(AT(page, 0x2264), PTR(SLICE(page), 0xbc));
    if (S32(page, 0x10) == 0) {
        enable(page, 0x19cc, 0);
        enable(page, 0x2264, 0);
    }
    SERVICES->set_sims_text(AT(page, 0x185c), (const char *)AT(SLICE(page), 0x4a));
    SERVICES->set_sims_text(AT(page, 0x1914), (const char *)AT(SLICE(page), 0x44));
    SERVICES->set_sims_text(AT(page, 0x112c), PTR(SLICE(page), 0x80));
    SERVICES->set_sims_text(AT(page, 0x157c), PTR(SLICE(page), 0x6c));
    enable_all(page, nav1, 8, W16(SLICE(page), 0x2c) != 0 && S32(page, 4) != 0);
    show = S32(page, 0x90) != 0 && W16(SLICE(page), 0x94) != 0 && S32(page, 4) != 0 ? 5 : 0;
    SERVICES->show_window(AT(page, 0x21ac), show);
    SERVICES->show_window(AT(page, 0x20f4), show);
    SERVICES->show_window(AT(page, 0x2404), show);
    SERVICES->set_sims_text(AT(page, 0x16ec), (const char *)AT(SLICE(page), 0xd2));
    SERVICES->set_sims_text(AT(page, 0x17a4), (const char *)AT(SLICE(page), 0xcc));
    SERVICES->set_sims_text(AT(page, 0x129c), PTR(SLICE(page), 0x108));
    SERVICES->set_sims_text(AT(page, 0x140c), PTR(SLICE(page), 0xf4));
    enable_all(page, nav2, 8, W16(SLICE(page), 0xb4) != 0 && S32(page, 0xc) != 0);
}

/* 0x22318b80, vtable +0xf0 */
void TC fs98_flights_nav_radios_init(void *page)
{
    static const uint32_t labels[] = {0x21ac, 0x1ed0, 0x1e18, 0x1b3c, 0x1a84, 0x1914, 0x185c, 0x17a4,
                                      0x16ec, 0x11e4, 0x112c, 0x1634, 0x157c, 0x14c4, 0x140c, 0x1354, 0x129c};
    /* each edit after its label: frequency 5 (a filter flag), OBS 1 */
    static const uint32_t edits[][3] = {{0x239c, 0x1f88, 5}, {0x22cc, 0x1bf4, 5}, {0x2334, 0x1d60, 1}, {0x2264, 0x19cc, 1}};
    static const uint32_t limits[][2] = {{0x23bc, 0x20}, {0x22ec, 0xa8}, {0x2354, 0x38}, {0x2284, 0xc0}};
    uint32_t index;
    D32(page, 0x84) = 0xb;
    SERVICES->init_group(AT(page, 0x2040), AT(page, 0x760));
    SERVICES->init_group(AT(page, 0x1cac), AT(page, 0x760));
    SERVICES->init_text(AT(page, 0x20f4), AT(page, 0x370));
    SERVICES->init_combo(AT(page, 0x2404), AT(page, 0x3e0), AT(page, 0x20f4));
    SERVICES->send_message(D32(page, 0x2424), CB_SETCURSEL_, 0, 0);
    for (index = 0; index < 17; ++index) SERVICES->init_text(AT(page, labels[index]), AT(page, 0x220));
    for (index = 0; index < 4; ++index) {
        SERVICES->init_text(AT(page, edits[index][1]), AT(page, 0x220));
        SERVICES->init_edit(AT(page, edits[index][0]), SERVICES->base_font(2), 0xffffff, 0, 0xff0000,
                            AT(page, edits[index][1]), edits[index][2]);
    }
    for (index = 0; index < 4; ++index)
        SERVICES->send_message(D32(page, limits[index][0]), EM_LIMITTEXT, S32(page, limits[index][1]) - 1, 0);
    fs98_flights_nav_radios_fill(page);
}

/* Is it an edit (CObject::IsKindOf against CEdit's runtime class)? */
static int is_edit(void *window) { return window && SERVICES->is_kind_of(window, s_edit_class); }

/* 0x22318e20, vtable +0xd8: only once the page exists. A focus that is an unmodified edit
 * passes at once; otherwise the controls into the slice, the dialog's check (its answer
 * unused), and the first failed status's edit. An edit with focus is marked unmodified
 * when nothing failed. */
uint32_t TC fs98_flights_nav_radios_validate(void *page, uint32_t control)
{
    static const uint32_t fields[][4] = {   /* edit, text, room, status: its failure */
        {0x239c, 0x1c, 0x20, 0x18}, {0x2334, 0x34, 0x38, 0x30}, {0x22cc, 0xa4, 0xa8, 0xa0}, {0x2264, 0xbc, 0xc0, 0xb8}
    };
    static const uint32_t failures[] = {0xcb9, 0xcba, 0xcbb, 0xcbc};
    void *window, *descriptor;
    uint32_t failure = 0, index;
    if (D32(page, 0x1100) == 0) return 0;
    window = SERVICES->dialog_item(page, (int32_t)control);
    if (is_edit(window) && SERVICES->send_message(D32(window, 0x20), EM_GETMODIFY_, 0, 0) == 0) return 0;
    failure = (uint32_t)SERVICES->send_message(D32(page, 0x2424), CB_GETCURSEL_, 0, 0);
    S32(page, 0x9c) = failure;      /* the slice is read after the call */
    failure = 0;
    for (index = 0; index < 4; ++index)
        SERVICES->get_window_text(AT(page, fields[index][0]), PTR(SLICE(page), fields[index][1]),
                                  (int32_t)S32(page, fields[index][2]));
    descriptor = PTR(page, 0x111c);
    ((fs98_flights_navigation_check_fn)PTR(descriptor, 4))(descriptor);
    for (index = 0; index < 4; ++index)
        if (S32(page, fields[index][3]) != 0) { failure = failures[index]; break; }
    if (failure == 0 && is_edit(window)) SERVICES->send_message(D32(window, 0x20), EM_SETMODIFY_, 0, 0);
    fs98_flights_nav_radios_fill(page);
    return failure;
}

/* 0x22318f90 and 0x22319a40, vtable +0xdc: the first failed status is shown against its
 * edit (given the status's address), and if the error dialog answers, that edit's id goes
 * to vtable +0xe0; otherwise 0 does. */
static uint32_t nav_page_error(void *page, uint32_t code, const uint32_t (*fields)[3], uint32_t count)
{
    uint32_t id = 0, index;
    if (code != 0) {
        uint8_t *slice = SLICE(page);
        for (index = 0; index < count; ++index) {
            if (D32(slice, fields[index][0]) == 0) continue;
            if (SERVICES->error_dialog(page, AT(page, fields[index][1]), (uint32_t)(uintptr_t)(slice + fields[index][0])))
                id = fields[index][2];
            break;
        }
    }
    return ((uint32_t (TC *)(void *, uint32_t))PTR(PTR(page, 0), 0xe0))(page, id);
}
uint32_t TC fs98_flights_nav_radios_error(void *page, uint32_t code)
{
    static const uint32_t fields[][3] = {{0x18, 0x239c, 0xcb9}, {0x30, 0x2334, 0xcba}, {0xa0, 0x22cc, 0xcbb}, {0xb8, 0x2264, 0xcbc}};
    return nav_page_error(page, code, fields, 4);
}

/* ---- the transponder and ADF page -------------------------------------------------------
 * Labels 0x112c..0x185c, the transponder edit 0x1914 (window +0x1934, label 0x17a4, its
 * limit +0x1978) and the ADF frequency edit 0x197c (window +0x199c, label 0x185c, limit
 * +0x19e0), and a CSimsStatic 0x19e4. Its slice: transponder fitted +4, ADF off +8, the
 * ADF station in place +0xc and +0x12, the transponder's status +0x30, text +0x34, room
 * +0x38, the ADF frequency's status +0x44, text +0x48, room +0x4c, its digits (a byte)
 * +0x58 (3 or more), a 16-bit "received" +0x5a, names +0x60 +0x74. */

/* 0x22319ac0 */
void TC fs98_flights_nav_adf_fill(void *page)
{
    static const uint32_t station[] = {0x14c4, 0x1634, 0x140c, 0x157c, 0x129c, 0x112c, 0x1354, 0x11e4};
    D32(page, 0x1978) = S32(page, 0x38) - 1;
    D32(page, 0x19e0) = S32(page, 0x4c) - 1;
    SERVICES->set_window_text(AT(page, 0x1914), PTR(SLICE(page), 0x34));
    SERVICES->set_window_text(AT(page, 0x197c), PTR(SLICE(page), 0x48));
    SERVICES->set_sims_text(AT(page, 0x140c), (const char *)AT(SLICE(page), 0x12));
    SERVICES->set_sims_text(AT(page, 0x157c), (const char *)AT(SLICE(page), 0xc));
    SERVICES->set_sims_text(AT(page, 0x129c), PTR(SLICE(page), 0x60));
    SERVICES->set_sims_text(AT(page, 0x112c), PTR(SLICE(page), 0x74));
    if (S32(page, 8) != 0) {
        enable_all(page, station, 8, 0);
        enable(page, 0x16ec, 0);
        enable(page, 0x185c, 0);
        enable(page, 0x197c, 0);
    } else {
        enable_all(page, station, 8, W16(SLICE(page), 0x5a) != 0);
    }
    if (S32(page, 4) != 0) {
        enable(page, 0x17a4, 0);
        enable(page, 0x1914, 0);
    }
}

/* 0x22319810, vtable +0xf0 */
void TC fs98_flights_nav_adf_init(void *page)
{
    static const uint32_t labels[] = {0x16ec, 0x140c, 0x14c4, 0x157c, 0x1634, 0x112c, 0x11e4, 0x129c, 0x1354, 0x185c};
    struct fs98_flights_rect rect;
    uint32_t index, flags;
    for (index = 0; index < 10; ++index) SERVICES->init_text(AT(page, labels[index]), AT(page, 0x220));
    flags = *((uint8_t *)SLICE(page) + 0x58) == 3 ? 0x11 : 0x15;
    SERVICES->init_edit(AT(page, 0x197c), SERVICES->base_font(2), 0xffffff, 0, 0xff0000, AT(page, 0x185c), flags);
    SERVICES->send_message(D32(page, 0x199c), EM_LIMITTEXT, *((uint8_t *)SLICE(page) + 0x58), 0);
    SERVICES->init_text(AT(page, 0x17a4), AT(page, 0x220));
    SERVICES->init_edit(AT(page, 0x1914), SERVICES->base_font(2), 0xffffff, 0, 0xff0000, AT(page, 0x17a4), 0x11);
    SERVICES->remap_rect(&rect, AT(page, 0x19e4));
    SERVICES->move_window(AT(page, 0x19e4), rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, 1);
    SERVICES->send_message(D32(page, 0x1934), EM_LIMITTEXT, S32(page, 0x38) - 1, 0);
    fs98_flights_nav_adf_fill(page);
}

/* 0x223199b0, vtable +0xd8: the ADF frequency's status wins over the transponder's */
uint32_t TC fs98_flights_nav_adf_validate(void *page, uint32_t control)
{
    void *descriptor;
    uint32_t failure;
    (void)control;
    if (D32(page, 0x1100) == 0) return 0;
    SERVICES->get_window_text(AT(page, 0x197c), PTR(SLICE(page), 0x48), 7);
    SERVICES->get_window_text(AT(page, 0x1914), PTR(SLICE(page), 0x34), 6);
    descriptor = PTR(page, 0x111c);
    ((fs98_flights_navigation_check_fn)PTR(descriptor, 4))(descriptor);
    failure = S32(page, 0x44) != 0 ? 0xcb7 : S32(page, 0x30) != 0 ? 0xcb8 : 0;
    fs98_flights_nav_adf_fill(page);
    return failure;
}
uint32_t TC fs98_flights_nav_adf_error(void *page, uint32_t code)
{
    static const uint32_t fields[][3] = {{0x44, 0x197c, 0xcb7}, {0x30, 0x1914, 0xcb8}};
    return nav_page_error(page, code, fields, 2);
}

/* ---- the dialog's OnInitDialog ---------------------------------------------------------- */
#include "flights_objects.h"

/* 0x22317ff0, vtable +0xc4. The vtable is read once, after CSimsDialog's OnInitDialog, for
 * both the skin and InitTab. Each page is made, stored, then given CSimsTabPage::Init(9)
 * even when ui_new failed. Then the tabs, captioned from Flights' strings and flagged with
 * each slice's first word; no descriptor stops it there, after the tabs have read it. The
 * first tab shown is 1 or 2 when the first slice's word is set (2 when the second's is
 * too), otherwise the descriptor's +8. */
int32_t TC fs98_flights_navigation_init(void *object)
{
    static const uint32_t pages[] = {0x2160, 0x2164, 0x2168}, slices[] = {0x10, 0x130, 0x1bc};
    static const uint32_t kinds[] = {FS98_FLIGHTS_NAV_RADIOS, FS98_FLIGHTS_NAV_ADF, FS98_FLIGHTS_NAV_EFIS};
    static const uint32_t captions[] = {0xf6fc, 0xf6fa, 0xf6fb}, helpers[] = {0x2088, 0x20d0, 0x2118};
    struct fs98_flights_rect empty = {0, 0, 0, 0};
    uint32_t module = fs98_flights_resource_module(), index;
    void *tabs = AT(object, 0x12d8), *descriptor;
    void **vtable;
    char caption[0x1d];
    int32_t active;
    SERVICES->base_init(object);
    vtable = PTR(object, 0);
    if (!((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))vtable[0xe4 / 4])(object, 0x14f, module, 0xf))
        return 0;
    SERVICES->init_art(AT(object, 0x1fa0), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    SERVICES->init_button(AT(object, 0x10e0), AT(object, 0x5a0), AT(object, 0x610), 0x132,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff, module, 0, 0, empty);
    SERVICES->init_button(AT(object, 0x11dc), AT(object, 0x5a0), AT(object, 0x610), 0x132,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff, module, 0, 0, empty);
    SERVICES->tab_init(tabs, object, SERVICES->base_font(0xa), 0x148, AT(object, 0xff4), AT(object, 0x7e8),
                       0, 0xc4c4c4, 5, 0x1d, 0x47, -1, module, 0);
    ((int32_t (TC *)(void *, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t))vtable[0xe8 / 4])(
        object, 0x150, 0x1d, 0x47, 0x1a8, 0x121, module);
    for (index = 0; index < 3; ++index) {
        void *page = fs98_flights_new_page(kinds[index], object);
        PTR(object, pages[index]) = page;
        SERVICES->page_init(page, 9);
    }
    for (index = 0; index < 3; ++index) {
        SERVICES->load_string_a(D32(object, 0x80), captions[index], caption, 0x1d);
        SERVICES->tab_add_page(tabs, PTR(object, pages[index]), caption,
                               (int32_t)D32(PTR(object, 0x10dc), slices[index]));
    }
    for (index = 0; index < 3; ++index) SERVICES->tab_helper_init(AT(object, helpers[index]), tabs);
    SERVICES->tab_set_last_page(tabs, PTR(object, 0x2168));
    if (!fs98_flights_picker_validate(object)) return 0;
    fs98_flights_navigation_wire(object);
    descriptor = PTR(object, 0x10dc);
    if (D32(descriptor, 0x10) != 0) active = D32(descriptor, 0x130) != 0 ? 2 : 1;
    else active = (int32_t)D32(descriptor, 8);
    SERVICES->tab_set_active(tabs, active, 0);
    SERVICES->show_window(object, 1);
    PTR(object, 0x7c) = tabs;
    return 1;
}
